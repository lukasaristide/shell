#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>


#include "config.h"
#include "siparse.h"
#include "utils.h"

//name says everything
size_t count_args(command *com){
	argseq * argseq = com->args;
	size_t argcounter = 0;
	begin_counting:
		argcounter++;
		argseq = argseq->next;
		if(argseq==com->args)
			goto end_counting;
		goto begin_counting;
	end_counting:
	return argcounter;
}

//copying strings representing args from command struct to array of null-terminated strings
void copy_args(char ** dest, command * source){
	argseq * argseq = source->args;
	size_t i = 0;
	begin_rewriting:
		dest[i] = malloc(strlen(argseq->arg)+1);
		strcpy(dest[i++], argseq->arg);
		argseq = argseq->next;
			if(argseq==source->args)
				goto end_rewriting;
		goto begin_rewriting;
	end_rewriting:
	return;
}

//free memory at Argv to prevent memory leaks
void free_Argv(char ** Argv, int argcounter){
	size_t i = 0;
	freeing:
		free(Argv[i++]);
		if(i >= argcounter)
			goto end_freeing;
		goto freeing;
	end_freeing:
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
		case EPERM:
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
	size_t howManyDidIRead, argcounter;
	char buf[MAX_LINE_LENGTH];
	char ** Argv;
	__pid_t forked;
	struct stat fstat_buf;
	int fstat_status;

	if(fstat(0,&fstat_buf))
		exit(EXIT_FAILURE);

	begin_main_loop:
		//writing prompt
		if(S_ISCHR(fstat_buf.st_mode))
			write(1, PROMPT_STR, strlen(PROMPT_STR));

		//clearing buffer
		memset(buf, 0, MAX_LINE_LENGTH);

		//reading line + memorizing, how many chars were there
		howManyDidIRead = read(0, buf, MAX_LINE_LENGTH);

		//if eof encountered - stop
		if(howManyDidIRead <= 0)
			goto end_main_loop;

		//handling too long input lines
		if(buf[howManyDidIRead-1] != '\n'){
			erasing:
				read(0, buf, 1);
				if(buf[0] == '\n')
					goto end_erasing;
				goto erasing;
			end_erasing:
			goto syntax_err;
		}

		//parsing input line
		ln = parseline(buf);
		//seperating first command
		com = pickfirstcommand(ln);

		//handling another syntax errors
		if(com == NULL){
		syntax_err:
			//this shall prevent throwing too many errors when entering empty line
			if(buf[0] == '\n')
				goto begin_main_loop;
			write(2, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
			write(2,"\n",1);
			goto begin_main_loop;
		}

		//rewriting parsed args to array
		//first, let's count them
		argcounter = count_args(com);

		//let's reserve enough place for them all
		Argv = malloc(sizeof(char*)*(--argcounter));

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
	write(1,"\n",1);

	exit(EXIT_SUCCESS);
}
