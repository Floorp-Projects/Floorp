/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "IEnumFE.h"
#include "nsAlgorithm.h"

CEnumFormatEtc::CEnumFormatEtc() :
  mRefCnt(0),
  mCurrentIdx(0)
{
}

// Constructor used by Clone()
CEnumFormatEtc::CEnumFormatEtc(nsTArray<FormatEtc>& aArray) :
  mRefCnt(0),
  mCurrentIdx(0)
{
  // a deep copy, calls FormatEtc's copy constructor on each
  mFormatList.AppendElements(aArray);
}

CEnumFormatEtc::~CEnumFormatEtc()
{
}

/* IUnknown impl. */

STDMETHODIMP
CEnumFormatEtc::QueryInterface(REFIID riid, LPVOID *ppv)
{
  *ppv = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_IEnumFORMATETC))
      *ppv = (LPVOID)this;

  if (*ppv == NULL)
      return E_NOINTERFACE;

  // AddRef any interface we'll return.
  ((LPUNKNOWN)*ppv)->AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG)
CEnumFormatEtc::AddRef()
{
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "CEnumFormatEtc",sizeof(*this));
  return mRefCnt;
}

STDMETHODIMP_(ULONG)
CEnumFormatEtc::Release()
{
  PRUint32 refReturn;

  refReturn = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "CEnumFormatEtc");

  if (mRefCnt == 0)
      delete this;

  return refReturn;
}

/* IEnumFORMATETC impl. */

STDMETHODIMP
CEnumFormatEtc::Next(ULONG aMaxToFetch, FORMATETC *aResult, ULONG *aNumFetched)
{
  // If the method retrieves the number of items requested, the return
  // value is S_OK. Otherwise, it is S_FALSE.

  if (aNumFetched)
      *aNumFetched = 0;

  // aNumFetched can be null if aMaxToFetch is 1
  if (!aNumFetched && aMaxToFetch > 1)
      return S_FALSE;

  if (!aResult)
      return S_FALSE;

  // We're done walking the list
  if (mCurrentIdx >= mFormatList.Length())
      return S_FALSE;

  PRUint32 left = mFormatList.Length() - mCurrentIdx;

  if (!aMaxToFetch)
      return S_FALSE;

  PRUint32 count = NS_MIN(static_cast<PRUint32>(aMaxToFetch), left);

  PRUint32 idx = 0;
  while (count > 0) {
      // Copy out to aResult
      mFormatList[mCurrentIdx++].CopyOut(&aResult[idx++]);
      count--;
  }

  if (aNumFetched)
      *aNumFetched = idx;

  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Skip(ULONG aSkipNum)
{
  // If the method skips the number of items requested, the return value is S_OK.
  // Otherwise, it is S_FALSE.

  if ((mCurrentIdx + aSkipNum) >= mFormatList.Length())
      return S_FALSE;

  mCurrentIdx += aSkipNum;

  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Reset(void)
{
  mCurrentIdx = 0;
  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Clone(LPENUMFORMATETC *aResult)
{
  // Must return a new IEnumFORMATETC interface with the same iterative state.

  if (!aResult)
      return E_INVALIDARG;

  CEnumFormatEtc * pEnumObj = new CEnumFormatEtc(mFormatList);

  if (!pEnumObj)
      return E_OUTOFMEMORY;

  pEnumObj->AddRef();
  pEnumObj->SetIndex(mCurrentIdx);

  *aResult = pEnumObj;

  return S_OK;
}

/* utils */

void
CEnumFormatEtc::AddFormatEtc(LPFORMATETC aFormat)
{
  if (!aFormat)
      return;
  FormatEtc * etc = mFormatList.AppendElement();
  // Make a copy of aFormat
  if (etc)
      etc->CopyIn(aFormat);
}

/* private */

void
CEnumFormatEtc::SetIndex(PRUint32 aIdx)
{
  mCurrentIdx = aIdx;
}
