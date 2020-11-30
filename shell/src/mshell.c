#include "std_includes.h"
#include "my_includes.h"

int main(int argc, char *argv[])
{
	struct buffers buf = {0, 0, 0};
	pipelineseq * ln, * curln;
	commandseq * comseq;
	struct stat fstat_buf;

	//let's block sigint
	set_sigint();

	//override handler for sigchld
	set_sigchild();

	if(fstat(0,&fstat_buf))
		exit(EXIT_FAILURE);

	is_not_file = S_ISCHR(fstat_buf.st_mode);

	begin_main_loop: {
		//writing prompt
		if(is_not_file){
			//writing possible process endings
			for(int i = 0; i < cur_processes_number; i++){
				if(look_child(i))
					i--;
			}
			write(STDOUT_FILENO, PROMPT_STR, strlen(PROMPT_STR));
		}

		write_prompt_if_sig = true;
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
		write_prompt_if_sig = false;

		//parsing input line
		ln = parseline(buf.firstLine);

		//handling syntax errors
		if(ln == NULL){
		syntax_err:
			write(STDERR_FILENO, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
			write(STDERR_FILENO,"\n",1);
			goto begin_main_loop;
		}

		//iterating through pipelines
		curln = ln;
		do {
			switch(deal_with_pipeline(curln->pipeline)){
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
			curln = curln->next;
		} while(curln != ln);

		goto begin_main_loop;
	} end_main_loop:

	//no zombies - wait for background processes
	//while(cur_processes_number--)
	//	look_child(cur_processes_number, true);

	//end of line here makes it a bit more eye candy
	//end yet it doesn't sit well with the tests, so it's commented
	//write(1,"\n",1);

	exit(EXIT_SUCCESS);
}

void handler_sigchld(int signal, siginfo_t * info, void * ucontext){
	int status;
	for(size_t i = 0; i < place_forg; i++){
		if(waitpid(FOREGR_PROCESSES[i],&status,WNOHANG) > 0){
			FOREGR_PROCESSES[i--] = FOREGR_PROCESSES[--place_forg];
		}
	}
	for(size_t i = 0; i < cur_processes_number; i++){
		if(waitpid(CHILD_PROCESSES[i],&status,WNOHANG) > 0){
			CHILD_STATUSES[i] = status;
		}
	}
	//CHILD_PROCESSES[cur_processes_number++] = info->si_pid;
	//CHILD_STATUSES[cur_processes_number] = info->si_status;
}

void set_sigchild(){
	struct sigaction act;
	act.sa_sigaction = handler_sigchld;
	act.sa_flags = SA_SIGINFO; // | SA_NOCLDWAIT;
	sigaction(SIGCHLD, &act, NULL);
}

void handler_sigint(int sig) {
	if(write_prompt_if_sig && is_not_file){
		write(STDOUT_FILENO,"\n",1);
		write(STDOUT_FILENO, PROMPT_STR, strlen(PROMPT_STR));
	}
}

void set_sigint(){
	struct sigaction act;
	act.sa_handler = handler_sigint;
	sigaction(SIGINT, &act, NULL);
}

bool look_child(int i){
	int status_child = CHILD_STATUSES[i];
	if(status_child == -777)
		return false;
	//if(!waitpid(CHILD_PROCESSES[i],&status_child, hang ? 0 : WNOHANG))
	//	return false;

	char pid_str[10];
	//looks like my gcc doesn't know of itoa - hence this sprintf
	sprintf(pid_str,"%d",CHILD_PROCESSES[i]);

	write_to_stdout("Background process ");			//write(STDOUT_FILENO,"Background process (",20);
	write_to_stdout(pid_str);						//write(STDOUT_FILENO,pid_str,strlen(pid_str));
	write_to_stdout(" terminated.");					//write(STDOUT_FILENO,") terminated.", 12);

	//exited - let's write exit code
	if(WIFEXITED(status_child)){
		char return_val[10];
		sprintf(return_val,"%d",WEXITSTATUS(status_child));
		write_to_stdout(" (exited with status ");	//write(STDOUT_FILENO," (exited with status (",22);
		write_to_stdout(return_val);					//write(STDOUT_FILENO,return_val,strlen(return_val));
		write_to_stdout(")\n");						//write(STDOUT_FILENO,"))\n",3);
	//received signal
	} else if(WIFSIGNALED(status_child)) {
		char signal_no[10];
		sprintf(signal_no,"%d",WTERMSIG(status_child));
		write_to_stdout(" (killed by signal ");
		write_to_stdout(signal_no);
		write_to_stdout(")\n");
	}
	CHILD_PROCESSES[i] = CHILD_PROCESSES[--cur_processes_number];
	CHILD_STATUSES[i] = CHILD_STATUSES[cur_processes_number];
	return true;
}

int read_before_parse(struct buffers * buf){
	bool erasing_now = false;
	bool only_white = true;
	bool comment = false;
	if ((*buf).placeInBuffer >= (*buf).howManyDidIRead) {
		//reading line + memorizing, how many chars were there

	read_repeat:
		(*buf).howManyDidIRead = read(STDIN_FILENO, (*buf).buf, 10 * MAX_LINE_LENGTH);
		(*buf).placeInBuffer = 0;

		//interrupted - repeat
		if ((*buf).howManyDidIRead < 0 && errno == EINTR)
			goto read_repeat;

		//if eof encountered - stop
		if ((*buf).howManyDidIRead == 0)
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
			(*buf).firstLine[(*buf).placeInLine] = 0;
			break;
		//apparently, upcoming line is too long - let's start erasing it from the buffer
		} else if ((*buf).placeInLine == MAX_LINE_LENGTH - 1)
			erasing_now = true;

		//buf is insufficient - let's move it forward
		if ((*buf).placeInBuffer >= (*buf).howManyDidIRead - 1) {
			//reading line + memorizing, how many chars were there
			(*buf).howManyDidIRead = read(STDIN_FILENO, (*buf).buf, 10 * MAX_LINE_LENGTH);
			(*buf).placeInBuffer = -777;

			//if eof encountered - stop
			if ((*buf).howManyDidIRead == 0) {
				(*buf).firstLine[(*buf).placeInLine + 1] = '\n';
				(*buf).firstLine[(*buf).placeInLine + 2] = 0;
				break;
			}

			//interrupted - repeat
			if ((*buf).howManyDidIRead < 0 && errno == EINTR)
				goto read_repeat;

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
				write(STDERR_FILENO, "Builtin ", 8);
				write(STDERR_FILENO, Argv[0], strlen(Argv[0]));
				write(STDERR_FILENO, " error.\n", 8);
			}
			free_Argv(Argv);
			return true;
		}
	}
	return false;
}

void execute_process(char ** Argv){
	if(-777 == execvp(Argv[0], Argv)){
		int errorno = errno;
		//write command's text
		write(STDERR_FILENO, Argv[0], strlen(Argv[0]));
		switch(errorno){
		case ENOENT:
			//no such file or dir
			write(STDERR_FILENO,": no such file or directory\n",28);
			break;
		case EACCES:
			//permission denied
			write(STDERR_FILENO,": permission denied\n",20);
			break;
		default:
			//anything else that can possibly go wrong
			write(STDERR_FILENO,": exec error\n",13);
		}
		exit(EXEC_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

int deal_with_pipeline(pipeline * pline){
	commandseq * cseq;
	redirseq * redseq;
	size_t argcounter;
	char ** Argv;
	__pid_t forked, fork_bg = 0;
	bool might_be_builtin = true;

	if(!pline || !pline->commands)
		return GO_SYNTAX;

	cseq = pline->commands;
	do{
		if(!cseq->com || !cseq->com->args || !cseq->com->args->arg || 1 >= strlen(cseq->com->args->arg))
			return GO_SYNTAX;
	} while (cseq != pline->commands);


	//fork and run it in bg
	//if(pline->flags & INBACKGROUND)
	//	if(fork_bg = fork())
	//		return GO_BEG_LOOP;

	int pipe_before[2], pipe_after[2];
	pipe(pipe_before);
	close(pipe_before[1]);
	pipe(pipe_after);

	do {
		might_be_builtin = true;
		//rewriting parsed args to array
		//first, let's count them
		argcounter = count_args(cseq->com);

		//let's reserve enough place for them all
		Argv = malloc(sizeof(char*)*(argcounter+1));
		Argv[argcounter] = NULL;

		//and let's copy them to said array
		copy_args(Argv, cseq->com);

		if(handle_builtins(Argv)){
			for(int i = 0; i < 2; i++){
				close(pipe_after[i]);
				close(pipe_before[i]);
			}
			return GO_BEG_LOOP;
		}

		//starting new process

		forked = fork();
		if(forked > 0){
			if(pline->flags & INBACKGROUND) {
				CHILD_STATUSES[cur_processes_number] = -777;
				CHILD_PROCESSES[cur_processes_number++] = forked;
			} else {
				FOREGR_PROCESSES[place_forg++] = forked;
			}

			//reseting pipes
			close(pipe_before[0]);
			close(pipe_after[1]);
			for(int i = 0; i < 2; i++)
				pipe_before[i] = pipe_after[i];
			pipe(pipe_after);

			//freeing memory at Argv
			free_Argv(Argv);

			cseq = cseq->next;
			continue;
		} else if (forked < 0){
			//if somehow we didn't manage to fork, let's end with error - that's not good
			exit(EXIT_FAILURE);
		}

		if(pline->flags & INBACKGROUND)
			setsid();

		bool redir_in = false, redir_out = false;

		//iteratng through redirs and handling them
		redseq = cseq->com->redirs;
		do {
			if(!redseq || !redseq->r) break;

			if(handle_redir(redseq->r)){
				if(IS_RIN(redseq->r->flags))
					redir_in = true;
				else
					redir_out = true;
			} else exit(EXIT_SUCCESS);

			redseq = redseq->next;
		} while (redseq != cseq->com->redirs);


		if(!redir_in && cseq != pline->commands){
			dup2(pipe_before[0],STDIN_FILENO);
			close(pipe_before[0]);
		}
		else
			close(pipe_before[0]);
		if(!redir_out && cseq->next != pline->commands){
			dup2(pipe_after[1],STDOUT_FILENO);
			close(pipe_after[1]);
		}
		else{
			close(pipe_after[0]);
			close(pipe_after[1]);
		}

		//run command
		execute_process(Argv);

	} while (cseq != pline->commands);

	//closing pipes
	for(int i = 0; i < 2; i++){
		if(pipe_after[i] != STDIN_FILENO && pipe_after[i] != STDOUT_FILENO && pipe_after[i] != STDERR_FILENO)
			close(pipe_after[i]);
		if(pipe_before[i] != STDIN_FILENO && pipe_before[i] != STDOUT_FILENO && pipe_before[i] != STDERR_FILENO)
			close(pipe_before[i]);
	}

	//waiting for processes
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGINT);
	while(place_forg > 0){
		sigsuspend(&set);
		for(size_t i = 0; i < place_forg; i++){
			if(0 < waitpid(FOREGR_PROCESSES[i],NULL,WNOHANG))
				FOREGR_PROCESSES[i--] = FOREGR_PROCESSES[--place_forg];
		}
	}


	//if(pline->flags & INBACKGROUND)
	//	exit(EXIT_SUCCESS);

	return GO_NORMAL;
}

bool handle_redir(redir * r){
	//backup - in case something goes wrong, let's not lose out stdin/stdout
	int fd, backup, err;

	//if there is a file on input
	if(IS_RIN(r->flags)){
		backup = dup(STDIN_FILENO);
		close(STDIN_FILENO);
		fd = open(r->filename,O_RDONLY);
		err = errno;
		if(fd < 0)
			dup2(backup,STDIN_FILENO);
		close(backup);
	//if we shall write to file
	} else if(IS_ROUT(r->flags) || IS_RAPPEND(r->flags)){
		backup = dup(STDOUT_FILENO);
		close(STDOUT_FILENO);
		fd = open(r->filename, IS_ROUT(r->flags) ? O_WRONLY|O_CREAT|O_TRUNC : O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		err = errno;
		if(fd < 0)
			dup2(backup,STDOUT_FILENO);
		close(backup);
	}

	//if something went wrong - write, what exactly
	if(fd < 0){
		write(STDERR_FILENO, r->filename, strlen(r->filename));
		switch(err){
		case EACCES:
			write(STDERR_FILENO, ": permission denied\n", 20);
			break;
		case ENOENT:
			write(STDERR_FILENO, ": no such file or directory\n", 28);
			break;
		default:
			write(STDERR_FILENO, ": weird error\n", 14);
		}
		return false;
	}
	return true;
}
