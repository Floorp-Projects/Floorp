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
 * The linkage of XPT API functions differs depending on whether the file is
 * used within the XPT library or not.  Any source file within the XPT
 * library should define EXPORT_XPT_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPT_API
#define XPT_PUBLIC_API(t)    PR_IMPLEMENT(t)
#define XPT_PUBLIC_DATA(t)   PR_IMPLEMENT_DATA(t)
#else
#ifdef _WIN32
#    define XPT_PUBLIC_API(t)    _declspec(dllimport) t
#    define XPT_PUBLIC_DATA(t)   _declspec(dllimport) t
#else
#    define XPT_PUBLIC_API(t)    PR_IMPLEMENT(t)
#    define XPT_PUBLIC_DATA(t)   t
#endif
#endif
#define XPT_FRIEND_API(t)    XPT_PUBLIC_API(t)
#define XPT_FRIEND_DATA(t)   XPT_PUBLIC_DATA(t)

PR_BEGIN_EXTERN_C

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
  (to).m3[3] = (from).m3[3];                                                  \
  (to).m3[4] = (from).m3[4];                                                  \
  (to).m3[5] = (from).m3[5];                                                  \
  (to).m3[6] = (from).m3[6];                                                  \
  (to).m3[7] = (from).m3[7];


/*
 * Every XPCOM typelib file begins with a header.
 */
struct XPTHeader {
    PRUint8                     magic[16];
    PRUint8                     major_version;
    PRUint8                     minor_version;
    PRUint16                    num_interfaces;
    PRUint32                    file_length;
    XPTInterfaceDirectoryEntry  *interface_directory;
    PRUint32                    data_pool;
    XPTAnnotation               *annotations;
};

#define XPT_MAGIC "XPCOM\nTypeLib\r\n\032"
#define XPT_MAJOR_VERSION 0x01
#define XPT_MINOR_VERSION 0x00

extern XPT_PUBLIC_API(XPTHeader *)
XPT_NewHeader(PRUint16 num_interfaces);

/* size of header and annotations */
extern XPT_PUBLIC_API(PRUint32)
XPT_SizeOfHeader(XPTHeader *header);

/* size of header and annotations and InterfaceDirectoryEntries */
extern XPT_PUBLIC_API(PRUint32)
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
    char                   *name_space;
    XPTInterfaceDescriptor *interface_descriptor;

    /* not stored on disk. Available for compiler and at runtime */
    void                   *user_data;
#if 0 /* not yet */
    /* not stored on disk */
    PRUint32                 offset; /* the offset for an ID still to be read */
#endif
};

extern XPT_PUBLIC_API(PRBool)
XPT_FillInterfaceDirectoryEntry(XPTInterfaceDirectoryEntry *ide,
                                nsID *iid, char *name, char *name_space,
                                XPTInterfaceDescriptor *descriptor,
                                void *user_data);

/*
 * An InterfaceDescriptor is a variable-size record used to describe a 
 * single XPCOM interface, including all of its methods. 
 */
struct XPTInterfaceDescriptor {
    PRUint16                parent_interface;
    PRUint16                num_methods;
    XPTMethodDescriptor     *method_descriptors;
    PRUint16                num_constants;
    XPTConstDescriptor      *const_descriptors;
    PRUint8                 flags;
};

#define XPT_ID_SCRIPTABLE           0x80
#define XPT_ID_FLAGMASK             0x80
#define XPT_ID_TAGMASK              (~XPT_ID_FLAGMASK)
#define XPT_ID_TAG(id)              ((id).flags & XPT_ID_TAGMASK)

#define XPT_ID_IS_SCRIPTABLE(flags) (flags & XPT_ID_SCRIPTABLE)

extern XPT_PUBLIC_API(PRBool)
XPT_GetInterfaceIndexByName(XPTInterfaceDirectoryEntry *ide_block,
                            PRUint16 num_interfaces, char *name, 
                            PRUint16 *indexp);

extern XPT_PUBLIC_API(XPTInterfaceDescriptor *)
XPT_NewInterfaceDescriptor(PRUint16 parent_interface, PRUint16 num_methods,
                           PRUint16 num_constants, PRUint8 flags);

extern XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddMethods(XPTInterfaceDescriptor *id, PRUint16 num);

extern XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddConsts(XPTInterfaceDescriptor *id, PRUint16 num);

/*
 * This is our special string struct with a length value associated with it,
 * which means that it can contains embedded NULs.
 */
struct XPTString {
    PRUint16 length;
    char   *bytes;
};

extern XPT_PUBLIC_API(XPTString *)
XPT_NewString(PRUint16 length, char *bytes);

extern XPT_PUBLIC_API(XPTString *)
XPT_NewStringZ(char *bytes);

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

/* XXX why bother with a struct? */
struct XPTTypeDescriptorPrefix {
    PRUint8 flags;
};

/* flag bits -- fur and jband were right, I was miserably wrong */
#define XPT_TDP_POINTER          0x80
#define XPT_TDP_UNIQUE_POINTER   0x40
#define XPT_TDP_REFERENCE        0x20
#define XPT_TDP_FLAGMASK         0xe0
#define XPT_TDP_TAGMASK          (~XPT_TDP_FLAGMASK)
#define XPT_TDP_TAG(tdp)         ((tdp).flags & XPT_TDP_TAGMASK)

#define XPT_TDP_IS_POINTER(flags)        (flags & XPT_TDP_POINTER)
#define XPT_TDP_IS_UNIQUE_POINTER(flags) (flags & XPT_TDP_UNIQUE_POINTER)
#define XPT_TDP_IS_REFERENCE(flags)      (flags & XPT_TDP_REFERENCE)

/* 
 * The following enum maps mnemonic names to the different numeric values 
 * of XPTTypeDescriptor->tag.
 */
enum XPTTypeDescriptorTags {
    TD_INT8              = 0,
    TD_INT16             = 1,
    TD_INT32             = 2,
    TD_INT64             = 3,
    TD_UINT8             = 4,
    TD_UINT16            = 5,
    TD_UINT32            = 6,
    TD_UINT64            = 7,
    TD_FLOAT             = 8, 
    TD_DOUBLE            = 9,
    TD_BOOL              = 10,  
    TD_CHAR              = 11,  
    TD_WCHAR             = 12, 
    TD_VOID              = 13,  
    TD_PNSIID            = 14,
    TD_PBSTR             = 15,
    TD_PSTRING           = 16,
    TD_PWSTRING          = 17,
    TD_INTERFACE_TYPE    = 18,
    TD_INTERFACE_IS_TYPE = 19
};

struct XPTTypeDescriptor {
    XPTTypeDescriptorPrefix prefix;
    union {
        PRUint16 interface;
        PRUint8  argnum;
    } type;
};

#define XPT_TYPEDESCRIPTOR_SIZE (1 + 2)

#define XPT_COPY_TYPE(to, from)                                               \
  (to).prefix.flags = (from).prefix.flags;                                    \
  (to).type.interface = (from).type.interface;

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
    PRInt8    i8;
    PRUint8   ui8; 
    PRInt16   i16; 
    PRUint16  ui16;
    PRInt32   i32; 
    PRUint32  ui32;
    PRInt64   i64; 
    PRUint64  ui64; 
    float     flt;
    double    dbl;
    PRBool    bul;
    char      ch; 
    PRUint16  wch;
    nsID      *iid;
    XPTString *string;
    char      *str;
    PRUint16  *wstr;
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
    PRUint8           flags;
    XPTTypeDescriptor type;
};

/* flag bits -- jband and fur were right, and I was miserably wrong */
#define XPT_PD_IN       0x80
#define XPT_PD_OUT      0x40
#define XPT_PD_RETVAL   0x20
#define XPT_PD_SHARED   0x10
#define XPT_PD_FLAGMASK 0xf0

#define XPT_PD_IS_IN(flags)     (flags & XPT_PD_IN)
#define XPT_PD_IS_OUT(flags)    (flags & XPT_PD_OUT)
#define XPT_PD_IS_RETVAL(flags) (flags & XPT_PD_RETVAL)
#define XPT_PD_IS_SHARED(flags) (flags & XPT_PD_SHARED)

#define XPT_PARAMDESCRIPTOR_SIZE (XPT_TYPEDESCRIPTOR_SIZE + 1)

extern XPT_PUBLIC_API(PRBool)
XPT_FillParamDescriptor(XPTParamDescriptor *pd, PRUint8 flags,
                        XPTTypeDescriptor *type);

/*
 * A MethodDescriptor is a variable-size record used to describe a single 
 * interface method.
 */
struct XPTMethodDescriptor {
    PRUint8             flags;
    char                *name;
    PRUint8             num_args;
    XPTParamDescriptor  *params;
    XPTParamDescriptor  *result;
};

/* flag bits -- jband and fur were right, and I was miserably wrong */
#define XPT_MD_GETTER   0x80
#define XPT_MD_SETTER   0x40
#define XPT_MD_VARARGS  0x20
#define XPT_MD_CTOR     0x10
#define XPT_MD_HIDDEN   0x08
#define XPT_MD_FLAGMASK 0xf8

#define XPT_MD_IS_GETTER(flags)     (flags & XPT_MD_GETTER)
#define XPT_MD_IS_SETTER(flags)     (flags & XPT_MD_SETTER)
#define XPT_MD_IS_VARARGS(flags)    (flags & XPT_MD_VARARGS)
#define XPT_MD_IS_CTOR(flags)       (flags & XPT_MD_CTOR)
#define XPT_MD_IS_HIDDEN(flags)     (flags & XPT_MD_HIDDEN)

extern XPT_PUBLIC_API(PRBool)
XPT_FillMethodDescriptor(XPTMethodDescriptor *meth, PRUint8 flags, char *name,
                         PRUint8 num_args);

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

struct XPTAnnotation {
    XPTAnnotation *next;
    PRUint8       flags;
    /* remaining fields are present in typelib iff XPT_ANN_IS_PRIVATE */
    XPTString     *creator;
    XPTString     *private_data;
};

#define XPT_ANN_LAST	                0x80
#define XPT_ANN_IS_LAST(flags)          (flags & XPT_ANN_LAST)
#define XPT_ANN_PRIVATE                 0x40
#define XPT_ANN_IS_PRIVATE(flags)       (flags & XPT_ANN_PRIVATE)

extern XPT_PUBLIC_API(XPTAnnotation *)
XPT_NewAnnotation(PRUint8 flags, XPTString *creator, XPTString *private_data);

/***************************************************************************/
/* 
* XXX It's not clear that these should really be exported
*/

extern XPT_PUBLIC_API(PRUint32)
XPT_SizeOfTypeDescriptor(XPTTypeDescriptor *td);

extern XPT_PUBLIC_API(PRUint32)
XPT_SizeOfMethodDescriptor(XPTMethodDescriptor *md);

extern XPT_PUBLIC_API(PRUint32)
XPT_SizeOfConstDescriptor(XPTConstDescriptor *cd);

extern XPT_PUBLIC_API(PRUint32)
XPT_SizeOfInterfaceDescriptor(XPTInterfaceDescriptor *id);

extern XPT_PUBLIC_API(PRBool)
XPT_FillConstDescriptor(XPTConstDescriptor *cd, char *name,
                        XPTTypeDescriptor type, union XPTConstValue value);

PR_END_EXTERN_C

#endif /* __xpt_struct_h__ */
