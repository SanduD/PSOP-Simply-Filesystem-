
main: main.o libfile_system.a
	gcc main.o -o main -lfile_system -L .

libfile_system.a: fs.o disk.o
	ar rc libfile_system.a fs.o disk.o

main.o: main.c
	gcc -c main.c -o main.o

fs.o:fs.c
	gcc -c fs.c -o fs.o

disk.o:disk.c
	gcc -c disk.c -o disk.o

clean:
	rm *.o *.a main
	clear