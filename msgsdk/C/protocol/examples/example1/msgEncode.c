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

#include <stdio.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <string.h>

#include "mime.h"
#include "nsStream.h"

#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))
mime_header_t *mime_header_new (char *szName, char *szValue);

#ifdef XP_UNIX
#define TMPDIR "/tmp/"
#else
#define TMPDIR "C:\\temp\\"
#endif


/* 
 *    This program demonstrates how to build and encode a MIME message, given
 *   
 *   			(1) Simple text
 *   			(2) A file
 *   			(3) Set of RFC822 headers.
 *
 *   The API permits building a message with just a text or a file or both.
 *   Additional files can also be added to the message later on if needed.
 * 
 *   NOTE: For the file the API automatically detects MIME content-types, MIME-Encoding etc.
 *	 However one can also override the MIME encoding.
 *
 *   NOTE: This program works on Windows and on all tested UNIX platforms 
 *         (Solaris, HPUX, AIX, IRIX etc.)
 */
void main (int argc, char *argv[])
{

	char textBuf[1024], *pBuf, *pEnc;
	char filename [1024];
	mime_message_t * pMsg = NULL;
	mime_encoding_type enc = 0;
	nsmail_outputstream_t *pOS;
	int ret;

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
			enc = MIME_ENCODING_BASE64;
		else if (fStrEqual (pEnc, "Q"))
			enc = MIME_ENCODING_QP;
	}

	fprintf (stderr, "FileName = %s\n", argv[1]);

	pBuf = textBuf;
	sprintf (pBuf,  "Hello this is a test Message");
	
	/* Build a MIME Message with the "text" and the file (argv[1]) */
	ret = mime_message_create (pBuf, argv[1],  enc, &pMsg);

	/* If building a message with just text and NO file do as follows */
	/* ret = mime_message_create (pBuf, NULL, 0, &pMsg);    */

	/* If building a message with just a file and NO text do as follows */
	/* ret = mime_message_create (NULL, argv[1], 0, &pMsg); */

	if (ret != MIME_OK)
	{
		fprintf (stderr, "mime_message_create() failed! Err= %d\n", ret);
		exit (1);
	}

	/* Add all needed RFC822 headers to the message */
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
	

       
	/* Write the MIME encoded message to where you want. Here it is a file */
	sprintf (filename, "%s%s", TMPDIR, "sdkCEnc.out");

	file_outputStream_create (filename, &pOS);

	outmsg(" Invoking mime_message_putByteStream (pMsg, pOS)");

	mime_message_putByteStream (pMsg, pOS);

	file_outputStream_close (pOS->rock);
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

mime_header_t *mime_header_new (char *szName, char *szValue)
{
        mime_header_t *p;
 
        if ( szName == NULL || szValue == NULL )
        {
                return NULL;
        }
 
        p = (mime_header_t *) malloc( sizeof( mime_header_t ) );
 
        if ( p == NULL )
        {
                return NULL;
        }
 
        p->name = strdup (szName);
        p->value = strdup (szValue);
        p->next = NULL;
 
        return p;
}
