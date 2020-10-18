#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"

int
main(int argc, char *argv[])
{
	pipelineseq * ln;
	command *com;
	argseq * argseq;
	size_t howManyDidIRead;
	char buf[MAX_LINE_LENGTH];
	char ** Argv;
	__pid_t forked;
	int argcounter, i, errorno;

begin_main_loop:
		write(1, PROMPT_STR, 2);
		memset(buf, 0, MAX_LINE_LENGTH);
		howManyDidIRead = read(0, buf, MAX_LINE_LENGTH);
		if(howManyDidIRead == 0)
			goto end_main_loop;
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
		com = pickfirstcommand(ln);
		if(com == NULL){
		syntax_err:
			if(buf[0] == '\n')
				goto begin_main_loop;
			write(2, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
			write(2,"\n",1);
			goto begin_main_loop;
		}
		argseq = com->args;

		argcounter = 0;
	begin_counting:
			argcounter++;
			argseq = argseq->next;
			if(argseq==com->args)
				goto end_counting;
			goto begin_counting;
	end_counting:

		Argv = malloc(sizeof(char*)*(--argcounter));

		i = 0;
	begin_rewriting:
			Argv[i] = malloc(strlen(argseq->arg)+1);
			strcpy(Argv[i++], argseq->arg);
			argseq = argseq->next;
				if(argseq==com->args)
					goto end_rewriting;
			goto begin_rewriting;
	end_rewriting:

		//start new process
		forked = fork();
		if(forked == 0){
			//new process - run command


			if(-1 == execvp(com->args->arg, Argv)){
				errorno = errno;
				write(2, com->args->arg, strlen(com->args->arg));
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
					write(2,": exec error\n",13);
				}
				exit(EXEC_FAILURE);
			}
		} else if(forked > 0){
			waitpid(forked, NULL, 0);
		} else{
			return FAIL;
		}

	//freeing Argv
		i = 0;
	freeing:
			free(Argv[i++]);
			if(i >= argcounter)
				goto end_freeing;
			goto freeing;
	end_freeing:
		free(Argv);

		goto begin_main_loop;
end_main_loop:
	return 0;
/*
	while (fgets(buf, 2048, stdin)){	
		ln = parseline(buf);
		printparsedline(ln);
	}

	return 0;

	ln = parseline("ls -las | grep k | wc ; echo abc > f1 ;  cat < f2 ; echo abc >> f3\n");
	printparsedline(ln);
	printf("\n");
	com = pickfirstcommand(ln);
	printcommand(com,1);

	ln = parseline("sleep 3 &");
	printparsedline(ln);
	printf("\n");
	
	ln = parseline("echo  & abc >> f3\n");
	printparsedline(ln);
	printf("\n");
	com = pickfirstcommand(ln);
	printcommand(com,1);
*/
}
