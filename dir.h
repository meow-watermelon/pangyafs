#ifndef DIR_H
#define DIR_H

#include <stdint.h>
#include "inode.h"
#include "superblock.h"

#define DIR_NAME_LENGTH 14

struct dirent {
    uint32_t inode_number;
    char dir_name[DIR_NAME_LENGTH];
};

extern int32_t search_dir(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode, char *dir_name);
extern struct inode *namei(int input_disk_image_fd, struct superblock *input_super_block, char *input_path_name);
extern int add_entry(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_parent_inode, char *input_entry_name, uint32_t input_entry_inode);
extern int detach_entry(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_parent_inode, char *input_entry_name);

#endif /* DIR_H */
