/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
/*
** w16stdio.c -- Callback functions for Win16 stdio read/write.
**
**
*/
#include "primpl.h"

/*
** _PL_MDStdioWrite() -- Win16 hackery to get console output
**
** Returns: number of bytes written.
**
*/
PRInt32
_PL_W16StdioWrite( void *buf, PRInt32 amount )
{
    int   rc;
    
    rc = fputs( buf, stdout );
    if ( rc == EOF )
    {
        // something about errno
        return(PR_FAILURE);
    }
    return( strlen(buf));
} /* end _PL_fputs() */

/*
** _PL_W16StdioRead() -- Win16 hackery to get console input
**
*/
PRInt32
_PL_W16StdioRead( void *buf, PRInt32 amount )
{
    char *bp;

    bp = fgets( buf, (int) amount, stdin );
    if ( bp == NULL )
    {
        // something about errno
        return(PR_FAILURE);
    }
    
    return( strlen(buf));
} /* end _PL_fgets() */
/* --- end w16stdio.c --- */

/*
** Wrappers, linked into the client, that call
** functions in LibC
**
*/

/*
** _PL_W16CallBackPuts() -- Wrapper for puts()
**
*/
int PR_CALLBACK _PL_W16CallBackPuts( const char *outputString )
{
    return( puts( outputString ));
} /* end _PL_W16CallBackPuts()  */    

/*
** _PL_W16CallBackStrftime() -- Wrapper for strftime()
**
*/
size_t PR_CALLBACK _PL_W16CallBackStrftime( 
    char *s, 
    size_t len, 
    const char *fmt,
    const struct tm *p )
{
    return( strftime( s, len, fmt, p ));
} /* end _PL_W16CallBackStrftime()  */    

/*
** _PL_W16CallBackMalloc() -- Wrapper for malloc()
**
*/
void * PR_CALLBACK _PL_W16CallBackMalloc( size_t size )
{
    return( malloc( size ));
} /* end _PL_W16CallBackMalloc()  */    

/*
** _PL_W16CallBackCalloc() -- Wrapper for calloc()
**
*/
void * PR_CALLBACK _PL_W16CallBackCalloc( size_t n, size_t size )
{
    return( calloc( n, size ));
} /* end _PL_W16CallBackCalloc()  */    

/*
** _PL_W16CallBackRealloc() -- Wrapper for realloc()
**
*/
void * PR_CALLBACK _PL_W16CallBackRealloc( 
    void *old_blk, 
    size_t size )
{
    return( realloc( old_blk, size ));
} /* end _PL_W16CallBackRealloc()  */

/*
** _PL_W16CallBackFree() -- Wrapper for free()
**
*/
void PR_CALLBACK _PL_W16CallBackFree( void *ptr )
{
    free( ptr );
    return;
} /* end _PL_W16CallBackFree()  */

/*
** _PL_W16CallBackGetenv() -- Wrapper for getenv()
**
*/
void * PR_CALLBACK _PL_W16CallBackGetenv( const char *name )
{
    return( getenv( name ));
} /* end _PL_W16CallBackGetenv  */


/*
** _PL_W16CallBackPutenv() -- Wrapper for putenv()
**
*/
int PR_CALLBACK _PL_W16CallBackPutenv( const char *assoc )
{
    return( putenv( assoc ));
} /* end _PL_W16CallBackGetenv  */
