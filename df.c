#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bitmap.h"
#include "fs_utils.h"
#include "superblock.h"

#define VERSION "0.0.1"

/* command line options */
static int opt_flag_f = 0;

/* define usage function */
static void usage(void) {
    printf(
        "df - Version %s\n"
        "usage: df.pangya -f|--filename <PangYa filesystem disk image filename>\n"
        "                 [-h|--help]\n", VERSION
    );
}

static void print_superblock_info(struct superblock *input_super_block) {
    printf("===== SUPERBLOCK INFORMATION =====\n");
    printf("block size: %hu B\n", BLOCKSIZE);
    printf("filesystem size(byte): %lu B\n", input_super_block->s_fsize * BLOCKSIZE);
    printf("filesystem size(block): %lu\n", input_super_block->s_fsize);
    printf("number of inode(s): %lu\n", input_super_block->s_ninodes);
    printf("inode list size(block): %lu\n", input_super_block->s_isize);
    printf("inode bitmap size(block): %lu\n", input_super_block->s_inode_map_size);
    printf("block bitmap size(block): %lu\n", input_super_block->s_block_map_size);
    printf("\n");
}

static void print_inode_usage_info(int input_disk_image_fd, struct superblock *input_super_block) {
    char *banner = "      Inodes    IUsed    IFree  IUse%";

    unsigned long int inodes = input_super_block->s_ninodes;

    long int iused = count_bits(input_disk_image_fd, input_super_block, INODE_BITMAP);
    if (iused < 0) {
        fprintf(stderr, "ERROR: failed to retrieve bitmap count from inode bitmap\n");
        return;
    }

    unsigned long int ifree = inodes - (unsigned long int)iused;
    double iuse = (double)iused / (double)inodes * 100;

    printf("===== INODE USAGE INFORMATION =====\n");
    printf("%s\n", banner);
    printf("%12lu %8lu %8lu %5.2f%%\n", inodes, iused, ifree, iuse);
    printf("\n");
}

static void print_block_usage_info(int input_disk_image_fd, struct superblock *input_super_block) {
    char *banner = "      1024-blocks     Used     Available  Capacity";

    unsigned long int blocks = input_super_block->s_fsize;

    long int bused = count_bits(input_disk_image_fd, input_super_block, BLOCK_BITMAP);
    if (bused < 0) {
        fprintf(stderr, "ERROR: failed to retrieve bitmap count from block bitmap\n");
        return;
    }

    unsigned long int bavail = blocks - (unsigned long int)bused;
    double bcap = (double)bused / (double)blocks * 100;

    printf("===== BLOCK USAGE INFORMATION =====\n");
    printf("%s\n", banner);
    printf("%17lu %8lu %13lu %8.2f%%\n", blocks, bused, bavail, bcap);
    printf("\n");
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

    /* display superblock information */
    int ret_read_superblock = read_superblock(disk_image_fd, &sb);
    if (ret_read_superblock < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    print_superblock_info(&sb);

    /* display inode usage information */
    print_inode_usage_info(disk_image_fd, &sb);

    /* display block usage information */
    print_block_usage_info(disk_image_fd, &sb);

    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
