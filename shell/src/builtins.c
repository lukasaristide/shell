#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#include "builtins.h"

int lexit(char*[]);
int echo(char*[]);
int lcd(char*[]);
int lkill(char*[]);
int lls(char*[]);
int undefined(char *[]);

builtin_pair builtins_table[] = {
	{"exit",	&lexit},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

int lexit(char* argv[]){
	if(argv[1] != NULL)
		return -1;
	exit(EXIT_SUCCESS);
}

int echo( char * argv[]) {
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int undefined(char * argv[]) {
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}

int lcd(char* argv[]){
	if(argv[1] == NULL)
		return chdir(getenv("HOME"));
	if(argv[2] != NULL)
		return -1;
	return chdir(argv[1]);
}

int lkill(char* argv[]){
	if(argv[1] == NULL || (argv[1][0] != '-' && argv[2] != NULL) || (argv[1][0] == '-' && argv[3] != NULL))
		return -1;
	if(argv[1][0] != '-')
		return kill(atoi(argv[1]),SIGTERM);
	return kill(atoi(argv[2]), atoi(argv[1]+1));
}

int lls(char* argv[]){
	if(argv[1] != NULL)
		return -1;
	char path[PATH_MAX];
	DIR * cur_dir = opendir(getcwd(path,PATH_MAX));
	if(cur_dir == NULL)
		return -1;
	struct dirent * tmp_cont;
	while(tmp_cont = readdir(cur_dir)){
		if(tmp_cont->d_name[0] == '.')
			continue;
		write(1,tmp_cont->d_name,strlen(tmp_cont->d_name));
		write(1,"\n",1);
	}
	return closedir(cur_dir);
}

