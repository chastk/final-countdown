#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_MAX 1024
#define PROCS_MAX 26

// p_name is the letter for that procedure
// mem_size is the # of blocks it takes up
// times are the times in the input file when it either
//    enters or leaves memory. This *should* have an 
//    even number of entries.
// time_index gives the next index in times to be handled
// in_or_out = 0 if the procedure is OUT of memory,
//           = 1 if it the procedure is IN memory.
typedef struct procedure{
	char p_name;
	int mem_size;
	int * times;
	int time_index; // should always be set to 0 initially
	int in_or_out; // should always be set to 0 initially
}proc;

int numProcs;

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
		//     by default.
		procs[index].time_index = 0;
		procs[index].in_or_out = 0;
		// this process is initialized!
		// increment the index counter for the overall array,
		//    and get the next item in the loop
		index++;
	}// /while reading lines
	return procs;
}// /read_from_file

int main(int argc, char* argv[]){
	if(argc < 2){
		printf("USAGE: ./a.out <input.txt>\n");
		return EXIT_FAILURE;
	}
	char * filename = argv[1];
	FILE * input = fopen(filename, "r");
	if( input == NULL ){
		printf("Invalid input file =C\n");
		return EXIT_FAILURE;
	}
	struct procedure * procs = read_from_file(input);
	int i;
	for (i = 0; i < numProcs; i++){
		printf("%c:: %d blocks\n", procs[i].p_name, procs[i].mem_size);
	}

	return EXIT_SUCCESS;
}
