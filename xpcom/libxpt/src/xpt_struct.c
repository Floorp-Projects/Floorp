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

PRBool
XPT_DoHeader(XPTCursor *cursor, XPTHeader **headerp)
{
    XPTMode mode = cursor->state->mode;
    XPTCursor my_cursor;
    XPTHeader *header;
    PRBool already;
    int i;

    XPT_PREAMBLE(cursor, headerp, XPT_HEADER, XPT_SizeOfHeader(*headerp), 
                 my_cursor, already, XPTHeader, header);

    for (i = 0; i < 16; i++) {
        if (!XPT_Do8(&my_cursor, &header->magic[i]))
            goto error;
    }
    
    if(!XPT_Do8(&my_cursor, &header->major_version) ||
       !XPT_Do8(&my_cursor, &header->minor_version) ||
       !XPT_Do16(&my_cursor, &header->num_interfaces) ||
       !XPT_Do32(&my_cursor, &header->file_length) ||
       !XPT_DoInterfaceDirectoryEntry(&my_cursor, 
                                      &header->interface_directory) ||
       !XPT_Do8(&my_cursor, header->data_pool) ||
       !XPT_DoAnnotation(&my_cursor, &header->annotations)) { 

        goto error;
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(header);    
}   

/* InterfaceDirectoryEntry records go in the header */
PRBool
XPT_DoInterfaceDirectoryEntry(XPTCursor *cursor,
                              XPTInterfaceDirectoryEntry **idep)
{    
    XPTMode mode = cursor->state->mode;
    XPTCursor my_cursor;
    XPTInterfaceDirectoryEntry *ide;
    PRBool already;

    XPT_PREAMBLE(cursor, idep, XPT_HEADER, XPT_IDE_SIZE, 
                 my_cursor, already, XPTInterfaceDirectoryEntry, 
                 ide);
    
    /* write the IID in our cursor space */
    if (!XPT_DoIID(&my_cursor, &ide->iid) ||
        
        /* write the name string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(&my_cursor, &ide->name) ||
        
        /* write the namespace string in the data pool, and the offset in our
           cursor space */
        !XPT_DoCString(&my_cursor, &ide->namespace) ||
        
        /* write the InterfaceDescriptor in the data pool, and the offset
           in our cursor space */
        !XPT_DoInterfaceDescriptor(&my_cursor, &ide->interface_descriptor)) {

        goto error;
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(ide);    
}

PRBool
XPT_DoInterfaceDescriptor(XPTCursor *cursor, XPTInterfaceDescriptor **idp)
{
    XPTMode mode = cursor->state->mode;
    XPTInterfaceDescriptor *id;
    XPTCursor my_cursor;
    PRBool already;
    int i, len;

    XPT_PREAMBLE(cursor, idp, XPT_DATA, XPT_SizeOfInterfaceDescriptor(*idp), 
                 my_cursor, already, XPTInterfaceDescriptor, id);

    if(!XPT_DoInterfaceDirectoryEntry(&my_cursor, &id->parent_interface) ||
       !XPT_Do16(&my_cursor, &id->num_methods)) {
     
        goto error;
    }

    if (mode == XPT_DECODE)
        id->method_descriptors = PR_CALLOC(id->num_methods * 
                                           sizeof(XPTMethodDescriptor));
    
    for (i = 0; i < id->num_methods; i++) {
        if (!XPT_DoMethodDescriptor(&my_cursor, &id->method_descriptors[i]))
            goto error;   
    }
    
    if (!XPT_Do16(&my_cursor, &id->num_constants)) {
        goto error;
    }
    
    if (mode == XPT_DECODE)
        id->const_descriptors = PR_CALLOC(id->num_constants * 
                                          sizeof(XPTConstDescriptor));
    
    for (i = 0; i < id->num_constants; i++) {
        if (!XPT_DoConstDescriptor(&my_cursor, &id->const_descriptors[i])) {
            goto error;
        }
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(id);    
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

    switch(cd->type.prefix->tag) {
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
        if (cd->type.prefix->is_pointer == 1) {
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
    
    if (td->prefix->tag == TD_INTERFACE_TYPE) {
        if (!XPT_DoInterfaceDirectoryEntry(cursor, &td->type.interface))
            goto error;
    } else {
        if (td->prefix->tag == TD_INTERFACE_IS_TYPE) {
            if (!XPT_Do8(cursor, &td->type.argnum))
                goto error;
        }
    }
   
    return PR_TRUE;
    
    XPT_ERROR_HANDLE(td);    
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
    
    if (a->prefix->tag == PRIVATE_ANNOTATION) {
        if (!XPT_DoPrivateAnnotation(cursor, &a->private)) {
            goto error;
        }
    }
    
    return PR_TRUE;

    XPT_ERROR_HANDLE(a);
}

int
XPT_SizeOfHeader(XPTHeader *headerp)
{
    return 0;
}    

int
XPT_SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *idp)
{
    return 0;
}
