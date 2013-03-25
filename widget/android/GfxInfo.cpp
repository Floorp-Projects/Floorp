/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "prenv.h"
#include "prprf.h"
#include "nsHashKeys.h"
#include "nsVersionComparator.h"

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
    nsCString *stringToFill = nullptr;
    char *bufptr = gfxInfoData.BeginWriting();

    while(true) {
      char *line = NS_strtok("\n", &bufptr);
      if (!line)
        break;
      if (stringToFill) {
        stringToFill->Assign(line);
        stringToFill = nullptr;
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
    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", mModel)) {
      mAdapterDescription.AppendPrintf(" -- Model: %s",  NS_LossyConvertUTF16toASCII(mModel).get());
    }

    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "PRODUCT", mProduct)) {
      mAdapterDescription.AppendPrintf(", Product: %s", NS_LossyConvertUTF16toASCII(mProduct).get());
    }

    if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MANUFACTURER", mManufacturer)) {
      mAdapterDescription.AppendPrintf(", Manufacturer: %s", NS_LossyConvertUTF16toASCII(mManufacturer).get());
    }

    int32_t sdkVersion;
    if (!mozilla::AndroidBridge::Bridge()->GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &sdkVersion))
      sdkVersion = 0;

    // the HARDWARE field isn't available on Android SDK < 8
    if (sdkVersion >= 8 && mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", mHardware)) {
      mAdapterDescription.AppendPrintf(", Hardware: %s", NS_LossyConvertUTF16toASCII(mHardware).get());
    }

    nsString release;
    mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build$VERSION", "RELEASE", release);
    mOSVersion = NS_LossyConvertUTF16toASCII(release);

    mOSVersionInteger = 0;
    char a[5], b[5], c[5], d[5];
    SplitDriverVersion(mOSVersion.get(), a, b, c, d);
    uint8_t na = atoi(a);
    uint8_t nb = atoi(b);
    uint8_t nc = atoi(c);
    uint8_t nd = atoi(d);

    mOSVersionInteger = (uint32_t(na) << 24) |
                        (uint32_t(nb) << 16) |
                        (uint32_t(nc) << 8)  |
                        uint32_t(nd);
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
  nsAutoCString note;
  note.AppendPrintf("AdapterDescription: '%s'\n", mAdapterDescription.get());

  CrashReporter::AppendAppNotesToCrashReport(note);
#endif
}

const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  if (mDriverInfo->IsEmpty()) {
    APPEND_TO_DRIVER_BLOCKLIST2( DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorAll), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_NO_INFO,
      DRIVER_COMPARISON_IGNORED, GfxDriverInfo::allDriverVersions );
  }

  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(int32_t aFeature, 
                              int32_t *aStatus, 
                              nsAString & aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo, 
                              OperatingSystem* aOS /* = nullptr */)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  OperatingSystem os = mOS;
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

      if (mHardware.Equals(NS_LITERAL_STRING("ville"))) {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        return NS_OK;
      }
    }

    if (aFeature == FEATURE_STAGEFRIGHT) {
      NS_LossyConvertUTF16toASCII cManufacturer(mManufacturer);
      NS_LossyConvertUTF16toASCII cModel(mModel);
      if (CompareVersions(mOSVersion.get(), "2.2.0") >= 0 &&
          CompareVersions(mOSVersion.get(), "2.3.0") < 0)
      {
        // Froyo LG devices are whitelisted.
        // All other Froyo
        bool isWhitelisted =
          cManufacturer.Equals("lge", nsCaseInsensitiveCStringComparator());

        if (!isWhitelisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
      else if (CompareVersions(mOSVersion.get(), "2.3.0") >= 0 &&
          CompareVersions(mOSVersion.get(), "2.4.0") < 0)
      {
        // Gingerbread HTC devices are whitelisted.
        // Gingerbread Samsung devices are whitelisted.
        // All other Gingerbread devices are blacklisted.
	bool isWhitelisted =
          cManufacturer.Equals("htc", nsCaseInsensitiveCStringComparator()) ||
          cManufacturer.Equals("samsung", nsCaseInsensitiveCStringComparator());

        if (!isWhitelisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
      else if (CompareVersions(mOSVersion.get(), "3.0.0") >= 0 &&
          CompareVersions(mOSVersion.get(), "4.0.0") < 0)
      {
        // Honeycomb Samsung devices are whitelisted.
        // All other Honeycomb devices are blacklisted.
	bool isWhitelisted =
          cManufacturer.Equals("samsung", nsCaseInsensitiveCStringComparator());

        if (!isWhitelisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
      else if (CompareVersions(mOSVersion.get(), "4.0.0") < 0)
      {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
        return NS_OK;
      }
      else if (CompareVersions(mOSVersion.get(), "4.1.0") < 0)
      {
        // Whitelist:
        //   All Samsung ICS devices
        //   All Galaxy nexus ICS devices
        //   Sony Xperia Ion (LT28) ICS devices
        bool isWhitelisted =
          cModel.Equals("LT28h", nsCaseInsensitiveCStringComparator()) ||
          cManufacturer.Equals("samsung", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("galaxy nexus", nsCaseInsensitiveCStringComparator()); // some Galaxy Nexus have manufacturer=amazon

        if (!isWhitelisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
      else if (CompareVersions(mOSVersion.get(), "4.2.0") < 0)
      {
        // Whitelist:
        //   All JB phones except for those in blocklist below
        // Blocklist:
        //   Samsung SPH-L710 (Bug 812881)
        //   Samsung SGH-T999 (Bug 812881)
        //   Samsung SCH-I535 (Bug 812881)
        //   Samsung GT-I8190 (Bug 812881)
        //   Samsung SGH-I747M (Bug 812881)
        //   Samsung SGH-I747 (Bug 812881)
        bool isBlocklisted =
          cModel.Equals("SAMSUNG-SPH-L710", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("SAMSUNG-SGH-T999", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("SAMSUNG-SCH-I535", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("SAMSUNG-GT-I8190", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("SAMSUNG-SGH-I747M", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("SAMSUNG-SGH-I747", nsCaseInsensitiveCStringComparator());

        if (isBlocklisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
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
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  EnsureInitializedFromGfxInfoData(); // initialization from GfxInfo data overwrites mOSVersion
  mOSVersion = aVersion;
  return NS_OK;
}

#endif

nsString GfxInfo::Model() const
{
  return mModel;
}

nsString GfxInfo::Hardware() const
{
  return mHardware;
}

nsString GfxInfo::Product() const
{
  return mProduct;
}

nsString GfxInfo::Manufacturer() const
{
  return mManufacturer;
}

uint32_t GfxInfo::OperatingSystemVersion() const
{
  return mOSVersionInteger;
}
