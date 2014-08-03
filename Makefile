
CFLAGS=-std=c11 -Wall -g
LDFLAGS=-lm
OPTFLAGS=-O3 -ffast-math -fno-finite-math-only -fno-strict-aliasing -msse2 -mfpmath=sse -fopenmp

all: test

test: main.c wtf.h noiseprofile.h
	$(CC) $(CFLAGS) $(OPTFLAGS) main.c $(LDFLAGS) -o test
