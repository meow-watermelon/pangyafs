#ifndef INODE_H
#define INODE_H

/*
 * TODO: add indirect block support later
 * max file size == BLOCKSIZE * NDIRECT == 1024 * 16 == 16 KB
*/
#define NDIRECT 16

/* root inode number starts from 1; inode number 0 is reserved for unused */
#define ROOT_INODE 1

/* file mode */
#define IALLOC 0x0001 /* inode is in use */
#define IFREG 0x0002 /* regular file */
#define IFDIR 0x0004 /* directory */

/* in-core inode dirtiness */
#define CLEAN 0
#define DIRTY 1

/* disk inode struct */
struct disk_inode {
    unsigned int i_mode;
    unsigned int i_nlink;
    unsigned int i_size0;
    unsigned long int i_addr[NDIRECT];
};

/* in-core inode struct */
struct inode {
    unsigned int i_mode;
    unsigned int i_nlink;
    unsigned int i_size0;
    unsigned int i_dirty;
    unsigned long int i_addr[NDIRECT];
    unsigned long int i_num;
};

#endif /* INODE_H */
