all: 
	mpicc -o main main.c

run: 
	mpirun -n 2 -f hosts ./main

push:
	git add -A
	git commit -m "Sync"
	git push
pull:
	git pull
