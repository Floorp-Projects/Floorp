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

/* A set of support classes for defining weak references, rather like
   nsIWeakReference, but for use with non-COM objects
*/

#include "nsCWeakReference.h"

/************ a thing supporting weak references to itself ***********/
nsCWeakReferent::nsCWeakReferent(void *aRealThing) :
                 mRealThing(aRealThing),
                 mProxy(0) {

}

nsCWeakReferent::~nsCWeakReferent() {

  if (mProxy)
    mProxy->RealThingDeleted();
}

void nsCWeakReferent::SetReferent(void *aRealThing) {

  NS_ASSERTION(!mRealThing && !mProxy, "weak referent set twice");
  mRealThing = aRealThing;
  mProxy = 0;
}

nsCWeakProxy *nsCWeakReferent::GetProxy() {

  if (!mProxy)
    mProxy = new nsCWeakProxy(mRealThing, this);
  return mProxy;
}

/************ a reference proxy whose lifetime we control ***********/
/* the nsCWeakProxy object is an object whose creation and lifetime is
   under our control, unlike the nsCWeakReferent and its family of
   nsCWeakReferences.  An nsCWeakProxy is created by the nsCWeakReferent
   when the first weak reference is necessary, and refcounted for each
   additional reference.  It knows about the lifetime of the nsCWeakReferent,
   and deletes itself once all weak references have been broken.
*/
nsCWeakProxy::nsCWeakProxy(void *aRealThing, nsCWeakReferent *aReferent) :
              mRealPointer(aRealThing),
              mReferent(aReferent),
              mRefCount(0) {

  NS_ASSERTION(aRealThing && aReferent, "weak proxy constructed with null ptr");
}

nsCWeakProxy::~nsCWeakProxy() {
  if (mReferent)
    mReferent->ProxyDeleted();
}

void nsCWeakProxy::ReleaseReference() {

  if (--mRefCount == 0) {
    delete this;
  }
}

