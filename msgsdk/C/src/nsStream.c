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
 * nsStream.c
 * prasad@netscape.com, Jan 1998.
 */

#include "nsStream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <memory.h>

#define MAX_BUFSIZE    1024


/* Internal structures */

typedef struct buf_inputStream_tracker
{
        char * pDataBuf;                        /* data buffer */
        long total_size;                        /* totoal buffer size */
        long bytes_read;                        /* number of bytes read */
        char * pNext;
 
} buf_inputStream_tracker_t;
 
 
typedef struct  file_inputStream_tracker
{
     FILE * fp;
     char * file_name;
     long bytes_read;
 
} file_inputStream_tracker_t;
 
typedef struct  file_outputStream_tracker
{
     FILE * fp;
     char * file_name;
     long bytes_written;
 
} file_outputStream_tracker_t;


/* ========================= STREAM UTILITIES =============================*/


/*
* Prasad, jan 8,98 
* Reader function for a buffer based inputstream
* Note buf space is allocated/freed by the caller.
*
* parameter : 
*
* return :
*/
int buf_inputStream_read (void * rock, char *buf, int size)
{
	buf_inputStream_tracker_t * pStream;
	int bytes_to_read = 0, bytes_left;

	if (rock == NULL || buf == NULL || size <= 0)
	{
	    return NSMAIL_ERR_INVALIDPARAM;
	}

	pStream = (buf_inputStream_tracker_t *) rock;

	if (pStream->pDataBuf == NULL)
	{
	    return NSMAIL_ERR_INVALIDPARAM;
	}
	
	bytes_left = pStream->total_size - pStream->bytes_read;

	if (bytes_left <= 0)
	{
	     return NSMAIL_ERR_EOF;
	}

	if (bytes_left >= size)
		bytes_to_read = size;
	else
		bytes_to_read = bytes_left;

	memcpy( buf, pStream->pNext, bytes_to_read );

	if (bytes_to_read < size)
	     buf [bytes_to_read] = NULL; /* NULL terminate it */
	
	if (bytes_to_read == size)
	{
	    /* we still have data to read on the stream */
	    pStream->bytes_read += bytes_to_read;

	    if (pStream->bytes_read < pStream->total_size)
	          pStream->pNext += bytes_to_read;
	    else
	         pStream->pNext = pStream->pDataBuf + pStream->total_size -1; /* on last byte */
	}
	else
	{
	    /* We returned less than what was asked. So we are at EOF */
	    pStream->bytes_read = pStream->total_size;
	    pStream->pNext = pStream->pDataBuf + pStream->total_size -1; /* on last byte */
	}

	return bytes_to_read;
}



/*
* Prasad, jan 8,98 
* Rewind function for a buffer based inputstream
*
* parameter : rock
*
* return :	nothing
*/
void buf_inputStream_rewind (void * rock)
{
	buf_inputStream_tracker_t * pStream;

	if (rock == NULL)
	     return;
	
	pStream = (buf_inputStream_tracker_t *) rock;
	pStream->bytes_read = 0;
	pStream->pNext =  pStream->pDataBuf;
}



/*
* Prasad, jan 8,98 
* Close function for a buffer based inputstream
* The client must free the stream after return from close.
*
* parameter : rock
*
* return :	nothing
*/
void buf_inputStream_close (void * rock)
{
	buf_inputStream_tracker_t * pStream;

	if (rock == NULL)
	{
	    return;
	}
	
	pStream = (buf_inputStream_tracker_t *) rock;
	free (pStream->pDataBuf);
	pStream->pDataBuf = NULL;
	pStream->pNext = NULL;
	pStream->total_size = 0;
	pStream->bytes_read = 0;

	free (rock);
	/* rock = NULL; */
}



/*
* Prasad, jan 8,98 
* Makes and returns an input stream out of the databuffer Passed.
* Uses the passed buffer as is. That is, data is not copied. 
* data_size must be identical to actual size of data in pDataBuf.
*
* parameter :
*
* return :
*/
int buf_inputStream_create (char * pDataBuf, long data_size, nsmail_inputstream_t ** ppRetInputStream)
{
	nsmail_inputstream_t * pNewStream;
	buf_inputStream_tracker_t * pTracker;

	if (pDataBuf == NULL || data_size <= 0)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	pNewStream = (nsmail_inputstream_t *) malloc (sizeof (nsmail_inputstream_t));

	if (pNewStream == NULL)
	{
	    	return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker = (buf_inputStream_tracker_t *) malloc (sizeof (buf_inputStream_tracker_t));
	
	if (pTracker == NULL)
	{
	    	return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker->pDataBuf   = pDataBuf;	
	pTracker->pNext      = pDataBuf;
	pTracker->total_size = data_size;
	pTracker->bytes_read = 0;

	pNewStream->rock = (void *) pTracker;

	pNewStream->read   = buf_inputStream_read;
	pNewStream->rewind = buf_inputStream_rewind;
	pNewStream->close  = buf_inputStream_close;

	*ppRetInputStream = pNewStream;

	return NSMAIL_OK;
}

/*
* Prasad, jan 8,98 
* Reader function for a file based inputstream
* Note: buf space is allocated/freed by the caller.
*
* parameter : 
*
* return :
*/
int file_inputStream_read (void * rock, char *buf, int size)
{
	file_inputStream_tracker_t  * pStream;
	int read_len = 0;
 
    	if (rock == NULL)
	{
       		 return -1;
	}

	if (buf == NULL)
		NSMAIL_ERR_INVALIDPARAM;

    	pStream = (file_inputStream_tracker_t *) rock;

    	if (pStream->fp == NULL)
    	{
         	return -1;
    	}

	read_len = fread (buf, sizeof (char), size, pStream->fp);
	if (read_len < size)
	   buf [read_len] = NULL; /* NULL terminate it */

	if (read_len > 0)
	    (pStream->bytes_read += read_len);
	else
	     read_len = -1;

	return read_len;
}



/*
* Prasad, jan 8,98 
* Rewind function for a file based inputstream
*
* parameter : rock
*
* return :	nothing
*/
void file_inputStream_rewind (void * rock)
{
	file_inputStream_tracker_t  * pStream;
 
    	if (rock == NULL)
	{
        	return;
	}

    	pStream = (file_inputStream_tracker_t *) rock;

    	if (pStream->fp == NULL)
    	{
        	return;
    	}

    	rewind (pStream->fp);

    	pStream->bytes_read = 0;
}



/*
* Prasad, jan 8,98 
* Close function for a file based inputstream
* The client must free the stream after return from close.
*
* parameter : rock
*
* return :	nothing
*/
void file_inputStream_close (void * rock)
{
	file_inputStream_tracker_t  * pStream;

	if (rock == NULL)
	{
        	return;
	}

	pStream = (file_inputStream_tracker_t *) rock;

	if (pStream->fp == NULL)
	{
	     free (rock);
	     return;
	}

	fclose (pStream->fp);

	free (pStream->file_name);
	free (rock);
	/* rock = NULL; */
}



/*
* Prasad, jan 8,98 
* Makes and returns an input stream out of the fileName Passed.
* NOTE: fileName must be that of an existing file since this is an inputStream.
*
* parameter :
*
* return :
*/
int file_inputStream_create (char * fileName, nsmail_inputstream_t ** ppRetInputStream)
{
	FILE * fp;
	nsmail_inputstream_t * pNewStream;
	file_inputStream_tracker_t * pTracker;

    	if (fileName == NULL || ppRetInputStream == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
													
	if ((fp = fopen(fileName, "rb")) == (FILE *) NULL)
	{
		return NSMAIL_ERR_CANTOPENFILE;
	}

	 pNewStream = (nsmail_inputstream_t *) malloc (sizeof (nsmail_inputstream_t));

	if (pNewStream == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker = (file_inputStream_tracker_t *) malloc (sizeof (file_inputStream_tracker_t));
 
	if (pTracker == NULL)
	{
        	return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker->file_name = (char *) strdup (fileName);
	pTracker->fp = fp;
	pTracker->bytes_read = 0;

	pNewStream->rock = (void *) pTracker;
	pNewStream->read   = file_inputStream_read;
    	pNewStream->rewind = file_inputStream_rewind;
    	pNewStream->close  = file_inputStream_close;

    	*ppRetInputStream = pNewStream;

	return 0;
}



/*
* Prasad, jan 8,98 
* Returns the size of data that is available on the inputstream. -1 on an error.
*
* parameter : 
*
* return :
*/
static long get_inputstream_size (nsmail_inputstream_t * pTheStream)
{
	static char * pLocalBuf; 	/* allocated only once */
	long stream_size = 0;
	long read_size = 0;

	if (pLocalBuf == NULL)
	     pLocalBuf = (char *) malloc (MAX_BUFSIZE+1); /* allocated only once */

	if (pLocalBuf == NULL)
	{
	    	return NSMAIL_ERR_OUTOFMEMORY;
	}

	while (TRUE)
	{
		read_size = pTheStream->read (pTheStream->rock, pLocalBuf, MAX_BUFSIZE);

		if (read_size > 0)
		{
		     stream_size += read_size;
		     read_size = 0;
		}
		else
		      break;
        } /* while */

	if (stream_size > 0)
	{
		pTheStream->rewind (pTheStream->rock);
		return (stream_size);
	}
	else
  	      return (-1);
} /* get_inputstream_size */


void file_outputStream_write (void * rock, const char *buf, int size)
{
	file_outputStream_tracker_t  * pStream;
	int write_len = 0;
 
    	if (rock == NULL)
	{
       		return;
	}

    	pStream = (file_outputStream_tracker_t *) rock;

    	if (pStream->fp == NULL)
    	{
         	return;
    	}

	write_len = fwrite (buf, sizeof (char), size, pStream->fp);
		
	if (write_len > 0)
	    (pStream->bytes_written += write_len);

	return;
}


void file_outputStream_close (void * rock)
{
	file_outputStream_tracker_t  * pStream;

	if (rock == NULL)
	{
        	return;
	}

	pStream = (file_outputStream_tracker_t *) rock;

	if (pStream->fp == NULL)
	{
	     free (rock);
	     return;
	}

	fclose (pStream->fp);

	free (pStream->file_name);
	free (rock);
	/* rock = NULL; */
}


int file_outputStream_create (char * fileName, nsmail_outputstream_t ** ppRetOutputStream)
{
	FILE * fp;
	nsmail_outputstream_t * pNewStream;
	file_outputStream_tracker_t * pTracker;

    	if (fileName == NULL || ppRetOutputStream == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
													
	if ((fp = fopen(fileName, "wb")) == (FILE *) NULL)
	{
		return NSMAIL_ERR_CANTOPENFILE;
	}

	 pNewStream = (nsmail_outputstream_t *) malloc (sizeof (nsmail_outputstream_t));

	if (pNewStream == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker = (file_outputStream_tracker_t *) malloc (sizeof (file_outputStream_tracker_t));
 
	if (pTracker == NULL)
	{
        	return NSMAIL_ERR_OUTOFMEMORY;
	}

	pTracker->file_name = (char *) strdup (fileName);
	pTracker->fp = fp;
	pTracker->bytes_written = 0;

	pNewStream->rock = (void *) pTracker;
	pNewStream->write   = file_outputStream_write;
    	/* pNewStream->rewind = file_inputStream_rewind; */
    	pNewStream->close  = file_outputStream_close; 

    	*ppRetOutputStream = pNewStream;

	return 0;
}

void nsStream_free (void * pMem)
{
	free (pMem);
}
