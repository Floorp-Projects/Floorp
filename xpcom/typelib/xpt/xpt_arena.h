/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Simple arena allocator for xpt (which avoids using NSPR).
 */

#ifndef __xpt_arena_h__
#define __xpt_arena_h__

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
XPT_NewArena(size_t block_size8, size_t block_size1);

XPT_PUBLIC_API(void)
XPT_DestroyArena(XPTArena *arena);

XPT_PUBLIC_API(void *)
XPT_ArenaCalloc(XPTArena *arena, size_t size, size_t alignment);

XPT_PUBLIC_API(size_t)
XPT_SizeOfArenaIncludingThis(XPTArena *arena, MozMallocSizeOf mallocSizeOf);

/* --------------------------------------------------------- */

#define XPT_CALLOC8(_arena, _bytes) XPT_ArenaCalloc((_arena), (_bytes), 8)
#define XPT_CALLOC1(_arena, _bytes) XPT_ArenaCalloc((_arena), (_bytes), 1)
#define XPT_NEWZAP(_arena, _struct) ((_struct *) XPT_CALLOC8((_arena), sizeof(_struct)))

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
