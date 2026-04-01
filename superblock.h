#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdint.h>
#include <time.h>

/* TODO: add why block size is 1024 bytes */
#define BLOCKSIZE 1024
#define BLOCK_BIT_SIZE (BLOCKSIZE * 8)

/* superblock magic number */
#define SUPERBLOCK_MAGIC_NUMBER 0x10102024U

struct superblock {
    uint32_t s_isize; /* size in blocks of inode list */
    uint32_t s_fsize; /* size in blocks of entire volume */
    uint32_t s_ninodes; /* total number of inodes */
    uint32_t s_inode_map_size; /* size in blocks of inode bitmap */
    uint32_t s_block_map_size; /* size in blocks of block bitmap */
    uint8_t s_fmod; /* super block modified flag */
    uint32_t s_magic; /* super block magic number */
    time_t s_time; /* current date of last update */
};

#endif /* SUPERBLOCK_H */
