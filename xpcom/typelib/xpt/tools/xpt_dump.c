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

/*
 * A utility for dumping the contents of a typelib file (.xpt) to screen 
 */ 

#include "xpt_xdr.h"
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define BASE_INDENT 3

int 
main(int argc, char **argv)
{
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    struct stat file_stat;
    int flen;
    char *whole;
    FILE *in;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename.xpt>\n"
                "       Dumps a typelib file to screen.\n", argv[0]);
        return 1;
    }

    if (stat(argv[1], &file_stat) != 0) {
        perror("FAILED: fstat");
        return 1;
    }
    flen = file_stat.st_size;

    in = fopen(argv[1], "r");
    if (!in) {
        perror("FAILED: fopen");
        return 1;
    }

    whole = malloc(flen);
    if (!whole) {
        perror("FAILED: malloc for whole");
        return 1;
    }

    if (flen > 0) {
        fread(whole, flen, 1, in);
        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            fprintf(stderr, "MakeCursor failed\n");
            return 1;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            fprintf(stderr, "DoHeader failed\n");
            return 1;
        }
        if (!XPT_DumpHeader(header, BASE_INDENT)) {
            perror("FAILED: XPT_DumpHeader");
            return 1;
        }
        XPT_DestroyXDRState(state);
        free(whole);
    }

    fclose(in);

    return 0;
}

PRBool
XPT_DumpHeader(XPTHeader *header, const int indent) 
{
    int i;
    
    fprintf(stderr, "%*sMagic beans:      ", indent, " ");
    for (i=0; i<16; i++) {
        fprintf(stderr, "%02x", header->magic[i]);
    }
    fprintf(stderr, "\n");
    if (strncmp(header->magic, XPT_MAGIC, 16) == 0)
        fprintf(stderr, "%*s                  PASSED\n", indent, " ");
    else
        fprintf(stderr, "%*s                  FAILED\n", indent, " "); 
    fprintf(stderr, "%*sMajor version:    %d\n", indent, " ",
            header->major_version);
    fprintf(stderr, "%*sMinor version:    %d\n", indent, " ",
            header->minor_version);
    fprintf(stderr, "%*s# of interfaces:  %d\n", indent, " ",
            header->num_interfaces);
    fprintf(stderr, "%*sFile length:      %d\n", indent, " ",
            header->file_length);
    fprintf(stderr, "%*sData pool offset: %d\n", indent, " ",
            header->data_pool);

    fprintf(stderr, "\n%*sAnnotations:\n", indent, " ");
    if (!XPT_DumpAnnotations(header->annotations, indent*2))
        return PR_FALSE;
    
    fprintf(stderr, "\n%*sInterface Directory:\n", indent, " ");
    for (i=0; i<header->num_interfaces; i++) {
        fprintf(stderr, "%*sInterface #%d:\n", indent*2, " ", i);

        if (!XPT_DumpInterfaceDirectoryEntry(&header->interface_directory[i],
                                             indent*3))
            return PR_FALSE;
    }

    return PR_TRUE;
}    

PRBool
XPT_DumpAnnotations(XPTAnnotation *ann, const int indent) 
{
    int i = -1, j = 0;
    XPTAnnotation *last;
    int new_indent = indent + BASE_INDENT;

    do {
        i++;
        if (XPT_ANN_IS_PRIVATE(ann->flags)) {
            fprintf(stderr, "%*sAnnotation #%d is private.\n", 
                    indent, " ", i);
            fprintf(stderr, "%*sCreator:      ", new_indent, " ");
            if (!XPT_DumpXPTString(ann->creator))
                return PR_FALSE;
            fprintf(stderr, "\n");
            fprintf(stderr, "%*sPrivate Data: ", new_indent, " ");            
            if (!XPT_DumpXPTString(ann->private_data))
                return PR_FALSE;
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "%*sAnnotation #%d is empty.\n", 
                    indent, " ", i);
        }
        last = ann;
        ann = ann->next;
    } while (!XPT_ANN_IS_LAST(last->flags));
        
    fprintf(stderr, "%*sAnnotation #%d is the last annotation.\n", 
            indent, " ", i);
    
    return PR_TRUE;
}

PRBool
XPT_DumpInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide, const int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stderr, "%*sIID:                             "
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", indent, " ",
            (PRUint32) ide->iid.m0, (PRUint32) ide->iid.m1,(PRUint32) ide->iid.m2,
            (PRUint32) ide->iid.m3[0], (PRUint32) ide->iid.m3[1],
            (PRUint32) ide->iid.m3[2], (PRUint32) ide->iid.m3[3],
            (PRUint32) ide->iid.m3[4], (PRUint32) ide->iid.m3[5],
            (PRUint32) ide->iid.m3[6], (PRUint32) ide->iid.m3[7]);
    
    fprintf(stderr, "%*sName:                            %s\n", 
            indent, " ", ide->name);
    fprintf(stderr, "%*sNamespace:                       %s\n", 
            indent, " ", ide->namespace);
    fprintf(stderr, "%*sAddress of interface descriptor: %p\n", 
            indent, " ", ide->interface_descriptor);

    fprintf(stderr, "%sDescriptor:\n", indent);
    if (!XPT_DumpInterfaceDescriptor(&ide->interface_descriptor, new_indent))
        return PR_FALSE;

    return PR_TRUE;
}    

PRBool
XPT_DumpInterfaceDescriptor(XPTInterfaceDescriptor *id, int indent)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    fprintf(stderr, "%*sOffset of parent interface (in data pool): %d\n", 
            indent, " ", id->parent_interface);
    fprintf(stderr, "%*s# of Method Descriptors:                   %d\n", 
            indent, " ", id->num_methods);
    for (i=0; i<id->num_methods; i++) {
        fprintf(stderr, "%*sMethod #%d:\n", new_indent, " ", i);
        if (!XPT_DumpMethodDescriptor(&id->method_descriptors[i], more_indent))
            return PR_FALSE;
    }
    fprintf(stderr, "%*s# of Constant Descriptors:                  %d\n", 
            indent, id->num_constants);
    for (i=0; i<id->num_constants; i++) {
        fprintf(stderr, "%*sConstant #%d:\n", new_indent, " ", i);
        if (!XPT_DumpConstDescriptor(&id->const_descriptors[i], more_indent))
            return PR_FALSE;
    }
    
    return PR_TRUE;
}

PRBool
XPT_DumpMethodDescriptor(XPTMethodDescriptor *md, int indent)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    fprintf(stderr, "%*sName:             %s\n", indent, " ", md->name);
    fprintf(stderr, "%*sIs Getter?        ", indent, " ");
    if (XPT_MD_IS_GETTER(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Setter?        ", indent, " ");
    if (XPT_MD_IS_SETTER(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Varargs?       ", indent, " ");
    if (XPT_MD_IS_VARARGS(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Constructor?   ", indent, " ");
    if (XPT_MD_IS_CTOR(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Hidden?        ", indent, " ");
    if (XPT_MD_IS_HIDDEN(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*s# of arguments:   %d\n", indent, " ", md->num_args);
    fprintf(stderr, "%*sParameter Descriptors:\n", indent, " ");

    for (i=0; i<md->num_args; i++) {
        fprintf(stderr, "%*sParameter #%d:\n", new_indent, " ", i);
        
        if (!XPT_DumpParamDescriptor(&md->params[i], more_indent))
            return PR_FALSE;
    }
   
    fprintf(stderr, "%*sResult:\n", indent, " ");
    if (!XPT_DumpParamDescriptor(md->result, new_indent))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpXPTString(XPTString *str)
{
    int i;
    for (i=0; i<str->length; i++) {
        fprintf(stderr, "%c", str->bytes[i]);
    }
    return PR_TRUE;
}

PRBool
XPT_DumpParamDescriptor(XPTParamDescriptor *pd, int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stderr, "%*sIn Param?   ", indent, " ");
    if (XPT_PD_IS_IN(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");
    
    fprintf(stderr, "%*sOut Param?  ", indent, " ");
    if (XPT_PD_IS_OUT(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");
    
    fprintf(stderr, "%*sRetval?     ", indent, " ");
    if (XPT_PD_IS_RETVAL(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sType Descriptor:\n", indent, " ");
    if (!XPT_DumpTypeDescriptor(&pd->type, new_indent))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td, int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stderr, "%*sIs Pointer?        ", indent, " ");
    if (XPT_TDP_IS_POINTER(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Unique Pointer? ", indent, " ");
    if (XPT_TDP_IS_UNIQUE_POINTER(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sIs Reference?      ", indent, " ");
    if (XPT_TDP_IS_REFERENCE(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%*sTag:               %d\n", indent, " ", 
            XPT_TDP_TAG(td->prefix));
    
    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        fprintf(stderr, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stderr, "%*sIndex of IDE:             %d\n", new_indent, " ", 
                td->type.interface);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
        fprintf(stderr, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stderr, "%*sIndex of Method Argument: %d\n", new_indent, " ", 
                td->type.argnum);        
    }

    return PR_TRUE;
}

PRBool
XPT_DumpConstDescriptor(XPTConstDescriptor *cd, int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stderr, "%*sName:   %s\n", indent, " ", cd->name);
    fprintf(stderr, "%*sType Descriptor: \n", " ", indent);
    if (!XPT_DumpTypeDescriptor(&cd->type, new_indent))
        return PR_FALSE;
    fprintf(stderr, "%*sValue:  ", indent, " ");

    switch(XPT_TDP_TAG(cd->type.prefix)) {
    case TD_INT8:
        fprintf(stderr, "%d\n", cd->value.i8);
        break;
    case TD_INT16:
        fprintf(stderr, "%d\n", cd->value.i16);
        break;
    case TD_INT32:
        fprintf(stderr, "%d\n", cd->value.i32);
        break;
    case TD_INT64:
        fprintf(stderr, "%d\n", cd->value.i64);
        break;
    case TD_UINT8:
        fprintf(stderr, "%d\n", cd->value.ui8);
        break;
    case TD_UINT16:
        fprintf(stderr, "%d\n", cd->value.ui16);
        break;
    case TD_UINT32:
        fprintf(stderr, "%d\n", cd->value.ui32);
        break;
    case TD_UINT64:
        fprintf(stderr, "%d\n", cd->value.ui64);
        break;
    case TD_FLOAT:
        fprintf(stderr, "%f\n", cd->value.flt);
        break;
    case TD_DOUBLE:
        fprintf(stderr, "%g\n", cd->value.dbl);
        break;
    case TD_BOOL:
        if (cd->value.bul)
            fprintf(stderr, "TRUE\n");
        else 
            fprintf(stderr, "FALSE\n");
            break;
    case TD_CHAR:
        fprintf(stderr, "%d\n", cd->value.ui64);
        break;
    case TD_WCHAR:
        fprintf(stderr, "%d\n", cd->value.ui64);
        break;
    case TD_VOID:
        fprintf(stderr, "VOID\n");
        break;
    case TD_PPNSIID:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
        fprintf(stderr, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                cd->value.iid->m0, (PRUint32) cd->value.iid->m1,
                (PRUint32) cd->value.iid->m2,
                (PRUint32) cd->value.iid->m3[0], 
                (PRUint32) cd->value.iid->m3[1],
                (PRUint32) cd->value.iid->m3[2], 
                (PRUint32) cd->value.iid->m3[3],
                (PRUint32) cd->value.iid->m3[4], 
                (PRUint32) cd->value.iid->m3[5],
                (PRUint32) cd->value.iid->m3[6], 
                (PRUint32) cd->value.iid->m3[7]);
        } else 
            return PR_FALSE;
        break;
    case TD_PBSTR:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            if (!XPT_DumpXPTString(cd->value.string))
                return PR_FALSE;
            fprintf(stderr, "\n");
        } else 
            return PR_FALSE;
        break;            
    case TD_PSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            fprintf(stderr, "%s\n", cd->value.str);
        } else 
            return PR_FALSE;
        break;
    case TD_PWSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            fprintf(stderr, "%s\n", cd->value.wstr);
        } else 
            return PR_FALSE;
        break;
    default:
        perror("Unrecognized type");
        return PR_FALSE;
        break;   
    }

    return PR_TRUE;
}
