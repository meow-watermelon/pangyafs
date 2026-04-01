# PangYa Filesystem
## Introduction
**PangYa FS** is a minimalist, user-space filesystem implementation inspired by the classic **UNIX V6** architecture. Developed as a self-educational project, it implements the core logic of a persistent, block-based storage engine. 

PangYa FS manages raw binary disk images through a structured geometry of superblocks, inode tables, and bitmaps. It features a complete suite of user-space utilities to manipulate the filesystem without requiring kernel-level mounting.

> **Note on Development:** Large Language Models (Gemini/ChatGPT) were utilized strictly for unit-test automation and debugging assistance. The filesystem architecture and core logic are original implementations.

## Filesystem Specification
-   Block Size: 1024 bytes
-   Max Filename: 13 Characters (+ 1 NUL terminator)
-   Max File Size: 16 KB (Direct Mapping: 16 blocks)

## Filesystem Block Layout
```
▲  [ Block 0 ]  - Boot / Reserved Block
│               - Usually empty; reserved for bootloaders or system markers
├───────────────┤
│  [ Block 1 ]  - Superblock
│               - Contains s_isize, s_fsize, s_ninodes
│               - Stores timestamps and filesystem state flags
├───────────────┤
│  [ Block 2 ]  - Inode Bitmap
│  [   ...   ]  - (Size depends on s_inode_map_size)
│               - 1 bit per Inode (0 = Free, 1 = Used)
├───────────────┤
│  [ Block X ]  - Data Block Bitmap
│  [   ...   ]  - (Size depends on s_block_map_size)
│               - 1 bit per Data Block (0 = Free, 1 = Used)
├───────────────┤
│  [ Block Y ]  - Inode Table (Inode List)
│  [   ...   ]  - (Size depends on s_isize)
│               - Contains 'disk_inode' structures
│               - Includes ROOT_INODE at the beginning of the list
├───────────────┤
│  [ Block Z ]  - Data Blocks Area
│  [   ...   ]  - (Occupies the remainder of s_fsize)
│               - Stores File Contents
│               - Stores Directory Entries (dirent)
▼  [  End    ]  - End of Virtual Disk Image
```

## Known Limitations
As an educational project focusing on core fundamentals, PangYa FS currently operates under the following constraints:

-   Designed for a single-threaded environment
-   No indirect block support. Only 16 direct blocks layout is supported
-   No reference count item in inode structures
-   No POSIX MAC(modify / access / change) timestamps in inode structures
-   No UNIX VFS / FUSE interface

## User-space Tools
| Command | Description |
|--|--|
| `mkfs.pangya` | Formats a new disk image with custom inode and size parameters |
| `df.pangya` | Reports filesystem usage |
| `stat.pangya` | Displays detailed inode-level metadata for any path |
| `ls.pangya` | Lists directory contents with metadata (Inode, Link Count, Size, Type) |
| `mkdir.pangya` | Creates directory |
| `rm.pangya` | Removes files/directories |
| `cp.pangya` | Copies files from the Host OS into a PangYa FS directory |

## Get Started

 - **Build PangYa FS user-space utilities**
```
$ make
```
 - **Create a 500 KB filesystem with 100 inodes**
```
$ ./mkfs.pangya -s 500 -i 100
===== INITIALIZE DISK IMAGE =====
disk image filename: pangyafs.img
disk image file size: 500 KB

===== INITIALIZE FILESYSTEM =====
block size: 1024 B
superblock size: 40 B
inode unit size: 76 B
directory entry unit size: 20 B
filesystem size in block: 500
number of inode bitmap blocks: 1
number of block bitmap blocks: 1
number of allocated inodes: 100
number of inodes per block: 13
number of blocks for allocated inodes: 8

===== WRITE INODE BITMAP =====

===== WRITE BLOCK BITMAP =====

```
 - **Basic Operations**
```
# check disk space usage
$ ./df.pangya -f pangyafs.img
===== SUPERBLOCK INFORMATION =====
block size: 1024 B
filesystem size(byte): 512000 B
filesystem size(block): 500
number of inode(s): 100
inode list size(block): 8
inode bitmap size(block): 1
block bitmap size(block): 1

===== INODE USAGE INFORMATION =====
      Inodes    IUsed    IFree  IUse%
         100        2       98  2.00%

===== BLOCK USAGE INFORMATION =====
      1024-blocks     Used     Available  Capacity
              500       13           487     2.60%


# list the root directory
$ ./ls.pangya -f pangyafs.img /
       INODE LINK         SIZE  TYPE           NAME
           1    2           40   DIR              .
           1    2           40   DIR             ..

# create a directory
$ ./mkdir.pangya -f pangyafs.img /dir1

# copy a file from host OS to PangYa FS disk image
$ echo "This is PangYa FS test file!" > testfile1
$ ./cp.pangya -f pangyafs.img testfile1 /dir1/testfile1

# inspect the file metadata
$ ./stat.pangya -f pangyafs.img /dir1/testfile1
  File: /dir1/testfile1
  Size: 29              Blocks: 1          IO Block: 1024  
  Type: REG    Inode: 3          Links: 1

# list the /dir1 directory
$ ./ls.pangya -f pangyafs.img /dir1
       INODE LINK         SIZE  TYPE           NAME
           2    2           60   DIR              .
           1    3           60   DIR             ..
           3    1           29   REG      testfile1

# remove the test file
$ ./rm.pangya -f pangyafs.img /dir1/testfile1

# remove the test directory /dir1
$ ./rm.pangya -f pangyafs.img /dir1

# list the root directory again
$ ./ls.pangya -f pangyafs.img /
       INODE LINK         SIZE  TYPE           NAME
           1    2           60   DIR              .
           1    2           60   DIR             ..

```

## Roadmap & Future Implementation
- [x] Fixed-Width Integer Cleanup for Cross-Platform Safety
- [ ] Indirect Block Support (Files > 16KB)
- [ ] Hard Link Support & Reference Counting
- [ ] Symbolic Links
- [ ] `fsck.pangya` Implementation
- [ ] UNIX VFS Support

## Design Documentation
For detailed PangYa FS implementation and design, please refer to [coming soon].
