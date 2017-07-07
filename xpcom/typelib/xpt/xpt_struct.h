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

extern "C" {

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

/*
 * Every XPCOM typelib file begins with a header.
 */
struct XPTHeader {
    // Some of these fields exists in the on-disk format but don't need to be
    // stored in memory (other than very briefly, which can be done with local
    // variables).

    //uint8_t                   magic[16];
    uint8_t                     major_version;
    uint8_t                     minor_version;
    uint16_t                    num_interfaces;
    //uint32_t                  file_length;
    XPTInterfaceDirectoryEntry  *interface_directory;
    //uint32_t                  data_pool;
};

#define XPT_MAGIC "XPCOM\nTypeLib\r\n\032"
/* For error messages. */
#define XPT_MAGIC_STRING "XPCOM\\nTypeLib\\r\\n\\032"
#define XPT_MAJOR_VERSION 0x01
#define XPT_MINOR_VERSION 0x02

/* Any file with a major version number of XPT_MAJOR_INCOMPATIBLE_VERSION
 * or higher is to be considered incompatible by this version of xpt and
 * we will refuse to read it. We will return a header with magic, major and
 * minor versions set from the file. num_interfaces will be set to zero to
 * confirm our inability to read the file; i.e. even if some client of this
 * library gets out of sync with us regarding the agreed upon value for
 * XPT_MAJOR_INCOMPATIBLE_VERSION, anytime num_interfaces is zero we *know*
 * that this library refused to read the file due to version incompatibility.
 */
#define XPT_MAJOR_INCOMPATIBLE_VERSION 0x02

/*
 * A contiguous array of fixed-size InterfaceDirectoryEntry records begins at
 * the byte offset identified by the interface_directory field in the file
 * header.  The array is used to quickly locate an interface description
 * using its IID.  No interface should appear more than once in the array.
 */
struct XPTInterfaceDirectoryEntry {
    nsID                   iid;
    char                   *name;

    // This field exists in the on-disk format. But it isn't used so we don't
    // allocate space for it in memory.
    //char                 *name_space;

    XPTInterfaceDescriptor *interface_descriptor;
};

/*
 * An InterfaceDescriptor describes a single XPCOM interface, including all of
 * its methods.
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
    uint8_t                 num_additional_types;
};

#define XPT_ID_SCRIPTABLE           0x80
#define XPT_ID_FUNCTION             0x40
#define XPT_ID_BUILTINCLASS         0x20
#define XPT_ID_MAIN_PROCESS_SCRIPTABLE_ONLY 0x10
#define XPT_ID_FLAGMASK             0xf0

#define XPT_ID_IS_SCRIPTABLE(flags) (!!(flags & XPT_ID_SCRIPTABLE))
#define XPT_ID_IS_FUNCTION(flags) (!!(flags & XPT_ID_FUNCTION))
#define XPT_ID_IS_BUILTINCLASS(flags) (!!(flags & XPT_ID_BUILTINCLASS))
#define XPT_ID_IS_MAIN_PROCESS_SCRIPTABLE_ONLY(flags) (!!(flags & XPT_ID_MAIN_PROCESS_SCRIPTABLE_ONLY))

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

/* flag bits */

#define XPT_TDP_FLAGMASK         0xe0
#define XPT_TDP_TAGMASK          (~XPT_TDP_FLAGMASK)
#define XPT_TDP_TAG(tdp)         ((tdp).flags & XPT_TDP_TAGMASK)

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

    // The memory layout here doesn't exactly match (for the appropriate types)
    // the on-disk format. This is to save memory.
    union {
        // Used for TD_INTERFACE_IS_TYPE.
        struct {
            uint8_t argnum;
        } interface_is;

        // Used for TD_PSTRING_SIZE_IS, TD_PWSTRING_SIZE_IS.
        struct {
            uint8_t argnum;
            //uint8_t argnum2;          // Present on disk, omitted here.
        } pstring_is;

        // Used for TD_ARRAY.
        struct {
            uint8_t argnum;
            //uint8_t argnum2;          // Present on disk, omitted here.
            uint8_t additional_type;    // uint16_t on disk, uint8_t here;
                                        // in practice it never exceeds 20.
        } array;

        // Used for TD_INTERFACE_TYPE.
        struct {
            // We store the 16-bit iface value as two 8-bit values in order to
            // avoid 16-bit alignment requirements for XPTTypeDescriptor, which
            // reduces its size and also the size of XPTParamDescriptor.
            uint8_t iface_hi8;
            uint8_t iface_lo8;
        } iface;
    } u;
};

/*
 * A ConstDescriptor is a variable-size record that records the name and
 * value of a scoped interface constant.
 *
 * The types of the method parameter are restricted to the following subset
 * of TypeDescriptors:
 *
 * int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
 * int64_t, uint64_t, wchar_t, char
 *
 * The type (and thus the size) of the value record is determined by the
 * contents of the associated TypeDescriptor record. For instance, if type
 * corresponds to int16_t, then value is a two-byte record consisting of a
 * 16-bit signed integer.
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
    char      ch;
    uint16_t  wch;
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

/* flag bits */
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

/* flag bits */
#define XPT_MD_GETTER   0x80
#define XPT_MD_SETTER   0x40
#define XPT_MD_NOTXPCOM 0x20
#define XPT_MD_HIDDEN   0x08
#define XPT_MD_OPT_ARGC 0x04
#define XPT_MD_CONTEXT  0x02
#define XPT_MD_FLAGMASK 0xfe

#define XPT_MD_IS_GETTER(flags)      (flags & XPT_MD_GETTER)
#define XPT_MD_IS_SETTER(flags)      (flags & XPT_MD_SETTER)
#define XPT_MD_IS_NOTXPCOM(flags)    (flags & XPT_MD_NOTXPCOM)
#define XPT_MD_IS_HIDDEN(flags)      (flags & XPT_MD_HIDDEN)
#define XPT_MD_WANTS_OPT_ARGC(flags) (flags & XPT_MD_OPT_ARGC)
#define XPT_MD_WANTS_CONTEXT(flags)  (flags & XPT_MD_CONTEXT)

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
 *
 * We don't actually store annotations; we just skip over them if they are
 * present.
 */

#define XPT_ANN_LAST	                0x80
#define XPT_ANN_IS_LAST(flags)          (flags & XPT_ANN_LAST)
#define XPT_ANN_PRIVATE                 0x40
#define XPT_ANN_IS_PRIVATE(flags)       (flags & XPT_ANN_PRIVATE)

}

#endif /* __xpt_struct_h__ */
