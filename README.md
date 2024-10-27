# xv6 File System Checker

## Overview

This project is a simple file system checker (xcheck) and a file system image generator (mkfs) for the xv6 operating system file system. The purpose of the file system checker is to ensure the consistency of a file system image, detecting and reporting errors such as bad inodes, improper directory formats, overlapping addresses, and incorrect reference counts.

The project has the following core functionalities:
- **File System Checker (xcheck):** Verifies the consistency of an xv6 file system image.
- **File System Generator (mkfs):** Creates a file system image, optionally introducing inconsistencies for testing purposes.

## File Structure

```plaintext
xv6_fs_checker/
├── Makefile
├── include/
│   ├── fs.h
│   └── types.h
├── src/
│   └── xcheck.c
├── tools/
│   └── mkfs.c
└── images/
    └── (Generated file system images)
```

## Prerequisites

- GCC compiler (`gcc`)
- UNIX-like operating system (Linux or macOS)

## Project Files

### Source Files

- **xcheck.c:** Contains the implementation of the file system checker.
- **mkfs.c:** Contains the implementation of the file system image generator.

### Header Files

- **fs.h:** Defines the structures and constants related to the xv6 file system.
- **types.h:** Defines the basic types used in the project.

## Makefile

The Makefile provides rules for building the project, creating file system images, and running consistency checks.

## Building the Project

To compile the project, simply run:

```bash
make
```

This command will compile the following executables:
- `xcheck`: The file system checker.
- `mkfs`: The file system image generator.

## Generating a File System Image

To create a file system image, use the following command:

```bash
make fs.img
```

This will generate a basic file system image in the `images/` directory. The default command creates an image with two files: `file1.txt` and `file2.txt`.

## Introducing Inconsistencies for Testing

The `mkfs` tool can intentionally introduce inconsistencies in the file system image to test the functionality of `xcheck`. Here are the available error types:
- **Error 1:** Missing root directory.
- **Error 2:** Bad inode type.
- **Error 3:** Bad direct address.
- **Error 4:** Bad indirect address.
- **Error 5:** Directory not properly formatted.
- **Error 6:** Address used by inode but marked free in bitmap.
- **Error 7:** Bitmap marks block in use but it is not in use.
- **Error 8:** Direct address used more than once.
- **Error 9:** Indirect address used more than once.
- **Error 10:** Inode marked in use but not found in directory.
- **Error 11:** Inode referred to in directory but marked free.
- **Error 12:** Bad reference count for file.
- **Error 13:** Directory appearing more than once in the file system.

### Example Commands to Create Inconsistent File System Images

```bash
# Create an image with a missing root directory
./tools/mkfs images/fs_error_missing_root.img error_missing_root

# Create an image with bad inode type
./tools/mkfs images/fs_error_bad_inode_type.img error_bad_inode_type

# Create an image with bad direct address
./tools/mkfs images/fs_error_bad_direct_addr.img error_bad_direct_addr

# Create an image with bad indirect address
./tools/mkfs images/fs_error_bad_indirect_addr.img error_bad_indirect_addr

# Create an image with directory not properly formatted
./tools/mkfs images/fs_error_dir_not_formatted.img error_dir_not_formatted

# Create an image with address used by inode but marked free in bitmap
./tools/mkfs images/fs_error_free_addr_in_use.img error_free_addr_in_use

# Create an image with bitmap marking block in use but it is not in use
./tools/mkfs images/fs_error_bmap_not_in_use.img error_bmap_not_in_use

# Create an image with direct address used more than once
./tools/mkfs images/fs_error_duplicate_direct_addr.img error_duplicate_direct_addr

# Create an image with indirect address used more than once
./tools/mkfs images/fs_error_duplicate_indirect_addr.img error_duplicate_indirect_addr

# Create an image with inode marked in use but not found in directory
./tools/mkfs images/fs_error_inode_not_found.img error_inode_not_found

# Create an image with inode referred to in directory but marked free
./tools/mkfs images/fs_error_inode_referred_not_used.img error_inode_referred_not_used

# Create an image with a bad reference count for a file
./tools/mkfs images/fs_error_bad_ref_count.img error_bad_ref_count

# Create an image with a directory appearing more than once
./tools/mkfs images/fs_error_directory_appears_more_than_once.img error_directory_appears_more_than_once
```

## Running the File System Checker

To check a file system image for consistency, use the following command:

```bash
./xcheck images/fs.img
```

This will run the `xcheck` tool on the specified image (`fs.img`) and report any inconsistencies detected. The program will output an error message and exit if any issues are found.

### Example Commands to Check File System Images

```bash
# Check a consistent file system image
./xcheck images/fs_normal.img

# Check an image with a missing root directory
./xcheck images/fs_error_missing_root.img

# Check an image with a directory not properly formatted
./xcheck images/fs_error_dir_not_formatted.img

# Check an image with a bad inode type
./xcheck images/fs_error_bad_inode_type.img

# Check an image with a directory appearing more than once in the file system
./xcheck images/fs_error_directory_appears_more_than_once.img
```

## Cleaning Up

To clean up all generated files, including executables and images:

```bash
make clean
```

To clean up only the compiled executables:

```bash
make clean-bin
```

## Project Structure

### Makefile Overview

The Makefile includes the following rules:
- **all:** Compiles both the `xcheck` and `mkfs` executables.
- **images:** Generates file system images named based on the error they have using the `mkfs` tool.
- **check:** Runs the `xcheck` tool on the generated images.
- **clean:** Deletes all generated files including images and executables.
- **clean-bin:** Deletes only the executables (`xcheck` and `mkfs`).

## Testing the Project

To thoroughly test the project, follow these steps:

### Build the Project:

```bash
make
```

### Generate Different Test Images:
```bash
make images
```

### Run the Checker on Generated Images:
```bash
make check
```