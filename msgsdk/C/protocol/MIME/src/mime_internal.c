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
*	mime_internal.c
*
* the routines in this file are internal and not exposed to the user
*
*	Prasad, oct 97
*	carsonl, oct 97
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

#ifdef DEBUG
#define outmsg(x) (fprintf(stderr,"%s:%d>%s\n",__FILE__,__LINE__,x))
#else
#define outmsg(x) 
#endif


/* -------------------------------------------- */



/*
* return NULL if unsuccessful 
*
* parameter :
*
*	szName : name
*	szValue : value
*
* returns :	MIME_OK if successful
*/
mime_header_t *mime_header_new( char *szName, char *szValue )
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
	else
		memset (p, 0, sizeof (mime_header_t));

	p->name = szStringClone( szName );
	p->value = szStringClone( szValue );
	p->next = NULL;

	return p;
}



/*
* carsonl, jan 8,98 
* Add additional header structures after the initial one is created 
* create and append header to end of list
* return MIME_OK if success 
*
* parameter :
*
*	pStart : starting header
*	szName : name
*	szValue : value
*
* returns :	MIME_OK if successful
*/
int mime_header_add( mime_header_t *pStart, char *szName, char *szValue )
{
	mime_header_t *p = NULL;
	mime_header_t *pNew = NULL;

	if ( pStart == NULL || szName == NULL || szValue == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	for ( p = pStart; p->next != NULL; p = p->next )
		;

	pNew = (mime_header_t *) malloc( sizeof( mime_header_t ) );
	
	if ( pNew == NULL )
	{
		return MIME_ERR_OUTOFMEMORY;
	}
	else
		memset (pNew, 0, sizeof (mime_header_t));

	pNew->name = szStringClone( szName );
	pNew->value = szStringClone( szValue );
	pNew->next = NULL;
	p->next = pNew;

	return MIME_OK;
}


/*
* carsonl, jan 8,98 
* Append header values to an existing header structure
* return MIME_OK if success 
*
* parameter :
*
*	pStart : starting header
*	szName : name
*	szValue : value
*
* returns :	MIME_OK if successful
*/
int mime_header_apend( mime_header_t *pStart, char *szName, char *szValue )
{
	mime_header_t *p = NULL;
	mime_header_t *pNew = NULL;
	char achTemp[512];

	if ( pStart == NULL || szName == NULL || szValue == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* first try to add to the last one if matches */
	for (p = pStart; p->next != NULL; p = p->next);
	
	if (p != NULL)
	{
		if (strcmp (szName, p->name) == 0)
		{
			strcpy( achTemp, p->value );
			strcat( achTemp, szValue );

			if ( p->value != NULL )
				free( p->value );

			p->value = szStringClone( achTemp );
			
			return MIME_OK;
		}
	}

	/* Try to add to the matching one */

	for ( p = pStart; p != NULL; p = p->next )
	{
		if ( strcmp( szName, p->name ) == 0 )
		{
			strcpy( achTemp, p->value );
			strcat( achTemp, szValue );

			if ( p->value != NULL )
				free( p->value );

			p->value = szStringClone( achTemp );
			
			return MIME_OK;
		}
	}

	return MIME_ERR_NOT_FOUND;
}



/*
* parameter :
*
*	p : mime header to free
*
* returns :	MIME_OK if successful
*/
int mime_header_free( mime_header_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->name != NULL )
	{
		free( p->name );
		p->name = NULL;
	}

	if ( p->value != NULL )
	{
		free( p->value );
		p->value = NULL;
	}

	free( p );

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* free all header structures 
* to a max of 256 headers
*
* parameter : pStart : starting header ( headers are linked together using a link list )
*
* returns :	MIME_OK if successful
*/
int mime_header_freeAll( mime_header_t *pStart )
{
	mime_header_t *pt[256];		/* 256 headers max */
	int i=0;
	mime_header_t *p = NULL;

	if ( pStart == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	for ( pt[0] = pStart, i = 1, p = pStart->next;
		p != NULL && p->next != NULL && i < 255;
		p = p->next, i++ )
	{
		pt[i] = p;
	}
	
	pt[i] = p;

	for ( ; i >= 0; i-- )
	{
		if ( pt[i] != NULL )
			mime_header_free( pt[i] );
	}

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* create new instance and clone of mime_header
*
* parameter :
*
*	p : mime header
*
* returns :	MIME_OK if successful
*/
mime_header_t *mime_header_clone( mime_header_t *p )
{
	mime_header_t *pNew;
	mime_header_t *p2;
	mime_header_t *pNextMimeHeader;
	BOOLEAN firstOne = TRUE;

	if ( p == NULL )
	{
		return NULL;
	}

	pNew = mime_header_new( p->name, p->value );
	
	if ( pNew != NULL )
	{
		for (	p2 = pNew, pNextMimeHeader = p->next;
				pNextMimeHeader != NULL;
				p2 = p2->next, pNextMimeHeader = pNextMimeHeader->next )
		{
		     if (pNextMimeHeader == NULL)
			  break;

		     p2->next = mime_header_new( pNextMimeHeader->name, pNextMimeHeader->value );
		}
	}

	return pNew;
}




/* ----------------------------------- */



/*
* Constructor.
* Return NULL if unsuccessful.
*
* parameter :
*
* returns :	new basicPart
*/
mime_basicPart_internal_t *mime_basicPart_internal_new()
{
	mime_basicPart_internal_t *p = (mime_basicPart_internal_t *) 
					malloc( sizeof( mime_basicPart_internal_t ) );
	
	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_basicPart_internal_t));

	p->pTheDataStream = NULL;
	p->szMessageBody = NULL;
	p->nMessageSize = 0;
	p->nRawMessageSize = 0;
	p->nStartMessageDataIndex = -1;
	p->nEndMessageDataIndex = -1;
	p->bDecodedData = FALSE;
	p->pUserObject = NULL;
	p->nDynamicBufferSize = 0;

	return p;
}



/*
* Destructor.
*
* parameter :
*
*	p : basicPart internal
*
* returns :	MIME_OK if successful
*/
int mime_basicPart_internal_free (mime_basicPart_internal_t * p)
{
	if (p == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (p->szMessageBody != NULL)
	{
		free (p->szMessageBody);
		p->szMessageBody = NULL;
	}

	if (p->pTheDataStream != NULL)
	{
		p->pTheDataStream->close (p->pTheDataStream->rock);
		p->pTheDataStream = NULL;
	}

	free (p);

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* Free message pointed to by the "pNext" pointer.
*
* parameter :
*
* returns :	MIME_OK if successful
*/
/*
int next_free( void *p, mime_content_type nContentType )
{
	if ( p == NULL )
	{
#ifdef DEBUG
		errorLog( "next_free()", MIME_ERR_INVALIDPARAM );
#endif
		return MIME_ERR_INVALIDPARAM;
	}

	if ( nContentType == MIME_CONTENT_MESSAGEPART )
		mime_messagePart_free( (mime_messagePart_t *) p );

	else if ( nContentType == MIME_CONTENT_MULTIPART )
		mime_multiPart_free( (mime_multiPart_t *) p );

	else if ( bIsBasicPart( nContentType ) )
		mime_basicPart_free( (mime_basicPart_t *) p );

	return MIME_OK;
}
*/



/*
* parameter :
*
*	p : original mime_basicPart_internal
*
* returns :	new mime_basicPart_internal
*/
mime_basicPart_internal_t * mime_basicPart_internal_clone (mime_basicPart_internal_t *p)
{
	mime_basicPart_internal_t *pNew = NULL;
	void *buffer;

	if ( p == NULL )
	{
		return NULL;
	}

	pNew = mime_basicPart_internal_new();

	if ( pNew != NULL )
	{
		if (p->szMessageBody != NULL)
		{
			buffer = (void *) malloc( p->nMessageSize + 1 );

			if ( buffer == NULL )
			{
				mime_basicPart_internal_free( pNew );

				return NULL;
			}

			memset (buffer, 0, p->nMessageSize + 1 );
			memcpy( buffer, p->szMessageBody, p->nMessageSize );
			pNew->szMessageBody = buffer;
		}
		else
			 pNew->szMessageBody = NULL;

		pNew->nMessageSize = p->nMessageSize;
		pNew->nRawMessageSize = p->nRawMessageSize;
		pNew->nStartMessageDataIndex = p->nStartMessageDataIndex;
		pNew->nEndMessageDataIndex = p->nEndMessageDataIndex;
		pNew->pUserObject = p->pUserObject;
	}

	return pNew;
}



/*
* carsonl, jan 8,98 
* set dynamic buffer size
*
* parameter : 
*
*	p : mime parser
*	nSize : new buffer size
*
* returns : none
* NOTE : this is useful for speed optimization if you know what the message size ahead of time
*/
/*
void setDynamicBufferSize( mimeParser_t *p, int nSize )
{
	char *szNewBuffer;

	if ( p == NULL && nSize <= 0 )
	{
#ifdef DEBUG
		errorLog( "setDynamicBufferSize()", MIME_ERR_INVALIDPARAM );
#endif
		return;
	}

	if ( p->szDynamicBuffer == NULL )
	{
		p->szDynamicBuffer = (char*) malloc( nSize );

		if ( p->szDynamicBuffer == NULL )
		{
#ifdef DEBUG
			errorLog( "setDynamicBufferSize()", MIME_ERR_OUTOFMEMORY );
#endif
		}

		p->nDynamicBufferSize = nSize;
	}

	szNewBuffer = (char*) malloc( nSize );

	if ( szNewBuffer == NULL )
	{
#ifdef DEBUG
		errorLog( "setDynamicBufferSize()", MIME_ERR_OUTOFMEMORY );
#endif
		return;
	}

	memcpy( szNewBuffer, p->szDynamicBuffer, p->nDynamicBufferSize > nSize ? nSize : p->nDynamicBufferSize );

	p->szDynamicBuffer = szNewBuffer;
	p->nDynamicBufferSize = nSize;
}
*/



/* ----------------------------------- */




/*
* Constructor 
* return NULL if unsuccessful.
*
* parameter :
*
* returns :	new instance of basicPart
*/
mime_basicPart_t *mime_basicPart_new()
{
	mime_basicPart_t *p = (mime_basicPart_t *) malloc( sizeof( mime_basicPart_t ) );

	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_basicPart_t));

	p->content_description = NULL;
	p->content_disposition = MIME_DISPOSITION_UNINITIALIZED;
	p->content_disp_params = NULL;
	p->content_type = MIME_BASICPART;
	p->content_subtype = NULL;
	p->content_type_params = NULL;
	p->contentID = NULL;
	p->extra_headers = NULL;
	p->contentMD5 = NULL;
	p->encoding_type = MIME_ENCODING_UNINITIALIZED;
	p->pInternal = (void *) mime_basicPart_internal_new();

	return p;
}




/*
* destructor 
*
* parameter :
*
*	p : basicPart
*
* returns :	MIME_OK if successful
* NOTE : will also free it's internal structure as well
*/
int mime_basicPart_free (mime_basicPart_t *p)
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->content_description != NULL )
	{
		free (p->content_description);
		p->content_description = NULL;
	}

	if ( p->content_disp_params != NULL )
	{
		free( p->content_disp_params );
		p->content_disp_params = NULL;
	}

	if ( p->content_subtype != NULL )
	{
		free( p->content_subtype );
		p->content_subtype = NULL;
	}

	if ( p->content_type_params != NULL )
	{
		free( p->content_type_params );
		p->content_type_params = NULL;
	}

	if ( p->contentID != NULL )
	{
		free( p->contentID );
		p->contentID = NULL;
	}

	if ( p->extra_headers != NULL )
	{
		mime_header_freeAll( p->extra_headers );
		p->extra_headers = NULL;
	}
	
	if ( p->contentMD5 != NULL )
	{
		free( p->contentMD5 );
		p->contentMD5 = NULL;
	}

	if ( p->pInternal != NULL )
	{
		mime_basicPart_internal_free( (mime_basicPart_internal_t *) p->pInternal );
		p->pInternal = NULL;
	}

	free( p );

	return MIME_OK;
}




/*
* carsonl, jan 8,98 
* Read from input stream into a basicPart
*
* parameter :
*
*	pInput_stream : input stream
*	ppBasicPart : (output) basicPart
*
* returns :	MIME_OK if successful
*/
/*
int mime_basicPart_decode( nsmail_inputstream_t *pInput_stream, mime_basicPart_t **ppBasicPart)
{
	mimeParser_t *p;
	int nRet = MIME_ERR_OUTOFMEMORY;

	if ( pInput_stream == NULL || ppBasicPart == NULL )
	{
#ifdef DEBUG
		errorLog( "mime_basicPart_decode()", MIME_ERR_INVALIDPARAM );
#endif
		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal();

	if ( p != NULL )
	{
		p->bDeleteMimeMessage = FALSE;
		nRet = mimeParser_parseMimeMessage(p, pInput_stream, NULL, 0, MIME_CONTENT_TEXT, (void **) ppBasicPart);
		mimeParser_free (p);
	}

	return nRet;
}
*/



/*
* Create new instance and clone
*
* parameter :
*
*	p : original basicPart
*
* returns :	new instance of basicPart
*/
mime_basicPart_t *mime_basicPart_clone( mime_basicPart_t *p )
{
	mime_basicPart_t *pNew = NULL;

	outmsg("In mime_basicPart_clone()");

	if ( p == NULL )
	{
		return NULL;
	}

	outmsg("doing mime_basicPart_new()");

	pNew = mime_basicPart_new();

	if ( pNew != NULL )
	{
		outmsg("Copying content* fileds etc. to new basicPart");

		pNew->content_description = szStringClone( p->content_description );
		pNew->content_disposition = p->content_disposition;
		pNew->content_disp_params = szStringClone( p->content_disp_params );
		pNew->content_type = p->content_type;
		pNew->content_subtype = szStringClone( p->content_subtype );
		pNew->content_type_params = szStringClone( p->content_type_params );
		pNew->contentID = szStringClone( p->contentID );
		pNew->extra_headers = mime_header_clone( p->extra_headers );
		pNew->contentMD5 = szStringClone( p->contentMD5 );
		pNew->encoding_type = p->encoding_type;

		outmsg("doing mime_basicPart_new()");

		pNew->pInternal = mime_basicPart_internal_clone( p->pInternal );
	}

	return pNew;
}



/* ----------------------------------- */




/*
* Constructor.
* Return NULL if unsuccessful.
*
* parameter :
*
* returns :	new instance of messagePart
*/
mime_messagePart_t *mime_messagePart_new()
{
	mime_messagePart_t *p = (mime_messagePart_t *) malloc( sizeof( mime_messagePart_t ) );
	
	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_messagePart_t));

	p->content_description = NULL;
	p->content_disposition = MIME_DISPOSITION_UNINITIALIZED;
	p->encoding_type = MIME_ENCODING_UNINITIALIZED;
	p->content_disp_params = NULL;
	p->content_type = MIME_MESSAGEPART;
	p->content_subtype = NULL;
	p->content_type_params = NULL;
	p->contentID = NULL;
	p->extra_headers = NULL;
	p->pInternal = (void *) mime_messagePart_internal_new();

	return p;
}


/*
* destructor
*
* parameter :
*
*	p : messagePart to free
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_free( mime_messagePart_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->content_description != NULL )
	{
		free (p->content_description);
		p->content_description = NULL;
	}

	if ( p->content_disp_params != NULL )
	{
		free( p->content_disp_params );
		p->content_disp_params = NULL;
	}

	if ( p->content_subtype != NULL )
	{
		free( p->content_subtype );
		p->content_subtype = NULL;
	}

	if ( p->content_type_params != NULL )
	{
		free( p->content_type_params );
		p->content_type_params = NULL;
	}

	if ( p->contentID != NULL )
	{
		free( p->contentID );
		p->contentID = NULL;
	}

	if ( p->extra_headers != NULL )
	{
		mime_header_freeAll( p->extra_headers );
		p->extra_headers = NULL;
	}
	
	if ( p->pInternal != NULL )
	{
		mime_messagePart_internal_free ((mime_messagePart_internal_t *) p->pInternal);
		p->pInternal = NULL;
	}

	free( p );

	return MIME_OK;
}



/*
* read from input stream into a messagepart
*
* parameter :
*
*	pInput_stream : input stream
*	ppMessagePart : (output) decoded and populated messagePart
*
* returns :	MIME_OK if successful
*/
/*
int mime_messagePart_decode( nsmail_inputstream_t *pInput_stream, mime_messagePart_t ** ppMessagePart)
{
	mimeParser_t *p;
	int nRet = MIME_ERR_OUTOFMEMORY;

	if ( pInput_stream == NULL || ppMessagePart == NULL )
	{
#ifdef DEBUG
		errorLog( "mime_messagePart_decode()", MIME_ERR_INVALIDPARAM );
#endif
		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal();

	if ( p != NULL )
	{
		p->bDeleteMimeMessage = FALSE;
		nRet = mimeParser_parseMimeMessage (p, pInput_stream, NULL, 0, MIME_CONTENT_MESSAGEPART, (void **) ppMessagePart);

		mimeParser_free( p );
	}

	return nRet;
}
*/



/*
* parameter :
*
*	p : original messagePart
*
* returns :	new instance of messagePart
*/
mime_messagePart_t *mime_messagePart_clone (mime_messagePart_t * p)
{
	mime_messagePart_t *pNew = NULL;

	if (p == NULL)
	{
		return NULL;
	}

	pNew = mime_messagePart_new();

	if ( pNew != NULL )
	{
		pNew->content_description = szStringClone( p->content_description );
		pNew->content_disposition = p->content_disposition;
		pNew->content_disp_params = szStringClone( p->content_disp_params );
		pNew->content_type = p->content_type;
		pNew->content_subtype = szStringClone( p->content_subtype );
		pNew->content_type_params = szStringClone( p->content_type_params );
		pNew->contentID = szStringClone( p->contentID );
		pNew->encoding_type = p->encoding_type;
		pNew->extra_headers = mime_header_clone( p->extra_headers );
		pNew->extern_headers = mime_header_clone( p->extern_headers );
		pNew->pInternal = mime_messagePart_internal_clone( p->pInternal );
	}

	return pNew;
}



/*
* carsonl, jan 8,98 
* test if messagePart have a part attached to itself 
*
* parameter :
*
*	pMessagePart : messagePart
*
* returns :	return TRUE if have part, FALSE if no part
*/
BOOLEAN mime_messagePart_isEmpty( mime_messagePart_t *pMessagePart )
{
	mime_messagePart_internal_t *p;

	if ( pMessagePart == NULL )
	{
		return TRUE;
	}

	p = (mime_messagePart_internal_t *) pMessagePart->pInternal;

	if ( p != NULL && p->pTheMessage != NULL )
	{
		return mime_message_isEmpty( p->pTheMessage );
	}

	return TRUE;
}




/* ----------------------------------- */




/*
* parameter : none
*
* returns :	new instance if successful, Return NULL if unsuccessful.
*/
mime_messagePart_internal_t *mime_messagePart_internal_new()
{
	mime_messagePart_internal_t *p = (mime_messagePart_internal_t *) 
						malloc (sizeof (mime_messagePart_internal_t));
	
	if (p == NULL)
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_messagePart_internal_t));

	p->pTheMessage = NULL;
	p->pUserObject = NULL;

	return p;
}




/*
* parameter :
*
*	p : messagePart_internal
*
* returns :	MIME_OK if successful
*/
int mime_messagePart_internal_free (mime_messagePart_internal_t * p)
{
	if (p == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (p->pTheMessage != NULL)
	{
		mime_message_free (p->pTheMessage);
		p->pTheMessage = NULL;
	}

	free (p);

	return MIME_OK;
}




/*
* parameter :
*
*	p : original messagePart_internal
*
* returns :	new instance of messagePart_internal
*/
mime_messagePart_internal_t * mime_messagePart_internal_clone (mime_messagePart_internal_t * p)
{
	mime_messagePart_internal_t *pNew = NULL;

	if (p == NULL)
	{
		return NULL;
	}

	pNew = mime_messagePart_internal_new();

	if (pNew == NULL)
	{
		return NULL;
	}

	if (p->pTheMessage != NULL)
	{
		pNew->pTheMessage = mime_message_clone (p->pTheMessage);
		pNew->pUserObject = p->pUserObject;
	}

	return pNew;
}




/* ----------------------------------- */



/*
* constructor
*
* parameter :
*
* returns :	new instance, NULL if unsuccessful
*/
mime_multiPart_t *mime_multiPart_new()
{
	mime_multiPart_t *p = (mime_multiPart_t *) malloc( sizeof( mime_multiPart_t ) );
	mime_multiPart_internal_t * pInternal;
	
	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_multiPart_t));

	p->content_description = NULL;
	p->content_disposition = MIME_DISPOSITION_UNINITIALIZED;
	p->content_disp_params = NULL;
	p->content_type = MIME_MULTIPART;
	p->content_subtype = NULL;
	p->content_type_params = NULL;
	p->contentID = NULL;
	p->encoding_type = MIME_ENCODING_UNINITIALIZED;
	p->preamble = NULL;
	p->extra_headers = NULL;
	p->pInternal = (void *) mime_multiPart_internal_new();
	pInternal = (mime_multiPart_internal_t *) p->pInternal;
	pInternal->fParsedPart = FALSE;

	return p;
}



/*
* destructor
*
* parameter :
*
*	p : multiPart
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_free( mime_multiPart_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->content_description != NULL )
	{
		free( p->content_description );
		p->content_description = NULL;
	}

	if ( p->content_disp_params != NULL )
	{
		free( p->content_disp_params );
		p->content_disp_params = NULL;
	}

	if ( p->content_subtype != NULL )
	{
		free( p->content_subtype );
		p->content_subtype = NULL;
	}

	if ( p->content_type_params != NULL )
	{
		free( p->content_type_params );
		p->content_type_params = NULL;
	}

	if ( p->contentID != NULL )
	{
		free( p->contentID );
		p->contentID = NULL;
	}

	if (p->preamble != NULL);
	{
		free(p->preamble);
		p->preamble = NULL;
	}

	if ( p->extra_headers != NULL )
	{
		mime_header_freeAll( p->extra_headers );
		p->extra_headers = NULL;
	}
	
	if ( p->pInternal != NULL )
	{
		mime_multiPart_internal_free( (mime_multiPart_internal_t *) p->pInternal );
		p->pInternal = NULL;
	}

	free( p );

	return MIME_OK;
}




/*
* carsonl, jan 8,98 
* read from inputstream into a multipart
*
* parameter :
*
*	pInput_stream : input stream
*	ppMultiPart : (output) new instance of multipart, populated with data from inputstream
*
* returns :	MIME_OK if successful
*/
/*
int mime_multiPart_decode( nsmail_inputstream_t *pInput_stream, mime_multiPart_t ** ppMultiPart)
{
	mimeParser_t *p;
	int nRet;

	if ( pInput_stream == NULL || ppMultiPart == NULL )
	{
#ifdef DEBUG
		errorLog( "mime_multiPart_decode()", MIME_ERR_INVALIDPARAM );
#endif
		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal();

	if ( p != NULL )
	{
		p->bDeleteMimeMessage = FALSE;
		nRet = mimeParser_parseMimeMessage (p, pInput_stream, NULL, 0, MIME_CONTENT_MULTIPART, (void **) ppMultiPart);

		mimeParser_free (p);
		return nRet;
	}

	return MIME_ERR_OUTOFMEMORY;
}
*/



/*
* create new instance and clone
*
* parameter :
*
*	p : multiPart
*
* returns :	MIME_OK new instance of multipart
*/
mime_multiPart_t *mime_multiPart_clone( mime_multiPart_t *p )
{
	mime_multiPart_t *pNew = NULL;

	if ( p == NULL )
	{
		return NULL;
	}

	pNew = mime_multiPart_new();

	if ( pNew != NULL )
	{
		pNew->content_description = szStringClone( p->content_description );
		pNew->content_disposition = p->content_disposition;
		pNew->content_disp_params = szStringClone( p->content_disp_params );
		pNew->content_type = p->content_type;
		pNew->content_subtype = szStringClone( p->content_subtype );
		pNew->content_type_params = szStringClone( p->content_type_params );
		pNew->contentID = szStringClone( p->contentID );
		pNew->preamble = szStringClone( p->preamble );
		pNew->encoding_type = p->encoding_type;
		pNew->extra_headers = mime_header_clone( p->extra_headers );
		pNew->pInternal = mime_multiPart_internal_clone( p->pInternal );
	}

	return pNew;
}



/*
* Prasad, jan 8,98 
*  Add a part to multipart
*  Uses new mime_mp_partInfo_t of mime_multiPart_internal_t
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addPart (mime_multiPart_t *pMultiPart,
				void * pMessage,
				mime_content_type nContentType,
				int * pIndex_assigned )
{
	return mime_multiPart_addPart_clonable (pMultiPart, pMessage, 
						nContentType, TRUE, pIndex_assigned);
}


				
/*
* Prasad, jan 8,98 
* Add a part to multipart
* Uses new mime_mp_partInfo_t of mime_multiPart_internal_t
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_addPart_clonable (mime_multiPart_t *pMultiPart,
				void * pMessage,
				mime_content_type nContentType,
				BOOLEAN clone,
				int * pIndex_assigned)
{
	mime_multiPart_internal_t *pMultiPartInternal;
	mime_mp_partInfo_t * pPartInfo;

	mime_basicPart_t   * pBasicPart2;
	mime_multiPart_t   * pMultiPart2;
	mime_messagePart_t * pMessagePart2;

	if ( pMultiPart == NULL || pMessage == NULL || pIndex_assigned == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pMultiPart->pInternal == NULL)
	{
		/* Allocate a new one */
		pMultiPart->pInternal = mime_multiPart_internal_new();
	}

	pMultiPartInternal = (mime_multiPart_internal_t *) pMultiPart->pInternal;
	*pIndex_assigned = ++(pMultiPartInternal->nPartCount);

	pPartInfo = &pMultiPartInternal->partInfo [*pIndex_assigned];
	pPartInfo->nContentType = nContentType;
	pPartInfo->pThePart = clone ? mime_clone_any_part (pMessage, nContentType) : pMessage;
	
	if ( bIsBasicPart( nContentType ) )
	{
		pBasicPart2 = (mime_basicPart_t *) pMessage;

		if (pBasicPart2->contentID != NULL)
			pPartInfo->contentID = strdup (pBasicPart2->contentID);
		else
			pPartInfo->contentID = NULL;
	}

	else if ( nContentType == MIME_CONTENT_MULTIPART )
	{
		pMultiPart2 = (mime_multiPart_t *) pMessage;

		if (pMultiPart2->contentID != NULL)
			pPartInfo->contentID = strdup (pMultiPart2->contentID);
		else
			pPartInfo->contentID = NULL;
	}

	else if ( nContentType == MIME_CONTENT_MESSAGEPART )
	{
		pMessagePart2 = (mime_messagePart_t *) pMessage;

		if (pMessagePart2->contentID != NULL)
			pPartInfo->contentID = strdup (pMessagePart2->contentID);
		else
			pPartInfo->contentID = NULL;
	}

	return MIME_OK;
}




/*
* Helper function to get part of a multipart message
*
* Will use contentID if index < 0
* else use index if contentID == NULL
*
* parameter :
*
*	pMultiPart : multiPart
*	index : which part ( offset of zero ) do you want to retrieve
*	contentID : (output) content ID of part
*	pContentType : (output) content type of part
*	ppTheBodyPart : (output) the part itself
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_getPart2 (mime_multiPart_t *pMultiPart,
				int  index,
				char *contentID,
				mime_content_type *pContentType,
				void **ppTheBodyPart)
{
	mime_content_type nContentType;
	mime_mp_partInfo_t * pPartInfo;
	mime_multiPart_internal_t * pMultiPartInternal;

	int nIndex;
	BOOLEAN bTestForIndex;
	BOOLEAN bTestForContentID;

	if (pMultiPart == NULL || pContentType == NULL || ppTheBodyPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	bTestForIndex = index > 0 ? TRUE : FALSE;
	bTestForContentID = !bTestForIndex && contentID != NULL ? TRUE : FALSE;

	if (bTestForIndex == FALSE && bTestForContentID == FALSE)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	pMultiPartInternal = (mime_multiPart_internal_t *) pMultiPart->pInternal;

	if (pMultiPartInternal == NULL)
	{
		return MIME_ERR_UNINITIALIZED;
	}

	/* make sure index is not out of range */
	if (bTestForIndex == TRUE && pMultiPartInternal->nPartCount < index)
	{
		return MIME_ERR_INVALID_INDEX;
	}

	if (bTestForIndex)   /* return based on Index */
	{
		pPartInfo = &pMultiPartInternal->partInfo [index];
		nContentType = pPartInfo->nContentType;
		*pContentType = nContentType;

		if ( bIsBasicPart( nContentType ) )
		{
		  *ppTheBodyPart = mime_basicPart_clone ((mime_basicPart_t *) pPartInfo->pThePart);
		  return MIME_OK;
		}

		else if ( nContentType == MIME_CONTENT_MULTIPART )
		{
		  *ppTheBodyPart = mime_multiPart_clone ((mime_multiPart_t *) pPartInfo->pThePart);
		  return MIME_OK;
		}

		else if ( nContentType == MIME_CONTENT_MESSAGEPART )
		{
		  *ppTheBodyPart = mime_messagePart_clone( (mime_messagePart_t *) pPartInfo->pThePart );
		  return MIME_OK;
		}

		else
		{
			return MIME_ERR_NOT_FOUND;
		}

	} /* bTestForIndex */

	/* match based on contentID and return */

	for (nIndex = 1; nIndex <= pMultiPartInternal->nPartCount; nIndex++)
	{
		pPartInfo = &pMultiPartInternal->partInfo [nIndex];

		if (bStringEquals (pPartInfo->contentID, contentID))
		{
			*pContentType = pPartInfo->nContentType;

			if ( bIsBasicPart( pPartInfo->nContentType ) )
			{
				 *ppTheBodyPart = mime_basicPart_clone (pPartInfo->pThePart);
				 return MIME_OK;
			}

			else if ( pPartInfo->nContentType == MIME_CONTENT_MULTIPART )
			{
				 *ppTheBodyPart = mime_multiPart_clone (pPartInfo->pThePart);
				 return MIME_OK;
			}

			else if ( pPartInfo->nContentType == MIME_CONTENT_MESSAGEPART )
			{
				*ppTheBodyPart = mime_messagePart_clone (pPartInfo->pThePart);
				return MIME_OK;
			}

			else
			{
				return MIME_ERR_UNEXPECTED;
			}

		} /* if */
	}

	return MIME_ERR_NOT_FOUND;
}



/* ----------------------------------- */




/*
* Constructor 
*
* parameter : none
*
* returns :	new instance of multiPart_internal, NULL if unsuccessful.
*/
mime_multiPart_internal_t *mime_multiPart_internal_new()
{
	mime_multiPart_internal_t * p = (mime_multiPart_internal_t *) 
					 malloc (sizeof (mime_multiPart_internal_t));
	
	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_multiPart_internal_t));

	p->nPartCount = 0;
	p->szBoundary = NULL;
	p->pUserObject = NULL;

	return p;
}



/*
* destructor 
*
* parameter :
*
*	p : multiPart_internal
*
* returns :	MIME_OK if successful
*/
int mime_multiPart_internal_free (mime_multiPart_internal_t * p)
{
	int nIndex;
	mime_mp_partInfo_t * pPartInfo;

	if (p == NULL)
	{
	    return MIME_OK; 
	}

	if (p->nPartCount > 0)
	{
		for (nIndex = 1; nIndex <= p->nPartCount; nIndex++)
		{
			pPartInfo = &p->partInfo [nIndex];

		        if (pPartInfo->contentID != NULL)
			{
			    free (pPartInfo->contentID);
			    pPartInfo->contentID = NULL;
			}

			if ( bIsBasicPart( pPartInfo->nContentType ) )
				mime_basicPart_free ((mime_basicPart_t *) pPartInfo->pThePart);
			
			else if ( pPartInfo->nContentType == MIME_CONTENT_MULTIPART )
				mime_multiPart_free ((mime_multiPart_t *) pPartInfo->pThePart);

			else if ( pPartInfo->nContentType == MIME_CONTENT_MESSAGEPART )
				mime_messagePart_free ((mime_messagePart_t *) pPartInfo->pThePart);

			else
			{
				return MIME_ERR_UNEXPECTED;
			}

			pPartInfo->pThePart = NULL;

		} /* for */
	} /* if */

	if ( p->szBoundary != NULL )
		free( p->szBoundary );
	
	free (p);

	return MIME_OK; 
}




/*
* Clone a new instance  of multiPart_internal 
*
* parameter :
*
*	p : original multiPart_internal
*
* returns :	new instance of multiPart_internal
*/
mime_multiPart_internal_t * mime_multiPart_internal_clone (mime_multiPart_internal_t * p)
{
	int nIndex;
	mime_mp_partInfo_t * pPartInfo;
	mime_mp_partInfo_t * pPartInfoNew;
	mime_multiPart_internal_t *pNew = NULL;

	if (p == NULL)
	{
		return NULL;
	}

	pNew = mime_multiPart_internal_new();

	if (pNew == NULL)
		return NULL;

	pNew->nPartCount = p->nPartCount;
	pNew->szBoundary = szStringClone( p->szBoundary );
	pNew->pUserObject = p->pUserObject;

	if (p->nPartCount > 0)
	{
		for (nIndex = 1; nIndex <= p->nPartCount; nIndex++)
		{
			pPartInfo = &p->partInfo [nIndex];
			pPartInfoNew = &pNew->partInfo [nIndex];

			pPartInfoNew->nContentType = pPartInfo->nContentType;

			if (pPartInfo->contentID !=  NULL)
			{
				pPartInfoNew->contentID = strdup (pPartInfo->contentID);
			}

			if ( bIsBasicPart( pPartInfo->nContentType ) )
			     pPartInfoNew->pThePart = (void *) mime_basicPart_clone ((mime_basicPart_t *) pPartInfo->pThePart);
			
			else if ( pPartInfo->nContentType == MIME_CONTENT_MULTIPART )
                 pPartInfoNew->pThePart = (void *) mime_multiPart_clone ((mime_multiPart_t *) pPartInfo->pThePart);

			else if ( pPartInfo->nContentType == MIME_CONTENT_MESSAGEPART )
                 pPartInfoNew->pThePart = (void *) mime_messagePart_clone ((mime_messagePart_t *) pPartInfo->pThePart);

			else
			     return pNew;

		} /* for */
	} /* if */

	return pNew;
}



/* ----------------------------------- */



/* ORIGINAL ONE */
/* return NULL if unsuccessful 
 * mime_message_t *mime_message_new()
 * {
 *	mime_message_t *pMimeMessage = (mime_message_t *) malloc( sizeof( mime_message_t ) );
 *
 *	if ( pMimeMessage != NULL )
 *	{
 *		pMimeMessage->rfc822_headers = NULL;
 *		pMimeMessage->pInternal = (void *) mime_message_internal_new();
 *	}
 *
 *	return pMimeMessage;
 * }
 */


/* New one! Moved to mime.c    */
/* return NULL if unsuccessful */
/*
 * mime_message_t *mime_message_new( mimeDataSink_t *pDataSink )
 * {
 *	mime_message_internal_t *pMimeMessageInternal = NULL;
 *	mime_message_t *pMimeMessage = (mime_message_t *) malloc( sizeof( mime_message_t ) );
 *
 *	if ( pMimeMessage != NULL )
 *	{
 *		pMimeMessage->rfc822_headers = NULL;
 *		pMimeMessage->pInternal = (void *) mime_message_internal_new();
 *
 *		pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;
 *
 *		if ( pMimeMessageInternal != NULL )
 *			pMimeMessageInternal->pDataSink = pDataSink;
 *	}
 *
 *	return pMimeMessage;
 * }
 */


/*
* destructor
*
* parameter :
*
*	p : mime message
*
* returns :	MIME_OK if successful
*/
int mime_message_free( mime_message_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->rfc822_headers != NULL )
	{
		mime_header_freeAll( p->rfc822_headers );
		p->rfc822_headers = NULL;
	}

	if ( p->pInternal != NULL )
	{
		mime_message_internal_free( (mime_message_internal_t *) p->pInternal );
		p->pInternal = NULL;
	}

	free( p );

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* read from inputstream into a mime_message
*
* parameter :
*
*	pInput_stream : input stream
*	ppMessage : (output) new instance of mime message
*
* returns :	MIME_OK if successful
*/
/*
int mime_message_decode( nsmail_inputstream_t *pInput_stream, mime_message_t ** ppMessage)
{
	mimeParser_t *p;
	int nRet = MIME_ERR_OUTOFMEMORY;

	if ( pInput_stream == NULL || ppMessage == NULL )
	{
#ifdef DEBUG
		errorLog( "mime_message_decode()", MIME_ERR_INVALIDPARAM );
#endif
		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal();

	if ( p != NULL )
	{
		p->bDeleteMimeMessage = FALSE;
		nRet = mimeParser_parseMimeMessage (p, pInput_stream, NULL, 0, 0, (void **)ppMessage);
		mimeParser_free( p );
	}

	return nRet;
}
*/



/*
* create new instance and clone
*
* parameter :
*
*	p : original mime message
*
* returns :	new instance of mime message
*/
struct mime_message *mime_message_clone( struct mime_message *p )
{
	mime_message_t *pNew = NULL;

	if ( p == NULL )
	{
		return NULL;
	}

	pNew = mime_message_new( NULL );

	if ( pNew != NULL )
	{
		pNew->rfc822_headers = mime_header_clone( p->rfc822_headers );
		pNew->pInternal = mime_message_internal_clone( p->pInternal );
	}

	return pNew;
}



/* ----------------------------------- */



/*
* constructor
*
* parameter :
*
* returns :	new instance, NULL if unsuccessful
*/
mime_message_internal_t *mime_message_internal_new()
{
	mime_message_internal_t *p = (mime_message_internal_t *) malloc( sizeof( mime_message_internal_t ) );
	
	if ( p == NULL )
	{
		return NULL;
	}
	else
	     memset (p, 0, sizeof (mime_message_internal_t));

	p->pMimeBasicPart = NULL;
	p->pMimeMultiPart = NULL;
	p->pMimeMessagePart = NULL;
	p->pUserObject = NULL;
	p->pDataSink = NULL;
/*	p->pParser = NULL;	*/

	return p;
}



/*
* destructor 
*
* parameter :
*
*	p : mime message internal
*
* returns :	MIME_OK if successful
*/
int mime_message_internal_free( mime_message_internal_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->pMimeBasicPart != NULL )
	{
		mime_basicPart_free( p->pMimeBasicPart );
		p->pMimeBasicPart = NULL;
	}

	if ( p->pMimeMultiPart != NULL )
	{
		mime_multiPart_free( p->pMimeMultiPart );
		p->pMimeMultiPart = NULL;
	}

	if ( p->pMimeMessagePart != NULL )
	{
		mime_messagePart_free( p->pMimeMessagePart );
		p->pMimeMessagePart = NULL;
	}
/*
	if ( p->pParser != NULL )
	{
		mimeParser_free( p->pParser );
		p->pParser = NULL;
	}
*/
	free( p );

	return MIME_OK;
}



/*
* create new instance and clone
*
* parameter :
*
*	p : original mime message internal
*
* returns :	new instance of mime message internal
*/
mime_message_internal_t *mime_message_internal_clone( mime_message_internal_t *p )
{
	mime_message_internal_t *pNew = NULL;

	if ( p == NULL )
	{
		return NULL;
	}

	pNew = mime_message_internal_new();

	if ( pNew != NULL )
	{
		if ( p->pMimeBasicPart != NULL )
			pNew->pMimeBasicPart = mime_basicPart_clone( p->pMimeBasicPart );
		
		else if ( p->pMimeMultiPart != NULL )
			pNew->pMimeMultiPart = mime_multiPart_clone( p->pMimeMultiPart );
		
		else if ( p->pMimeMessagePart != NULL )
			pNew->pMimeMessagePart = mime_messagePart_clone( p->pMimeMessagePart );

		pNew->pUserObject = p->pUserObject;
		pNew->pDataSink = p->pDataSink;
	}

	return pNew;
}


/* Prasad
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addBasicPart_clonable( mime_message_t * pMessage, mime_basicPart_t * pBasicPart, BOOLEAN clone )
{
	mime_message_internal_t * pInternal;

	outmsg("In mime_message_addBasicPart_clonable");

	if (pMessage == NULL || pBasicPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pMessage->pInternal == NULL)
	{
		outmsg("pMessage->pInternal == NULL");

		/* allocate a new one. Never had a body */
		pMessage->pInternal = (void *) mime_message_internal_new();

		if (pMessage->pInternal == NULL)
		{
			return MIME_ERR_OUTOFMEMORY;
		}
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;
		
	if (pInternal->pMimeBasicPart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	if (pInternal->pMimeMultiPart != NULL || pInternal->pMimeMessagePart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	outmsg("Adding the basicPart to the message");

	pInternal->pMimeBasicPart = clone ? mime_basicPart_clone (pBasicPart) : pBasicPart;

	if (pInternal->pMimeBasicPart == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}
	
	outmsg("returning from mime_message_addBasicPart_clonable");

	return MIME_OK;
}


/* Prasad
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addMessagePart_clonable( mime_message_t * pMessage, mime_messagePart_t * pMessagePart, BOOLEAN clone )
{
	mime_message_internal_t * pInternal;

	if (pMessage == NULL || pMessagePart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pMessage->pInternal == NULL)
	{
		/* allocate a new one. Never had a body */
		pMessage->pInternal = (void *) mime_message_internal_new();

		if (pMessage->pInternal == NULL)
		{
			return MIME_ERR_OUTOFMEMORY;
		}
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;
		
	if (pInternal->pMimeBasicPart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	if (pInternal->pMimeMultiPart != NULL || pInternal->pMimeMessagePart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	pInternal->pMimeMessagePart = clone ? mime_messagePart_clone (pMessagePart) : pMessagePart ;

	if (pInternal->pMimeMessagePart == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}
	
	return MIME_OK;
}


/* Prasad
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_message_addMultiPart_clonable( mime_message_t * pMessage, mime_multiPart_t * pMultiPart, BOOLEAN clone )
{
	mime_message_internal_t * pInternal;

	if (pMessage == NULL || pMultiPart == NULL)
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if (pMessage->pInternal == NULL)
	{
		/* allocate a new one. Never had a body */
		pMessage->pInternal = (void *) mime_message_internal_new();

		if (pMessage->pInternal == NULL)
		{
			return MIME_ERR_OUTOFMEMORY;
		}
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;
		
	if (pInternal->pMimeBasicPart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	if (pInternal->pMimeMultiPart != NULL || pInternal->pMimeMessagePart != NULL)
	{
		return MIME_ERR_ALREADY_SET;
	}

	pInternal->pMimeMultiPart = clone ? mime_multiPart_clone (pMultiPart) : pMultiPart;

	if (pInternal->pMimeMultiPart == NULL)
	{
		return MIME_ERR_OUTOFMEMORY;
	}
	
	return MIME_OK;
}



/* test is there is any part inside this message
* return TRUE if have parts
* return FALSE if empty or no parts
*/
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
BOOLEAN mime_message_isEmpty( mime_message_t *pMessage )
{
	mime_message_internal_t * pInternal;

	if ( pMessage == NULL )
	{
		return TRUE;
	}

	pInternal = (mime_message_internal_t *) pMessage->pInternal;

	if ( pInternal == NULL &&
		(	pInternal->pMimeBasicPart != NULL ||
			pInternal->pMimeMultiPart != NULL ||
			pInternal->pMimeMessagePart != NULL ) )

	{
		return FALSE;
	}

	return TRUE;
}




/**********************************************************************************/


	

/* translate encoding type to an integer */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_translateMimeEncodingType( char *s )
{
	if ( s == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( bStringEquals( s, "base64" ) )
		return MIME_ENCODING_BASE64;

	else if ( bStringEquals( s, "qp" ) || bStringEquals( s, "quoted-printable" ) )
		return MIME_ENCODING_QP;

	else if ( bStringEquals( s, "7bit" ) )
		return MIME_ENCODING_7BIT;

	else if ( bStringEquals( s, "8bit" ) )
		return MIME_ENCODING_8BIT;

	else if ( bStringEquals( s, "binary" ) )
		return MIME_ENCODING_BINARY;

	return MIME_ENCODING_7BIT;
}


/* translate disposition type to an integer */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mime_translateDispType( char *s, char **ppParam )
{
	if ( s == NULL || ppParam == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( bStringEquals( s, "inline" ) )
	{
		*ppParam = strlen( s ) >= 8 ? &s[7] : NULL;
		return MIME_DISPOSITION_INLINE;
	}

	else if ( bStringEquals( s, "attachment" ) )
	{
		*ppParam =  strlen( s ) >= 12 ? &s[11] : NULL;
		return MIME_DISPOSITION_ATTACHMENT;
	}

	*ppParam = strlen( s ) >= 8 ? &s[7] : NULL;

	return MIME_DISPOSITION_INLINE;
}



/* -------------------------------------------------- */



/* used by parser */
/* constructor */
/* return NULL if failed */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
mimeInfo_t *mimeInfo_new()
{
	mimeInfo_t *p = (mimeInfo_t *) malloc( sizeof( mimeInfo_t ) );

	if ( p != NULL )
	{
		p->nType = 0;
		p->szName = NULL;
		p->szValue = NULL;
		p->pVectorParam = NULL;
	}
	else
	     memset (p, 0, sizeof (mimeInfo_t));

	return p;
}



/* destructor */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mimeInfo_free( mimeInfo_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->szName != NULL )
	{
		free( p->szName );
		p->szName = NULL;
	}

	if ( p->szValue != NULL )
	{
		free( p->szValue );
		p->szValue = NULL;
	}

	if ( p->pVectorParam != NULL )
	{
		Vector_free( p->pVectorParam );
		p->pVectorParam = NULL;
	}

	if ( p != NULL )
		free( p );

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mimeInfo_init( mimeInfo_t *p, int nType1, char *szName1 )
{
	if ( p == NULL || szName1 == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	p->nType = nType1;
	p->szName = szStringClone( szName1 );
	p->pVectorParam = NULL;

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
int mimeInfo_init2( mimeInfo_t *p, int nType1, char *szName1, char *szValue1 )
{
	if ( p == NULL || szName1 == NULL || szValue1 == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	p->nType = nType1;
	p->szName = szStringClone( szName1 );
	p->szValue = szStringClone( szValue1 );
	p->pVectorParam = NULL;

	return MIME_OK;
}


/* create new instance and clone */
/*
* carsonl, jan 8,98 
*
* parameter :
*
* returns :	MIME_OK if successful
*/
mimeInfo_t *mimeInfo_clone( mimeInfo_t *p )
{
	mimeInfo_t *m = (mimeInfo_t *) malloc( sizeof( mimeInfo_t ) );

	if ( p == NULL )
	{
		free( m );

		return NULL;
	}

	if ( m == NULL )
	{
		return NULL;
	}
	else
	     memset (m, 0, sizeof (mimeInfo_t));

	m->nType = p->nType;
	
	if ( p->szName != NULL && strlen( p->szName ) > 0 )
		m->szName = szStringClone( p->szName );
	else
		m->szName = NULL;

	if ( p->szValue != NULL && strlen( p->szValue ) > 0 )
		m->szValue = szStringClone( p->szValue );
	else
		m->szValue = NULL;

	m->pVectorParam = p->pVectorParam != NULL ? Vector_clone( p->pVectorParam ) : NULL;

	return m;
}


/*
 * Prasad
 * Utility routine to Clone and returns a body-part of any-type
*
* parameter :
*
* returns :	MIME_OK if successful
*/
void * mime_clone_any_part (void * pThePart, mime_content_type nContentType)
{
	if (pThePart == NULL)
	{
	    return (void *) NULL;
	}

	if ( bIsBasicPart( nContentType ) )
		return (void *) mime_basicPart_clone ((mime_basicPart_t * ) pThePart);

	else if ( nContentType == MIME_CONTENT_MULTIPART )
		return (void *) mime_multiPart_clone ((mime_multiPart_t * ) pThePart);

	else if ( nContentType == MIME_CONTENT_MESSAGEPART )
		return (void *) mime_messagePart_clone ((mime_messagePart_t * ) pThePart);

	else
	    return (void *) NULL;

} /* mime_clone_any_part() */

/*
* carsonl, jan 8,98
* callback constructor
*
* parameter :
*
*       pDataSink : the user's datasink, NULL if don't want to use callbacks
*
* returns :     new mimeMessage
*/
mime_message_t *mime_message_new( mimeDataSink_t *pDataSink )
{
        mime_message_internal_t *pMimeMessageInternal = NULL;
        mime_message_t *pMimeMessage = (mime_message_t *) malloc( sizeof( mime_message_t ) );
 
        if ( pMimeMessage != NULL )
        {
                pMimeMessage->rfc822_headers = NULL;
                pMimeMessage->pInternal = (void *) mime_message_internal_new();
                pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;
 
                if ( pMimeMessageInternal != NULL )
                        pMimeMessageInternal->pDataSink = pDataSink;
        }
	else
	     memset (pMimeMessage, 0, sizeof (mime_message_t));
 
        return pMimeMessage;
}

