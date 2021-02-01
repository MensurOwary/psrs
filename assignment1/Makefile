all:
	gcc -Wall -O main2.c -lpthread -o main
	./main $(args)
compile:
	gcc -Wall -O main2.c -lpthread -o main
cd:
	gcc -Wall -g main2.c -lpthread -o main
run: 
	./main $(args)
debug:
	gcc -Wall -g main2.c -lpthread -o main
	gdb ./main
gprof:
	gcc -Wall -pg main2.c -lpthread -o main
