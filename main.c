#include <stdio.h>

#define MINIOSC_IMPLEMENTATION
#include "miniosc.h"

int main()
{
	miniosc * server = minioscInit( 12665, 0, 0 );
	miniosc * client = minioscINit( 12665, "127.0.0.1", 0 );
	

	// Initialize a miniosc.  It mallocs the data in the return structure.  If there was an
// error, it will return NULL, and minioscerrorcode will be populated if it was nonnull.

// Send an OSC message, this will pull the types off of varargs.
//  'i': send an int, include an int in your varargs.
//  'f': send a float, include a float (or rather auto-promoted double) to your varargs.
//  's': send a string, include a char * in your varargs.
//  'b': send a blob, include an int and then a char * in your varargs. 
// You must prefix your path with '/' and you must prefix type with ','
int minioscSend( miniosc * osc, int flags, const char * path, const char * type, ... );

// Poll for an OSC message.
//   - If the value is negative, there was an error, it reports the error.
//   - If the value is zero, no messages were received, it was timed out.
//   - If the value is positive, it reports the number of messages received and processed.
int minioscPoll( miniosc * osc, int timeoutms, void (*callback)( const char * path, const char * type, void ** parameters ) );

// Close the socket. Free the memory.
void minioscClose( miniosc * osc );

}