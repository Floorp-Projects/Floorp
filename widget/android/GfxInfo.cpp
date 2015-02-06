/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfo.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "nsUnicharUtils.h"
#include "prenv.h"
#include "prprf.h"
#include "nsHashKeys.h"
#include "nsVersionComparator.h"
#include "AndroidBridge.h"
#include "nsIWindowWatcher.h"
#include "nsServiceManagerUtils.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

namespace mozilla {
namespace widget {

class GfxInfo::GLStrings
{
  nsCString mVendor;
  nsCString mRenderer;
  nsCString mVersion;
  bool mReady;

public:
  GLStrings()
    : mReady(false)
  {}

  const nsCString& Vendor() {
    EnsureInitialized();
    return mVendor;
  }

  void SpoofVendor(const nsCString& s) {
    EnsureInitialized();
    mVendor = s;
  }

  const nsCString& Renderer() {
    EnsureInitialized();
    return mRenderer;
  }

  void SpoofRenderer(const nsCString& s) {
    EnsureInitialized();
    mRenderer = s;
  }

  const nsCString& Version() {
    EnsureInitialized();
    return mVersion;
  }

  void SpoofVersion(const nsCString& s) {
    EnsureInitialized();
    mVersion = s;
  }

  void EnsureInitialized() {
    if (mReady) {
      return;
    }

    nsRefPtr<gl::GLContext> gl;
    gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(16, 16),
                                                gl::SurfaceCaps::ForRGB());

    if (!gl) {
      // Setting mReady to true here means that we won't retry. Everything will
      // remain blacklisted forever. Ideally, we would like to update that once
      // any GLContext is successfully created, like the compositor's GLContext.
      mReady = true;
      return;
    }

    gl->MakeCurrent();

    const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
    if (spoofedVendor)
        mVendor.Assign(spoofedVendor);
    else
        mVendor.Assign((const char*)gl->fGetString(LOCAL_GL_VENDOR));
    const char *spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
    if (spoofedRenderer)
        mRenderer.Assign(spoofedRenderer);
    else
        mRenderer.Assign((const char*)gl->fGetString(LOCAL_GL_RENDERER));
    const char *spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
    if (spoofedVersion)
        mVersion.Assign(spoofedVersion);
    else
        mVersion.Assign((const char*)gl->fGetString(LOCAL_GL_VERSION));

    mReady = true;
  }
};

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

GfxInfo::GfxInfo()
  : mInitialized(false)
  , mGLStrings(new GLStrings)
{
}

GfxInfo::~GfxInfo()
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
GfxInfo::EnsureInitialized()
{
  if (mInitialized)
    return;

  mGLStrings->EnsureInitialized();

  MOZ_ASSERT(mozilla::AndroidBridge::Bridge());

  if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", mModel)) {
    mAdapterDescription.AppendPrintf("Model: %s",  NS_LossyConvertUTF16toASCII(mModel).get());
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

  mAdapterDescription.AppendPrintf(", OpenGL: %s -- %s -- %s",
                                   mGLStrings->Vendor().get(),
                                   mGLStrings->Renderer().get(),
                                   mGLStrings->Version().get());

  AddCrashReportAnnotations();

  mInitialized = true;
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  EnsureInitialized();
  aAdapterDescription = NS_ConvertASCIItoUTF16(mAdapterDescription);
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  EnsureInitialized();
  aAdapterRAM.Truncate();
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  EnsureInitialized();
  aAdapterDriver.Truncate();
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  EnsureInitialized();
  aAdapterDriverVersion = NS_ConvertASCIItoUTF16(mGLStrings->Version());
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  EnsureInitialized();
  aAdapterDriverDate.Truncate();
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  EnsureInitialized();
  aAdapterVendorID = NS_ConvertASCIItoUTF16(mGLStrings->Vendor());
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  EnsureInitialized();
  aAdapterDeviceID = NS_ConvertASCIItoUTF16(mGLStrings->Renderer());
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterSubsysID; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString & aAdapterSubsysID)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterSubsysID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString & aAdapterSubsysID)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  EnsureInitialized();
  return NS_ERROR_FAILURE;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
                                     mGLStrings->Vendor());
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
                                     mGLStrings->Renderer());
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDriverVersion"),
                                     mGLStrings->Version());

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
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_STATUS_OK,
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

  // OpenGL layers are never blacklisted on Android.
  // This early return is so we avoid potentially slow
  // GLStrings initialization on startup when we initialize GL layers.
  if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS) {
    *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
    return NS_OK;
  }

  EnsureInitialized();

  if (mGLStrings->Vendor().IsEmpty() || mGLStrings->Renderer().IsEmpty()) {
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

  // Don't evaluate special cases when evaluating the downloaded blocklist.
  if (aDriverInfo.IsEmpty()) {
    if (aFeature == FEATURE_WEBGL_OPENGL) {
      if (mGLStrings->Renderer().Find("Adreno 200") != -1 ||
          mGLStrings->Renderer().Find("Adreno 205") != -1)
      {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        return NS_OK;
      }

      if (mHardware.EqualsLiteral("ville")) {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        return NS_OK;
      }
    }

    if (aFeature == FEATURE_STAGEFRIGHT) {
      NS_LossyConvertUTF16toASCII cManufacturer(mManufacturer);
      NS_LossyConvertUTF16toASCII cModel(mModel);
      NS_LossyConvertUTF16toASCII cHardware(mHardware);

      if (cHardware.EqualsLiteral("antares") ||
          cHardware.EqualsLiteral("harmony") ||
          cHardware.EqualsLiteral("picasso") ||
          cHardware.EqualsLiteral("picasso_e") ||
          cHardware.EqualsLiteral("ventana") ||
          cHardware.EqualsLiteral("rk30board"))
      {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        return NS_OK;
      }

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
        // Gingerbread Samsung devices are whitelisted except for:
        //   Samsung devices identified in Bug 847837
        // Gingerbread Sony devices are whitelisted.
        // All other Gingerbread devices are blacklisted.
        bool isWhitelisted =
          cManufacturer.Equals("htc", nsCaseInsensitiveCStringComparator()) ||
          (cManufacturer.Find("sony", true) != -1) ||
          cManufacturer.Equals("samsung", nsCaseInsensitiveCStringComparator());

        if (cModel.Equals("GT-I8160", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-I8160L", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-I8530", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-I9070", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-I9070P", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-I8160P", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-S7500", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-S7500T", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-S7500L", nsCaseInsensitiveCStringComparator()) ||
            cModel.Equals("GT-S6500T", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("smdkc110", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("smdkc210", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("herring", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("shw-m110s", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("shw-m180s", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("n1", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("latona", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("aalto", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("atlas", nsCaseInsensitiveCStringComparator()) ||
            cHardware.Equals("qcom", nsCaseInsensitiveCStringComparator()))
        {
          isWhitelisted = false;
        }

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
        //   All Samsung ICS devices, except for:
        //     Samsung SGH-I717 (Bug 845729)
        //     Samsung SGH-I727 (Bug 845729)
        //     Samsung SGH-I757 (Bug 845729)
        //   All Galaxy nexus ICS devices
        //   Sony Xperia Ion (LT28) ICS devices
        bool isWhitelisted =
          cModel.Equals("LT28h", nsCaseInsensitiveCStringComparator()) ||
          cManufacturer.Equals("samsung", nsCaseInsensitiveCStringComparator()) ||
          cModel.Equals("galaxy nexus", nsCaseInsensitiveCStringComparator()); // some Galaxy Nexus have manufacturer=amazon

        if (cModel.Find("SGH-I717", true) != -1 ||
            cModel.Find("SGH-I727", true) != -1 ||
            cModel.Find("SGH-I757", true) != -1)
        {
          isWhitelisted = false;
        }

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
        //   Samsung devices from bug 812881 and 853522.
        //   Motorola XT890 from bug 882342.
        bool isBlocklisted =
          cModel.Find("GT-P3100", true) != -1 ||
          cModel.Find("GT-P3110", true) != -1 ||
          cModel.Find("GT-P3113", true) != -1 ||
          cModel.Find("GT-P5100", true) != -1 ||
          cModel.Find("GT-P5110", true) != -1 ||
          cModel.Find("GT-P5113", true) != -1 ||
          cModel.Find("XT890", true) != -1;

        if (isBlocklisted) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
      else if (CompareVersions(mOSVersion.get(), "4.3.0") < 0)
      {
        // Blocklist all Sony devices
        if (cManufacturer.Find("Sony", true) != -1) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
          return NS_OK;
        }
      }
    }

    if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION) {
      NS_LossyConvertUTF16toASCII cManufacturer(mManufacturer);
      NS_LossyConvertUTF16toASCII cModel(mModel);
      NS_LossyConvertUTF16toASCII cHardware(mHardware);

      if (cHardware.EqualsLiteral("hammerhead") &&
          CompareVersions(mOSVersion.get(), "4.4.2") >= 0 &&
          cManufacturer.Equals("lge", nsCaseInsensitiveCStringComparator()) &&
          cModel.Equals("nexus 5", nsCaseInsensitiveCStringComparator())) {
        *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
        return NS_OK;
      } else {
        // Blocklist all other devices except Nexus 5 which VP8 hardware acceleration is supported
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
  EnsureInitialized();
  mGLStrings->SpoofVendor(NS_LossyConvertUTF16toASCII(aVendorID));
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  EnsureInitialized();
  mGLStrings->SpoofRenderer(NS_LossyConvertUTF16toASCII(aDeviceID));
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  EnsureInitialized();
  mGLStrings->SpoofVersion(NS_LossyConvertUTF16toASCII(aDriverVersion));
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  EnsureInitialized();
  mOSVersion = aVersion;
  return NS_OK;
}

#endif

nsString GfxInfo::Model()
{
  EnsureInitialized();
  return mModel;
}

nsString GfxInfo::Hardware()
{
  EnsureInitialized();
  return mHardware;
}

nsString GfxInfo::Product()
{
  EnsureInitialized();
  return mProduct;
}

nsString GfxInfo::Manufacturer()
{
  EnsureInitialized();
  return mManufacturer;
}

uint32_t GfxInfo::OperatingSystemVersion()
{
  EnsureInitialized();
  return mOSVersionInteger;
}

}
}
