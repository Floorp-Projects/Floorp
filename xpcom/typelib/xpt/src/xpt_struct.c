/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implementation of XDR routines for typelib structures. */

#include "xpt_xdr.h"
#include "xpt_struct.h"
#include <string.h>
#include <stdio.h>

/***************************************************************************/
/* Forward declarations. */

static PRUint32
SizeOfTypeDescriptor(XPTTypeDescriptor *td, XPTInterfaceDescriptor *id);

static PRUint32
SizeOfMethodDescriptor(XPTMethodDescriptor *md, XPTInterfaceDescriptor *id);

static PRUint32
SizeOfConstDescriptor(XPTConstDescriptor *cd, XPTInterfaceDescriptor *id);

static PRUint32
SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *id);

static PRBool
DoInterfaceDirectoryEntry(XPTArena *arena, XPTCursor *cursor,
                          XPTInterfaceDirectoryEntry *ide, PRUint16 entry_index);

static PRBool
DoConstDescriptor(XPTArena *arena, XPTCursor *cursor, XPTConstDescriptor *cd,
                  XPTInterfaceDescriptor *id);

static PRBool
DoMethodDescriptor(XPTArena *arena, XPTCursor *cursor, XPTMethodDescriptor *md, 
                   XPTInterfaceDescriptor *id);

static PRBool
DoAnnotation(XPTArena *arena, XPTCursor *cursor, XPTAnnotation **annp);

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

XPT_PUBLIC_API(PRUint32)
XPT_SizeOfHeader(XPTHeader *header)
{
    XPTAnnotation *ann, *last;
    PRUint32 size = 16 /* magic */ +
        1 /* major */ + 1 /* minor */ +
        2 /* num_interfaces */ + 4 /* file_length */ +
        4 /* interface_directory */ + 4 /* data_pool */;

    ann = header->annotations;
    do {
        size += 1; /* Annotation prefix */
        if (XPT_ANN_IS_PRIVATE(ann->flags))
            size += 2 + ann->creator->length + 2 + ann->private_data->length;
        last = ann;
        ann = ann->next;
    } while (!XPT_ANN_IS_LAST(last->flags));
        
    return size;
}

XPT_PUBLIC_API(PRUint32)
XPT_SizeOfHeaderBlock(XPTHeader *header)
{
    PRUint32 size = XPT_SizeOfHeader(header);

    size += header->num_interfaces * sizeof (XPTInterfaceDirectoryEntry);

    return size;
}

XPT_PUBLIC_API(XPTHeader *)
XPT_NewHeader(XPTArena *arena, PRUint16 num_interfaces, PRUint8 major_version, PRUint8 minor_version)
{
    XPTHeader *header = XPT_NEWZAP(arena, XPTHeader);
    if (!header)
        return NULL;
    memcpy(header->magic, XPT_MAGIC, 16);
    header->major_version = major_version;
    header->minor_version = minor_version;
    header->num_interfaces = num_interfaces;
    if (num_interfaces) {
        header->interface_directory = 
            XPT_CALLOC(arena, 
                       num_interfaces * sizeof(XPTInterfaceDirectoryEntry));
        if (!header->interface_directory) {
            XPT_DELETE(arena, header);
            return NULL;
        }
    }
    header->data_pool = 0;      /* XXX do we even need this struct any more? */
    
    return header;
}

XPT_PUBLIC_API(void)
XPT_FreeHeader(XPTArena *arena, XPTHeader* aHeader)
{
    if (aHeader) {
        XPTAnnotation* ann;
        XPTInterfaceDirectoryEntry* entry = aHeader->interface_directory;
        XPTInterfaceDirectoryEntry* end = entry + aHeader->num_interfaces;
        for (; entry < end; entry++) {
            XPT_DestroyInterfaceDirectoryEntry(arena, entry);
        }

        ann = aHeader->annotations;
        while (ann) {
            XPTAnnotation* next = ann->next;
            if (XPT_ANN_IS_PRIVATE(ann->flags)) {
                XPT_FREEIF(arena, ann->creator);
                XPT_FREEIF(arena, ann->private_data);
            }
            XPT_DELETE(arena, ann);
            ann = next;
        }

        XPT_FREEIF(arena, aHeader->interface_directory);
        XPT_DELETE(arena, aHeader);
    }
}

XPT_PUBLIC_API(PRBool)
XPT_DoHeaderPrologue(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp, PRUint32 * ide_offset)
{
    XPTMode mode = cursor->state->mode;
    unsigned int i;
    XPTHeader * header;

    if (mode == XPT_DECODE) {
        header = XPT_NEWZAP(arena, XPTHeader);
        if (!header)
            return PR_FALSE;
        *headerp = header;
    } else {
        header = *headerp;
    }

    if (mode == XPT_ENCODE) {
        /* IDEs appear after header, including annotations */
        if (ide_offset != NULL)
        {
            *ide_offset = XPT_SizeOfHeader(*headerp) + 1; /* one-based offset */
        }
        header->data_pool = XPT_SizeOfHeaderBlock(*headerp);
        XPT_SetDataOffset(cursor->state, header->data_pool);
    }

    for (i = 0; i < sizeof(header->magic); i++) {
        if (!XPT_Do8(cursor, &header->magic[i]))
            goto error;
    }

    if (mode == XPT_DECODE && 
        strncmp((const char*)header->magic, XPT_MAGIC, 16) != 0)
    {
        /* Require that the header contain the proper magic */
        fprintf(stderr,
                "libxpt: bad magic header in input file; "
                "found '%s', expected '%s'\n",
                header->magic, XPT_MAGIC_STRING);
        goto error;
    }
    
    if (!XPT_Do8(cursor, &header->major_version) ||
        !XPT_Do8(cursor, &header->minor_version)) {
        goto error;
    }

    if (mode == XPT_DECODE &&
        header->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION) {
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
        goto error;
    }
    return PR_TRUE;
    /* XXX need to free child data sometimes! */
    XPT_ERROR_HANDLE(arena, header);    
}

XPT_PUBLIC_API(PRBool)
XPT_DoHeader(XPTArena *arena, XPTCursor *cursor, XPTHeader **headerp)
{
    const int HEADER_SIZE = 24;
    XPTMode mode = cursor->state->mode;
    XPTHeader * header;
    PRUint32 ide_offset;
    int i;
    XPTAnnotation *ann, *next, **annp;

    if (!XPT_DoHeaderPrologue(arena, cursor, headerp, &ide_offset))
        return PR_FALSE;
    header = *headerp;
    /* 
     * Make sure the file length reported in the header is the same size as
     * as our buffer unless it is zero (not set) 
     */
    if (mode == XPT_DECODE && (header->file_length != 0 && 
        cursor->state->pool->allocated < header->file_length)) {
        fputs("libxpt: File length in header does not match actual length. File may be corrupt\n",
            stderr);
        goto error;
    }

    if (mode == XPT_ENCODE)
        XPT_DataOffset(cursor->state, &header->data_pool);
    if (!XPT_Do32(cursor, &header->data_pool))
        goto error;
    if (mode == XPT_DECODE)
        XPT_DataOffset(cursor->state, &header->data_pool);

    if (mode == XPT_DECODE && header->num_interfaces) {
        header->interface_directory = 
            XPT_CALLOC(arena, header->num_interfaces * 
                       sizeof(XPTInterfaceDirectoryEntry));
        if (!header->interface_directory)
            goto error;
    }

    /*
     * Iterate through the annotations rather than recurring, to avoid blowing
     * the stack on large xpt files.
     */
    ann = next = header->annotations;
    annp = &header->annotations;
    do {
        ann = next;
        if (!DoAnnotation(arena, cursor, &ann))
            goto error;
        if (mode == XPT_DECODE) {
            /*
             * Make sure that we store the address of the newly allocated
             * annotation in the previous annotation's ``next'' slot, or
             * header->annotations for the first one.
             */
            *annp = ann;
            annp = &ann->next;
        }
        next = ann->next;
    } while (!XPT_ANN_IS_LAST(ann->flags));

    /* shouldn't be necessary now, but maybe later */
    XPT_SeekTo(cursor, ide_offset); 

    for (i = 0; i < header->num_interfaces; i++) {
        if (!DoInterfaceDirectoryEntry(arena, cursor, 
                                       &header->interface_directory[i],
                                       (PRUint16)(i + 1)))
            goto error;
    }
    
    return PR_TRUE;

    /* XXX need to free child data sometimes! */
    XPT_ERROR_HANDLE(arena, header);    
}   

XPT_PUBLIC_API(PRBool)
XPT_FillInterfaceDirectoryEntry(XPTArena *arena,
                                XPTInterfaceDirectoryEntry *ide,
                                nsID *iid, char *name, char *name_space,
                                XPTInterfaceDescriptor *descriptor)
{
    XPT_COPY_IID(ide->iid, *iid);
    ide->name = name ? XPT_STRDUP(arena, name) : NULL; /* what good is it w/o a name? */
    ide->name_space = name_space ? XPT_STRDUP(arena, name_space) : NULL;
    ide->interface_descriptor = descriptor;
    return PR_TRUE;
}

XPT_PUBLIC_API(void)
XPT_DestroyInterfaceDirectoryEntry(XPTArena *arena,
                                   XPTInterfaceDirectoryEntry* ide)
{
    if (ide) {
        if (ide->name) XPT_FREE(arena, ide->name);
        if (ide->name_space) XPT_FREE(arena, ide->name_space);
        XPT_FreeInterfaceDescriptor(arena, ide->interface_descriptor);
    }
}

/* InterfaceDirectoryEntry records go in the header */
PRBool
DoInterfaceDirectoryEntry(XPTArena *arena, XPTCursor *cursor,
                          XPTInterfaceDirectoryEntry *ide, PRUint16 entry_index)
{
    XPTMode mode = cursor->state->mode;
    
    /* write the IID in our cursor space */
    if (!XPT_DoIID(cursor, &(ide->iid)) ||
        
        /* write the name string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(arena, cursor, &(ide->name)) ||
        
        /* write the name_space string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(arena, cursor, &(ide->name_space)) ||
        
        /* do InterfaceDescriptors -- later, only on encode (see below) */
        !DoInterfaceDescriptor(arena, cursor, &ide->interface_descriptor)) {
        goto error;
    }
    
    if (mode == XPT_DECODE)
        XPT_SetOffsetForAddr(cursor, ide, entry_index);

    return PR_TRUE;

    XPT_ERROR_HANDLE(arena, ide);    
}

XPT_PUBLIC_API(XPTInterfaceDescriptor *)
XPT_NewInterfaceDescriptor(XPTArena *arena, 
                           PRUint16 parent_interface, PRUint16 num_methods,
                           PRUint16 num_constants, PRUint8 flags)
{

    XPTInterfaceDescriptor *id = XPT_NEWZAP(arena, XPTInterfaceDescriptor);
    if (!id)
        return NULL;

    if (num_methods) {
        id->method_descriptors = XPT_CALLOC(arena, num_methods *
                                            sizeof(XPTMethodDescriptor));
        if (!id->method_descriptors)
            goto free_id;
        id->num_methods = num_methods;
    }

    if (num_constants) {
        id->const_descriptors = XPT_CALLOC(arena, num_constants *
                                           sizeof(XPTConstDescriptor));
        if (!id->const_descriptors)
            goto free_meth;
        id->num_constants = num_constants;
    }

    if (parent_interface) {
        id->parent_interface = parent_interface;
    } else {
        id->parent_interface = 0;
    }

    id->flags = flags;

    return id;

 free_meth:
    XPT_FREEIF(arena, id->method_descriptors);
 free_id:
    XPT_DELETE(arena, id);
    return NULL;
}

XPT_PUBLIC_API(void)
XPT_FreeInterfaceDescriptor(XPTArena *arena, XPTInterfaceDescriptor* id)
{
    if (id) {
        XPTMethodDescriptor *md, *mdend;
        XPTConstDescriptor *cd, *cdend;

        /* Free up method descriptors */
        md = id->method_descriptors;
        mdend = md + id->num_methods;
        for (; md < mdend; md++) {
            XPT_FREEIF(arena, md->name);
            XPT_FREEIF(arena, md->params);
            XPT_FREEIF(arena, md->result);
        }
        XPT_FREEIF(arena, id->method_descriptors);

        /* Free up const descriptors */
        cd = id->const_descriptors;
        cdend = cd + id->num_constants;
        for (; cd < cdend; cd++) {
            XPT_FREEIF(arena, cd->name);
        }
        XPT_FREEIF(arena, id->const_descriptors);

        /* Free up type descriptors */
        XPT_FREEIF(arena, id->additional_types);

        XPT_DELETE(arena, id);
    }
}

XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddTypes(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                PRUint16 num)
{
    XPTTypeDescriptor *old = id->additional_types;
    XPTTypeDescriptor *new;
    size_t old_size = id->num_additional_types * sizeof(XPTTypeDescriptor);
    size_t new_size = (num * sizeof(XPTTypeDescriptor)) + old_size;

    /* XXX should grow in chunks to minimize alloc overhead */
    new = XPT_CALLOC(arena, new_size);
    if (!new)
        return PR_FALSE;
    if (old) {
        if (old_size)
            memcpy(new, old, old_size);
        XPT_FREE(arena, old);
    }
    id->additional_types = new;
    id->num_additional_types += num;
    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddMethods(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                  PRUint16 num)
{
    XPTMethodDescriptor *old = id->method_descriptors;
    XPTMethodDescriptor *new;
    size_t old_size = id->num_methods * sizeof(XPTMethodDescriptor);
    size_t new_size = (num * sizeof(XPTMethodDescriptor)) + old_size;

    /* XXX should grow in chunks to minimize alloc overhead */
    new = XPT_CALLOC(arena, new_size);
    if (!new)
        return PR_FALSE;
    if (old) {
        if (old_size)
            memcpy(new, old, old_size);
        XPT_FREE(arena, old);
    }
    id->method_descriptors = new;
    id->num_methods += num;
    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddConsts(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                 PRUint16 num)
{
    XPTConstDescriptor *old = id->const_descriptors;
    XPTConstDescriptor *new;
    size_t old_size = id->num_constants * sizeof(XPTConstDescriptor);
    size_t new_size = (num * sizeof(XPTConstDescriptor)) + old_size;

    /* XXX should grow in chunks to minimize alloc overhead */
    new = XPT_CALLOC(arena, new_size);
    if (!new)
        return PR_FALSE;
    if (old) {
        if (old_size)
            memcpy(new, old, old_size);
        XPT_FREE(arena, old);
    }
    id->const_descriptors = new;
    id->num_constants += num;
    return PR_TRUE;
}

PRUint32
SizeOfTypeDescriptor(XPTTypeDescriptor *td, XPTInterfaceDescriptor *id)
{
    PRUint32 size = 1; /* prefix */

    switch (XPT_TDP_TAG(td->prefix)) {
      case TD_INTERFACE_TYPE:
        size += 2; /* interface_index */
        break;
      case TD_INTERFACE_IS_TYPE:
        size += 1; /* argnum */
        break;
      case TD_ARRAY:
        size += 2 + SizeOfTypeDescriptor(
                        &id->additional_types[td->type.additional_type], id);
        break;
      case TD_PSTRING_SIZE_IS:
        size += 2; /* argnum + argnum2 */
        break;
      case TD_PWSTRING_SIZE_IS:
        size += 2; /* argnum + argnum2 */
        break;
      default:
        /* nothing special */
        break;
    }
    return size;
}

PRUint32
SizeOfMethodDescriptor(XPTMethodDescriptor *md, XPTInterfaceDescriptor *id)
{
    PRUint32 i, size =  1 /* flags */ + 4 /* name */ + 1 /* num_args */;

    for (i = 0; i < md->num_args; i++) 
        size += 1 + SizeOfTypeDescriptor(&md->params[i].type, id);

    size += 1 + SizeOfTypeDescriptor(&md->result->type, id);
    return size;
}

PRUint32
SizeOfConstDescriptor(XPTConstDescriptor *cd, XPTInterfaceDescriptor *id)
{
    PRUint32 size = 4 /* name */ + SizeOfTypeDescriptor(&cd->type, id);

    switch (XPT_TDP_TAG(cd->type.prefix)) {
      case TD_INT8:
      case TD_UINT8:
      case TD_CHAR:
        size ++;
        break;
      case TD_INT16:
      case TD_UINT16:
      case TD_WCHAR:
        size += 2;
        break;
      case TD_INT32:
      case TD_UINT32:
      case TD_PSTRING:
        size += 4;
        break;
      case TD_INT64:
      case TD_UINT64:
        size += 8;
        break;
      default:
        fprintf(stderr, "libxpt: illegal type in ConstDescriptor: 0x%02x\n",
                XPT_TDP_TAG(cd->type.prefix));
        return 0;
    }

    return size;
}

PRUint32
SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *id)
{
    PRUint32 size = 2 /* parent interface */ + 2 /* num_methods */
        + 2 /* num_constants */ + 1 /* flags */, i;
    for (i = 0; i < id->num_methods; i++)
        size += SizeOfMethodDescriptor(&id->method_descriptors[i], id);
    for (i = 0; i < id->num_constants; i++)
        size += SizeOfConstDescriptor(&id->const_descriptors[i], id);
    return size;
}

PRBool
DoInterfaceDescriptor(XPTArena *arena, XPTCursor *outer, 
                      XPTInterfaceDescriptor **idp)
{
    XPTMode mode = outer->state->mode;
    XPTInterfaceDescriptor *id;
    XPTCursor curs, *cursor = &curs;
    PRUint32 i, id_sz = 0;

    if (mode == XPT_DECODE) {
        id = XPT_NEWZAP(arena, XPTInterfaceDescriptor);
        if (!id)
            return PR_FALSE;
        *idp = id;
    } else {
        id = *idp;
        if (!id) {
            id_sz = 0;
            return XPT_Do32(outer, &id_sz);
        }
        id_sz = SizeOfInterfaceDescriptor(id);
    }

    if (!XPT_MakeCursor(outer->state, XPT_DATA, id_sz, cursor))
        goto error;

    if (!XPT_Do32(outer, &cursor->offset))
        goto error;
    if (mode == XPT_DECODE && !cursor->offset) {
        XPT_DELETE(arena, *idp);
        return PR_TRUE;
    }
    if(!XPT_Do16(cursor, &id->parent_interface) ||
       !XPT_Do16(cursor, &id->num_methods)) {
        goto error;
    }

    if (mode == XPT_DECODE && id->num_methods) {
        id->method_descriptors = XPT_CALLOC(arena, id->num_methods *
                                            sizeof(XPTMethodDescriptor));
        if (!id->method_descriptors)
            goto error;
    }
    
    for (i = 0; i < id->num_methods; i++) {
        if (!DoMethodDescriptor(arena, cursor, &id->method_descriptors[i], id))
            goto error;   
    }
    
    if (!XPT_Do16(cursor, &id->num_constants)) {
        goto error;
    }
    
    if (mode == XPT_DECODE && id->num_constants) {
        id->const_descriptors = XPT_CALLOC(arena, id->num_constants * 
                                           sizeof(XPTConstDescriptor));
        if (!id->const_descriptors)
            goto error;
    }
    
    for (i = 0; i < id->num_constants; i++) {
        if (!DoConstDescriptor(arena, cursor, &id->const_descriptors[i], id)) {
            goto error;
        }
    }

    if (!XPT_Do8(cursor, &id->flags)) {
        goto error;
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(arena, id);    
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
        ok = XPT_Do8(cursor, (PRUint8*) &cd->value.i8);
        break;
      case TD_INT16:
        ok = XPT_Do16(cursor, (PRUint16*) &cd->value.i16);
        break;
      case TD_INT32:
        ok = XPT_Do32(cursor, (PRUint32*) &cd->value.i32);
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
        ok = XPT_Do64(cursor, (PRInt64 *)&cd->value.ui64);
        break;
      case TD_CHAR:
        ok = XPT_Do8(cursor, (PRUint8*) &cd->value.ch);
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

XPT_PUBLIC_API(PRBool)
XPT_FillMethodDescriptor(XPTArena *arena, XPTMethodDescriptor *meth, 
                         PRUint8 flags, char *name, PRUint8 num_args)
{
    meth->flags = flags & XPT_MD_FLAGMASK;
    meth->name = XPT_STRDUP(arena, name);
    if (!meth->name)
        return PR_FALSE;
    meth->num_args = num_args;
    if (num_args) {
        meth->params = XPT_CALLOC(arena, num_args * sizeof(XPTParamDescriptor));
        if (!meth->params)
            goto free_name;
    } else {
        meth->params = NULL;
    }
    meth->result = XPT_NEWZAP(arena, XPTParamDescriptor);
    if (!meth->result)
        goto free_params;
    return PR_TRUE;

 free_params:
    XPT_DELETE(arena, meth->params);
 free_name:
    XPT_DELETE(arena, meth->name);
    return PR_FALSE;
}

PRBool
DoMethodDescriptor(XPTArena *arena, XPTCursor *cursor, XPTMethodDescriptor *md,
                   XPTInterfaceDescriptor *id)
{
    XPTMode mode = cursor->state->mode;
    int i;

    if (!XPT_Do8(cursor, &md->flags) ||
        !XPT_DoCString(arena, cursor, &md->name) ||
        !XPT_Do8(cursor, &md->num_args))
        return PR_FALSE;

    if (mode == XPT_DECODE && md->num_args) {
        md->params = XPT_CALLOC(arena, md->num_args * sizeof(XPTParamDescriptor));
        if (!md->params)
            return PR_FALSE;
    }

    for(i = 0; i < md->num_args; i++) {
        if (!DoParamDescriptor(arena, cursor, &md->params[i], id))
            goto error;
    }
    
    if (mode == XPT_DECODE) {
        md->result = XPT_NEWZAP(arena, XPTParamDescriptor);
        if (!md->result)
            return PR_FALSE;
    }

    if (!md->result ||
        !DoParamDescriptor(arena, cursor, md->result, id))
        goto error;
    
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(arena, md->params);    
}

XPT_PUBLIC_API(PRBool)
XPT_FillParamDescriptor(XPTArena *arena, XPTParamDescriptor *pd, PRUint8 flags,
                        XPTTypeDescriptor *type)
{
    pd->flags = flags & XPT_PD_FLAGMASK;
    XPT_COPY_TYPE(pd->type, *type);
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
        goto error;
    }
    
    switch (XPT_TDP_TAG(td->prefix)) {
      case TD_INTERFACE_TYPE:
        if (!XPT_Do16(cursor, &td->type.iface))
            goto error;
        break;
      case TD_INTERFACE_IS_TYPE:
        if (!XPT_Do8(cursor, &td->argnum))
            goto error;
        break;
      case TD_ARRAY:
        if (!XPT_Do8(cursor, &td->argnum) ||
            !XPT_Do8(cursor, &td->argnum2))
            goto error;

        if (cursor->state->mode == XPT_DECODE) {
            if(!XPT_InterfaceDescriptorAddTypes(arena, id, 1))
                goto error;
            td->type.additional_type = id->num_additional_types - 1;
        }

        if (!DoTypeDescriptor(arena, cursor, 
                              &id->additional_types[td->type.additional_type], 
                              id))
            goto error;
        break;
      case TD_PSTRING_SIZE_IS:
      case TD_PWSTRING_SIZE_IS:
        if (!XPT_Do8(cursor, &td->argnum) ||
            !XPT_Do8(cursor, &td->argnum2))
            goto error;
        break;

      default:
        /* nothing special */
        break;
    }
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(arena, td);    
}

XPT_PUBLIC_API(XPTAnnotation *)
XPT_NewAnnotation(XPTArena *arena, PRUint8 flags, XPTString *creator, 
                  XPTString *private_data)
{
    XPTAnnotation *ann = XPT_NEWZAP(arena, XPTAnnotation);
    if (!ann)
        return NULL;
    ann->flags = flags;
    if (XPT_ANN_IS_PRIVATE(flags)) {
        ann->creator = creator;
        ann->private_data = private_data;
    }
    return ann;
}

PRBool
DoAnnotation(XPTArena *arena, XPTCursor *cursor, XPTAnnotation **annp)
{
    XPTMode mode = cursor->state->mode;
    XPTAnnotation *ann;
    
    if (mode == XPT_DECODE) {
        ann = XPT_NEWZAP(arena, XPTAnnotation);
        if (!ann)
            return PR_FALSE;
        *annp = ann;
    } else {
        ann = *annp;
    }
    
    if (!XPT_Do8(cursor, &ann->flags))
        goto error;

    if (XPT_ANN_IS_PRIVATE(ann->flags)) {
        if (!XPT_DoStringInline(arena, cursor, &ann->creator) ||
            !XPT_DoStringInline(arena, cursor, &ann->private_data))
            goto error_2;
    }

    return PR_TRUE;
    
 error_2:
    if (ann && XPT_ANN_IS_PRIVATE(ann->flags)) {
        XPT_FREEIF(arena, ann->creator);
        XPT_FREEIF(arena, ann->private_data);
    }
    XPT_ERROR_HANDLE(arena, ann);
}

PRBool
XPT_GetInterfaceIndexByName(XPTInterfaceDirectoryEntry *ide_block,
                            PRUint16 num_interfaces, char *name, 
                            PRUint16 *indexp) 
{
    int i;
    
    for (i=1; i<=num_interfaces; i++) {
        fprintf(stderr, "%s == %s ?\n", ide_block[i].name, name);
        if (strcmp(ide_block[i].name, name) == 0) {
            *indexp = i;
            return PR_TRUE;
        }
    }
    indexp = 0;
    return PR_FALSE;
}

static XPT_TYPELIB_VERSIONS_STRUCT versions[] = XPT_TYPELIB_VERSIONS;
#define XPT_TYPELIB_VERSIONS_COUNT (sizeof(versions) / sizeof(versions[0]))

XPT_PUBLIC_API(PRUint16)
XPT_ParseVersionString(const char* str, PRUint8* major, PRUint8* minor)
{
    int i;
    for (i = 0; i < XPT_TYPELIB_VERSIONS_COUNT; i++) {
        if (!strcmp(versions[i].str, str)) {
            *major = versions[i].major;
            *minor = versions[i].minor;
            return versions[i].code;
        }
    }
    return XPT_VERSION_UNKNOWN;
}


