CC=gcc
TARGET=main.o
CFLAGS=-Wall -O
LFLAGS=-lpthread

all:
	$(CC) $(CFLAGS) main.c $(LFLAGS) -o $(TARGET)
	./main $(args)
compile:
	$(CC) $(CFLAGS) main.c $(LFLAGS) -o $(TARGET)
run: 
	./main $(args)
clean:
	rm *.o 
