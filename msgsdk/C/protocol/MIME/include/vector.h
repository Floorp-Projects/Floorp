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

/* vector.h
*
* carsonl, oct 1,97
*/

#ifndef VECTOR_H
#define VECTOR_H

#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INITIAL_SIZE			16
#define VECTOR_TYPE_STRING		1
#define VECTOR_TYPE_MIMEINFO	2


/* private */
typedef struct Vector
{
	int nMaxSize;			/* max size */
	int nSize;				/* current size */
	int nType;				/* content type */
	void **pt;				/* pointer to object */

} Vector;


Vector *Vector_new( int nType1 );
int Vector_free( Vector *v );
int Vector_size( Vector *v );
int Vector_addElement( Vector *v, void *pObject, int nObjectSize );
void *Vector_elementAt( Vector *v, int nIndex );
Vector *Vector_clone( Vector *v );
int Vector_deleteAll( Vector *v );
void *Vector_deleteLastElement (Vector *v);
void *Vector_popLastElement (Vector *v);


#ifdef __cplusplus
}
#endif


#endif /* VECTOR_H */
