/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A utility for dumping the contents of a typelib file (.xpt) to screen 
 */ 

#include "xpt_xdr.h"
#include <stdio.h>
#ifdef XP_MAC
#include <stat.h>
#include <StandardFile.h>
#include "FullPath.h"
#else
#ifdef XP_OS2_EMX
#include <sys/types.h>
#endif
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "prprf.h"

#define BASE_INDENT 3

static char *type_array[32] = 
            {"int8",        "int16",       "int32",       "int64",
             "uint8",       "uint16",      "uint32",      "uint64",
             "float",       "double",      "boolean",     "char",
             "wchar_t",     "void",        "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved"};

static char *ptype_array[32] = 
            {"int8 *",      "int16 *",     "int32 *",     "int64 *",
             "uint8 *",     "uint16 *",    "uint32 *",    "uint64 *",
             "float *",     "double *",    "boolean *",   "char *",
             "wchar_t *",   "void *",      "nsIID *",     "DOMString *",
             "string",      "wstring",     "Interface *", "InterfaceIs *",
             "array",       "string_s",    "wstring_s",   "reserved",
             "reserved",    "reserved",    "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved"};

static char *rtype_array[32] = 
            {"int8 &",      "int16 &",     "int32 &",     "int64 &",
             "uint8 &",     "uint16 &",    "uint32 &",    "uint64 &",
             "float &",     "double &",    "boolean &",   "char &",
             "wchar_t &",   "void &",      "nsIID &",     "DOMString &",
             "string &",    "wstring &",   "Interface &", "InterfaceIs &",
             "array &",     "string_s &",  "wstring_s &", "reserved",
             "reserved",    "reserved",    "reserved",    "reserved",
             "reserved",    "reserved",    "reserved",    "reserved"};

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
                         XPTInterfaceDescriptor *id,
                         const int indent, PRBool verbose_mode);
PRBool
XPT_GetStringForType(XPTHeader *header, XPTTypeDescriptor *td,
                     XPTInterfaceDescriptor *id,
                     char **type_string);

PRBool
XPT_DumpXPTString(XPTString *str);

PRBool
XPT_DumpParamDescriptor(XPTHeader *header, XPTParamDescriptor *pd,
                        XPTInterfaceDescriptor *id,
                        const int indent, PRBool verbose_mode,
                        PRBool is_result);

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td, 
                       XPTInterfaceDescriptor *id,
                       int indent, PRBool verbose_mode);

PRBool
XPT_DumpConstDescriptor(XPTHeader *header, XPTConstDescriptor *cd,
                        XPTInterfaceDescriptor *id,
                        const int indent, PRBool verbose_mode);

static void
xpt_dump_usage(char *argv[]) {
    fprintf(stdout, "Usage: %s [-v] <filename.xpt>\n"
            "       -v verbose mode\n", argv[0]);
}

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)

#define main xptdump_main
int xptdump_main(int argc, char *argv[]);

#define get_file_length mac_get_file_length
extern size_t mac_get_file_length(const char* filename);

#else /* !(XP_MAC && XPIDL_PLUGIN) */

static size_t get_file_length(const char* filename)
{
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        perror("FAILED: get_file_length");
        exit(1);
    }
    return file_stat.st_size;
}

#endif /* !(XP_MAC && XPIDL_PLUGIN) */

int 
main(int argc, char **argv)
{
    PRBool verbose_mode = PR_FALSE;
    XPTArena *arena;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    size_t flen;
    char *name;
    char *whole;
    FILE *in;
    int result = 1;

    switch (argc) {
    case 2:
        if (argv[1][0] == '-') {
            xpt_dump_usage(argv);
            return 1;
        }
        name = argv[1];
        flen = get_file_length(name);
        in = fopen(name, "rb");
        break;
    case 3:
        verbose_mode = PR_TRUE;
        if (argv[1][0] != '-' || argv[1][1] != 'v') {
            xpt_dump_usage(argv);
            return 1;
        }
        name = argv[2];
        flen = get_file_length(name);
        in = fopen(name, "rb");
        break;
    default:
        xpt_dump_usage(argv);
        return 1;
    }

    if (!in) {
        perror("FAILED: fopen");
        return 1;
    }

    arena = XPT_NewArena(1024, sizeof(double), "main xpt_dump arena");
    if (!arena) {
        perror("XPT_NewArena failed");
        return 1;
    }

    /* after arena creation all exits via 'goto out' */

    whole = XPT_MALLOC(arena, flen);
    if (!whole) {
        perror("FAILED: XPT_MALLOC for whole");
        goto out;
    }

    if (flen > 0) {
        size_t rv = fread(whole, 1, flen, in);
        if (rv < flen) {
            fprintf(stderr, "short read (%d vs %d)! ouch!\n", rv, flen);
            goto out;
        }
        if (ferror(in) != 0 || fclose(in) != 0)
            perror("FAILED: Unable to read typelib file.\n");

        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            fprintf(stdout, "XPT_MakeCursor failed for %s\n", name);
            goto out;
        }
        if (!XPT_DoHeader(arena, cursor, &header)) {
            fprintf(stdout,
                    "DoHeader failed for %s.  Is %s a valid .xpt file?\n",
                    name, name);
            goto out;
        }

        if (!XPT_DumpHeader(cursor, header, BASE_INDENT, verbose_mode)) {
            perror("FAILED: XPT_DumpHeader");
            goto out;
        }
   
        if (param_problems) {
            fprintf(stdout, "\nWARNING: ParamDescriptors are present with "
                    "bad in/out/retval flag information.\n"
                    "These have been marked with 'XXX'.\n"
                    "Remember, retval params should always be marked as out!\n");
        }

        XPT_DestroyXDRState(state);
        XPT_FREE(arena, whole);

    } else {
        fclose(in);
        perror("FAILED: file length <= 0");
        goto out;
    }

    result = 0;

out:
    XPT_DestroyArena(arena);
    return result;
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
        if (strncmp((const char*)header->magic, XPT_MAGIC, 16) == 0)
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

    if (id->parent_interface) {

        parent_ide = &header->interface_directory[id->parent_interface - 1];

        fprintf(stdout, "%*sParent: %s::%s\n", indent, " ", 
                parent_ide->name_space ? 
                parent_ide->name_space : "", 
                parent_ide->name);
    }

    fprintf(stdout, "%*sFlags:\n", indent, " ");

    fprintf(stdout, "%*sScriptable: %s\n", new_indent, " ", 
            XPT_ID_IS_SCRIPTABLE(id->flags) ? "TRUE" : "FALSE");

    fprintf(stdout, "%*sFunction: %s\n", new_indent, " ", 
            XPT_ID_IS_FUNCTION(id->flags) ? "TRUE" : "FALSE");

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
                                              &id->method_descriptors[i], id,
                                              more_indent, verbose_mode)) {
                    return PR_FALSE;
                } 
            } else { 
                if (!XPT_DumpMethodDescriptor(header,
                                              &id->method_descriptors[i], id,
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
                                             &id->const_descriptors[i], id,
                                             more_indent, verbose_mode))
                return PR_FALSE;
            } else {
                if (!XPT_DumpConstDescriptor(header, 
                                             &id->const_descriptors[i], id,
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
                         XPTInterfaceDescriptor *id,
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
        
        fprintf(stdout, "%*sIs NotXPCOM?      ", indent, " ");
        if (XPT_MD_IS_NOTXPCOM(md->flags))
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
            
            if (!XPT_DumpParamDescriptor(header, &md->params[i], id, 
                                         more_indent, verbose_mode, PR_FALSE))
                return PR_FALSE;
        }
   
        fprintf(stdout, "%*sResult:\n", indent, " ");
        if (!XPT_DumpParamDescriptor(header, md->result, id, new_indent,
                                     verbose_mode, PR_TRUE)) {
            return PR_FALSE;
        }
    } else {
        char *param_type;
        XPTParamDescriptor *pd;

        if (!XPT_GetStringForType(header, &md->result->type, id, &param_type)) {
            return PR_FALSE;
        }
        fprintf(stdout, "%*s%c%c%c%c%c %s %s(", indent - 6, " ",
                XPT_MD_IS_GETTER(md->flags) ? 'G' : ' ',
                XPT_MD_IS_SETTER(md->flags) ? 'S' : ' ',
                XPT_MD_IS_HIDDEN(md->flags) ? 'H' : ' ',
                XPT_MD_IS_NOTXPCOM(md->flags) ? 'N' : ' ',
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
                    fprintf(stdout, "out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                    if (XPT_PD_IS_SHARED(pd->flags)) {
                        fprintf(stdout, "shared ");
                    }
                } else {
                    fprintf(stdout, " ");
                    if (XPT_PD_IS_DIPPER(pd->flags)) {
                        fprintf(stdout, "dipper ");
                    }
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                }
            } else {
                if (XPT_PD_IS_OUT(pd->flags)) {
                    fprintf(stdout, "out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                    if (XPT_PD_IS_SHARED(pd->flags)) {
                        fprintf(stdout, "shared ");
                    }
                } else {
                    param_problems = PR_TRUE;
                    fprintf(stdout, "XXX ");
                }
            }
            if (!XPT_GetStringForType(header, &pd->type, id, &param_type)) {
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
                     XPTInterfaceDescriptor *id,
                     char **type_string)
{
    static char buf[128]; /* ugly non-reentrant use of static buffer! */
    PRBool isArray = PR_FALSE;

    int tag = XPT_TDP_TAG(td->prefix);
    
    if (tag == TD_ARRAY) {
        isArray = PR_TRUE;
        td = &id->additional_types[td->type.additional_type];
        tag = XPT_TDP_TAG(td->prefix);
    }
    
    if (tag == TD_INTERFACE_TYPE) {
        int idx = td->type.iface;
        if (!idx || idx > header->num_interfaces)
            *type_string = "UNKNOWN_INTERFACE";
        else
            *type_string = header->interface_directory[idx-1].name;
    } else if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
        if (XPT_TDP_IS_REFERENCE(td->prefix.flags))
            *type_string = rtype_array[tag];
        else
            *type_string = ptype_array[tag];
    } else {
        *type_string = type_array[tag];
    }

    if(isArray) {
        sprintf(buf, "%s []", *type_string);
        *type_string = buf;
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
                        XPTInterfaceDescriptor *id,
                        const int indent, PRBool verbose_mode, 
                        PRBool is_result)
{
    int new_indent = indent + BASE_INDENT;
    
    if (!XPT_PD_IS_IN(pd->flags) && 
        !XPT_PD_IS_OUT(pd->flags) &&
        (XPT_PD_IS_RETVAL(pd->flags) ||
         XPT_PD_IS_SHARED(pd->flags))) {
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

    fprintf(stdout, "%*sShared?     ", indent, " ");
    if (XPT_PD_IS_SHARED(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sDipper?     ", indent, " ");
    if (XPT_PD_IS_DIPPER(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");


    fprintf(stdout, "%*sType Descriptor:\n", indent, " ");
    if (!XPT_DumpTypeDescriptor(&pd->type, id, new_indent, verbose_mode))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td, 
                       XPTInterfaceDescriptor *id,
                       int indent, PRBool verbose_mode)
{
    int new_indent;

    if (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
        fprintf(stdout, "%*sArray (size in arg %d and length in arg %d) of...\n", 
            indent, " ", td->argnum, td->argnum2);
        td = &id->additional_types[td->type.additional_type];
        indent += BASE_INDENT;
    }

    new_indent = indent + BASE_INDENT;

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
    
    if (XPT_TDP_TAG(td->prefix) == TD_PSTRING_SIZE_IS ||
        XPT_TDP_TAG(td->prefix) == TD_PWSTRING_SIZE_IS) {
        fprintf(stdout, "%*s - size in arg %d and length in arg %d\n", 
            indent, " ", td->argnum, td->argnum2);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of IDE:             %d\n", new_indent, " ", 
                td->type.iface);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of Method Argument: %d\n", new_indent, " ", 
                td->argnum);        
    }

    return PR_TRUE;
}

PRBool
XPT_DumpConstDescriptor(XPTHeader *header, XPTConstDescriptor *cd,
                        XPTInterfaceDescriptor *id,
                        const int indent, PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;
    char *const_type;
/*      char *out; */
    PRUint32 uintout;
    PRInt32 intout;

    if (verbose_mode) {
        fprintf(stdout, "%*sName:   %s\n", indent, " ", cd->name);
        fprintf(stdout, "%*sType Descriptor: \n", indent, " ");
        if (!XPT_DumpTypeDescriptor(&cd->type, id, new_indent, verbose_mode))
            return PR_FALSE;
        fprintf(stdout, "%*sValue:  ", indent, " ");
    } else {
        if (!XPT_GetStringForType(header, &cd->type, id, &const_type)) {
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
    case TD_INT64:
        /* XXX punt for now to remove NSPR linkage...
         * borrow from mozilla/nsprpub/pr/src/io/prprf.c::cvt_ll? */

/*          out = PR_smprintf("%lld", cd->value.i64); */
/*          fputs(out, stdout); */
/*          PR_smprintf_free(out); */
        LL_L2I(intout, cd->value.i64);
        fprintf(stdout, "%d", intout);
        break;
    case TD_INT32:
        fprintf(stdout, "%d", cd->value.i32);
        break;
    case TD_UINT8:
        fprintf(stdout, "%u", cd->value.ui8);
        break;
    case TD_UINT16:
        fprintf(stdout, "%u", cd->value.ui16);
        break;
    case TD_UINT64:
/*          out = PR_smprintf("%lld", cd->value.ui64); */
/*          fputs(out, stdout); */
/*          PR_smprintf_free(out); */
        /* XXX punt for now to remove NSPR linkage. */
        LL_L2UI(uintout, cd->value.ui64);
        fprintf(stdout, "%u", uintout);
        break;
    case TD_UINT32:
        fprintf(stdout, "%u", cd->value.ui32);
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
