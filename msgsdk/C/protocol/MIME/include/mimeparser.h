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
* mimeparser.h
* carsonl, jan 8,97
*
*/

#ifndef MIMEPARSER_H
#define MIMEPARSER_H

#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------------------------------------- mimeDataSink ------------------------------------------- */

/* forward declaration */
struct mime_basicPart;
struct mime_multiPart;
struct mime_messagePart;
struct mime_message;

/* Forward declaration of the MIME data sink. */
typedef struct mimeDataSink * mimeDataSinkPtr_t;


typedef struct mimeDataSink
{
    /* Client data Opaque to the API. allocated / freed and managed by the client*/
    void * pOpaqueData;   

    void (*header)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char *name, char *value );	/* mime headers */
    void (*addHeader)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char *name, char *value);	/* additional header value */
    void (*endMessageHeader)(mimeDataSinkPtr_t pSink, void *pCallbackObject);			/* end of message hdrs */
    void (*contentType)(mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentType );	/* content type */
    void (*contentSubType)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentSubType );/* content sub type */
    void (*contentTypeParams)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentTypeParams );/* content type parameters */
    void (*contentID)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentID );		/* content ID */
    void (*contentMD5)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentMD5 );		/* content MD5 */
    void (*contentDisposition)(mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentDisposition ); /* content disposition */
    void (*contentDispParams)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentDispParams ); /* content disposition parameters */
    void (*contentDescription)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentDescription );/* content description */
    void (*contentEncoding)(mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentEncoding );	/* content encoding */
    void *(*startMessage)(mimeDataSinkPtr_t pSink);				/* signal start of message */
    void (*endMessage)(mimeDataSinkPtr_t pSink, void *pCallbackObject );	/* signal end of message */

    void *(*startBasicPart)(mimeDataSinkPtr_t pSink);						/* signal start of basicpart */
    void (*bodyData)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char bodyData[], int len );	/* message data */
    void (*endBasicPart)(mimeDataSinkPtr_t pSink, void *pCallbackObject );	/* signal end of basicpart */

    void *(*startMultiPart)(mimeDataSinkPtr_t pSink);		/* signal start of multipart */
    void (*boundary)(mimeDataSinkPtr_t pSink, void *pCallbackObject, char * boundary );			/**/
    void (*endMultiPart)(mimeDataSinkPtr_t pSink, void *pCallbackObject );		/* signal end of multipart */

    void *(*startMessagePart)(mimeDataSinkPtr_t pSink);		/* signal start of messagepart */
    void (*endMessagePart)(mimeDataSinkPtr_t pSink, void *pCallbackObject );	/* signal end of messagepart */

} mimeDataSink_t;



int mimeDataSink_new (mimeDataSink_t ** out_ppDataSink);	/* constructor */
void mimeDataSink_free (mimeDataSink_t ** in_ppDataSink);	/* destructor */


/* ---------------------------------------------- mimeParser ------------------------------------------- */


struct mimeParser;

/* constructor */
int mimeDynamicParser_new (mimeDataSink_t * in_pDataSink, 
			   struct mimeParser ** out_ppParser);

/* destructor */
void mimeDynamicParser_free (struct mimeParser ** in_ppParser);

/* begin parsing new message */
int beginDynamicParse (struct mimeParser * in_pParser);

/* parse more data  (given as an input-stream) */
int dynamicParseInputstream (struct mimeParser * in_pParser, 
			    struct nsmail_inputstream *in_pInput);		

/* parse more data  (given as a data-buffer) */
int dynamicParse (struct mimeParser * in_pParser, char * in_pData, int in_nLen);

/* Tell the parser no more data to parse */
int endDynamicParse (struct mimeParser * in_pParser);

/* Parse an entire message in one shot. Data given as an input-stream */
int parseEntireMessageInputstream (struct nsmail_inputstream * in_pInput, 
				   struct mime_message ** out_ppMimeMessage);

/* Parse an entire message in one shot. Data given as data-buffer */
int parseEntireMessage (char * in_pData, int  in_nLen, 
			struct mime_message ** in_ppMimeMessage);


#ifdef __cplusplus
}
#endif


#endif /* MIMEPARSER_H */
