/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsIEnumerator.h"

class nsConjoiningEnumerator : public nsIBidirectionalEnumerator
{
public:
  NS_DECL_ISUPPORTS

  // nsIEnumerator methods:
  NS_IMETHOD First(void);
  NS_IMETHOD Next(void);
  NS_IMETHOD CurrentItem(nsISupports **aItem);
  NS_IMETHOD IsDone(void);

  // nsIBidirectionalEnumerator methods:
  NS_IMETHOD Last(void);
  NS_IMETHOD Prev(void);

  // nsConjoiningEnumerator methods:
  nsConjoiningEnumerator(nsIEnumerator* first, nsIEnumerator* second);
  virtual ~nsConjoiningEnumerator(void);

protected:
  nsIEnumerator* mFirst;
  nsIEnumerator* mSecond;
  nsIEnumerator* mCurrent;
};

nsConjoiningEnumerator::nsConjoiningEnumerator(nsIEnumerator* first, nsIEnumerator* second)
  : mFirst(first), mSecond(second), mCurrent(first)
{
  NS_ADDREF(mFirst);
  NS_ADDREF(mSecond);
}

nsConjoiningEnumerator::~nsConjoiningEnumerator(void)
{
  NS_RELEASE(mFirst);
  NS_RELEASE(mSecond);
}

NS_IMPL_ADDREF(nsConjoiningEnumerator);
NS_IMPL_RELEASE(nsConjoiningEnumerator);

NS_IMETHODIMP
nsConjoiningEnumerator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER; 

  if (aIID.Equals(nsIBidirectionalEnumerator::IID()) || 
      aIID.Equals(nsIEnumerator::IID()) || 
      aIID.Equals(nsISupports::IID())) {
    *aInstancePtr = (void*) this; 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  return NS_NOINTERFACE; 
}

NS_IMETHODIMP 
nsConjoiningEnumerator::First(void)
{
  mCurrent = mFirst;
  return mCurrent->First();
}

NS_IMETHODIMP 
nsConjoiningEnumerator:: Next(void)
{
  nsresult rv = mCurrent->Next();
  if (NS_FAILED(rv) && mCurrent == mFirst) {
    mCurrent = mSecond;
    rv = mCurrent->First();
  }
  return rv;
}

NS_IMETHODIMP 
nsConjoiningEnumerator:: CurrentItem(nsISupports **aItem)
{
  return mCurrent->CurrentItem(aItem);
}

NS_IMETHODIMP 
nsConjoiningEnumerator:: IsDone(void)
{
  return (mCurrent == mFirst && mCurrent->IsDone())
      || (mCurrent == mSecond && mCurrent->IsDone());
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsConjoiningEnumerator:: Last(void)
{
  nsresult rv;
  nsIBidirectionalEnumerator* be;
  rv = mSecond->QueryInterface(nsIBidirectionalEnumerator::IID(), (void**)&be);
  if (NS_FAILED(rv)) return rv;
  mCurrent = mSecond;
  rv = be->Last();
  NS_RELEASE(be);
  return rv;
}

NS_IMETHODIMP 
nsConjoiningEnumerator:: Prev(void)
{
  nsresult rv;
  nsIBidirectionalEnumerator* be;
  rv = mCurrent->QueryInterface(nsIBidirectionalEnumerator::IID(), (void**)&be);
  if (NS_FAILED(rv)) return rv;
  rv = be->Prev();
  NS_RELEASE(be);
  if (NS_FAILED(rv) && mCurrent == mSecond) {
    rv = mFirst->QueryInterface(nsIBidirectionalEnumerator::IID(), (void**)&be);
    if (NS_FAILED(rv)) return rv;
    mCurrent = mFirst;
    rv = be->Last();
    NS_RELEASE(be);
  }  
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewConjoiningEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                           nsIBidirectionalEnumerator* *aInstancePtrResult)
{
  if (aInstancePtrResult == 0)
    return NS_ERROR_NULL_POINTER;
  nsConjoiningEnumerator* e = new nsConjoiningEnumerator(first, second);
  if (e == 0)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(e);
  *aInstancePtrResult = e;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
