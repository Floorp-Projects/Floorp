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

#ifndef NSSOCKET_H
#define NSSOCKET_H

/*
 * nssocket.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "linklist.h"
#include <stdio.h>

#ifdef XP_UNIX
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif /* XP_UNIX */

#ifdef AIX
#include <sys/select.h>
#endif /* AIX */
#ifdef WIN32
#include <winsock.h>
#endif /* WIN32 */

#ifdef __cplusplus
extern "C" { 
#endif /* __cplusplus */

/* Functions used to initialize and terminate socket IO. */
void NSSock_Initialize();

/* Functions used to initialize and terminate socket IO. */
void NSSock_Terminate();

/* Close the socket connection. */
int NSSock_Close( int in_socket );

/* Connect to the specified host. */
int NSSock_Connect( int in_socket, 
                    const struct sockaddr * in_pHostInetAddr, 
                    int in_port );

/* Read data from the socket. */
int NSSock_Read( int in_socket, char * io_buffer, int in_length );

/*
 * Select to determine the status of one or more sockets.
 * We use this to simulate timeouts.
 */
int NSSock_Select( int in_nfds, 
                    fd_set * in_readfds, 
                    fd_set * in_writefds, 
                    fd_set * in_exceptfds,
                    struct timeval * in_timeout );

/* Create a new socket descriptor. */
int NSSock_Socket ( int in_af, int in_type, int in_protocol );

/* Write data to the socket. */
int NSSock_Write( int in_socket, char * in_buffer, int in_length );

/* Call the platform specific getHostbyName(). */
int NSSock_GetHostName( char * out_hostName, int in_maxLength );

LinkedList_t * NSSock_GetHostByName( const char * in_hostName );

/*
int Flush( NSMAIL_Socket_t * in_socket );

char * GetHostByAddr( char * in_dottedDecimal );
char * GetHostByName( const char * in_hostName );
*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NSSOCKET_H */
