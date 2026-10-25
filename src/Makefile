CC := gcc
CFLAGS := -Wall -g -lgd

all: old-photo-paralelo-B old-photo-pipeline

old-photo-paralelo-B: old-photo-paralelo-B.c image-lib.c image-lib.h
	$(CC) old-photo-paralelo-B.c image-lib.c -o old-photo-paralelo-B $(CFLAGS)
old-photo-pipeline: old-photo-pipeline.c image-lib.c image-lib.h
	$(CC) old-photo-pipeline.c image-lib.c -o old-photo-pipeline $(CFLAGS)

clean:
	rm -rf old-photo-paralelo-B old-photo-pipeline