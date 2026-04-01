#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>
#include "superblock.h"

extern int32_t alloc_block(int input_disk_image_fd, struct superblock *input_super_block);
extern int free_block(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_block_number);
extern struct inode *get_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_inode_number);
extern struct inode *alloc_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t mode);
extern void update_inode(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode);
extern void put_inode(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode);

#endif /* ALLOCATOR_H */
