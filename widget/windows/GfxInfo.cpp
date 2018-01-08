/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include <windows.h>
#include <setupapi.h>
#include "gfxConfig.h"
#include "gfxWindowsPlatform.h"
#include "GfxInfo.h"
#include "GfxInfoWebGL.h"
#include "nsUnicharUtils.h"
#include "prenv.h"
#include "prprf.h"
#include "GfxDriverInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#include "nsPrintfCString.h"
#include "jsapi.h"
#include <intrin.h>

#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

static const uint32_t allWindowsVersions = 0xffffffff;

GfxInfo::GfxInfo()
 :  mWindowsVersion(0),
    mActiveGPUIndex(0),
    mHasDualGPU(false)
{
}

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after gfxPlatform initialization
 * has occurred because they depend on it for information. (See bug 591561) */
nsresult
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  // Telemetry queries this during XPCOM initialization, and there's no
  // gfxPlatform by then. Just bail out if gfxPlatform isn't initialized.
  if (!gfxPlatform::Initialized()) {
    *aEnabled = false;
    return NS_OK;
  }

  // We check gfxConfig rather than the actual render mode, since the UI
  // process does not use Direct2D if the GPU process is enabled. However,
  // content processes can still use Direct2D.
  *aEnabled = gfx::gfxConfig::IsEnabled(gfx::Feature::DIRECT2D);
  return NS_OK;
}

nsresult
GfxInfo::GetDWriteEnabled(bool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->DWriteEnabled();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  gfxWindowsPlatform::GetDLLVersion(L"dwrite.dll", aDwriteVersion);
  return NS_OK;
}

#define PIXEL_STRUCT_RGB  1
#define PIXEL_STRUCT_BGR  2

NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString & aCleartypeParams)
{
  nsTArray<ClearTypeParameterInfo> clearTypeParams;

  gfxWindowsPlatform::GetPlatform()->GetCleartypeParams(clearTypeParams);
  uint32_t d, numDisplays = clearTypeParams.Length();
  bool displayNames = (numDisplays > 1);
  bool foundData = false;
  nsString outStr;

  for (d = 0; d < numDisplays; d++) {
    ClearTypeParameterInfo& params = clearTypeParams[d];

    if (displayNames) {
      outStr.AppendPrintf("%S [ ", params.displayName.get());
    }

    if (params.gamma >= 0) {
      foundData = true;
      outStr.AppendPrintf("Gamma: %.4g ", params.gamma / 1000.0);
    }

    if (params.pixelStructure >= 0) {
      foundData = true;
      if (params.pixelStructure == PIXEL_STRUCT_RGB ||
          params.pixelStructure == PIXEL_STRUCT_BGR)
      {
        outStr.AppendPrintf("Pixel Structure: %S ",
                            (params.pixelStructure == PIXEL_STRUCT_RGB
                            ? u"RGB" : u"BGR"));
      } else {
        outStr.AppendPrintf("Pixel Structure: %d ", params.pixelStructure);
      }
    }

    if (params.clearTypeLevel >= 0) {
      foundData = true;
      outStr.AppendPrintf("ClearType Level: %d ", params.clearTypeLevel);
    }

    if (params.enhancedContrast >= 0) {
      foundData = true;
      outStr.AppendPrintf("Enhanced Contrast: %d ", params.enhancedContrast);
    }

    if (displayNames) {
      outStr.Append(u"] ");
    }
  }

  if (foundData) {
    aCleartypeParams.Assign(outStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

static nsresult GetKeyValue(const WCHAR* keyLocation, const WCHAR* keyName, nsAString& destString, int type)
{
  HKEY key;
  DWORD dwcbData;
  DWORD dValue;
  DWORD resultType;
  LONG result;
  nsresult retval = NS_OK;

  result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyLocation, 0, KEY_QUERY_VALUE, &key);
  if (result != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  switch (type) {
    case REG_DWORD: {
      // We only use this for vram size
      dwcbData = sizeof(dValue);
      result = RegQueryValueExW(key, keyName, nullptr, &resultType,
                                (LPBYTE)&dValue, &dwcbData);
      if (result == ERROR_SUCCESS && resultType == REG_DWORD) {
        dValue = dValue / 1024 / 1024;
        destString.AppendInt(int32_t(dValue));
      } else {
        retval = NS_ERROR_FAILURE;
      }
      break;
    }
    case REG_QWORD: {
      // We only use this for vram size
      LONGLONG qValue;
      dwcbData = sizeof(qValue);
      result = RegQueryValueExW(key, keyName, nullptr, &resultType,
                                (LPBYTE)&qValue, &dwcbData);
      if (result == ERROR_SUCCESS && resultType == REG_QWORD) {
        qValue = qValue / 1024 / 1024;
        destString.AppendInt(int32_t(qValue));
      } else {
        retval = NS_ERROR_FAILURE;
      }
      break;
    }
    case REG_MULTI_SZ: {
      // A chain of null-separated strings; we convert the nulls to spaces
      WCHAR wCharValue[1024];
      dwcbData = sizeof(wCharValue);

      result = RegQueryValueExW(key, keyName, nullptr, &resultType,
                                (LPBYTE)wCharValue, &dwcbData);
      if (result == ERROR_SUCCESS && resultType == REG_MULTI_SZ) {
        // This bit here could probably be cleaner.
        bool isValid = false;

        DWORD strLen = dwcbData/sizeof(wCharValue[0]);
        for (DWORD i = 0; i < strLen; i++) {
          if (wCharValue[i] == '\0') {
            if (i < strLen - 1 && wCharValue[i + 1] == '\0') {
              isValid = true;
              break;
            } else {
              wCharValue[i] = ' ';
            }
          }
        }

        // ensure wCharValue is null terminated
        wCharValue[strLen-1] = '\0';

        if (isValid)
          destString = wCharValue;

      } else {
        retval = NS_ERROR_FAILURE;
      }

      break;
    }
  }
  RegCloseKey(key);

  return retval;
}

static nsresult GetKeyValues(const WCHAR* keyLocation, const WCHAR* keyName, nsTArray<nsString>& destStrings)
{
  // First ask for the size of the value
  DWORD size;
  LONG rv = RegGetValueW(HKEY_LOCAL_MACHINE, keyLocation, keyName, RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &size);
  if (rv != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  // Create a buffer with the proper size and retrieve the value
  WCHAR* wCharValue = new WCHAR[size / sizeof(WCHAR)];
  rv = RegGetValueW(HKEY_LOCAL_MACHINE, keyLocation, keyName, RRF_RT_REG_MULTI_SZ, nullptr, (LPBYTE)wCharValue, &size);
  if (rv != ERROR_SUCCESS) {
    delete[] wCharValue;
    return NS_ERROR_FAILURE;
  }

  // The value is a sequence of null-terminated strings, usually terminated by an empty string (\0).
  // RegGetValue ensures that the value is properly terminated with a null character.
  DWORD i = 0;
  DWORD strLen = size / sizeof(WCHAR);
  while (i < strLen) {
    nsString value(wCharValue + i);
    if (!value.IsEmpty()) {
      destStrings.AppendElement(value);
    }
    i += value.Length() + 1;
  }
  delete[] wCharValue;

  return NS_OK;
}

// The device ID is a string like PCI\VEN_15AD&DEV_0405&SUBSYS_040515AD
// this function is used to extract the id's out of it
uint32_t
ParseIDFromDeviceID(const nsAString &key, const char *prefix, int length)
{
  nsAutoString id(key);
  ToUpperCase(id);
  int32_t start = id.Find(prefix);
  if (start != -1) {
    id.Cut(0, start + strlen(prefix));
    id.Truncate(length);
  }
  nsresult err;
  return id.ToInteger(&err, 16);
}

// OS version in 16.16 major/minor form
// based on http://msdn.microsoft.com/en-us/library/ms724834(VS.85).aspx
enum {
  kWindowsUnknown = 0,
  kWindows7 = 0x60001,
  kWindows8 = 0x60002,
  kWindows8_1 = 0x60003,
  kWindows10 = 0xA0000
};

static int32_t
WindowsOSVersion()
{
  static int32_t winVersion = UNINITIALIZED_VALUE;

  OSVERSIONINFO vinfo;

  if (winVersion == UNINITIALIZED_VALUE) {
    vinfo.dwOSVersionInfoSize = sizeof (vinfo);
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    if (!GetVersionEx(&vinfo)) {
#ifdef _MSC_VER
#pragma warning(pop)
#endif
      winVersion = kWindowsUnknown;
    } else {
      winVersion = int32_t(vinfo.dwMajorVersion << 16) + vinfo.dwMinorVersion;
    }
  }

  return winVersion;
}

/* Other interesting places for info:
 *   IDXGIAdapter::GetDesc()
 *   IDirectDraw7::GetAvailableVidMem()
 *   e->GetAvailableTextureMem()
 * */

#define DEVICE_KEY_PREFIX L"\\Registry\\Machine\\"
nsresult
GfxInfo::Init()
{
  nsresult rv = GfxInfoBase::Init();

  DISPLAY_DEVICEW displayDevice;
  displayDevice.cb = sizeof(displayDevice);
  int deviceIndex = 0;

  const char *spoofedWindowsVersion = PR_GetEnv("MOZ_GFX_SPOOF_WINDOWS_VERSION");
  if (spoofedWindowsVersion) {
    PR_sscanf(spoofedWindowsVersion, "%x", &mWindowsVersion);
  } else {
    mWindowsVersion = WindowsOSVersion();
  }

  mDeviceKeyDebug = NS_LITERAL_STRING("PrimarySearch");

  while (EnumDisplayDevicesW(nullptr, deviceIndex, &displayDevice, 0)) {
    if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      mDeviceKeyDebug = NS_LITERAL_STRING("NullSearch");
      break;
    }
    deviceIndex++;
  }

  // make sure the string is nullptr terminated
  if (wcsnlen(displayDevice.DeviceKey, ArrayLength(displayDevice.DeviceKey))
      == ArrayLength(displayDevice.DeviceKey)) {
    // we did not find a nullptr
    return rv;
  }

  mDeviceKeyDebug = displayDevice.DeviceKey;

  /* DeviceKey is "reserved" according to MSDN so we'll be careful with it */
  /* check that DeviceKey begins with DEVICE_KEY_PREFIX */
  /* some systems have a DeviceKey starting with \REGISTRY\Machine\ so we need to compare case insenstively */
  if (_wcsnicmp(displayDevice.DeviceKey, DEVICE_KEY_PREFIX, ArrayLength(DEVICE_KEY_PREFIX)-1) != 0)
    return rv;

  // chop off DEVICE_KEY_PREFIX
  mDeviceKey[0] = displayDevice.DeviceKey + ArrayLength(DEVICE_KEY_PREFIX)-1;

  mDeviceID[0] = displayDevice.DeviceID;
  mDeviceString[0] = displayDevice.DeviceString;

  // On Windows 8 and Server 2012 hosts, we want to not block RDP
  // sessions from attempting hardware acceleration.  RemoteFX
  // provides features and functionaltiy that can give a good D3D10 +
  // D2D + DirectWrite experience emulated via a software GPU.
  //
  // Unfortunately, the Device ID is nullptr, and we can't enumerate
  // it using the setup infrastructure (SetupDiGetClassDevsW below
  // will return INVALID_HANDLE_VALUE).
  if (mWindowsVersion >= kWindows8 &&
      mDeviceID[0].Length() == 0 &&
      mDeviceString[0].EqualsLiteral("RDPUDD Chained DD"))
  {
    WCHAR sysdir[255];
    UINT len = GetSystemDirectory(sysdir, sizeof(sysdir));
    if (len < sizeof(sysdir)) {
      nsString rdpudd(sysdir);
      rdpudd.AppendLiteral("\\rdpudd.dll");
      gfxWindowsPlatform::GetDLLVersion(rdpudd.BeginReading(), mDriverVersion[0]);
      mDriverDate[0].AssignLiteral("01-01-1970");

      // 0x1414 is Microsoft; 0xfefe is an invented (and unused) code
      mDeviceID[0].AssignLiteral("PCI\\VEN_1414&DEV_FEFE&SUBSYS_00000000");
    }
  }

  /* create a device information set composed of the current display device */
  HDEVINFO devinfo = SetupDiGetClassDevsW(nullptr, mDeviceID[0].get(), nullptr,
                                          DIGCF_PRESENT | DIGCF_PROFILE | DIGCF_ALLCLASSES);

  if (devinfo != INVALID_HANDLE_VALUE) {
    HKEY key;
    LONG result;
    WCHAR value[255];
    DWORD dwcbData;
    SP_DEVINFO_DATA devinfoData;
    DWORD memberIndex = 0;

    devinfoData.cbSize = sizeof(devinfoData);
    NS_NAMED_LITERAL_STRING(driverKeyPre, "System\\CurrentControlSet\\Control\\Class\\");
    /* enumerate device information elements in the device information set */
    while (SetupDiEnumDeviceInfo(devinfo, memberIndex++, &devinfoData)) {
      /* get a string that identifies the device's driver key */
      if (SetupDiGetDeviceRegistryPropertyW(devinfo,
                                            &devinfoData,
                                            SPDRP_DRIVER,
                                            nullptr,
                                            (PBYTE)value,
                                            sizeof(value),
                                            nullptr)) {
        nsAutoString driverKey(driverKeyPre);
        driverKey += value;
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, driverKey.get(), 0, KEY_QUERY_VALUE, &key);
        if (result == ERROR_SUCCESS) {
          /* we've found the driver we're looking for */
          dwcbData = sizeof(value);
          result = RegQueryValueExW(key, L"DriverVersion", nullptr, nullptr,
                                    (LPBYTE)value, &dwcbData);
          if (result == ERROR_SUCCESS) {
            mDriverVersion[0] = value;
          } else {
            // If the entry wasn't found, assume the worst (0.0.0.0).
            mDriverVersion[0].AssignLiteral("0.0.0.0");
          }
          dwcbData = sizeof(value);
          result = RegQueryValueExW(key, L"DriverDate", nullptr, nullptr,
                                    (LPBYTE)value, &dwcbData);
          if (result == ERROR_SUCCESS) {
            mDriverDate[0] = value;
          } else {
            // Again, assume the worst
            mDriverDate[0].AssignLiteral("01-01-1970");
          }
          RegCloseKey(key);
          break;
        }
      }
    }

    SetupDiDestroyDeviceInfoList(devinfo);
  }

  // It is convenient to have these as integers
  uint32_t adapterVendorID[2] = {0, 0};
  uint32_t adapterDeviceID[2] = {0, 0};
  uint32_t adapterSubsysID[2] = {0, 0};

  adapterVendorID[0] = ParseIDFromDeviceID(mDeviceID[0], "VEN_", 4);
  adapterDeviceID[0] = ParseIDFromDeviceID(mDeviceID[0], "&DEV_", 4);
  adapterSubsysID[0] = ParseIDFromDeviceID(mDeviceID[0],  "&SUBSYS_", 8);

  // Sometimes we don't get the valid device using this method.  For now,
  // allow zero vendor or device as valid, as long as the other value is
  // non-zero.
  bool foundValidDevice = (adapterVendorID[0] != 0 ||
                           adapterDeviceID[0] != 0);

  // We now check for second display adapter.  If we didn't find the valid
  // device using the original approach, we will try the alternative.

  // Device interface class for display adapters.
  CLSID GUID_DISPLAY_DEVICE_ARRIVAL;
  HRESULT hresult = CLSIDFromString(L"{1CA05180-A699-450A-9A0C-DE4FBE3DDD89}",
                               &GUID_DISPLAY_DEVICE_ARRIVAL);
  if (hresult == NOERROR) {
    devinfo = SetupDiGetClassDevsW(&GUID_DISPLAY_DEVICE_ARRIVAL,
                                   nullptr, nullptr,
                                   DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

    if (devinfo != INVALID_HANDLE_VALUE) {
      HKEY key;
      LONG result;
      WCHAR value[255];
      DWORD dwcbData;
      SP_DEVINFO_DATA devinfoData;
      DWORD memberIndex = 0;
      devinfoData.cbSize = sizeof(devinfoData);

      nsAutoString adapterDriver2;
      nsAutoString deviceID2;
      nsAutoString driverVersion2;
      nsAutoString driverDate2;

      NS_NAMED_LITERAL_STRING(driverKeyPre, "System\\CurrentControlSet\\Control\\Class\\");
      /* enumerate device information elements in the device information set */
      while (SetupDiEnumDeviceInfo(devinfo, memberIndex++, &devinfoData)) {
        /* get a string that identifies the device's driver key */
        if (SetupDiGetDeviceRegistryPropertyW(devinfo,
                                              &devinfoData,
                                              SPDRP_DRIVER,
                                              nullptr,
                                              (PBYTE)value,
                                              sizeof(value),
                                              nullptr)) {
          nsAutoString driverKey2(driverKeyPre);
          driverKey2 += value;
          result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, driverKey2.get(), 0, KEY_QUERY_VALUE, &key);
          if (result == ERROR_SUCCESS) {
            dwcbData = sizeof(value);
            result = RegQueryValueExW(key, L"MatchingDeviceId", nullptr,
                                      nullptr, (LPBYTE)value, &dwcbData);
            if (result != ERROR_SUCCESS) {
              continue;
            }
            deviceID2 = value;
            adapterVendorID[1] = ParseIDFromDeviceID(deviceID2, "VEN_", 4);
            adapterDeviceID[1] = ParseIDFromDeviceID(deviceID2, "&DEV_", 4);
            // Skip the devices we already considered, as well as any
            // "zero" ones.
            if ((adapterVendorID[0] == adapterVendorID[1] &&
                 adapterDeviceID[0] == adapterDeviceID[1]) ||
                (adapterVendorID[1] == 0 && adapterDeviceID[1] == 0)) {
              RegCloseKey(key);
              continue;
            }

            // If this device is missing driver information, it is unlikely to
            // be a real display adapter.
            if (NS_FAILED(GetKeyValue(driverKey2.get(), L"InstalledDisplayDrivers",
                           adapterDriver2, REG_MULTI_SZ))) {
              RegCloseKey(key);
              continue;
            }
            dwcbData = sizeof(value);
            result = RegQueryValueExW(key, L"DriverVersion", nullptr, nullptr,
                                      (LPBYTE)value, &dwcbData);
            if (result != ERROR_SUCCESS) {
              RegCloseKey(key);
              continue;
            }
            driverVersion2 = value;
            dwcbData = sizeof(value);
            result = RegQueryValueExW(key, L"DriverDate", nullptr, nullptr,
                                      (LPBYTE)value, &dwcbData);
            if (result != ERROR_SUCCESS) {
              RegCloseKey(key);
              continue;
            }
            driverDate2 = value;
            dwcbData = sizeof(value);
            result = RegQueryValueExW(key, L"Device Description", nullptr,
                                      nullptr, (LPBYTE)value, &dwcbData);
            if (result != ERROR_SUCCESS) {
              dwcbData = sizeof(value);
              result = RegQueryValueExW(key, L"DriverDesc", nullptr, nullptr,
                                        (LPBYTE)value, &dwcbData);
            }
            RegCloseKey(key);
            if (result == ERROR_SUCCESS) {
              // If we didn't find a valid device with the original method
              // take this one, and continue looking for the second GPU.
              if (!foundValidDevice) {
                foundValidDevice = true;
                adapterVendorID[0] = adapterVendorID[1];
                adapterDeviceID[0] = adapterDeviceID[1];
                mDeviceString[0] = value;
                mDeviceID[0] = deviceID2;
                mDeviceKey[0] = driverKey2;
                mDriverVersion[0] = driverVersion2;
                mDriverDate[0] = driverDate2;
                adapterSubsysID[0] = ParseIDFromDeviceID(mDeviceID[0], "&SUBSYS_", 8);
                continue;
              }

              mHasDualGPU = true;
              mDeviceString[1] = value;
              mDeviceID[1] = deviceID2;
              mDeviceKey[1] = driverKey2;
              mDriverVersion[1] = driverVersion2;
              mDriverDate[1] = driverDate2;
              adapterSubsysID[1] = ParseIDFromDeviceID(mDeviceID[1], "&SUBSYS_", 8);
              mAdapterVendorID[1].AppendPrintf("0x%04x", adapterVendorID[1]);
              mAdapterDeviceID[1].AppendPrintf("0x%04x", adapterDeviceID[1]);
              mAdapterSubsysID[1].AppendPrintf("%08x", adapterSubsysID[1]);
              break;
            }
          }
        }
      }

      SetupDiDestroyDeviceInfoList(devinfo);
    }
  }

  mAdapterVendorID[0].AppendPrintf("0x%04x", adapterVendorID[0]);
  mAdapterDeviceID[0].AppendPrintf("0x%04x", adapterDeviceID[0]);
  mAdapterSubsysID[0].AppendPrintf("%08x", adapterSubsysID[0]);

  // Sometimes, the enumeration is not quite right and the two adapters
  // end up being swapped.  Actually enumerate the adapters that come
  // back from the DXGI factory to check, and tag the second as active
  // if found.
  if (mHasDualGPU) {
    nsModuleHandle dxgiModule(LoadLibrarySystem32(L"dxgi.dll"));
    decltype(CreateDXGIFactory)* createDXGIFactory = (decltype(CreateDXGIFactory)*)
      GetProcAddress(dxgiModule, "CreateDXGIFactory");

    if (createDXGIFactory) {
      RefPtr<IDXGIFactory> factory = nullptr;
      createDXGIFactory(__uuidof(IDXGIFactory),
                        (void**)(&factory) );
      if (factory) {
        RefPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(factory->EnumAdapters(0, getter_AddRefs(adapter)))) {
          DXGI_ADAPTER_DESC desc;
          PodZero(&desc);
          if (SUCCEEDED(adapter->GetDesc(&desc))) {
            if (desc.VendorId != adapterVendorID[0] &&
                desc.DeviceId != adapterDeviceID[0] &&
                desc.VendorId == adapterVendorID[1] &&
                desc.DeviceId == adapterDeviceID[1]) {
              mActiveGPUIndex = 1;
            }
          }
        }
      }
    }
  }

  mHasDriverVersionMismatch = false;
  if (mAdapterVendorID[mActiveGPUIndex] == GfxDriverInfo::GetDeviceVendor(VendorIntel)) {
    // we've had big crashers (bugs 590373 and 595364) apparently correlated
    // with bad Intel driver installations where the DriverVersion reported
    // by the registry was not the version of the DLL.

    // Note that these start without the .dll extension but eventually gain it.
    bool is64bitApp = sizeof(void*) == 8;
    nsAutoString dllFileName(is64bitApp ? u"igd10umd64" : u"igd10umd32");
    nsAutoString dllFileName2(is64bitApp ? u"igd10iumd64" : u"igd10iumd32");

    nsString dllVersion, dllVersion2;
    uint64_t dllNumericVersion = 0, dllNumericVersion2 = 0,
             driverNumericVersion = 0, knownSafeMismatchVersion = 0;

    // Only parse the DLL version for those found in the driver list
    nsAutoString eligibleDLLs;
    if (NS_SUCCEEDED(GetAdapterDriver(eligibleDLLs))) {
      if (FindInReadable(dllFileName, eligibleDLLs)) {
        dllFileName += NS_LITERAL_STRING(".dll");
        gfxWindowsPlatform::GetDLLVersion(dllFileName.get(), dllVersion);
        ParseDriverVersion(dllVersion, &dllNumericVersion);
      }
      if (FindInReadable(dllFileName2, eligibleDLLs)) {
        dllFileName2 += NS_LITERAL_STRING(".dll");
        gfxWindowsPlatform::GetDLLVersion(dllFileName2.get(), dllVersion2);
        ParseDriverVersion(dllVersion2, &dllNumericVersion2);
      }
    }

    // Sometimes the DLL is not in the System32 nor SysWOW64 directories. But UserModeDriverName
    // (or UserModeDriverNameWow, if available) might provide the full path to the DLL in some
    // DriverStore FileRepository.
    if (dllNumericVersion == 0 && dllNumericVersion2 == 0) {
      nsTArray<nsString> eligibleDLLpaths;
      const WCHAR* keyLocation = mDeviceKey[mActiveGPUIndex].get();
      GetKeyValues(keyLocation, L"UserModeDriverName", eligibleDLLpaths);
      GetKeyValues(keyLocation, L"UserModeDriverNameWow", eligibleDLLpaths);
      size_t length = eligibleDLLpaths.Length();
      for (size_t i=0; i<length && dllNumericVersion == 0 && dllNumericVersion2 == 0; ++i) {
        if (FindInReadable(dllFileName, eligibleDLLpaths[i])) {
          gfxWindowsPlatform::GetDLLVersion(eligibleDLLpaths[i].get(), dllVersion);
          ParseDriverVersion(dllVersion, &dllNumericVersion);
        } else if (FindInReadable(dllFileName2, eligibleDLLpaths[i])) {
          gfxWindowsPlatform::GetDLLVersion(eligibleDLLpaths[i].get(), dllVersion2);
          ParseDriverVersion(dllVersion2, &dllNumericVersion2);
        }
      }
    }

    ParseDriverVersion(mDriverVersion[mActiveGPUIndex], &driverNumericVersion);
    ParseDriverVersion(NS_LITERAL_STRING("9.17.10.0"), &knownSafeMismatchVersion);

    // If there's a driver version mismatch, consider this harmful only when
    // the driver version is less than knownSafeMismatchVersion.  See the
    // above comment about crashes with old mismatches.  If the GetDllVersion
    // call fails, we are not calling it a mismatch.
    if ((dllNumericVersion != 0 && dllNumericVersion != driverNumericVersion) ||
        (dllNumericVersion2 != 0 && dllNumericVersion2 != driverNumericVersion)) {
      if (driverNumericVersion < knownSafeMismatchVersion ||
          std::max(dllNumericVersion, dllNumericVersion2) < knownSafeMismatchVersion) {
        mHasDriverVersionMismatch = true;
        gfxCriticalNoteOnce << "Mismatched driver versions between the registry "
                            << NS_ConvertUTF16toUTF8(mDriverVersion[mActiveGPUIndex]).get()
                            << " and DLL(s) "
                            << NS_ConvertUTF16toUTF8(dllVersion).get() << ", "
                            << NS_ConvertUTF16toUTF8(dllVersion2).get() << " reported.";
      }
    } else if (dllNumericVersion == 0 && dllNumericVersion2 == 0) {
      // Leave it as an asserting error for now, to see if we can find
      // a system that exhibits this kind of a problem internally.
      gfxCriticalErrorOnce() << "Potential driver version mismatch ignored due to missing DLLs "
                             << NS_ConvertUTF16toUTF8(dllFileName).get() << " v="
                             << NS_ConvertUTF16toUTF8(dllVersion).get() << " and "
                             << NS_ConvertUTF16toUTF8(dllFileName2).get() << " v="
                             << NS_ConvertUTF16toUTF8(dllVersion2).get();
    }
  }

  const char *spoofedDriverVersionString = PR_GetEnv("MOZ_GFX_SPOOF_DRIVER_VERSION");
  if (spoofedDriverVersionString) {
    mDriverVersion[mActiveGPUIndex].AssignASCII(spoofedDriverVersionString);
  }

  const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_VENDOR_ID");
  if (spoofedVendor) {
    mAdapterVendorID[mActiveGPUIndex].AssignASCII(spoofedVendor);
  }

  const char *spoofedDevice = PR_GetEnv("MOZ_GFX_SPOOF_DEVICE_ID");
  if (spoofedDevice) {
    mAdapterDeviceID[mActiveGPUIndex].AssignASCII(spoofedDevice);
  }

  AddCrashReportAnnotations();

  return rv;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey[mActiveGPUIndex].get(), L"HardwareInformation.qwMemorySize", aAdapterRAM, REG_QWORD)) || aAdapterRAM.Length() == 0) {
    if (NS_FAILED(GetKeyValue(mDeviceKey[mActiveGPUIndex].get(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD))) {
      aAdapterRAM = L"Unknown";
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  if (!mHasDualGPU) {
    aAdapterRAM.Truncate();
  } else if (NS_FAILED(GetKeyValue(mDeviceKey[1 - mActiveGPUIndex].get(), L"HardwareInformation.qwMemorySize", aAdapterRAM, REG_QWORD)) || aAdapterRAM.Length() == 0) {
    if (NS_FAILED(GetKeyValue(mDeviceKey[1 - mActiveGPUIndex].get(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD))) {
      aAdapterRAM = L"Unknown";
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey[mActiveGPUIndex].get(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
    aAdapterDriver = L"Unknown";
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  if (!mHasDualGPU) {
    aAdapterDriver.Truncate();
  } else if (NS_FAILED(GetKeyValue(mDeviceKey[1 - mActiveGPUIndex].get(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ))) {
    aAdapterDriver = L"Unknown";
  }
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion = mDriverVersion[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate = mDriverDate[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion = mDriverVersion[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate = mDriverDate[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString & aAdapterSubsysID)
{
  aAdapterSubsysID = mAdapterSubsysID[mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString & aAdapterSubsysID)
{
  aAdapterSubsysID = mAdapterSubsysID[1 - mActiveGPUIndex];
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  // This is never the case, as the active GPU ends up being
  // the first one.  It should probably be removed.
  *aIsGPU2Active = false;
  return NS_OK;
}

/* Cisco's VPN software can cause corruption of the floating point state.
 * Make a note of this in our crash reports so that some weird crashes
 * make more sense */
static void
CheckForCiscoVPN() {
  LONG result;
  HKEY key;
  /* This will give false positives, but hopefully no false negatives */
  result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Cisco Systems\\VPN Client", 0, KEY_QUERY_VALUE, &key);
  if (result == ERROR_SUCCESS) {
    RegCloseKey(key);
    CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("Cisco VPN\n"));
  }
}

void
GfxInfo::AddCrashReportAnnotations()
{
  CheckForCiscoVPN();

  if (mHasDriverVersionMismatch) {
    CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("DriverVersionMismatch\n"));
  }

  nsString deviceID, vendorID, driverVersion, subsysID;
  nsCString narrowDeviceID, narrowVendorID, narrowDriverVersion, narrowSubsysID;

  GetAdapterDeviceID(deviceID);
  CopyUTF16toUTF8(deviceID, narrowDeviceID);
  GetAdapterVendorID(vendorID);
  CopyUTF16toUTF8(vendorID, narrowVendorID);
  GetAdapterDriverVersion(driverVersion);
  CopyUTF16toUTF8(driverVersion, narrowDriverVersion);
  GetAdapterSubsysID(subsysID);
  CopyUTF16toUTF8(subsysID, narrowSubsysID);

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
                                     narrowVendorID);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
                                     narrowDeviceID);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDriverVersion"),
                                     narrowDriverVersion);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterSubsysID"),
                                     narrowSubsysID);

  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsAutoCString note;
  /* AppendPrintf only supports 32 character strings, mrghh. */
  note.AppendLiteral("AdapterVendorID: ");
  note.Append(narrowVendorID);
  note.AppendLiteral(", AdapterDeviceID: ");
  note.Append(narrowDeviceID);
  note.AppendLiteral(", AdapterSubsysID: ");
  note.Append(narrowSubsysID);
  note.AppendLiteral(", AdapterDriverVersion: ");
  note.Append(NS_LossyConvertUTF16toASCII(driverVersion));

  if (vendorID == GfxDriverInfo::GetDeviceVendor(VendorAll)) {
    /* if we didn't find a valid vendorID lets append the mDeviceID string to try to find out why */
    note.AppendLiteral(", ");
    LossyAppendUTF16toASCII(mDeviceID[mActiveGPUIndex], note);
    note.AppendLiteral(", ");
    LossyAppendUTF16toASCII(mDeviceKeyDebug, note);
    LossyAppendUTF16toASCII(mDeviceKeyDebug, note);
  }
  note.AppendLiteral("\n");

  if (mHasDualGPU) {
    nsString deviceID2, vendorID2, subsysID2;
    nsAutoString adapterDriverVersionString2;
    nsCString narrowDeviceID2, narrowVendorID2, narrowSubsysID2;

    // Make a slight difference between the two cases so that we
    // can see it in the crash reports.  It may come in handy.
    if (mActiveGPUIndex == 1) {
      note.AppendLiteral("Has dual GPUs. GPU-#2: ");
    } else {
      note.AppendLiteral("Has dual GPUs. GPU #2: ");
    }
    GetAdapterDeviceID2(deviceID2);
    CopyUTF16toUTF8(deviceID2, narrowDeviceID2);
    GetAdapterVendorID2(vendorID2);
    CopyUTF16toUTF8(vendorID2, narrowVendorID2);
    GetAdapterDriverVersion2(adapterDriverVersionString2);
    GetAdapterSubsysID(subsysID2);
    CopyUTF16toUTF8(subsysID2, narrowSubsysID2);
    note.AppendLiteral("AdapterVendorID2: ");
    note.Append(narrowVendorID2);
    note.AppendLiteral(", AdapterDeviceID2: ");
    note.Append(narrowDeviceID2);
    note.AppendLiteral(", AdapterSubsysID2: ");
    note.Append(narrowSubsysID2);
    note.AppendLiteral(", AdapterDriverVersion2: ");
    note.Append(NS_LossyConvertUTF16toASCII(adapterDriverVersionString2));
  }
  CrashReporter::AppendAppNotesToCrashReport(note);
}

static OperatingSystem
WindowsVersionToOperatingSystem(int32_t aWindowsVersion)
{
  switch(aWindowsVersion) {
    case kWindows7:
      return OperatingSystem::Windows7;
    case kWindows8:
      return OperatingSystem::Windows8;
    case kWindows8_1:
      return OperatingSystem::Windows8_1;
    case kWindows10:
      return OperatingSystem::Windows10;
    case kWindowsUnknown:
    default:
      return OperatingSystem::Unknown;
    }
}

// Return true if the CPU supports AVX, but the operating system does not.
#if defined(_M_X64)
static inline bool
DetectBrokenAVX()
{
  int regs[4];
  __cpuid(regs, 0);
  if (regs[0] == 0) {
    // Level not supported.
    return false;
  }

  __cpuid(regs, 1);

  const unsigned AVX = 1u << 28;
  const unsigned XSAVE = 1u << 26;
  if ((regs[2] & (AVX|XSAVE)) != (AVX|XSAVE)) {
    // AVX is not supported on this CPU.
    return false;
  }

  const unsigned OSXSAVE = 1u << 27;
  if ((regs[2] & OSXSAVE) != OSXSAVE) {
    // AVX is supported, but the OS didn't enable it.
    // This can be forced via bcdedit /set xsavedisable 1.
    return true;
  }

  const unsigned AVX_CTRL_BITS = (1 << 1) | (1 << 2);
  return (_xgetbv(0) & AVX_CTRL_BITS) != AVX_CTRL_BITS;
}
#endif

const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  if (!mDriverInfo->Length()) {
    /*
     * It should be noted here that more specialized rules on certain features
     * should be inserted -before- more generalized restriction. As the first
     * match for feature/OS/device found in the list will be used for the final
     * blacklisting call.
     */

    /*
     * NVIDIA entries
     */
    /*
     * The last 5 digit of the NVIDIA driver version maps to the version that
     * NVIDIA uses. The minor version (15, 16, 17) corresponds roughtly to the
     * OS (Vista, Win7, Win7) but they show up in smaller numbers across all
     * OS versions (perhaps due to OS upgrades). So we want to support
     * October 2009+ drivers across all these minor versions.
     *
     * 187.45 (late October 2009) and earlier contain a bug which can cause us
     * to crash on shutdown.
     */
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,11,8745),
      "FEATURE_FAILURE_NV_W7_15", "nVidia driver > 187.45" );
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,16,10,0000), V(8,16,11,8745),
      "FEATURE_FAILURE_NV_W7_16", "nVidia driver > 187.45" );
    // Telemetry doesn't show any driver in this range so it might not even be required.
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,17,10,0000), V(8,17,11,8745),
      "FEATURE_FAILURE_NV_W7_17", "nVidia driver > 187.45" );

    /*
     * AMD/ATI entries. 8.56.1.15 is the driver that shipped with Windows 7 RTM
     */
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,56,1,15), "FEATURE_FAILURE_AMD1", "8.56.1.15" );
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,56,1,15), "FEATURE_FAILURE_AMD2", "8.56.1.15" );

    // Bug 1099252
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,832,0,0), "FEATURE_FAILURE_BUG_1099252");

    // Bug 1118695
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,783,2,2000), "FEATURE_FAILURE_BUG_1118695");

    // Bug 1198815
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(15,200,0,0), V(15,200,1062,1004),
      "FEATURE_FAILURE_BUG_1198815", "15.200.0.0-15.200.1062.1004");

    // Bug 1267970
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows10,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(15,200,0,0), V(15,301,2301,1002),
      "FEATURE_FAILURE_BUG_1267970", "15.200.0.0-15.301.2301.1002");
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows10,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(16,100,0,0), V(16,300,2311,0),
      "FEATURE_FAILURE_BUG_1267970", "16.100.0.0-16.300.2311.0");

    /*
     * Bug 783517 - crashes in AMD driver on Windows 8
     */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows8,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,982,0,0), V(8,983,0,0),
      "FEATURE_FAILURE_BUG_783517_AMD", "!= 8.982.*.*" );
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows8,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,982,0,0), V(8,983,0,0),
      "FEATURE_FAILURE_BUG_783517_ATI", "!= 8.982.*.*" );

    /* OpenGL on any ATI/AMD hardware is discouraged
     * See:
     *  bug 619773 - WebGL: Crash with blue screen : "NMI: Parity Check / Memory Parity Error"
     *  bugs 584403, 584404, 620924 - crashes in atioglxx
     *  + many complaints about incorrect rendering
     */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_OGL_ATI_DIS" );
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_OGL_AMD_DIS" );

    /*
     * Intel entries
     */

    /* The driver versions used here come from bug 594877. They might not
     * be particularly relevant anymore.
     */
    #define IMPLEMENT_INTEL_DRIVER_BLOCKLIST(winVer, devFamily, driverVer, ruleId)                                              \
      APPEND_TO_DRIVER_BLOCKLIST2( winVer,                                                                                      \
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(devFamily), \
        GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,                                                 \
        DRIVER_LESS_THAN, driverVer, ruleId )

    #define IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(winVer, devFamily, driverVer, ruleId)                                          \
      APPEND_TO_DRIVER_BLOCKLIST2( winVer,                                                                                      \
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(devFamily), \
        nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,                                               \
        DRIVER_BUILD_ID_LESS_THAN, driverVer, ruleId )

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelGMA500,   2026, "FEATURE_FAILURE_594877_7");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelGMA900,   GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_594877_8");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelGMA950,   1930, "FEATURE_FAILURE_594877_9");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelGMA3150,  2117, "FEATURE_FAILURE_594877_10");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelGMAX3000, 1930, "FEATURE_FAILURE_594877_11");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(OperatingSystem::Windows7, IntelHDGraphicsToSandyBridge, 2202, "FEATURE_FAILURE_594877_12");

    /* Disable Direct2D on Intel GMAX4500 devices because of rendering corruption discovered
     * in bug 1180379. These seems to affect even the most recent drivers. We're black listing
     * all of the devices to be safe even though we've only confirmed the issue on the G45
     */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_1180379");

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMA500,   V(5,0,0,2026), "FEATURE_FAILURE_INTEL_16");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMA900,   GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_INTEL_17");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMA950,   V(8,15,10,1930), "FEATURE_FAILURE_INTEL_18");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMA3150,  V(8,14,10,1972), "FEATURE_FAILURE_INTEL_19");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMAX3000, V(7,15,10,1666), "FEATURE_FAILURE_INTEL_20");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelGMAX4500HD, V(7,15,10,1666), "FEATURE_FAILURE_INTEL_21");
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(OperatingSystem::Windows7, IntelHDGraphicsToSandyBridge, V(7,15,10,1666), "FEATURE_FAILURE_INTEL_22");

    // Bug 1074378
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel),
      (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,15,10,1749), "FEATURE_FAILURE_BUG_1074378_1", "8.15.10.2342");
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel),
      (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelHDGraphicsToSandyBridge),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,15,10,1749), "FEATURE_FAILURE_BUG_1074378_2", "8.15.10.2342");

    /* OpenGL on any Intel hardware is discouraged */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_INTEL_OGL_DIS" );

    /**
     * Disable acceleration on Intel HD 3000 for graphics drivers <= 8.15.10.2321.
     * See bug 1018278 and bug 1060736.
     */
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelHD3000),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL, 2321, "FEATURE_FAILURE_BUG_1018278", "X.X.X.2342");

    /* Disable D2D on Win7 on Intel HD Graphics on driver <= 8.15.10.2302
     * See bug 806786
     */
    APPEND_TO_DRIVER_BLOCKLIST2( OperatingSystem::Windows7,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelMobileHDGraphics),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,10,2302), "FEATURE_FAILURE_BUG_806786" );

    /* Disable D2D on Win8 on Intel HD Graphics on driver <= 8.15.10.2302
     * See bug 804144 and 863683
     */
    APPEND_TO_DRIVER_BLOCKLIST2( OperatingSystem::Windows8,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelMobileHDGraphics),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,10,2302), "FEATURE_FAILURE_BUG_804144" );

    /* Disable D3D11 layers on Intel G41 express graphics and Intel GM965, Intel X3100, for causing device resets.
     * See bug 1116812.
     */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(Bug1116812),
      nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1116812" );

    /* Disable D3D11 layers on Intel GMA 3150 for failing to allocate a shared handle for textures.
     * See bug 1207665. Additionally block D2D so we don't accidentally use WARP.
     */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(Bug1207665),
        nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1207665_1" );
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(Bug1207665),
        nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1207665_2" );

    /* Disable D2D on AMD Catalyst 14.4 until 14.6
     * See bug 984488
     */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(14,1,0,0), V(14,2,0,0), "FEATURE_FAILURE_BUG_984488_1", "ATI Catalyst 14.6+");
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(14,1,0,0), V(14,2,0,0), "FEATURE_FAILURE_BUG_984488_2", "ATI Catalyst 14.6+");

    /* Disable D3D9 layers on NVIDIA 6100/6150/6200 series due to glitches
     * whilst scrolling. See bugs: 612007, 644787 & 645872.
     */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(NvidiaBlockD3D9Layers),
      nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_612007" );

    /* Microsoft RemoteFX; blocked less than 6.2.0.0 */
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorMicrosoft), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(6,2,0,0), "< 6.2.0.0", "FEATURE_FAILURE_REMOTE_FX" );

    /* Bug 1008759: Optimus (NVidia) crash.  Disable D2D on NV 310M. */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Nvidia310M),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1008759");

    /* Bug 1139503: DXVA crashes with ATI cards on windows 10. */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows10,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(15,200,1006,0), "FEATURE_FAILURE_BUG_1139503");

    /* Bug 1213107: D3D9 crashes with ATI cards on Windows 7. */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows7,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(8,861,0,0), V(8,862,6,5000), "FEATURE_FAILURE_BUG_1213107_1", "Radeon driver > 8.862.6.5000");
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(OperatingSystem::Windows7,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_ANGLE, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(8,861,0,0), V(8,862,6,5000), "FEATURE_FAILURE_BUG_1213107_2", "Radeon driver > 8.862.6.5000");

    /* This may not be needed at all */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows7,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Bug1155608),
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,15,10,2869), "FEATURE_FAILURE_INTEL_W7_HW_DECODING");

    /* Bug 1203199/1092166: DXVA startup crashes on some intel drivers. */
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Windows,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL, 2849, "FEATURE_FAILURE_BUG_1203199_1", "Intel driver > X.X.X.2849");

    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Nvidia8800GTS),
      nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(9,18,13,4052), "FEATURE_FAILURE_BUG_1203199_2");

    /* Bug 1137716: XXX this should really check for the matching Intel piece as well.
     * Unfortunately, we don't have the infrastructure to do that */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE_GPU2(OperatingSystem::Windows7,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Bug1137716),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE, V(8,17,12,5730), V(8,17,12,6901), "FEATURE_FAILURE_BUG_1137716", "Nvidia driver > 8.17.12.6901");

    /* Bug 1153381: WebGL issues with D3D11 ANGLE on Intel. These may be fixed by an ANGLE update. */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD),
      nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1153381");

    /* Bug 1336710: Crash in rx::Blit9::initialize. */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::WindowsXP,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD),
      nsIGfxInfo::FEATURE_WEBGL_ANGLE, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1336710");

    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::WindowsXP,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(IntelHDGraphicsToSandyBridge),
      nsIGfxInfo::FEATURE_WEBGL_ANGLE, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1336710");

    /* Bug 1304360: Graphical artifacts with D3D9 on Windows 7. */
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(IntelGMAX3000),
      nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL, 1749, "FEATURE_FAILURE_INTEL_W7_D3D9_LAYERS");

#if defined(_M_X64)
    if (DetectBrokenAVX()) {
      APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows7,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
        nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
        DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1403353");
    }
#endif

    ////////////////////////////////////
    // WebGL

    // Older than 5-15-2016
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED, DRIVER_LESS_THAN,
      V(16,200,1010,1002), "WEBGL_NATIVE_GL_OLD_AMD");

    // Older than 11-18-2015
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED, DRIVER_BUILD_ID_LESS_THAN,
      4331, "WEBGL_NATIVE_GL_OLD_INTEL");

    // Older than 2-23-2016
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED, DRIVER_LESS_THAN,
      V(10,18,13,6200), "WEBGL_NATIVE_GL_OLD_NVIDIA");

    ////////////////////////////////////
    // FEATURE_DX_INTEROP2

    // All AMD.
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_DX_INTEROP2, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "DX_INTEROP2_AMD_CRASH");

    ////////////////////////////////////
    // FEATURE_D3D11_KEYED_MUTEX

    // bug 1359416
    APPEND_TO_DRIVER_BLOCKLIST2(OperatingSystem::Windows,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel),
      (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelHDGraphicsToSandyBridge),
      nsIGfxInfo::FEATURE_D3D11_KEYED_MUTEX, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions, "FEATURE_FAILURE_BUG_1359416");

  }
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(int32_t aFeature,
                              int32_t *aStatus,
                              nsAString & aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo,
                              nsACString& aFailureId,
                              OperatingSystem* aOS /* = nullptr */)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  OperatingSystem os = WindowsVersionToOperatingSystem(mWindowsVersion);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  if (aOS)
    *aOS = os;

  // Don't evaluate special cases if we're checking the downloaded blocklist.
  if (!aDriverInfo.Length()) {
    nsAutoString adapterVendorID;
    nsAutoString adapterDeviceID;
    nsAutoString adapterDriverVersionString;
    if (NS_FAILED(GetAdapterVendorID(adapterVendorID)) ||
        NS_FAILED(GetAdapterDeviceID(adapterDeviceID)) ||
        NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
    {
      aFailureId = "FEATURE_FAILURE_GET_ADAPTER";
      *aStatus = FEATURE_BLOCKED_DEVICE;
      return NS_OK;
    }

    if (!adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorIntel), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorAMD), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorATI), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorMicrosoft), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorParallels), nsCaseInsensitiveStringComparator()) &&
        // FIXME - these special hex values are currently used in xpcshell tests introduced by
        // bug 625160 patch 8/8. Maybe these tests need to be adjusted now that we're only whitelisting
        // intel/ati/nvidia.
        !adapterVendorID.LowerCaseEqualsLiteral("0xabcd") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xdcba") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xabab") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xdcdc"))
    {
      aFailureId = "FEATURE_FAILURE_UNKNOWN_DEVICE_VENDOR";
      *aStatus = FEATURE_BLOCKED_DEVICE;
      return NS_OK;
    }

    uint64_t driverVersion;
    if (!ParseDriverVersion(adapterDriverVersionString, &driverVersion)) {
      aFailureId = "FEATURE_FAILURE_PARSE_DRIVER";
      *aStatus = FEATURE_BLOCKED_DRIVER_VERSION;
      return NS_OK;
    }

    if (mHasDriverVersionMismatch) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_MISMATCHED_VERSION;
      return NS_OK;
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, aFailureId, &os);
}

nsresult
GfxInfo::FindMonitors(JSContext* aCx, JS::HandleObject aOutArray)
{
  int deviceCount = 0;
  for (int deviceIndex = 0;; deviceIndex++) {
    DISPLAY_DEVICEA device;
    device.cb = sizeof(device);
    if (!::EnumDisplayDevicesA(nullptr, deviceIndex, &device, 0)) {
      break;
    }

    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
      continue;
    }

    DEVMODEA mode;
    mode.dmSize = sizeof(mode);
    mode.dmDriverExtra = 0;
    if (!::EnumDisplaySettingsA(device.DeviceName, ENUM_CURRENT_SETTINGS, &mode)) {
      continue;
    }

    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

    JS::Rooted<JS::Value> screenWidth(aCx, JS::Int32Value(mode.dmPelsWidth));
    JS_SetProperty(aCx, obj, "screenWidth", screenWidth);

    JS::Rooted<JS::Value> screenHeight(aCx, JS::Int32Value(mode.dmPelsHeight));
    JS_SetProperty(aCx, obj, "screenHeight", screenHeight);

    JS::Rooted<JS::Value> refreshRate(aCx, JS::Int32Value(mode.dmDisplayFrequency));
    JS_SetProperty(aCx, obj, "refreshRate", refreshRate);

    JS::Rooted<JS::Value> pseudoDisplay(aCx,
      JS::BooleanValue(!!(device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)));
    JS_SetProperty(aCx, obj, "pseudoDisplay", pseudoDisplay);

    JS::Rooted<JS::Value> element(aCx, JS::ObjectValue(*obj));
    JS_SetElement(aCx, aOutArray, deviceCount++, element);
  }
  return NS_OK;
}

void
GfxInfo::DescribeFeatures(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  // Add the platform neutral features
  GfxInfoBase::DescribeFeatures(aCx, aObj);

  JS::Rooted<JSObject*> obj(aCx);

  gfx::FeatureStatus d3d11 = gfxConfig::GetValue(Feature::D3D11_COMPOSITING);
  if (!InitFeatureObject(aCx, aObj, "d3d11", FEATURE_DIRECT3D_11_ANGLE,
                         Some(d3d11), &obj)) {
    return;
  }
  if (d3d11 == gfx::FeatureStatus::Available) {
    DeviceManagerDx* dm = DeviceManagerDx::Get();
    JS::Rooted<JS::Value> val(aCx, JS::Int32Value(dm->GetCompositorFeatureLevel()));
    JS_SetProperty(aCx, obj, "version", val);

    val = JS::BooleanValue(dm->IsWARP());
    JS_SetProperty(aCx, obj, "warp", val);

    val = JS::BooleanValue(dm->TextureSharingWorks());
    JS_SetProperty(aCx, obj, "textureSharing", val);

    bool blacklisted = false;
    if (nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo()) {
      int32_t status;
      nsCString discardFailureId;
      if (SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, discardFailureId, &status))) {
        blacklisted = (status != nsIGfxInfo::FEATURE_STATUS_OK);
      }
    }

    val = JS::BooleanValue(blacklisted);
    JS_SetProperty(aCx, obj, "blacklisted", val);
  }

  gfx::FeatureStatus d2d = gfxConfig::GetValue(Feature::DIRECT2D);
  if (!InitFeatureObject(aCx, aObj, "d2d", nsIGfxInfo::FEATURE_DIRECT2D,
                         Some(d2d), &obj)) {
    return;
  }
  {
    const char* version = "1.1";
    JS::Rooted<JSString*> str(aCx, JS_NewStringCopyZ(aCx, version));
    JS::Rooted<JS::Value> val(aCx, JS::StringValue(str));
    JS_SetProperty(aCx, obj, "version", val);
  }
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  mAdapterVendorID[mActiveGPUIndex] = aVendorID;
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  mAdapterDeviceID[mActiveGPUIndex] = aDeviceID;
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  mDriverVersion[mActiveGPUIndex] = aDriverVersion;
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  mWindowsVersion = aVersion;
  return NS_OK;
}

#endif
