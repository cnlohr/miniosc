all : miniosctest

miniosctest : miniosctest.c
	gcc -o $@ $^ -Wall

clean :
	rm -rf miniosctest