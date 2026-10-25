# Old Photo - Parallel (Version B) and Pipeline

Concurrent Programming project (MEEC 21/22). The program applies an "old
photo" effect to a set of JPEG images using two different concurrency
strategies, and the repository also contains the timing results collected on
three different machines.

The effect is built from four image transformations, applied in this order:

  1. Contrast reduction
  2. Smoothing
  3. Paper texture overlay
  4. Sepia colouring


## Repository structure

old-photo-paralelo-B/
  README.md
  src/
    old-photo-paralelo-B.c   Thread pool version (B)
    old-photo-pipeline.c     4-stage pipeline version
    image-lib.c / .h         Image helpers (libgd) and file utilities
    Makefile
  timmings/
    HP/                      Results from an HP machine
    Mac M1/                  Results from a Mac with an M1 chip
    PC/                      Results from a PC
      (each one contains Dataset 1 to 4 results)


## Requirements

  - GCC
  - libgd (GD graphics library) and its development headers
  - A POSIX system (pthreads, pipes)

The programs expect a texture image named paper-texture.png in the directory
from which they are run.


## Build

From the src/ directory:

  make old-photo-paralelo-B     builds only the thread pool version
  make old-photo-pipeline       builds only the pipeline version
  make                          builds both (same as "make all")
  make clean                    removes both executables


## Input format

Both programs receive a directory containing:

  - image-list.txt: a text file with one image file name per line
  - the JPEG images (.jpeg or .jpg) listed in that file

Entries that are not JPEG files, that cannot be found, or that were already
processed (a file with the same name already exists in the output directory)
are skipped.


## Usage

Thread pool version:

  ./old-photo-paralelo-B <files_dir> <nn_threads>

Pipeline version:

  ./old-photo-pipeline <files_dir>

Examples:

  ./old-photo-paralelo-B ./Dataset1 8
  ./old-photo-pipeline ./Dataset1


## How it works

Thread pool version (old-photo-paralelo-B)
  - Creates nn_threads worker threads.
  - The main thread sends the image file names to the workers through an
    input pipe, followed by one termination marker per thread.
  - Each worker takes a file name, applies the four transformations to the
    image, writes the result and reports the processing time of that image
    through an output pipe.
  - Workers return the number of images they processed and their total run
    time.
  - Output images are saved in <files_dir>/old_photo_PAR_B/.

Pipeline version (old-photo-pipeline)
  - Uses four threads, one per stage: contrast, smooth, texture and sepia.
  - Stages are connected by pipes. Image indices enter the first stage and
    image pointers are passed from stage to stage.
  - The main thread collects the finished images from the last stage and
    writes them to disk.
  - Start and end timestamps of each image are recorded.
  - Output images are saved in <files_dir>/old_photo_PIPELINE/.


## Timing output

Each run writes a timing file inside the input directory (only if it does not
already exist):

  timming_<nn_threads>.txt    thread pool version
  timming_pipeline.txt        pipeline version

Thread pool version file:
  - one line per image with its processing time
  - one line per thread with the number of images processed and its run time
  - a final line with the number of images and the total time

Pipeline version file:
  - one start line and one end line per image, relative to program start
  - a final line with the total time

Times are in seconds. The program also prints the duration of the sequential
initialisation (seq), the parallel part (par) and the final sequential part
(seq2) to the standard output.


## Timing results

The timmings/ folder contains the collected results, organised as:

  timmings/<machine>/<dataset>/timming_<N>.txt   with N = 1, 2, 4, 8, 16, 32
  timmings/<machine>/<dataset>/timming_pipeline.txt

Machines: HP, Mac M1 and PC.
Datasets: four image sets (the first with 10 images, the others with 32).
