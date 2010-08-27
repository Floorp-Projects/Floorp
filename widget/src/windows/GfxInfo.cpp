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

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(GfxInfo, nsIGfxInfo)

nsresult GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->GetRenderMode() == gfxWindowsPlatform::RENDER_DIRECT2D;
  return NS_OK;
}

nsresult GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  *aEnabled = gfxWindowsPlatform::GetPlatform()->DWriteEnabled();
  return NS_OK;
}

/* XXX: GfxInfo doesn't handle multiple GPUs. We should try to do that. Bug #591057 */

static nsresult GetKeyValue(const TCHAR* keyLocation, const TCHAR* keyName, nsAString& destString, int type)
{
  HKEY key;
  DWORD dwcbData;
  WCHAR wCharValue[1024];
  TCHAR tCharValue[1024];
  DWORD dValue;
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
      result = RegQueryValueExW(key, keyName, NULL, NULL, (LPBYTE)&dValue, &dwcbData);
      if (result != ERROR_SUCCESS) {
        retval = NS_ERROR_FAILURE;
      }
      dValue = dValue / 1024 / 1024;
      destString.AppendInt(static_cast<PRInt32>(dValue));
      break;
    }
    case REG_MULTI_SZ: {
      // A chain of null-separated strings; we convert the nulls to spaces
      dwcbData = sizeof(tCharValue);
      result = RegQueryValueExW(key, keyName, NULL, NULL, (LPBYTE)tCharValue, &dwcbData);
      if (result != ERROR_SUCCESS) {
        retval = NS_ERROR_FAILURE;
      }
      // This bit here could probably be cleaner.
      for (DWORD i = 0, len = dwcbData/sizeof(tCharValue[0]); i < len; i++) {
        if (tCharValue[i] == '\0')
          tCharValue[i] = ' ';
      }
      destString = tCharValue;
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
void GfxInfo::Init()
{
  NS_TIME_FUNCTION;

  DISPLAY_DEVICE lpDisplayDevice;
  lpDisplayDevice.cb = sizeof(lpDisplayDevice);
  int deviceIndex = 0;

  while (EnumDisplayDevices(NULL, deviceIndex, &lpDisplayDevice, 0)) {
    if (lpDisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      break;
    deviceIndex++;
  }

  /* DeviceKey is "reserved" according to MSDN so we'll be careful with it */
  if (wcsncmp(lpDisplayDevice.DeviceKey, DEVICE_KEY_PREFIX, wcslen(DEVICE_KEY_PREFIX)) != 0)
    return;

  // make sure the string is NULL terminated
  size_t i;
  for (i = 0; i < sizeof(lpDisplayDevice.DeviceKey); i++) {
    if (lpDisplayDevice.DeviceKey[i] == L'\0')
      break;
  }

  if (i == sizeof(lpDisplayDevice.DeviceKey)) {
      // we did not find a NULL
      return;
  }

  // chop off DEVICE_KEY_PREFIX
  mDeviceKey = lpDisplayDevice.DeviceKey + wcslen(DEVICE_KEY_PREFIX);

  mDeviceID = lpDisplayDevice.DeviceID;
  mDeviceString = lpDisplayDevice.DeviceString;


  HKEY key, subkey;
  LONG result, enumresult;
  DWORD index = 0;
  TCHAR subkeyname[64];
  TCHAR value[128];
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
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mDeviceString;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.BeginReading(), L"HardwareInformation.MemorySize", aAdapterRAM, REG_DWORD)))
    aAdapterRAM = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  if (NS_FAILED(GetKeyValue(mDeviceKey.BeginReading(), L"InstalledDisplayDrivers", aAdapterDriver, REG_MULTI_SZ)))
    aAdapterDriver = L"Unknown";
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion = mDriverVersion;
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate = mDriverDate;
  return NS_OK;
}

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
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
NS_IMETHODIMP GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
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
