#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "disk.h"

#define DISK_MAGIC 0xdeadbeef

static FILE *diskfile;
static int nblocks=0;
static int nreads=0;
static int nwrites=0;


//initializez disk emulator cu nr de blocuri introduse de utilizator
int disk_init( const char *filename, int n )
{
	diskfile = fopen(filename,"r+");
	if(!diskfile) diskfile = fopen(filename,"w+");
	if(!diskfile) return 0;

	ftruncate(fileno(diskfile),n*DISK_BLOCK_SIZE);
	//set the size of the emulated disk
	nblocks = n;//nr de blocuri din emulated disk
	//trebuie sa retin cate blocuri am citit/scris
	nreads = 0; 
	nwrites = 0;

	return 1;
}

int disk_size()
{
	return nblocks;
}

static void sanity_check( int blocknum, const void *data )
{
	//verific inainte de a face o operatie de read sau write pe disk
	if(blocknum<0) {
		printf("ERROR: blocknum (%d) is negative!\n",blocknum);
		abort();
	}

	if(blocknum>=nblocks) {
		printf("ERROR: blocknum (%d) is too big!\n",blocknum);
		abort();
	}

	if(!data) {
		printf("ERROR: null data pointer!\n");
		abort();
	}
}

void disk_read( int blocknum, char *data )
{
	
	sanity_check(blocknum,data);

	fseek(diskfile,blocknum*DISK_BLOCK_SIZE,SEEK_SET);
	//set the pointer to the current block

	if(fread(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nreads++;
	} else {
		printf("ERROR: couldn't access simulated disk: %s\n",strerror(errno));
		abort();
	}
}

void disk_write( int blocknum, const char *data )
{
	sanity_check(blocknum,data);

	fseek(diskfile,blocknum*DISK_BLOCK_SIZE,SEEK_SET);

	if(fwrite(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nwrites++;
	} else {
		printf("ERROR: couldn't access simulated disk: %s\n",strerror(errno));
		abort();
	}
}

void disk_close()
{
	if(diskfile) {
		printf("%d disk block reads\n",nreads);
		printf("%d disk block writes\n",nwrites);
		fclose(diskfile);
		diskfile = 0;
	}
}
