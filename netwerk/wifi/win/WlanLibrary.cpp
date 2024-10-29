/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WlanLibrary.h"

#include "mozilla/Logging.h"

// Moz headers (alphabetical)
namespace mozilla {
extern LazyLogModule gWifiScannerLog;
}
#define WIFISCAN_LOG(args) \
  MOZ_LOG(mozilla::gWifiScannerLog, mozilla::LogLevel::Debug, args)

WinWLANLibrary* WinWLANLibrary::Load() {
  WinWLANLibrary* ret = new WinWLANLibrary();
  if (!ret) {
    return nullptr;
  }

  if (!ret->Initialize()) {
    delete ret;
    return nullptr;
  }

  return ret;
}

HANDLE
WinWLANLibrary::GetWLANHandle() const { return mWlanHandle; }

decltype(::WlanEnumInterfaces)* WinWLANLibrary::GetWlanEnumInterfacesPtr()
    const {
  return mWlanEnumInterfacesPtr;
}

decltype(::WlanGetNetworkBssList)* WinWLANLibrary::GetWlanGetNetworkBssListPtr()
    const {
  return mWlanGetNetworkBssListPtr;
}

decltype(::WlanFreeMemory)* WinWLANLibrary::GetWlanFreeMemoryPtr() const {
  return mWlanFreeMemoryPtr;
}

decltype(::WlanCloseHandle)* WinWLANLibrary::GetWlanCloseHandlePtr() const {
  return mWlanCloseHandlePtr;
}

decltype(::WlanOpenHandle)* WinWLANLibrary::GetWlanOpenHandlePtr() const {
  return mWlanOpenHandlePtr;
}

decltype(::WlanRegisterNotification)*
WinWLANLibrary::GetWlanRegisterNotificationPtr() const {
  return mWlanRegisterNotificationPtr;
}

decltype(::WlanScan)* WinWLANLibrary::GetWlanScanPtr() const {
  return mWlanScanPtr;
}

bool WinWLANLibrary::Initialize() {
  mWlanLibrary = LoadLibraryW(L"Wlanapi.dll");
  if (!mWlanLibrary) {
    WIFISCAN_LOG(
        ("WinWLANLibrary::Initialize failed - couldn't open wlanapi.dll"));
    return false;
  }

  mWlanOpenHandlePtr = (decltype(::WlanOpenHandle)*)GetProcAddress(
      mWlanLibrary, "WlanOpenHandle");
  mWlanEnumInterfacesPtr = (decltype(::WlanEnumInterfaces)*)GetProcAddress(
      mWlanLibrary, "WlanEnumInterfaces");
  mWlanRegisterNotificationPtr =
      (decltype(::WlanRegisterNotification)*)GetProcAddress(
          mWlanLibrary, "WlanRegisterNotification");
  mWlanScanPtr =
      (decltype(::WlanScan)*)GetProcAddress(mWlanLibrary, "WlanScan");

  mWlanFreeMemoryPtr = (decltype(::WlanFreeMemory)*)GetProcAddress(
      mWlanLibrary, "WlanFreeMemory");
  mWlanCloseHandlePtr = (decltype(::WlanCloseHandle)*)GetProcAddress(
      mWlanLibrary, "WlanCloseHandle");
  mWlanGetNetworkBssListPtr =
      (decltype(::WlanGetNetworkBssList)*)GetProcAddress(
          mWlanLibrary, "WlanGetNetworkBssList");

  if (!mWlanOpenHandlePtr || !mWlanEnumInterfacesPtr ||
      !mWlanRegisterNotificationPtr || !mWlanGetNetworkBssListPtr ||
      !mWlanScanPtr || !mWlanFreeMemoryPtr || !mWlanCloseHandlePtr) {
    WIFISCAN_LOG(
        ("WinWLANLibrary::Initialize failed to find functions in wlanapi.dll"));
    return false;
  }

  // Get the handle to the WLAN API.
  DWORD negotiated_version;
  // We could be executing on either Windows XP or Windows Vista, so use the
  // lower version of the client WLAN API. It seems that the negotiated version
  // is the Vista version irrespective of what we pass!
  static const int kXpWlanClientVersion = 1;
  if (ERROR_SUCCESS != (*mWlanOpenHandlePtr)(kXpWlanClientVersion, nullptr,
                                             &negotiated_version,
                                             &mWlanHandle)) {
    WIFISCAN_LOG(("WinWLANLibrary::Initialize: WlanOpenHandle failed"));
    return false;
  }

  WIFISCAN_LOG(("WinWLANLibrary::Initialize succeeded - version is %lu",
                negotiated_version));
  return true;
}

WinWLANLibrary::~WinWLANLibrary() {
  if (mWlanLibrary) {
    if (mWlanHandle) {
      (*mWlanCloseHandlePtr)(mWlanLibrary, mWlanHandle);
      mWlanHandle = nullptr;
    }
    ::FreeLibrary(mWlanLibrary);
    mWlanLibrary = nullptr;
  }
}

#undef WIFISCAN_LOG
