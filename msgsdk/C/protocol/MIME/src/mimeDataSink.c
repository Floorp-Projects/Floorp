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
* mimeparser.c
* carsonl, Jan 8,97
*
*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "nsmail.h"
#include "vector.h"
#include "util.h"
#include "mime.h"
#include "mime_internal.h"
#include "mimeparser.h"



/*
* carsonl, jan 7,97 
* default do nothing callback methods
*
* NOTE : by default all callbacks do nothing, it's up to the user to override the methods they're interested in
*/
void mimeDataSink_header(mimeDataSink_t *pSink,  void *pCallbackObject, char *name, char *value ) { return; }
void mimeDataSink_contentType(mimeDataSink_t *pSink,  void *pCallbackObject, int nContentType ) { return; }
void mimeDataSink_contentSubType(mimeDataSink_t *pSink,  void *pCallbackObject, char * contentSubType ) { return; }
void mimeDataSink_contentTypeParams(mimeDataSink_t *pSink,  void *pCallbackObject, char * contentTypeParams ) { return; }
void mimeDataSink_contentID(mimeDataSink_t *pSink,  void *pCallbackObject, char * contentID ) { return; }
void mimeDataSink_contentMD5(mimeDataSink_t *pSink,  void *pCallbackObject, char * contentMD5 ) { return; }
void mimeDataSink_contentDisposition(mimeDataSink_t *pSink,  void *pCallbackObject, int	nContentDisposition ) { return; }
void mimeDataSink_contentDispParams(mimeDataSink_t *pSink,  void *pCallbackObject, char * contentDispParams ) { return; }
void mimeDataSink_contentDescription(mimeDataSink_t *pSink, void *pCallbackObject, char * contentDescription ) { return; }
void mimeDataSink_contentEncoding(mimeDataSink_t *pSink,  void *pCallbackObject, int nContentEncoding ) { return; }

void *mimeDataSink_startMessage(mimeDataSink_t *pSink) { return NULL; }
void mimeDataSink_endMessage(mimeDataSink_t *pSink, void *pCallbackObject ) { return; }

void *mimeDataSink_startBasicPart(mimeDataSink_t *pSink) { return NULL; }
void mimeDataSink_bodyData(mimeDataSink_t *pSink, void *pCallbackObject, char bodyData[], int len ) { return; }
void mimeDataSink_endBasicPart(mimeDataSink_t *pSink, void *pCallbackObject ) { return; }

void *mimeDataSink_startMultiPart(mimeDataSink_t *pSink) { return NULL; }
void mimeDataSink_boundary(mimeDataSink_t *pSink,  void *pCallbackObject, char * boundary ) { return; }
void mimeDataSink_endMultiPart(mimeDataSink_t *pSink,  void *pCallbackObject ) { return; }

void *mimeDataSink_startMessagePart(mimeDataSink_t *pSink) { return NULL; }
void mimeDataSink_endMessagePart(mimeDataSink_t *pSink,  void *pCallbackObject ) { return; }

void mimeDataSink_addHeader(mimeDataSinkPtr_t pSink, void *pCallbackObject, char *name, char *value) { return; }
void mimeDataSink_endMessageHeader(mimeDataSinkPtr_t pSink, void *pCallbackObject) { return; }


/*
* carsonl, jan 7,97 
* datasink constructor
*
* parameter :  none
*
* returns : instance of newly created datasink
*
* NOTE : by default all callbacks do nothing, it's up to the user to override the methods they're interested in
*/
int mimeDataSink_new( mimeDataSink_t **pp )
{
	if ( pp == NULL )
		return MIME_ERR_INVALIDPARAM;

	*pp = (mimeDataSink_t *) malloc( sizeof( mimeDataSink_t ) );

	if ( *pp == NULL )
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	(*pp)->pOpaqueData = NULL;
	(*pp)->header = &mimeDataSink_header;
	(*pp)->addHeader = &mimeDataSink_addHeader;
	(*pp)->endMessageHeader = &mimeDataSink_endMessageHeader;
	(*pp)->contentType = &mimeDataSink_contentType;
    (*pp)->contentSubType = &mimeDataSink_contentSubType;
    (*pp)->contentTypeParams = &mimeDataSink_contentTypeParams;
    (*pp)->contentID = &mimeDataSink_contentID;
    (*pp)->contentMD5 = &mimeDataSink_contentMD5;
    (*pp)->contentDisposition = &mimeDataSink_contentDisposition;
    (*pp)->contentDispParams = &mimeDataSink_contentDispParams;
    (*pp)->contentDescription = &mimeDataSink_contentDescription;
    (*pp)->contentEncoding = &mimeDataSink_contentEncoding;

    (*pp)->startMessage = &mimeDataSink_startMessage;
    (*pp)->endMessage = &mimeDataSink_endMessage;

    (*pp)->startBasicPart = &mimeDataSink_startBasicPart;
    (*pp)->bodyData = &mimeDataSink_bodyData;
    (*pp)->endBasicPart = &mimeDataSink_endBasicPart;

    (*pp)->startMultiPart = &mimeDataSink_startMultiPart;
    (*pp)->boundary = &mimeDataSink_boundary;
    (*pp)->endMultiPart = &mimeDataSink_endMultiPart;

    (*pp)->startMessagePart = &mimeDataSink_startMessagePart;
    (*pp)->endMessagePart = &mimeDataSink_endMessagePart;

	return MIME_OK;
}



/*
* carsonl, jan 7,97 
* datasink destructor
*
* parameter :
*
*	mimeDataSink_t *p : instance of datasink
*
* returns : nothing
*/
void mimeDataSink_free( mimeDataSink_t **pp )
{
	if ( pp != NULL && *pp != NULL )
	{
		free( *pp );
		*pp = NULL;
	}
}


