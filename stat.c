#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "allocator.h"
#include "dir.h"
#include "fs_utils.h"
#include "superblock.h"

#define VERSION "0.0.2"

/* command line options */
static int opt_flag_f = 0;

/* define usage function */
static void usage(void) {
    printf(
        "stat - Version %s\n"
        "usage: stat.pangya -f|--filename <PangYa filesystem disk image filename> <path_name>\n"
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
    disk_image_fd = open(filename, O_RDONLY);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", filename, strerror(errno));
        goto error_handler;
    }

    /* read superblock */
    if (read_superblock(disk_image_fd, &sb) < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    /* get target path name inode */
    struct inode *pathname_inode = namei(disk_image_fd, &sb, pathname);
    if (pathname_inode == NULL) {
        fprintf(stderr, "ERROR: failed to get path name %s inode\n", pathname);
        goto error_handler;
    }

    /* print output */
    printf("  File: %s\n", pathname);
    printf("  Size: %-15" PRIu32 " Blocks: %-10zu IO Block: %-6d\n", pathname_inode->i_size0, get_file_block_size(pathname_inode->i_size0), BLOCKSIZE);
    printf("  Type: %-6s Inode: %-10" PRIu32 " Links: %-6" PRIu16 "\n", get_file_type_name(pathname_inode->i_mode), pathname_inode->i_num, pathname_inode->i_nlink);

    /* release resource */
    put_inode(disk_image_fd, &sb, pathname_inode);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
