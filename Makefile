all : miniosctest

miniosctest : miniosctest.c
	gcc -o $@ $^ -lm -Wall

clean :
	rm -rf miniosctest