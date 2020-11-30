#ifndef SRC_MY_INCLUDES_H_
#define SRC_MY_INCLUDES_H_

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"
#include "globals.h"

struct buffers{
	size_t placeInBuffer;
	size_t placeInLine;
	long long howManyDidIRead;
	char buf[15 * MAX_LINE_LENGTH + 1];
	char firstLine[MAX_LINE_LENGTH + 1];
};

#include "method_decl.h"

#endif /* SRC_MY_INCLUDES_H_ */
