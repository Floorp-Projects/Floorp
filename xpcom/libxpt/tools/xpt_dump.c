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
#include <string.h>
#include "prprf.h"

#define BASE_INDENT 3

PRBool
XPT_VDumpHeader(XPTHeader *header, const int indent);

PRBool
XPT_VDumpAnnotations(XPTAnnotation *ann, const int indent);

PRBool
XPT_VDumpInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide, 
                                 const int indent);
PRBool
XPT_VDumpInterfaceDescriptor(XPTInterfaceDescriptor *id, const int indent);

PRBool
XPT_VDumpMethodDescriptor(XPTMethodDescriptor *md, const int indent);

PRBool
XPT_DumpXPTString(XPTString *str);

PRBool
XPT_VDumpParamDescriptor(XPTParamDescriptor *pd, const int indent);

PRBool
XPT_VDumpTypeDescriptor(XPTTypeDescriptor *td, int indent);

PRBool
XPT_VDumpConstDescriptor(XPTConstDescriptor *cd, const int indent);

static void
xpt_dump_usage(char *argv[]) {
    fprintf(stdout, "Usage: %s [-v] <filename.xpt>\n"
            "       -v verbose mode\n", argv[0]);
}

int 
main(int argc, char **argv)
{
    PRBool verbose_mode = PR_FALSE;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    struct stat file_stat;
    int flen;
    char *whole;
    FILE *in;

    switch (argc) {
    case 2:
        if (argv[1][0] == '-') {
            xpt_dump_usage(argv);
            return 1;
        }
        if (stat(argv[1], &file_stat) != 0) {
            perror("FAILED: fstat");
            return 1;
        }
        flen = file_stat.st_size;
        in = fopen(argv[1], "r");
        break;
    case 3:
        verbose_mode = PR_TRUE;
        if (argv[1][0] != '-' || argv[1][1] != 'v') {
            xpt_dump_usage(argv);
            return 1;
        }
        if (stat(argv[2], &file_stat) != 0) {
            perror("FAILED: fstat");
            return 1;
        }
        flen = file_stat.st_size;
        in = fopen(argv[2], "r");
        break;
    default:
        xpt_dump_usage(argv);
        return 1;
    }

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
            fprintf(stdout, "MakeCursor failed\n");
            return 1;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            fprintf(stdout, "DoHeader failed\n");
            return 1;
        }
        if (verbose_mode) {
            if (!XPT_VDumpHeader(header, BASE_INDENT)) {
                perror("FAILED: XPT_VDumpHeader");
                return 1;
            }
        } else {
            if (!XPT_VDumpHeader(header, BASE_INDENT)) {
                perror("FAILED: XPT_DumpHeader");
                return 1;
            }
        }
   
        XPT_DestroyXDRState(state);
        free(whole);
    }

    fclose(in);

    return 0;
}

PRBool
XPT_VDumpHeader(XPTHeader *header, const int indent) 
{
    int i;
    
    fprintf(stdout, "%*sMagic beans:      ", indent, " ");
    for (i=0; i<16; i++) {
        fprintf(stdout, "%02x", header->magic[i]);
    }
    fprintf(stdout, "\n");
    if (strncmp(header->magic, XPT_MAGIC, 16) == 0)
        fprintf(stdout, "%*s                  PASSED\n", indent, " ");
    else
        fprintf(stdout, "%*s                  FAILED\n", indent, " "); 
    fprintf(stdout, "%*sMajor version:    %d\n", indent, " ",
            header->major_version);
    fprintf(stdout, "%*sMinor version:    %d\n", indent, " ",
            header->minor_version);
    fprintf(stdout, "%*s# of interfaces:  %d\n", indent, " ",
            header->num_interfaces);
    fprintf(stdout, "%*sFile length:      %d\n", indent, " ",
            header->file_length);
    fprintf(stdout, "%*sData pool offset: %d\n", indent, " ",
            header->data_pool);

    fprintf(stdout, "\n%*sAnnotations:\n", indent, " ");
    if (!XPT_VDumpAnnotations(header->annotations, indent*2))
        return PR_FALSE;
    
    fprintf(stdout, "\n%*sInterface Directory:\n", indent, " ");
    for (i=0; i<header->num_interfaces; i++) {
        fprintf(stdout, "%*sInterface #%d:\n", indent*2, " ", i);

        if (!XPT_VDumpInterfaceDirectoryEntry(&header->interface_directory[i],
                                             indent*3))
            return PR_FALSE;
    }

    return PR_TRUE;
}    

PRBool
XPT_VDumpAnnotations(XPTAnnotation *ann, const int indent) 
{
    int i = -1;
    XPTAnnotation *last;
    int new_indent = indent + BASE_INDENT;

    do {
        i++;
        if (XPT_ANN_IS_PRIVATE(ann->flags)) {
            fprintf(stdout, "%*sAnnotation #%d is private.\n", 
                    indent, " ", i);
            fprintf(stdout, "%*sCreator:      ", new_indent, " ");
            if (!XPT_DumpXPTString(ann->creator))
                return PR_FALSE;
            fprintf(stdout, "\n");
            fprintf(stdout, "%*sPrivate Data: ", new_indent, " ");            
            if (!XPT_DumpXPTString(ann->private_data))
                return PR_FALSE;
            fprintf(stdout, "\n");
        } else {
            fprintf(stdout, "%*sAnnotation #%d is empty.\n", 
                    indent, " ", i);
        }
        last = ann;
        ann = ann->next;
    } while (!XPT_ANN_IS_LAST(last->flags));
        
    fprintf(stdout, "%*sAnnotation #%d is the last annotation.\n", 
            indent, " ", i);
    
    return PR_TRUE;
}

PRBool
XPT_VDumpInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide, 
                                 const int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stdout, "%*sIID:                             "
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", indent, " ",
            (PRUint32) ide->iid.m0, (PRUint32) ide->iid.m1,(PRUint32) ide->iid.m2,
            (PRUint32) ide->iid.m3[0], (PRUint32) ide->iid.m3[1],
            (PRUint32) ide->iid.m3[2], (PRUint32) ide->iid.m3[3],
            (PRUint32) ide->iid.m3[4], (PRUint32) ide->iid.m3[5],
            (PRUint32) ide->iid.m3[6], (PRUint32) ide->iid.m3[7]);
    
    fprintf(stdout, "%*sName:                            %s\n", 
            indent, " ", ide->name);
    fprintf(stdout, "%*sNamespace:                       %s\n", 
            indent, " ", ide->namespace);
    fprintf(stdout, "%*sAddress of interface descriptor: %p\n", 
            indent, " ", ide->interface_descriptor);

    fprintf(stdout, "%*sDescriptor:\n", indent, " ");
    if (!XPT_VDumpInterfaceDescriptor(ide->interface_descriptor, new_indent))
        return PR_FALSE;

    return PR_TRUE;
}    

PRBool
XPT_VDumpInterfaceDescriptor(XPTInterfaceDescriptor *id, const int indent)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    fprintf(stdout, "%*sOffset of parent interface (in data pool): %p\n", 
            indent, " ", id->parent_interface);
    fprintf(stdout, "%*s# of Method Descriptors:                   %d\n", 
            indent, " ", id->num_methods);
    for (i=0; i<id->num_methods; i++) {
        fprintf(stdout, "%*sMethod #%d:\n", new_indent, " ", i);
        if (!XPT_VDumpMethodDescriptor(&id->method_descriptors[i], more_indent))
            return PR_FALSE;
    }
    fprintf(stdout, "%*s# of Constant Descriptors:                  %d\n", 
            indent, " ", id->num_constants);
    for (i=0; i<id->num_constants; i++) {
        fprintf(stdout, "%*sConstant #%d:\n", new_indent, " ", i);
        if (!XPT_VDumpConstDescriptor(&id->const_descriptors[i], more_indent))
            return PR_FALSE;
    }
    
    return PR_TRUE;
}

PRBool
XPT_VDumpMethodDescriptor(XPTMethodDescriptor *md, const int indent)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    fprintf(stdout, "%*sName:             %s\n", indent, " ", md->name);
    fprintf(stdout, "%*sIs Getter?        ", indent, " ");
    if (XPT_MD_IS_GETTER(md->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Setter?        ", indent, " ");
    if (XPT_MD_IS_SETTER(md->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Varargs?       ", indent, " ");
    if (XPT_MD_IS_VARARGS(md->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Constructor?   ", indent, " ");
    if (XPT_MD_IS_CTOR(md->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Hidden?        ", indent, " ");
    if (XPT_MD_IS_HIDDEN(md->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*s# of arguments:   %d\n", indent, " ", md->num_args);
    fprintf(stdout, "%*sParameter Descriptors:\n", indent, " ");

    for (i=0; i<md->num_args; i++) {
        fprintf(stdout, "%*sParameter #%d:\n", new_indent, " ", i);
        
        if (!XPT_VDumpParamDescriptor(&md->params[i], more_indent))
            return PR_FALSE;
    }
   
    fprintf(stdout, "%*sResult:\n", indent, " ");
    if (!XPT_VDumpParamDescriptor(md->result, new_indent))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpXPTString(XPTString *str)
{
    int i;
    for (i=0; i<str->length; i++) {
        fprintf(stdout, "%c", str->bytes[i]);
    }
    return PR_TRUE;
}

PRBool
XPT_VDumpParamDescriptor(XPTParamDescriptor *pd, const int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stdout, "%*sIn Param?   ", indent, " ");
    if (XPT_PD_IS_IN(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");
    
    fprintf(stdout, "%*sOut Param?  ", indent, " ");
    if (XPT_PD_IS_OUT(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");
    
    fprintf(stdout, "%*sRetval?     ", indent, " ");
    if (XPT_PD_IS_RETVAL(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sType Descriptor:\n", indent, " ");
    if (!XPT_VDumpTypeDescriptor(&pd->type, new_indent))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_VDumpTypeDescriptor(XPTTypeDescriptor *td, int indent)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stdout, "%*sIs Pointer?        ", indent, " ");
    if (XPT_TDP_IS_POINTER(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Unique Pointer? ", indent, " ");
    if (XPT_TDP_IS_UNIQUE_POINTER(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Reference?      ", indent, " ");
    if (XPT_TDP_IS_REFERENCE(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sTag:               %d\n", indent, " ", 
            XPT_TDP_TAG(td->prefix));
    
    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of IDE:             %d\n", new_indent, " ", 
                td->type.interface);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of Method Argument: %d\n", new_indent, " ", 
                td->type.argnum);        
    }

    return PR_TRUE;
}

PRBool
XPT_VDumpConstDescriptor(XPTConstDescriptor *cd, const int indent)
{
    int new_indent = indent + BASE_INDENT;
    char *out;

    fprintf(stdout, "%*sName:   %s\n", indent, " ", cd->name);
    fprintf(stdout, "%*sType Descriptor: \n", indent, " ");
    if (!XPT_VDumpTypeDescriptor(&cd->type, new_indent))
        return PR_FALSE;
    fprintf(stdout, "%*sValue:  ", indent, " ");

    switch(XPT_TDP_TAG(cd->type.prefix)) {
    case TD_INT8:
        fprintf(stdout, "%d\n", cd->value.i8);
        break;
    case TD_INT16:
        fprintf(stdout, "%d\n", cd->value.i16);
        break;
    case TD_INT32:
        fprintf(stdout, "%d\n", cd->value.i32);
        break;
    case TD_INT64:
        out = PR_smprintf("%lld\n", cd->value.i64);
        fputs(out, stdout);
        PR_smprintf_free(out);
        break;
    case TD_UINT8:
        fprintf(stdout, "%d\n", cd->value.ui8);
        break;
    case TD_UINT16:
        fprintf(stdout, "%d\n", cd->value.ui16);
        break;
    case TD_UINT32:
        fprintf(stdout, "%d\n", cd->value.ui32);
        break;
    case TD_UINT64:
        out = PR_smprintf("%lld\n", cd->value.ui64);
        fputs(out, stdout);
        PR_smprintf_free(out);
        break;
    case TD_FLOAT:
        fprintf(stdout, "%f\n", cd->value.flt);
        break;
    case TD_DOUBLE:
        fprintf(stdout, "%g\n", cd->value.dbl);
        break;
    case TD_BOOL:
        if (cd->value.bul)
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
            break;
    case TD_CHAR:
        fprintf(stdout, "%c\n", cd->value.ch);
        break;
    case TD_WCHAR:
        fprintf(stdout, "%d\n", cd->value.wch);
        break;
    case TD_VOID:
        fprintf(stdout, "VOID\n");
        break;
    case TD_PPNSIID:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
        fprintf(stdout, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
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
            fprintf(stdout, "\n");
        } else 
            return PR_FALSE;
        break;            
    case TD_PSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            fprintf(stdout, "%s\n", cd->value.str);
        } else 
            return PR_FALSE;
        break;
    case TD_PWSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            PRUint16 *ch = cd->value.wstr;
            while (*ch) {
                fprintf(stderr, "%c", *ch & 0xff);
                ch++;
            }
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
