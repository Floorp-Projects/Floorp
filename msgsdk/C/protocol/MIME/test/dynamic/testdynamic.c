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
  
/* * Copyright (c) 1997 and 1998 Netscape Communications Corporation
* (http://home.netscape.com/misc/trademarks.html)
*/

/* 
 * Example program to demonstrate the use of the MIME dynamic parser 
 *
 */


#include "stdio.h"
#include "nsmail.h"
#include "mime.h"
#include "mimeparser.h"
#include "nsStream.h"
#include "testdynamic.h"


char achTemp[512];
int messageNo = 0;
int basicPartNo = 0;
int multiPartNo = 0;
int messagePartNo = 0;
nsmail_outputstream_t *pBodyDataStream;

#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))
#define outmsg2(x, y) (fprintf(stderr,"%s:%d>%s%s\n",__FILE__,__LINE__,x,y))

#ifdef XP_UNIX
#define BASICPART_FILE "/tmp/bodypart.out"
#define BODYDATA_FILE "/tmp/boddata.out"
#define BASICPART_BYTESTREAM "/tmp/basicpart.bstream"
#else
#define BASICPART_FILE "C:\\temp\\bodypart.out"
#define BODYDATA_FILE "C:\\temp\\boddata.out"
#define BASICPART_BYTESTREAM "C:\\temp\\basicpart.bstream"
#endif

/* allocate and return the callback object */
char * getCBObject (char * s, int num)
{
	char * CBObj = (char *) mime_malloc (20);

	memset (CBObj, 0, 20);

	sprintf (CBObj, "%s%d", s, num);
	return CBObj;
}

void freeCBObject (void * pCBObj)
{
	mime_memfree (pCBObj);
}


/************************************************************************************/
/* 			------ datasink methods ---------- 			    */
/************************************************************************************/


void mimeDataSink2_header (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *name, char *value)
{
	if (name != NULL && value != NULL)
	     sprintf( achTemp, "> header()   name = [%s]   value = [%s]\n", name, value);
	else if (name != NULL && value == NULL)
	     sprintf( achTemp, "> header()   name = [%s]   value = [%s]\n", name, "null");
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_addHeader (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *name, char *value)
{
	sprintf( achTemp, "> addHeader()   name = [%s]   value = [%s]\n", name, value);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_endMessageHeader (mimeDataSinkPtr_t pSink, void *pCallbackObject)
{
	sprintf( achTemp, "> endMessageHeader()");
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentType (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentType)
{
	sprintf( achTemp, "> contentType() = [%d]\n", nContentType);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentSubType (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentSubType)
{
	sprintf( achTemp, "> contentSubType() = [%s]\n", contentSubType);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentTypeParams (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentTypeParams)
{
	sprintf( achTemp, "> contentTypeParams() = [%s]\n", contentTypeParams);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentID (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *contentID)
{
	sprintf( achTemp, "> contentID() = [%s]\n", contentID);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentMD5 (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentMD5)
{
	sprintf( achTemp, "> contentMD5() = [%s]\n", contentMD5);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentDisposition (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentDisposition)
{
	sprintf( achTemp, "> contentDisposition() = [%d]\n", nContentDisposition);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentDispParams (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentDispParams)
{
	sprintf( achTemp, "> contentDispParams() = [%s]\n", contentDispParams);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentDescription (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *contentDescription)
{
	sprintf( achTemp, "> contentDescription() = [%s]\n", contentDescription);
	outmsg2((char *)pCallbackObject, achTemp);
}

void mimeDataSink2_contentEncoding (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentEncoding)
{
	sprintf( achTemp, "> contentEncoding() = [%d]\n", nContentEncoding);
	outmsg2((char *)pCallbackObject, achTemp);
}


void *mimeDataSink2_startMessage (mimeDataSinkPtr_t pSink)
{
	char * msgCB = getCBObject ("Message", ++messageNo);
	outmsg2("startMessage() ", msgCB);
	return msgCB;
}

void mimeDataSink2_endMessage (mimeDataSinkPtr_t pSink, void *pCallbackObject)
{
	outmsg2((char*)pCallbackObject, "> endMessage()");
	/*outmsg( "endMessage()\n");*/
}


void *mimeDataSink2_startBasicPart (mimeDataSinkPtr_t pSink)
{
	char * basicCB = getCBObject ("BasicPart", ++basicPartNo);
	outmsg2("startBasicPart() ", basicCB);
	return basicCB;
}

void mimeDataSink2_bodyData (mimeDataSinkPtr_t pSink, void *pCallbackObject, char bodyData[], int len)
{
	outmsg((char *)pCallbackObject);
	outmsg( "bodyData() = [");
	outmsg( "]\n");
	if (pBodyDataStream != NULL)
        {
               pBodyDataStream->write (pBodyDataStream->rock, bodyData, len);
        }
}

void mimeDataSink2_endBasicPart (mimeDataSinkPtr_t pSink, void *pCallbackObject)
{
	outmsg2((char*)pCallbackObject, "> endBasicPart()");
}

void *mimeDataSink2_startMultiPart (mimeDataSinkPtr_t pSink)
{
	char * mpCB = getCBObject ("MultiPart", ++multiPartNo);
	outmsg2("startMultiPart() ", mpCB);
	return mpCB;
}

void mimeDataSink2_boundary (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * boundary)
{
	sprintf( achTemp, "> boundary() = [%s]\n", boundary);
	outmsg2((char*)pCallbackObject, achTemp);
}

void mimeDataSink2_endMultiPart (mimeDataSinkPtr_t pSink, void *pCallbackObject)
{
	outmsg2((char*)pCallbackObject, "> endMultiPart()");
}

void *mimeDataSink2_startMessagePart (mimeDataSinkPtr_t pSink)
{
	char * msgpCB = getCBObject ("MessagePart", ++messagePartNo);
	outmsg2("startMessagePart() ", msgpCB);
	return msgpCB;
}

void mimeDataSink2_endMessagePart (mimeDataSinkPtr_t pSink, void *pCallbackObject)
{
	outmsg2((char*)pCallbackObject, "> endMessagePart()");
}



void dynamicParseEntireFile (char *szFilename) 
{
	int len, ret;
	char buffer[BUFFER_SIZE2];
	struct mimeParser *p;
	mimeDataSink_t *pDataSink;
	nsmail_inputstream_t *pInStream;
	
	/* Create a new data-sink */
	ret = mimeDataSink_new (&pDataSink);

	if (ret != MIME_OK)
	{
		 fprintf (stderr, "mimeDataSink_new failed! ret = %d\n", ret);
                 exit (1);
	}

	/* initialize the data-sink */
	pDataSink->header 		= &mimeDataSink2_header;
	pDataSink->addHeader 		= &mimeDataSink2_addHeader;
	pDataSink->endMessageHeader 	= &mimeDataSink2_endMessageHeader;
	pDataSink->contentType 		= &mimeDataSink2_contentType;
    	pDataSink->contentSubType 	= &mimeDataSink2_contentSubType;
    	pDataSink->contentTypeParams    = &mimeDataSink2_contentTypeParams;
    	pDataSink->contentID 		= &mimeDataSink2_contentID;
    	pDataSink->contentMD5 		= &mimeDataSink2_contentMD5;
    	pDataSink->contentDisposition 	= &mimeDataSink2_contentDisposition;
    	pDataSink->contentDispParams 	= &mimeDataSink2_contentDispParams;
    	pDataSink->contentDescription 	= &mimeDataSink2_contentDescription;
    	pDataSink->contentEncoding 	= &mimeDataSink2_contentEncoding;

    	pDataSink->startMessage 	= &mimeDataSink2_startMessage;
    	pDataSink->endMessage 		= &mimeDataSink2_endMessage;

    	pDataSink->startBasicPart 	= &mimeDataSink2_startBasicPart;
    	pDataSink->bodyData 		= &mimeDataSink2_bodyData;
    	pDataSink->endBasicPart		= &mimeDataSink2_endBasicPart;

    	pDataSink->startMultiPart 	= &mimeDataSink2_startMultiPart;
    	pDataSink->boundary 		= &mimeDataSink2_boundary;
    	pDataSink->endMultiPart 	= &mimeDataSink2_endMultiPart;

    	pDataSink->startMessagePart 	= &mimeDataSink2_startMessagePart;
    	pDataSink->endMessagePart 	= &mimeDataSink2_endMessagePart;

	/* create a new parser instance */
	ret = mimeDynamicParser_new (pDataSink, &p);

	if (ret != MIME_OK)
	{
		mimeDataSink_free( &pDataSink);
		fprintf (stderr, "mimeDynamicParser_new failed! ret = %d\n", ret);
		exit (1);
	}

	/* open an input-stream to the file with MIME message to parse */
	ret = file_inputStream_create (szFilename, &pInStream);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "file_inputStream_create failed! ret = %d\n", ret);
		exit (1);
	}
	
	ret = file_outputStream_create (BODYDATA_FILE, &pBodyDataStream);

        if (ret != MIME_OK)
        {
                fprintf (stderr, "file_outputStream_create on %s failed! ret = %d\n", BODYDATA_FILE, ret);
                pBodyDataStream = NULL;
        }

	beginDynamicParse (p);

	/* You can also pass the entire stream to the parser */
	/* ret = dynamicParseInputstream (p, pInStream);     */

	for (len = pInStream->read (pInStream->rock, buffer, BUFFER_SIZE2);
	     len > 0;
	     len = pInStream->read (pInStream->rock, buffer, BUFFER_SIZE2))
	{
		if (dynamicParse (p, buffer, len) != MIME_OK)
			break;
	}

	ret = endDynamicParse (p);

	if (ret != MIME_OK)
	{
		fprintf (stderr, "file_inputStream_create failed! ret = %d\n", ret);
		exit (1);
	}

	/* close and free the inputStream created */
	pInStream->close (pInStream->rock);
	nsStream_free (pInStream);

	/* free the parser and data-sink */
	mimeDynamicParser_free (&p);
	mimeDataSink_free (&pDataSink);
}



int main (int argc, char *argv[])
{
	if (argc !=  2)
        {
                fprintf (stderr, "Usage: %s <filename> \n", argv[0]);
#ifdef XP_UNIX
                fprintf (stderr, "example: %s /tmp/TestCases_MIME/Messages/mime1.txt\n", argv[0]);
#else
                fprintf (stderr, "example: %s e:\\share\\MIME_Test\\Messages\\mime1.txt\n", argv[0]);
#endif
                exit (1);
        }

	dynamicParseEntireFile (argv[1]);

	return 0;
}
