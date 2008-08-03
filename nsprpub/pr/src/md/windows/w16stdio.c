/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
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
