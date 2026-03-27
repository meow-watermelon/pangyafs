#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <time.h>

/* TODO: add why block size is 1024 bytes */
#define BLOCKSIZE 1024
#define BLOCK_BIT_SIZE (BLOCKSIZE * 8)

struct superblock {
    unsigned long int s_isize; /* size in blocks of inode list */
    unsigned long int s_fsize; /* size in blocks of entire volume */
    unsigned long int s_ninodes; /* total number of inodes */
    unsigned long int s_inode_map_size; /* size in blocks of inode bitmap */
    unsigned long int s_block_map_size; /* size in blocks of block bitmap */
    unsigned int s_fmod; /* super block modified flag */
    time_t s_time; /* current date of last update */
};

#endif /* SUPERBLOCK_H */
