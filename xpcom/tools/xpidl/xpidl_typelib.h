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
 * Declarations used in building typelib files.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef __xpidl_typelib_h
#define __xpidl_typelib_h

#include "prtypes.h"
#include "prio.h"

#define MAGIC_STRING "XPCOM\nTypeLib\r\n\0"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define IS_EMPTY_ANNOTATION 0 
#define IS_PRIVATE_ANNOTATION 1 

/* Non-standard uint fields. */
typedef struct uint4 {
    PRUint8 bitfield;
} uint4;

typedef uint4 uint5;
typedef uint4 uint6;
typedef uint4 uint7;

typedef struct uint128 {
    PRUint64 hi;
    PRUint64 lo;
} uint128;

#define GET_UINT4(record) ((record)->bitfield & PR_BITMASK(4)) 
#define SET_UINT4(record, val) ((record)->bitfield = ((record)->bitfield &~ PR_BITMASK(4)) | (val)) 
#define GET_UINT5(record) ((record)->bitfield & PR_BITMASK(5)) 
#define SET_UINT5(record, val) ((record)->bitfield = ((record)->bitfield &~ PR_BITMASK(5)) | (val)) 
#define GET_UINT6(record) ((record)->bitfield & PR_BITMASK(6)) 
#define SET_UINT6(record, val) ((record)->bitfield = ((record)->bitfield &~ PR_BITMASK(6)) | (val))
#define GET_UINT7(record) ((record)->bitfield & PR_BITMASK(7)) 
#define SET_UINT7(record, val) ((record)->bitfield = ((record)->bitfield &~ PR_BITMASK(7)) | (val))

/*
 * Identifier records are used to represent variable-length, 
 * human-readable strings.
 */
typedef struct Identifier { 
    char *bytes; 
} Identifier;

/*
 * String records are used to represent variable-length, human-readable 
 * strings.
 */
typedef struct String { 
    PRUint16 length; 
    char   *bytes; 
} String;

/* Forward references. */
typedef struct InterfaceDirectoryEntry InterfaceDirectoryEntry;

/*
 * The first byte of all these TypeDescriptor variants has the identical 
 * layout.
 */
typedef struct TypeDescriptorPrefix { 
    gboolean is_pointer; 
    gboolean is_unique_pointer;
    gboolean is_reference; 
    uint5    tag; 
} TypeDescriptorPrefix;

/*
 * The one-byte SimpleTypeDescriptor is a kind of TypeDescriptor used to 
 * represent scalar types,  pointers to scalar types, the void type, the 
 * void* type and, as a special case, the nsIID* type.
 */
typedef struct SimpleTypeDescriptor { 
    TypeDescriptorPrefix prefix;
} SimpleTypeDescriptor;

/*
 * An InterfaceTypeDescriptor is used to represent either a pointer to an 
 * interface type or a pointer to a pointer to an interface type, 
 * e.g. nsISupports* or nsISupports**. 
 */
typedef struct InterfaceTypeDescriptor { 
    TypeDescriptorPrefix    prefix;
    InterfaceDirectoryEntry *interface; 
} InterfaceTypeDescriptor;

/*
 * An InterfaceIsTypeDescriptor describes an interface pointer type.  It 
 * is similar to an InterfaceTypeDescriptor except that the type of the 
 * interface pointer is specified at runtime by the value of another 
 * argument, rather than being specified by the typelib.
 */
typedef struct InterfaceIsTypeDescriptor {
    TypeDescriptorPrefix prefix;
    PRUint8              arg_num;
} InterfaceIsTypeDescriptor;

/*
 * A TypeDescriptor is a variable-size record used to identify the type of 
 * a method argument or return value.
 */
typedef union TypeDescriptor { 
    SimpleTypeDescriptor      simple_type; 
    InterfaceTypeDescriptor interface_type;
    InterfaceIsTypeDescriptor interface_is_type; 
} TypeDescriptor;

/*
 * A ParamDescriptor is a variable-size record used to describe either a 
 * single argument to a method or a method's result.
 */
typedef struct ParamDescriptor {
    gboolean       in, out;
    uint6          reserved;
    TypeDescriptor type;
} ParamDescriptor;

/*
 * A MethodDescriptor is a variable-size record used to describe a single 
 * interface method. 
 */
typedef struct MethodDescriptor {
    gboolean        is_getter;
    gboolean        is_setter;
    gboolean        is_varargs;
    gboolean        is_constructor;
    uint4           reserved;
    Identifier      *name;
    PRUint8         num_args;
    ParamDescriptor *params;
    ParamDescriptor result;
} MethodDescriptor;

/*
 * A ConstDescriptor is a variable-size record that records the name and 
 * value of a scoped interface constant.  All ConstDescriptor records have 
 * this form.  
 */
typedef struct ConstDescriptor { 
    Identifier     *name; 
    TypeDescriptor type;
    void           *value; 
} ConstDescriptor;

/*
 * An InterfaceDescriptor is a variable-size record used to describe a 
 * single XPCOM interface, including all of its methods. 
 */
typedef struct InterfaceDescriptor {
    InterfaceDirectoryEntry *parent_interface;
    PRUint16                num_methods;
    MethodDescriptor        *method_descriptors;
    PRUint16                num_constants;
    ConstDescriptor         *const_descriptors;
} InterfaceDescriptor;

/*
 * A contiguous array of fixed-size InterfaceDirectoryEntry records begins 
 * at the byte offset identified by the interface_directory field in the 
 * file header.  The array is used to quickly locate an interface 
 * description using its IID.  No interface should appear more than once 
 * in the array. 
 */
struct InterfaceDirectoryEntry {
    uint128             iid;
    Identifier          *name;
    InterfaceDescriptor *interface_descriptor;
};

/*
 * Annotation records are variable-size records used to store secondary 
 * information about the typelib or about individual interfaces, 
 * e.g. such as the name of the tool that generated the typelib file,
 * the date it was generated, etc.  The information is stored with very 
 * loose format requirements so as to allow virtually any private data 
 * to be stored in the typelib. 
 */
typedef struct EmptyAnnotation { 
    gboolean is_last;
    uint7     tag;
} EmptyAnnotation;

typedef struct PrivateAnnotation {
    gboolean is_last;
    uint7    tag;
    String   creator;
    String   private_data;
} PrivateAnnotation;

typedef union Annotation{ 
    EmptyAnnotation   empty_note;
    PrivateAnnotation private_note; 
} Annotation;

/* 
 * Every XPCOM typelib file begins with a header.
 */
typedef struct TypeLibHeader {
    char                     magic[16];
    PRUint8                  major_version;
    PRUint8                  minor_version;
    PRUint16                 num_interfaces;
    PRUint32                 file_length;
    InterfaceDirectoryEntry* interface_directory;
    PRUint8                  *data_pool;
    Annotation               *annotations;
} TypeLibHeader;

#endif /* __xpidl_typelib_h */









