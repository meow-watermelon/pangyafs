#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bitmap.h"
#include "dir.h"
#include "fs_utils.h"
#include "inode.h"
#include "superblock.h"

#define VERSION "0.0.1"
#define DISK_IMAGE_FILENAME "pangyafs.img"
#define DISK_IMAGE_FILE_MODE 0644

/* command line options */
static int opt_flag_i = 0;
static int opt_flag_s = 0;

/* define usage function */
static void usage(void) {
    printf(
        "Make PangYa Filesystem - Version %s\n"
        "usage: mkfs.pangya -i|--inode <number of inodes>\n"
        "                   -s|--size <disk image size in KB>\n"
        "                  [-h|--help]\n", VERSION
    );
}

static int init_image(int input_disk_image_fd, unsigned long int input_image_size) {
    char zero_block[BLOCKSIZE] = {0};

    /* write zero blocks. input_image_size == number of blocks need to be written */
    for (size_t i = 0; i < input_image_size; ++i) {
        int ret_write_block = write_block(input_disk_image_fd, i, zero_block, sizeof(zero_block), 0);
        if (ret_write_block < 0) {
            fprintf(stderr, "ERROR: failed to zero disk image file %s in %lu-th block\n", DISK_IMAGE_FILENAME, i);
            goto error_handler;
        }
    }

    return 1;

error_handler:
    return -1;
}

static void print_init_image_info(unsigned long int input_image_size) {
    printf("===== INITIALIZE DISK IMAGE =====\n");
    printf("disk image filename: %s\n", DISK_IMAGE_FILENAME);
    printf("disk image file size: %lu KB\n", input_image_size);
    printf("\n");
}

static int init_inode(int input_disk_image_fd, unsigned long int block_number, struct disk_inode *input_disk_inode, unsigned long int input_inode_size, unsigned long int input_inode_per_block) {
    size_t input_inode_block_size = 0;
    size_t input_inode_block_leftover_size = 0;

    input_inode_block_size = input_inode_size / input_inode_per_block;
    input_inode_block_leftover_size = input_inode_size % input_inode_per_block;

    for (size_t i = block_number; i < input_inode_block_size + block_number; ++i) {
        for (size_t j = 0; j < input_inode_per_block; ++j) {
            off_t block_offset = (off_t)sizeof(struct disk_inode) * (off_t)j;

            int ret_write_block = write_block(input_disk_image_fd, i, input_disk_inode, sizeof(struct disk_inode), block_offset);
            if (ret_write_block < 0) {
                fprintf(stderr, "ERROR: failed to write inode data on %lu-th item at %lu-th disk block\n", j, i);
                goto error_handler;
            }
        }
    }

    /* if input_inode_block_leftover_size is greater than 0, we need to write another block for inode struct leftover */
    if (input_inode_block_leftover_size > 0) {
        for (size_t j = 0; j < input_inode_block_leftover_size; ++j) {
            off_t block_offset = (off_t)sizeof(struct disk_inode) * (off_t)j;
            int ret_write_block = write_block(input_disk_image_fd, block_number + input_inode_block_size, input_disk_inode, sizeof(struct disk_inode), block_offset);
            if (ret_write_block < 0) {
                fprintf(stderr, "ERROR: failed to write inode data on %lu-th item at %lu-th disk block\n", j, block_number + input_inode_block_size);
                goto error_handler;
            }
        }
    }

    return 1;

error_handler:
    return -1;
}

static void print_init_fs_info(struct superblock *input_super_block) {
    printf("===== INITIALIZE FILESYSTEM =====\n");
    printf("block size: %d B\n", BLOCKSIZE);
    printf("superblock size: %lu B\n", sizeof(struct superblock));
    printf("inode unit size: %lu B\n", sizeof(struct disk_inode));
    printf("directory entry unit size: %lu B\n", sizeof(struct dirent));
    printf("filesystem size in block: %lu\n", input_super_block->s_fsize);
    printf("number of inode bitmap blocks: %lu\n", input_super_block->s_inode_map_size);
    printf("number of block bitmap blocks: %lu\n", input_super_block->s_block_map_size);
    printf("number of allocated inodes: %lu \n", input_super_block->s_ninodes);
    printf("number of inodes per block: %lu\n", get_inode_per_block());
    printf("number of blocks for allocated inodes: %lu\n", input_super_block->s_isize);
    printf("\n");
}

int main(int argc, char *argv[]) {
    /* define command line options */
    char *short_opts = "i:s:h";
    struct option long_opts[] = {
        {"inode", required_argument, NULL, 'i'},
        {"size", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    unsigned long int inode_size;
    unsigned long int image_size;

    int disk_image_fd = -1;
    int ret_write_block;
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
            case 'i':
                errno = 0;
                inode_size = strtoul(optarg, NULL, 10);

                if (errno != 0) {
                    fprintf(stderr, "ERROR: failed to convert inode size value\n\n");
                    exit(EXIT_FAILURE);
                }

                if (inode_size <= 0) {
                    fprintf(stderr, "ERROR: inode size must be an integer and greater than 0\n\n");
                    usage();
                    exit(EXIT_FAILURE);
                }

                opt_flag_i = 1;

                break;
            case 's':
                errno = 0;
                image_size = strtoul(optarg, NULL, 10);

                if (errno != 0) {
                    fprintf(stderr, "ERROR: failed to convert image size value\n\n");
                    exit(EXIT_FAILURE);
                }

                if (image_size <= 0) {
                    fprintf(stderr, "ERROR: image size must be an integer and greater than 0\n\n");
                    usage();
                    exit(EXIT_FAILURE);
                }

                opt_flag_s = 1;

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
    if (!opt_flag_i || !opt_flag_s) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* open disk image file */
    disk_image_fd = open(DISK_IMAGE_FILENAME, O_CREAT | O_TRUNC | O_RDWR, DISK_IMAGE_FILE_MODE);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", DISK_IMAGE_FILENAME, strerror(errno));
        goto error_handler;
    }

    /* initialize and zero disk image */
    print_init_image_info(image_size);

    int ret_init_image = init_image(disk_image_fd, image_size);
    if (ret_init_image < 0) {
        fprintf(stderr, "ERROR: failed to initialized PangYa filesystem image %s with size %lu KB\n", DISK_IMAGE_FILENAME, image_size);
        goto error_handler;
    }

    /*
     * initialize PangYa filesystem
     * 
     * step #1: initialize and write superblock in block #1
     * step #2: allocate inode + block bitmap blocks
     * step #3: bootstrap inode blocks based on input inode_size
     * step #4: create "." and ".." directory items for root directory in 1st data block
     * step #5: update inode #1 with root directory information
     * step #6: update block + inode bitmaps
     *
    */

    /* initialize superblock */
    struct superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.s_fsize = image_size;
    sb.s_isize = get_inode_block_size(inode_size);
    sb.s_ninodes = inode_size;
    sb.s_inode_map_size = get_inode_bitmap_block_size(inode_size);
    sb.s_block_map_size = get_block_bitmap_block_size(image_size);

    /* if block#0 + block #1 + sb.s_inode_map_size + sb.s_block_map_size + sb.s_isize >= total number of blocks, bail out */
    if (2 + sb.s_inode_map_size + sb.s_block_map_size + sb.s_isize >= sb.s_fsize) {
        fprintf(stderr, "ERROR: insufficient blocks to allocate inode blocks\n");
        goto error_handler;
    }

    /* write superblock data in block #1 */
    ret_write_superblock = write_superblock(disk_image_fd, &sb);
    if (ret_write_superblock < 0) {
        fprintf(stderr, "ERROR: failed to write superblock in mkfs procedure\n");
        goto error_handler;
    }

    print_init_fs_info(&sb);

    /* initialize disk inode */
    struct disk_inode dinode;
    memset(&dinode, 0, sizeof(dinode));

    /* write inode data from block after block #0 + block #1 + inode bitmap block(s) + block bitmap block(s) */
    int ret_init_inode = init_inode(disk_image_fd, 2 + sb.s_inode_map_size + sb.s_block_map_size, &dinode, inode_size, get_inode_per_block());
    if (ret_init_inode < 0) {
        fprintf(stderr, "ERROR: failed to write inode data\n");
        goto error_handler;
    }

    /* write "." and ".." for root directory on 1st data block */
    struct dirent root_dir[2];
    memset(root_dir, 0, sizeof(root_dir));

    root_dir[0].inode_number = ROOT_INODE;
    strcpy(root_dir[0].dir_name, ".");
    root_dir[1].inode_number = ROOT_INODE;
    strcpy(root_dir[1].dir_name, "..");

    /* calculate 1st data block location */
    unsigned long int data_block_start = 2 + sb.s_inode_map_size + sb.s_block_map_size + sb.s_isize;

    /* write root_dir in 1st data block */
    ret_write_block = write_block(disk_image_fd, data_block_start, root_dir, sizeof(root_dir), 0);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write root directory data\n");
        goto error_handler;
    }

    /* update ROOT_INODE to point "." and ".." */
    struct disk_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));

    root_inode.i_mode = IALLOC | IFDIR;
    root_inode.i_nlink = 2;
    root_inode.i_size0 = 2 * sizeof(struct dirent);
    root_inode.i_addr[0] = data_block_start;

    /* write root_inode in inode #1 */
    ret_write_block = write_block(disk_image_fd, 2 + sb.s_inode_map_size + sb.s_block_map_size, &root_inode, sizeof(root_inode), (off_t)sizeof(struct disk_inode) * (off_t)ROOT_INODE);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write root inode data\n");
        goto error_handler;
    }

    /* update inode bitmap */
    printf("===== WRITE INODE BITMAP =====\n");
    int ret_inode_bitmap_set;

    /* inode 0 is reserved and is marked as allocated in inode bitmap */
    for (unsigned long int inode_index = 0; inode_index <= ROOT_INODE; ++inode_index) {
        ret_inode_bitmap_set = bitmap_set(disk_image_fd, &sb, inode_index, INODE_BITMAP);
        if (ret_inode_bitmap_set < 0) {
            fprintf(stderr, "ERROR: failed to write inode bitmap on inode %lu\n", inode_index);
            goto error_handler;
        }
    }
    printf("\n");

    /* update block bitmap */
    printf("===== WRITE BLOCK BITMAP =====\n");
    int ret_block_bitmap_set;

    /* block 0 is reserved and is marked as allocated in block bitmap */
    for (unsigned long int block_index = 0; block_index <= data_block_start; ++block_index) {
        ret_block_bitmap_set = bitmap_set(disk_image_fd, &sb, block_index, BLOCK_BITMAP);
        if (ret_block_bitmap_set < 0) {
            fprintf(stderr, "ERROR: failed to write block bitmap on block %lu\n", block_index);
            goto error_handler;
        }
    }
    printf("\n");

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
