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

#ifndef NSIO_H
#define NSIO_H

/*
 * nsio.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "nssocket.h"

#define DEFAULT_BUFFER_LENGTH 1024

const static char eoline[] = "\r\n";

/*IO structure.*/
typedef struct IO
{
    int socket;
    struct timeval timeout;
    nsmail_io_fns_t ioFunctions;
    unsigned int readBufferLimit;
    unsigned int writeBufferLimit;
    unsigned int readBufferSize;
    unsigned int writeBufferSize;
    char * readBuffer;
    char * writeBuffer;
} IO_t;

/*Initialize the IO structure.*/
/*You must make sure that in_bufferSize is at least large
  enough to hold an entire line including CRLF*/
int IO_initialize( IO_t * in_pIO, 
                    unsigned int in_readBufferLimit, 
                    unsigned int in_writeBufferLimit );

/*Free the all members of the IO structure.*/
void IO_free( IO_t * in_pIO );

/*Connect to the specified server at the specified port.*/
int IO_connect( IO_t * in_pIO, const char * in_szServer, unsigned short in_nPort );

/*Disconnect from the server.*/
int IO_disconnect( IO_t * in_pIO );

/*Send a byte of data to the server.*/
int IO_writeByte( IO_t * in_pIO, const char in_byte );

/*Send data to the server.*/
int IO_write( IO_t * in_pIO, const char * in_data, unsigned int in_length );

/*Send data to the server appending the \r\n and optionally flushing.*/
int IO_send( IO_t * in_pIO, const char * in_data, boolean in_bufferData );

/*Send a byte of data to the server.*/
int IO_flush( IO_t * in_pIO );

/*Read size number of bytes directly from the socket.*/
int IO_read( IO_t * in_pIO, char * out_response, int in_size, int * out_bytesRead );

/*Reads a line of data from the socket.*/
int IO_readLine( IO_t * in_pIO, char * out_response, int in_maxLength );

/*Reads a line of data from the socket and allocates the proper amount of space for out_ppResponse.*/
int IO_readDLine( IO_t * in_pIO, char** out_ppResponse );

/*Frees the line dynamically allocated by IO_readDLine.*/
int IO_freeDLine( char ** out_ppResponse );

/*Set the timeout for reading data from the socket.*/
int IO_setTimeout( IO_t * in_pIO, double in_timeout );

/* Function to set IO functions.*/
void IO_setFunctions( IO_t * in_pIO, nsmail_io_fns_t * in_pIOFunctions );

/* Functions to get IO functions.*/
void IO_getFunctions( IO_t * in_pIO, nsmail_io_fns_t * in_pIOFunctions );

#endif /* NSIO_H */
