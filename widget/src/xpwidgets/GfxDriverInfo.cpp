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
