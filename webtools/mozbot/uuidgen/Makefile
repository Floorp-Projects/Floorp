CFLAGS=-g

OBJS=md5.o token.o main.o

all: $(OBJS) uuidgen

uuidgen: $(OBJS)
	gcc -o uuidgen $(OBJS)

md5.o: md5.c md5.h

token.o: token.c token.h

main.o: main.c

clean:
	rm -f *.o *~ core uuidgen
