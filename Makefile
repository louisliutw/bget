LD_FLAGS = -lcurl
CFLAGS = -ggdb -Wall
.PHONY: clean
.c.o:
	gcc -c $< $(CFLAGS) -o $@
all: bget

bget: bget.o
	gcc $< -ggdb ${LD_FLAGS} -o $@

clean:
	rm -rf *.o bget
