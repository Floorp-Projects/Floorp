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

/* Test the structure creation and serialization APIs from xpt_struct.c */

#include "xpt_xdr.h"
#include "xpt_struct.h"

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

nsID iid = {
    0x00112233,
    0x4455,
    0x6677,
    {0x88, 0x99, 0xaa, 0xbb} };

int
main(int argc, char **argv)
{
    XPTHeader *header;
    XPTInterfaceDescriptor *id;

    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *data;
    FILE *out;
    uint32 len, header_sz;

    PRBool ok;
    
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <filename.xpt>\n"
		"       Creates a simple typelib file.\n", argv[0]);
	
	return 1;
    }

    /* construct a header */
    header = XPT_NewHeader(1);
    TRY("NewHeader", header);

    header->annotations = XPT_NewAnnotation(PR_TRUE, PR_TRUE, NULL, NULL);
    TRY("NewAnnotation", header->annotations);

    header_sz = XPT_SizeOfHeaderBlock(header);

    id = XPT_NewInterfaceDescriptor(0, 0, 0);
    TRY("NewInterfaceDescriptor", id);
    
    ok = XPT_FillInterfaceDirectoryEntry(header->interface_directory, &iid,
					 "nsIFoo", "nsIBar", id);
    TRY("FillInterfaceDirectoryEntry", ok);

    /* serialize it */
    state = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    TRY("NewState (ENCODE)", state);
    
    XPT_SetDataOffset(state, header_sz);
    
    ok = XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor);
    TRY("MakeCursor", ok);

    ok = XPT_DoHeader(cursor, &header);
    TRY("DoHeader", ok);

    out = fopen(argv[1], "w");
    TRY_Q("fopen", out);
    
    XPT_GetXDRData(state, XPT_HEADER, &data, &len);
    fwrite(data, len, 1, out);

    XPT_GetXDRData(state, XPT_DATA, &data, &len);
    fwrite(data, len, 1, out);
    fclose(out);
    XPT_DestroyXDRState(state);
    /* XXX DestroyHeader */
    return 0;
}
