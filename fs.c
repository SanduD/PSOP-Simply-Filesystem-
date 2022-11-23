#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128 // 64, assume 512 every block;
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024 //128

/* don't care about config and operation, they are used mixedly*/
#define FS_CONFIG_SUCCESS 1
#define FS_CONFIG_FAIL 0
#define FS_OPERATION_SUCCESS 0
#define FS_OPERATION_FAIL -1

#define MAX(a,b) ((a) >(b)? (a):(b))

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};


struct fs_info{
	// pre-requisite for operations except for format and debug
	int mounted;

	// bit map
	int* free_block_bitmap; // 0 -> free
	int* free_inode_bitmap; // 0 -> free
	int free_blocks;

	// cache info in super block;
	int nblocks;
	int ninodeblocks;
	int ninodes;
} FS_INFO = {.mounted = 0};


int fs_format()
{
	// disable the mounted flag;
	FS_INFO.mounted = 0;

	union fs_block block;

	disk_read(0, block.data);
	
	int nblocks = disk_size();
	if(nblocks < 2){
		return FS_CONFIG_FAIL;
	}
	block.super.nblocks = nblocks;

	// ninodeblocks =  10 percent of nblocks, rounding up.
	int ninodeblocks = (nblocks + 9)/10;
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = ninodeblocks * INODES_PER_BLOCK;
	disk_write(0, block.data);
	
	// set up cached general info;
	FS_INFO.nblocks = nblocks;
	FS_INFO.ninodeblocks = ninodeblocks;
	FS_INFO.ninodes = block.super.ninodes;

	// set inode block, isvalid = 0
	memset(block.data, 0, DISK_BLOCK_SIZE);
	for(int i=1; i<=ninodeblocks; i++){
		disk_write(i, block.data);
	}

	return FS_CONFIG_SUCCESS;
}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);


	int ninodeblocks = block.super.ninodeblocks;
	for(int i=0; i< ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(int j=0; j < INODES_PER_BLOCK; j++){
			// skip the invalid inode;
			if(!block.inode[j].isvalid) continue;

			struct fs_inode *inode = &block.inode[j]; 
			int inode_id = i*INODES_PER_BLOCK + j;
			printf("inode %d:\n", inode_id);
			printf("    size: %d bytes\n", inode->size);
			if(!inode->size) continue;

			// output the direct blocks ids
			int inode_blocks = (inode->size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;
			int k=0;
			printf("    direct blocks: ");
			for(; k<POINTERS_PER_INODE && k<inode_blocks; k++){
				printf("%d ", inode->direct[k]);
			}
			printf("\n");

			// output indirect block id and it's inner ids;
			if(inode_blocks > POINTERS_PER_INODE && inode->indirect){
				printf("    indirect block: %d\n", inode->indirect);
				union fs_block indirect_block;
				disk_read(inode->indirect, indirect_block.data);
				printf("    indirect data blocks: ");
				for(; k<inode_blocks; k++){
					printf("%d ", indirect_block.pointers[k - POINTERS_PER_INODE]);
				}
				printf("\n");
			}
		}
	}
}

int fs_mount()
{
	if(FS_INFO.mounted)
		return FS_CONFIG_SUCCESS;

	union fs_block block;
	disk_read(0, block.data);
	int nblocks = block.super.nblocks;
	int ninodeblocks = block.super.ninodeblocks;
	int ninodes = block.super.ninodes;

	if(nblocks<2)
		return FS_CONFIG_FAIL;

	FS_INFO.free_block_bitmap = (int*)calloc(1, nblocks * sizeof(int));
	FS_INFO.free_inode_bitmap = (int*)calloc(1, ninodes * sizeof(int));
	FS_INFO.free_blocks = nblocks;
	// set super block and inode blocks not free
	for(int i=0; i< 1 + ninodeblocks; i++){
		FS_INFO.free_block_bitmap[i] = 1;
		FS_INFO.free_blocks--;
	}

	// scan the filesystem and mark;
	for(int i=0; i<ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(int j=0; j < INODES_PER_BLOCK; j++){
			// skip the invalid inode;
			if(!block.inode[j].isvalid) continue;
			int inode_id = i*INODES_PER_BLOCK + j;
			FS_INFO.free_inode_bitmap[inode_id] = 1; // mark the inode;
			struct fs_inode *inode = &block.inode[j];
			if(!inode->size) continue;

			// mark the direct blocks ids
			int inode_blocks = (inode->size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;
			int k=0;
			for(; k<POINTERS_PER_INODE && k < inode_blocks; k++)
				FS_INFO.free_block_bitmap[inode->direct[k]] = 1;
				FS_INFO.free_blocks--;

			// mark indirect block id and it's inner ids;
			if(inode_blocks > POINTERS_PER_INODE && inode->indirect){
				FS_INFO.free_block_bitmap[inode->indirect] = 1;
				FS_INFO.free_blocks--;
				union fs_block indirect_block;
				disk_read(inode->indirect, indirect_block.data);
				for(; k<inode_blocks; k++)
					FS_INFO.free_block_bitmap[indirect_block.pointers[k - POINTERS_PER_INODE]] = 1;
					FS_INFO.free_blocks--;
			}
		}
	}

	FS_INFO.mounted = 1;
	return FS_CONFIG_SUCCESS;
}

