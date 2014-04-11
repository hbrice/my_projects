/*
* Holly Brice
* CIS 415: Proj1
* Create a Shell in C.
*/
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>

pid_t pid;
#define BUF_SIZE 1024
#define MAX_ARG_NUM 100
int flag;

/** signal handler function */
void handler (int signum){								// the process has taken longer than the alarm timer allows
	flag = 1;
	write(1, "What do you mean there's no pie? Just my luck.\n", 48);
	kill(pid, SIGKILL);									// kill the child process
}

int main(int argc, char *argv[]){
	
	int status;											// used in wait
	char *ptr;      									// pointer for parsing
	char *head;											// pointer for parsing
	ssize_t numOfRead; 		 					 
	char *extras[MAX_ARG_NUM]; 							// for parsing
	int i;
	int params;											// number of paramters in array
	int alarmCtr; 										// takes in the seconds to allow process to run

// create 
when i set alarmCtr... check if args[1] =<0: if so, perror
					else{ set alarmCtr

	if(argv[1]!='\0'){
		alarmCtr = atoi(argv[1]);   					// the input of time
	}	
	
	char* readSize = (char*)malloc(1024); 				// readSize = array that holds input
	for(i = 0; i< 1024; i++){							// setting everything to null in array
		readSize[i] = NULL;
	} 
	
	char *newargv[] = { argv[0], NULL}; 				 // for execve //char *newargv[] = { "ls", NULL}; //original	
	char *newenviron[] = {NULL};	 					 // for execve
 
	/** the loop */
	while(1){
		
		flag = 0;										// reset flag
		params = 0; 									// initialize to zero
		write(1, "DEAN# ", 6);							// writes the user input into the terminal 
		numOfRead = read(0, readSize, 1024);			// attempts to read up to count bytes from the fd into the buffer 
		
		for(i= 0; i< 1024; i++){						// set end of line char to null
			if(readSize[i] == '\n'){
				readSize[i] = NULL;
			}
		}	
			
		if(numOfRead >= BUF_SIZE){ 						// error handling
			perror("Length of args exceeds the limits");	
			continue;
		}
		
		/** START OF PARSING */
		readSize[numOfRead-1] = '\0';					// defensive checking for the reading buffer to see if null
		ptr = readSize;  								// ptr is now pointing to readSize array
		head = readSize;								// head is now pointing to readSize array
		while(*ptr != '\0'){							// if ptr isn't null:
			if(*ptr != ' '){							// if ptr isn't a space char
				*ptr++;									// move ptr to next element in array 
			}else{										// Have reached a space in the input
				*ptr = '\0';							// ptr changes it to a end of line
				if(params < MAX_ARG_NUM -1){
					extras[params] = head;
					params++;							// increment number of parameters
					ptr += 1;							// ptr increases
					head = ptr;							// head now pts to ptr
				}else{
					break;
				}
			}
		}
		 /** END OF PARSING */
		
		switch(pid = fork()){								// creates new process  
		case -1: 											
			perror("system fork failure");
			break;
		case 0:												// if no child then create the child process
			execve(readSize, newargv, newenviron);			// execute program readSize
			break;									
		default:											// child process was already made									//child process was already made
			if(argv[1]!='\0'){
				signal(SIGALRM, handler);  					// Alarm handler: After alrm ctr seconds, exit program. 
				alarm(alarmCtr); 							//calls alarm clock on 10 seconds change to ctrtime
				wait(&status);
				alarm(0);
				if(flag == 0){
					write(1, "This was great, I'm having pie tonight!\n", 41);
				}
			}else{											// there isn't a number paramter passed in
				signal(SIGALRM, handler);  					// Alarm handler: After alrm ctr seconds, exit program.
				wait(&status);
				if(flag == 0){
					write(1, "This was great, I'm having pie tonight!\n", 41);
				}
			}
		}
	} 
	free(readSize);											// free up the memory after code is done
}



//add in this functions
int myatoi(char *string){
	int sign = 1; 		// positive or negaitve
	int length = 0;		// how many characters in the string
	int i =0;			// start index at 0
	int number = 0;		// 
	//*ptr = string;	//ptr goes to start of string
	while(*string != '\0'){
		length++;
	}
	for (int p = 0; p < length; p++){
		if(string[0] == '-'){
			sign = -1;
			i = 1; //look past sign start index at 1
		}
		
		/// example if you "425"
		//  number = 0*10 + *string - '0' = 0+52-48 = 4
		//	number = 4*10 + 
		for(i; i<length; i++){
			number = number * 10 + (string[i]- '0');			// '0' is 48 in ascii //string[i] could be (*string)
		}
		number = number * sign;
		return number;
	}
	
}

