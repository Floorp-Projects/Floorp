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


/* vector.c
*
* simple vector class
*  carsonl, oct 1,97
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "nsmail.h"
#include "vector.h"
#include "util.h"
#include "mime.h"
#include "mime_internal.h"
#include "mimeparser.h"



/*
* carsonl, jan 8,98 
* constructor
*
* parameter :
*
*	int type1 : content type
*
* returns :	new instance of vector
*/
Vector *Vector_new( int nType )
{
	Vector *v = (Vector *) malloc( sizeof( Vector ) );
	
	if ( v == NULL )
	{
		return NULL;
	}

	v->nSize = 0;
	v->nMaxSize = INITIAL_SIZE;
	v->nType = nType;

	v->pt = (void **) malloc( INITIAL_SIZE * sizeof( void * ) );

	if ( v->pt == NULL )
	{
		free( v );

		return NULL;
	}

	return v;
}



/*
* carsonl, jan 8,98 
* clone
*
* parameter :
*
*	Vector *v : original vector
*
* returns :	new instance of vector, or NULL if incorrect parameters or out of memory
*/
Vector *Vector_clone( Vector *v )
{
	int i, len;
	Vector *v2;
	
	if ( v == NULL )
	{
		return NULL;
	}

	v2 = (Vector *) malloc( sizeof( Vector ) );

	if ( v2 == NULL )
	{
		return NULL;
	}

	v2->nMaxSize = v->nMaxSize;
	v2->nSize = v->nSize;
	v2->nType = v->nType;

	/* clone pointer array */
	v2->pt = (void **) malloc( v->nMaxSize * sizeof( void* ) );

	if ( v2->pt == NULL )
	{
		free( v2 );

		return NULL;
	}

	memcpy( v2->pt, v->pt, v->nSize * sizeof( void* ) );

	/* clone each element	 */
	for ( i=0; i < v->nSize; i++ )
	{
		if ( v->pt[i] != NULL )
		{
			switch ( v->nType )
			{
				case VECTOR_TYPE_STRING:
					len = strlen( (char *) v->pt[i] ) + 1;
					v2->pt[i] = (char *) malloc( len );

					if ( v2->pt[i] != NULL )
					{
						memcpy( v2->pt[i], v->pt[i], len ); 
					}
					break;

				case VECTOR_TYPE_MIMEINFO:
					v2->pt[i] = (void *) mimeInfo_clone( (mimeInfo_t *) v->pt[i] );
					break;
			}
		}
	}

	return v2;
}



/*
* carsonl, jan 8,98 
* destructor
*
* parameter :
*
*	Vector *v : vector
*
* returns :	MIME_OK if successful
*/
int Vector_free( Vector *v )
{
	int i;

	if ( v == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( v->pt != NULL )
	{
		for ( i=0; i < v->nSize; i++ )
		{
			if ( v->pt[i] != NULL )
			{
				switch ( v->nType )
				{
					case VECTOR_TYPE_STRING:
						free( v->pt[i] );
						break;

					case VECTOR_TYPE_MIMEINFO:
						mimeInfo_free( v->pt[i] );
						break;

					default:
						free( v->pt[i] );
				}
			}
		}

		free( v->pt );
	}

	free( v );

	return MIME_OK;
}




/*
* carsonl, jan 8,98 
* delete all elements
*
* parameter :
*
*	Vector *v : original vector
*
* returns :	MIME_OK if successful
*/
int Vector_deleteAll( Vector *v )
{
	int i;

	if ( v == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	if ( v->pt != NULL )
	{
		for ( i=0; i < v->nSize; i++ )
		{
			if ( v->pt[i] != NULL )
			{
				switch ( v->nType )
				{
					case VECTOR_TYPE_STRING:
						free( v->pt[i] );
						break;

					case VECTOR_TYPE_MIMEINFO:
						mimeInfo_free( v->pt[i] );
						break;

					default:
						free( v->pt[i] );
				}

				v->pt[i] = NULL;
			}
		}

		v->nSize = 0;
	}

	return MIME_OK;
}



/*
* carsonl, jan 8,98 
* add a new element
*
* parameter :
*
*	Vector *v : original vector
*	void *pObject : user's object
*	int nObjectSize : object size
*
* returns :	return a negative error code if failed, return the vector index if successful
*/
int Vector_addElement( Vector *v, void *pObject, int nObjectSize )
{
	if ( v == NULL || pObject == NULL || v->pt == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	/*  reallocate more space */
	if ( v->nSize >= v->nMaxSize )
	{
		void *ptOld = v->pt;
		v->nMaxSize += INITIAL_SIZE;

		v->pt = (void **) malloc( v->nMaxSize * sizeof( void* ) );

		if ( v->pt == NULL )
		{
			return MIME_ERR_OUTOFMEMORY;
		}

		memcpy( v->pt, ptOld, v->nSize * sizeof( void* ) );
		
		if ( ptOld != NULL )
			free( ptOld );
	}

	/* copy content */
	switch ( v->nType )
	{
		case VECTOR_TYPE_MIMEINFO:
			v->pt[ v->nSize ] = (void **) mimeInfo_clone( (mimeInfo_t *) pObject );
			break;

		default:
			/* store object */
			v->pt[ v->nSize ] = (void **) malloc( nObjectSize + 1 );

			/* out of memory */
			if ( v->pt[ v->nSize ] == NULL )
			{
				return MIME_ERR_OUTOFMEMORY;
			}

			memcpy( v->pt[v->nSize], pObject, nObjectSize + 1 );
			break;
	}

	/* add element */
	v->nSize++;

	return v->nSize - 1;
}



/*
* carsonl, jan 8,98 
* return the vector at nIndex
*
* parameter :
*
*	Vector *v : original vector
*	int nIndex : element index
*
* returns :	object at index
*/
void *Vector_elementAt( Vector *v, int nIndex )
{
	if ( v != NULL && nIndex < v->nSize )
		return v->pt[ nIndex ];

	return NULL;
}


void *Vector_deleteLastElement (Vector *v)
{
	int i;
	
	if (v != NULL &&  v->nSize > 0)
	{
		i = v->nSize;
		free(v->pt[i-1] );
		v->pt[i-1] = NULL;
		v->nSize--;
	}

	return NULL;
}


void *Vector_popLastElement (Vector *v)
{
	int i;
	void * p;
	
	if (v != NULL &&  v->nSize > 0)
	{
		i = v->nSize;
		p = v->pt[i-1];
		v->pt[i-1] = NULL;
		v->nSize--;
		return p;
	}

	return NULL;
}

/*
* carsonl, jan 8,98 
* return # of elements in the vector class 
*
* parameter :
*
*	Vector *v : original vector
*
* returns :	size of vector
*/
int Vector_size( Vector *v )
{
	if ( v == NULL )
	{
		return MIME_ERR_INVALIDPARAM;
	}

	return v->nSize;
}


