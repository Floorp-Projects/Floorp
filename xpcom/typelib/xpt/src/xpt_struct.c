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

/* Implementation of XDR routines for typelib structures. */

#include "xpt_xdr.h"
#include "xpt_struct.h"
#include <string.h>

#define CURS_POOL_OFFSET_RAW(cursor)                                          \
  ((cursor)->pool == XPT_HEADER                                               \
   ? (cursor)->offset                                                         \
   : (PR_ASSERT((cursor)->state->data_offset),                                \
      (cursor)->offset + (cursor)->state->data_offset))

#define CURS_POOL_OFFSET(cursor)                                              \
  (CURS_POOL_OFFSET_RAW(cursor) - 1)

uint32
XPT_SizeOfHeader(XPTHeader *header)
{
    XPTAnnotation *ann;
    uint32 size = 16 /* magic */ +
        1 /* major */ + 1 /* minor */ +
        2 /* num_interfaces */ + 4 /* file_length */ +
        4 /* interface_directory */ + 4 /* data_pool */;

    fprintf(stderr, "header size is %d ", size);
    ann = header->annotations;
    do {
        size += 1; /* Annotation prefix */
        if (XPT_ANN_IS_PRIVATE(ann->flags))
            size += 2 + ann->creator->length + 2 + ann->private_data->length;
    } while (!XPT_ANN_IS_LAST(ann->flags));
    
    fprintf(stderr, " (%d with annotations)\n", size);
    return size;
}

uint32
XPT_SizeOfHeaderBlock(XPTHeader *header)
{
    uint32 size = XPT_SizeOfHeader(header);

    size += header->num_interfaces * sizeof (XPTInterfaceDirectoryEntry);

    return size;
}

XPTHeader *
XPT_NewHeader(uint32 num_interfaces)
{
    XPTHeader *header = PR_NEWZAP(XPTHeader);
    if (!header)
        return NULL;
    memcpy(header->magic, XPT_MAGIC, 16);
    header->major_version = XPT_MAJOR_VERSION;
    header->minor_version = XPT_MINOR_VERSION;
    header->num_interfaces = num_interfaces;
    header->interface_directory = PR_CALLOC(num_interfaces *
                                            sizeof(XPTInterfaceDirectoryEntry));
    if (!header->interface_directory) {
        PR_DELETE(header);
        return NULL;
    }
    header->data_pool = 0;      /* XXX do we even need this struct any more? */
    
    return header;
}

PRBool
XPT_DoHeader(XPTCursor *cursor, XPTHeader **headerp)
{
    XPTMode mode = cursor->state->mode;
    XPTHeader *header;
    uint32 ide_offset;
    int i;

    if (mode == XPT_DECODE) {
        header = PR_NEWZAP(XPTHeader);
        if (!header)
            return PR_FALSE;
    } else {
        header = *headerp;
    }

    if (mode == XPT_ENCODE) {
        /* IDEs appear after header, including annotations */
        ide_offset = XPT_SizeOfHeader(*headerp) + 1; /* one-based offset */
    }

    for (i = 0; i < 16; i++) {
        if (!XPT_Do8(cursor, &header->magic[i]))
            goto error;
    }
    
    if(!XPT_Do8(cursor, &header->major_version) ||
       !XPT_Do8(cursor, &header->minor_version) ||
       /* XXX check major for compat! */
       !XPT_Do16(cursor, &header->num_interfaces) ||
       !XPT_Do32(cursor, &header->file_length) ||
       !XPT_Do32(cursor, &ide_offset)) {
        goto error;
    }
    
    if (mode == XPT_ENCODE)
        XPT_DataOffset(cursor->state, &header->data_pool);
    if (!XPT_Do32(cursor, &header->data_pool))
        goto error;
    if (mode == XPT_DECODE)
        XPT_DataOffset(cursor->state, &header->data_pool);

    if (mode == XPT_DECODE) {
        header->interface_directory = 
            PR_CALLOC(header->num_interfaces * 
                      sizeof(XPTInterfaceDirectoryEntry));
        if (!header->interface_directory)
            goto error;
    }

    if (!XPT_DoAnnotation(cursor, &header->annotations))
        goto error;

    /* shouldn't be necessary now, but maybe later */
    XPT_SeekTo(cursor, ide_offset); 

    for (i = 0; i < header->num_interfaces; i++) {
        if (!XPT_DoInterfaceDirectoryEntry(cursor, 
                                           &header->interface_directory[i]))
            goto error;
    }
    
    return PR_TRUE;

    /* XXX need to free child data sometimes! */
    XPT_ERROR_HANDLE(header);    
}   

PRBool
XPT_FillInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide,
                                nsID *iid, char *name, char *namespace,
                                XPTInterfaceDescriptor *descriptor)
{
    XPT_COPY_IID(ide->iid, *iid);
    ide->name = strdup(name);
    ide->namespace = strdup(namespace);
    ide->interface_descriptor = descriptor;
    return PR_TRUE;
}

/* InterfaceDirectoryEntry records go in the header */
PRBool
XPT_DoInterfaceDirectoryEntry(XPTCursor *cursor,
                              XPTInterfaceDirectoryEntry *ide)
{    
    XPTMode mode = cursor->state->mode;
    
    /* write the IID in our cursor space */
    if (!XPT_DoIID(cursor, &(ide->iid)) ||
        
        /* write the name string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(cursor, &(ide->name)) ||
        
        /* write the namespace string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(cursor, &(ide->namespace))) {
        
        goto error;
    }
    
    /* write the InterfaceDescriptor in the data pool, and the offset
       in our cursor space, but only if we're encoding. */
    if (mode == XPT_ENCODE) {
        if (!XPT_DoInterfaceDescriptor(cursor, 
                                       &ide->interface_descriptor)) {
            goto error;
        }
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(ide);    
}

PRBool
XPT_IndexForInterface(XPTInterfaceDirectoryEntry *ide_block,
                      uint32 num_interfaces, nsID *iid, uint32 *indexp)
{
    *indexp = 0;
    return PR_TRUE;
}

XPTInterfaceDescriptor *
XPT_NewInterfaceDescriptor(uint32 parent_interface, uint32 num_methods,
                           uint32 num_constants)
{
    XPTInterfaceDescriptor *id = PR_NEWZAP(XPTInterfaceDescriptor);
    if (!id)
        return NULL;

    if (num_methods) {
        id->method_descriptors = PR_CALLOC(num_methods *
                                           sizeof(XPTMethodDescriptor));
        if (!id->method_descriptors)
            goto free_id;
        id->num_methods = num_methods;
    }

    if (num_constants) {
        id->const_descriptors = PR_CALLOC(num_constants *
                                          sizeof(XPTConstDescriptor));
        if (!id->const_descriptors)
            goto free_meth;
        id->num_constants = num_constants;
    }

    return id;

 free_meth:
    PR_FREEIF(id->method_descriptors);
 free_id:
    PR_DELETE(id);
    return NULL;
}

uint32
XPT_SizeOfMethodDescriptor(XPTMethodDescriptor *md)
{
    return 1 /* flags */ + 4 /* name */ + 1 /* num_args */
        + ((md->num_args + 1) * XPT_PARAMDESCRIPTOR_SIZE);
}

uint32
XPT_SizeOfConstDescriptor(XPTConstDescriptor *cd)
{
    return 0;
}

uint32
XPT_SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *id)
{
    uint32 size = 4 /* parent interface */ + 2 /* num_methods */
        + 2 /* num_constants */, i;
    for (i = 0; i < id->num_methods; i++)
        size += XPT_SizeOfMethodDescriptor(&id->method_descriptors[i]);
    for (i = 0; i < id->num_constants; i++)
        size += XPT_SizeOfConstDescriptor(&id->const_descriptors[i]);
    return size;
}

PRBool
XPT_DoInterfaceDescriptor(XPTCursor *outer, XPTInterfaceDescriptor **idp)
{
    XPTMode mode = outer->state->mode;
    XPTInterfaceDescriptor *id;
    XPTCursor curs, *cursor = &curs;
    uint32 i, id_sz = 0;

    if (mode == XPT_DECODE) {
        id = PR_NEWZAP(XPTInterfaceDescriptor);
        if (!id)
            return PR_FALSE;
    } else {
        id = *idp;
        id_sz = XPT_SizeOfInterfaceDescriptor(id);
    }

    if (!XPT_MakeCursor(outer->state, XPT_DATA, id_sz, cursor))
        goto error;

    if(!XPT_Do32(cursor, &id->parent_interface) ||
       !XPT_Do16(cursor, &id->num_methods)) {
        goto error;
    }

    if (mode == XPT_DECODE && id->num_methods) {
        id->method_descriptors = PR_CALLOC(id->num_methods *
                                           sizeof(XPTMethodDescriptor));
        if (!id->method_descriptors)
            goto error;
    }
    
    for (i = 0; i < id->num_methods; i++) {
        if (!XPT_DoMethodDescriptor(cursor, &id->method_descriptors[i]))
            goto error;   
    }
    
    if (!XPT_Do16(cursor, &id->num_constants)) {
        goto error;
    }
    
    if (mode == XPT_DECODE)
        id->const_descriptors = PR_CALLOC(id->num_constants * 
                                          sizeof(XPTConstDescriptor));
    
    for (i = 0; i < id->num_constants; i++) {
        if (!XPT_DoConstDescriptor(&cursor, &id->const_descriptors[i])) {
            goto error;
        }
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(id);    
}

PRBool
XPT_FillConstDescriptor(XPTConstDescriptor *cd, char *name,
                        XPTTypeDescriptor type, union XPTConstValue value)
{
    cd->name = strdup(name);
    if (!cd->name)
        return PR_FALSE;
    XPT_COPY_TYPE(cd->type, type);
    /* XXX copy value */
    return PR_TRUE;
}

PRBool
XPT_DoConstDescriptor(XPTCursor *cursor, XPTConstDescriptor *cd)
{
    XPTMode mode = cursor->state->mode;

    if (!XPT_DoCString(&cursor, &cd->name) ||
        !XPT_DoTypeDescriptor(&cursor, &cd->type)) {

        goto error;
    }

    switch(XPT_TDP_TAG(cd->type.prefix)) {
      case TD_INT8:
        XPT_Do8(cursor, &cd->value.i8);
        break;
      case TD_INT16:
        XPT_Do16(cursor, &cd->value.i16);
        break;
      case TD_INT32:
        XPT_Do32(cursor, &cd->value.i32);
        break;
      case TD_INT64:
        XPT_Do64(cursor, &cd->value.i64);
        break;
      case TD_UINT8:
        XPT_Do8(cursor, &cd->value.ui8);
        break;
      case TD_UINT16:
        XPT_Do16(cursor, &cd->value.ui16);
        break;
      case TD_UINT32:
        XPT_Do32(cursor, &cd->value.ui32);
        break;
      case TD_UINT64:
        XPT_Do64(cursor, &cd->value.ui64);
        break;
      case TD_CHAR:
        XPT_Do8(cursor, &cd->value.ch);
        break;
      case TD_WCHAR:
        XPT_Do16(cursor, &cd->value.wch);
        break;
      case TD_PBSTR:
        if (cd->type.prefix.flags & XPT_TDP_POINTER) {
            XPT_DoString(cursor, &cd->value.string);
            break;
        }
      default:
        fprintf(stderr, "illegal type!\n");
        goto error;
    }

    return PR_TRUE;

    XPT_ERROR_HANDLE(cd);    
}

PRBool
XPT_FillMethodDescriptor(XPTMethodDescriptor *meth, uint8 flags, char *name,
                         uint8 num_args)
{
    meth->flags = flags & XPT_MD_FLAGMASK;
    meth->name = strdup(name);
    if (!name)
        return PR_FALSE;
    meth->num_args = num_args;
    if (meth->num_args) {
        meth->params = PR_CALLOC(num_args * sizeof(XPTParamDescriptor));
        if (!meth->params)
            goto free_name;
    } else {
        meth->params = NULL;
    }
    meth->result = PR_NEWZAP(XPTParamDescriptor);
    if (!meth->result)
        goto free_params;
    return PR_TRUE;

 free_params:
    PR_DELETE(meth->params);
 free_name:
    PR_DELETE(meth->name);
    return PR_FALSE;
}

PRBool
XPT_DoMethodDescriptor(XPTCursor *cursor, XPTMethodDescriptor *md)
{
    XPTMode mode = cursor->state->mode;
    uintn scratch;
    int i;

#ifdef DEBUG_shaver_method
    if (mode == XPT_ENCODE)
        fprintf(stderr, "wrote method \"%s\" at offset %x\n",
                md->name, CURS_POOL_OFFSET(cursor));
#endif

    if (!XPT_Do8(cursor, &md->flags) ||
        !XPT_DoCString(cursor, &md->name) ||
        !XPT_Do8(cursor, &md->num_args))
        return PR_FALSE;

    if (mode == XPT_DECODE) {
        if (md->num_args)
            md->params = PR_CALLOC(md->num_args * sizeof(XPTParamDescriptor));
        if (!md->params)
            return PR_FALSE;
    }

    for(i = 0; i < md->num_args; i++) {
        if (!XPT_DoParamDescriptor(cursor, &md->params[i]))
            goto error;
    }
    
    if (!XPT_DoParamDescriptor(cursor, md->result))
        goto error;
    
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(md->params);    
}

PRBool
XPT_FillParamDescriptor(XPTParamDescriptor *pd, uint8 flags,
                        XPTTypeDescriptor *type)
{
    pd->flags = flags & XPT_PD_FLAGMASK;
    XPT_COPY_TYPE(pd->type, *type);
    return PR_TRUE;
}

PRBool
XPT_DoParamDescriptor(XPTCursor *cursor, XPTParamDescriptor *pd)
{
    XPTMode mode = cursor->state->mode;
    uintn scratch;

#ifdef DEBUG_shaver_param
    if (mode == XPT_ENCODE)
        fprintf(stderr, "wrote param %02x%02x at offset %x\n",
                pd->flags, pd->type.prefix.flags, CURS_POOL_OFFSET(cursor));
#endif
    
    if (!XPT_Do8(cursor, &pd->flags) ||
        !XPT_DoTypeDescriptor(cursor, &pd->type))
        return PR_FALSE;
        
    return PR_TRUE;
}

/* XXX when we lose the useless TDP wrapper struct, #define this to Do8 */
PRBool
XPT_DoTypeDescriptorPrefix(XPTCursor *cursor, XPTTypeDescriptorPrefix *tdp)
{
    return XPT_Do8(cursor, &tdp->flags);
}

PRBool
XPT_DoTypeDescriptor(XPTCursor *cursor, XPTTypeDescriptor *td)
{
    XPTMode mode = cursor->state->mode;
    
    if (!XPT_DoTypeDescriptorPrefix(cursor, &td->prefix)) {
        goto error;
    }
    
    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        if (!XPT_Do32(cursor, &td->type.interface))
            goto error;
    } else {
        if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
            if (!XPT_Do8(cursor, &td->type.argnum))
                goto error;
        }
    }
   
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(td);    
}

XPTAnnotation *
XPT_NewAnnotation(uint8 flags, XPTString *creator, XPTString *private_data)
{
    XPTAnnotation *ann = PR_NEWZAP(XPTAnnotation);
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
XPT_DoAnnotation(XPTCursor *cursor, XPTAnnotation **annp)
{
    XPTMode mode = cursor->state->mode;
    XPTAnnotation *ann;
    
    fprintf(stderr, "DoAnnotation\n");
    if (mode == XPT_DECODE) {
        ann = PR_NEWZAP(XPTAnnotation);
        if (!ann)
            return PR_FALSE;
        *annp = ann;
    } else {
        ann = *annp;
    }
    
    if (!XPT_Do8(cursor, &ann->flags))
        goto error;

    if (XPT_ANN_IS_PRIVATE(ann->flags)) {
        if (!XPT_DoStringInline(cursor, &ann->creator) ||
            !XPT_DoStringInline(cursor, &ann->private_data))
            goto error_2;
    }
    
    /*
     * If a subsequent Annotation fails, what to do?
     * - free all annotations, return PR_FALSE? (current behaviout)
     * - free failed annotation only, return PR_FALSE (caller can check for
     *   non-NULL *annp on PR_FALSE return to detect partial annotation
     *   decoding)?
     */
    if (!XPT_ANN_IS_LAST(ann->flags) &&
        !XPT_DoAnnotation(cursor, &ann->next))
        goto error_2;

    return PR_TRUE;

 error_2:
    if (ann && XPT_ANN_IS_PRIVATE(ann->flags)) {
        PR_FREEIF(ann->creator);
        PR_FREEIF(ann->private_data);
    }
    XPT_ERROR_HANDLE(ann);
}

PRBool 
XPT_DoAnnotations(XPTCursor *cursor, XPTAnnotation **ap)
{
    return PR_FALSE;
}

XPTInterfaceDescriptor *
XPT_GetDescriptorByOffset(XPTState *state, XPTHeader *header, uint32
                          descriptor_num)
{
}

