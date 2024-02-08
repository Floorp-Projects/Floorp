/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFClearKeyCDMFactory.h"

#include <string>

#include <Mferror.h>

#include "WMFClearKeyCDMAccess.h"
#include "WMFClearKeyUtils.h"

namespace mozilla {

using Microsoft::WRL::MakeAndInitialize;

ActivatableClass(WMFClearKeyCDMFactory);

bool isRequiringHDCP22OrAbove(LPCWSTR aType) {
  if (aType == nullptr || *aType == L'\0') {
    return false;
  }

  // The HDCP value follows the feature value in
  // https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.protectioncapabilities.istypesupported?view=winrt-19041
  // - 1 (on without HDCP 2.2 Type 1 restriction)
  // - 2 (on with HDCP 2.2 Type 1 restriction)
  std::wstring wstr(aType);
  std::string hdcpStr(wstr.begin(), wstr.end());
  return wstr.find(L"hdcp=2") != std::string::npos;
}

STDMETHODIMP_(BOOL)
WMFClearKeyCDMFactory::IsTypeSupported(_In_ LPCWSTR aKeySystem,
                                       _In_opt_ LPCWSTR aContentType) {
  // For testing, return support for most of cases. Only returns false for some
  // special cases.

  bool needHDCP22OrAbove = isRequiringHDCP22OrAbove(aContentType);
  ENTRY_LOG_ARGS("Need-HDCP2.2+=%d", needHDCP22OrAbove);

  // As the API design of the Media Foundation, we can only know whether the
  // requester is asking for HDCP 2.2+ or not, we can't know the exact HDCP
  // version which is used in getStatusPolicy web api. Therefore, we pretend
  // ourselves only having HDCP 2.1 compliant.
  if (needHDCP22OrAbove) {
    return false;
  }
  return true;
}

STDMETHODIMP
WMFClearKeyCDMFactory::CreateContentDecryptionModuleAccess(
    LPCWSTR aKeySystem, IPropertyStore** aConfigurations,
    DWORD aNumConfigurations, IMFContentDecryptionModuleAccess** aCdmAccess) {
  ENTRY_LOG();
  if (aKeySystem == nullptr || aKeySystem[0] == L'\0') {
    ENTRY_LOG_ARGS("Key system is null or empty");
    return MF_TYPE_ERR;
  }

  if (aNumConfigurations == 0) {
    ENTRY_LOG_ARGS("No available configration");
    return MF_TYPE_ERR;
  }

  if (!IsTypeSupported(aKeySystem, nullptr)) {
    ENTRY_LOG_ARGS("Not supported type");
    return MF_NOT_SUPPORTED_ERR;
  }

  RETURN_IF_FAILED((
      MakeAndInitialize<WMFClearKeyCDMAccess, IMFContentDecryptionModuleAccess>(
          aCdmAccess)));
  ENTRY_LOG_ARGS("Created CDM access!");
  return S_OK;
}

#undef LOG

}  // namespace mozilla
