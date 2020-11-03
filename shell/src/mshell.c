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
#include "builtins.h"

struct buffers{
	size_t placeInBuffer;
	size_t placeInLine;
	size_t howManyDidIRead;
	char buf[10 * MAX_LINE_LENGTH + 1];
	char firstLine[MAX_LINE_LENGTH + 1];
};

//name says everything
size_t count_args(command *com);

//copying strings representing args from command struct to array of null-terminated strings
void copy_args(char ** dest, command * source);

//free memory at Argv to prevent memory leaks
void free_Argv(char ** Argv);

//name says everything
//returns whether a builting has been executed
bool handle_builtins(char ** Argv);

//run command and handle possible errors
void execute_process(char ** Argv);

#define GO_END_LOOP 1
#define GO_BEG_LOOP -1
#define GO_NORMAL 0
#define GO_SYNTAX 2
//this handles buffers, reads from stdin and extracts lines one by one
//defines above are all the possible return values - they specify, what to do next
int read_before_parse(struct buffers * buf);

int main(int argc, char *argv[])
{
	pipelineseq * ln;
	command * com;
	size_t argcounter;
	struct buffers buf = {0, 0, 0};
	char ** Argv;
	__pid_t forked;
	struct stat fstat_buf;

	if(fstat(0,&fstat_buf))
		exit(EXIT_FAILURE);

	begin_main_loop:
		//writing prompt
		if(S_ISCHR(fstat_buf.st_mode))
			write(1, PROMPT_STR, strlen(PROMPT_STR));

		//reading from stdin and extracting first line to buf.firstLine
		switch(read_before_parse(&buf)){
		case GO_NORMAL:
			break;
		case GO_BEG_LOOP:
			goto begin_main_loop;
		case GO_END_LOOP:
			goto end_main_loop;
		case GO_SYNTAX:
			goto syntax_err;
		default:
			goto end_main_loop;
		}

		//parsing input line
		ln = parseline(buf.firstLine);
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

		//handle builtins
		if(handle_builtins(Argv))
			goto begin_main_loop;

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
			free_Argv(Argv);
			exit(EXIT_FAILURE);
		}

		//freeing memory at Argv
		free_Argv(Argv);

		goto begin_main_loop;
	end_main_loop:

	//end of line here makes it a bit more eye candy
	//end yet it doesn't sit well with the tests, so it's commented
	//write(1,"\n",1);

	exit(EXIT_SUCCESS);
}

int read_before_parse(struct buffers * buf){
	bool erasing_now = false;
	bool only_white = true;
	bool comment = false;
	if ((*buf).placeInBuffer >= (*buf).howManyDidIRead) {
		//reading line + memorizing, how many chars were there
		(*buf).howManyDidIRead = read(0, (*buf).buf, 10 * MAX_LINE_LENGTH);
		(*buf).placeInBuffer = 0;

		//if eof encountered - stop
		if ((*buf).howManyDidIRead <= 0)
			return GO_END_LOOP;

		//instead of clearing buffer - let's add zero after read content
		(*buf).buf[(*buf).howManyDidIRead] = 0;
	}

	//extracting first line
	for ((*buf).placeInLine = 0; (*buf).placeInBuffer < (*buf).howManyDidIRead;
			(*buf).placeInLine++, (*buf).placeInBuffer++) {
		//if current char is '#' - end line here, but consume rest from the buffer
		if ((*buf).buf[(*buf).placeInBuffer] == '#') {
			(*buf).firstLine[(*buf).placeInLine] = '\n';
			(*buf).firstLine[(*buf).placeInLine + 1] = 0;
			comment = true;
		}

		//copy current char from buffer to current line
		//skipped if we're in a comment or we are erasing a line too long to be parsed
		if (!erasing_now && !comment)
			(*buf).firstLine[(*buf).placeInLine] =
					(*buf).buf[(*buf).placeInBuffer];

		//if endline encountered - finalize extracting line
		if ((*buf).buf[(*buf).placeInBuffer] == '\n') {
			(*buf).placeInBuffer++;
			//we finished a line that is too long - syntax error it is
			if (erasing_now) {
				erasing_now = false;
				return GO_SYNTAX;
			}
			(*buf).firstLine[(*buf).placeInLine + 1] = 0;
			break;
		//apparently, upcoming line is too long - let's start erasing it from the buffer
		} else if ((*buf).placeInLine == MAX_LINE_LENGTH - 1)
			erasing_now = true;

		//buf is insufficient - let's move it forward
		if ((*buf).placeInBuffer >= (*buf).howManyDidIRead - 1) {
			//reading line + memorizing, how many chars were there
			(*buf).howManyDidIRead = read(0, (*buf).buf, 10 * MAX_LINE_LENGTH);
			(*buf).placeInBuffer = -1;

			//if eof encountered - stop
			if ((*buf).howManyDidIRead == 0) {
				(*buf).firstLine[(*buf).placeInLine + 1] = '\n';
				(*buf).firstLine[(*buf).placeInLine + 2] = 0;
				break;
			}

			//if read failed - end shell
			if ((*buf).howManyDidIRead < 0)
				return GO_END_LOOP;

			//instead of clearing buffer - let's add zero after read content
			(*buf).buf[(*buf).howManyDidIRead] = 0;
		}

	}

	for (int i = 0; i < strlen((*buf).firstLine); i++) {
		if (!isspace((*buf).firstLine[i]))
			only_white = false;
	}
	if (only_white || comment) {
		comment = false;
		return GO_BEG_LOOP;
	}
	only_white = true;
	return GO_NORMAL;
}

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

void free_Argv(char ** Argv){
	size_t i = 0;
	while(1){
		if(Argv[i] != NULL)
			free(Argv[i++]);
		else
			break;
	}
	free(Argv);
}

//returns true if any builtin has been found (and executed)
bool handle_builtins(char ** Argv){
	for(int i = 0; builtins_table[i].name != NULL; i++){
		if(strcmp(builtins_table[i].name, Argv[0]) == 0){
			if(builtins_table[i].fun(Argv)){
				write(2, "Builtin ", 8);
				write(2, Argv[0], strlen(Argv[0]));
				write(2, " error.\n", 8);
			}
			free_Argv(Argv);
			return true;
		}
	}
	return false;
}

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
