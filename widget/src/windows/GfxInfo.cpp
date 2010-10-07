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
#include "gfxWindowsPlatform.h"
#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"

#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#endif


using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(GfxInfo, nsIGfxInfo)

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after gfxPlatform initialization
 * has occurred because they depend on it for information. (See bug 591561) */
nsresult
GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->GetRenderMode() == gfxWindowsPlatform::RENDER_DIRECT2D;
  return NS_OK;
}

nsresult
GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->DWriteEnabled();
  return NS_OK;
}

/* XXX: GfxInfo doesn't handle multiple GPUs. We should try to do that. Bug #591057 */

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



/* Other interesting places for info:
 *   IDXGIAdapter::GetDesc()
 *   IDirectDraw7::GetAvailableVidMem()
 *   e->GetAvailableTextureMem()
 * */

#define DEVICE_KEY_PREFIX L"\\Registry\\Machine\\"
void
GfxInfo::Init()
{
  NS_TIME_FUNCTION;

  DISPLAY_DEVICEW displayDevice;
  displayDevice.cb = sizeof(displayDevice);
  int deviceIndex = 0;

  while (EnumDisplayDevicesW(NULL, deviceIndex, &displayDevice, 0)) {
    if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      break;
    deviceIndex++;
  }

  /* DeviceKey is "reserved" according to MSDN so we'll be careful with it */
  /* check that DeviceKey begins with DEVICE_KEY_PREFIX */
  /* some systems have a DeviceKey starting with \REGISTRY\Machine\ so we need to compare case insenstively */
  if (_wcsnicmp(displayDevice.DeviceKey, DEVICE_KEY_PREFIX, NS_ARRAY_LENGTH(DEVICE_KEY_PREFIX)-1) != 0)
    return;

  // make sure the string is NULL terminated
  if (wcsnlen(displayDevice.DeviceKey, NS_ARRAY_LENGTH(displayDevice.DeviceKey))
      == NS_ARRAY_LENGTH(displayDevice.DeviceKey)) {
    // we did not find a NULL
    return;
  }

  // chop off DEVICE_KEY_PREFIX
  mDeviceKey = displayDevice.DeviceKey + NS_ARRAY_LENGTH(DEVICE_KEY_PREFIX)-1;

  mDeviceID = displayDevice.DeviceID;
  mDeviceString = displayDevice.DeviceString;


  HKEY key, subkey;
  LONG result, enumresult;
  DWORD index = 0;
  WCHAR subkeyname[64];
  WCHAR value[128];
  DWORD dwcbData = sizeof(subkeyname);

  // "{4D36E968-E325-11CE-BFC1-08002BE10318}" is the display class
  result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                        L"System\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}", 
                        0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &key);
  if (result != ERROR_SUCCESS) {
    return;
  }

  nsAutoString wantedDriverId(mDeviceID);
  normalizeDriverId(wantedDriverId);

  while ((enumresult = RegEnumKeyExW(key, index, subkeyname, &dwcbData, NULL, NULL, NULL, NULL)) != ERROR_NO_MORE_ITEMS) {
    result = RegOpenKeyExW(key, subkeyname, 0, KEY_QUERY_VALUE, &subkey);
    if (result == ERROR_SUCCESS) {
      dwcbData = sizeof(value);
      result = RegQueryValueExW(subkey, L"MatchingDeviceId", NULL, NULL, (LPBYTE)value, &dwcbData);
      if (result == ERROR_SUCCESS) {
        nsAutoString matchingDeviceId(value);
        normalizeDriverId(matchingDeviceId);
        if (wantedDriverId.Find(matchingDeviceId) > -1) {
          /* we've found the driver we're looking for */
          result = RegQueryValueExW(subkey, L"DriverVersion", NULL, NULL, (LPBYTE)value, &dwcbData);
          if (result == ERROR_SUCCESS)
            mDriverVersion = value;
          result = RegQueryValueExW(subkey, L"DriverDate", NULL, NULL, (LPBYTE)value, &dwcbData);
          if (result == ERROR_SUCCESS)
            mDriverDate = value;
          break;
        }
      }
      RegCloseKey(subkey);
    }
    index++;
    dwcbData = sizeof(subkeyname);
  }

  RegCloseKey(key);


  AddCrashReportAnnotations();
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString;
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

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.BeginReading(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
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

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
{
  nsAutoString vendor(mDeviceID);
  ToUpperCase(vendor);
  PRInt32 start = vendor.Find(NS_LITERAL_CSTRING("VEN_"));
  if (start != -1) {
    vendor.Cut(0, start + strlen("VEN_"));
    vendor.Truncate(4);
  }
  nsresult err;
  *aAdapterVendorID = vendor.ToInteger(&err, 16);
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  nsAutoString device(mDeviceID);
  ToUpperCase(device);
  PRInt32 start = device.Find(NS_LITERAL_CSTRING("&DEV_"));
  if (start != -1) {
    device.Cut(0, start + strlen("&DEV_"));
    device.Truncate(4);
  }
  nsresult err;
  *aAdapterDeviceID = device.ToInteger(&err, 16);
  return NS_OK;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
  nsCAutoString deviceIDString, vendorIDString;
  PRUint32 deviceID, vendorID;

  GetAdapterDeviceID(&deviceID);
  GetAdapterVendorID(&vendorID);

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
  note.AppendPrintf("AdapterDeviceID: %04x\n", deviceID);

  CrashReporter::AppendAppNotesToCrashReport(note);

#endif
}

enum VersionComparisonOp {
  DRIVER_LESS_THAN,             // driver <  version
  DRIVER_LESS_THAN_OR_EQUAL,    // driver <= version
  DRIVER_GREATER_THAN,          // driver >  version
  DRIVER_GREATER_THAN_OR_EQUAL, // driver >= version
  DRIVER_EQUAL,                 // driver == version
  DRIVER_NOT_EQUAL,             // driver != version
  DRIVER_BETWEEN_EXCLUSIVE,     // driver > version && driver < versionMax
  DRIVER_BETWEEN_INCLUSIVE,     // driver >= version && driver <= versionMax
  DRIVER_BETWEEN_INCLUSIVE_START // driver >= version && driver < versionMax
};

typedef const PRUint32 *GfxDeviceFamily;

struct GfxDriverInfo {
  PRUint32 windowsVersion;

  PRUint32 vendor;
  GfxDeviceFamily devices;

  PRInt32 feature;
  PRInt32 featureStatus;

  VersionComparisonOp op;

  /* versions are assumed to be A.B.C.D packed as 0xAAAABBBBCCCCDDDD */
  PRUint64 version;
  PRUint64 versionMax;
};

static const PRUint32 allWindowsVersions = 0xffffffff;
static const PRInt32  allFeatures = -1;
static const PRUint32 *allDevices = (PRUint32*) nsnull;
static const PRUint64 allDriverVersions = 0xffffffffffffffffULL;

/* Intel vendor and device IDs */
static const PRUint32 vendorIntel = 0x8086;

/* NVIDIA vendor and device IDs */

/* AMD vendor and device IDs */

#define V(a,b,c,d)   ((PRUint64(a)<<48) | (PRUint64(b)<<32) | (PRUint64(c)<<16) | PRUint64(d))

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

// see bug 595364 comment 10
static const PRUint32 deviceFamilyIntelBlockDirect2D[] = {
    0x2982, /* IntelG35_1 */
    0x2983, /* IntelG35_2 */
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

static const GfxDriverInfo driverInfo[] = {
  /*
   * Notice that the first match defines the result. So always implement special cases firsts and general case last.
   */

  /*
   * Intel entries
   */

  /*
   * Implement special Direct2D blocklist from bug 595364
   */
  { allWindowsVersions,
    vendorIntel, deviceFamilyIntelBlockDirect2D,
    nsIGfxInfo::FEATURE_DIRECT2D, nsIGfxInfo::FEATURE_BLOCKED,
    DRIVER_LESS_THAN, allDriverVersions },

  /* implement the blocklist from bug 594877
   * Block all features on any drivers before this, as there's a crash when a MS Hotfix is installed.
   * The crash itself is Direct2D-related, but for safety we block all features.
   */
#define IMPLEMENT_INTEL_DRIVER_BLOCKLIST(winVer, devFamily, driverVer) \
  { winVer,                                                            \
    vendorIntel, devFamily,                                            \
    allFeatures, nsIGfxInfo::FEATURE_BLOCKED,                          \
    DRIVER_LESS_THAN, driverVer },

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMA500,   V(6,14,11,1018))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMA900,   V(6,14,10,4764))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMA950,   V(6,14,10,4926))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMA3150,  V(6,14,10,5260))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMAX3000, V(6,14,10,5218))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsXP, deviceFamilyIntelGMAX4500HD, V(6,14,10,5284))

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMA500,   V(7,14,10,1006))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMA900,   allDriverVersions)
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMA950,   V(7,14,10,1504))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMA3150,  V(7,14,10,2124))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMAX3000, V(7,15,10,1666))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindowsVista, deviceFamilyIntelGMAX4500HD, V(8,15,10,2202))

  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMA500,   V(5,0,0,2026))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMA900,   allDriverVersions)
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMA950,   V(8,15,10,1930))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMA3150,  V(8,14,10,2117))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMAX3000, V(8,15,10,1930))
  IMPLEMENT_INTEL_DRIVER_BLOCKLIST(gfxWindowsPlatform::kWindows7, deviceFamilyIntelGMAX4500HD, V(8,15,10,2202))

  /* OpenGL on any Intel hardware is not suggested */
  { allWindowsVersions,
    vendorIntel, allDevices,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_NOT_SUGGESTED,
    DRIVER_LESS_THAN, allDriverVersions },
  { allWindowsVersions,
    vendorIntel, allDevices,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_NOT_SUGGESTED,
    DRIVER_LESS_THAN, allDriverVersions },

  /*
   * NVIDIA entries
   */

  /*
   * AMD entries
   */

  { 0, 0, allDevices, 0 }
};

static bool
ParseDriverVersion(nsAString& aVersion, PRUint64 *aNumericVersion)
{
  int a, b, c, d;
  /* honestly, why do I even bother */
  if (sscanf(nsPromiseFlatCString(NS_LossyConvertUTF16toASCII(aVersion)).get(),
             "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return false;
  if (a < 0 || a > 0xffff) return false;
  if (b < 0 || b > 0xffff) return false;
  if (c < 0 || c > 0xffff) return false;
  if (d < 0 || d > 0xffff) return false;

  *aNumericVersion = V(a, b, c, d);
  return true;
}

NS_IMETHODIMP
GfxInfo::GetFeatureStatus(PRInt32 aFeature, PRInt32 *aStatus)
{
  PRInt32 status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  PRUint32 adapterVendor = 0;
  PRUint32 adapterDeviceID = 0;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(&adapterVendor)) ||
      NS_FAILED(GetAdapterDeviceID(&adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
  {
    return NS_ERROR_FAILURE;
  }

  PRUint64 driverVersion;
  if (!ParseDriverVersion(adapterDriverVersionString, &driverVersion)) {
    return NS_ERROR_FAILURE;
  }

  const GfxDriverInfo *info = &driverInfo[0];
  while (info->windowsVersion) {

    if (info->windowsVersion != allWindowsVersions &&
        info->windowsVersion != gfxWindowsPlatform::WindowsOSVersion())
    {
      info++;
      continue;
    }

    if (info->vendor != adapterVendor) {
      info++;
      continue;
    }

    if (info->devices != allDevices) {
        bool deviceMatches = false;
        for (const PRUint32 *devices = info->devices; *devices; ++devices) {
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

    switch (info->op) {
    case DRIVER_LESS_THAN:
      match = driverVersion < info->version;
      break;
    case DRIVER_LESS_THAN_OR_EQUAL:
      match = driverVersion <= info->version;
      break;
    case DRIVER_GREATER_THAN:
      match = driverVersion > info->version;
      break;
    case DRIVER_GREATER_THAN_OR_EQUAL:
      match = driverVersion >= info->version;
      break;
    case DRIVER_EQUAL:
      match = driverVersion == info->version;
      break;
    case DRIVER_NOT_EQUAL:
      match = driverVersion != info->version;
      break;
    case DRIVER_BETWEEN_EXCLUSIVE:
      match = driverVersion > info->version && driverVersion < info->versionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE:
      match = driverVersion >= info->version && driverVersion <= info->versionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE_START:
      match = driverVersion >= info->version && driverVersion < info->versionMax;
      break;
    default:
      NS_WARNING("Bogus op in GfxDriverInfo");
      break;
    }

    if (match) {
      if (info->feature == allFeatures ||
          info->feature == aFeature)
      {
        status = info->featureStatus;
        break;
      }
    }

    info++;
  }

  *aStatus = status;
  return NS_OK;
}

