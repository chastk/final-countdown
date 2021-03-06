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

/*******************************
Structures
*******************************/
// p_name is the letter for that procedure
// mem_size is the # of blocks it takes up
// times are the times in the input file when it either
// enters or leaves memory. This *should* have an
// even number of entries.
// time_index gives the next index in times to be handled
// start_index = -1 if the procedure is OUT of memory,
// = first space it takes up if it the procedure is IN memory.
typedef struct procedure{
    char p_name;
    int mem_size;
    int * times;
    int time_index; // should always be set to 0 initially
    int start_index; // should always be set to -1 initially
}proc;

// start_index is the index in memory of the FIRST space
// the free block takes up
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

// procedure/free block methods:
int index_sort(const void *a,const void *b);
void init_free_blocks();
void reset_free_blocks(int start, int b_size);
void take_process(int pindex);
void merge_blocks();
void put_process(int pindex, int bindex);
int get_next_event(int time);
int get_initial_events();
int defrag();
// memory methods:
void init_memory();
void printmemory();
void write_procedure(struct procedure * p);
void write_free(struct free_block * b);
// input handler method:
struct procedure * read_from_file(FILE * input);


/*******************************
Utility Functions
*******************************/
// Utility function for qsort-ing free_blocks
// by start_index
int index_sort(const void *a,const void *b){
    struct free_block * x = (struct free_block*)a;
    struct free_block * y = (struct free_block*)b;
    int diff = x->start_index - y->start_index;
    return diff;
}

// Utility function for initializing free_blocks
// To start with, there is ONE free block
void init_free_blocks(){
    list = (struct free_block*)(malloc(sizeof(struct free_block)));
    list->start_index = OS_SIZE;
    list->size = (MEM_SIZE - OS_SIZE);
    bc = 1;
}

// Utility function for resetting free_blocks after defrag
// Basically init, with arguments for the start and size following defrag
void reset_free_blocks(int start, int b_size){
    list = (struct free_block*)(malloc(sizeof(struct free_block)));
    list->start_index = start;
    list->size = b_size;
    bc = 1;
}

// Utility function for adding a free_block
// In other words, this happens when a process LEAVES memory
// index = the start index of the new block
// size = the size of the new block
void take_process(int pindex){
    printf("Taking process %d from memory....\n", pindex);
    // create new free block
    struct free_block * temp = malloc(sizeof(struct free_block));
    temp->start_index = procs[pindex].start_index;
    temp->size = procs[pindex].mem_size;
    // reallocate memory in the list, and add the new block
    bc++;
    list = realloc(list, (bc * sizeof(struct free_block)) );
    list[bc-1] = *temp;
    // update fields in the procedure
    procs[pindex].time_index++;
    procs[pindex].start_index = -1;
    // re-sort everything and merge if necessary
    write_free(&list[bc-1]);
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
// we already know that procs[pindex] fits into list[bindex]
void put_process(int pindex, int bindex){
    int diff = (list[bindex].size - procs[pindex].mem_size);
    //printf(":: Block size: %d\n:: Procedure size: %d\n", list[bindex].size, procs[pindex].mem_size);
    // Check to be sure the process fits!
    if( diff < 0 ){
        printf("Error placing process: block %d is too small for process %d!\n", bindex, pindex);
        exit(-1);
    }
    // If the process is the same size as the block,
    // eliminate block list[bindex] and shift the rest of list
    else if( diff == 0 ){
        int k;
        procs[pindex].start_index = list[bindex].start_index;
        procs[pindex].time_index++;
        write_procedure(&procs[pindex]);
        for(k = bindex; k < bc; k++){
            list[k] = list[k+1];
        }
        bc--;
        list = realloc(list, (bc * sizeof(struct free_block)) );
    }
    // If the process is smaller than the block, just update the block's stats
    // eg, ADD to start_index, SUBTRACT from size
    else {
        list[bindex].start_index += procs[pindex].mem_size;
        list[bindex].size -= procs[pindex].mem_size;
        //printf("updated block %d: start: %d, size: %d\n", bindex, list[bindex].start_index, list[bindex].size);
        procs[pindex].start_index = list[bindex].start_index;
        procs[pindex].time_index++;
        write_procedure(&procs[pindex]);
    }
}// /put_process

// Utility function for reading in the input file and parsing it
// into process structures for the rest of the program.
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
        strcpy(toProcess, buffer);

        // set process name
        procs[index].p_name = (strtok(toProcess, " \t"))[0];

        // set process memory size
        int * array = malloc(sizeof(int));
        char * token;
        int temp;
        token = strtok(NULL, " \t");
        procs[index].mem_size = atoi(token);

        // read all the times for that process
        token = strtok(NULL, " \t");
        tindex = 0;
        while ( token != NULL) {
            temp = atoi(token);
            array = realloc(array, (tindex+1)*sizeof(int));
            array[tindex] = temp;
            tindex++;
            token = strtok(NULL, " \t");
        }// /while
        procs[index].times = array;
        // With everything from the file set, set the time_index to 0,
        // and the start_index to -1 to show that it is out of memory
        // and nothing has actually happened yet.
        procs[index].time_index = 0;
        procs[index].start_index = -1;
        // this process is initialized!
        // increment the index counter for the overall array,
        // and get the next item in the loop
        index++;
    }// /while reading lines
    return procs;
}// /read_from_file

// Utility function for updating the memory array
// with a process
void write_procedure(struct procedure * p){
    int i;
    if(p->start_index < 0){
        perror("Cannot write OUT process\n");
    }
    int span = (p->start_index + p->mem_size);
    if (span>MEM_SIZE) {
        printf("ERROR:out of memory to intialize \n");
        exit(EXIT_FAILURE);
    }
    for(i = p->start_index; i < span; i++){
        if(memory[i] != '.'){
            //perror("Invalid memory write location!\n");
        }
        memory[i] = p->p_name;
    }
}

// Utility function for updating the memory array
// with a free block
void write_free(struct free_block * b){
    printf("writing a free block: start %d, size %d\n", b->start_index, b-> size);
    int i;
    int span = (b->start_index + b->size);
    for(i = b->start_index; i < span; i++){
        memory[i] = '.';
    }
}

// Utility function for initializing the memory representation
void init_memory(){
    int j;
    // Opearting system first:
    for (j = 0; j < OS_SIZE; j++) {
        memory[j]='#';
    }
    // Then the rest of the memory:
    for (j = OS_SIZE; j < MEM_SIZE; j++) {
        memory[j]='.';
    }
    // Place any processes at time_counter = 0 by first-best placement
    int next = get_initial_events();
    int k;
    // while there's an event to update,
    while(next != -1){
        //printf("Next process: %c\n", procs[next].p_name);
        // find the FIRST block large enough to hold procs[next]
        k = -1;
        for(j = 0; j < bc; j++){
            //printf(":: attmept placement at %d: (%d)\n", j, list[j].size);
            if( list[j].size >= procs[next].mem_size ){
                k = j;
                break;
            }
        }
        // if we found an apprpriate block,
        // PUT PROCESS
        if(k != -1){
            put_process(next,k);
        }
        else{
            printf("[Initilization Error:] Memory overflow at time 0!\n");
            exit(-1);
        }
        next = get_initial_events();
    }// /while
    printf("Memory Initalized\n");
}


// Utility function for printing memory contents
void printmemory() {
    int i;
    int count=0;
    printf("Memory at time %i:\n",time_counter);
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
                reset_free_blocks(i, (1600-i));
                printf("Defragmentation complete. \nRelocated %i processes to create a free memory block of %i units (%f%% of total memory).\n",count, 1600-i, (float)(1600-i)/16.0);
                for(i = 0; i < bc; i++){
                    printf("[Block %d] start: %d, size: %d\n", i, list[i].start_index, list[i].size);
                }
                return 1600-i;
            }
        }
    }
    return -1;
}

// utility method to get the index of the next procedure
// that has an event (entering OR leaving).
// As this only gets one at a time, it will get the first event
// by the order provided in the original input text.
int get_next_event(int time){
    int i, event, index;
    for(i = 0; i < numProcs; i++){
        index = procs[i].time_index;
        event = procs[i].times[index];
        printf("Process[%d]'s next event: #%d, %d\n", i, index, event);
        if( event != 0 && event <= time ) return i;
    }
    return -1;
}

// as above, but modified for the specific case of
int get_initial_events(){
    int i, event, index;
    for(i = 0; i < numProcs; i++){
        index = procs[i].time_index;
        event = procs[i].times[index];
        //printf("Process[%d]'s next event: #%d, %d\n", i, index, event);
        if( event == 0 ) return i;
    }
    return -1;
}


/*******************************
Placement Methods
*******************************/
void First(int time) {
    int j = 0;
    int next = get_next_event(time);
    // while there's an event to update,
    while(next != -1){
        printf("Next = [%c]: %d (%d)\n", procs[next].p_name, procs[next].mem_size, procs[next].start_index);
        // if it's in memory, the event is a take_process
        if (procs[next].start_index != -1){
            take_process(next);
            next = get_next_event(time);
        }// /take process
        // if the process is not in memory, the event is a put_process
        else if (procs[next].start_index == -1){
            // find the FIRST block large enough to hold procs[next]
            int k = -1;
            for(j = 0; j < bc; j++){
                if( list[j].size >= procs[next].mem_size ){
                    k = j;
                    break;
                }
            }
            // if we found an apprpriate block,
            // PUT PROCESS and update necessary fields
            if(k != -1){
                put_process(next,k);
                printf("put_process successful\n");
                next = get_next_event(time);
                printf("got next: %d\n", next);
            }
            else {
                int x=defrag();
                if (x<procs[next].mem_size) {
                    perror("OUT OF MEMEORY");
                    exit(EXIT_FAILURE);
                }
            // If there is enough space after we defrag,
            // don't incrememnt next yet, just run the loop again with the same next event
            }
        }// /put process
    }// /while
    return;
}// /First

void Best(int time) {
    int j = 0;
    int next = get_next_event(time);
    // while there's an event to update,
    while(next != -1){
        // if it's in memory, the event is a take_process
        if (procs[next].start_index > 0){
            take_process(next);
            next = get_next_event(time);
        }// /take process
        // if the process is not in memory, the event is a put_process
        else if (procs[next].start_index == -1){
            // find the SMALLEST block large enough to hold procs[next]
            int smallest = MEM_SIZE - OS_SIZE;
            int k = -1;
            for(j = 0; j < bc; j++){
                if( (list[j].size >= procs[next].mem_size) && (list[j].size < smallest) ){
                    smallest = list[j].size;
                    k = j;
                }
            }
            // if we find an appropriate block,
            // PUT PROCESS and update necessary fields
            if(k != -1){
                put_process(next,k);
                next = get_next_event(time);
            }
            else {
                int x=defrag();
                if (x<procs[next].mem_size) {
                    perror("OUT OF MEMEORY");
                    exit(EXIT_FAILURE);
                }
            // If there is enough space after we defrag,
            // don't incrememnt next yet, just run the loop again with the same next event
            }
        }// /put process
    }// /while
    return;
}// /Best

void Next(int time) {
    int j = 0;
    int prev_k = 0;
    int next = get_next_event(time);
    // while there's an event to update,
    while(next != -1){
        // if it's in memory, the event is a take_process
        if (procs[next].start_index > 0){
            take_process(next);
            next = get_next_event(time);
        }// /take process
        // if the process is not in memory, the event is a put_process
        else if (procs[next].start_index == -1){
            // find the NEXT block large enough to hold procs[next]
            int k = -1;
            // start looking where we placed the last one
            for(j = prev_k; j < bc; j++){
                if(list[j].size > procs[next].mem_size) k = j;
            }
            // if we haven't found it in the second half of the array,
            // loop around and check the first half....
            if(k < 0){
                for(j = 0; j < prev_k; j++){
                    if(list[j].size > procs[next].mem_size) k = j;
                }
            }
            // if we find an appropriate block,
            // PUT PROCESS and update necessary fields
            if(k != -1){
                put_process(next,k);
                next = get_next_event(time);
            }
            else {
                int x=defrag();
                if (x<procs[next].mem_size) {
                    perror("OUT OF MEMEORY");
                    exit(EXIT_FAILURE);
                }
                // If there is enough space after we defrag,
                // don't incrememnt next yet, just run the loop again with the same next event
            }
        }// /put process
    }// /while
    return;
}// /Next

void Worst(int time) {
    int j=0;
    int next = get_next_event(time);
    // while there's an event to update,
    while(next != -1){
        // if it's in memory, the event is a take_process
        if (procs[next].start_index > 0){
            take_process(next);
            next = get_next_event(time);
        }// /take process
        // if the process is not in memory, the event is a put_process
        else if (procs[next].start_index == -1){
            // find the SMALLEST block large enough to hold procs[next]
            int largest = 0;
            int k = -1;
            for(j = 0; j < bc; j++){
                if( (list[j].size >= procs[next].mem_size) && (list[j].size > largest) ){
                    largest = list[j].size;
                    k = j;
                }
            }
            // if we find an appropriate block,
            // PUT PROCESS and update necessary fields
            if(k != -1){
                put_process(next,k);
                next = get_next_event(time);
            }
            else {
                int x=defrag();
                if (x<procs[next].mem_size) {
                    perror("OUT OF MEMEORY");
                    exit(EXIT_FAILURE);
                }
                // If there is enough space after we defrag,
                // don't incrememnt next yet, just run the loop again with the same next event
            }
        }// /put process
    }// /while
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
        printf("USAGE: ./a.out <input.txt> <method>\n");
        return EXIT_FAILURE;
    }
    //int i, n, j;
    // Initlaize processes
    char * filename = argv[1];
    FILE * input = fopen(filename, "r");
    if (input==NULL) {
        perror("open failed");
        return EXIT_FAILURE;
    }
    procs = read_from_file(input);
    printf("... Processes initialized\n");
    /*for(i = 0; i < numProcs; i++){
    n = sizeof(procs[i].times) / sizeof(int);
    for(j = 0; j < n; j++)
    printf("%c: times[%d] = %d\n", procs[i].p_name, j, procs[i].times[j]);
    }*/
    // Initialize free block(s)
    init_free_blocks();
    printf("... Free blocks initalized\n");

    /*for(i = 0; i < bc; i++){
    printf("free blocks[%d]: %d, %d\n", i, list[i].start_index, list[i].size);
    }*/
    // Initialize memory representation
    init_memory();
    // Get method:
    char method[8];
    strcpy(method, argv[2]);
    int len = strlen(method);
    method[len] = '\0';
    if( method == NULL ){
        perror("Error: No method supplied!\n");
        return EXIT_FAILURE;
    }
    printf("Method acquired\n");
    printmemory();
    // Do the thing:
    int t_input;
    if( (strcasecmp(method, "first") == 0) ){
        while(1){
            printf("[FIRST:] Enter next time, or 0 to quit:\n");
            scanf("%d", &t_input);
            if( t_input == 0 )
                break;
            if( t_input < time_counter ){
                printf("Ascending times only, please\n");
                continue;
            }
            time_counter = t_input;
            First(time_counter);
            printmemory();
        }
    }
    else if( (strcasecmp(method, "next") == 0) ){
        while(1){
            printf("[NEXT:] Enter next time, or 0 to quit:\n");
            scanf("%d", &t_input);
            if( t_input == 0 )
                break;
            if( t_input < time_counter ){
                printf("Ascending times only, please\n");
                continue;
            }
            time_counter = t_input;
            Next(time_counter);
            printmemory();
        }
    }
    else if( (strcasecmp(method, "best") == 0) ){
        while(1){
            printf("[BEST:] Enter next time, or 0 to quit:\n");
            scanf("%d", &t_input);
            if( t_input == 0 )
                break;
            if( t_input < time_counter ){
                printf("Ascending times only, please\n");
                continue;
            }
            time_counter = t_input;
            Best(time_counter);
            printmemory();
        }
    }
    else if( (strcasecmp(method, "worst") == 0) ){
        while(1){
            printf("[WORST:] Enter next time, or 0 to quit:\n");
            scanf("%d", &t_input);
            if( t_input == 0 )
                break;
            if( t_input < time_counter ){
                printf("Ascending times only, please\n");
                continue;
            }
            time_counter = t_input;
            Worst(time_counter);
            printmemory();
        }
    }
    else{
        perror("Error: Invalid method! Options: first, next, best, worst\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
