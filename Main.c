#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

char memory [1600];

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

int main (int argc, char* argv[]) {
    if (argc==3 || argc==4) {
        int i,fd;
        for (i=0; i<80; i++) {
            memory[i]='#';
        }
        for (i=80; i<1600; i++) {
            memory[i]='.';
        }
		if (argc==3) {
			fd=open(argv[2], O_RDONLY);
			
		}
        printmemory(0);
        return EXIT_SUCCESS;
    }
    else {
        printf("ERROR WRONG NUMBER OF ARGUMENTS\n");
        return EXIT_FAILURE;
    }
}
