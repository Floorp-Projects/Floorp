/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of XDR primitives. */

#include "xpt_xdr.h"
#include "nscore.h"
#include <string.h>             /* strchr */
#include "mozilla/EndianUtils.h"

#define CURS_POOL_OFFSET_RAW(cursor)                                          \
  ((cursor)->pool == XPT_HEADER                                               \
   ? (cursor)->offset                                                         \
   : (XPT_ASSERT((cursor)->state->data_offset),                               \
      (cursor)->offset + (cursor)->state->data_offset))

#define CURS_POOL_OFFSET(cursor)                                              \
  (CURS_POOL_OFFSET_RAW(cursor) - 1)

/* can be used as lvalue */
#define CURS_POINT(cursor)                                                    \
  ((cursor)->state->pool_data[CURS_POOL_OFFSET(cursor)])

static bool
CHECK_COUNT(NotNull<XPTCursor*> cursor, uint32_t space)
{
    // Fail if we're in the data area and about to exceed the allocation.
    // XXX Also fail if we're in the data area and !state->data_offset
    if (cursor->pool == XPT_DATA &&
        (CURS_POOL_OFFSET(cursor) + space > (cursor)->state->pool_allocated)) {
        XPT_ASSERT(0);
        fprintf(stderr, "FATAL: no room for %u in cursor\n", space);
        return false;
    }

    return true;
}

XPT_PUBLIC_API(void)
XPT_InitXDRState(XPTState* state, char *data, uint32_t len)
{
    state->next_cursor[0] = state->next_cursor[1] = 1;
    state->pool_data = data;
    state->pool_allocated = len;
}

/* All offsets are 1-based */
XPT_PUBLIC_API(void)
XPT_SetDataOffset(XPTState *state, uint32_t data_offset)
{
   state->data_offset = data_offset;
}

XPT_PUBLIC_API(bool)
XPT_MakeCursor(XPTState *state, XPTPool pool, uint32_t len,
               NotNull<XPTCursor*> cursor)
{
    cursor->state = state;
    cursor->pool = pool;
    cursor->bits = 0;
    cursor->offset = state->next_cursor[pool];

    if (!(CHECK_COUNT(cursor, len)))
        return false;

    /* this check should be in CHECK_CURSOR */
    if (pool == XPT_DATA && !state->data_offset) {
        fprintf(stderr, "no data offset for XPT_DATA cursor!\n");
        return false;
    }

    state->next_cursor[pool] += len;

    return true;
}

XPT_PUBLIC_API(bool)
XPT_SeekTo(NotNull<XPTCursor*> cursor, uint32_t offset)
{
    /* XXX do some real checking and update len and stuff */
    cursor->offset = offset;
    return true;
}

XPT_PUBLIC_API(bool)
XPT_SkipStringInline(NotNull<XPTCursor*> cursor)
{
    uint16_t length;
    if (!XPT_Do16(cursor, &length))
        return false;

    uint8_t byte;
    for (uint16_t i = 0; i < length; i++)
        if (!XPT_Do8(cursor, &byte))
            return false;

    return true;
}

XPT_PUBLIC_API(bool)
XPT_DoCString(XPTArena *arena, NotNull<XPTCursor*> cursor, char **identp,
              bool ignore)
{
    uint32_t offset = 0;
    if (!XPT_Do32(cursor, &offset))
        return false;

    if (!offset) {
        *identp = NULL;
        return true;
    }

    XPTCursor my_cursor;
    my_cursor.pool = XPT_DATA;
    my_cursor.offset = offset;
    my_cursor.state = cursor->state;
    char* start = &CURS_POINT(&my_cursor);

    char* end = strchr(start, 0); /* find the end of the string */
    if (!end) {
        fprintf(stderr, "didn't find end of string on decode!\n");
        return false;
    }
    int len = end - start;
    XPT_ASSERT(len > 0);

    if (!ignore) {
        char *ident = (char*)XPT_CALLOC1(arena, len + 1u);
        if (!ident)
            return false;

        memcpy(ident, start, (size_t)len);
        ident[len] = 0;
        *identp = ident;
    }

    return true;
}

/*
 * IIDs are written in struct order, in the usual big-endian way.  From the
 * typelib file spec:
 *
 *   "For example, this IID:
 *     {00112233-4455-6677-8899-aabbccddeeff}
 *   is converted to the 128-bit value
 *     0x00112233445566778899aabbccddeeff
 *   Note that the byte storage order corresponds to the layout of the nsIID
 *   C-struct on a big-endian architecture."
 *
 * (http://www.mozilla.org/scriptable/typelib_file.html#iid)
 */
XPT_PUBLIC_API(bool)
XPT_DoIID(NotNull<XPTCursor*> cursor, nsID *iidp)
{
    int i;

    if (!XPT_Do32(cursor, &iidp->m0) ||
        !XPT_Do16(cursor, &iidp->m1) ||
        !XPT_Do16(cursor, &iidp->m2))
        return false;

    for (i = 0; i < 8; i++)
        if (!XPT_Do8(cursor, (uint8_t *)&iidp->m3[i]))
            return false;

    return true;
}

// MSVC apparently cannot handle functions as template parameters very well,
// so we need to use a macro approach here.

#define XPT_DOINT(T, func, valuep)                \
    do {                                          \
        const size_t sz = sizeof(T);              \
                                                  \
        if (!CHECK_COUNT(cursor, sz)) {           \
            return false;                         \
        }                                         \
                                                  \
        *valuep = func(&CURS_POINT(cursor));      \
        cursor->offset += sz;                     \
        return true;                              \
    } while(0)

XPT_PUBLIC_API(bool)
XPT_Do64(NotNull<XPTCursor*> cursor, int64_t *u64p)
{
    XPT_DOINT(int64_t, mozilla::BigEndian::readInt64, u64p);
}

/*
 * When we're handling 32- or 16-bit quantities, we handle a byte at a time to
 * avoid alignment issues.  Someone could come and optimize this to detect
 * well-aligned cases and do a single store, if they cared.  I might care
 * later.
 */
XPT_PUBLIC_API(bool)
XPT_Do32(NotNull<XPTCursor*> cursor, uint32_t *u32p)
{
    XPT_DOINT(uint32_t, mozilla::BigEndian::readUint32, u32p);
}

XPT_PUBLIC_API(bool)
XPT_Do16(NotNull<XPTCursor*> cursor, uint16_t *u16p)
{
    XPT_DOINT(uint16_t, mozilla::BigEndian::readUint16, u16p);
}

#undef XPT_DOINT

XPT_PUBLIC_API(bool)
XPT_Do8(NotNull<XPTCursor*> cursor, uint8_t *u8p)
{
    if (!CHECK_COUNT(cursor, 1))
        return false;

    *u8p = CURS_POINT(cursor);

    cursor->offset++;

    return true;
}


