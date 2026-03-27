#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "allocator.h"
#include "dir.h"
#include "fs_utils.h"
#include "superblock.h"

#define VERSION "0.0.1"

/* command line options */
static int opt_flag_f = 0;

/* define usage function */
static void usage(void) {
    printf(
        "rm - Version %s\n"
        "usage: rm.pangya -f|--filename <PangYa filesystem disk image filename> <path_name>\n"
        "                 [-h|--help]\n", VERSION
    );
}

int main(int argc, char *argv[]) {
    /* define command line options */
    char *short_opts = "f:h";
    struct option long_opts[] = {
        {"filename", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    char filename[PATH_MAX];
    int disk_image_fd = -1;
    struct superblock sb;
    char *pathname;
    char *dir_name_copy = NULL;
    char *base_name_copy = NULL;
    struct inode *parent_dir_inode = NULL;
    struct inode *pathname_inode = NULL;

    /* suppress default getopt error messages */
    opterr = 0;
    int c;

    while (1) {
        c = getopt_long(argc, argv, short_opts, long_opts, NULL);

        if (c < 0) {
            break;
        }

        switch (c) {
            case 'f':
                strcpy(filename, optarg);
                opt_flag_f = 1;

                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "ERROR: Unknown option\n\n");
                usage();
                exit(EXIT_FAILURE);
            default:
                fprintf(stderr, "ERROR: Unimplemented option\n\n");
                usage();
                exit(EXIT_FAILURE);
        }
    }

    /* check if necessary options are specified */
    if (!opt_flag_f) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* handle path name parameter */
    if (optind != argc - 1) {
        usage();
        exit(EXIT_FAILURE);
    }

    pathname = argv[optind];

    /* open disk image file */
    disk_image_fd = open(filename, O_RDWR);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", filename, strerror(errno));
        goto error_handler;
    }

    /* read superblock */
    if (read_superblock(disk_image_fd, &sb) < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    /* split dirname and basename */
    dir_name_copy = strdup(pathname);
    base_name_copy = strdup(pathname);

    if (dir_name_copy == NULL || base_name_copy == NULL) {
        fprintf(stderr, "ERROR: failed to copy original path name %s\n", pathname);
        goto error_handler;
    }

    char *dir_name = dirname(dir_name_copy);
    char *base_name = basename(base_name_copy);

    /* avoid deleting root directory */
    if (strcmp(base_name, "/") == 0) {
        fprintf(stderr, "ERROR: cannot delete directory /: %s\n", strerror(EINVAL));
        goto error_handler;
    }

    if (strcmp(base_name, ".") == 0 || strcmp(base_name, "..") == 0) {
        fprintf(stderr, "ERROR: cannot delete directory %s: %s\n", base_name, strerror(EINVAL));
        goto error_handler;
    }

    /* check if base_name is too long */
    if (strlen(base_name) >= DIR_NAME_LENGTH) {
        fprintf(stderr, "ERROR: %s: %s\n", base_name, strerror(ENAMETOOLONG));
        goto error_handler;
    }

    /* get parent directory inode struct */
    parent_dir_inode = namei(disk_image_fd, &sb, dir_name);
    if (parent_dir_inode == NULL) {
        fprintf(stderr, "ERROR: failed to get parent directory inode %s\n", dir_name);
        goto error_handler;
    }

    /* check if pathname exists or not */
    long int ret_search_dir = search_dir(disk_image_fd, &sb, parent_dir_inode, base_name);

    if (ret_search_dir == 0) {
        fprintf(stderr, "ERROR: %s: %s\n", pathname, strerror(ENOENT));
        goto error_handler;
    }

    if (ret_search_dir < 0) {
        fprintf(stderr, "ERROR: failed to call search direcotry function: %s\n", strerror(-(int)ret_search_dir));
        goto error_handler;
    }

    /* get target path name inode struct */
    pathname_inode = namei(disk_image_fd, &sb, pathname);
    if (pathname_inode == NULL) {
        fprintf(stderr, "ERROR: failed to get path name inode %s\n", pathname);
        goto error_handler;
    }

    /* handle regular file type(IFREG) */
    if ((pathname_inode->i_mode & IFREG) != 0) {
        /* decrement link count */
        --pathname_inode->i_nlink;
    }

    /* handle directory file type(IFDIR) */
    if ((pathname_inode->i_mode & IFDIR) != 0) {
        /* check if target directory is empty or not, bail out if it is not empty */
        int dir_empty_flag = 0; /* 0: empty; > 0: not empty */
        size_t dirent_entries = get_dirent_per_block();
        struct dirent dirent_entries_buffer[dirent_entries];

        for (unsigned short int logical_block = 0; logical_block < NDIRECT; ++logical_block) {
            if (pathname_inode->i_addr[logical_block] != 0) {
                /* FIXME: ignore read block failure */
                read_block(disk_image_fd, pathname_inode->i_addr[logical_block], dirent_entries_buffer, sizeof(dirent_entries_buffer), 0);

                for (size_t i = 0; i < dirent_entries; ++i) {
                    if (dirent_entries_buffer[i].inode_number == 0) {
                        continue;
                    }

                    if (strcmp(dirent_entries_buffer[i].dir_name, ".") == 0 || strcmp(dirent_entries_buffer[i].dir_name, "..") == 0) {
                        continue;
                    }

                    ++dir_empty_flag;
                }
            }
        }

        if (dir_empty_flag > 0) {
            fprintf(stderr, "ERROR: %s has %d entries: %s\n", pathname, dir_empty_flag, strerror(ENOTEMPTY));
            goto error_handler;
        }

        /* set target path directory link count to 0 and decrement parent directory link count */
        pathname_inode->i_nlink = 0;
        --parent_dir_inode->i_nlink;
    }

    /* remove entry */
    int ret_detach_entry = detach_entry(disk_image_fd, &sb, parent_dir_inode, base_name);
    if (ret_detach_entry < 0) {
        fprintf(stderr, "ERROR: failed to remove entry %s: %s\n", base_name, strerror(-ret_detach_entry));
        goto error_handler;
    }

    /* update DIRTY flag last only to make sure all operations are successful */
    pathname_inode->i_dirty = DIRTY;
    parent_dir_inode->i_dirty = DIRTY;

    /* release resource */
    put_inode(disk_image_fd, &sb, pathname_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    put_inode(disk_image_fd, &sb, pathname_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);

    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
