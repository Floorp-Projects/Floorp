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

struct nsID iid = {
    0x00112233,
    0x4455,
    0x6677,
    {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}
};

XPTTypeDescriptor td_void = {0};

int
main(int argc, char **argv)
{
    XPTHeader *header;
    XPTAnnotation *ann;
    XPTInterfaceDescriptor *id;
    XPTMethodDescriptor *meth;

    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *data, *head;
    FILE *out;
    uint32 len, header_sz;

    PRBool ok;

    td_void.prefix.flags = TD_VOID;

#ifdef XP_MAC
	if (argc == 0) {
		static char* args[] = { "SimpleTypeLib", "simple.xpt", NULL };
		argc = 2;
		argv = args;
	}
#endif

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <filename.xpt>\n"
		"       Creates a simple typelib file.\n", argv[0]);
	
	return 1;
    }

    /* construct a header */
    header = XPT_NewHeader(1);
    TRY("NewHeader", header);

    
    ann = XPT_NewAnnotation(XPT_ANN_LAST | XPT_ANN_PRIVATE,
                            XPT_NewStringZ("SimpleTypeLib 1.0"),
                            XPT_NewStringZ("See You In Rome"));
    TRY("NewAnnotation", ann);
    header->annotations = ann;

    header_sz = XPT_SizeOfHeaderBlock(header);

    id = XPT_NewInterfaceDescriptor(0xdead, 2, 2, 0);
    TRY("NewInterfaceDescriptor", id);
    
    ok = XPT_FillInterfaceDirectoryEntry(header->interface_directory, &iid,
					 "Interface", "NS", id, NULL);
    TRY("FillInterfaceDirectoryEntry", ok);

    /* void method1(void) */
    meth = &id->method_descriptors[0];
    ok = XPT_FillMethodDescriptor(meth, 0, "method1", 0);
    TRY("FillMethodDescriptor", ok);
    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_VOID;

    /* wstring method2(in uint32, in bool) */
    meth = &id->method_descriptors[1];
    ok = XPT_FillMethodDescriptor(meth, 0, "method2", 2);
    TRY("FillMethodDescriptor", ok);

    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_PBSTR | XPT_TDP_POINTER;
    meth->params[0].type.prefix.flags = TD_UINT32;
    meth->params[0].flags = XPT_PD_IN;
    meth->params[1].type.prefix.flags = TD_BOOL;
    meth->params[1].flags = XPT_PD_IN;

    /* const one = 1; */
    id->const_descriptors[0].name = "one";
    id->const_descriptors[0].type.prefix.flags = TD_UINT16;
    id->const_descriptors[0].value.ui16 = 1;
    
    /* const squeamish = "ossifrage"; */
    id->const_descriptors[1].name = "squeamish";
    id->const_descriptors[1].type.prefix.flags = TD_PBSTR | XPT_TDP_POINTER;
    id->const_descriptors[1].value.string = XPT_NewStringZ("ossifrage");

    /* serialize it */
    state = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    TRY("NewState (ENCODE)", state);
    
    ok = XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor);
    TRY("MakeCursor", ok);

    ok = XPT_DoHeader(cursor, &header);
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
    
    /* XXX DestroyHeader */
    return 0;
}

