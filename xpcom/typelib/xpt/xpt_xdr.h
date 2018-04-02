/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Basic APIs for streaming typelib structures from disk.
 */

#ifndef __xpt_xdr_h__
#define __xpt_xdr_h__

#include "xpt_struct.h"
#include "mozilla/NotNull.h"

using mozilla::NotNull;

struct XPTArena;
struct XPTCursor;
struct XPTState;

bool
XPT_SkipStringInline(NotNull<XPTCursor*> cursor);

bool
XPT_DoCString(XPTArena *arena, NotNull<XPTCursor*> cursor, const char **strp,
              bool ignore = false);

bool
XPT_DoIID(NotNull<XPTCursor*> cursor, nsID *iidp);

bool
XPT_Do64(NotNull<XPTCursor*> cursor, int64_t *u64p);

bool
XPT_Do32(NotNull<XPTCursor*> cursor, uint32_t *u32p);

bool
XPT_Do16(NotNull<XPTCursor*> cursor, uint16_t *u16p);

bool
XPT_Do8(NotNull<XPTCursor*> cursor, uint8_t *u8p);

bool
XPT_DoHeader(XPTArena *arena, NotNull<XPTCursor*> cursor, XPTHeader **headerp);

enum XPTPool {
    XPT_HEADER = 0,
    XPT_DATA = 1
};

struct XPTState {
    uint32_t         data_offset;
    uint32_t         next_cursor[2];
    char             *pool_data;
    uint32_t         pool_allocated;
};

struct XPTCursor {
    XPTState    *state;
    XPTPool     pool;
    uint32_t    offset;
    uint8_t     bits;
};

void
XPT_InitXDRState(XPTState* state, char* data, uint32_t len);

bool
XPT_MakeCursor(XPTState *state, XPTPool pool, uint32_t len,
               NotNull<XPTCursor*> cursor);

bool
XPT_SeekTo(NotNull<XPTCursor*> cursor, uint32_t offset);

void
XPT_SetDataOffset(XPTState *state, uint32_t data_offset);

#endif /* __xpt_xdr_h__ */
