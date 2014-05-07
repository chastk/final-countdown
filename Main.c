/*Op Sys Project 2*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_MAX 1024
#define PROCS_MAX 26
#define MEM_SIZE 1600

struct procedure {
	char p_name;
	int mem_size;
	int * times;
	int time_index;
	int in_or_out;
}procs; 

typedef struct free_block{
	int start_index;
	int size;
}fb;

char memory [1600];
int numProcs;
// block counter
int bc = 0;

// Utility function for qsort-ing free_blocks
int index_sort(const void *a,const void *b) {
	struct free_block * x = (struct free_block*)a;
	struct free_block * y = (struct free_block*)b;
	int diff = x->start_index - y->start_index;
	return diff;
}

// Utility function for initializing free_blocks
// To start with, there is ONE free block
void init(struct free_block * list){
	list = (struct free_block)(malloc(sizeof(struct free_block)));
	list->start_index = 0;
	list->size = MEM_SIZE;
	bc = 1;
}

// Utility function for adding a free_block
struct free_block * add(struct free_block * list, int index, int size){
	// create new free block
	struct free_block temp = (struct free_block)(malloc(sizeof(struct free_block)));
	temp.start_index = index;
	temp.size = size;
	// reallocate memory in the list, and add the new block
	bc++;
	list = realloc(list, (bc * sizeof(struct free_block)) );
	list[bc] = temp;
	// re-sort everything before we return the list
	qsort(list, bc, sizeof(struct free_block), &index_sort);
	// cleanup~
	free((void *)temp);
	return list;
}

// Utility function for merging free blocks
// We want the listof free blocks to be sorted at all times.
// If there are two consecutive blocks a and b where
// (a.size) - (a.start_index) == (b.start_index),
// then we want to merge these two blocks.
// This function *should* make ALL such reductions.
// This function assumes list is sorted.
struct free_block * merge_blocks(struct free_block * list){
	// If there are no free blocks, something is probably wrong
	if(bc == 0){
		perror("No free blocks?? Check to be sure the list is initialized\n");
		exit(1);
	}
// If there's only one free block, no need to do anything
	if(bc == 1) return list;

	// Otherwise, loop through everything:
	else{
		int i = 0; // current block
		int j = 1; // next block to check for merging
		int k;
		int temp = bc;
		struct free_block a = list[i];
		struct free_block b = list[j];
		while(j < temp){
			// check for merge condition
			if( ((a.size)-(a.start_index) == (b.start_index)) ){
				// merge blocks list[i] and list[j]
				list[i].size += list[j].size;
				// remove list[j] by shifting everything down
				for(k = j; k < temp; k++){
					list[k] = list[j+1];
				}
				// decrement temp (which will become the new block counter)
				temp--;
				// check to see if the new block i can merge with
				// the next block in the next loop iteration
				j++;
			}// /if
			// if current block i doesn't need to be merged, increment it to the next block
			// and also increment j, to check for the next possible merge
			else{
				i++;
				j++;
			}
		}// /while
	}// /else
	return list;
}// /merge_blocks




struct procedure * read_from_file(FILE * input){
	char buffer[BUFFER_MAX];
	fgets(buffer, BUFFER_MAX, input);
	numProcs = atoi(buffer);
	int len = strlen(buffer);
	if( buffer[len-1] == '\n' ) buffer[len-1] = '\0';
	printf("[init]: %d processes in this input\n", numProcs);
	if(numProcs > PROCS_MAX) printf("[init]: WARNING: too many processes!\n");

	struct procedure * procs = calloc(numProcs, sizeof(struct procedure));
	char toProcess[BUFFER_MAX];
	int index, tindex;
	index = 0;
	tindex = 0;
	while( fgets(buffer, BUFFER_MAX, input) ){
		len = strlen(buffer);
		if( buffer[len-1] == '\n' ) buffer[len-1] = '\0';
		printf("[init]: read: %s\n", buffer);
		strcpy(toProcess, buffer);

		// set process name
		procs[index].p_name = (strtok(toProcess, " \t"))[0];

		// set process memory size
		int * array = malloc(sizeof(int));
		char * token;
		int temp;
		token = strtok(NULL, " \t");
		procs[index].mem_size = atoi(token);
		printf("[init]: Proc: %c, Size: %d\n", procs[index].p_name, procs[index].mem_size);

		// read all the times for that process
		token = strtok(NULL, " \t");
		while ( token != NULL) {
			temp = atoi(token);
			array = realloc(array, (tindex+1)*sizeof(int));
			array[tindex] = temp;
			printf("[init]: added time: %d\n", array[tindex]);
			tindex++;
			token = strtok(NULL, " \t");
		}// /while
		procs[index].times = array;

		// with everything from the file set, assign 0 to the other two fields
		// by default.
		procs[index].time_index = 0;
		procs[index].in_or_out = 0;
		// this process is initialized!
		// increment the index counter for the overall array,
		// and get the next item in the loop
		index++;
	}// /while reading lines
	return procs;
}// /read_from_file

void printmemory(int time) {
    int i;
    int count=0;
    printf("Memory at time %i:\n",time);
    for (i=0; i<1600; i++) {
        count++;
        if (count==81) {
            printf("\n");
            count=1;
        }
        printf("%c", memory[i]);
    }
    printf("\n");
}

int defrag() {
    printf("Performing defragmentation...\n");
    int i,j,mark;
    int count=0;
    for (i=0;i<1600;i++) {
        mark=0;
        if (memory[i]=='.') {
            for (j=i; j<1600; j++) {
                if (memory[j]!='.') {
                    memory[i]=memory[j];
                    memory[j]='.';
                    count++;
                    mark=1;
                    break;
                }
            }
            if (mark==0) {
                printf("Defragmentation complete. \nRelocated %i processes to create a free memory block of %i units (%f%% of total memory).\n",count, 1600-i, (float)(1600-i)/1600.0); 
                return 1600-i;
            }
        }
    }
    return -1;
}

struct free_block * First(int time, struct procedure * proc, struct free_block * blocks) {
	int i=0;
	int j=0;
	int k=0;
	for (i=0; i<numProcs; i++) {
		if (proc[i].in_or_out==1 && proc[i].times[proc[i].time_index]<=time) {
			for (j=0; j<1600; j++) {
				if (memory[j]==proc[i].p_name) {
					k=j;
					while (memory[j]==proc[i].p_name) {
						memory[j]='.';
						j++;
					}
				}
			}
			proc[i].in_or_out=0;
			proc[i].time_index++;
			blocks=add(blocks, j, proc[i].mem_size);
			blocks=merge_blocks(blocks);
		}
	}
	i=0;
	j=0;
	k=0;
	while (blocks[i].size>0) {
		if (blocks[i].size>=proc[j].mem_size && proc[j].in_or_out==0 && proc[j].times[proc[j].time_index]<=time) {
			int counter=blocks[i].start_index;
			while (k<proc[j].mem_size) {
				memory[counter]=proc[j].p_name;
				counter++;
			}
			blocks[i].size=blocks[i].size-proc[j].mem_size;
			blocks[i].start_index=blocks[i].start_index+proc[j].mem_size;
			proc[j].time_index++;
			proc[j].in_or_out=1;
		}
	}
	return blocks;
}

int main (int argc, char* argv[]) {
    if (argc==3 || argc==4) {
        int i;
        for (i=0; i<80; i++) {
            memory[i]='#';
        }
        for (i=80; i<1600; i++) {
            memory[i]='.';
        }
		if (argc==3) {
			char * filename = argv[1];
			FILE * input = fopen(filename, "r");
			if (input==NULL) {
				perror("open failed");
				return EXIT_FAILURE;
			}
			struct procedure * proc;
			proc = read_from_file(input);
			struct free_block * blocks;
			blocks = init(blocks);
			int i;
			for (i = 0; i < numProcs; i++){
				printf("%c:: %d blocks\n", proc[i].p_name, proc[i].mem_size);
			}
		}
        printmemory(0);
        if ((argc==3 && strcmp(argv[2], "first")) || (argc==4 && strcmp(argv[3], "first"))) {
			First(time, proc, blocks);
		}
        return EXIT_SUCCESS;
    }
    else {
        printf("ERROR WRONG NUMBER OF ARGUMENTS\n");
        return EXIT_FAILURE;
    }
}
