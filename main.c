#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

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
		  else if(!strcmp(cmd,"help")) {
			printf("FS commands:\n");
			printf("    format\n");
			printf("    mount\n");
			printf("    debug\n");
			printf("    create\n");
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