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

#ifndef NSMAIL_H
#define NSMAIL_H

/*
 * nsio.h
 * @author prasad
 * @version 1.0
 */


#ifdef XP_UNIX
#include <sys/time.h>
#endif /* XP_UNIX */
#ifdef AIX
#include <sys/select.h>
#endif /* AIX */
#ifdef WIN32
#include <winsock.h>
#endif /* WIN32 */

#define FALSE 0
#define TRUE 1

typedef unsigned char boolean;

#ifdef XP_UNIX
#define BOOLEAN int
#endif


/*
Definitions for various options that might want to be set.
*/
typedef enum NSMAIL_Options
{
    NSMAIL_OPT_IO_FN_PTRS
} NSMAIL_Options_t;

/*
Error codes.
*/
#define NSMAIL_OK                     		 0
#define NSMAIL_ERR_UNINITIALIZED       		-1
#define NSMAIL_ERR_INVALIDPARAM       		-2
#define NSMAIL_ERR_OUTOFMEMORY        		-3
#define NSMAIL_ERR_UNEXPECTED         		-4
#define NSMAIL_ERR_IO                 		-5
#define NSMAIL_ERR_IO_READ            		-6
#define NSMAIL_ERR_IO_WRITE            		-7
#define NSMAIL_ERR_IO_SOCKET           		-8
#define NSMAIL_ERR_IO_SELECT           		-9
#define NSMAIL_ERR_IO_CONNECT          		-10
#define NSMAIL_ERR_IO_CLOSE          		-11
#define NSMAIL_ERR_PARSE      		        -12
#define NSMAIL_ERR_TIMEOUT           		-13
#define NSMAIL_ERR_INVALID_INDEX	      	-14
#define NSMAIL_ERR_CANTOPENFILE	      		-15
#define NSMAIL_ERR_CANT_SET               	-16
#define NSMAIL_ERR_ALREADY_SET            	-17
#define NSMAIL_ERR_CANT_DELETE            	-18
#define NSMAIL_ERR_CANT_ADD               	-19
#define NSMAIL_ERR_SENDDATA        	        -20
#define NSMAIL_ERR_MUSTPROCESSRESPONSES		-21
#define NSMAIL_ERR_PIPELININGNOTSUPPORTED	-22
#define NSMAIL_ERR_ALREADYCONNECTED     	-23
#define NSMAIL_ERR_NOTCONNECTED     	        -24

#define NSMAIL_ERR_EOF       			 -1
#define NSMAIL_ERR_NOTIMPL            		-99

#define ERRORLOG				/* turn on error log */

struct sockaddr;

/*
Definition for a structure to plug in specific io functionality.
*/
typedef struct nsmail_io_fns 
{
    int (*liof_read)( int, char *, int );
    int (*liof_write)( int, char *, int );
    int (*liof_socket)( int, int, int );
    int (*liof_select)( int, fd_set *, fd_set *, fd_set *, struct timeval * );    
    int (*liof_connect)( int, const struct sockaddr *, int );
    int (*liof_close)( int );
} nsmail_io_fns_t;

typedef struct nsmail_inputstream
{
    void *rock;   /* opaque client data */
 
    /* Returns the number of bytes actually read and -1 on eof on the stream and 
     * may return an NSMAIL_ERR* (int value < 0) in case of any other error.
     * Can be invoked multiple times. Each read returns the next sequence of bytes
     * in the stream, starting past the last byte returned by previous read.
     * The buf space allocated and freed by the caller. The size parameter 
     * specifies the maximum number of bytes to be returned by read. If the number
     * of bytes actually returned by read is less than size, an eof on the stream 
     * is assumed.
     */
    int (*read) (void *rock, char *buf, int size);
 
   /* Reset the stream to the beginning. A subsequent read returns data from the 
      start of the stream. 
    */
    void (*rewind) (void *rock);

    /* Closes the stream, freeing any internal buffers and rock etc. 
       Once a stream is closed it is illegal to attempt read() or rewind() 
       or close() on the stream.  After close(), the nsmail_inputstream structure
       corresponding to the stream needs to be freed by the user of the stream.
    */
    void (*close) (void *rock);
 
} nsmail_inputstream_t;
 
typedef struct nsmail_outputstream
{
    void *rock;   /* opaque client data */
 
    /* write (CRLF-separated) output in 'buf', of size 'size'. May be called multiple times */
    void (*write) (void *rock, const char *buf, int size);
 
    void (*close) (void *rock);

    /* Closes the stream, freeing any internal buffers and rock etc. 
       Once a stream is closed it is illegal to attempt write() or close() 
       on the stream.  After close(), the nsmail_outputstream structure
       corresponding to the stream needs to be freed by the user of the stream.
    */

} nsmail_outputstream_t;

#endif /* NSMAIL_H */
