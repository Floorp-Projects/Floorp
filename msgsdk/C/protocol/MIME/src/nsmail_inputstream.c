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
* nsmail_inputstream.c 
* carsonl, jan 8,98
*
* simple routines for a file inputstream implementation
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
#include <nsmail_inputstream.h>



/*
* carsonl, jan 8,98 
* constructor
*
* parameter : filename
*
* returns : new rock
*/
int fileRock_new( char *filename, fileRock_t **ppRock )
{
	if ( filename == NULL || ppRock == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	*ppRock = (fileRock_t *) malloc( sizeof( fileRock_t ) );

	if ( *ppRock == NULL )
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	(*ppRock)->filename = filename;
	(*ppRock)->offset = 0;

	(*ppRock)->buffer = (char *) malloc( BUFFER_SIZE );

	if ( (*ppRock)->buffer == NULL )
	{
		free( *ppRock );
		*ppRock = NULL;

		return MIME_ERR_OUTOFMEMORY;
	}

	(*ppRock)->f = fopen( filename, "r" );

	if ( (*ppRock)->f == NULL )
	{
		free( (*ppRock)->buffer );
		free( *ppRock );
		*ppRock = NULL;

		return MIME_ERR_CANTOPENFILE;
	}

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* free rock
*
* parameter : rock
*
* returns : nothing
*/
void fileRock_free( fileRock_t **ppRock )
{
	if ( ppRock == NULL )
	{
		return;
	}

	if ( *ppRock == NULL )
		return;

	if ( (*ppRock)->f != NULL )
		fclose( (*ppRock)->f );

	if ( (*ppRock)->buffer != NULL )
		free( (*ppRock)->buffer );

	free( *ppRock );
	*ppRock = NULL;
}



/*
* carsonl, jan 8,98 
* read
*
* parameter : rock, input buffer, size of buffer
*
* returns : number of bytes read
*/
int fileRock_read( void *rock, char *buf, unsigned size )
{
	fileRock_t *pRock;

	if ( rock == NULL || buf == NULL )
	{
		return 0;
	}

	pRock = (fileRock_t *) rock;
	
	if ( pRock->f == NULL )
		return 0;

	return fread( buf, sizeof(char), size, pRock->f );
}



/*
* carsonl, jan 8,98 
* rewind
*
* parameter : rock
*
* returns : nothing
*/
void fileRock_rewind( void *rock )
{
	fileRock_t *pRock;

	if ( rock == NULL )
	{
		return;
	}

	pRock = (fileRock_t *) rock;
	
	if ( pRock->f != NULL )
	{
		fseek( pRock->f, 0, SEEK_END );
	}

	return;
}



/*
* carsonl, jan 8,98 
* close
*
* parameter : rock
*
* returns : nothing
*/
void fileRock_close( void *rock )
{
	fileRock_t *pRock;

	if ( rock == NULL )
	{
		return;
	}

	pRock = (fileRock_t *) rock;
	
	if ( pRock->f != NULL )
	{
		fclose( pRock->f );
		pRock->f = NULL;
	}

	return;
}



/*
* carsonl, jan 8,98 
* constructor
*
* parameter :
*
*	char *filename : filename
*
* returns : new inputstream or NULL if failed
*/
int nsmail_inputstream_new( char *filename, nsmail_inputstream_t **ppInput )
{
	int nRet;

	if ( filename == NULL || ppInput == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	*ppInput = (nsmail_inputstream_t *) malloc( sizeof( nsmail_inputstream_t ) );

	if ( *ppInput == NULL )
	{
		return MIME_ERR_OUTOFMEMORY;
	}

	nRet = fileRock_new( filename, (fileRock_t **) &(*ppInput)->rock );

	if ( nRet == MIME_OK )
	{
		(*ppInput)->read = &fileRock_read;
		(*ppInput)->rewind = &fileRock_rewind;
		(*ppInput)->close = &fileRock_close;
	}

	return nRet;
}



/*
* carsonl, jan 8,98 
* destructor
*
* parameter :
*
*	nsmail_inputstream_t *in : inputstream
*
* returns : NSMAIL_OK, NSMAIL_ERR_INVALIDPARAM
*/
void nsmail_inputstream_free( nsmail_inputstream_t **ppInput )
{
	if ( ppInput == NULL )
	{
		return;
	}

	if ( *ppInput == NULL )
		return;

	if ( (*ppInput)->rock != NULL )
		fileRock_free( (fileRock_t **) &(*ppInput)->rock );

	free( *ppInput );
	*ppInput = NULL;
}

