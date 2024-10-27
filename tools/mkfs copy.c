// mkfs.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "types.h"
#include "fs.h"

#define NINODES 200
#define FSSIZE 1000
#define LOGSIZE 30

int nbitmap = FSSIZE / (BSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, super, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void balloc(int used);
ushort xshort(ushort x);
uint xint(uint x);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img [files...] [error_type]\n");
        exit(1);
    }

    int i, cc, fd;
    uint rootino, inum, off;
    struct dirent de;
    char buf[BSIZE];
    struct dinode din;
    int create_error = 0;

    int file_start = 2;
    int file_end = argc; // exclusive

    // Check if the last argument is an error_type
    if (argc >= 3 && strncmp(argv[argc - 1], "error_", 6) == 0) {
        // The last argument is an error_type
        if (strcmp(argv[argc - 1], "error_bad_inode_type") == 0) {
            create_error = 1;  // Bad inode type
        } else if (strcmp(argv[argc - 1], "error_bad_direct_addr") == 0) {
            create_error = 2;  // Bad direct address
        } else if (strcmp(argv[argc - 1], "error_bad_indirect_addr") == 0) {
            create_error = 3;  // Bad indirect address
        } else if (strcmp(argv[argc - 1], "error_missing_root") == 0) {
            create_error = 4;  // Missing root directory
        } else if (strcmp(argv[argc - 1], "error_dir_not_formatted") == 0) {
            create_error = 5;  // Directory not properly formatted
        } else if (strcmp(argv[argc - 1], "error_free_addr_in_use") == 0) {
            create_error = 6;  // Address used by inode but marked free in bitmap
        } else if (strcmp(argv[argc - 1], "error_bmap_not_in_use") == 0) {
            create_error = 7;  // Bitmap marks block in use but it is not in use
        } else if (strcmp(argv[argc - 1], "error_duplicate_direct_addr") == 0) {
            create_error = 8;  // Direct address used more than once
        } else if (strcmp(argv[argc - 1], "error_duplicate_indirect_addr") == 0) {
            create_error = 9;  // Indirect address used more than once
        } else if (strcmp(argv[argc - 1], "error_inode_not_found") == 0) {
            create_error = 10; // Inode marked in use but not found in directory
        } else if (strcmp(argv[argc - 1], "error_inode_referred_not_used") == 0) {
            create_error = 11; // Inode referred to in directory but marked free
        } else if (strcmp(argv[argc - 1], "error_bad_ref_count") == 0) {
            create_error = 12; // Bad reference count for file
        } else {
            fprintf(stderr, "Unknown error type: %s\n", argv[argc - 1]);
            exit(1);
        }
        file_end = argc - 1; // Exclude the last argument
    }

    assert((BSIZE % sizeof(struct dinode)) == 0);
    assert((BSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) {
        perror(argv[1]);
        exit(1);
    }

    // Initialize filesystem layout
    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;

    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);

    printf("nmeta %d (boot, super, log %u inode %u, bitmap %u) blocks %d total %d\n",
           nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

    freeblock = nmeta;     // the first free block that we can allocate

    for (i = 0; i < FSSIZE; i++) {
        wsect(i, zeroes);
    }

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    if (create_error == 4) { // Missing root directory
        // Simulate missing root directory by not allocating or initializing it
        printf("Creating a filesystem with missing root directory.\n");
        balloc(freeblock);
        close(fsfd);
        return 0;
    }

    rootino = ialloc(T_DIR);
    assert(rootino == ROOTINO);

    // Initialize root directory entries
    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    if (create_error != 5) { // If not error_dir_not_formatted
        iappend(rootino, &de, sizeof(de));
    }

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    if (create_error != 5) { // If not error_dir_not_formatted
        iappend(rootino, &de, sizeof(de));
    }

    // Update root inode's link count
    rinode(rootino, &din);
    din.nlink = xshort(2);  // '.' and '..'
    winode(rootino, &din);

    // Write additional files into the filesystem image
    for (i = file_start; i < file_end; i++) {
        if ((fd = open(argv[i], 0)) < 0) {
            perror(argv[i]);
            exit(1);
        }

        if (argv[i][0] == '_')
            argv[i]++;

        inum = ialloc(T_FILE);

        // Create directory entry
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, argv[i], DIRSIZ);
        if (create_error != 10) { // If not error_inode_not_found
            iappend(rootino, &de, sizeof(de));
        }

        // Update file inode's link count
        rinode(inum, &din);
        din.nlink = xshort(1);
        if (create_error == 1) { // Bad inode type
            din.type = xshort(99); // Invalid type
        }
        winode(inum, &din);

        // Write file content
        while ((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);

        close(fd);

        if (create_error == 2) {
            // Bad direct address
            rinode(inum, &din);
            din.addrs[0] = xint(FSSIZE + 1); // Invalid block number
            winode(inum, &din);
        }

        if (create_error == 8) {
            // Direct address used more than once
            // We'll duplicate a direct address in another inode
            uint inum2 = ialloc(T_FILE);
            rinode(inum2, &din);
            din.nlink = xshort(1);
            din.addrs[0] = xint(freeblock - 1); // Use the same block as previous inode
            winode(inum2, &din);
            // Create directory entry
            bzero(&de, sizeof(de));
            de.inum = xshort(inum2);
            strncpy(de.name, "dup_file", DIRSIZ);
            iappend(rootino, &de, sizeof(de));
        }

        

        if (create_error == 12) {
            // Bad reference count for file
            rinode(inum, &din);
            din.nlink = xshort(2); // Incorrect reference count
            winode(inum, &din);
        }
    }

    if (create_error == 3) {
        // Bad indirect address
        printf("Creating a filesystem with bad indirect address.\n");
        inum = ialloc(T_FILE);
        // Create directory entry
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, "bad_indirect", DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        // Update inode
        rinode(inum, &din);
        din.nlink = xshort(1);
        din.addrs[NDIRECT] = xint(FSSIZE + 1); // Invalid block number
        winode(inum, &din);
    }

    if (create_error == 9) {
        // Indirect address used more than once
        printf("Creating a filesystem with duplicate indirect addresses.\n");
        inum = ialloc(T_FILE);
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, "dup_indirect", DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        // Write data to allocate indirect block
        bzero(buf, BSIZE);
        for (int j = 0; j < NDIRECT + 1; j++) {
            iappend(inum, buf, BSIZE);
        }

        rinode(inum, &din);
        uint indir_block = xint(din.addrs[NDIRECT]);

        // Allocate another inode and set its indirect block to the same block
        uint inum2 = ialloc(T_FILE);
        bzero(&de, sizeof(de));
        de.inum = xshort(inum2);
        strncpy(de.name, "dup_indirect2", DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        rinode(inum2, &din);
        din.nlink = xshort(1);
        din.addrs[NDIRECT] = xint(indir_block); // Use the same indirect block
        winode(inum2, &din);
    }

    // Fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off / BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);


    if (create_error == 6) {
        // Address used by inode but marked free in bitmap
        printf("Creating a filesystem with address used by inode but marked free in bitmap.\n");

        // Allocate a new inode for testing
        inum = ialloc(T_FILE);

        // Allocate a data block for this inode and update inode's direct address
        rinode(inum, &din);
        din.addrs[0] = xint(freeblock++);  // Assign a block number to inode
        din.size = xint(BSIZE); // Set a size to indicate it uses one block
        winode(inum, &din);

        // Reference the inode in the root directory
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, "file_with_free_block", DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        // Mark this block as free in the bitmap
        uint addr = xint(din.addrs[0]);
        uchar bitmap_buf[BSIZE];
        uint bmap_block = xint(sb.bmapstart) + (addr / BPB);
        rsect(bmap_block, bitmap_buf);
        uint block_offset = addr % BPB;
        bitmap_buf[block_offset / 8] &= ~(1 << (block_offset % 8));  // Mark as free
        wsect(bmap_block, bitmap_buf);
    }

    if (create_error == 11) {
        // Inode referred to in directory but marked free
        printf("Creating a filesystem with inode referred in directory but marked free.\n");
        // Create a directory entry pointing to a free inode
        bzero(&de, sizeof(de));
        de.inum = xshort(freeinode + 1);  // Inode not allocated
        strncpy(de.name, "bad_inode_ref", DIRSIZ);
        iappend(rootino, &de, sizeof(de));
    }

    balloc(freeblock);
    if (create_error == 7) {
        // Bitmap marks block in use but it is not in use
        printf("Creating a filesystem with bitmap marking block in use but not in use.\n");
        // Mark a block as used in the bitmap, but don't allocate it
        uchar bitmap_buf[BSIZE];
        uint fake_block = freeblock; // Next free block (not allocated)
        uint bmap_block = xint(sb.bmapstart) + (fake_block / BPB);
        rsect(bmap_block, bitmap_buf);
        uint block_offset = fake_block % BPB;
        bitmap_buf[block_offset / 8] |= (1 << (block_offset % 8));  // Mark as used
        wsect(bmap_block, bitmap_buf);
    }



    if (create_error == 6) {
        // Address used by inode but marked free in bitmap
        printf("Creating a filesystem with address used by inode but marked free in bitmap.\n");

        // Allocate a new inode for testing
        inum = ialloc(T_FILE);

        // Allocate a data block for this inode and update inode's direct address
        rinode(inum, &din);
        din.addrs[0] = xint(freeblock++);  // Assign a block number to inode
        din.size = xint(BSIZE); // Set a size to indicate it uses one block
        winode(inum, &din);

        // Reference the inode in the root directory
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, "file_with_free_block", DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        // Mark this block as free in the bitmap
        uint addr = xint(din.addrs[0]);
        uchar bitmap_buf[BSIZE];
        uint bmap_block = xint(sb.bmapstart) + (addr / BPB);
        rsect(bmap_block, bitmap_buf);
        uint block_offset = addr % BPB;
        bitmap_buf[block_offset / 8] &= ~(1 << (block_offset % 8));  // Mark as free
        wsect(bmap_block, bitmap_buf);
    }


    if (create_error == 5) {
        // Directory not properly formatted (already handled above)
        printf("Creating a filesystem with directory not properly formatted.\n");
    }

    close(fsfd);
    return 0;
}


void wsect(uint sec, void *buf) {
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != (off_t)(sec * BSIZE)) {
        perror("lseek");
        exit(1);
    }
    if (write(fsfd, buf, BSIZE) != BSIZE) {
        perror("write");
        exit(1);
    }
}

void winode(uint inum, struct dinode *ip) {
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip) {
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

void rsect(uint sec, void *buf) {
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != (off_t)(sec * BSIZE)) {
        perror("lseek");
        exit(1);
    }
    if (read(fsfd, buf, BSIZE) != BSIZE) {
        perror("read");
        exit(1);
    }
}

uint ialloc(ushort type) {
    uint inum = freeinode++;
    struct dinode din;

    bzero(&din, sizeof(din));
    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

void iappend(uint inum, void *xp, int n) {
    char *p = (char *)xp;
    uint fbn, off, n1;
    struct dinode din;
    char buf[BSIZE];
    uint indirect[NINDIRECT];
    uint x;

    rinode(inum, &din);
    off = xint(din.size);

    while (n > 0) {
        fbn = off / BSIZE;
        assert(fbn < MAXFILE);

        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        } else {
            if (xint(din.addrs[NDIRECT]) == 0) {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            if (xint(indirect[fbn - NDIRECT]) == 0) {
                indirect[fbn - NDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            }
            x = xint(indirect[fbn - NDIRECT]);
        }

        n1 = n < (int)(BSIZE - off % BSIZE) ? n : (int)(BSIZE - off % BSIZE);
        rsect(x, buf);
        memmove(buf + off % BSIZE, p, n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }

    din.size = xint(off);
    winode(inum, &din);
}

void balloc(int used) {
    uchar buf[BSIZE];
    int i;

    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < FSSIZE);

    int blocks = FSSIZE;
    int bmap_blocks = (blocks + BPB - 1) / BPB;

    for (int b = 0; b < bmap_blocks; b++) {
        bzero(buf, BSIZE);
        for (i = 0; i < BPB && (b * BPB + i) < used; i++) {
            buf[i / 8] |= (0x1 << (i % 8));
        }
        wsect(xint(sb.bmapstart) + b, buf);
    }
}

ushort xshort(ushort x) {
    ushort y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x) {
    uint y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}