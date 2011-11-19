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

#include "GfxDriverInfo.h"
#include "nsIGfxInfo.h"

using namespace mozilla::widget;

PRUint32 GfxDriverInfo::allAdapterVendors = 0;
PRInt32 GfxDriverInfo::allFeatures = 0;
PRUint64 GfxDriverInfo::allDriverVersions = ~(PRUint64(0));

PRUint32 GfxDriverInfo::vendorIntel = 0x8086;
PRUint32 GfxDriverInfo::vendorNVIDIA = 0x10de;
PRUint32 GfxDriverInfo::vendorAMD = 0x1022;
PRUint32 GfxDriverInfo::vendorATI = 0x1002;

GfxDeviceFamily GfxDriverInfo::allDevices = nsnull;

GfxDriverInfo::GfxDriverInfo()
  : mOperatingSystem(DRIVER_OS_UNKNOWN),
    mAdapterVendor(allAdapterVendors),
    mDevices(allDevices),
    mDeleteDevices(false),
    mFeature(allFeatures),
    mFeatureStatus(nsIGfxInfo::FEATURE_NO_INFO),
    mComparisonOp(DRIVER_UNKNOWN_COMPARISON),
    mDriverVersion(0),
    mDriverVersionMax(0),
    mSuggestedVersion(nsnull)
{}

GfxDriverInfo::GfxDriverInfo(OperatingSystem os, PRUint32 vendor,
                             GfxDeviceFamily devices,
                             PRInt32 feature, PRInt32 featureStatus,
                             VersionComparisonOp op,
                             PRUint64 driverVersion,
                             const char *suggestedVersion /* = nsnull */,
                             bool ownDevices /* = false */)
  : mOperatingSystem(os),
    mAdapterVendor(vendor),
    mDevices(devices),
    mDeleteDevices(ownDevices),
    mFeature(feature),
    mFeatureStatus(featureStatus),
    mComparisonOp(op),
    mDriverVersion(driverVersion),
    mDriverVersionMax(0),
    mSuggestedVersion(suggestedVersion)
{}

GfxDriverInfo::GfxDriverInfo(const GfxDriverInfo& aOrig)
  : mOperatingSystem(aOrig.mOperatingSystem),
    mAdapterVendor(aOrig.mAdapterVendor),
    mFeature(aOrig.mFeature),
    mFeatureStatus(aOrig.mFeatureStatus),
    mComparisonOp(aOrig.mComparisonOp),
    mDriverVersion(aOrig.mDriverVersion),
    mDriverVersionMax(aOrig.mDriverVersionMax),
    mSuggestedVersion(aOrig.mSuggestedVersion)
{
  // If we're managing the lifetime of the devices array, we have to make a
  // copy of the original's array.
  if (aOrig.mDeleteDevices) {
    PRUint32 count = 0;
    const PRUint32 *device = aOrig.mDevices;
    while (*device) {
      count++;
      device++;
    }

    mDevices = new PRUint32[count + 1];
    memcpy(mDevices, aOrig.mDevices, sizeof(PRUint32) * (count + 1));
  } else {
    mDevices = aOrig.mDevices;
  }

  mDeleteDevices = aOrig.mDeleteDevices;
}

GfxDriverInfo::~GfxDriverInfo()
{
  if (mDeleteDevices)
    delete[] mDevices;
}

const GfxDeviceFamily GfxDriverInfo::GetDeviceFamily(DeviceFamily id)
{
  switch (id) {
    case IntelGMA500: {
      static const PRUint32 intelGMA500[] = {
        0x8108, /* IntelGMA500_1 */
        0x8109, /* IntelGMA500_2 */
        0
      };
      return (const GfxDeviceFamily) &intelGMA500[0];
    }
    case IntelGMA900: {
      static const PRUint32 intelGMA900[] = {
        0x2582, /* IntelGMA900_1 */
        0x2782, /* IntelGMA900_2 */
        0x2592, /* IntelGMA900_3 */
        0x2792, /* IntelGMA900_4 */
        0
      };
      return (const GfxDeviceFamily) &intelGMA900[0];
    }
    case IntelGMA950: {
      static const PRUint32 intelGMA950[] = {
        0x2772, /* Intel945G_1 */
        0x2776, /* Intel945G_2 */
        0x27A2, /* Intel945_1 */
        0x27A6, /* Intel945_2 */
        0x27AE, /* Intel945_3 */
        0
      };
      return (const GfxDeviceFamily) &intelGMA950[0];
    }
    case IntelGMA3150: {
      static const PRUint32 intelGMA3150[] = {
        0xA001, /* IntelGMA3150_Nettop_1 */
        0xA002, /* IntelGMA3150_Nettop_2 */
        0xA011, /* IntelGMA3150_Netbook_1 */
        0xA012, /* IntelGMA3150_Netbook_2 */
        0
      };
      return (const GfxDeviceFamily) &intelGMA3150[0];
    }
    case IntelGMAX3000: {
      static const PRUint32 intelGMAX3000[] = {
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
      return (const GfxDeviceFamily) &intelGMAX3000[0];
    }
    case IntelGMAX4500HD: {
      static const PRUint32 intelGMAX4500HD[] = {
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
      return (const GfxDeviceFamily) &intelGMAX4500HD[0];
    }
    case NvidiaBlockD3D9Layers: {
      // Glitches whilst scrolling (see bugs 612007, 644787, 645872)
      static const PRUint32 nvidiaBlockD3D9Layers[] = {
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
      return (const GfxDeviceFamily) &nvidiaBlockD3D9Layers[0];
    }
    default:
      NS_WARNING("Invalid device family");
  }

  return nsnull;
}
