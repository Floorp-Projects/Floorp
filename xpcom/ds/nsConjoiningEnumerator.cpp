/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIEnumerator.h"

////////////////////////////////////////////////////////////////////////////////
// Intersection Enumerators
////////////////////////////////////////////////////////////////////////////////

class nsConjoiningEnumerator : public nsIBidirectionalEnumerator
{
public:
  NS_DECL_ISUPPORTS

  // nsIEnumerator methods:
  NS_DECL_NSIENUMERATOR

  // nsIBidirectionalEnumerator methods:
  NS_DECL_NSIBIDIRECTIONALENUMERATOR

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

NS_IMPL_ISUPPORTS2(nsConjoiningEnumerator, nsIBidirectionalEnumerator, nsIEnumerator)

NS_IMETHODIMP 
nsConjoiningEnumerator::First(void)
{
  mCurrent = mFirst;
  return mCurrent->First();
}

NS_IMETHODIMP 
nsConjoiningEnumerator::Next(void)
{
  nsresult rv = mCurrent->Next();
  if (NS_FAILED(rv) && mCurrent == mFirst) {
    mCurrent = mSecond;
    rv = mCurrent->First();
  }
  return rv;
}

NS_IMETHODIMP 
nsConjoiningEnumerator::CurrentItem(nsISupports **aItem)
{
  return mCurrent->CurrentItem(aItem);
}

NS_IMETHODIMP 
nsConjoiningEnumerator::IsDone(void)
{
  return (mCurrent == mFirst && mCurrent->IsDone() == NS_OK)
      || (mCurrent == mSecond && mCurrent->IsDone() == NS_OK)
    ? NS_OK : NS_ENUMERATOR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsConjoiningEnumerator::Last(void)
{
  nsresult rv;
  nsIBidirectionalEnumerator* be;
  rv = mSecond->QueryInterface(NS_GET_IID(nsIBidirectionalEnumerator), (void**)&be);
  if (NS_FAILED(rv)) return rv;
  mCurrent = mSecond;
  rv = be->Last();
  NS_RELEASE(be);
  return rv;
}

NS_IMETHODIMP 
nsConjoiningEnumerator::Prev(void)
{
  nsresult rv;
  nsIBidirectionalEnumerator* be;
  rv = mCurrent->QueryInterface(NS_GET_IID(nsIBidirectionalEnumerator), (void**)&be);
  if (NS_FAILED(rv)) return rv;
  rv = be->Prev();
  NS_RELEASE(be);
  if (NS_FAILED(rv) && mCurrent == mSecond) {
    rv = mFirst->QueryInterface(NS_GET_IID(nsIBidirectionalEnumerator), (void**)&be);
    if (NS_FAILED(rv)) return rv;
    mCurrent = mFirst;
    rv = be->Last();
    NS_RELEASE(be);
  }  
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_COM nsresult
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

static nsresult
nsEnumeratorContains(nsIEnumerator* e, nsISupports* item)
{
  nsresult rv;
  for (e->First(); e->IsDone() != NS_OK; e->Next()) {
    nsISupports* other;
    rv = e->CurrentItem(&other);
    if (NS_FAILED(rv)) return rv;
    if (item == other) {
      NS_RELEASE(other);
      return NS_OK;     // true -- exists in enumerator
    }
    NS_RELEASE(other);
  }
  return NS_ENUMERATOR_FALSE;   // false -- doesn't exist
}

////////////////////////////////////////////////////////////////////////////////
// Intersection Enumerators
////////////////////////////////////////////////////////////////////////////////

class nsIntersectionEnumerator : public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS

  // nsIEnumerator methods:
  NS_DECL_NSIENUMERATOR

  // nsIntersectionEnumerator methods:
  nsIntersectionEnumerator(nsIEnumerator* first, nsIEnumerator* second);
  virtual ~nsIntersectionEnumerator(void);

protected:
  nsIEnumerator* mFirst;
  nsIEnumerator* mSecond;
};

nsIntersectionEnumerator::nsIntersectionEnumerator(nsIEnumerator* first, nsIEnumerator* second)
  : mFirst(first), mSecond(second)
{
  NS_ADDREF(mFirst);
  NS_ADDREF(mSecond);
}

nsIntersectionEnumerator::~nsIntersectionEnumerator(void)
{
  NS_RELEASE(mFirst);
  NS_RELEASE(mSecond);
}

NS_IMPL_ISUPPORTS1(nsIntersectionEnumerator, nsIEnumerator)

NS_IMETHODIMP 
nsIntersectionEnumerator::First(void)
{
  nsresult rv = mFirst->First();
  if (NS_FAILED(rv)) return rv;
  return Next();
}

NS_IMETHODIMP 
nsIntersectionEnumerator::Next(void)
{
  nsresult rv;

  // find the first item that exists in both
  for (; mFirst->IsDone() != NS_OK; mFirst->Next()) {
    nsISupports* item;
    rv = mFirst->CurrentItem(&item);
    if (NS_FAILED(rv)) return rv;

    // see if it also exists in mSecond
    rv = nsEnumeratorContains(mSecond, item);
    if (NS_FAILED(rv)) return rv;

    NS_RELEASE(item);
    if (rv == NS_OK) {
      // found in both, so return leaving it as the current item of mFirst
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsIntersectionEnumerator::CurrentItem(nsISupports **aItem)
{
  return mFirst->CurrentItem(aItem);
}

NS_IMETHODIMP 
nsIntersectionEnumerator::IsDone(void)
{
  return mFirst->IsDone();
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_COM nsresult
NS_NewIntersectionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                      nsIEnumerator* *aInstancePtrResult)
{
  if (aInstancePtrResult == 0)
    return NS_ERROR_NULL_POINTER;
  nsIntersectionEnumerator* e = new nsIntersectionEnumerator(first, second);
  if (e == 0)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(e);
  *aInstancePtrResult = e;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Union Enumerators
////////////////////////////////////////////////////////////////////////////////

class nsUnionEnumerator : public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS

  // nsIEnumerator methods:
  NS_DECL_NSIENUMERATOR

  // nsUnionEnumerator methods:
  nsUnionEnumerator(nsIEnumerator* first, nsIEnumerator* second);
  virtual ~nsUnionEnumerator(void);

protected:
  nsIEnumerator* mFirst;
  nsIEnumerator* mSecond;
};

nsUnionEnumerator::nsUnionEnumerator(nsIEnumerator* first, nsIEnumerator* second)
  : mFirst(first), mSecond(second)
{
  NS_ADDREF(mFirst);
  NS_ADDREF(mSecond);
}

nsUnionEnumerator::~nsUnionEnumerator(void)
{
  NS_RELEASE(mFirst);
  NS_RELEASE(mSecond);
}

NS_IMPL_ISUPPORTS1(nsUnionEnumerator, nsIEnumerator)

NS_IMETHODIMP 
nsUnionEnumerator::First(void)
{
  nsresult rv = mFirst->First();
  if (NS_FAILED(rv)) return rv;
  return Next();
}

NS_IMETHODIMP 
nsUnionEnumerator::Next(void)
{
  nsresult rv;

  // find the first item that exists in both
  for (; mFirst->IsDone() != NS_OK; mFirst->Next()) {
    nsISupports* item;
    rv = mFirst->CurrentItem(&item);
    if (NS_FAILED(rv)) return rv;

    // see if it also exists in mSecond
    rv = nsEnumeratorContains(mSecond, item);
    if (NS_FAILED(rv)) return rv;

    NS_RELEASE(item);
    if (rv != NS_OK) {
      // if it didn't exist in mSecond, return, making it the current item
      return NS_OK;
    }
    
    // each time around, make sure that mSecond gets reset to the beginning
    // so that when mFirst is done, we'll be ready to enumerate mSecond
    rv = mSecond->First();
    if (NS_FAILED(rv)) return rv;
  }
  
  return mSecond->Next();
}

NS_IMETHODIMP 
nsUnionEnumerator::CurrentItem(nsISupports **aItem)
{
  if (mFirst->IsDone() != NS_OK)
    return mFirst->CurrentItem(aItem);
  else
    return mSecond->CurrentItem(aItem);
}

NS_IMETHODIMP 
nsUnionEnumerator::IsDone(void)
{
  return (mFirst->IsDone() == NS_OK && mSecond->IsDone() == NS_OK)
    ? NS_OK : NS_ENUMERATOR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_COM nsresult
NS_NewUnionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                      nsIEnumerator* *aInstancePtrResult)
{
  if (aInstancePtrResult == 0)
    return NS_ERROR_NULL_POINTER;
  nsUnionEnumerator* e = new nsUnionEnumerator(first, second);
  if (e == 0)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(e);
  *aInstancePtrResult = e;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
