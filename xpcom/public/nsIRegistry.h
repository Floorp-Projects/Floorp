/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef __nsIRegistry_h
#define __nsIRegistry_h

#include "nsISupports.h"

// {5D41A440-8E37-11d2-8059-00600811A9C3}
#define NS_IREGISTRY_IID { 0x5d41a440, 0x8e37, 0x11d2, { 0x80, 0x59, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }
#define NS_REGISTRY_PROGID "component://netscape/registry"
#define NS_REGISTRY_CLASSNAME "Mozilla Registry"

// {D1B54831-AC07-11d2-805E-00600811A9C3}
#define NS_IREGISTRYNODE_IID { 0xd1b54831, 0xac07, 0x11d2, { 0x80, 0x5e, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }
// {5316C380-B2F8-11d2-A374-0080C6F80E4B}
#define NS_IREGISTRYVALUE_IID { 0x5316c380, 0xb2f8, 0x11d2, { 0xa3, 0x74, 0x0, 0x80, 0xc6, 0xf8, 0xe, 0x4b } }


class nsIEnumerator;

/*-------------------------------- nsIRegistry ---------------------------------
| This interface provides access to a tree of arbitrary values.                |
|                                                                              |
| Each node of the tree contains either a value or a subtree or both.          |
|                                                                              |
| The value at any of these leaf nodes can be any of these "primitive" types:  |
|   o string (null terminated UTF string)                                      |
|   o array of 32-bit integers                                                 |
|   o arbitrary array of bytes                                                 |
|   o file identifier                                                          |
| Of course, since you can store an arbitrary array of bytes, you can put      |
| any data you like into a registry (although you have the burden of           |
| encoding/decoding your data in that case).                                   |
|                                                                              |
| Each branch of the tree is labelled with a string "key."  The entire path    |
| from a given point of the tree to another point further down can be          |
| presented as a single string composed of each branch's label, concatenated   |
| to the next, with an intervening forward slash ('/').  The term "key"        |
| refers to both specific tree branch labels and to such concatenated paths.   |
|                                                                              |
| The branches from a given node must have unique labels.  Distinct nodes can  |
| have branches with the same label.                                           |
|                                                                              |
| For example, here's a small registry tree:                                   |
|                                            |                                 |
|                                           /\                                 |
|                                         /    \                               |
|                                       /        \                             |
|                                     /            \                           |
|                              "Classes"          "Users"                      |
|                                 /                  \                         |
|                               /                      \                       |
|                             /                      ["joe"]                   |
|                           /                          / \                     |
|                          |                          /   \                    |
|                         /\                         /     \                   |
|                       /    \                    "joe"   "bob"                |
|                     /        \                    /       \                  |
|                                                  /         \                 |
|        "{xxxx-xx-1}"       "{xxxx-xx-2}"  ["c:/joe"]   ["d:/Robert"]         |
|             |                    |                                           |
|            /\                   /\                                           |
|          /    \               /    \                                         |
|        /        \           /        \                                       |
|    "Library" "Version"  "Library" "Version"                                  |
|       /         \          /          \                                      |
| ["foo.dll"]      2   ["bar.dll"]       1                                     |
|                                                                              |
| In this example, there are 2 keys under the root: "Classes" and "Users".     |
| The first denotes a subtree only (which has two subtrees, ...).  The second  |
| denotes both a value ["joe"] and two subtrees labelled "joe" and "bob".      |
| The value at the node "/Users" is ["joe"], at "/Users/bob" is ["d:/Robert"]. |
| The value at "/Classes/{xxxx-xx-1}/Version" is 2.                            |
|                                                                              |
| The registry interface provides functions that let you navigate the tree     |
| and manipulate it's contents.                                                |
|                                                                              |
| Please note that the registry itself does not impose any structure or        |
| meaning on the contents of the tree.  For example, the registry doesn't      |
| control whether the value at the key "/Users" is the label for the subtree   |
| with information about the last active user.  That meaning is applied by     |
| the code that stores these values and uses them for that purpose.            |
|                                                                              |
| [Any resemblence between this example and actual contents of any actual      |
| registry is purely coincidental.]                                            |
------------------------------------------------------------------------------*/
struct nsIRegistry : public nsISupports {
    /*------------------------------ Constants ---------------------------------
    | The following enumerated types and values are used by the registry       |
    | interface.                                                               |
    --------------------------------------------------------------------------*/
    typedef enum {
        String = 1,
        Int32,
        Bytes,
        File
    } DataType;

    /*-------------------------------- Types -----------------------------------
    | The following data types are used by this interface.  All are basically  |
    | opaque types.  You obtain objects of these types via certain member      |
    | function calls and re-use them later (without having to know what they   |
    | contain).                                                                |
    |                                                                          |
    |   Key       - Placeholder to represent a particular node in a registry   |
    |               tree.  There are 3 enumerated values that correspond to    |
    |               specific nodes:                                            |
    |                   Common      - Where most stuff goes.                   |
    |                   Users       - Special subtree to hold info about       |
    |                                 "users"; if you don't know what goes     |
    |                                 here, don't mess with it.                |
    |                   CurrentUser - Subtree under Users corresponding to     |
    |                                 whatever user is designed the "current"  |
    |                                 one; see note above.                     |
    |               You can specify any of these enumerated values as "keys"   |
    |               on any member function that takes a nsRegistry::Key.       |
    |   ValueInfo - Structure describing a registry value.                     |
    --------------------------------------------------------------------------*/
    typedef uint32 Key;

    enum { Users = 1, Common = 2, CurrentUser = 3 };

    struct ValueInfo {
        DataType type;
        uint32   length;
    };

  static const nsIID& GetIID() { static nsIID iid = NS_IREGISTRY_IID; return iid; }

    /*--------------------------- Opening/Closing ------------------------------
    | These functions open the specified registry file (Open() with a non-null |
    | argument) or the default "standard" registry file (Open() with a null    |
    | argument or OpenDefault()).                                              |
    |                                                                          |
    | Once opened, you can access the registry contents via the read/write     |
    | or query functions.                                                      |
    |                                                                          |
    | The registry file will be closed automatically when the registry object  |
    | is destroyed.  You can close the file prior to that by using the         |
    | Close() function.                                                        |
    --------------------------------------------------------------------------*/
    NS_IMETHOD Open( const char *regFile = 0 ) = 0;
    NS_IMETHOD OpenDefault() = 0;
    NS_IMETHOD Close() = 0;

    /*----------------------- Reading/Writing Values ---------------------------
    | These functions read/write the registry values at a given node.          |
    |                                                                          |
    | All functions require you to specify where in the registry key to        |
    | get/set the value.  The location is specified using two components:      |
    |   o A "base key" indicating where to start from; this is a value of type |
    |     nsIRegistry::Key.  You use either one of the special "root" key      |
    |     values or a subkey obtained via some other member function call.     |
    |   o A "relative path," expressed as a sequence of subtree names          |
    |     separated by forward slashes.  This path describes how to get from   |
    |     the base key to the node at which you want to store the data.  This  |
    |     component can be a null pointer which means the value goes directly  |
    |     at the node denoted by the base key.                                 |
    |                                                                          |
    | When you request a value of a given type, the data stored at the         |
    | specified node must be of the type requested.  If not, an error results. |
    |                                                                          |
    | GetString - Obtains a newly allocated copy of a string type value.  The  |
    |             caller is obligated to free the returned string using        |
    |             PR_Free.                                                     |
    | SetString - Stores the argument string at the specified node.            |
    | GetInt    - Obtains an int32 value at the specified node.  The result    |
    |             is returned into an int32 location you specify.              |
    | SetInt    - Stores a given int32 value at a node.                        |
    | GetBytes  - Obtains a byte array value; this returns both an allocated   |
    |             array of bytes and a length (necessary because there may be  |
    |             embedded null bytes in the array).  You must free the        |
    |             resulting array using PR_Free.                               |
    | SetBytes  - Stores a given array of bytes; you specify the bytes via a   |
    |             pointer and a length.                                        |
    | GetIntArray - Obtains the array of int32 values stored at a given node.  |
    |             The result is composed of two values: a pointer to an        |
    |             array of integer values (which must be freed using           |
    |             PR_Free) and the number of elements in that array.           |
    | SetIntArray - Stores a set of int32 values at a given node.  You must    |
    |             provide a pointer to the array and the number of entries.    |
    --------------------------------------------------------------------------*/
    NS_IMETHOD GetString( Key baseKey, const char *path, char **result ) = 0;
    NS_IMETHOD SetString( Key baseKey, const char *path, const char *value ) = 0;
    NS_IMETHOD GetInt( Key baseKey, const char *path, int32 *result ) = 0;
    NS_IMETHOD SetInt( Key baseKey, const char *path, int32 value ) = 0;
    NS_IMETHOD GetBytes( Key baseKey, const char *path, void **result, uint32 *len ) = 0;
    NS_IMETHOD SetBytes( Key baseKey, const char *path, void *value, uint32 len ) = 0;
    NS_IMETHOD GetIntArray( Key baseKey, const char *path, int32 **result, uint32 *len ) = 0;
    NS_IMETHOD SetIntArray( Key baseKey, const char *path, const int32 *value, uint32 len ) = 0;

    /*------------------------------ Navigation --------------------------------
    | These functions let you navigate through the registry tree, querying     |
    | its contents.                                                            |
    |                                                                          |
    | As above, all these functions requires a starting tree location ("base   |
    | key")  specified as a nsIRegistry::Key.  Some also require a path        |
    | name to locate the registry node location relative to this base key.     |
    |                                                                          |
    | AddSubtree           - Adds a new registry subtree at the specified      |
    |                        location.  Returns the resulting key in           |
    |                        the location specified by the third argument      |
    |                        (unless that pointer is 0).                       |
    | RemoveNode           - Removes the specified registry subtree or         |
    |                        value at the specified location.                  |
    | GetSubtree           - Returns a nsIRegistry::Key that can be used       |
    |                        to refer to the specified registry location.      |
    | EnumerateSubtrees    - Returns a nsIEnumerator object that you can       |
    |                        use to enumerate all the subtrees descending      |
    |                        from a specified location.  You must free the     |
    |                        enumerator via Release() when you're done with    |
    |                        it.                                               |
    | EnumerateAllSubtrees - Like EnumerateSubtrees, but will recursively      |
    |                        enumerate lower-level subtrees, too.              |
    | GetValueInfo         - Returns a uint32 value that designates the type   |
    |                        of data stored at this location in the registry;  |
    |                        the possible values are defined by the enumerated |
    |                        type nsIRegistry::DataType.                       |
    | GetValueLength       - Returns a uint32 value that indicates the length  |
    |                        of this registry value; the length is the number  |
    |                        of characters (for Strings), the number of bytes  |
    |                        (for Bytes), or the number of int32 values (for   |
    |                        Int32).                                           |
    | EnumerateValues      - Returns a nsIEnumerator that you can use to       |
    |                        enumerate all the value nodes descending from     |
    |                        a specified location.                             |
    --------------------------------------------------------------------------*/
    NS_IMETHOD AddSubtree( Key baseKey, const char *path, Key *result ) = 0;
    NS_IMETHOD RemoveSubtree( Key baseKey, const char *path ) = 0;
    NS_IMETHOD GetSubtree( Key baseKey, const char *path, Key *result ) = 0;

    NS_IMETHOD EnumerateSubtrees( Key baseKey, nsIEnumerator **result ) = 0;
    NS_IMETHOD EnumerateAllSubtrees( Key baseKey, nsIEnumerator **result ) = 0;

    NS_IMETHOD GetValueType( Key baseKey, const char *path, uint32 *result ) = 0;
    NS_IMETHOD GetValueLength( Key baseKey, const char *path, uint32 *result ) = 0;

    NS_IMETHOD EnumerateValues( Key baseKey, nsIEnumerator **result ) = 0;

    /*------------------------------ User Name ---------------------------------
    | These functions manipulate the current "user name."  This value controls |
    | the behavior of certain registry functions (namely, ?).                  |
    |                                                                          |
    | GetCurrentUserName allocates a copy of the current user name (which the  |
    | caller should free using PR_Free).                                       |
    --------------------------------------------------------------------------*/
    NS_IMETHOD GetCurrentUserName( char **result ) = 0;
    NS_IMETHOD SetCurrentUserName( const char *name ) = 0;

    /*------------------------------ Utilities ---------------------------------
    | Various utility functions:                                               |
    |                                                                          |
    | Pack() is used to compress the contents of an open registry file.        |
    --------------------------------------------------------------------------*/
    NS_IMETHOD Pack() = 0;

}; // nsIRegistry

/*------------------------------ nsIRegistryNode -------------------------------
| This interface is implemented by all the objects obtained from the           |
| nsIEnumerators that nsIRegistry provides when you call either of the         |
| subtree enumeration functions EnumerateSubtrees or EnumerateAllSubtrees.     |
|                                                                              |
| You can call this function to get the name of this subtree.  This is the     |
| relative path from the base key from which you got this interface.           |
|                                                                              |
|   GetName - Returns the path name of this node; this is the relative path    |
|             from the base key from which this subtree was obtained.  The     |
|             function allocates a copy of the name; the caller must free it   |
|             using PR_Free.                                                   |
------------------------------------------------------------------------------*/
struct nsIRegistryNode : public nsISupports {
    NS_IMETHOD GetName( char **result ) = 0;
}; // nsIRegistryNode

/*------------------------------ nsIRegistryValue ------------------------------
| This interface is implemented by the objects obtained from the               |
| nsIEnumerators that nsIRegistry provides when you call the                   |
| EnumerateValues function.  An object supporting this interface is            |
| returned when you call the CurrentItem() function on that enumerator.        |
|                                                                              |
| You use the member functions of this interface to obtain information         |
| about each registry value.                                                   |
|                                                                              |
|   GetName      - Returns the path name of this node; this is the relative    |
|                  path\ from the base key from which this value was obtained. |
|                  The function allocates a copy of the name; the caller must  |
|                  subsequently free it via PR_Free.                           |
|   GetValueType - Returns (into a location provided by the caller) the type   |
|                  of the value; the types are defined by the enumerated       |
|                  type nsIRegistry::DataType.                                 |
|   GetValueLength - Returns a uint32 value that indicates the length          |
|                    of this registry value; the length is the number          |
|                    of characters (for Strings), the number of bytes          |
|                    (for Bytes), or the number of int32 values (for           |
|                    Int32).                                                   |
------------------------------------------------------------------------------*/
struct nsIRegistryValue : public nsISupports {
    NS_IMETHOD GetName( char **result ) = 0;
    NS_IMETHOD GetValueType( uint32 *result ) = 0;
    NS_IMETHOD GetValueLength( uint32 *result ) = 0;
}; // nsIRegistryEntry


/*------------------------------- Error Codes ----------------------------------
------------------------------------------------------------------------------*/
#define NS_ERROR_REG_BADTYPE          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 1 )
#define NS_ERROR_REG_NO_MORE          NS_ERROR_GENERATE_SUCCESS( NS_ERROR_MODULE_REG, 2 )
#define NS_ERROR_REG_NOT_FOUND        NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 3 )
#define NS_ERROR_REG_NOFILE	          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 4 )
#define NS_ERROR_REG_BUFFER_TOO_SMALL NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 5 )
#define NS_ERROR_REG_NAME_TOO_LONG    NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 6 )
#define NS_ERROR_REG_NO_PATH          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 7 )
#define NS_ERROR_REG_READ_ONLY        NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 8 )
#define NS_ERROR_REG_BAD_UTF8         NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 9 )

#endif
