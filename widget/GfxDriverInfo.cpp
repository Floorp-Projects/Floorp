/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxDriverInfo.h"

#include "nsIGfxInfo.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"

using namespace mozilla::widget;

int32_t GfxDriverInfo::allFeatures = 0;
uint64_t GfxDriverInfo::allDriverVersions = ~(uint64_t(0));

GfxDeviceFamily*
    GfxDriverInfo::sDeviceFamilies[static_cast<size_t>(DeviceFamily::Max)];
nsString*
    GfxDriverInfo::sWindowProtocol[static_cast<size_t>(WindowProtocol::Max)];
nsString* GfxDriverInfo::sDeviceVendors[static_cast<size_t>(DeviceVendor::Max)];
nsString* GfxDriverInfo::sDriverVendors[static_cast<size_t>(DriverVendor::Max)];

GfxDriverInfo::GfxDriverInfo()
    : mOperatingSystem(OperatingSystem::Unknown),
      mOperatingSystemVersion(0),
      mScreen(ScreenSizeStatus::All),
      mBattery(BatteryStatus::All),
      mWindowProtocol(GfxDriverInfo::GetWindowProtocol(WindowProtocol::All)),
      mAdapterVendor(GfxDriverInfo::GetDeviceVendor(DeviceFamily::All)),
      mDriverVendor(GfxDriverInfo::GetDriverVendor(DriverVendor::All)),
      mDevices(GfxDriverInfo::GetDeviceFamily(DeviceFamily::All)),
      mDeleteDevices(false),
      mFeature(allFeatures),
      mFeatureStatus(nsIGfxInfo::FEATURE_STATUS_OK),
      mComparisonOp(DRIVER_COMPARISON_IGNORED),
      mDriverVersion(0),
      mDriverVersionMax(0),
      mSuggestedVersion(nullptr),
      mRuleId(nullptr),
      mGpu2(false) {}

GfxDriverInfo::GfxDriverInfo(
    OperatingSystem os, ScreenSizeStatus screen, BatteryStatus battery,
    const nsAString& windowProtocol, const nsAString& vendor,
    const nsAString& driverVendor, GfxDeviceFamily* devices, int32_t feature,
    int32_t featureStatus, VersionComparisonOp op, uint64_t driverVersion,
    const char* ruleId, const char* suggestedVersion /* = nullptr */,
    bool ownDevices /* = false */, bool gpu2 /* = false */)
    : mOperatingSystem(os),
      mOperatingSystemVersion(0),
      mScreen(screen),
      mBattery(battery),
      mWindowProtocol(windowProtocol),
      mAdapterVendor(vendor),
      mDriverVendor(driverVendor),
      mDevices(devices),
      mDeleteDevices(ownDevices),
      mFeature(feature),
      mFeatureStatus(featureStatus),
      mComparisonOp(op),
      mDriverVersion(driverVersion),
      mDriverVersionMax(0),
      mSuggestedVersion(suggestedVersion),
      mRuleId(ruleId),
      mGpu2(gpu2) {}

GfxDriverInfo::GfxDriverInfo(const GfxDriverInfo& aOrig)
    : mOperatingSystem(aOrig.mOperatingSystem),
      mOperatingSystemVersion(aOrig.mOperatingSystemVersion),
      mScreen(aOrig.mScreen),
      mBattery(aOrig.mBattery),
      mWindowProtocol(aOrig.mWindowProtocol),
      mAdapterVendor(aOrig.mAdapterVendor),
      mDriverVendor(aOrig.mDriverVendor),
      mFeature(aOrig.mFeature),
      mFeatureStatus(aOrig.mFeatureStatus),
      mComparisonOp(aOrig.mComparisonOp),
      mDriverVersion(aOrig.mDriverVersion),
      mDriverVersionMax(aOrig.mDriverVersionMax),
      mSuggestedVersion(aOrig.mSuggestedVersion),
      mRuleId(aOrig.mRuleId),
      mGpu2(aOrig.mGpu2) {
  // If we're managing the lifetime of the device family, we have to make a
  // copy of the original's device family.
  if (aOrig.mDeleteDevices && aOrig.mDevices) {
    GfxDeviceFamily* devices = new GfxDeviceFamily;
    *devices = *aOrig.mDevices;
    mDevices = devices;
  } else {
    mDevices = aOrig.mDevices;
  }

  mDeleteDevices = aOrig.mDeleteDevices;
}

GfxDriverInfo::~GfxDriverInfo() {
  if (mDeleteDevices) {
    delete mDevices;
  }
}

void GfxDeviceFamily::Append(const nsAString& aDeviceId) {
  mIds.AppendElement(aDeviceId);
}

void GfxDeviceFamily::AppendRange(int32_t aBeginDeviceId,
                                  int32_t aEndDeviceId) {
  mRanges.AppendElement(
      GfxDeviceFamily::DeviceRange{aBeginDeviceId, aEndDeviceId});
}

nsresult GfxDeviceFamily::Contains(nsAString& aDeviceId) const {
  for (const auto& id : mIds) {
    if (id.Equals(aDeviceId, nsCaseInsensitiveStringComparator)) {
      return NS_OK;
    }
  }

  if (mRanges.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult valid = NS_OK;
  int32_t deviceId = aDeviceId.ToInteger(&valid, 16);
  if (valid != NS_OK) {
    return NS_ERROR_INVALID_ARG;
  }

  for (const auto& range : mRanges) {
    if (deviceId >= range.mBegin && deviceId <= range.mEnd) {
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

// Macros for appending a device to the DeviceFamily.
#define APPEND_DEVICE(device) APPEND_DEVICE2(#device)
#define APPEND_DEVICE2(device) \
  deviceFamily->Append(NS_LITERAL_STRING_FROM_CSTRING(device))
#define APPEND_RANGE(start, end) deviceFamily->AppendRange(start, end)

const GfxDeviceFamily* GfxDriverInfo::GetDeviceFamily(DeviceFamily id) {
  if (id >= DeviceFamily::Max) {
    MOZ_ASSERT_UNREACHABLE("DeviceFamily id is out of range");
    return nullptr;
  }

  // All of these have no specific device ID filtering.
  switch (id) {
    case DeviceFamily::All:
    case DeviceFamily::IntelAll:
    case DeviceFamily::NvidiaAll:
    case DeviceFamily::AtiAll:
    case DeviceFamily::MicrosoftAll:
    case DeviceFamily::ParallelsAll:
    case DeviceFamily::QualcommAll:
    case DeviceFamily::AppleAll:
    case DeviceFamily::AmazonAll:
      return nullptr;
    default:
      break;
  }

  // If it already exists, we must have processed it once, so return it now.
  auto idx = static_cast<size_t>(id);
  if (sDeviceFamilies[idx]) {
    return sDeviceFamilies[idx];
  }

  sDeviceFamilies[idx] = new GfxDeviceFamily;
  GfxDeviceFamily* deviceFamily = sDeviceFamilies[idx];

  switch (id) {
    case DeviceFamily::IntelGMA500:
      APPEND_DEVICE(0x8108); /* IntelGMA500_1 */
      APPEND_DEVICE(0x8109); /* IntelGMA500_2 */
      break;
    case DeviceFamily::IntelGMA900:
      APPEND_DEVICE(0x2582); /* IntelGMA900_1 */
      APPEND_DEVICE(0x2782); /* IntelGMA900_2 */
      APPEND_DEVICE(0x2592); /* IntelGMA900_3 */
      APPEND_DEVICE(0x2792); /* IntelGMA900_4 */
      break;
    case DeviceFamily::IntelGMA950:
      APPEND_DEVICE(0x2772); /* Intel945G_1 */
      APPEND_DEVICE(0x2776); /* Intel945G_2 */
      APPEND_DEVICE(0x27a2); /* Intel945_1 */
      APPEND_DEVICE(0x27a6); /* Intel945_2 */
      APPEND_DEVICE(0x27ae); /* Intel945_3 */
      break;
    case DeviceFamily::IntelGMA3150:
      APPEND_DEVICE(0xa001); /* IntelGMA3150_Nettop_1 */
      APPEND_DEVICE(0xa002); /* IntelGMA3150_Nettop_2 */
      APPEND_DEVICE(0xa011); /* IntelGMA3150_Netbook_1 */
      APPEND_DEVICE(0xa012); /* IntelGMA3150_Netbook_2 */
      break;
    case DeviceFamily::IntelGMAX3000:
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
    case DeviceFamily::IntelGMAX4500HD:
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
      break;
    case DeviceFamily::IntelHDGraphicsToIvyBridge:
      APPEND_DEVICE(0x015A); /* IntelIvyBridge_GT1_1 (HD Graphics) */
      // clang-format off
      APPEND_DEVICE(0x0152); /* IntelIvyBridge_GT1_2 (HD Graphics 2500, desktop) */
      APPEND_DEVICE(0x0162); /* IntelIvyBridge_GT2_1 (HD Graphics 4000, desktop) */
      APPEND_DEVICE(0x0166); /* IntelIvyBridge_GT2_2 (HD Graphics 4000, mobile) */
      APPEND_DEVICE(0x016A); /* IntelIvyBridge_GT2_3 (HD Graphics P4000, workstation) */
      // clang-format on
      [[fallthrough]];
    case DeviceFamily::IntelHDGraphicsToSandyBridge:
      APPEND_DEVICE(0x0042); /* IntelHDGraphics */
      APPEND_DEVICE(0x0046); /* IntelMobileHDGraphics */
      APPEND_DEVICE(0x0102); /* IntelSandyBridge_1 */
      APPEND_DEVICE(0x0106); /* IntelSandyBridge_2 */
      APPEND_DEVICE(0x0112); /* IntelSandyBridge_3 */
      APPEND_DEVICE(0x0116); /* IntelSandyBridge_4 */
      APPEND_DEVICE(0x0122); /* IntelSandyBridge_5 */
      APPEND_DEVICE(0x0126); /* IntelSandyBridge_6 */
      APPEND_DEVICE(0x010a); /* IntelSandyBridge_7 */
      break;
    case DeviceFamily::IntelHaswell:
      APPEND_DEVICE(0x0402); /* IntelHaswell_GT1_1 */
      APPEND_DEVICE(0x0406); /* IntelHaswell_GT1_2 */
      APPEND_DEVICE(0x040A); /* IntelHaswell_GT1_3 */
      APPEND_DEVICE(0x040B); /* IntelHaswell_GT1_4 */
      APPEND_DEVICE(0x040E); /* IntelHaswell_GT1_5 */
      APPEND_DEVICE(0x0A02); /* IntelHaswell_GT1_6 */
      APPEND_DEVICE(0x0A06); /* IntelHaswell_GT1_7 */
      APPEND_DEVICE(0x0A0A); /* IntelHaswell_GT1_8 */
      APPEND_DEVICE(0x0A0B); /* IntelHaswell_GT1_9 */
      APPEND_DEVICE(0x0A0E); /* IntelHaswell_GT1_10 */
      APPEND_DEVICE(0x0412); /* IntelHaswell_GT2_1 */
      APPEND_DEVICE(0x0416); /* IntelHaswell_GT2_2 */
      APPEND_DEVICE(0x041A); /* IntelHaswell_GT2_3 */
      APPEND_DEVICE(0x041B); /* IntelHaswell_GT2_4 */
      APPEND_DEVICE(0x041E); /* IntelHaswell_GT2_5 */
      APPEND_DEVICE(0x0A12); /* IntelHaswell_GT2_6 */
      APPEND_DEVICE(0x0A16); /* IntelHaswell_GT2_7 */
      APPEND_DEVICE(0x0A1A); /* IntelHaswell_GT2_8 */
      APPEND_DEVICE(0x0A1B); /* IntelHaswell_GT2_9 */
      APPEND_DEVICE(0x0A1E); /* IntelHaswell_GT2_10 */
      APPEND_DEVICE(0x0422); /* IntelHaswell_GT3_1 */
      APPEND_DEVICE(0x0426); /* IntelHaswell_GT3_2 */
      APPEND_DEVICE(0x042A); /* IntelHaswell_GT3_3 */
      APPEND_DEVICE(0x042B); /* IntelHaswell_GT3_4 */
      APPEND_DEVICE(0x042E); /* IntelHaswell_GT3_5 */
      APPEND_DEVICE(0x0A22); /* IntelHaswell_GT3_6 */
      APPEND_DEVICE(0x0A26); /* IntelHaswell_GT3_7 */
      APPEND_DEVICE(0x0A2A); /* IntelHaswell_GT3_8 */
      APPEND_DEVICE(0x0A2B); /* IntelHaswell_GT3_9 */
      APPEND_DEVICE(0x0A2E); /* IntelHaswell_GT3_10 */
      APPEND_DEVICE(0x0D22); /* IntelHaswell_GT3e_1 */
      APPEND_DEVICE(0x0D26); /* IntelHaswell_GT3e_2 */
      APPEND_DEVICE(0x0D2A); /* IntelHaswell_GT3e_3 */
      APPEND_DEVICE(0x0D2B); /* IntelHaswell_GT3e_4 */
      APPEND_DEVICE(0x0D2E); /* IntelHaswell_GT3e_5 */
      break;
    case DeviceFamily::IntelSandyBridge:
      APPEND_DEVICE(0x0102);
      APPEND_DEVICE(0x0106);
      APPEND_DEVICE(0x010a);
      APPEND_DEVICE(0x0112);
      APPEND_DEVICE(0x0116);
      APPEND_DEVICE(0x0122);
      APPEND_DEVICE(0x0126);
      break;
    case DeviceFamily::IntelGen7Baytrail:
      APPEND_DEVICE(0x0f30);
      APPEND_DEVICE(0x0f31);
      APPEND_DEVICE(0x0f33);
      APPEND_DEVICE(0x0155);
      APPEND_DEVICE(0x0157);
      break;
    case DeviceFamily::IntelSkylake:
      APPEND_DEVICE(0x1902);
      APPEND_DEVICE(0x1906);
      APPEND_DEVICE(0x190a);
      APPEND_DEVICE(0x190B);
      APPEND_DEVICE(0x190e);
      APPEND_DEVICE(0x1912);
      APPEND_DEVICE(0x1913);
      APPEND_DEVICE(0x1915);
      APPEND_DEVICE(0x1916);
      APPEND_DEVICE(0x1917);
      APPEND_DEVICE(0x191a);
      APPEND_DEVICE(0x191b);
      APPEND_DEVICE(0x191d);
      APPEND_DEVICE(0x191e);
      APPEND_DEVICE(0x1921);
      APPEND_DEVICE(0x1923);
      APPEND_DEVICE(0x1926);
      APPEND_DEVICE(0x1927);
      APPEND_DEVICE(0x192a);
      APPEND_DEVICE(0x192b);
      APPEND_DEVICE(0x192d);
      APPEND_DEVICE(0x1932);
      APPEND_DEVICE(0x193a);
      APPEND_DEVICE(0x193b);
      APPEND_DEVICE(0x193d);
      break;
    case DeviceFamily::IntelKabyLake:
      APPEND_DEVICE(0x5902);
      APPEND_DEVICE(0x5906);
      APPEND_DEVICE(0x5908);
      APPEND_DEVICE(0x590A);
      APPEND_DEVICE(0x590B);
      APPEND_DEVICE(0x590E);
      APPEND_DEVICE(0x5913);
      APPEND_DEVICE(0x5915);
      APPEND_DEVICE(0x5912);
      APPEND_DEVICE(0x5916);
      APPEND_DEVICE(0x5917);
      APPEND_DEVICE(0x591A);
      APPEND_DEVICE(0x591B);
      APPEND_DEVICE(0x591D);
      APPEND_DEVICE(0x591E);
      APPEND_DEVICE(0x5921);
      APPEND_DEVICE(0x5923);
      APPEND_DEVICE(0x5926);
      APPEND_DEVICE(0x5927);
      APPEND_DEVICE(0x593B);
      APPEND_DEVICE(0x591C);
      APPEND_DEVICE(0x87C0);
      break;
    case DeviceFamily::IntelHD520:
      APPEND_DEVICE(0x1916);
      break;
    case DeviceFamily::IntelMobileHDGraphics:
      APPEND_DEVICE(0x0046); /* IntelMobileHDGraphics */
      break;
    case DeviceFamily::NvidiaBlockD3D9Layers:
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
    case DeviceFamily::RadeonX1000:
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
    case DeviceFamily::RadeonCaicos:
      APPEND_DEVICE(0x6766);
      APPEND_DEVICE(0x6767);
      APPEND_DEVICE(0x6768);
      APPEND_DEVICE(0x6770);
      APPEND_DEVICE(0x6771);
      APPEND_DEVICE(0x6772);
      APPEND_DEVICE(0x6778);
      APPEND_DEVICE(0x6779);
      APPEND_DEVICE(0x677b);
      break;
    case DeviceFamily::RadeonBlockZeroVideoCopy:
      // Stoney
      APPEND_DEVICE(0x98e4);
      // Carrizo
      APPEND_RANGE(0x9870, 0x9877);
      break;
    case DeviceFamily::Geforce7300GT:
      APPEND_DEVICE(0x0393);
      break;
    case DeviceFamily::Nvidia310M:
      APPEND_DEVICE(0x0A70);
      break;
    case DeviceFamily::Nvidia8800GTS:
      APPEND_DEVICE(0x0193);
      break;
    case DeviceFamily::Bug1137716:
      APPEND_DEVICE(0x0a29);
      APPEND_DEVICE(0x0a2b);
      APPEND_DEVICE(0x0a2d);
      APPEND_DEVICE(0x0a35);
      APPEND_DEVICE(0x0a6c);
      APPEND_DEVICE(0x0a70);
      APPEND_DEVICE(0x0a72);
      APPEND_DEVICE(0x0a7a);
      APPEND_DEVICE(0x0caf);
      APPEND_DEVICE(0x0dd2);
      APPEND_DEVICE(0x0dd3);
      // GF180M ids
      APPEND_DEVICE(0x0de3);
      APPEND_DEVICE(0x0de8);
      APPEND_DEVICE(0x0de9);
      APPEND_DEVICE(0x0dea);
      APPEND_DEVICE(0x0deb);
      APPEND_DEVICE(0x0dec);
      APPEND_DEVICE(0x0ded);
      APPEND_DEVICE(0x0dee);
      APPEND_DEVICE(0x0def);
      APPEND_DEVICE(0x0df0);
      APPEND_DEVICE(0x0df1);
      APPEND_DEVICE(0x0df2);
      APPEND_DEVICE(0x0df3);
      APPEND_DEVICE(0x0df4);
      APPEND_DEVICE(0x0df5);
      APPEND_DEVICE(0x0df6);
      APPEND_DEVICE(0x0df7);
      APPEND_DEVICE(0x1050);
      APPEND_DEVICE(0x1051);
      APPEND_DEVICE(0x1052);
      APPEND_DEVICE(0x1054);
      APPEND_DEVICE(0x1055);
      break;
    case DeviceFamily::Bug1116812:
      APPEND_DEVICE(0x2e32);
      APPEND_DEVICE(0x2a02);
      break;
    case DeviceFamily::Bug1155608:
      APPEND_DEVICE(0x2e22); /* IntelG45_1 */
      break;
    case DeviceFamily::Bug1447141:
      APPEND_DEVICE(0x9991);
      APPEND_DEVICE(0x9993);
      APPEND_DEVICE(0x9996);
      APPEND_DEVICE(0x9998);
      APPEND_DEVICE(0x9901);
      APPEND_DEVICE(0x990b);
      break;
    case DeviceFamily::Bug1207665:
      APPEND_DEVICE(0xa001); /* Intel Media Accelerator 3150 */
      APPEND_DEVICE(0xa002);
      APPEND_DEVICE(0xa011);
      APPEND_DEVICE(0xa012);
      break;
    case DeviceFamily::AmdR600:
      // AMD R600 generation GPUs
      // R600
      APPEND_RANGE(0x9400, 0x9403);
      APPEND_DEVICE(0x9405);
      APPEND_RANGE(0x940a, 0x940b);
      APPEND_DEVICE(0x940f);
      // RV610
      APPEND_RANGE(0x94c0, 0x94c1);
      APPEND_RANGE(0x94c3, 0x94c9);
      APPEND_RANGE(0x94cb, 0x94cd);
      // RV630
      APPEND_RANGE(0x9580, 0x9581);
      APPEND_DEVICE(0x9583);
      APPEND_RANGE(0x9586, 0x958f);
      // RV670
      APPEND_RANGE(0x9500, 0x9501);
      APPEND_RANGE(0x9504, 0x9509);
      APPEND_DEVICE(0x950f);
      APPEND_DEVICE(0x9511);
      APPEND_DEVICE(0x9515);
      APPEND_DEVICE(0x9517);
      APPEND_DEVICE(0x9519);
      // RV620
      APPEND_DEVICE(0x95c0);
      APPEND_DEVICE(0x95c2);
      APPEND_RANGE(0x95c4, 0x95c7);
      APPEND_DEVICE(0x95c9);
      APPEND_RANGE(0x95cc, 0x95cf);
      // RV635
      APPEND_RANGE(0x9590, 0x9591);
      APPEND_DEVICE(0x9593);
      APPEND_RANGE(0x9595, 0x9599);
      APPEND_DEVICE(0x959b);
      // RS780
      APPEND_RANGE(0x9610, 0x9616);
      // RS880
      APPEND_RANGE(0x9710, 0x9715);
      break;
    case DeviceFamily::NvidiaWebRenderBlocked:
      APPEND_RANGE(0x0190, 0x019e);  // early tesla
      APPEND_RANGE(0x0500, 0x05df);  // C67-C68
      break;
    case DeviceFamily::IntelWebRenderBlocked:
      // powervr
      // sgx535
      APPEND_DEVICE(0x2e5b);
      APPEND_DEVICE(0x8108);
      APPEND_DEVICE(0x8109);
      APPEND_DEVICE(0x4102);
      // sgx545
      APPEND_DEVICE(0x0be0);
      APPEND_DEVICE(0x0be1);
      APPEND_DEVICE(0x0be3);
      APPEND_RANGE(0x08c7, 0x08cf);

      // gen4
      APPEND_DEVICE(0x2972);
      APPEND_DEVICE(0x2973);
      APPEND_DEVICE(0x2992);
      APPEND_DEVICE(0x2993);
      APPEND_DEVICE(0x29a2);
      APPEND_DEVICE(0x29a3);

      APPEND_DEVICE(0x2982);
      APPEND_DEVICE(0x2983);

      APPEND_DEVICE(0x2a02);
      APPEND_DEVICE(0x2a03);
      APPEND_DEVICE(0x2a12);
      APPEND_DEVICE(0x2a13);

      // gen4.5
      APPEND_DEVICE(0x2e02);
      APPEND_DEVICE(0x2e42); /* IntelB43_1 */
      APPEND_DEVICE(0x2e43); /* IntelB43_2 */
      APPEND_DEVICE(0x2e92); /* IntelB43_3 */
      APPEND_DEVICE(0x2e93); /* IntelB43_4 */
      APPEND_DEVICE(0x2e12); /* IntelQ45_1 */
      APPEND_DEVICE(0x2e13); /* IntelQ45_2 */
      APPEND_DEVICE(0x2e32); /* IntelG41_1 */
      APPEND_DEVICE(0x2e33); /* IntelG41_2 */
      APPEND_DEVICE(0x2e22); /* IntelG45_1 */

      APPEND_DEVICE(0x2e23); /* IntelG45_2 */
      APPEND_DEVICE(0x2a42); /* IntelGMA4500MHD_1 */
      APPEND_DEVICE(0x2a43); /* IntelGMA4500MHD_2 */

      // gen5 (ironlake)
      APPEND_DEVICE(0x0042);
      APPEND_DEVICE(0x0046);
      break;
    // This should never happen, but we get a warning if we don't handle this.
    case DeviceFamily::Max:
    case DeviceFamily::All:
    case DeviceFamily::IntelAll:
    case DeviceFamily::NvidiaAll:
    case DeviceFamily::AtiAll:
    case DeviceFamily::MicrosoftAll:
    case DeviceFamily::ParallelsAll:
    case DeviceFamily::QualcommAll:
    case DeviceFamily::AppleAll:
    case DeviceFamily::AmazonAll:
      NS_WARNING("Invalid DeviceFamily id");
      break;
  }

  return deviceFamily;
}

// Macro for assigning a window protocol id to a string.
#define DECLARE_WINDOW_PROTOCOL_ID(name, windowProtocolId) \
  case WindowProtocol::name:                               \
    sWindowProtocol[idx]->AssignLiteral(windowProtocolId); \
    break;

const nsAString& GfxDriverInfo::GetWindowProtocol(WindowProtocol id) {
  if (id >= WindowProtocol::Max) {
    MOZ_ASSERT_UNREACHABLE("WindowProtocol id is out of range");
    id = WindowProtocol::All;
  }

  auto idx = static_cast<size_t>(id);
  if (sWindowProtocol[idx]) {
    return *sWindowProtocol[idx];
  }

  sWindowProtocol[idx] = new nsString();

  switch (id) {
    DECLARE_WINDOW_PROTOCOL_ID(X11, "x11");
    DECLARE_WINDOW_PROTOCOL_ID(XWayland, "xwayland");
    DECLARE_WINDOW_PROTOCOL_ID(Wayland, "wayland");
    DECLARE_WINDOW_PROTOCOL_ID(WaylandDRM, "wayland/drm");
    DECLARE_WINDOW_PROTOCOL_ID(WaylandAll, "wayland/all");
    DECLARE_WINDOW_PROTOCOL_ID(X11All, "x11/all");
    case WindowProtocol::Max:  // Suppress a warning.
      DECLARE_WINDOW_PROTOCOL_ID(All, "");
  }

  return *sWindowProtocol[idx];
}

// Macro for assigning a device vendor id to a string.
#define DECLARE_VENDOR_ID(name, deviceId)         \
  case DeviceVendor::name:                        \
    sDeviceVendors[idx]->AssignLiteral(deviceId); \
    break;

const nsAString& GfxDriverInfo::GetDeviceVendor(DeviceFamily id) {
  if (id >= DeviceFamily::Max) {
    MOZ_ASSERT_UNREACHABLE("DeviceVendor id is out of range");
    id = DeviceFamily::All;
  }

  DeviceVendor vendor = DeviceVendor::All;
  switch (id) {
    case DeviceFamily::IntelAll:
    case DeviceFamily::IntelGMA500:
    case DeviceFamily::IntelGMA900:
    case DeviceFamily::IntelGMA950:
    case DeviceFamily::IntelGMA3150:
    case DeviceFamily::IntelGMAX3000:
    case DeviceFamily::IntelGMAX4500HD:
    case DeviceFamily::IntelHDGraphicsToIvyBridge:
    case DeviceFamily::IntelHDGraphicsToSandyBridge:
    case DeviceFamily::IntelHaswell:
    case DeviceFamily::IntelSandyBridge:
    case DeviceFamily::IntelGen7Baytrail:
    case DeviceFamily::IntelSkylake:
    case DeviceFamily::IntelKabyLake:
    case DeviceFamily::IntelHD520:
    case DeviceFamily::IntelMobileHDGraphics:
    case DeviceFamily::IntelWebRenderBlocked:
    case DeviceFamily::Bug1116812:
    case DeviceFamily::Bug1155608:
    case DeviceFamily::Bug1207665:
      vendor = DeviceVendor::Intel;
      break;
    case DeviceFamily::NvidiaAll:
    case DeviceFamily::NvidiaBlockD3D9Layers:
    case DeviceFamily::NvidiaWebRenderBlocked:
    case DeviceFamily::Geforce7300GT:
    case DeviceFamily::Nvidia310M:
    case DeviceFamily::Nvidia8800GTS:
    case DeviceFamily::Bug1137716:
      vendor = DeviceVendor::NVIDIA;
      break;
    case DeviceFamily::AtiAll:
    case DeviceFamily::RadeonBlockZeroVideoCopy:
    case DeviceFamily::RadeonCaicos:
    case DeviceFamily::RadeonX1000:
    case DeviceFamily::Bug1447141:
    case DeviceFamily::AmdR600:
      vendor = DeviceVendor::ATI;
      break;
    case DeviceFamily::MicrosoftAll:
      vendor = DeviceVendor::Microsoft;
      break;
    case DeviceFamily::ParallelsAll:
      vendor = DeviceVendor::Parallels;
      break;
    case DeviceFamily::AppleAll:
      vendor = DeviceVendor::Apple;
      break;
    case DeviceFamily::AmazonAll:
      vendor = DeviceVendor::Amazon;
      break;
    case DeviceFamily::QualcommAll:
      // Choose an arbitrary Qualcomm PCI VENdor ID for now.
      // TODO: This should be "QCOM" when Windows device ID parsing is reworked.
      vendor = DeviceVendor::Qualcomm;
      break;
    case DeviceFamily::All:
    case DeviceFamily::Max:
      break;
  }

  return GetDeviceVendor(vendor);
}

const nsAString& GfxDriverInfo::GetDeviceVendor(DeviceVendor id) {
  if (id >= DeviceVendor::Max) {
    MOZ_ASSERT_UNREACHABLE("DeviceVendor id is out of range");
    id = DeviceVendor::All;
  }

  auto idx = static_cast<size_t>(id);
  if (sDeviceVendors[idx]) {
    return *sDeviceVendors[idx];
  }

  sDeviceVendors[idx] = new nsString();

  switch (id) {
    DECLARE_VENDOR_ID(Intel, "0x8086");
    DECLARE_VENDOR_ID(NVIDIA, "0x10de");
    DECLARE_VENDOR_ID(ATI, "0x1002");
    // AMD has 0x1022 but continues to release GPU hardware under ATI.
    DECLARE_VENDOR_ID(Microsoft, "0x1414");
    DECLARE_VENDOR_ID(MicrosoftBasic, "0x00ba");
    DECLARE_VENDOR_ID(MicrosoftHyperV, "0x000b");
    DECLARE_VENDOR_ID(Parallels, "0x1ab8");
    DECLARE_VENDOR_ID(VMWare, "0x15ad");
    DECLARE_VENDOR_ID(VirtualBox, "0x80ee");
    DECLARE_VENDOR_ID(Apple, "0x106b");
    DECLARE_VENDOR_ID(Amazon, "0x1d0f");
    // Choose an arbitrary Qualcomm PCI VENdor ID for now.
    // TODO: This should be "QCOM" when Windows device ID parsing is reworked.
    DECLARE_VENDOR_ID(Qualcomm, "0x5143");
    case DeviceVendor::Max:  // Suppress a warning.
      DECLARE_VENDOR_ID(All, "");
  }

  return *sDeviceVendors[idx];
}

// Macro for assigning a driver vendor id to a string.
#define DECLARE_DRIVER_VENDOR_ID(name, driverVendorId)  \
  case DriverVendor::name:                              \
    sDriverVendors[idx]->AssignLiteral(driverVendorId); \
    break;

const nsAString& GfxDriverInfo::GetDriverVendor(DriverVendor id) {
  if (id >= DriverVendor::Max) {
    MOZ_ASSERT_UNREACHABLE("DriverVendor id is out of range");
    id = DriverVendor::All;
  }

  auto idx = static_cast<size_t>(id);
  if (sDriverVendors[idx]) {
    return *sDriverVendors[idx];
  }

  sDriverVendors[idx] = new nsString();

  switch (id) {
    DECLARE_DRIVER_VENDOR_ID(MesaAll, "mesa/all");
    DECLARE_DRIVER_VENDOR_ID(MesaLLVMPipe, "mesa/llvmpipe");
    DECLARE_DRIVER_VENDOR_ID(MesaSoftPipe, "mesa/softpipe");
    DECLARE_DRIVER_VENDOR_ID(MesaSWRast, "mesa/swrast");
    DECLARE_DRIVER_VENDOR_ID(MesaSWUnknown, "mesa/software-unknown");
    DECLARE_DRIVER_VENDOR_ID(MesaUnknown, "mesa/unknown");
    DECLARE_DRIVER_VENDOR_ID(MesaR600, "mesa/r600");
    DECLARE_DRIVER_VENDOR_ID(MesaNouveau, "mesa/nouveau");
    DECLARE_DRIVER_VENDOR_ID(NonMesaAll, "non-mesa/all");
    DECLARE_DRIVER_VENDOR_ID(HardwareMesaAll, "mesa/hw-all");
    DECLARE_DRIVER_VENDOR_ID(SoftwareMesaAll, "mesa/sw-all");
    DECLARE_DRIVER_VENDOR_ID(MesaNonIntelNvidiaAtiAll,
                             "mesa/non-intel-nvidia-ati-all");
    DECLARE_DRIVER_VENDOR_ID(MesaVM, "mesa/vmwgfx");
    case DriverVendor::Max:  // Suppress a warning.
      DECLARE_DRIVER_VENDOR_ID(All, "");
  }

  return *sDriverVendors[idx];
}
