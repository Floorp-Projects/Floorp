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
* nsmail_inputstream.h
* prasad jan 8,98
*
* basic inputstream implementation for file i/o
*/

#ifndef NSMAIL_INPUTSTREAM_H
#define NSMAIL_INPUTSTREAM_H

#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif


/* carson's additions -------------------------------------- */

	
typedef struct fileRock		/* my custom rock for persisting to a file */
{
	char *filename;
	FILE *f;
	char *buffer;
	int offset;

} fileRock_t;



int fileRock_new( char *filename, fileRock_t **ppRock );					/* constructor */
void fileRock_free( fileRock_t **ppRock );					/* destructor */
int fileRock_read( void *pRock, char *buf, unsigned size );	/* read */
void fileRock_rewind( void *pRock );							/* rewind */
void fileRock_close( void *pRock );							/* close */

int nsmail_inputstream_new( char *filename, nsmail_inputstream_t **ppInput );	/* constructor */
void nsmail_inputstream_free( nsmail_inputstream_t **ppInput );		/* destructor */


#ifdef __cplusplus
}
#endif


#endif /* NSMAIL_INPUTSTREAM_H */
