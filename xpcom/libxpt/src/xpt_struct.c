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

/* Implementation of XDR routines for typelib structures. */

#include "xpt_xdr.h"
#include "xpt_struct.h"

/* InterfaceDirectoryEntry records go in the header */
PRBool
XPT_XDRInterfaceDirectoryEntry(XPTXDRState *state,
			       XPTInterfaceDirectoryEntry **idep)
{
    
    XPT_Cursor cursor;
    XPTInterfaceDirectoryEntry *ide;

    if (state->mode == XDRXPT_DECODE) {
	if !((*idep = ide = PR_NEW(XPTInterfaceDirectoryEntry)))
	    return PR_FALSE;
    } else {
	ide = *idep;
    }
    
    if (!XPT_CreateCursor(state, XPT_HEADER, XPT_IDE_SIZE, &cursor) ||
	!XPT_DoIID(state, &ide->iid, &cursor) ||
	!XPT_DoIdentifier(state, &ide->name, &cursor) ||
	!XPT_DoInterfaceDescriptor(state, &ide->interface_descriptor,
				   &cursor)) {
	if (state->mode == XPTXDR_DECODE) {
	    PR_FREE(ide);
	    *idep = 0;
	}
	return PR_FALSE;
    }

    return PR_TRUE;
}

XPT_DoInterfaceDescriptor(state, ...)
{
    if (mode == DECODE)
	id = PR_NEWZAP();
    else
	id = *idp;

    XPT_CreateCursor(state, XPT_DATA, 4 + 2 + id->num_methods * 12 + 2 ...);
    XPT_DoInterfaceDescriptorEntry();
    XPT_Do16(&num_methods);
    for (i = 0; i < num_methods; i++) {
	XPT_DoMethodDescriptor(state, &id->method_descriptors[i], &cursor);
    }
    XPT_Do16(&num_constants);
    for (i = 0; i < num_constants; i++) {
	XPT_DoConstantDescriptor(state, &id->constant_descriptors[i], &cursor);
    }

    return PR_TRUE;
}
