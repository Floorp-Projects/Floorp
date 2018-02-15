/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
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
 * Simple Arena support. Use with caution!
 */

struct XPTArena;

XPTArena*
XPT_NewArena(size_t block_size8, size_t block_size1);

void
XPT_DestroyArena(XPTArena *arena);

void*
XPT_ArenaCalloc(XPTArena *arena, size_t size, size_t alignment);

size_t
XPT_SizeOfArenaIncludingThis(XPTArena *arena, MozMallocSizeOf mallocSizeOf);

/* --------------------------------------------------------- */

#define XPT_CALLOC8(_arena, _bytes) XPT_ArenaCalloc((_arena), (_bytes), 8)
#define XPT_CALLOC1(_arena, _bytes) XPT_ArenaCalloc((_arena), (_bytes), 1)
#define XPT_NEWZAP(_arena, _struct) ((_struct *) XPT_CALLOC8((_arena), sizeof(_struct)))

/* --------------------------------------------------------- */

#ifdef DEBUG
void
XPT_AssertFailed(const char *s, const char *file, uint32_t lineno)
  MOZ_PRETEND_NORETURN_FOR_STATIC_ANALYSIS;
#define XPT_ASSERT(_expr) \
    ((_expr)?((void)0):XPT_AssertFailed(# _expr, __FILE__, __LINE__))
#else
#define XPT_ASSERT(_expr) ((void)0)
#endif

#endif /* __xpt_arena_h__ */
