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

typedef struct uint128 {
    PRUint64 hi;
    PRUint64 lo;
} uint128;

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
    char     *bytes; 
} String;

/* Forward references. */
typedef struct InterfaceDirectoryEntry InterfaceDirectoryEntry;

/*
 * The first byte of all TypeDescriptor variants has an identical layout.
 *
 * compound_record is made up of the following 4 bitfields:
 *   boolean is_pointer        1
 *   boolean is_unique_pointer 1
 *   boolean is_reference      1 
 *   uint5 tag                 5 
 */
typedef struct TypeDescriptorPrefix { 
    PRUint8 compound_record;
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
    InterfaceTypeDescriptor   interface_type;
    InterfaceIsTypeDescriptor interface_is_type; 
} TypeDescriptor;

/*
 * A ParamDescriptor is a variable-size record used to describe either a 
 * single argument to a method or a method's result.
 *
 * compound_record is made up of the following 4 bitfields:
 *   boolean in     1
 *   boolean out    1
 *   booleam retval 1
 *   uint6 reserved 5 
 */
typedef struct ParamDescriptor {
    PRUint8        compound_record;
    TypeDescriptor type;
} ParamDescriptor;

/*
 * A MethodDescriptor is a variable-size record used to describe a single 
 * interface method. 
 *
 * compound_record is made up of the following 5 bitfields:
 *   boolean is_getter      1
 *   boolean is_setter      1
 *   boolean is_varargs     1
 *   boolean is_constructor 1
 *   uint4 reserved         4 
 */
typedef struct MethodDescriptor {
    PRUint8         compound_record;
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
 *
 * compound_record is made up of the following 2 bitfields:
 *   boolean is_last 1
 *   uint7 tag       7 
 */
typedef struct EmptyAnnotation { 
    PRUint8         compound_record;
} EmptyAnnotation;

typedef struct PrivateAnnotation {
    PRUint8         compound_record;
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









