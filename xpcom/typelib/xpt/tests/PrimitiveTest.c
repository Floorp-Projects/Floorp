/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Test the xdr primitives from xpt_xdr.c */

#include "xpt_xdr.h"
#include <stdio.h>
#include <string.h> /* for memcpy */

#define PASS(msg)							      \
  fprintf(stderr, "PASSED : %s\n", msg);

#define FAIL(msg)							      \
  fprintf(stderr, "FAILURE: %s\n", msg);

#define TRY_(msg, cond, silent)						      \
  if ((cond) && !silent) {						      \
    PASS(msg);								      \
  } else {								      \
    FAIL(msg);								      \
    return 1;								      \
  }

#define TRY(msg, cond)		TRY_(msg, cond, 0)
#define TRY_Q(msg, cond)	TRY_(msg, cond, 1);

XPTString in_str = { 4, "bazz" };

struct TestData {
    uint32_t	bit32;
    uint16_t      bit16;
    uint8_t       bit8[2];
    char	*cstr;
    XPTString   *str;
} input = { 0xdeadbeef, 0xcafe, {0xba, 0xbe}, "foobar", &in_str},
  output = {0, 0, {0, 0}, NULL, NULL };

void
dump_struct(char *label, struct TestData *str)
{
    fprintf(stderr, "%s: {%#08x, %#04x, {%#02x, %#02x}, %s, %d/%s}\n",
	    label, str->bit32, str->bit16, str->bit8[0], str->bit8[1],
	    str->cstr, str->str->length, str->str->bytes);
}

PRBool
XDR(XPTArena *arena, XPTCursor *cursor, struct TestData *str)
{
    TRY("Do32", XPT_Do32(cursor, &str->bit32));
    TRY("Do16", XPT_Do16(cursor, &str->bit16));
    TRY("Do8",  XPT_Do8 (cursor, &str->bit8[0]));
    TRY("Do8",  XPT_Do8 (cursor, &str->bit8[1]));
    TRY("DoCString", XPT_DoCString(arena, cursor, &str->cstr));
    TRY("DoString", XPT_DoString(arena, cursor, &str->str));
    return 0;
}

int
main(int argc, char **argv)
{
    XPTArena *arena;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *header, *data, *whole;
    uint32_t hlen, dlen, i;

    TRY("XPT_NewArena", (arena = XPT_NewArena(1024, sizeof(double), "main")));
    
    TRY("NewState (ENCODE)", (state = XPT_NewXDRState(XPT_ENCODE, NULL, 0)));

    XPT_SetDataOffset(state, sizeof input);

    TRY("MakeCursor", XPT_MakeCursor(state, XPT_HEADER, sizeof input, cursor));

    dump_struct("before", &input);

    if (XDR(arena, cursor, &input))
	return 1;

    fprintf(stderr, "ENCODE successful\n");
    XPT_GetXDRData(state, XPT_HEADER, &header, &hlen);
    fprintf(stderr, "XDR header %d bytes at %p:",
	    hlen, header);
    for (i = 0; i < hlen; i++)
	fprintf(stderr, "%c%02x", i ? ',' : ' ', (uint8_t)header[i]);
    fprintf(stderr, "\n");

    XPT_GetXDRData(state, XPT_DATA, &data, &dlen);

    fprintf(stderr, "XDR data %d bytes at %p:",
	    dlen, data);
    for (i = 0; i < dlen; i++)
	fprintf(stderr, "%c%02x/%c", i ? ',' : ' ', (uint8_t)data[i],
		(uint8_t)data[i]);
    fprintf(stderr, "\n");

    whole = malloc(dlen + hlen);
    if (!whole) {
	fprintf(stderr, "malloc %d failed!\n", dlen + hlen);
	return 1;
    }

    /* TRY_Q("malloc", (data2 = malloc(len))); */
    memcpy(whole, header, hlen);
    memcpy(whole + hlen, data, dlen);
    XPT_DestroyXDRState(state);

    TRY("NewState (DECODE)", (state = XPT_NewXDRState(XPT_DECODE, whole,
						      hlen + dlen)));

    TRY("MakeCursor", XPT_MakeCursor(state, XPT_HEADER, sizeof input, cursor));
    XPT_SetDataOffset(state, sizeof input);

    if (XDR(arena, cursor, &output))
	return 1;
    
    dump_struct("after", &output);
    XPT_DestroyXDRState(state);
    XPT_DestroyArena(arena);
    free(whole);

    return 0;
}
