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
XPT_XDRInterfaceDirectoryEntry(XPTCursor *cursor,
                               XPTInterfaceDirectoryEntry **idep)
{
    
    XPTCursor my_cursor;
    XPTInterfaceDirectoryEntry *ide;
    XPTMode mode = cursor->state->mode;

    /*
     * if we were a ``normal'' function, we'd have to get our offset
     * with something like
     *    offset = XPT_GetOffset(cursor->state, XPT_DATA);
     * and then write/read it into/from our _caller_'s cursor:
     *    XPT_Do32(cursor, &offset).
     * In the decode case below, we would want to check for already-restored
     * data at this offset, and create+register if none was found:
     *    ide = XPT_GetAddrForOffset(cursor->state, offset);
     *    if (!ide) {
     *      -- none registered! --
     *        ide = PR_NEW(XPTInterfaceDirectoryEntry);
     *        if (!ide) return PR_FALSE;
     *        XPT_SetAddrForOffset(cursor->state, offset, (void *)ide);
     *        *idep = ide;
     *    } else {
     *        -- found it! --
     *        *idep = ide;
     *        return PR_TRUE;
     *    }
     * I think I'll write a macro that does the right thing here, because
     * it'll appear at the beginning of just about every such function.
     *
     */

    if (mode == XPT_DECODE) {
        if !((*idep = ide = PR_NEW(XPTInterfaceDirectoryEntry)))
	    return PR_FALSE;
    } else {
        ide = *idep;
    }
    

    /* create a cursor, reserving XPT_IDE_SIZE bytes in the encode case */
    if (!XPT_CreateCursor(cursor->state, XPT_HEADER, XPT_IDE_SIZE,
                          &my_cursor) ||

        /* write the IID in our cursor space */
        !XPT_DoIID(&my_cursor, &ide->iid) ||

        /* write the string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(&my_cursor, &ide->name) ||

        /* write the InterfaceDescriptor in the data pool, and the offset
           in our cursor space */
        !XPT_DoInterfaceDescriptor(&my_cursor, &ide->interface_descriptor)) {
        if (mode == XPT_DECODE) {
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
