# Makefile for xv6_fs_checker

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Include directory for header files
INCLUDE = -I include

# Source files and target executables
XCHECK_SRC = src/xcheck.c
MKFS_SRC = tools/mkfs.c

XCHECK_BIN = src/xcheck
MKFS_BIN = tools/mkfs

# Images and errors
IMAGES_DIR = images
NORMAL_IMAGE = $(IMAGES_DIR)/fs_normal.img
ERROR_IMAGES = $(IMAGES_DIR)/fs_error_bad_inode_type.img \
               $(IMAGES_DIR)/fs_error_bad_direct_addr.img \
               $(IMAGES_DIR)/fs_error_bad_indirect_addr.img \
               $(IMAGES_DIR)/fs_error_missing_root.img \
               $(IMAGES_DIR)/fs_error_dir_not_formatted.img \
               $(IMAGES_DIR)/fs_error_free_addr_in_use.img \
               $(IMAGES_DIR)/fs_error_bmap_not_in_use.img \
               $(IMAGES_DIR)/fs_error_duplicate_direct_addr.img \
               $(IMAGES_DIR)/fs_error_duplicate_indirect_addr.img \
               $(IMAGES_DIR)/fs_error_inode_not_found.img \
               $(IMAGES_DIR)/fs_error_inode_referred_not_used.img \
               $(IMAGES_DIR)/fs_error_bad_ref_count.img \
		   $(IMAGES_DIR)/fs_error_directory_appears_more_than_once.img
ALL_IMAGES = $(NORMAL_IMAGE) $(ERROR_IMAGES)

# Sample files
SAMPLE_FILES = file1.txt file2.txt

# Default rule when running `make` without arguments
all: $(MKFS_BIN) $(XCHECK_BIN)

# Rule for xcheck
$(XCHECK_BIN): $(XCHECK_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $<

# Rule for mkfs
$(MKFS_BIN): $(MKFS_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $<

# Rule to create sample files
sample_files: $(SAMPLE_FILES)

file1.txt:
	@echo "This is file 1" > $@

file2.txt:
	@echo "This is file 2" > $@

# Rule to create normal image
$(NORMAL_IMAGE): $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt

# Rule to create error images
$(IMAGES_DIR)/fs_error_bad_inode_type.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_bad_inode_type

$(IMAGES_DIR)/fs_error_bad_direct_addr.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_bad_direct_addr

$(IMAGES_DIR)/fs_error_bad_indirect_addr.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_bad_indirect_addr

$(IMAGES_DIR)/fs_error_missing_root.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_missing_root

$(IMAGES_DIR)/fs_error_dir_not_formatted.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_dir_not_formatted

$(IMAGES_DIR)/fs_error_free_addr_in_use.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_free_addr_in_use

$(IMAGES_DIR)/fs_error_bmap_not_in_use.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_bmap_not_in_use

$(IMAGES_DIR)/fs_error_duplicate_direct_addr.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_duplicate_direct_addr

$(IMAGES_DIR)/fs_error_duplicate_indirect_addr.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_duplicate_indirect_addr

$(IMAGES_DIR)/fs_error_inode_not_found.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_inode_not_found

$(IMAGES_DIR)/fs_error_inode_referred_not_used.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_inode_referred_not_used

$(IMAGES_DIR)/fs_error_bad_ref_count.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_bad_ref_count

$(IMAGES_DIR)/fs_error_directory_appears_more_than_once.img: $(MKFS_BIN) sample_files
	@mkdir -p $(IMAGES_DIR)
	@./$(MKFS_BIN) $@ file1.txt file2.txt error_directory_appears_more_than_once


# Rule to create all images
images: $(ALL_IMAGES)

# Rule to run checker on images
check: $(XCHECK_BIN)
	@if [ ! -f $(NORMAL_IMAGE) ]; then \
		echo "Error: Images have not been created. Run 'make images' first."; \
		exit 1; \
	fi
	@echo "0. Checking normal image:"
	@./$(XCHECK_BIN) $(NORMAL_IMAGE) || true
	@echo "1. Checking image with bad inode type:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_bad_inode_type.img || true
	@echo "2a. Checking image with bad direct address:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_bad_direct_addr.img || true
	@echo "2b. Checking image with bad indirect address:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_bad_indirect_addr.img || true
	@echo "3. Checking image with missing root directory:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_missing_root.img || true
	@echo "4. Checking image with improperly formatted directory:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_dir_not_formatted.img || true
	@echo "5. Checking image with address marked free but used:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_free_addr_in_use.img || true
	@echo "6. Checking image with bitmap marking block in use but not in use:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_bmap_not_in_use.img || true
	@echo "7. Checking image with duplicate direct addresses:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_duplicate_direct_addr.img || true
	@echo "8. Checking image with duplicate indirect addresses:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_duplicate_indirect_addr.img || true
	@echo "9. Checking image with inode marked in use but not found in directory:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_inode_not_found.img || true
	@echo "10. Checking image with inode referred to in directory but not marked in use:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_inode_referred_not_used.img || true
	@echo "11. Checking image with bad reference count for file:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_bad_ref_count.img || true
	@echo "12. Checking image with directory appearing more than once in the file system:"
	@./$(XCHECK_BIN) $(IMAGES_DIR)/fs_error_directory_appears_more_than_once.img || true

# Clean up generated files
clean:
	rm -f $(XCHECK_BIN) $(MKFS_BIN) $(ALL_IMAGES) $(SAMPLE_FILES)

# Clean up executables only
clean-bin:
	rm -f $(XCHECK_BIN) $(MKFS_BIN)
