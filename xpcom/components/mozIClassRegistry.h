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
#ifndef __mozIClassRegistry_h
#define __mozIClassRegistry_h

#include "nsRepository.h"

/*---------------------------- mozIClassRegistry -------------------------------
| This interface provides access to a mapping from mnemonic interface names    |
| to shared libraries where implementations of those interfaces can be located.|
|                                                                              |
| This interface is designed to provide two things:                            |
|   1. A means of less static binding between clients and the code that        |
|      implements the XPCOM interfaces those clients use.  This accomplished   |
|      by the mapping from interface names to nsCID (and the shared libraries  |
|      that implement those classes).                                          |
|   2. A means of dynamically changing the mapping from interface name to      |
|      implementation.  The canonical example of this is to switch to a        |
|      "native" widget set versus an "XP" (implemented using gfx) set.         |
|                                                                              |
| The first goal is achieved by storing (in a "Netscape Registry" file, see    |
| nsReg.h) information that maps interface/class "names" to information about  |
| where to load implementations of those interfaces.  That information         |
| includes the interface ID, class ID (of the implementation class), and the   |
| name of the shared library that contains the implementation.                 |
|                                                                              |
| The class registry object will register those classes with the "repository"  |
| (see nsRepository.h) the first time a request is made to create an           |
| instance.  Subsequent requests will simply be forwarded to the repository    |
| after the appropriate interface and class IDs are determined.                |
|                                                                              |
| The second goal is accomplished by permitting the mnemonic interface         |
| "names" to be "overloaded", that is, mapped to distinct implementations      |
| by separate class registry objects.  Further, class registries can be        |
| cascaded: they can be chained together so that when a name is not            |
| recognized by one registry, it can pass the request to the next registry in  |
| the chain.  Users can control resolution by making the request of a          |
| registry further up/down the chain.                                          |
|                                                                              |
| For example, consider the case of "native" vs. "gfx" widgets.  This might    |
| be structured by a class registry arrangment like this:                      |
|                                                                              |
|      nativeWidgetRegistry          baseWidgetRegistry                        |
|          +----------+                 +----------+                           |
|          |      ----+---------------->|          |                           |
|          +----------+                 +----------+                           |
|          |toolbar| -+-----+           |toolbar| -+-----+                     |
|          +----------+     |           +----------+     |                     |
|          |button | -+-----+           |button | -+-----+                     |
|          +----------+     |           +----------+     |                     |
|                           V                            V                     |
|                  +-----------------+          +-----------------+            |
|                  |   native.dll    |          |    base.dll     |            |
|                  +-----------------+          +-----------------+            |
|                                                                              |
|  If a specialized implementation of widgets is present (e.g., native.dll)    |
|  then a corresponding class registry object is created and added to the      |
|  head of the registry chain.  Object creation requests (normal ones) are     |
|  resolved to the native implementation.  If such a library is not present,   |
|  then the resolution is to the base implementation.  If objects of the       |
|  base implementation are required, then creation requests can be directed    |
|  directly to the baseWidgetRegistry object, rather than the head of the      |
|  registry chain.                                                             |
|                                                                              |
|  It is intended that there be a single instance of this interface, accessed  |
|  via the Service Manager (see nsServiceManager.h).                           |
------------------------------------------------------------------------------*/
struct mozIClassRegistry : public ISupports {

    /*------------------------------ CreateInstance ----------------------------
    | Create an instance of the requested class/interface.  The interface      |
    | ID is required to specify how the result will be treated.  The class     |
    | named by aName must support this interface.  The result is placed in     |
    | ncomment aResult (NULL if the request fails).  "start" specifies the     |
    | registry at which the search for an implementation of the named          |
    | interface should start.  It defaults to 0 (indicating to start at the    |
    | head of the registry chain).                                             |
    --------------------------------------------------------------------------*/
    NS_IMETHOD CreateInstance( const char *anInterfaceName,
                               const nsIID &aIID,
                               void*      *aResult,
                               const char *start = 0 ) = 0;

    /*--------------------------- CreateEnumerator -----------------------------
    | Creates an nsIEnumerator interface object that can be used to examine    |
    | the contents of the registry.  "pattern" specifies either "*" or the     |
    | name of a specific interface that you want to query.  "result" will      |
    | be set to point to a new object (which will be freed on the last call    |
    | to its Release() member).  See nsIEnumerator.h for details on how to     |                                          
    | use the returned interface pointer.                                      |
    --------------------------------------------------------------------------*/
    NS_IMETHOD CreateEnumerator( const char     *pattern,
                                 nsIEnumerator* *result ) = 0;
}; // mozIClassRegistry


/*-------------------------- mozIClassRegistryEntry ----------------------------
| Objects of this class represent the individual elements that comprise a      |
| mozIClassRegistry interface.  You obtain such objects by applying the        |
| CreateEnumerator member function to the class registry and then applying     |
| the CurrentItem member function to the resulting nsIEnumerator interface.    |
|                                                                              |
| Each entry can be queried for the following information:                     |
|   o sub-registry name                                                        |
|   o interface name                                                           |
|   o Class ID                                                                 |
|   o IIDs implemented                                                         |
|                                                                              |
| The information obtained from the entry (specifically, the const char*       |
| strings) remains valid for the life of the entry (i.e., until you            |
| Release() it).                                                               |
|                                                                              |
| Here is an example of code that uses this interface to dump the contents     |
| of a mozIClassRegistry:                                                      |
|                                                                              |
|    mozIClassRegistry *reg = nsServiceManager::GetService( kIDRegistry );     |
|    nsIEnumerator *enum;                                                      |
|    reg->CreateEnumerator( "*", &enum );                                      |
|    for ( enum->First(); !enum->IsDone(); enum->Next(); ) {                   |
|       mozIClassRegistryEntry *entry;                                         |
|       enum->CurrentItem( &entry );                                           |
|       const char *subreg;                                                    |
|       const char *name;                                                      |
|       nsCID cid;                                                             |
|       int   numIIDs;                                                         |
|       entry->GetSubRegistryName( &subreg );                                  |
|       entry->GetInterfaceName( &name );                                      |
|       entry->GetClassID( &cid );                                             |
|       entry->GetNumIIDs( &numIIDs );                                         |
|       cout << subreg << "/" << name << " = " << cid.ToString() << endl;      |
|       for ( int i = 0; i < numIIDs; i++ ) {                                  |
|           nsIID iid;                                                         |
|           entry->GetInterfaceID( i, &iid );                                  |
|           cout << "/tIID[" << i << "] = " << iid.ToString() << endl;         |
|       }                                                                      |
|       entry->Release();                                                      |
|    }                                                                         |
|    enum->Release();                                                          |
------------------------------------------------------------------------------*/
struct mozIClassRegistryEntry : public nsISupports {
    NS_IMETHOD GetSubRegistryName( const char **result ) = 0;
    NS_IMETHOD GetInterfaceName( const char **result ) = 0;
    NS_IMETHOD GetClassID( nsCID *result ) = 0;
    NS_IMETHOD GetNumIIDs( int *result ) = 0;
    NS_IMETHOD GetInterfaceID( int n, nsIID *result ) = 0;
}; // mozIClassRegistryEntry

// {5D41A440-8E37-11d2-8059-00600811A9C3}
#define MOZ_ICLASSREGISTRY_IID { 0x5d41a440, 0x8e37, 0x11d2, { 0x80, 0x59, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }
// {D1B54831-AC07-11d2-805E-00600811A9C3}
#define MOZ_ICLASSREGISTRYENTRY_IID { 0xd1b54831, 0xac07, 0x11d2, { 0x80, 0x5e, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

#endif
