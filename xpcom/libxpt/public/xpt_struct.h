/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Structures matching the in-memory representation of typelib structures.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef __xpt_struct_h__
#define __xpt_struct_h__

#include "prtypes.h"

/*
 * Originally, I was going to have structures that exactly matched the on-disk
 * representation, but that proved difficult: different compilers can pack
 * their structs differently, and that makes overlaying them atop a
 * read-from-disk byte buffer troublesome.  So now I just have some structures
 * that are used in memory, and we're going to write a nice XDR library to
 * write them to disk and stuff.  It is pure joy. -- shaver
 */

/* Structures for the typelib components */

typedef struct XPTHeader XPTHeader;
typedef struct XPTAnnotation XPTAnnotation;
typedef struct XPTInterfaceDirectoryEntry XPTInterfaceDirectoryEntry;
typedef struct XPTInterfaceDescriptor XPTInterfaceDescriptor;
typedef struct XPTConstDescriptor XPTConstDescriptor;
typedef struct XPTMethodDescriptor XPTMethodDescriptor;
typedef struct XPTParamDescriptor XPTParamDescriptor;
typedef struct XPTTypeDescriptor XPTTypeDescriptor;
typedef struct XPTTypeDescriptorPrefix XPTTypeDescriptorPrefix;
typedef struct XPTString XPTString;
typedef struct XPTAnnotation XPTAnnotation;
typedef struct XPTAnnotationPrefix XPTAnnotationPrefix;
typedef struct XPTPrivateAnnotation XPTPrivateAnnotation;

struct XPTHeader {
    char                        magic[16];
    uint8                       major_version;
    uint8                       minor_version;
    uint16                      num_interfaces;
    uint32                      file_length;
    XPTInterfaceDirectoryEntry  *interface_directory;
    uint8                       *data_pool;
    XPTAnnotation               *annotations;
};

struct XPTInterfaceDirectoryEntry {
    uint128                iid;
    char                   *name;
    char                   *namespace;    
    XPTInterfaceDescriptor *interface_descriptor;
};

struct XPTInterfaceDescriptor {
    XPTInterfaceDescriptorEntry *parent_interface;
    uint16                      num_methods;
    XPTMethodDescriptor         *method_descriptors;
    uint16                      num_constants;
    XPTConstDescriptor          *const_descriptors;
};

/*
 * A ConstDescriptor is a variable-size record that records the name and 
 * value of a scoped interface constant. 
 *
 * The types of the method parameter are restricted to the following subset 
 * of TypeDescriptors: 
 *
 * int8, uint8, int16, uint16, int32, uint32, 
 * int64, uint64, wchar_t, char, string
 * 
 * The type (and thus the size) of the value record is determined by the 
 * contents of the associated TypeDescriptor record. For instance, if type 
 * corresponds to int16, then value is a two-byte record consisting of a 
 * 16-bit signed integer.  For a ConstDescriptor type of string, the value 
 * record is of type String*, i.e. an offset within the data pool to a 
 * String record containing the constant string.
 */
struct XPTConstDescriptor {
    char                *name;
    XPTTypeDescriptor   type;
    union {
        int8      i8;
        uint8     ui8; 
        int16     i16; 
        uint16    ui16;
        int32     i32; 
        uint32    ui32;
        int64     i64; 
        uint64    ui64; 
        uint16    wch;
        char      ch; 
        XPTstring *string;
    } value; /* varies according to type */
};

struct XPTMethodDescriptor {
    uint8               is_getter:1, is_setter:1, is_varargs:1,
                        is_constructor:1, is_hidden:1, reserved:3;
    char                *name;
    uint8               num_args;
    XPTParamDescriptor  *params;
    XPTParamDescriptor  *result;
};

struct XPTParamDescriptor {
    uint8             in:1, out:1, retval:1, reserved:5;
    XPTTypeDescriptor type;
};

/* 
 * A TypeDescriptor is a variable-size record used to identify the type of a 
 * method argument or return value. 
 *
 * There are three types of TypeDescriptors:
 *
 * SimpleTypeDescriptor
 * InterfaceTypeDescriptor
 * InterfaceIsTypeDescriptor
 *
 * The tag field in the prefix indicates which of the variant TypeDescriptor 
 * records is being used, and hence the way any remaining fields should be 
 * parsed. Values from 0 to 17 refer to SimpleTypeDescriptors. The value 18 
 * designates an InterfaceTypeDescriptor, while 19 represents an 
 * InterfaceIsTypeDescriptor.
 */
#define TD_INTERFACE_TYPE    18
#define TD_INTERFACE_IS_TYPE 19

struct XPTTypeDescriptorPrefix {
    uint8 is_pointer:1, is_unique_pointer:1, is_reference:1,
          tag:5;
};

struct XPTTypeDescriptor {
    XPTTypeDescriptorPrefix prefix;
    union {
        XPTInterfaceDirectoryEntry *interface;
        uint8                      argnum;
    } type;
}

struct XPTString {
    uint16 length;
    char   bytes[];
};

/*
 * Annotation records are variable-size records used to store secondary 
 * information about the typelib, e.g. such as the name of the tool that 
 * generated the typelib file, the date it was generated, etc.  The 
 * information is stored with very loose format requirements so as to 
 * allow virtually any private data to be stored in the typelib.
 *
 * There are two types of Annotations:
 *
 * EmptyAnnotation
 * PrivateAnnotation
 *
 * The tag field of the prefix discriminates among the variant record 
 * types for Annotation's.  If the tag is 0, this record is an 
 * EmptyAnnotation. EmptyAnnotation's are ignored - they're only used to 
 * indicate an array of Annotation's that's completely empty.  If the tag 
 * is 1, the record is a PrivateAnnotation. 
 */
#define EMPTY_ANNOTATION 0
#define PRIVATE_ANNOTATION 1

struct XPTAnnotationPrefix {
    uint8 is_last:1, tag:7;
};

struct XPTPrivateAnnotation {
    XPTString *creator;
    XPTString *private_data;
};

struct XPTAnnotation {
    XPTAnnotationPrefix prefix;
    XPTPrivateAnnotation private;
};

#endif /* __xpt_struct_h__ */
