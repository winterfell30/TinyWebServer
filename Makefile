CC = gcc
CFLAGS = -O2 -Wall -I .

#使用了lpthread，这个LIB选项其他系统可能有所不同
LIB = -lpthread

all: main cgi

main: main.c helper.o
	$(CC) $(CFLAGS) -o main main.c helper.o $(LIB)

helper.o: helper.c
	$(CC) $(CFLAGS) -c helper.c

cgi:
	(cd cgi-bin; make)

clean:
	rm -f *.o main *~
	(cd cgi-bin; make clean)
