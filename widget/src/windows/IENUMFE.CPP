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

#include "ienumfe.h"
#include <stdio.h>
#include "nsISupports.h"

/*
 * CEnumFormatEtc::CEnumFormatEtc
 * CEnumFormatEtc::~CEnumFormatEtc
 *
 * Parameters (Constructor):
 *  cFE             ULONG number of FORMATETCs in pFE
 *  prgFE           LPFORMATETC to the array to enumerate.
 */

CEnumFormatEtc::CEnumFormatEtc(ULONG aNumFEs, LPFORMATETC aFEList)
{
  UINT        i;

  mRefCnt     = 0;

  mCurrentInx = 0;
  mNumFEs     = aNumFEs;
  mMaxNumFEs  = aNumFEs;
  mFEList     = new FORMATETC[(UINT)aNumFEs];

  if (NULL!=mFEList) {
    for (i=0; i < aNumFEs; i++) {
      mFEList[i] = aFEList[i];
    }
  }

}

/*
 * CEnumFormatEtc::CEnumFormatEtc
 * CEnumFormatEtc::~CEnumFormatEtc
 *
 * Parameters (Constructor):
 *  pUnkRef         LPUNKNOWN to use for reference counting.
 *  cFE             ULONG number of FORMATETCs in pFE
 */

CEnumFormatEtc::CEnumFormatEtc(ULONG aMaxFE)
{
  mRefCnt     = 0;

  mCurrentInx = 0;
  mNumFEs     = 0;
  mMaxNumFEs  = aMaxFE;
  mFEList     = new FORMATETC[(UINT)aMaxFE];
}


//----------------------------------------------------
CEnumFormatEtc::~CEnumFormatEtc(void)
{
  if (NULL != mFEList) {
    delete [] mFEList;
  }

}

//----------------------------------------------------
void CEnumFormatEtc::AddFE(LPFORMATETC aFE)
{
  mFEList[mNumFEs++] = *aFE;
}

/*
 * CEnumFormatEtc::QueryInterface
 * CEnumFormatEtc::AddRef
 * CEnumFormatEtc::Release
 *
 * Purpose:
 *  IUnknown members for CEnumFormatEtc object.  For QueryInterface
 *  we only return out own interfaces and not those of the data
 *  object.  However, since enumerating formats only makes sense
 *  when the data object is around, we insure that it stays as
 *  long as we stay by calling an outer IUnknown for AddRef
 *  and Release.  But since we are not controlled by the lifetime
 *  of the outer object, we still keep our own reference count in
 *  order to free ourselves.
 */

STDMETHODIMP CEnumFormatEtc::QueryInterface(REFIID riid, LPVOID *ppv)
{
  *ppv=NULL;

  /*
   * Enumerators are separate objects, not the data object, so
   * we only need to support out IUnknown and IEnumFORMATETC
   * interfaces here with no concern for aggregation.
   */
  if (IsEqualIID(riid, IID_IUnknown)
      || IsEqualIID(riid, IID_IEnumFORMATETC))
      *ppv=(LPVOID)this;

  //AddRef any interface we'll return.
  if (NULL!=*ppv)
      {
      ((LPUNKNOWN)*ppv)->AddRef();
      return NOERROR;
      }

  return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CEnumFormatEtc::AddRef(void)
{
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "CEnumFormatEtc",sizeof(*this));
#ifdef DEBUG
  //printf("CEnumFormatEtc::AddRef >>>>>>>>>>>>>>>>>> %d on %p\n", mRefCnt, this);
#endif
  return mRefCnt;
}

STDMETHODIMP_(ULONG) CEnumFormatEtc::Release(void)
{
  ULONG cRefT;

  cRefT = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "CEnumFormatEtc");

  if (0L == mRefCnt)
      delete this;

#ifdef DEBUG
  //printf("CEnumFormatEtc::Release >>>>>>>>>>>>>>>>>> %d on %p\n", mRefCnt, this);
#endif
  return cRefT;
}


/*
 * CEnumFormatEtc::Next
 *
 * Purpose:
 *  Returns the next element in the enumeration.
 *
 * Parameters:
 *  cFE             ULONG number of FORMATETCs to return.
 *  pFE             LPFORMATETC in which to store the returned
 *                  structures.
 *  pulFE           ULONG * in which to return how many we
 *                  enumerated.
 *
 * Return Value:
 *  HRESULT         NOERROR if successful, S_FALSE otherwise,
 */

STDMETHODIMP CEnumFormatEtc::Next(ULONG cFE, LPFORMATETC pFE, ULONG * pulFE)
{
  ULONG               cReturn=0L;

  if (NULL==mFEList)
      return ResultFromScode(S_FALSE);

  if (NULL==pulFE)
      {
      if (1L!=cFE)
          return ResultFromScode(E_POINTER);
      }
  else
      *pulFE=0L;

  if (NULL==pFE || mCurrentInx >= mNumFEs)
      return ResultFromScode(S_FALSE);

  while (mCurrentInx < mNumFEs && cFE > 0)
      {
      *pFE++=mFEList[mCurrentInx++];
      cReturn++;
      cFE--;
      }

  if (NULL!=pulFE)
      *pulFE=cReturn;

  return NOERROR;
}

/*
 * CEnumFormatEtc::Skip
 *
 * Purpose:
 *  Skips the next n elements in the enumeration.
 *
 * Parameters:
 *  cSkip           ULONG number of elements to skip.
 *
 * Return Value:
 *  HRESULT         NOERROR if successful, S_FALSE if we could not
 *                  skip the requested number.
 */

STDMETHODIMP CEnumFormatEtc::Skip(ULONG cSkip)
{
  if (((mCurrentInx+cSkip) >= mNumFEs) || NULL==mFEList)
      return ResultFromScode(S_FALSE);

  mCurrentInx+=cSkip;
  return NOERROR;
}

/*
 * CEnumFormatEtc::Reset
 *
 * Purpose:
 *  Resets the current element index in the enumeration to zero.
 *
 * Parameters:
 *  None
 *
 * Return Value:
 *  HRESULT         NOERROR
 */

STDMETHODIMP CEnumFormatEtc::Reset(void)
{
  mCurrentInx=0;
  return NOERROR;
}

/*
 * CEnumFormatEtc::Clone
 *
 * Purpose:
 *  Returns another IEnumFORMATETC with the same state as ourselves.
 *
 * Parameters:
 *  ppEnum          LPENUMFORMATETC * in which to return the
 *                  new object.
 *
 * Return Value:
 *  HRESULT         NOERROR or a general error value.
 */

STDMETHODIMP CEnumFormatEtc::Clone(LPENUMFORMATETC *ppEnum)
{
  LPCEnumFormatEtc    pNew;

  *ppEnum=NULL;

  //Create the clone
  pNew = new CEnumFormatEtc(mNumFEs, mFEList);

  if (NULL==pNew)
      return ResultFromScode(E_OUTOFMEMORY);

  pNew->AddRef();
  pNew->mCurrentInx=mCurrentInx;

  *ppEnum=pNew;
  return NOERROR;
}
