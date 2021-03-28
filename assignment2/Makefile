CC=mpicc
TARGET=a.out
CFLAGS=-Wall -O
SOURCES=main.c
MR=mpirun
MRARGS=-n $(n) -f hosts

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
compile:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
run: 
	$(MR) $(MRARGS) ./$(TARGET) $(args)
clean:
	rm $(TARGET) 
