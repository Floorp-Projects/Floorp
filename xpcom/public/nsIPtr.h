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

#ifndef nsIPtr_h___
#define nsIPtr_h___

#include "nsISupports.h"

/*
 * nsIPtr is an "auto-release pointer" class for nsISupports based interfaces
 *
 * It's intent is to be a "set and forget" pointer to help with managing
 * active references to nsISupports bases objects.
 * 
 * The pointer object ensures that the underlying pointer is always 
 * released whenever the value is changed or when the object leaves scope.
 *
 * Proper care needs to be taken when assigning pointers to a nsIPtr. 
 * When asigning from a C pointer (nsISupports*), the pointer presumes
 * an active reference and subsumes it. When assigning from another nsIPtr,
 * a new reference is established.
 *
 * There are 3 ways to assign a value to a nsIPtr.
 * 1) Direct construction or assignment from a C pointer.
 * 2) Direct construction or assignment form another nsIPtr.
 * 3) Usage of an "out parameter" method.
 *    a) AssignRef() releases the underlying pointer and returns a reference to it.
 *       Useful for pointer reference out paramaters.
 *    b) AssignPtr() releases the underlying pointer and returns a pointer to it.
 *    c) Query() releases the underlying pointer and returns a (void**) pointer to it.
 *       Useful for calls to QueryInterface()
 * 4) The SetAddRef() method. This is equivalent to an assignment followed by an AddRef().
 *
 *    examples:
 *
 *    class It {
 *      void    NS_NewFoo(nsIFoo** aFoo);
 *      nsIFoo* GetFoo(void);
 *      void    GetBar(nsIBar*& aBar);
 *    };
 *
 *    nsIFooPtr foo = it->GetFoo();
 *    nsIBarPtr bar;
 *
 *    it->NS_NewFoo(foo.AssignPtr());
 *    it->GetBar(bar.AssignRef());
 *    it->QueryInterface(kIFooIID, foo.Query());
 *    bar.SetAddRef(new Bar());
 *
 * Advantages:
 * Set and forget. Once a pointer is assigned to a nsIPtr, it is impossible
 * to forget to release it.
 * Always pre-initialized. You can't forget to initialize the pointer.
 *
 * Disadvantages:
 * Usage of this class doesn't eliminate the need to think about ref counts
 * and assign values properly, AddRef'ing as needed. 
 * The nsIPtr doesn't typecast exactly like a C pointer. In order to achieve
 * typecasting, it may be necessary to first cast to a C pointer of the 
 * underlying type.
 *
 */

#define NS_DEF_PTR(cls)                                                       \
class cls##Ptr {                                                              \
public:                                                                       \
  cls##Ptr(void)  : mPtr(0) {}                                                \
  cls##Ptr(const cls##Ptr& aCopy) : mPtr(aCopy.mPtr)                          \
    { NS_IF_ADDREF(mPtr);  }                                                  \
  cls##Ptr(cls* aInterface)       : mPtr(aInterface)  {}                      \
  ~cls##Ptr(void)     { NS_IF_RELEASE(mPtr);  }                               \
  cls##Ptr& operator=(const cls##Ptr& aCopy)                                  \
    { if(mPtr == aCopy.mPtr) return *this;                                    \
      NS_IF_ADDREF(aCopy.mPtr);                                               \
      NS_IF_RELEASE(mPtr);                                                    \
      mPtr = aCopy.mPtr;  return *this; }                                     \
  cls##Ptr& operator=(cls* aInterface)                                        \
    { if(mPtr == aInterface) return *this;                                    \
      NS_IF_RELEASE(mPtr); mPtr = aInterface;                                 \
      return *this; }                                                         \
  cls##Ptr& operator=(PRInt32 aInt)                                           \
    { NS_IF_RELEASE(mPtr);                                                    \
      return *this; }                                                         \
  void  SetAddRef(cls* aInterface)                                            \
    { if(aInterface == mPtr)  return;                                         \
      NS_IF_ADDREF(aInterface);                                               \
      NS_IF_RELEASE(mPtr); mPtr = aInterface;  }                              \
  cls*  AddRef(void)  { NS_ADDREF(mPtr); return mPtr;  }                      \
  cls*  IfAddRef(void)                                                        \
    { NS_IF_ADDREF(mPtr); return mPtr;  }                                     \
  cls*& AssignRef(void)                                                       \
    { NS_IF_RELEASE(mPtr);  return mPtr; }                                    \
  cls** AssignPtr(void)                                                       \
    { NS_IF_RELEASE(mPtr);  return &mPtr; }                                   \
  void** Query(void)                                                          \
    { NS_IF_RELEASE(mPtr);  return (void**)&mPtr; }                           \
  PRBool  IsNull() const                                                      \
    { return PRBool(0 == mPtr); }                                             \
  PRBool  IsNotNull() const                                                   \
    { return PRBool(0 != mPtr); }                                             \
  PRBool  operator==(const cls##Ptr& aCopy) const                             \
    { return PRBool(mPtr == aCopy.mPtr);  }                                   \
  PRBool  operator==(cls* aInterface) const                                   \
    { return PRBool(mPtr == aInterface);  }                                   \
  PRBool  operator!=(const cls##Ptr& aCopy) const                             \
    { return PRBool(mPtr != aCopy.mPtr);  }                                   \
  PRBool  operator!=(cls* aInterface) const                                   \
    { return PRBool(mPtr != aInterface);  }                                   \
  cls*  operator->(void)  { return mPtr;  }                                   \
  cls&  operator*(void)   { return *mPtr; }                                   \
  operator cls*(void)     { return mPtr;  }                                   \
  const cls*  operator->(void) const  { return mPtr;  }                       \
  const cls&  operator*(void) const   { return *mPtr; }                       \
  operator const cls*(void) const     { return mPtr;  }                       \
private:                                                                      \
  void* operator new(size_t size) { return 0; }                               \
  void operator delete(void* aPtr)  {}                                        \
  cls*  mPtr;                                                                 \
public:                                                                       \
friend inline PRBool operator==(const cls* aInterface, const cls##Ptr& aPtr)  \
{ return PRBool(aInterface == aPtr.mPtr); }                                   \
friend inline PRBool operator!=(const cls* aInterface, const cls##Ptr& aPtr)  \
{ return PRBool(aInterface != aPtr.mPtr); }                                   \
}

#endif // nsIPtr_h___

