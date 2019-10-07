#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "siparse.h"
#include "siparseutils.h"

void
resetutils(void){
		resetbuffer();
		resetargvs();
		resetcommands();
		resetredirs();
		resetredirseqs();
		resetpipelines();
		resetpipelineseqs();
}

/*
 * buffer for string from the parsed line
 */
static char linebuffer[MAX_LINE_LENGTH+1];
static const char *linebufferend= linebuffer+MAX_LINE_LENGTH;
static char *bufptr=linebuffer;

/*
 * name is 0 terminated string
 * length include terminating 0
 */
char *
copytobuffer(const char *name, const short length){
	char *saved;
	int copied;

	saved=bufptr;
	bufptr+=length;
	if (bufptr > linebufferend) return NULL;
	strncpy(saved, name, length);
	return saved;
}

void 
resetbuffer(void){
	bufptr= linebuffer;
}

/* 
 * buffer for args
 * each argv is NULL terminated substring of the buffer
 */
static char * argvs[MAX_ARGS*2];
static char **nextarg = argvs;
static char **currentargv = argvs;

char ** 
appendtoargv(char* arg){
	*nextarg = arg;
	nextarg++;
	return currentargv;
}

char **
closeargv(void){
	char ** oldstart;

	appendtoargv(NULL);
	oldstart = currentargv;
	currentargv = nextarg;
	return oldstart;
}

void
resetargvs(void){
	currentargv=argvs;
	nextarg=currentargv;
}

/*
 * buffer for commands
 */
static command commandsbuf[MAX_COMMANDS+1];
static command * nextcom=commandsbuf;

command *
nextcommand(void){
	command * curr;
	
	curr = nextcom;
	nextcom++;
//	nextcom->argv=NULL;
	return curr;
}

void
resetcommands(void){
	nextcom= commandsbuf;
}

/* 
 * buffer for redirections
 */
static redirection redirbuf[MAX_REDIRS];
static redirection *nextred;

redirection * nextredir(void){
	return (nextred++);	
}

void resetredirs(void){
	nextred = redirbuf;
};

static redirection *  redirseqbuf[MAX_REDIRS*2];
static redirection ** currentredirseq;
static redirection ** currentredirseqstart;

redirection ** appendtoredirseq(redirection * redir){

	*currentredirseq = redir;
	currentredirseq++;

	return currentredirseqstart;
}

redirection ** closeredirseq(void){
	redirection ** oldstart;
		
	oldstart = appendtoredirseq(NULL);
	currentredirseqstart = currentredirseq;
	
	return oldstart;
};

void resetredirseqs(void){
	currentredirseqstart = redirseqbuf;
	currentredirseq = redirseqbuf;
}

/*
 * pipelines buffer
 */

static command *	commandseqbuf[MAX_COMMANDS*2];
static command **	currentcommandseqend;
static command **	currentcommandseqstart;

static pipeline 	pipelinebuf[MAX_PIPELINES];
static pipeline * 	currentpipeline;

pipeline * appendtopipeline(command * comm){

	*currentcommandseqend = comm;
	currentcommandseqend++;

	return currentpipeline;
}

pipeline * closepipeline(void){

	appendtopipeline(NULL);
	currentpipeline->commands = currentcommandseqstart;
	currentpipeline->flags = 0;

	currentcommandseqstart = currentcommandseqend;

	return (currentpipeline++);
};

void resetpipelines(void){
	currentpipeline = pipelinebuf;
	currentcommandseqstart=currentcommandseqend= commandseqbuf;
//	currentpipelinestart = pipelinesbuf;
}

/*
 * pipelinesseq buffer
 */

static pipeline*	pipelineseqbuf[MAX_PIPELINES*2];
static pipeline**	currentpipelineseqend;	//first free entry
static pipeline**	currentpipelineseqstart;

pipelineseq appendtopipelineseq(pipeline * pline){

	*currentpipelineseqend = pline;
	currentpipelineseqend++;

	return currentpipelineseqstart;
}

void lastpipelinesetflags(int flags){
	if (currentpipelineseqstart==currentpipelineseqend){
		//pipeline seq empty
		return;
	}
	pipeline * last = *(currentpipelineseqend-1);
	last->flags = flags; 
}

pipelineseq closepipelineseq(void){
	pipelineseq oldstart;
		
	oldstart = appendtopipelineseq(NULL);
	currentpipelineseqstart = currentpipelineseqend;
	
	return oldstart;
};

void resetpipelineseqs(void){
	currentpipelineseqend = pipelineseqbuf;
	currentpipelineseqstart=currentpipelineseqend = pipelineseqbuf;
}
/*
 * printing
void printcommand(command *com){
	char **arg;
	redirection ** red;


	printf("argv:\n");	
	for (arg = com->argv; *arg; arg++){
		printf(":%s:", *arg);
	}
	printf("\n");

	printf("redirections:\n");
	for (red = com->redirs; *red; red++){
		printf("filename:%s: flags: %d\n", (*red)->filename, (*red)->flags); 
	}
	printf("\n");
}
 */
