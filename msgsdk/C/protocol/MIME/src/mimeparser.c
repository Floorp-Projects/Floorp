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
* mimeParser is line based parser.  It breaks the inputstream into lines and parse line by line.
*
* carsonl, jan 8,97
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
#include "mimeparser_internal.h"

static BOOLEAN m_needEndMultiPart = FALSE;


/* --------------------------------- public functions -------------------------------- */

/*
* carsonl, jan 8,98 
* default constructor for both callback and regular version
*
* parameter :
*
*	mimeDataSink_t *pDataSink :		NULL if not using callbacks ( dynamic parsing )
*									pointer to user's datasink if using callbacks
*
* returns
*
*	NULL				not enough memory
*	mimeParser_t *		new instance of mimeParser
*/
/*int mimeDynamicParser_new( mimeDataSink_t *pDataSink, mimeParser_t **pp, BOOLEAN bDecodeData, BOOLEAN bLocalStorage )*/
int mimeDynamicParser_new( mimeDataSink_t *pDataSink, mimeParser_t **pp)
{
	mimeParser_t *p;

	if ( pDataSink == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal2( NULL );

	if ( p != NULL )
	{
		p->pDataSink = pDataSink;
		p->bParseEntireFile = FALSE;
		p->bDecodeData = FALSE;
		p->bLocalStorage = FALSE;
		/*p->bDecodeData = bDecodeData;*/
		/*p->bLocalStorage = bLocalStorage;*/

		*pp = p;
		return MIME_OK;
	}

	*pp = NULL;

	return MIME_ERR_OUTOFMEMORY;
}
    



/*
* carsonl, jan 8,98 
* ONLY FOR DYNAMIC PARSING
* signal the begin of a parse
*
* parameter :
*
*	mimeDataSink_t *pDataSink :		NULL if not using callbacks ( dynamic parsing )
*									pointer to user's datasink if using callbacks
*
* returns :	error code
*/
int beginDynamicParse( mimeParser_t *p )
{
	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* only for callbacks */
    if ( p->pDataSink == null )
		return MIME_ERR_EMPTY_DATASINK;

    p->nLeftoverBytes = 0;
    p->out_byte = 0;
    p->out_bits = 0;
	p->QPNoOfLeftOverBytes = 0;

    if ( p->pDataSink != null )
    {
        /* if (IsLocalStorage (p) && p->pDataSink->startMessageLocalStorage != null) */
        /*    setUserObject (p->pMimeMessage, MIME_MESSAGE, (p->pDataSink->startMessageLocalStorage)(p->pDataSink, p->pMimeMessage)); */
	/* else  */
	if ( p->pDataSink->startMessage != NULL )
            setUserObject( p->pMimeMessage, MIME_MESSAGE, (p->pDataSink->startMessage)(p->pDataSink) );
    }

	return MIME_OK;
}




/*
* carsonl, jan 8,98 
* ONLY FOR DYNAMIC PARSING
* parse inputstream
*
* parameter :
*
*	mimeParser_t *p :				pointer to mimeParser
*	nsmail_inputstream_t *pInStream :inputstream, user must implement this
*
* returns :	error code
* NOTE :	all data will be sent to the user via callbacks defined within the datasink
*/
int dynamicParseInputstream( mimeParser_t *p, nsmail_inputstream_t *pInStream )
{
	int len, ret;
	char buffer[BUFFER_SIZE];


	if ( p == NULL || pInStream == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* only for callbacks */
    if ( p->pDataSink == null )
	{
		return MIME_ERR_EMPTY_DATASINK;
	}

	 for (len = pInStream->read (pInStream->rock, buffer, BUFFER_SIZE);
             len > 0;
             len = pInStream->read (pInStream->rock, buffer, BUFFER_SIZE))
        {
                ret = dynamicParse (p, buffer, len);
                if (ret != MIME_OK)
			return ret;
        }

	return MIME_OK;

	/*return mimeParser_parseMimeMessage( p, pInStream, NULL, 0, MIME_PARSE_ALL, NULL );*/
}




/*
* carsonl, jan 8,98 
* ONLY FOR DYNAMIC PARSING
* parse data buffer
*
* parameter :
*
*	mimeParser_t *p :	pointer to mimeParser
*	char *pData		:	data buffer
*	int nLen		:	size of data buffer
*
* returns :	nothing
* NOTE :	all data will be sent to the user via callbacks defined within the datasink
*/
int dynamicParse( mimeParser_t *p, char *pData, int nLen )
{
	int ret;

	if ( p == NULL || pData == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/* only for callbacks */
    if ( p->pDataSink == null )
	{
		return MIME_ERR_EMPTY_DATASINK;
	}

	ret = mimeParser_parseMimeMessage( p, NULL, pData, nLen, MIME_PARSE_ALL, NULL );
	return ret;
}



/*
* carsonl, jan 8,98 
* signal end of parse
*
* parameter :
*
*	mimeParser_t *p :	pointer to mimeParser
*
* returns	:	nothing
*/
int endDynamicParse( mimeParser_t *p )
{
	mime_message_t *pTmpMsg;
	mime_message_internal_t *pTmpMsg_internal;
	mime_message_internal_t *pMimeMessage_internal;
	mime_basicPart_t *pMimeBasicPart;
	mime_multiPart_t *pMimeMultiPart;
	mime_messagePart_t *pMimeMessagePart;
	mime_basicPart_internal_t *pMimeBasicPart_internal;
	mime_multiPart_internal_t *pMimeMultiPart_internal;
	mime_messagePart_internal_t *pMimeMessagePart_internal;
	char *szBuffer;
	int len, i;

	if ( p == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}
	
	/* only for callbacks */
    if ( p->pDataSink == null )
		return MIME_ERR_EMPTY_DATASINK;

    decodeDataBuffer( p );

    if ( p->pDataSink != NULL )
    {
         /* take care of leftover bytes */
        if ((p->nCurrentMessageType == MIME_BASICPART) || (bIsBasicPart(p->nCurrentMessageType)))
        {
			pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
			pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			/* base64 */
			if ( pMimeBasicPart_internal != NULL )
			{
				if ( pMimeBasicPart->encoding_type == MIME_ENCODING_BASE64 &&	p->out_bits > 0 )
				{
            				szBuffer = decodeBase64LeftOverBytes( p->out_bits, p->out_byte, &len );

					if ( szBuffer != NULL )
					{
						saveBodyData( p, szBuffer, len, pMimeBasicPart );

						if ( p->pDataSink != NULL && p->pDataSink->bodyData != NULL )
							(p->pDataSink->bodyData)(p->pDataSink,  pMimeBasicPart_internal->pUserObject, szBuffer, len );
					}
				}

				/* quoted printable */
				else if ( pMimeBasicPart->encoding_type == MIME_ENCODING_QP && p->out_bits > 0 )
				{
					/* will never happen */
				}
            
				if ( p->pDataSink->bodyData != NULL )
					(p->pDataSink->endBasicPart)(p->pDataSink, pMimeBasicPart_internal->pUserObject );
			}
		}

        else if ( p->nCurrentMessageType == MIME_MULTIPART )
		{
			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
			pMimeMultiPart_internal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

			if ( pMimeMultiPart_internal != NULL && p->pDataSink->endMultiPart != NULL )
				(p->pDataSink->endMultiPart)(p->pDataSink, pMimeMultiPart_internal->pUserObject );
		}

        if ( p->nCurrentMessageType == MIME_MESSAGEPART )
		{
			pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
			pMimeMessagePart_internal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			if ( pMimeMessagePart_internal != NULL )
			{
				(p->pDataSink->endMessagePart)(p->pDataSink, pMimeMessagePart_internal->pUserObject );
			}
		}


	/* make end*() callback on all pending parents */					
	if (p->nCurrentParent > 0)								
	/*for (i = p->nCurrentParent - 1; i >= 0; i--)*/					
	for (i = p->nCurrentParent+1; i >= 0; i--)						
	{											
		/* MultiPart handled at END_BOUNDARY. Handle MESSAGEPART & MESSAGE only */	
		if ( p->aCurrentParent[i].p != NULL && p->aCurrentParent[i].type == MIME_MESSAGEPART)
		{										
		    pMimeMessagePart = (mime_messagePart_t *) p->aCurrentParent[i].p;		

		    if (pMimeMessagePart_internal != NULL)					
		    {										
			pMimeMessagePart_internal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			/* if it is rfc822 part call endMessage() also */			
			if (pMimeMessagePart->content_subtype != NULL && 			
			    bStringEquals (pMimeMessagePart->content_subtype, "rfc822") &&	
			    pMimeMessagePart_internal != NULL)					
			{									
				pTmpMsg = pMimeMessagePart_internal->pTheMessage;		

				if (pTmpMsg != NULL)						
				{								
					pTmpMsg_internal = (mime_message_internal_t *)pTmpMsg->pInternal; 
					if (pTmpMsg_internal != NULL)				
	        			      (p->pDataSink->endMessage)(p->pDataSink, pTmpMsg_internal->pUserObject);
				}								
			}									
		    }										

		    if (pMimeMessagePart_internal != NULL)					
		    {
			 (p->pDataSink->endMessagePart) (p->pDataSink, pMimeMessagePart_internal->pUserObject);
		    }
		}										
	}											

		pMimeMessage_internal = (mime_message_internal_t *) p->pMimeMessage->pInternal;

		if ( pMimeMessage_internal != NULL )
	        (p->pDataSink->endMessage)(p->pDataSink, pMimeMessage_internal->pUserObject );
    }

/* 
 *	if ( p->bLocalStorage && p->pDataSink == null )
 *	    checkForEmptyMessages( p, p->pMimeMessage, MIME_MESSAGE );
 */

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* parse entire message
*
* parameter :
*
*	nsmail_inputstream_t *pInput : inputstream
*
* returns
*
*	mime_message_t * :	mime Message Structure populated with data from inputstream
*	NULL			 :	bad parameter or out of memory
*
* NOTE: no need to create an instance of the parser, just call this one method to parse a message
*/
int parseEntireMessageInputstream( nsmail_inputstream_t *pInput, mime_message_t **ppMimeMessage )
{
	mimeParser_t *p;
	int nRet;

	if ( pInput == NULL )
	{
		*ppMimeMessage = NULL;

		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal2( NULL );

	if ( p == NULL )
	{
		*ppMimeMessage = NULL;

		return MIME_ERR_OUTOFMEMORY;
	}

	nRet = mimeParser_parseMimeMessage( p, pInput, NULL, 0, MIME_PARSE_ALL, (void **)ppMimeMessage );
	mimeDynamicParser_free( &p );

	return nRet;
}



/*
* carsonl, jan 8,98 
* parse entire message
*
* parameter :
*
*	char *pData : data buffer
*	int nLen	: size of buffer
*
* returns
*
*	mime_message_t * :	mime Message Structure populated with data from inputstream
*	NULL			 :	bad parameter or out of memory
*
* NOTE: no need to create an instance of the parser, just call this one method to parse a message
*/
int parseEntireMessage( char *pData, int nLen, mime_message_t **ppMimeMessage )
{
	mimeParser_t *p;
	int nRet;
	
	if ( pData == NULL )
	{
		*ppMimeMessage = NULL;

		return MIME_ERR_INVALIDPARAM;
	}

	p = mimeParser_new_internal2( NULL );

	if ( p == NULL )
	{
		*ppMimeMessage = NULL;

		return MIME_ERR_OUTOFMEMORY;
	}

	nRet = mimeParser_parseMimeMessage( p, NULL, pData, nLen, MIME_PARSE_ALL, (void**)ppMimeMessage );
	mimeDynamicParser_free( &p );

	return nRet;
}




/*
* carsonl, jan 8,98 
* mimeParser destructor
*
* parameter :
*
*	mimeParser_t *p	:	instance of mimeParser
*
* returns : nothing
*
*	NULL	not enought buffer space, should never happen
*	char *	decoded buffer, null terminated
*	pLen	length of data
*
* NOTE: the user should set the bDecodeData field within their datasinks prior to giving it to the parser
* NOTE: if using callbacks with local storage enabled, all data structures are not released, it's up to the user
*		to call mimeMessage_free( pMimeMessage ) to release the memory used
*/
void mimeDynamicParser_free( mimeParser_t **pp )
{
	if ( pp == NULL )
	{
		return;
	}

	if ( *pp == NULL )
		return;

	if ( (*pp)->pVectorMimeInfo != NULL )
		Vector_free( (*pp)->pVectorMimeInfo );

	if ( (*pp)->pVectorMessageData != NULL )
		Vector_free( (*pp)->pVectorMessageData );

	if ( (*pp)->pMimeMessage != NULL && (*pp)->bDeleteMimeMessage && ( (*pp)->pDataSink != NULL && !IsLocalStorage( *pp ) ) )
	{
		mime_message_free( (*pp)->pMimeMessage );
		(*pp)->pMimeMessage = NULL;
	}

	if ( (*pp)->pLeftoverBuffer != NULL )
		free( (*pp)->pLeftoverBuffer );
	
	if ( (*pp)->pInputBuffer != NULL )
		free( (*pp)->pInputBuffer );
    
	if ( (*pp)->pMimeInfoQueue != NULL )
		Vector_free( (*pp)->pMimeInfoQueue );

	if ( (*pp)->pHeaderQueue != NULL )
		Vector_free( (*pp)->pHeaderQueue );

	free( *pp );
	*pp = NULL;
}





/* -------------------------- internal routines ------------------------------- */




/*
* carsonl, jan 8,98 
* constructor
*
* parameter : none
*
* returns
*
*	new instance of mimeParser
*
* NOTE : this is the non dynamic parsing version
*/
mimeParser_t *mimeParser_new_internal()
{
	return mimeParser_new_internal2( NULL );
}




/*
* carsonl, jan 8,98 
* constructor
*
* parameter : 
*
*	mime_message_t * pMimeMessage :	pointer to user's mimeMessage object
*
* returns
*
*	new instance of mimeParser
*
* NOTE : this is the non dynamic parsing version
*/
mimeParser_t *mimeParser_new_internal2( mime_message_t * pMimeMessage )
{
	mime_message_internal_t *pMimeMessageInternal = NULL;
	mimeParser_t *p = (mimeParser_t *) malloc( sizeof( mimeParser_t ) );

	if ( p == NULL )
	{
		return NULL;
	}

	p->pVectorMimeInfo = Vector_new( VECTOR_TYPE_MIMEINFO );

	if ( p->pVectorMimeInfo == NULL )
	{
		mimeDynamicParser_free ( &p );

		return NULL;
	}

	p->pVectorMessageData = Vector_new( VECTOR_TYPE_STRING );

	if ( p->pVectorMessageData == NULL )
	{
		mimeDynamicParser_free( &p );

		return NULL;
	}			  

	if ( pMimeMessage != NULL )
	{
		pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

		if ( pMimeMessageInternal != NULL )
			p->pDataSink = pMimeMessageInternal->pDataSink;

		/* two way link */
		p->pMimeMessage = pMimeMessage;
	}
	
	else
	{
		p->pMimeMessage = mime_message_new( NULL );

		if ( p->pMimeMessage == NULL )
		{
			mimeDynamicParser_free( &p );
			
			return NULL;
		}
	}

	p->bStartData = FALSE;
	p->nMessageType = MIME_CONTENT_UNINITIALIZED;
	p->nCurrentMessageType = MIME_CONTENT_UNINITIALIZED;
	p->pCurrentMessage = NULL;
	p->bDeleteMimeMessage = TRUE;		/* set to false to use it outside the parser, free it yourself */

	/* callback support */
	p->pDataSink = NULL;

    p->pLeftoverBuffer = (char *) malloc( BUFFER_SIZE + 1 );
    p->pInputBuffer = (char *) malloc( BUFFER_SIZE + 1 );

	if ( p->pLeftoverBuffer == NULL || p->pInputBuffer == NULL )
	{
		mimeDynamicParser_free( &p );

		return NULL;
	}

    p->nLeftoverBytes = 0;
    p->out_byte = 0;
    p->out_bits = 0;
    p->bParseEntireFile = TRUE;
	p->nLineType = MIME_HEADER; 
	p->fSeenBoundary = FALSE; 
	p->fEndMessageHeader = FALSE; 
	p->QPNoOfLeftOverBytes = 0;
	p->pCurrentMimeMessage = p->pMimeMessage;

	p->nCurrentParent = 0;
	p->aCurrentParent[0].type = MIME_MESSAGE;
	p->aCurrentParent[0].p = p->pMimeMessage;
	p->bDecodeData = TRUE;
	p->bLocalStorage = TRUE;
	
    p->pNextMimeMessage = p->pMimeMessage;
    p->nLastBoundry = START_BOUNDARY;
    p->bQPEncoding = FALSE;
    p->bReadCR = FALSE;
    p->szPreviousHeaderName = NULL;
    p->nMessagePartSubType = SUBTYPE_RFC822;

	p->tHeaderParent.type = MIME_MESSAGE;
	p->tHeaderParent.p = p->pMimeMessage;

	p->tNextHeaderParent.type = UNINITIALIZED;
	p->tNextHeaderParent.p = NULL;

	p->pMimeInfoQueue = Vector_new( VECTOR_TYPE_MIMEINFO );

	if ( p->pMimeInfoQueue == NULL )
	{
		mimeDynamicParser_free ( &p );

		return NULL;
	}

	p->pHeaderQueue = Vector_new( VECTOR_TYPE_STRING );

	if ( p->pHeaderQueue == NULL )
	{
		mimeDynamicParser_free( &p );

		return NULL;
	}			  

	return p;
}




/*
* carsonl, jan 8,98 
* create new message structure based on content type
*
* parameter :
*
*	mimeParser_t *p : instance of mimeParser
*	char *s			: content type input line
*
* returns : MIME_OK or MIME_INVALIDPARAM
*/
static int nNewMessageStructure( mimeParser_t *p, char *s )
{
	mime_message_internal_t *pInternal;
	mime_content_type nContentType;
	mime_messagePart_t *pMimeMessagePart;
	mime_basicPart_t *pMimeBasicPart;
	mime_multiPart_t *pMimeMultiPart;
	mime_multiPart_internal_t *pMimeMultiPart_internal;
	mime_messagePart_internal_t *pMimeMessagePart_internal;
	char achContentSubtype[64];
	char *szParam;
	void *pMessage = NULL;
	char *szContentSubType;
	char *szContentParams;
	void *pUserObject;

	if ( p == NULL || s == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	nContentType = mimeParser_nGetContentType( s, achContentSubtype, &szParam );

	if (((int)nContentType) < 0)
		return nContentType; /* Its the error code */

	pInternal = (mime_message_internal_t *) p->pMimeMessage->pInternal;

	switch ( nContentType )
	{
		case MIME_CONTENT_AUDIO:
		case MIME_CONTENT_IMAGE:
		case MIME_CONTENT_VIDEO:
		case MIME_CONTENT_APPLICATION:
		case MIME_BASICPART:
			pMessage = mime_basicPart_new();
			nAddMessage( p, pMessage, nContentType);

			if ( p->pDataSink != NULL )
			{
			     /* if (IsLocalStorage( p ) && p->pDataSink->startBasicPartLocalStorage != NULL) */
		             /*   setUserObject(pMessage, MIME_BASICPART, (p->pDataSink->startBasicPartLocalStorage)(p->pDataSink, pMessage)); */
			     /* else */ 
			     if (p->pDataSink->startBasicPart != NULL)
			     {
		            	setUserObject( pMessage, MIME_BASICPART, (p->pDataSink->startBasicPart)(p->pDataSink) );
			     }
			}

			break;

		case MIME_CONTENT_MULTIPART:
			pMessage = mime_multiPart_new();
			nAddMessage( p, pMessage, MIME_CONTENT_MULTIPART );

			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
			pMimeMultiPart_internal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;
			pMimeMultiPart_internal->fParsedPart = TRUE;

			if ( p->pDataSink != NULL )
			{
			    /* if (IsLocalStorage( p ) && p->pDataSink->startMultiPartLocalStorage != NULL)*/
		            /*    setUserObject(pMessage, MIME_MULTIPART, (p->pDataSink->startMultiPartLocalStorage)(p->pDataSink, pMessage));*/
			    /* else */
			    if (p->pDataSink->startMultiPart != NULL)
		                 setUserObject (pMessage, MIME_MULTIPART, (p->pDataSink->startMultiPart)(p->pDataSink));
			}

			/* save boundary */
			if ( pMimeMultiPart_internal != NULL )
			{
				pMimeMultiPart_internal->szBoundary = mimeParser_parseForBoundary( s );

			}

			break;

		case MIME_CONTENT_MESSAGEPART:
			/* add code here */
			pMimeMessagePart = mime_messagePart_new();
			pMimeMessagePart_internal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			nAddMessage( p, (void *) pMimeMessagePart, MIME_CONTENT_MESSAGEPART );

			if ( p->pDataSink != NULL )
			{
			   /*	if ( IsLocalStorage( p ) && p->pDataSink->startMessagePartLocalStorage != NULL ) */
			   /*	{ 										 */
			   /*		pUserObject = (p->pDataSink->startMessagePartLocalStorage)(p->pDataSink,  pMimeMessagePart); */
		           /*		setUserObject (pMimeMessagePart, MIME_MESSAGEPART, pUserObject);         */
			   /*	}										 */
			   /*	else 										 */
				if ( p->pDataSink->startMessagePart != NULL )
				{
					pUserObject = (p->pDataSink->startMessagePart)(p->pDataSink); 	  
		            		setUserObject( pMimeMessagePart, MIME_MESSAGEPART, pUserObject ); 
				}
			}

            if ( bStringEquals( achContentSubtype, "external-body" ) )
                p->nMessagePartSubType = SUBTYPE_EXTERNAL_BODY;

            /* unsupported partial subtype */
            else if ( bStringEquals( achContentSubtype, "partial" ) )
            {
		return MIME_ERR_UNSUPPORTED_PARTIAL_SUBTYPE;
            }
            
            /* determine subtype */
            else if ( bStringEquals( achContentSubtype, "rfc822" ) )
            {
                pMimeMessagePart_internal->pTheMessage = mime_message_new (NULL); 		 	   

		if (p->pDataSink != NULL)								   
		{								   			   
		     /*	if (IsLocalStorage(p) && p->pDataSink->startMessagePartLocalStorage != NULL)*/     
		     /*	{									    */	   
		     /*		pUserObject = (p->pDataSink->startMessageLocalStorage)(p->pDataSink,*/     
		     /*				 pMimeMessagePart_internal->pTheMessage);    	    */	   
		     /*	}								            */	   
		     /*	else 									    */
			if (p->pDataSink->startMessage != NULL)					   	   
			{										   
				pUserObject = (p->pDataSink->startMessage)(p->pDataSink); 	  	   
			}										   

		       setUserObject (pMimeMessagePart_internal->pTheMessage, MIME_MESSAGE, pUserObject);  
		}

                p->nMessagePartSubType = SUBTYPE_RFC822;
                p->tNextHeaderParent.p = pMimeMessagePart_internal->pTheMessage;
                p->tNextHeaderParent.type = MIME_MESSAGE;

                /* defered assignment till received first blank line */
                p->pNextMimeMessage = pMimeMessagePart_internal->pTheMessage;
            }                
			break;
	}

	szContentSubType = szStringClone( achContentSubtype );
	szContentParams = szStringClone( szLTrim( szParam ) );

	/* set content type */
	switch ( p->nCurrentMessageType )
	{
		case MIME_CONTENT_AUDIO:
		case MIME_CONTENT_IMAGE:
		case MIME_CONTENT_VIDEO:
		case MIME_CONTENT_APPLICATION:
		case MIME_BASICPART:
			pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
			pMimeBasicPart->content_type = nContentType;
			pMimeBasicPart->content_subtype = szContentSubType;
			pMimeBasicPart->content_type_params = szContentParams;
			break;

		case MIME_CONTENT_MULTIPART:
			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
			pMimeMultiPart->content_type = nContentType;
			pMimeMultiPart->content_subtype = szContentSubType;
            		if (!bStringEquals (szContentParams, "boundary"))
			     pMimeMultiPart->content_type_params = szContentParams;
			break;

		case MIME_CONTENT_MESSAGEPART:
			pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
			pMimeMessagePart->content_type = nContentType;
			pMimeMessagePart->content_subtype = szContentSubType;
			pMimeMessagePart->content_type_params = szContentParams;
			break;
	}

	if ( p->pDataSink != NULL )
	{
		if ( p->pDataSink->contentType != NULL )
			(p->pDataSink->contentType)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), nContentType );

		if ( p->pDataSink->contentSubType )
			(p->pDataSink->contentSubType)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), szContentSubType );

		if (p->pDataSink->contentTypeParams && szContentParams != NULL && !bStringEquals (szContentParams, "boundary"))
		{
			
			(p->pDataSink->contentTypeParams)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), szContentParams );
		}
	}

	return MIME_OK;
}





/*
* carsonl, jan 8,98 
* populate message structure based on info inside pMimeInfo
*
* parameter :
*
*	mimeParser_t *p			: instance of mimeParser
*	mimeInfo_t *pMimeInfo	: mimeInfo object
*
* returns : MIME_OK, MIME_INVALIDPARAM
*/
int mimeParser_setData( mimeParser_t *p, mimeInfo_t *pMimeInfo )
{
	mime_messagePart_t *pMimeMessagePart;
	mime_messagePart_internal_t *pMimeMessagePartInternal;
	mime_basicPart_t *pMimeBasicPart;
	mime_multiPart_t *pMimeMultiPart;
	char *szParam;
	int nContentDisposition;
	char *szContentDispParams;
	int i, nSize, ret;

	if ( p == NULL || pMimeInfo == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}


/* externalheaders -----------------------------------------------

  if ( p->nMessagePartSubType == SUBTYPE_EXTERNAL_BODY &&
		p->nCurrentMessageType == MIME_MESSAGEPART )
    {
		pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
		pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

		if ( pMimeMessagePart->extra_headers  == NULL )
			pMimeMessagePart->extra_headers  = mime_header_new( pMimeInfo->szName, pMimeInfo->szValue );
		else
			mime_header_add( pMimeMessagePart->extra_headers, pMimeInfo->szName, pMimeInfo->szValue );

		if ( p->pDataSink != NULL && pMimeMessagePartInternal != NULL && p->pDataSink->header != NULL )
			(p->pDataSink->header)(p->pDataSink, pMimeMessagePartInternal->pUserObject, pMimeInfo->szName, pMimeInfo->szValue );

		p->szPreviousHeaderName = pMimeInfo->szName;
    }
------------------------------------------------------------ */

	/* externalheaders */
    if ( p->nMessagePartSubType == SUBTYPE_EXTERNAL_BODY &&
		p->nCurrentMessageType == MIME_MESSAGEPART )
/*		p->nEmptyLineNo > 0 ) */
    {
        /* add as external body headers */
		pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
		pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

		if ( pMimeMessagePart->extern_headers  == NULL )
			pMimeMessagePart->extern_headers  = mime_header_new( pMimeInfo->szName, pMimeInfo->szValue );
		else
			mime_header_add( pMimeMessagePart->extern_headers, pMimeInfo->szName, pMimeInfo->szValue );

		if ( p->pDataSink != NULL && pMimeMessagePartInternal != NULL && p->pDataSink->header != NULL )
			(p->pDataSink->header)(p->pDataSink, pMimeMessagePartInternal->pUserObject, pMimeInfo->szName, pMimeInfo->szValue );

		p->szPreviousHeaderName = pMimeInfo->szName;
    }

	/* create message structure */
    else if ( bStringEquals( pMimeInfo->szName, "content-type" ) )
	{
		if ((ret = nNewMessageStructure (p, pMimeInfo->szValue)) != MIME_OK)
			return ret;

		nSize = Vector_size( p->pMimeInfoQueue );

		/* add from queue */
    	if ( nSize > 0 && p->nMessagePartSubType != SUBTYPE_EXTERNAL_BODY )
    	{
    		for ( i = 0; i < nSize; i++ )
    		{
   			ret = mimeParser_setData( p, (mimeInfo_t*) Vector_elementAt( p->pMimeInfoQueue, i ) );
			if (ret != MIME_OK)
			    return ret;
    		}
    		
			Vector_deleteAll( p->pMimeInfoQueue );
    	}
	}

	/* no message structure */
	else if ( p->pCurrentMessage == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	else if ( bStringEquals( pMimeInfo->szName, "content-description" ) )
    {
        if ( p->nCurrentMessageType == UNINITIALIZED )
		{
			Vector_addElement( p->pMimeInfoQueue, pMimeInfo, sizeof( mimeInfo_t ) );
		}

		else
		{
			switch ( p->nCurrentMessageType )
			{
				case MIME_CONTENT_AUDIO:
				case MIME_CONTENT_IMAGE:
				case MIME_CONTENT_VIDEO:
				case MIME_CONTENT_APPLICATION:
				case MIME_BASICPART:
					pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
					pMimeBasicPart->content_description = szStringClone( pMimeInfo->szValue );
					break;

				case MIME_CONTENT_MULTIPART:
					pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
					pMimeMultiPart->content_description = szStringClone( pMimeInfo->szValue );
					break;

				case MIME_CONTENT_MESSAGEPART:
					pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
					pMimeMessagePart->content_description = szStringClone( pMimeInfo->szValue );
					break;
			}

			if ( p->pDataSink != null && p->pDataSink->contentDescription != NULL )
				(p->pDataSink->contentDescription)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), pMimeInfo->szValue );
		}
    }
    
	else if ( bStringEquals( pMimeInfo->szName, "content-disposition" ) )
    {
        if ( p->nCurrentMessageType == UNINITIALIZED )
		{
			Vector_addElement( p->pMimeInfoQueue, pMimeInfo, sizeof( mimeInfo_t ) );
		}

		else
		{
			nContentDisposition = mime_translateDispType( pMimeInfo->szValue, &szParam );
			szContentDispParams = szStringClone( szLTrim( szParam ) );

			switch ( p->nCurrentMessageType )
			{
				case MIME_CONTENT_AUDIO:
				case MIME_CONTENT_IMAGE:
				case MIME_CONTENT_VIDEO:
				case MIME_CONTENT_APPLICATION:
				case MIME_BASICPART:
					pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
					pMimeBasicPart->content_disposition = nContentDisposition;
					pMimeBasicPart->content_disp_params = szContentDispParams;
					break;

				case MIME_CONTENT_MULTIPART:
					pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
					pMimeMultiPart->content_disposition = nContentDisposition;
					pMimeMultiPart->content_disp_params = szContentDispParams;
					break;

				case MIME_CONTENT_MESSAGEPART:
					pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
					pMimeMessagePart->content_disposition = nContentDisposition;
					pMimeMessagePart->content_disp_params = szContentDispParams;
					break;
			}

			if ( p->pDataSink != null && p->pDataSink->contentDisposition != NULL )
			{
				(p->pDataSink->contentDisposition)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), nContentDisposition );

				if ( szParam != null && p->pDataSink->contentDispParams != NULL )
					(p->pDataSink->contentDispParams)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), szContentDispParams );
			}
		}
    }

	else if ( bStringEquals( pMimeInfo->szName, "content-id" ) )
    {
        if ( p->nCurrentMessageType == UNINITIALIZED )
		{
			Vector_addElement( p->pMimeInfoQueue, pMimeInfo, sizeof( mimeInfo_t ) );
		}

		else
		{
			switch ( p->nCurrentMessageType )
			{
				case MIME_CONTENT_AUDIO:
				case MIME_CONTENT_IMAGE:
				case MIME_CONTENT_VIDEO:
				case MIME_CONTENT_APPLICATION:
				case MIME_BASICPART:
					pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
					pMimeBasicPart->contentID = szStringClone( pMimeInfo->szValue );
					break;

				case MIME_CONTENT_MULTIPART:
					pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
					pMimeMultiPart->contentID = szStringClone( pMimeInfo->szValue );
					break;

				case MIME_CONTENT_MESSAGEPART:
					pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
					pMimeMessagePart->contentID = szStringClone( pMimeInfo->szValue );
					break;
			}

			if ( p->pDataSink != null && p->pDataSink->contentID != NULL )
				(p->pDataSink->contentID)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), pMimeInfo->szValue );
		}
    }

	else if ( bStringEquals( pMimeInfo->szName, "content-md5" ) )
    {
        if ( p->nCurrentMessageType == UNINITIALIZED )
		{
			Vector_addElement( p->pMimeInfoQueue, pMimeInfo, sizeof( mimeInfo_t ) );
		}

		else if ( bIsBasicPart( p->nCurrentMessageType ) )
		{
			pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
			pMimeBasicPart->contentMD5 = szStringClone( pMimeInfo->szValue );

			if ( p->pDataSink != null && p->pDataSink->contentMD5 != NULL )
				(p->pDataSink->contentMD5)(p->pDataSink, getUserObject2 (p->pCurrentMessage, p->nCurrentMessageType), pMimeInfo->szValue );
		}
    }
    
	else if ( bStringEquals( pMimeInfo->szName, "content-transfer-encoding" ) )
    {
        if ( p->nCurrentMessageType == UNINITIALIZED )
		{
			Vector_addElement( p->pMimeInfoQueue, pMimeInfo, sizeof( mimeInfo_t ) );
		}

		else if (  bIsBasicPart( p->nCurrentMessageType ) )
		{
			pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
			pMimeBasicPart->encoding_type = mime_translateMimeEncodingType( pMimeInfo->szValue );

			if ( pMimeBasicPart->encoding_type == MIME_ENCODING_QP )
        		p->bQPEncoding = TRUE;

			if ( p->pDataSink != null && p->pDataSink->contentEncoding != NULL )
				(p->pDataSink->contentEncoding)(p->pDataSink, getUserObject( pMimeBasicPart, MIME_BASICPART ), pMimeBasicPart->encoding_type );
		}

		else if ( p->nCurrentMessageType == MIME_MESSAGEPART )
		{
			pMimeMessagePart = (mime_messagePart_t *) p->pCurrentMessage;
			pMimeMessagePart->encoding_type = mime_translateMimeEncodingType( pMimeInfo->szValue );

			if ( p->pDataSink != null && p->pDataSink->contentEncoding != NULL )
				(p->pDataSink->contentEncoding)(p->pDataSink, getUserObject( pMimeMessagePart, MIME_MESSAGEPART ), pMimeMessagePart->encoding_type );
		}
		else if ( p->nCurrentMessageType == MIME_MULTIPART )
		{
			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;

			pMimeMultiPart->encoding_type = mime_translateMimeEncodingType (pMimeInfo->szValue);

			if (pMimeMultiPart->encoding_type == MIME_ENCODING_7BIT ||
			    pMimeMultiPart->encoding_type == MIME_ENCODING_8BIT ||
			    pMimeMultiPart->encoding_type == MIME_ENCODING_8BIT)
			{
				if (p->pDataSink != null && p->pDataSink->contentEncoding != NULL)
				(p->pDataSink->contentEncoding)(p->pDataSink, getUserObject (pMimeMultiPart, MIME_MULTIPART), 
								pMimeMultiPart->encoding_type);
			}
			else
			    pMimeMultiPart->encoding_type == MIME_ENCODING_UNINITIALIZED;
		}
    }
	else if ( bStringEquals( pMimeInfo->szName, "content-name" ) )
	{
		addHeader (p, pMimeInfo->szName, pMimeInfo->szValue, TRUE);
	}
    
	return MIME_OK;
}





/*
* carsonl, jan 8,98 
* parse the contents of mimeInfo
*
* parameter :
*
*	mimeParser_t *p			: instance of mimeParser
*	mimeInfo_t *pMimeInfo	: mimeInfo object
*
* returns : MIME_OK, MIME_INVALIDPARAM
*/
int mimeParser_parseMimeInfo( mimeParser_t *p, mimeInfo_t *pMimeInfo )
{
    char *paramName;
    char *value;
    int offset;
    int len;
    BOOLEAN beginQuote;
    char ch;
	int i;
	int valueLen;
	mimeInfo_t *pMimeInfo2;

	if ( p == NULL || pMimeInfo == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	beginQuote = FALSE;
    paramName = "";
    value = "";
    offset = 0;
    len = 0;
    beginQuote = FALSE;
	valueLen = strlen( pMimeInfo->szValue );

	/* go through entire line */
    for ( i=0; i < valueLen; i++ )
    {
        ch = pMimeInfo->szValue[i];
        
        /* handle quotes */
        if ( ch == '"' )
            beginQuote = beginQuote ? FALSE : TRUE;

        if ( beginQuote )
        {
            len++;
            continue;
        }
        
		/* parse and fill in fields */
        switch ( ch )
        {
            case ';':
				pMimeInfo->szValue[offset+len] = 0;
				value = szTrim( &pMimeInfo->szValue[offset] );
                offset = i + 1;
                
                if ( pMimeInfo->pVectorParam == NULL )
				{
					pMimeInfo->pVectorParam = Vector_new( VECTOR_TYPE_MIMEINFO );

					if ( pMimeInfo->pVectorParam == NULL )
					{
						return MIME_ERR_OUTOFMEMORY;
					}
                }

				pMimeInfo2 = mimeInfo_new();
				
				if ( pMimeInfo2 == NULL )
				{
					return MIME_ERR_OUTOFMEMORY;
				}

				mimeInfo_init2( pMimeInfo2, MIME_INFO, paramName, value );
                Vector_addElement( pMimeInfo->pVectorParam, pMimeInfo2, sizeof( mimeInfo_t ) );

                value = "";
                paramName = "";
                
                len = 0;
				mimeInfo_free( pMimeInfo2 );
				pMimeInfo2 = NULL;

                break;

            case '=':
				pMimeInfo->szValue[ offset + len ] = 0;
				paramName = szTrim( &pMimeInfo->szValue[ offset ] );

                offset = i + 1;
                len = 0;
                break;

            default:
                len++;
        }
    }
    
	/* add it to internal buffer */
    if ( len > 0 )
    {
		pMimeInfo->szValue[offset+len] = 0;
		value = szTrim( &pMimeInfo->szValue[offset] );
        
        if ( pMimeInfo->pVectorParam == NULL )
		{
            pMimeInfo->pVectorParam = Vector_new( VECTOR_TYPE_MIMEINFO );

			if ( pMimeInfo->pVectorParam == NULL )
			{
				return MIME_ERR_OUTOFMEMORY;
			}
		}
		
		pMimeInfo2 = mimeInfo_new();
		
		if ( pMimeInfo2== NULL )
		{
			return MIME_ERR_OUTOFMEMORY;
		}

		mimeInfo_init2( pMimeInfo2, MIME_INFO, paramName, value );
        Vector_addElement( pMimeInfo->pVectorParam, pMimeInfo2, sizeof( mimeInfo_t ) );
		mimeInfo_free( pMimeInfo2 );
		pMimeInfo2 = NULL;
    }

	return MIME_OK;
}


/* prototype */
void appToLastHdrOnQueue (mimeParser_t *p, char *value);   

/*
* carsonl, jan 8,98 
* parse and extract data from input string
*
* parameter :
*
*	mimeParser_t *p	: instance of mimeParser
*	char *s			: input string
*	BOOLEAN bLastLine : TRUE if this is the last line
*
* returns : MIME_OK, MIME_ERR_INVALIDPARAM
*/
int mimeParser_parseLine( mimeParser_t *p, char *s, int len, BOOLEAN bLastLine )
{
	int i;
	int nIndex, ret;
    	BOOLEAN finished = FALSE;
	mimeInfo_t *mi, *mi2;
	mime_basicPart_t *pMimeBasicPart;
	mime_basicPart_internal_t *pMimeBasicPart_internal;
	mime_multiPart_t *pMimeMultiPart;
	mime_multiPart_internal_t *pMimeMultiPart_internal;
	mime_messagePart_t *pMimeMessagePart;
	mime_messagePart_internal_t *pMimeMessagePart_internal;
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessage_internal;
	char achTemp[256], *pTmp;
	int nSize;

	if ( p == NULL || s == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	mi = NULL;
	mi2 = NULL;
	pMimeBasicPart = NULL;
	pMimeBasicPart_internal = NULL;
	pMimeMultiPart = NULL;
	pMimeMultiPart_internal = NULL;
	pMimeMessagePart = NULL;
	pMimeMessagePart_internal = NULL;
	pMimeMessage_internal = NULL;

	
	/* debug 
	sprintf(achTemp, "%s\n", s );
	fprintf (stderr, achTemp);

	if (strncmp (s, "-----------------------------2082357617733318156662372089--", 59) == 0)
	{
		nSize = nSize;
	}

	if (strncmp (s, "Content-Type: multipart", 23) == 0)
	{
		nSize = nSize;
	}

	*/


	/* ignore additional header info */
    if ((p->nLineType == MIME_XHEADER || p->nLineType == MIME_HEADER) && (len > 0 && (s[0] == '\t' || s[0] == ' ')))
    {
      /*if (!(p->nCurrentMessageType == MIME_BASICPART && p->bStartData == TRUE))*/	
        if (!(bIsBasicPart(p->nCurrentMessageType) && p->bStartData == TRUE))		
	{										
        	sprintf( achTemp, "\n%s", s );

		/* continuation headers */
		if (p->tHeaderParent.type == MIME_BASICPART || bIsBasicPart(p->tHeaderParent.type)) 
		{
			pMimeBasicPart = (mime_basicPart_t *) p->tHeaderParent.p;
			pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPart->extra_headers == NULL )
				pMimeBasicPart->extra_headers = mime_header_new( p->szPreviousHeaderName, achTemp );
			else
			{
				mime_header_apend( pMimeBasicPart->extra_headers, p->szPreviousHeaderName, achTemp );
			}

			if ( p->pDataSink != NULL && pMimeBasicPart_internal != NULL && p->pDataSink->header != NULL )
				(p->pDataSink->header)(p->pDataSink, pMimeBasicPart_internal->pUserObject, 
							p->szPreviousHeaderName, achTemp );
        	}
		/*else if ( p->tHeaderParent.type == MIME_MESSAGEPART ) */
		else
		{
			if (p->szPreviousHeaderName == NULL)
			{
				appToLastHdrOnQueue (p, achTemp);
			}
			else
			{
			     pMimeMessage = (mime_message_t*) p->tHeaderParent.p;
			     pMimeMessage_internal = (mime_message_internal_t *) p->pCurrentMimeMessage->pInternal;

			     if ( pMimeMessage->rfc822_headers == NULL )
				   pMimeMessage->rfc822_headers = mime_header_new( p->szPreviousHeaderName, achTemp );
			     else
				   mime_header_apend( pMimeMessage->rfc822_headers, p->szPreviousHeaderName, achTemp );

			     if (p->pDataSink != NULL && pMimeMessage_internal != NULL && p->pDataSink->addHeader != NULL)
			     {
				(p->pDataSink->addHeader)(p->pDataSink, pMimeMessage_internal->pUserObject, 
							p->szPreviousHeaderName, achTemp);
			     }
			}
		}

		return MIME_OK;
	} 
    }

    if (len == 0)
    {
        p->nEmptyLineNo++;

	if (p->nEmptyLineNo == 1 && p->fEndMessageHeader == FALSE) 
	{
	       p->fEndMessageHeader = TRUE; 

	       if (p->pDataSink != NULL &&  p->pDataSink->endMessageHeader != NULL)
	       {
	             (p->pDataSink->endMessageHeader) (p->pDataSink, getUserObject( p->pMimeMessage, MIME_MESSAGE));
	       }
	}
    }

    /* for messagePart, special case */
    if ( len == 0 && !p->bStartData )
    {
        if ( p->bQPEncoding && p->nEmptyLineNo == 1 )
            p->bReadCR = TRUE;

        if (p->nEmptyLineNo > 1 && p->nCurrentMessageType == MIME_CONTENT_UNINITIALIZED && 	
	    (nGetCurrentParentType(p) == MIME_MESSAGEPART))					
	{											
		p->nLineType = MIME_CRLF;							
			return MIME_OK;   							
	}											

        if (p->nEmptyLineNo == 2 && !(p->nCurrentMessageType == MIME_MULTIPART && p->fSeenBoundary ==FALSE))
       		p->bStartData = TRUE;
		else if (p->nEmptyLineNo == 1  && (nGetCurrentParentType(p) == MIME_MULTIPART) && p->fSeenBoundary == TRUE)
		{
			if (p->nCurrentMessageType == UNINITIALIZED) 
       				p->bStartData = TRUE;		
		}
		else if (p->nEmptyLineNo > 1  && p->nCurrentMessageType == MIME_MULTIPART) 
		{ 					
		     if (p->fSeenBoundary == TRUE) 	
       		         p->bStartData = TRUE;	        
		     else      /* goes in preamble */   					
		     {								   		
			/* Add the preamble */					   		
			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;  		
			s[0] = '\r';						   		
			s[1] = '\n';						   		
			if (pMimeMultiPart->preamble == NULL)			   		
			{						   	   		
				 pMimeMultiPart->preamble =  (char *) malloc (3);  		
				 memset (pMimeMultiPart->preamble, 0, 3);	   		
				 strncpy (pMimeMultiPart->preamble, s, 2);	   		
			}							   		
			else							   		
			{							   		
				/*append string*/
				pTmp = (char *) malloc (strlen (pMimeMultiPart->preamble) + 3); 
		     		memset (pTmp, 0, strlen (pMimeMultiPart->preamble) + 3);	
				strcpy (pTmp, pMimeMultiPart->preamble); 			
				free (pMimeMultiPart->preamble);   				
				pMimeMultiPart->preamble = strncat (pTmp, s, 2);   		
			}   									

			p->nLineType = MIME_CRLF;						
			return MIME_OK;   							
		     }										
		} 										
		else if ( p->nEmptyLineNo == 1 )
        {
            /* change mimeMessage parent */
            if ( p->pNextMimeMessage != null )
            {
                /* change parent only after first blank line */
                p->pCurrentMimeMessage = p->pNextMimeMessage;
                p->pNextMimeMessage = NULL;
            }

            /* insert headers from queue */
			nSize = Vector_size( p->pHeaderQueue );

            if ( p->nCurrentMessageType != UNINITIALIZED && nSize > 0 )
            {
            	for ( i = 0; i < nSize; i++ )
            	{
					mi = (mimeInfo_t *) Vector_elementAt( p->pHeaderQueue, i );
					addHeader( p, mi->szName, mi->szValue, FALSE );
				}
        	
            	Vector_deleteAll( p->pHeaderQueue );
		    }

		    /* change header parent */
			if ( p->tNextHeaderParent.p != NULL && p->tNextHeaderParent.type != MIME_MULTIPART )
			{
				p->tHeaderParent.p = p->tNextHeaderParent.p;
				p->tHeaderParent.type = p->tNextHeaderParent.type;
				p->tNextHeaderParent.p = NULL;
			}
		}
    }

    /* for messagePart, special case */
/* carsonl
/*    if ( len == 0 )
        p->nEmptyLineNo++;
*/

	if ( bStringEquals( s, "content-type" ) )
        p->nEmptyLineNo = 0;
            
    /* boundary */
    if ( ( p->nLineType == MIME_INFO || p->nLineType == MIME_HEADER || p->nLineType == MIME_XHEADER ) && len == 0 &&
		  p->nCurrentMessageType != UNINITIALIZED && p->nMessagePartSubType != SUBTYPE_EXTERNAL_BODY )
    {
    	if ( p->nCurrentMessageType == MIME_CONTENT_MESSAGEPART )
    	{
    		if ( p->nEmptyLineNo == 1 || p->nEmptyLineNo == 2 )
        		p->nLineType = MIME_HEADER;
            else
            {
                mi = mimeInfo_new();
                
				if ( mi == NULL )
					return MIME_ERR_OUTOFMEMORY;

				/* content type */
                mi->nType = MIME_INFO;
                mi->szName = "content-type";
                mi->szValue = "text/plain";
		ret = mimeParser_setData( p, mi );
		if (ret != MIME_OK)
		    return ret;
                mimeParser_parseMimeInfo( p, mi );
                Vector_addElement( p->pVectorMimeInfo, mi, sizeof( mimeInfo_t ) );
                
                /* encoding type */
                mi->nType = MIME_INFO;
                mi->szName = "content-transfer-encoding";
                mi->szValue = "7bit";
		ret = mimeParser_setData( p, mi );
		if (ret != MIME_OK)
		    return ret;
                mimeParser_parseMimeInfo( p, mi );
                Vector_addElement( p->pVectorMimeInfo, mi, sizeof( mimeInfo_t ) );

                mi->szName = NULL;
                mi->szValue = NULL;
                mimeInfo_free( mi );
				mi = NULL;

        	p->bStartData = TRUE;
        	p->fSeenBoundary = FALSE; 				
       	    }
    	}
    	
		/* start data only if it's a simple message with no boundary */
		else if ( bIsBasicPart( p->nCurrentMessageType ) )
		{
    		    p->bStartData = TRUE;
        	    p->fSeenBoundary = FALSE; 				
		}

        	if (p->nEmptyLineNo == 1)
			return MIME_OK;
    }

	/* empty line, don't process anymore */
    if ( !p->bStartData && len == 0 )
    {
		if (p->nCurrentMessageType == MIME_MULTIPART)
		{
        		p->nEmptyLineNo = 0;					   

			/* Add the preamble */					   
			pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;  
					s[0] = '\r';						   
					s[1] = '\n';						   
			if (pMimeMultiPart->preamble == NULL)			   
			{						   	   
				 pMimeMultiPart->preamble =  (char *) malloc (3);	   
				 memset (pMimeMultiPart->preamble, 0, 3);		   
				 strncpy (pMimeMultiPart->preamble, s, 2);		   
			}							   
			else							   
			{							   
				/*append string*/
				pTmp = (char *) malloc (strlen (pMimeMultiPart->preamble) + 3); 
		     		memset (pTmp, 0, strlen (pMimeMultiPart->preamble) + 3);	
				strcpy (pTmp, pMimeMultiPart->preamble); 			
				free (pMimeMultiPart->preamble);   				
				pMimeMultiPart->preamble = strncat (pTmp, s, 2);   		
			}   									
		}

		p->nLineType = MIME_CRLF;
			return MIME_OK;   
     }

    /* identify line */
    mi = mimeInfo_new();

	if ( mi == NULL )
	{
		return MIME_ERR_OUTOFMEMORY;
	}

    /* -------------------------- identify line ----------------------- */
    mi->nType = mimeParser_checkForLineType( p, s, len );

    if (mi->nType == START_BOUNDARY)					
    {									
        p->fSeenBoundary = TRUE; 					
        p->nEmptyLineNo = 0;						
    	if (p->nCurrentMessageType == MIME_CONTENT_MESSAGEPART &&	
	    p->nMessagePartSubType == SUBTYPE_EXTERNAL_BODY)		
		p->bStartData = FALSE;			       		

	if (p->pDataSink != NULL && p->pDataSink->boundary != NULL) 	
	{
		if (p->nCurrentMessageType == MIME_MULTIPART)
		{
		    (p->pDataSink->boundary) (p->pDataSink, 
					     getUserObject(p->pCurrentMessage, MIME_MULTIPART), 
					      mimeParser_getCurrentBoundary (p));
		}
		else if (nGetCurrentParentType(p) == MIME_MULTIPART)
		{
		    (p->pDataSink->boundary) (p->pDataSink, 
					     getUserObject(pGetCurrentParent(p), MIME_MULTIPART), 
					     mimeParser_getCurrentBoundary (p));
		}
	}
    }									

    /* start of simple basicpart message   */
	if (p->nLineType == MIME_CRLF && 
		p->nCurrentMessageType == UNINITIALIZED &&
		p->nLastBoundry == START_BOUNDARY &&
		mi->nType == MIME_MESSAGE_DATA &&
		( p->nEmptyLineNo == 1 || p->nEmptyLineNo == 2 ) &&
		p->nMessagePartSubType != SUBTYPE_EXTERNAL_BODY )
    {
        mi2 = mimeInfo_new();

		/* content type */
        mi2->nType = MIME_INFO;
        mi2->szName = "content-type";
        mi2->szValue = "text/plain";
		ret = mimeParser_setData( p, mi2 );
	
		if (ret != MIME_OK)
			return ret;

        mimeParser_parseMimeInfo( p, mi2 );
        Vector_addElement( p->pVectorMimeInfo, mi2, sizeof( mimeInfo_t ) );

        /* encoding type */
        mi2->nType = MIME_INFO;
        mi2->szName = "content-transfer-encoding";
        mi2->szValue = "7bit";
		ret = mimeParser_setData( p, mi2 );
	
		if (ret != MIME_OK)
			return ret;

        mimeParser_parseMimeInfo( p, mi2 );
        Vector_addElement( p->pVectorMimeInfo, mi2, sizeof( mimeInfo_t ) );

        mi2->szName = NULL;
        mi2->szValue = NULL;
        mimeInfo_free( mi2 );
		mi2 = NULL;
		p->bStartData = TRUE;

		if ( p->nEmptyLineNo == 1 || p->nEmptyLineNo == 2 )
        {
			nSize = Vector_size( p->pHeaderQueue );

            /* insert headers from queue */
            if ( nSize > 0 )
            {
                 /* change header parent */
                 if ( p->tNextHeaderParent.p != null )
	         {
                     p->tHeaderParent.p = p->tNextHeaderParent.p;
                     p->tHeaderParent.type = p->tNextHeaderParent.type;
		     p->tNextHeaderParent.p = NULL;
	         }

        	for ( i = 0; i < nSize; i++ )
        	{
		      mi2 = (mimeInfo_t *) Vector_elementAt( p->pHeaderQueue, i );
        	      addHeader( p, mi2->szName, mi2->szValue, FALSE );
        	}
    		
            	Vector_deleteAll( p->pHeaderQueue );
            }

            /* change header parent */
            if ( p->tNextHeaderParent.p != null )
	    {
                p->tHeaderParent.p = p->tNextHeaderParent.p;
                p->tHeaderParent.type = p->tNextHeaderParent.type;
		p->tNextHeaderParent.p = NULL;
	    }

	    /* should add from mimeinfo queue */

	    p->nEmptyLineNo = 0; /* reset for headers */
        }            
    }
        
    /* start boundary  */
    if ( !p->bStartData && mi->nType == START_BOUNDARY )
    {
/*	mimeInfo_free( mi );
	mi = NULL;
*/
		p->nLineType = MIME_INFO;

		p->nLeftoverBytes = 0;
		p->out_byte = 0;
		p->out_bits = 0;
		p->QPNoOfLeftOverBytes = 0;
		p->nMessagePartSubType = SUBTYPE_RFC822;
		p->bReadCR = FALSE;
		p->bQPEncoding = FALSE;
		p->nEmptyLineNo = 0;
		p->nCurrentMessageType = MIME_CONTENT_UNINITIALIZED;
		p->nLastBoundry = START_BOUNDARY;
		p->fSeenBoundary = TRUE; 				

		/* adjust to current parent */
		mimeParser_unwindCurrentBoundary( p, s, FALSE );

        /*return MIME_INFO;*/ 					
    }
	/* end boundary */
    else if ( mi->nType == END_BOUNDARY || mi->nType == START_BOUNDARY )
    {
		decodeDataBuffer(p);

        if ( p->pDataSink != null )
        {
    		switch( p->nCurrentMessageType )
    		{
				case MIME_CONTENT_AUDIO:
				case MIME_CONTENT_IMAGE:
				case MIME_CONTENT_VIDEO:
				case MIME_CONTENT_APPLICATION:
    		    		case MIME_BASICPART:
					if ( p->pDataSink->endBasicPart != NULL )
						(p->pDataSink->endBasicPart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_BASICPART ) );
					break;

				case MIME_MULTIPART:
					if (mi->nType == END_BOUNDARY && p->pDataSink->endMultiPart != NULL) 
						(p->pDataSink->endMultiPart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_MULTIPART ) );
					break;

				case MIME_MESSAGEPART:
					if ( p->pDataSink->endMessagePart != NULL )
						(p->pDataSink->endMessagePart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_MESSAGEPART ) );
					break;
    		}

		/* 4/24 */
		if ( mi->nType == END_BOUNDARY )
		{
		    if (nGetCurrentParentType(p) == MIME_MULTIPART)
		    {
			(p->pDataSink->endMultiPart)(p->pDataSink, getUserObject(pGetCurrentParent(p), MIME_MULTIPART));
			m_needEndMultiPart = FALSE;
		    }
		    else
			m_needEndMultiPart = TRUE;
		}
		
        }

		if ( mi->nType == END_BOUNDARY )
		{
			mimeParser_unwindCurrentBoundary( p, s, TRUE );
			p->nCurrentMessageType = MIME_CONTENT_UNINITIALIZED;
        		p->fSeenBoundary = FALSE; 		
		}

		p->bStartData = FALSE;
		p->nLineType = MIME_BOUNDARY;
        	p->nMessagePartSubType = SUBTYPE_RFC822;
		p->bReadCR = FALSE;
		p->bQPEncoding = FALSE;
        	p->nEmptyLineNo = 0;
        	p->nCurrentMessageType = MIME_CONTENT_UNINITIALIZED;
        	p->nLastBoundry = START_BOUNDARY;

		mimeInfo_free( mi );
		mi = NULL;

		return MIME_OK;
	}

    /* message data	   */
    else if ( p->bStartData )
    {
        	nIndex = Vector_addElement( p->pVectorMessageData, s, len );
		mimeInfo_free( mi );
		mi = NULL;

		if (p->nCurrentMessageType == UNINITIALIZED)
		{
		    if (nGetCurrentParentType(p) == MIME_MULTIPART)
		    {
			mi = mimeInfo_new();
        		mi->nType = MIME_INFO;
        		mi->szName = "content-type";
        		mi->szValue = "text/plain";
        		mimeParser_setData (p, mi);
        		mimeParser_parseMimeInfo (p, mi);
                	Vector_addElement (p->pVectorMimeInfo, mi, sizeof(mimeInfo_t));

			/* free */
			mi->szName = NULL;
			mi->szValue = NULL;
			mimeInfo_free (mi);

			mi = NULL;
		    }
		}

		if ( bIsBasicPart( p->nCurrentMessageType ) )
		{
			pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
			pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPart_internal->nStartMessageDataIndex == UNINITIALIZED )
				pMimeBasicPart_internal->nStartMessageDataIndex = nIndex;

			pMimeBasicPart_internal->nEndMessageDataIndex = nIndex;
			pMimeBasicPart_internal->nRawMessageSize += len;

			if ( bLastLine )
			{
				decodeDataBuffer(p);

				if ( p->pDataSink != null )
				{
    				switch( p->nCurrentMessageType )
    				{
					case MIME_CONTENT_AUDIO:
					case MIME_CONTENT_IMAGE:
					case MIME_CONTENT_VIDEO:
					case MIME_CONTENT_APPLICATION:
    					case MIME_BASICPART:
						if ( p->pDataSink->endBasicPart != NULL )
							(p->pDataSink->endBasicPart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_BASICPART ) );
						break;

						case MIME_MULTIPART:
							if ( p->pDataSink->endMultiPart != NULL )
								(p->pDataSink->endMultiPart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_MULTIPART ) );
							break;

						case MIME_MESSAGEPART:
							if ( p->pDataSink->endMessagePart != NULL )
								(p->pDataSink->endMessagePart)(p->pDataSink, getUserObject( p->pCurrentMessage, MIME_MESSAGEPART ) );
							break;
    				}
				}
			}
		}

        p->nLineType = MIME_MESSAGE_DATA;

		return MIME_OK;
    }
    
     /* check and add preamble  */
     if (p->nCurrentMessageType == MIME_MULTIPART && p->nEmptyLineNo > 0 && p->fSeenBoundary == FALSE)  
     {
	  pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;				 
          s[len] = '\n';								         
	  if (pMimeMultiPart->preamble == NULL)							 
	  {											 
	     pMimeMultiPart->preamble =  (char *) malloc (len + 2);				 
	     memset (pMimeMultiPart->preamble, 0, len + 2);					 
	     strncpy (pMimeMultiPart->preamble, s, len+1);					 
	  }							 				 
	  else							 				 
	  {							 				 
		/*append string*/							 	 
		pTmp = (char *) malloc (strlen (pMimeMultiPart->preamble) + len + 2);		 
	     	memset (pTmp, 0, strlen (pMimeMultiPart->preamble) + len + 2);			 
		strcpy (pTmp, pMimeMultiPart->preamble);					 
		free (pMimeMultiPart->preamble);						 
		pMimeMultiPart->preamble = strncat (pTmp, s, len+1);				 
	  }							 				 

	  p->nLineType = mi->nType;								 
	  return MIME_OK;   									 
    }						 					 	 

    for ( len = strlen(s), i = 0; i < len && !finished; i++ )
    {
        switch ( s[i] )
        {
            case ':':
				s[i] = 0;
                mi->szName = s;

				/* remove junk spaces */
				for ( ++i; s[i] != 0 && isspace( s[i] ); i++ )
					;

                mi->szValue = &s[i];

                if ( mi->nType == MIME_INFO )
				{
					ret = mimeParser_setData( p, mi );
					if (ret != MIME_OK)
	    					return ret;

					if ( p->nMessagePartSubType != SUBTYPE_EXTERNAL_BODY )
					{
						mimeParser_parseMimeInfo( p, mi );
						Vector_addElement( p->pVectorMimeInfo, mi, sizeof( mimeInfo_t ) );
					}
				}

                else if ( ( mi->nType == MIME_HEADER || mi->nType == MIME_XHEADER ) && p->tHeaderParent.p != NULL )
                {
			addHeader( p, mi->szName, mi->szValue, TRUE );
        		p->nEmptyLineNo = 0;
		}
                
                finished = TRUE;
                break;
        }
    }

	/* add preamble  */
	if (!finished &&  p->nCurrentMessageType == MIME_MULTIPART)
	{
		pMimeMultiPart = (mime_multiPart_t *) p->pCurrentMessage;
                s[len] = '\n';
		if (pMimeMultiPart->preamble == NULL)
		{
		     pMimeMultiPart->preamble =  (char *) malloc (len + 2);
		     memset (pMimeMultiPart->preamble, 0, len + 2);
		     strncpy (pMimeMultiPart->preamble, s, len+1);
		}
		else
		{
			/*append string*/
			pTmp = (char *) malloc (strlen (pMimeMultiPart->preamble) + len + 2);
		     	memset (pTmp, 0, strlen (pMimeMultiPart->preamble) + len + 2);
			strcpy (pTmp, pMimeMultiPart->preamble);
			free (pMimeMultiPart->preamble);
			pMimeMultiPart->preamble = strncat (pTmp, s, len+1);
		}

        	p->nEmptyLineNo = 0; /* reset for preamble */
	}

	p->nLineType = mi->nType;

	/* note, don't free this stuff, wasn't allocated with malloc */
	mi->szName = NULL;
	mi->szValue = NULL;
	mimeInfo_free( mi );
	mi = NULL;

	if ( bLastLine )
		decodeDataBuffer(p);

	return MIME_OK;
}






/*
* carsonl, jan 8,98 
* decode message data
*
* parameter :
*
*	mimeParser_t *p	: instance of mimeParser
*
* returns : nothing
*/
static void decodeDataBuffer( mimeParser_t *p )
{
    	int offset = 0;
	int i;
	int len;
	mime_basicPart_t *pMimeBasicPart;
	mime_basicPart_internal_t *pMimeBasicPart_internal;
	char *szNewBuffer;
	char *szDecodedBuffer;
	char *szLine;
    	BOOLEAN bDecoded = FALSE;
	char achTemp[512];
	
	/* decode message */			  
    	if ( p == NULL )
	{
		return;
	}

	if ( !bIsBasicPart( p->nCurrentMessageType ) || p->pCurrentMessage == NULL )
	{
		return;
	}

	pMimeBasicPart = (mime_basicPart_t *) p->pCurrentMessage;
	pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

	/* already decoded */
	if ( pMimeBasicPart_internal == NULL ||
		pMimeBasicPart_internal->bDecodedData ||
		pMimeBasicPart_internal->nRawMessageSize == 0 ||
		pMimeBasicPart_internal->nStartMessageDataIndex == UNINITIALIZED )
	{
		return;
	}

	/* base64 */
	if ( pMimeBasicPart->encoding_type == MIME_ENCODING_BASE64 && IsDecodeData(p) )
	{
		len = decodeBase64Vector( pMimeBasicPart_internal->nStartMessageDataIndex,
							pMimeBasicPart_internal->nEndMessageDataIndex,
							p->pVectorMessageData,
							&szDecodedBuffer,
							pMimeBasicPart_internal->nRawMessageSize,
							p->pDataSink != NULL ? &p->out_byte : NULL,
							p->pDataSink != NULL ? &p->out_bits : NULL );

		if ( len > 0 )
		{
			saveBodyData( p, szDecodedBuffer, len, pMimeBasicPart );
			bDecoded = TRUE;

			if ( p->pDataSink != NULL && !IsLocalStorage(p) )
			{
				free( szDecodedBuffer );
			}
		}
	}

	/* quoted printable */
	else if ( pMimeBasicPart->encoding_type == MIME_ENCODING_QP && IsDecodeData(p) )
	{
		len = decodeQPVector (pMimeBasicPart_internal->nStartMessageDataIndex,
					pMimeBasicPart_internal->nEndMessageDataIndex,
					p->pVectorMessageData,
					&szDecodedBuffer,
					pMimeBasicPart_internal->nRawMessageSize,
					p->achQPLeftOverBytes,
					&p->QPNoOfLeftOverBytes);

		if ( len > 0 )
		{
			saveBodyData( p, szDecodedBuffer, len, pMimeBasicPart );
			bDecoded = TRUE;

	   		if ( p->pDataSink != NULL && !IsLocalStorage(p) )
			{
				free( szDecodedBuffer );
			}
		}
	}
	/* plain buffer creation */
	else
	{
		i = pMimeBasicPart_internal->nEndMessageDataIndex - pMimeBasicPart_internal->nStartMessageDataIndex + 1;

		/* build buffer */
		szNewBuffer = (char *) malloc( pMimeBasicPart_internal->nRawMessageSize + 2 + ( i * 2) );

		if ( szNewBuffer != NULL )
		{
			for (	i = pMimeBasicPart_internal->nStartMessageDataIndex, offset = 0;
					i <= pMimeBasicPart_internal->nEndMessageDataIndex;
					i++ )
			{
				szLine = (char *) Vector_elementAt( p->pVectorMessageData, i );
				sprintf( achTemp, "%s\r\n", szLine ); 

				if ( szLine != NULL )
				{
					len = strlen( achTemp );
					strncpy( &szNewBuffer[ offset ], achTemp, len );
					offset += len;
				}
			}
			
			/* pMimeBasicPart_internal->nMessageSize = pMimeBasicPart_internal->nRawMessageSize; */
			pMimeBasicPart_internal->nMessageSize = offset;
			szNewBuffer[offset] = NULL;
		}

		saveBodyData( p, szNewBuffer, offset, pMimeBasicPart );
		bDecoded = TRUE;

		if ( p->pDataSink != NULL && !IsLocalStorage(p) )
		{
			free( szNewBuffer );
		}
	}

	Vector_deleteAll( p->pVectorMessageData );

	if ( bDecoded )
    {
   		if ( p->pDataSink == NULL )
		{
			pMimeBasicPart_internal->bDecodedData = TRUE;
			p->bStartData = FALSE;
		}
       	else
       	{
			pMimeBasicPart_internal->nStartMessageDataIndex = UNINITIALIZED;
			pMimeBasicPart_internal->nEndMessageDataIndex = UNINITIALIZED;
        }
    }

    return;
}




/*
* carsonl, jan 8,98 
* parse data from either an inputstream or data buffer, if pInput != NULL, then it read data off the inputstream
* else if pData != NULL, then it reads data from the data buffer
*
* parameter :
*
*	mimeParser_t *p : instance of mimeParser
*	nsmail_inputstream_t *pInput : inputstream
*	char *pData : data buffer
*	int nLen :	size of data buffer
*	int nContentType :
*		use nContentType = 0 if you want it to parse the entire message 
*			nContentType >= MIME_CONTENT_TEXT && <= MIME_CONTENT_APPLICATION for basicPart
*			nContentType = MIME_CONTENT_MULTIPART for multiPart
*			nContentType = MIME_CONTENT_MESSAGEPART for messagePart
*
*	void **ppReturn : (output) fully populated message structure
*		if  nContentType = 0 or MIME_PARSE_ALL		mimeMessage
*			nContentType == MIME_BASICPART			mimeBasicPart
*			nContentType = MIME_MULTIPART				mimeMultiPart
*			nContentType = MIME_MESSAGEPART			mimeMessagePart
*
* returns : MIME_OK, MIME_INVALIDPARAM
*/
int mimeParser_parseMimeMessage (mimeParser_t *p, nsmail_inputstream_t *pInput, 
				char *pData, int nLen, int nContentType, void **ppReturn )
{
	int offset;
    	int ch;
    	int nNoOfBytes;
	char *s;
	int i, ret;
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	BOOLEAN bLastLine = FALSE;
	BOOLEAN bBufferFull = TRUE;

	if ( p == NULL || ( pInput == NULL && pData == NULL ) )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	offset = p->nLeftoverBytes;

	/* inputstream version */
	if ( pInput != NULL )
	{
		for ( nNoOfBytes = (pInput->read)( pInput->rock, (char *) p->pInputBuffer, BUFFER_SIZE );
				nNoOfBytes > 0;
				nNoOfBytes = (pInput->read)( pInput->rock, (char *) p->pInputBuffer, BUFFER_SIZE ) )
		{
			if ( nNoOfBytes < BUFFER_SIZE )
				bBufferFull = FALSE;

			for ( i = 0, ch = p->pInputBuffer[i]; i < nNoOfBytes; ch = p->pInputBuffer[ ++i ] )
			{
				/* eat it */
				if ( !p->bReadCR && ch == '\r' )
					continue;
        
				else if ( ch == '\n' )
				{
					p->pLeftoverBuffer[offset] = 0;
					s = szTrim( p->pLeftoverBuffer );

					if ( !bBufferFull && i == nNoOfBytes - 1 )
						bLastLine = TRUE;

					/* look for continuation */
					if ( bStringEquals( s, "content-" ) )
					{
					/*fprintf (stderr, "strlen(s)=%d offset=%d\n", strlen(s),offset);*/
						if (offset > strlen(s))  
						     offset = strlen(s); 
						if ( p->pInputBuffer[i+1] == ' ' || p->pInputBuffer[i+1] == '\t' )
						{
							p->pLeftoverBuffer[offset++] = ch;
							p->pLeftoverBuffer[offset] = 0;
							continue;
						}
					}
					
					if ( p->bReadCR )
					{
						p->pLeftoverBuffer[offset++] = ch;
						p->pLeftoverBuffer[offset] = 0;
					}

					ret = mimeParser_parseLine( p, p->pLeftoverBuffer, offset, bLastLine );
					if (ret != MIME_OK)
					     return ret;
					offset = 0;
            
					continue;
				}
        
				p->pLeftoverBuffer[ offset++ ] = ch;
			}
		}

		/* handle lastline */
		if ( p->pDataSink == NULL && offset != 0 )
		{
			ret = mimeParser_parseLine( p, p->pLeftoverBuffer, offset, bLastLine );
			if (ret != MIME_OK)
			     return ret;
		}

		p->nLeftoverBytes = offset;
	}		

	/* buffer version */
	else if ( pData != NULL )
	{
		for ( i = 0, ch = pData[i]; i < nLen; ch = pData[ ++i ] )
		{
			/* eat it */
			if ( !p->bReadCR && ch == '\r' )
				continue;
    
			else if ( ch == '\n' )
			{
				p->pLeftoverBuffer[offset] = 0;
				s = szTrim( p->pLeftoverBuffer );

				/* look for continuation */
				/*if ( bStringEquals( s, "content-" ) && p->pLeftoverBuffer[ offset - 1 ] == ';' )*/
				if (bStringEquals(s, "content-")) 				
				{								
					if (offset > strlen(s))  				
					     offset = strlen(s); 				
					if ( pData[i+1] == ' ' || pData[i+1] == '\t' )		
					{							
						p->pLeftoverBuffer[offset++] = ch;		
						p->pLeftoverBuffer[offset] = 0;			
						continue;					
					}							
				}								

				if ( p->bReadCR )
				{
					p->pLeftoverBuffer[offset++] = ch;
					p->pLeftoverBuffer[offset] = 0;
				}

				ret = mimeParser_parseLine( p, p->pLeftoverBuffer, offset, bLastLine );
				if (ret != MIME_OK)
			     		return ret;
				offset = 0;
        
				continue;
			}
    
			p->pLeftoverBuffer[ offset++ ] = ch;
		}

        /* handle lastline */
        if ( p->pDataSink == NULL && offset != 0 )
	{
		ret = mimeParser_parseLine( p, p->pLeftoverBuffer, offset, bLastLine );
		if (ret != MIME_OK)
	  		return ret;
	}

		p->nLeftoverBytes = offset;
	}

	/* for simple messages with no boundary */
    if ( ( p->pMimeMessage != NULL && p->pDataSink == null ) ||
		( p->bLocalStorage && p->pDataSink != null ) )
	{
		decodeDataBuffer(p);
	}

	pMimeMessage = p->pMimeMessage;
	pMimeMessageInternal = (mime_message_internal_t *) p->pMimeMessage->pInternal;

	if ( ppReturn != NULL )
	{
		switch ( nContentType )
		{
			case MIME_PARSE_ALL:
				*ppReturn = (void *) p->pMimeMessage;
				p->pMimeMessage = NULL;		/* user is responsible for freeing the memory */
				break;

			case MIME_CONTENT_AUDIO:
			case MIME_CONTENT_IMAGE:
			case MIME_CONTENT_VIDEO:
			case MIME_CONTENT_APPLICATION:
			case MIME_BASICPART:
				*ppReturn = (void *) pMimeMessageInternal->pMimeBasicPart;
				pMimeMessageInternal->pMimeBasicPart = NULL;	/* user is responsible for freeing the memory */
				break;

			case MIME_CONTENT_MULTIPART:
				*ppReturn = (void *) pMimeMessageInternal->pMimeMultiPart;
				pMimeMessageInternal->pMimeMultiPart = NULL;	/* user is responsible for freeing the memory */
				break;

			case MIME_CONTENT_MESSAGEPART:
				*ppReturn = (void *) pMimeMessageInternal->pMimeMessagePart;
				pMimeMessageInternal->pMimeMessagePart = NULL;	/* user is responsible for freeing the memory */
				break;
		}
	}

/* 
	if ( pMimeMessage != NULL && )
		checkForEmptyMessages( p, pMimeMessage, MIME_MESSAGE );
*/

	return MIME_OK;
}





/*
* carsonl, jan 8,98 
* save message data
*
* parameter :
*
*	mimeParser_t *p						: instance of mimeParser
*	char *pBuffer						: input buffer
*	int nLen							: size of buffer
*	mime_basicPart_t *pMimeBasicPart	: the corresponding basicPart that is linked with this data
*
* returns : nothing
*/
static void saveBodyData( mimeParser_t *p, char *pBuffer, int nLen, mime_basicPart_t *pMimeBasicPart )
{
	mime_basicPart_internal_t *pMimeBasicPart_internal;
	int nSize;
	char *szBuffer;

    if ( p == NULL || pBuffer == NULL || pMimeBasicPart == NULL )
	{
		return;
	}

	if ( nLen <= 0 )
		return;

	pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

	if ( pMimeBasicPart_internal == NULL )
		return;

	if ( IsLocalStorage(p) )
	{
		if ( p->bParseEntireFile )
		{
			pMimeBasicPart_internal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPart_internal != NULL )
			{
				pMimeBasicPart_internal->szMessageBody = pBuffer;
				pMimeBasicPart_internal->nMessageSize = nLen;
    		}
		}
		else
		{
    		if ( pMimeBasicPart_internal->szMessageBody == NULL )
    		{
				pMimeBasicPart_internal->szMessageBody = pBuffer;
				pMimeBasicPart_internal->nMessageSize = nLen;
				pMimeBasicPart_internal->nDynamicBufferSize = nLen;
    		}

		/* append to existing buffer */
    		else
    		{
				/* still fits */
				if ( pMimeBasicPart_internal->nDynamicBufferSize - pMimeBasicPart_internal->nMessageSize > nLen )
				{
					memcpy( &pMimeBasicPart_internal->szMessageBody[ pMimeBasicPart_internal->nMessageSize ], pBuffer, nLen );
					pMimeBasicPart_internal->nMessageSize += nLen;
				}

				/* doesn't fit */
				else
				{
					nSize = ( nLen * 2 ) > BUFFER_INCREMENT ? nLen * 2 : BUFFER_INCREMENT;
					nSize += pMimeBasicPart_internal->nMessageSize;

					szBuffer = (char*) malloc( nSize + 1 );

					if ( szBuffer != NULL )
					{
						memcpy( szBuffer, pMimeBasicPart_internal->szMessageBody, pMimeBasicPart_internal->nMessageSize );
						memcpy( &szBuffer[ pMimeBasicPart_internal->nMessageSize ], pBuffer, nLen );

						if ( pMimeBasicPart_internal->szMessageBody != NULL )
						{
							free( pMimeBasicPart_internal->szMessageBody );
							pMimeBasicPart_internal->szMessageBody = NULL;
						}             

						pMimeBasicPart_internal->szMessageBody = szBuffer;
						pMimeBasicPart_internal->nMessageSize += nLen;
						pMimeBasicPart_internal->nDynamicBufferSize = nSize;
					}
					else
					{
					}
				}

				free( pBuffer );
    		}
		}
	}

	if ( p->pDataSink != null && p->pDataSink->bodyData != NULL )
		(p->pDataSink->bodyData)(p->pDataSink, pMimeBasicPart_internal->pUserObject, pBuffer, nLen );
	else
		pMimeBasicPart_internal->bDecodedData = TRUE;
}




static int nAddMessage( mimeParser_t *p, void *pMessage, mime_content_type nContentType )
{
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	mime_multiPart_t *pMimeMultiPart;
	mime_messagePart_t *pMimeMessagePart;
	mime_messagePart_internal_t *pMimeMessagePartInternal;
	int index;

	if ( p == NULL || pMessage == NULL )
	{
#ifdef ERRORLOG
		errorLog( "nAddMessage()", MIME_ERR_INVALIDPARAM );
#endif

		return MIME_ERR_INVALIDPARAM;
	}

	pMimeMessageInternal = (mime_message_internal_t *) p->pMimeMessage->pInternal;

	if ( p->nMessageType == MIME_CONTENT_UNINITIALIZED && pMimeMessageInternal != NULL )
	{
		if ( bIsBasicPart( nContentType ) )
		{
			mime_message_addBasicPart_clonable( p->pMimeMessage, (mime_basicPart_t *) pMessage, FALSE );
			p->nMessageType = MIME_BASICPART;
		}
	
		else if ( nContentType == MIME_CONTENT_MULTIPART )
		{
			mime_message_addMultiPart_clonable( p->pMimeMessage, (mime_multiPart_t *) pMessage, FALSE );
			p->nMessageType = MIME_CONTENT_MULTIPART;
			vAddCurrentParent( p, MIME_MULTIPART, pMessage );
			p->fSeenBoundary = FALSE;
		}
		
		else if ( nContentType == MIME_CONTENT_MESSAGEPART )
		{
			mime_message_addMessagePart_clonable( p->pMimeMessage, (mime_messagePart_t *) pMessage, FALSE );
			p->nMessageType = MIME_CONTENT_MESSAGEPART;

			vAddCurrentParent( p, MIME_MESSAGEPART, pMessage );
		}
	}

	/* add to mimemessage */
	else if ( nGetCurrentParentType(p) == MIME_MESSAGE )
	{
		pMimeMessage = pGetCurrentParent(p);

		if ( pMimeMessage != NULL && pMimeMessage->pInternal != NULL )
		{
			if ( bIsBasicPart( nContentType ) )
			{
				mime_message_addBasicPart( pMimeMessage, (mime_basicPart_t *) pMessage, FALSE );
			}

			else if ( nContentType == MIME_CONTENT_MULTIPART )
			{
				mime_message_addMultiPart( pMimeMessage, (mime_multiPart_t *) pMessage, FALSE );

				/* reset current parent */
				vAddCurrentParent( p, MIME_MULTIPART, pMessage );
			}
		
			else if ( nContentType == MIME_CONTENT_MESSAGEPART )
			{
				mime_message_addMessagePart( pMimeMessage, (mime_messagePart_t *) pMessage, FALSE );

				/* reset current parent */
				pMimeMessagePart = (mime_messagePart_t *) pMessage;
				pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessage->pInternal;
								
				if ( pMimeMessagePartInternal != NULL )
				{
					vAddCurrentParent( p, MIME_MESSAGEPART, pMessage );

					/* reset current mimeMessage */
					p->pCurrentMimeMessage = pMimeMessagePartInternal->pTheMessage;
				}
			}
		}
	}
	
	else if ( nGetCurrentParentType(p) == MIME_MULTIPART )
	{
		pMimeMultiPart = pGetCurrentParent(p);

		if ( bIsBasicPart( nContentType ) )
		{
			mime_multiPart_addBasicPart( pMimeMultiPart, (mime_basicPart_t *) pMessage, FALSE, &index );
		}

		else if ( nContentType == MIME_CONTENT_MULTIPART )
		{
			mime_multiPart_addMultiPart( pMimeMultiPart, (mime_multiPart_t *) pMessage, FALSE, &index );

			/* reset current parent */
			vAddCurrentParent( p, MIME_MULTIPART, pMessage );
		}

		else if ( nContentType == MIME_CONTENT_MESSAGEPART )
		{
			mime_multiPart_addMessagePart( pMimeMultiPart, (mime_messagePart_t *) pMessage, FALSE, &index );

			/* reset current parent */
			vAddCurrentParent( p, MIME_MESSAGEPART, pMessage );
		}
	}

	else if ( nGetCurrentParentType(p) == MIME_MESSAGEPART )
	{
		pMimeMessagePart = pGetCurrentParent(p);

		if ( pMimeMessagePart != NULL && pMimeMessagePart->pInternal != NULL )
		{
			pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;
			pMimeMessage = pMimeMessagePartInternal->pTheMessage;

			if ( pMimeMessage != NULL )
			{
				if ( bIsBasicPart( nContentType ) )
				{
					mime_message_addBasicPart( pMimeMessage, (mime_basicPart_t *) pMessage, FALSE );
				}

				else if ( nContentType == MIME_CONTENT_MULTIPART )
				{
					mime_message_addMultiPart( pMimeMessage, (mime_multiPart_t *) pMessage, FALSE );

					/* reset current parent */
					vAddCurrentParent( p, MIME_MULTIPART, pMessage );
				}
			
				else if ( nContentType == MIME_CONTENT_MESSAGEPART )
				{
					mime_message_addMessagePart( pMimeMessage, (mime_messagePart_t *) pMessage, FALSE );

					/* reset current parent */
					pMimeMessagePart = (mime_messagePart_t *) pMessage;
					pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessage->pInternal;
									
					if ( pMimeMessagePartInternal != NULL )
					{
						vAddCurrentParent( p, MIME_MESSAGEPART, pMessage );

						/* reset current mimeMessage */
						p->pCurrentMimeMessage = pMimeMessagePartInternal->pTheMessage;
					}
				}
			}
		}
	}

	setCurrentMessage( p, pMessage, nContentType );

	return MIME_OK;
}



/* --------------------------------- helper routines -------------------------------- */



/*
* carsonl, jan 8,98 
* get content type
*
* parameter :
*
*	char *s			:	input buffer
*	char *szSubtype :	(output) subtype returned
*	char **ppParam	:	(output) extra parameters
*
*	out_bits : number of bits remaining, used internally by mimeParser
*	out_byte : output byte, used internally by mimeParser
*	int *pLen :	number of bytes in buffer
*
* returns	: content type
*/
mime_content_type mimeParser_nGetContentType( char *s, char *szSubtype, char **ppParam )
{
	int i, nStart, nSubtypeStart;
	char achContentType[64];
	mime_content_type nContentType = MIME_CONTENT_UNINITIALIZED;

	if ( s == NULL || szSubtype == NULL || ppParam == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	achContentType[0] = 0;
	szSubtype[0] = 0;

	/* skip spaces and colons */
	for ( i=0; s[i] != 0 && ( isspace( s[i] ) || s[i] == ':' ); i++ )
		;

	/* skip to subtype */
	for ( nStart = i; s[i] != 0 && s[i] != '/' && s[i] != ';'; i++ )
		;

	/* content type */
	strncpy( achContentType, &s[nStart], i - nStart );
	achContentType[ i - nStart ] = 0;

	/* content subtype */
	if ( s[i] == '/' )
	{
		for ( nSubtypeStart = ++i; s[i] != 0 && s[i] != ';' && s[i] != ','; i++ )
		;

		/* content subtype */
		strncpy( szSubtype, &s[nSubtypeStart], i - nSubtypeStart );
		szSubtype[ i - nSubtypeStart ] = 0;
	}

	for ( ; i < s[i] != 0 && ( s[i] == ' ' || s[i] == ';'  || s[i] == ',' ); i++ )
		;

	*ppParam = i > (int) strlen(s) - 1 ? NULL : &s[i];

	/* determine content type */
	if ( bStringEquals( achContentType, "text" ) )
		nContentType = MIME_CONTENT_TEXT;

	else if ( bStringEquals( achContentType, "audio" ) )
		nContentType = MIME_CONTENT_AUDIO;

	else if ( bStringEquals( achContentType, "image" ) )
		nContentType = MIME_CONTENT_IMAGE;

	else if ( bStringEquals( achContentType, "video" ) )
		nContentType = MIME_CONTENT_VIDEO;

	else if ( bStringEquals( achContentType, "application" ) )
		nContentType = MIME_CONTENT_APPLICATION;

	else if ( bStringEquals( achContentType, "multipart" ) )
		nContentType = MIME_CONTENT_MULTIPART;

	else if ( bStringEquals( achContentType, "message" ) )
		nContentType = MIME_CONTENT_MESSAGEPART;

	else
	{
		/*nContentType = MIME_CONTENT_TEXT;*/
		return MIME_ERR_INVALID_CONTENTTYPE;
	}

	return nContentType;
}



/* return boundary string, default to "-----" if none is found */
/*
* carsonl, jan 8,98 
* parse for boundary
*
* parameter :
*
*	char *s : input buffer
*
* returns : boundary string
* returns : default to "-----" if none is found 
*/
char *mimeParser_parseForBoundary( char *s )
{
	char achBuffer[64];
	char *szStart;
	char *szStart2;
	int nLen;
	BOOLEAN bQuotes = FALSE;

	if ( s == NULL )
	{
		return NULL;
	}

	szStart = strstr( s, "boundary" );

	if ( szStart == NULL )
		strcpy( achBuffer, "-----" );

	else
	{
		/* skip spaces and colons */
		for ( szStart += 9; *szStart != 0 && ( isspace( *szStart ) || *szStart == '=' ); szStart++ )
			;

		if ( *szStart == '"' )
		{
			bQuotes = TRUE;
			szStart++;
		}

		szStart2 = szStart;

		for ( nLen = 0; *szStart != 0 && *szStart != ';'; nLen++, szStart++ )
		{
			if ( bQuotes && *szStart == '"' )
				break;
		}

		if ( nLen < 61 )
		{
			strncpy( achBuffer, szStart2, nLen );
			achBuffer[nLen] = 0;
		}
	}

	return szStringClone( achBuffer );
}




/*
* carsonl, jan 8,98 
* check what kind of line this is
*
* parameter :
*
*	mimeParser_t *p : instance of mimeParser
*	char *s			: input line
*
* returns : line type
*
*			MIME_ERR_INVALIDPARAM;
*			MIME_INFO;
*			MIME_XHEADER;
*			MIME_CRLF;
*			MIME_HEADER;
*			NOT_A_BOUNDARY;
*			END_BOUNDARY;
*			START_BOUNDARY;
*/
int mimeParser_checkForLineType( mimeParser_t *p, char *s, int len )
{
	int nContentType;
	int i;

	if ( p == NULL || s == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( p->bStartData )
	{
		nContentType = mimeParser_nBoundaryCheck( p, s, len );
		
		if ( nContentType != NOT_A_BOUNDARY )
			return nContentType;
	}

    else if ( bStringEquals( s, "content-" ) )
        return MIME_INFO;

    else if ( bStringEquals( s, "x-" ) )
        return MIME_XHEADER;

    else if ( strlen( s ) == 0 )
        return MIME_CRLF;

    else
	{
		nContentType = mimeParser_nBoundaryCheck( p, s, len );
		
		if ( nContentType != NOT_A_BOUNDARY )
			return nContentType;
        else
        {
            for ( i = 0; i < len; i++ )
            {
                if ( s[i] == ':' )
                    return MIME_HEADER;
            }
        }
	}

    return MIME_MESSAGE_DATA;
}




/*
* carsonl, jan 8,98 
* determine if line is a boundary
*
* parameter :
*
*	mimeParser_t *p	: instance of mimeParser
*	char *s			: input string
*
* returns : boundary type
*
*			START_BOUNDARY		if it's a start boundary
*			END_BOUNDARY		if it's a end boundary
*			NOT_A_BOUNDARY		if it's not a boundary
*/
int mimeParser_nBoundaryCheck( mimeParser_t *p, char *s, int len )
{
	int boundaryLen;
	char * currentBoundary;

	if ( p == NULL || s == NULL )
	{
		return NOT_A_BOUNDARY;
	}

	currentBoundary =  mimeParser_getCurrentBoundary (p);

	if (currentBoundary == NULL)
	     return NOT_A_BOUNDARY;

	boundaryLen = strlen (currentBoundary);

	if ((len >= boundaryLen + 4) && s [0] == '-' && s [1] == '-' &&
	     s [boundaryLen + 2] == '-' && s [boundaryLen + 3] == '-' &&
	     bStringEquals (&s[2], mimeParser_getCurrentBoundary (p)) )
		return END_BOUNDARY;
	else if ((len >= boundaryLen + 2) && s [0] == '-' && s [1] == '-' &&
	     bStringEquals (&s[2], mimeParser_getCurrentBoundary (p)) )
 		return START_BOUNDARY;


/*	if ( s[len - 1] == '-' && s[len - 2] == '-' && bStringEquals( &s[2], mimeParser_getCurrentBoundary( p ) ) )
 *		return END_BOUNDARY;
 *	
 *	else if ( s[0] == '-' && s[1] == '-' && bStringEquals( &s[2], mimeParser_getCurrentBoundary( p ) ) )
 *		return START_BOUNDARY;
 */

	return NOT_A_BOUNDARY;
}


static void * getUserObject2 (void *pMessage, int contType)
{
	if (contType >= 0)
	switch (contType)
	{
		case MIME_CONTENT_TEXT:
		case MIME_CONTENT_AUDIO:
		case MIME_CONTENT_IMAGE:
		case MIME_CONTENT_VIDEO:
		case MIME_CONTENT_APPLICATION:
		     return getUserObject (pMessage, MIME_BASICPART);
		case MIME_CONTENT_MULTIPART:
		     return getUserObject (pMessage, MIME_MULTIPART);
		case MIME_CONTENT_MESSAGEPART:
		     return getUserObject (pMessage, MIME_MESSAGEPART);
	}

	return null; 
}


/*
* carsonl, jan 8,98 
* get user object, for dynamic parsing only
*
* parameter :
*
*	void *pMessage	: input message
*	int nType		: message/part type
*
* returns : return user object for that part
*/
static void *getUserObject( void *pMessage, int nType )
{
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	mime_basicPart_t *pMimeBasicPart;
	mime_multiPart_t *pMimeMultiPart;
	mime_messagePart_t *pMimeMessagePart;
	mime_basicPart_internal_t *pMimeBasicPartInternal;
	mime_multiPart_internal_t *pMimeMultiPartInternal;
	mime_messagePart_internal_t *pMimeMessagePartInternal;

	if ( pMessage == NULL )
	{
		return NULL;
	}

	switch ( nType )
	{
		case MIME_MESSAGE:

			pMimeMessage = (mime_message_t *) pMessage;
			pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

			if ( pMimeMessageInternal != NULL )
				return pMimeMessageInternal->pUserObject;
			break;

		case MIME_BASICPART:

			pMimeBasicPart = (mime_basicPart_t *) pMessage;
			pMimeBasicPartInternal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPartInternal != NULL )
				return pMimeBasicPartInternal->pUserObject;
			break;

		case MIME_MULTIPART:

			pMimeMultiPart = (mime_multiPart_t *) pMessage;
			pMimeMultiPartInternal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

			if ( pMimeMultiPartInternal != NULL )
				return pMimeMultiPartInternal->pUserObject;
			break;

		case MIME_MESSAGEPART:

			pMimeMessagePart = (mime_messagePart_t *) pMessage;
			pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			if ( pMimeMessagePartInternal != NULL )
				return pMimeMessagePartInternal->pUserObject;
			break;
	}
}




/*
* carsonl, jan 8,98 
* set user object, only for dynamic parsing
*
* parameter :
*
*	void *pMessage	: input message
*	int nType		: message/part type
*	void *pUserObject : user object
*
* returns : nothing
*/
static void setUserObject( void *pMessage, int nType, void *pUserObject )
{
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	mime_basicPart_t *pMimeBasicPart;
	mime_multiPart_t *pMimeMultiPart;
	mime_messagePart_t *pMimeMessagePart;
	mime_basicPart_internal_t *pMimeBasicPartInternal;
	mime_multiPart_internal_t *pMimeMultiPartInternal;
	mime_messagePart_internal_t *pMimeMessagePartInternal;

	if ( pMessage == NULL )
	{
		return;
	}

	switch ( nType )
	{
		case MIME_MESSAGE:

			pMimeMessage = (mime_message_t *) pMessage;
			pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

			if ( pMimeMessageInternal != NULL )
				pMimeMessageInternal->pUserObject = pUserObject;
			break;

		case MIME_BASICPART:

			pMimeBasicPart = (mime_basicPart_t *) pMessage;
			pMimeBasicPartInternal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPartInternal != NULL )
				pMimeBasicPartInternal->pUserObject = pUserObject;
			break;

		case MIME_MULTIPART:

			pMimeMultiPart = (mime_multiPart_t *) pMessage;
			pMimeMultiPartInternal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

			if ( pMimeMultiPartInternal != NULL )
				pMimeMultiPartInternal->pUserObject = pUserObject;
			break;

		case MIME_MESSAGEPART:

			pMimeMessagePart = (mime_messagePart_t *) pMessage;
			pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			if ( pMimeMessagePartInternal != NULL )
				pMimeMessagePartInternal->pUserObject = pUserObject;
			break;
	}
}



/*
* carsonl, jan 8,98 
* decode remaining bytes for dynamic parsing
*
* parameter :
*
*	out_bits : number of bits remaining, used internally by mimeParser
*	out_byte : output byte, used internally by mimeParser
*	int *pLen :	number of bytes in buffer
*
* returns
*
*	NULL	not enought buffer space, should never happen
*	char *	decoded buffer, null terminated
*	pLen	length of data
*
* NOTE: the user should set the bDecodeData field within their datasinks prior to giving it to the parser
*/
static char *decodeBase64LeftOverBytes( int out_bits, int out_byte, int *pLen )
{
	int	mask;
	static char szOutput[5];
	int byte_pos = 0;

	if ( pLen == NULL )
	{
		return NULL;
	}

	/* Handle any bits still in the queue */
	while (out_bits >= 8) 
	{
		if ( byte_pos >= 4 )
		{
			return NULL;
		}

		if (out_bits == 8) 
		{
			szOutput[byte_pos++] = out_byte;
			out_byte = 0;
		}
		
		else 
		{
			mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
			szOutput[byte_pos++] = (out_byte & mask) >> (out_bits - 8);
			out_byte &= ~mask;
		}

		out_bits -= 8;
	}

	szOutput[ byte_pos ] = 0;
	*pLen = byte_pos;

	return szOutput;
}




/*
* carsonl, jan 8,98 
* test if data should be stored locally
*
* parameter :
*
*	mimeParser_t *p		pointer to mimeParser
*
* return :
*
*	TRUE	if not using dynamic parsing or data should be stored locally
*	FALSE	if using dynamic parsing with no local storage
*
* NOTE: the user should set the bLocalStorage field within their datasinks prior to giving it to the parser
*/
/*BOOLEAN IsLocalStorage( mimeParser_t *p ) { return p->pDataSink == NULL ? TRUE : p->pDataSink->bLocalStorage; } */
BOOLEAN IsLocalStorage( mimeParser_t *p ) { return p->bLocalStorage; }



/*
* carsonl, jan 8,98 
* test if data should be decoded
*
* parameter :
*
*	mimeParser_t *p		pointer to mimeParser
*
* return :
*
*	TRUE	decode data
*	FALSE	don't decode data, just parse
*
* NOTE: the user should set the bDecodeData field within their datasinks prior to giving it to the parser
*/
/*BOOLEAN IsDecodeData( mimeParser_t *p ) { return p->pDataSink == NULL ? TRUE : p->pDataSink->bDecodeData; } */
BOOLEAN IsDecodeData( mimeParser_t *p ) { return p->bDecodeData; }




/*
* carsonl, jan 8,98 
* get the boundary corresponding to the current multiPart message
*
* parameter :
*
*	mimeParser_t *p			: instance of mimeParser
*
* returns : current boundary as a null terminated string
*/
char *mimeParser_getCurrentBoundary( mimeParser_t *p )
{
	mime_multiPart_t *pMimeMultiPart;
	mime_multiPart_internal_t *pMimeMultiPartInternal;
	int i;

	if ( p == NULL )
	{
#ifdef ERRORLOG
		errorLog( "mimeParser_getCurrentBoundary()", MIME_ERR_INVALIDPARAM );
#endif

		return NULL;
	}

	for ( i = p->nCurrentParent - 1; i >= 0; i-- )
	{
		if ( p->aCurrentParent[i].p != NULL && p->aCurrentParent[i].type == MIME_MULTIPART )
		{
			pMimeMultiPart = (mime_multiPart_t *) p->aCurrentParent[i].p;
			pMimeMultiPartInternal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

			if ( pMimeMultiPartInternal != NULL )
				return pMimeMultiPartInternal->szBoundary;
		}
	}

	return NULL;
}



int nGetCurrentParentType( mimeParser_t *p )
{
	return p->aCurrentParent[ p->nCurrentParent - 1 ].type;
}



void *pGetCurrentParent( mimeParser_t *p )
{
	return p->aCurrentParent[ p->nCurrentParent - 1 ].p;
}



void vAddCurrentParent(  mimeParser_t *p, int nType, void *pParent )
{
	if ( p->nCurrentParent < MAX_MULTIPART )
	{
		p->aCurrentParent[ p->nCurrentParent ].type = nType;
		p->aCurrentParent[ p->nCurrentParent++ ].p = pParent;
	}
	
	else
	{
#ifdef ERRORLOG
		errorLog( "mimeParser_vAddCurrentParent()", MIME_ERR_MAX_NESTED_PARTS_REACHED );
#endif
 	}
}



void mimeParser_unwindCurrentBoundary( mimeParser_t *p, char *s, BOOLEAN bDelete )
{
	mime_multiPart_t *pMimeMultiPart;
	mime_multiPart_internal_t *pMimeMultiPartInternal;
	int i;
	char *szBoundary = NULL;

	if ( p == NULL )
	{
#ifdef ERRORLOG
		errorLog( "mimeParser_getCurrentBoundary()", MIME_ERR_INVALIDPARAM );
#endif
	}

	for ( i = p->nCurrentParent - 1; i >= 0; i-- )
	{
		if ( p->aCurrentParent[i].p != NULL && p->aCurrentParent[i].type == MIME_MULTIPART )
		{
			pMimeMultiPart = (mime_multiPart_t *) p->aCurrentParent[i].p;
			pMimeMultiPartInternal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

			if ( pMimeMultiPartInternal != NULL )
				szBoundary = pMimeMultiPartInternal->szBoundary;
			
			if ( szBoundary != NULL && bStringEquals( &s[2], szBoundary ) )
			{
				if (p->pDataSink != null && m_needEndMultiPart == TRUE)
				{
					(p->pDataSink->endMultiPart)(p->pDataSink, getUserObject(p->aCurrentParent[i].p, MIME_MULTIPART));
					m_needEndMultiPart = FALSE;
				}

				if ( bDelete )
					p->nCurrentParent--;
				
				break;
			}
		}

		p->nCurrentParent--;
	}
}


void appToLastHdrOnQueue (mimeParser_t *p, char *value)   
{
      mimeInfo_t * mi;
      int valueLen;
      char * newValue;

      mi = (mimeInfo_t *) Vector_popLastElement (p->pHeaderQueue);
      valueLen = strlen (mi->szValue) + strlen (value);

      /*newValue = (char *) malloc(valueLen+3);
       *memset (newValue, 0, valueLen+3);
       *strncat (newValue, "\r\n", 2);  
       *strcat (newValue, value);
       */

      newValue = (char *) malloc(valueLen+1);
      memset (newValue, 0, valueLen+1);
      strcpy (newValue, mi->szValue);
      strcat (newValue, value);
      /* free (mi->szValue) */
      mi->szValue = newValue;

      Vector_addElement (p->pHeaderQueue, mi, sizeof(mimeInfo_t));
}

/**
* add header
*
* @author Carson Lee
* @version %I%, %G%
*
* @param  mi.name header name
* @param  mi.value header value
* @return none
* @exception MIMEException
*/
static void addHeader( mimeParser_t *p, char *name, char *value, BOOLEAN addToQueue )
{
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	mime_basicPart_t *pMimeBasicPart;
	mime_basicPart_internal_t *pMimeBasicPartInternal;
	mime_messagePart_t *pMimeMessagePart;
	mime_messagePart_internal_t *pMimeMessagePartInternal;
	mimeInfo_t *mi;
	char *szName;
	char *szValue;
	int i, nSize;

    if ( name == NULL || value == NULL )
	{
#ifdef ERRORLOG
		errorLog( "addHeader()", MIME_ERR_INVALIDPARAM );
#endif
		return;
	}

    if (p->nCurrentMessageType == UNINITIALIZED &&  p->tHeaderParent.type == MIME_MESSAGE && 
	p->tHeaderParent.p == p->pMimeMessage) 						     
    { 						     					     
	szName = szStringClone( name );							     
	szValue = szStringClone( value );						     

	pMimeMessage =  p->pMimeMessage;						     
	pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;	     

	if ( pMimeMessage->rfc822_headers == NULL )					     
		pMimeMessage->rfc822_headers = mime_header_new( szName, szValue );	     
	else										     
		mime_header_add( pMimeMessage->rfc822_headers, szName, szValue );	     

	if (p->pDataSink != NULL && pMimeMessageInternal != NULL && p->pDataSink->header != NULL) 
		(p->pDataSink->header)(p->pDataSink, pMimeMessageInternal->pUserObject, szName, szValue ); 

  	p->szPreviousHeaderName = szName;						     
	
	return;						     				     
    }											     
	
    /* insert headers from queue */
	if ( addToQueue )
	{
		nSize = Vector_size( p->pHeaderQueue );

		if ( p->nCurrentMessageType != UNINITIALIZED && nSize > 0 )
		{
			for ( i = 0; i < nSize; i++ )
			{
				mi = (mimeInfo_t *) Vector_elementAt( p->pHeaderQueue, i );
				addHeader( p, mi->szName, mi->szValue, FALSE );
			}
    
			Vector_deleteAll( p->pHeaderQueue );
		}
	}

    /* add to header queue */
    if ( p->nCurrentMessageType == UNINITIALIZED && addToQueue )
    {
        mi = mimeInfo_new();

		if ( mi == NULL )
		{
			return;
		}

		szName = szStringClone( name );
		szValue = szStringClone( value );

		mi->szName = szName;
		mi->szValue = szValue;

        Vector_addElement( p->pHeaderQueue, mi, sizeof( mimeInfo_t ) );
	}

    else if ( p->tHeaderParent.type == MIME_MESSAGE )
    {
		szName = szStringClone( name );
		szValue = szStringClone( value );

		pMimeMessage = (mime_message_t *) p->tHeaderParent.p;
		pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

		if ( pMimeMessage->rfc822_headers == NULL )
			pMimeMessage->rfc822_headers = mime_header_new( szName, szValue );
		else
			mime_header_add( pMimeMessage->rfc822_headers, szName, szValue );

		if ( p->pDataSink != NULL && pMimeMessageInternal != NULL && p->pDataSink->header != NULL )
			(p->pDataSink->header)(p->pDataSink, pMimeMessageInternal->pUserObject, szName, szValue );

  		p->szPreviousHeaderName = szName;
    }

    else
    {
        if ((p->tHeaderParent.type == MIME_BASICPART) || (bIsBasicPart(p->tHeaderParent.type))) 
        {
			szName = szStringClone( name );
			szValue = szStringClone( value );

			pMimeBasicPart = (mime_basicPart_t *) p->tHeaderParent.p;
			pMimeBasicPartInternal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

			if ( pMimeBasicPart->extra_headers  == NULL )
				pMimeBasicPart->extra_headers  = mime_header_new( szName, szValue );
			else
				mime_header_add( pMimeBasicPart->extra_headers, szName, szValue );

			if ( p->pDataSink != NULL && pMimeBasicPartInternal != NULL && p->pDataSink->header != NULL )
				(p->pDataSink->header)(p->pDataSink, pMimeBasicPartInternal->pUserObject, szName, szValue );

  			p->szPreviousHeaderName = szName;
    	}
        
        else if ( p->tHeaderParent.type == MIME_MULTIPART || p->tHeaderParent.type == MIME_MESSAGEPART )
        {
			szName = szStringClone( name );
			szValue = szStringClone( value );

			pMimeMessagePart = (mime_messagePart_t *) p->tHeaderParent.p;
			pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

			if ( pMimeMessagePart->extra_headers  == NULL )
				pMimeMessagePart->extra_headers  = mime_header_new( szName, szValue );
			else
				mime_header_add( pMimeMessagePart->extra_headers, szName, szValue );

			if ( p->pDataSink != NULL && pMimeMessagePartInternal != NULL && p->pDataSink->header != NULL )
				(p->pDataSink->header)(p->pDataSink, pMimeMessagePartInternal->pUserObject, szName, szValue );

  			p->szPreviousHeaderName = szName;
        }
    }
}



/**
* check for empty messages in all basicparts
*
* @author Carson Lee
* @version %I%, %G%
*
* @param
*
*   o : any mime message structure, usually MIMEMessage
*
* @return none
* @exception MIMEException
*/
static BOOLEAN checkForEmptyMessages( mimeParser_t *p, void *pMessage, int type )
{
	mime_message_t *pMimeMessage;
	mime_message_internal_t *pMimeMessageInternal;
	mime_basicPart_t *pMimeBasicPart;
	mime_basicPart_internal_t *pMimeBasicPartInternal;
	mime_multiPart_t *pMimeMultiPart;
	mime_multiPart_internal_t *pMimeMultiPartInternal;
	mime_messagePart_t *pMimeMessagePart;
	mime_messagePart_internal_t *pMimeMessagePartInternal;
	int i;
	mime_mp_partInfo_t *pPartInfo;
	BOOLEAN bEmpty = FALSE;

    if ( p == NULL || pMessage == NULL )
	{
#ifdef ERRORLOG
		errorLog( "checkForEmptyMessages()", MIME_ERR_INVALIDPARAM );
#endif
		return TRUE;
	}

	else if ( type == MIME_MESSAGE )
	{
		pMimeMessage = (mime_message_t *) pMessage;
		pMimeMessageInternal = (mime_message_internal_t *) pMimeMessage->pInternal;

		if ( pMimeMessageInternal != NULL )
		{
			if ( pMimeMessageInternal->pMimeBasicPart != NULL )
				bEmpty = checkForEmptyMessages( p, pMimeMessageInternal->pMimeBasicPart, MIME_BASICPART );
		
			else if ( pMimeMessageInternal->pMimeMultiPart != NULL )
				bEmpty = checkForEmptyMessages( p, pMimeMessageInternal->pMimeBasicPart, MIME_MULTIPART );
		
			else if ( pMimeMessageInternal->pMimeMessagePart != NULL )
				bEmpty = checkForEmptyMessages( p, pMimeMessageInternal->pMimeBasicPart, MIME_MESSAGEPART );
		}
	}

	else if (type == MIME_BASICPART || bIsBasicPart(type))
	{
		pMimeBasicPart = (mime_basicPart_t *) pMessage;
		pMimeBasicPartInternal = (mime_basicPart_internal_t *) pMimeBasicPart->pInternal;

		if ( pMimeBasicPartInternal != NULL && pMimeBasicPartInternal->nMessageSize == 0 )
		{
			bEmpty = TRUE;

#ifdef ERRORLOG
			errorLog( "checkForEmptyMessages()", MIME_ERR_EMPTY_BODY );
#endif
		}
	}

	else if ( type == MIME_MULTIPART )
	{
		pMimeMultiPart = (mime_multiPart_t *) pMessage;
		pMimeMultiPartInternal = (mime_multiPart_internal_t *) pMimeMultiPart->pInternal;

		if ( pMimeMultiPartInternal != NULL )
		{
			if ( pMimeMultiPartInternal->nPartCount == 0 )
			{
				bEmpty = TRUE;

#ifdef ERRORLOG
				errorLog( "checkForEmptyMessages()", MIME_ERR_EMPTY_BODY );
#endif
			}
			else
			{
				for ( i = 0; !bEmpty && i < pMimeMultiPartInternal->nPartCount; i++ )
				{
					pPartInfo = &pMimeMultiPartInternal->partInfo[i];

					if ( pPartInfo != NULL && pPartInfo->pThePart != NULL )
						bEmpty = checkForEmptyMessages( p, pPartInfo->pThePart, pPartInfo->nContentType );
				}
			}
		}
	}

	else if ( type == MIME_MESSAGEPART )
	{
		pMimeMessagePart = (mime_messagePart_t *) pMessage;
		pMimeMessagePartInternal = (mime_messagePart_internal_t *) pMimeMessagePart->pInternal;

		if ( pMimeMessagePartInternal != NULL && pMimeMessagePartInternal->pTheMessage != NULL )
			bEmpty = checkForEmptyMessages( p, pMimeMessagePartInternal->pTheMessage, MIME_MESSAGE );
	}

	return bEmpty;
}




/**
* Set current mesage
*
* @author Carson Lee
* @version %I%, %G%
*
* @param  message input message
* @param  nMessageType message type
* @return MIME_OK if successful, an error code otherwise
* @exception none
*/
static int setCurrentMessage( mimeParser_t *p, void *pMessage, int nMessageType )
{

    if ( pMessage == null )
	{	
#ifdef ERRORLOG
		errorLog( "setCurrentMessage()", MIME_ERR_INVALIDPARAM );
#endif

    	return NSMAIL_ERR_INVALIDPARAM;
	}

    p->pCurrentMessage = pMessage;
    p->nCurrentMessageType = nMessageType;
    p->tNextHeaderParent.p = pMessage;
    p->tNextHeaderParent.type = nMessageType;

    /* folowing lines duplicated from java side */
    /*if (nMessageType == MIME_BASICPART)*/
    if (bIsBasicPart(nMessageType))
    {
	if ((nGetCurrentParentType(p) == MIME_MULTIPART) && p->fSeenBoundary == TRUE) 	
	{ 										
	    /* start adding following headers to this part itself */
	    /* I.e. change header parent */
            p->tHeaderParent.p = pMessage;
            p->tHeaderParent.type = nMessageType;
	    p->tNextHeaderParent.p = pMessage;
	    p->tNextHeaderParent.type = nMessageType;
	} 										
    }

    return NSMAIL_OK;
}


