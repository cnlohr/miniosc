#include <stdio.h>

#define MINIOSC_IMPLEMENTATION
#include "miniosc.h"

void rxcb( const char * address, const char * type, void ** parameters )
{
	// Called when a message is received.  Check "type" to get parameters 
	// This message just blindly assumes it's getting a float.
	printf( "RXCB: %s %s [%p %p] %f\n", address, type, type, parameters[0],
		(double)*((float*)parameters[0]) );
}

int main()
{
	// 9003 is the input port, 9005 is the output port.
	// Each is optional.
	miniosc * osc = minioscInit( 9003, 9005, "127.0.0.1", 0 );

	int frameno = 0;
	while( 1 )
	{
		// Poll, waiting for up to 10 ms for a message.
		int r = minioscPoll( osc, 10, rxcb );

		char strtosend[128];
		sprintf( strtosend, "Frameno: %d", frameno );
		
		// Immediately send a message
		minioscSend( osc, "/label1", ",s", strtosend );

		// Bundle messages together and send.
		mobundle bun = { 0 };
		minioscBundle( &bun, "/text1", ",s", strtosend );
		minioscBundle( &bun, "/label2", ",i", frameno&1 );
		minioscBundle( &bun, "/box2", ",i", (frameno&255) );
		minioscSendBundle( osc, &bun );

		frameno++;
	}
}