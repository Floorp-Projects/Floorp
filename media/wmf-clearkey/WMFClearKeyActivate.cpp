/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyActivate.h"

#include <mfapi.h>
#include <mferror.h>

#include "WMFClearKeyContentEnabler.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

HRESULT WMFClearKeyActivate::RuntimeClassInitialize() { return S_OK; }

STDMETHODIMP WMFClearKeyActivate::ActivateObject(REFIID aRiid, void** aPpv) {
  ENTRY_LOG();
  ComPtr<IMFContentEnabler> contentEnabler;
  RETURN_IF_FAILED(
      MakeAndInitialize<WMFClearKeyContentEnabler>(&contentEnabler));
  RETURN_IF_FAILED(contentEnabler.CopyTo(aRiid, aPpv));
  return S_OK;
}

STDMETHODIMP WMFClearKeyActivate::ShutdownObject() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::DetachObject() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

// IMFAttributes inherited by IMFActivate
STDMETHODIMP WMFClearKeyActivate::GetItem(REFGUID aGuidKey,
                                          PROPVARIANT* aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetItemType(REFGUID aGuidKey,
                                              MF_ATTRIBUTE_TYPE* aType) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::CompareItem(REFGUID aGuidKey,
                                              REFPROPVARIANT aValue,
                                              BOOL* aResult) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::Compare(IMFAttributes* aAttributes,
                                          MF_ATTRIBUTES_MATCH_TYPE aType,
                                          BOOL* aResult) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetUINT32(REFGUID aGuidKey, UINT32* aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetUINT64(REFGUID aGuidKey, UINT64* aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetDouble(REFGUID aGuidKey, double* aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetGUID(REFGUID aGuidKey, GUID* aGuidValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetStringLength(REFGUID aGuidKey,
                                                  UINT32* aPcchLength) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetString(REFGUID aGuidKey, LPWSTR aPwszValue,
                                            UINT32 aCchBufSize,
                                            UINT32* aPcchLength) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetAllocatedString(REFGUID aGuidKey,
                                                     LPWSTR* aPpwszValue,
                                                     UINT32* aPcchLength) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetBlobSize(REFGUID aGuidKey,
                                              UINT32* aPcbBlobSize) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetBlob(REFGUID aGuidKey, UINT8* pBuf,
                                          UINT32 aCbBufSize,
                                          UINT32* aPcbBlobSize) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetAllocatedBlob(REFGUID aGuidKey,
                                                   UINT8** aBuf,
                                                   UINT32* aPcbSize) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetUnknown(REFGUID aGuidKey, REFIID aRiid,
                                             LPVOID* aPpv) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetItem(REFGUID aGuidKey,
                                          REFPROPVARIANT aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::DeleteItem(REFGUID aGuidKey) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::DeleteAllItems() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetUINT32(REFGUID aGuidKey, UINT32 aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetUINT64(REFGUID aGuidKey, UINT64 aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetDouble(REFGUID aGuidKey, double aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetGUID(REFGUID aGuidKey,
                                          REFGUID aGuidValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetString(REFGUID aGuidKey,
                                            LPCWSTR aWszValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetBlob(REFGUID aGuidKey, const UINT8* aBuf,
                                          UINT32 aCbBufSize) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::SetUnknown(REFGUID aGuidKey,
                                             IUnknown* aUnknown) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::LockStore() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::UnlockStore() {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetCount(UINT32* aPcItems) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::GetItemByIndex(UINT32 aIndex, GUID* aGuidKey,
                                                 PROPVARIANT* aValue) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyActivate::CopyAllItems(IMFAttributes* aDest) {
  NOT_IMPLEMENTED();
  return E_NOTIMPL;
}

}  // namespace mozilla
