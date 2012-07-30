/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "mozilla/Util.h"

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"
#include "nsCocoaFeatures.h"
#include "mozilla/Preferences.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <Cocoa/Cocoa.h>

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

#define MAC_OS_X_VERSION_MASK       0x0000FFFF
#define MAC_OS_X_VERSION_MAJOR_MASK 0x0000FFF0
#define MAC_OS_X_VERSION_10_4_HEX   0x00001040 // Not supported
#define MAC_OS_X_VERSION_10_5_HEX   0x00001050
#define MAC_OS_X_VERSION_10_6_HEX   0x00001060
#define MAC_OS_X_VERSION_10_7_HEX   0x00001070

using namespace mozilla;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED1(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

GfxInfo::GfxInfo()
{
}

static OperatingSystem
OSXVersionToOperatingSystem(PRUint32 aOSXVersion)
{
  switch (aOSXVersion & MAC_OS_X_VERSION_MAJOR_MASK) {
    case MAC_OS_X_VERSION_10_5_HEX:
      return DRIVER_OS_OS_X_10_5;
    case MAC_OS_X_VERSION_10_6_HEX:
      return DRIVER_OS_OS_X_10_6;
    case MAC_OS_X_VERSION_10_7_HEX:
      return DRIVER_OS_OS_X_10_7;
  }

  return DRIVER_OS_UNKNOWN;
}
// The following three functions are derived from Chromium code
static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                       CFStringRef propertyName)
{
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}

static PRUint32 IntValueOfCFData(CFDataRef d)
{
  PRUint32 value = 0;

  if (d) {
    const PRUint32 *vp = reinterpret_cast<const PRUint32*>(CFDataGetBytePtr(d));
    if (vp != NULL)
      value = *vp;
  }

  return value;
}

void
GfxInfo::GetDeviceInfo()
{
  io_registry_entry_t dsp_port = CGDisplayIOServicePort(kCGDirectMainDisplay);
  CFTypeRef vendor_id_ref = SearchPortForProperty(dsp_port, CFSTR("vendor-id"));
  if (vendor_id_ref) {
    mAdapterVendorID.AppendPrintf("0x%4x", IntValueOfCFData((CFDataRef)vendor_id_ref));
    CFRelease(vendor_id_ref);
  }
  CFTypeRef device_id_ref = SearchPortForProperty(dsp_port, CFSTR("device-id"));
  if (device_id_ref) {
    mAdapterDeviceID.AppendPrintf("0x%4x", IntValueOfCFData((CFDataRef)device_id_ref));
    CFRelease(device_id_ref);
  }
}

nsresult
GfxInfo::Init()
{
  NS_TIME_FUNCTION;

  nsresult rv = GfxInfoBase::Init();

  // Calling CGLQueryRendererInfo causes us to switch to the discrete GPU
  // even when we don't want to. We'll avoid doing so for now and just
  // use the device ids.
#if 0
  CGLRendererInfoObj renderer = 0;
  GLint rendererCount = 0;

  memset(mRendererIDs, 0, sizeof(mRendererIDs));

  if (CGLQueryRendererInfo(0xffffffff, &renderer, &rendererCount) != kCGLNoError)
    return rv;

  rendererCount = (GLint) NS_MIN(rendererCount, (GLint) ArrayLength(mRendererIDs));
  for (GLint i = 0; i < rendererCount; i++) {
    GLint prop = 0;

    if (!mRendererIDsString.IsEmpty())
      mRendererIDsString.AppendLiteral(",");
    if (CGLDescribeRenderer(renderer, i, kCGLRPRendererID, &prop) == kCGLNoError) {
#ifdef kCGLRendererIDMatchingMask
      prop = prop & kCGLRendererIDMatchingMask;
#else
      prop = prop & 0x00FE7F00; // this is the mask token above, but it doesn't seem to exist everywhere?
#endif
      mRendererIDs[i] = prop;
      mRendererIDsString.AppendPrintf("0x%04x", prop);
    } else {
      mRendererIDs[i] = 0;
      mRendererIDsString.AppendPrintf("???");
    }
  }

  CGLDestroyRendererInfo(renderer);
#endif

  GetDeviceInfo();

  AddCrashReportAnnotations();

  mOSXVersion = nsCocoaFeatures::OSXVersion();

  return rv;
}

NS_IMETHODIMP
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
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

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mRendererIDsString;
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
  aAdapterRAM = mAdapterRAMString;
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
  aAdapterDriverVersion.AssignLiteral("");
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
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
  nsString deviceID, vendorID;
  nsCAutoString narrowDeviceID, narrowVendorID;

  GetAdapterDeviceID(deviceID);
  CopyUTF16toUTF8(deviceID, narrowDeviceID);
  GetAdapterVendorID(vendorID);
  CopyUTF16toUTF8(vendorID, narrowVendorID);

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
  CrashReporter::AppendAppNotesToCrashReport(note);
#endif
}

// We don't support checking driver versions on Mac.
#define IMPLEMENT_MAC_DRIVER_BLOCKLIST(os, vendor, device, features, blockOn) \
  APPEND_TO_DRIVER_BLOCKLIST(os, vendor, device, features, blockOn,           \
                             DRIVER_COMPARISON_IGNORED, V(0,0,0,0), "")


const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  if (!mDriverInfo->Length()) {
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), GfxDriverInfo::allDevices,
      nsIGfxInfo::FEATURE_WEBGL_MSAA, nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION);
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorATI), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(RadeonX1000),
      nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE);
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(DRIVER_OS_ALL,
      (nsAString&) GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), (GfxDeviceFamily*) GfxDriverInfo::GetDeviceFamily(Geforce7300GT), 
      nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_BLOCKED_DEVICE);
  }
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, 
                              PRInt32* aStatus,
                              nsAString& aSuggestedDriverVersion,
                              const nsTArray<GfxDriverInfo>& aDriverInfo,
                              OperatingSystem* aOS /* = nullptr */)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  OperatingSystem os = OSXVersionToOperatingSystem(mOSXVersion);
  if (aOS)
    *aOS = os;

  // Don't evaluate special cases when we're evaluating the downloaded blocklist.
  if (!aDriverInfo.Length()) {
    // Many WebGL issues on 10.5, especially:
    //   * bug 631258: WebGL shader paints using textures belonging to other processes on Mac OS 10.5
    //   * bug 618848: Post process shaders and texture mapping crash OS X 10.5
    if (aFeature == nsIGfxInfo::FEATURE_WEBGL_OPENGL &&
        !nsCocoaFeatures::OnSnowLeopardOrLater()) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
      return NS_OK;
    }

    // The code around the following has been moved into the global blocklist.
#if 0
      // CGL reports a list of renderers, some renderers are slow (e.g. software)
      // and AFAIK we can't decide which one will be used among them, so let's implement this by returning NO_INFO
      // if any not-known-to-be-bad renderer is found.
      // The assumption that we make here is that the system will spontaneously use the best/fastest renderer in the list.
      // Note that the presence of software renderer fallbacks means that slow software rendering may be automatically
      // used, which seems to be the case in bug 611292 where the user had a Intel GMA 945 card (non programmable hardware).
      // Therefore we need to explicitly blacklist non-OpenGL2 hardware, which could result in a software renderer
      // being used.

      for (PRUint32 i = 0; i < ArrayLength(mRendererIDs); ++i) {
        switch (mRendererIDs[i]) {
          case kCGLRendererATIRage128ID: // non-programmable
          case kCGLRendererATIRadeonID: // non-programmable
          case kCGLRendererATIRageProID: // non-programmable
          case kCGLRendererATIRadeon8500ID: // no OpenGL 2 support, http://en.wikipedia.org/wiki/Radeon_R200
          case kCGLRendererATIRadeon9700ID: // no OpenGL 2 support, http://en.wikipedia.org/wiki/Radeon_R200
          case kCGLRendererATIRadeonX1000ID: // can't render to non-power-of-two texture backed framebuffers
          case kCGLRendererIntel900ID: // non-programmable
          case kCGLRendererGeForce2MXID: // non-programmable
          case kCGLRendererGeForce3ID: // no OpenGL 2 support,
                                       // http://en.wikipedia.org/wiki/Comparison_of_Nvidia_graphics_processing_units
          case kCGLRendererGeForceFXID: // incomplete OpenGL 2 support with software fallbacks,
                                        // http://en.wikipedia.org/wiki/Comparison_of_Nvidia_graphics_processing_units
          case kCGLRendererVTBladeXP2ID: // Trident DX8 chip, assuming it's not GL2 capable
          case kCGLRendererMesa3DFXID: // non-programmable
          case kCGLRendererGenericFloatID: // software renderer
          case kCGLRendererGenericID: // software renderer
          case kCGLRendererAppleSWID: // software renderer
            break;
          default:
            if (mRendererIDs[i])
              foundGoodDevice = true;
        }
      }
#endif

    if (aFeature == nsIGfxInfo::FEATURE_WEBGL_MSAA) {
      // Blacklist all ATI cards on OSX, except for
      // 0x6760 and 0x9488
      if (mAdapterVendorID.Equals(GfxDriverInfo::GetDeviceVendor(VendorATI), nsCaseInsensitiveStringComparator()) && 
          (mAdapterDeviceID.LowerCaseEqualsLiteral("0x6760") ||
           mAdapterDeviceID.LowerCaseEqualsLiteral("0x9488"))) {
        *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
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
  mOSXVersion = aVersion;
  return NS_OK;
}

#endif
