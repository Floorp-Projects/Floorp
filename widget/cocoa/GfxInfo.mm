/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "mozilla/ArrayUtils.h"

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "nsExceptionHandler.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsICrashReporter.h"
#include "mozilla/Preferences.h"
#include <algorithm>

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <Cocoa/Cocoa.h>

#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

GfxInfo::GfxInfo() : mOSXVersion{0} {}

static OperatingSystem OSXVersionToOperatingSystem(uint32_t aOSXVersion) {
  if (nsCocoaFeatures::ExtractMajorVersion(aOSXVersion) == 10) {
    switch (nsCocoaFeatures::ExtractMinorVersion(aOSXVersion)) {
      case 6:
        return OperatingSystem::OSX10_6;
      case 7:
        return OperatingSystem::OSX10_7;
      case 8:
        return OperatingSystem::OSX10_8;
      case 9:
        return OperatingSystem::OSX10_9;
      case 10:
        return OperatingSystem::OSX10_10;
      case 11:
        return OperatingSystem::OSX10_11;
      case 12:
        return OperatingSystem::OSX10_12;
      case 13:
        return OperatingSystem::OSX10_13;
    }
  }

  return OperatingSystem::Unknown;
}
// The following three functions are derived from Chromium code
static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort, CFStringRef propertyName) {
  return IORegistryEntrySearchCFProperty(dspPort, kIOServicePlane, propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively | kIORegistryIterateParents);
}

static uint32_t IntValueOfCFData(CFDataRef d) {
  uint32_t value = 0;

  if (d) {
    const uint32_t* vp = reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(d));
    if (vp != NULL) value = *vp;
  }

  return value;
}

void GfxInfo::GetDeviceInfo() {
  io_registry_entry_t dsp_port = CGDisplayIOServicePort(kCGDirectMainDisplay);
  CFTypeRef vendor_id_ref = SearchPortForProperty(dsp_port, CFSTR("vendor-id"));
  if (vendor_id_ref) {
    mAdapterVendorID.AppendPrintf("0x%04x", IntValueOfCFData((CFDataRef)vendor_id_ref));
    CFRelease(vendor_id_ref);
  }
  CFTypeRef device_id_ref = SearchPortForProperty(dsp_port, CFSTR("device-id"));
  if (device_id_ref) {
    mAdapterDeviceID.AppendPrintf("0x%04x", IntValueOfCFData((CFDataRef)device_id_ref));
    CFRelease(device_id_ref);
  }
}

nsresult GfxInfo::Init() {
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
GfxInfo::GetD2DEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString& aDwriteVersion) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString cleartypeParameters; */
NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString& aCleartypeParams) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString windowProtocol; */
NS_IMETHODIMP
GfxInfo::GetWindowProtocol(nsAString& aWindowProtocol) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString& aAdapterDescription) {
  aAdapterDescription.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString& aAdapterDescription) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString& aAdapterRAM) {
  aAdapterRAM = mAdapterRAMString;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString& aAdapterRAM) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString& aAdapterDriver) {
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString& aAdapterDriver) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDriverVendor; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) {
  aAdapterDriverVendor.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVendor2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString& aAdapterDriverVersion) {
  aAdapterDriverVersion.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString& aAdapterDriverVersion) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString& aAdapterDriverDate) {
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString& aAdapterDriverDate) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString& aAdapterVendorID) {
  aAdapterVendorID = mAdapterVendorID;
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString& aAdapterVendorID) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString& aAdapterDeviceID) {
  aAdapterDeviceID = mAdapterDeviceID;
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString& aAdapterDeviceID) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterSubsysID; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString& aAdapterSubsysID) { return NS_ERROR_FAILURE; }

/* readonly attribute DOMString adapterSubsysID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString& aAdapterSubsysID) { return NS_ERROR_FAILURE; }

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active) { return NS_ERROR_FAILURE; }

void GfxInfo::AddCrashReportAnnotations() {
  nsString deviceID, vendorID, driverVersion;
  nsAutoCString narrowDeviceID, narrowVendorID, narrowDriverVersion;

  GetAdapterDeviceID(deviceID);
  CopyUTF16toUTF8(deviceID, narrowDeviceID);
  GetAdapterVendorID(vendorID);
  CopyUTF16toUTF8(vendorID, narrowVendorID);
  GetAdapterDriverVersion(driverVersion);
  CopyUTF16toUTF8(driverVersion, narrowDriverVersion);

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AdapterVendorID, narrowVendorID);
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AdapterDeviceID, narrowDeviceID);
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AdapterDriverVersion,
                                     narrowDriverVersion);
}

// We don't support checking driver versions on Mac.
#define IMPLEMENT_MAC_DRIVER_BLOCKLIST(os, vendor, driverVendor, device, features, blockOn, \
                                       ruleId)                                              \
  APPEND_TO_DRIVER_BLOCKLIST(os, vendor, driverVendor, device, features, blockOn,           \
                             DRIVER_COMPARISON_IGNORED, V(0, 0, 0, 0), ruleId, "")

const nsTArray<GfxDriverInfo>& GfxInfo::GetGfxDriverInfo() {
  if (!sDriverInfo->Length()) {
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(
        OperatingSystem::OSX, (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(RadeonX1000),
        nsIGfxInfo::FEATURE_OPENGL_LAYERS, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
        "FEATURE_FAILURE_MAC_RADEONX1000_NO_TEXTURE2D");
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(
        OperatingSystem::OSX, (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(Geforce7300GT),
        nsIGfxInfo::FEATURE_WEBGL_OPENGL, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
        "FEATURE_FAILURE_MAC_7300_NO_WEBGL");
    IMPLEMENT_MAC_DRIVER_BLOCKLIST(
        OperatingSystem::OSX, (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        (GfxDeviceFamily*)GfxDriverInfo::GetDeviceFamily(IntelHDGraphicsIvyBridge),
        nsIGfxInfo::FEATURE_GL_SWIZZLE, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
        "FEATURE_FAILURE_MAC_INTELHD4000_NO_SWIZZLE");
  }
  return *sDriverInfo;
}

nsresult GfxInfo::GetFeatureStatusImpl(int32_t aFeature, int32_t* aStatus,
                                       nsAString& aSuggestedDriverVersion,
                                       const nsTArray<GfxDriverInfo>& aDriverInfo,
                                       nsACString& aFailureId,
                                       OperatingSystem* aOS /* = nullptr */) {
  NS_ENSURE_ARG_POINTER(aStatus);
  aSuggestedDriverVersion.SetIsVoid(true);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  OperatingSystem os = OSXVersionToOperatingSystem(mOSXVersion);
  if (aOS) *aOS = os;

  if (sShutdownOccurred) {
    return NS_OK;
  }

  // Don't evaluate special cases when we're evaluating the downloaded blocklist.
  if (!aDriverInfo.Length()) {
    if (aFeature == nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION) {
      // See bug 1249659
      switch (os) {
        case OperatingSystem::OSX10_5:
        case OperatingSystem::OSX10_6:
        case OperatingSystem::OSX10_7:
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
          aFailureId = "FEATURE_FAILURE_CANVAS_OSX_VERSION";
          break;
        default:
          *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
          break;
      }
      return NS_OK;
    } else if (aFeature == nsIGfxInfo::FEATURE_WEBRENDER) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
      aFailureId = "FEATURE_UNQUALIFIED_WEBRENDER_MAC";
      return NS_OK;
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo,
                                           aFailureId, &os);
}

nsresult GfxInfo::FindMonitors(JSContext* aCx, JS::HandleObject aOutArray) {
  // Getting the refresh rate is a little hard on OS X. We could use
  // CVDisplayLinkGetNominalOutputVideoRefreshPeriod, but that's a little
  // involved. Ideally we could query it from vsync. For now, we leave it out.
  int32_t deviceCount = 0;
  for (NSScreen* screen in [NSScreen screens]) {
    NSRect rect = [screen frame];

    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

    JS::Rooted<JS::Value> screenWidth(aCx, JS::Int32Value((int)rect.size.width));
    JS_SetProperty(aCx, obj, "screenWidth", screenWidth);

    JS::Rooted<JS::Value> screenHeight(aCx, JS::Int32Value((int)rect.size.height));
    JS_SetProperty(aCx, obj, "screenHeight", screenHeight);

    JS::Rooted<JS::Value> scale(aCx, JS::NumberValue(nsCocoaUtils::GetBackingScaleFactor(screen)));
    JS_SetProperty(aCx, obj, "scale", scale);

    JS::Rooted<JS::Value> element(aCx, JS::ObjectValue(*obj));
    JS_SetElement(aCx, aOutArray, deviceCount++, element);
  }
  return NS_OK;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString& aVendorID) {
  mAdapterVendorID = aVendorID;
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString& aDeviceID) {
  mAdapterDeviceID = aDeviceID;
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString& aDriverVersion) {
  mDriverVersion = aDriverVersion;
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion) {
  mOSXVersion = aVersion;
  return NS_OK;
}

/* void fireTestProcess (); */
NS_IMETHODIMP GfxInfo::FireTestProcess() { return NS_OK; }

#endif
