/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    uint32	bit32;
    uint16      bit16;
    uint8       bit8[2];
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
    uint32 hlen, dlen, i;

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

    if (XDR(arena, cursor, &output))
	return 1;
    
    dump_struct("after", &output);
    XPT_DestroyXDRState(state);
    XPT_DestroyArena(arena);
    free(whole);

    return 0;
}
