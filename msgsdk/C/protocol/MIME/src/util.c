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
* util.c
*
* Prasad, Nov 1997
* clee,   Oct 1997
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <memory.h>
#include <time.h>
#include "vector.h"
#include "util.h"

#include "mime.h"
#include "mime_internal.h"
#include "mimeparser.h"


static errorLog_t *g_pLog;		/* default error log file */

#define CR 	'\r'
#define LF 	'\n'


/* base64 translation table */
struct 
{
	unsigned char	digits[66];
	unsigned char	xlate[256];
	unsigned char	xlate_table_made;
} base64 = 
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=",
	"",
	0
};




/* ------------------- utility functions ------------------ */
#ifdef HPUX
#define GETTIMEOFDAY(x) gettimeofday(x,(void *)NULL)
#elif defined(AIX)
#define GETTIMEOFDAY(x) gettimeofday(x,(void *)NULL)
#else
#define GETTIMEOFDAY(x) gettimeofday(x)
#endif



/*
* carsonl, jan 8,98 
* determine if both strings are the same based on the length of the second string
* case insensitive
*
* parameter :
*
*	s1 : first string
*	s2 : second string
*
* returns : TRUE if equals
*/
int bStringEquals( char *s1, char *s2 )
{
	static char achBuffer[64];
	int len;

	if ( s1 != NULL && s2 != NULL )
	{
		len = strlen( s2 );
		strncpy( achBuffer, s1, len );
		achBuffer[len] = 0;

#ifdef XP_UNIX
	if (strcasecmp (achBuffer, s2) == 0)
		return TRUE;
#else
	    if ( stricmp( achBuffer, s2 ) == 0 )
			return TRUE;
#endif
	}

	return FALSE;
}

/* If s1 equals s2, returns TRUE, FALSE otherwise. */
BOOLEAN equalsIgnoreCase (char * s1, char * s2)
{
#ifdef XP_UNIX
	if (strcasecmp (s1, s2) == 0)
		return TRUE;
	else
		return FALSE;
#else
	if (stricmp (s1, s2 ) == 0)
                return TRUE;
	else
		return FALSE;
#endif

}


/*
* carsonl, jan 8,98 
* right trim
*
* parameter :
*
*	s : string to be trimmed
*
* returns : string
*/
char *szRTrim( char *s )
{
	int i;

	if ( s != NULL )
	{
		for ( i = strlen(s); i > 0; i-- )
		{
			if ( isspace( s[i-1] ) )
				s[i-1] = 0;
			else
				return s;
		}
	}

	return s;
}



/*
* carsonl, jan 8,98 
* left trim
*
* parameter :
*
*	s : string to be trimmed
*
* returns : string
*/
char *szLTrim( char *s )
{
	char *ss = NULL;
	int i;
	int len;

	if ( s != NULL )
	{
		ss = s;
		len = strlen( s );

		for ( i = 0; i < len; i++, ss++ )
		{
			if ( !isspace( s[i] ) )
				return ss;
		}
	}

	return ss;
}




/*
* carsonl, jan 8,98 
* trim both sides of the string
*
* parameter :
*
*	s : string to be trimmed
*
* returns : string
*/
char *szTrim( char *s ) { return szLTrim( szRTrim( s ) ); }




/*
* carsonl, jan 8,98 
* return a new copy of a string passed in
*
* parameter :
*
*	s : string to be cloned
*
* returns : cloned string
*/
char *szStringClone( char *s )
{
	char *s2 = NULL;
	int len;

	if ( s != NULL )
	{
		len = strlen( s );

		if ( len  > 0 )
		{
			s2 = (char *) malloc( len + 1 );

			if ( s2 != NULL )
				strcpy( s2, s );
		}
	}

	return s2;
}



/*
* carsonl, jan 8,98 
* determine if content type is a basicpart
*
* parameter :
*
*	int nType : content type
*
* returns : TRUE if basicpart
*/
BOOLEAN bIsBasicPart( int nType )
{
	if (nType == MIME_CONTENT_TEXT ||
		nType == MIME_CONTENT_AUDIO ||
		nType == MIME_CONTENT_IMAGE ||
		nType == MIME_CONTENT_VIDEO ||
		nType == MIME_CONTENT_APPLICATION )
		return TRUE;

	return FALSE;
}



/* --------------------------------------------- */



/*
* carsonl, jan 8,98 
* Decodes a string encoded in RFC2047 format. If the string is not encoded
* returns the original string. Can be used on the header value returned by
* getHeader() and getAllHeaders() methods in MIMEMessage class.
*
* parameter :
*
*	szInputString : string to be trimmed
*
* return : The decoded header string.
*/
char *decodeHeader( char *szInputString )
{
	int len, retLen = 0;
	int i, j, outBytes, outBits;
	int nStart = 0;
	int nEnd = 0;
	char *szEncodedText = NULL;
	char *szDecodedText = NULL;
	char *plainPart = NULL, *retBuf=NULL;
	char *leftOverBuf=NULL, *leftOverDecodedBuf=NULL;
	int  plainTextLen = 0, leftOverLen=0;
	char achCharset[16];
	BOOLEAN bEncodeBase64 = FALSE;
	BOOLEAN bEncodeQP = FALSE;
	BOOLEAN bEncodeString = FALSE;

	if ( szInputString == NULL )
		return NULL;

    len = strlen( szInputString );
    
    /* find start of encoding text */
    for ( i = 0; i < len && szInputString[i] != 0 ; i++ )
    {
    	if ( szInputString[i] == '=' && szInputString[i+1] == '?' )
	{
		bEncodeString = TRUE;
    		break;
	}
    }

    if (bEncodeString == FALSE)
	return (strdup (szInputString));

    if (i > 0)
    {
	plainTextLen = (i);
	plainPart = malloc (i+1);
	strncpy (plainPart, szInputString, plainTextLen);
	plainPart [i] = 0;
    }

    /* get charset */
    for ( i += 2, j = 0; i < len && szInputString[i] != '?' && j < 15; i++, j++ )
    {
    	achCharset[j] = szInputString[i];
    }

    achCharset[j] = 0;
    i++;	/* skip ? */

    /* get encoding type */
    if ( szInputString[i] == 'Q' || szInputString[i] == 'q' )
    	bEncodeQP = TRUE;

    else if ( szInputString[i] == 'B' || szInputString[i] == 'b' )
    	bEncodeBase64 = TRUE;

    if (szInputString[i] == '?' )
    	i++;	/* skip ? */
    nStart = ++i;

    /* look for end of encoded text */
    for ( j = 0; i < len && szInputString[i] != 0; i++, j++ )
    {
    	if ( szInputString[i] == '?' && (i+1) < len && szInputString[i+1] == '=' )
    	{
    		nEnd = i;
    		break;
    	}
    }

    if ( nEnd > 0 )
    {
    	/* extract encoded text */
		szEncodedText = (char *) malloc( j + 1 );
		szDecodedText = (char *) malloc( j + 1 );

		if ( szEncodedText != NULL && szDecodedText != NULL )
		{
    		strncpy( szEncodedText, &szInputString[nStart], j );

			if ( bEncodeQP )
				decodeQP( szEncodedText, szDecodedText, j );

			else if ( bEncodeBase64 )
				decodeBase64( szEncodedText, szDecodedText, j, j, &outBytes, &outBits );

			free( szEncodedText );
		}
    }

    if (nEnd+2 < len-1)
    {
	leftOverLen = len - (nEnd+2);
	leftOverBuf = malloc (leftOverLen + 1);	
    	strncpy (leftOverBuf, &szInputString[nEnd+2], leftOverLen);
	leftOverBuf [leftOverLen] = 0;
	leftOverDecodedBuf = decodeHeader (leftOverBuf);
    }

    retLen = plainTextLen + strlen (szDecodedText) + strlen (leftOverDecodedBuf);
    retBuf = malloc (retLen +1);

    if (plainPart != NULL && szDecodedText != NULL && leftOverDecodedBuf != NULL)
    {
	sprintf (retBuf, "%s%s%s", plainPart, szDecodedText, leftOverDecodedBuf);
	free (plainPart);
	free (szDecodedText);
	free (leftOverBuf);
	free (leftOverDecodedBuf);
    }
    else if (plainPart != NULL && szDecodedText != NULL)
    {
	sprintf (retBuf, "%s%s", plainPart, szDecodedText);
	free (plainPart);
	free (szDecodedText);
    }
    else if (szDecodedText != NULL && leftOverDecodedBuf != NULL)
    {
	sprintf (retBuf, "%s%s", szDecodedText, leftOverDecodedBuf);
	free (szDecodedText);
	free (leftOverBuf);
	free (leftOverDecodedBuf);
    }
    else if (szDecodedText != NULL)
    {
	free (retBuf);
	return szDecodedText;
    }
    else
    {
	free (retBuf);
	return null;
    }

    return retBuf;
    
}




/*
* Prasad
* Generates and returns a boundary string that can be used in multi-parts etc.
*
* @return The boundary string.
*/
char * generateBoundary()
{
 	int randval1, randval2;
        struct timeval tv;
        char ca[100];
        char * pca;
	int ct;
	static int prev_randval1=0, prev_randval2=0, counter = 0;
	
 
#ifdef WIN32
	time_t ltime;
	struct tm *ptm;
	time( &ltime );
	ptm = localtime( &ltime );
	ct = ptm->tm_sec;
#else
	/*gettimeofday (&tv);*/
	GETTIMEOFDAY(&tv);
	ct = tv.tv_sec;
#endif

    	/*srand ((unsigned int)ct);*/
    	srand ((unsigned int)clock());
    	randval1 = rand();
    	randval2 = rand();
	/*if (randval1 == prev_randval1 || randval2 == prev_randval2)*/
	{
		counter++;
		randval1 += counter * 200;
		randval2 += counter * 200;
	}

	prev_randval1 = randval1;
	prev_randval2 = randval2;

    	pca = ca;
    	sprintf (pca, "-------");
    	pca += 7;
    	sprintf (pca, "%x%x%x%x", randval1, randval2, randval1, randval2);

    	return (strdup (ca));
}




/*
* carsonl, jan 8,98 
* hex to decimal
*
* parameter :
*
*	int nFirstByte : first hex byte
*	int nSecondByte : second hex byte
*
* returns : decimal
*/
int nConvertHexToDec( int nFirstByte, int nSecondByte )
{
	int nValue = 0;

	switch ( nFirstByte )
	{
		case ' ':
		case '0':	nValue += 0; break;
		case '1':	nValue += 16; break;
		case '2':	nValue += 32; break;
		case '3':	nValue += 48; break;
		case '4':	nValue += 64; break;
		case '5':	nValue += 80; break;
		case '6':	nValue += 96; break;
		case '7':	nValue += 112; break;
		case '8':	nValue += 128; break;
		case '9':	nValue += 144; break;
		case 'A':	
		case 'a':	nValue += 160; break;
		case 'B':	
		case 'b':	nValue += 176; break;
		case 'C':	
		case 'c':	nValue += 192; break;
		case 'D':	
		case 'd':	nValue += 208; break;
		case 'E':	
		case 'e':	nValue += 224; break;
		case 'F':	
		case 'f':	nValue += 240; break;
	}

	switch ( nSecondByte )
	{
		case ' ':
		case '0':	nValue += 0; break;
		case '1':	nValue += 1; break;
		case '2':	nValue += 2; break;
		case '3':	nValue += 3; break;
		case '4':	nValue += 4; break;
		case '5':	nValue += 5; break;
		case '6':	nValue += 6; break;
		case '7':	nValue += 7; break;
		case '8':	nValue += 8; break;
		case '9':	nValue += 9; break;
		case 'A':	
		case 'a':	nValue += 10; break;
		case 'B':	
		case 'b':	nValue += 11; break;
		case 'C':	
		case 'c':	nValue += 12; break;
		case 'D':	
		case 'd':	nValue += 13; break;
		case 'E':	
		case 'e':	nValue += 14; break;
		case 'F':	
		case 'f':	nValue += 15; break;
	}

	return nValue;
}




/* ========================= STREAM UTILITIES =============================*/



/*
* Prasad, jan 8,98 
* Reader function for a buffer based mime_inputstream
* Note buf space is allocated/freed by the caller.
*
* parameter : 
*
* return :
*/
int buf_instream_read (void * rock, char *buf, int size)
{
	buf_instream_tracker_t * pStream;
	int bytes_to_read = 0, bytes_left;

	if (rock == NULL || buf == NULL || size <= 0)
	{
	    return MIME_ERR_INVALIDPARAM;
	}

	pStream = (buf_instream_tracker_t *) rock;

	if (pStream->pDataBuf == NULL)
	{
	    return MIME_ERR_INVALIDPARAM;
	}
	
	bytes_left = pStream->total_size - pStream->bytes_read;

	if (bytes_left <= 0)
	{
	    return MIME_ERR_EOF;
	}

	if (bytes_left >= size)
		bytes_to_read = size;
	else
		bytes_to_read = bytes_left;

	memcpy( buf, pStream->pNext, bytes_to_read );

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
* Rewind function for a buffer based mime_inputstream
*
* parameter : rock
*
* return :	nothing
*/
void buf_instream_rewind (void * rock)
{
	buf_instream_tracker_t * pStream;

	if (rock == NULL)
	     return;
	
	pStream = (buf_instream_tracker_t *) rock;
	pStream->bytes_read = 0;
	pStream->pNext =  pStream->pDataBuf;
}



/*
* Prasad, jan 8,98 
* Close function for a buffer based mime_inputstream
* The client must free the stream after return from close.
*
* parameter : rock
*
* return :	nothing
*/
void buf_instream_close (void * rock)
{
	buf_instream_tracker_t * pStream;

	if (rock == NULL)
	{
	    return;
	}
	
	pStream = (buf_instream_tracker_t *) rock;
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
int buf_instream_create (char * pDataBuf, long data_size, nsmail_inputstream_t ** ppRetInputStream)
{
	nsmail_inputstream_t * pNewStream;
	buf_instream_tracker_t * pTracker;

	if (pDataBuf == NULL || data_size <= 0)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pNewStream = (nsmail_inputstream_t *) malloc (sizeof (nsmail_inputstream_t));

	if (pNewStream == NULL)
	{
	    	return MIME_ERR_OUTOFMEMORY;
	}

	pTracker = (buf_instream_tracker_t *) malloc (sizeof (buf_instream_tracker_t));
	
	if (pTracker == NULL)
	{
	    	return MIME_ERR_OUTOFMEMORY;
	}

	pTracker->pDataBuf   = pDataBuf;	
	pTracker->pNext      = pDataBuf;
	pTracker->total_size = data_size;
	pTracker->bytes_read = 0;

	pNewStream->rock = (void *) pTracker;

	pNewStream->read   = buf_instream_read;
	pNewStream->rewind = buf_instream_rewind;
	pNewStream->close  = buf_instream_close;

	*ppRetInputStream = pNewStream;

	return MIME_OK;
}



/*
* Prasad, jan 8,98 
* Appends src to dest and return the length of src string.
* Assumes there is enough room in dest.
*
* parameter :
*
* return :
*/
int append_str (char * dest, char * src)
{
	int len;
	
	len = strlen (src);

	if (len > 0)
	    strncpy (dest, src, len);

	return len;
}



/*
* Prasad, jan 8,98 
* Reader function for a file based mime_inputstream
* Note: buf space is allocated/freed by the caller.
*
* parameter : 
*
* return :
*/
int file_instream_read (void * rock, char *buf, int size)
{
	file_instream_tracker_t  * pStream;
	int read_len = 0;
 
    	if (rock == NULL)
	{
       		 return -1;
	}

    	pStream = (file_instream_tracker_t *) rock;

    	if (pStream->fp == NULL)
    	{
         	return -1;
    	}

	read_len = fread (buf, sizeof (char), size, pStream->fp);
	buf [read_len] = NULL; /* NULL terminate it */

	if (read_len > 0)
	    (pStream->bytes_read += read_len);
	else
	     read_len = -1;

	return read_len;
}



/*
* Prasad, jan 8,98 
* Rewind function for a file based mime_inputstream
*
* parameter : rock
*
* return :	nothing
*/
void file_instream_rewind (void * rock)
{
	file_instream_tracker_t  * pStream;
 
    if (rock == NULL)
	{
        	return;
	}

    pStream = (file_instream_tracker_t *) rock;

    if (pStream->fp == NULL)
    {
        	return;
    }

    rewind (pStream->fp);

    pStream->bytes_read = 0;
}



/*
* Prasad, jan 8,98 
* Close function for a file based mime_inputstream
* The client must free the stream after return from close.
*
* parameter : rock
*
* return :	nothing
*/
void file_instream_close (void * rock)
{
	file_instream_tracker_t  * pStream;

	if (rock == NULL)
	{
        	return;
	}

	pStream = (file_instream_tracker_t *) rock;

	if (pStream->fp == NULL)
	{
	     free (rock);
	     return;
	}

	fclose (pStream->fp);

	free (pStream->file_name);
	free (rock);
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
int file_instream_create (char * fileName, nsmail_inputstream_t ** ppRetInputStream)
{
	FILE * fp;
	nsmail_inputstream_t * pNewStream;
	file_instream_tracker_t * pTracker;

    	if (fileName == NULL || ppRetInputStream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}
													
	if ((fp = fopen(fileName, "rb")) == (FILE *) NULL)
	{
		return MIME_ERR_CANTOPENFILE;
	}

	 pNewStream = (nsmail_inputstream_t *) malloc (sizeof (nsmail_inputstream_t));

	if (pNewStream == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	pTracker = (file_instream_tracker_t *) malloc (sizeof (file_instream_tracker_t));
 
	if (pTracker == NULL)
	{
        	return MIME_ERR_OUTOFMEMORY;
	}

	pTracker->file_name = (char *) strdup (fileName);
	pTracker->fp = fp;
	pTracker->bytes_read = 0;

	pNewStream->rock = (void *) pTracker;
	pNewStream->read   = file_instream_read;
    	pNewStream->rewind = file_instream_rewind;
    	pNewStream->close  = file_instream_close;

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
long get_inputstream_size (nsmail_inputstream_t * pTheStream)
{
	static char * pLocalBuf; 	/* allocated only once */
	long stream_size = 0;
	long read_size = 0;

	if (pLocalBuf == NULL)
	     pLocalBuf = (char *) malloc (MIME_BUFSIZE+1); /* allocated only once */

	if (pLocalBuf == NULL)
	{
	    	return MIME_ERR_OUTOFMEMORY;
	}

	while (TRUE)
	{
		read_size = pTheStream->read (pTheStream->rock, pLocalBuf, MIME_BUFSIZE);

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

char * getFileShortName (char * fileName)
{
        char * pCh;
        int i, len;

#ifdef XP_UNIX
#define SEPCHAR '/'
#else
#define SEPCHAR '\\'
#endif
 
        if (fileName != NULL)
        {
                len = strlen (fileName);
 
                if (len <= 2)
                     return NULL;
 
                pCh = fileName + len;
 
                for (i = len -1; i >=  0; i--)
                {
                        if (fileName [i] == SEPCHAR)
                        {
                           return strdup (pCh);
                        }
 
                        pCh--;
                }

		return strdup (fileName);
      
        }
 
        return NULL;
}

char * getFileExtn (char * fileName)
{
        char * pCh;
        int i, len;
 
        if (fileName != NULL)
        {
                len = strlen (fileName);
 
                if (len <= 2)
                     return NULL;
 
                pCh = fileName + len;
 
                for (i = len -1; i > (len - 6); i--)
                {
                        if (fileName [i] == '.')
                        {
                           return strdup (pCh);
                        }
 
                        pCh--;
                }
      
        }
 
        return NULL;
}

void file_outstream_write (void * rock, const char *buf, int size)
{
	file_outstream_tracker_t  * pStream;
	int write_len = 0;
 
    	if (rock == NULL)
	{
       		return;
	}

    	pStream = (file_outstream_tracker_t *) rock;

    	if (pStream->fp == NULL)
    	{
         	return;
    	}

#ifdef DEBUG
	fprintf (stderr, "%s:%d> size=%d; %s\n", __FILE__, __LINE__, size, buf);
#endif

	write_len = fwrite (buf, sizeof (char), size, pStream->fp);
		
	if (write_len > 0)
	    (pStream->bytes_written += write_len);

	return;
}





int file_outstream_create (char * fileName, nsmail_outputstream_t ** ppRetOutputStream)
{
	FILE * fp;
	nsmail_outputstream_t * pNewStream;
	file_outstream_tracker_t * pTracker;

    	if (fileName == NULL || ppRetOutputStream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}
													
	if ((fp = fopen(fileName, "wb")) == (FILE *) NULL)
	{
		return MIME_ERR_CANTOPENFILE;
	}

	 pNewStream = (nsmail_outputstream_t *) malloc (sizeof (nsmail_outputstream_t));

	if (pNewStream == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	pTracker = (file_outstream_tracker_t *) malloc (sizeof (file_outstream_tracker_t));
 
	if (pTracker == NULL)
	{
        	return MIME_ERR_OUTOFMEMORY;
	}

	pTracker->file_name = (char *) strdup (fileName);
	pTracker->fp = fp;
	pTracker->bytes_written = 0;

	pNewStream->rock = (void *) pTracker;
	pNewStream->write   = file_outstream_write;
    	/* pNewStream->rewind = file_instream_rewind; */
    	pNewStream->close  = file_outstream_close; 

    	*ppRetOutputStream = pNewStream;

	return 0;
}

void file_outstream_close (void * rock)
{
	file_outstream_tracker_t  * pStream;

	if (rock == NULL)
	{
        	return;
	}

	pStream = (file_outstream_tracker_t *) rock;

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
/* ------------------------------- decoding ---------------------------- */



/*
* carson, Jan 8,98
*
* Base64 Decodes data from input buffer and writes to Output buffer
*
* params :
*
* char *szInput : input buffer
* char *szOutput : output buffer
* int nInputBufferSize : input buffer size
* BOOLEAN bFirstBuffer : TRUE if first buffer
* BOOLEAN bLastBuffer : TRUE if last buffer
*
* return : number of decoded bytes
*/
int base64Decoder( char *szInput, char *szOutput, int nInputBufferSize, BOOLEAN bFirstBuffer, BOOLEAN bLastBuffer )
{
	int				add_bits;
	int				mask;
	int				out_byte = 0;
	int				out_bits = 0;
	int				i;
	int				byte_pos = 0;

	if ( szInput == NULL || szOutput == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* Make translation table if it doesn't exist already */
	if ( !( base64.xlate_table_made ) )
	{
		for ( i = 0; i < 256; i++ ) 
			base64.xlate[i] = 127;

		for (i = 0; base64.digits[i]; i++) 
		{
			base64.xlate[base64.digits[i]] = i;
		}

		base64.xlate_table_made = 1;
	}
		
	/* Queue up relevant bits */
	for (i = 0; szInput[i] && i < nInputBufferSize; i++ )
	{
		/* too small for us to decode */
		if ( i + 4 > nInputBufferSize )
			break;

		add_bits = base64.xlate[(unsigned char) szInput[i]];

		if (add_bits >= 64) 
			continue;

		out_byte = (out_byte << 6) + add_bits;
		out_bits += 6;

		/* If the queue has gotten big enough, put into the buffer */
		if (out_bits == 24) 
		{
			szOutput[byte_pos++] = (out_byte & 0xFF0000) >> 16;
			szOutput[byte_pos++] = (out_byte & 0x00FF00) >> 8;
			szOutput[byte_pos++] = (out_byte & 0x0000FF);
			out_bits = 0;
			out_byte = 0;
		}
	}

	/* Handle any bits still in the queue */
	while (out_bits >= 8) 
	{
		if (out_bits == 8) 
		{
			szOutput[byte_pos++] = out_byte;
			out_byte = 0;
		}
		
		else 
		{
			mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
			szOutput[byte_pos++] = (out_byte & mask) >> (out_bits - 8);
			out_byte &= ~mask;
		}

		out_bits -= 8;
	}

	/* Ignore any remaining bits */
	return out_byte ? BASE64_SOMELEFT : byte_pos;
}



/*
* carson, Jan 8,98
*
* Base64 Decodes data from input buffer and writes to Output buffer
*
* params :
*
* char *szInput : input buffer
* char *szOutput : output buffer
* int nInputBufferSize : input buffer size
* int nMaxBufferSize : max output buffer size
* int *pOut_byte : leftover byte
* int *pOut_bits : leftover bits
*
* return : number of decoded bytes
*/
int decodeBase64( char *szInput, char *szOutput, int nInputBufferSize, int nMaxBufferSize, int *pOut_byte, int *pOut_bits  )
{
	int				add_bits;
	int				mask;
	int				out_byte = 0;
	int				out_bits = 0;
	int				i;
	int				byte_pos = 0;

	if ( szInput == NULL || szOutput == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* dynamic parsing */
	if ( pOut_byte != NULL && pOut_bits != NULL )
	{
		out_byte = *pOut_byte;
		out_bits = *pOut_bits;
	}

	/* Make translation table if it doesn't exist already */
	if (!(base64.xlate_table_made)) 
	{
		for ( i = 0; i < 256; i++ )
			base64.xlate[i] = 127;

		for (i = 0; base64.digits[i]; i++) 
		{
			base64.xlate[base64.digits[i]] = i;
		}

		base64.xlate_table_made = 1;
	}
		
	/* Queue up relevant bits */
	for (i = 0; szInput[i] && i < nInputBufferSize; i++) 
	{
		add_bits = base64.xlate[(unsigned char) szInput[i]];

		if (add_bits >= 64) 
			continue;

		out_byte = (out_byte << 6) + add_bits;
		out_bits += 6;

		/* If the queue has gotten big enough, put into the buffer */
		if (out_bits == 24) 
		{
			if (byte_pos + 2 >= nMaxBufferSize ) 
				return BASE64_NOFIT;

			szOutput[byte_pos++] = (out_byte & 0xFF0000) >> 16;
			szOutput[byte_pos++] = (out_byte & 0x00FF00) >> 8;
			szOutput[byte_pos++] = (out_byte & 0x0000FF);
			out_bits = 0;
			out_byte = 0;
		}
	}

	/* Handle any bits still in the queue */
	if ( pOut_byte == NULL || pOut_bits == NULL )
	{
		while (out_bits >= 8) 
		{
			if (byte_pos >= nMaxBufferSize )
				return BASE64_NOFIT;
			
			if (out_bits == 8) 
			{
				szOutput[byte_pos++] = out_byte;
				out_byte = 0;
			}
			
			else 
			{
				mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
				szOutput[byte_pos++] = (out_byte & mask) >> (out_bits - 8);
				out_byte &= ~mask;
			}

			out_bits -= 8;
		}

		/* Ignore any remaining bits */
		return out_byte ? BASE64_SOMELEFT : byte_pos; 
	}

	/* dynamic parsing */
	else
	{
		*pOut_byte = out_byte;
		*pOut_bits = out_bits;

		return byte_pos;
	}
}



/*
* carson, Jan 8,98
*
* Base64 Decodes data from vector and writes to Output buffer
*
* params :
*
* int nStart : start element in vector
* int nEnd : end element in vector
* Vector *v : input vector
* char *szOutput : output buffer
* int nMaxBufferSize : max output buffer size
* int *pOut_byte : leftover byte
* int *pOut_bits : leftover bits
*
* return : number of decoded bytes
*/
int decodeBase64Vector( int nStart, int nEnd, Vector *v, char **szDecodedBuffer, int nRawMessageSize, int *pOut_byte, int *pOut_bits )
{
	int				add_bits;
	int				mask;
	int				out_byte = 0;
	int				out_bits = 0;
	int				byte_pos = 0;
	int				i, j, nLen;
	char			*szLine;
	char			*szOutput;

	if ( v == NULL || szDecodedBuffer == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* dynamic parsing */
	if ( pOut_byte != NULL && pOut_bits != NULL )
	{
		out_byte = *pOut_byte;
		out_bits = *pOut_bits;
	}

	/* Make translation table if it doesn't exist already */
	if (!(base64.xlate_table_made)) 
	{
		for (i = 0; i < 256; i++) 
			base64.xlate[i] = 127;

		for (i = 0; base64.digits[i]; i++) 
		{
			base64.xlate[base64.digits[i]] = i;
		}

		base64.xlate_table_made = 1;
	}

	szOutput = (char *) malloc( nRawMessageSize + 1 );

	if ( szOutput == NULL )
	{
#ifdef ERRORLOG
		errorLog( "baseBase64Vector()", MIME_ERR_OUTOFMEMORY );
#endif

		return 0;
	}

	*szDecodedBuffer = szOutput;

	for ( j = nStart; j <= nEnd; j++ )
	{
		szLine = (char *) Vector_elementAt( v, j );
		nLen = strlen( szLine );

		/* Queue up relevant bits */
		for (i = 0; szLine[i] && i < nLen; i++) 
		{
			add_bits = base64.xlate[(unsigned char) szLine[i]];

			if (add_bits >= 64) 
				continue;

			out_byte = (out_byte << 6) + add_bits;
			out_bits += 6;

			/* If the queue has gotten big enough, put into the buffer */
			if (out_bits == 24) 
			{
				if (byte_pos + 2 >= nRawMessageSize ) 
					return BASE64_NOFIT;

				szOutput[byte_pos++] = (out_byte & 0xFF0000) >> 16;
				szOutput[byte_pos++] = (out_byte & 0x00FF00) >> 8;
				szOutput[byte_pos++] = (out_byte & 0x0000FF);
				out_bits = 0;
				out_byte = 0;
			}
		}
	}

	/* Handle any bits still in the queue */
	if ( pOut_byte == NULL || pOut_bits == NULL )
	{
		while (out_bits >= 8) 
		{
			if (byte_pos >= nRawMessageSize )
				return BASE64_NOFIT;
			
			if (out_bits == 8) 
			{
				szOutput[byte_pos++] = out_byte;
				out_byte = 0;
			}
			
			else 
			{
				mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
				szOutput[byte_pos++] = (out_byte & mask) >> (out_bits - 8);
				out_byte &= ~mask;
			}

			out_bits -= 8;
		}

		/* Ignore any remaining bits */
		return out_byte ? BASE64_SOMELEFT : byte_pos;
	}

	/* dynamic parsing */
	else
	{
		*pOut_byte = out_byte;
		*pOut_bits = out_bits;

		return byte_pos;
	}
}



/*
* carson, Jan 8,98
* decode quoted printable
*
* param :
*
*	char *szInput : input buffer
*	char *szOutput : output buffer
*	int MaxBufferSize : max buffer size
*
* return : number of decoded bytes
*/
int decodeQP( char *szInput, char *szOutput, int nMaxBufferSize )
{
	char ch;
	int i = 0;
	int j = 0;

	if ( szInput == NULL || szOutput == NULL )
		return MIME_ERR_INVALIDPARAM;

	for ( ch = szInput[i]; ch != 0 && j < nMaxBufferSize; i++ )
	{
		if ( ( ch >= 33 && ch <= 60 ) || ( ch >= 62 && ch <= 126 ) || ( ch == 9 || ch == 32 ) )
		{
			szOutput[j++] = ch;
		}

		else if ( ch == '=' )
		{
			szOutput[j++] = nConvertHexToDec( szInput[++i], szInput[++i] );
		}
	}

	szOutput[j] = 0;

	return j;
}



/* New routine prasad 3/31/98 */
int decodeQPVector (int nStart, int nEnd, Vector *v, char **ppDecodedBuffer, 
		    int nRawMessageSize, char *pLeftOverBytes, int *nNoOfLeftOverBytes)
{
	char token[3], ch;
	char *pLine, *pOutput, *pInput;
	int i = 0, j, ii=0, nLen = 0, written=0;
	
	if (v == NULL || pLeftOverBytes == NULL || nNoOfLeftOverBytes == NULL || ppDecodedBuffer == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pOutput = (char *) malloc (nRawMessageSize + 1);
	if (pOutput == NULL)
		return MIME_ERR_OUTOFMEMORY;
	memset (pOutput, 0, (nRawMessageSize + 1));

	*ppDecodedBuffer = pOutput;

	/* get each line */
	for (ii = nStart; ii <= nEnd; ii++)
	{
		pLine = (char *) Vector_elementAt( v, ii );
		nLen += strlen (pLine);
		if (ii == nStart)
		{
		      if (*nNoOfLeftOverBytes > 0)
		      {
		          strncpy (pOutput, pLeftOverBytes, *nNoOfLeftOverBytes);
		          nLen += *nNoOfLeftOverBytes;
		      }
		      strcat (pOutput, pLine);
		      /* see if this helps! */
		      strcat (pOutput, "\r\n");
		      nLen += 2;
		}
		else
		{
		      strcat (pOutput, pLine);
		      /* see if this helps! */
		      strcat (pOutput, "\r\n");
		      nLen += 2;
		}
		/* Don't free() pLine. It is freed by caller as part of freeing the vector */
	}

	pInput = pOutput;
	written=0;
	i=0;

        /* first time do it outside the loop */
	while (i < 3 && nLen > 0)
	{
		token [i++] =  *pInput;
		pInput++;
                nLen--;
	}

	while (nLen > 0 || i != 0)
	{
		 while (i < 3 && nLen > 0)
		 {
			token [i++] =  *pInput;
			pInput++;
                	nLen--;
		 }
		
		 if (i < 3)    /* did not get enough for a token */
		 {
		     if (i == 2 && token [0] == CR && token [1] == LF)
		     {
			 *pOutput++ = token[0]; written++; 
			 *pOutput++ = token[1]; written++; 
		         *nNoOfLeftOverBytes = 0;
		     }
		     else
		     {
		          for (j=0; j < i; j++)
			      pLeftOverBytes [j] = token [j];;
		          *nNoOfLeftOverBytes = i;
		     }
			
		     if (written > 0)
			*pOutput++ = 0; /* null terminate it */
			
		     return (written);
		 }

		i = 0;

	  	if (token [0] == '=')
		{
		  unsigned char c = 0;
		  if (token[1] >= '0' && token[1] <= '9')
			c = token[1] - '0';
		  else if (token[1] >= 'A' && token[1] <= 'F')
			c = token[1] - ('A' - 10);
		  else if (token[1] >= 'a' && token[1] <= 'f')
			c = token[1] - ('a' - 10);
		  /*else if (token[1] == CR || token[1] == LF)*/
		  else if (token[1] == CR || token[1] == LF)
		  {
		        /* =\n means ignore the newline. */
		        if (token[1] == CR && token[2] == LF)
				;		/* swallow all three chars */
			else
			{
			     pInput--;	/* put the third char back */
			     nLen++;
			}
			continue;
		  }
		  else
		  {
		       /* = followed by something other than hex or newline -
			 pass it through unaltered, I guess.
			*/
			if (pInput > pOutput) { *pOutput++ = token[0]; written++; }
			if (pInput > pOutput) { *pOutput++ = token[1]; written++; }
			if (pInput > pOutput) { *pOutput++ = token[2]; written++; }
			continue;
		  }

		  /* Second hex digit */
		  c = (c << 4);
		  if (token[2] >= '0' && token[2] <= '9')
			c += token[2] - '0';
		  else if (token[2] >= 'A' && token[2] <= 'F')
			c += token[2] - ('A' - 10);
		  else if (token[2] >= 'a' && token[2] <= 'f')
			c += token[2] - ('a' - 10);
		  else
		  {
			/* We got =xy where "x" was hex and "y" was not, so
			   treat that as a literal "=", x, and y. */
			if (pInput > pOutput) { *pOutput++ = token[0]; written++; }
			if (pInput > pOutput) { *pOutput++ = token[1]; written++; }
			if (pInput > pOutput) { *pOutput++ = token[2]; written++; }
			continue;
		  }

		  *pOutput++ = (char) c;
		  written++;
		}
	  	else
		{
		  *pOutput++ = token[0];
		  written++;

		  token[0] = token[1];
		  token[1] = token[2];
		  i = 2;
		}
	}

        if (written > 0)
		*pOutput++ = 0; /* null terminate it */
	return written;
}



/*
* carson, Jan 8,98
* QuotedPrintable Decodes data from vector, write decoded data to szOutput
*
* params :
*
*	int nStart : starting element in vector
*	int nEndt  : ending element in vector
*	Vector *v  : vector
*	char *szOutput : output buffer
*	int MaxBufferSize : max buffer size
*
* return : number of decoded bytes
*/
int decodeQPVectorOrig( int nStart, int nEnd, Vector *v, char **szDecodedBuffer, int nRawMessageSize, char *szLeftOverBytes, int *nNoOfLeftOverBytes )
{
	char ch;
	int i = 0;
	int j = 0;
	int	ii, nLen;
	char *szLine;
	char achBuffer[96];
	boolean bAppendLeftOverBytes = FALSE;
	char *szOutput;
	
	if ( v == NULL || szLeftOverBytes == NULL || nNoOfLeftOverBytes == NULL || szDecodedBuffer == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	szOutput = (char *) malloc( nRawMessageSize + 1 );

	if ( szOutput == NULL )
	{
#ifdef ERRORLOG
		errorLog( "baseQPVector()", MIME_ERR_OUTOFMEMORY );
#endif

		return MIME_ERR_OUTOFMEMORY;
	}

	*szDecodedBuffer = szOutput;

	/* get each line */
	for ( ii = nStart; ii <= nEnd; ii++ )
	{
		szLine = (char *) Vector_elementAt( v, ii );
		nLen = strlen( szLine );

		if ( !bAppendLeftOverBytes )
		{
			if ( *nNoOfLeftOverBytes > 0 )
			{
				strncpy( achBuffer, szLeftOverBytes, *nNoOfLeftOverBytes );
				achBuffer[ *nNoOfLeftOverBytes ] = 0;

				strcat( achBuffer, szLine );
				nLen = nLen + *nNoOfLeftOverBytes;
				szLine = achBuffer;
			}

			bAppendLeftOverBytes = TRUE;
		}

		/* enumerate through each char */
		for ( i = 0, ch = szLine[i]; ch != 0 && i < nLen && j < nRawMessageSize; ch = szLine[++i] )
		{
			if ( ( ch >= 33 && ch <= 60 ) || ( ch >= 62 && ch <= 126 ) || ( ch == 9 || ch == 32 ) )
			{
				szOutput[j++] = ch;
			}

			else if ( ch == '=' )
			{
				if ( i + 2 < nLen )
				{
					szOutput[j++] = nConvertHexToDec( szLine[++i], szLine[++i] );
				}

				/* get more data */
				else if ( ii <= nEnd )
				{
					strcpy( achBuffer, &szLine[i+1] );
					szLine = (char *) Vector_elementAt( v, ++ii );
					strcat( achBuffer, szLine );
					nLen = strlen( achBuffer );
					szLine = achBuffer;
					i = -1;
				}

				/* save it for next time */
				else
				{
					szLeftOverBytes[0] = ch; *nNoOfLeftOverBytes = 1;

					if ( i+1 < nLen )
						szLeftOverBytes[1] = ch; *nNoOfLeftOverBytes = 2;
				}
			}
		}
	}

	szOutput[j] = 0;

	return j;
}




/* ---------------------------------- log support ------------------------------------- */



/*
* carsonl, jan 8,98 
* create new log
*
* parameter :
*
*	char *szFilename :	name of log file
*
* return :	instance of log object
*/
errorLog_t *errorLog_new( char *szFilename )
{
	errorLog_t *pLog;

	if ( szFilename == NULL )
		return NULL;

	pLog = (errorLog_t*) malloc( sizeof( errorLog_t ) );

	/* initialize log file */
	if ( pLog != NULL )
	{
		pLog->m_pFile = fopen( szFilename, "a" );

		if ( pLog->m_pFile == NULL )
		{
			free( pLog );
			pLog = NULL;
		}
		else
		{
			strcpy( pLog->m_achFilename, szFilename );
			pLog->m_bActive = TRUE;
		}
	}

	return pLog;
}



/*
* carsonl, jan 8,98 
* error logger
*
* parameter :
*
*	char *pOwner : caller
*	int nError	 : error No
*
* return :	nothing
* NOTE : will only write to log file if m_bActive flag is set to TRUE ( default )
*/
void errorLog2( errorLog_t *pLog, char *szOwner, int nError )
{
	static char buffer[128];
	char *szError;

	if ( pLog == NULL || pLog->m_pFile == NULL || szOwner == NULL )
		return;

	if ( !pLog->m_bActive )
		return;

	switch( nError )
	{
		case MIME_ERR_EOF: szError = "End of file"; break;
		case MIME_ERR_INVALIDPARAM: szError = "Invalid parameter"; break;
		case MIME_ERR_ENCODE: szError = "encoding"; break;
		case MIME_ERR_UNEXPECTED: szError = "unexpected"; break;
		case MIME_ERR_OUTOFMEMORY: szError = "out of memory"; break;
		case MIME_ERR_IO: szError = "I/O"; break;
		case MIME_ERR_IO_READ: szError = "I/O read"; break;
		case MIME_ERR_IO_WRITE: szError = "I/O write"; break;
		case MIME_ERR_PARSE: szError = "parse"; break;
		case MIME_ERR_INVALID_INDEX: szError = "invalid index"; break;
		case MIME_ERR_UNINITIALIZED: szError = "unintialized"; break;
		case MIME_ERR_CANTOPENFILE: szError = "can't open file"; break;
		case MIME_ERR_CANT_SET: szError = "can't set"; break;
		case MIME_ERR_ALREADY_SET: szError = "already set"; break;
		case MIME_ERR_CANT_DELETE: szError = "can't delete"; break;
		case MIME_ERR_CANT_ADD: szError = "can't add"; break;
		case MIME_ERR_NO_HEADERS: szError = "no headers"; break;
		case MIME_ERR_NOT_SET: szError = "not set"; break;
		case MIME_ERR_NO_BODY: szError = "no body"; break;
		case MIME_ERR_NOT_FOUND: szError = "not found"; break;
		case MIME_ERR_NO_CONTENT_SUBTYPE: szError = "no content subtype"; break;
		case MIME_ERR_INVALID_ENCODING: szError = "invalid encoding"; break;
		case MIME_ERR_INVALID_BASICPART: szError = "invalid basicpart"; break;
		case MIME_ERR_INVALID_MULTIPART: szError = "invalid multipart"; break;
		case MIME_ERR_INVALID_MESSAGEPART: szError = "invalid messagepart"; break;
		case MIME_ERR_INVALID_MESSAGE: szError = "invalid message"; break;
		case MIME_ERR_INVALID_CONTENTTYPE: szError = "invalid content type"; break;
		case MIME_ERR_INVALID_CONTENTID: szError = "invalid content ID"; break;
		case MIME_ERR_NO_DATA: szError = "no data"; break;
		case MIME_ERR_NOTIMPL: szError = "not implemented"; break;
		case MIME_ERR_EMPTY_BODY: szError = "empty body"; break;
		case MIME_ERR_UNSUPPORTED_PARTIAL_SUBTYPE: szError = "partial subtype not supported";
		case MIME_ERR_MAX_NESTED_PARTS_REACHED : szError = "maximum number of nested parts reached.";
		default: szError = "Undefined error"; break;
	}

	sprintf( buffer, "ERROR : %s error called from %s\n", szError, szOwner );
	fwrite( buffer, sizeof(char), strlen( buffer ), pLog->m_pFile );
}


/*
* carsonl, jan 8,98 
* error logger
*
* parameter :
*
*	char *pOwner : caller
*	int nError	 : error No
*
* return :	nothing
* NOTE : will only write to log file if m_bActive flag is set to TRUE ( default )
*/
void errorLogMsg2( errorLog_t *pLog, char *szMsg )
{
	static char buffer[128];

	if ( pLog == NULL || pLog->m_pFile == NULL || szMsg == NULL )
		return;

	if ( !pLog->m_bActive )
		return;

	sprintf( buffer, "ERROR : %s\n", szMsg );
	fwrite( buffer, sizeof(char), strlen( buffer ), pLog->m_pFile );
}



/*
* carsonl, jan 8,98 
* free log object
*
* parameter :
*
*	char *pLog : log object to free
*	int nError	 : error No
*
* return :	nothing
*/
void errorLog_free( errorLog_t *pLog )
{
	if ( pLog != NULL )
	{
		if ( pLog->m_pFile != NULL )
			fclose( pLog->m_pFile );

		free( pLog );
	}
}



/*
* carsonl, jan 8,98 
* init log
*
* parameter :
*
*	char *szFilename : log filename
*
* return :	nothing
*/
void initErrorLog( char *szFilename ) { g_pLog = errorLog_new( szFilename ); }




/*
* carsonl, jan 8,98 
* close log file
*
* parameter : none
*
* return :	nothing
*/
void closeErrorLog() { if ( g_pLog != NULL ) errorLog_free( g_pLog ); }



/*
* carsonl, jan 8,98 
* log error
*
* parameter :
*
*	char *pOwner : caller
*	int nError	 : error No
*
* return :	nothing
*/
void errorLog( char *szOwner, int nError )
{
	if ( g_pLog == NULL )
		g_pLog = errorLog_new( LOG_FILE );

	errorLog2( g_pLog, szOwner, nError );
}




/*
* carsonl, jan 8,98 
* log error
*
* parameter :
*
*	char *pOwner : caller
*	int nError	 : error No
*
* return :	nothing
*/
void errorLogMsg( char *szMsg )
{
	if ( g_pLog == NULL )
		g_pLog = errorLog_new( LOG_FILE );

	errorLogMsg2( g_pLog, szMsg );
}


/*
* carsonl, jan 8,98 
* turn error log on
*
* parameter : none
*
* return :	nothing
*/
void errorLogOn() { if ( g_pLog == NULL ) g_pLog->m_bActive = TRUE; }



/*
* carsonl, jan 8,98 
* turn error log off
*
* parameter : none
*
* return :	nothing
*/
void errorLogOff() { if ( g_pLog == NULL ) g_pLog->m_bActive = FALSE; }


