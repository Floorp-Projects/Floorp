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

/* mime.c 
*
* prasad, Jan 98
* carsonl, Jan 98
*
*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>

#include "nsmail.h"
#include "vector.h"
#include "util.h"
#include "mime.h"
#include "mime_internal.h"
#include "mimeparser.h"
#include "mimeparser_internal.h"

static BOOLEAN fNeedPreamble = FALSE;
static BOOLEAN fStartedStream = FALSE;
#define MIME_ENCODING_NOTSET 0


#ifdef DEBUG
#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))
#else
#define outmsg(x) 
#endif


static char base64map [] =
{/*      0   1   2   3   4   5   6   7 */
        'A','B','C','D','E','F','G','H', /* 0 */
        'I','J','K','L','M','N','O','P', /* 1 */
        'Q','R','S','T','U','V','W','X', /* 2 */
        'Y','Z','a','b','c','d','e','f', /* 3 */
        'g','h','i','j','k','l','m','n', /* 4 */
        'o','p','q','r','s','t','u','v', /* 5 */
        'w','x','y','z','0','1','2','3', /* 6 */
        '4','5','6','7','8','9','+','/'  /* 7 */
};

 
static char hexmap [] =
{
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


char * dupDataBuf (char * buf, int size)
{
	char * newbuf;

	newbuf = (char *) malloc (size+1);
	if (newbuf == NULL)
	     return NULL;

	memset (newbuf, 0, size+1);
	/*strncpy (newbuf, buf, size);*/
	memcpy (newbuf, buf, size);
	return newbuf;
}


/* -------------------------------------------- */


void freeFMT (file_mime_type * pFMT)
{
	free (pFMT->content_subtype);
	free (pFMT->content_params);
}


void freeParts (mime_basicPart_t * pBp1, mime_basicPart_t * pBp2, 
		mime_multiPart_t * pMP, mime_message_t *pMsg)
{
	if (pBp1 != NULL)
		mime_basicPart_free_all (pBp1);

	if (pBp2 != NULL)
		mime_basicPart_free_all (pBp2);

	if (pMP != NULL)
		mime_multiPart_free_all (pMP);

	if (pMsg != NULL)
		mime_message_free_all (pMsg);
}

/*
* Prasad, jan 8,98 
* Set the body-data for this part from an input stream.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_setDataStream (mime_basicPart_t * pBasicPart,
		                  nsmail_inputstream_t *pTheDataStream,
				  BOOLEAN fCopyData)
{
	mime_basicPart_internal_t *pBasicPartInternal=NULL;
	char * pLocalBuf=NULL, * pDest=NULL; 
	long stream_size=0, total_read = 0;
	int bytes_read = 0;

	if (pBasicPart == NULL || pTheDataStream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pBasicPart->pInternal == NULL)
		pBasicPart->pInternal = (void *) mime_basicPart_internal_new();

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal->szMessageBody != NULL || pBasicPartInternal->pTheDataStream != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	stream_size = get_inputstream_size (pTheDataStream);

	if (stream_size <= 0)
	{
	    return MIME_ERR_IO_READ;
	}


	if (fCopyData == FALSE)
	{
		/* save ref to the stream */
		pBasicPartInternal->pTheDataStream = pTheDataStream;
		pBasicPartInternal->nMessageSize = stream_size;
		return MIME_OK;
	}

	pLocalBuf = (char *) malloc (stream_size + 1);

	if (pLocalBuf == NULL)
		return MIME_ERR_OUTOFMEMORY;

	pDest = pLocalBuf;

	while (total_read <= stream_size)
	{
		bytes_read = pTheDataStream->read (pTheDataStream->rock, pDest, MIME_BUFSIZE);

		if (bytes_read <= 0)
		{
			break;
		}
		else
		{
			total_read += bytes_read;
			pDest += bytes_read;
		}
	} /* while */

	if (total_read <= 0)
	{
	    free (pLocalBuf);

	    return MIME_ERR_IO_READ;
	}

	pBasicPartInternal->szMessageBody = (char *) pLocalBuf;	
	pBasicPartInternal->nMessageSize = total_read;
	
	return MIME_OK;

} /* mime_basicPart_setDataStream */



/*
* Prasad, jan 8,98 
* Set the body-data for this part from a buffer.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_setDataBuf (mime_basicPart_t * pBasicPart,
			       unsigned int size,
			       const char * pDataBuf,
				BOOLEAN fCopyData)
{
	mime_basicPart_internal_t * pBasicPartInternal=NULL;
	char * pLocalBuf=NULL; 
 	
	if ( pBasicPart == NULL || pDataBuf == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (size == 0)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (size > strlen (pDataBuf))
	{
	   return MIME_ERR_INVALIDPARAM;
	}

	if (pBasicPart->pInternal == NULL)
		pBasicPart->pInternal = (void *) mime_basicPart_internal_new();

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal->szMessageBody != NULL || pBasicPartInternal->pTheDataStream != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	if (fCopyData == FALSE)
	{
		pBasicPartInternal->szMessageBody = (char *) pDataBuf;	
		pBasicPartInternal->nMessageSize = size;
    		return MIME_OK;
	}

	/* Copy the data. The user might FREE pDataBuf */

	pLocalBuf = (char *) malloc (size);	
	/*bcopy (pDataBuf, pLocalBuf, size);*/
	memcpy (pLocalBuf, pDataBuf, size);
	pBasicPartInternal->szMessageBody = (char *) pLocalBuf;	
	pBasicPartInternal->nMessageSize = size;
	
    	return MIME_OK;
}




/*
* Prasad, jan 8,98 
* Deletes the bodyData for this part. 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_deleteData (mime_basicPart_t * pBasicPart)
{
	mime_basicPart_internal_t *pBasicPartInternal=NULL;

	if (pBasicPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal != NULL && pBasicPartInternal->szMessageBody != NULL)
	{
		free (pBasicPartInternal->szMessageBody);
		pBasicPartInternal->szMessageBody = NULL;
		pBasicPartInternal->nMessageSize = 0;

		return MIME_OK;
	}

	if (pBasicPartInternal != NULL && pBasicPartInternal->pTheDataStream != NULL)
	{
		pBasicPartInternal->pTheDataStream = NULL;
		pBasicPartInternal->nMessageSize = 0;
		return MIME_OK;
	}
	
	/* Delete with no Data O.K. */

	return MIME_OK;
}



/*
* gives the size of the body-data in bytes 
*
* parameter :
*
*	pBasicPart : basicPart
*	pSize : (output) size of message data
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_getSize (mime_basicPart_t * pBasicPart, unsigned int * pSize)
{
	mime_basicPart_internal_t *pBasicPartInternal=NULL;

	if ( pBasicPart == NULL || pSize == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if ( pBasicPartInternal != NULL )
	{
		*pSize = pBasicPartInternal->nMessageSize;

		return MIME_OK;
	}
	else
	{
		return 0;
	}
}



/*
* Prasad, jan 8,98 
* returns the body-data after any decoding in an input stream 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_getDataStream (mime_basicPart_t * pBasicPart,
				  char 		   * pFileName,
			          nsmail_inputstream_t ** ppDataStream)
{
	char * pLocalBuf=NULL;
	mime_basicPart_internal_t * pBasicPartInternal=NULL;
	FILE * fp;
        char * pDest=NULL;
	nsmail_inputstream_t * pDataStream=NULL;
        int bytes_read=0, total_read=0, stream_size=0;

	if (pBasicPart == NULL || ppDataStream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal != NULL && pBasicPartInternal->szMessageBody != NULL)
	{

		if (pFileName == NULL)
		{
			pLocalBuf = dupDataBuf (pBasicPartInternal->szMessageBody, 
						pBasicPartInternal->nMessageSize);

			return  buf_instream_create (pLocalBuf, 
						pBasicPartInternal->nMessageSize, 
						ppDataStream);
		}
		else
		{
			/* Return the specified file-based input-stream */
			if ((fp = fopen(pFileName, "wb")) == (FILE *) NULL)
        		{
                		return MIME_ERR_CANTOPENFILE;

        		}

			fwrite (pBasicPartInternal->szMessageBody, sizeof (char), pBasicPartInternal->nMessageSize, fp);

		        fclose (fp);
		    	free (pLocalBuf);
			pLocalBuf = NULL;

			return  file_instream_create (pFileName, 
					             ppDataStream);
		}
	}
	else if (pBasicPartInternal != NULL && pBasicPartInternal->pTheDataStream != NULL)
	{
		pDataStream = pBasicPartInternal->pTheDataStream;
		stream_size = pBasicPartInternal->nMessageSize;

		if (pFileName != NULL)
		{
			/* Return the specified file-based input-stream */
			if ((fp = fopen(pFileName, "wb")) == (FILE *) NULL)
        		{
                		return MIME_ERR_CANTOPENFILE;
        		}

			pLocalBuf = (char *) malloc (MIME_BUFSIZE);
			memset (pLocalBuf, 0, MIME_BUFSIZE);

			while (total_read <= stream_size)
			{

				bytes_read = pDataStream->read (pDataStream->rock, 
								pLocalBuf, MIME_BUFSIZE);
	
				if (bytes_read <= 0)
				{
					break;
				}
				else
				{
					total_read += bytes_read;
					fwrite (pLocalBuf, sizeof (char), bytes_read, fp);
					memset (pLocalBuf, 0, MIME_BUFSIZE);
				}
			}

			if (total_read <= 0)
			{
			     fclose (fp);
		    	     free (pLocalBuf);
			     pLocalBuf = NULL;
			     return MIME_ERR_IO_READ; 
			}

		        fclose (fp);
		    	free (pLocalBuf);
			pLocalBuf = NULL;

			return  file_instream_create (pFileName, 
					             ppDataStream);

		}
		else
		{
			/* Return a buf-based input-stream */
	
			pLocalBuf = (char *) malloc (stream_size +1);
			memset (pLocalBuf, 0, stream_size+1);
	
			if (pLocalBuf == NULL)
				return MIME_ERR_OUTOFMEMORY;
	
			pDest = pLocalBuf;
			pDataStream->rewind (pDataStream->rock);
	
			while (total_read <= stream_size)
			{
				bytes_read = pDataStream->read (pDataStream->rock, 
								pDest, MIME_BUFSIZE);
	
				if (bytes_read <= 0)
				{
					break;
				}
				else
				{
					total_read += bytes_read;
					pDest += bytes_read;
				}
			} /* while */
	
			if (total_read <= 0)
			{
		    	     free (pLocalBuf);
			     pLocalBuf = NULL;
			     return MIME_ERR_IO_READ; 
			}
	
			pDest[ stream_size ] = NULL;

			return  buf_instream_create (pLocalBuf, 
						     stream_size,
					             ppDataStream);
	
		}
/*
 *		*ppDataStream = pBasicPartInternal->pTheDataStream;
 *		(*ppDataStream)->rewind ((*ppDataStream)->rock);
 *		return MIME_OK;
 */
	}

	return MIME_ERR_NO_DATA;

} /* mime_basicPart_getDataStream() */



/*
* Prasad, jan 8,98 
* returns the body-data after any decoding in a buffer 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_getDataBuf (mime_basicPart_t * pBasicPart,
				unsigned int * pSize,
				char **ppDataBuf )
{
	mime_basicPart_internal_t * pBasicPartInternal=NULL;
	nsmail_inputstream_t * pDataStream=NULL;
	char * pDest=NULL;
	int bytes_read=0, total_read=0, stream_size=0;

	if (pBasicPart == NULL || pSize == NULL || ppDataBuf == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal != NULL && pBasicPartInternal->szMessageBody != NULL)
	{
		*pSize = pBasicPartInternal->nMessageSize;
		/* *ppDataBuf = szStringClone( pBasicPartInternal->szMessageBody );*/
		/**ppDataBuf = strdup (pBasicPartInternal->szMessageBody);*/
		*ppDataBuf = dupDataBuf (pBasicPartInternal->szMessageBody, 
					 pBasicPartInternal->nMessageSize);
		if (*ppDataBuf == NULL)
			return MIME_ERR_OUTOFMEMORY;

		return MIME_OK;
	}
	else if (pBasicPartInternal != NULL && pBasicPartInternal->pTheDataStream != NULL)
	{
		pDataStream = pBasicPartInternal->pTheDataStream;
		stream_size = pBasicPartInternal->nMessageSize;

		*ppDataBuf = (char *) malloc (stream_size +1);

		if (*ppDataBuf == NULL)
			return MIME_ERR_OUTOFMEMORY;

		pDest = *ppDataBuf;
		pDataStream->rewind (pDataStream->rock);

		while (total_read <= stream_size)
		{
			bytes_read = pDataStream->read (pDataStream->rock, 
							pDest, MIME_BUFSIZE);

			if (bytes_read <= 0)
			{
				break;
			}
			else
			{
				total_read += bytes_read;
				pDest += bytes_read;
			}
		} /* while */

		if (total_read <= 0)
		{
	    	     free (*ppDataBuf);
		     *ppDataBuf = NULL;
		     return MIME_ERR_IO_READ; 
		}

		pDest[ stream_size ] = NULL;

		return MIME_OK;
	}
	else
	{
		return MIME_ERR_NO_DATA;
	}
	
} /* mime_basicPart_getDataBuf */



/*
* Prasad, jan 8,98 
* Writes out byte stream for this part with MIME headers and encoded body-data 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_putByteStream (mime_basicPart_t * pBasicPart,
			    	  nsmail_outputstream_t *pOutput_stream)
{
	mime_basicPart_internal_t *pBasicPartInternal=NULL;
	static char  * hdrbuf = NULL;
	char  * pHdr = NULL;
	mime_header_t * pEHdr = NULL;
	int len=0, ret=0, read_len=0;
	mime_encoding_type encoding;
	nsmail_inputstream_t * pInput_stream = NULL;
	BOOLEAN fDatainBuf = FALSE, fWroteVersion = FALSE;

	outmsg("Entered mime_basicPart_putByteStream()");

	if (pBasicPart == NULL || pOutput_stream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pBasicPartInternal = (mime_basicPart_internal_t *) pBasicPart->pInternal;

	if (pBasicPartInternal == NULL || (pBasicPartInternal->szMessageBody == NULL 
		&& pBasicPartInternal->pTheDataStream == NULL))
	{
		return MIME_ERR_NO_BODY;
	}

	if (pBasicPartInternal->szMessageBody != NULL)
	{
		outmsg("fDatainBuf = TRUE");
		fDatainBuf = TRUE;
	}

	/* Write out the headers first */

	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (MIME_BUFSIZE);
		
		if (hdrbuf == NULL)
			return MIME_ERR_NO_BODY;
	}
	
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	outmsg("Writing out content_type/sub-type; params!");

	/* Content-Type */
	switch (pBasicPart->content_type)
	{

		case MIME_CONTENT_TEXT:
		     len = append_str (pHdr, "Content-Type: text/");
		     pHdr += len;

		     if (pBasicPart->content_subtype != NULL)
		     {
		     	  len = append_str (pHdr, pBasicPart->content_subtype);
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "plain");
		     	  pHdr += len;
		     }

		     if (pBasicPart->content_type_params != NULL)
		     {
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, pBasicPart->content_type_params);
		     	  pHdr += len;
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     else
		     {
#if (0)  /* removed */
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, "charset=us-ascii\r\n");
#endif
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     break;

		case MIME_CONTENT_AUDIO:
		     len = append_str (pHdr, "Content-Type: audio/");
		     pHdr += len;

		     if (pBasicPart->content_subtype != NULL)
		     {
		     	  len = append_str (pHdr, pBasicPart->content_subtype);
		     	  pHdr += len;
		     }
		     else
		     {
			  /*l_hdrbuf.append ("basic"); */
		     	return MIME_ERR_NO_CONTENT_SUBTYPE;
		     }

		     if (pBasicPart->content_type_params != NULL)
		     {
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, pBasicPart->content_type_params);
		     	  pHdr += len;
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     break;

		case MIME_CONTENT_IMAGE:
		     len = append_str (pHdr, "Content-Type: image/");
		     pHdr += len;

		     if (pBasicPart->content_subtype != NULL)
		     {
		     	  len = append_str (pHdr, pBasicPart->content_subtype);
		     	  pHdr += len;
		     }
		     else
		     {
			   /*l_hdrbuf.append ("jpeg"); */
		     	   return MIME_ERR_NO_CONTENT_SUBTYPE;
		     }

		     if (pBasicPart->content_type_params != NULL)
		     {
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, pBasicPart->content_type_params);
		     	  pHdr += len;
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     break;

		case MIME_CONTENT_VIDEO:
		     len = append_str (pHdr, "Content-Type: video/");
		     pHdr += len;

		     if (pBasicPart->content_subtype != NULL)
		     {
		     	  len = append_str (pHdr, pBasicPart->content_subtype);
		     	  pHdr += len;
		     }
		     else
		     {
			   /*l_hdrbuf.append ("mpeg"); */
		     	   return MIME_ERR_NO_CONTENT_SUBTYPE;
		     }

		     if (pBasicPart->content_type_params != NULL)
		     {
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, pBasicPart->content_type_params);
		     	  pHdr += len;
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     break;

		case MIME_CONTENT_APPLICATION:
		     len = append_str (pHdr, "Content-Type: application/");
		     pHdr += len;

		     if (pBasicPart->content_subtype != NULL)
		     {
		     	  len = append_str (pHdr, pBasicPart->content_subtype);
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "octet-stream");
		     	  pHdr += len;
		     }

		     if (pBasicPart->content_type_params != NULL)
		     {
		     	  len = append_str (pHdr, "; ");
		     	  pHdr += len;
		     	  len = append_str (pHdr, pBasicPart->content_type_params);
		     	  pHdr += len;
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     else
		     {
		     	  len = append_str (pHdr, "\r\n");
		     	  pHdr += len;
		     }
		     break;
		
		case MIME_CONTENT_MULTIPART:
		case MIME_CONTENT_MESSAGEPART:
		
		default:
		     return MIME_ERR_INVALID_CONTENTTYPE;
	}

	/* write out content-type */

	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	outmsg("DONE Writing out content_type/sub-type; params!");

	/* contentID */
	if (pBasicPart->contentID != NULL)
    	{
         	len = append_str (pHdr, "Content-ID: ");
         	pHdr += len;
         	len = append_str (pHdr, pBasicPart->contentID);
         	pHdr += len;
		len = append_str (pHdr, "\r\n");
		pHdr += len;
		/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
		memset (hdrbuf, 0, MIME_BUFSIZE);
		pHdr = hdrbuf;
    	}

	/* contentMD5 */
	if (pBasicPart->contentMD5 != NULL)
    	{
         	len = append_str (pHdr, "Content-MD5: ");
         	pHdr += len;
         	len = append_str (pHdr, pBasicPart->contentMD5);
         	pHdr += len;
		len = append_str (pHdr, "\r\n");
		pHdr += len;
		/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
		memset (hdrbuf, 0, MIME_BUFSIZE);
		pHdr = hdrbuf;
    	}

	/* contentDisposition */
	if (pBasicPart->content_disposition != MIME_DISPOSITION_UNINITIALIZED && pBasicPart->content_disposition > 0)
        {
	     fprintf (stderr, "Disp = %d\n", pBasicPart->content_disposition);
             len = append_str (pHdr, "Content-Disposition: ");
             pHdr += len;

	     switch (pBasicPart->content_disposition)
	     {
		  case MIME_DISPOSITION_INLINE:
             	       len = append_str (pHdr, "inline");
                       pHdr += len;
		       break;
		  case MIME_DISPOSITION_ATTACHMENT:
             	       len = append_str (pHdr, "attachment");
                       pHdr += len;
		       break;
	     }
	     
	     if (pBasicPart->content_disp_params != NULL)
	     {
		  len = append_str (pHdr, "; ");
		  pHdr += len;
		  len = append_str (pHdr, pBasicPart->content_disp_params);
		  pHdr += len;
	     }

	     len = append_str (pHdr, "\r\n");
	     pHdr += len;

	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* content-Description */
	if (pBasicPart->content_description != NULL)
        {
             len = append_str (pHdr, "Content-Description: ");
             pHdr += len;
             len = append_str (pHdr, pBasicPart->content_description);
             pHdr += len;
	     len = append_str (pHdr, "\r\n");
	     pHdr += len;
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	outmsg("Writing out content-Transfer-Encoding");

	/* content-transfer-encoding */
	if (pBasicPart->encoding_type != MIME_ENCODING_UNINITIALIZED && pBasicPart->encoding_type != MIME_ENCODING_NOTSET)
	{
	    switch (pBasicPart->encoding_type)
	    {
		case MIME_ENCODING_BASE64:
		     encoding = MIME_ENCODING_BASE64;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: base64\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_QP:
		     encoding = MIME_ENCODING_QP;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: quoted-printable\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_7BIT:
		     encoding = MIME_ENCODING_7BIT;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: 7bit\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_8BIT:
		     encoding = MIME_ENCODING_8BIT;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: 8bit\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_BINARY:
		     encoding = MIME_ENCODING_BINARY;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: binary\r\n");
                     pHdr += len;
		     break;
#if (0)
		case MIME_ENCODING_UNINITIALIZED:
		     if (pBasicPart->content_type == MIME_CONTENT_TEXT)
		     {
			 encoding = MIME_ENCODING_7BIT;
             	         len = append_str (pHdr, "Content-Transfer-Encoding: 7bit\r\n");
                         pHdr += len;
#ifdef XP_UNIX
			  if ((strlen (pBasicPart->content_type_params) == 8) &&
			      (strncasecmp (pBasicPart->content_type_params, "us-ascii", 8) == 0))
#else
			  if ((strlen (pBasicPart->content_type_params) == 8) &&
			      bStringEquals (pBasicPart->content_type_params, "us-ascii"))
#endif
			  {
				encoding = MIME_ENCODING_7BIT;
             	                len = append_str (pHdr, "Content-Transfer-Encoding: 7bit\r\n");
                                pHdr += len;
			  }
			  else
			  {
				encoding = MIME_ENCODING_BASE64;
             	                len = append_str (pHdr, "Content-Transfer-Encoding: base64\r\n");
                                pHdr += len;
			  }
		     }
		     else
		     {
			   encoding = MIME_ENCODING_BASE64;
             	           len = append_str (pHdr, "Content-Transfer-Encoding: base64\r\n");
                           pHdr += len;
		     }
		     break;
#endif
		default:
		     return MIME_ERR_INVALID_ENCODING;
	     } /* switch */

	     /* write-out encoding line */
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
	     outmsg("DONE Writing out content-Transfer-Encoding");
	}
	else
	     encoding = MIME_ENCODING_7BIT; /* for use with writing body-data only */


	fWroteVersion = FALSE;
	/* Write-out all the additional headers if any */
	if (pBasicPart->extra_headers != NULL)
        {
		for (pEHdr = pBasicPart->extra_headers; 
		     pEHdr != NULL;
		     /*pEHdr != NULL, pEHdr->name != NULL, pEHdr->value != NULL; */
		     pEHdr = pEHdr->next)
		{
		     if (pEHdr->name != NULL  && pEHdr->value != NULL)
		     {
	    		if (bStringEquals (pEHdr->name, "MIME-Version"))
			{
	    			if (fWroteVersion)
				    continue;
				else
	    	 		     fWroteVersion = TRUE;
			}
             	     	len = append_str (pHdr, pEHdr->name);
                     	pHdr += len;
             	     	len = append_str (pHdr, ": ");
                     	pHdr += len;
             	     	len = append_str (pHdr, pEHdr->value);
                     	pHdr += len;
             	     	len = append_str (pHdr, "\r\n");
                     	pHdr += len;

		     	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     	     	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
		     	memset (hdrbuf, 0, MIME_BUFSIZE);

		     }

	             pHdr = hdrbuf;
		} /* for */
	} /* end extra hdrs */

	/* extra line after headers */
    	len = append_str (pHdr, "\r\n");
    	pHdr += len;
	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	outmsg("DONE Writing out Header");

	if (fDatainBuf == TRUE)
	{
		outmsg("Doing buf_instream_create()");

		ret = buf_instream_create (pBasicPartInternal->szMessageBody, 
				     	   pBasicPartInternal->nMessageSize, 
				     	   &pInput_stream);
		if (ret != MIME_OK)
	     		return ret;
	}
	else
	{
		pInput_stream = pBasicPartInternal->pTheDataStream;
		pInput_stream->rewind (pInput_stream->rock);
	}

	if (pInput_stream == NULL)
	{
	     return MIME_ERR_UNEXPECTED;
	}

	switch (encoding)
	{
	    case MIME_ENCODING_BASE64:
		 outmsg("Invoking mime_encodeBase64()");

		 ret =  mime_encodeBase64 (pInput_stream, pOutput_stream);
		 if (ret < 0)
		     return ret;
		 break;
	    
	    case MIME_ENCODING_QP:
		 outmsg("Invoking mime_encodeQP()");

		 ret =  mime_encodeQP (pInput_stream, pOutput_stream);
		 if (ret < 0)
		     return ret;
		 break;
	    
	    case MIME_ENCODING_7BIT:
	    case MIME_ENCODING_8BIT:
	    case MIME_ENCODING_BINARY:
	    case MIME_ENCODING_NOTSET:
		
		outmsg("No encoding!");

		read_len = pInput_stream->read (pInput_stream->rock, hdrbuf, MIME_BUFSIZE);

		while (read_len > 0)
		{
			outmsg("Writing data out");
#ifdef DEBUG
			fprintf (stderr, "read_len=%d\n", read_len);
#endif
			pOutput_stream->write (pOutput_stream->rock, hdrbuf, read_len);

			read_len = pInput_stream->read (pInput_stream->rock, hdrbuf, MIME_BUFSIZE);
		 }
		 break;
	    
	     default:
#ifdef DEBUG
		errorLog( "mime_basicPart_putByteStream()", MIME_ERR_INVALID_ENCODING );
#endif

		return MIME_ERR_INVALID_ENCODING;
	}

#if (0)
	/* final touch */
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;
    	len = append_str (pHdr, "\r\n\r\n");
    	pHdr += len;

	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
#endif

	return MIME_OK;

} /* mime_basicPart_putByteStream() */



/*
* Frees up all parts of the basic part including internal structures.
*
* parameter :
*
*	pBasicPart : basicPart
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_free_all (mime_basicPart_t * pBasicPart)
{
	if ( pBasicPart == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_basicPart_free( pBasicPart );
}




/*************************************************************************/
/**                       COMPOSITE BODY PART TYPES                     **/
/*************************************************************************/


/*=========================== MULTI PART =================================*/


/*
* creates a multi part from an input stream. 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
/*
 * int mime_multiPart_create (nsmail_inputstream_t *pInput_stream, 
 *                           mime_multiPart_t ** ppMultiPart)
 * {
 *	if ( pInput_stream == NULL || ppMultiPart == NULL )
 *	{
 *		errorLog( "mime_multiPart_create()", MIME_ERR_INVALIDPARAM );
 *		return MIME_ERR_INVALIDPARAM;
 *	}
 *
 *	return mime_multiPart_decode( pInput_stream, ppMultiPart);
 * }
 */


/*
* Prasad, Jan 98 
* Adds a file as basic part to the multi-part. 
* If in_pref_file_encoding  > 0 attempts to use the encoding 
* for the file-attachment. If encoding is not valid for the 
* file-type overrides with the correct value.              
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addFile (mime_multiPart_t * pMultiPart,
		            char *   pFileFullName,
			    mime_encoding_type pref_file_encoding,
		            int * pIndex_assigned)
{
	nsmail_inputstream_t * pInput_stream = NULL;
        mime_basicPart_t * pFilePart = NULL;
        file_mime_type     fmt;
        char * extn = NULL, *short_file_name = NULL;
        int ret = 0, idx = 0;

	if (pMultiPart == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (pFileFullName == NULL || pIndex_assigned == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pFileFullName != NULL)
	{
		outmsg("Creating File Part");

		extn = getFileExtn (pFileFullName);

		ret = getFileMIMEType (extn, &fmt);
	
		if (pref_file_encoding > 0 && pref_file_encoding <= MIME_ENCODING_BINARY)
		{
			if (fmt.mime_encoding != MIME_ENCODING_BASE64)
				fmt.mime_encoding = pref_file_encoding;

		}

		/* Create and return a Message from file */
		pFilePart = (mime_basicPart_t *) malloc (sizeof (mime_basicPart_t));

		if (pFilePart == NULL)
		{
			free (extn);
			freeFMT (&fmt);
			return MIME_ERR_OUTOFMEMORY;
		}
		else    memset (pFilePart, 0, sizeof (mime_basicPart_t));

		ret =  file_instream_create (pFileFullName, &pInput_stream);

		if (ret != MIME_OK)
		{
			free (extn);
			freeFMT (&fmt);
			freeParts (NULL, pFilePart, NULL, NULL);
			free (pFilePart);
			return ret;
		}

		pFilePart->content_type = fmt.content_type;
		pFilePart->content_subtype = fmt.content_subtype;
		pFilePart->content_type_params = fmt.content_params;
		pFilePart->encoding_type = fmt.mime_encoding;

		short_file_name = getFileShortName (pFileFullName);
		if (short_file_name != NULL)
			pFilePart->content_description = short_file_name;

		/* Set to FALSE below if performance becomes a factor!!! */
		ret = mime_basicPart_setDataStream (pFilePart, pInput_stream, TRUE);

		if (ret != MIME_OK)
		{
			pInput_stream->close (pInput_stream->rock);
			free (pInput_stream);
			free (extn);
			freeFMT (&fmt);

			freeParts (NULL, pFilePart, NULL, NULL);
			free (pFilePart);
			return ret;
		}

		outmsg ("adding basic part to multi-part");

		ret = mime_multiPart_addBasicPart (pMultiPart, pFilePart, FALSE, &idx);
		
#ifdef DEBUG
		fprintf (stderr, "Index assigned = %d\n", idx);
#endif

		if (ret != MIME_OK)
		{
			if (pInput_stream != NULL)
			    pInput_stream->close (pInput_stream->rock);
			free (pInput_stream);
			free (extn);
			freeFMT (&fmt);
			freeParts (NULL, pFilePart, NULL, NULL);
			free (pFilePart);
			return ret;
		}

		*pIndex_assigned = idx;

		return MIME_OK;
	}

}
/*=============*/


/*
* Prasad, jan 8,98 
* Adds a basic part to the multi-part. 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addBasicPart (mime_multiPart_t * pMultiPart,
		            	 mime_basicPart_t * pBasicPart,
				 BOOLEAN clone,
		            	 int * pIndex_assigned)
{
	if (pMultiPart == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (pBasicPart == NULL)
	{
		return MIME_ERR_INVALID_BASICPART;
	}

	if (pIndex_assigned == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pBasicPart->content_type <= MIME_CONTENT_UNINITIALIZED)
	{
		return MIME_ERR_INVALID_CONTENTTYPE;
	}

	return mime_multiPart_addPart_clonable (pMultiPart, (void *) pBasicPart, 
					pBasicPart->content_type, clone, pIndex_assigned);
}




/*
* Prasad, jan 8,98 
* Adds a message part to the multi-part. 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addMessagePart (mime_multiPart_t * pMultiPart,
		            	   mime_messagePart_t * pMessagePart,
				   BOOLEAN clone,
		            	   int * pIndex_assigned)
{
	if (pMultiPart == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (pMessagePart == NULL || pIndex_assigned == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_multiPart_addPart_clonable (pMultiPart, (void *) pMessagePart, 
					MIME_CONTENT_MESSAGEPART, clone, pIndex_assigned);
}




/*
* Prasad, jan 8,98 
* Adds a message part to the multi-part. 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addMultiPart (mime_multiPart_t * pMultiPart,
		            	 mime_multiPart_t * pMultiPart2,
				 BOOLEAN clone,
		            	 int * pIndex_assigned)
{
	if (pMultiPart == NULL || pMultiPart2 == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (pIndex_assigned == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_multiPart_addPart_clonable (pMultiPart, (void *) pMultiPart2, 
					MIME_CONTENT_MULTIPART, clone, pIndex_assigned);
}



/*
* Prasad, jan 8,98 
* Deletes the bodyPart at the requested index from this multi-part 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_deletePart (mime_multiPart_t * pMultiPart,
		               int index)
{
	int partCount=0, i=0;
	mime_mp_partInfo_t * pPartInfo=NULL;
	mime_mp_partInfo_t * pNextPartInfo=NULL;
	mime_multiPart_internal_t *   pMultiPartInternal=NULL;

	if (pMultiPart == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (index <= 0)
	{
		return MIME_ERR_INVALID_INDEX;
	}

	pMultiPartInternal = (mime_multiPart_internal_t *) pMultiPart->pInternal;

	if (pMultiPartInternal == NULL)
	{
		return MIME_ERR_UNINITIALIZED;
	}

	partCount = pMultiPartInternal->nPartCount;

	if (partCount < index)
	{
		return MIME_ERR_INVALID_INDEX;
	}

	pPartInfo = &pMultiPartInternal->partInfo [index];

	switch (pPartInfo->nContentType)
	{
		case MIME_CONTENT_TEXT:
		case MIME_CONTENT_AUDIO:
                case MIME_CONTENT_IMAGE:
                case MIME_CONTENT_VIDEO:
                case MIME_CONTENT_APPLICATION:
		     mime_basicPart_free ((mime_basicPart_t *) pPartInfo->pThePart);
		     break;
                case MIME_CONTENT_MULTIPART:
		     mime_multiPart_free ((mime_multiPart_t *) pPartInfo->pThePart);
		     break;
                case MIME_CONTENT_MESSAGEPART:
		     mime_messagePart_free ((mime_messagePart_t *) pPartInfo->pThePart);
		     break;
		default:
                     return MIME_ERR_UNEXPECTED;
	} /* switch */

	pPartInfo->pThePart = NULL;

	if (pPartInfo->contentID != NULL)
	{
		free (pPartInfo->contentID);
		pPartInfo->contentID = NULL;
	}

	/* update links */
	if  (index <  partCount)
	{
		for (i=index+1; i <= partCount; i++)
		{
		    	 
			pPartInfo = &pMultiPartInternal->partInfo [i-1];
			pNextPartInfo = &pMultiPartInternal->partInfo [i];
	
			pPartInfo->nContentType = pNextPartInfo->nContentType;
			pPartInfo->contentID = pNextPartInfo->contentID;
			pPartInfo->pThePart = pNextPartInfo->pThePart;
		}
	}

    pPartInfo = &pMultiPartInternal->partInfo [partCount];
	pPartInfo->nContentType = MIME_CONTENT_UNINITIALIZED;
	pPartInfo->contentID = NULL;
	pPartInfo->pThePart = NULL;

	partCount--;

    return MIME_OK;
}



/*
* returns the count of the body parts in this multi-part 
*
* parameter :
*
*	pMultiPart : multiPart
*	pCount : (output) number of parts
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_getPartCount (mime_multiPart_t * pMultiPart,
		                int * pCount)
{
	mime_multiPart_internal_t * pMultiPartInternal=NULL;

	if (pMultiPart == NULL || pCount == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	pMultiPartInternal = (mime_multiPart_internal_t *) pMultiPart->pInternal;

	if (pMultiPartInternal != NULL)
	{
		*pCount = pMultiPartInternal->nPartCount;

		return MIME_OK;
	}
	
	return MIME_ERR_UNINITIALIZED;
}



/*
* returns the body part  at the specified index 
* index always starts with 1 
*
* parameter :
*
*	pMultiPart : multiPart
*	index : which part within the multipart
*	pContentType : (output) content type of part returned
*	pptheBodyPart : (output) part returned
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_getPart (mime_multiPart_t * pMultiPart,
	            	    int  index,
			    mime_content_type *pContentType,
			    void **ppTheBodyPart  /* Client can cast this based on pContentType */ 
			    )
{
	if (pMultiPart == NULL || pContentType == NULL || ppTheBodyPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_multiPart_getPart2 (pMultiPart, index, NULL, pContentType, ppTheBodyPart);
}



/*
* carsonl, jan 8,98 
* returns the body part with the specified contentID 
*
* parameter :
*
*	pMultiPart : multiPart
*	contentID : content ID
*	pContentType : (output) content type of part returned
*	pptheBodyPart : (output) part returned, cast this based on pContent_type 
*
* returns :	MIME_OK if successful
*/
/*
int mime_multiPart_getCIDPart (mime_multiPart_t * pMultiPart,
			       char * contentID,
			       mime_content_type * pContentType,
			       void **ppTheBodyPart  
			      )
{
	if (pMultiPart == NULL)
	{
		errorLog( "mime_multiPart_getCIDPart()", MIME_ERR_INVALID_MULTIPART );
		return MIME_ERR_INVALID_MULTIPART;
	}

	if (contentID == NULL || pContentType == NULL || ppTheBodyPart == NULL)
	{
		errorLog( "mime_multiPart_getCIDPart()", MIME_ERR_INVALIDPARAM );
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_multiPart_getPart2 (pMultiPart, -1, contentID, pContentType, ppTheBodyPart);
}
*/


/*
* Small utility routine to output boundary, in other words write the boundary to the outputstream
*
* parameter :
*
*	pBoundary : boundary string
*	pOutput_stream : output stream
*
* returns :	MIME_OK if successful
*/
static void mime_output_boundary (char * pBoundary, nsmail_outputstream_t *pOutput_stream)
{
 	char  * pHdr = NULL;
	static char  * hdrbuf = NULL;
        int len=0;

	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (100);
		if (hdrbuf == NULL)
			return;
	}

	memset (hdrbuf, 0, 100);
	pHdr = hdrbuf;

        /*len = append_str (pHdr, "\r\n\r\n");*/
        len = append_str (pHdr, "\r\n");
        pHdr += len;
        len = append_str (pHdr, "--");
        pHdr += len;
	len = append_str (pHdr, pBoundary);
	pHdr += len;
        len = append_str (pHdr, "\r\n");
        pHdr += len;

	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf ));
}


static void mime_output_trail_boundary (char * pBoundary, nsmail_outputstream_t *pOutput_stream)
{
 	static char  * hdrbuf = NULL;
 	char  * pHdr = NULL;
        int len=0;


	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (100);
		if (hdrbuf == NULL)
			return;
	}

	memset (hdrbuf, 0, 100);
	pHdr = hdrbuf;

        /*len = append_str (pHdr, "\r\n\r\n");*/
        len = append_str (pHdr, "\r\n");
        pHdr += len;
        len = append_str (pHdr, "--");
        pHdr += len;
	len = append_str (pHdr, pBoundary);
	pHdr += len;
        len = append_str (pHdr, "--");
        pHdr += len;
        len = append_str (pHdr, "\r\n");
        pHdr += len;

	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
}



/*
* Prasad, jan 8,98 
* Writes out byte stream for this part with MIME headers and encoded body-data 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_putByteStream (mime_multiPart_t * pMultiPart,
			    	  nsmail_outputstream_t *pOutput_stream)
{
    	char * pHdr=NULL, * pBoundary=NULL;
    	mime_header_t * pEHdr=NULL;
    	int fwriteit = 0, len=0, bpCount=0, idx=0;
 	static char  * hdrbuf=NULL;
	mime_multiPart_internal_t * pMultiPartInternal = NULL;
	mime_mp_partInfo_t * pPartInfo = NULL;
	BOOLEAN fquoteBounds = FALSE;

	mime_basicPart_t * pBasicPart2 = NULL;
	mime_multiPart_t * pMultiPart2 = NULL;
	mime_messagePart_t * pMessagePart2 = NULL;


	outmsg("Entered mime_multiPart_putByteStream()");

	if (pMultiPart == NULL || pOutput_stream == NULL)
	{
		return MIME_ERR_INVALID_MULTIPART;
	}

	pMultiPartInternal = (mime_multiPart_internal_t *) pMultiPart->pInternal;

	if (pMultiPartInternal == NULL)
	{
		return MIME_ERR_NO_BODY;
	}

	bpCount = pMultiPartInternal->nPartCount;
	
#ifdef DEBUG
	fprintf (stderr, "%s:%d> bpCount = %d\n", __FILE__, __LINE__, bpCount);
#endif
	
	if (bpCount < 1)
	{
		return MIME_ERR_NO_BODY;
	}


	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (MIME_BUFSIZE);
		if (hdrbuf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}


	/* Write the Headers out first */

	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	/* Content-Type */
        len = append_str (pHdr, "Content-Type: multipart/");
        pHdr += len;

	if (pMultiPart->content_subtype != NULL)
	{
	    len = append_str (pHdr, pMultiPart->content_subtype);
	    pHdr += len;
	}
	else
	{
        	return MIME_ERR_NO_CONTENT_SUBTYPE;
	}

	if (pMultiPart->content_type_params != NULL)
	{
	     len = append_str (pHdr, "; ");
	     pHdr += len;
	     len = append_str (pHdr, pMultiPart->content_type_params);
	     pHdr += len;
	}

	if (pMultiPartInternal->szBoundary == NULL)
	     pBoundary = (char *) generateBoundary();
	else
	{
	     pBoundary = strdup (pMultiPartInternal->szBoundary);
	     fquoteBounds = TRUE;
	}

	len = append_str (pHdr, "; ");
	pHdr += len;
	len = append_str (pHdr, "boundary=");
	pHdr += len;
	if (fquoteBounds == TRUE)
		*pHdr++ = '"';
	len = append_str (pHdr, pBoundary);
	pHdr += len;
	if (fquoteBounds == TRUE)
		*pHdr++ = '"';
	len = append_str (pHdr, "\r\n");
	pHdr += len;

	/* write out content-type */
	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	/* contentID */
	if (pMultiPart->contentID != NULL)
        {
             len = append_str (pHdr, "Content-ID: ");
             pHdr += len;
             len = append_str (pHdr, pMultiPart->contentID);
             pHdr += len;
	     len = append_str (pHdr, "\r\n");
	     pHdr += len;
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* contentDisposition */
	if (pMultiPart->content_disposition != MIME_DISPOSITION_UNINITIALIZED && pMultiPart->content_disposition > 0)
        {
             len = append_str (pHdr, "Content-Disposition: ");
             pHdr += len;

	     switch (pMultiPart->content_disposition)
	     {
		  case MIME_DISPOSITION_INLINE:
             	       len = append_str (pHdr, "inline");
                       pHdr += len;
		       break;
		  case MIME_DISPOSITION_ATTACHMENT:
             	       len = append_str (pHdr, "attachment");
                       pHdr += len;
		       break;
	     }
	     
	     if (pMultiPart->content_disp_params != NULL)
	     {
		  len = append_str (pHdr, "; ");
		  pHdr += len;
		  len = append_str (pHdr, pMultiPart->content_disp_params);
		  pHdr += len;
	     }

	     len = append_str (pHdr, "\r\n");
	     pHdr += len;

	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* content-Description */
	if (pMultiPart->content_description != NULL)
        {
             len = append_str (pHdr, "Content-Description: ");
             pHdr += len;
             len = append_str (pHdr, pMultiPart->content_description);
             pHdr += len;
	     len = append_str (pHdr, "\r\n");
	     pHdr += len;
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* Should I handle the extra-header? Multi-parts should not have these! */
	/* Write-out all the additional headers if any */
	if (pMultiPart->extra_headers != NULL)
        {
		for (pEHdr = pMultiPart->extra_headers; 
		     pEHdr != NULL;
		     /*pEHdr != NULL, pEHdr->name != NULL, pEHdr->value != NULL; */
		     pEHdr = pEHdr->next)
		{
		     if (pEHdr->name != NULL && pEHdr->value != NULL)
		     {
             	     	len = append_str (pHdr, pEHdr->name);
                     	pHdr += len;
             	     	len = append_str (pHdr, ": ");
                     	pHdr += len;
             	     	len = append_str (pHdr, pEHdr->value);
                     	pHdr += len;
             	     	len = append_str (pHdr, "\r\n");
                     	pHdr += len;

		     	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		     	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     	     	memset (hdrbuf, 0, MIME_BUFSIZE);
		     }

	             pHdr = hdrbuf;

		} /* for */
	} /* end extra hdrs */

	if (fNeedPreamble == TRUE && pMultiPart->encoding_type >= MIME_ENCODING_7BIT)
	{
		/* write out any content-encoding */
		fwriteit = 0;
		switch (pMultiPart->encoding_type)
		{
			case MIME_ENCODING_7BIT:
	             	     len = append_str (pHdr, "Content-Transfer-Encoding: 7bit\r\n");
	                     pHdr += len;
			     fwriteit = 1;
			     break;
			case MIME_ENCODING_8BIT:
	             	     len = append_str (pHdr, "Content-Transfer-Encoding: 8bit\r\n");
	                     pHdr += len;
			     fwriteit = 1;
			     break;
			case MIME_ENCODING_BINARY:
	             	     len = append_str (pHdr, "Content-Transfer-Encoding: binary\r\n");
	                     pHdr += len;
			     fwriteit = 1;
			     break;
		} /* switch */
	
		if (fwriteit)
		{
			pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
			memset (hdrbuf, 0, MIME_BUFSIZE);
			pHdr = hdrbuf;
		}
	}

	/* extra line after headers */
        len = append_str (pHdr, "\r\n");
        pHdr += len;
	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	if (pMultiPart->preamble != NULL)
	{
		/* write it out */
        	len = append_str (pHdr, pMultiPart->preamble);
        	pHdr += len;
		pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
		memset (hdrbuf, 0, MIME_BUFSIZE);
		pHdr = hdrbuf;
	}
	/* Need to conditionally output Pre-amble here ! */
	else if (fNeedPreamble == TRUE && pMultiPartInternal->fParsedPart == FALSE)
	{
	     fNeedPreamble = FALSE;

             len = append_str (pHdr, "\r\nThis is a multi-part message in MIME format\r\n");
             pHdr += len;
		
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
	}

	/* make sure boundary starts on a new line + put the extra -- in the beginning */
        /*mime_output_boundary (pBoundary, pOutput_stream);*/

	/* For each part in bodyParts, output boundary and call putByteStream passing OS */

	for (idx = 1; idx <= bpCount; idx++)
	{
	     pPartInfo = &pMultiPartInternal->partInfo [idx];

	     switch (pPartInfo->nContentType)
	     {
		     case MIME_CONTENT_TEXT:
                     case MIME_CONTENT_AUDIO:
                     case MIME_CONTENT_IMAGE:
                     case MIME_CONTENT_VIDEO:
                     case MIME_CONTENT_APPLICATION:
			  mime_output_boundary (pBoundary, pOutput_stream);
			  pBasicPart2 = (mime_basicPart_t *) pPartInfo->pThePart;
			  mime_basicPart_putByteStream (pBasicPart2, pOutput_stream);
			  break;
                     case MIME_CONTENT_MULTIPART:
			  mime_output_boundary (pBoundary, pOutput_stream);
			  pMultiPart2 = (mime_multiPart_t *) pPartInfo->pThePart;
			  mime_multiPart_putByteStream (pMultiPart2, pOutput_stream);
			  break;
                     case MIME_CONTENT_MESSAGEPART:
			  mime_output_boundary (pBoundary, pOutput_stream);
			  pMessagePart2 = (mime_messagePart_t *) pPartInfo->pThePart;
			  mime_messagePart_putByteStream (pMessagePart2, pOutput_stream);
			  break;

					 default:
			  free (pBoundary);
			  return MIME_ERR_INVALID_CONTENTTYPE;
	     } /* switch */
	}

	/* Trailing boundary. */
	mime_output_trail_boundary (pBoundary, pOutput_stream);
 	free (pBoundary);

	return MIME_OK;
}



/*
* Frees up all parts of the multi-part including internal structures.
*
* parameter :
*
*	pMultiPart : multipart
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_free_all (mime_multiPart_t * pMultiPart)
{
	if ( pMultiPart == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_multiPart_free( pMultiPart );
}



/*=========================== MESSAGE PART =================================*/



/*
* creates a message part from an input stream. 
*
* parameter :
*
*	pInput_stream : input stream
*
* returns :	MIME_OK if successful
*/
/*
int mime_messagePart_create (nsmail_inputstream_t *pInput_stream, 
                             mime_messagePart_t ** ppMessagePart)
{
	if ( pInput_stream == NULL || ppMessagePart == NULL )
	{
		errorLog( "mime_multiPart_create()", MIME_ERR_INVALIDPARAM );
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_messagePart_decode( pInput_stream, ppMessagePart);
}
*/



/*
* Prasad, jan 8,98 
* creates a message part from a message structure
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_fromMessage (mime_message_t  * pMessage, 
                                  mime_messagePart_t ** ppMessagePart)
{
	mime_messagePart_t * pNewMessagePart=NULL;
	mime_messagePart_internal_t * pInternal=NULL;

	if (pMessage == NULL || ppMessagePart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pNewMessagePart = mime_messagePart_new();

	if (pNewMessagePart == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	pNewMessagePart->content_type = MIME_CONTENT_MESSAGEPART;

	pInternal = (mime_messagePart_internal_t *) pNewMessagePart->pInternal;

	if (pInternal == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	pInternal->pTheMessage = mime_message_clone (pMessage);

	if (pInternal->pTheMessage == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	*ppMessagePart = pNewMessagePart;

	return MIME_OK;
}



/*
* Prasad, jan 8,98 
* Retrieves the message that constitutes the body of the message part 
* from the message part.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_getMessage (mime_messagePart_t * pMessagePart,
		                 mime_message_t ** ppMessage)
{
	mime_message_t *pNewMessage=NULL;
	mime_messagePart_internal_t *pInternal=NULL;

	if (pMessagePart == NULL || ppMessage == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_messagePart_internal_t *) pMessagePart->pInternal;

	if (pInternal == NULL)
	{
		return MIME_ERR_INVALID_MESSAGEPART;
	}

	pNewMessage =  mime_message_clone (pInternal->pTheMessage);

	if (pNewMessage == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	*ppMessage = pNewMessage;

	return MIME_OK;
}



/*
* Prasad, jan 8,98 
* deletes the mime message that is the body of this part 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_deleteMessage (mime_messagePart_t * pMessagePart)
{
	mime_messagePart_internal_t * pInternal=NULL;

	if (pMessagePart == NULL)
	{
		return MIME_ERR_INVALID_MESSAGEPART;
	}

	pInternal = (mime_messagePart_internal_t *) pMessagePart->pInternal;

	if (pInternal == NULL)
	{
		return MIME_ERR_UNINITIALIZED;
	}

	mime_message_free (pInternal->pTheMessage);
	pInternal->pTheMessage = NULL;

	return MIME_OK;
}



/*
* Prasad, jan 8,98 
* Sets the passed message as body of this message-part. 
* It is an error to set message when it is already set.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_setMessage (mime_messagePart_t * pMessagePart,
		                 mime_message_t * pMessage)
{
	mime_messagePart_internal_t * pInternal=NULL;

	if (pMessagePart == NULL)
	{
		return MIME_ERR_INVALID_MESSAGEPART;
	}

	if (pMessage == NULL)
	{
		return MIME_ERR_INVALID_MESSAGE;
	}

	if (pMessagePart->pInternal == NULL)   /* New MessagePart. Allocate internal stuff */
	{
		pMessagePart->pInternal = (void *) mime_messagePart_internal_new();

		if (pMessagePart->pInternal == NULL)
		{
			return MIME_ERR_OUTOFMEMORY;
		}
	}

	pInternal = (mime_messagePart_internal_t *) pMessagePart->pInternal;

	if (pInternal->pTheMessage != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	pInternal->pTheMessage = mime_message_clone (pMessage);

	if (pInternal->pTheMessage == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	return MIME_OK;
}



/*
* Prasad, jan 8,98 
* Writes out byte stream for this part to the output stream 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_putByteStream (mime_messagePart_t * pMessagePart,
			    	    nsmail_outputstream_t *pOutput_stream)
{
	int len=0, ret=0;
	mime_header_t * pEHdr=NULL;
	static char *  hdrbuf=NULL;
	char * pHdr=NULL;
    	mime_encoding_type encoding;
	mime_messagePart_internal_t * pMsgPartInternal=NULL;
	mime_message_t * pMsg=NULL;
	BOOLEAN fExternal_body = FALSE, fMsgPartial = FALSE;
	BOOLEAN fFirstExternalHeader = TRUE;

	if ( pMessagePart == NULL || pOutput_stream == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

#ifdef XP_UNIX
        if ((pMessagePart->content_subtype != NULL) && (strlen (pMessagePart->content_subtype) == 13) &&
	    (strncasecmp(pMessagePart->content_subtype, "external-body", 13) ==0))
		fExternal_body = TRUE;
	else if ((pMessagePart->content_subtype != NULL) && (strlen (pMessagePart->content_subtype) == 7) &&
	         (strncasecmp (pMessagePart->content_subtype, "partial", 7) == 0))
		     fMsgPartial = TRUE;
#else
	if ((pMessagePart->content_subtype != NULL) && (strlen (pMessagePart->content_subtype) == 13) &&
                  bStringEquals (pMessagePart->content_subtype, "external-body") )
                fExternal_body = TRUE;
        else if ((pMessagePart->content_subtype != NULL) && (strlen (pMessagePart->content_subtype) == 7) &&
                  bStringEquals (pMessagePart->content_subtype, "partial") )
                     fMsgPartial = TRUE;
#endif

	pMsgPartInternal = (mime_messagePart_internal_t * ) pMessagePart->pInternal;

	if ((pMsgPartInternal == NULL) && fMsgPartial != TRUE && fExternal_body != TRUE)
	{
		return MIME_ERR_NO_BODY;

	}
	
	if (pMsgPartInternal != NULL && !fMsgPartial && !fExternal_body)
	{
		pMsg  = (mime_message_t *) pMsgPartInternal->pTheMessage;

		if ((pMsg == NULL) && (fMsgPartial != TRUE))
		{
			return MIME_ERR_NO_BODY;
		}
	}


	
	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (MIME_BUFSIZE);
		if (hdrbuf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}

	/* Write out the headers first */
	
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	/* Content-Type */
        len = append_str (pHdr, "Content-Type: message/");
        pHdr += len;

	if (pMessagePart->content_subtype != NULL)
	{
	    len = append_str (pHdr, pMessagePart->content_subtype);
	    pHdr += len;
	}
	else
	{
	     if (pMsg != NULL)
	     {
	         len = append_str (pHdr, "rfc822"); /* default */
	         pHdr += len;
	     }
	     else
		 {
			return MIME_ERR_NO_CONTENT_SUBTYPE;
		 }
	}

	if (pMessagePart->content_type_params != NULL)
	{
	     len = append_str (pHdr, "; ");
	     pHdr += len;
	     len = append_str (pHdr, pMessagePart->content_type_params);
	     pHdr += len;
	}

	len = append_str (pHdr, "\r\n");
	pHdr += len;

	/* write out content-type */
	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	/* contentID */
	if (pMessagePart->contentID != NULL)
        {
             len = append_str (pHdr, "Content-ID: ");
             pHdr += len;
             len = append_str (pHdr, pMessagePart->contentID);
             pHdr += len;
	     len = append_str (pHdr, "\r\n");
	     pHdr += len;
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* contentDisposition */
	if (pMessagePart->content_disposition != MIME_DISPOSITION_UNINITIALIZED && pMessagePart->content_disposition > 0)
        {
             len = append_str (pHdr, "Content-Disposition: ");
             pHdr += len;

	     switch (pMessagePart->content_disposition)
	     {
		  case MIME_DISPOSITION_INLINE:
             	       len = append_str (pHdr, "inline");
                       pHdr += len;
		       break;
		  case MIME_DISPOSITION_ATTACHMENT:
             	       len = append_str (pHdr, "attachment");
                       pHdr += len;
		       break;
	     }
	     
	     if (pMessagePart->content_disp_params != NULL)
	     {
		  len = append_str (pHdr, "; ");
		  pHdr += len;
		  len = append_str (pHdr, pMessagePart->content_disp_params);
		  pHdr += len;
	     }

	     len = append_str (pHdr, "\r\n");
	     pHdr += len;

	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	/* content-Description */
	if (pMessagePart->content_description != NULL)
        {
             len = append_str (pHdr, "Content-Description: ");
             pHdr += len;
             len = append_str (pHdr, pMessagePart->content_description);
             pHdr += len;
	     len = append_str (pHdr, "\r\n");
	     pHdr += len;
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
        }

	if (pMessagePart->encoding_type != MIME_ENCODING_UNINITIALIZED)
	{
	     if (pMessagePart->encoding_type == 0)
		encoding = MIME_ENCODING_7BIT;	
	     else
		encoding = pMessagePart->encoding_type;

	     if ((fMsgPartial == TRUE) || (fExternal_body == TRUE))
		     encoding = MIME_ENCODING_7BIT;	/* As mandated by MIME spec */

	     if ((encoding != MIME_ENCODING_7BIT) &&
	         (encoding != MIME_ENCODING_8BIT) &&
	         (encoding != MIME_ENCODING_BINARY))
	     {
		return MIME_ERR_INVALID_ENCODING;
	     }

	     /* content-transfer-encoding */
	     switch (encoding)
	     {
		case MIME_ENCODING_7BIT:
		     encoding = MIME_ENCODING_7BIT;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: 7bit\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_8BIT:
		     encoding = MIME_ENCODING_8BIT;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: 8bit\r\n");
                     pHdr += len;
		     break;
		case MIME_ENCODING_BINARY:
		     encoding = MIME_ENCODING_BINARY;
             	     len = append_str (pHdr, "Content-Transfer-Encoding: binary\r\n");
                     pHdr += len;
		     break;
		default:
		     return MIME_ERR_INVALID_ENCODING;
	     } /* switch */

	     /* write-out encoding line */
	     /*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	     pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	     memset (hdrbuf, 0, MIME_BUFSIZE);
	     pHdr = hdrbuf;
	}

	/* Write-out all the additional headers if any */
	if (pMessagePart->extra_headers != NULL)
        {
		for (pEHdr = pMessagePart->extra_headers; 
		     pEHdr != NULL;
		     /*pEHdr != NULL, pEHdr->name != NULL, pEHdr->value != NULL;*/
		     pEHdr = pEHdr->next)
		{

		   /*
		    *if (fExternal_body == TRUE)
            	    *	fprintf (stderr, "got an extern header: %s:%s\n", pEHdr->name,pEHdr->value);
		    */

		     if (pEHdr->name != NULL &&  pEHdr->value != NULL)
		     {
             	     	len = append_str (pHdr, pEHdr->name);
                     	pHdr += len;
             	     	len = append_str (pHdr, ": ");
                     	pHdr += len;
             	     	len = append_str (pHdr, pEHdr->value);
                     	pHdr += len;
             	     	len = append_str (pHdr, "\r\n");
                     	pHdr += len;

		     	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		     	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
		     	memset (hdrbuf, 0, MIME_BUFSIZE);
		     }

	             pHdr = hdrbuf;

		} /* for */
	} /* end extra hdrs */

	/* extra line after headers */
        len = append_str (pHdr, "\r\n");
        pHdr += len;
	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
        memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;


	if (fExternal_body != TRUE)
	{
		ret = mime_message_putByteStream (pMsg, pOutput_stream);
		
		if (ret != MIME_OK)
		    return ret;
	}
	else if (pMessagePart->extern_headers != NULL)
	/* Write-out the extern headers if any */
        {
		for (pEHdr = pMessagePart->extern_headers; 
		     pEHdr != NULL;
		     /*pEHdr != NULL, pEHdr->name != NULL, pEHdr->value != NULL;*/ 
		     pEHdr = pEHdr->next)
		{
		     if (pEHdr->name != NULL && pEHdr->value != NULL)
		     {
		     	if (fFirstExternalHeader == TRUE)
		     	{
			    fFirstExternalHeader = FALSE;
             	            len = append_str (pHdr, "\r\n");
                            pHdr += len;
		     	}

             	     	len = append_str (pHdr, pEHdr->name);
                     	pHdr += len;
             	     	len = append_str (pHdr, ": ");
                     	pHdr += len;
             	     	len = append_str (pHdr, pEHdr->value);
                     	pHdr += len;
             	     	len = append_str (pHdr, "\r\n");
                     	pHdr += len;

		     	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		     	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
        	     	memset (hdrbuf, 0, MIME_BUFSIZE);
		     }
	             pHdr = hdrbuf;
		} /* for */
		
	} /* end extern hdrs */

        len = append_str (pHdr, "\r\n");
        pHdr += len;

	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
        memset (hdrbuf, 0, MIME_BUFSIZE);
	pHdr = hdrbuf;

	return MIME_OK;
}



/*
* Frees up all parts of the message-part including internal structures.
*
* parameter :
*
*	pMessagePart : messagePart
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_free_all (mime_messagePart_t * pMessagePart)
{
	if ( pMessagePart == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_messagePart_free( pMessagePart );
}



/*============================== MESSAGE ===================================*/


/* read from inputstream into a mime_message */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
/*
int parseMimeMessage( mime_message_t * pMimeMessage, nsmail_inputstream_t *pInput_stream, BOOLEAN bStart, BOOLEAN bEnd )
{
	struct mime_message_internal *pMimeMessageInternal = NULL;
	mimeParser_t *p=NULL;
	int nRet = MIME_ERR_OUTOFMEMORY;

	if ( pInput_stream == NULL || pMimeMessage == NULL )
	{
		errorLog( "parseMimeMessage()", MIME_ERR_INVALIDPARAM );
		return MIME_ERR_INVALIDPARAM;
	}

	pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

	if ( pMimeMessageInternal != NULL && pMimeMessageInternal->pParser == NULL )
	{
		p = mimeParser_new_internal2( pMimeMessage );
		pMimeMessageInternal->pParser = p;
	}

	else
	{
		p = pMimeMessageInternal->pParser;
		p->pMimeMessage = pMimeMessage;
	}

	if ( p != NULL )
	{
		p->bDeleteMimeMessage = FALSE;
		nRet = mimeParser_parseMimeMessage( p, pInput_stream, NULL, 0, MIME_PARSE_ALL, NULL );
	}

	if ( bEnd )
		mimeParser_free(p);

	return nRet;
}
*/

/* Creates a message given text-data and a file to attach.    */
/* The file-name supplied must be fully-qualified file-name.  */
/* If either in_pText or in_pFileFullName are NULL creates a  */
/* MIME message with the non-NULL one. It is an error to pass */
/* NULL for both text and file-name parameters.               */
/* If pref_file_encoding  > 0 attempts to use the encoding    */
/* for the file-attachment. If encoding is not valid for the  */ 
/* file-type overrides with the correct value.                */
/* Returns MIME_OK on success and an error code otherwise.    */
/*                                                            */
/* Copies the data in pText. User responsible for free'ing    */
/* pText up on return as needed. pFileFullName can also	      */
/* be free'd up on return.                                    */
int mime_message_create (char *            pText,
                         char *            pFileFullName,
			 mime_encoding_type pref_file_encoding,
                         mime_message_t ** ppMessage)
{
	nsmail_inputstream_t * pInput_stream = NULL;	
	mime_multiPart_t * pMultiPart=NULL;
	mime_basicPart_t * pTextPart = NULL, * pFilePart = NULL;
	mime_message_t   * pMessage = NULL;
	file_mime_type     fmt;
	char * extn = NULL, *short_file_name = NULL;
	int ret = 0, idx = 0;
	BOOLEAN fMultiPart = FALSE;
	

	if (pText == NULL && pFileFullName == NULL)
  		return MIME_ERR_INVALIDPARAM;

	if (pText != NULL && pFileFullName != NULL)
	{
		outmsg("fMultiPart is TRUE");

		fMultiPart = TRUE;
	}

	if (pText != NULL)
	{
		outmsg("Creating Text Part");

		pTextPart = (mime_basicPart_t *) malloc (sizeof (mime_basicPart_t));

		if (pTextPart == NULL)
			return MIME_ERR_OUTOFMEMORY;
		else   
			memset (pTextPart, 0, sizeof (mime_basicPart_t));

		pTextPart->content_type = MIME_CONTENT_TEXT;
		pTextPart->content_subtype = strdup ("plain");
		pTextPart->content_type_params = strdup ("charset=us-ascii");
		pTextPart->encoding_type = MIME_ENCODING_7BIT;

		ret = mime_basicPart_setDataBuf (pTextPart, strlen (pText), pText, TRUE);

		if (ret != MIME_OK)
		{
#ifdef DEBUG
			fprintf(stderr, "mime_basicPart_setDataBuf failed! ret=%d\n", ret);
#endif
			mime_basicPart_free_all (pTextPart);
			free (pTextPart);
			return ret;
		}
	}


	if (pFileFullName != NULL)
	{
		outmsg("Creating File Part");

		extn = getFileExtn (pFileFullName);

		ret = getFileMIMEType (extn, &fmt);

		if (pref_file_encoding > 0 && pref_file_encoding <= MIME_ENCODING_BINARY)
		{
			/*if (fmt.mime_encoding != MIME_ENCODING_BASE64)*/
				fmt.mime_encoding = pref_file_encoding;

		}
	
		/* Create and return a Message from file */
		pFilePart = (mime_basicPart_t *) malloc (sizeof (mime_basicPart_t));

		if (pFilePart == NULL)
		{
			free (extn);
			freeFMT (&fmt);
			freeParts (pTextPart, NULL, NULL, NULL);
			free (pTextPart);
			return MIME_ERR_OUTOFMEMORY;
		}
		else 
			memset (pFilePart, 0, sizeof (mime_basicPart_t));

		ret =  file_instream_create (pFileFullName, &pInput_stream);

		if (ret != MIME_OK)
		{
			free (extn);
			freeFMT (&fmt);
			freeParts (pTextPart, pFilePart, NULL, NULL);
			free (pTextPart);
			free (pFilePart);
			return ret;
		}

		pFilePart->content_type = fmt.content_type;
		pFilePart->content_subtype = fmt.content_subtype;
		pFilePart->content_type_params = fmt.content_params;
		pFilePart->encoding_type = fmt.mime_encoding;
		
		short_file_name = getFileShortName (pFileFullName);
		if (short_file_name != NULL)
			pFilePart->content_description = short_file_name;

		/* Set to FALSE below if performance becomes a factor!!! */
		ret = mime_basicPart_setDataStream (pFilePart, pInput_stream, TRUE);

		if (ret != MIME_OK)
		{
			pInput_stream->close (pInput_stream->rock);
			free (pInput_stream);
			free (extn);
			freeFMT (&fmt);

			freeParts (pTextPart, pFilePart, NULL, NULL);
			free (pTextPart);
			free (pFilePart);
			return ret;
		}
	}

	if (fMultiPart)
	{
		outmsg("Creating Multi Part");

		/* Create and return a multi-part Message */
		pMultiPart = (mime_multiPart_t *) malloc (sizeof (mime_multiPart_t));

		if (pMultiPart == NULL)
			return MIME_ERR_OUTOFMEMORY;
		else 
			memset (pMultiPart, 0, sizeof (mime_multiPart_t));

		pMultiPart->content_subtype = strdup ("mixed");

		outmsg ("adding basic part to multi-part");

		ret = mime_multiPart_addBasicPart (pMultiPart, pTextPart, FALSE, &idx);

#ifdef DEBUG
		fprintf (stderr, "Index assigned = %d\n", idx);
#endif

		if (ret != MIME_OK)
		{
			if (pInput_stream != NULL)
			    pInput_stream->close (pInput_stream->rock);
			free (pInput_stream);
			free (extn);
			freeFMT (&fmt);
			freeParts (pTextPart, pFilePart, pMultiPart, NULL);
			free (pTextPart);
			free (pFilePart);
			free (pMultiPart);
			return ret;
		}

		outmsg ("adding basic part to multi-part");

		ret = mime_multiPart_addBasicPart (pMultiPart, pFilePart, FALSE, &idx);
		
#ifdef DEBUG
		fprintf (stderr, "Index assigned = %d\n", idx);
#endif

		if (ret != MIME_OK)
		{
			if (pInput_stream != NULL)
			    pInput_stream->close (pInput_stream->rock);
			free (pInput_stream);
			free (extn);
			freeFMT (&fmt);
			freeParts (pTextPart, pFilePart, pMultiPart, NULL);
			free (pTextPart);
			free (pFilePart);
			free (pMultiPart);
			return ret;
		}
	} /* multi-part */ 

	outmsg("Creating Message");

	pMessage = (mime_message_t *) malloc (sizeof (mime_message_t));

	if (pMessage == NULL)
	{
		if (pInput_stream != NULL)
		    pInput_stream->close (pInput_stream->rock);
		free (pInput_stream);
		free (extn);
		freeFMT (&fmt);
		freeParts (pTextPart, pFilePart, pMultiPart, NULL);
		free (pTextPart);
		free (pFilePart);
		free (pMultiPart);
		return ret;
	}
	else
		memset (pMessage, 0, sizeof (mime_message_t));

	if (fMultiPart)
	{
		outmsg("Adding Multi Part to message");

		ret = mime_message_addMultiPart (pMessage, pMultiPart, FALSE);
	}
	else if (pText != NULL)
	{
		outmsg("Adding Text Part to message");
		ret = mime_message_addBasicPart (pMessage, pTextPart, FALSE);
	}
	else
	{
		outmsg("Adding Text Part to message");
		ret = mime_message_addBasicPart (pMessage, pFilePart, FALSE);
	}

	if (ret != MIME_OK)
	{
		outmsg("Adding Part to message Failed!");

		if (pInput_stream != NULL)
		    pInput_stream->close (pInput_stream->rock);
		free (pInput_stream);
		free (extn);
		freeFMT (&fmt);
		freeParts (pTextPart, pFilePart, pMultiPart, NULL);
		free (pTextPart);
		free (pFilePart);
		free (pMultiPart);
		free (pMessage);
		return ret;
	}

	outmsg("Adding Part to message Good!");

	*ppMessage = pMessage;


	/* Remove these if perf becomes a problem ! */
	if (pInput_stream != NULL)
	{
		outmsg("closing the inputStream");
	    	pInput_stream->close (pInput_stream->rock);
		free (pInput_stream);
	}

	outmsg("mime_message_create() returning ok!");

	return MIME_OK;

} /* mime_message_create () */

/*
* Prasad, jan 8,98 
* Adds a basic part as the body of the message. 
* Error if Body is already present.             
* Sets content-type accordingly.                
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addBasicPart (mime_message_t * pMessage,
		               mime_basicPart_t * pBasicPart,
			       BOOLEAN fClone)
{
	outmsg("In mime_message_addBasicPart");
	return mime_message_addBasicPart_clonable( pMessage, pBasicPart, fClone);
}

					   

/*
* Prasad, jan 8,98 
* Adds the message part as the body of the message. 
* Error if Body is already present.             
* Sets content-type accordingly.                    
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addMessagePart (mime_message_t * pMessage,
		            	 mime_messagePart_t * pMessagePart,
			         BOOLEAN fClone)
{
	return mime_message_addMessagePart_clonable( pMessage, pMessagePart, fClone);
}



/*
* Prasad, jan 8,98 
* Adds the multipart as the body of the message 
* Error if Body is already present.             
* Sets content-type accordingly.                
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addMultiPart (mime_message_t   * pMessage,
		               mime_multiPart_t * pMultiPart,
			       BOOLEAN fClone)
{
	return mime_message_addMultiPart_clonable( pMessage, pMultiPart, fClone);
}



/*
* Prasad, jan 8,98 
* Deletes the bodyPart that constitutes the body of this message 
*
* parameter :
*
*	pMessage : mimeMessage
*
* returns :	MIME_OK if successful
*/
int mime_message_deleteBody (mime_message_t * pMessage)
{
	mime_message_internal_t *pInternal=NULL;

	if (pMessage == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if (pInternal == NULL)
		return MIME_OK;  /* does not have body */

	if (pInternal->pMimeBasicPart != NULL)
	{
		mime_basicPart_free (pInternal->pMimeBasicPart);
		pInternal->pMimeBasicPart = NULL;
		return MIME_OK;
	}

	if ( pInternal->pMimeMultiPart != NULL )
	{
		mime_multiPart_free (pInternal->pMimeMultiPart);
		pInternal->pMimeMultiPart = NULL;
		return MIME_OK;
	}

	if (pInternal->pMimeMessagePart != NULL)
	{
		mime_messagePart_free( pInternal->pMimeMessagePart );
		pInternal->pMimeMessagePart = NULL;
		return MIME_OK;
	}

	return MIME_OK; /* Never had body */
}



/*
* returns the content_type of this message 
*
* parameter :
*
*	pMessage : mimeMessage
*	content_type : (output) content type
*
* returns :	MIME_OK if successful
*/
int mime_message_getContentType (mime_message_t * pMessage,
			         mime_content_type * content_type)
{
	mime_message_internal_t *pInternal=NULL;

	if (pMessage == NULL || content_type == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if ( pInternal != NULL )
	{
		if ( pInternal->pMimeBasicPart != NULL )
		{
			*content_type = pInternal->pMimeBasicPart->content_type;
			return MIME_OK;
		}
		
		else if ( pInternal->pMimeMultiPart != NULL )
		{
			*content_type = pInternal->pMimeMultiPart->content_type;
			return MIME_OK;
		}

		else if ( pInternal->pMimeMessagePart != NULL )
		{
			*content_type = pInternal->pMimeMessagePart->content_type;
			return MIME_OK;
		}
	}
	
	return MIME_ERR_NO_BODY;
}



/*
* Prasad, jan 8,98 
* returns the content_subtype of this message 
* NOTE: Assumes user has allocated enough space in pSubType
*
* parameter :
*
*	pMessage : mimeMessage
*	pSubType : (output) sub type
*
* returns :	MIME_OK if successful
*/
int mime_message_getContentSubType (mime_message_t * pMessage,
			            char ** ppSubType)
{
	mime_message_internal_t *pInternal=NULL;

	if ( pMessage == NULL || ppSubType == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if (pInternal != NULL)
	{
		if (pInternal->pMimeBasicPart != NULL && 
		    pInternal->pMimeBasicPart->content_subtype != NULL)
		{
			/*strcpy( pSubType, pInternal->pMimeBasicPart->content_subtype );*/
			*ppSubType = strdup (pInternal->pMimeBasicPart->content_subtype);
			return MIME_OK;
		}
		
		else if (pInternal->pMimeMultiPart != NULL && 
			pInternal->pMimeMultiPart->content_subtype != NULL)
		{
			/*strcpy( pSubType, pInternal->pMimeMultiPart->content_subtype );*/
			*ppSubType = strdup (pInternal->pMimeMultiPart->content_subtype);
			return MIME_OK;
		}

		else if (pInternal->pMimeMessagePart != NULL && 
			 pInternal->pMimeMessagePart->content_subtype != NULL)
		{
			/*strcpy( pSubType, pInternal->pMimeMessagePart->content_subtype );*/
			*ppSubType = strdup (pInternal->pMimeMessagePart->content_subtype);
			return MIME_OK;
		}
	}
	else
	{
		return MIME_ERR_NO_BODY;
	}

	return MIME_ERR_NOT_SET;
}



/*
* Prasad, jan 8,98 
* returns the content_type params of this message 
* NOTE: Assumes user has allocated enough space in pParams
*
* parameter :
*
*	pMessage : mimeMessage
*	pParams : (output) content type parameters
*
* returns :	MIME_OK if successful
*/
int mime_message_getContentTypeParams (mime_message_t * pMessage,
			               char **ppParams)
{
	mime_message_internal_t *pInternal=NULL;

	if ( pMessage == NULL || ppParams == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if (pInternal != NULL)
	{
		if (pInternal->pMimeBasicPart != NULL )
		{
			if ( pInternal->pMimeBasicPart->content_type_params != NULL )
			     /*strcpy( pParams, pInternal->pMimeBasicPart->content_type_params );*/
			     *ppParams = strdup (pInternal->pMimeBasicPart->content_type_params );
			else
				*ppParams = NULL;
				/*pParams[0] = NULL;*/

			return MIME_OK;
		}
		else if (pInternal->pMimeMultiPart != NULL )
		{
			if ( pInternal->pMimeMultiPart->content_type_params != NULL )
			      /*strcpy(pParams, pInternal->pMimeMultiPart->content_type_params);*/
			     *ppParams = strdup (pInternal->pMimeMultiPart->content_type_params );
			else
				*ppParams = NULL;
				/*pParams[0] = NULL;*/

			return MIME_OK;
		}
		else if (pInternal->pMimeMessagePart != NULL )
		{
			if ( pInternal->pMimeMessagePart->content_type_params != NULL )
			      /*strcpy(pParams, pInternal->pMimeMessagePart->content_type_params);*/
			     *ppParams = strdup (pInternal->pMimeMessagePart->content_type_params );
			else
				*ppParams = NULL;
				/*pParams[0] = NULL;*/

			return MIME_OK;
		}
	
	}
	else
	{
		return MIME_ERR_NO_BODY;
	}
}

int mime_message_getHeader (mime_message_t * pMessage,
			    char * pName,
			    char **ppValue)
{
	mime_header_t * pRfcHdr = NULL;

	if (pMessage == NULL || pName == NULL || ppValue == NULL)
        {
                return MIME_ERR_INVALIDPARAM;
        }

	if (pMessage->rfc822_headers == NULL)
	{
                return MIME_ERR_NO_HEADERS;
	}

	pRfcHdr = pMessage->rfc822_headers;

	while (pRfcHdr != NULL)
        {
		if (bStringEquals (pName, pRfcHdr->name))
		{
			*ppValue = strdup (pRfcHdr->value);
			return MIME_OK;
		}
		
		pRfcHdr = pRfcHdr->next;

	} /* end while */

	*ppValue = NULL;
	return MIME_ERR_NO_SUCH_HEADER;
}


/*
* Prasad, jan 8,98 
* Returns the bodyPart that constitues the Body of the message.
*
* parameter :
*
*	pMessage : mimeMessage
*	pContentType : (output) body / part type
*	ppTheBodyPart : (output) the message part
*
* returns :	MIME_OK if successful
*/
int mime_message_getBody (mime_message_t * pMessage,
			  mime_content_type * pContentType,
			  void **ppTheBodyPart  /* Client can cast this based on pContentType */ 
			  )
{
	mime_message_internal_t *pInternal=NULL;

	if (pMessage == NULL || pContentType == NULL || ppTheBodyPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if (pInternal != NULL)
	{
		if (pInternal->pMimeBasicPart != NULL)
		{
			*pContentType = pInternal->pMimeBasicPart->content_type;
			*ppTheBodyPart = mime_basicPart_clone (pInternal->pMimeBasicPart);
			return MIME_OK;
		}
		
		else if (pInternal->pMimeMultiPart != NULL)
		{
			*pContentType = MIME_CONTENT_MULTIPART;
			*ppTheBodyPart = mime_multiPart_clone (pInternal->pMimeMultiPart);
			return MIME_OK;
		}

		else if (pInternal->pMimeMessagePart != NULL)
		{
			*pContentType = MIME_CONTENT_MESSAGEPART;
			*ppTheBodyPart = mime_messagePart_clone (pInternal->pMimeMessagePart);
			return MIME_OK;
		}

	}

	return MIME_ERR_NO_BODY;
}



/*
* Prasad, jan 8,98 
* Writes out byte stream for this part to the output stream 
*
* parameter :
*
* returns :	
*/
int mime_message_putByteStream (mime_message_t * pMessage,
			    	nsmail_outputstream_t *pOutput_stream)
{
    	int len=0, ret=0;
 	static char *  hdrbuf = NULL;
 	char  * pHdr = NULL;
	mime_header_t * pRfcHdr = NULL;
	mime_message_internal_t *pInternal = NULL;
	mime_basicPart_t * pBasicPart = NULL;
	mime_multiPart_t * pMultiPart = NULL;
	mime_messagePart_t * pMessagePart = NULL;
	BOOLEAN fWroteVersion = FALSE;

	outmsg("Entered mime_message_putByteStream()");

	if (pMessage == NULL || pOutput_stream == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if (pInternal == NULL)
	{
		return MIME_ERR_NO_BODY;
	}

	if ((pInternal->pMimeBasicPart == NULL) &&
	    (pInternal->pMimeMultiPart == NULL) && 
	    (pInternal->pMimeMessagePart == NULL))
	{
		return MIME_ERR_NO_BODY;
	}

	if (hdrbuf==NULL)
	{
		hdrbuf = (char *) malloc (MIME_BUFSIZE);
		
		if (hdrbuf == NULL)
			return MIME_ERR_NO_BODY;
	}
	
	pRfcHdr = pMessage->rfc822_headers;
	memset (hdrbuf, 0, MIME_BUFSIZE);
        pHdr = hdrbuf;
	
	outmsg("Writing out headers");

	while (pRfcHdr != NULL)
	{
	    if (pRfcHdr->name != NULL && pRfcHdr->value != NULL) /* py 4/03 */
	    {
            	len = append_str (pHdr, pRfcHdr->name);
            	pHdr += len;
            	len = append_str (pHdr, ": ");
            	pHdr += len;
            	len = append_str (pHdr, pRfcHdr->value);
            	pHdr += len;
            	len = append_str (pHdr, "\r\n");
            	pHdr += len;

	    	/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
	    	pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
            	memset (hdrbuf, 0, MIME_BUFSIZE);
	    	pHdr = hdrbuf;
	    
	    	if (!fWroteVersion && bStringEquals (pRfcHdr->name, "MIME-Version"))
	    	 	fWroteVersion = TRUE;

	    }

	    pRfcHdr = pRfcHdr->next;

	} /* while */

	if (fWroteVersion == FALSE)
	{
		outmsg("Writing out MIME Version Hdr");
	
		/* MIME Version. */
	        len = append_str (pHdr, "MIME-Version: 1.0\r\n");
	        pHdr += len;
		/*pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf +1));*/
		pOutput_stream->write (pOutput_stream->rock, hdrbuf, (pHdr - hdrbuf));
	        memset (hdrbuf, 0, MIME_BUFSIZE);
		pHdr = hdrbuf;
	}

	if (pInternal->pMimeBasicPart != NULL)
	{
		outmsg("Its a BasicPart! Invoking mime_basicPart_putByteStream");

	  	pBasicPart = (mime_basicPart_t *) pInternal->pMimeBasicPart;
		return mime_basicPart_putByteStream (pBasicPart, pOutput_stream);
	}
	else if (pInternal->pMimeMultiPart != NULL)
	{
		outmsg("Its a MultiPart! Invoking mime_multiPart_putByteStream");

	  	pMultiPart = (mime_multiPart_t *) pInternal->pMimeMultiPart;
		fNeedPreamble = TRUE;
		ret = mime_multiPart_putByteStream (pMultiPart, pOutput_stream);
		fNeedPreamble = FALSE;
		return (ret);
	}
	else if (pInternal->pMimeMessagePart != NULL)
	{
		outmsg("Its a MessagePart! Invoking mime_messagePart_putByteStream");

	  	pMessagePart = (mime_messagePart_t *) pInternal->pMimeMessagePart;
	  	return mime_messagePart_putByteStream (pMessagePart, pOutput_stream);
	}

	return MIME_ERR_NO_BODY;
}



/*
* Frees up all parts of the message including internal structures. 
*
* parameter :
*
*	pMessage : mimeMessage to free
*
* returns :	MIME_OK if successful
*/
int mime_message_free_all (mime_message_t * pMessage)
{
	if ( pMessage == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return mime_message_free( pMessage );
}



/*
int mime_message_setCallback( mime_message_t * pMimeMessage, mimeDataSink_t *pDataSink )
{
	mime_message_internal_t *pMimeMessageInternal = NULL;

	if ( pMimeMessage == NULL || pDataSink == NULL )
		return MIME_ERR_INVALIDPARAM;

	pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

	if ( pMimeMessageInternal != NULL )
		pMimeMessageInternal->pDataSink = pDataSink;

	return MIME_OK;
}
*/



/*************************************************************************/
/**                      UTILITY ROUTINES                                */
/*************************************************************************/
 


/*
* Prasad, jan 8,98 
* Base64 encodes the data from input_stream and writes to output_stream.
* Returns the number of bytes written to the output_stream or an 
* error-code (< 0) in case of an error.
*
* parameter :
*
* returns :	
*/
int mime_encodeBase64 (nsmail_inputstream_t * pInput_stream,
                       nsmail_outputstream_t * pOutput_stream)
{
	char buf[3], * pOutbuf=NULL;
	static char * outbuf=NULL;
	char a, b, c;
	int linelen = 0, written = 0, bytes_read = 0, len=0;
	

	bytes_read = pInput_stream->read (pInput_stream->rock, buf, 3);

	if (bytes_read <= 0)
	{
		return MIME_ERR_NO_DATA;
	}

	if (outbuf == NULL)
	{
		outbuf = (char *) malloc (80);

		if (outbuf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}

	memset (outbuf, 0, 80);
	pOutbuf = outbuf;

	while (bytes_read > 0)
	{
		if (bytes_read <= 0)
			break;

		if (bytes_read == 3)
		{
			a = buf[0];
                        b = buf[1];
                        c = buf[2];

			*pOutbuf++ = base64map[(a >> 2) & 0x3F];
			*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
			*pOutbuf++ = base64map[((b << 2) & 0x3c) | ((c >> 6) & 0x3)],
			*pOutbuf++ = base64map[c & 0x3F],

			linelen += 4;
			written += 4;
		}
		else if (bytes_read == 2)
		{
			a = buf[0];
                        b = buf[1];
                        c = 0;

			*pOutbuf++ = base64map[(a >> 2) & 0x3F];
			*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
			*pOutbuf++ = base64map[((b << 2) & 0x3c) | ((c >> 6) & 0x3)],
			*pOutbuf++ = '=';

			linelen += 4;
			written += 4;
		}
		else if (bytes_read == 1)
		{
			a = buf[0];
                        b = 0;
                        c = 0;

			*pOutbuf++ = base64map[(a >> 2) & 0x3F];
			*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
			*pOutbuf++ = '=';
			*pOutbuf++ = '=';

			linelen += 4;
			written += 4;
		}

		if (linelen > 71)
		{
			len = append_str (pOutbuf, "\r\n");
			linelen += len;
			written += len;
			pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
			memset (outbuf, 0, 80);
			pOutbuf = outbuf;
			linelen = 0;
		}

		bytes_read = pInput_stream->read (pInput_stream->rock, buf, 3);
		
	} /* while */

	if (linelen > 0)
	{
		len = append_str (pOutbuf, "\r\n");
		linelen += len;
		written += len;
		pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
	}

	return written;
}


 
/*
* Prasad, jan 8,98 
* QuotedPrintable encodes the data from input_stream and writes to output_stream 
*
* parameter :
*
* returns :	
*/
int mime_encodeQP (nsmail_inputstream_t * pInput_stream,
                   nsmail_outputstream_t  * pOutput_stream)
{
	static char CR  = '\r';
	static char LF  = '\n';
	static char HT  = '\t';
	static char  * outbuf=NULL, * buf=NULL;
	char  * pOutbuf=NULL;
	unsigned char current, previous, tmp;
	int i=0, len=0, read_len=0, linelen=0, written=0, lastspace=0;
	int nullCount=0, idx;

	if (buf == NULL)
	{
		buf = (char *) malloc (MIME_BUFSIZE);

		if (buf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}

	if (outbuf == NULL)
	{
		outbuf = (char *) malloc (80);

		if (outbuf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}


	memset (buf, 0, MIME_BUFSIZE);
	read_len = pInput_stream->read (pInput_stream->rock, buf, MIME_BUFSIZE);

	memset (outbuf, 0, 80);
	pOutbuf = outbuf;

	while (read_len > 0)
	{

		for (i=0; i < read_len; i++)
		{
			current = buf[i];

			if (current == 0x00)
			{
				nullCount++;
				previous = current;
				lastspace = 0;
				continue;
			}
			else if (nullCount > 0)
			{
				/* write out all nulls and fall through to process current char.*/
				for (idx = 1; idx <= nullCount ; idx++)
                                {
					tmp = (unsigned char)0x00;
					*pOutbuf++ = '=';
					*pOutbuf++ = hexmap [(tmp >> 4)];
					*pOutbuf++ = hexmap [(tmp & 0xF)];
					linelen += 3;
					written += 3;
 
                                        if (linelen > 74)
                                        {
					    len = append_str (pOutbuf, "=\r\n");
					    pOutbuf += len;
					    linelen += len;

					    pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
					    memset (outbuf, 0, 80);
					    pOutbuf = outbuf;
					    written += len;
					    linelen = 0;
                                        }
                                }
 
				previous = 0;
                                nullCount = 0;
			}

			if ((current > ' ') && (current < 0x7F) && (current != '='))
			{
				/* Printable chars */
				*pOutbuf++ = current;
				linelen += 1;
				written += 1;
				lastspace = 0;
			        previous = current;
			}
			else if ((current == ' ') || (current == HT))
			{
				*pOutbuf++ = current;
				linelen += 1;
				written += 1;
				lastspace = 1;
			        previous = current;
			}
			else if ((current == LF) && (previous == CR))
			{
				/* handled this already. Ignore. */
			        previous = 0;
			}
			else if ((current == CR ) || (current == LF))
			{
				/* Need to emit a soft line break if last char was SPACE/HT or
				   if we have a period on a line by itself. */
				if ((lastspace == 1) || ((previous == '.') && (linelen == 1)))
				{
					len = append_str (pOutbuf, "=\r\n");
					pOutbuf += len;
					linelen += len;
					written += len;
					/*written += 3;*/
				}

				len = append_str (pOutbuf, "\r\n");
				pOutbuf += len;
				linelen += len;
				written += len;
				
				pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
				memset (outbuf, 0, 80);
				pOutbuf = outbuf;

				lastspace = 0;
				linelen = 0;
			        previous = current;
			}
			else if ( (current < ' ') || (current == '=') || (current >= 0x7F) )
			{
				/* Special Chars */
				*pOutbuf++ = '=';
				*pOutbuf++ = hexmap [(current >> 4)];
				*pOutbuf++ = hexmap [(current & 0xF)];
				lastspace = 0;
				linelen += 3;
				written += 3;
				previous = current;
			}
			else
			{
				*pOutbuf++ = current;
				lastspace = 0;
				linelen += 1;
				written += 1;
				previous = current;
			}

			if (linelen > 74)
			{
				len = append_str (pOutbuf, "=\r\n");
				pOutbuf += len;
				linelen += len;

				pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
				memset (outbuf, 0, 80);
				pOutbuf = outbuf;
				written += len;

				linelen = 0;
				previous = 0;
			}
		} /* for */
		
		memset (buf, 0, MIME_BUFSIZE);
		read_len = pInput_stream->read (pInput_stream->rock, buf, MIME_BUFSIZE);

	} /* while */
	
	if (linelen > 0)
	{
		len = append_str (pOutbuf, "\r\n");
		linelen += len;
		written += len;
		pOutput_stream->write (pOutput_stream->rock, outbuf, linelen);
	}

	return written;
}



/*
* Prasad, jan 8,98 
* Q encodes the data from input_stream and writes to output_stream 
*
* parameter :
*
* returns :	
*/
int mime_encodeQ (nsmail_inputstream_t * pInput_stream,
                   nsmail_outputstream_t  * pOutput_stream)
{
	static char  * outbuf=NULL, * buf=NULL;
	char  * pOutbuf=NULL, current;
	int i=0, read_len=0, written=0;
	BOOLEAN encode = FALSE;


	if (buf == NULL)
	{
		buf = (char *) malloc (MIME_BUFSIZE);

		if (buf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}

	if (outbuf == NULL)
	{
		outbuf = (char *) malloc (80);

		if (outbuf == NULL)
			return MIME_ERR_OUTOFMEMORY;
	}

	memset (buf, 0, MIME_BUFSIZE);
	read_len = pInput_stream->read (pInput_stream->rock, buf, MIME_BUFSIZE);

	memset (outbuf, 0, 80);
	pOutbuf = outbuf;

	while (read_len > 0)
	{

		for (i=0; i < read_len; i++)
		{
			current = buf[i];

			if ((current >= 0x30 && current <= 0x39) ||      /* 0-9 */
                            (current >= 0x41 && current <= 0x5A) ||      /* A-Z */
                            (current >= 0x61 && current <= 0x7A) ||      /* a-z */
                            (current == 0x21 || current == 0x2A) ||      /* !* */
                            (current == 0x2B || current == 0x2D) ||      /* +- */
                            (current == 0x2F || current == 0x5F))        /* /_ */
                        {
                                encode = FALSE;
                        }
                        else
                                encode = TRUE;

		        if (encode == TRUE)
                        {
                                /* Special Chars */
				*pOutbuf++ = '=';
				*pOutbuf++ = hexmap [(current >> 4)];
				*pOutbuf++ = hexmap [(current & 0xF)];
				pOutput_stream->write (pOutput_stream->rock, outbuf, 3);
				memset (outbuf, 0, 80);
				pOutbuf = outbuf;
                                written += 3;
                        }
                        else
                        {
				buf[0] = current;
				pOutput_stream->write (pOutput_stream->rock, buf, 1);
                                written += 1;
                        }
		} /* for */

	} /* while */

	return written;
}



/*
* Prasad, jan 8,98 
* Q encodes the data from inBuf and write to outBuf.
* Caller responsible for both inBuf and outBuf.
*
* parameter :
*
* returns :	
*/
int mime_bufEncodeQ (char  * pInBuf,
                     char ** ppOutBuf)
{
	char current, * pOutbuf;
	int i, in_len=0, written=0;
	BOOLEAN encode = FALSE;

	if (pInBuf == NULL || ppOutBuf == NULL)
	{
		return -1;
	}

	in_len = strlen (pInBuf);

	pOutbuf = *ppOutBuf;

	if (in_len > 0)
	{

		for (i=0; i < in_len; i++)
		{
			current = *pInBuf++;

			if ((current >= 0x30 && current <= 0x39) ||      /* 0-9 */
                            (current >= 0x41 && current <= 0x5A) ||      /* A-Z */
                            (current >= 0x61 && current <= 0x7A) ||      /* a-z */
                            (current == 0x21 || current == 0x2A) ||      /* !* */
                            (current == 0x2B || current == 0x2D) ||      /* +- */
                            (current == 0x2F || current == 0x5F))        /* /_ */
                        {
                                encode = FALSE;
                        }
                        else
                                encode = TRUE;

		        if (encode == TRUE)
                        {
                                /* Special Chars */
				*pOutbuf++ = '=';
				*pOutbuf++ = hexmap [(current >> 4)];
				*pOutbuf++ = hexmap [(current & 0xF)];
                                written += 3;
                        }
                        else
                        {
				*pOutbuf++ = current;
                                written += 1;
                        }
		} /* for */

	} /* if */
	else
		return -1;

	return written;
}


 
/*
* Prasad, jan 8,98 
* base64 encodes the data from inBuf and write to outBuf.
* Caller responsible for both inBuf and outBuf.
* It is expected that data in pInBuf <= 71 chars
*
* parameter :
*
* returns :	
*/
int mime_bufEncodeB64 (char  * pInBuf,
                       char ** ppOutBuf)
{
	char buf[3], * pOutbuf;
	char a, b, c;
	int in_len = 0, enclen = 0;
	
	in_len =  strlen (pInBuf);

	if (in_len <= 0)
	{
		return MIME_ERR_NO_DATA;
	}

	if (in_len >= 71)
	{
		return MIME_ERR_ENCODE;
	}

	pOutbuf = * ppOutBuf;

	while (in_len >= 3)
	{
		strncpy (buf, pInBuf, 3); 
		pInBuf += 3;
		in_len -= 3;
	     
		a = buf[0];
		b = buf[1];
		c = buf[2];

		*pOutbuf++ = base64map[(a >> 2) & 0x3F];
		*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
		*pOutbuf++ = base64map[((b << 2) & 0x3c) | ((c >> 6) & 0x3)],
		*pOutbuf++ = base64map[c & 0x3F],

		enclen += 4;

	} /* while */

	if (in_len == 2)
	{
		strncpy (buf, pInBuf, 2); 
		pInBuf += 3;
		a = buf[0];
                b = buf[1];
                c = 0;

		*pOutbuf++ = base64map[(a >> 2) & 0x3F];
		*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
		*pOutbuf++ = base64map[((b << 2) & 0x3c) | ((c >> 6) & 0x3)],
		*pOutbuf++ = '=';

		enclen += 4;
	}
	else if (in_len == 1)
	{
		strncpy (buf, pInBuf, 1); 
		a = buf[0];
                b = 0;
                c = 0;

		*pOutbuf++ = base64map[(a >> 2) & 0x3F];
		*pOutbuf++ = base64map[((a << 4) & 0x30) | ((b >> 4) & 0xf)],
		*pOutbuf++ = '=';
		*pOutbuf++ = '=';

		enclen += 4;
	}

	return enclen;
}



/*
* Prasad, jan 8,98 
* Encode a header string per RFC 2047. The encoded string can be used as the
* value of unstructured headers or in the comments section of structured headers.
* Below, encoding must be one of  'B' or 'Q'.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_encodeHeaderString (char *  inString,
                             char *  pCharset,          /* charset the input string is in */
                             char encoding,
                             char ** ppOutString)
{
	int i, l_encoding = -1, enclen, csetlen;
	char * pBuf, buf [1024], * pOutString;

	if (inString == NULL || pCharset == NULL || ppOutString == NULL || *ppOutString == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	csetlen = strlen (pCharset);

	if (csetlen <= 0)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (encoding == 'B' || encoding == 'b')
                     l_encoding = 1;
        else if (encoding == 'Q' || encoding == 'q')
                     l_encoding = 2;
        else
		{
			return MIME_ERR_INVALIDPARAM;
		}

	pOutString = *ppOutString;
	pBuf = buf;
	

	switch (encoding)
	{
		case 1: 	/* Base64 */
		     enclen = mime_bufEncodeB64 (inString, &pBuf);
		     break;
		case 2: 	/* Q */
		     enclen = mime_bufEncodeQ (inString, &pBuf);
		     break;
	}

	if (enclen <= 0)
		MIME_ERR_ENCODE;

	*pOutString++ = '=';
	*pOutString++ = '?';

	for (i = 0; i < csetlen; i++)
		*pOutString++ = *pCharset++;

	*pOutString++ = '?';
	if (encoding == 1)
		*pOutString++ = 'B';
	else
		*pOutString++ = 'Q';
	*pOutString++ = '?';

	for (i = 0; i < enclen; i++)
		*pOutString++ = buf[i]; /* encoded stuff */

	*pOutString++ = '?';
	*pOutString++ = '=';

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* If the input string is not encoded (per RFC 2047), returns the same.
*   Otherwise, decodes and returns the decoded string 
*
* =?  charset  ?  encoding(q/b)  ?  encoded text  ?=
*
* parameter :
*
*	inString : input string
*	outString : (output) decoded header string
*
* returns :	MIME_OK if successful
*/

int mime_decodeHeaderString (char *  inString,
                             char ** outString)
{

	if ( inString == NULL || outString == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	*outString = decodeHeader (inString);

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* Base64 decodes the data from input_stream and writes to output_stream
*
* parameter :
*
*	pInput_stream : inputstream
*	pOutput_stream : (output) output stream
*
* returns :	MIME_OK if successful
*/
int mime_decodeBase64 (nsmail_inputstream_t * pInput_stream,
                       nsmail_outputstream_t  * pOutput_stream)
{
	char achReadBuffer[BUFFER_SIZE];
	char achWriteBuffer[BUFFER_SIZE];
	int nBytesRead = 0;
	int nBytesWrite = 0;
	int out_byte = 0;
	int out_bits = 0;

	if ( pInput_stream == NULL || pOutput_stream == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}
	 
	for ( nBytesRead = pInput_stream->read( pInput_stream->rock, achReadBuffer, BUFFER_SIZE );
		nBytesRead > 0;
		nBytesRead = pInput_stream->read( pInput_stream->rock, achReadBuffer, BUFFER_SIZE ) )
	{
		nBytesWrite = decodeBase64( achReadBuffer, achWriteBuffer, nBytesRead, BUFFER_SIZE, &out_byte, &out_bits );

		if ( nBytesWrite > 0 )
			pOutput_stream->write( pOutput_stream->rock, achWriteBuffer, nBytesWrite );
	}

	return MIME_OK;
}


 
/*
* carsonl, jan 8,98 
* QuotedPrintable decodes the data from input_stream and writes to output_stream
*
* parameter :
*
*	pInput_stream : inputstream
*	pOutput_stream : (output) output stream
*
* returns :	MIME_OK if successful
*/
int mime_decodeQP (nsmail_inputstream_t * pInput_stream,
                   nsmail_outputstream_t * pOutput_stream)
{
	char achReadBuffer[BUFFER_SIZE];
	char achWriteBuffer[BUFFER_SIZE];
	int nBytesRead = 0;
	int nBytesWrite = 0;
	int i = 0;
	int j = 0;
	char ch;

	if ( pInput_stream == NULL || pOutput_stream == NULL )
	{
		return MIME_ERR_INVALIDPARAM;;
	}

	for ( nBytesRead = pInput_stream->read( pInput_stream->rock, achReadBuffer, BUFFER_SIZE );
		nBytesRead > 0;
		nBytesRead = pInput_stream->read( pInput_stream->rock, achReadBuffer, BUFFER_SIZE ) )
	{
		for (	i = 0, j = 0, ch = achReadBuffer[i];
				ch != -1 && i < nBytesRead;
				i++, ch = achReadBuffer[i] )
		{
			if ( ( ch >= 33 && ch <= 60 ) || ( ch >= 62 && ch <= 126 ) || ( ch == 9 || ch == 32 ) )
			{
				achWriteBuffer[j++] = ch;
			}

			else if ( ch == '=' )
			{
				achWriteBuffer[j++] = nConvertHexToDec( achReadBuffer[++i], achReadBuffer[++i] );
			}
		}

		pOutput_stream->write( pOutput_stream->rock, achWriteBuffer, j );
		nBytesWrite += j;
	}

	return MIME_OK;
}


/* Returns file's MIME type info based on file's extension. */
/* By default (or if filename_ext is null) returns application/octet-stream. */
int getFileMIMEType (char * filename_ext, file_mime_type * pFmt)
{
	if (pFmt == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (filename_ext == NULL)
	{
		
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("octet-stream");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	} else
	if (equalsIgnoreCase (filename_ext, "txt") || equalsIgnoreCase (filename_ext, "java") ||
	    equalsIgnoreCase (filename_ext, "c")   || equalsIgnoreCase (filename_ext, "C")    || 
            equalsIgnoreCase (filename_ext, "cc")  || equalsIgnoreCase (filename_ext, "CC")   ||
	    equalsIgnoreCase (filename_ext, "h")   || equalsIgnoreCase (filename_ext, "hxx")  || 
            equalsIgnoreCase (filename_ext, "bat") || equalsIgnoreCase (filename_ext, "rc")   || 
	    equalsIgnoreCase (filename_ext, "ini") ||  equalsIgnoreCase (filename_ext, "cmd") ||
	    equalsIgnoreCase (filename_ext, "awk") || equalsIgnoreCase (filename_ext, "html") ||
	    equalsIgnoreCase (filename_ext, "sh")  || equalsIgnoreCase (filename_ext, "ksh")  ||
	    equalsIgnoreCase (filename_ext, "pl")  ||  equalsIgnoreCase (filename_ext, "DIC") ||
	    equalsIgnoreCase (filename_ext, "EXC") ||  equalsIgnoreCase (filename_ext, "LOG") ||
	    equalsIgnoreCase (filename_ext, "SCP") ||  equalsIgnoreCase (filename_ext, "WT") ||
	    equalsIgnoreCase (filename_ext, "mk")  || equalsIgnoreCase (filename_ext, "htm"))  
        {
		pFmt->content_type = MIME_CONTENT_TEXT;
		if (equalsIgnoreCase (filename_ext, "html") || 
		    equalsIgnoreCase (filename_ext, "htm"))
			pFmt->content_subtype = strdup ("html");
		else
			pFmt->content_subtype = strdup ("plain");
		pFmt->content_params = strdup ("us-ascii");
		pFmt->mime_encoding = MIME_ENCODING_7BIT;
	}
	else if (equalsIgnoreCase (filename_ext, "pdf")) 
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("pdf");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "AIF") || 
		 equalsIgnoreCase (filename_ext, "AIFC") ||
		 equalsIgnoreCase (filename_ext, "AIFF"))
	{
		pFmt->content_type = MIME_CONTENT_AUDIO;
		pFmt->content_subtype = strdup ("aiff");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "AU") || 
		 equalsIgnoreCase (filename_ext, "SND"))
	{
		pFmt->content_type = MIME_CONTENT_AUDIO;
		pFmt->content_subtype = strdup ("basic");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "WAV"))
	{
		pFmt->content_type = MIME_CONTENT_AUDIO;
		pFmt->content_subtype = strdup ("wav");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "gif") )
	{
		pFmt->content_type = MIME_CONTENT_IMAGE;
		pFmt->content_subtype = strdup ("gif");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "jpg") )
	{
		pFmt->content_type = MIME_CONTENT_IMAGE;
		pFmt->content_subtype = strdup ("jpeg");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "jpeg") )
	{
		pFmt->content_type = MIME_CONTENT_IMAGE;
		pFmt->content_subtype = strdup ("jpeg");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "tif") )
	{
		pFmt->content_type = MIME_CONTENT_IMAGE;
		pFmt->content_subtype = strdup ("tiff");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "XBM") )
	{
		pFmt->content_type = MIME_CONTENT_IMAGE;
		pFmt->content_subtype = strdup ("x-xbitmap");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "avi") )
	{
		pFmt->content_type = MIME_CONTENT_VIDEO;
		pFmt->content_subtype = strdup ("avi");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "mpeg") )
	{
		pFmt->content_type = MIME_CONTENT_VIDEO;
		pFmt->content_subtype = strdup ("mpeg");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "ps") || 
		 equalsIgnoreCase (filename_ext, "EPS"))
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("postscript");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "tar") )
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("x-tar");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "zip") )
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("zip");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "js") )
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("x-javascript");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "doc") )
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("msword");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "nsc") )
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("x-conference");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else if (equalsIgnoreCase (filename_ext, "ARC") || 
		 equalsIgnoreCase (filename_ext, "ARJ") ||
		 equalsIgnoreCase (filename_ext, "B64") || 
		 equalsIgnoreCase (filename_ext, "BHX") ||
		 equalsIgnoreCase (filename_ext, "GZ") || 
		 equalsIgnoreCase (filename_ext, "HQX"))
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("x-gzip");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}
	else
	{
		pFmt->content_type = MIME_CONTENT_APPLICATION;
		pFmt->content_subtype = strdup ("octet-stream");
		pFmt->content_params = null;
		pFmt->mime_encoding = MIME_ENCODING_BASE64;
	}

	return MIME_OK;
}

void * mime_malloc (unsigned int size)
{
	return (void *) malloc (size);
}

void mime_memfree (void * pMem)
{
	free (pMem);
}
