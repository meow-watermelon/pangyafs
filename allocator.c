#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bitmap.h"
#include "fs_utils.h"
#include "superblock.h"

int32_t alloc_block(int input_disk_image_fd, struct superblock *input_super_block) {
    unsigned char bitmap[BLOCKSIZE];

    /* configure block bitmap start + end blocks */
    uint32_t bitmap_start_block = 2 + input_super_block->s_inode_map_size;
    uint32_t bitmap_end_block = bitmap_start_block + input_super_block->s_block_map_size - 1;

    /* iterate each block bit from block bitmap */
    int ret_read_block;
    uint32_t block_number = 0;

    for (uint32_t bitmap_block_number = bitmap_start_block; bitmap_block_number <= bitmap_end_block; ++bitmap_block_number) {
        ret_read_block = read_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
        if (ret_read_block < 0) {
            fprintf(stderr, "ERROR: failed to read bitmap block %" PRIu32 "\n", bitmap_block_number);
            return -EIO;
        }

        /* read each byte from bitmap buffer */
        for (uint32_t byte_index = 0; byte_index < BLOCKSIZE; ++byte_index) {
            for (uint8_t bit_index = 0; bit_index < 8; ++bit_index) {
                /* if block_number is equal to or greater than total number of blocks, no need to find additional bit */
                if (block_number >= input_super_block->s_fsize) {
                    return -ENOSPC;
                }

                if ((bitmap[byte_index] & (unsigned char)(1 << (7 - bit_index))) == 0) {
                    /* if found block number is bad block, go back to loop */
                    if (badblock(input_super_block, block_number)) {
                        ++block_number;
                        continue;
                    }

                    /* set bit on block bitmap */
                    int ret_bitmap_set = bitmap_set(input_disk_image_fd, input_super_block, block_number, BLOCK_BITMAP);
                    if (ret_bitmap_set < 0) {
                        fprintf(stderr, "ERROR: failed to set bit on block bitmap for free block %" PRIu32 "\n", block_number);
                        return -EIO;
                    }

                    /* zero found free block */
                    int ret_zero_block = zero_block(input_disk_image_fd, block_number);
                    if (ret_zero_block < 0) {
                        fprintf(stderr, "ERROR: failed to zero free block %" PRIu32 "\n", block_number);
                        return -EIO;
                    }

                    return (int32_t)block_number;
                }

                ++block_number;
            }
        }
    }

    return -EIO;
}

int free_block(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_block_number) {
    /* if block number is bad block, bail out */
    if (badblock(input_super_block, input_block_number)) {
        return -EINVAL;
    }

    /* clear bit on block bitmap */
    int ret_bitmap_clear = bitmap_clear(input_disk_image_fd, input_super_block, input_block_number, BLOCK_BITMAP);
    if (ret_bitmap_clear < 0) {
        fprintf(stderr, "ERROR: failed to clear bit on block bitmap for free block %" PRIu32 "\n", input_block_number);
        return -EIO;
    }

    /* zero found free block */
    int ret_zero_block = zero_block(input_disk_image_fd, input_block_number);
    if (ret_zero_block < 0) {
        fprintf(stderr, "ERROR: failed to zero free block %" PRIu32 "\n", input_block_number);
        return -EIO;
    }

    return 1;
}

struct inode *get_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_inode_number) {
    /* check inode number range */
    if (input_inode_number == 0 || input_inode_number >= input_super_block->s_ninodes) {
        fprintf(stderr, "ERROR: input disk inode number is invalid\n");
        return NULL;
    }

    /* prepare disk inode + in-core inode */
    struct disk_inode dinode;
    memset(&dinode, 0, sizeof(struct disk_inode));

    struct inode *incore_inode = (struct inode *)malloc(sizeof(struct inode));
    if (incore_inode == NULL) {
        fprintf(stderr, "ERROR: failed to allocate in-core inode for disk inode number %" PRIu32 "\n", input_inode_number);
        goto error_handler;
    }

    /* read disk inode. disk inode should be marked as allocated */
    int ret_read_disk_inode = read_disk_inode(input_disk_image_fd, input_super_block, input_inode_number, &dinode);
    if (ret_read_disk_inode < 0) {
        fprintf(stderr, "ERROR: failed to read disk inode number %" PRIu32 "\n", input_inode_number);
        goto error_handler;
    }

    /* populate in-core inode struct */
    incore_inode->i_mode = dinode.i_mode;
    incore_inode->i_nlink = dinode.i_nlink;
    incore_inode->i_size0 = dinode.i_size0;
    memcpy(incore_inode->i_addr, dinode.i_addr, sizeof(dinode.i_addr));
    incore_inode->i_dirty = CLEAN;
    incore_inode->i_num = input_inode_number;

    return incore_inode;

error_handler:
    free(incore_inode);

    return NULL;
}

struct inode *alloc_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t mode) {
    unsigned char bitmap[BLOCKSIZE];
    struct inode *incore_inode;
    struct disk_inode dinode;

    /* configure inode bitmap start + end blocks */
    uint32_t bitmap_start_block = 2;
    uint32_t bitmap_end_block = bitmap_start_block + input_super_block->s_inode_map_size - 1;

    /* iterate each block bit from inode bitmap */
    int ret_read_block;
    uint32_t inode_number = 0;

    for (uint32_t bitmap_block_number = bitmap_start_block; bitmap_block_number <= bitmap_end_block; ++bitmap_block_number) {
        ret_read_block = read_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
        if (ret_read_block < 0) {
            fprintf(stderr, "ERROR: failed to read bitmap block %" PRIu32 "\n", bitmap_block_number);
            return NULL;
        }

        /* read each byte from bitmap buffer */
        for (uint32_t byte_index = 0; byte_index < BLOCKSIZE; ++byte_index) {
            for (uint8_t bit_index = 0; bit_index < 8; ++bit_index) {
                /* if inode_number is equal to or greater than total number of inodes, no need to find additional bit */
                if (inode_number >= input_super_block->s_ninodes) {
                    return NULL;
                }

                if ((bitmap[byte_index] & (unsigned char)(1 << (7 - bit_index))) == 0) {
                    /* if found inode number is in wrong range, go back to loop */
                    if (inode_number == 0 || inode_number >= input_super_block->s_ninodes) {
                        ++inode_number;
                        continue;
                    }

                    /* set bit on inode bitmap */
                    int ret_bitmap_set = bitmap_set(input_disk_image_fd, input_super_block, inode_number, INODE_BITMAP);
                    if (ret_bitmap_set < 0) {
                        fprintf(stderr, "ERROR: failed to set bit on block bitmap for free inode %" PRIu32 "\n", inode_number);
                        return NULL;
                    }

                    /* initialize allocated disk inode */
                    memset(&dinode, 0, sizeof(struct disk_inode));

                    dinode.i_mode = IALLOC | mode;
                    dinode.i_size0 = 0;
                    if ((mode & IFDIR) != 0) {
                        dinode.i_nlink = 2;
                    } else {
                        dinode.i_nlink = 1;
                    }

                    /* write back disk inode */
                    int ret_write_disk_inode = write_disk_inode(input_disk_image_fd, input_super_block, inode_number, &dinode);
                    if (ret_write_disk_inode < 0) {
                        fprintf(stderr, "ERROR: failed to write disk inode number %" PRIu32 "\n", inode_number);
                        return NULL;
                    }

                    /* get in-core inode */
                    incore_inode = get_inode(input_disk_image_fd, input_super_block, inode_number);
                    if (incore_inode == NULL) {
                        fprintf(stderr, "ERROR: failed to get in-core inode for inode number %" PRIu32 "\n", inode_number);
                        return NULL;
                    }

                    return incore_inode;
                }

                ++inode_number;
            }
        }
    }

    return NULL;
}

static void truncate_inode(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode) {
    /* free physical blocks in i_addr. double safe: free non-0 block only */
    for (int i = 0; i < NDIRECT; ++i) {
        if (input_inode->i_addr[i] != 0) {
            /* free physical blocks and clear block bitmap */
            free_block(input_disk_image_fd, input_super_block, input_inode->i_addr[i]);

            /* set each logical block to block 0 */
            input_inode->i_addr[i] = 0;
        }
    }

    input_inode->i_size0 = 0;

    /* need to write back to disk, mark inode as dirty */
    input_inode->i_dirty = DIRTY;
}

static int free_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_inode_number) {
    /* check inode number range */
    if (input_inode_number == 0 || input_inode_number >= input_super_block->s_ninodes) {
        fprintf(stderr, "ERROR: input disk inode number is invalid\n");
        return -EINVAL;
    }

    /* clear bit on inode bitmap */
    int ret_bitmap_clear = bitmap_clear(input_disk_image_fd, input_super_block, input_inode_number, INODE_BITMAP);
    if (ret_bitmap_clear < 0) {
        fprintf(stderr, "ERROR: failed to clear bit on inode bitmap for free inode %" PRIu32 "\n", input_inode_number);
        return -EIO;
    }

    return 1;
}

void update_inode(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode) {
    struct disk_inode dinode;
    memset(&dinode, 0, sizeof(dinode));

    dinode.i_mode = input_inode->i_mode;
    dinode.i_nlink = input_inode->i_nlink;
    dinode.i_size0 = input_inode->i_size0;
    memcpy(dinode.i_addr, input_inode->i_addr, sizeof(input_inode->i_addr));

    int ret_write_disk_inode = write_disk_inode(input_disk_image_fd, input_super_block, input_inode->i_num, &dinode);
    if (ret_write_disk_inode < 0) {
        fprintf(stderr, "ERROR: failed to update in-core inode %" PRIu32 "back to disk\n", input_inode->i_num);
    }
}

void put_inode(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode) {
    if (input_inode == NULL) {
        return;
    }

    if (input_inode->i_nlink <= 0) {
        truncate_inode(input_disk_image_fd, input_super_block, input_inode);

        input_inode->i_mode = 0;

        update_inode(input_disk_image_fd, input_super_block, input_inode);
        free_inode(input_disk_image_fd, input_super_block, input_inode->i_num);
    } else {
        if (input_inode->i_dirty == DIRTY) {
            update_inode(input_disk_image_fd, input_super_block, input_inode);
        }
    }

    free(input_inode);
}
