/**

* Holly Brice

* Heidi Niu

* CIS 415: Program 2

* 5/9/13

*/

#include <signal.h>

#include <unistd.h>

#include <sys/wait.h>

#include <stdlib.h>

#include <errno.h>

#include <sys/types.h>

#include <sys/stat.h>

#include <fcntl.h>

#include <stdio.h>

#include <string.h>

#include <assert.h>

#include <ctype.h>

#include "tokenizer.h"

#define MAX_LENGTH 1024    

#define SHELL_CODE "Voldemort# "

#define SHELL_CODE_LENGTH 11

#define MAX_NUMBER 100

pid_t pid;

int status;

int val; //used for toInt()

int result;

int new_stdout;

int new_stdin;

char *outputFile;

char *inputFile;

int marker;

char *array[20];    //holds tokens for inputs

void handler (int signum) {

  status = 1;

  if (signum == SIGALRM) {

      write(1, "Avada kedavra!\n", 15); //process failed

      kill(pid, SIGKILL);

  }

}

/**

* Initializes the tokenizer

*

* @param string the string that will be tokenized.  Should be non-NULL.

* @return an initialized string tokenizer on success, NULL on error.

*/

TOKENIZER *init_tokenizer( char *string )

{

   TOKENIZER *tokenizer;

   int len;

   assert( string != NULL );

   // COMMENTS BY HOLLY AND HEIDI

   tokenizer = (TOKENIZER *)malloc(sizeof(TOKENIZER));        // stores all parameters in tokenizer

   assert( tokenizer != NULL );                    // abort proram if function is false

     len = strlen(string) + 1;    /* don't forget \0 char */

     tokenizer->str = (char *)malloc(len);            //

     assert( tokenizer->str != NULL );

     memcpy( tokenizer->str, string, len );

     tokenizer->pos = tokenizer->str;

   return tokenizer;

}

/**

* Deallocates space used by the tokenizer.

* @param tokenizer a non-NULL, initialized string tokenizer

*/

void free_tokenizer( TOKENIZER *tokenizer )

{

   assert( tokenizer != NULL );

     free( tokenizer->str );

     free( tokenizer );

}

/**

* Retrieves the next token in the string.  The returned token is

* malloc'd in this function, so you should free it when done.

*

* @param tokenizer an initiated string tokenizer

* @return the next token

*/

char *get_next_token( TOKENIZER *tokenizer )

{

   assert( tokenizer != NULL );

   char *startptr = tokenizer->pos;

     char *endptr;

     char *tok;

     //char *moe;

   

   if( *tokenizer->pos == '\0' )    /* handle end-case */

           return NULL;

/* if current position is a delimiter, then return it */

     if( (*startptr == '|') || (*startptr == '&') ||

        (*startptr == '<') || (*startptr == '>') ) {

       tok = (char *)malloc(2);

           tok[0] = *startptr;

           tok[1] = '\0';

           tokenizer->pos++;

       return tok;

     }

     while( isspace(*startptr) )    /* remove initial white spaces */

           startptr++;

     if( *startptr == '\0' )

           return NULL;

     //moe = startptr;

   //char *temp;

   /*

     while (*moe != '\0') {

       if (*(moe) == '|') {

           marker = 1;

       }

      *moe++;

     }

   */

   

     /* go until next character is a delimiter */

     endptr = startptr;

     for( ;; ) {

       if( (*(endptr+1) == '|') || (*(endptr+1) == '&') || (*(endptr+1) == '<') ||

          (*(endptr+1) == '>') || (*(endptr+1) == '\0') || (isspace(*(endptr+1))) ) {

           tok = (char *)malloc( (endptr - startptr) + 2 );

           memcpy( tok, startptr, (endptr - startptr) + 1 );

           tok[(endptr - startptr) + 1] = '\0'; /* null-terminate the string */

             tokenizer->pos = endptr + 1;

           while( isspace(*tokenizer->pos) ) /* remove trailing white space */

                tokenizer->pos++;

                     return tok;

             }

       endptr++;

   }

   assert( 0 );            /* should never reach here */

   return NULL;            /* but satisfy compiler */

   }

int main (int argc, char *argv[]) {

   TOKENIZER *tokenizer;

      char string[256] = "";        // ascii table lenth?

      char *tok;            // char ptr called tok

     int br;             //

      //char *buf[1024];

      int pass;

      size_t numRead;

      

      char *ptr;    

      int i;

      char *first;

      marker = 0;

     int dupCheck;

   int num;

   int ind;

   int pipefd[2];        //array for pipe file descriptors

   char buf[1];        // used for piping

   int pipectr;        // keeps track of the number of pipes passed in    

   

      string[255] = '\0';       /* ensure that string is always null-terminated */

      write (1, SHELL_CODE, SHELL_CODE_LENGTH); //Output prompt

  

   while(1) {

       while ((br = read( STDIN_FILENO, string, 255 )) > 0) {

               if (br <= 1){

                       continue;

                  }

           string[br-1] = '\0';   /* remove trailing \n */

                     /* tokenize string */

           printf( "Parsing '%s'\n", string );

           tokenizer = init_tokenizer( string );

               first = get_next_token(tokenizer);    // first gets initial token            

           array[0] = first;

                  ind = 1;

           while( (tok = get_next_token( tokenizer )) != NULL ) {

               array[ind] = tok;

               ind++;

                      

               printf( "Got token '%s'\n", tok );              

                      //free( tok );    /* free the token now that we're done with it */

                  }

           while((tok = get_next_token( tokenizer )) != NULL ){ //frees tokens in array

               free(tok);    

               free(first);        

           }

                  break;

       }

       char *temp = array[0];

       int ctr = 0;
       
      
       int j;
       char *newArgv[20];
       for(j = 0; j< 20; j++){	//now this runs the right command correctly it jsut doesn't know how to redirect
			newArgv[j] = array[j];
	   } 
		

	

       if (array[1] == NULL) { //one command passed in

           inputFile = NULL;    

       }

       

       //if (array[2] == NULL) { //two commands passed in, only works for cat + filename

         //  inputFile = array[1];            

       //}

       pipectr = 0; //initially no pipes

       while( array[ctr] != NULL){

           //printf("%d\n", ctr);

           if (*array[ctr] == '<') {

               inputFile = array[ctr+1];

           }

           if(*array[ctr]== '>' ){

               outputFile = array[ctr+1];

           }

           if(*array[ctr] == '|'){

               pipectr++;

               if (pipectr > 1) { //only one pipeline allowed

                   perror("Too many pipes");

                   exit(1);

               }

               if(pipe(pipefd) ==-1){

                   perror(" PIPE RETURN -1");

               }

           }        

           ctr++;

       }

       

              status = 0;    

       pid = fork();    

       /* pid = 0 -> child; else it's a parent */

       if (!pid) { // if pid = child; process enters    

           if (inputFile != '\0'){    // has an input to pass in

       //printf("%s",inputFile);

                   

               close(pipefd[0]);    // close unused write end

               dup2(pipefd[0], 0);    // redirects the stdout

               //close(pipefd[1]);

               new_stdin = open(inputFile, O_RDONLY, 0);

               if(new_stdin == -1){

                   perror("OPEN FAIL");

               }                

               dupCheck = dup2(new_stdin, 0);

               if(dupCheck == -1){

                   perror("Dup Fail 1");

               }    

               close(new_stdin);

           }

           if(outputFile != '\0') {

               new_stdout = open(outputFile, O_WRONLY | O_CREAT, 0644);

               if (new_stdout == -1) {

                          perror("Bad input"); //double check this

                }

                  dupCheck = dup2(new_stdout, 1);

               if(dupCheck == -1){

                   perror("Dup Fail 2");

               }

               close(new_stdout);

           }

           

           result = execvp(first, newArgv);

           if (result == -1) { //Invalid command inputted

               perror("Invalid command");

               exit(1);

                  }    

              }

       else { //no time restriction given

           

           close(pipefd[1]);        // close unused read end

           dup2(pipefd[1], 1);

           while(read(pipefd[0], buf, 1) > 0){

               write(1, buf, 1);

           }    

           //close(pipefd[0]);            

           //dup2(pipefd[1], 0);    // redirects the stdout

       

           signal(SIGALRM, handler);

                 //wait(&pass);

               wait(NULL);            

                  

           if (status == 0) {

                      write(1, "I'll get you next time, Harry...\n", 33); //process passed

                  }

       }

       close(pipefd[0]);

       close(pipefd[1]);

       exit(1);    // THIS EXITS THE WHOLE PROGRAM <--FIX ME!

   }

  free(tokenizer);

}
