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
#include "nsPrintfCString.h"

#ifndef __mozilla_widget_GfxDriverInfo_h__
#define __mozilla_widget_GfxDriverInfo_h__

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

/* A zero-terminated array of devices to match, or all devices */
typedef PRUint32 *GfxDeviceFamily;

struct GfxDriverInfo
{
  // If |ownDevices| is true, you are transferring ownership of the devices
  // array, and it will be deleted when this GfxDriverInfo is destroyed.
  GfxDriverInfo(OperatingSystem os, PRUint32 vendor, GfxDeviceFamily devices,
                PRInt32 feature, PRInt32 featureStatus, VersionComparisonOp op,
                PRUint64 driverVersion, bool ownDevices = false);

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
};

#define GFX_DRIVER_VERSION(a,b,c,d) \
  ((PRUint64(a)<<48) | (PRUint64(b)<<32) | (PRUint64(c)<<16) | PRUint64(d))

inline bool
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

  *aNumericVersion = GFX_DRIVER_VERSION(a, b, c, d);
  return true;
}

};
};

#endif /*__mozilla_widget_GfxDriverInfo_h__ */
