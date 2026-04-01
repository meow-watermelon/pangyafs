#ifndef BITMAP_H
#define BITMAP_H

#include "superblock.h"

#define INODE_BITMAP 0
#define BLOCK_BITMAP 1

extern void set_bit(unsigned char *bitmap, uint32_t index, uint8_t bit_position);
extern void clear_bit(unsigned char *bitmap, uint32_t index, uint8_t bit_position);
extern int bitmap_set(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_number, int bitmap_type);
extern int bitmap_clear(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_number, int bitmap_type);
extern int32_t count_bits(int input_disk_image_fd, struct superblock *input_super_block, int bitmap_type);

#endif /* BITMAP_H */
