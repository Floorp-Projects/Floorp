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

/* Test the structure creation and serialization APIs from xpt_struct.c */

#include "xpt_xdr.h"
#include "xpt_struct.h"

#define PASS(msg)							      \
  fprintf(stderr, "PASSED : %s\n", msg);

#define FAIL(msg)							      \
  fprintf(stderr, "FAILURE: %s\n", msg);

#define TRY_(msg, cond, silent)						      \
  if ((cond) && !silent) {						      \
    PASS(msg);								      \
  } else {								      \
    FAIL(msg);								      \
    return 1;								      \
  }

#define TRY(msg, cond)		TRY_(msg, cond, 0)
#define TRY_Q(msg, cond)	TRY_(msg, cond, 1);

#define INDENT        "   "
#define DOUBLE_INDENT "      "
#define TRIPLE_INDENT "         "
#define QUAD_INDENT   "            "
#define QUINT_INDENT  "               "
#define SEXT_INDENT   "                  "
#define SEPT_INDENT   "                     "
#define OCT_INDENT    "                        "
#define NONA_INDENT   "                           "
#define DECA_INDENT   "                              "

struct nsID iid = {
    0x00112233,
    0x4455,
    0x6677,
    {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}
};

XPTTypeDescriptor td_void = { TD_VOID };

int
main(int argc, char **argv)
{
    XPTHeader *header;
    XPTAnnotation *ann;
    XPTInterfaceDescriptor *id;
    XPTMethodDescriptor *meth;

    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    char *data, *head;
    FILE *out;
    uint32 len, header_sz;

    PRBool ok;
    
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <filename.xpt>\n"
		"       Creates a simple typelib file.\n", argv[0]);
	
	return 1;
    }

    /* construct a header */
    header = XPT_NewHeader(1);
    TRY("NewHeader", header);

    
    ann = XPT_NewAnnotation(XPT_ANN_LAST | XPT_ANN_PRIVATE,
                            XPT_NewStringZ("SimpleTypeLib 1.0"),
                            XPT_NewStringZ("See You In Rome"));
    TRY("NewAnnotation", ann);
    header->annotations = ann;

    header_sz = XPT_SizeOfHeaderBlock(header);

    id = XPT_NewInterfaceDescriptor(0xdead, 2, 2);
    TRY("NewInterfaceDescriptor", id);
    
    ok = XPT_FillInterfaceDirectoryEntry(header->interface_directory, &iid,
					 "Interface", "NS", id);
    TRY("FillInterfaceDirectoryEntry", ok);

    /* void method1(void) */
    meth = &id->method_descriptors[0];
    ok = XPT_FillMethodDescriptor(meth, 0, "method1", 0);
    TRY("FillMethodDescriptor", ok);
    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_VOID;

    /* wstring method2(in uint32, in bool) */
    meth = &id->method_descriptors[1];
    ok = XPT_FillMethodDescriptor(meth, 0, "method2", 2);
    TRY("FillMethodDescriptor", ok);

    meth->result->flags = 0;
    meth->result->type.prefix.flags = TD_PBSTR | XPT_TDP_POINTER;
    meth->params[0].type.prefix.flags = TD_UINT32;
    meth->params[0].flags = XPT_PD_IN;
    meth->params[1].type.prefix.flags = TD_BOOL;
    meth->params[1].flags = XPT_PD_IN;

    /* const one = 1; */
    id->const_descriptors[0].name = "one";
    id->const_descriptors[0].type.prefix.flags = TD_UINT16;
    id->const_descriptors[0].value.ui16 = 1;
    
    /* const squeamish = "ossifrage"; */
    id->const_descriptors[1].name = "squeamish";
    id->const_descriptors[1].type.prefix.flags = TD_PBSTR | XPT_TDP_POINTER;
    id->const_descriptors[1].value.string = XPT_NewStringZ("ossifrage");

    /* serialize it */
    state = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    TRY("NewState (ENCODE)", state);
    
    XPT_SetDataOffset(state, header_sz);
    
    ok = XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor);
    TRY("MakeCursor", ok);

    ok = XPT_DoHeader(cursor, &header);
    TRY("DoHeader", ok);

    /*    out = fopen(argv[1], "w");
    if (!out) {
        perror("FAILED: fopen");
        return 1;
    }
    
    XPT_GetXDRData(state, XPT_HEADER, &head, &len);
    fwrite(head, len, 1, out);

    XPT_GetXDRData(state, XPT_DATA, &data, &len);
    fwrite(data, len, 1, out);
    fclose(out);
    XPT_DestroyXDRState(state);
    */
    fprintf(stderr, "\n'%s' Header:\n", argv[1]);
    XPT_DumpHeader(header);

    /* XXX DestroyHeader */
    return 0;
}

PRBool
XPT_DumpHeader(XPTHeader *header) 
{
    fprintf(stderr, "%sMagic beans:      %s\n", INDENT, header->magic);
    fprintf(stderr, "%sMajor version:    %d\n", INDENT, header->major_version);
    fprintf(stderr, "%sMinor version:    %d\n", INDENT, header->minor_version);
    fprintf(stderr, "%s# of interfaces:  %d\n", INDENT, 
            header->num_interfaces);
    fprintf(stderr, "%sFile length:      %d\n", INDENT, header->file_length);
    fprintf(stderr, "%sData pool offset: %d\n", INDENT, header->data_pool);

    if (!XPT_DumpAnnotations(header->annotations))
        return PR_FALSE;
    
    if (!XPT_DumpIDEs(header->interface_directory, header->num_interfaces))
        return PR_FALSE;

    return PR_TRUE;
}    

PRBool
XPT_DumpAnnotations(XPTAnnotation *ann) 
{
    int i = -1, j = 0;
    XPTAnnotation *last;

    fprintf(stderr, "\n%sAnnotations:\n", INDENT);

    do {
        i++;
        if (XPT_ANN_IS_PRIVATE(ann->flags)) {
            fprintf(stderr, "%sAnnotation #%d is private.\n", 
                    DOUBLE_INDENT, i);
            fprintf(stderr, "%sCreator:      ", TRIPLE_INDENT);
            if (!XPT_DumpXPTString(ann->creator))
                return PR_FALSE;
            fprintf(stderr, "\n");
            fprintf(stderr, "%sPrivate Data: ", TRIPLE_INDENT);            
            if (!XPT_DumpXPTString(ann->private_data))
                return PR_FALSE;
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "%sAnnotation #%d is empty.\n", 
                    DOUBLE_INDENT, i);
        }
        last = ann;
        ann = ann->next;
    } while (!XPT_ANN_IS_LAST(last->flags));
        
    fprintf(stderr, "%sAnnotation #%d is the last annotation.\n", 
            DOUBLE_INDENT, i);
    
    return PR_TRUE;
}    

PRBool
XPT_DumpIDEs(XPTInterfaceDirectoryEntry *ide, int num_interfaces)
{
    int i;
    
    fprintf(stderr, "\n%sInterface Directory:\n", INDENT);
    for (i=0; i<num_interfaces; i++) {
        fprintf(stderr, "%sInterface #%d:\n", DOUBLE_INDENT, i);
        fprintf(stderr, "%sIID:                             "
                "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                TRIPLE_INDENT,
                ide->iid.m0, (PRUint32) ide->iid.m1,(PRUint32) ide->iid.m2,
                (PRUint32) ide->iid.m3[0], (PRUint32) ide->iid.m3[1],
                (PRUint32) ide->iid.m3[2], (PRUint32) ide->iid.m3[3],
                (PRUint32) ide->iid.m3[4], (PRUint32) ide->iid.m3[5],
                (PRUint32) ide->iid.m3[6], (PRUint32) ide->iid.m3[7]);

        fprintf(stderr, "%sName:                            %s\n", 
                TRIPLE_INDENT, ide->name);
        fprintf(stderr, "%sNamespace:                       %s\n", 
                TRIPLE_INDENT, ide->namespace);
        fprintf(stderr, "%sAddress of interface descriptor: %p\n", 
                TRIPLE_INDENT, ide->interface_descriptor);
        if (!XPT_DumpInterfaceDescriptor(ide->interface_descriptor))
            return PR_FALSE;
    }
    return PR_TRUE;
}    

PRBool
XPT_DumpInterfaceDescriptor(XPTInterfaceDescriptor *id)
{
    int i;

    fprintf(stderr, "%sDescriptor:\n", TRIPLE_INDENT);
    fprintf(stderr, "%sOffset of parent interface (in data pool): %d\n", 
            QUAD_INDENT, id->parent_interface);
    fprintf(stderr, "%s# of Method Descriptors:                   %d\n", 
            QUAD_INDENT, id->num_methods);
    for (i=0; i<id->num_methods; i++) {
        fprintf(stderr, "%sMethod #%d:\n", QUINT_INDENT, i);
        if (!XPT_DumpMethodDescriptor(&id->method_descriptors[i]))
            return PR_FALSE;
    }
    fprintf(stderr, "%s# of Constant Descriptors:                  %d\n", 
            QUAD_INDENT, id->num_constants);
    for (i=0; i<id->num_constants; i++) {
        fprintf(stderr, "%sConstant #%d:\n", QUINT_INDENT, i);
        if (!XPT_DumpConstDescriptor(&id->const_descriptors[i]))
            return PR_FALSE;
    }
    
    return PR_TRUE;
}

PRBool
XPT_DumpMethodDescriptor(XPTMethodDescriptor *md)
{
    int i;

    fprintf(stderr, "%sName:             %s\n", SEXT_INDENT, md->name);
    fprintf(stderr, "%sIs Getter?        ", SEXT_INDENT);
    if (XPT_MD_IS_GETTER(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Setter?        ", SEXT_INDENT);
    if (XPT_MD_IS_SETTER(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Varargs?       ", SEXT_INDENT);
    if (XPT_MD_IS_VARARGS(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Constructor?   ", SEXT_INDENT);
    if (XPT_MD_IS_CTOR(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Hidden?        ", SEXT_INDENT);
    if (XPT_MD_IS_HIDDEN(md->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%s# of arguments:   %d\n", SEXT_INDENT, md->num_args);
    fprintf(stderr, "%sParameter Descriptors:\n", SEXT_INDENT);

    for (i=0; i<md->num_args; i++) {
        fprintf(stderr, "%sParameter #%d:\n", SEPT_INDENT, i);
        
        if (!XPT_DumpParamDescriptor(&md->params[i]))
            return PR_FALSE;
    }
   
    fprintf(stderr, "%sResult:\n", SEXT_INDENT);
    if (!XPT_DumpParamDescriptor(md->result))
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
XPT_DumpParamDescriptor(XPTParamDescriptor *pd)
{
    fprintf(stderr, "%sIn Param?   ", OCT_INDENT);
    if (XPT_PD_IS_IN(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");
    
    fprintf(stderr, "%sOut Param?  ", OCT_INDENT);
    if (XPT_PD_IS_OUT(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");
    
    fprintf(stderr, "%sRetval?     ", OCT_INDENT);
    if (XPT_PD_IS_RETVAL(pd->flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sType Descriptor:\n", OCT_INDENT);
    if (!XPT_DumpTypeDescriptor(&pd->type))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
XPT_DumpTypeDescriptor(XPTTypeDescriptor *td)
{
    fprintf(stderr, "%sIs Pointer?        ", NONA_INDENT);
    if (XPT_TDP_IS_POINTER(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Unique Pointer? ", NONA_INDENT);
    if (XPT_TDP_IS_UNIQUE_POINTER(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sIs Reference?      ", NONA_INDENT);
    if (XPT_TDP_IS_REFERENCE(td->prefix.flags))
        fprintf(stderr, "TRUE\n");
    else 
        fprintf(stderr, "FALSE\n");

    fprintf(stderr, "%sTag:               %d\n", NONA_INDENT, 
            XPT_TDP_TAG(td->prefix));
    
    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        fprintf(stderr, "%sInterfaceTypeDescriptor:\n", NONA_INDENT); 
        fprintf(stderr, "%sIndex of IDE:             %d\n", DECA_INDENT, 
                td->type.interface);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
        fprintf(stderr, "%sInterfaceTypeDescriptor:\n", NONA_INDENT); 
        fprintf(stderr, "%sIndex of Method Argument: %d\n", DECA_INDENT, 
                td->type.argnum);        
    }

    return PR_TRUE;
}

PRBool
XPT_DumpConstDescriptor(XPTConstDescriptor *cd)
{
    fprintf(stderr, "%sName:   %s\n", SEXT_INDENT, cd->name);
    fprintf(stderr, "%sType Descriptor: \n", SEXT_INDENT);
    if (!XPT_DumpTypeDescriptor(&cd->type))
        return PR_FALSE;
    fprintf(stderr, "%sValue:  ", SEXT_INDENT);

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

