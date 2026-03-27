#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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
        "cp - Version %s\n"
        "usage: cp.pangya -f|--filename <PangYa filesystem disk image filename> <host_path_name> <target_path_name>\n"
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
    int host_file_fd = -1;
    struct superblock sb;
    char *host_path_name;
    char *target_path_name;
    char *dir_name_copy = NULL;
    char *base_name_copy = NULL;
    struct inode *parent_dir_inode = NULL;
    struct inode *target_path_inode = NULL;
    char buffer[BLOCKSIZE];

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

    /* handle path name parameters */
    if (optind != argc - 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    host_path_name = argv[optind];
    target_path_name = argv[optind + 1];

    /* check host file type, bail out if file type is not regular file */
    struct stat host_stat;

    if (stat(host_path_name, &host_stat) < 0) {
        fprintf(stderr, "ERROR: failed to get host file stat struct: %s\n", strerror(errno));
        goto error_handler;
    }

    if ((host_stat.st_mode & S_IFMT) != S_IFREG) {
        fprintf(stderr, "ERROR: host file %s is not regular file type\n", host_path_name);
        goto error_handler;
    }

    /* check host file size, bail out if file size is too large */
    if (host_stat.st_size > NDIRECT * BLOCKSIZE) {
        fprintf(stderr, "ERROR: %s: %s\n", host_path_name, strerror(EFBIG));
        goto error_handler;
    }

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
    dir_name_copy = strdup(target_path_name);
    base_name_copy = strdup(target_path_name);

    if (dir_name_copy == NULL || base_name_copy == NULL) {
        fprintf(stderr, "ERROR: failed to copy target path name %s\n", target_path_name);
        goto error_handler;
    }

    char *dir_name = dirname(dir_name_copy);
    char *base_name = basename(base_name_copy);

    /* avoid re-creating root directory */
    if (strcmp(base_name, "/") == 0) {
        fprintf(stderr, "ERROR: cannot create target filename /: %s\n", strerror(EEXIST));
        goto error_handler;
    }

    if (strcmp(base_name, ".") == 0 || strcmp(base_name, "..") == 0) {
        fprintf(stderr, "ERROR: cannot create target filename %s: %s\n", base_name, strerror(EINVAL));
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
        fprintf(stderr, "ERROR: failed to get target parent directory inode %s\n", dir_name);
        goto error_handler;
    }

    /* check if target pathname exists or not */
    long int ret_search_dir = search_dir(disk_image_fd, &sb, parent_dir_inode, base_name);

    if (ret_search_dir > 0) {
        fprintf(stderr, "ERROR: target path name %s exists already\n", target_path_name);
        goto error_handler;
    }

    if (ret_search_dir < 0) {
        fprintf(stderr, "ERROR: failed to call search direcotry function: %s\n", strerror(-(int)ret_search_dir));
        goto error_handler;
    }

    /* allocate inode for new file */
    target_path_inode = alloc_inode(disk_image_fd, &sb, IFREG);
    if (target_path_inode == NULL) {
        fprintf(stderr, "ERROR: failed to allocate inode for file %s\n", target_path_name);
        goto error_handler;
    }

    /* copy data */
    host_file_fd = open(host_path_name, O_RDONLY);
    if (host_file_fd < 0) {
        fprintf(stderr, "ERROR: failed to open host file %s: %s\n", host_path_name, strerror(errno));
        goto error_handler;
    }

    for (int i = 0; i < (host_stat.st_size + BLOCKSIZE - 1) / BLOCKSIZE; ++i) {
        /* clear buffer */
        memset(buffer, 0, BLOCKSIZE);

        /* read data from host file by BLOCKSIZE size */
        if (read(host_file_fd, buffer, BLOCKSIZE) < 0) {
            fprintf(stderr, "ERROR: failed to read %d round of data from %s: %s\n", i, host_path_name, strerror(errno));
            goto error_handler;
        }

        /* allocate block */
        long int block_number = alloc_block(disk_image_fd, &sb);
        if (block_number < 0) {
            fprintf(stderr, "ERROR: failed to allocate block for %s: %s\n", target_path_name, strerror(-(int)block_number));
            goto error_handler;
        }

        /* write data to allocated block */
        int ret_write_block = write_block(disk_image_fd, (unsigned long int)block_number, buffer, BLOCKSIZE, 0);
        if (ret_write_block < 0) {
            fprintf(stderr, "ERROR: failed to write block number %lu for %s: %s\n", (unsigned long int)block_number, target_path_name, strerror(-ret_write_block));
            goto error_handler;
        }

        /* write block number into i_addr[] array */
        target_path_inode->i_addr[i] = (unsigned long int)block_number;
    }

    /* set new file metadata */
    target_path_inode->i_size0 = (unsigned int)host_stat.st_size;
    target_path_inode->i_nlink = 1; /* fail-safe. alloc_inode() should set this item to 1 already */

    /* add new file entry to parent directory */
    int ret_add_entry = add_entry(disk_image_fd, &sb, parent_dir_inode, base_name, target_path_inode->i_num);
    if (ret_add_entry < 0) {
        fprintf(stderr, "ERROR: failed to add file entry to parent directory: %s\n", strerror(-ret_add_entry));
        goto error_handler;
    }

    /* update DIRTY flag last only to make sure all operations are successful */
    target_path_inode->i_dirty = DIRTY;
    parent_dir_inode->i_dirty = DIRTY;

    /* release resource */
    put_inode(disk_image_fd, &sb, target_path_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);
    close(host_file_fd);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    /* set nlink to 0 to force release resource */
    if (target_path_inode != NULL) {
        target_path_inode->i_nlink = 0;
    }

    put_inode(disk_image_fd, &sb, target_path_inode);
    put_inode(disk_image_fd, &sb, parent_dir_inode);
    free(dir_name_copy);
    free(base_name_copy);

    if (host_file_fd >= 0) {
        close(host_file_fd);
    }

    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
