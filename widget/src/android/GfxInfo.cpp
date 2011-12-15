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

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"
#include "prenv.h"
#include "prprf.h"
#include "EGLUtils.h"
#include "nsHashKeys.h"

#include "AndroidBridge.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

using namespace mozilla::widget;

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after gfxPlatform initialization
 * has occurred because they depend on it for information. (See bug 591561) */
nsresult
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

nsresult
GfxInfo::GetDWriteEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

nsresult
GfxInfo::GetAzureEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString cleartypeParameters; */
NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString & aCleartypeParams)
{
  return NS_ERROR_FAILURE;
}

nsresult
GfxInfo::Init()
{
  mSetCrashReportAnnotations = false;

  mAdapterDescription.AssignASCII(mozilla::gl::GetVendor());
  if (mozilla::AndroidBridge::Bridge()) {
    nsAutoString str;

    mAdapterDescription.Append(NS_LITERAL_STRING(", Model: '"));
    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", str)) {
      mAdapterDeviceID = str;
      mAdapterDescription.Append(str);
    }

    mAdapterDescription.Append(NS_LITERAL_STRING("', Product: '"));
    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "PRODUCT", str))
      mAdapterDescription.Append(str);

    mAdapterDescription.Append(NS_LITERAL_STRING("', Manufacturer: '"));
    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MANUFACTURER", str))
      mAdapterDescription.Append(str);

    mAdapterDescription.Append(NS_LITERAL_STRING("', Hardware: '"));
    PRInt32 version; // the HARDWARE field isn't available on Android SDK < 8
    if (!mozilla::AndroidBridge::Bridge()->GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &version))
      version = 0;

    if (version >= 8 && mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
        mAdapterVendorID = str;
        mAdapterDescription.Append(str);
      }
    }

    mAdapterDescription.Append(NS_LITERAL_STRING("'"));
    mAndroidSDKVersion = version;
  }

  return GfxInfoBase::Init();
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mAdapterDescription;
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion.Truncate(0);
  aAdapterDriverVersion.AppendInt(mAndroidSDKVersion);
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  aAdapterVendorID = mAdapterVendorID;
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  aAdapterDeviceID = mAdapterDeviceID;
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  return NS_ERROR_FAILURE;
}

void
GfxInfo::AddOpenGLCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
  nsAutoString adapterDescriptionString, deviceID, vendorID;
  nsCAutoString narrowDeviceID, narrowVendorID;

  GetAdapterDeviceID(deviceID);
  GetAdapterVendorID(vendorID);
  GetAdapterDescription(adapterDescriptionString);

  narrowDeviceID = NS_ConvertUTF16toUTF8(deviceID);
  narrowVendorID = NS_ConvertUTF16toUTF8(vendorID);

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
                                     narrowVendorID);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
                                     narrowDeviceID);

  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsCAutoString note;
  /* AppendPrintf only supports 32 character strings, mrghh. */
  note.Append("AdapterVendorID: ");
  note.Append(narrowVendorID);
  note.Append(", AdapterDeviceID: ");
  note.Append(narrowDeviceID);
  note.Append(".\n");
  note.AppendPrintf("AdapterDescription: '%s'.", NS_ConvertUTF16toUTF8(adapterDescriptionString).get());
  note.Append("\n");

  CrashReporter::AppendAppNotesToCrashReport(note);
#endif
}

const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  // Nothing here yet.
  //if (!mDriverInfo->Length()) {
  //
  //}
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, 
                              PRInt32 *aStatus, 
                              nsAString & aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo, 
                              OperatingSystem* aOS /* = nsnull */)
{
  PRInt32 status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  aSuggestedDriverVersion.SetIsVoid(true);

  OperatingSystem os = DRIVER_OS_ANDROID;

  if (aFeature == FEATURE_OPENGL_LAYERS) {
    if (!mSetCrashReportAnnotations) {
      AddOpenGLCrashReportAnnotations();
      mSetCrashReportAnnotations = true;
    }

    /* XXX: Use this code when we're ready to whitelist devices. */
    // nsAutoString str;
    // /* Whitelist Galaxy S phones */
    // if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
    //   if (str != NS_LITERAL_STRING("smdkc110")) {
    //     status = FEATURE_BLOCKED_DEVICE;
    //   }
    // }

    status = FEATURE_BLOCKED_DEVICE;
  }

  *aStatus = status;
  if (aOS)
    *aOS = os;

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, &os);
}
