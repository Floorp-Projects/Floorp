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
 
/* This example demonstrates the use of the MIME C-API.
 * Demonstrates parsing  a MIME message from a file and
 * walking through various parts of the returned (parsed)
 * message.
 */

#include "stdio.h"
#include "nsmail.h"
#include "nsStream.h"
#include "mime.h"
#include "mimeparser.h"


mime_message_t *pMessage;

char infilename [512];
char outfilename [512];
mime_message_t *pTopMessage;

#ifdef XP_UNIX
#define SEP "/"
#define BASICPART_FILE "/tmp/bodypart.out"
#define BASICPART_BYTESTREAM "/tmp/basicpart.bstream"
#else
#define SEP "\\"
#define BASICPART_FILE "C:\\Temp\\bodypart.out"
#define BASICPART_BYTESTREAM "C:\\temp\\basicpart.bstream"
#endif

/* forward declaration */
void walkMessage (mime_message_t * pMessage);


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

void free_this_body_part (void * pBodyPart, mime_content_type contentType)
{
	switch (contentType)
	{
		case MIME_CONTENT_TEXT:
		case MIME_CONTENT_AUDIO:
		case MIME_CONTENT_IMAGE:
		case MIME_CONTENT_VIDEO:
		case MIME_CONTENT_APPLICATION:
			mime_basicPart_free_all ((mime_basicPart_t *)pBodyPart);
			break;
		case MIME_CONTENT_MULTIPART:
			mime_multiPart_free_all ((mime_multiPart_t *)pBodyPart);
			break;
		case MIME_CONTENT_MESSAGEPART:
			mime_messagePart_free_all ((mime_messagePart_t *)pBodyPart);
			break;
	}
}

void walkBodyPart (void * pBody, mime_content_type contentType)
{
	BOOLEAN fBasicPart = FALSE, fMultiPart = FALSE, fMsgPart = FALSE;
	mime_basicPart_t * pBasicPart;
	mime_multiPart_t * pMultiPart;
	mime_messagePart_t * pMessagePart;
	mime_message_t * pLocalMessage;
	unsigned int body_len=0;
	nsmail_inputstream_t * pDataStream;
	nsmail_outputstream_t * pOutStream;
	char * pDataBuf, *cont_subType;
	int i, ret = 0, part_count=0;
	mime_content_type cType;
	void * pBodyPart=NULL;
	char basic_byteStream_file[1024];
	static int count=0;

	switch (contentType)
	{
		case MIME_CONTENT_TEXT:
		case MIME_CONTENT_AUDIO:
		case MIME_CONTENT_IMAGE:
		case MIME_CONTENT_VIDEO:
		case MIME_CONTENT_APPLICATION:
			pBasicPart = (mime_basicPart_t *) pBody;
			fBasicPart = TRUE;
			break;
		case MIME_CONTENT_MULTIPART:
			pMultiPart = (mime_multiPart_t *) pBody;
			fMultiPart = TRUE;
			break;
		case MIME_CONTENT_MESSAGEPART:
			pMessagePart = (mime_messagePart_t *) pBody;
			fMsgPart = TRUE;
			break;
	}

	if (fBasicPart == TRUE)
	{
		count++;
		sprintf (basic_byteStream_file, "%s%d", BASICPART_BYTESTREAM, count);
		fprintf (stderr, "Creating output-stream on %s\n", basic_byteStream_file);
		ret = file_outputStream_create (basic_byteStream_file, &pOutStream);

		if (ret != MIME_OK)
		{
			fprintf (stderr, "file_outputStream_create failed! ret = %d\n", ret);
			exit (1);
		}

		ret = mime_basicPart_putByteStream (pBasicPart, pOutStream);

		/* If you want to retireve the bodydata as a stream do */
		ret  = mime_basicPart_getDataStream (pBasicPart, BASICPART_FILE, &pDataStream);

		if (ret != MIME_OK)
		{
			fprintf (stderr, "mime_basicPart_getDataStream failed! ret = %d\n", ret);
			exit (1);
		}
		else
		{
		      /* Do what ever you want with the bodydata. When done, free up things */
		      pDataStream->close (pDataStream->rock); /* close the stream */
		      nsStream_free (pDataStream);
		      pDataStream = NULL;
		}

		/* Or If you to retireve the bodydata in a buffer do */
		ret  = mime_basicPart_getDataBuf (pBasicPart, &body_len, &pDataBuf);

		if (ret != MIME_OK)
		{
			fprintf (stderr, "mime_basicPart_getDataBuf failed! ret = %d\n", ret);
			exit (1);
		}
		else
		{
		      /* Do what ever you want with the bodydata. When done, free up things */
		      mime_memfree (pDataBuf);
		      pDataBuf = NULL;
		}

		/* When done with the basicPart itself, free it also */
		mime_basicPart_free_all (pBasicPart);
		pBasicPart = NULL;
	}
	else if (fMultiPart == TRUE)
	{
		ret = mime_multiPart_getPartCount (pMultiPart, &part_count);

		if (ret != MIME_OK)
		{
			fprintf (stderr, "mime_multiPart_getPartCount failed! ret = %d\n", ret);
			exit (1);
		}

		for (i = 1; i <= part_count; i++)
		{
			ret = mime_multiPart_getPart (pMultiPart, i, &cType, &pBodyPart);

			if (ret != MIME_OK)
			{
				fprintf (stderr, "mime_multiPart_getPartCount failed! ret = %d\n", ret);
				exit (1);
			}

			walkBodyPart (pBodyPart, cType);

			/* Free the part when done */
			free_this_body_part (pBodyPart, cType);
			pBodyPart = NULL;
		}

		/* free the multi-part */
		mime_multiPart_free_all (pMultiPart);
		pMultiPart = NULL;
	}
	else if (fMsgPart == TRUE)
	{
		cont_subType = pMessagePart->content_subtype;

		if (fStrEqual (cont_subType, "rfc822"))
		{
			/* get the message that is the body of this message-part */
			ret = mime_messagePart_getMessage (pMessagePart, &pLocalMessage);
			walkMessage (pLocalMessage);

			/* free up the message and messagePart when done */
			mime_message_free_all (pLocalMessage);
			pLocalMessage = NULL;
		}
		/* handle other sub-types as needed */

		/* free the message-part when done */
		mime_messagePart_free_all (pMessagePart);
		pMessagePart = NULL;
	}
}

void walkMessage (mime_message_t * pMessage)
{
	int ret = 0;
	void * pBody;
	mime_header_t  * pRFC822_hdrs;
	mime_content_type contentType;

	/* walk through the headers as needed */
	pRFC822_hdrs = pMessage->rfc822_headers;
	/* Now simply walk through the list of headers */

	/* walk through the Body of the message */
	ret = mime_message_getBody (pMessage, &contentType, &pBody);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_getBody failed! ret = %d\n", ret);
		exit (1);
	}

 	walkBodyPart (pBody, contentType);
	
	/* free the body-part */
	free_this_body_part (pBody, contentType);
	pBody = NULL;
}

void parseEntireFile (char *path,
                      char *filename)
{
	int ret;
	nsmail_inputstream_t *pInStream;
	nsmail_outputstream_t *pOutStream;
	
	sprintf (infilename, "%s%s%s", path, SEP, filename);
	fprintf (stderr, "input filename=%s\n", infilename);
	
	ret = file_inputStream_create (infilename, &pInStream);

	if (ret != MIME_OK)
	{
		 fprintf (stderr, "file_inputStream_create() failed! rc = %d\n", ret);
		 exit (1);
	}

	ret = parseEntireMessageInputstream (pInStream, &pTopMessage);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "parseEntireMessage() failed! rc = %d\n", ret);
		exit (1);
	}
	else
	{
		fprintf (stderr, "parseEntireMessage() SUCCESSFUL!!\n");

		sprintf (outfilename, "%s%s%s%s", path, SEP, filename,".out");
		fprintf (stderr, "%s\n", outfilename);
		ret = file_outputStream_create (outfilename, &pOutStream);

		if (ret != MIME_OK)
		{
		 	fprintf (stderr, "file_outputStream_create() failed! rc = %d\n", ret);
			exit (1);
		}
		else
		{
			/* encoding it back */
			ret = mime_message_putByteStream (pTopMessage, pOutStream);

			if (ret != MIME_OK)
			{
		 		fprintf (stderr, "mime_message_putByteStream() failed! rc = %d\n", ret);
				exit (1);
			}
			else
			{
		 		fprintf (stderr, "mime_message_putByteStream() SUCCESSFUL!!\n");
				
				/* close and free the streams */
				pOutStream->close (pOutStream->rock);
				nsStream_free (pOutStream);
				pOutStream = NULL;
				pInStream->close (pInStream->rock);
				nsStream_free (pInStream);
				pInStream = NULL;
			}
		}
	}
}


/*
 * This example program parses a MIME message in a file and walks through different
 * parts of the parsed message.
 *
 * Input Parameters:  Path-name and file-name of the file with MIME message to be parsed.
 * 
 */
int main (int argc, char *argv[])
{
	int ret = 0;

	if (argc < 3)
        {
                fprintf (stderr, "Usage: %s <path> <filename>\n", argv[0]);
#ifdef XP_UNIX
                fprintf (stderr, "example: %s /tmp/TestCases_MIME/Messages mime1.txt\n", argv[0]);
#else
                fprintf (stderr, "example: %s e:\\share\\MIME_Test\\Messages mime1.txt\n", argv[0]);
#endif
                exit (1);
        }

	/* parse the message */
	parseEntireFile (argv[1], argv[2]);

	/* Walk through the parsed message and retrieve the needed parts */
	walkMessage (pTopMessage);

	/* free up the message and all its constituent parts */
        mime_message_free_all (pTopMessage);
	pTopMessage = NULL;
}
