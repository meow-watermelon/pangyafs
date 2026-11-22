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

#define VERSION "0.0.1"

/* command line options */
static int opt_flag_f = 0;

/* define usage function */
static void usage(void) {
    printf(
        "fsck - Version %s\n"
        "usage: fsck.pangya -f|--filename <PangYa filesystem disk image filename>\n"
        "                   [-h|--help]\n", VERSION
    );
}

static void print_stats(unsigned int error_count, unsigned int fatal_count) {
    printf("Error Count: %u\n", error_count);
    printf("Fatal Count: %u\n", fatal_count);
}

static void check_superblock(struct superblock *input_super_block, unsigned int *error_count, unsigned int *fatal_count) {
    /* check number of blocks for inode bitmap */
    uint32_t report_inode_bitmap_block_size;
    report_inode_bitmap_block_size = get_inode_bitmap_block_size(input_super_block->s_ninodes);
    if (report_inode_bitmap_block_size != input_super_block->s_inode_map_size) {
        fprintf(stderr, "ERROR: number of blocks for inode bitmap does not match superblock record: %" PRIu32 " <=> %" PRIu32 "\n", report_inode_bitmap_block_size, input_super_block->s_inode_map_size);

        ++(*fatal_count);
    }

    /* check number of blocks for block bitmap */
    uint32_t report_block_bitmap_block_size;
    report_block_bitmap_block_size = get_block_bitmap_block_size(input_super_block->s_fsize);
    if (report_block_bitmap_block_size != input_super_block->s_block_map_size) {
        fprintf(stderr, "ERROR: number of blocks for block bitmap does not match superblock record: %" PRIu32 " <=> %" PRIu32 "\n", report_block_bitmap_block_size, input_super_block->s_block_map_size);

        ++(*fatal_count);
    }

    /* check read-only flag */
    if (input_super_block->s_ronly != READ_WRITE && input_super_block->s_ronly != READ_ONLY) {
        fprintf(stderr, "ERROR: read-only flag is invalid\n");

        ++(*error_count);
    }

    /* check last update timestamp */
    if (input_super_block->s_time <= 0) {
        fprintf(stderr, "ERROR: last update timestamp is invalid\n");

        ++(*error_count);
    }
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
    memset(&sb, 0, sizeof(struct superblock));

    /* error and fatal counts */
    unsigned int superblock_error = 0;
    unsigned int superblock_fatal = 0;
    unsigned int inode_error = 0;
    unsigned int inode_fatal = 0;
    unsigned int block_error = 0;
    unsigned int block_fatal = 0;
    unsigned int total_failure = 0;

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

    /* open disk image file */
    disk_image_fd = open(filename, O_RDONLY);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", filename, strerror(errno));
        goto error_handler;
    }

    /* read superblock */
    int ret_read_superblock = read_superblock(disk_image_fd, &sb);
    if (ret_read_superblock < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    /*
     * fsck check procedure
     * 1. superblock check
     *   + magic number check has been included in read_superblock() function, no magic number check in this utility
     * 2. inode check
     * 3. disk block check
     *
    */

    /* superblock check */
    printf("===== SUPERBLOCK CHECK =====\n");
    check_superblock(&sb, &superblock_error, &superblock_fatal);
    printf("\n");
    print_stats(superblock_error, superblock_fatal);

    total_failure = superblock_error + superblock_fatal + inode_error + inode_fatal + block_error + block_fatal;

    if (total_failure > 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);

error_handler:
    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
