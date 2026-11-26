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
#include "bitmap.h"
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

static void print_error_stats(unsigned int error_count) {
    printf("Error Count: %u\n", error_count);
}

static void print_fatal_stats(unsigned int fatal_count) {
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

static void check_inode(int input_disk_image_fd, struct superblock *input_super_block, unsigned int *error_count) {
    int32_t superblock_used_inodes = count_bits(input_disk_image_fd, input_super_block, INODE_BITMAP);
    if (superblock_used_inodes < 0) {
        fprintf(stderr, "ERROR: failed to count inode bitmap bits\n");

        ++(*error_count);

        return;
    }

    /* inode number 0 is reserved */
    int32_t scan_used_inodes = 1;

    /* scan each inode, starts with inode number 1 as inode number 0 is reserved */
    int ret_read_disk_inode;
    struct disk_inode inode;

    for (uint32_t inode_number = 1; inode_number < input_super_block->s_ninodes; ++inode_number) {
        ret_read_disk_inode = read_disk_inode(input_disk_image_fd, input_super_block, inode_number, &inode);
        if (ret_read_disk_inode < 0) {
            ++(*error_count);

            continue;
        }

        /* check inode format */
        if (inode.i_mode != 0 && inode.i_mode != (IALLOC | IFREG) && inode.i_mode != (IALLOC | IFDIR)) {
            fprintf(stderr, "ERROR: inode %" PRIu32 " is in invalid mode\n", inode_number);

            ++(*error_count);
        }

        if (inode.i_size0 > NDIRECT * BLOCKSIZE) {
            fprintf(stderr, "ERROR: inode %" PRIu32 " has invalid file size: %" PRIu32 "\n", inode_number, inode.i_size0);

            ++(*error_count);
        }

        /* check used inode consistency */
        int bitmap_bit = bitmap_get(input_disk_image_fd, input_super_block, inode_number, INODE_BITMAP);
        if (bitmap_bit < 0) {
            ++(*error_count);

            continue;
        }

        if (bitmap_bit == 1) {
            if ((inode.i_mode & IALLOC) != 0) {
                ++scan_used_inodes;
            } else {
                fprintf(stderr, "ERROR: inode %" PRIu32 " in inode bitmap is allocated but inode itself is not allocated\n", inode_number);

                ++(*error_count);
            }
        } else {
            if ((inode.i_mode & IALLOC) != 0) {
                ++scan_used_inodes;
                fprintf(stderr, "ERROR: inode %" PRIu32 " in inode bitmap is not allocated but inode itself is allocated\n", inode_number);

                ++(*error_count);
            }
        }
    }

    /* compare number of scanned used inodes and used inodes from superblock */
    if (superblock_used_inodes != scan_used_inodes) {
        fprintf(stderr, "ERROR: number of used inodes in superblock inode bitmap does not match number of used inodes in scanned inodes: %" PRId32 " <=> %" PRId32 "\n", superblock_used_inodes, scan_used_inodes);

        ++(*error_count);
    }

    /* check root inode */
    ret_read_disk_inode = read_disk_inode(input_disk_image_fd, input_super_block, ROOT_INODE, &inode);
    if (ret_read_disk_inode < 0) {
        fprintf(stderr, "ERROR: failed to read root inode\n");

        ++(*error_count);
    } else {
        uint32_t data_block_start = 2 + input_super_block->s_inode_map_size + input_super_block->s_block_map_size + input_super_block->s_isize;

        if (inode.i_mode != (IALLOC | IFDIR) || inode.i_addr[0] != data_block_start) {
            fprintf(stderr, "ERROR: root inode is invalid\n");

            ++(*error_count);
        }
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
    unsigned int block_error = 0;
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
    print_error_stats(superblock_error);
    print_fatal_stats(superblock_fatal);

    /* bail out if any fatal is found in superblock check stage. filesystem layout is broken */
    if (superblock_fatal > 0) {
        fprintf(stderr, "ERROR: superblock has fatal failure(s), cannot move forward\n");
        exit(EXIT_FAILURE);
    }

    /* inode check */
    printf("\n===== INODE CHECK =====\n");
    check_inode(disk_image_fd, &sb, &inode_error);
    printf("\n");
    print_error_stats(inode_error);

    /* disk block check */

    total_failure = superblock_error + inode_error + block_error;

    close(disk_image_fd);

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
