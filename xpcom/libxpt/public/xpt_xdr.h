/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/*
 * Basic APIs for streaming typelib structures to/from disk.
 */

#ifndef __xpt_xdr_h__
#define __xpt_xdr_h__

#include "xpt_struct.h"
#include "prosdep.h"
#include "plhash.h"

typedef struct XPTXDRState      XPTXDRState;
typedef struct XPTXDRDatapool   XPTXDRDatapool;

PRBool
XPT_DoString(XPTXDRState *state, XPTString **strp);

PRBool
XPT_DoIdentifier(XPTState *state, char **identp);

/*
 * XXX need to think about ordering and API issues for offset<->addr
 * XXX conversion
 */
PRBool
XPT_GetOffsetForAddr(XPTXDRState *state, void *addr, uint32 *offsetp);

PRBool
XPT_GetAddrForOffset(XPTXDRState *state, uint32 offset, void **addr);

PRBool
XPT_Do32(XPTXDRState *state, uint32 *u32p);

PRBool
XPT_Do16(XPTXDRState *state, uint16 *u16p);

PRBool
XPT_Do8(XPTXDRState *state, uint8 *u8p);

/*
 * When working with bitfields, use the DoBits call with a uint8.
 * Only the appropriate number of bits are manipulated, so when
 * you're decoding you probably want to ensure that the uint8 is pre-zeroed.
 * When you're done sending bits along, use
 * XPT_FlushBits to make sure that you don't eat a leading bit from the
 * next structure.  (You should probably be writing full bytes' worth of bits
 * anyway, and zeroing out the bits you don't use, but just to be sure...)
 */

PRBool
XPT_DoBits(XPTXDRState *state, uint8 *u8p, uintN nbits);

/* returns the number of bits skipped, which should be 0-7 */
int
XPT_FlushBits(XPTXDRState *state);

typedef enum {
    XPTXDR_ENCODE,
    XPTXDR_DECODE
} XPTXDRMode;

struct XPTXDRState {
    XPTXDRMode          mode;
    XPTXDRDatapool      *pool;
};

struct XPTXDRDatapool {
    PLHash      *offset_map;
    char        *data;
    uint32      point;
    uint8       bit;
    uint32      allocated;
};

/* increase the data allocation for the pool by XPT_GROW_CHUNK */
#define XPT_GROW_CHUNK 8192
PRBool
XPT_GrowPool(XPTXDRDatapool *pool);

/* all data structures are big-endian */

#if defined IS_BIG_ENDIAN
#  define XPTXDR_SWAB32(x) x
#  define XPTXDR_SWAB16(x) x
#elif defined IS_LITTLE_ENDIAN
#  define XPTXDR_SWAB32(x) (((x) >> 24) |                                     \
			 (((x) >> 8) & 0xff00) |                                          \
			 (((x) << 8) & 0xff0000) |                                        \
			 ((x) << 24))
#  define XPTXDR_SWAB16(x) (((x) >> 8) | ((x) << 8))
#else
#  error "unknown byte order"
#endif

#endif /* __xpt_xdr_h__ */
