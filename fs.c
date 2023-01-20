#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

           
#define INODES_PER_BLOCK   128 // 64, assume 512 every block;
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024 //128

#define FS_CONFIG_SUCCESS 1
#define FS_CONFIG_FAIL 0
#define FS_OPERATION_SUCCESS 0
#define FS_OPERATION_FAIL -1

#define MAX(a,b) ((a) >(b)? (a):(b))

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;//10%*nblocks
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	//vector care contine pointeri catre blocurile de date ale fisierului
	int direct[POINTERS_PER_INODE];
	//variabila care va stoca o locatie fizica de pe disc unde se afla un block de pointeri catre dataBLocks
	//este initializat in momentul in care am mai mult de POINTERS_PER_INODE in direct[]
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	//variabila care va stoca adresele de blocuri care sunt accesate indirect
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

struct fs_info{
	int mounted;

	// bit map-ul
	//bitmap care va tine evidenta blocurilor libere. folosit pt a aloca sau elibera blocuri 
	//atunci cand se creeaza/sterge un inode
	int* free_block_bitmap; // 0 -> free
	int* free_inode_bitmap; // 0 -> free
	int free_blocks;

	
	int nblocks;
	int ninodeblocks;
	int ninodes;
} FS_INFO = {.mounted = 0};//initializez variabila statica FS_INFO si seteaza mounted


int fs_format()
{
	
	FS_INFO.mounted = 0;

	union fs_block block;

	disk_read(0, block.data);
	
	int nblocks = disk_size();
	if(nblocks < 2){
		return FS_CONFIG_FAIL;
	}
	block.super.nblocks = nblocks;

	// ninodeblocks =  10 %din blocks

	int ninodeblocks = (nblocks + 9)/10;
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = ninodeblocks * INODES_PER_BLOCK;
	disk_write(0, block.data);
	
	// general info;
	FS_INFO.nblocks = nblocks;
	FS_INFO.ninodeblocks = ninodeblocks;
	FS_INFO.ninodes = block.super.ninodes;

	// inode block, isvalid = 0
	memset(block.data, 0, DISK_BLOCK_SIZE);//setez toate elem din block.data=0
	
	for(int i=1; i<=ninodeblocks; i++){
		disk_write(i, block.data);
		//initialitez toate inodurile ca fiind invalide, eliberenad astfel orice date vechi
		//care ar fi putut fi stocate in blocuri
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

			if(!block.inode[j].isvalid) //daca inodul nu este valid sar peste
				continue;

			struct fs_inode *inode = &block.inode[j]; 
			int inode_id = i*INODES_PER_BLOCK + j;//get inode id

			printf("inode %d:\n", inode_id);
			printf("    size: %d bytes\n", inode->size);

			if(!inode->size) //daca size==0 sar peste el
				continue;

			// output the direct blocks ids
			int inode_blocks = (inode->size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;
			int k=0;
			printf("    direct blocks: ");

			for(; k<POINTERS_PER_INODE && k<inode_blocks; k++){
				printf("%d ", inode->direct[k]);
			}
			printf("\n");

			
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

	// scan the filesystem and mark it;
	//for in care trebuie sa: marchez in free_inode_bitmap fiecare inode valid ca fiind utilizat
	//si in free_block_bitmap fiecare block direct si indirect care este utilizat de fiecare inode.
	//actualizeaza nr liber de locuri disponibile in sisten.
	for(int i=0; i<ninodeblocks; i++){

		disk_read(i+1, block.data);
		for(int j=0; j < INODES_PER_BLOCK; j++){

			if(!block.inode[j].isvalid) 
				continue;
			int inode_id = i*INODES_PER_BLOCK + j;

			FS_INFO.free_inode_bitmap[inode_id] = 1; // mark the inode;
			struct fs_inode *inode = &block.inode[j];
			if(!inode->size) 
				continue;

			//numarul de blocuri de date necesare pentru a stoca date
			int inode_blocks = (inode->size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;
			
			//marchez fiecare bloc de date utilizat ca fiind ocupat
			for(int k=0; k<POINTERS_PER_INODE && k < inode_blocks; k++)
			{
				FS_INFO.free_block_bitmap[inode->direct[k]] = 1;
				FS_INFO.free_blocks--;
			}
				

			//vf daca exista un block indirect. True=> marchez ca fiind folosit in bitmap
			//apoi citesc indirect_block de pe disc si marchez fiecare dataBlock ca fiind folosit.
			if(inode_blocks > POINTERS_PER_INODE && inode->indirect){

				FS_INFO.free_block_bitmap[inode->indirect] = 1;
				FS_INFO.free_blocks--;
				union fs_block indirect_block;
				disk_read(inode->indirect, indirect_block.data);
				for(int k=0; k<inode_blocks; k++)
				{
					FS_INFO.free_block_bitmap[indirect_block.pointers[k - POINTERS_PER_INODE]] = 1;
					FS_INFO.free_blocks--;
				}
					
			}
		}
	}

	FS_INFO.mounted = 1;
	return FS_CONFIG_SUCCESS;
}

static void inode_load( int inumber, struct fs_inode *inode )
{
	// incarc un inode specificat inumber;
	int block_id = inumber/INODES_PER_BLOCK + 1;
	int inner_block_inode_id = inumber%INODES_PER_BLOCK;

	union fs_block block;
	disk_read(block_id, block.data);
	memcpy(inode, &block.inode[inner_block_inode_id], sizeof(struct fs_inode));
}

static void inode_save( int inumber, struct fs_inode *inode ) {
	//salvare inode
	int block_id = inumber/INODES_PER_BLOCK + 1;
	int inner_block_inode_id = inumber%INODES_PER_BLOCK;

	union fs_block block;
	disk_read(block_id, block.data);
	memcpy(&block.inode[inner_block_inode_id], inode, sizeof(struct fs_inode));
	disk_write(block_id, block.data);
}

int fs_create()
{
	//caut un inode liber si il creez. apoi returnez indexul sau.
	if(!FS_INFO.mounted)
		return FS_OPERATION_FAIL;

	for(int i=0; i<FS_INFO.ninodes; i++){
		// find a free inode;
		if(FS_INFO.free_inode_bitmap[i]) 
			continue;
		
		FS_INFO.free_inode_bitmap[i] = 1;

		struct fs_inode inode;
		inode_load(i, &inode);
		inode.isvalid = 1;
		inode.size = 0;
		inode_save(i, &inode);

		return i; // the inode number;
	}
	return FS_OPERATION_FAIL;
}

static int check_inode_number(int inumber){
	if(inumber<0 || inumber >= FS_INFO.ninodes)
		return FS_CONFIG_SUCCESS;
	return FS_CONFIG_SUCCESS;
}

int fs_delete( int inumber )
{
	if(!FS_INFO.mounted)
		return FS_CONFIG_FAIL;

	if(!check_inode_number(inumber))
		return FS_CONFIG_FAIL;
	
	if(!FS_INFO.free_inode_bitmap[inumber])
		return FS_CONFIG_FAIL;

	FS_INFO.free_inode_bitmap[inumber]	= 0;
	
	struct fs_inode inode; 
	inode_load(inumber, &inode);
	inode.isvalid = 0;
	int inode_blocks = (inode.size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;
	

	for(int k=0;k<inode_blocks && k<POINTERS_PER_INODE; k++)
	{
		// marcheaza block free
		FS_INFO.free_block_bitmap[inode.direct[k]] = 0;
		FS_INFO.free_blocks++;
		// set direct block 0
		inode.direct[k] = 0;
	}

	//eliberez spatiul alocat pt blocurile indirecte utilizate 
	if(inode_blocks > POINTERS_PER_INODE && inode.indirect)
	{
		union fs_block indirect_block;
		disk_read(inode.indirect, indirect_block.data);

		for(int k=0; k<inode_blocks; k++)
		{
			FS_INFO.free_block_bitmap[indirect_block.pointers[k - POINTERS_PER_INODE]] = 0;
			FS_INFO.free_blocks++;
		}

		// mark indirect block free
		FS_INFO.free_block_bitmap[inode.indirect] = 0;
		FS_INFO.free_blocks++;
		// set indrect block 0;
		inode.indirect = 0;
	}
	inode.size = 0;
	inode_save(inumber, &inode);
	return FS_CONFIG_SUCCESS;
}

//pt a obtine nr. blockului corespunzator unui anumit index
static int translate_block(struct fs_inode* inode, int inner_block_id){
	if(inner_block_id<POINTERS_PER_INODE)
		return inode->direct[inner_block_id];


	assert(inode->indirect!=0);//ne asiguram ca inodul are un pointer direct setat

	union fs_block block;
	disk_read(inode->indirect, block.data);
	return block.pointers[inner_block_id - POINTERS_PER_INODE];
}

static int find_free_block(){
	for(int i=0; i<FS_INFO.nblocks; i++)
		if(!FS_INFO.free_block_bitmap[i])
			return i;
	return 0;
}



int fs_write( int inumber, const char *data, int length, int offset )
{
	if(!FS_INFO.mounted)
		return FS_CONFIG_FAIL;

	if(!check_inode_number(inumber))
		return FS_CONFIG_FAIL;
	
	if(!FS_INFO.free_inode_bitmap[inumber])
		return FS_CONFIG_FAIL;

	if(length<=0)
		return 0;

	struct fs_inode inode; 
	inode_load(inumber, &inode);

	int start = offset;
	int end = offset + length;

	//nr de blocuri existente in prezent pe disk pt un anumit inode
	int exists_block = (inode.size + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE; 
	//nt total de blocuri  necesar
	int all_blocks = (MAX(end, inode.size) + DISK_BLOCK_SIZE - 1) /DISK_BLOCK_SIZE;

	int needed = all_blocks - exists_block;

	if(all_blocks > POINTERS_PER_INODE + POINTERS_PER_BLOCK)
		return FS_CONFIG_FAIL;
	
	if(needed > FS_INFO.free_blocks)
		return FS_CONFIG_FAIL;

	if(all_blocks > exists_block){
		// create the indirect block if necessory;
		//daca am mai multe blocuri decat exista, va trebui sa fac un indirect pointer
		union fs_block* indirect_block = (union fs_block*) calloc(1, sizeof(union fs_block));

		if(exists_block<=POINTERS_PER_INODE && all_blocks > POINTERS_PER_INODE){
			int block_id = find_free_block();
			assert(block_id!=0);
			FS_INFO.free_block_bitmap[block_id] = 1;
			FS_INFO.free_blocks--;
			inode.indirect = block_id;
		}else if(all_blocks > POINTERS_PER_INODE){
			disk_read(inode.indirect, indirect_block->data);
		}

		// find free blocks;
		for(int i=0; i<needed; i++){
			int block_id = find_free_block();
			assert(block_id!=0);
			FS_INFO.free_block_bitmap[block_id] = 1;
			FS_INFO.free_blocks--;
			// new_blocks[new_blocks_p++] = block_id;
			int inner_block_id = exists_block + i;
			if(inner_block_id < POINTERS_PER_INODE){
				inode.direct[inner_block_id] = block_id;
			}else{
				indirect_block->pointers[inner_block_id - POINTERS_PER_INODE] = block_id;
			}
		}

		//daca depasesc dimensiunea, imi va scrie in indirect
		if(all_blocks > POINTERS_PER_INODE)
			disk_write(inode.indirect, indirect_block->data);

		inode.size = end; 
		inode_save(inumber, &inode);
	}

	int start_block = start / DISK_BLOCK_SIZE;
	int start_offset = start % DISK_BLOCK_SIZE;
	int end_block = end  / DISK_BLOCK_SIZE;
	int end_offset = end % DISK_BLOCK_SIZE;

	union fs_block block;
	//daca nu mai am mem pe disk
	if(start_block == end_block){
		int block_id = translate_block(&inode, start_block);
		disk_read(block_id, block.data);
		memcpy(block.data + start_offset, data, end_offset - start_offset);
		disk_write(block_id, block.data);
		return end_offset - start_offset;
	}

	int write_len = 0;

	//scrie primul block
	if(start_offset!=0){

		int block_id = translate_block(&inode, start_block);
		disk_read(block_id, block.data);
		//scrie datele din data in zona de mem specificata. 
		memcpy(block.data + start_offset, data, DISK_BLOCK_SIZE - start_offset);
		disk_write(block_id, block.data);
		write_len += DISK_BLOCK_SIZE - start_offset;
	}else{
		int block_id = translate_block(&inode, start_block);
		disk_write(block_id, data);
		write_len += DISK_BLOCK_SIZE;
	}
	
	
	int i;
	//
	for(i = start_block + 1; i<=end_block; i++){
		int block_id = translate_block(&inode, i);
		if(i== end_block && end_offset != 0){ // end_offset == 0 => sfarsit
			//sunt pe ultimul block dar nu am cursorul pe end_offset
			//se citeste blocul si se copiaza doar o parte din datele furnizate in el
			disk_read(block_id, block.data);
			memcpy(block.data, data + write_len, end_offset);
			disk_write(block_id, block.data);
			write_len += end_offset;
		}else if(i < end_block) {
			//daca nu sunt pe ultimul bloc scriu intreg blocul pe disc.
			disk_write(block_id, data + write_len);
			write_len += DISK_BLOCK_SIZE;
		}
	}

	return write_len;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if(!FS_INFO.mounted)
		return FS_CONFIG_FAIL;

	if(!check_inode_number(inumber))
		return FS_CONFIG_FAIL;
	
	if(!FS_INFO.free_inode_bitmap[inumber])
		return FS_CONFIG_FAIL;

	if(length<=0)
		return 0;

	struct fs_inode inode; 
	inode_load(inumber, &inode);	

	int start = offset;
	int end = offset + length > inode.size? inode.size: offset + length;

	int start_block = start / DISK_BLOCK_SIZE;
	int start_offset = start % DISK_BLOCK_SIZE;
	int end_block = end  / DISK_BLOCK_SIZE;
	int end_offset = end % DISK_BLOCK_SIZE;

	union fs_block block;
	if(start_block == end_block){
		//copiez datele dorita in data. 
		disk_read(translate_block(&inode, start_block), block.data);
		memcpy(data, block.data + start_offset, end_offset - start_offset);
		return end_offset - start_offset;//returnez cat am reusit sa citesc
		
	}

	int read_len = 0;
	// read start bloc
	if(start_offset!=0){
		disk_read(translate_block(&inode, start_block), block.data);
		memcpy(data,  block.data + start_offset, DISK_BLOCK_SIZE - start_offset);
		read_len += DISK_BLOCK_SIZE - start_offset;
	}else{
		disk_read(translate_block(&inode, start_block), data);
		read_len += DISK_BLOCK_SIZE;
	}

	// read inner blocks
	for(int i = start_block + 1; i< end_block; i++){
		disk_read(translate_block(&inode, i), data + read_len);
		read_len += DISK_BLOCK_SIZE;
	}

	// read end block
	disk_read(translate_block(&inode, end_block), block.data);
	memcpy(data + read_len,  block.data, end_offset);
	read_len += end_offset;
	return read_len;
	

}