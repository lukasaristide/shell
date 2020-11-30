#ifndef SRC_GLOBALS_H_
#define SRC_GLOBALS_H_

//global defines concerning remembering child processes
#define MAX_PROCESSES_NUMBER 1000
size_t cur_processes_number = 0;
__pid_t CHILD_PROCESSES[MAX_PROCESSES_NUMBER];
int CHILD_STATUSES[MAX_PROCESSES_NUMBER];

//table for foreground processes
__pid_t FOREGR_PROCESSES[1000];
	size_t place_forg = 0;

//mainly for writing newline and prompt in case of sigint
bool write_prompt_if_sig = false;

//name says everything
bool is_not_file = false;

//possible endings of functions
#define GO_END_LOOP 1
#define GO_BEG_LOOP -1
#define GO_NORMAL 0
#define GO_SYNTAX 2

//useful thing
#define write_to_stdout(str) write(STDOUT_FILENO,str,strlen(str))

#endif /* SRC_GLOBALS_H_ */
