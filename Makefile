CFLAGS=-g
CFLAGS+= $(shell pkg-config --cflags libpng)
LDFLAGS+= $(shell pkg-config --libs libpng)

all: ql570.c
	gcc $(CFLAGS) -o ql570 ql570.c $(LDFLAGS)

clean:
	rm ql570
