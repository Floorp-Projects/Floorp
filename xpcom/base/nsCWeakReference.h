/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsCWeakReference_h___
#define nsCWeakReference_h___

#include "nsDebug.h"

/* A set of support classes for defining weak references, rather like
   nsIWeakReference, but for use with non-COM objects.

   Expected use is when an object, call it A, holds a reference to an object B,
   and B may be unexpectedly deleted from underneath A.  To use these classes
   to solve that problem, add an nsCWeakReferent member variable to B and
   construct that member in B's constructor.  (B::B():mWeakRef(this)).
   Hold an nsCWeakReference<B> variable in A, rather than a B directly, and
   dereference B from that variable afresh each time that B may have been
   deleted.
*/

class nsCWeakProxy;

/* An object wishing to support weak references to itself has an nsCWeakReferent
   member variable and provides an accessor for getting to it.  Notice that
   nsCWeakReferent has no default constructor, so must be initialized with
   a pointer to the object.
*/
class NS_COM nsCWeakReferent {

public:
  nsCWeakReferent(void *aRealThing);
  virtual ~nsCWeakReferent();

  void SetReferent(void *aRealThing);
  nsCWeakProxy *GetProxy();
  void ProxyDeleted()
         { mProxy = 0; }

private:
  // copy and assignment constructors can't be implemented without help
  // from the containing class, so they're made inaccessible, forcing
  // the container to address this issue explicitly.
  nsCWeakReferent(const nsCWeakReferent &aOriginal);
  nsCWeakReferent& operator= (const nsCWeakReferent &aOriginal);

  void         *mRealThing;
  nsCWeakProxy *mProxy;
};

/*   The nsCWeakProxy object is an object whose creation and lifetime is
   under our control, unlike the nsCWeakReferent and its family of
   nsCWeakReferences.  An nsCWeakProxy is created by the nsCWeakReferent
   when the first weak reference is necessary, and refcounted for each
   additional reference.  It knows about the lifetime of the nsCWeakReferent,
   and deletes itself once all weak references have been broken.
     This is an internal-use class; clients need not use it or ever see it.
*/
class NS_COM nsCWeakProxy {

public:
  nsCWeakProxy(void *aRealThing, nsCWeakReferent *aReferent);
  virtual ~nsCWeakProxy();

  void *Reference()
         { return mRealPointer; }

  void AddReference()
         { ++mRefCount; }

  void ReleaseReference();
  void RealThingDeleted()
         { mRealPointer = 0; mReferent = 0; }

private:
  void            *mRealPointer;
  nsCWeakReferent *mReferent;
  PRUint32        mRefCount;
};

/* internal use only: there's no need for clients to use this class */
class nsCWeakReferenceBase {
public:
  nsCWeakReferenceBase() {};
  virtual ~nsCWeakReferenceBase() {};
};

/* This class is the actual weak reference. Clients hold one of these
   and access the actual object by dereferencing this weak reference
   using operator*, operator-> or Reference().
*/
template<class T> class nsCWeakReference : public nsCWeakReferenceBase {

public:
  nsCWeakReference()
    { mProxy = 0; }

  nsCWeakReference(nsCWeakReferent *aReferent) {
    mProxy = 0;
    SetReference(aReferent);
  }

  nsCWeakReference(const nsCWeakReference<T> &aOriginal) {
    mProxy = aOriginal.mProxy;
    if (mProxy)
      mProxy->AddReference();
  }

  nsCWeakReference<T>& operator= (const nsCWeakReference<T> &aOriginal) {
    nsCWeakProxy *temp = mProxy;
    mProxy = aOriginal.mProxy;
    if (mProxy)
      mProxy->AddReference();
    if (temp)
      temp->ReleaseReference();
    return *this;
  }

  T& operator*() const {
       NS_ASSERTION(mProxy, "weak reference used without being set");
       return (T&) *(T*)mProxy->Reference();
  }

  T* operator->() const {
       NS_ASSERTION(mProxy, "weak reference used without being set");
       return (T*) mProxy->Reference();
  }

  virtual ~nsCWeakReference() {
    mProxy->ReleaseReference();
  }

  T* Reference() {
       NS_ASSERTION(mProxy, "weak reference used without being set");
       return (T*) mProxy->Reference();
  }

  void SetReference(nsCWeakReferent *aReferent) {
    NS_ASSERTION(aReferent, "weak reference set with null referent");
    if (mProxy)
      mProxy->ReleaseReference();
    mProxy = aReferent->GetProxy();
    NS_ASSERTION(mProxy, "weak reference proxy allocation failed");
    mProxy->AddReference();
  }

private:
  nsCWeakProxy *mProxy;
};

#endif

