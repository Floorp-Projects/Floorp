/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of XDR routines for typelib structures. */

#include "xpt_xdr.h"
#include "xpt_struct.h"
#include <string.h>
#include <stdio.h>

/***************************************************************************/
/* Forward declarations. */

static PRBool
DoInterfaceDirectoryEntry(XPTArena *arena, XPTCursor *cursor,
                          XPTInterfaceDirectoryEntry *ide);

static PRBool
DoConstDescriptor(XPTArena *arena, XPTCursor *cursor, XPTConstDescriptor *cd,
                  XPTInterfaceDescriptor *id);

static PRBool
DoMethodDescriptor(XPTArena *arena, XPTCursor *cursor, XPTMethodDescriptor *md, 
                   XPTInterfaceDescriptor *id);

static PRBool
DoAnnotations(XPTCursor *cursor);

static PRBool
DoInterfaceDescriptor(XPTArena *arena, XPTCursor *outer, XPTInterfaceDescriptor **idp);

static PRBool
DoTypeDescriptorPrefix(XPTArena *arena, XPTCursor *cursor, XPTTypeDescriptorPrefix *tdp);

static PRBool
DoTypeDescriptor(XPTArena *arena, XPTCursor *cursor, XPTTypeDescriptor *td,
                 XPTInterfaceDescriptor *id);

static PRBool
DoParamDescriptor(XPTArena *arena, XPTCursor *cursor, XPTParamDescriptor *pd,
                  XPTInterfaceDescriptor *id);

/***************************************************************************/

XPT_PUBLIC_API(PRBool)
XPT_DoHeaderPrologue(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp, uint32_t * ide_offset)
{
    unsigned int i;
    XPTHeader* header = XPT_NEWZAP(arena, XPTHeader);
    if (!header)
        return PR_FALSE;
    *headerp = header;

    for (i = 0; i < sizeof(header->magic); i++) {
        if (!XPT_Do8(cursor, &header->magic[i]))
            return PR_FALSE;
    }

    if (strncmp((const char*)header->magic, XPT_MAGIC, 16) != 0) {
        /* Require that the header contain the proper magic */
        fprintf(stderr,
                "libxpt: bad magic header in input file; "
                "found '%s', expected '%s'\n",
                header->magic, XPT_MAGIC_STRING);
        return PR_FALSE;
    }

    if (!XPT_Do8(cursor, &header->major_version) ||
        !XPT_Do8(cursor, &header->minor_version)) {
        return PR_FALSE;
    }

    if (header->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION) {
        /* This file is newer than we are and set to an incompatible version
         * number. We must set the header state thusly and return.
         */
        header->num_interfaces = 0;
        header->file_length = 0;
        return PR_TRUE;
    }

    if (!XPT_Do16(cursor, &header->num_interfaces) ||
        !XPT_Do32(cursor, &header->file_length) ||
        (ide_offset != NULL && !XPT_Do32(cursor, ide_offset))) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_DoHeader(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp)
{
    XPTHeader * header;
    uint32_t ide_offset;
    int i;

    if (!XPT_DoHeaderPrologue(arena, cursor, headerp, &ide_offset))
        return PR_FALSE;
    header = *headerp;
    /* 
     * Make sure the file length reported in the header is the same size as
     * as our buffer unless it is zero (not set) 
     */
    if (header->file_length != 0 && 
        cursor->state->pool_allocated < header->file_length) {
        fputs("libxpt: File length in header does not match actual length. File may be corrupt\n",
            stderr);
        return PR_FALSE;
    }

    if (!XPT_Do32(cursor, &header->data_pool))
        return PR_FALSE;

    XPT_SetDataOffset(cursor->state, header->data_pool);

    if (header->num_interfaces) {
        header->interface_directory = 
            (XPTInterfaceDirectoryEntry*)XPT_CALLOC(arena, header->num_interfaces * 
                                                    sizeof(XPTInterfaceDirectoryEntry));
        if (!header->interface_directory)
            return PR_FALSE;
    }

    if (!DoAnnotations(cursor))
        return PR_FALSE;

    /* shouldn't be necessary now, but maybe later */
    XPT_SeekTo(cursor, ide_offset); 

    for (i = 0; i < header->num_interfaces; i++) {
        if (!DoInterfaceDirectoryEntry(arena, cursor, 
                                       &header->interface_directory[i]))
            return PR_FALSE;
    }
    
    return PR_TRUE;
}   

/* InterfaceDirectoryEntry records go in the header */
PRBool
DoInterfaceDirectoryEntry(XPTArena *arena, XPTCursor *cursor,
                          XPTInterfaceDirectoryEntry *ide)
{
    /* write the IID in our cursor space */
    if (!XPT_DoIID(cursor, &(ide->iid)) ||
        
        /* write the name string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(arena, cursor, &(ide->name)) ||
        
        /* write the name_space string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(arena, cursor, &(ide->name_space)) ||
        
        /* do InterfaceDescriptors */
        !DoInterfaceDescriptor(arena, cursor, &ide->interface_descriptor)) {
        return PR_FALSE;
    }

    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddTypes(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                uint16_t num)
{
    XPTTypeDescriptor *old = id->additional_types;
    XPTTypeDescriptor *new_;
    size_t old_size = id->num_additional_types * sizeof(XPTTypeDescriptor);
    size_t new_size = (num * sizeof(XPTTypeDescriptor)) + old_size;

    /* XXX should grow in chunks to minimize alloc overhead */
    new_ = (XPTTypeDescriptor*)XPT_CALLOC(arena, new_size);
    if (!new_)
        return PR_FALSE;
    if (old) {
        memcpy(new_, old, old_size);
    }
    id->additional_types = new_;
    id->num_additional_types += num;
    return PR_TRUE;
}

PRBool
DoInterfaceDescriptor(XPTArena *arena, XPTCursor *outer, 
                      XPTInterfaceDescriptor **idp)
{
    XPTInterfaceDescriptor *id;
    XPTCursor curs, *cursor = &curs;
    uint32_t i, id_sz = 0;

    id = XPT_NEWZAP(arena, XPTInterfaceDescriptor);
    if (!id)
        return PR_FALSE;
    *idp = id;

    if (!XPT_MakeCursor(outer->state, XPT_DATA, id_sz, cursor))
        return PR_FALSE;

    if (!XPT_Do32(outer, &cursor->offset))
        return PR_FALSE;
    if (!cursor->offset) {
        return PR_TRUE;
    }
    if(!XPT_Do16(cursor, &id->parent_interface) ||
       !XPT_Do16(cursor, &id->num_methods)) {
        return PR_FALSE;
    }

    if (id->num_methods) {
        id->method_descriptors = (XPTMethodDescriptor*)XPT_CALLOC(arena, id->num_methods *
                                                                  sizeof(XPTMethodDescriptor));
        if (!id->method_descriptors)
            return PR_FALSE;
    }
    
    for (i = 0; i < id->num_methods; i++) {
        if (!DoMethodDescriptor(arena, cursor, &id->method_descriptors[i], id))
            return PR_FALSE;
    }
    
    if (!XPT_Do16(cursor, &id->num_constants)) {
        return PR_FALSE;
    }
    
    if (id->num_constants) {
        id->const_descriptors = (XPTConstDescriptor*)XPT_CALLOC(arena, id->num_constants * 
                                                                sizeof(XPTConstDescriptor));
        if (!id->const_descriptors)
            return PR_FALSE;
    }
    
    for (i = 0; i < id->num_constants; i++) {
        if (!DoConstDescriptor(arena, cursor, &id->const_descriptors[i], id)) {
            return PR_FALSE;
        }
    }

    if (!XPT_Do8(cursor, &id->flags)) {
        return PR_FALSE;
    }
    
    return PR_TRUE;
}

PRBool
DoConstDescriptor(XPTArena *arena, XPTCursor *cursor, XPTConstDescriptor *cd,
                  XPTInterfaceDescriptor *id)
{
    PRBool ok = PR_FALSE;

    if (!XPT_DoCString(arena, cursor, &cd->name) ||
        !DoTypeDescriptor(arena, cursor, &cd->type, id)) {

        return PR_FALSE;
    }

    switch(XPT_TDP_TAG(cd->type.prefix)) {
      case TD_INT8:
        ok = XPT_Do8(cursor, (uint8_t*) &cd->value.i8);
        break;
      case TD_INT16:
        ok = XPT_Do16(cursor, (uint16_t*) &cd->value.i16);
        break;
      case TD_INT32:
        ok = XPT_Do32(cursor, (uint32_t*) &cd->value.i32);
        break;
      case TD_INT64:
        ok = XPT_Do64(cursor, &cd->value.i64);
        break;
      case TD_UINT8:
        ok = XPT_Do8(cursor, &cd->value.ui8);
        break;
      case TD_UINT16:
        ok = XPT_Do16(cursor, &cd->value.ui16);
        break;
      case TD_UINT32:
        ok = XPT_Do32(cursor, &cd->value.ui32);
        break;
      case TD_UINT64:
        ok = XPT_Do64(cursor, (int64_t *)&cd->value.ui64);
        break;
      case TD_CHAR:
        ok = XPT_Do8(cursor, (uint8_t*) &cd->value.ch);
        break;
      case TD_WCHAR:
        ok = XPT_Do16(cursor, &cd->value.wch);
        break;
        /* fall-through */
      default:
        fprintf(stderr, "illegal type!\n");
        break;
    }

    return ok;

}

PRBool
DoMethodDescriptor(XPTArena *arena, XPTCursor *cursor, XPTMethodDescriptor *md,
                   XPTInterfaceDescriptor *id)
{
    int i;

    if (!XPT_Do8(cursor, &md->flags) ||
        !XPT_DoCString(arena, cursor, &md->name) ||
        !XPT_Do8(cursor, &md->num_args))
        return PR_FALSE;

    if (md->num_args) {
        md->params = (XPTParamDescriptor*)XPT_CALLOC(arena, md->num_args * sizeof(XPTParamDescriptor));
        if (!md->params)
            return PR_FALSE;
    }

    for(i = 0; i < md->num_args; i++) {
        if (!DoParamDescriptor(arena, cursor, &md->params[i], id))
            return PR_FALSE;
    }
    
    if (!DoParamDescriptor(arena, cursor, &md->result, id))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
DoParamDescriptor(XPTArena *arena, XPTCursor *cursor, XPTParamDescriptor *pd, 
                  XPTInterfaceDescriptor *id)
{
    if (!XPT_Do8(cursor, &pd->flags) ||
        !DoTypeDescriptor(arena, cursor, &pd->type, id))
        return PR_FALSE;
        
    return PR_TRUE;
}

PRBool
DoTypeDescriptorPrefix(XPTArena *arena, XPTCursor *cursor, XPTTypeDescriptorPrefix *tdp)
{
    return XPT_Do8(cursor, &tdp->flags);
}

PRBool
DoTypeDescriptor(XPTArena *arena, XPTCursor *cursor, XPTTypeDescriptor *td,
                 XPTInterfaceDescriptor *id)
{
    if (!DoTypeDescriptorPrefix(arena, cursor, &td->prefix)) {
        return PR_FALSE;
    }
    
    switch (XPT_TDP_TAG(td->prefix)) {
      case TD_INTERFACE_TYPE:
        if (!XPT_Do16(cursor, &td->type.iface))
            return PR_FALSE;
        break;
      case TD_INTERFACE_IS_TYPE:
        if (!XPT_Do8(cursor, &td->argnum))
            return PR_FALSE;
        break;
      case TD_ARRAY:
        if (!XPT_Do8(cursor, &td->argnum) ||
            !XPT_Do8(cursor, &td->argnum2))
            return PR_FALSE;

        if (!XPT_InterfaceDescriptorAddTypes(arena, id, 1))
            return PR_FALSE;
        td->type.additional_type = id->num_additional_types - 1;

        if (!DoTypeDescriptor(arena, cursor, 
                              &id->additional_types[td->type.additional_type], 
                              id))
            return PR_FALSE;
        break;
      case TD_PSTRING_SIZE_IS:
      case TD_PWSTRING_SIZE_IS:
        if (!XPT_Do8(cursor, &td->argnum) ||
            !XPT_Do8(cursor, &td->argnum2))
            return PR_FALSE;
        break;

      default:
        /* nothing special */
        break;
    }
    return PR_TRUE;
}

PRBool
DoAnnotations(XPTCursor *cursor)
{
    uint8_t flags;
    if (!XPT_Do8(cursor, &flags))
        return PR_FALSE;

    // All we handle now is a single, empty annotation.
    if (XPT_ANN_IS_PRIVATE(flags) || !XPT_ANN_IS_LAST(flags)) {
        fprintf(stderr, "private annotations are no longer handled\n");
        return PR_FALSE;
    }

    return PR_TRUE;
}

