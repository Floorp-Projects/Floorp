/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyOutputPolicy.h"

#include <mfapi.h>
#include <mferror.h>

#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

HRESULT WMFClearKeyOutputPolicy::RuntimeClassInitialize(
    MFPOLICYMANAGER_ACTION aAction) {
  ENTRY_LOG_ARGS("aAction=%d", aAction);
  if (aAction != PEACTION_PLAY && aAction != PEACTION_EXTRACT &&
      aAction != PEACTION_NO) {
    return MF_E_UNEXPECTED;
  }
  return S_OK;
}

// IMFOutputPolicy
STDMETHODIMP WMFClearKeyOutputPolicy::GenerateRequiredSchemas(
    DWORD aAttributes, GUID aGuidOutputSubtype,
    GUID* aGuidProtectionSchemasSupported,
    DWORD aProtectionSchemasSupportedCount,
    IMFCollection** aRequiredProtectionSchemas) {
  ENTRY_LOG();
  // We don't require an OTA (output trust authority) to enforce for testing.
  // However, we still need to return an empty collection on success.
  ComPtr<IMFCollection> collection;
  RETURN_IF_FAILED(MFCreateCollection(&collection));
  *aRequiredProtectionSchemas = collection.Detach();
  return S_OK;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetOriginatorID(GUID* aGuidOriginatorId) {
  ENTRY_LOG();
  *aGuidOriginatorId = GUID_NULL;
  return S_OK;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetMinimumGRLVersion(
    DWORD* aMinimumGrlVersion) {
  ENTRY_LOG();
  *aMinimumGrlVersion = 0;
  return S_OK;
}

// IMFAttributes inherited by IMFOutputPolicy
STDMETHODIMP WMFClearKeyOutputPolicy::GetItem(REFGUID aGuidKey,
                                              PROPVARIANT* aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetItemType(REFGUID aGuidKey,
                                                  MF_ATTRIBUTE_TYPE* aType) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::CompareItem(REFGUID aGuidKey,
                                                  REFPROPVARIANT aValue,
                                                  BOOL* aResult) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::Compare(IMFAttributes* aAttributes,
                                              MF_ATTRIBUTES_MATCH_TYPE aType,
                                              BOOL* aResult) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetUINT32(REFGUID aGuidKey,
                                                UINT32* aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetUINT64(REFGUID aGuidKey,
                                                UINT64* aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetDouble(REFGUID aGuidKey,
                                                double* aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetGUID(REFGUID aGuidKey,
                                              GUID* aGuidValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetStringLength(REFGUID aGuidKey,
                                                      UINT32* aPcchLength) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetString(REFGUID aGuidKey,
                                                LPWSTR aPwszValue,
                                                UINT32 aCchBufSize,
                                                UINT32* aPcchLength) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetAllocatedString(REFGUID aGuidKey,
                                                         LPWSTR* aPpwszValue,
                                                         UINT32* aPcchLength) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetBlobSize(REFGUID aGuidKey,
                                                  UINT32* aPcbBlobSize) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetBlob(REFGUID aGuidKey, UINT8* pBuf,
                                              UINT32 aCbBufSize,
                                              UINT32* aPcbBlobSize) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetAllocatedBlob(REFGUID aGuidKey,
                                                       UINT8** aBuf,
                                                       UINT32* aPcbSize) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetUnknown(REFGUID aGuidKey, REFIID aRiid,
                                                 LPVOID* aPpv) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetItem(REFGUID aGuidKey,
                                              REFPROPVARIANT aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::DeleteItem(REFGUID aGuidKey) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::DeleteAllItems() { return E_NOTIMPL; }

STDMETHODIMP WMFClearKeyOutputPolicy::SetUINT32(REFGUID aGuidKey,
                                                UINT32 aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetUINT64(REFGUID aGuidKey,
                                                UINT64 aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetDouble(REFGUID aGuidKey,
                                                double aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetGUID(REFGUID aGuidKey,
                                              REFGUID aGuidValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetString(REFGUID aGuidKey,
                                                LPCWSTR aWszValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetBlob(REFGUID aGuidKey,
                                              const UINT8* aBuf,
                                              UINT32 aCbBufSize) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::SetUnknown(REFGUID aGuidKey,
                                                 IUnknown* aUnknown) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::LockStore() { return E_NOTIMPL; }

STDMETHODIMP WMFClearKeyOutputPolicy::UnlockStore() { return E_NOTIMPL; }

STDMETHODIMP WMFClearKeyOutputPolicy::GetCount(UINT32* aPcItems) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::GetItemByIndex(UINT32 aIndex,
                                                     GUID* aGuidKey,
                                                     PROPVARIANT* aValue) {
  return E_NOTIMPL;
}

STDMETHODIMP WMFClearKeyOutputPolicy::CopyAllItems(IMFAttributes* aDest) {
  return E_NOTIMPL;
}

}  // namespace mozilla
