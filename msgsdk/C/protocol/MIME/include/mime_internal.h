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


/**** mime_internal.h ****/

#ifndef MIME_INTERNAL_H
#define MIME_INTERNAL_H

#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif


#define MIME_INFO				1
#define MIME_HEADER				2
#define MIME_XHEADER				3
#define MIME_MESSAGE_DATA			4
#define MIME_PARAM				5
#define MIME_BOUNDARY				6
#define MIME_CRLF				7

#define MIME_MESSAGE				20
#define MIME_BASICPART				MIME_CONTENT_TEXT
#define MIME_MULTIPART				MIME_CONTENT_MULTIPART
#define MIME_MESSAGEPART			MIME_CONTENT_MESSAGEPART
#define MIME_PARSE_ALL				30
#define MIME_MP_MAX_PARTS			50    /* Max 50	parts in a multi-part */
#define BUFFER_INCREMENT	1024	/* buffer increases by this when it runs out of space */



/* used by parser to store general message information */
typedef struct mimeInfo
{
	int nType;				/* mimeInfo type */
	int nContentType;			/* current message content type */
	char *szName;				/* name */
	char *szValue;				/* value */
	Vector *pVectorParam;			/* extra parameters */

} mimeInfo_t;


typedef struct mime_mp_partInfo
{
	mime_content_type nContentType;		/* content type  of the part */
	char *contentID;			/* content ID */
	void *pThePart;				/* pointer to actual part */

} mime_mp_partInfo_t;


typedef struct mime_basicPart_internal
{
	nsmail_inputstream_t *pTheDataStream;   /* pointer to bodydata STREAM! */
	char *szMessageBody;			/* message body data */
	long nMessageSize;				/* size of decoded message body data */
	long nRawMessageSize;			/* size of raw message body data */
	int nStartMessageDataIndex;		/* start of message vector index */
	int nEndMessageDataIndex;		/* end of message vector index */
	BOOLEAN bDecodedData;			/* TRUE if data has been decoded */
	int nDynamicBufferSize;			/* dynamic buffer size, only 4 callbacks */
	void *pUserObject;			/* user object */

} mime_basicPart_internal_t;


typedef struct mime_multiPart_internal
{
	int nPartCount;			
	mime_mp_partInfo_t partInfo [MIME_MP_MAX_PARTS];	
    	char *szBoundary;	
	void *pUserObject;
	BOOLEAN fParsedPart;

} mime_multiPart_internal_t;


typedef struct mime_messagePart_internal
{
	struct mime_message *pTheMessage;
	void *pUserObject;		

} mime_messagePart_internal_t;


typedef struct mime_message_internal
{
	mime_basicPart_t *pMimeBasicPart;	
	mime_multiPart_t *pMimeMultiPart;
	mime_messagePart_t *pMimeMessagePart;

	/* callback support */
	void *pUserObject;		/* user object */
	struct mimeDataSink *pDataSink;	/* user's datasink. NULL if not using dynamic parsing */

} mime_message_internal_t;



/* ---------------- BasicPart ---------------- */
struct mime_basicPart * mime_basicPart_new();
int mime_basicPart_free (struct mime_basicPart *p);
struct mime_basicPart * mime_basicPart_clone (struct mime_basicPart *p);

mime_basicPart_internal_t * mime_basicPart_internal_new();
int mime_basicPart_internal_free (mime_basicPart_internal_t *p);
mime_basicPart_internal_t * mime_basicPart_internal_clone (mime_basicPart_internal_t *p);


/* ---------------- Multi-part ---------------- */
struct mime_multiPart * mime_multiPart_new();
int mime_multiPart_free (struct mime_multiPart *p);
struct mime_multiPart * mime_multiPart_clone (struct mime_multiPart *p);

int mime_multiPart_addPart (struct mime_multiPart *pMultiPart,
		             void *pMessage,
			     mime_content_type nContentType,
		             int *index_assigned );

int mime_multiPart_addPart_clonable( struct mime_multiPart *pMultiPart,
				void * pMessage,
				mime_content_type nContentType,
				BOOLEAN clone,
				int * pIndex_assigned);

int mime_multiPart_getPart2 (struct mime_multiPart *pMultiPart,
			     int  index,
			     char *contentID,
			     mime_content_type *pContentType,
		             void **ppTheBodyPart );

mime_multiPart_internal_t * mime_multiPart_internal_new();
int mime_multiPart_internal_free (mime_multiPart_internal_t *p);
mime_multiPart_internal_t * mime_multiPart_internal_clone (mime_multiPart_internal_t *p);


/* ---------------- MessagePart ---------------- */
struct mime_messagePart * mime_messagePart_new();
int mime_messagePart_free (struct mime_messagePart *p);
struct mime_messagePart * mime_messagePart_clone (struct mime_messagePart *p);
BOOLEAN mime_messagePart_isEmpty (struct mime_messagePart *pMessage);

mime_messagePart_internal_t * mime_messagePart_internal_new();
int mime_messagePart_internal_free (mime_messagePart_internal_t *p);
mime_messagePart_internal_t * mime_messagePart_internal_clone (mime_messagePart_internal_t *p);


/* ---------------- Message ---------------- */
mime_message_internal_t * mime_message_internal_new();
int mime_message_internal_free (mime_message_internal_t *p);
mime_message_internal_t * mime_message_internal_clone (mime_message_internal_t *p);


/* common constructor for both regular and dynamic parsing versions
 * pass in a NULL parameter means regular parsing
 * pass in a proper datasink to turn on dynamic parsing
 */
mime_message_t * mime_message_new (struct mimeDataSink *pDataSink);
int mime_message_free (struct mime_message *p);
struct mime_message * mime_message_clone (struct mime_message *p );
int mime_message_addBasicPart_clonable (struct mime_message * pMessage, 
					struct mime_basicPart * pBasicPart, 
					BOOLEAN clone );
int mime_message_addMultiPart_clonable (struct mime_message * pMessage, 
					struct mime_multiPart * pMultiPart, 
					BOOLEAN clone );
int mime_message_addMessagePart_clonable (struct mime_message * pMessage, 
					  struct mime_messagePart * pMessagePart, 
					  BOOLEAN clone );
BOOLEAN mime_message_isEmpty (struct mime_message *pMessage);


/* ---------------- Header ---------------- */
/*struct mime_header * mime_header_new (char *szName, char *szValue); */
int mime_header_add (struct mime_header *pStart, char *szName, char *szValue);
int mime_header_apend( mime_header_t *pStart, char *szName, char *szValue );
/*int mime_header_free (struct mime_header *p); */
int mime_header_freeAll (struct mime_header *pStart);
struct mime_header * mime_header_clone (struct mime_header *pMimeHeader);



/* -------------- Generic -------------- */
int mime_getStructType (void *pStruct);

void * mime_clone_any_part (void * pThePart, mime_content_type nContentType);

int mime_translateMimeInfo (char *name, char *value);
int mime_translateMimeEncodingType (char *s);
int mime_translateDispType (char *s, char **ppParam);

struct mimeInfo *mimeInfo_new();
int mimeInfo_free (struct mimeInfo *p);
int mimeInfo_init (struct mimeInfo *p, int nType1, char *szName1);
int mimeInfo_init2 (struct mimeInfo *p, int nType1, char *szName1, char *szValue1);
struct mimeInfo *mimeInfo_clone (struct mimeInfo *p);



#ifdef __cplusplus
}
#endif


#endif /* MIME_INTERNAL_H */
