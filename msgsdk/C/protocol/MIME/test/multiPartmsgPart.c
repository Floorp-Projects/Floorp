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

/* This example demonstrates use of MIME C-API to create message with multi-parts 
 * and message-parts and nesting of parts.
 *
 * This program creates a multi-part message that contains a message-part and a text part.
 * The message-part itself contains a multi-part message.
 */

#include <stdio.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <string.h>

#include "mime.h"
#include "nsStream.h"

#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))

#ifdef XP_UNIX
#define TMPDIR "/tmp/"
#else
#define TMPDIR "C:\\Temp\\"
#endif

static char * getFileExtn (char * fileName)
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

BOOLEAN fStrEqual (char *s1, char *s2)
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

main (int argc, char *argv[])
{
	char filename [1024];
	file_mime_type     fmt;
	char *pTextBuf, *pEnc, * extn = NULL;
	mime_message_t 		*pMsg = NULL, *pInnerMsg = NULL;
	mime_messagePart_t 	*pMsgPart = NULL;
	mime_multiPart_t 	*pMultiPart = NULL;
	mime_basicPart_t 	*pFilePart = NULL;
	nsmail_outputstream_t 	*pOS;
	nsmail_inputstream_t 	*pInput_stream;
	mime_encoding_type enc = 0;
	mime_header_t  * pRFC822_hdrs, * pHdr, *pTmpHdr;
	int ret, part_index;

	if (argc < 2)
	{
		fprintf (stderr, "Usage: %s <filename> [encoding]\n", argv[0]);
		exit (1);
	}

	if (argc > 2)
	{
		pEnc = argv[2];
		
		if (fStrEqual (pEnc, "B"))	
		{
			enc = MIME_ENCODING_BASE64;
		}
		else if (fStrEqual (pEnc, "Q"))
		{
			enc = MIME_ENCODING_QP;
		}
	}

	fprintf (stderr, "FileName = %s\n", argv[1]);

	/*ret = mime_message_create (pTextBuf, NULL,  &pMsg); */
	/*ret = mime_message_create (NULL, argv[1],  &pMsg);*/
	/*ret = mime_message_create (pTextBuf, argv[1],  enc, &pMsg);*/
	pTextBuf = (char *) mime_malloc (80);
	sprintf (pTextBuf, "Hello this is a simple text");

	/* create a new (multi-part) message with the above text and specified file */
	ret = mime_message_create (pTextBuf, argv[1],  enc, &pInnerMsg);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_create() on Inner Msg failed! Err= %d\n", ret);
		exit (1);
	}
	else
	{
		/* not needed anymore */
		mime_memfree (pTextBuf);
		pTextBuf = NULL;
	}

	/* Add headers to the inner message */
	pTmpHdr = mime_header_new ("From", "prasad@mcom.com");
	pRFC822_hdrs = pTmpHdr;
	pHdr = pRFC822_hdrs;

	pTmpHdr = mime_header_new ("To", "prasad@mcom.com");
	pHdr->next = pTmpHdr;
	pHdr = pHdr->next;

	pTmpHdr = mime_header_new ("Subject", "This is the Inner Message");
	pHdr->next = pTmpHdr;
	pHdr = pHdr->next;

	pInnerMsg->rfc822_headers = pRFC822_hdrs;

	/* create a messagePart from above message */
	ret = mime_messagePart_fromMessage (pInnerMsg, &pMsgPart);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_messagePart_fromMessage() on Inner Msg failed! Err= %d\n", ret);
		exit (1);
	}

#if (0)
	/* alternatively you could also create a messagePart from a message as follows */
	/*create a new messagePart */
	pMsgPart = (mime_messagePart_t *) mime_malloc (sizeof (mime_messagePart_t));
	memset (pMsgPart, 0, sizeof (mime_messagePart_t));
	pMsgPart->content_type = MIME_CONTENT_MESSAGEPART;
	
	ret = mime_messagePart_setMessage (pMsgPart, pInnerMsg);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_messagePart_setMessage() on Inner Msg failed! Err= %d\n", ret);
		exit (1);
	}
#endif


	/* create a multi-part */
	pMultiPart = (mime_multiPart_t *) mime_malloc (sizeof (mime_multiPart_t));
	memset (pMultiPart, 0, sizeof (mime_multiPart_t));
	pMsgPart->content_type = MIME_CONTENT_MULTIPART;
	pMultiPart->content_subtype = strdup ("mixed");

	/* add the messagePart above to the multi-part */

	ret = mime_multiPart_addMessagePart (pMultiPart, pMsgPart, FALSE, &part_index);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_multiPart_addMessagePar() failed! Err= %d\n", ret);
		exit (1);
	}
	else
	{
		fprintf (stderr, "mime_multiPart_addMessagePar() added at index %d\n", part_index);
	}


	/* create the file part */
	pFilePart = (mime_basicPart_t *) mime_malloc (sizeof (mime_basicPart_t));
	memset (pFilePart, 0, sizeof (mime_basicPart_t));
	extn = getFileExtn (argv[1]);
	ret = getFileMIMEType (extn, &fmt);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "getFileMIMEType() failed! Err= %d\n", ret);
		exit (1);
	}

	/* set content-type etc. */

	pFilePart->content_type = fmt.content_type;
        pFilePart->content_subtype = fmt.content_subtype;
        pFilePart->content_type_params = fmt.content_params;
        pFilePart->encoding_type = fmt.mime_encoding;


	/* create an input stream to the file to be added */
	ret =  file_inputStream_create (argv[1], &pInput_stream);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "file_inputStream_create() failed! Err= %d\n", ret);
		exit (1);
	}
	
	/* Set the stream as body-data of the file-part */
        ret = mime_basicPart_setDataStream (pFilePart, pInput_stream, TRUE);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_basicPart_setDataStream() failed! Err= %d\n", ret);
		exit (1);
	}

	/* add the basicPart above to the multi-part */

	ret = mime_multiPart_addBasicPart (pMultiPart, pFilePart, FALSE, &part_index);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_multiPart_addBasicPart() failed! Err= %d\n", ret);
		exit (1);
	}
	else
	{
		fprintf (stderr, "mime_multiPart_addBasicPart() added at index %d\n", part_index);
	}

	/* create an empty (Outer) message */
	pMsg = (mime_message_t *) mime_malloc (sizeof (mime_message_t));
	memset (pMsg, 0, sizeof (mime_message_t));

	/* Add multi-Part above to the message */
	ret = mime_message_addMultiPart (pMsg, pMultiPart, FALSE);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_addMultiPart() failed! Err= %d\n", ret);
		exit (1);
	}


	outmsg("building RFC822 headers");

	pTmpHdr = mime_header_new ("From", "prasad@netscape.com");
	pRFC822_hdrs = pTmpHdr;
	pHdr = pRFC822_hdrs;

	pTmpHdr = mime_header_new ("To", "prasad@netscape.com");
	pHdr->next = pTmpHdr;
	pHdr = pHdr->next;

	pTmpHdr = mime_header_new ("Subject", "Hello This is a C Test");
	pHdr->next = pTmpHdr;
	pHdr = pHdr->next;

	pTmpHdr = mime_header_new ("X-Msg-SDK-HDR", "X-Test-Value1");
	pHdr->next = pTmpHdr;
	pHdr = pHdr->next;

	outmsg("adding RFC822 headers to message");

	pMsg->rfc822_headers = pRFC822_hdrs;
	
	sprintf (filename, "%s%s", TMPDIR, "sdkCEnc.out");
	outmsg(filename);

	file_outputStream_create (filename, &pOS);

	outmsg(" Invoking mime_message_putByteStream (pMsg, pOS)");

	ret = mime_message_putByteStream (pMsg, pOS);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_putByteStream() failed! Err= %d\n", ret);
		exit (1);
	}

	/* close and free up the input and output streams */
	pOS->close (pOS->rock);
	nsStream_free (pOS);
	pInput_stream->close (pInput_stream->rock);
	nsStream_free (pInput_stream);

	/* free up the message itself */
	mime_message_free_all (pMsg);
	
	pMsg = NULL;
	
	/* NOTE: mime_message_free_all() frees all its constituent parts (recursively as needed) */
	pMsgPart = NULL;
	pMultiPart = NULL;
	pInnerMsg = NULL;

	return 0;
}
