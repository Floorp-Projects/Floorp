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

uint32
XPT_SizeOfHeader(XPTHeader *header)
{
    XPTAnnotation *ann;
    uint32 size = 16 /* magic */ +
        1 /* major */ + 1 /* minor */ +
        2 /* num_interfaces */ + 4 /* file_length */ +
        4 /* interface_directory */ + 4 /* data_pool */;

    /* XXX annotations */
    
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

    if (mode == XPT_DECODE)
        header = PR_NEWZAP(XPTHeader);
    else
        header = *headerp;

    if (mode == XPT_ENCODE) {
        /* IDEs appear after header, including annotations */
        ide_offset = XPT_SizeOfHeader(*headerp);
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

    /* XXX handle annotations */

    /* shouldn't be necessary now, but maybe later */
    XPT_SeekTo(cursor, ide_offset); 

    for (i = 0; i < header->num_interfaces; i++) {
        XPT_DoInterfaceDirectoryEntry(cursor,
                                      &header->interface_directory[i]);
    }
    
    for (i = 0; i < header->num_interfaces; i++) {
        if (!XPT_DoInterfaceDirectoryEntry(&cursor, 
                                           &header->interface_directory[i]))
            goto error;
    }
    
    return PR_TRUE;

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
                              XPTInterfaceDirectoryEntry **idep)
{    
    XPTMode mode = cursor->state->mode;
    XPTInterfaceDirectoryEntry *ide;

    if (mode == XPT_DECODE)
        ide = PR_NEWZAP(XPTInterfaceDirectoryEntry);
    else
        ide = *idep;
    
    /* write the IID in our cursor space */
    if (!XPT_DoIID(cursor, &ide->iid) ||
        
        /* write the name string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(&cursor, &ide->name) ||
        
        /* write the namespace string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(&cursor, &ide->namespace)) {
        
        goto error;
    }
    
    /* write the InterfaceDescriptor in the data pool, and the offset
       in our cursor space, but only if we're encoding. */
    if (mode == XPT_ENCODE) {
        if (!XPT_DoInterfaceDescriptor(&cursor, 
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

PRBool
XPT_DoInterfaceDescriptor(XPTCursor *cursor, XPTInterfaceDescriptor **idp)
{
    XPTMode mode = cursor->state->mode;
    XPTInterfaceDescriptor *id;
    int i;

    if (mode == XPT_DECODE)
        id = PR_NEWZAP(XPTInterfaceDescriptor);
    else
        id = *idp;

    if(!XPT_Do32(cursor, &id->parent_interface) ||
       !XPT_Do16(cursor, &id->num_methods)) {
     
        goto error;
    }

    if (mode == XPT_DECODE)
        id->method_descriptors = PR_CALLOC(id->num_methods * 
                                           sizeof(XPTMethodDescriptor));
    
    for (i = 0; i < id->num_methods; i++) {
        if (!XPT_DoMethodDescriptor(&cursor, &id->method_descriptors[i]))
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
XPT_DoConstDescriptor(XPTCursor *cursor, XPTConstDescriptor **cdp)
{
    XPTMode mode = cursor->state->mode;
    XPTConstDescriptor *cd;

    if (mode == XPT_DECODE)
        cd = PR_NEWZAP(XPTConstDescriptor);
    else
        cd = *cdp;
    
    if (!XPT_DoCString(&cursor, &cd->name) ||
        !XPT_DoTypeDescriptor(&cursor, &cd->type)) {

        goto error;
    }

    switch(cd->type.prefix.tag) {
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
        if (cd->type.prefix.is_pointer == 1) {
            XPT_DoString(cursor, &cd->value.string);
            break;
        }
        goto error;
    default:
        goto error;
    }

    return PR_TRUE;

    XPT_ERROR_HANDLE(cd);    
}

PRBool
XPT_FillMethodDescriptor(XPTMethodDescriptor *meth, PRBool is_getter,
                         PRBool is_setter, PRBool is_varargs,
                         PRBool is_constructor, PRBool is_hidden, char *name,
                         uint8 num_args)
{
    meth->is_getter = is_getter;
    meth->is_setter = is_setter;
    meth->is_constructor = is_constructor;
    meth->is_hidden = is_hidden;
    meth->reserved = 0;
    meth->name = strdup(name);
    if (!name)
        return PR_FALSE;
    meth->num_args = num_args;
    meth->params = PR_CALLOC(num_args * sizeof(XPTParamDescriptor));
    if (!meth->params)
        goto free_name;
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
XPT_DoMethodDescriptor(XPTCursor *cursor, XPTMethodDescriptor **mdp)
{
    XPTMode mode = cursor->state->mode;
    XPTMethodDescriptor *md;
    uintn scratch;
    int i;

    if (mode == XPT_DECODE)
        md = PR_NEWZAP(XPTMethodDescriptor);
    else
        md = *mdp;

    if (!XPT_DO_BITS(&cursor, md->is_getter, 1, scratch) ||
        !XPT_DO_BITS(&cursor, md->is_setter, 1, scratch) ||
        !XPT_DO_BITS(&cursor, md->is_varargs, 1, scratch) ||
        !XPT_DO_BITS(&cursor, md->is_constructor, 1, scratch) ||
        !XPT_DO_BITS(&cursor, md->is_hidden, 1, scratch) ||
        !XPT_DO_BITS(&cursor, md->reserved, 3, scratch) ||
        !XPT_DoCString(&cursor, &md->name) ||
        !XPT_Do8(cursor, &md->num_args)) {
      
        goto error;
    }

    if (mode == XPT_DECODE)
        (uint8)md->num_args = PR_CALLOC(md->num_args * (int)sizeof(XPTParamDescriptor));
    
    for(i = 0; i < md->num_args; i++) {
        if (!XPT_DoParamDescriptor(&cursor, &md->params))
            goto error;
    }
    
    if (!XPT_DoParamDescriptor(&cursor, &md->result))
        goto error;
    
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(md);    
}

PRBool
XPT_FillParamDescriptor(XPTParamDescriptor *pd, PRBool in, PRBool out,
                        PRBool retval, XPTTypeDescriptor type)
{
    pd->in = in;
    pd->out = out;
    pd->retval = retval;
    pd->reserved = 0;
    XPT_COPY_TYPE(pd->type, type);
}

PRBool
XPT_DoParamDescriptor(XPTCursor *cursor, XPTParamDescriptor **pdp)
{
    XPTMode mode = cursor->state->mode;
    XPTParamDescriptor *pd;
    uintn scratch;

    if (mode == XPT_DECODE)
        pd = PR_NEWZAP(XPTParamDescriptor);
    else
        pd = *pdp;

    if (!XPT_DO_BITS(&cursor, pd->in, 1, scratch) ||
        !XPT_DO_BITS(&cursor, pd->out, 1, scratch) ||
        !XPT_DO_BITS(&cursor, pd->retval, 1, scratch) ||
        !XPT_DO_BITS(&cursor, pd->reserved, 5, scratch) ||
        !XPT_DoTypeDescriptor(&cursor, &pd->type)) {

        goto error;
    }
        
    return PR_TRUE;

    XPT_ERROR_HANDLE(pd);    
}

PRBool
XPT_DoTypeDescriptorPrefix(XPTCursor *cursor, XPTTypeDescriptorPrefix **tdpp)
{
    XPTMode mode = cursor->state->mode;
    XPTTypeDescriptorPrefix *tdp;
    uintn scratch;
    
    if (mode == XPT_DECODE)
        tdp = PR_NEWZAP(XPTTypeDescriptorPrefix);
    else
        tdp = *tdpp;
    
    if (!XPT_DO_BITS(&cursor, tdp->is_pointer, 1, scratch) ||
        !XPT_DO_BITS(&cursor, tdp->is_unique_pointer, 1, scratch) ||
        !XPT_DO_BITS(&cursor, tdp->is_reference, 1, scratch) ||
        !XPT_DO_BITS(&cursor, tdp->tag, 5, scratch)) { 

        goto error;
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(tdp);    
}

PRBool
XPT_DoTypeDescriptor(XPTCursor *cursor, XPTTypeDescriptor **tdp)
{
    XPTMode mode = cursor->state->mode;
    XPTTypeDescriptor *td;
    
    if (mode == XPT_DECODE)
        td = PR_NEWZAP(XPTTypeDescriptor);
    else
        td = *tdp;
    
    if (!XPT_DoTypeDescriptorPrefix(cursor, &td->prefix)) {
        goto error;
    }
    
    if (td->prefix.tag == TD_INTERFACE_TYPE) {
        if (!XPT_Do32(cursor, &td->type.interface))
            goto error;
    } else {
        if (td->prefix.tag == TD_INTERFACE_IS_TYPE) {
            if (!XPT_Do8(cursor, &td->type.argnum))
                goto error;
        }
    }
   
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(td);    
}

XPTAnnotation *
XPT_NewAnnotation(PRBool is_last, PRBool is_empty, XPTString *creator,
                  XPTString *private_data)
{
    XPTAnnotation *ann = PR_NEWZAP(XPTAnnotation);
    if (!ann)
        return NULL;
    ann->prefix.is_last = is_last;
    ann->prefix.tag = is_empty ? 0 : 1;
    if (!is_empty) {
        ann->private.creator = creator;
        ann->private.private_data = private_data;
    }
    return ann;
}

PRBool
XPT_DoAnnotationPrefix(XPTCursor *cursor, XPTAnnotationPrefix **app)
{
    XPTMode mode = cursor->state->mode;
    XPTAnnotationPrefix *ap;
    uintn scratch;
    
    if (mode == XPT_DECODE)
        ap = PR_NEWZAP(XPTAnnotationPrefix);
    else
        ap = *app;
    
    if (!XPT_DO_BITS(&cursor, ap->is_last, 1, scratch) ||
        !XPT_DO_BITS(&cursor, ap->tag, 7, scratch)) {

        goto error;
    }

    return PR_TRUE;
    
    XPT_ERROR_HANDLE(ap);
}

PRBool
XPT_DoPrivateAnnotation(XPTCursor *cursor, XPTPrivateAnnotation **pap)
{
    XPTMode mode = cursor->state->mode;
    XPTPrivateAnnotation *pa;
    
    if (mode == XPT_DECODE)
        pa = PR_NEWZAP(XPTPrivateAnnotation);
    else
        pa = *pap;
    
    if (!XPT_DoString(cursor, &pa->creator) ||
        !XPT_DoString(cursor, &pa->private_data)) {

        goto error;
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(pa);
}

PRBool
XPT_DoAnnotation(XPTCursor *cursor, XPTAnnotation **ap)
{
    XPTMode mode = cursor->state->mode;
    XPTAnnotation *a;
    
    if (mode == XPT_DECODE)
        a = PR_NEWZAP(XPTAnnotation);
    else
        a = *ap;
    
    if (!XPT_DoAnnotationPrefix(cursor, &a->prefix)) {
        goto error;
    }
    
    if (a->prefix.tag == PRIVATE_ANNOTATION) {
        if (!XPT_DoPrivateAnnotation(cursor, &a->private)) {
            goto error;
        }
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(a);
}

PRBool 
XPT_DoAnnotations(XPTCursor *cursor, XPTAnnotation **ap) {
    return PR_FALSE;
}

int
XPT_SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *idp)
{
    return 0;
}

XPTInterfaceDescriptor
XPT_GetDescriptorByOffset(XPTState *state, XPTHeader *header, uint32
                          descriptor_num) {
}

