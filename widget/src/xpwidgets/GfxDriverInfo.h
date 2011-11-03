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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "prtypes.h"
#include "nsString.h"

#ifndef __mozilla_widget_GfxDriverInfo_h__
#define __mozilla_widget_GfxDriverInfo_h__

#define V(a,b,c,d) GFX_DRIVER_VERSION(a,b,c,d)

namespace mozilla {
namespace widget {

enum OperatingSystem {
  DRIVER_OS_UNKNOWN = 0,
  DRIVER_OS_WINDOWS_2000,
  DRIVER_OS_WINDOWS_XP,
  DRIVER_OS_WINDOWS_SERVER_2003,
  DRIVER_OS_WINDOWS_VISTA,
  DRIVER_OS_WINDOWS_7,
  DRIVER_OS_LINUX,
  DRIVER_OS_OS_X_10_5,
  DRIVER_OS_OS_X_10_6,
  DRIVER_OS_OS_X_10_7,
  DRIVER_OS_ANDROID,
  DRIVER_OS_ALL
};

enum VersionComparisonOp {
  DRIVER_LESS_THAN,             // driver <  version
  DRIVER_LESS_THAN_OR_EQUAL,    // driver <= version
  DRIVER_GREATER_THAN,          // driver >  version
  DRIVER_GREATER_THAN_OR_EQUAL, // driver >= version
  DRIVER_EQUAL,                 // driver == version
  DRIVER_NOT_EQUAL,             // driver != version
  DRIVER_BETWEEN_EXCLUSIVE,     // driver > version && driver < versionMax
  DRIVER_BETWEEN_INCLUSIVE,     // driver >= version && driver <= versionMax
  DRIVER_BETWEEN_INCLUSIVE_START, // driver >= version && driver < versionMax
  DRIVER_UNKNOWN_COMPARISON
};

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

/* A zero-terminated array of devices to match, or all devices */
typedef PRUint32 *GfxDeviceFamily;

struct GfxDriverInfo
{
  // If |ownDevices| is true, you are transferring ownership of the devices
  // array, and it will be deleted when this GfxDriverInfo is destroyed.
  GfxDriverInfo(OperatingSystem os, PRUint32 vendor, GfxDeviceFamily devices,
                PRInt32 feature, PRInt32 featureStatus, VersionComparisonOp op,
                PRUint64 driverVersion, const char *suggestedVersion = nsnull,
                bool ownDevices = false);

  GfxDriverInfo();
  GfxDriverInfo(const GfxDriverInfo&);
  ~GfxDriverInfo();

  OperatingSystem mOperatingSystem;

  PRUint32 mAdapterVendor;
  static PRUint32 allAdapterVendors;

  GfxDeviceFamily mDevices;
  static GfxDeviceFamily allDevices;

  // Whether the mDevices array should be deleted when this structure is
  // deallocated. False by default.
  bool mDeleteDevices;

  /* A feature from nsIGfxInfo, or all features */
  PRInt32 mFeature;
  static PRInt32 allFeatures;

  /* A feature status from nsIGfxInfo */
  PRInt32 mFeatureStatus;

  VersionComparisonOp mComparisonOp;

  /* versions are assumed to be A.B.C.D packed as 0xAAAABBBBCCCCDDDD */
  PRUint64 mDriverVersion;
  PRUint64 mDriverVersionMax;
  static PRUint64 allDriverVersions;

  static PRUint32 vendorIntel;
  static PRUint32 vendorNVIDIA;
  static PRUint32 vendorAMD;
  static PRUint32 vendorATI;

  const char *mSuggestedVersion;
};

#define GFX_DRIVER_VERSION(a,b,c,d) \
  ((PRUint64(a)<<48) | (PRUint64(b)<<32) | (PRUint64(c)<<16) | PRUint64(d))

inline bool
ParseDriverVersion(nsAString& aVersion, PRUint64 *aNumericVersion)
{
  int a, b, c, d;
  /* honestly, why do I even bother */
  if (sscanf(NS_LossyConvertUTF16toASCII(aVersion).get(),
             "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return false;
  if (a < 0 || a > 0xffff) return false;
  if (b < 0 || b > 0xffff) return false;
  if (c < 0 || c > 0xffff) return false;
  if (d < 0 || d > 0xffff) return false;

  *aNumericVersion = GFX_DRIVER_VERSION(a, b, c, d);
  return true;
}

}
}

#endif /*__mozilla_widget_GfxDriverInfo_h__ */
