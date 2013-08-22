/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Structures matching the in-memory representation of typelib structures.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef __xpt_struct_h__
#define __xpt_struct_h__

#include "xpt_arena.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    uint32_t m0;
    uint16_t m1;
    uint16_t m2;
    uint8_t  m3[8];
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
    uint8_t                     magic[16];
    uint8_t                     major_version;
    uint8_t                     minor_version;
    uint16_t                    num_interfaces;
    uint32_t                    file_length;
    XPTInterfaceDirectoryEntry  *interface_directory;
    uint32_t                    data_pool;
    XPTAnnotation               *annotations;
};

#define XPT_MAGIC "XPCOM\nTypeLib\r\n\032"
/* For error messages. */
#define XPT_MAGIC_STRING "XPCOM\\nTypeLib\\r\\n\\032"
#define XPT_MAJOR_VERSION 0x01
#define XPT_MINOR_VERSION 0x02

/* Any file with a major version number of XPT_MAJOR_INCOMPATIBLE_VERSION 
 * or higher is to be considered incompatible by this version of xpt and
 * we will refuse to read it. We will return a header with magic, major and
 * minor versions set from the file. num_interfaces and file_length will be
 * set to zero to confirm our inability to read the file; i.e. even if some
 * client of this library gets out of sync with us regarding the agreed upon
 * value for XPT_MAJOR_INCOMPATIBLE_VERSION, anytime num_interfaces and
 * file_length are both zero we *know* that this library refused to read the 
 * file due to version imcompatibility.  
 */
#define XPT_MAJOR_INCOMPATIBLE_VERSION 0x02

/*
 * The "[-t version number]" cmd line parameter to the XPIDL compiler and XPT
 * linker specifies the major and minor version number of the output 
 * type library.
 * 
 * The goal is for the compiler to check that the input IDL file only uses 
 * constructs that are supported in the version specified. The linker will
 * check that all typelib files it reads are of the version specified or
 * below.
 * 
 * Both the compiler and the linker will report errors and abort if these
 * checks fail.
 * 
 * When you rev up major or minor versions of the type library in the future,
 * think about the new stuff that you added to the type library and add checks
 * to make sure that occurrences of that new "stuff" will get caught when [-t
 * version number] is used with the compiler. Here's what you'll probably
 * have to do each time you rev up major/minor versions:
 * 
 *   1) Add the current version number string (before your change) to the
 *   XPT_TYPELIB_VERSIONS list.
 * 
 *   2) Do your changes add new features to XPIDL? Ensure that those new
 *   features are rejected by the XPIDL compiler when any version number in
 *   the XPT_TYPELIB_VERSIONS list is specified on the command line. The
 *   one place that currently does this kind of error checking is the function
 *   verify_type_fits_version() in xpidl_util.c. It currently checks
 *   attribute types, parameter types, and return types. You'll probably have
 *   to add to it or generalize it further based on what kind of changes you
 *   are making.
 *
 *   3) You will probably NOT need to make any changes to the error checking
 *   in the linker.
 */
  
#define XPT_VERSION_UNKNOWN     0
#define XPT_VERSION_UNSUPPORTED 1
#define XPT_VERSION_OLD         2
#define XPT_VERSION_CURRENT     3

typedef struct {
    const char* str;
    uint8_t     major;
    uint8_t     minor;
    uint16_t    code;
} XPT_TYPELIB_VERSIONS_STRUCT; 

/* Currently accepted list of versions for typelibs */
#define XPT_TYPELIB_VERSIONS {                                                \
    {"1.0", 1, 0, XPT_VERSION_UNSUPPORTED},                                   \
    {"1.1", 1, 1, XPT_VERSION_OLD},                                           \
    {"1.2", 1, 2, XPT_VERSION_CURRENT}                                        \
}

extern XPT_PUBLIC_API(uint16_t)
XPT_ParseVersionString(const char* str, uint8_t* major, uint8_t* minor);

extern XPT_PUBLIC_API(XPTHeader *)
XPT_NewHeader(XPTArena *arena, uint16_t num_interfaces, 
              uint8_t major_version, uint8_t minor_version);

extern XPT_PUBLIC_API(void)
XPT_FreeHeader(XPTArena *arena, XPTHeader* aHeader);

/* size of header and annotations */
extern XPT_PUBLIC_API(uint32_t)
XPT_SizeOfHeader(XPTHeader *header);

/* size of header and annotations and InterfaceDirectoryEntries */
extern XPT_PUBLIC_API(uint32_t)
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

#if 0 /* not yet */
    /* not stored on disk */
    uint32_t                 offset; /* the offset for an ID still to be read */
#endif
};

extern XPT_PUBLIC_API(PRBool)
XPT_FillInterfaceDirectoryEntry(XPTArena *arena, 
                                XPTInterfaceDirectoryEntry *ide,
                                nsID *iid, char *name, char *name_space,
                                XPTInterfaceDescriptor *descriptor);

extern XPT_PUBLIC_API(void)
XPT_DestroyInterfaceDirectoryEntry(XPTArena *arena, 
                                   XPTInterfaceDirectoryEntry* ide);

/*
 * An InterfaceDescriptor is a variable-size record used to describe a 
 * single XPCOM interface, including all of its methods. 
 */
struct XPTInterfaceDescriptor {
    /* This field ordering minimizes the size of this struct.
    *  The fields are serialized on disk in a different order.
    *  See DoInterfaceDescriptor().
    */
    XPTMethodDescriptor     *method_descriptors;
    XPTConstDescriptor      *const_descriptors;
    XPTTypeDescriptor       *additional_types;
    uint16_t                parent_interface;
    uint16_t                num_methods;
    uint16_t                num_constants;
    uint8_t                 flags;

    /* additional_types are used for arrays where we may need multiple
    *  XPTTypeDescriptors for a single XPTMethodDescriptor. Since we still
    *  want to have a simple array of XPTMethodDescriptor (each with a single
    *  embedded XPTTypeDescriptor), a XPTTypeDescriptor can have a reference
    *  to an 'additional_type'. That reference is an index in this 
    *  "additional_types" array. So a given XPTMethodDescriptor might have 
    *  a whole chain of these XPTTypeDescriptors to represent, say, a multi
    *  dimensional array.
    *
    *  Note that in the typelib file these additional types are stored 'inline'
    *  in the MethodDescriptor. But, in the typelib MethodDescriptors can be 
    *  of varying sizes, where in XPT's in memory mapping of the data we want 
    *  them to be of fixed size. This additional_types scheme is here to allow 
    *  for that.
    */

    uint16_t                num_additional_types;
};

#define XPT_ID_SCRIPTABLE           0x80
#define XPT_ID_FUNCTION             0x40
#define XPT_ID_BUILTINCLASS         0x20
#define XPT_ID_FLAGMASK             0xe0
#define XPT_ID_TAGMASK              (~XPT_ID_FLAGMASK)
#define XPT_ID_TAG(id)              ((id).flags & XPT_ID_TAGMASK)

#define XPT_ID_IS_SCRIPTABLE(flags) (!!(flags & XPT_ID_SCRIPTABLE))
#define XPT_ID_IS_FUNCTION(flags) (!!(flags & XPT_ID_FUNCTION))
#define XPT_ID_IS_BUILTINCLASS(flags) (!!(flags & XPT_ID_BUILTINCLASS))

extern XPT_PUBLIC_API(PRBool)
XPT_GetInterfaceIndexByName(XPTInterfaceDirectoryEntry *ide_block,
                            uint16_t num_interfaces, char *name, 
                            uint16_t *indexp);

extern XPT_PUBLIC_API(XPTInterfaceDescriptor *)
XPT_NewInterfaceDescriptor(XPTArena *arena, 
                           uint16_t parent_interface, uint16_t num_methods,
                           uint16_t num_constants, uint8_t flags);

extern XPT_PUBLIC_API(void)
XPT_FreeInterfaceDescriptor(XPTArena *arena, XPTInterfaceDescriptor* id);

extern XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddTypes(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                uint16_t num);

extern XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddMethods(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                  uint16_t num);

extern XPT_PUBLIC_API(PRBool)
XPT_InterfaceDescriptorAddConsts(XPTArena *arena, XPTInterfaceDescriptor *id, 
                                 uint16_t num);

/*
 * This is our special string struct with a length value associated with it,
 * which means that it can contains embedded NULs.
 */
struct XPTString {
    uint16_t length;
    char   *bytes;
};

extern XPT_PUBLIC_API(XPTString *)
XPT_NewString(XPTArena *arena, uint16_t length, char *bytes);

extern XPT_PUBLIC_API(XPTString *)
XPT_NewStringZ(XPTArena *arena, char *bytes);

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

/* why bother with a struct?  - other code relies on this being a struct */
struct XPTTypeDescriptorPrefix {
    uint8_t flags;
};

/* flag bits -- fur and jband were right, I was miserably wrong */

// THESE TWO FLAGS ARE DEPRECATED. DO NOT USE THEM. See bug 692342.
#define XPT_TDP_POINTER          0x80
#define XPT_TDP_REFERENCE        0x20

#define XPT_TDP_FLAGMASK         0xe0
#define XPT_TDP_TAGMASK          (~XPT_TDP_FLAGMASK)
#define XPT_TDP_TAG(tdp)         ((tdp).flags & XPT_TDP_TAGMASK)

#define XPT_TDP_IS_POINTER(flags)        (flags & XPT_TDP_POINTER)
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
    TD_DOMSTRING         = 15,
    TD_PSTRING           = 16,
    TD_PWSTRING          = 17,
    TD_INTERFACE_TYPE    = 18,
    TD_INTERFACE_IS_TYPE = 19,
    TD_ARRAY             = 20,
    TD_PSTRING_SIZE_IS   = 21,
    TD_PWSTRING_SIZE_IS  = 22,
    TD_UTF8STRING        = 23,
    TD_CSTRING           = 24,
    TD_ASTRING           = 25,
    TD_JSVAL             = 26
};

struct XPTTypeDescriptor {
    XPTTypeDescriptorPrefix prefix;
    uint8_t argnum;                 /* used for iid_is and size_is */
    uint8_t argnum2;                /* used for length_is */
    union {                         
        uint16_t iface;             /* used for TD_INTERFACE_TYPE */
        uint16_t additional_type;   /* used for TD_ARRAY */
    } type;
};

#define XPT_COPY_TYPE(to, from)                                               \
  (to).prefix.flags = (from).prefix.flags;                                    \
  (to).argnum = (from).argnum;                                                \
  (to).argnum2 = (from).argnum2;                                              \
  (to).type.additional_type = (from).type.additional_type;

/*
 * A ConstDescriptor is a variable-size record that records the name and 
 * value of a scoped interface constant. 
 *
 * The types of the method parameter are restricted to the following subset 
 * of TypeDescriptors: 
 *
 * int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, 
 * int64_t, uint64_t, wchar_t, char, string
 * 
 * The type (and thus the size) of the value record is determined by the 
 * contents of the associated TypeDescriptor record. For instance, if type 
 * corresponds to int16_t, then value is a two-byte record consisting of a 
 * 16-bit signed integer.  For a ConstDescriptor type of string, the value 
 * record is of type String*, i.e. an offset within the data pool to a 
 * String record containing the constant string.
 */
union XPTConstValue {
    int8_t    i8;
    uint8_t   ui8; 
    int16_t   i16; 
    uint16_t  ui16;
    int32_t   i32; 
    uint32_t  ui32;
    int64_t   i64; 
    uint64_t  ui64; 
    float     flt;
    double    dbl;
    PRBool    bul;
    char      ch; 
    uint16_t  wch;
    nsID      *iid;
    XPTString *string;
    char      *str;
    uint16_t  *wstr;
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
    uint8_t           flags;
    XPTTypeDescriptor type;
};

/* flag bits -- jband and fur were right, and I was miserably wrong */
#define XPT_PD_IN       0x80
#define XPT_PD_OUT      0x40
#define XPT_PD_RETVAL   0x20
#define XPT_PD_SHARED   0x10
#define XPT_PD_DIPPER   0x08
#define XPT_PD_OPTIONAL 0x04
#define XPT_PD_FLAGMASK 0xfc

#define XPT_PD_IS_IN(flags)     (flags & XPT_PD_IN)
#define XPT_PD_IS_OUT(flags)    (flags & XPT_PD_OUT)
#define XPT_PD_IS_RETVAL(flags) (flags & XPT_PD_RETVAL)
#define XPT_PD_IS_SHARED(flags) (flags & XPT_PD_SHARED)
#define XPT_PD_IS_DIPPER(flags) (flags & XPT_PD_DIPPER)
#define XPT_PD_IS_OPTIONAL(flags) (flags & XPT_PD_OPTIONAL)

extern XPT_PUBLIC_API(PRBool)
XPT_FillParamDescriptor(XPTArena *arena, 
                        XPTParamDescriptor *pd, uint8_t flags,
                        XPTTypeDescriptor *type);

/*
 * A MethodDescriptor is a variable-size record used to describe a single 
 * interface method.
 */
struct XPTMethodDescriptor {
    char                *name;
    XPTParamDescriptor  *params;
    XPTParamDescriptor  result;
    uint8_t             flags;
    uint8_t             num_args;
};

/* flag bits -- jband and fur were right, and I was miserably wrong */
#define XPT_MD_GETTER   0x80
#define XPT_MD_SETTER   0x40
#define XPT_MD_NOTXPCOM 0x20
#define XPT_MD_CTOR     0x10
#define XPT_MD_HIDDEN   0x08
#define XPT_MD_OPT_ARGC 0x04
#define XPT_MD_CONTEXT  0x02
#define XPT_MD_FLAGMASK 0xfe

#define XPT_MD_IS_GETTER(flags)      (flags & XPT_MD_GETTER)
#define XPT_MD_IS_SETTER(flags)      (flags & XPT_MD_SETTER)
#define XPT_MD_IS_NOTXPCOM(flags)    (flags & XPT_MD_NOTXPCOM)
#define XPT_MD_IS_CTOR(flags)        (flags & XPT_MD_CTOR)
#define XPT_MD_IS_HIDDEN(flags)      (flags & XPT_MD_HIDDEN)
#define XPT_MD_WANTS_OPT_ARGC(flags) (flags & XPT_MD_OPT_ARGC)
#define XPT_MD_WANTS_CONTEXT(flags)  (flags & XPT_MD_CONTEXT)

extern XPT_PUBLIC_API(PRBool)
XPT_FillMethodDescriptor(XPTArena *arena, 
                         XPTMethodDescriptor *meth, uint8_t flags, char *name,
                         uint8_t num_args);

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
    uint8_t       flags;
    /* remaining fields are present in typelib iff XPT_ANN_IS_PRIVATE */
    XPTString     *creator;
    XPTString     *private_data;
};

#define XPT_ANN_LAST	                0x80
#define XPT_ANN_IS_LAST(flags)          (flags & XPT_ANN_LAST)
#define XPT_ANN_PRIVATE                 0x40
#define XPT_ANN_IS_PRIVATE(flags)       (flags & XPT_ANN_PRIVATE)

extern XPT_PUBLIC_API(XPTAnnotation *)
XPT_NewAnnotation(XPTArena *arena, uint8_t flags, XPTString *creator, 
                  XPTString *private_data);

#ifdef __cplusplus
}
#endif

#endif /* __xpt_struct_h__ */
