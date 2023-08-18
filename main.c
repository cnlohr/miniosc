#include <stdio.h>
#include <math.h>

#define MINIOSC_IMPLEMENTATION
#include "miniosc.h"

#include "os_generic.h"

void rxcb( const char * address, const char * type, const void ** parameters )
{
	// Called when a message is received.  Check "type" to get parameters 
	// This message just blindly assumes it's getting a float.
	printf( "RXCB: %s %s [%p %p] %f\n", address, type, type, parameters[0],
		(double)*((float*)parameters[0]) );
}


int frameno = 0;
int r = 0;
miniosc * osc;
miniosc * osc2;

void * thread( void * v )
{
	while( 1 )
	{
		getchar();
		r++;
	}
}

int main()
{
	// 9003 is the input port, 9005 is the output port.
	// Each is optional.
	osc = minioscInit( 9001, 9000, "127.0.0.1", 0 );
	osc2 = minioscInit( 0, 9993, "127.0.0.1", 0 );

	OGCreateThread( thread, 0 );

	while( 1 )
	{
		// Poll, waiting for up to 10 ms for a message.
		minioscPoll( osc, 15, rxcb );

		char strtosend[128];
		sprintf( strtosend, "Frameno: %d", frameno );
		
		// Immediately send a message
	//	minioscSend( osc, "/label1", ",s", strtosend );

		// Bundle messages together and send.
		mobundle bun = { 0 };
		minioscBundle( &bun, "/avatar/parameters/parameter0", ",f", sin(frameno/100.) );//((frameno/20)%256)/255.0 );
		minioscBundle( &bun, "/avatar/parameters/parameter1", ",f", ((frameno/100)%256)/255.0 );
		minioscBundle( &bun, "/avatar/parameters/parameter2", ",f", (double)(r&1) );
	//	minioscBundle( &bun, "/text1", ",s", strtosend );
	//	minioscBundle( &bun, "/label2", ",i", frameno&1 );
	//	minioscBundle( &bun, "/box2", ",i", (frameno&255) );
		minioscSendBundle( osc, &bun );


		int cr = (frameno+0)    % 1536; if( cr > 768 ) cr = 1024-cr; if( cr > 255 ) cr = 255; if( cr < 0 ) cr = 0;
		int cg = (frameno+512)  % 1536; if( cg > 768 ) cg = 1024-cg; if( cg > 255 ) cg = 255; if( cg < 0 ) cg = 0;
		int cb = (frameno+1024) % 1536; if( cb > 768 ) cb = 1024-cb; if( cb > 255 ) cb = 255; if( cb < 0 ) cb = 0;
		minioscSend( osc2, "/opc/zone6", ",r", cr|(cg<<8)|(cb<<16) );

		frameno++;
	}
}