/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"
#include "prenv.h"
#include "prprf.h"
#include "nsHashKeys.h"

#include "AndroidBridge.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED1(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

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
  mAdapterDescription.AssignLiteral(""); // we may append to it below
  if (mozilla::AndroidBridge::Bridge()) {
    nsAutoString str;

    mAdapterDescription.Append(NS_LITERAL_STRING("Model: '"));
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

  AddOpenGLCrashReportAnnotations();

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
  if (!mDriverInfo->Length()) {
    /* The following entry, when uncommented, will allow us to whitelist a
     * specific device. See the long comment in GetFeatureStatusImpl for more
     * info. */
 // APPEND_TO_DRIVER_BLOCKLIST( DRIVER_OS_ALL,
 //   my_vendor_id, my_device_id,
 //   nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_NO_INFO,
 //   DRIVER_LESS_THAN, GfxDriverInfo::allDevices );
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAll), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_LESS_THAN, GfxDriverInfo::allDriverVersions );
  }
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, 
                              PRInt32 *aStatus, 
                              nsAString & aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo, 
                              OperatingSystem* aOS /* = nsnull */)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  OperatingSystem os = DRIVER_OS_ANDROID;
  if (aOS)
    *aOS = os;

  // Don't evaluate special cases when evaluating the downlaoded blocklist.
  if (!aDriverInfo.Length()) {
    if (aFeature == FEATURE_OPENGL_LAYERS) {
      /* The following code is an old way to whitelist devices when we're ready.
       * It is staying here for reference. The best way to do this now is to add
       * an entry in the list above. There is a dummy entry which will whitelist a
       * device when uncommented and device/vendor IDs are inserted. It is
       * preferred that we stop whitelisting and instead go to blocklisting, where
       * everything is assumed to be okay as long as it's not in the blocklist. */
      // nsAutoString str;
      // /* Whitelist Galaxy S phones */
      // if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
      //   if (str != NS_LITERAL_STRING("smdkc110")) {
      //     status = FEATURE_BLOCKED_DEVICE;
      //   }
      // }
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, &os);
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  mAdapterVendorID = aVendorID;
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  mAdapterDeviceID = aDeviceID;
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  mDriverVersion = aDriverVersion;
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(PRUint32 aVersion)
{
  return NS_OK;
}

#endif
