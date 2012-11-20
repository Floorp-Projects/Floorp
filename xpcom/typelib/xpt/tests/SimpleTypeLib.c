/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Test the structure creation and serialization APIs from xpt_struct.c */
#include <stdio.h>

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

struct nsID iid = {
    0x00112233,
    0x4455,
    0x6677,
    {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}
};

XPTTypeDescriptor td_void;

int
main(int argc, char **argv)
{
    XPTArena *arena;
    XPTHeader *header;
    XPTAnnotation *ann;
    XPTInterfaceDescriptor *id;
    XPTMethodDescriptor *meth;

    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *data, *head;
    FILE *out;
    uint32_t len, header_sz;

    PRBool ok;

    td_void.prefix.flags = TD_VOID;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <filename.xpt>\n"
		"       Creates a simple typelib file.\n", argv[0]);
	
	return 1;
    }

    arena = XPT_NewArena(1024, sizeof(double), "main");
    TRY("XPT_NewArena", arena);

    /* construct a header */
    header = XPT_NewHeader(arena, 1, XPT_MAJOR_VERSION, XPT_MINOR_VERSION);
    TRY("NewHeader", header);

    
    ann = XPT_NewAnnotation(arena, XPT_ANN_LAST | XPT_ANN_PRIVATE,
                            XPT_NewStringZ(arena, "SimpleTypeLib 1.0"),
                            XPT_NewStringZ(arena, "See You In Rome"));
    TRY("NewAnnotation", ann);
    header->annotations = ann;

    header_sz = XPT_SizeOfHeaderBlock(header);

    id = XPT_NewInterfaceDescriptor(arena, 0, 2, 2, 0);
    TRY("NewInterfaceDescriptor", id);
    
    ok = XPT_FillInterfaceDirectoryEntry(arena, header->interface_directory, &iid,
					 "Interface", "NS", id);
    TRY("FillInterfaceDirectoryEntry", ok);

    /* void method1(void) */
    meth = &id->method_descriptors[0];
    ok = XPT_FillMethodDescriptor(arena, meth, 0, "method1", 0);
    TRY("FillMethodDescriptor", ok);
    meth->result.flags = 0;
    meth->result.type.prefix.flags = TD_VOID;

    /* wstring method2(in uint32_t, in bool) */
    meth = &id->method_descriptors[1];
    ok = XPT_FillMethodDescriptor(arena, meth, 0, "method2", 2);
    TRY("FillMethodDescriptor", ok);

    meth->result.flags = 0;
    meth->result.type.prefix.flags = TD_PSTRING | XPT_TDP_POINTER;
    meth->params[0].type.prefix.flags = TD_UINT32;
    meth->params[0].flags = XPT_PD_IN;
    meth->params[1].type.prefix.flags = TD_BOOL;
    meth->params[1].flags = XPT_PD_IN;

#if 0
    /* const one = 1; */
    id->const_descriptors[0].name = "one";
    id->const_descriptors[0].type.prefix.flags = TD_UINT16;
    id->const_descriptors[0].value.ui16 = 1;
    
    /* const squeamish = "ossifrage"; */
    id->const_descriptors[1].name = "squeamish";
    id->const_descriptors[1].type.prefix.flags = TD_PBSTR | XPT_TDP_POINTER;
    id->const_descriptors[1].value.string = XPT_NewStringZ(arena, "ossifrage");
#endif

    /* serialize it */
    state = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    TRY("NewState (ENCODE)", state);
    
    ok = XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor);
    TRY("MakeCursor", ok);

    ok = XPT_DoHeader(arena, cursor, &header);
    TRY("DoHeader", ok);

    out = fopen(argv[1], "wb");
    if (!out) {
        perror("FAILED: fopen");
        return 1;
    }
    
    XPT_GetXDRData(state, XPT_HEADER, &head, &len);
    fwrite(head, len, 1, out);

    XPT_GetXDRData(state, XPT_DATA, &data, &len);
    fwrite(data, len, 1, out);

    if (ferror(out) != 0 || fclose(out) != 0) {
        fprintf(stderr, "\nError writing file: %s\n\n", argv[1]);
    } else {
        fprintf(stderr, "\nFile written: %s\n\n", argv[1]);
    }
    XPT_DestroyXDRState(state);
    
    XPT_FreeHeader(arena, header);
    XPT_DestroyArena(arena);

    return 0;
}

