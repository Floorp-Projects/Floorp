/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsXPComFactory_h__
#define nsXPComFactory_h__

#include "nsIFactory.h"

/*
 * This file contains a templatized implementation of a simple XPCOM factory.
 *
 * To implement a factory for a given component, just provide a function 
 * like the one below which can be called by your DLL's NSGetFactory(...)
 * entry point...
 *
 *    nsresult NS_New_SomeComponent_Factory(nsIFactory** aResult)
 *    {
 *      nsresult rv = NS_OK;
 *      nsIFactory* inst = new nsFactory<SomeComponent>();
 *      if (NULL == inst) {
 *        rv = NS_ERROR_OUT_OF_MEMORY;
 *      } else {
 *        NS_ADDREF(inst);
 *      }
 *      *aResult = inst;
 *      return rv;
 *    }
 *
 * NOTE:
 * ----
 *  The factories created by this template are not thread-safe and do not
 *  support aggregation.
 *
 */
template <class T> class nsFactory : public nsIFactory
{
public:
  nsFactory() { NS_INIT_REFCNT(); }
  
  //
  // nsISupports interface...
  //
  // These implementations were copied from the NS_IMPL_ISUPPORTS macro
  // in nsISupports.h
  //
  NS_IMETHOD_(nsrefcnt) AddRef (void) 
  { 
    return ++mRefCnt; 
  }
  
  NS_IMETHOD_(nsrefcnt) Release(void)
  {
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    if (--mRefCnt == 0) {
      NS_DELETEXPCOM(this);
      return 0;
    }
    return mRefCnt;
  }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr)
  {
    if (NULL == aInstancePtr) {
      return NS_ERROR_NULL_POINTER;
    }

    *aInstancePtr = NULL;

    static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(kIFactoryIID)) {
      *aInstancePtr = (void*) this;
      NS_ADDREF_THIS();
      return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = (void*) ((nsISupports*)this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  //
  // nsIFactory interface...
  //
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult)
  {
    nsresult rv;
    T* inst;
    // Parameter validation...
    if (NULL == aResult) {
      rv = NS_ERROR_NULL_POINTER;
      goto done;
    }
    // Do not support aggregatable components...
    *aResult = NULL;
    if (NULL != aOuter) {
      rv = NS_ERROR_NO_AGGREGATION;
      goto done;
    }
    // Create a new instance of the component...
    NS_NEWXPCOM(inst, T);
    if (NULL == inst) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    // If the QI fails, the component will be destroyed...
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

  done:
    return rv;
  }

  NS_IMETHOD LockFactory(PRBool aLock)
  {
    // Not implemented in simplest case.
    return NS_OK;
  }


protected:
  virtual ~nsFactory()
  {
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  }
  
  // Reference count variable used by nsISupports...
  nsrefcnt mRefCnt;
};


#endif /* nsXPComFactory_h__ */
