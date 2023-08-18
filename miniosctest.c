#include <stdio.h>
#include <math.h>

#define MINIOSC_IMPLEMENTATION
#include "miniosc.h"

int totalRX = 0;
int totalRXI = 0;

void rxcb( const char * address, const char * type, const void ** parameters )
{
	// Called when a message is received.  Check "type" to get parameters 
	// This message just blindly assumes it's getting a float.
	printf( "RXCB: %s %s ", address, type );
	int p = 0;
	if( type[0] == 0 ) return; // No type.
	totalRX++;
	for( const char * t = type + 1; *t; t++ )
	{
		switch( *t )
		{
			case 'f':
				printf( "%f ", (double)*((const float*)(parameters[p])) );
				break;
			case 'r': case 'c': case 'i':
				totalRXI += *((const uint32_t*)(parameters[p]));
				printf( "%d ", *((const uint32_t*)(parameters[p])) );
				break;
			case 's':
				printf( "%s ", ((const char *)(parameters[p])) );
				break;
		}
		p++;
	}
	printf( "\n" );
}


int frameno = 0;
int r = 0;
miniosc * osc;
miniosc * osc2;

int main()
{
	// 9001 is the input port, 9000 is the output port.
	// Each is optional.
	osc = minioscInit( 9001, 9000, "127.0.0.1", 0 );
	osc2 = minioscInit( 9000, 9001, "127.0.0.1", 0 );

	int j;
	for( j = 0; j < 10; j++ )
	{
		// Poll, waiting for up to 10 ms for a message.
		minioscPoll( osc, 10, rxcb );
		minioscPoll( osc2, 0, rxcb );

		char strtosend[128];
		sprintf( strtosend, "Frameno: %d", frameno );
		
		// Immediately send a message
		minioscSend( osc, "/label1", ",s", strtosend );

		// Bundle messages together and send.
		mobundle bun = { 0 };
		minioscBundle( &bun, "/avatar/parameters/parameter0", ",f", sin(frameno/100.) );//((frameno/20)%256)/255.0 );
		minioscBundle( &bun, "/avatar/parameters/parameter1", ",f", ((frameno/100)%256)/255.0 );
		minioscBundle( &bun, "/avatar/parameters/parameter2", ",f", (double)(r&1) );
		minioscBundle( &bun, "/text1", ",s", strtosend );
		minioscBundle( &bun, "/label2", ",i", frameno&1 );
		minioscBundle( &bun, "/box2", ",i", (frameno&255) );
		minioscBundle( &bun, "/composite", ",ifs", frameno, 3.24, "hello" );
		minioscSendBundle( osc, &bun );


		int cr = (frameno+0)    % 1536; if( cr > 768 ) cr = 1024-cr; if( cr > 255 ) cr = 255; if( cr < 0 ) cr = 0;
		int cg = (frameno+512)  % 1536; if( cg > 768 ) cg = 1024-cg; if( cg > 255 ) cg = 255; if( cg < 0 ) cg = 0;
		int cb = (frameno+1024) % 1536; if( cb > 768 ) cb = 1024-cb; if( cb > 255 ) cb = 255; if( cb < 0 ) cb = 0;
		minioscSend( osc2, "/opc/zone6", ",r", cr|(cg<<8)|(cb<<16) );

		frameno++;
	}
	
	for( j = 0; j < 90; j++ )
	{
		// Poll, waiting for up to 10 ms for a message.
		minioscPoll( osc, 2, rxcb );
		minioscPoll( osc2, 2, rxcb );
	}
	printf( "Totals: %d %d\n", totalRX, totalRXI );
	if( totalRX == 90 && totalRXI == 652940 ) return 0;
	fprintf( stderr, "\x01b[31mHmm, something may be wrong\n" );
	return -99;
}
