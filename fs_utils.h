#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include "inode.h"
#include "superblock.h"

extern char *get_file_type_name(uint32_t mode);
extern int badblock(struct superblock *input_super_block, uint32_t input_block_number);
extern int read_block(int input_disk_image_fd, uint32_t block_number, void *data, size_t data_size, off_t block_offset);
extern int write_block(int input_disk_image_fd, uint32_t block_number, void *data, size_t data_size, off_t block_offset);
extern int zero_block(int input_disk_image_fd, uint32_t block_number);
extern int read_superblock(int input_disk_image_fd, struct superblock *input_super_block_buffer);
extern int write_superblock(int input_disk_image_fd, struct superblock *input_super_block);
extern size_t get_inode_per_block(void);
extern size_t get_dirent_per_block(void);
extern size_t get_file_block_size(uint32_t file_size);
extern size_t get_inode_block_size(uint32_t input_inode_size);
extern uint32_t get_inode_bitmap_block_size(uint32_t input_inode_size);
extern uint32_t get_block_bitmap_block_size(uint32_t input_image_size);
extern int read_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_disk_inode_number, struct disk_inode *input_disk_inode_buffer);
extern int write_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, uint32_t input_disk_inode_number, struct disk_inode *input_disk_inode);
extern uint32_t bmap(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode, uint32_t input_logical_block_number);

#endif /* FS_UTILS_H */
