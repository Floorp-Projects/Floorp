/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XPTState         XPTState;
typedef struct XPTCursor        XPTCursor;

extern XPT_PUBLIC_API(bool)
XPT_SkipStringInline(NotNull<XPTCursor*> cursor);

extern XPT_PUBLIC_API(bool)
XPT_DoCString(XPTArena *arena, NotNull<XPTCursor*> cursor, char **strp,
              bool ignore = false);

extern XPT_PUBLIC_API(bool)
XPT_DoIID(NotNull<XPTCursor*> cursor, nsID *iidp);

extern XPT_PUBLIC_API(bool)
XPT_Do64(NotNull<XPTCursor*> cursor, int64_t *u64p);

extern XPT_PUBLIC_API(bool)
XPT_Do32(NotNull<XPTCursor*> cursor, uint32_t *u32p);

extern XPT_PUBLIC_API(bool)
XPT_Do16(NotNull<XPTCursor*> cursor, uint16_t *u16p);

extern XPT_PUBLIC_API(bool)
XPT_Do8(NotNull<XPTCursor*> cursor, uint8_t *u8p);

extern XPT_PUBLIC_API(bool)
XPT_DoHeader(XPTArena *arena, NotNull<XPTCursor*> cursor, XPTHeader **headerp);

typedef enum {
    XPT_HEADER = 0,
    XPT_DATA = 1
} XPTPool;

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

extern XPT_PUBLIC_API(void)
XPT_InitXDRState(XPTState* state, char* data, uint32_t len);

extern XPT_PUBLIC_API(bool)
XPT_MakeCursor(XPTState *state, XPTPool pool, uint32_t len,
               NotNull<XPTCursor*> cursor);

extern XPT_PUBLIC_API(bool)
XPT_SeekTo(NotNull<XPTCursor*> cursor, uint32_t offset);

extern XPT_PUBLIC_API(void)
XPT_SetDataOffset(XPTState *state, uint32_t data_offset);

/* all data structures are big-endian */

#if defined IS_BIG_ENDIAN
#  define XPT_SWAB32(x) x
#  define XPT_SWAB16(x) x
#elif defined IS_LITTLE_ENDIAN
#  define XPT_SWAB32(x) (((x) >> 24) |                                        \
             (((x) >> 8) & 0xff00) |                                          \
             (((x) << 8) & 0xff0000) |                                        \
             ((x) << 24))
#  define XPT_SWAB16(x) (((x) >> 8) | ((x) << 8))
#else
#  error "unknown byte order"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __xpt_xdr_h__ */
