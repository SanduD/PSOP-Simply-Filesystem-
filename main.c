#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

//comment
int copyInContent( const char *filename, int inumber )
{
	FILE *file;
	int offset=0, result, actual;
	char buffer[16384];

	file = fopen(filename,"r");
	if(!file) {
		printf("couldn't open %s: %s\n",filename,strerror(errno));
		return 0;
	}

	while(1) {
		result = fread(buffer,1,sizeof(buffer),file);
		if(result<=0) break;
		if(result>0) {
			actual = fs_write(inumber,buffer,result,offset);
			if(actual<0) {
				printf("ERROR: fs_write return invalid result %d\n",actual);
				break;
			}
			offset += actual;
			if(actual!=result) {
				printf("WARNING: fs_write only wrote %d bytes, not %d bytes\n",actual,result);
				break;
			}
		}
	}

	printf("%d bytes copied\n",offset);

	fclose(file);
	return 1;
}
static int copyOUTContent( int inumber, const char *filename );

int main( int argc, char *argv[] )
{
	char line[1024];
	char cmd[1024];
	char arg1[1024];
	char arg2[1024];
	int args,inumber;
	//int result;

	if(argc!=3) {
		printf("use: %s <diskfile> <nblocks>\n",argv[0]);
		return 1;
	}

	if(!disk_init(argv[1],atoi(argv[2]))) {
		printf("couldn't initialize %s: %s\n",argv[1],strerror(errno));
		return 1;
	}

	printf("opened emulated disk image %s with %d blocks\n",argv[1],disk_size());

	while(1) {
		printf(" simplefs> ");
		fflush(stdout);

		if(!fgets(line,sizeof(line),stdin)) break;

		if(line[0]=='\n') continue;
		line[strlen(line)-1] = 0;

		args = sscanf(line,"%s %s %s",cmd,arg1,arg2);
		if(args==0) continue;

		if(!strcmp(cmd,"format")) {
			if(args==1) {
				if(fs_format()) {
					printf("disk formatted.\n");
				} else {
					printf("format failed!\n");
				}
			} else {
				printf("use: format\n");
			}
		} else if(!strcmp(cmd,"mount")) {
			if(args==1) {
				if(fs_mount()) {
					printf("disk mounted.\n");
				} else {
					printf("mount failed!\n");
				}
			} else {
				printf("use: mount\n");
			}
		} else if(!strcmp(cmd,"debug")) {
			if(args==1) {
				fs_debug();
			} else {
				printf("use: debug\n");
			}
		} else if(!strcmp(cmd,"create")) {
			if(args==1) {
				inumber = fs_create();
				/* Bug fixed on April 30th: check for inumber>=0 */
				if(inumber>=0) {
					printf("created inode %d\n",inumber);
				} else {
					printf("create failed!\n");
				}
			} else {
				printf("use: create\n");
			}
			
		}
		else if(!strcmp(cmd,"copyin")) {
			if(args==3) {
				inumber = atoi(arg2);
				if(copyInContent(arg1,inumber)) {
					printf("copied file %s to inode %d\n",arg1,inumber);
				} else {
					printf("copy failed!\n");
				}
			} else {
				printf("use: copyin <filename> <inumber>\n");
			}

		}
		else if(!strcmp(cmd,"delete")) {
			if(args==2) {
				inumber = atoi(arg1);
				if(fs_delete(inumber)) {
					printf("inode %d deleted.\n",inumber);
				} else {
					printf("delete failed!\n");	
				}
			} else {
				printf("use: delete <inumber>\n");
			}
		}
		else if(!strcmp(cmd,"copyout")) {
			if(args==3) {
				inumber = atoi(arg1);
				if(copyOUTContent(inumber,arg2)) {
					printf("copied inode %d to file %s\n",inumber,arg2);
				} else {
					printf("copy failed!\n");
				}
			} else {
				printf("use: copyout <inumber> <filename>\n");
			}

		}
		  else if(!strcmp(cmd,"help")) {
			printf("FS commands:\n");
			printf("    format\n");
			printf("    mount\n");
			printf("    debug\n");
			printf("    create\n");
			printf("    delete  <inode>\n");
			printf("    copyin  <file> <inode>\n");
			printf("    copyout <inode> <file>\n");
			printf("    help\n");
			printf("    exit\n");
		} else if(!strcmp(cmd,"exit")) {
			break;
		} else {
			printf("unknown command: %s\n",cmd);
			printf("type 'help' for a list of commands.\n");
			//result = 1;
		}
	}

	printf("closing emulated disk.\n");
	disk_close();

	return 0;
}
static int copyOUTContent( int inumber, const char *filename )
{
	FILE *file;
	int offset=0, result;
	char buffer[16384];

	file = fopen(filename,"w");
	if(!file) {
		printf("couldn't open %s: %s\n",filename,strerror(errno));
		return 0;
	}

	while(1) {
		result = fs_read(inumber,buffer,sizeof(buffer),offset);
		if(result<=0) break;
		fwrite(buffer,1,result,file);
		offset += result;
	}

	printf("%d bytes copied\n",offset);

	fclose(file);
	return 1;
}