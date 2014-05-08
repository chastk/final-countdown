/*
* Jamie Dockendorff and Katie Chastain
* Operating Systems, Spring 2014
* Project #2
*/

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
#define OS_SIZE 80

int index_sort(const void *a,const void *b);
void init();
void take_process(int pindex);
void merge_blocks();
void put_process(int pindex, int bindex);
struct procedure * read_from_file(FILE * input);

/*******************************
	Structures
*******************************/
// p_name is the letter for that procedure
// mem_size is the # of blocks it takes up
// times are the times in the input file when it either
//    enters or leaves memory. This *should* have an 
//    even number of entries.
// time_index gives the next index in times to be handled
// start_index = -1 if the procedure is OUT of memory,
//           = first space it takes up if it the procedure is IN memory.
typedef struct procedure{
	char p_name;
	int mem_size;
	int * times;
	int time_index; // should always be set to 0 initially
	int start_index; // should always be set to -1 initially
}proc;

// start_index is the index in memory of the FIRST space
//    the free block takes up
// size is the TOTAL spaces the block takes up
// (the last space occupied by the block is start_index+size)
typedef struct free_block{
	int start_index;
	int size;
}fb;

/*******************************
	Global Variables
*******************************/
char memory [MEM_SIZE]; // character array representing memory
struct procedure * procs; // stores the procedure array
struct free_block * list; // stores the free_block array
int numProcs; // total number of processes
int bc = 0; // block counter
int time_counter = 0; // simulation time


/*******************************
	Utility Functions
*******************************/
// Utility function for qsort-ing free_blocks
//	by start_index
int index_sort(const void *a,const void *b){
	struct free_block * x = (struct free_block*)a;
	struct free_block * y = (struct free_block*)b;
	int diff = x->start_index - y->start_index;
	return diff;
}

// Utility function for initializing free_blocks
// To start with, there is ONE free block
void init(){
	list = (struct free_block*)(malloc(sizeof(struct free_block)));
	list->start_index = OS_SIZE;
	list->size = (MEM_SIZE - OS_SIZE);
	bc = 1;
}

// Utility function for adding a free_block
// In other words, this happens when a process LEAVES memory
// index = the start index of the new block
// size = the size of the new block
void take_process(int pindex){
	// create new free block
	struct free_block * temp = malloc(sizeof(struct free_block));
	temp->start_index = procs[pindex].start_index;
	temp->size = procs[pindex].mem_size;
	// reallocate memory in the list, and add the new block
	bc++;
	list = realloc(list, (bc * sizeof(struct free_block)) );
	list[bc] = *temp;
	// re-sort everything and merge if necessary
	qsort(list, bc, sizeof(struct free_block), &index_sort);
	merge_blocks();
}

// Utility function for merging free blocks
// We want the listof free blocks to be sorted at all times.
// If there are two consecutive blocks a and b where
// (a.size) + (a.start_index) == (b.start_index),
// then we want to merge these two blocks.
// This function *should* make ALL such reductions.
void merge_blocks(){
	// If there are no free blocks, something is probably wrong
	if(bc == 0){
		perror("No free blocks?? Check to be sure the list is initialized\n");
		exit(1);
	}
	// If there's only one free block, no need to do anything
	if(bc == 1) return;
	// Otherwise, loop through everything:
	else{
		qsort(list, bc, sizeof(struct free_block), &index_sort);
		int i = 0; // current block
		int j = 1; // next block to check for merging
		int k;
		int temp = bc;
		struct free_block a = list[i];
		struct free_block b = list[j];
		while(j < temp){
			// check for merge condition
			int a_end = a.size + a.start_index;
			if( a_end == b.start_index ){
				// merge blocks list[i] and list[j]
				list[i].size += list[j].size;
				// remove list[j] by shifting everything down
				for(k = j; k < temp; k++){
					list[k] = list[k+1];
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
		// reallocate the memory in the list array, and update the block counter
		bc = temp;
		list = realloc(list, (bc * sizeof(struct free_block)) );
	}// /else
}// /merge_blocks


// Utility function for updating/removing a free block
// In other words, this happens when a process ENTERS memory
// proc_index is the index of the process to add
// block_index is the block we want to put it into
// We would like to assume that this method is NOT called unless
// 	we already know that procs[pindex] fits into list[bindex]
void put_process(int pindex, int bindex){
	int diff = (list[bindex].size - procs[pindex].mem_size);
	// Check to be sure the process fits!
	if( diff < 0 ){
		printf("Error placing process: block %d is too small for process %d!", bindex, pindex);
		exit(-1);
	}
	// If the process is the same size as the block,
	// 	eliminate block list[bindex] and shift the rest of list
	else if( diff == 0 ){
		int k;
		for(k = bindex; k < bc; k++){
			list[k] = list[k+1];
		}
		bc--;
		list = realloc(list, (bc * sizeof(struct free_block)) );
	}
	// If the process is smaller than the block, just update the block's stats
	//	eg, ADD to start_index, SUBTRACT from size
	else {
		list[bindex].start_index += list[bindex].size;
		list[bindex].size -= list[bindex].size;
	}
}// /put_process

// Utility function for reading in the input file and parsing it
//	into process structures for the rest of the program.
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
		// With everything from the file set, set the time_index to 0,
		//	and the start_index to -1 to show that it is out of memory 
		// 	and nothing has actually happened yet.
		procs[index].time_index = 0;
		procs[index].start_index = -1;
		// this process is initialized!
		// increment the index counter for the overall array,
		//    and get the next item in the loop
		index++;
	}// /while reading lines
	return procs;
}// /read_from_file

// Utility function for printing memory contents
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

// Utility function for defragmentation
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
                free(list);
                init();
                list->start_index=i;
                printf("Defragmentation complete. \nRelocated %i processes to create a free memory block of %i units (%f%% of total memory).\n",count, 1600-i, (float)(1600-i)/1600.0);
                return 1600-i;
            }
        }
    }
    return -1;
}


/*******************************
	Placement Methods
*******************************/
void First(int time) {
    int i=0;
    int j=0;
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index!=-1 && procs[i].times[procs[i].time_index]<=time) {
            take_process(procs[i].start_index);
        }
    }
    i=0;
    j=0;
    for (j=0; j<numProcs; j++) {
        int k=-1;
        while (list[i].size>0) {
            if (list[i].size>=procs[j].mem_size && procs[j].start_index==-1 && procs[j].times[procs[j].time_index]<=time) {
                k=1;
                put_process(j,i);
            }
            i++;
        }
        if (k==-1) {
            defrag();
        }
        i=0;
    }
    return;
}// /First

void Best(int time) {
    int i=0;
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index!=-1 && procs[i].times[procs[i].time_index]<=time) {
            take_process(procs[i].start_index);
        }
    }
    i=0;
    int j=0;
    int k=1700;
    int p=-1;
    for (j=0; j<numProcs; j++) {
        while (list[i].size>0) {
            if (procs[j].start_index!=-1 && procs[j].times[procs[j].time_index]<=time) {
                if (list[i].size>=procs[j].mem_size && list[i].size<k) {
                    k=list[i].size;
                    p=i;
                }
            }
        }
        if (p!=-1) {
            put_process(j,p);
        }
        else {
            defrag();
        }
    }
    return;
}// /Best

void Next(int time) {
    int i=0;
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index!=-1 && procs[i].times[procs[i].time_index]<=time) {
            take_process(procs[i].start_index);
        }
    }
    i=0;
    int j=0;
    for (j=0; j<numProcs; j++) {
        int k=-1;
        while (list[i].size>0) {
            if (list[i].size>=procs[j].mem_size && procs[j].start_index==-1 && procs[j].times[procs[j].time_index]<=time) {
                k=1;
                put_process(j,i);
            }
            i++;
        }
        if (k==-1) defrag();
    }
    return;
}// /Next

void Worst(int time) {
    int i=0;
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index!=-1 && procs[i].times[procs[i].time_index]<=time) {
            take_process(procs[i].start_index);
        }
    }
    i=0;
    int j=0;
    int k=0;
    int p=-1;
    for (j=0; j<numProcs; j++) {
        while (list[i].size>0) {
            if (procs[j].start_index!=-1 && procs[j].times[procs[j].time_index]<=time) {
                if (list[i].size>=procs[j].mem_size && list[i].size>k) {
                    k=list[i].size;
                    p=i;
                }
            }
        }
        if (p==-1) defrag();
        else {
            put_process(j,p);
        }
    }
    return;
}// /Worst

void noncontiguous (int time) {
    int i=0;
    int j;
    int count=0;
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index!=-1 && procs[i].times[procs[i].time_index]<=time) {
            for (j=0; j<1600; j++) {
                if (memory[j]==procs[i].p_name) {
                    memory[j]='.';
                }
            }
        }
    }
    for (i=0; i<numProcs; i++) {
        if (procs[i].start_index==-1 && procs[i].times[procs[i].time_index]<=time) {
            for (j=0; j<1600; j++) {
                if (memory[j]=='.' && count<procs[i].mem_size) {
                    memory[j]=procs[i].p_name;
                    count++;
                }
            }
        }
    }
}// /noncontiguous




/*******************************
	Main
*******************************/
int main (int argc, char* argv[]) {
	if(argc < 2){
		printf("USAGE: ./a.out <input.txt>\n");
		return EXIT_FAILURE;
	}
    int i;
	// Initialize memory
	for (i=0; i<OS_SIZE; i++) {
		memory[i]='#';
	}
	for (i=OS_SIZE; i<MEM_SIZE; i++) {
		memory[i]='.';
	}
	// Initlaize processes
	char * filename = argv[1];
	FILE * input = fopen(filename, "r");
	if (input==NULL) {
		perror("open failed");
		return EXIT_FAILURE;
	}
	procs = read_from_file(input);
	// Initialize free block(s)
	init(); 
	//for (i = 0; i < numProcs; i++){
	//	printf("%c:: %d blocks\n", procs[i].p_name, procs[i].mem_size);
	//}
	// Get method:
	char * method;
	if( !(strcpy(method, argv[2])) ){
		perror("Error: No method supplied!\n");
		return EXIT_FAILURE;
	}

	printmemory(0);
	// Do the thing:
	int t_input;
	if(strcasecmp(method, "first") ){
		while(1){
			printf("Enter next time, or 0 to quit:\n");
			if( scanf("%d", &t_input) == 0 )
				break;
			First(t_input);
			printmemory(t_input);
		}
	}
	else if(strcasecmp(method, "next") ){
		while(1){
			printf("Enter next time, or 0 to quit:\n");
			if( scanf("%d", &t_input) == 0 )
				break;
			Next(t_input);
			printmemory(t_input);
		}
	}
	else if(strcasecmp(method, "best") ){
		while(1){
			printf("Enter next time, or 0 to quit:\n");
			if( scanf("%d", &t_input) == 0 )
				break;
			Best(t_input);
			printmemory(t_input);
		}
	}
	else if(strcasecmp(method, "worst") ){
		while(1){
			printf("Enter next time, or 0 to quit:\n");
			if( scanf("%d", &t_input) == 0 )
				break;
			Worst(t_input);
			printmemory(t_input);
		}
	}
	else{
		perror("Error: Invalid method! Options: first, next, best, worst\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
