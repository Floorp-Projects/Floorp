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
* mimeparser_internal.h
* carsonl, jan 8,97
*
*/

#ifndef MIMEPARSER_INTERNAL_H
#define MIMEPARSER_INTERNAL_H

#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef XP_UNIX
#define BOOLEAN int
#define FALSE 0 
#define TRUE 1 
#endif


#define MAX_MULTIPART		40			/* maximum number of multipart per message */
#define START_BOUNDARY		11			/* start of boundary */
#define END_BOUNDARY		21			/* end of boundary */
#define NOT_A_BOUNDARY		31			/* not a boundary */
#define UNINITIALIZED		-1			/* uninitialized */	  	/* don't modify */
#define BUFFER_SIZE		1024			/* base64 buffer size */
#define SUBTYPE_RFC822			1
#define SUBTYPE_EXTERNAL_BODY	2



typedef struct currentParent
{
	int type;
	void *p;

} currentParent_t;



typedef struct mimeParser
{
	/* parser related */
	Vector *pVectorMimeInfo;				/* mimeInfo, preparse storage */
	Vector *pVectorMessageData;				/* message data */
   	int bStartData;						/* TRUE if next line is message data */

	/* api related */
	mime_content_type nMessageType;				/* message type for entire message */
	mime_content_type nCurrentMessageType;			/* for current message */
	void *pCurrentMessage;					/* current message structure */
	mime_message_t *pMimeMessage;				/* mimeMessage structure where all data will reside */
	BOOLEAN bDeleteMimeMessage;				/* FALSE if you want the data to persist outside the parser */
	int nEmptyLineNo;					/* number of consecutive blank lines */

	/* callback fields */
	mimeDataSink_t *pDataSink;				/* user's datasink, NULL for no callbacks */
    	char *pLeftoverBuffer;					/* base64 left over buffer */
    	char *pInputBuffer;					/* base64 input buffer */
    	int nLeftoverBytes;					/* base64 left over bytes */
    	int out_byte;						/* base64 output byte */
    	int out_bits;						/* base64 output bits */
    	BOOLEAN bParseEntireFile;				/* TRUE to parse entire file, which means no dynamic parsing, */
								/* data is treated as the entire mimeMessage each time it's parsed */
	int nLineType;						/* line type */

	int QPNoOfLeftOverBytes;				/* number of lefted over QP bytes */
	char achQPLeftOverBytes[4];				/* lefted over QP bytes */

    	currentParent_t aCurrentParent[MAX_MULTIPART];   	/* current message parent */
	int nCurrentParent;					/* current parent index */
    	mime_message_t *pCurrentMimeMessage;    		/* current mime message */

	BOOLEAN bDecodeData;					/* TRUE to turn on decoding before sending it to user */
	BOOLEAN bLocalStorage;					/* TRUE to let parser manage storage */

    	mime_message_t *pNextMimeMessage;
    	int nLastBoundry;
    	Vector *pMimeInfoQueue;
    	Vector *pHeaderQueue;
    	BOOLEAN bQPEncoding;
    	BOOLEAN bReadCR;
    	currentParent_t tHeaderParent;
    	currentParent_t tNextHeaderParent;
    	void *szPreviousHeaderName;
    	int nMessagePartSubType;
    	BOOLEAN fSeenBoundary;
    	BOOLEAN fEndMessageHeader;

} mimeParser_t;



/* ------------------- internal routines ---------------------- */

mimeParser_t *mimeParser_new_internal();											/* internal constructor */
mimeParser_t *mimeParser_new_internal2( mime_message_t * pMimeMessage );			/* internal constructor */
int mimeParser_checkForLineType( mimeParser_t *pp, char *s, int len );						/* check for line type */
int mimeParser_parseLine( mimeParser_t *pp, char *s, int len, BOOLEAN lastLine );			/* parse a line */

int mimeParser_parseMimeInfo( mimeParser_t *pp, mimeInfo_t *pmi );					/* parse mimeInfo structure */
mime_content_type mimeParser_nGetContentType( char *s, char *szSubtype, char **ppParam );	/* get content type */
char *mimeParser_parseForBoundary( char *s ); 										/* parse a line for boundary */
int nNewMessageStructure( mimeParser_t *p, char *s );								/* create a new message structure */
int nAddMessage( mimeParser_t *p, void *pMessage, mime_content_type nContentType );	/* add a new message */

int mimeParser_setData( mimeParser_t *p, mimeInfo_t *pMimeInfo );					/* extra data from mimeInfo structure and set fields */
int mimeParser_parseMimeMessage( mimeParser_t *p, nsmail_inputstream_t *pInput, char *pData, int nLen, int nContentType, void **ppReturn );			/* core parser routine */
void decodeDataBuffer( mimeParser_t *p );															/* decode message data */
char *mimeParser_getCurrentBoundary( mimeParser_t *p );												/* get current boundary */
int mimeParser_bIsStartBoundary( mimeParser_t *p, char *s );										/* TRUE if current line is a starting boundary */

int mimeParser_nBoundaryCheck( mimeParser_t *p, char *s, int len );											/* type of boundary */
void setUserObject( void *pMessage, int nType, void *pUserObject );									/* set user object */
void *getUserObject( void *pMessage, int nType ); /* get user object */
void *getUserObject2 ( void *pMessage, int nType ); /* get user object version 2 */
BOOLEAN IsLocalStorage( mimeParser_t *p );															/* TRUE for local storage, default to TRUE for non callbacks */
BOOLEAN IsDecodeData( mimeParser_t *p );															/* TRUE to decode message data, default to TRUE */

void saveBodyData( mimeParser_t *p, char *pBuffer, int nLen, mime_basicPart_t *pMimeBasicPart );	/* save body data to message structure */
char *decodeBase64LeftOverBytes( int out_bits, int out_byte, int *pLen );							/* base64, leftover decoding */

int nGetCurrentParentType( mimeParser_t *p );
void *pGetCurrentParent( mimeParser_t *p );
void vAddCurrentParent(  mimeParser_t *p, int nType, void *pParent );
void mimeParser_unwindCurrentBoundary( mimeParser_t *p, char *s, BOOLEAN bDelete );

void addHeader( mimeParser_t *p, char *name, char *value, BOOLEAN addToQueue );
BOOLEAN checkForEmptyMessages( mimeParser_t *p, void *pMessage, int type );
int setCurrentMessage( mimeParser_t *p, void *pMessage, int nMessageType );


#ifdef __cplusplus
}
#endif


#endif /* MIMEPARSER_INTERNAL_H */
