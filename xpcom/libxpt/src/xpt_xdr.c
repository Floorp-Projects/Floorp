/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Implementation of XDR primitives. */

#include "xpt_xdr.h"

#define CHECK_COUNT(state)                            \
    ((state)->pool->count = (state)->pool->allocated ?                \
        ((state)->mode == XPT_ENCODE ? XPT_GrowPool((state)->pool) :       \
         PR_FALSE) : PR_TRUE)

XPTState *
XPT_NewXDRState(XPTMode mode, char *data, uint32 len)
{
    XPTState *state;
    state = PR_NEW(XPTState);
    if (!state)
        return NULL;
    state->mode = mode;
    state->pool = PR_NEW(XPTDatapool);
    if (!state->pool) {
        PR_FREE(state);
        return NULL;
    }
    state->pool->count = 0;
    state->pool->bit = 0;
    if (mode == XPT_DECODE) {
        state->pool->data = data;
        state->pool->allocated = len;
    } else {
        state->pool->data = PR_MALLOC(XPT_GROW_CHUNK);
        if (!state->pool->data) {
            PR_FREE(state->pool);
            PR_FREE(state);
            return NULL;
        }
        state->data->allocated = XPT_GROW_CHUNK;
    }
}

void
XPT_DestroyXDRState(XPTState *state)
{
    if (state->mode == XPT_ENCODE) {
        PR_FREE_IF(state->pool->data);
        PR_FREE(state->pool);
        PR_FREE(state);
    } else {
        PR_FREE(state->pool);
        PR_FREE(state);
    }
}

void
XPT_GetXDRData(XPTState *state, char **data, uint32 *len)
{
    *data = state->pool->data;
    *len = state->pool->count;
}

static PRBool
XPT_GrowPool(XPTDatapool *pool)
{
    char *newdata = realloc(pool->data, pool->allocated + XPT_GROW_CHUNK);
    if (!newdata)
        return PR_FALSE;
    pool->data = newdata;
    pool->allocated += XPT_GROW_CHUNK;
    return PR_TRUE;
}

PRBool
XPT_DoString(XPTCursor *cursor, XPTString **strp)
{
    XPTCursor my_cursor;
    XPTString *str = *strp;
    PRBool already;

    XPT_PREAMBLE(cursor, strp, XPT_DATA, str->length + 2, my_cursor,
                 already, XPTString, str);
    
    if (!XPT_Do16(&my_cursor, &str->length))
        goto error;

    for (i = 0; i < str->length; i++)
        if (!XPT_Do8(&my_cursor, &str->bytes[i]))
            goto error;

    return PR_TRUE;

    XPT_ERROR_HANDLE(str);
}

PRBool
XPT_DoCString(XPTCursor *cursor, char **identp)
{
    XPTCursor my_cursor;
    char *ident = *identp;
    PRBool already;
    
    XPT_PREAMBLE_NO_ALLOC(cursor, identp, XPT_DATA, strlen(ident) + 1,
                          my_cursor, already, ident);

    if (mode == XPT_DECODE) {
        char *start = my_cursor->state->data[my_cursor->offset], *end;
        int len;

        end = strchr(start, 0); /* find the end of the string */
        if (!end) {
            fprintf(stderr, "didn't find end of string on decode!\n");
            return PR_FALSE;
        }
        len = end - start;

        ident = PR_MALLOC(len + 1);
        if (!ident)
            return PR_FALSE;

        memcpy(ident, start, len);
        ident[len] = 0;
        *identp = ident;

        if (!XPT_SetAddrForOffset(my_cursor, my_cursor->offset, ident)) {
            PR_FREE(ident);
            return PR_FALSE;
        }

    } else {
        while(*ident)
            if (!XPT_Do8(&my_cursor, ident++))
                return PR_FALSE;
        if (!XPT_Do8(&my_cursor, ident)) /* write trailing zero */
            return PR_FALSE;
    }
    
    return PR_TRUE;
}

uint32
XPT_GetOffsetForAddr(XPTCursor *cursor, void *addr)
{
    return 0;
}

PRBool
XPT_SetOffsetForAddr(XPTState *state, uint32 offset, void **addr)
{
    *addr = NULL;
    return PR_FALSE;
}

PRBool
XPT_SetAddrForOffset(XPTCursor *cursor, void *addr)
{
    return PR_FALSE;
}

void *
XPT_GetAddrForOffset(XPTCursor *cursor)
{
    return PR_FALSE;
}

PRBool
XPT_CheckForRepeat(XPTCursor *cursor, void **addrp, XPTPool pool, int len,
                   XPTCursor *new_cursor, PRBool *already)
{
    void *last = *addrp;

    *already = PR_FALSE;
    new_cursor->state = cursor->state;
    new_cursor->pool = pool;

    if (cursor->state->mode = XPT_DECODE) {

        last = XPT_GetAddrForOffset(new_cursor);

        if (last) {
            *already = PR_TRUE;
            *addrp = last;
        }

    } else {

        new_cursor->offset = XPT_GetOffsetForAddr(new_cursor, last);
        if (new_cursor->offset) {
            *already = PR_TRUE;
            return PR_TRUE;
        }

        /* haven't already found it, so allocate room for it. */
        if (!XPT_AllocateCursor(cursor, pool, len, new_cursor) ||
            !XPT_SetOffsetForAddr(new_cursor, last))
            return PR_FALSE;
    }
    return PR_TRUE;
}


/*
 * When we're writing an IID, we have to do it in a magic order.  From the
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
PRBool
XPT_DoIID(XPTCursor *cursor, nsID *iidp)
{
    return PR_FALSE;
}

PRBool
XPT_Do32(XPTCursor *cursor, uint32 *u32p)
{
    return PR_FALSE;
}

PRBool
XPT_Do16(XPTCursor *cursor, uint16 *u16p)
{
    return PR_FALSE;
}

PRBool
XPT_Do8(XPTCursor *cursor, uint8 *u8p)
{
    return PR_FALSE;
}

static PRBool
do_bit(XPTCursor *cursor, uint8 *u8p, int bitno)
{
    int bit_value, delta, new_value;
    XPTDatapool *pool = state->pool;

    if (state->mode == XPT_ENCODE) {
        bit_value = (*u8p & 1) << (bitno);   /* 7 = 0100 0000, 6 = 0010 0000 */
        if (bit_value) {
            delta = pool->bit + (bitno) - 7;
            new_value = delta >= 0 ? bit_value >> delta : bit_value << -delta;
            pool->data[pool->count] |= new_value;
        }
    } else {
        bit_value = pool->data[pool->count] & (1 << (7 - pool->bit));
        *u2p = bit_value >> (7 - pool->bit);
    }
    if (++pool->bit == 8) {
        pool->count++;
        pool->bit = 0;
    }

    return CHECK_COUNT(state);
}

int
XPT_DoBits(XPTCursor *cursor, uint8 *u8p, uintN nbits)
{

#define DO_BIT(state, u8p, nbits)                         \
    if (!do_bit(state, u8p, nbits))                       \
       return PR_FALSE;

    switch(nbits) {
      case 7:
        DO_BIT(state, u8p, 7);
      case 6:
        DO_BIT(state, u8p, 6);
      case 5:
        DO_BIT(state, u8p, 5);
      case 4:
        DO_BIT(state, u8p, 4);
      case 3:
        DO_BIT(state, u8p, 3);
      case 2:
        DO_BIT(state, u8p, 2);
      case 1:
        DO_BIT(state, u8p, 1);
      default:;
    };

#undef DO_BIT

    return PR_TRUE;
}

int
XPT_FlushBits(XPTCursor *cursor)
{
    int skipped = 8 - state->pool->bits;

    state->pool->bits = 0;
    state->count++;

    if (!CHECK_COUNT(state))
        return -1;

    return skipped == 8 ? 0 : skipped;
}
