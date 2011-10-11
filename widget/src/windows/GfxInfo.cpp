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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
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

#include <windows.h>
#include <setupapi.h>
#include "gfxWindowsPlatform.h"
#include "GfxInfo.h"
#include "GfxInfoWebGL.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"
#include "prenv.h"
#include "prprf.h"
#include "GfxDriverInfo.h"
#include "mozilla/Preferences.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif


using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED1(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

static const PRUint32 allWindowsVersions = 0xffffffff;
static const PRUint64 allDriverVersions = 0xffffffffffffffffULL;

static const PRUint32 vendorIntel = 0x8086;

static const PRUint32 vendorNVIDIA = 0x10de;

static const PRUint32 vendorAMD = 0x1022;
static const PRUint32 vendorATI = 0x1002;

#define V(a,b,c,d) GFX_DRIVER_VERSION(a,b,c,d)


GfxInfo::GfxInfo()
  : mAdapterVendorID(0),
    mAdapterDeviceID(0),
    mAdapterVendorID2(0),
    mAdapterDeviceID2(0),
    mWindowsVersion(0),
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

nsresult
GfxInfo::GetAzureEnabled(bool *aEnabled)
{
  *aEnabled = false;

  bool d2dEnabled = 
    gfxWindowsPlatform::GetPlatform()->GetRenderMode() == gfxWindowsPlatform::RENDER_DIRECT2D;

  if (d2dEnabled) {
    bool azure = false;
    nsresult rv = mozilla::Preferences::GetBool("gfx.canvas.azure.enabled", &azure);

    if (NS_SUCCEEDED(rv) && azure) {
      *aEnabled = true;
    }
  }

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
  PRUint32 d, numDisplays = clearTypeParams.Length();
  bool displayNames = (numDisplays > 1);
  bool foundData = false;
  nsString outStr;
  WCHAR valStr[256];

  for (d = 0; d < numDisplays; d++) {
    ClearTypeParameterInfo& params = clearTypeParams[d];

    if (displayNames) {
      swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                 L"%s [ ", params.displayName.get());
      outStr.Append(valStr);
    }

    if (params.gamma >= 0) {
      foundData = true;
      swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                 L"Gamma: %d ", params.gamma);
      outStr.Append(valStr);
    }

    if (params.pixelStructure >= 0) {
      foundData = true;
      if (params.pixelStructure == PIXEL_STRUCT_RGB ||
          params.pixelStructure == PIXEL_STRUCT_BGR)
      {
        swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                   L"Pixel Structure: %s ",
                   (params.pixelStructure == PIXEL_STRUCT_RGB ?
                      L"RGB" : L"BGR"));
      } else {
        swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                   L"Pixel Structure: %d ", params.pixelStructure);
      }
      outStr.Append(valStr);
    }

    if (params.clearTypeLevel >= 0) {
      foundData = true;
      swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                 L"ClearType Level: %d ", params.clearTypeLevel);
      outStr.Append(valStr);
    }

    if (params.enhancedContrast >= 0) {
      foundData = true;
      swprintf_s(valStr, NS_ARRAY_LENGTH(valStr),
                 L"Enhanced Contrast: %d ", params.enhancedContrast);
      outStr.Append(valStr);
    }

    if (displayNames) {
      outStr.Append(L"] ");
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
      result = RegQueryValueExW(key, keyName, NULL, &resultType, (LPBYTE)&dValue, &dwcbData);
      if (result == ERROR_SUCCESS && resultType == REG_DWORD) {
        dValue = dValue / 1024 / 1024;
        destString.AppendInt(PRInt32(dValue));
      } else {
        retval = NS_ERROR_FAILURE;
      }
      break;
    }
    case REG_MULTI_SZ: {
      // A chain of null-separated strings; we convert the nulls to spaces
      WCHAR wCharValue[1024];
      dwcbData = sizeof(wCharValue);

      result = RegQueryValueExW(key, keyName, NULL, &resultType, (LPBYTE)wCharValue, &dwcbData);
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

// The driver ID is a string like PCI\VEN_15AD&DEV_0405&SUBSYS_040515AD, possibly
// followed by &REV_XXXX.  We uppercase the string, and strip the &REV_ part
// from it, if found.
static void normalizeDriverId(nsString& driverid) {
  ToUpperCase(driverid);
  PRInt32 rev = driverid.Find(NS_LITERAL_CSTRING("&REV_"));
  if (rev != -1) {
    driverid.Cut(rev, driverid.Length());
  }
}

// Setup API functions
typedef HDEVINFO (WINAPI*SetupDiGetClassDevsWFunc)(
  CONST GUID *ClassGuid,
  PCWSTR Enumerator,
  HWND hwndParent,
  DWORD Flags
);
typedef BOOL (WINAPI*SetupDiEnumDeviceInfoFunc)(
  HDEVINFO DeviceInfoSet,
  DWORD MemberIndex,
  PSP_DEVINFO_DATA DeviceInfoData
);
typedef BOOL (WINAPI*SetupDiGetDeviceRegistryPropertyWFunc)(
  HDEVINFO DeviceInfoSet,
  PSP_DEVINFO_DATA DeviceInfoData,
  DWORD Property,
  PDWORD PropertyRegDataType,
  PBYTE PropertyBuffer,
  DWORD PropertyBufferSize,
  PDWORD RequiredSize
);
typedef BOOL (WINAPI*SetupDiDestroyDeviceInfoListFunc)(
  HDEVINFO DeviceInfoSet
);

// The device ID is a string like PCI\VEN_15AD&DEV_0405&SUBSYS_040515AD
// this function is used to extract the id's out of it
PRUint32
ParseIDFromDeviceID(const nsAString &key, const char *prefix, int length)
{
  nsAutoString id(key);
  ToUpperCase(id);
  PRInt32 start = id.Find(prefix);
  if (start != -1) {
    id.Cut(0, start + strlen(prefix));
    id.Truncate(length);
  }
  nsresult err;
  return id.ToInteger(&err, 16);
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
  NS_TIME_FUNCTION;

  nsresult rv = GfxInfoBase::Init();

  DISPLAY_DEVICEW displayDevice;
  displayDevice.cb = sizeof(displayDevice);
  int deviceIndex = 0;

  mDeviceKeyDebug = NS_LITERAL_STRING("PrimarySearch");

  while (EnumDisplayDevicesW(NULL, deviceIndex, &displayDevice, 0)) {
    if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      mDeviceKeyDebug = NS_LITERAL_STRING("NullSearch");
      break;
    }
    deviceIndex++;
  }

  // make sure the string is NULL terminated
  if (wcsnlen(displayDevice.DeviceKey, NS_ARRAY_LENGTH(displayDevice.DeviceKey))
      == NS_ARRAY_LENGTH(displayDevice.DeviceKey)) {
    // we did not find a NULL
    return rv;
  }

  mDeviceKeyDebug = displayDevice.DeviceKey;

  /* DeviceKey is "reserved" according to MSDN so we'll be careful with it */
  /* check that DeviceKey begins with DEVICE_KEY_PREFIX */
  /* some systems have a DeviceKey starting with \REGISTRY\Machine\ so we need to compare case insenstively */
  if (_wcsnicmp(displayDevice.DeviceKey, DEVICE_KEY_PREFIX, NS_ARRAY_LENGTH(DEVICE_KEY_PREFIX)-1) != 0)
    return rv;

  // chop off DEVICE_KEY_PREFIX
  mDeviceKey = displayDevice.DeviceKey + NS_ARRAY_LENGTH(DEVICE_KEY_PREFIX)-1;

  mDeviceID = displayDevice.DeviceID;
  mDeviceString = displayDevice.DeviceString;


  HMODULE setupapi = LoadLibraryW(L"setupapi.dll");

  if (setupapi) {
    SetupDiGetClassDevsWFunc setupGetClassDevs = (SetupDiGetClassDevsWFunc)
      GetProcAddress(setupapi, "SetupDiGetClassDevsW");
    SetupDiEnumDeviceInfoFunc setupEnumDeviceInfo = (SetupDiEnumDeviceInfoFunc)
      GetProcAddress(setupapi, "SetupDiEnumDeviceInfo");
    SetupDiGetDeviceRegistryPropertyWFunc setupGetDeviceRegistryProperty = (SetupDiGetDeviceRegistryPropertyWFunc)
      GetProcAddress(setupapi, "SetupDiGetDeviceRegistryPropertyW");
    SetupDiDestroyDeviceInfoListFunc setupDestroyDeviceInfoList = (SetupDiDestroyDeviceInfoListFunc)
      GetProcAddress(setupapi, "SetupDiDestroyDeviceInfoList");

    if (setupGetClassDevs &&
        setupEnumDeviceInfo &&
        setupGetDeviceRegistryProperty &&
        setupDestroyDeviceInfoList) {
      /* create a device information set composed of the current display device */
      HDEVINFO devinfo = setupGetClassDevs(NULL, mDeviceID.get(), NULL,
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
        while (setupEnumDeviceInfo(devinfo, memberIndex++, &devinfoData)) {
          /* get a string that identifies the device's driver key */
          if (setupGetDeviceRegistryProperty(devinfo,
                                             &devinfoData,
                                             SPDRP_DRIVER,
                                             NULL,
                                             (PBYTE)value,
                                             sizeof(value),
                                             NULL)) {
            nsAutoString driverKey(driverKeyPre);
            driverKey += value;
            result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, driverKey.BeginReading(), 0, KEY_QUERY_VALUE, &key);
            if (result == ERROR_SUCCESS) {
              /* we've found the driver we're looking for */
              dwcbData = sizeof(value);
              result = RegQueryValueExW(key, L"DriverVersion", NULL, NULL, (LPBYTE)value, &dwcbData);
              if (result == ERROR_SUCCESS)
                mDriverVersion = value;
              dwcbData = sizeof(value);
              result = RegQueryValueExW(key, L"DriverDate", NULL, NULL, (LPBYTE)value, &dwcbData);
              if (result == ERROR_SUCCESS)
                mDriverDate = value;
              RegCloseKey(key); 

              // Check for second adapter:
              //
              // A second adapter will have the same driver key as the first adapter except for 
              // the last character, where '1' will be swapped for '0' or vice-versa.
              // We know driverKey.Length() > 0 since driverKeyPre is a prefix of driverKey.
              if (driverKey[driverKey.Length()-1] == '0') {
                driverKey.SetCharAt('1', driverKey.Length()-1);
              } else {
                driverKey.SetCharAt('0', driverKey.Length()-1);
              }
              result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, driverKey.BeginReading(), 0, KEY_QUERY_VALUE, &key);
              if (result == ERROR_SUCCESS) {
                mHasDualGPU = true;
                mDeviceKey2 = driverKey;
                dwcbData = sizeof(value);
                result = RegQueryValueExW(key, L"DriverVersion", NULL, NULL, (LPBYTE)value, &dwcbData);
                if (result == ERROR_SUCCESS)
                  mDriverVersion2 = value;
                dwcbData = sizeof(value);
                result = RegQueryValueExW(key, L"DriverDate", NULL, NULL, (LPBYTE)value, &dwcbData);
                if (result == ERROR_SUCCESS)
                  mDriverDate2 = value;
                dwcbData = sizeof(value);
                result = RegQueryValueExW(key, L"Device Description", NULL, NULL, (LPBYTE)value, &dwcbData);
                if (result == ERROR_SUCCESS)
                  mDeviceString2 = value;
                dwcbData = sizeof(value);
                result = RegQueryValueExW(key, L"MatchingDeviceId", NULL, NULL, (LPBYTE)value, &dwcbData);
                if (result == ERROR_SUCCESS)
                  mDeviceID2 = value;
                RegCloseKey(key);
              }  
              break;
            }
          }
        }

        setupDestroyDeviceInfoList(devinfo);
      }
    }

    FreeLibrary(setupapi);
  }

  mAdapterVendorID  = ParseIDFromDeviceID(mDeviceID,  "VEN_", 4);
  mAdapterVendorID2 = ParseIDFromDeviceID(mDeviceID2, "VEN_", 4);
  mAdapterDeviceID  = ParseIDFromDeviceID(mDeviceID,  "&DEV_", 4);
  mAdapterDeviceID2 = ParseIDFromDeviceID(mDeviceID2, "&DEV_", 4);
  mAdapterSubsysID  = ParseIDFromDeviceID(mDeviceID,  "&SUBSYS_", 8);
  mAdapterSubsysID2 = ParseIDFromDeviceID(mDeviceID2, "&SUBSYS_", 8);

  const char *spoofedDriverVersionString = PR_GetEnv("MOZ_GFX_SPOOF_DRIVER_VERSION");
  if (spoofedDriverVersionString) {
    mDriverVersion.AssignASCII(spoofedDriverVersionString);
  }

  const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_VENDOR_ID");
  if (spoofedVendor) {
     PR_sscanf(spoofedVendor, "%x", &mAdapterVendorID);
  }

  mHasDriverVersionMismatch = false;
  if (mAdapterVendorID == vendorIntel) {
    // we've had big crashers (bugs 590373 and 595364) apparently correlated
    // with bad Intel driver installations where the DriverVersion reported
    // by the registry was not the version of the DLL.
    bool is64bitApp = sizeof(void*) == 8;
    const PRUnichar *dllFileName = is64bitApp
                                 ? L"igd10umd64.dll"
                                 : L"igd10umd32.dll";
    nsString dllVersion;
    gfxWindowsPlatform::GetDLLVersion((PRUnichar*)dllFileName, dllVersion);

    PRUint64 dllNumericVersion = 0, driverNumericVersion = 0;
    ParseDriverVersion(dllVersion, &dllNumericVersion);
    ParseDriverVersion(mDriverVersion, &driverNumericVersion);

    // if GetDLLVersion fails, it gives "0.0.0.0"
    // so if GetDLLVersion failed, we get dllNumericVersion = 0
    // so this test implicitly handles the case where GetDLLVersion failed
    if (dllNumericVersion != driverNumericVersion)
      mHasDriverVersionMismatch = true;
  }

  const char *spoofedDevice = PR_GetEnv("MOZ_GFX_SPOOF_DEVICE_ID");
  if (spoofedDevice) {
    PR_sscanf(spoofedDevice, "%x", &mAdapterDeviceID);
  }

  const char *spoofedWindowsVersion = PR_GetEnv("MOZ_GFX_SPOOF_WINDOWS_VERSION");
  if (spoofedWindowsVersion) {
    PR_sscanf(spoofedWindowsVersion, "%x", &mWindowsVersion);
  } else {
    mWindowsVersion = gfxWindowsPlatform::WindowsOSVersion();
  }

  AddCrashReportAnnotations();

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
  if (NS_FAILED(GetKeyValue(mDeviceKey.BeginReading(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD)))
    aAdapterRAM = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey2.BeginReading(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD)))
    aAdapterRAM = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.BeginReading(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
    aAdapterDriver = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey2.BeginReading(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
    aAdapterDriver = L"Unknown";
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

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
{
  *aAdapterVendorID = mAdapterVendorID;
  return NS_OK;
}

/* readonly attribute unsigned long adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(PRUint32 *aAdapterVendorID)
{
  *aAdapterVendorID = mAdapterVendorID2;
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = mAdapterDeviceID;
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = mAdapterDeviceID2;
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

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
  CheckForCiscoVPN();

  nsCAutoString deviceIDString, vendorIDString;
  PRUint32 deviceID, vendorID;
  nsAutoString adapterDriverVersionString;

  GetAdapterDeviceID(&deviceID);
  GetAdapterVendorID(&vendorID);
  GetAdapterDriverVersion(adapterDriverVersionString);

  deviceIDString.AppendPrintf("%04x", deviceID);
  vendorIDString.AppendPrintf("%04x", vendorID);

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
      vendorIDString);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
      deviceIDString);
  
  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsCAutoString note;
  /* AppendPrintf only supports 32 character strings, mrghh. */
  note.AppendPrintf("AdapterVendorID: %04x, ", vendorID);
  note.AppendPrintf("AdapterDeviceID: %04x, ", deviceID);
  note.AppendPrintf("AdapterSubsysID: %08x, ", mAdapterSubsysID);
  note.AppendPrintf("AdapterDriverVersion: ");
  note.Append(NS_LossyConvertUTF16toASCII(adapterDriverVersionString));

  if (vendorID == 0) {
      /* if we didn't find a valid vendorID lets append the mDeviceID string to try to find out why */
      note.Append(", ");
      note.AppendWithConversion(mDeviceID);
      note.Append(", ");
      note.AppendWithConversion(mDeviceKeyDebug);
  }
  note.Append("\n");

  if (mHasDualGPU) {
    PRUint32 deviceID2, vendorID2;
    nsAutoString adapterDriverVersionString2;

    note.Append("Has dual GPUs. GPU #2: ");
    GetAdapterDeviceID2(&deviceID2);
    GetAdapterVendorID2(&vendorID2);
    GetAdapterDriverVersion2(adapterDriverVersionString2);
    note.AppendPrintf("AdapterVendorID2: %04x, ", vendorID2);
    note.AppendPrintf("AdapterDeviceID2: %04x, ", deviceID2);
    note.AppendPrintf("AdapterSubsysID2: %08x, ", mAdapterSubsysID2);
    note.AppendPrintf("AdapterDriverVersion2: ");
    note.Append(NS_LossyConvertUTF16toASCII(adapterDriverVersionString2));
  }
  CrashReporter::AppendAppNotesToCrashReport(note);

#endif
}

#define V(a,b,c,d) GFX_DRIVER_VERSION(a,b,c,d)

static const PRUint32 deviceFamilyIntelGMA500[] = {
    0x8108, /* IntelGMA500_1 */
    0x8109, /* IntelGMA500_2 */
    0
};

static const PRUint32 deviceFamilyIntelGMA900[] = {
    0x2582, /* IntelGMA900_1 */
    0x2782, /* IntelGMA900_2 */
    0x2592, /* IntelGMA900_3 */
    0x2792, /* IntelGMA900_4 */
    0
};

static const PRUint32 deviceFamilyIntelGMA950[] = {
    0x2772, /* Intel945G_1 */
    0x2776, /* Intel945G_2 */
    0x27A2, /* Intel945_1 */
    0x27A6, /* Intel945_2 */
    0x27AE, /* Intel945_3 */
    0
};

static const PRUint32 deviceFamilyIntelGMA3150[] = {
    0xA001, /* IntelGMA3150_Nettop_1 */
    0xA002, /* IntelGMA3150_Nettop_2 */
    0xA011, /* IntelGMA3150_Netbook_1 */
    0xA012, /* IntelGMA3150_Netbook_2 */
    0
};

static const PRUint32 deviceFamilyIntelGMAX3000[] = {
    0x2972, /* Intel946GZ_1 */
    0x2973, /* Intel946GZ_2 */
    0x2982, /* IntelG35_1 */
    0x2983, /* IntelG35_2 */
    0x2992, /* IntelQ965_1 */
    0x2993, /* IntelQ965_2 */
    0x29A2, /* IntelG965_1 */
    0x29A3, /* IntelG965_2 */
    0x29B2, /* IntelQ35_1 */
    0x29B3, /* IntelQ35_2 */
    0x29C2, /* IntelG33_1 */
    0x29C3, /* IntelG33_2 */
    0x29D2, /* IntelQ33_1 */
    0x29D3, /* IntelQ33_2 */
    0x2A02, /* IntelGL960_1 */
    0x2A03, /* IntelGL960_2 */
    0x2A12, /* IntelGM965_1 */
    0x2A13, /* IntelGM965_2 */
    0
};

static const PRUint32 deviceFamilyIntelGMAX4500HD[] = {
    0x2A42, /* IntelGMA4500MHD_1 */
    0x2A43, /* IntelGMA4500MHD_2 */
    0x2E42, /* IntelB43_1 */
    0x2E43, /* IntelB43_2 */
    0x2E92, /* IntelB43_3 */
    0x2E93, /* IntelB43_4 */
    0x2E32, /* IntelG41_1 */
    0x2E33, /* IntelG41_2 */
    0x2E22, /* IntelG45_1 */
    0x2E23, /* IntelG45_2 */
    0x2E12, /* IntelQ45_1 */
    0x2E13, /* IntelQ45_2 */
    0x0042, /* IntelHDGraphics */
    0x0046, /* IntelMobileHDGraphics */
    0x0102, /* IntelSandyBridge_1 */
    0x0106, /* IntelSandyBridge_2 */
    0x0112, /* IntelSandyBridge_3 */
    0x0116, /* IntelSandyBridge_4 */
    0x0122, /* IntelSandyBridge_5 */
    0x0126, /* IntelSandyBridge_6 */
    0x010A, /* IntelSandyBridge_7 */
    0x0080, /* IntelIvyBridge */
    0
};

// Glitches whilst scrolling (see bugs 612007, 644787, 645872)
static const PRUint32 deviceFamilyNvidiaBlockD3D9Layers[] = {
    0x00f3, /* NV43 [GeForce 6200 (TM)] */
    0x0146, /* NV43 [Geforce Go 6600TE/6200TE (TM)] */
    0x014f, /* NV43 [GeForce 6200 (TM)] */
    0x0161, /* NV44 [GeForce 6200 TurboCache (TM)] */
    0x0162, /* NV44 [GeForce 6200SE TurboCache (TM)] */
    0x0163, /* NV44 [GeForce 6200 LE (TM)] */
    0x0164, /* NV44 [GeForce Go 6200 (TM)] */
    0x0167, /* NV43 [GeForce Go 6200/6400 (TM)] */
    0x0168, /* NV43 [GeForce Go 6200/6400 (TM)] */
    0x0169, /* NV44 [GeForce 6250 (TM)] */
    0x0222, /* NV44 [GeForce 6200 A-LE (TM)] */
    0x0240, /* C51PV [GeForce 6150 (TM)] */
    0x0241, /* C51 [GeForce 6150 LE (TM)] */
    0x0244, /* C51 [Geforce Go 6150 (TM)] */
    0x0245, /* C51 [Quadro NVS 210S/GeForce 6150LE (TM)] */
    0x0247, /* C51 [GeForce Go 6100 (TM)] */
    0x03d0, /* C61 [GeForce 6150SE nForce 430 (TM)] */
    0x03d1, /* C61 [GeForce 6100 nForce 405 (TM)] */
    0x03d2, /* C61 [GeForce 6100 nForce 400 (TM)] */
    0x03d5, /* C61 [GeForce 6100 nForce 420 (TM)] */
    0
};

static const GfxDriverInfo gDriverInfo[] = {
  /*
   * Notice that the first match defines the result. So always implement special cases firsts and general case last.
   */

  /*
   * NVIDIA entries
   */
  GfxDriverInfo( DRIVER_OS_WINDOWS_XP,
    vendorNVIDIA, GfxDriverInfo::allDevices,
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
    DRIVER_LESS_THAN, V(6,14,12,5721), "257.21" ),
  GfxDriverInfo( DRIVER_OS_WINDOWS_VISTA,
    vendorNVIDIA, GfxDriverInfo::allDevices,
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
    DRIVER_LESS_THAN, V(8,17,12,5721), "257.21" ),
  GfxDriverInfo( DRIVER_OS_WINDOWS_7,
    vendorNVIDIA, GfxDriverInfo::allDevices,
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
    DRIVER_LESS_THAN, V(8,17,12,5721), "257.21" ),

  /* Disable D3D9 layers on NVIDIA 6100/6150/6200 series due to glitches
   * whilst scrolling. See bugs: 612007, 644787 & 645872.
   */
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorNVIDIA, (GfxDeviceFamily) deviceFamilyNvidiaBlockD3D9Layers,
    nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
    DRIVER_LESS_THAN, allDriverVersions ),

  /*
   * AMD/ATI entries
   */
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorATI, GfxDriverInfo::allDevices,
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
    DRIVER_LESS_THAN, V(8,741,0,0), "10.6" ),
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorAMD, GfxDriverInfo::allDevices,
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
    DRIVER_LESS_THAN, V(8,741,0,0), "10.6" ),

  /* OpenGL on any ATI/AMD hardware is discouraged
   * See:
   *  bug 619773 - WebGL: Crash with blue screen : "NMI: Parity Check / Memory Parity Error"
   *  bugs 584403, 584404, 620924 - crashes in atioglxx
   *  + many complaints about incorrect rendering
   */
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorATI, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorATI, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorAMD, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorAMD, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),

  /*
   * Intel entries
   */

  /* implement the blocklist from bug 594877
   * Block all features on any drivers before this, as there's a crash when a MS Hotfix is installed.
   * The crash itself is Direct2D-related, but for safety we block all features.
   */
#define IMPLEMENT_INTEL_DRIVER_BLOCKLIST(winVer, devFamily, driverVer) \
  GfxDriverInfo( winVer,                                               \
    vendorIntel, (GfxDeviceFamily) devFamily,                          \
    GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,           \
    DRIVER_LESS_THAN, driverVer ),

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMA500,   V(6,14,11,1018))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMA900,   V(6,14,10,4764))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMA950,   V(6,14,10,4926))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMA3150,  V(6,14,10,5260))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMAX3000, V(6,14,10,5218))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_XP, deviceFamilyIntelGMAX4500HD, V(6,14,10,5284))

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMA500,   V(7,14,10,1006))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMA900,   allDriverVersions)
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMA950,   V(7,14,10,1504))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMA3150,  V(7,14,10,2124))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMAX3000, V(7,15,10,1666))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_VISTA, deviceFamilyIntelGMAX4500HD, V(8,15,10,2202))

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMA500,   V(5,0,0,2026))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMA900,   allDriverVersions)
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMA950,   V(8,15,10,1930))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMA3150,  V(8,14,10,2117))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMAX3000, V(8,15,10,1930))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(DRIVER_OS_WINDOWS_7, deviceFamilyIntelGMAX4500HD, V(8,15,10,2202))

  /* OpenGL on any Intel hardware is discouraged */
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorIntel, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),
  GfxDriverInfo( DRIVER_OS_ALL,
    vendorIntel, GfxDriverInfo::allDevices,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_DISCOURAGED,
    DRIVER_LESS_THAN, allDriverVersions ),

  GfxDriverInfo()
};

static OperatingSystem
WindowsVersionToOperatingSystem(PRInt32 aWindowsVersion)
{
  switch(aWindowsVersion) {
    case gfxWindowsPlatform::kWindows2000:
      return DRIVER_OS_WINDOWS_2000;
    case gfxWindowsPlatform::kWindowsXP:
      return DRIVER_OS_WINDOWS_XP;
    case gfxWindowsPlatform::kWindowsServer2003:
      return DRIVER_OS_WINDOWS_SERVER_2003;
    case gfxWindowsPlatform::kWindowsVista:
      return DRIVER_OS_WINDOWS_VISTA;
    case gfxWindowsPlatform::kWindows7:
      return DRIVER_OS_WINDOWS_7;
    case gfxWindowsPlatform::kWindowsUnknown:
    default:
      return DRIVER_OS_UNKNOWN;
    };
}

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, PRInt32 *aStatus, nsAString & aSuggestedDriverVersion, GfxDriverInfo* aDriverInfo /* = nsnull */)
{
  *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
  aSuggestedDriverVersion.SetIsVoid(true);

  PRInt32 status = nsIGfxInfo::FEATURE_NO_INFO;

  PRUint32 adapterVendor = 0;
  PRUint32 adapterDeviceID = 0;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(&adapterVendor)) ||
      NS_FAILED(GetAdapterDeviceID(&adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
  {
    return NS_ERROR_FAILURE;
  }

  if (adapterVendor != vendorIntel &&
      adapterVendor != vendorNVIDIA &&
      adapterVendor != vendorAMD &&
      adapterVendor != vendorATI &&
      // FIXME - these special hex values are currently used in xpcshell tests introduced by
      // bug 625160 patch 8/8. Maybe these tests need to be adjusted now that we're only whitelisting
      // intel/ati/nvidia.
      adapterVendor != 0xabcd &&
      adapterVendor != 0xdcba &&
      adapterVendor != 0xabab &&
      adapterVendor != 0xdcdc)
  {
    *aStatus = FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

  PRUint64 driverVersion;
  if (!ParseDriverVersion(adapterDriverVersionString, &driverVersion)) {
    return NS_ERROR_FAILURE;
  }
  
  if (aFeature == FEATURE_DIRECT3D_9_LAYERS &&
      mWindowsVersion < gfxWindowsPlatform::kWindowsXP)
  {
    *aStatus = FEATURE_BLOCKED_OS_VERSION;
    return NS_OK;
  }

  // ANGLE currently uses D3D10 <-> D3D9 interop, which crashes on Optimus
  // machines.
  if (aFeature == FEATURE_WEBGL_ANGLE &&
      gfxWindowsPlatform::IsOptimus())
  {
    *aStatus = FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

  OperatingSystem os = WindowsVersionToOperatingSystem(mWindowsVersion);

  // Windows Server 2003 should be just like Windows XP for present purpose, but still has a different version number.
  // OTOH Windows Server 2008 R1 and R2 already have the same version numbers as Vista and Seven respectively
  if (os == DRIVER_OS_WINDOWS_SERVER_2003)
    os = DRIVER_OS_WINDOWS_XP;

  const GfxDriverInfo *info;
  if (aDriverInfo)
    info = aDriverInfo;
  else
    info = &gDriverInfo[0];

  if (mHasDriverVersionMismatch) {
    if (aFeature == nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS ||
        aFeature == nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS ||
        aFeature == nsIGfxInfo::FEATURE_DIRECT2D)
    {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
      return NS_OK;
    }
  }

  // special-case the WinXP test slaves: they have out-of-date drivers, but we still want to
  // whitelist them, actually we do know that this combination of device and driver version
  // works well.
  if (os == DRIVER_OS_WINDOWS_XP &&
      adapterVendor == vendorNVIDIA &&
      adapterDeviceID == 0x0861 && // GeForce 9400
      driverVersion == V(6,14,11,7756))
  {
    return NS_OK;
  }

  while (info->mOperatingSystem) {

    if (info->mOperatingSystem != DRIVER_OS_ALL &&
        info->mOperatingSystem != os)
    {
      info++;
      continue;
    }

    if (info->mAdapterVendor != GfxDriverInfo::allAdapterVendors &&
        info->mAdapterVendor != adapterVendor) {
      info++;
      continue;
    }

    if (info->mDevices != GfxDriverInfo::allDevices) {
        bool deviceMatches = false;
        for (const PRUint32 *devices = info->mDevices; *devices; ++devices) {
            if (*devices == adapterDeviceID) {
                deviceMatches = true;
                break;
            }
        }

        if (!deviceMatches) {
            info++;
            continue;
        }
    }

    bool match = false;

    switch (info->mComparisonOp) {
    case DRIVER_LESS_THAN:
      match = driverVersion < info->mDriverVersion;
      break;
    case DRIVER_LESS_THAN_OR_EQUAL:
      match = driverVersion <= info->mDriverVersion;
      break;
    case DRIVER_GREATER_THAN:
      match = driverVersion > info->mDriverVersion;
      break;
    case DRIVER_GREATER_THAN_OR_EQUAL:
      match = driverVersion >= info->mDriverVersion;
      break;
    case DRIVER_EQUAL:
      match = driverVersion == info->mDriverVersion;
      break;
    case DRIVER_NOT_EQUAL:
      match = driverVersion != info->mDriverVersion;
      break;
    case DRIVER_BETWEEN_EXCLUSIVE:
      match = driverVersion > info->mDriverVersion && driverVersion < info->mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE:
      match = driverVersion >= info->mDriverVersion && driverVersion <= info->mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE_START:
      match = driverVersion >= info->mDriverVersion && driverVersion < info->mDriverVersionMax;
      break;
    default:
      NS_WARNING("Bogus op in GfxDriverInfo");
      break;
    }

    if (match) {
      if (info->mFeature == GfxDriverInfo::allFeatures ||
          info->mFeature == aFeature)
      {
        status = info->mFeatureStatus;
        break;
      }
    }

    info++;
  }

  *aStatus = status;

  if (status == FEATURE_BLOCKED_DRIVER_VERSION) {
      if (info->mSuggestedVersion) {
          aSuggestedDriverVersion.AppendPrintf("%s", info->mSuggestedVersion);
      } else if (info->mComparisonOp == DRIVER_LESS_THAN &&
                 info->mDriverVersion != allDriverVersions)
      {
          aSuggestedDriverVersion.AppendPrintf("%lld.%lld.%lld.%lld",
                                               (info->mDriverVersion & 0xffff000000000000) >> 48,
                                               (info->mDriverVersion & 0x0000ffff00000000) >> 32,
                                               (info->mDriverVersion & 0x00000000ffff0000) >> 16,
                                               (info->mDriverVersion & 0x000000000000ffff));
      }
  }
  
  return NS_OK;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in unsigned long aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(PRUint32 aVendorID)
{
  mAdapterVendorID = aVendorID;
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(PRUint32 aDeviceID)
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
NS_IMETHODIMP GfxInfo::SpoofOSVersion(PRUint32 aVersion)
{
  mWindowsVersion = aVersion;
  return NS_OK;
}

#endif
