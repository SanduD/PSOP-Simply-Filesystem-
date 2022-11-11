
void thread_fs_info(void *arg)
{
    char *diskname=arg;

	if (fs_mount(diskname))
		printf("Cannot mount diskname");

	fs_info();

	if (fs_umount())
		printf("Cannot unmount diskname");
}
int main(int argc, char **argv)
{
    int i;
	char *cmd;
    cmd=argv[1];
    printf("%s",cmd);
	
    if(strcmp(cmd,"info")==0)
        thread_fs_info("driver1");
	
}