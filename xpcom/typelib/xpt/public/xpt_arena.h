/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * Simple arena allocator for xpt (which avoids using NSPR).
 */

#ifndef __xpt_arena_h__
#define __xpt_arena_h__

#include "prtypes.h"
#include <assert.h>
#include <stdlib.h>


/*
 * The linkage of XPT API functions differs depending on whether the file is
 * used within the XPT library or not.  Any source file within the XPT
 * library should define EXPORT_XPT_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPT_API
#define XPT_PUBLIC_API(t)    PR_IMPLEMENT(t)
#define XPT_PUBLIC_DATA(t)   PR_IMPLEMENT_DATA(t)
#else
#ifdef _WIN32
#    define XPT_PUBLIC_API(t)    _declspec(dllimport) t
#    define XPT_PUBLIC_DATA(t)   _declspec(dllimport) t
#else
#    define XPT_PUBLIC_API(t)    PR_IMPLEMENT(t)
#    define XPT_PUBLIC_DATA(t)   t
#endif
#endif
#define XPT_FRIEND_API(t)    XPT_PUBLIC_API(t)
#define XPT_FRIEND_DATA(t)   XPT_PUBLIC_DATA(t)

PR_BEGIN_EXTERN_C

/*
 * Simple Arena support. Use with caution!
 */ 

typedef struct XPTArena XPTArena;

XPT_PUBLIC_API(XPTArena *)
XPT_NewArena(PRUint32 block_size, size_t alignment, const char* name);

XPT_PUBLIC_API(void)
XPT_DestroyArena(XPTArena *arena);

XPT_PUBLIC_API(void)
XPT_DumpStats(XPTArena *arena);

XPT_PUBLIC_API(void *)
XPT_ArenaMalloc(XPTArena *arena, size_t size);

XPT_PUBLIC_API(char *)
XPT_ArenaStrDup(XPTArena *arena, const char * s);

XPT_PUBLIC_API(void)
XPT_NotifyDoneLoading(XPTArena *arena);

XPT_PUBLIC_API(void)
XPT_ArenaFree(XPTArena *arena, void* block);

/* --------------------------------------------------------- */

#define XPT_MALLOC(_arena, _bytes) \
    XPT_ArenaMalloc((_arena), (_bytes))

#ifdef DEBUG
#define XPT_FREE(_arena, _ptr) \
    XPT_ArenaFree((_arena), (_ptr))
#else
#define XPT_FREE(_arena, _ptr) \
    ((void)0)
#endif

#define XPT_STRDUP(_arena, _s) \
    XPT_ArenaStrDup((_arena), (_s))

#define XPT_CALLOC(_arena, _size) XPT_MALLOC((_arena), (_size))
#define XPT_NEW(_arena, _struct) ((_struct *) XPT_MALLOC((_arena), sizeof(_struct)))
#define XPT_NEWZAP(_arena, _struct) XPT_NEW((_arena), _struct)
#define XPT_DELETE(_arena, _ptr) do{XPT_FREE((_arena), (_ptr)); ((_ptr)) = NULL;}while(0)
#define XPT_FREEIF(_arena, _ptr) do{if ((_ptr)) XPT_FREE((_arena), (_ptr));}while(0)

/* --------------------------------------------------------- */

#define XPT_ASSERT(_expr) assert(_expr)

PR_END_EXTERN_C

#endif /* __xpt_arena_h__ */
