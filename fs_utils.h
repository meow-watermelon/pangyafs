#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <sys/types.h>
#include "inode.h"
#include "superblock.h"

extern char *get_file_type_name(unsigned int mode);
extern unsigned short int badblock(struct superblock *input_super_block, unsigned long int input_block_number);
extern int read_block(int input_disk_image_fd, unsigned long int block_number, void *data, size_t data_size, off_t block_offset);
extern int write_block(int input_disk_image_fd, unsigned long int block_number, void *data, size_t data_size, off_t block_offset);
extern int zero_block(int input_disk_image_fd, unsigned long int block_number);
extern int read_superblock(int input_disk_image_fd, struct superblock *input_super_block_buffer);
extern int write_superblock(int input_disk_image_fd, struct superblock *input_super_block);
extern size_t get_inode_per_block(void);
extern size_t get_dirent_per_block(void);
extern size_t get_file_block_size(unsigned int file_size);
extern size_t get_inode_block_size(unsigned long int input_inode_size);
extern unsigned long int get_inode_bitmap_block_size(unsigned long int input_inode_size);
extern unsigned long int get_block_bitmap_block_size(unsigned long int input_image_size);
extern int read_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, unsigned long int input_disk_inode_number, struct disk_inode *input_disk_inode_buffer);
extern int write_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, unsigned long int input_disk_inode_number, struct disk_inode *input_disk_inode);
extern unsigned long int bmap(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode, unsigned long int input_logical_block_number);

#endif /* FS_UTILS_H */
