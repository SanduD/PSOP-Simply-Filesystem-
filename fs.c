#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define _UTHREAD_PRIVATE
#include "disk.h"
#include "fs.h"


#define fs_error(fmt, ...) \
	fprintf(stderr, "%s: ERROR-"fmt"\n", __func__, ##__VA_ARGS__)

#define EOC 0xFFFF
#define EMPTY 0

typedef enum { false, true } bool;

/* 
 * Superblock:
 * Offset	Length (bytes)	Description
 * 0x00		8-				Signature 
 * 0x08		2-				Total amount of blocks of virtual disk
 * 0x0A		2-				Root directory block index
 * 0x0C		2-				Data block start index
 * 0x0E		2				Amount of data blocks
 * 0x10		1				Number of blocks for FAT
 * 0x11		4079			Unused/Padding
 *
 */

struct superblock_t {
    char     signature[8];
    uint16_t num_blocks;
    uint16_t root_dir_index;
    uint16_t data_start_index;
    uint16_t num_data_blocks;
    uint8_t  num_FAT_blocks; 
    uint8_t  unused[4079];
} __attribute__((packed));

struct FAT_t {
	uint16_t words;
};


/* 
 *
 * Root Directory:
 * Offset	Length (bytes)	Description
 * 0x00		16				Filename (including NULL character)
 * 0x10		4				Size of the file (in bytes)
 * 0x14		2				Index of the first data block
 * 0x16		10				Unused/Padding
 *
 */

struct rootdirectory_t {
	char     filename[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t start_data_block;
	uint8_t  unused[10];
} __attribute__((packed));


struct file_descriptor_t {
    bool   is_used;       
    int    file_index;              
    size_t offset;  
	char   file_name[FS_FILENAME_LEN];
};


struct superblock_t      *superblock;
struct rootdirectory_t   *root_dir_block;
struct FAT_t             *FAT_blocks;
struct file_descriptor_t fd_table[FS_OPEN_MAX_COUNT]; 


int fs_mount(const char *diskname) {

	superblock = malloc(BLOCK_SIZE);

	if(block_disk_open(diskname) < 0){
		fs_error("failure to open virtual disk \n");
		return -1;
	}
	
	if(block_read(0, (void*)superblock) < 0){
		fs_error( "failure to read from block \n");
		return -1;
	}
	if(strncmp(superblock->signature, "ECS150FS", 8) != 0){
		fs_error( "invalid disk signature \n");
		return -1;
	}
	if(superblock->num_blocks != block_disk_count()) {
		fs_error("incorrect block disk count \n");
		return -1;
	}

	FAT_blocks = malloc(superblock->num_FAT_blocks * BLOCK_SIZE);
	for(int i = 0; i < superblock->num_FAT_blocks; i++) {
		if(block_read(i + 1, (void*)FAT_blocks + (i * BLOCK_SIZE)) < 0) {
			fs_error("failure to read from block \n");
			return -1;
		}
	}

	root_dir_block = malloc(sizeof(struct rootdirectory_t) * FS_FILE_MAX_COUNT);
	if(block_read(superblock->num_FAT_blocks + 1, (void*)root_dir_block) < 0) { 
		fs_error("failure to read from block \n");
		return -1;
	}
	
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].is_used = false;
	}
        
	return 0;
}


int fs_umount(void) {

	if(!superblock){
		fs_error("No disk available to unmount\n");
		return -1;
	}

	if(block_write(0, (void*)superblock) < 0) {
		fs_error("failure to write to block \n");
		return -1;
	}

	for(int i = 0; i < superblock->num_FAT_blocks; i++) {
		if(block_write(i + 1, (void*)FAT_blocks + (i * BLOCK_SIZE)) < 0) {
			fs_error("failure to write to block \n");
			return -1;
		}
	}

	if(block_write(superblock->num_FAT_blocks + 1, (void*)root_dir_block) < 0) {
		fs_error("failure to write to block \n");
			return -1;
	}

	free(superblock);
	free(root_dir_block);
	free(FAT_blocks);

	// reset file descriptors
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].offset = 0;
		fd_table[i].is_used = false;
		fd_table[i].file_index = -1;
		memset(fd_table[i].file_name, 0, FS_FILENAME_LEN);
    }

	block_disk_close();
	return 0;
}


int fs_info(void) {

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superblock->num_blocks);
	printf("fat_blk_count=%d\n", superblock->num_FAT_blocks);
	printf("rdir_blk=%d\n", superblock->num_FAT_blocks + 1);
	printf("data_blk=%d\n", superblock->num_FAT_blocks + 2);
	printf("data_blk_count=%d\n", superblock->num_data_blocks);
	printf("fat_free_ratio=%d/%d\n", get_num_FAT_free_blocks(), superblock->num_data_blocks);
	printf("rdir_free_ratio=%d/128\n", count_num_open_dir());

	return 0;
}



