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

    arena = XPT_NewArena(1024, sizeof(double), "main");
    TRY("XPT_NewArena", arena);

    /* construct a header */
    header = XPT_NewHeader(arena, 1);
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
    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_VOID;

    /* wstring method2(in uint32, in bool) */
    meth = &id->method_descriptors[1];
    ok = XPT_FillMethodDescriptor(arena, meth, 0, "method2", 2);
    TRY("FillMethodDescriptor", ok);

    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_PSTRING | XPT_TDP_POINTER;
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

