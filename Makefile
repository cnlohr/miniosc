all : miniosctest

ifeq ($(OS),Windows_NT)
LDFLAGS:=-lws2_32 -lm
else
LDFLAGS:=-lm
endif

CFLAGS:=-Wall

miniosctest : miniosctest.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf miniosctest
