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
#include <stdlib.h>
#include <string.h>
#include "prprf.h"

#define BASE_INDENT 3

static char *type_array[20] = {"int8", "int16", "int32", "int64",
                               "uint8", "uint16", "uint32", "uint64",
                               "float", "double", "boolean", "char",
                               "wchar_t", "void", "reserved", "reserved",
                               "reserved", "reserved", "Interface *", 
                               "InterfaceIs *"};

static char *ptype_array[18] = {"int8 *", "int16 *", "int32 *", "int64 *",
                                "uint8 *", "uint16 *", "uint32 *", "uint64 *",
                                "float *", "double *", "boolean *", "char *",
                                "wchar_t *", "void *", "nsIID *", "bstr",
                                "string", "wstring"};

PRBool param_problems = PR_FALSE;

PRBool
XPT_DumpHeader(XPTCursor *cursor, XPTHeader *header, 
               const int indent, PRBool verbose_mode);

PRBool
XPT_DumpAnnotations(XPTAnnotation *ann, const int indent, PRBool verbose_mode);

PRBool
XPT_DumpInterfaceDirectoryEntry(XPTCursor *cursor,
                                XPTInterfaceDirectoryEntry *ide, 
                                XPTHeader *header, const int indent,
                                PRBool verbose_mode);

PRBool
XPT_DumpInterfaceDescriptor(XPTCursor *cursor, XPTInterfaceDescriptor *id, 
                            XPTHeader *header, const int indent,
                            PRBool verbose_mode);

PRBool
XPT_DumpMethodDescriptor(XPTHeader *header, XPTMethodDescriptor *md,
                         const int indent, PRBool verbose_mode);
PRBool
XPT_GetStringForType(XPTHeader *header, XPTTypeDescriptor *td,
                     char **type_string);

PRBool
XPT_DumpXPTString(XPTString *str);

PRBool
XPT_DumpParamDescriptor(XPTHeader *header, XPTParamDescriptor *pd,
                        const int indent, PRBool verbose_mode,
                        PRBool is_result);

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td, int indent, 
                       PRBool verbose_mode);

PRBool
XPT_DumpConstDescriptor(XPTHeader *header, XPTConstDescriptor *cd,
                        const int indent, PRBool verbose_mode);

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
    size_t flen;
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
        in = fopen(argv[1], "rb");
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
        in = fopen(argv[2], "rb");
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
        size_t rv = fread(whole, 1, flen, in);
        if (rv < 0) {
            perror("FAILED: reading typelib file");
            return 1;
        }
        if (rv < flen) {
            fprintf(stderr, "short read (%d vs %d)! ouch!\n", rv, flen);
            return 1;
        }
        if (ferror(in) != 0 || fclose(in) != 0)
            perror("FAILED: Unable to read typelib file.\n");
        
        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            fprintf(stdout, "MakeCursor failed\n");
            return 1;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            fprintf(stdout, "DoHeader failed\n");
            return 1;
        }

        if (!XPT_DumpHeader(cursor, header, BASE_INDENT, verbose_mode)) {
            perror("FAILED: XPT_DumpHeader");
            return 1;
        }
   
        if (param_problems) {
            fprintf(stdout, "\nWARNING: ParamDescriptors are present with "
                    "bad in/out/retval flag information.\nThese have been marked "
                    "with 'XXX'.\nRemember, retval params should always be marked as out!\n");
        }

        XPT_DestroyXDRState(state);
        free(whole);

    } else {
        fclose(in);
        perror("FAILED: file length <= 0");
        return 1;
    }

    return 0;
}

PRBool
XPT_DumpHeader(XPTCursor *cursor, XPTHeader *header, 
               const int indent, PRBool verbose_mode) 
{
    int i;
    
    fprintf(stdout, "Header:\n");

    if (verbose_mode) {
        fprintf(stdout, "%*sMagic beans:           ", indent, " ");
        for (i=0; i<16; i++) {
            fprintf(stdout, "%02x", header->magic[i]);
        }
        fprintf(stdout, "\n");
        if (strncmp(header->magic, XPT_MAGIC, 16) == 0)
            fprintf(stdout, "%*s                       PASSED\n", indent, " ");
        else
            fprintf(stdout, "%*s                       FAILED\n", indent, " ");
    }
    fprintf(stdout, "%*sMajor version:         %d\n", indent, " ",
            header->major_version);
    fprintf(stdout, "%*sMinor version:         %d\n", indent, " ",
            header->minor_version);    
    fprintf(stdout, "%*sNumber of interfaces:  %d\n", indent, " ",
            header->num_interfaces);

    if (verbose_mode) {
        fprintf(stdout, "%*sFile length:           %d\n", indent, " ",
                header->file_length);
        fprintf(stdout, "%*sData pool offset:      %d\n\n", indent, " ",
                header->data_pool);
    }
    
    fprintf(stdout, "%*sAnnotations:\n", indent, " ");
    if (!XPT_DumpAnnotations(header->annotations, indent*2, verbose_mode))
        return PR_FALSE;

    fprintf(stdout, "\nInterface Directory:\n");
    for (i=0; i<header->num_interfaces; i++) {
        if (verbose_mode) {
            fprintf(stdout, "%*sInterface #%d:\n", indent, " ", i);
            if (!XPT_DumpInterfaceDirectoryEntry(cursor,
                                                 &header->interface_directory[i],
                                                 header, indent*2,
                                                 verbose_mode)) {
                return PR_FALSE;
            }
        } else {
            if (!XPT_DumpInterfaceDirectoryEntry(cursor,
                                                 &header->interface_directory[i],
                                                 header, indent,
                                                 verbose_mode)) {
                return PR_FALSE;
            }
        }
    }

    return PR_TRUE;
}    

PRBool
XPT_DumpAnnotations(XPTAnnotation *ann, const int indent, PRBool verbose_mode) 
{
    int i = -1;
    XPTAnnotation *last;
    int new_indent = indent + BASE_INDENT;

    do {
        i++;
        if (XPT_ANN_IS_PRIVATE(ann->flags)) {
            if (verbose_mode) {
                fprintf(stdout, "%*sAnnotation #%d is private.\n", 
                        indent, " ", i);
            } else {
                fprintf(stdout, "%*sAnnotation #%d:\n", 
                        indent, " ", i);
            }                
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
        
    if (verbose_mode) {
        fprintf(stdout, "%*sAnnotation #%d is the last annotation.\n", 
                indent, " ", i);
    }

    return PR_TRUE;
}

static void
print_IID(struct nsID *iid, FILE *file)
{
    fprintf(file, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (PRUint32) iid->m0, (PRUint32) iid->m1,(PRUint32) iid->m2,
            (PRUint32) iid->m3[0], (PRUint32) iid->m3[1],
            (PRUint32) iid->m3[2], (PRUint32) iid->m3[3],
            (PRUint32) iid->m3[4], (PRUint32) iid->m3[5],
            (PRUint32) iid->m3[6], (PRUint32) iid->m3[7]);
            
}

PRBool
XPT_DumpInterfaceDirectoryEntry(XPTCursor *cursor, 
                                XPTInterfaceDirectoryEntry *ide, 
                                XPTHeader *header, const int indent,
                                PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;

    if (verbose_mode) {
        fprintf(stdout, "%*sIID:                             ", indent, " ");
        print_IID(&ide->iid, stdout);
        fprintf(stdout, "\n");
        
        fprintf(stdout, "%*sName:                            %s\n", 
                indent, " ", ide->name);
        fprintf(stdout, "%*sNamespace:                       %s\n", 
                indent, " ", ide->name_space ? ide->name_space : "none");
        fprintf(stdout, "%*sAddress of interface descriptor: %p\n", 
                indent, " ", ide->interface_descriptor);

        fprintf(stdout, "%*sDescriptor:\n", indent, " ");
    
        if (!XPT_DumpInterfaceDescriptor(cursor, ide->interface_descriptor, 
                                         header, new_indent, verbose_mode)) {
            return PR_FALSE;
        }
    } else {
        fprintf(stdout, "%*s- %s::%s (", indent, " ", 
                ide->name_space ? ide->name_space : "", ide->name);
        print_IID(&ide->iid, stdout);
        fprintf(stdout, "):\n");
        if (!XPT_DumpInterfaceDescriptor(cursor, ide->interface_descriptor, 
                                         header, new_indent, verbose_mode)) {
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}    

PRBool
XPT_DumpInterfaceDescriptor(XPTCursor *cursor, XPTInterfaceDescriptor *id,
                            XPTHeader *header, const int indent,
                            PRBool verbose_mode)
{
    XPTInterfaceDirectoryEntry *parent_ide;
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    if (!id) {
        fprintf(stdout, "%*s[Unresolved]\n", indent, " ");
        return PR_TRUE;
    }

    if (id->parent_interface && id->parent_interface != 0) {

        parent_ide = &header->interface_directory[id->parent_interface - 1];
        
        if (parent_ide) {
            fprintf(stdout, "%*sParent: %s::%s\n", indent, " ", 
                    parent_ide->name_space ? 
                    parent_ide->name_space : "", 
                    parent_ide->name);
        }
        
    }

    if (verbose_mode) {
        if (id->parent_interface) {
            fprintf(stdout, 
                    "%*sIndex of parent interface (in data pool): %d\n", 
                    indent, " ", id->parent_interface);
            
        }
    } else {
    }   

    if (id->num_methods > 0) {
        if (verbose_mode) {
            fprintf(stdout, 
                    "%*s# of Method Descriptors:                   %d\n", 
                    indent, " ", id->num_methods);
        } else {
            fprintf(stdout, "%*sMethods:\n", indent, " ");
        }

        for (i=0; i<id->num_methods; i++) {
            if (verbose_mode) {
                fprintf(stdout, "%*sMethod #%d:\n", new_indent, " ", i);
                if (!XPT_DumpMethodDescriptor(header,
                                              &id->method_descriptors[i], 
                                              more_indent, verbose_mode)) {
                    return PR_FALSE;
                } 
            } else { 
                if (!XPT_DumpMethodDescriptor(header,
                                              &id->method_descriptors[i], 
                                              new_indent, verbose_mode)) {
                    return PR_FALSE;
                } 
            }
        }        
    } else {
        fprintf(stdout, "%*sMethods:\n", indent, " ");
        fprintf(stdout, "%*sNo Methods\n", new_indent, " ");
    }
    
    if (id->num_constants > 0) {
        if (verbose_mode) {
            fprintf(stdout, 
                    "%*s# of Constant Descriptors:                  %d\n", 
                    indent, " ", id->num_constants);
        } else {
            fprintf(stdout, "%*sConstants:\n", indent, " ");
        }
        
        for (i=0; i<id->num_constants; i++) {
            if (verbose_mode) {
                fprintf(stdout, "%*sConstant #%d:\n", new_indent, " ", i);
                if (!XPT_DumpConstDescriptor(header, 
                                             &id->const_descriptors[i], 
                                             more_indent, verbose_mode))
                return PR_FALSE;
            } else {
                if (!XPT_DumpConstDescriptor(header, 
                                             &id->const_descriptors[i], 
                                             new_indent, verbose_mode)) {
                    return PR_FALSE;
                }
            }
        }
    } else { 
        fprintf(stdout, "%*sConstants:\n", indent, " ");
        fprintf(stdout, "%*sNo Constants\n", new_indent, " ");
    }

    return PR_TRUE;
}

PRBool
XPT_DumpMethodDescriptor(XPTHeader *header, XPTMethodDescriptor *md,
                         const int indent, PRBool verbose_mode)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    if (verbose_mode) {
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
            
            if (!XPT_DumpParamDescriptor(header, &md->params[i], more_indent, 
                                         verbose_mode, PR_FALSE))
                return PR_FALSE;
        }
   
        fprintf(stdout, "%*sResult:\n", indent, " ");
        if (!XPT_DumpParamDescriptor(header, md->result, new_indent,
                                     verbose_mode, PR_TRUE)) {
            return PR_FALSE;
        }
    } else {
        char *param_type;
        XPTParamDescriptor *pd;

        if (!XPT_GetStringForType(header, &md->result->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(stdout, "%*s%c%c%c%c%c %s %s(", indent - 6, " ",
                XPT_MD_IS_GETTER(md->flags) ? 'G' : ' ',
                XPT_MD_IS_SETTER(md->flags) ? 'S' : ' ',
                XPT_MD_IS_HIDDEN(md->flags) ? 'H' : ' ',
                XPT_MD_IS_VARARGS(md->flags) ? 'V' : ' ',
                XPT_MD_IS_CTOR(md->flags) ? 'C' : ' ',
                param_type, md->name);
        for (i=0; i<md->num_args; i++) {
            if (i!=0) {
                fprintf(stdout, ", ");
            }
            pd = &md->params[i];
            if (XPT_PD_IS_IN(pd->flags)) {
                fprintf(stdout, "in");
                if (XPT_PD_IS_OUT(pd->flags)) {
                    fprintf(stdout, "/out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                } else {
                    fprintf(stdout, " ");
                }
            } else {
                if (XPT_PD_IS_OUT(pd->flags)) {
                    fprintf(stdout, "out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                } else {
                    param_problems = PR_TRUE;
                    fprintf(stdout, "XXX ");
                }
            }
            if (!XPT_GetStringForType(header, &pd->type, &param_type)) {
                return PR_FALSE;
            }
            fprintf(stdout, "%s", param_type);
        }
        fprintf(stdout, ");\n");   
    }
    return PR_TRUE;
}
    
PRBool
XPT_GetStringForType(XPTHeader *header, XPTTypeDescriptor *td,
                     char **type_string)
{
    int tag = XPT_TDP_TAG(td->prefix);
    
    if (tag == TD_INTERFACE_TYPE) {
        int index = td->type.interface;
        if (!index || index > header->num_interfaces)
            *type_string = "UNKNOWN_INTERFACE";
        else
            *type_string = header->interface_directory[index-1].name;
    } else if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
        *type_string = ptype_array[tag];
    } else {
        *type_string = type_array[tag];
    }

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
XPT_DumpParamDescriptor(XPTHeader *header, XPTParamDescriptor *pd,
                        const int indent, PRBool verbose_mode, 
                        PRBool is_result)
{
    int new_indent = indent + BASE_INDENT;
    
    if (!XPT_PD_IS_IN(pd->flags) && 
        !XPT_PD_IS_OUT(pd->flags) &&
        XPT_PD_IS_RETVAL(pd->flags)) {
        param_problems = PR_TRUE;
        fprintf(stdout, "XXX\n");
    } else {
        if (!XPT_PD_IS_IN(pd->flags) && !XPT_PD_IS_OUT(pd->flags)) {
            if (is_result) {
                if (XPT_TDP_TAG(pd->type.prefix) != TD_UINT32 &&
                    XPT_TDP_TAG(pd->type.prefix) != TD_VOID) {
                    param_problems = PR_TRUE;
                    fprintf(stdout, "XXX\n");   
                }
            } else {
                param_problems = PR_TRUE;
                fprintf(stdout, "XXX\n");
            }
        }
    }

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
    if (!XPT_DumpTypeDescriptor(&pd->type, new_indent, verbose_mode))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td, int indent, PRBool verbose_mode)
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
XPT_DumpConstDescriptor(XPTHeader *header, XPTConstDescriptor *cd,
                        const int indent, PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;
    char *out, *const_type;

    if (verbose_mode) {
        fprintf(stdout, "%*sName:   %s\n", indent, " ", cd->name);
        fprintf(stdout, "%*sType Descriptor: \n", indent, " ");
        if (!XPT_DumpTypeDescriptor(&cd->type, new_indent, verbose_mode))
            return PR_FALSE;
        fprintf(stdout, "%*sValue:  ", indent, " ");
    } else {
        if (!XPT_GetStringForType(header, &cd->type, &const_type)) {
            return PR_FALSE;
        }
        fprintf(stdout, "%*s%s %s = ", indent, " ", const_type, cd->name);
    }        

    switch(XPT_TDP_TAG(cd->type.prefix)) {
    case TD_INT8:
        fprintf(stdout, "%d", cd->value.i8);
        break;
    case TD_INT16:
        fprintf(stdout, "%d", cd->value.i16);
        break;
    case TD_INT32:
        fprintf(stdout, "%d", cd->value.i32);
        break;
    case TD_INT64:
        out = PR_smprintf("%lld", cd->value.i64);
        fputs(out, stdout);
        PR_smprintf_free(out);
        break;
    case TD_UINT8:
        fprintf(stdout, "%d", cd->value.ui8);
        break;
    case TD_UINT16:
        fprintf(stdout, "%d", cd->value.ui16);
        break;
    case TD_UINT32:
        fprintf(stdout, "%d", cd->value.ui32);
        break;
    case TD_UINT64:
        out = PR_smprintf("%lld", cd->value.ui64);
        fputs(out, stdout);
        PR_smprintf_free(out);
        break;
    case TD_FLOAT:
        fprintf(stdout, "%f", cd->value.flt);
        break;
    case TD_DOUBLE:
        fprintf(stdout, "%g", cd->value.dbl);
        break;
    case TD_BOOL:
        if (cd->value.bul)
            fprintf(stdout, "TRUE");
        else 
            fprintf(stdout, "FALSE");
            break;
    case TD_CHAR:
        fprintf(stdout, "%c", cd->value.ch);
        break;
    case TD_WCHAR:
        fprintf(stdout, "%c", cd->value.wch & 0xff);
        break;
    case TD_VOID:
        fprintf(stdout, "VOID");
        break;
    case TD_PNSIID:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            print_IID(cd->value.iid, stdout);
        } else 
            return PR_FALSE;
        break;
    case TD_PBSTR:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            if (!XPT_DumpXPTString(cd->value.string))
                return PR_FALSE;
        } else 
            return PR_FALSE;
        break;            
    case TD_PSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            fprintf(stdout, "%s", cd->value.str);
        } else 
            return PR_FALSE;
        break;
    case TD_PWSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            PRUint16 *ch = cd->value.wstr;
            while (*ch) {
                fprintf(stdout, "%c", *ch & 0xff);
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
    
    if (verbose_mode) {
        fprintf(stdout, "\n");
    } else {
        fprintf(stdout, ";\n");
    }

    return PR_TRUE;
}
