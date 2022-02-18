#ifndef _MINIOSC_H
#define _MINIOSC_H

/* 
 miniosc.h - a small single-file header OSC library (including sockets) for C.
  It only suppors a subset of the OSC protocol. i, f, s, and b.
  
 Copyright 2022 <>< cnlohr - you may use this file freely.
 
 It may be licensed under the MIT/x11, NewBSD, CC0 or Public Domain where applicable.
*/

#ifndef MINIOSCBUFFER
#define MINIOSCBUFFER 1536
#endif

#define MINIOSC_ERROR_SOCKET    -1
#define MINIOSC_ERROR_BIND      -2
#define MINIOSC_ERROR_CONNECT   -2
#define MINIOSC_ERROR_PARAMS    -3
#define MINIOSC_ERROR_OVERFLOW  -4
#define MINIOSC_ERROR_TRANSPORT -5 //May not be fatal.
#define MINIOSC_ERROR_PROTOCOL  -6

#define MINIOSC_MORE           1

typedef struct
{
	int sock; // Surely there will be other things.
} miniosc;

// Initialize a miniosc.  It mallocs the data in the return structure.  If there was an
// error, it will return NULL, and minioscerrorcode will be populated if it was nonnull.
miniosc * minioscInit( int portnumber, char * remoteAddressOrNull, int * minioscerrorcode );

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


#ifdef MINIOSC_IMPLEMENTATION

#include <stdarg.h>
#include <stdint.h>

#if defined(WINDOWS) || defined(WIN32) || defined(WIN64)
#define MINIOSCWIN
#endif

#ifdef MINIOSCWIN
	#include <WinSock2.h>
	#include <Windows.h>
	#define socklen_t int
#else
	#include <string.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <net/if.h>
	#define SOCKET int
	#define closesocket close
#endif


#ifdef __cplusplus
#define RESTRICT
#else
#define RESTRICT restrict
#endif

// Internal functions
static int _minioscAppend( RESTRICT char * start, RESTRICT char ** ptr, int max_len, int datalen, RESTRICT const char * data )
{
	int quad_byte_length = ((datalen + 3) & (~3));
	if( *ptr - start + quad_byte_length >= max_len ) return MINIOSC_ERROR_OVERFLOW;
	memcpy( *ptr, data, datalen );
	memset( *ptr + datalen, 0, quad_byte_length - datalen );
	*ptr += quad_byte_length;
	return 0;
}

static int _minioscGetQBL( const char * start, const char ** here, int length )
{
	// Note this function does not round up so it can be used with blobs.
	char * h = *here;
	do
	{
		if( h-start >= length ) return -1;
		if( ! *h ) break;
		h++;
	} while( 1 );
	int thislen = h-start;
	int quad_byte_length = ((thislen + 3) & (~3));	
	*here += quad_byte_length;
	return thislen;
}

miniosc * minioscInit( int portnumber, char * remoteAddressOrNull, int * minioscerrorcode )
{
#ifdef MINIOSCWIN
	WSADATA rep;
	if( WSAStartup( MAKEWORD( 2, 2 ), &rep ) != 0 )
	{
		// Really, this code can't be executed past windows 3.11, windows for workgroups
		// but I like having it here, as a faint reminder of the terrible world from
		// whence we've come.
		if( minioscerrorcode ) *minioscerrorcode = MINIOSC_ERROR_SOCKET;
		return 0;
	}
#endif

	int sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	
	if( sock == -1 )
	{
		if( minioscerrorcode ) *minioscerrorcode = MINIOSC_ERROR_SOCKET;
		return 0;
	}
	
	struct sockaddr_in peeraddy;

	if( remoteAddressOrNull )
	{
		// We have a remote address, therefore we are outgoing.
		m_pairaddr.sin_family = AF_INET;
		m_pairaddr.sin_port = htons( portnumber );
		m_pairaddr.sin_addr.s_addr = inet_addr( remoteAddressOrNull );

		// If you find the OS's buffer is insufficient, we could override 
		// setsockopt( m_sockfd, SOL_SOCKET, SO_RCVBUF, ( const char * ) &n, sizeof( n ) ) ...
		//
		// Possible future feature: bind to a specific network device.
		// setsockopt( m_sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof( ifr ) ) ...
		
		if( connect( sock, ( const struct sockaddr * ) &m_pairaddr, sizeof( m_pairaddr ) ) < 0 )
		{
			if( minioscerrorcode ) *minioscerrorcode = MINIOSC_ERROR_CONNECT;
			closesocket( sock );
			return 0;
		}		
	}
	else
	{
		// We have no remote address, therefore we are bound.
		peeraddy.sin_family = AF_INET;
		peeraddy.sin_addr.s_addr = INADDR_ANY;
		peeraddy.sin_port = htons( portnumber );

		// Allow address reuse. No reason to force exclusivity. NBD if it fails
		int opt = 1;
		setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, ( const char * ) &opt, sizeof( opt ) );

		// Bind the socket with the server address 
		if ( bind( sock, ( const struct sockaddr * )&peeraddy, sizeof( peeraddy ) ) < 0 )
		{
			closesocket( sock );
			if( minioscerrorcode ) *minioscerrorcode = MINIOSC_ERROR_BIND;
			return 0;
		}
	}
	

#ifndef MINIOSCWIN
	// Tricky: We want to make OSC use telephony QoS / ToS.  This
	// will reduce its latency in places where it would be inappropriate
	// to queue too many packets.
	// Officially, 46<<2; //46 = Telephony, priority.
	// But, experimentally, most wifi drivers respond better with 0xff.
	int tos_local = 0xff;
	socklen_t len = sizeof( tos_local );
	if( setsockopt( m_sockfd, IPPROTO_IP, IP_TOS, &tos_local, sizeof( tos_local ) ) )
	{
		// Not a big deal if this fails.  We can fail silently.  Alternatively
		// we could after setting also check to make sure it was actually
		// set by calling getsockopt.
	}
#endif
	if( minioscerrorcode ) *minioscerrorcode = 0;
	miniosc * ret = malloc( sizeof( miniosc ) );
	ret->sock = sock;
	return ret;
}


int minioscSend( miniosc * osc, int flags, const char * path, const char * type, ... )
{
	int err;
	char buffer[MINIOSCBUFFER];
	char * bptr = buffer;

	if( !path || path[0] != '/' || !type || type[0] != ',' ) return MINIOSC_ERROR_PARAMS;

	if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, strlen( path ) + 1, path ) ) ) return err;
	if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, strlen( type ) + 1, type ) ) ) return err;
	
	va_list ap;
    va_start(ap, type);
	char * t;
	char c;
	for( t = type+1; (c=*t); t++ )
	{
		switch( c )
		{
		case 'i':
		{
			int i = va_arg(ap, unsigned int);
			i = htonl( i );
			if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, 4, &i ) ) ) return err;
			break;
		}
		case 'f':
		{
			float f = va_arg( ap, double ); //Quirk: var args in C use doubles not floats because of automatic type promotion.
			if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, 4, &f ) ) ) return err;
			break;
		}
		case 's':
		{
			const char * st = va_arg( ap, const char * );
			int sl = strlen( st );
			if( sl == 0 )
			{
				// Tricky: If an empty string, we replace it with a null set of chars.
				st = "\0\0\0\0";
				sl = 4;
			}
			if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, strlen( st ), st ) ) ) return err;
			break;
		}
		case 'b':
		{
			int len = va_arg( ap, int );
			const void * st = va_arg( ap, const void * );
			int lensend = htonl( len );
			if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, 4, &lensend ) ) ) return err;
			if( ( err = _minioscAppend( buffer, *bptr, MINIOSCBUFFER, len, st ) ) ) return err;
			break;
		}
		default:
			return MINIOSC_ERROR_PARAMS;
		}
	}
	va_end( end );
	
	// We have constructed a valid packet.  Send it.
	int sendlen = bptr - buffer;
	if( send( osc->sock, buffer, sendlen, MSG_NOSIGNAL | ((flags&1)?MSG_MORE:0) ) != sendlen )
		return MINIOSC_ERROR_TRANSPORT;
	return 0;
}

int minioscPoll( miniosc * osc, int timeoutms, void (*callback)( const char * path, const char * type, void ** parameters ) )
{
	int r;
	// Do a poll.
	
	#ifdef MINIOSCWIN
	WSAPOLLFD pfd = { 0 };
	pfd.fd = osc->sock;
	pfd.events = POLLRDNORM;
	r = WSAPoll( &pfd, 1, timeoutms );
	#else
	pollfd pfd = { 0 };
	pfd.fd = osc->sock;
	pfd.events = POLLRDNORM;
	r = poll( &pfd, 1, timeoutms );		
	#endif

	if( r < 0 ) return MINIOSC_ERROR_TRANSPORT;
	
	// No data.
	if( r == 0 ) return 0;

	char buffer[MINIOSCBUFFER];
	int r = recv( osc->sock, buffer, sizeof( buffer ), MSG_NOSIGNAL );
	if( r <= 0 ) return MINIOSC_ERROR_TRANSPORT;
	
	char * eptr = buffer;
	int rxed = 0;

	do
	{
		const char * name = eptr;
		int namelen = _minioscGetQBL( buffer, &eptr, r );
		const char * type = eptr;
		int typelen = _minioscGetQBL( buffer, &eptr, r );
		if( namelen <= 0 || typelen <= 0 || name[0] != '/' || type[0] != ',' )
		{
			return MINIOSC_ERROR_PROTOCOL;
		}
		void * parameters[MINIOSCBUFFER/4]; // Worst-case scenario: all blobs.
		int p = 0;
		char * ct;
		for( ct = type+1; *ct; ct++ )
		{
			switch( *ct )
			{
			case 'i':
			{
				if( eptr-buffer+4 > r ) return MINIOSC_ERROR_PROTOCOL;
				*((int *)eptr = htonl( *((int *)eptr );
				parameters[p++] = eptr;
				eptr += 4;
				break;
			}
			case 'f':
			{
				if( eptr-buffer+4 > r ) return MINIOSC_ERROR_PROTOCOL;
				parameters[p++] = eptr;
				eptr += 4;
				break;
			}
			case 's':
			{
				char * st = eptr;
				int sl = _minioscGetQBL( buffer, &eptr, r );
				if( sl < 0 ) MINIOSC_ERROR_PROTOCOL;
				if( sl == 0 )
				{
					parameters[p] = "\0\0\0\0";
					eptr += 4
				}
				else parameters[p++] = st;
				break;
			}
			case 'b':
			{
				if( eptr-buffer+4 > r ) return MINIOSC_ERROR_PROTOCOL;
				parameters[p++] = eptr;
				eptr += 4;
				char * st = eptr;
				int sl = _minioscGetQBL( buffer, &eptr, r );
				if( sl < 0 ) return MINIOSC_ERROR_PROTOCOL;
				if( sl == 0 ) parameters[p++] = "\0\0\0\0";
				else parameters[p++] = st;
				break;
			}
		}
		callback( name, type, parameters );
		rxed++;
	}
	return rxed;
}

void minioscClose( miniosc * osc )
{
	closesocket( osc->sock );
	osc->sock = 0;
	free( osc );
}

#endif

#endif
