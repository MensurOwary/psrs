all: 
	mpicc -o a.out main.c

run: 
	mpirun -n 3 -f hosts ./a.out 36

push:
	git add -A
	git commit -m "Sync"
	git push
pull:
	rm -f a.out
	git pull
