#ifndef INODE_H
#define INODE_H

#include <stdint.h>

/*
 * TODO: add indirect block support later
 * max file size == BLOCKSIZE * NDIRECT == 1024 * 16 == 16 KB
*/
#define NDIRECT 16

/* root inode number starts from 1; inode number 0 is reserved for unused */
#define ROOT_INODE 1

/* file mode */
#define IALLOC 0x00000001U /* inode is in use */
#define IFREG 0x00000002U /* regular file */
#define IFDIR 0x00000004U /* directory */

/* in-core inode dirtiness */
#define CLEAN 0
#define DIRTY 1

/* disk inode struct */
struct disk_inode {
    uint32_t i_mode;
    uint16_t i_nlink;
    uint32_t i_size0;
    uint32_t i_addr[NDIRECT];
};

/* in-core inode struct */
struct inode {
    uint32_t i_mode;
    uint16_t i_nlink;
    uint32_t i_size0;
    uint8_t i_dirty;
    uint32_t i_addr[NDIRECT];
    uint32_t i_num;
};

#endif /* INODE_H */
