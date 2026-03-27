#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "allocator.h"
#include "dir.h"
#include "fs_utils.h"
#include "superblock.h"

#define VERSION "0.0.1"

/* command line options */
static int opt_flag_f = 0;

/* define usage function */
static void usage(void) {
    printf(
        "ls - Version %s\n"
        "usage: ls.pangya -f|--filename <PangYa filesystem disk image filename> [<path_name>]\n"
        "                 [-h|--help]\n", VERSION
    );
}

int main(int argc, char *argv[]) {
    /* define command line options */
    char *short_opts = "f:h";
    struct option long_opts[] = {
        {"filename", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    char filename[PATH_MAX];
    int disk_image_fd = -1;
    struct superblock sb;
    char *pathname = "/";

    /* suppress default getopt error messages */
    opterr = 0;
    int c;

    while (1) {
        c = getopt_long(argc, argv, short_opts, long_opts, NULL);

        if (c < 0) {
            break;
        }

        switch (c) {
            case 'f':
                strcpy(filename, optarg);
                opt_flag_f = 1;

                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "ERROR: Unknown option\n\n");
                usage();
                exit(EXIT_FAILURE);
            default:
                fprintf(stderr, "ERROR: Unimplemented option\n\n");
                usage();
                exit(EXIT_FAILURE);
        }
    }

    /* check if necessary options are specified */
    if (!opt_flag_f) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* handle path name parameter */
    if (optind < argc) {
        pathname = argv[optind];
    }

    /* open disk image file */
    disk_image_fd = open(filename, O_RDONLY);
    if (disk_image_fd < 0) {
        fprintf(stderr, "ERROR: failed to open disk image file %s: %s\n", filename, strerror(errno));
        goto error_handler;
    }

    /* read superblock */
    if (read_superblock(disk_image_fd, &sb) < 0) {
        fprintf(stderr, "ERROR: failed to read superblock from disk image file %s\n", filename);
        goto error_handler;
    }

    /* get target path name inode */
    struct inode *pathname_inode = namei(disk_image_fd, &sb, pathname);
    if (pathname_inode == NULL) {
        fprintf(stderr, "ERROR: failed to get path name %s inode\n", pathname);
        goto error_handler;
    }

    /*
     * output format:
     * inode number | number of links | file size in byte | file type | filename
     *
    */

    printf("%12s %4s %12s %5s %14s\n", "INODE", "LINK", "SIZE", "TYPE", "NAME");

    /* scenario #1: regular file */
    if ((pathname_inode->i_mode & IFREG) != 0) {
        char *pathname_copy = strdup(pathname);
        if (pathname_copy != NULL) {
            printf("%12lu %4d %12u %5s %14s\n", pathname_inode->i_num, pathname_inode->i_nlink, pathname_inode->i_size0, "REG", basename(pathname_copy));
            free(pathname_copy);
        } else {
            printf("%12lu %4d %12u %5s %14s\n", pathname_inode->i_num, pathname_inode->i_nlink, pathname_inode->i_size0, "REG", pathname);
        }
    }

    /* scenario #2: directory */
    if ((pathname_inode->i_mode & IFDIR) != 0) {
        /* iterate dirent entries from each physical block in i_addr[] array */
        unsigned long int physical_block;
        struct inode *entry_inode;
        size_t dirent_entries = get_dirent_per_block();
        struct dirent dirent_entries_buffer[dirent_entries];

        for (unsigned short int logical_block = 0; logical_block < NDIRECT; ++logical_block) {
            physical_block = pathname_inode->i_addr[logical_block];

            /* only processing non-zero physical block */
            if (physical_block != 0) {
                if (read_block(disk_image_fd, physical_block, dirent_entries_buffer, sizeof(dirent_entries_buffer), 0) < 0) {
                    fprintf(stderr, "ERROR: failed to read physical block %lu\n", physical_block);
                    continue;
                }

                for (size_t i = 0; i < dirent_entries; ++i) {
                    /* only processing dirent entry with inode number is not zero */
                    if (dirent_entries_buffer[i].inode_number != 0) {
                        char *entry_mode = "UNK"; /* file type: UNKNOWN */

                        /* get entry inode struct and acquire metadata */
                        entry_inode = get_inode(disk_image_fd, &sb, dirent_entries_buffer[i].inode_number);
                        if (entry_inode == NULL) {
                            fprintf(stderr, "ERROR: failed to get inode struct for filename %s\n", dirent_entries_buffer[i].dir_name);
                            continue;
                        }

                        /* check mode of entry */
                        if ((entry_inode->i_mode & IFREG) != 0) {
                            entry_mode = "REG";
                        }

                        if ((entry_inode->i_mode & IFDIR) != 0) {
                            entry_mode = "DIR";
                        }

                        /* print output */
                        printf("%12lu %4d %12u %5s %14s\n", entry_inode->i_num, entry_inode->i_nlink, entry_inode->i_size0, entry_mode, dirent_entries_buffer[i].dir_name);

                        /* release resource */
                        put_inode(disk_image_fd, &sb, entry_inode);
                    }
                }
            }
        }
    }

    /* release resource */
    put_inode(disk_image_fd, &sb, pathname_inode);
    close(disk_image_fd);

    exit(EXIT_SUCCESS);

error_handler:
    if (disk_image_fd >= 0) {
        close(disk_image_fd);
    }

    exit(EXIT_FAILURE);
}
