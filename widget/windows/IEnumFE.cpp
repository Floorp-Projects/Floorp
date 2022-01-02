/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IEnumFE.h"
#include "nsAlgorithm.h"
#include <algorithm>

CEnumFormatEtc::CEnumFormatEtc() : mRefCnt(0), mCurrentIdx(0) {}

// Constructor used by Clone()
CEnumFormatEtc::CEnumFormatEtc(nsTArray<FormatEtc>& aArray)
    : mRefCnt(0), mCurrentIdx(0) {
  // a deep copy, calls FormatEtc's copy constructor on each
  mFormatList.AppendElements(aArray);
}

CEnumFormatEtc::~CEnumFormatEtc() {}

/* IUnknown impl. */

STDMETHODIMP
CEnumFormatEtc::QueryInterface(REFIID riid, LPVOID* ppv) {
  *ppv = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumFORMATETC))
    *ppv = (LPVOID)this;

  if (*ppv == nullptr) return E_NOINTERFACE;

  // AddRef any interface we'll return.
  ((LPUNKNOWN)*ppv)->AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG)
CEnumFormatEtc::AddRef() {
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "CEnumFormatEtc", sizeof(*this));
  return mRefCnt;
}

STDMETHODIMP_(ULONG)
CEnumFormatEtc::Release() {
  uint32_t refReturn;

  refReturn = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "CEnumFormatEtc");

  if (mRefCnt == 0) delete this;

  return refReturn;
}

/* IEnumFORMATETC impl. */

STDMETHODIMP
CEnumFormatEtc::Next(ULONG aMaxToFetch, FORMATETC* aResult,
                     ULONG* aNumFetched) {
  // If the method retrieves the number of items requested, the return
  // value is S_OK. Otherwise, it is S_FALSE.

  if (aNumFetched) *aNumFetched = 0;

  // aNumFetched can be null if aMaxToFetch is 1
  if (!aNumFetched && aMaxToFetch > 1) return S_FALSE;

  if (!aResult) return S_FALSE;

  // We're done walking the list
  if (mCurrentIdx >= mFormatList.Length()) return S_FALSE;

  uint32_t left = mFormatList.Length() - mCurrentIdx;

  if (!aMaxToFetch) return S_FALSE;

  uint32_t count = std::min(static_cast<uint32_t>(aMaxToFetch), left);

  uint32_t idx = 0;
  while (count > 0) {
    // Copy out to aResult
    mFormatList[mCurrentIdx++].CopyOut(&aResult[idx++]);
    count--;
  }

  if (aNumFetched) *aNumFetched = idx;

  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Skip(ULONG aSkipNum) {
  // If the method skips the number of items requested, the return value is
  // S_OK. Otherwise, it is S_FALSE.

  if ((mCurrentIdx + aSkipNum) >= mFormatList.Length()) return S_FALSE;

  mCurrentIdx += aSkipNum;

  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Reset(void) {
  mCurrentIdx = 0;
  return S_OK;
}

STDMETHODIMP
CEnumFormatEtc::Clone(LPENUMFORMATETC* aResult) {
  // Must return a new IEnumFORMATETC interface with the same iterative state.

  if (!aResult) return E_INVALIDARG;

  CEnumFormatEtc* pEnumObj = new CEnumFormatEtc(mFormatList);

  if (!pEnumObj) return E_OUTOFMEMORY;

  pEnumObj->AddRef();
  pEnumObj->SetIndex(mCurrentIdx);

  *aResult = pEnumObj;

  return S_OK;
}

/* utils */

void CEnumFormatEtc::AddFormatEtc(LPFORMATETC aFormat) {
  if (!aFormat) return;
  FormatEtc* etc = mFormatList.AppendElement();
  // Make a copy of aFormat
  if (etc) etc->CopyIn(aFormat);
}

/* private */

void CEnumFormatEtc::SetIndex(uint32_t aIdx) { mCurrentIdx = aIdx; }
