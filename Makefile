CC      := gcc 
CFLAGS  := -Wall 
CFLAGS  += -c

libfile_system.a: disk.o fs.o
	ar -rc libfile_system.a disk.o fs.o

%.x: %.o libfile_system.a
	$(Q)$(CC) $(CFLAGS) $^ -o $@ -L. -lfile_system

%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@


clean:
	rm *.o *.a