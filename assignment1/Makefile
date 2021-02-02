all:
	gcc -Wall -O main.c -lpthread -o main
	./main $(args)
compile:
	gcc -Wall -O main.c -lpthread -o main
cd:
	gcc -Wall -g main.c -lpthread -o main
run: 
	./main $(args)
debug:
	gcc -Wall -g main.c -lpthread -o main
	gdb ./main
gprof:
	gcc -Wall -pg main.c -lpthread -o main
