/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYACTIVATE_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYACTIVATE_H

#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"

namespace mozilla {

// This class is used to provide WMFClearKeyContentEnabler.
class WMFClearKeyActivate
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFActivate, Microsoft::WRL::FtmBase> {
 public:
  WMFClearKeyActivate() = default;
  ~WMFClearKeyActivate() = default;
  WMFClearKeyActivate(const WMFClearKeyActivate&) = delete;
  WMFClearKeyActivate& operator=(const WMFClearKeyActivate&) = delete;

  HRESULT RuntimeClassInitialize();

  // IMFActivate
  STDMETHODIMP ActivateObject(REFIID aRiid, void** aPpv) override;
  STDMETHODIMP ShutdownObject() override;
  STDMETHODIMP DetachObject() override;

  // IMFAttributes inherited by IMFActivate
  STDMETHODIMP GetItem(REFGUID aGuidKey, PROPVARIANT* aValue) override;
  STDMETHODIMP GetItemType(REFGUID aGuidKey, MF_ATTRIBUTE_TYPE* aType) override;
  STDMETHODIMP CompareItem(REFGUID aGuidKey, REFPROPVARIANT aValue,
                           BOOL* aResult) override;
  STDMETHODIMP Compare(IMFAttributes* aAttributes,
                       MF_ATTRIBUTES_MATCH_TYPE aType, BOOL* aResult) override;
  STDMETHODIMP GetUINT32(REFGUID aGuidKey, UINT32* aValue) override;
  STDMETHODIMP GetUINT64(REFGUID aGuidKey, UINT64* aValue) override;
  STDMETHODIMP GetDouble(REFGUID aGuidKey, double* aValue) override;
  STDMETHODIMP GetGUID(REFGUID aGuidKey, GUID* aGuidValue) override;
  STDMETHODIMP GetStringLength(REFGUID aGuidKey, UINT32* aPcchLength) override;
  STDMETHODIMP GetString(REFGUID aGuidKey, LPWSTR aPwszValue,
                         UINT32 aCchBufSize, UINT32* aPcchLength) override;
  STDMETHODIMP GetAllocatedString(REFGUID aGuidKey, LPWSTR* aPpwszValue,
                                  UINT32* aPcchLength) override;
  STDMETHODIMP GetBlobSize(REFGUID aGuidKey, UINT32* aPcbBlobSize) override;
  STDMETHODIMP GetBlob(REFGUID aGuidKey, UINT8* aBuf, UINT32 aCbBufSize,
                       UINT32* aPcbBlobSize) override;
  STDMETHODIMP GetAllocatedBlob(REFGUID aGuidKey, UINT8** aBuf,
                                UINT32* aPcbSize) override;
  STDMETHODIMP GetUnknown(REFGUID aGuidKey, REFIID aRiid,
                          LPVOID* aPpv) override;
  STDMETHODIMP SetItem(REFGUID aGuidKey, REFPROPVARIANT aValue) override;
  STDMETHODIMP DeleteItem(REFGUID aGuidKey) override;
  STDMETHODIMP DeleteAllItems() override;
  STDMETHODIMP SetUINT32(REFGUID aGuidKey, UINT32 aValue) override;
  STDMETHODIMP SetUINT64(REFGUID aGuidKey, UINT64 aValue) override;
  STDMETHODIMP SetDouble(REFGUID aGuidKey, double aValue) override;
  STDMETHODIMP SetGUID(REFGUID aGuidKey, REFGUID aGuidValue) override;
  STDMETHODIMP SetString(REFGUID aGuidKey, LPCWSTR aWszValue) override;
  STDMETHODIMP SetBlob(REFGUID aGuidKey, const UINT8* aBuf,
                       UINT32 aCbBufSize) override;
  STDMETHODIMP SetUnknown(REFGUID aGuidKey, IUnknown* aUnknown) override;
  STDMETHODIMP LockStore() override;
  STDMETHODIMP UnlockStore() override;
  STDMETHODIMP GetCount(UINT32* aPcItems) override;
  STDMETHODIMP GetItemByIndex(UINT32 aIndex, GUID* aGuidKey,
                              PROPVARIANT* aValue) override;
  STDMETHODIMP CopyAllItems(IMFAttributes* aDest) override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYACTIVATE_H
