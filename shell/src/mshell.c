#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"

//name says everything
size_t count_args(command *com){
	argseq * argseq = com->args;
	size_t argcounter = 0;
	while(1){
		argcounter++;
		argseq = argseq->next;
		if(argseq==com->args)
			break;
	}
	return argcounter;
}

//copying strings representing args from command struct to array of null-terminated strings
void copy_args(char ** dest, command * source){
	argseq * argseq = source->args;
	size_t i = 0;
	while(1){
		dest[i] = malloc(strlen(argseq->arg)+1);
		strcpy(dest[i++], argseq->arg);
		argseq = argseq->next;
			if(argseq==source->args)
				break;
	}
}

//free memory at Argv to prevent memory leaks
void free_Argv(char ** Argv, int argcounter){
	size_t i = 0;
	while(1){
		free(Argv[i++]);
		if(i >= argcounter)
			break;
	}
	free(Argv);
}

//run command and handle possible errors
void execute_process(char ** Argv){
	if(-1 == execvp(Argv[0], Argv)){
		int errorno = errno;
		//write command's text
		write(2, Argv[0], strlen(Argv[0]));
		switch(errorno){
		case ENOENT:
			//no such file or dir
			write(2,": no such file or directory\n",28);
			break;
		case EACCES:
			//permission denied
			write(2,": permission denied\n",20);
			break;
		default:
			//anything else that can possibly go wrong
			write(2,": exec error\n",13);
		}
		exit(EXEC_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	pipelineseq * ln;
	command * com;
	size_t howManyDidIRead = 0;
	size_t argcounter;
	size_t placeInBuffer = 0, placeInLine = 0, i;
	char buf[10 * MAX_LINE_LENGTH + 1];
	char firstLine[MAX_LINE_LENGTH + 1];
	char ** Argv;
	__pid_t forked;
	struct stat fstat_buf;
	int fstat_status;
	bool erasing_now = false;
	bool only_white = true;
	bool comment = false;

	if(fstat(0,&fstat_buf))
		exit(EXIT_FAILURE);

	begin_main_loop:
		//writing prompt
		if(S_ISCHR(fstat_buf.st_mode))
			write(1, PROMPT_STR, strlen(PROMPT_STR));

		if(placeInBuffer >= howManyDidIRead){
			//reading line + memorizing, how many chars were there
			howManyDidIRead = read(0, buf, 10 * MAX_LINE_LENGTH);
			placeInBuffer = 0;

			//if eof encountered - stop
			if(howManyDidIRead <= 0)
				goto end_main_loop;

			//instead of clearing buffer - let's add zero after read content
			buf[howManyDidIRead] = 0;
		}

		//extracting first line
		for(placeInLine = 0; placeInBuffer < howManyDidIRead; placeInLine++, placeInBuffer++){
			//if current char is '#' - end line here, but consume rest from the buffer
			if(buf[placeInBuffer] == '#'){
				firstLine[placeInLine] = '\n';
				firstLine[placeInLine+1] = 0;
				comment = true;
			}

			//copy current char from buffer to current line
			//skipped if we're in a comment or we are erasing a line too long to be parsed
			if(!erasing_now && !comment)
				firstLine[placeInLine] = buf[placeInBuffer];

			//if endline encountered - finalize extracting line
			if(buf[placeInBuffer] == '\n'){
				placeInBuffer++;
				//we finished a line that is too long - syntax error it is
				if(erasing_now){
					erasing_now = false;
					goto syntax_err;
				}
				firstLine[placeInLine+1] = 0;
				break;
			} else if(placeInLine == MAX_LINE_LENGTH - 1)
				erasing_now = true;

			//buf is insufficient - let's move it forward
			if(placeInBuffer >= howManyDidIRead - 1){
				//reading line + memorizing, how many chars were there
				howManyDidIRead = read(0, buf, 10 * MAX_LINE_LENGTH);
				placeInBuffer = -1;

				//if eof encountered - stop
				if(howManyDidIRead == 0){
					firstLine[placeInLine+1] = '\n';
					firstLine[placeInLine+2] = 0;
					break;
				}

				if(howManyDidIRead < 0)
					goto end_main_loop;

				//instead of clearing buffer - let's add zero after read content
				buf[howManyDidIRead] = 0;
			}

		}

		for(i = 0; i < strlen(firstLine); i++){
			if(!isspace(firstLine[i]))
				only_white = false;
		}
		if(only_white || comment){
			comment = false;
			goto begin_main_loop;
		}
		only_white = true;

		//parsing input line
		ln = parseline(firstLine);
		//seperating first command
		com = pickfirstcommand(ln);

		//handling syntax errors
		if(com == NULL){
		syntax_err:
			write(2, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
			write(2,"\n",1);
			goto begin_main_loop;
		}

		//rewriting parsed args to array
		//first, let's count them
		argcounter = count_args(com);

		//let's reserve enough place for them all
		Argv = malloc(sizeof(char*)*(argcounter+1));
		Argv[argcounter] = NULL;

		//and let's copy them to said array
		copy_args(Argv, com);

		//starting new process
		forked = fork();
		if(forked == 0){
			//new process - run command
			execute_process(Argv);
		} else if(forked > 0){
			//parent process - wait for child precess to end
			waitpid(forked, NULL, 0);
		} else{
			//if somehow we didn't manage to fork, let's end with error - that's not good
			exit(EXIT_FAILURE);
		}

		//freeing memory at Argv
		free_Argv(Argv, argcounter);

		goto begin_main_loop;
	end_main_loop:

	//end of line here makes it a bit more eye candy
	//end yet it doesn't sit well with the tests, so it's commented
	//write(1,"\n",1);

	exit(EXIT_SUCCESS);
}
