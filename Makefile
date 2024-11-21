 
all: respawn

respawn: respawn.c Makefile
	gcc -Wpedantic -ansi -O3 -o respawn respawn.c

clean:
	@rm respawn

strip: respawn
	@strip respawn
