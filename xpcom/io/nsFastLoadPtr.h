/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla FastLoad code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (original author)
 */

#ifndef nsFastLoadPtr_h___
#define nsFastLoadPtr_h___

/**
 * Mozilla FastLoad file object pointer template type.
 *
 * Use nsFastLoadPtr<T> rather than nsCOMPtr<T> when declaring a strong XPCOM
 * ref member of a data structure that's conditionally loaded at application
 * startup.  You must be willing to tolerate the null mRawPtr test on every
 * dereference of this member pointer, or else copy it to a local to optimize
 * away the cost.
 */

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#ifndef nsIFastLoadService_h___
#include "nsIFastLoadService.h"
#endif

/**
 * nsFastLoadPtr is a template class, so we don't want a class static service
 * pointer member declared in nsFastLoadPtr, above.  Plus, we need special
 * declaration magic to export data across DLL/DSO boundaries.  So we use an
 * old-fashioned global variable that refers weakly to the one true FastLoad
 * service.  This pointer is maintained by that singleton's ctor and dtor.
 */
PR_EXPORT_DATA(nsIFastLoadService*) gFastLoadService_;

template <class T>
class nsFastLoadPtr : public nsCOMPtr<T> {
  public:
    nsDerivedSafe<T>* get() const {
        if (!this->mRawPtr) {
            gFastLoadService_->GetFastLoadReferent(
                                         NS_REINTERPRET_CAST(nsISupports**,
                                                             &this->mRawPtr));
        }
        return NS_REINTERPRET_CAST(nsDerivedSafe<T>*, this->mRawPtr);
    }

    /**
     * Deserialize an nsFastLoadPtr from aInputStream, skipping the referent
     * object, but saving the object's offset for later deserialization.
     *
     * Lowercase name _a la_ get, because it's called the same way -- not via
     * operator->().
     */
    nsresult read(nsIObjectInputStream* aInputStream) {
        return gFastLoadService_->ReadFastLoadPtr(aInputStream,
                                         NS_REINTERPRET_CAST(nsISupports**,
                                                             &this->mRawPtr));
    }

    /**
     * Serialize an nsFastLoadPtr reference and possibly the referent object,
     * if that object has not yet been serialized.
     *
     * Lowercase name _a la_ get, because it's called the same way -- not via
     * operator->().
     */
    nsresult write(nsIObjectOutputStream* aOutputStream) {
        return gFastLoadService_->WriteFastLoadPtr(aOutputStream,
                                          NS_REINTERPRET_CAST(nsISupports*,
                                                              this->mRawPtr));
    }
};

#endif // nsFastLoadPtr_h___
