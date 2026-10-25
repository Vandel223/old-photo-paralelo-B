/******************************************************************************
 * Programacao Concorrente
 * MEEC 21/22
 *
 * Projecto - Parte1
 *                           serial-complexo.c
 * 
 * Compilacao: gcc serial-complexo -o serial-complex -lgd
 *           
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "image-lib.h"

/* the directories wher output files will be placed */
#define OLD_IMAGE_DIR "/old_photo_PAR_B"
/* the paper texture file path */
#define PAPER_TEXTURE "./paper-texture.png"

/******************************************************************************
 * struct retPack
 *
 * Atributes:	cnt - 		number of files read
 * 				times - 	time struct storing thread execution time
 *
 * Description: struct to store how many files were read and time of execution
 * 				of each thread
 *
 *****************************************************************************/
typedef struct {

	struct timespec times;
	int cnt;

} retPack;

/******************************************************************************
 * struct imageTime
 *
 * Atributes:	idx - 		index of the image in files
 * 				times - 	time struct storing image execution time
 *
 * Description: struct to store time taken to process a given image
 *
 *****************************************************************************/
typedef struct {

	struct timespec times;
	char *filename;

} imageTime;

/* declare all global variables */
char *dir;				/* directory passed as argument */
char **files;			/* files in given directory to be processed */
gdImagePtr texture;		/* texture image */
int nn_files = 0;
int nn_threads = 0;
int pipe_in[2];			/* pipe for input */
int pipe_out[2];		/* pipe for output */

/******************************************************************************
 * oldFilter()
 *
 * Arguments:	args - 		a pointer to a struct with all the args
 * 							(as in argsPack):
 * 					rem -	essentially the thread number that decides what
 * 							images to process
 *
 * Return:		(void *)	ret -	a pointer with all return information
 * 									(as in retPack):
 * 								cnt - file counter
 * 								times - execution time
 * 
 * Description: iterates through files[st] to files[end] (excluding), checks
 * 				if file was already processed before.
 * 				Then tries to get JPEG image out of fileand applies a old photo
 * 				filter to it.
 *
 *****************************************************************************/
void *oldFilter(void *args) {

	struct timespec start_time_thread, end_time_thread;
	struct timespec start_time_image, end_time_image;

	clock_gettime(CLOCK_MONOTONIC, &start_time_thread);

	char *filename;

	int cnt = 0;			/* counter of processed files */

	/* declare image ptrs */
	gdImagePtr img;

	gdImagePtr contrastImage;
	gdImagePtr smoothImage;
	gdImagePtr textureImage;
	gdImagePtr sepiaImage;

	char outFileName[128];

	while (1){	

		read(pipe_in[0], &filename, sizeof(filename));

		if (filename == -1) break;

		clock_gettime(CLOCK_MONOTONIC, &start_time_image);

		/* outFileName */
		sprintf(outFileName, "%s%s%s", dir,  OLD_IMAGE_DIR, strrchr(filename, '/'));

		fprintf(stdout, "%s\n", filename);

		/* load of the input file */
	    img = read_jpeg_file(filename);
		if (img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", filename);

			clock_gettime(CLOCK_MONOTONIC, &end_time_image);

			imageTime aux;
			aux.times = diff_timespec(&end_time_image, &start_time_image);
			aux.filename = filename;

			/* still output image time */
			write(pipe_out[1], &aux, sizeof(aux));

			continue;
		}

		/* increment files read */
		cnt++;

		/* apply filter */
		contrastImage = contrast_image(img);
		gdImageDestroy(img);
		smoothImage = smooth_image(contrastImage);
		gdImageDestroy(contrastImage);
		textureImage = texture_image(smoothImage, texture);
		gdImageDestroy(smoothImage);
		sepiaImage = sepia_image(textureImage);
		gdImageDestroy(textureImage);

		/* save resized */ 
		if(write_jpeg_file(sepiaImage, outFileName) == 0){
			fprintf(stderr, "Impossible to write %s image\n", outFileName);
		}
		gdImageDestroy(sepiaImage);

		clock_gettime(CLOCK_MONOTONIC, &end_time_image);

		imageTime aux;
		aux.times = diff_timespec(&end_time_image, &start_time_image);
		aux.filename = filename;

		/* write the image times */
		write(pipe_out[1], &aux, sizeof(aux));

	}

	clock_gettime(CLOCK_MONOTONIC, &end_time_thread);

	retPack *ret = (retPack *) malloc(sizeof(retPack));
	ret->times = diff_timespec(&end_time_thread, &start_time_thread);
	ret->cnt = cnt;

	return (void *) ret;

}

/******************************************************************************
 * main()
 *
 * Arguments: (none)
 * Returns: 0 in case of sucess, positive number in case of failure
 * Side-Effects: creates thumbnail, resized copy and watermarked copies
 *               of images
 *
 * Description: implementation of the complex serial version 
 *              This application only works for a fixed pre-defined set of files
 *
 *****************************************************************************/
int main(int argc, char **argv){
    struct timespec start_time_total, end_time_total;
    struct timespec start_time_seq, end_time_seq;
    struct timespec start_time_par, end_time_par;
	struct timespec start_time_seq2, end_time_seq2;

	clock_gettime(CLOCK_MONOTONIC, &start_time_total);
	clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

	/* if argc isn't three we quit*/
	if (argc != 3) {
		fprintf(stdout, "\n\tUse the command:\n\n\t.old-photo-paral <files_dir> <nn_threads>\n\n");
		exit(0);
	}

	dir = argv[1];						/* directory of files */

	if (dir[strlen(dir) - 1] == '/') dir[strlen(dir) - 1] = '\0';
 
	/* check if directory is valid */
	if (!isDirExists(dir)) {
		fprintf(stderr, "The directory provided wasn't found - %s\n", dir);
		exit(1);
	}

	files = readFiles(dir, &nn_files, OLD_IMAGE_DIR);	/* read files list */
	nn_threads = atoi(argv[2]);

	/* array of threads */
	pthread_t threads[nn_threads];

	/* return of threads */
	retPack *retThreads[nn_threads];

	/* creation of output directories */
	char oldImgsPath[strlen(dir) + 18]; 
	sprintf(oldImgsPath, "%s%s", dir, OLD_IMAGE_DIR);
	if (create_directory(oldImgsPath) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", oldImgsPath);
		exit(-1);
	}

	texture = read_png_file(PAPER_TEXTURE);

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
	clock_gettime(CLOCK_MONOTONIC, &start_time_par);

	/* create pipe */
	if (pipe(pipe_in) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
	if (pipe(pipe_out) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}

	/* Iteration over all the threads
	 */
	for (int i = 0; i < nn_threads; i++){	

		/* initialize thread */
		pthread_create(&threads[i], NULL, oldFilter, NULL);

	}

	for (int i = 0; i < nn_files; i++) {

		/* write to pipe */
		write(pipe_in[1], &files[i], sizeof(files[i]));

	}

	long int term = -1;
	for (int i = 0; i < nn_threads; i++) {

		/* write to pipe */
		write(pipe_in[1], &term, sizeof(term));

	}


	/* Iteration over all the threads
	 */
	for (int i = 0; i < nn_threads; i++) {

		/* join thread */
		pthread_join(threads[i], (void *) &retThreads[i]);

	}

	clock_gettime(CLOCK_MONOTONIC, &end_time_par);
	clock_gettime(CLOCK_MONOTONIC, &start_time_seq2);

	gdImageDestroy(texture);

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq2);
	clock_gettime(CLOCK_MONOTONIC, &end_time_total);

char buffer[256];
	sprintf(buffer, "%s%s%d%s", dir, "/timming_", nn_threads, ".txt");
FILE *timing;
int tCnt = 0;
imageTime aux;

struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);
struct timespec seq2_time = diff_timespec(&end_time_seq2, &start_time_seq2);
struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);

if (!isFileExists(buffer)) {
	
	timing = fopen(buffer, "w");

	/* write to timing_<n>.txt */
	for (int i = 0; i < nn_threads; i++) {
		tCnt += retThreads[i]->cnt;
	}

	for (int i = 0; i < nn_files; i++) {

		/* read from output pipe, the image times */
		read(pipe_out[0], &aux, sizeof(aux));

		/* -> write for each image */
		fprintf(timing, "%s\t\t %10jd.%02ld\n", strrchr(aux.filename, '/') + 1 , aux.times.tv_sec, (long) aux.times.tv_nsec / 10000000);

	}

	/* -> write for each thread */
	for (int i = 0; i < nn_threads; i++) {
		fprintf(timing, "Thread_%d \t %d\t%10jd.%02ld\n", i, retThreads[i]->cnt, retThreads[i]->times.tv_sec, retThreads[i]->times.tv_nsec / 10000000);
		free(retThreads[i]);
	}

	/* -> write total */
	fprintf(timing, "total \t\t %d\t%10jd.%02ld\n", tCnt, total_time.tv_sec, (long) total_time.tv_nsec / 10000000);

	/* close timing_<n>.txt */
	fclose(timing);
}

else fprintf(stdout, "Found file:\t%s\n", buffer);

	destroyFiles(files, nn_files);

	/* write to stdout */
    printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
	printf("\tseq2 \t %10jd.%09ld\n", seq2_time.tv_sec, seq2_time.tv_nsec);

	exit(0);
}
