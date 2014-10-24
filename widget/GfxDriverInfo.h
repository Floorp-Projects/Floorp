/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxDriverInfo_h__
#define __mozilla_widget_GfxDriverInfo_h__

#include "mozilla/ArrayUtils.h" // ArrayLength
#include "nsString.h"

// Macros for adding a blocklist item to the static list.
#define APPEND_TO_DRIVER_BLOCKLIST(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion, suggestedVersion) \
    mDriverInfo->AppendElement(GfxDriverInfo(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion, suggestedVersion))
#define APPEND_TO_DRIVER_BLOCKLIST2(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion) \
    mDriverInfo->AppendElement(GfxDriverInfo(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion))

#define APPEND_TO_DRIVER_BLOCKLIST_RANGE(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion, driverVersionMax, suggestedVersion) \
    do { \
      MOZ_ASSERT(driverComparator == DRIVER_BETWEEN_EXCLUSIVE || \
                 driverComparator == DRIVER_BETWEEN_INCLUSIVE || \
                 driverComparator == DRIVER_BETWEEN_INCLUSIVE_START); \
      GfxDriverInfo info(os, vendor, devices, feature, featureStatus, driverComparator, driverVersion, suggestedVersion); \
      info.mDriverVersionMax = driverVersionMax; \
      mDriverInfo->AppendElement(info); \
    } while (false)

namespace mozilla {
namespace widget {

enum OperatingSystem {
  DRIVER_OS_UNKNOWN = 0,
  DRIVER_OS_WINDOWS_XP,
  DRIVER_OS_WINDOWS_SERVER_2003,
  DRIVER_OS_WINDOWS_VISTA,
  DRIVER_OS_WINDOWS_7,
  DRIVER_OS_WINDOWS_8,
  DRIVER_OS_WINDOWS_8_1,
  DRIVER_OS_LINUX,
  DRIVER_OS_OS_X_10_5,
  DRIVER_OS_OS_X_10_6,
  DRIVER_OS_OS_X_10_7,
  DRIVER_OS_OS_X_10_8,
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
  DRIVER_COMPARISON_IGNORED
};

enum DeviceFamily {
  IntelGMA500,
  IntelGMA900,
  IntelGMA950,
  IntelGMA3150,
  IntelGMAX3000,
  IntelGMAX4500HD,
  IntelHD3000,
  IntelMobileHDGraphics,
  NvidiaBlockD3D9Layers,
  RadeonX1000,
  Geforce7300GT,
  Nvidia310M,
  DeviceFamilyMax
};

enum DeviceVendor {
  VendorAll,
  VendorIntel,
  VendorNVIDIA,
  VendorAMD,
  VendorATI,
  VendorMicrosoft,
  DeviceVendorMax
};

/* Array of devices to match, or an empty array for all devices */
typedef nsTArray<nsString> GfxDeviceFamily;

struct GfxDriverInfo
{
  // If |ownDevices| is true, you are transferring ownership of the devices
  // array, and it will be deleted when this GfxDriverInfo is destroyed.
  GfxDriverInfo(OperatingSystem os, nsAString& vendor, GfxDeviceFamily* devices,
                int32_t feature, int32_t featureStatus, VersionComparisonOp op,
                uint64_t driverVersion, const char *suggestedVersion = nullptr,
                bool ownDevices = false);

  GfxDriverInfo();
  GfxDriverInfo(const GfxDriverInfo&);
  ~GfxDriverInfo();

  OperatingSystem mOperatingSystem;
  uint32_t mOperatingSystemVersion;

  nsString mAdapterVendor;

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

  const char *mSuggestedVersion;

  static const GfxDeviceFamily* GetDeviceFamily(DeviceFamily id);
  static GfxDeviceFamily* mDeviceFamilies[DeviceFamilyMax];

  static const nsAString& GetDeviceVendor(DeviceVendor id);
  static nsAString* mDeviceVendors[DeviceVendorMax];

  nsString mModel, mHardware, mProduct, mManufacturer;
};

#define GFX_DRIVER_VERSION(a,b,c,d) \
  ((uint64_t(a)<<48) | (uint64_t(b)<<32) | (uint64_t(c)<<16) | uint64_t(d))

inline uint64_t
V(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
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
inline bool SplitDriverVersion(const char *aSource, char *aAStr, char *aBStr, char *aCStr, char *aDStr)
{
  // sscanf doesn't do what we want here to we parse this manually.
  int len = strlen(aSource);
  char *dest[4] = { aAStr, aBStr, aCStr, aDStr };
  unsigned destIdx = 0;
  unsigned destPos = 0;

  for (int i = 0; i < len; i++) {
    if (destIdx > ArrayLength(dest)) {
      // Invalid format found. Ensure we don't access dest beyond bounds.
      return false;
    }

    if (aSource[i] == '.') {
      dest[destIdx++][destPos] = 0;
      destPos = 0;
      continue;
    }

    if (destPos > 3) {
      // Ignore more than 4 chars. Ensure we never access dest[destIdx]
      // beyond its bounds.
      continue;
    }

    dest[destIdx][destPos++] = aSource[i];
  }

  // Add last terminator.
  dest[destIdx][destPos] = 0;

  if (destIdx != ArrayLength(dest) - 1) {
    return false;
  }
  return true;
}

// This allows us to pad driver version 'substrings' with 0s, this
// effectively allows us to treat the version numbers as 'decimals'. This is
// a little strange but this method seems to do the right thing for all
// different vendor's driver strings. i.e. .98 will become 9800, which is
// larger than .978 which would become 9780.
inline void PadDriverDecimal(char *aString)
{
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

inline bool
ParseDriverVersion(const nsAString& aVersion, uint64_t *aNumericVersion)
{
  *aNumericVersion = 0;

#if defined(XP_WIN)
  int a, b, c, d;
  char aStr[8], bStr[8], cStr[8], dStr[8];
  /* honestly, why do I even bother */
  if (!SplitDriverVersion(NS_LossyConvertUTF16toASCII(aVersion).get(), aStr, bStr, cStr, dStr))
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
  return true;
#elif defined(ANDROID)
  // Can't use aVersion.ToInteger() because that's not compiled into our code
  // unless we have XPCOM_GLUE_AVOID_NSPR disabled.
  *aNumericVersion = atoi(NS_LossyConvertUTF16toASCII(aVersion).get());
  return true;
#else
  return false;
#endif
}

}
}

#endif /*__mozilla_widget_GfxDriverInfo_h__ */
