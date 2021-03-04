all: 
	mpicc -O -o a.out main.c

run: 
	mpirun -n $(n) -f hosts ./a.out $(args)

push:
	git add -A
	git commit -m "Sync"
	git push
pull:
	rm -f a.out
	git pull
