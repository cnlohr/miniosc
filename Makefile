all : miniosctest

miniosctest : miniosctest.c
	$(CC) -o $@ $^ -lm -Wall

clean :
	rm -rf miniosctest
