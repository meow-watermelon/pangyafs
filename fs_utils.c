#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "allocator.h"
#include "dir.h"
#include "inode.h"
#include "superblock.h"

char *get_file_type_name(unsigned int mode) {
    if ((mode & IFREG) != 0) {
        return "REG";
    }

    if ((mode & IFDIR) != 0) {
        return "DIR";
    }

    return "UNK";
}

size_t get_inode_per_block(void) {
    size_t inode_unit_size = sizeof(struct disk_inode);

    return BLOCKSIZE / inode_unit_size;
}

size_t get_dirent_per_block(void) {
    size_t dirent_unit_size = sizeof(struct dirent);

    return BLOCKSIZE / dirent_unit_size;
}

size_t get_file_block_size(unsigned int file_size) {
    return (size_t)((file_size + BLOCKSIZE - 1) / BLOCKSIZE);
}

size_t get_inode_block_size(unsigned long int input_inode_size) {
    size_t inode_block_size = 0;
    size_t inode_per_block = get_inode_per_block();

    inode_block_size = input_inode_size / inode_per_block;
    if (input_inode_size % inode_per_block > 0) {
        ++inode_block_size;
    }

    return inode_block_size;
}

unsigned long int get_inode_bitmap_block_size(unsigned long int input_inode_size) {
    size_t inode_bitmap_block_size = 0;

    inode_bitmap_block_size = input_inode_size / BLOCK_BIT_SIZE;
    if (input_inode_size % BLOCK_BIT_SIZE > 0) {
        ++inode_bitmap_block_size;
    }

    return inode_bitmap_block_size;
}

unsigned long int get_block_bitmap_block_size(unsigned long int input_image_size) {
    size_t block_bitmap_block_size = 0;

    block_bitmap_block_size = input_image_size / BLOCK_BIT_SIZE;
    if (input_image_size % BLOCK_BIT_SIZE > 0) {
        ++block_bitmap_block_size;
    }

    return block_bitmap_block_size;
}

unsigned short int badblock(struct superblock *input_super_block, unsigned long int input_block_number) {
    /* block_number should not be within (block #0 + block #1(super block) + (inode + block) bitmap blocks + inode block(s)) or over total number of blocks */
    if (input_block_number < 2 + input_super_block->s_inode_map_size + input_super_block->s_block_map_size + input_super_block->s_isize || input_block_number >= input_super_block->s_fsize) {
        return 1;
    }

    return 0;
}

int read_block(int input_disk_image_fd, unsigned long int block_number, void *data, size_t data_size, off_t block_offset) {
    memset(data, 0, data_size);
    off_t final_offset = (off_t)(block_number * BLOCKSIZE) + block_offset;
    ssize_t ret_pread = pread(input_disk_image_fd, data, data_size, final_offset);

    if (ret_pread < 0) {
        fprintf(stderr, "ERROR: failed to read data from block #%lu with data size %lu at offset %lu: %s\n", block_number, data_size, (unsigned long int)final_offset, strerror(errno));
        goto error_handler;
    }

    if ((unsigned long int)ret_pread != data_size) {
        fprintf(stderr, "ERROR: failed to read data from block #%lu with data size %lu at offset %lu: %lu byte(s) returned\n", block_number, data_size, (unsigned long int)final_offset, (unsigned long int)ret_pread);
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int write_block(int input_disk_image_fd, unsigned long int block_number, void *data, size_t data_size, off_t block_offset) {
    off_t final_offset = (off_t)(block_number * BLOCKSIZE) + block_offset;
    ssize_t ret_pwrite = pwrite(input_disk_image_fd, data, data_size, final_offset);

    if (ret_pwrite < 0) {
        fprintf(stderr, "ERROR: failed to write data into block #%lu with data size %lu at offset %lu: %s\n", block_number, data_size, (unsigned long int)final_offset, strerror(errno));
        goto error_handler;
    }

    if ((unsigned long int)ret_pwrite != data_size) {
        fprintf(stderr, "ERROR: failed to write data into block #%lu with data size %lu at offset %lu: %lu byte(s) returned\n", block_number, data_size, (unsigned long int)final_offset, (unsigned long int)ret_pwrite);
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int zero_block(int input_disk_image_fd, unsigned long int block_number) {
    unsigned char zero[BLOCKSIZE];
    memset(zero, 0, BLOCKSIZE);

    int ret_write_block = write_block(input_disk_image_fd, block_number, zero, BLOCKSIZE, 0);
    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write zero block on block number %lu\n", block_number);
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int read_superblock(int input_disk_image_fd, struct superblock *input_super_block_buffer) {
    memset(input_super_block_buffer, 0, sizeof(struct superblock));

    int ret_read_superblock = read_block(input_disk_image_fd, 1, input_super_block_buffer, sizeof(struct superblock), 0);
    if (ret_read_superblock < 0) {
        fprintf(stderr, "ERROR: failed to read superblock\n");
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int write_superblock(int input_disk_image_fd, struct superblock *input_super_block) {
    /* update superblock modification flag and timestamp */
    input_super_block->s_fmod = 0;
    input_super_block->s_time = time(NULL);

    int ret_write_superblock = write_block(input_disk_image_fd, 1, input_super_block, sizeof(struct superblock), 0);
    if (ret_write_superblock < 0) {
        fprintf(stderr, "ERROR: failed to write superblock\n");
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int read_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, unsigned long int input_disk_inode_number, struct disk_inode *input_disk_inode_buffer) {
    /* if input disk inode number is invalid, bail out */
    if (input_disk_inode_number == 0 || input_disk_inode_number >= input_super_block->s_ninodes) {
        fprintf(stderr, "ERROR: input disk inode number is invalid\n");
        goto error_handler;
    }

    unsigned long int disk_inode_block = 2 + input_super_block->s_inode_map_size + input_super_block->s_block_map_size;

    /* calculate disk inode block number + disk inode block offset */
    size_t disk_inode_per_block = get_inode_per_block();
    unsigned long int disk_inode_block_number = input_disk_inode_number / disk_inode_per_block;
    unsigned long int disk_inode_block_offset = input_disk_inode_number % disk_inode_per_block;

    disk_inode_block += disk_inode_block_number;

    /* read disk inode */
    int ret_read_block = read_block(input_disk_image_fd, disk_inode_block, input_disk_inode_buffer, sizeof(struct disk_inode), (off_t)disk_inode_block_offset * (off_t)sizeof(struct disk_inode));

    if (ret_read_block < 0) {
        fprintf(stderr, "ERROR: failed to read disk inode block %lu\n", disk_inode_block);
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

int write_disk_inode(int input_disk_image_fd, struct superblock *input_super_block, unsigned long int input_disk_inode_number, struct disk_inode *input_disk_inode) {
    /* if input disk inode number is invalid, bail out */
    if (input_disk_inode_number == 0 || input_disk_inode_number >= input_super_block->s_ninodes) {
        fprintf(stderr, "ERROR: input disk inode number is invalid\n");
        goto error_handler;
    }

    unsigned long int disk_inode_block = 2 + input_super_block->s_inode_map_size + input_super_block->s_block_map_size;

    /* calculate disk inode block number + disk inode block offset */
    size_t disk_inode_per_block = get_inode_per_block();
    unsigned long int disk_inode_block_number = input_disk_inode_number / disk_inode_per_block;
    unsigned long int disk_inode_block_offset = input_disk_inode_number % disk_inode_per_block;

    disk_inode_block += disk_inode_block_number;

    /* write disk inode */
    int ret_write_block = write_block(input_disk_image_fd, disk_inode_block, input_disk_inode, sizeof(struct disk_inode), (off_t)disk_inode_block_offset * (off_t)sizeof(struct disk_inode));

    if (ret_write_block < 0) {
        fprintf(stderr, "ERROR: failed to write disk inode block %lu\n", disk_inode_block);
        goto error_handler;
    }

    return 1;

error_handler:
    return -EIO;
}

unsigned long int bmap(int input_disk_image_fd, struct superblock *input_super_block, struct inode *input_inode, unsigned long int input_logical_block_number) {
    long int block_number;

    /* return block number 0 if requested logical block number >= NDIRECT */
    if (input_logical_block_number >= NDIRECT) {
        fprintf(stderr, "ERROR: logical block number %lu is invalid\n", input_logical_block_number);
        return 0;
    }

    /* cast i_addr[input_logical_block_number] to long int to match type of block_number */
    block_number = (long int)input_inode->i_addr[input_logical_block_number];

    /* if physical block number in logical block number is 0, allocate a new block; return block number 0 if allocation is failed */
    if (block_number == 0) {
        block_number = alloc_block(input_disk_image_fd, input_super_block);

        if (block_number < 0) {
            return 0;
        }

        input_inode->i_addr[input_logical_block_number] = (unsigned long int)block_number;
        input_inode->i_dirty = DIRTY;
    }

    return (unsigned long int)block_number;
}
