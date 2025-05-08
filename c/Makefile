# c/Makefile
slice: slice.c
	$(CC) -Wall -O2 -o slice slice.c

slice1: slice1.c
	$(CC) -Wall -O2 -o slice1 slice1.c

slice2: slice2.c
	$(CC) -Wall -O2 -o slice2 slice2.c

slice3: slice3.c
	$(CC) -Wall -O2 -o slice3 slice3.c

slice4: slice4.c
	$(CC) -Wall -O2 -o slice4 slice4.c

linex: linex.c
	$(CC) -Wall -O2 -o linex linex.c

all: slice slice1 slice2 slice3 slice4 linex

clean:
	rm -f slice slice1 slice2 slice3 slice4 linex *.o

.PHONY: clean all