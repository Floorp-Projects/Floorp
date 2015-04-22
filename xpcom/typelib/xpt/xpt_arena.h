/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Simple arena allocator for xpt (which avoids using NSPR).
 */

#ifndef __xpt_arena_h__
#define __xpt_arena_h__

#include "prtypes.h"
#include <stdlib.h>
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include <stdint.h>


/*
 * The XPT library is statically linked: no functions are exported from
 * shared libraries.
 */
#define XPT_PUBLIC_API(t)    t
#define XPT_PUBLIC_DATA(t)   t

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple Arena support. Use with caution!
 */ 

typedef struct XPTArena XPTArena;

XPT_PUBLIC_API(XPTArena *)
XPT_NewArena(uint32_t block_size, size_t alignment, const char* name);

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

XPT_PUBLIC_API(size_t)
XPT_SizeOfArena(XPTArena *arena, MozMallocSizeOf mallocSizeOf);

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
XPT_AssertFailed(const char *s, const char *file, uint32_t lineno)
  MOZ_PRETEND_NORETURN_FOR_STATIC_ANALYSIS;
#define XPT_ASSERT(_expr) \
    ((_expr)?((void)0):XPT_AssertFailed(# _expr, __FILE__, __LINE__))
#else
#define XPT_ASSERT(_expr) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __xpt_arena_h__ */
