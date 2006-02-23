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
 * nsio.c
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "nsio.h"
#include "nssocket.h"

#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <stdio.h>

/* Internal functions not to be used directly. */
int IO_checkTimeout( IO_t * in_pIO );
void IO_resetFunctions( IO_t * in_pIO );
int UpdateBuffer( IO_t * in_pIO );

/* Initialize the IO structure. */
int IO_initialize( IO_t * in_pIO, 
                    unsigned int in_readBufferLimit, 
                    unsigned int in_writeBufferLimit )
{
    /*
	 * Parameter validation.
	 */

    if ( in_pIO == NULL || 
            in_pIO->readBuffer != NULL || 
            in_pIO->writeBuffer != NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_readBufferLimit == 0 )
    {
        in_pIO->readBufferLimit = DEFAULT_BUFFER_LENGTH;
    }
    else
    {
        in_pIO->readBufferLimit = in_readBufferLimit;
    }

    if ( in_writeBufferLimit == 0 )
    {
        in_pIO->writeBufferLimit = DEFAULT_BUFFER_LENGTH;
    }
    else
    {
        in_pIO->writeBufferLimit = in_writeBufferLimit;
    }

    /*
	 * Set the socket descriptor to an invalid value to identify a 
     * disconnected state.
	 */

    in_pIO->socket = -1;

    /*
	 * Initialize the read buffer.
	 */

    in_pIO->readBuffer = (char*)malloc( in_pIO->readBufferLimit * sizeof( char ) );
    if ( in_pIO->readBuffer == NULL )
    {
		return NSMAIL_ERR_OUTOFMEMORY;
    }
	memset(in_pIO->readBuffer, 0, in_pIO->readBufferLimit * sizeof( char ) );

    /*
	 * Initialize the write buffer.
	 */

    in_pIO->writeBuffer = (char*)malloc( in_pIO->writeBufferLimit * sizeof( char ) );
    if ( in_pIO->writeBuffer == NULL )
    {
		return NSMAIL_ERR_OUTOFMEMORY;
    }

    memset(in_pIO->writeBuffer, 0, in_pIO->writeBufferLimit * sizeof( char ) );

    /* Set the current size of the write buffer. */
    in_pIO->writeBufferSize = 0;

    /* Set the current size of the read buffer. */
    in_pIO->readBufferSize = 0;

    /* Set the IO function pointers. */
    IO_resetFunctions( in_pIO );

    /* Set the default timeout value. */
    return IO_setTimeout( in_pIO, -1 );
}

/* Free the IO structure. */
void IO_free( IO_t * in_pIO )
{
	if ( in_pIO == NULL )
	{
		return;
	}

    /*
	 * Reset the data members.
	 */

    if ( in_pIO->readBuffer != NULL )
    {
        free( in_pIO->readBuffer );
        in_pIO->readBuffer = NULL;
    }

    if ( in_pIO->writeBuffer != NULL )
    {
        free( in_pIO->writeBuffer );
        in_pIO->writeBuffer = NULL;
    }

    /*
	 * Terminate the socket session.
	 */

    NSSock_Terminate();
}

/* Connect to the specified server with the specified port. */
int IO_connect( IO_t * in_pIO, const char * in_szServer, unsigned short in_nPort )
{
    struct sockaddr_in l_hostInetAddr;
    LinkedList_t * l_addressList;
    LinkedList_t * l_tempList;

    int fHostIsIPAddr;
    int dotCount = 0;
    int i;

    /*
	 * Parameter validation.
	 */

    if ( in_pIO == NULL || in_szServer == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket > 0 )
    {
        return NSMAIL_ERR_ALREADYCONNECTED;
    }

    /*
	 * Initialize the socket session.
	 */

    NSSock_Initialize();

    /*
	 * Create a handle to the new socket.
	 */

    in_pIO->socket = in_pIO->ioFunctions.liof_socket( AF_INET, SOCK_STREAM, 0 );

    if ( in_pIO->socket < 0 )
    {
    	return NSMAIL_ERR_IO_SOCKET;
    }

    /* initialize the address structures */
    memset (&l_hostInetAddr, 0, sizeof(l_hostInetAddr));

    l_hostInetAddr.sin_family      = AF_INET;
    l_hostInetAddr.sin_port        = htons(in_nPort);

    /*
	 * Determine if a host name or IP address was specified. Assume
     * it is, then see if we can find something that means it isn't.
     * (The test below is not very careful, it should verify there
     * are numbers between the dots, and the numbers are less than 256.)
	 */
    fHostIsIPAddr = 1;
    for (i=0; i<strlen(in_szServer); ++i) {
        if (in_szServer[i] == '.') {
            ++dotCount;
        }
        else if (!isdigit(in_szServer[i])) {
            fHostIsIPAddr = 0;
            break;
        }
    }
    if (dotCount != 3) {
        fHostIsIPAddr = 0;
    }

    if ( fHostIsIPAddr )
    {
        l_hostInetAddr.sin_addr.s_addr = inet_addr( (char *)in_szServer );

        if ( in_pIO->ioFunctions.liof_connect( 
                                    in_pIO->socket, 
                                    (const struct sockaddr *)&l_hostInetAddr, 
                                    in_nPort ) != FALSE )
        {
            return NSMAIL_OK;
        }
    }
    else
    {
        l_addressList = NSSock_GetHostByName( in_szServer );
        while ( l_addressList != NULL )
        {
            if ( ( l_hostInetAddr.sin_addr.s_addr = inet_addr(l_addressList->pData) ) != 0 )
            {
                if ( in_pIO->ioFunctions.liof_connect( 
                                        in_pIO->socket, 
                                        (const struct sockaddr *)&l_hostInetAddr, 
                                        in_nPort ) != FALSE )
                {
                    while ( l_addressList != NULL )
                    {
                        free( l_addressList->pData );
                        l_tempList = l_addressList;
                        l_addressList = l_tempList->next;
                        free( l_tempList );
                    }
    	            return NSMAIL_OK;
	            }
            }
            free( l_addressList->pData );
            l_tempList = l_addressList;
            l_addressList = l_tempList->next;
            free( l_tempList );
	    }
    }

    in_pIO->socket = -1;
    return NSMAIL_ERR_IO_CONNECT;
}

/* Disconnect from the server. */
int IO_disconnect( IO_t * in_pIO )
{
    /*
	 * Parameter validation.
	 */

    if ( in_pIO == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

    /*
	 * Close the socket.	 
     */

    if ( in_pIO->ioFunctions.liof_close( in_pIO->socket ) == FALSE )
    {
        in_pIO->socket = -1;
        return NSMAIL_ERR_IO_CLOSE;
    }

    in_pIO->socket = -1;
	return NSMAIL_OK;
}

/*Send data to the server appending the \r\n and optionally flushing.*/
int IO_send( IO_t * in_pIO, const char * in_data, boolean in_bufferData )
{
    int l_nReturn;

    l_nReturn = IO_write( in_pIO, in_data, strlen(in_data) );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = IO_write( in_pIO, eoline, 2 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    if ( in_bufferData == FALSE )
    {
        l_nReturn = IO_flush( in_pIO );

        if ( l_nReturn != NSMAIL_OK )
        {
            return l_nReturn;
        }
    }

    return NSMAIL_OK;
}

/* Write operation to the buffered socket. */
int IO_write( IO_t * in_pIO, const char * in_data, unsigned int in_length )
{
    int l_nReturn;

    /* Parameter validation.*/
    if ( in_pIO == NULL || in_data == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

	/* Fill the buffer and send the data if it is full. */
    if ( in_pIO->writeBufferSize + in_length < in_pIO->writeBufferLimit )
    {
        memcpy( &(in_pIO->writeBuffer[in_pIO->writeBufferSize]), 
                in_data, 
                in_length );

        in_pIO->writeBufferSize += in_length;
    }
    else
    {
        l_nReturn = IO_flush( in_pIO );

        if ( l_nReturn != NSMAIL_OK )
        {
            return l_nReturn;
        }

        if ( in_length < in_pIO->writeBufferLimit )
        {
            memcpy( in_pIO->writeBuffer, in_data, in_length );
            in_pIO->writeBufferSize = in_length;
        }
        else
        {
            if ( in_pIO->ioFunctions.liof_write( in_pIO->socket,
                                                    (char*)in_data, 
                                                    in_length ) == FALSE )
            {
                return NSMAIL_ERR_IO_WRITE;
            }
        }
    }

    return NSMAIL_OK;
}

int IO_writeByte( IO_t * in_pIO, const char in_byte )
{
    int l_nReturn;

    /* Parameter validation */
    if ( in_pIO == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

    /*
	 * Fill the buffer and send the data if it is full.
	 */

    if ( in_pIO->writeBufferSize + 1 < in_pIO->writeBufferLimit )
    {
        in_pIO->writeBuffer[(in_pIO->writeBufferSize)++] = in_byte;
    }
    else
    {
        l_nReturn = IO_flush( in_pIO );

        if ( l_nReturn != NSMAIL_OK )
        {
            return l_nReturn;
        }

        in_pIO->writeBuffer[(in_pIO->writeBufferSize)++] = in_byte;
    }

    return NSMAIL_OK;
}

int IO_flush( IO_t * in_pIO )
{
	/* Parameter validation */
    if ( in_pIO == NULL || in_pIO->writeBuffer == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

    /* Send the data and verify that it was sent ok. */
    if ( in_pIO->writeBufferSize > 0 )
    {
        if ( in_pIO->ioFunctions.liof_write( in_pIO->socket,
                                                (char*)in_pIO->writeBuffer, 
                                                in_pIO->writeBufferSize ) == FALSE )
        {
            return NSMAIL_ERR_IO_WRITE;
        }

        memset( in_pIO->writeBuffer, 0, in_pIO->writeBufferLimit );
        in_pIO->writeBufferSize = 0;
    }

    return NSMAIL_OK;
}


/**
* Read in in_size bytes into out_response
* @param in_pIO The IO structure
* @param out_ppResponse The memory to store the data in
* @param in_size The amount of data requested
& @param out_bytesRead The total number of bytes read
*/
int IO_read( IO_t * in_pIO, char * out_response, int in_size, int * out_bytesRead )
{
    int l_returnCode;
	int l_bytesRead = 0;
	int l_bytesToRead = 0;
	int l_offset = 0;

	/* Parameter validation. */
    if ((in_pIO == NULL) || (out_response == NULL) ||
		(in_size < 1) || (out_bytesRead == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	/* Make sure the IO buffer is not empty */
    if ( in_pIO->readBufferSize == 0 )
    {
        l_returnCode = UpdateBuffer( in_pIO );

        if ( l_returnCode != NSMAIL_OK )
        {
            return l_returnCode;
        }
    }


	/* Read in_size bytes */
    while ( l_bytesRead < in_size )
	{
		/* Determine amount of data to read in from the IO buffer */
		if( (in_pIO->readBufferSize + l_bytesRead) <= in_size)
		{
			l_bytesToRead = in_pIO->readBufferSize;
		}
		else
		{
			l_bytesToRead = in_size - l_bytesRead;
		}

		memcpy(out_response + l_bytesRead, in_pIO->readBuffer, l_bytesToRead);
		in_pIO->readBufferSize -= l_bytesToRead;
		l_bytesRead += l_bytesToRead;
		l_offset += l_bytesToRead;

		/* Make sure the IO buffer is not empty */
		if ( in_pIO->readBufferSize == 0 )
		{
			l_returnCode = UpdateBuffer( in_pIO );
			if ( l_returnCode != NSMAIL_OK )
			{
				return l_returnCode;
			}
			l_offset = 0;
		}
	}
	
	/* Move the rest of the buffer to the beginning of the buffer. */
    memcpy( in_pIO->readBuffer, (&in_pIO->readBuffer[l_offset]), in_pIO->readBufferSize );
	(*out_bytesRead) = l_bytesRead;

    return NSMAIL_OK;;
}

/* Reads a line from the socket. */
int IO_readLine( IO_t * in_pIO, char * out_response, int in_maxLength )
{
    int l_returnCode;
    unsigned int offset = 0;

	/* Parameter validation. */
    if ( in_pIO == NULL || out_response == NULL || out_response == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

    if ( in_pIO->readBufferSize == 0 )
    {
        l_returnCode = UpdateBuffer( in_pIO );

        if ( l_returnCode != NSMAIL_OK )
        {
            return l_returnCode;
        }
    }

	/* Read byte by byte looking for the newline character. */
    while ( in_pIO->readBuffer[offset] != '\n' )
	{
        offset++;

        if ( offset == in_pIO->readBufferSize )
        {
            if ( offset == in_pIO->readBufferLimit )
            {
                return NSMAIL_ERR_UNEXPECTED;
            }

            l_returnCode = UpdateBuffer( in_pIO );

            if ( l_returnCode != NSMAIL_OK )
            {
                return l_returnCode;
            }
        }
	}

    if ( offset > in_maxLength )
    {
        return NSMAIL_ERR_UNEXPECTED;
    }

	/* Copy the line into the buffer. */
    memcpy( out_response, in_pIO->readBuffer, offset + 1 );
    in_pIO->readBufferSize -= (offset + 1);

	/* Move the rest of the buffer to the beginning of the buffer. */
    memcpy( in_pIO->readBuffer, 
            (&in_pIO->readBuffer[offset+1]), 
            in_pIO->readBufferSize );

    /*
	 * Look for the carriage return character.
     * If it is there, place the null character there,
     * else put the null character in the location of the linefeed character.
	 */
    if ( out_response[offset-1] == '\r' )
	{
		out_response[offset-1] = '\0';
	}
	else
	{
		out_response[offset] = '\0';
	}

    return NSMAIL_OK;;
}

/* Reads a line from the socket and dynamically allocates the necessary space for the response */
int IO_readDLine( IO_t * in_pIO, char** out_ppResponse )
{
    int l_returnCode;
    unsigned int offset = 0;
	char* l_tempBuffer = NULL;
	int l_bufferlength = 0;

	/* Parameter validation. */
    if ( in_pIO == NULL || (*out_ppResponse) != NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    if ( in_pIO->socket < 0 )
    {
        return NSMAIL_ERR_NOTCONNECTED;
    }

    /* Make sure the IO buffer is not empty */
    while( in_pIO->readBufferSize == 0 )
    {
        l_returnCode = UpdateBuffer( in_pIO );
        if ( l_returnCode != NSMAIL_OK )
        {
            return l_returnCode;
        }
    }

	/* Read byte by byte looking for the newline character. */
    while(in_pIO->readBuffer[offset] != '\n')
	{
        offset++;
        if(offset >= in_pIO->readBufferSize)
        {
            if(offset == in_pIO->readBufferLimit)
            {
				/*Grow the size of the read buffer*/
				l_bufferlength = in_pIO->readBufferLimit + DEFAULT_BUFFER_LENGTH;
				l_tempBuffer = in_pIO->readBuffer;
				in_pIO->readBuffer = (char*)malloc(l_bufferlength*sizeof(char));
				if(in_pIO->readBuffer == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(in_pIO->readBuffer, 0, l_bufferlength*sizeof(char));
				in_pIO->readBufferLimit = l_bufferlength;
				memcpy( in_pIO->readBuffer, 
						l_tempBuffer, 
						in_pIO->readBufferSize );
				free(l_tempBuffer);
				l_tempBuffer = NULL;
			}

			while(offset >= in_pIO->readBufferSize)
			{
				/* Put more data in the buffer. */
				l_returnCode = UpdateBuffer( in_pIO );
				if ( l_returnCode != NSMAIL_OK )
				{
					return l_returnCode;
				}
			}
        }
	}

	(*out_ppResponse) = (char*)malloc((offset + 2)*sizeof(char));
	if((*out_ppResponse) == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset((*out_ppResponse), 0, (offset + 2)*sizeof(char));
	memcpy((*out_ppResponse), in_pIO->readBuffer, offset + 1);
	
	in_pIO->readBufferSize -= (offset + 1);

	/* Move the rest of the buffer to the beginning of the buffer. */
    memcpy( in_pIO->readBuffer, 
            (&in_pIO->readBuffer[offset + 1]), 
            in_pIO->readBufferSize );
	memset((&in_pIO->readBuffer[in_pIO->readBufferSize]), 0, in_pIO->readBufferLimit - in_pIO->readBufferSize);

    return NSMAIL_OK;
}

int IO_freeDLine( char ** out_ppResponse )
{
    if ( out_ppResponse == NULL || (*out_ppResponse) == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    free( (*out_ppResponse) );
    (*out_ppResponse) = NULL;

    return NSMAIL_OK;
}

/* Updates the read buffer. */
int UpdateBuffer( IO_t * in_pIO )
{
    int l_nByteCount;
    int l_nSelectResponse;

    /* Determine if there is any data to be read in. */
    l_nSelectResponse = IO_checkTimeout( in_pIO );

    if ( l_nSelectResponse == 0 )
    {
        return NSMAIL_ERR_TIMEOUT;
    }
    
    /* Determine if there was an error. */
    if ( l_nSelectResponse < 0 )
    {
        return NSMAIL_ERR_IO_SELECT;
    }

    /* Read in as much data as possible up to the max. size from the socket. */
    l_nByteCount = in_pIO->ioFunctions.liof_read( 
                            in_pIO->socket,
                            in_pIO->readBuffer + in_pIO->readBufferSize,
                            in_pIO->readBufferLimit - in_pIO->readBufferSize );

    if ( l_nByteCount <= 0 )
    {
        return NSMAIL_ERR_IO_READ;
    }

    /* Set the new size of the read buffer. */
    in_pIO->readBufferSize += l_nByteCount;

    return NSMAIL_OK;
}

/* Set the timeout value. */
int IO_setTimeout( IO_t * in_pIO, double in_timeout )
{
	if ( in_pIO == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	/* represents infinite blocking, not timeout */
    if ( in_timeout == -1 )
    {
        in_pIO->timeout.tv_sec = (long)in_timeout;
        return NSMAIL_OK;
    }

	if (in_timeout < 0.0)   in_timeout = 0.0;
	if (in_timeout > 1.0e8) in_timeout = 1.0e8;

    in_pIO->timeout.tv_sec = (long) in_timeout;
	in_pIO->timeout.tv_usec = (long) (1.0e6 * (in_timeout-(double)(long)in_timeout));

	if (in_pIO->timeout.tv_usec >= 1000000)
	{
		in_pIO->timeout.tv_sec++;
		in_pIO->timeout.tv_usec = 0;
	}

    return NSMAIL_OK;
}

/* Check for a timeout using select() */
int IO_checkTimeout( IO_t * in_pIO )
{
    fd_set * l_fdSet;
    int l_nSelectResponse;
    int l_nSock = in_pIO->socket + 1;

    /*
	 * Create the fd_set structure to be used in select().
     */

    l_fdSet = (fd_set *)malloc( sizeof(fd_set) );

    FD_ZERO( l_fdSet );
    FD_SET( in_pIO->socket, l_fdSet );

    /*
	 * Determine if the timeout value is infinite or not.
     */

    if ( in_pIO->timeout.tv_sec == -1 )
    {
        l_nSelectResponse = in_pIO->ioFunctions.liof_select( l_nSock, l_fdSet, NULL, NULL, NULL );
    }
    else
    {
        l_nSelectResponse = in_pIO->ioFunctions.liof_select( l_nSock, l_fdSet, NULL, NULL, &(in_pIO->timeout) );
    }

    /*
	 * Free the fd_set structure.
     */

    free( l_fdSet );

    /*
	 * Return the result.
     */
    return l_nSelectResponse;
}


/* Reset the IO functions. */
void IO_resetFunctions( IO_t * in_pIO )
{
    in_pIO->ioFunctions.liof_read = NSSock_Read;
    in_pIO->ioFunctions.liof_write = NSSock_Write;
    in_pIO->ioFunctions.liof_socket = NSSock_Socket;
    in_pIO->ioFunctions.liof_select = NSSock_Select;
    in_pIO->ioFunctions.liof_connect = NSSock_Connect;
    in_pIO->ioFunctions.liof_close = NSSock_Close;
}

/* Get a structure to the IO functions. */
void IO_getFunctions( IO_t * in_pIO, nsmail_io_fns_t * in_pIOFunctions )
{
    in_pIOFunctions->liof_read = in_pIO->ioFunctions.liof_read;
    in_pIOFunctions->liof_write = in_pIO->ioFunctions.liof_write;
    in_pIOFunctions->liof_socket = in_pIO->ioFunctions.liof_socket;
    in_pIOFunctions->liof_select = in_pIO->ioFunctions.liof_select;
    in_pIOFunctions->liof_connect = in_pIO->ioFunctions.liof_connect;
    in_pIOFunctions->liof_close = in_pIO->ioFunctions.liof_close;
}

/* Set the IO functions. */
void IO_setFunctions( IO_t * in_pIO, nsmail_io_fns_t * in_pIOFunctions )
{
    in_pIO->ioFunctions.liof_read = in_pIOFunctions->liof_read;
    in_pIO->ioFunctions.liof_write = in_pIOFunctions->liof_write;
    in_pIO->ioFunctions.liof_socket = in_pIOFunctions->liof_socket;
    in_pIO->ioFunctions.liof_select = in_pIOFunctions->liof_select;
    in_pIO->ioFunctions.liof_connect = in_pIOFunctions->liof_connect;
    in_pIO->ioFunctions.liof_close = in_pIOFunctions->liof_close;
}
