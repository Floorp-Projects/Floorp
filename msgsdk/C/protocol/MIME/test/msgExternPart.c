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

/* This example demonstrates use of MIME C-API to create a message w/ external-body
 * content-subtype.
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
	mime_message_t 		*pMsg = NULL, *pInnerMsg = NULL;
	mime_messagePart_t 	*pMsgPart = NULL;
	nsmail_outputstream_t 	*pOS;
	nsmail_inputstream_t 	*pInput_stream;
	mime_header_t  * pRFC822_hdrs, * pHdr, *pTmpHdr;
	int ret;

	/*ret = mime_message_create (pTextBuf, NULL,  &pMsg); */
	/*ret = mime_message_create (NULL, argv[1],  &pMsg);*/
	/*ret = mime_message_create (pTextBuf, argv[1],  enc, &pMsg);*/

	/*create a new messagePart */
	pMsgPart = (mime_messagePart_t *) mime_malloc (sizeof (mime_messagePart_t));
	memset (pMsgPart, 0, sizeof (mime_messagePart_t));
	pMsgPart->content_type = MIME_CONTENT_MESSAGEPART;
	pMsgPart->content_subtype = (char *) mime_malloc (14);
	sprintf (pMsgPart->content_subtype,  "%s", "external-body");

	pMsgPart->content_type_params = (char *) mime_malloc (80);
	sprintf (pMsgPart->content_type_params,  "%s", "access-type=local-file;name=\"e:\\share\\jpeg1.jpg\"");
	
	/* set external-body headers */
	pTmpHdr = mime_header_new ("Content-Type", "image/jpg");
	pMsgPart->extern_headers = pTmpHdr;

	/* create an empty (Outer) message */
	pMsg = (mime_message_t *) mime_malloc (sizeof (mime_message_t));
	memset (pMsg, 0, sizeof (mime_message_t));

	/* Add MsgPart above to the message */
	ret = mime_message_addMessagePart (pMsg, pMsgPart, FALSE);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_addMessagePart() failed! Err= %d\n", ret);
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

	ret = file_outputStream_create (filename, &pOS);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "file_outputStream_create() failed! Err= %d\n", ret);
		exit (1);
	}

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

	/* free up the message itself */
	mime_message_free_all (pMsg);
	
	pMsg = NULL;
	
	/* NOTE: mime_message_free_all() frees all its constituent parts (recursively as needed) */
	pMsgPart = NULL;

	return 0;
}
