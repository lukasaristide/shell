#ifndef SRC_METHOD_DECL_H_
#define SRC_METHOD_DECL_H_
#include "my_includes.h"

//look at child process - if it has already finished
bool look_child(int i);

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

//this handles buffers, reads from stdin and extracts lines one by one
//defines above are all the possible return values - they specify, what to do next
int read_before_parse(struct buffers * buf);

//it does what it says
int deal_with_pipeline(pipeline * pline);

//it deals with placing files on stdin/out
bool handle_redir(redir * r);

void set_sigchild();
void set_sigint();


#endif /* SRC_METHOD_DECL_H_ */
