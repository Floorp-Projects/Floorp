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
#ifndef nsID_h__
/*
 * We can't include nsID.h, because it's full of C++ goop and we're not doing
 * C++ here, so we define our own minimal struct.  We protect against multiple
 * definitions of this struct, though, and use the same field naming.
 */
struct nsID {
    PRUint32 m0;
    PRUint16 m1;
    PRUint16 m2;
    PRUint8  m3[8];
};

typedef struct nsID nsID;
#endif

#define XPT_COPY_IID(to, from)                                                \
  (to).m0 = (from).m0;                                                        \
  (to).m1 = (from).m1;                                                        \
  (to).m2 = (from).m2;                                                        \
  (to).m3[0] = (from).m3[0];                                                  \
  (to).m3[1] = (from).m3[1];                                                  \
  (to).m3[2] = (from).m3[2];                                                  \
  (to).m3[3] = (from).m3[3];


/*
 * Every XPCOM typelib file begins with a header.
 */
struct XPTHeader {
    char                        magic[16];
    uint8                       major_version;
    uint8                       minor_version;
    uint16                      num_interfaces;
    uint32                      file_length;
    XPTInterfaceDirectoryEntry  *interface_directory;
    uint32                      data_pool;
    XPTAnnotation               *annotations;
};

#define XPT_MAGIC "XPCOM\nTypeLib\r\n\032"
#define XPT_MAJOR_VERSION 0x01
#define XPT_MINOR_VERSION 0x00

XPTHeader *
XPT_NewHeader(uint32 num_interfaces);

/* size of header and annotations */
uint32
XPT_SizeOfHeader(XPTHeader *header);

/* size of header and annotations and InterfaceDirectoryEntries */
uint32
XPT_SizeOfHeaderBlock(XPTHeader *header);

/*
 * A contiguous array of fixed-size InterfaceDirectoryEntry records begins at 
 * the byte offset identified by the interface_directory field in the file 
 * header.  The array is used to quickly locate an interface description 
 * using its IID.  No interface should appear more than once in the array.
 */
struct XPTInterfaceDirectoryEntry {
    nsID                   iid;
    char                   *name;
    char                   *namespace;
    XPTInterfaceDescriptor *interface_descriptor;
};

PRBool
XPT_FillInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide,
                                nsID *iid, char *name, char *namespace,
                                XPTInterfaceDescriptor *descriptor);

/*
 * An InterfaceDescriptor is a variable-size record used to describe a 
 * single XPCOM interface, including all of its methods. 
 */
struct XPTInterfaceDescriptor {
    uint32              parent_interface;
    uint16              num_methods;
    XPTMethodDescriptor *method_descriptors;
    uint16              num_constants;
    XPTConstDescriptor  *const_descriptors;
};

PRBool
XPT_IndexForInterface(XPTInterfaceDirectoryEntry *ide_block,
                      uint32 num_interfaces, nsID *iid, uint32 *indexp);

XPTInterfaceDescriptor *
XPT_NewInterfaceDescriptor(uint32 parent_interface, uint32 num_methods,
                           uint32 num_constants);
/*
 * This is our special string struct with a length value associated with it,
 * which means that it can contains embedded NULs.
 */
struct XPTString {
    uint16 length;
    char   *bytes;
};

XPTString *
XPT_NewString(char *bytes, uint16 len);

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

struct XPTTypeDescriptorPrefix {
    uint8 is_pointer:1, is_unique_pointer:1, is_reference:1,
          tag:5;
};

/* 
 * The following defines map mnemonic names to the different numeric values 
 * of XPTTypeDescriptor->tag when XPTTypeDescriptor->is_pointer is FALSE.
 */
#define TD_INT8   0   /* int8 */
#define TD_INT16  1   /* int16 */
#define TD_INT32  2   /* int32 */
#define TD_INT64  3   /* int64 */
#define TD_UINT8  4   /* uint8 */
#define TD_UINT16 5   /* uint16 */
#define TD_UINT32 6   /* uint32 */
#define TD_UINT64 7   /* uint64 */
#define TD_FLOAT  8   /* float */
#define TD_DOUBLE 9   /* double */
#define TD_BOOL   10  /* boolean (8-bit value) */
#define TD_CHAR   11  /* char (8-bit character) */
#define TD_WCHAR  12  /* wchar_t (16-bit character) */
#define TD_VOID   13  /* void */

/* These ones aren't used yet, but for completeness sake they're here.
 * #define TD_RESERVED 14
 * #define TD_RESERVED 15
 * #define TD_RESERVED 16
 * #define TD_RESERVED 17
 */

/* 
 * The following defines represent special cases XPTTypeDescriptor->tag 
 * when the TypeDescriptor is of type Interface or InterfaceIs.
 */
#define TD_INTERFACE_TYPE    18
#define TD_INTERFACE_IS_TYPE 19

/* 
 * The following defines map mnemonic names to the different numeric values 
 * of XPTTypeDescriptor->tag when XPTTypeDescriptor->is_pointer is TRUE.
 */
#define TD_PINT8    0   /* int8* */
#define TD_PINT16   1   /* int16* */
#define TD_PINT32   2   /* int32* */
#define TD_PINT64   3   /* int64* */
#define TD_PUINT8   4   /* uint8* */
#define TD_PUINT16  5   /* uint16* */
#define TD_PUINT32  6   /* uint32* */
#define TD_PUINT64  7   /* uint64* */
#define TD_PFLOAT   8   /* float* */
#define TD_PDOUBLE  9   /* double* */
#define TD_PBOOL    10  /* boolean* (8-bit value) */
#define TD_PCHAR    11  /* char* (pointer to a single 8-bit character) */
#define TD_PWCHAR   12  /* wchar_t* (pointer to a single 16-bit character) */
#define TD_PVOID    13  /* void* (generic opaque pointer) */
#define TD_PPNSIID  14  /* nsIID** */
#define TD_PBSTR    15  /* BSTR is an OLE type consisting of a 32-bit 
                           string-length field followed bu a NUL-terminated 
                           Unicode string */
#define TD_PSTRING  16  /* char* (pointer to a NUL-terminated array) */
#define TD_PWSTRING 17  /* wchar* (pointer to a NUL-terminated array) */

struct XPTTypeDescriptor {
    XPTTypeDescriptorPrefix prefix;
    union {
        uint32 interface;
        uint8  argnum;
    } type;
};

#define XPT_COPY_TYPE(to, from)                                              \
  (to).prefix.is_pointer = (from).prefix.is_pointer;                          \
  (to).prefix.is_unique_pointer = (from).prefix.is_unique_pointer;            \
  (to).prefix.is_reference = (from).prefix.is_reference;                      \
  (to).prefix.tag = (from).prefix.tag;                                        \
  (to).type.interface = (from).type.interface

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
union XPTConstValue {
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
        XPTString *string;
}; /* varies according to type */

struct XPTConstDescriptor {
    char                *name;
    XPTTypeDescriptor   type;
    union XPTConstValue value;
};

/*
 * A ParamDescriptor is a variable-size record used to describe either a 
 * single argument to a method or a method's result.
 */
struct XPTParamDescriptor {
    uint8             in:1, out:1, retval:1, reserved:5;
    XPTTypeDescriptor type;
};

PRBool
XPT_FillParamDescriptor(XPTParamDescriptor *pd, PRBool in, PRBool out,
                        PRBool retval, XPTTypeDescriptor type);

/*
 * A MethodDescriptor is a variable-size record used to describe a single 
 * interface method.
 */
struct XPTMethodDescriptor {
    uint8               is_getter:1, is_setter:1, is_varargs:1,
                        is_constructor:1, is_hidden:1, reserved:3;
    char                *name;
    uint8               num_args;
    XPTParamDescriptor  *params;
    XPTParamDescriptor  *result;
};

PRBool
XPT_FillMethodDescriptor(XPTMethodDescriptor *meth, PRBool is_getter,
                         PRBool is_setter, PRBool is_varargs,
                         PRBool is_constructor, PRBool is_hidden, char *name,
                         uint8 num_args);

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
    XPTAnnotation *next;
    XPTAnnotationPrefix prefix;
    XPTPrivateAnnotation private;
};

XPTAnnotation *
XPT_NewAnnotation(PRBool is_last, PRBool is_empty, XPTString *creator,
                  XPTString *private_data);

#endif /* __xpt_struct_h__ */
