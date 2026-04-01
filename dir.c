#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "dir.h"
#include "fs_utils.h"
#include "superblock.h"

int32_t search_dir(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode, char *dir_name) {
    size_t dirent_entries = get_dirent_per_block();
    struct dirent dirent_entries_buffer[dirent_entries];

    /* check if input_inode has directory flag */
    if ((input_inode->i_mode & IFDIR) == 0) {
        return -ENOTDIR;
    }

    /* check each logical block */
    for (uint8_t logical_block = 0; logical_block < NDIRECT; ++logical_block) {
        /* if physical block is 0, bail out. should not allocate new block by bmap() */
        if (input_inode->i_addr[logical_block] == 0) {
            break;
        }

        /* get physical block number */
        uint32_t block_number = bmap(input_disk_image_fd, input_super_block, input_inode, logical_block);
        if (block_number == 0) {
            break;
        }

        /* read physical block and iterate each dirent entry */
        if (read_block(input_disk_image_fd, block_number, dirent_entries_buffer, sizeof(dirent_entries_buffer), 0) < 0) {
            return -EIO;
        }

        for (size_t i = 0; i < dirent_entries; ++i) {
            /* only check if the inode number is not equal to 0 */
            if (dirent_entries_buffer[i].inode_number != 0) {
                if (strncmp(dirent_entries_buffer[i].dir_name, dir_name, DIR_NAME_LENGTH) == 0) {
                    return (int32_t)dirent_entries_buffer[i].inode_number;
                }
            }
        }
    }

    /* not found, bail out */
    return 0;
}

struct inode *namei(int input_disk_image_fd, struct superblock *input_super_block, char *input_path_name) {
    /* check if input_path_name is valid */
    if (input_path_name == NULL || input_path_name[0] != '/') {
        return NULL;
    }

    /* copy path_name as strtok() is destructive */
    char *path_copy = strdup(input_path_name);
    if (path_copy == NULL) {
        return NULL;
    }

    /* start to search from ROOT_INODE */
    struct inode *current_inode = get_inode(input_disk_image_fd, input_super_block, ROOT_INODE);
    if (current_inode == NULL) {
        free(path_copy);

        return NULL;
    }

    /* parse path name(path_copy) */
    char *token = strtok(path_copy, "/");

    while (token != NULL) {
        if ((current_inode->i_mode & IFDIR) == 0) {
            put_inode(input_disk_image_fd, input_super_block, current_inode);
            free(path_copy);

            return NULL;
        }

        /* find child path inode number */
        int32_t child_inode_number = search_dir(input_disk_image_fd, input_super_block, current_inode, token);

        put_inode(input_disk_image_fd, input_super_block, current_inode);

        if (child_inode_number <= 0) {
            free(path_copy);

            return NULL;
        }

        /* found child and get child's in-core inode */
        current_inode = get_inode(input_disk_image_fd, input_super_block, (uint32_t)child_inode_number);
        if (current_inode == NULL) {
            free(path_copy);

            return NULL;
        }

        /* parse next part of path name(path_copy) */
        token = strtok(NULL, "/");
    }

    free(path_copy);

    return current_inode;
}

int add_entry(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_parent_inode, char *input_entry_name, uint32_t input_entry_inode) {
    uint32_t physical_block;
    size_t dirent_entries = get_dirent_per_block();
    struct dirent dirent_entries_buffer[dirent_entries];

    /* check entry name length */
    if (strlen(input_entry_name) >= DIR_NAME_LENGTH) {
        return -ENAMETOOLONG;
    }

    /* check if entry name exists already */
    int32_t found_dir_inode = search_dir(input_disk_image_fd, input_super_block, input_parent_inode, input_entry_name);

    if (found_dir_inode > 0) {
        return -EEXIST;
    }

    if (found_dir_inode < 0) {
        return -EIO;
    }

    /* scenario #1 - find available dirent from non-zero block */
    for (uint8_t logical_block = 0; logical_block < NDIRECT; ++logical_block) {
        physical_block = input_parent_inode->i_addr[logical_block];

        /* scan each physical block and check if available dirent exists */
        if (physical_block != 0) {
            /* read physical block and iterate each dirent entry */
            if (read_block(input_disk_image_fd, physical_block, dirent_entries_buffer, sizeof(dirent_entries_buffer), 0) < 0) {
                return -EIO;
            }

            /* read each dirent from physical block */
            for (size_t i = 0; i < dirent_entries; ++i) {
                /* only check if the inode number is equal to 0 */
                if (dirent_entries_buffer[i].inode_number == 0) {
                    /* fill out dirent information */
                    dirent_entries_buffer[i].inode_number = input_entry_inode;
                    strncpy(dirent_entries_buffer[i].dir_name, input_entry_name, DIR_NAME_LENGTH);

                    /* write back dirent entry to disk */
                    if (write_block(input_disk_image_fd, physical_block, &dirent_entries_buffer[i], sizeof(struct dirent), (off_t)i * (off_t)sizeof(struct dirent)) < 0) {
                        return -EIO;
                    }

                    /* increase parent directory space size */
                    size_t current_entry_postition = (logical_block * (dirent_entries * sizeof(struct dirent))) + (i + 1) * sizeof(struct dirent);
                    if (current_entry_postition > input_parent_inode->i_size0) {
                        input_parent_inode->i_size0 = (uint32_t)current_entry_postition;
                    }

                    /* mark parent inode as dirty */
                    input_parent_inode->i_dirty = DIRTY;

                    return 1;
                }
            }
        }
    }

    /* scenario #2 - allocate a new block and write dirent entry */
    for (uint8_t logical_block = 0; logical_block < NDIRECT; ++logical_block) {
        physical_block = input_parent_inode->i_addr[logical_block];

        if (physical_block == 0) {
            /* a new block needs to be allocated */
            int32_t new_physical_block = alloc_block(input_disk_image_fd, input_super_block);

            if (new_physical_block < 0) {
                return -EIO;
            }

            /* update new physical block in i_addr[] */
            input_parent_inode->i_addr[logical_block] = (uint32_t)new_physical_block;

            /* fill out dirent information */
            struct dirent new_dirent;
            memset(&new_dirent, 0, sizeof(struct dirent));

            new_dirent.inode_number = input_entry_inode;
            strncpy(new_dirent.dir_name, input_entry_name, DIR_NAME_LENGTH);

            /* write back dirent entry to disk */
            if (write_block(input_disk_image_fd, (uint32_t)new_physical_block, &new_dirent, sizeof(struct dirent), 0) < 0) {
                return -EIO;
            }

            /* increase parent directory space size */
            input_parent_inode->i_size0 = (uint32_t)(logical_block * (dirent_entries * sizeof(struct dirent))) + (uint32_t)sizeof(struct dirent);

            /* mark parent inode as dirty */
            input_parent_inode->i_dirty = DIRTY;

            return 1;
        }
    }

    return -ENOSPC;
}

int detach_entry(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_parent_inode, char *input_entry_name) {
    uint32_t physical_block;
    size_t dirent_entries = get_dirent_per_block();
    struct dirent dirent_entries_buffer[dirent_entries];

    /* check entry name length */
    if (strlen(input_entry_name) >= DIR_NAME_LENGTH) {
        return -ENAMETOOLONG;
    }

    /* "." and ".." entries should not be removed */
    if (strcmp(".", input_entry_name) == 0 || strcmp("..", input_entry_name) == 0) {
        return -EINVAL;
    }

    /* get entry inode number */
    int32_t found_dir_inode = search_dir(input_disk_image_fd, input_super_block, input_parent_inode, input_entry_name);

    if (found_dir_inode == 0) {
        return -ENOENT;
    }

    if (found_dir_inode < 0) {
        return -EIO;
    }

    /* search dirent from non-zero physical blocks */
    for (uint8_t logical_block = 0; logical_block < NDIRECT; ++logical_block) {
        physical_block = input_parent_inode->i_addr[logical_block];

        if (physical_block != 0) {
            /* read physical block and iterate each dirent entry */
            if (read_block(input_disk_image_fd, physical_block, dirent_entries_buffer, sizeof(dirent_entries_buffer), 0) < 0) {
                return -EIO;
            }

            /* read each dirent from physical block */
            for (size_t i = 0; i < dirent_entries; ++i) {
                /* should only match the exact entry name */
                if (dirent_entries_buffer[i].inode_number == (uint32_t)found_dir_inode && strncmp(dirent_entries_buffer[i].dir_name, input_entry_name, DIR_NAME_LENGTH) == 0) {
                    /* set dirent inode_number as 0 */
                    dirent_entries_buffer[i].inode_number = 0;

                    /* write back dirent entry to disk */
                    if (write_block(input_disk_image_fd, physical_block, &dirent_entries_buffer[i], sizeof(struct dirent), (off_t)i * (off_t)sizeof(struct dirent)) < 0) {
                        return -EIO;
                    }

                    /* mark parent inode as dirty */
                    input_parent_inode->i_dirty = DIRTY;

                    return 1;
                }
            }
        }
    }

    return -ENOENT;
}
