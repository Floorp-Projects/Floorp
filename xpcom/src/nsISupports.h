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

#ifndef nsISupports_h___
#define nsISupports_h___

#include "nsDebug.h"
#include "nsID.h"

// An "interface id" which can be used to uniquely identify a given
// interface. Primarily used as an argument to nsISupports.QueryInterface
// method.

typedef nsID nsIID;

// Define an IID
#define NS_DEFINE_IID(_name, _iidspec) \
  const nsIID _name = _iidspec

//----------------------------------------------------------------------

// IID for the nsISupports interface
// {00000000-0000-0000-c000-000000000046}
#define NS_ISUPPORTS_IID      \
{ 0x00000000, 0x0000, 0x0000, \
  {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }

#define NS_FAILED(_nsresult) ((_nsresult) & 0x80000000)
#define NS_SUCCEEDED(_nsresult) (!((_nsresult) & 0x80000000))

// Standard "it worked" return value
#define NS_OK 0

#define NS_ERROR_BASE ((nsresult) 0xC1F30000)

// Some standard error codes we use
#define NS_ERROR_OUT_OF_MEMORY (NS_ERROR_BASE + 0)
#define NS_ERROR_NO_AGGREGATION (NS_ERROR_BASE + 1)
#define NS_ERROR_NULL_POINTER (NS_ERROR_BASE + 2)
#define NS_ERROR_ILLEGAL_VALUE (NS_ERROR_BASE + 3)
#define NS_ERROR_NOT_INITIALIZED (NS_ERROR_BASE + 4)
#define NS_ERROR_ALREADY_INITIALIZED (NS_ERROR_BASE + 5)
#define NS_ERROR_NOT_IMPLEMENTED (NS_ERROR_BASE + 6)

// Generic result data type
typedef PRUint32 nsresult;

// This is returned by QueryInterface when a given interface is not
// supported.
#define NS_NOINTERFACE ((nsresult) 0x80004002L)

// Reference count values
typedef PRUint32 nsrefcnt;

// Basic component object model interface. Objects which implement
// this interface support runtime interface discovery (QueryInterface)
// and a reference counted memory model (AddRef/Release). This is
// modelled after the win32 IUnknown API.
class nsISupports {
public:
  NS_IMETHOD QueryInterface(const nsIID& aIID,
                            void** aInstancePtr) = 0;
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

//----------------------------------------------------------------------

// Some convenience macros for implementing AddRef and Release

// Declare the reference count variable and the implementations of the
// AddRef and QueryInterface methods.
#define NS_DECL_ISUPPORTS                                                   \
public:                                                                     \
  NS_IMETHOD QueryInterface(const nsIID& aIID,                              \
                            void** aInstancePtr);                           \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       \
  NS_IMETHOD_(nsrefcnt) Release(void);                                      \
protected:                                                                  \
  nsrefcnt mRefCnt;                                                         \
public:

// Initialize the reference count variable. Add this to each and every
// constructor you implement.
#define NS_INIT_REFCNT() mRefCnt = 0

// Use this macro to implement the AddRef method for a given _class
#define NS_IMPL_ADDREF(_class)                               \
nsrefcnt _class::AddRef(void)                                \
{                                                            \
  return ++mRefCnt;                                          \
}

#define NS_ADDREF(_ptr) \
 (_ptr)->AddRef()

#define NS_IF_ADDREF(_ptr)  \
((0 != (_ptr)) ? (_ptr)->AddRef() : 0);

#define NS_RELEASE(_ptr) \
 (_ptr)->Release();      \
 (_ptr) = NULL

#define NS_IF_RELEASE(_ptr)             \
 ((0 != (_ptr)) ? (_ptr)->Release() : 0); \
 (_ptr) = NULL

// Use this macro to implement the Release method for a given _class
#define NS_IMPL_RELEASE(_class)                        \
nsrefcnt _class::Release(void)                         \
{                                                      \
  if (--mRefCnt == 0) {                                \
    delete this;                                       \
    return 0;                                          \
  }                                                    \
  return mRefCnt;                                      \
}

//----------------------------------------------------------------------

// Some convenience macros for implementing QueryInterface

// This implements query interface with two assumptions: First, the
// class in question implements nsISupports and it's own interface and
// nothing else. Second, the implementation of the class's primary
// inheritance chain leads to it's own interface.
//
// _class is the name of the class implementing the method
// _classiiddef is the name of the #define symbol that defines the IID
// for the class (e.g. NS_ISUPPORTS_IID)
#define NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)                     \
nsresult _class::QueryInterface(const nsIID& aIID, void** aInstancePtr)  \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) this;                                        \
    AddRef();                                                            \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(kISupportsIID)) {                                      \
    *aInstancePtr = (void*) ((nsISupports*)this);                        \
    AddRef();                                                            \
    return NS_OK;                                                        \
  }                                                                      \
  return NS_NOINTERFACE;                                                 \
}

#define NS_IMPL_ISUPPORTS(_class,_classiiddef) \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)

#endif /* nsISupports_h___ */
