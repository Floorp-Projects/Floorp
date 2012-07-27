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

GfxInfo::GfxInfo()
  : mInitializedFromJavaData(false)
{
}

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

void
GfxInfo::EnsureInitializedFromGfxInfoData()
{
  if (mInitializedFromJavaData)
    return;
  mInitializedFromJavaData = true;

  {
    nsCString gfxInfoData;
    mozilla::AndroidBridge::Bridge()->GetGfxInfoData(gfxInfoData);

    // the code here is a mini-parser for the text that GfxInfoThread.java produces.
    // Here, |stringToFill| is the parser state. If it's null, we are expecting
    // the next line to tell us what is the next string we'll read, e.g. "VENDOR"
    // means that the next string we'll read is |mVendor|. We record that knowledge
    // in the |stringToFill| pointer. So when it's not null, we just copy the next
    // input line into the string pointed to by |stringToFill|.
    nsCString *stringToFill = nsnull;
    char *bufptr = gfxInfoData.BeginWriting();

    while(true) {
      char *line = NS_strtok("\n", &bufptr);
      if (!line)
        break;
      if (stringToFill) {
        stringToFill->Assign(line);
        stringToFill = nsnull;
      } else if(!strcmp(line, "VENDOR")) {
        stringToFill = &mVendor;
      } else if(!strcmp(line, "RENDERER")) {
        stringToFill = &mRenderer;
      } else if(!strcmp(line, "VERSION")) {
        stringToFill = &mVersion;
      } else if(!strcmp(line, "ERROR")) {
        stringToFill = &mError;
      }
    }
  }


  if (!mError.IsEmpty()) {
    mAdapterDescription.AppendPrintf("An error occurred earlier while querying gfx info: %s. ",
                                     mError.get());
    printf_stderr("%s\n", mAdapterDescription.get());
  }

  const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
  if (spoofedVendor)
      mVendor.Assign(spoofedVendor);
  const char *spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
  if (spoofedRenderer)
      mRenderer.Assign(spoofedRenderer);
  const char *spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
  if (spoofedVersion)
      mVersion.Assign(spoofedVersion);

  mAdapterDescription.AppendPrintf("%s -- %s -- %s",
                                   mVendor.get(),
                                   mRenderer.get(),
                                   mVersion.get());

  // Now we append general (non-gfx) device information. The only reason why this code is still here
  // is that this used to be all we had in GfxInfo on Android, and we can't trivially remove it
  // as it's useful information that isn't given anywhere else in about:support of in crash reports.
  // But we should really move this out of GfxInfo.
  if (mozilla::AndroidBridge::Bridge()) {
    nsAutoString str;
    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", str)) {
      mAdapterDescription.AppendPrintf(" -- Model: %s",  NS_LossyConvertUTF16toASCII(str).get());
    }

    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "PRODUCT", str)) {
      mAdapterDescription.AppendPrintf(", Product: %s", NS_LossyConvertUTF16toASCII(str).get());
    }

    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MANUFACTURER", str)) {
      mAdapterDescription.AppendPrintf(", Manufacturer: %s", NS_LossyConvertUTF16toASCII(str).get());
    }

    int32_t version; // the HARDWARE field isn't available on Android SDK < 8
    if (!mozilla::AndroidBridge::Bridge()->GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &version))
      version = 0;

    if (version >= 8 && mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
        mAdapterDescription.AppendPrintf(", Hardware: %s", NS_LossyConvertUTF16toASCII(str).get());
      }
    }
  }

  AddCrashReportAnnotations();
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = NS_ConvertASCIItoUTF16(mAdapterDescription);
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
  aAdapterDriverVersion = NS_ConvertASCIItoUTF16(mVersion);
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
  aAdapterVendorID = NS_ConvertASCIItoUTF16(mVendor);
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
  aAdapterDeviceID = NS_ConvertASCIItoUTF16(mRenderer);
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
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
                                     mVendor);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
                                     mRenderer);

  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsCAutoString note;
  note.AppendPrintf("AdapterDescription: '%s'\n", mAdapterDescription.get());

  CrashReporter::AppendAppNotesToCrashReport(note);
#endif
}

const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  if (mDriverInfo->IsEmpty()) {
#ifdef MOZ_JAVA_COMPOSITOR
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAll), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_NO_INFO,
      DRIVER_COMPARISON_IGNORED, GfxDriverInfo::allDriverVersions );
#else
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAll), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
      DRIVER_COMPARISON_IGNORED, GfxDriverInfo::allDriverVersions );
#endif
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

  EnsureInitializedFromGfxInfoData();

  if (!mError.IsEmpty()) {
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

  // Don't evaluate special cases when evaluating the downloaded blocklist.
  if (aDriverInfo.IsEmpty()) {
    if (aFeature == FEATURE_WEBGL_OPENGL) {
      if (mRenderer.Find("Adreno 200") != -1 ||
          mRenderer.Find("Adreno 205") != -1)
      {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        return NS_OK;
      }
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, &os);
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  EnsureInitializedFromGfxInfoData(); // initialization from GfxInfo data overwrites mVendor
  mVendor = NS_LossyConvertUTF16toASCII(aVendorID);
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  EnsureInitializedFromGfxInfoData(); // initialization from GfxInfo data overwrites mRenderer
  mRenderer = NS_LossyConvertUTF16toASCII(aDeviceID);
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  EnsureInitializedFromGfxInfoData(); // initialization from GfxInfo data overwrites mVersion
  mVersion = NS_LossyConvertUTF16toASCII(aDriverVersion);
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(PRUint32 aVersion)
{
  return NS_OK;
}

#endif
