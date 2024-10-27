// xcheck.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fs.h"
#include "types.h"

#define UNUSED 0
#define DIRECT 1
#define INDIRECT 2

// Function prototypes
int block_is_marked(void *img_ptr, struct superblock *sb, uint blocknum);
struct dinode *get_inode(void *img_ptr, struct superblock *sb, uint inum);
void process_directory_block(void *img_ptr, uint addr, uint dir_inum, int *dot_found, int *dotdot_found, int *inode_referenced, int *inode_type, int *inode_used, int *inode_linkcount, int *inode_parent, uint num_inodes);

ushort xshort(ushort x) {
    uchar *a = (uchar *)&x;
    return ((ushort)a[0]) | ((ushort)a[1] << 8);
}

uint xint(uint x) {
    uchar *a = (uchar *)&x;
    return ((uint)a[0]) | ((uint)a[1] << 8) | ((uint)a[2] << 16) | ((uint)a[3] << 24);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: xcheck <file_system_image>\n");
        exit(1);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }

    struct stat sbuf;
    if (fstat(fd, &sbuf) < 0) {
        fprintf(stderr, "Error: fstat failed.\n");
        close(fd);
        exit(1);
    }

    void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (img_ptr == MAP_FAILED) {
        fprintf(stderr, "Error: mmap failed.\n");
        close(fd);
        exit(1);
    }

    struct superblock *sb = (struct superblock *)(img_ptr + BSIZE);

    uint num_inodes = xint(sb->ninodes);
    uint num_blocks = xint(sb->size);

    // Compute data block start
    uint bmapstart = xint(sb->bmapstart);
    uint sb_size = xint(sb->size);
    uint num_bitmap_blocks = (sb_size + BPB - 1) / BPB;
    uint data_block_start = bmapstart + num_bitmap_blocks;


    // Allocate arrays
    int *inode_used = calloc(num_inodes, sizeof(int));
    int *inode_referenced = calloc(num_inodes, sizeof(int));
    int *inode_type = calloc(num_inodes, sizeof(int));
    int *inode_nlink = calloc(num_inodes, sizeof(int));
    int *inode_linkcount = calloc(num_inodes, sizeof(int));
    int *inode_parent = calloc(num_inodes, sizeof(int));
    for (uint i = 0; i < num_inodes; i++) {
        inode_parent[i] = -1;
    }

    int *block_used = calloc(num_blocks, sizeof(int));
    int *block_type = calloc(num_blocks, sizeof(int));  // For distinguishing direct and indirect blocks

    // Check reference counts for files and directories
    for (uint inum = 1; inum < num_inodes; inum++) {
        if (inode_used[inum]) {
            if (inode_type[inum] == T_FILE) {
                if (inode_nlink[inum] != inode_linkcount[inum]) {
                    fprintf(stderr, "ERROR: bad reference count for file.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
            } else if (inode_type[inum] == T_DIR) {
                // Check for multiple links to a directory
                if (inum != ROOTINO && inode_linkcount[inum] > 1) {
                    fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
            }
        }
    }

    // Process inodes
    for (uint inum = 0; inum < num_inodes; inum++) {
        struct dinode *dip = get_inode(img_ptr, sb, inum);

        int type = xshort(dip->type);

        // Check 1: Each inode is either unallocated or one of the valid types
        if (type != 0 && type != T_FILE && type != T_DIR && type != T_DEV) {
            fprintf(stderr, "ERROR: bad inode.\n");
            free(inode_used); free(inode_referenced); free(inode_type);
            free(inode_nlink); free(inode_linkcount); free(inode_parent);
            free(block_used); free(block_type); close(fd);
            exit(1);
        }

        if (type != 0) {
            inode_used[inum] = 1;
            inode_type[inum] = type;
            inode_nlink[inum] = xshort(dip->nlink);

            // Process direct blocks
            for (int i = 0; i < NDIRECT; i++) {
                uint addr = xint(dip->addrs[i]);
                if (addr != 0) {
                    if (addr < data_block_start || addr >= sb->size) {
                        fprintf(stderr, "ERROR: bad direct address in inode.\n");
                        free(inode_used); free(inode_referenced); free(inode_type);
                        free(inode_nlink); free(inode_linkcount); free(inode_parent);
                        free(block_used); free(block_type); close(fd);
                        exit(1);
                    }
                    if (block_used[addr]) {
                        if (block_type[addr] == DIRECT) {
                            fprintf(stderr, "ERROR: direct address used more than once.\n");
                        } else {
                            fprintf(stderr, "ERROR: indirect address used more than once.\n");
                        }
                        free(inode_used); free(inode_referenced); free(inode_type);
                        free(inode_nlink); free(inode_linkcount); free(inode_parent);
                        free(block_used); free(block_type); close(fd);
                        exit(1);
                    }
                    block_used[addr] = 1;
                    block_type[addr] = DIRECT;
                    // Check that block is marked in bitmap
                    if (!block_is_marked(img_ptr, sb, addr)) {
                        fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                        free(inode_used); free(inode_referenced); free(inode_type);
                        free(inode_nlink); free(inode_linkcount); free(inode_parent);
                        free(block_used); free(block_type); close(fd);
                        exit(1);
                    }
                }
            }

            // Process indirect block
            uint indirect_addr = xint(dip->addrs[NDIRECT]);
            if (indirect_addr != 0) {
                if (indirect_addr < data_block_start || indirect_addr >= sb->size) {
                    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
                if (block_used[indirect_addr]) {
                    if (block_type[indirect_addr] == DIRECT) {
                        fprintf(stderr, "ERROR: direct address used more than once.\n");
                    } else {
                        fprintf(stderr, "ERROR: indirect address used more than once.\n");
                    }
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
                block_used[indirect_addr] = 1;
                block_type[indirect_addr] = INDIRECT;
                // Check that block is marked in bitmap
                if (!block_is_marked(img_ptr, sb, indirect_addr)) {
                    fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }

                // Read indirect block
                uint *indirect_block = (uint *)(img_ptr + indirect_addr * BSIZE);
                for (uint i = 0; i < NINDIRECT; i++) {
                    uint addr = xint(indirect_block[i]);
                    if (addr != 0) {
                        if (addr < data_block_start || addr >= sb->size) {
                            fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                            free(inode_used); free(inode_referenced); free(inode_type);
                            free(inode_nlink); free(inode_linkcount); free(inode_parent);
                            free(block_used); free(block_type); close(fd);
                            exit(1);
                        }
                        if (block_used[addr]) {
                            if (block_type[addr] == DIRECT) {
                                fprintf(stderr, "ERROR: direct address used more than once.\n");
                            } else {
                                fprintf(stderr, "ERROR: indirect address used more than once.\n");
                            }
                            free(inode_used); free(inode_referenced); free(inode_type);
                            free(inode_nlink); free(inode_linkcount); free(inode_parent);
                            free(block_used); free(block_type); close(fd);
                            exit(1);
                        }
                        block_used[addr] = 1;
                        block_type[addr] = INDIRECT;
                        // Check that block is marked in bitmap
                        if (!block_is_marked(img_ptr, sb, addr)) {
                            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                            free(inode_used); free(inode_referenced); free(inode_type);
                            free(inode_nlink); free(inode_linkcount); free(inode_parent);
                            free(block_used); free(block_type); close(fd);
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    // Check if root inode is allocated
    if (!inode_used[ROOTINO]) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        free(inode_used); free(inode_referenced); free(inode_type);
        free(inode_nlink); free(inode_linkcount); free(inode_parent);
        free(block_used); free(block_type); close(fd);
        exit(1);
    }

    // Process directories
    for (uint inum = 0; inum < num_inodes; inum++) {
        if (inode_used[inum] && inode_type[inum] == T_DIR) {
            struct dinode *dip = get_inode(img_ptr, sb, inum);

            int dot_found = 0;
            int dotdot_found = 0;

            // Process direct blocks
            for (int i = 0; i < NDIRECT; i++) {
                uint addr = xint(dip->addrs[i]);
                if (addr != 0) {
                    process_directory_block(img_ptr, addr, inum, &dot_found, &dotdot_found, inode_referenced, inode_type, inode_used, inode_linkcount, inode_parent, num_inodes);
                }
            }

            // Process indirect block
            uint indirect_addr = xint(dip->addrs[NDIRECT]);
            if (indirect_addr != 0) {
                uint *indirect_block = (uint *)(img_ptr + indirect_addr * BSIZE);
                for (uint i = 0; i < NINDIRECT; i++) {
                    uint addr = xint(indirect_block[i]);
                    if (addr != 0) {
                        process_directory_block(img_ptr, addr, inum, &dot_found, &dotdot_found, inode_referenced, inode_type, inode_used, inode_linkcount, inode_parent, num_inodes);
                    }
                }
            }

            if (!dot_found || !dotdot_found) {
                fprintf(stderr, "ERROR: directory not properly formatted.\n");
                free(inode_used); free(inode_referenced); free(inode_type);
                free(inode_nlink); free(inode_linkcount); free(inode_parent);
                free(block_used); free(block_type); close(fd);
                exit(1);
            }

            // For root directory, check that parent is itself
            if (inum == ROOTINO && inode_parent[inum] != ROOTINO) {
                fprintf(stderr, "ERROR: root directory does not exist.\n");
                free(inode_used); free(inode_referenced); free(inode_type);
                free(inode_nlink); free(inode_linkcount); free(inode_parent);
                free(block_used); free(block_type); close(fd);
                exit(1);
            }
        }
    }

    // Check for inodes marked in use but not found in a directory
    for (uint inum = 1; inum < num_inodes; inum++) {
        if (inode_used[inum] && !inode_referenced[inum] && inode_type[inum] != T_DIR) {
            fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
            free(inode_used); free(inode_referenced); free(inode_type);
            free(inode_nlink); free(inode_linkcount); free(inode_parent);
            free(block_used); free(block_type); close(fd);
            exit(1);
        }
    }

    // Check reference counts for files and directories
    for (uint inum = 1; inum < num_inodes; inum++) {
        if (inode_used[inum]) {
            if (inode_type[inum] == T_FILE) {
                if (inode_nlink[inum] != inode_linkcount[inum]) {
                    fprintf(stderr, "ERROR: bad reference count for file.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
            } else if (inode_type[inum] == T_DIR) {
                if (inode_linkcount[inum] > 1 && inum != ROOTINO) {
                    fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
                    free(inode_used); free(inode_referenced); free(inode_type);
                    free(inode_nlink); free(inode_linkcount); free(inode_parent);
                    free(block_used); free(block_type); close(fd);
                    exit(1);
                }
            }
        }
    }

    // Check for bitmap marks block in use but it is not in use
    for (uint blocknum = data_block_start; blocknum < sb->size; blocknum++) {
        if (block_is_marked(img_ptr, sb, blocknum) && !block_used[blocknum]) {
            fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
            free(inode_used); free(inode_referenced); free(inode_type);
            free(inode_nlink); free(inode_linkcount); free(inode_parent);
            free(block_used); free(block_type); close(fd);
            exit(1);
        }
    }

    // Check if blocks are used by an inode but marked as free in the bitmap
    for (uint blocknum = data_block_start; blocknum < sb->size; blocknum++) {
        if (block_used[blocknum] && !block_is_marked(img_ptr, sb, blocknum)) {
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            free(inode_used); free(inode_referenced); free(inode_type);
            free(inode_nlink); free(inode_linkcount); free(inode_parent);
            free(block_used); free(block_type); close(fd);
            exit(1);
        }
    }


    // Check for block in use but not marked in bitmap
    for (uint blocknum = data_block_start; blocknum < sb->size; blocknum++) {
        if (!block_is_marked(img_ptr, sb, blocknum) && block_used[blocknum]) {
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            free(inode_used); free(inode_referenced); free(inode_type);
            free(inode_nlink); free(inode_linkcount); free(inode_parent);
            free(block_used); free(block_type); close(fd);
            exit(1);
        }
    }


    // Free allocated memory and close file descriptor
    free(inode_used);
    free(inode_referenced);
    free(inode_type);
    free(inode_nlink);
    free(inode_linkcount);
    free(inode_parent);
    free(block_used);
    free(block_type);
    close(fd);

    // All checks passed
    return 0;
}

// Check if a block is marked in the bitmap
int block_is_marked(void *img_ptr, struct superblock *sb, uint blocknum) {
    uint bmapstart = xint(sb->bmapstart);
    uint bmap_block = bmapstart + (blocknum / BPB);
    uint bmap_block_offset = blocknum % BPB;

    uchar *bitmap_block = (uchar *)(img_ptr + bmap_block * BSIZE);

    uint byte_index = bmap_block_offset / 8;
    uint bit_index = bmap_block_offset % 8;

    uchar byte = bitmap_block[byte_index];
    return (byte >> bit_index) & 1;
}


// Get inode by inode number
struct dinode *get_inode(void *img_ptr, struct superblock *sb, uint inum) {
    uint block = sb->inodestart + inum / IPB;
    uint offset = (inum % IPB) * sizeof(struct dinode);
    return (struct dinode *)(img_ptr + block * BSIZE + offset);
}

// Process a directory block
void process_directory_block(void *img_ptr, uint addr, uint dir_inum, int *dot_found, int *dotdot_found, int *inode_referenced, int *inode_type, int *inode_used, int *inode_linkcount, int *inode_parent, uint num_inodes) {
    struct dirent *de = (struct dirent *)(img_ptr + addr * BSIZE);
    int num_entries = BSIZE / sizeof(struct dirent);

    for (int i = 0; i < num_entries; i++) {
        if (de[i].inum == 0)
            continue;

        ushort dir_inum_ref = xshort(de[i].inum);

        if (strncmp(de[i].name, ".", DIRSIZ) == 0) {
            *dot_found = 1;
            if (dir_inum_ref != dir_inum) {
                fprintf(stderr, "ERROR: directory not properly formatted.\n");
                exit(1);
            }
        } else if (strncmp(de[i].name, "..", DIRSIZ) == 0) {
            *dotdot_found = 1;
            inode_parent[dir_inum] = dir_inum_ref;
        }

        if (dir_inum_ref >= num_inodes) {
            fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
            exit(1);
        }

        if (!inode_used[dir_inum_ref]) {
            fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
            exit(1);
        }

        inode_referenced[dir_inum_ref] = 1;

        if (inode_type[dir_inum_ref] == T_FILE || inode_type[dir_inum_ref] == T_DIR) {
            inode_linkcount[dir_inum_ref]++;
        }
    }
}
