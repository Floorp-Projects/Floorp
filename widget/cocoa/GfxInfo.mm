/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "mozilla/Util.h"

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "nsCocoaFeatures.h"
#include "mozilla/Preferences.h"
#include <algorithm>

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
#define MAC_OS_X_VERSION_10_8_HEX   0x00001080

using namespace mozilla;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED1(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

GfxInfo::GfxInfo()
{
}

static OperatingSystem
OSXVersionToOperatingSystem(uint32_t aOSXVersion)
{
  switch (aOSXVersion & MAC_OS_X_VERSION_MAJOR_MASK) {
    case MAC_OS_X_VERSION_10_5_HEX:
      return DRIVER_OS_OS_X_10_5;
    case MAC_OS_X_VERSION_10_6_HEX:
      return DRIVER_OS_OS_X_10_6;
    case MAC_OS_X_VERSION_10_7_HEX:
      return DRIVER_OS_OS_X_10_7;
    case MAC_OS_X_VERSION_10_8_HEX:
      return DRIVER_OS_OS_X_10_8;
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

static uint32_t IntValueOfCFData(CFDataRef d)
{
  uint32_t value = 0;

  if (d) {
    const uint32_t *vp = reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(d));
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
  nsresult rv = GfxInfoBase::Init();

  // Calling CGLQueryRendererInfo causes us to switch to the discrete GPU
  // even when we don't want to. We'll avoid doing so for now and just
  // use the device ids.

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
  aAdapterDescription.AssignLiteral("");
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
  nsAutoCString narrowDeviceID, narrowVendorID;

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
  nsAutoCString note;
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
GfxInfo::GetFeatureStatusImpl(int32_t aFeature, 
                              int32_t* aStatus,
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
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion)
{
  mOSXVersion = aVersion;
  return NS_OK;
}

#endif
