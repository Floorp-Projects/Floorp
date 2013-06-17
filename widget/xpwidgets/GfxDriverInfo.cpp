/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxDriverInfo.h"

#include "nsIGfxInfo.h"
#include "nsTArray.h"

using namespace mozilla::widget;

int32_t GfxDriverInfo::allFeatures = 0;
uint64_t GfxDriverInfo::allDriverVersions = ~(uint64_t(0));
GfxDeviceFamily* const GfxDriverInfo::allDevices = nullptr;

GfxDeviceFamily* GfxDriverInfo::mDeviceFamilies[DeviceFamilyMax];
nsAString* GfxDriverInfo::mDeviceVendors[DeviceVendorMax];

GfxDriverInfo::GfxDriverInfo()
  : mOperatingSystem(DRIVER_OS_UNKNOWN),
    mOperatingSystemVersion(0),
    mAdapterVendor(GfxDriverInfo::GetDeviceVendor(VendorAll)),
    mDevices(allDevices),
    mDeleteDevices(false),
    mFeature(allFeatures),
    mFeatureStatus(nsIGfxInfo::FEATURE_NO_INFO),
    mComparisonOp(DRIVER_COMPARISON_IGNORED),
    mDriverVersion(0),
    mDriverVersionMax(0),
    mSuggestedVersion(nullptr)
{}

GfxDriverInfo::GfxDriverInfo(OperatingSystem os, nsAString& vendor,
                             GfxDeviceFamily* devices,
                             int32_t feature, int32_t featureStatus,
                             VersionComparisonOp op,
                             uint64_t driverVersion,
                             const char *suggestedVersion /* = nullptr */,
                             bool ownDevices /* = false */)
  : mOperatingSystem(os),
    mOperatingSystemVersion(0),
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
    mOperatingSystemVersion(aOrig.mOperatingSystemVersion),
    mAdapterVendor(aOrig.mAdapterVendor),
    mFeature(aOrig.mFeature),
    mFeatureStatus(aOrig.mFeatureStatus),
    mComparisonOp(aOrig.mComparisonOp),
    mDriverVersion(aOrig.mDriverVersion),
    mDriverVersionMax(aOrig.mDriverVersionMax),
    mSuggestedVersion(aOrig.mSuggestedVersion)
{
  // If we're managing the lifetime of the device family, we have to make a
  // copy of the original's device family.
  if (aOrig.mDeleteDevices && aOrig.mDevices) {
    mDevices = new GfxDeviceFamily;
    *mDevices = *aOrig.mDevices;
  } else {
    mDevices = aOrig.mDevices;
  }

  mDeleteDevices = aOrig.mDeleteDevices;
}

GfxDriverInfo::~GfxDriverInfo()
{
  if (mDeleteDevices)
    delete mDevices;
}

// Macros for appending a device to the DeviceFamily.
#define APPEND_DEVICE(device) APPEND_DEVICE2(#device)
#define APPEND_DEVICE2(device) deviceFamily->AppendElement(NS_LITERAL_STRING(device))

const GfxDeviceFamily* GfxDriverInfo::GetDeviceFamily(DeviceFamily id)
{
  // The code here is too sensitive to fall through to the default case if the
  // code is invalid.
  NS_ASSERTION(id >= 0 && id < DeviceFamilyMax, "DeviceFamily id is out of range");

  // If it already exists, we must have processed it once, so return it now.
  if (mDeviceFamilies[id])
    return mDeviceFamilies[id];

  mDeviceFamilies[id] = new GfxDeviceFamily;
  GfxDeviceFamily* deviceFamily = mDeviceFamilies[id];

  switch (id) {
    case IntelGMA500:
      APPEND_DEVICE(0x8108); /* IntelGMA500_1 */
      APPEND_DEVICE(0x8109); /* IntelGMA500_2 */
      break;
    case IntelGMA900:
      APPEND_DEVICE(0x2582); /* IntelGMA900_1 */
      APPEND_DEVICE(0x2782); /* IntelGMA900_2 */
      APPEND_DEVICE(0x2592); /* IntelGMA900_3 */
      APPEND_DEVICE(0x2792); /* IntelGMA900_4 */
      break;
    case IntelGMA950:
      APPEND_DEVICE(0x2772); /* Intel945G_1 */
      APPEND_DEVICE(0x2776); /* Intel945G_2 */
      APPEND_DEVICE(0x27a2); /* Intel945_1 */
      APPEND_DEVICE(0x27a6); /* Intel945_2 */
      APPEND_DEVICE(0x27ae); /* Intel945_3 */
      break;
    case IntelGMA3150:
      APPEND_DEVICE(0xa001); /* IntelGMA3150_Nettop_1 */
      APPEND_DEVICE(0xa002); /* IntelGMA3150_Nettop_2 */
      APPEND_DEVICE(0xa011); /* IntelGMA3150_Netbook_1 */
      APPEND_DEVICE(0xa012); /* IntelGMA3150_Netbook_2 */
      break;
    case IntelGMAX3000:
      APPEND_DEVICE(0x2972); /* Intel946GZ_1 */
      APPEND_DEVICE(0x2973); /* Intel946GZ_2 */
      APPEND_DEVICE(0x2982); /* IntelG35_1 */
      APPEND_DEVICE(0x2983); /* IntelG35_2 */
      APPEND_DEVICE(0x2992); /* IntelQ965_1 */
      APPEND_DEVICE(0x2993); /* IntelQ965_2 */
      APPEND_DEVICE(0x29a2); /* IntelG965_1 */
      APPEND_DEVICE(0x29a3); /* IntelG965_2 */
      APPEND_DEVICE(0x29b2); /* IntelQ35_1 */
      APPEND_DEVICE(0x29b3); /* IntelQ35_2 */
      APPEND_DEVICE(0x29c2); /* IntelG33_1 */
      APPEND_DEVICE(0x29c3); /* IntelG33_2 */
      APPEND_DEVICE(0x29d2); /* IntelQ33_1 */
      APPEND_DEVICE(0x29d3); /* IntelQ33_2 */
      APPEND_DEVICE(0x2a02); /* IntelGL960_1 */
      APPEND_DEVICE(0x2a03); /* IntelGL960_2 */
      APPEND_DEVICE(0x2a12); /* IntelGM965_1 */
      APPEND_DEVICE(0x2a13); /* IntelGM965_2 */
      break;
    case IntelGMAX4500HD:
      APPEND_DEVICE(0x2a42); /* IntelGMA4500MHD_1 */
      APPEND_DEVICE(0x2a43); /* IntelGMA4500MHD_2 */
      APPEND_DEVICE(0x2e42); /* IntelB43_1 */
      APPEND_DEVICE(0x2e43); /* IntelB43_2 */
      APPEND_DEVICE(0x2e92); /* IntelB43_3 */
      APPEND_DEVICE(0x2e93); /* IntelB43_4 */
      APPEND_DEVICE(0x2e32); /* IntelG41_1 */
      APPEND_DEVICE(0x2e33); /* IntelG41_2 */
      APPEND_DEVICE(0x2e22); /* IntelG45_1 */
      APPEND_DEVICE(0x2e23); /* IntelG45_2 */
      APPEND_DEVICE(0x2e12); /* IntelQ45_1 */
      APPEND_DEVICE(0x2e13); /* IntelQ45_2 */
      APPEND_DEVICE(0x0042); /* IntelHDGraphics */
      APPEND_DEVICE(0x0046); /* IntelMobileHDGraphics */
      APPEND_DEVICE(0x0102); /* IntelSandyBridge_1 */
      APPEND_DEVICE(0x0106); /* IntelSandyBridge_2 */
      APPEND_DEVICE(0x0112); /* IntelSandyBridge_3 */
      APPEND_DEVICE(0x0116); /* IntelSandyBridge_4 */
      APPEND_DEVICE(0x0122); /* IntelSandyBridge_5 */
      APPEND_DEVICE(0x0126); /* IntelSandyBridge_6 */
      APPEND_DEVICE(0x010a); /* IntelSandyBridge_7 */
      APPEND_DEVICE(0x0080); /* IntelIvyBridge */
      break;
    case IntelMobileHDGraphics:
      APPEND_DEVICE(0x0046); /* IntelMobileHDGraphics */
      break;
    case NvidiaBlockD3D9Layers:
      // Glitches whilst scrolling (see bugs 612007, 644787, 645872)
      APPEND_DEVICE(0x00f3); /* NV43 [GeForce 6200 (TM)] */
      APPEND_DEVICE(0x0146); /* NV43 [Geforce Go 6600TE/6200TE (TM)] */
      APPEND_DEVICE(0x014f); /* NV43 [GeForce 6200 (TM)] */
      APPEND_DEVICE(0x0161); /* NV44 [GeForce 6200 TurboCache (TM)] */
      APPEND_DEVICE(0x0162); /* NV44 [GeForce 6200SE TurboCache (TM)] */
      APPEND_DEVICE(0x0163); /* NV44 [GeForce 6200 LE (TM)] */
      APPEND_DEVICE(0x0164); /* NV44 [GeForce Go 6200 (TM)] */
      APPEND_DEVICE(0x0167); /* NV43 [GeForce Go 6200/6400 (TM)] */
      APPEND_DEVICE(0x0168); /* NV43 [GeForce Go 6200/6400 (TM)] */
      APPEND_DEVICE(0x0169); /* NV44 [GeForce 6250 (TM)] */
      APPEND_DEVICE(0x0222); /* NV44 [GeForce 6200 A-LE (TM)] */
      APPEND_DEVICE(0x0240); /* C51PV [GeForce 6150 (TM)] */
      APPEND_DEVICE(0x0241); /* C51 [GeForce 6150 LE (TM)] */
      APPEND_DEVICE(0x0244); /* C51 [Geforce Go 6150 (TM)] */
      APPEND_DEVICE(0x0245); /* C51 [Quadro NVS 210S/GeForce 6150LE (TM)] */
      APPEND_DEVICE(0x0247); /* C51 [GeForce Go 6100 (TM)] */
      APPEND_DEVICE(0x03d0); /* C61 [GeForce 6150SE nForce 430 (TM)] */
      APPEND_DEVICE(0x03d1); /* C61 [GeForce 6100 nForce 405 (TM)] */
      APPEND_DEVICE(0x03d2); /* C61 [GeForce 6100 nForce 400 (TM)] */
      APPEND_DEVICE(0x03d5); /* C61 [GeForce 6100 nForce 420 (TM)] */
      break;
    case RadeonX1000:
      // This list is from the ATIRadeonX1000.kext Info.plist
      APPEND_DEVICE(0x7187);
      APPEND_DEVICE(0x7210);
      APPEND_DEVICE(0x71de);
      APPEND_DEVICE(0x7146);
      APPEND_DEVICE(0x7142);
      APPEND_DEVICE(0x7109);
      APPEND_DEVICE(0x71c5);
      APPEND_DEVICE(0x71c0);
      APPEND_DEVICE(0x7240);
      APPEND_DEVICE(0x7249);
      APPEND_DEVICE(0x7291);
      break;
    case Geforce7300GT:
      APPEND_DEVICE(0x0393);
      break;
    // This should never happen, but we get a warning if we don't handle this.
    case DeviceFamilyMax:
      NS_WARNING("Invalid DeviceFamily id");
      break;
  }

  return deviceFamily;
}

// Macro for assigning a device vendor id to a string.
#define DECLARE_VENDOR_ID(name, deviceId) \
  case name: \
    mDeviceVendors[id]->AssignLiteral(deviceId); \
    break;

const nsAString& GfxDriverInfo::GetDeviceVendor(DeviceVendor id)
{
  NS_ASSERTION(id >= 0 && id < DeviceVendorMax, "DeviceVendor id is out of range");

  if (mDeviceVendors[id])
    return *mDeviceVendors[id];

  mDeviceVendors[id] = new nsString();

  switch (id) {
    DECLARE_VENDOR_ID(VendorAll, "");
    DECLARE_VENDOR_ID(VendorIntel, "0x8086");
    DECLARE_VENDOR_ID(VendorNVIDIA, "0x10de");
    DECLARE_VENDOR_ID(VendorAMD, "0x1022");
    DECLARE_VENDOR_ID(VendorATI, "0x1002");
    DECLARE_VENDOR_ID(VendorMicrosoft, "0x1414");
    // Suppress a warning.
    DECLARE_VENDOR_ID(DeviceVendorMax, "");
  }

  return *mDeviceVendors[id];
}
