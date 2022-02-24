#include <stdio.h>

#define MINIOSC_IMPLEMENTATION
#include "miniosc.h"

void rxcb( const char * address, const char * type, void ** parameters )
{
	printf( "RXCB: %s %s [%p %p] %f\n", address, type, type, parameters[0], (double)*((float*)parameters[0]) );
}

int main()
{
	miniosc * osc = minioscInit( 9003, 9005, "127.0.0.1", 0 );

	int frameno = 0;
	while( 1 )
	{
		int r = minioscPoll( osc, 1, rxcb );

		char strtosend[128];
		sprintf( strtosend, "Frameno: %d", frameno );
		minioscSend( osc, "/label1", ",s", strtosend );

		mobundle bun = { 0 };
		minioscBundle( &bun, "/text1", ",s", strtosend );
		minioscBundle( &bun, "/label2", ",i", frameno&1 );
		minioscBundle( &bun, "/box2", ",i", (frameno&255) );
		minioscSendBundle( osc, &bun );

		frameno++;
	}
}
