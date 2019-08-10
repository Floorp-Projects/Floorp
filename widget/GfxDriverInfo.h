/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxDriverInfo_h__
#define __mozilla_widget_GfxDriverInfo_h__

#include "nsString.h"

// Macros for adding a blocklist item to the static list.
#define APPEND_TO_DRIVER_BLOCKLIST(os, vendor, driverVendor, devices, feature, \
                                   featureStatus, driverComparator,            \
                                   driverVersion, ruleId, suggestedVersion)    \
  sDriverInfo->AppendElement(GfxDriverInfo(                                    \
      os, vendor, driverVendor, devices, feature, featureStatus,               \
      driverComparator, driverVersion, ruleId, suggestedVersion))
#define APPEND_TO_DRIVER_BLOCKLIST2(os, vendor, driverVendor, devices,         \
                                    feature, featureStatus, driverComparator,  \
                                    driverVersion, ruleId)                     \
  sDriverInfo->AppendElement(                                                  \
      GfxDriverInfo(os, vendor, driverVendor, devices, feature, featureStatus, \
                    driverComparator, driverVersion, ruleId))

#define APPEND_TO_DRIVER_BLOCKLIST_RANGE(                                      \
    os, vendor, driverVendor, devices, feature, featureStatus,                 \
    driverComparator, driverVersion, driverVersionMax, ruleId,                 \
    suggestedVersion)                                                          \
  do {                                                                         \
    MOZ_ASSERT(driverComparator == DRIVER_BETWEEN_EXCLUSIVE ||                 \
               driverComparator == DRIVER_BETWEEN_INCLUSIVE ||                 \
               driverComparator == DRIVER_BETWEEN_INCLUSIVE_START);            \
    GfxDriverInfo info(os, vendor, driverVendor, devices, feature,             \
                       featureStatus, driverComparator, driverVersion, ruleId, \
                       suggestedVersion);                                      \
    info.mDriverVersionMax = driverVersionMax;                                 \
    sDriverInfo->AppendElement(info);                                          \
  } while (false)

#define APPEND_TO_DRIVER_BLOCKLIST_RANGE_GPU2(                                 \
    os, vendor, driverVendor, devices, feature, featureStatus,                 \
    driverComparator, driverVersion, driverVersionMax, ruleId,                 \
    suggestedVersion)                                                          \
  do {                                                                         \
    MOZ_ASSERT(driverComparator == DRIVER_BETWEEN_EXCLUSIVE ||                 \
               driverComparator == DRIVER_BETWEEN_INCLUSIVE ||                 \
               driverComparator == DRIVER_BETWEEN_INCLUSIVE_START);            \
    GfxDriverInfo info(os, vendor, driverVendor, devices, feature,             \
                       featureStatus, driverComparator, driverVersion, ruleId, \
                       suggestedVersion, false, true);                         \
    info.mDriverVersionMax = driverVersionMax;                                 \
    sDriverInfo->AppendElement(info);                                          \
  } while (false)

namespace mozilla {
namespace widget {

enum class OperatingSystem {
  Unknown,
  Windows,
  WindowsXP,
  WindowsServer2003,
  WindowsVista,
  Windows7,
  Windows8,
  Windows8_1,
  Windows10,
  Linux,
  OSX,
  OSX10_5,
  OSX10_6,
  OSX10_7,
  OSX10_8,
  OSX10_9,
  OSX10_10,
  OSX10_11,
  OSX10_12,
  OSX10_13,
  Android,
  Ios
};

enum VersionComparisonOp {
  DRIVER_LESS_THAN,                    // driver <  version
  DRIVER_BUILD_ID_LESS_THAN,           // driver build id <  version
  DRIVER_LESS_THAN_OR_EQUAL,           // driver <= version
  DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL,  // driver build id <= version
  DRIVER_GREATER_THAN,                 // driver >  version
  DRIVER_GREATER_THAN_OR_EQUAL,        // driver >= version
  DRIVER_EQUAL,                        // driver == version
  DRIVER_NOT_EQUAL,                    // driver != version
  DRIVER_BETWEEN_EXCLUSIVE,        // driver > version && driver < versionMax
  DRIVER_BETWEEN_INCLUSIVE,        // driver >= version && driver <= versionMax
  DRIVER_BETWEEN_INCLUSIVE_START,  // driver >= version && driver < versionMax
  DRIVER_COMPARISON_IGNORED
};

enum DeviceFamily {
  IntelGMA500,
  IntelGMA900,
  IntelGMA950,
  IntelGMA3150,
  IntelGMAX3000,
  IntelGMAX4500HD,
  IntelHDGraphicsToSandyBridge,
  IntelHDGraphicsToHaswell,
  IntelHD3000,
  IntelHDGraphicsIvyBridge,
  IntelMobileHDGraphics,
  NvidiaBlockD3D9Layers,
  RadeonX1000,
  Geforce7300GT,
  Nvidia310M,
  Nvidia8800GTS,
  Bug1137716,
  Bug1116812,
  Bug1155608,
  Bug1207665,
  Bug1447141,
  NvidiaBlockWebRender,
  DeviceFamilyMax
};

enum DeviceVendor {
  VendorAll,  // There is an assumption that this is the first enum
  VendorIntel,
  VendorNVIDIA,
  VendorAMD,
  VendorATI,
  VendorMicrosoft,
  VendorParallels,
  VendorQualcomm,

  DeviceVendorMax
};

enum DriverVendor {
  DriverVendorAll,  // There is an assumption that this is the first enum
  // Wildcard for all Mesa drivers.
  DriverMesaAll,
  // Note that the following list of Mesa drivers is not comprehensive; we pull
  // the DRI driver at runtime. These drivers are provided for convenience when
  // populating the local blocklist.
  DriverMesaLLVMPipe,
  DriverMesaSoftPipe,
  DriverMesaSWRast,
  // A generic ID to be provided when we can't determine the DRI driver on Mesa.
  DriverMesaUnknown,
  // Wildcard for all non-Mesa drivers.
  DriverNonMesaAll,

  DriverVendorMax
};

/* Array of devices to match, or an empty array for all devices */
typedef nsTArray<nsString> GfxDeviceFamily;

struct GfxDriverInfo {
  // If |ownDevices| is true, you are transferring ownership of the devices
  // array, and it will be deleted when this GfxDriverInfo is destroyed.
  GfxDriverInfo(OperatingSystem os, const nsAString& vendor,
                const nsAString& driverVendor, GfxDeviceFamily* devices,
                int32_t feature, int32_t featureStatus, VersionComparisonOp op,
                uint64_t driverVersion, const char* ruleId,
                const char* suggestedVersion = nullptr, bool ownDevices = false,
                bool gpu2 = false);

  GfxDriverInfo();
  GfxDriverInfo(const GfxDriverInfo&);
  ~GfxDriverInfo();

  OperatingSystem mOperatingSystem;
  uint32_t mOperatingSystemVersion;

  nsString mAdapterVendor;
  nsString mDriverVendor;

  static GfxDeviceFamily* const allDevices;
  GfxDeviceFamily* mDevices;

  // Whether the mDevices array should be deleted when this structure is
  // deallocated. False by default.
  bool mDeleteDevices;

  /* A feature from nsIGfxInfo, or all features */
  int32_t mFeature;
  static int32_t allFeatures;

  /* A feature status from nsIGfxInfo */
  int32_t mFeatureStatus;

  VersionComparisonOp mComparisonOp;

  /* versions are assumed to be A.B.C.D packed as 0xAAAABBBBCCCCDDDD */
  uint64_t mDriverVersion;
  uint64_t mDriverVersionMax;
  static uint64_t allDriverVersions;

  const char* mSuggestedVersion;
  nsCString mRuleId;

  static const GfxDeviceFamily* GetDeviceFamily(DeviceFamily id);
  static GfxDeviceFamily* sDeviceFamilies[DeviceFamilyMax];

  static const nsAString& GetDeviceVendor(DeviceVendor id);
  static nsAString* sDeviceVendors[DeviceVendorMax];

  static const nsAString& GetDriverVendor(DriverVendor id);
  static nsAString* sDriverVendors[DriverVendorMax];

  nsString mModel, mHardware, mProduct, mManufacturer;

  bool mGpu2;
};

#define GFX_DRIVER_VERSION(a, b, c, d)                               \
  ((uint64_t(a) << 48) | (uint64_t(b) << 32) | (uint64_t(c) << 16) | \
   uint64_t(d))

inline uint64_t V(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  // We make sure every driver number is padded by 0s, this will allow us the
  // easiest 'compare as if decimals' approach. See ParseDriverVersion for a
  // more extensive explanation of this approach.
  while (b > 0 && b < 1000) {
    b *= 10;
  }
  while (c > 0 && c < 1000) {
    c *= 10;
  }
  while (d > 0 && d < 1000) {
    d *= 10;
  }
  return GFX_DRIVER_VERSION(a, b, c, d);
}

// All destination string storage needs to have at least 5 bytes available.
inline bool SplitDriverVersion(const char* aSource, char* aAStr, char* aBStr,
                               char* aCStr, char* aDStr) {
  // sscanf doesn't do what we want here to we parse this manually.
  int len = strlen(aSource);

  // This "4" is hardcoded in a few places, including once as a 3.
  char* dest[4] = {aAStr, aBStr, aCStr, aDStr};
  unsigned destIdx = 0;
  unsigned destPos = 0;

  for (int i = 0; i < len; i++) {
    if (destIdx >= 4) {
      // Invalid format found. Ensure we don't access dest beyond bounds.
      return false;
    }

    if (aSource[i] == '.') {
      MOZ_ASSERT(destIdx < 4 && destPos <= 4);
      dest[destIdx++][destPos] = 0;
      destPos = 0;
      continue;
    }

    if (destPos > 3) {
      // Ignore more than 4 chars. Ensure we never access dest[destIdx]
      // beyond its bounds.
      continue;
    }

    MOZ_ASSERT(destIdx < 4 && destPos < 4);
    dest[destIdx][destPos++] = aSource[i];
  }

  // Take care of the trailing period
  if (destIdx >= 4) {
    return false;
  }

  // Add last terminator.
  MOZ_ASSERT(destIdx < 4 && destPos <= 4);
  dest[destIdx][destPos] = 0;

  if (destIdx != 3) {
    return false;
  }
  return true;
}

// This allows us to pad driver version 'substrings' with 0s, this
// effectively allows us to treat the version numbers as 'decimals'. This is
// a little strange but this method seems to do the right thing for all
// different vendor's driver strings. i.e. .98 will become 9800, which is
// larger than .978 which would become 9780.
inline void PadDriverDecimal(char* aString) {
  for (int i = 0; i < 4; i++) {
    if (!aString[i]) {
      for (int c = i; c < 4; c++) {
        aString[c] = '0';
      }
      break;
    }
  }
  aString[4] = 0;
}

inline bool ParseDriverVersion(const nsAString& aVersion,
                               uint64_t* aNumericVersion) {
  *aNumericVersion = 0;

#if defined(XP_WIN) || defined(MOZ_X11)
  int a, b, c, d;
  char aStr[8], bStr[8], cStr[8], dStr[8];
  /* honestly, why do I even bother */
  if (!SplitDriverVersion(NS_LossyConvertUTF16toASCII(aVersion).get(), aStr,
                          bStr, cStr, dStr))
    return false;

  PadDriverDecimal(bStr);
  PadDriverDecimal(cStr);
  PadDriverDecimal(dStr);

  a = atoi(aStr);
  b = atoi(bStr);
  c = atoi(cStr);
  d = atoi(dStr);

  if (a < 0 || a > 0xffff) return false;
  if (b < 0 || b > 0xffff) return false;
  if (c < 0 || c > 0xffff) return false;
  if (d < 0 || d > 0xffff) return false;

  *aNumericVersion = GFX_DRIVER_VERSION(a, b, c, d);
  MOZ_ASSERT(*aNumericVersion != GfxDriverInfo::allDriverVersions);
  return true;
#elif defined(ANDROID)
  // Can't use aVersion.ToInteger() because that's not compiled into our code
  // unless we have XPCOM_GLUE_AVOID_NSPR disabled.
  *aNumericVersion = atoi(NS_LossyConvertUTF16toASCII(aVersion).get());
  MOZ_ASSERT(*aNumericVersion != GfxDriverInfo::allDriverVersions);
  return true;
#else
  return false;
#endif
}

}  // namespace widget
}  // namespace mozilla

#endif /*__mozilla_widget_GfxDriverInfo_h__ */
