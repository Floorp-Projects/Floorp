/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html) 
 */

/*
 * nssocket.c
 * @author derekt@netscape.com
 * @version 1.0
 */

/* our includes */
#include "nssocket.h"

/* system includes */
#include <string.h>

#ifdef XP_UNIX
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif /* XP_UNIX */

#ifdef WIN32
#include <winsock.h>
#endif /* WIN32 */

/* constants */
const int		kn_DefaultMTU = (32*1024);	/* 64kBytes, was 4 kBytes */
const double	defaultTimeout  = 1800.0;

void NSSock_Initialize()
{
#ifdef WIN32
	const WORD kWS_VERSION_REQUIRED = 0x0101;
	WSADATA wsaData;

	/* Initialize Windows Sockets */
	WSAStartup(kWS_VERSION_REQUIRED,&wsaData);
#endif /* WIN32 */
}

void NSSock_Terminate()
{
#ifdef WIN32
	WSACleanup();
#endif /* WIN32 */
}

/* Creates a socket. */
int NSSock_Socket ( int in_af, int in_type, int in_protocol )
{
    return socket( in_af, in_type, in_protocol );
}


/* Connect to the specified ip address with the specified port. */
int NSSock_Connect( int in_socket, 
                    const struct sockaddr * in_pHostInetAddr, 
                    int in_port )
{
	struct sockaddr_in ThisInetAddr;    /* our address */

	/* initialize the address structures */
	memset (&ThisInetAddr, 0, sizeof(ThisInetAddr));

	ThisInetAddr.sin_family      = AF_INET;
	ThisInetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ThisInetAddr.sin_port        = htons(0);


    if ( bind( in_socket, (struct sockaddr *)&ThisInetAddr, sizeof (ThisInetAddr)) < 0 )
    {
        return 0;
    }

    if ( connect( in_socket, (struct sockaddr *)in_pHostInetAddr, sizeof (*in_pHostInetAddr)) < 0)
    {
        return 0;
    }

    return 1;
}

/* Write the supplied buffer out to the socket. */
int NSSock_Write( int in_socket, char * in_buffer, int in_length )
{
    int nRet;
	int nSent = 0;

	/* send all the data in the output buffer */
	while ( nSent < in_length )
	{
   		nRet = send ( in_socket, &(in_buffer[nSent]), in_length-nSent, 0 );
        if ( nRet < 0 )
        {
            return 0;
        }
		nSent += nRet;
	}

    return (nSent == in_length);  
}

/* Read data from the socket into the buffer. */
int NSSock_Read( int in_socket, char * io_buffer, int in_length )
{
    int nRet;

    /* read data from the socket */
    nRet = recv ( in_socket, io_buffer, in_length, 0 );

    /* fail if an error occurred */
    if (nRet <= 0)
    {
        return 0;
    }

    return( nRet );
}

/* Close the socket. */
int NSSock_Close( int in_socket )
{
    /* disable further operations */
    shutdown (in_socket, 0);
#ifdef WIN32
        return ( closesocket( in_socket ) == 0 );
#endif /* WIN32 */

#ifdef XP_UNIX
        return ( close( in_socket ) == 0 );
#endif /* XP_UNIX */
}

/* Select used to simulate timeouts. */
int NSSock_Select( int nfds, 
            fd_set * readfds, 
            fd_set * writefds, 
            fd_set * exceptfds, 
            struct timeval * timeout)
{
    int l_retCode = select( nfds, readfds,  writefds, exceptfds, timeout );

    if ( l_retCode <= 0 )
    {
        return 0;
    }

    return l_retCode;
}

/* Get the host name. */
int NSSock_GetHostName( char * out_hostName, int in_maxLength )
{
    if ( gethostname( out_hostName, in_maxLength ) != 0 )
    {
		return 0;
    }

    return 1;
}

/* Get the host by name. */
LinkedList_t * NSSock_GetHostByName( const char * in_hostName )
{
#ifdef HPUX
    int l_nReturn;
    struct hostent_data * l_hostent_data;
#endif /* HPUX */
#ifdef AIX
    int l_nReturn;
    struct hostent_data * l_hostent_data;
#endif /* AIX */
#ifdef SOLARIS
    int l_nReturn;
    int l_nBufferLength;
    int l_nIter = 1;
    char * l_buffer;
#endif /* SOLARIS */
    struct hostent * l_hostent;
    char ** l_ppAddressList;
    char * l_pAddress;
    char * l_pTempAddress;
    LinkedList_t * l_list = 0;

#ifdef WIN32
    l_hostent = gethostbyname( in_hostName );

    if ( l_hostent == 0 )
    {
        return 0;
    }

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }
#endif /* WIN32 */

#ifdef AIX
    l_hostent = (struct hostent*)malloc( sizeof(struct hostent) );
    l_hostent_data = (struct hostent_data*)malloc( sizeof(struct hostent_data) );
	
    /* initialize the address structures */
    memset(l_hostent_data, 0, sizeof(struct hostent_data));

    /* derekt:check for an error here */
    l_nReturn = gethostbyname_r( in_hostName, l_hostent, l_hostent_data );
    
    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }

    free( l_hostent_data );
    free( l_hostent );
#endif /* AIX */

#ifdef HPUX
    /* derekt:check for an error here */
    if ( (l_hostent = gethostbyname( in_hostName )) == NULL )
    {
        return 0;
    }

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }

#ifdef OLDHPUX
    l_hostent = (struct hostent*)malloc( sizeof(struct hostent) );
    l_hostent_data = (struct hostent_data*)malloc( sizeof(struct hostent_data) );

	/* initialize the address structures */
    l_hostent_data->current = 0;

    /* derekt:check for an error here */
    l_nReturn = gethostbyname_r( in_hostName, l_hostent, l_hostent_data );

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }

    free( l_hostent_data );
    free( l_hostent );
#endif /* OLDHPUX */
#endif /* HPUX */
    
#ifdef SOLARIS
    l_hostent = (struct hostent*)malloc( sizeof(struct hostent) );
    l_nBufferLength = 2048;
    l_buffer = (char*)malloc( l_nBufferLength * sizeof(char) * l_nIter );

    while( gethostbyname_r( in_hostName, 
                            l_hostent, 
                            l_buffer, 
                            l_nBufferLength, 
                            &l_nReturn ) == 0 )
    {
        free( l_buffer );
        if ( l_nReturn == ERANGE )
        {
            l_buffer = (char*)malloc( l_nBufferLength * sizeof(char) * ++l_nIter );
        }
        else
        {
            free( l_hostent );
            return 0;
        }
    }

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }

    free( l_buffer );
    free( l_hostent );
#endif /* SOLARIS */

#if defined (IRIX) || defined (IRIX64) 
    l_hostent = gethostbyname( in_hostName );

    if ( l_hostent == 0 )
    {
        return 0;
    }

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }

#endif /* IRIX */

#ifdef OSF1
    l_hostent = gethostbyname( in_hostName );

    if ( l_hostent == 0 )
    {
        return 0;
    }

    for( l_ppAddressList = l_hostent->h_addr_list; *l_ppAddressList != 0; l_ppAddressList++ )
    {
        struct in_addr in;

        memcpy( &in.s_addr, *l_ppAddressList, sizeof(in.s_addr));
        l_pTempAddress = inet_ntoa(in);
        l_pAddress = (char*)malloc( (strlen( l_pTempAddress ) +1) * sizeof(char) );
        strcpy( l_pAddress, l_pTempAddress );
        AddElement( &l_list, l_pAddress );
    }
#endif /* OSF1 */

    /*
     * Add any addional platforms here
     */

    return l_list;
}


