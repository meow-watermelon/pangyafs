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
        "mkdir - Version %s\n"
        "usage: mkdir.pangya -f|--filename <PangYa filesystem disk image filename> <path_name>\n"
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
    struct inode *dir_inode = NULL;

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
        fprintf(stderr, "ERROR: failed to copy original directory name %s\n", pathname);
        goto error_handler;
    }

    char *dir_name = dirname(dir_name_copy);
    char *base_name = basename(base_name_copy);

    /* avoid re-creating root directory */
    if (strcmp(base_name, "/") == 0) {
        fprintf(stderr, "ERROR: cannot create directory /: %s\n", strerror(EEXIST));
        goto error_handler;
    }

    if (strcmp(base_name, ".") == 0 || strcmp(base_name, "..") == 0) {
        fprintf(stderr, "ERROR: cannot create directory %s: %s\n", base_name, strerror(EINVAL));
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

    if (ret_search_dir > 0) {
        fprintf(stderr, "ERROR: path name %s exists already\n", pathname);
        goto error_handler;
    }

    if (ret_search_dir < 0) {
        fprintf(stderr, "ERROR: failed to call search direcotry function: %s\n", strerror(-(int)ret_search_dir));
        goto error_handler;
    }

    /* allocate inode for new directory */
    dir_inode = alloc_inode(disk_image_fd, &sb, IFDIR);
    if (dir_inode == NULL) {
        fprintf(stderr, "ERROR: failed to allocate inode for directory %s\n", pathname);
        goto error_handler;
    }

    /* allocate block for dirents */
    long int dir_block_number = alloc_block(disk_image_fd, &sb);
    if (dir_block_number < 0) {
        fprintf(stderr, "ERROR: failed to allocate block for directory %s\n", pathname);
        goto error_handler;
    }

    /* write "." and ".." for new directory to allocated block */
    struct dirent dirents[2];
    memset(dirents, 0, sizeof(dirents));

    dirents[0].inode_number = dir_inode->i_num;
    strncpy(dirents[0].dir_name, ".", DIR_NAME_LENGTH);
    dirents[1].inode_number = parent_dir_inode->i_num;
    strncpy(dirents[1].dir_name, "..", DIR_NAME_LENGTH);

    int ret_write_block = write_block(disk_image_fd, (unsigned long int)dir_block_number, dirents, sizeof(dirents), 0);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write dirent entries to allocated block: %s\n", strerror(-ret_write_block));
        goto error_handler;
    }

    /* set new directory metadata */
    dir_inode->i_size0 = 2 * sizeof(struct dirent);
    dir_inode->i_nlink = 2; /* fail-safe. alloc_inode() should set this item to 2 already */
    dir_inode->i_addr[0] = (unsigned long int)dir_block_number;

    /* add new directory entry to parent directory */
    int ret_add_entry = add_entry(disk_image_fd, &sb, parent_dir_inode, base_name, dir_inode->i_num);
    if (ret_add_entry < 0) {
        fprintf(stderr, "ERROR: failed to add child directory entry to parent directory: %s\n", strerror(-ret_add_entry));
        goto error_handler;
    }

    /* increment parent directory link count */
    ++parent_dir_inode->i_nlink;

    /* update DIRTY flag last only to make sure all operations are successful */
    dir_inode->i_dirty = DIRTY;
    parent_dir_inode->i_dirty = DIRTY;

    /* release resource */
    put_inode(disk_image_fd, &sb, dir_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    /* set nlink to 0 to force release resource */
    if (dir_inode != NULL) {
        dir_inode->i_nlink = 0;
    }

    put_inode(disk_image_fd, &sb, dir_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);

    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
