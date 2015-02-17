/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include <windows.h>
#include <setupapi.h>
#include "gfxWindowsPlatform.h"
#include "GfxInfo.h"
#include "GfxInfoWebGL.h"
#include "nsUnicharUtils.h"
#include "prenv.h"
#include "prprf.h"
#include "GfxDriverInfo.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

using namespace mozilla;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfo2, nsIGfxInfoDebug)
#else
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfo2)
#endif

static const uint32_t allWindowsVersions = 0xffffffff;

GfxInfo::GfxInfo()
 :  mWindowsVersion(0),
    mHasDualGPU(false),
    mIsGPU2Active(false)
{
}

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after gfxPlatform initialization
 * has occurred because they depend on it for information. (See bug 591561) */
nsresult
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->GetRenderMode() == gfxWindowsPlatform::RENDER_DIRECT2D;
  return NS_OK;
}

nsresult
GfxInfo::GetDWriteEnabled(bool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->DWriteEnabled();
  return NS_OK;
}

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  gfxWindowsPlatform::GetDLLVersion(L"dwrite.dll", aDwriteVersion);
  return NS_OK;
}

#define PIXEL_STRUCT_RGB  1
#define PIXEL_STRUCT_BGR  2

/* readonly attribute DOMString cleartypeParameters; */
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
      outStr.AppendPrintf("%s [ ", params.displayName.get());
    }

    if (params.gamma >= 0) {
      foundData = true;
      outStr.AppendPrintf("Gamma: %d ", params.gamma);
    }

    if (params.pixelStructure >= 0) {
      foundData = true;
      if (params.pixelStructure == PIXEL_STRUCT_RGB ||
          params.pixelStructure == PIXEL_STRUCT_BGR)
      {
        outStr.AppendPrintf("Pixel Structure: %s ",
                   (params.pixelStructure == PIXEL_STRUCT_RGB ?
                      L"RGB" : L"BGR"));
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
      outStr.Append(MOZ_UTF16("] "));
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
  kWindowsXP = 0x50001,
  kWindowsServer2003 = 0x50002,
  kWindowsVista = 0x60000,
  kWindows7 = 0x60001,
  kWindows8 = 0x60002,
  kWindows8_1 = 0x60003,
  kWindows10 = 0x60004
};

static int32_t
WindowsOSVersion()
{
  static int32_t winVersion = UNINITIALIZED_VALUE;

  OSVERSIONINFO vinfo;

  if (winVersion == UNINITIALIZED_VALUE) {
    vinfo.dwOSVersionInfoSize = sizeof (vinfo);
#pragma warning(push)
#pragma warning(disable:4996)
    if (!GetVersionEx(&vinfo)) {
#pragma warning(pop)
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
  mDeviceKey = displayDevice.DeviceKey + ArrayLength(DEVICE_KEY_PREFIX)-1;

  mDeviceID = displayDevice.DeviceID;
  mDeviceString = displayDevice.DeviceString;

  // On Windows 8 and Server 2012 hosts, we want to not block RDP
  // sessions from attempting hardware acceleration.  RemoteFX
  // provides features and functionaltiy that can give a good D3D10 +
  // D2D + DirectWrite experience emulated via a software GPU.
  //
  // Unfortunately, the Device ID is nullptr, and we can't enumerate
  // it using the setup infrastructure (SetupDiGetClassDevsW below
  // will return INVALID_HANDLE_VALUE).
  if (mWindowsVersion == kWindows8 &&
      mDeviceID.Length() == 0 &&
      mDeviceString.EqualsLiteral("RDPUDD Chained DD"))
  {
    WCHAR sysdir[255];
    UINT len = GetSystemDirectory(sysdir, sizeof(sysdir));
    if (len < sizeof(sysdir)) {
      nsString rdpudd(sysdir);
      rdpudd.AppendLiteral("\\rdpudd.dll");
      gfxWindowsPlatform::GetDLLVersion(rdpudd.BeginReading(), mDriverVersion);
      mDriverDate.AssignLiteral("01-01-1970");

      // 0x1414 is Microsoft; 0xfefe is an invented (and unused) code
      mDeviceID.AssignLiteral("PCI\\VEN_1414&DEV_FEFE&SUBSYS_00000000");
    }
  }

  /* create a device information set composed of the current display device */
  HDEVINFO devinfo = SetupDiGetClassDevsW(nullptr, mDeviceID.get(), nullptr,
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
            mDriverVersion = value;
          } else {
            // If the entry wasn't found, assume the worst (0.0.0.0).
            mDriverVersion.AssignLiteral("0.0.0.0");
          }
          dwcbData = sizeof(value);
          result = RegQueryValueExW(key, L"DriverDate", nullptr, nullptr,
                                    (LPBYTE)value, &dwcbData);
          if (result == ERROR_SUCCESS) {
            mDriverDate = value;
          } else {
            // Again, assume the worst
            mDriverDate.AssignLiteral("01-01-1970");
          }
          RegCloseKey(key); 
          break;
        }
      }
    }

    SetupDiDestroyDeviceInfoList(devinfo);
  }

  mAdapterVendorID.AppendPrintf("0x%04x", ParseIDFromDeviceID(mDeviceID, "VEN_", 4));
  mAdapterDeviceID.AppendPrintf("0x%04x", ParseIDFromDeviceID(mDeviceID, "&DEV_", 4));
  mAdapterSubsysID.AppendPrintf("%08x", ParseIDFromDeviceID(mDeviceID,  "&SUBSYS_", 8));

  // We now check for second display adapter.

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
      uint32_t adapterVendorID2;
      uint32_t adapterDeviceID2;

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
            nsAutoString adapterVendorID2String;
            nsAutoString adapterDeviceID2String;
            adapterVendorID2 = ParseIDFromDeviceID(deviceID2, "VEN_", 4);
            adapterVendorID2String.AppendPrintf("0x%04x", adapterVendorID2);
            adapterDeviceID2 = ParseIDFromDeviceID(deviceID2, "&DEV_", 4);
            adapterDeviceID2String.AppendPrintf("0x%04x", adapterDeviceID2);
            if (mAdapterVendorID == adapterVendorID2String &&
                mAdapterDeviceID == adapterDeviceID2String) {
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
              mHasDualGPU = true;
              mDeviceString2 = value;
              mDeviceID2 = deviceID2;
              mDeviceKey2 = driverKey2;
              mDriverVersion2 = driverVersion2;
              mDriverDate2 = driverDate2;
              mAdapterVendorID2.AppendPrintf("0x%04x", adapterVendorID2);
              mAdapterDeviceID2.AppendPrintf("0x%04x", adapterDeviceID2);
              mAdapterSubsysID2.AppendPrintf("%08x", ParseIDFromDeviceID(mDeviceID2, "&SUBSYS_", 8));
              break;
            }
          }
        }
      }

      SetupDiDestroyDeviceInfoList(devinfo);
    }
  }

  mHasDriverVersionMismatch = false;
  if (mAdapterVendorID == GfxDriverInfo::GetDeviceVendor(VendorIntel)) {
    // we've had big crashers (bugs 590373 and 595364) apparently correlated
    // with bad Intel driver installations where the DriverVersion reported
    // by the registry was not the version of the DLL.
    bool is64bitApp = sizeof(void*) == 8;
    const char16_t *dllFileName = is64bitApp
                                 ? MOZ_UTF16("igd10umd64.dll")
                                 : MOZ_UTF16("igd10umd32.dll"),
                    *dllFileName2 = is64bitApp
                                 ? MOZ_UTF16("igd10iumd64.dll")
                                 : MOZ_UTF16("igd10iumd32.dll");
    nsString dllVersion, dllVersion2;
    gfxWindowsPlatform::GetDLLVersion((char16_t*)dllFileName, dllVersion);
    gfxWindowsPlatform::GetDLLVersion((char16_t*)dllFileName2, dllVersion2);

    uint64_t dllNumericVersion = 0, dllNumericVersion2 = 0,
             driverNumericVersion = 0, knownSafeMismatchVersion = 0;
    ParseDriverVersion(dllVersion, &dllNumericVersion);
    ParseDriverVersion(dllVersion2, &dllNumericVersion2);
    ParseDriverVersion(mDriverVersion, &driverNumericVersion);
    ParseDriverVersion(NS_LITERAL_STRING("9.17.10.0"), &knownSafeMismatchVersion);

    // If there's a driver version mismatch, consider this harmful only when
    // the driver version is less than knownSafeMismatchVersion.  See the
    // above comment about crashes with old mismatches. If the GetDllVersion
    // call fails, then they return 0, so that will be considered a mismatch.
    if (dllNumericVersion != driverNumericVersion &&
        dllNumericVersion2 != driverNumericVersion &&
        (driverNumericVersion < knownSafeMismatchVersion ||
         std::max(dllNumericVersion, dllNumericVersion2) < knownSafeMismatchVersion)) {
      mHasDriverVersionMismatch = true;
    }
  }

  const char *spoofedDriverVersionString = PR_GetEnv("MOZ_GFX_SPOOF_DRIVER_VERSION");
  if (spoofedDriverVersionString) {
    mDriverVersion.AssignASCII(spoofedDriverVersionString);
  }

  const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_VENDOR_ID");
  if (spoofedVendor) {
    mAdapterVendorID.AssignASCII(spoofedVendor);
  }

  const char *spoofedDevice = PR_GetEnv("MOZ_GFX_SPOOF_DEVICE_ID");
  if (spoofedDevice) {
    mAdapterDeviceID.AssignASCII(spoofedDevice);
  }

  AddCrashReportAnnotations();

  GetCountryCode();

  return rv;
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString;
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString2;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.get(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD)))
    aAdapterRAM = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  if (!mHasDualGPU) {
    aAdapterRAM.Truncate();
  } else if (NS_FAILED(GetKeyValue(mDeviceKey2.get(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD))) {
    aAdapterRAM = L"Unknown";
  }
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.get(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
    aAdapterDriver = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  if (!mHasDualGPU) {
    aAdapterDriver.Truncate();
  } else if (NS_FAILED(GetKeyValue(mDeviceKey2.get(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ))) {
    aAdapterDriver = L"Unknown";
  }
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion = mDriverVersion;
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate = mDriverDate;
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion = mDriverVersion2;
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate = mDriverDate2;
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID;
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID2;
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID;
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID2;
  return NS_OK;
}

/* readonly attribute DOMString adapterSubsysID; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString & aAdapterSubsysID)
{
  aAdapterSubsysID = mAdapterSubsysID;
  return NS_OK;
}

/* readonly attribute DOMString adapterSubsysID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString & aAdapterSubsysID)
{
  aAdapterSubsysID = mAdapterSubsysID2;
  return NS_OK;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  *aIsGPU2Active = mIsGPU2Active;
  return NS_OK;
}

#if defined(MOZ_CRASHREPORTER)
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
#endif

/* interface nsIGfxInfo2 */
/* readonly attribute DOMString countryCode; */
NS_IMETHODIMP
GfxInfo::GetCountryCode(nsAString& aCountryCode)
{
  aCountryCode = mCountryCode;
  return NS_OK;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
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
    LossyAppendUTF16toASCII(mDeviceID, note);
    note.AppendLiteral(", ");
    LossyAppendUTF16toASCII(mDeviceKeyDebug, note);
    LossyAppendUTF16toASCII(mDeviceKeyDebug, note);
  }
  note.Append("\n");

  if (mHasDualGPU) {
    nsString deviceID2, vendorID2, subsysID2;
    nsAutoString adapterDriverVersionString2;
    nsCString narrowDeviceID2, narrowVendorID2, narrowSubsysID2;

    note.AppendLiteral("Has dual GPUs. GPU #2: ");
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

#endif
}

void
GfxInfo::GetCountryCode()
{
  GEOID geoid = GetUserGeoID(GEOCLASS_NATION);
  if (geoid == GEOID_NOT_AVAILABLE) {
    return;
  }
  // Get required length
  int numChars = GetGeoInfoW(geoid, GEO_ISO2, nullptr, 0, 0);
  if (!numChars) {
    return;
  }
  // Now get the string for real
  mCountryCode.SetLength(numChars);
  numChars = GetGeoInfoW(geoid, GEO_ISO2, wwc(mCountryCode.BeginWriting()),
                         mCountryCode.Length(), 0);
  if (numChars) {
    // numChars includes null terminator
    mCountryCode.Truncate(numChars - 1);
  }
}

static OperatingSystem
WindowsVersionToOperatingSystem(int32_t aWindowsVersion)
{
  switch(aWindowsVersion) {
    case kWindowsXP:
      return DRIVER_OS_WINDOWS_XP;
    case kWindowsServer2003:
      return DRIVER_OS_WINDOWS_SERVER_2003;
    case kWindowsVista:
      return DRIVER_OS_WINDOWS_VISTA;
    case kWindows7:
      return DRIVER_OS_WINDOWS_7;
    case kWindows8:
      return DRIVER_OS_WINDOWS_8;
    case kWindows8_1:
      return DRIVER_OS_WINDOWS_8_1;
    case kWindowsUnknown:
    default:
      return DRIVER_OS_UNKNOWN;
    };
}

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
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_WINDOWS_XP,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(6,14,11,8265), "182.65" );
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_WINDOWS_VISTA,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,17,11,8265), "182.65" );
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_WINDOWS_7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,17,11,8265), "182.65" );

    /*
     * AMD/ATI entries
     */
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,62,0,0), "9.6" );
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(8,62,0,0), "9.6" );

    // Bug 1099252
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_WINDOWS_7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,832,0,0));

    // Bug 1118695
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_WINDOWS_7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,783,2,2000));

    /*
     * Bug 783517 - crashes in AMD driver on Windows 8
     */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE( DRIVER_OS_WINDOWS_8,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,982,0,0), V(8,983,0,0), "!= 8.982.*.*" );
    APPEND_TO_DRIVER_BLOCKLIST_RANGE( DRIVER_OS_WINDOWS_8,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(8,982,0,0), V(8,983,0,0), "!= 8.982.*.*" );

    /* OpenGL on any ATI/AMD hardware is discouraged
     * See:
     *  bug 619773 - WebGL: Crash with blue screen : "NMI: Parity Check / Memory Parity Error"
     *  bugs 584403, 584404, 620924 - crashes in atioglxx
     *  + many complaints about incorrect rendering
     */
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );

    /*
     * Intel entries
     */

    /* The driver versions used here come from bug 594877. They might not
     * be particularly relevant anymore.
     */
    #define IMPLEMENT_INTEL_DRIVER_BLOCKLIST(winVer, devFamily, driverVer)                                                      \
      APPEND_TO_DRIVER_BLOCKLIST2( winVer,                                                                                      \
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(devFamily), \
        GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,                                                 \
        DRIVER_LESS_THAN, driverVer )

    #define IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(winVer, devFamily, driverVer)                                                  \
      APPEND_TO_DRIVER_BLOCKLIST2( winVer,                                                                                      \
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(devFamily), \
        nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,                                               \
        DRIVER_LESS_THAN, driverVer )

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMA500,   V(7,14,10,1006));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMA900,   GfxDriverInfo::allDriverVersions);
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMA950,   V(7,14,10,1504));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMA3150,  V(7,14,10,2124));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMAX3000, V(7,15,10,1666));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_VISTA, IntelGMAX4500HD, V(8,15,10,2202));

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMA500,   V(5,0,0,2026));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMA900,   GfxDriverInfo::allDriverVersions);
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMA950,   V(8,15,10,1930));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMA3150,  V(8,14,10,2117));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMAX3000, V(8,15,10,1930));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST_D2D(DRIVER_OS_WINDOWS_7, IntelGMAX4500HD, V(8,15,10,2202));

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMA500,   V(3,0,20,3200));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMA900,   V(6,14,10,4764));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMA950,   V(6,14,10,4926));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMA3150,  V(6,14,10,5134));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMAX3000, V(6,14,10,5218));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, IntelGMAX4500HD, V(6,14,10,4969));

    // StrechRect seems to suffer from precision issues which leads to artifacting
    // during content drawing starting with at least version 6.14.10.5082
    // and going until 6.14.10.5218. See bug 919454 and bug 949275 for more info.
    APPEND_TO_DRIVER_BLOCKLIST_RANGE(DRIVER_OS_WINDOWS_XP,
      const_cast<nsAString&>(GfxDriverInfo::GetDeviceVendor(VendorIntel)),
      const_cast<GfxDeviceFamily*>(GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD)),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_EXCLUSIVE, V(6,14,10,5076), V(6,14,10,5218), "6.14.10.5218");

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMA500,   V(3,0,20,3200));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMA900,   GfxDriverInfo::allDriverVersions);
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMA950,   V(7,14,10,1504));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMA3150,  V(7,14,10,1910));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMAX3000, V(7,15,10,1666));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, IntelGMAX4500HD, V(7,15,10,1666));

    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMA500,   V(5,0,0,2026));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMA900,   GfxDriverInfo::allDriverVersions);
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMA950,   V(8,15,10,1930));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMA3150,  V(8,14,10,1972));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMAX3000, V(7,15,10,1666));
    IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, IntelGMAX4500HD, V(7,15,10,1666));

    // Bug 1074378
    APPEND_TO_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel),
      (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelGMAX4500HD),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_EQUAL, V(8,15,10,1749), "8.15.10.2342");

    /* OpenGL on any Intel hardware is discouraged */
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );

    /**
     * Disable acceleration on Intel HD 3000 for graphics drivers <= 8.15.10.2321.
     * See bug 1018278 and bug 1060736.
     */
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_ALL,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelHD3000),
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,10,2321), "8.15.10.2342" );

    /* Disable D2D on Win7 on Intel HD Graphics on driver <= 8.15.10.2302
     * See bug 806786
     */
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_WINDOWS_7,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelMobileHDGraphics),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,10,2302) );

    /* Disable D2D on Win8 on Intel HD Graphics on driver <= 8.15.10.2302
     * See bug 804144 and 863683
     */
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_WINDOWS_8,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorIntel), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(IntelMobileHDGraphics),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN_OR_EQUAL, V(8,15,10,2302) );

    /* Disable D2D on AMD Catalyst 14.4 until 14.6
     * See bug 984488
     */
    APPEND_TO_DRIVER_BLOCKLIST_RANGE( DRIVER_OS_ALL,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(14,1,0,0), V(14,2,0,0), "ATI Catalyst 14.6+");
    APPEND_TO_DRIVER_BLOCKLIST_RANGE( DRIVER_OS_ALL,
        (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAMD), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_BETWEEN_INCLUSIVE_START, V(14,1,0,0), V(14,2,0,0), "ATI Catalyst 14.6+");

    /* Disable D3D9 layers on NVIDIA 6100/6150/6200 series due to glitches
     * whilst scrolling. See bugs: 612007, 644787 & 645872.
     */
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(NvidiaBlockD3D9Layers),
      nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );

    /* Microsoft RemoteFX; blocked less than 6.2.0.0 */
    APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorMicrosoft), GfxDriverInfo::allDevices,
      GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
      DRIVER_LESS_THAN, V(6,2,0,0), "< 6.2.0.0" );

    /* Bug 1008759: Optimus (NVidia) crash.  Disable D2D on NV 310M. */
    APPEND_TO_DRIVER_BLOCKLIST2(DRIVER_OS_ALL,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Nvidia310M),
      nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions);

    APPEND_TO_DRIVER_BLOCKLIST2(DRIVER_OS_ALL,
      (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI), (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(AMDRadeonHD5800),
      nsIGfxInfo::FEATURE_DXVA, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions);
  }
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(int32_t aFeature,
                              int32_t *aStatus, 
                              nsAString & aSuggestedDriverVersion, 
                              const nsTArray<GfxDriverInfo>& aDriverInfo,
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
      return NS_ERROR_FAILURE;
    }

    if (!adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorIntel), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorAMD), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorATI), nsCaseInsensitiveStringComparator()) &&
        !adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorMicrosoft), nsCaseInsensitiveStringComparator()) &&
        // FIXME - these special hex values are currently used in xpcshell tests introduced by
        // bug 625160 patch 8/8. Maybe these tests need to be adjusted now that we're only whitelisting
        // intel/ati/nvidia.
        !adapterVendorID.LowerCaseEqualsLiteral("0xabcd") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xdcba") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xabab") &&
        !adapterVendorID.LowerCaseEqualsLiteral("0xdcdc"))
    {
      *aStatus = FEATURE_BLOCKED_DEVICE;
      return NS_OK;
    }

    uint64_t driverVersion;
    if (!ParseDriverVersion(adapterDriverVersionString, &driverVersion)) {
      return NS_ERROR_FAILURE;
    }

    // special-case the WinXP test slaves: they have out-of-date drivers, but we still want to
    // whitelist them, actually we do know that this combination of device and driver version
    // works well.
    if (mWindowsVersion == kWindowsXP &&
        adapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), nsCaseInsensitiveStringComparator()) &&
        adapterDeviceID.LowerCaseEqualsLiteral("0x0861") && // GeForce 9400
        driverVersion == V(6,14,11,7756))
    {
      *aStatus = FEATURE_STATUS_OK;
      return NS_OK;
    }

    // Windows Server 2003 should be just like Windows XP for present purpose, but still has a different version number.
    // OTOH Windows Server 2008 R1 and R2 already have the same version numbers as Vista and Seven respectively
    if (os == DRIVER_OS_WINDOWS_SERVER_2003)
      os = DRIVER_OS_WINDOWS_XP;

    if (mHasDriverVersionMismatch) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
      return NS_OK;
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, &os);
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  mAdapterVendorID = aVendorID;
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  mAdapterDeviceID = aDeviceID;
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  mDriverVersion = aDriverVersion;
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  mWindowsVersion = aVersion;
  return NS_OK;
}

#endif
