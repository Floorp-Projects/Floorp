/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/*-----------------------------------------------------------------------------
    coremem.h - used to be xp_mem.h
    
    Cross-Platform Memory API
-----------------------------------------------------------------------------*/


#ifndef _coremem_h_
#define _coremem_h_

#include "platform.h"

#ifdef XP_WIN16
#include <malloc.h>
#endif

/* global free routine */
#define XP_FREEIF(obj) do { if(obj) XP_FREE(obj); } while(0)

/*-----------------------------------------------------------------------------
Allocating Structures
-----------------------------------------------------------------------------*/

#define XP_NEW( x )       	(x*)malloc( sizeof( x ) )
#define XP_DELETE( p )  	free( p )



#ifdef XP_WIN16

XP_BEGIN_PROTOS
extern void * WIN16_realloc(void * ptr, unsigned long size);
extern void * WIN16_malloc(unsigned long size);
XP_END_PROTOS

#define XP_REALLOC(ptr, size)   WIN16_realloc(ptr, size)
#define XP_ALLOC(size)          WIN16_malloc(size)
#else

#if defined(DEBUG) && defined(MOZILLA_CLIENT)
/* Check that we never allocate anything greater than 64K.  If we ever tried,
   Win16 would choke, and we'd like to find out about it on some other platform
   (like, one where we have a working debugger). */
/* This code used to call abort. Unfortunately, on Windows, abort() doesn't
 * go to the debugger. Instead, it silently quits the program.
 * So use XP_ASSERT(FALSE) instead.
 */

#define XP_CHECK_ALLOC_SIZE(size)	((size) <= 0xFFFF ? size : (XP_ASSERT(FALSE), (size)))
#else
#define XP_CHECK_ALLOC_SIZE(size)	size
#endif

#define XP_REALLOC(ptr, size)   	realloc(ptr, XP_CHECK_ALLOC_SIZE(size))
#define XP_ALLOC(size)          	malloc(XP_CHECK_ALLOC_SIZE(size))
#endif


#ifdef DEBUG
#define XP_CALLOC(num, sz)	(((num)*(sz))<64000 ? calloc((num),(sz)) : (XP_ASSERT(FALSE), calloc((num),(sz))))
#else
#define XP_CALLOC(num, sz)      calloc((num), (sz))
#endif

#define XP_FREE(ptr)            free(ptr)
#define XP_NEW_ZAP(TYPE)        ( (TYPE*) calloc (1, sizeof (TYPE) ) )


/* --------------------------------------------------------------------------
  16-bit windows requires space allocated bigger than 32K to be of
  type huge.  For example:

  int HUGE * foo = halloc(100000);
-----------------------------------------------------------------------------*/

/* There's no huge realloc because win16 doesn't have a hrealloc,
 * and there's no API to discover the original buffer's size.
 */
#ifdef XP_WIN16
#define XP_HUGE __huge
#define XP_HUGE_ALLOC(SIZE) halloc(SIZE,1)
#define XP_HUGE_FREE(SIZE) hfree(SIZE)
#define XP_HUGE_MEMCPY(DEST, SOURCE, LEN) hmemcpy(DEST, SOURCE, LEN)
#else
#define XP_HUGE
#define XP_HUGE_ALLOC(SIZE) malloc(SIZE)
#define XP_HUGE_FREE(SIZE) free(SIZE)
#define XP_HUGE_MEMCPY(DEST, SOURCE, LEN) memcpy(DEST, SOURCE, LEN)
#endif

#define XP_HUGE_CHAR_PTR char XP_HUGE *

/*-----------------------------------------------------------------------------
Allocating Large Buffers
NOTE: this does not interchange with XP_ALLOC/XP_NEW/XP_FREE/XP_DELETE
-----------------------------------------------------------------------------*/

#if defined(XP_UNIX) || defined(XP_WIN32) || defined(XP_MAC)

/* don't typedef this to void* unless you want obscure bugs... */
typedef unsigned long *	XP_Block;

#define XP_ALLOC_BLOCK(SIZE)            malloc ((SIZE))
#define XP_FREE_BLOCK(BLOCK)            free ((BLOCK))
#ifdef XP_UNIXu
  /* On SunOS, realloc(0,n) ==> 0 */
# define XP_REALLOC_BLOCK(BLOCK,SIZE)    ((BLOCK) \
					  ? realloc ((BLOCK), (SIZE)) \
					  : malloc ((SIZE)))
#else /* !XP_UNIX */
# define XP_REALLOC_BLOCK(BLOCK,SIZE)    realloc ((BLOCK), (SIZE))
#endif /* !XP_UNIX */
#define XP_LOCK_BLOCK(PTR,TYPE,BLOCK)   PTR = ((TYPE) (BLOCK))
#ifdef DEBUG
#define XP_UNLOCK_BLOCK(BLOCK)          (void)BLOCK
#else
#define XP_UNLOCK_BLOCK(BLOCK)
#endif
#endif /* XP_UNIX || XP_WIN32 */

#ifdef XP_WIN16

typedef unsigned char * XP_Block;
#define XP_ALLOC_BLOCK(SIZE)            WIN16_malloc((SIZE))
#define XP_FREE_BLOCK(BLOCK)            free ((BLOCK))
#define XP_REALLOC_BLOCK(BLOCK,SIZE)    ((BLOCK) \
					  ? WIN16_realloc ((BLOCK), (SIZE)) \
					  : WIN16_malloc ((SIZE)))
#define XP_LOCK_BLOCK(PTR,TYPE,BLOCK)   PTR = ((TYPE) (BLOCK))
#ifdef DEBUG
#define XP_UNLOCK_BLOCK(BLOCK)          (void)BLOCK
#else
#define XP_UNLOCK_BLOCK(BLOCK)
#endif


#endif /* XP_WIN16 */



#define	PA_Block					XP_Block
#define	PA_ALLOC(S)				XP_ALLOC_BLOCK(S)
#define 	PA_FREE(B)				XP_FREE_BLOCK(B)
#define 	PA_REALLOC(B,S)			XP_REALLOC_BLOCK(B,S)
#define 	PA_LOCK(P,T,B)				XP_LOCK_BLOCK(P,T,B)
#define 	PA_UNLOCK(B)				XP_UNLOCK_BLOCK(B)

#endif /* _coremem_h_ */


