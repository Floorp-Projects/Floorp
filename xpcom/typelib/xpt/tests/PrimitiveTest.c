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

/* Test the xdr primitives from xpt_xdr.c */

#include "xpt_xdr.h"
#include <stdio.h>

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

struct TestData {
    uint32	bit32;
    uint16      bit16;
    uint8       bit8[2];
    char	*cstr;
} input = { 0xdeadbeef, 0xcafe, {0xba, 0xbe}, "foobar"},
  output = {0, 0, {0, 0}, NULL };

void
dump_struct(char *label, struct TestData *str)
{
    fprintf(stderr, "%s: {%#08x, %#04x, {%#02x, %#02x}, %s\n", label,
	    str->bit32, str->bit16, str->bit8[0], str->bit8[1], str->cstr);
}

PRBool
XDR(XPTCursor *cursor, struct TestData *str)
{
    TRY("Do32", XPT_Do32(cursor, &str->bit32));
    TRY("Do16", XPT_Do16(cursor, &str->bit16));
    TRY("Do8",  XPT_Do8 (cursor, &str->bit8[0]));
    TRY("Do8",  XPT_Do8 (cursor, &str->bit8[1]));
    TRY("DoCString", XPT_DoCString(cursor, &str->cstr));
    return 0;
}

int
main(int argc, char **argv)
{
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *header, *data, *whole;
    uint32 hlen, dlen, i;

    TRY("NewState (ENCODE)", (state = XPT_NewXDRState(XPT_ENCODE, NULL, 0)));

    XPT_SetDataOffset(state, sizeof input);

    TRY("MakeCursor", XPT_MakeCursor(state, XPT_HEADER, sizeof input, cursor));

    dump_struct("before", &input);

    if (XDR(cursor, &input))
	return 1;

    fprintf(stderr, "ENCODE successful\n");
    XPT_GetXDRData(state, XPT_HEADER, &header, &hlen);
    fprintf(stderr, "XDR header %d bytes at %p:",
	    hlen, header);
    for (i = 0; i < hlen; i++)
	fprintf(stderr, "%c%02x", i ? ',' : ' ', (uint8)header[i]);
    fprintf(stderr, "\n");

    XPT_GetXDRData(state, XPT_DATA, &data, &dlen);

    fprintf(stderr, "XDR data %d bytes at %p:",
	    dlen, data);
    for (i = 0; i < dlen; i++)
	fprintf(stderr, "%c%02x/%c", i ? ',' : ' ', (uint8)data[i],
		(uint8)data[i]);
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

    if (XDR(cursor, &output))
	return 1;
    
    dump_struct("after", &output);
    XPT_DestroyXDRState(state);
    free(whole);

    return 0;
}
