#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs_utils.h"
#include "superblock.h"

#define VERSION "0.0.2"

/* command line options */
static int opt_flag_f = 0;
static int opt_flag_r = 0;
static int opt_flag_w = 0;

/* define usage function */
static void usage(void) {
    printf(
        "chfs - Version %s\n"
        "usage: chfs.pangya -f|--filename <PangYa filesystem disk image filename>\n"
        "                   (-r|--readonly | -w|--readwrite)\n"
        "                   [-h|--help]\n", VERSION
    );
}

int main(int argc, char *argv[]) {
    /* define command line options */
    char *short_opts = "f:rwh";
    struct option long_opts[] = {
        {"filename", required_argument, NULL, 'f'},
        {"readonly", no_argument, NULL, 'r'},
        {"readwrite", no_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    char filename[PATH_MAX];
    int disk_image_fd = -1;
    struct superblock sb;
    memset(&sb, 0, sizeof(struct superblock));
    int ret_read_superblock;
    int ret_write_superblock;

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
            case 'r':
                opt_flag_r = 1;

                break;
            case 'w':
                opt_flag_w = 1;

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

    if ((opt_flag_r && opt_flag_w) || (!opt_flag_r && !opt_flag_w)) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* open disk image file */
    disk_image_fd = open(filename, O_RDWR);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", filename, strerror(errno));
        goto error_handler;
    }

    /* change s_ronly flag on superblock */
    ret_read_superblock = read_superblock(disk_image_fd, &sb);
    if (ret_read_superblock < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    if (opt_flag_r) {
        sb.s_ronly = READ_ONLY;
    }

    if (opt_flag_w) {
        sb.s_ronly = READ_WRITE;
    }

    /* write superblock data in block #1 */
    ret_write_superblock = write_superblock(disk_image_fd, &sb);
    if (ret_write_superblock < 0) {
        fprintf(stderr, "ERROR: failed to write superblock\n");
        goto error_handler;
    }

    /* sync data and close on disk image */
    fsync(disk_image_fd);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
