GCC=/usr/bin/gcc

simplefs: main.o fs.o disk.o
	$(GCC) main.o fs.o disk.o -o simplefs

main.o: main.c
	$(GCC) -Wall main.c -c -o main.o -g

fs.o: fs.c fs.h
	$(GCC) -Wall fs.c -c -o fs.o -g

disk.o: disk.c disk.h
	$(GCC) -Wall disk.c -c -o disk.o -g

clean:
	rm simplefs disk.o fs.o main.o