#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

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
	int argcounter, i;

begin_main_loop:
		write(1, "$ ", 2);
		howManyDidIRead = read(0, buf, MAX_LINE_LENGTH);
		if(howManyDidIRead == 0)
			goto end_main_loop;
		//parsing input line
		ln = parseline(buf);
		com = pickfirstcommand(ln);

		//start new process
		forked = fork();
		if(forked == 0){
			//new process - run command
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

			execvp(com->args->arg, Argv);

		i = 0;
		freeing:
				free(Argv[i++]);
				if(i >= argcounter)
					goto end_freeing;
				goto freeing;
		end_freeing:
			free(Argv);

			exit(EXIT_SUCCESS);
		} else if(forked > 0){
			waitpid(forked, NULL, 0);
		} else{
			return FAIL;
		}
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
