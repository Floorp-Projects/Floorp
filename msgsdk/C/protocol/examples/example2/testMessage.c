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
  
#include <stdio.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <string.h>

#include "mime.h"
#include "nsStream.h"

extern int file_outstream_create (char * fileName, nsmail_outputstream_t ** ppRetOutputStream);
extern void file_outstream_close (void * rock);

#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))

#ifdef XP_UNIX
#define TMPDIR "/tmp/"
#else
#define TMPDIR "E:\\share\\test\\mime_encode\\"
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

	char textBuf[1024], *pBuf;
	char filename [1024];
	mime_message_t * pMsg = NULL;
	mime_encoding_type enc = 0;
	nsmail_outputstream_t *pOS;
	nsmail_inputstream_t *pInput_stream;
	int ret;
	char *pEnc,  * extn = NULL;
	mime_basicPart_t * pFilePart = NULL;
	file_mime_type     fmt;
	mime_header_t  * pRFC822_hdrs, * pHdr, *pTmpHdr;

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

	/*ret = mime_message_create (pBuf, NULL,  &pMsg); !!!Worked!!! */
	/*ret = mime_message_create (NULL, argv[1],  &pMsg); !!WORKED!!*/
	/*ret = mime_message_create (pBuf, argv[1],  enc, &pMsg);*/

	/* create an empty message */
	pMsg = (mime_message_t *) mime_malloc (sizeof (mime_message_t));

	/* create the file part */
	pFilePart = (mime_basicPart_t *) mime_malloc (sizeof (mime_basicPart_t));
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

	ret = mime_message_addBasicPart (pMsg, pFilePart, FALSE);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_addBasicPart() failed! Err= %d\n", ret);
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

	/*file_outstream_create ("/tmp/sdkCEnc.out", &pOS);*/

	file_outputStream_create (filename, &pOS);

	outmsg(" Invoking mime_message_putByteStream (pMsg, pOS)");

	ret = mime_message_putByteStream (pMsg, pOS);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_putByteStream() failed! Err= %d\n", ret);
		exit (1);
	}

	pOS->close (pOS->rock);

	return 0;
}


