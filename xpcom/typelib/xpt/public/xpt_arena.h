/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Simple arena allocator for xpt (which avoids using NSPR).
 */

#ifndef __xpt_arena_h__
#define __xpt_arena_h__

#include "prtypes.h"
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

#ifdef DEBUG
XPT_PUBLIC_API(void)
XPT_AssertFailed(const char *s, const char *file, PRUint32 lineno);
#define XPT_ASSERT(_expr) \
    ((_expr)?((void)0):XPT_AssertFailed(# _expr, __FILE__, __LINE__))
#else
#define XPT_ASSERT(_expr) ((void)0)
#endif

PR_END_EXTERN_C

#endif /* __xpt_arena_h__ */
