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
#define OLD_IMAGE_DIR "/old_photo_PIPELINE"
/* the paper texture file path */
#define PAPER_TEXTURE "./paper-texture.png"
/* one pipeline consisting of 4 stages */
#define NN_THREADS 4

/******************************************************************************
 * struct imageTimes
 *
 * Atributes:	idx - 		index of given image
 * 				times - 	time struct storing image start or end timestamp
 * 				end - 		decides if given struct is for end (1) or start (0)
 *
 * Description: struct to store timestamps of start and end of image processing
 *
 *****************************************************************************/
typedef struct {

	struct timespec times;
	int idx;
	int end;

} imageTimes;

/******************************************************************************
 * struct imageI
 *
 * Atributes:	idx - 		index of given image
 * 				img - 		pointer to image
 *
 * Description: struct to store image pointer and its index in files
 *
 *****************************************************************************/
typedef struct {

	gdImagePtr img;
	int idx;

} imageI;

/* declare all global variables */
char *dir;				/* directory passed as argument */
char **files;			/* files in given directory to be processed */
gdImagePtr texture_png;		/* texture image */
int nn_files = 0;
/* declare pipes */
int pipe_input[2];
int pipe_contrast[2];
int pipe_smooth[2];
int pipe_texture[2];
int pipe_sepia[2];
int pipe_times[2];
struct timespec st_program;


/******************************************************************************
 * contrast()
 *
 * 
 *
 *****************************************************************************/
void *contrast(void *args) {

	struct timespec st_time;

    gdImagePtr img, contrast_img;          /* image pointers */
	int idx;

	imageI imgI;

    while (1) {

		clock_gettime(CLOCK_MONOTONIC, &st_time);

	    /* get the the thread idx from the pipe */
	    read(pipe_input[0], &idx, sizeof(idx));

        /* exit condition */
        if (idx == -1) {
			imgI.idx = -1;
			imgI.img = NULL;
            write(pipe_contrast[1], &imgI, sizeof(imgI)); /* pass terminating signal */
            break;
        }

		fprintf(stdout, "%s\n", files[idx]);

		/* read the jpeg file */
		img = read_jpeg_file(files[idx]);
		if (img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", files[idx]); 

			/* still write out */
			imageTimes aux;
			aux.times = diff_timespec(&st_time, &st_program);
			aux.idx = idx;
			aux.end = 0;
			write(pipe_times[1], &aux, sizeof(aux));
			aux.end = 1;
			write(pipe_times[1], &aux, sizeof(aux));		/* consider that it starts and ends here */

			continue;
		}

        contrast_img = contrast_image(img);

        /* write the contrast image pointer onto next pipe - contrast pipe */
		imgI.img = contrast_img;
		imgI.idx = idx;
        write(pipe_contrast[1], &imgI, sizeof(imgI));

        gdImageDestroy(img);

		/* write time of each image */
		imageTimes aux;
		aux.times = diff_timespec(&st_time, &st_program);
		aux.idx = idx;
		aux.end = 0;
		write(pipe_times[1], &aux, sizeof(aux));

    }

}


/******************************************************************************
 * smooth()
 *
 * 
 *
 *****************************************************************************/
void *smooth(void *args) {

	imageI imgI;
    gdImagePtr img, smooth_img;          /* image pointers */

    while (1) {

	    /* get the the thread idx from the pipe */
	    read(pipe_contrast[0], &imgI, sizeof(imgI));

        /* exit condition */
        if (imgI.idx == -1) {
            write(pipe_smooth[1], &imgI, sizeof(imgI)); /* pass terminating signal */
            break;
        }

		img = imgI.img;

        smooth_img = smooth_image(img);

        /* write the smooth image pointer onto next pipe - smooth pipe */
		imgI.img = smooth_img;
        write(pipe_smooth[1], &imgI, sizeof(imgI));

        gdImageDestroy(img);

    }

}

/******************************************************************************
 * texture()
 *
 * 
 *
 *****************************************************************************/
void *texture(void *args) {

	imageI imgI;
    gdImagePtr img, texture_img;          /* image pointers */

    while (1) {

	    /* get the the thread idx from the pipe */
	    read(pipe_smooth[0], &imgI, sizeof(imgI));
        
        /* exit condition */
        if (imgI.idx == -1) {
            write(pipe_texture[1], &imgI, sizeof(imgI)); /* pass terminating signal */
            break;
        }

        img = imgI.img;

        texture_img = texture_image(img, texture_png);

        /* write the texture image pointer onto next pipe - texture pipe */
		imgI.img = texture_img;
        write(pipe_texture[1], &imgI, sizeof(imgI));

        gdImageDestroy(img);

    }

}

/******************************************************************************
 * sepia()
 *
 * 
 *
 *****************************************************************************/
void *sepia(void *args) {

	struct timespec end_time;

	imageI imgI;
    gdImagePtr img, sepia_img;          /* image pointers */

    while (1) {

	    /* get the the thread idx from the pipe */
	    read(pipe_texture[0], &imgI, sizeof(imgI));

        /* exit condition */
        if (imgI.idx == -1) {
            write(pipe_sepia[1], &imgI, sizeof(imgI)); /* pass terminating signal */
            break;
        }

        img = imgI.img;

        sepia_img = sepia_image(img);

        /* write the sepia image pointer onto next pipe - sepia pipe */
		imgI.img = sepia_img;
        write(pipe_sepia[1], &imgI, sizeof(imgI));

        gdImageDestroy(img);

		clock_gettime(CLOCK_MONOTONIC, &end_time);

		imageTimes aux;
		aux.times = diff_timespec(&end_time, &st_program);
		aux.idx = imgI.idx;
		aux.end = 1;
		write(pipe_times[1], &aux, sizeof(aux));

    }

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

	clock_gettime(CLOCK_MONOTONIC, &st_program);
	clock_gettime(CLOCK_MONOTONIC, &start_time_total);
	clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

	/* if argc isn't three we quit*/
	if (argc != 2) {
		fprintf(stdout, "\n\tUse the command:\n\n\t./old-photo-paralelo-B <files_dir>\n\n");
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
    imageI imgI;

	/* array of threads */
	pthread_t threads[NN_THREADS];

	/* creation of output directories */
	char oldImgsPath[strlen(dir) + 18]; 
	sprintf(oldImgsPath, "%s%s", dir, OLD_IMAGE_DIR);
	if (create_directory(oldImgsPath) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", oldImgsPath);
		exit(-1);
	}

	texture_png = read_png_file(PAPER_TEXTURE);

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
	clock_gettime(CLOCK_MONOTONIC, &start_time_par);

	/* create pipes */
	if (pipe(pipe_input) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
    if (pipe(pipe_contrast) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
    if (pipe(pipe_smooth) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
    if (pipe(pipe_texture) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
    if (pipe(pipe_sepia) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}
	if (pipe(pipe_times) != 0) {
		fprintf(stderr, "Error creating pipe...\n");
		exit(-1);
	}

	/* initialize threads */
	pthread_create(&threads[0], NULL, contrast, NULL);
    pthread_create(&threads[1], NULL, smooth, NULL);
    pthread_create(&threads[2], NULL, texture, NULL);
    pthread_create(&threads[3], NULL, sepia, NULL);

    for (int i = 0; i < nn_files; i++){	

		write(pipe_input[1], &i, sizeof(i));

	}

    int term = -1;
	/* send term signal */
    write(pipe_input[1], &term, sizeof(term));

	for (int i = 0; i < NN_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	char outFileName[256];

    while (1) {
        
        /* get image */
        read(pipe_sepia[0], &imgI, sizeof(imgI));

        /* exit condition */
        if (imgI.idx == -1) {
            break;
        }

        /* outFileName */
		sprintf(outFileName, "%s%s%s", dir,  OLD_IMAGE_DIR, strrchr(files[imgI.idx], '/'));

        /* save resized */ 
		if(write_jpeg_file(imgI.img, outFileName) == 0){
			fprintf(stderr, "Impossible to write %s image\n", outFileName);
		}
		gdImageDestroy(imgI.img);

    }

	clock_gettime(CLOCK_MONOTONIC, &end_time_par);
	clock_gettime(CLOCK_MONOTONIC, &start_time_seq2);

	gdImageDestroy(texture_png);

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq2);
	clock_gettime(CLOCK_MONOTONIC, &end_time_total);

char buffer[256];
	sprintf(buffer, "%s%s", dir, "/timming_pipeline.txt");
FILE *timing;
imageTimes imgT;

struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);
struct timespec seq2_time = diff_timespec(&end_time_seq2, &start_time_seq2);
struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);

if (!isFileExists(buffer)) {
	
	timing = fopen(buffer, "w");

	/* -> write for each thread */
	for (int i = 0; i < 2 * nn_files; i++) {
		
		read(pipe_times[0], &imgT, sizeof(imgT));
		
		/* decide if it is a start or end time */
		if (!imgT.end)
			fprintf(timing, "%s \t start \t%10jd.%02ld\n", strrchr(files[imgT.idx], '/') + 1 , imgT.times.tv_sec, imgT.times.tv_nsec / 10000000);
		else
			fprintf(timing, "%s \t end \t%10jd.%02ld\n", strrchr(files[imgT.idx], '/') + 1 , imgT.times.tv_sec, imgT.times.tv_nsec / 10000000);
	}

	/* -> write total */
	fprintf(timing, "total \t\t\t%10jd.%02ld\n", total_time.tv_sec, (long) total_time.tv_nsec / 10000000);

	/* close timing_<n>.txt */
	fclose(timing);
}

else fprintf(stdout, "Found file:\t%s\n", buffer);

	/* write to stdout */
    printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
	printf("\tseq2 \t %10jd.%09ld\n", seq2_time.tv_sec, seq2_time.tv_nsec);

	destroyFiles(files, nn_files);

	exit(0);
}
