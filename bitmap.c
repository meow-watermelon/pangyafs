#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include "bitmap.h"
#include "fs_utils.h"
#include "superblock.h"

void set_bit(unsigned char *bitmap, uint32_t index, uint8_t bit_position) {
    bitmap[index] |= (unsigned char)(1 << bit_position);
}

void clear_bit(unsigned char *bitmap, uint32_t index, uint8_t bit_position) {
    bitmap[index] &= (unsigned char)~(1 << bit_position);
}

int bitmap_set(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_number, int bitmap_type) {
    uint32_t bitmap_start_block;
    unsigned char bitmap[BLOCKSIZE];

    /* configure bitmap start block based on bitmap type */
    if (bitmap_type == INODE_BITMAP) {
        bitmap_start_block = 2;

        /* if input inode number is greater than total number of inodes, bail out */
        if (input_number >= input_super_block->s_ninodes) {
            fprintf(stderr, "ERROR: input inode number %" PRIu32 " is greater than number of total inodes %" PRIu32 "\n", input_number, input_super_block->s_ninodes);
            return -EINVAL;
        }
    } else {
        bitmap_start_block = 2 + input_super_block->s_inode_map_size;
    }

    /* calculate block index where target block should be */
    uint32_t bitmap_block_number = bitmap_start_block + input_number / BLOCK_BIT_SIZE;

    /* calculate byte index position in target block */
    uint32_t byte_index = input_number % BLOCK_BIT_SIZE / 8;
    uint8_t bit_index = 7 - (uint8_t)(input_number % BLOCK_BIT_SIZE % 8);

    /* read target inode block into bitmap buffer */
    int ret_read_block = read_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
    if (ret_read_block < 0) {
        fprintf(stderr, "ERROR: failed to read target block %" PRIu32 "\n", bitmap_block_number);
        return -EIO;
    }

    /* set allocation bit at byte index on target block */
    set_bit(bitmap, byte_index, bit_index);

    /* write back block */
    int ret_write_block = write_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write target block %" PRIu32 "\n", bitmap_block_number);
        return -EIO;
    }

    return 1;
}

int bitmap_clear(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_number, int bitmap_type) {
    uint32_t bitmap_start_block;
    unsigned char bitmap[BLOCKSIZE];

    /* configure bitmap start block based on bitmap type */
    if (bitmap_type == INODE_BITMAP) {
        bitmap_start_block = 2;

        /* if input inode number is greater than total number of inodes, bail out */
        if (input_number >= input_super_block->s_ninodes) {
            fprintf(stderr, "ERROR: input inode number %" PRIu32 " is greater than number of total inodes %" PRIu32 "\n", input_number, input_super_block->s_ninodes);
            return -EINVAL;
        }
    } else {
        bitmap_start_block = 2 + input_super_block->s_inode_map_size;
    }

    /* calculate block index where target block should be */
    uint32_t bitmap_block_number = bitmap_start_block + input_number / BLOCK_BIT_SIZE;

    /* calculate byte index position in target block */
    uint32_t byte_index = input_number % BLOCK_BIT_SIZE / 8;
    uint8_t bit_index = 7 - (uint8_t)(input_number % BLOCK_BIT_SIZE % 8);

    /* read target inode block into bitmap buffer */
    int ret_read_block = read_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
    if (ret_read_block < 0) {
        fprintf(stderr, "ERROR: failed to read target block %" PRIu32 "\n", bitmap_block_number);
        return -EIO;
    }

    /* set allocation bit at byte index on target block */
    clear_bit(bitmap, byte_index, bit_index);

    /* write back block */
    int ret_write_block = write_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write target block %" PRIu32 "\n", bitmap_block_number);
        return -EIO;
    }

    return 1;
}

int32_t count_bits(int input_disk_image_fd, struct superblock *input_super_block, int bitmap_type) {
    uint32_t bitmap_start_block;
    uint32_t bitmap_last_block;
    unsigned char bitmap[BLOCKSIZE];
    int32_t bit_count = 0;

    /* configure bitmap start block based on bitmap type */
    if (bitmap_type == INODE_BITMAP) {
        bitmap_start_block = 2;
        bitmap_last_block = bitmap_start_block + input_super_block->s_inode_map_size;
    } else {
        bitmap_start_block = 2 + input_super_block->s_inode_map_size;
        bitmap_last_block = bitmap_start_block + input_super_block->s_block_map_size;
    }

    /*
     * read each bitmap block then test bit in each byte. no need to check exact number of bits for each bitmap because bitmap block is 
     * pre-allocated zero before set bit, read each byte is cheap and simple
    */

    int ret_read_block;

    for (uint32_t bitmap_block_number = bitmap_start_block; bitmap_block_number < bitmap_last_block; ++bitmap_block_number) {
        /* read target inode block into bitmap buffer */
        ret_read_block = read_block(input_disk_image_fd, bitmap_block_number, bitmap, BLOCKSIZE, 0);
        if (ret_read_block < 0) {
            fprintf(stderr, "ERROR: failed to read target block %" PRIu32 "\n", bitmap_block_number);
            return -EIO;
        }

        /* read each byte from bitmap buffer to test bit */
        for (uint32_t byte_index = 0; byte_index < BLOCKSIZE; ++byte_index) {
            for (uint8_t bit_index = 0; bit_index < 8; ++bit_index) {
                if ((bitmap[byte_index] & (unsigned char)(1 << bit_index)) != 0) {
                    ++bit_count;
                }
            }
        }
    }

    return bit_count;
}
