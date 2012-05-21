/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Basic APIs for streaming typelib structures to/from disk.
 */

#ifndef __xpt_xdr_h__
#define __xpt_xdr_h__

#include "xpt_struct.h"

PR_BEGIN_EXTERN_C

typedef struct XPTState         XPTState;
typedef struct XPTDatapool      XPTDatapool;
typedef struct XPTCursor        XPTCursor;

/* Opaque type, for internal use */
typedef struct XPTHashTable     XPTHashTable;

extern XPT_PUBLIC_API(PRBool)
XPT_DoString(XPTArena *arena, XPTCursor *cursor, XPTString **strp);

extern XPT_PUBLIC_API(PRBool)
XPT_DoStringInline(XPTArena *arena, XPTCursor *cursor, XPTString **strp);

extern XPT_PUBLIC_API(PRBool)
XPT_DoCString(XPTArena *arena, XPTCursor *cursor, char **strp);

extern XPT_PUBLIC_API(PRBool)
XPT_DoIID(XPTCursor *cursor, nsID *iidp);

extern XPT_PUBLIC_API(PRBool)
XPT_Do64(XPTCursor *cursor, PRInt64 *u64p);

extern XPT_PUBLIC_API(PRBool)
XPT_Do32(XPTCursor *cursor, PRUint32 *u32p);

extern XPT_PUBLIC_API(PRBool)
XPT_Do16(XPTCursor *cursor, PRUint16 *u16p);

extern XPT_PUBLIC_API(PRBool)
XPT_Do8(XPTCursor *cursor, PRUint8 *u8p);

extern XPT_PUBLIC_API(PRBool)
XPT_DoHeaderPrologue(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp, PRUint32 * ide_offset);
extern XPT_PUBLIC_API(PRBool)
XPT_DoHeader(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp);

typedef enum {
    XPT_ENCODE,
    XPT_DECODE
} XPTMode;

typedef enum {
    XPT_HEADER = 0,
    XPT_DATA = 1
} XPTPool;

struct XPTState {
    XPTMode          mode;
    PRUint32         data_offset;
    PRUint32         next_cursor[2];
    XPTDatapool      *pool;
    XPTArena         *arena;
};

struct XPTDatapool {
    XPTHashTable     *offset_map;
    char             *data;
    PRUint32         count;
    PRUint32         allocated;
};

struct XPTCursor {
    XPTState    *state;
    XPTPool     pool;
    PRUint32    offset;
    PRUint8     bits;
};

extern XPT_PUBLIC_API(XPTState *)
XPT_NewXDRState(XPTMode mode, char *data, PRUint32 len);

extern XPT_PUBLIC_API(PRBool)
XPT_MakeCursor(XPTState *state, XPTPool pool, PRUint32 len, XPTCursor *cursor);

extern XPT_PUBLIC_API(PRBool)
XPT_SeekTo(XPTCursor *cursor, PRUint32 offset);

extern XPT_PUBLIC_API(void)
XPT_DestroyXDRState(XPTState *state);

/* Set file_length based on the data used in the state.  (Only ENCODE.) */
extern XPT_PUBLIC_API(PRBool)
XPT_UpdateFileLength(XPTState *state);

/* returns the length of the specified data block */
extern XPT_PUBLIC_API(void)
XPT_GetXDRDataLength(XPTState *state, XPTPool pool, PRUint32 *len);

extern XPT_PUBLIC_API(void)
XPT_GetXDRData(XPTState *state, XPTPool pool, char **data, PRUint32 *len);

/* set or get the data offset for the state, depending on mode */
extern XPT_PUBLIC_API(void)
XPT_DataOffset(XPTState *state, PRUint32 *data_offsetp);

extern XPT_PUBLIC_API(void)
XPT_SetDataOffset(XPTState *state, PRUint32 data_offset);

extern XPT_PUBLIC_API(PRUint32)
XPT_GetOffsetForAddr(XPTCursor *cursor, void *addr);

extern XPT_PUBLIC_API(PRBool)
XPT_SetOffsetForAddr(XPTCursor *cursor, void *addr, PRUint32 offset);

extern XPT_PUBLIC_API(PRBool)
XPT_SetAddrForOffset(XPTCursor *cursor, PRUint32 offset, void *addr);

extern XPT_PUBLIC_API(void *)
XPT_GetAddrForOffset(XPTCursor *cursor, PRUint32 offset);

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

/*
 * If we're decoding, we want to read the offset before we check
 * for already-decoded values.
 *
 * Then we check for repetition: CheckForRepeat will see if we've already
 * encoded/decoded this value, and if so will set offset/addr correctly
 * and make already be true.  If not, it will set up the cursor for
 * encoding (reserve space) or decoding (seek to correct location) as
 * appropriate.  In the encode case, it will also set the addr->offset
 * mapping.
 */

#define XPT_PREAMBLE_(cursor, addrp, pool, size, new_curs, already)           \
    XPTMode mode = cursor->state->mode;                                       \
    if (!(mode == XPT_ENCODE || XPT_Do32(cursor, &new_curs.offset)) ||        \
        !CheckForRepeat(cursor, (void **)addrp, pool,                         \
                        mode == XPT_ENCODE ? size : 0u, &new_curs,            \
                        &already) ||                                          \
        !(mode == XPT_DECODE || XPT_Do32(cursor, &new_curs.offset)))          \
        return PR_FALSE;                                                      \
    if (already)                                                              \
        return PR_TRUE;                                                       \

#define XPT_PREAMBLE_NO_ALLOC(cursor, addrp, pool, size, new_curs, already)   \
  {                                                                           \
    XPT_PREAMBLE_(cursor, addrp, pool, size, new_curs, already)               \
  }

#define XPT_ERROR_HANDLE(arena, free_it)                                      \
 error:                                                                       \
    if (cursor->state->mode == XPT_DECODE)                                    \
    XPT_FREEIF(arena, free_it);                                               \
    return PR_FALSE;


PR_END_EXTERN_C

#endif /* __xpt_xdr_h__ */
