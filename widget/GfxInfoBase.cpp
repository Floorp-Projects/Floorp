/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "GfxInfoBase.h"

#include <mutex>  // std::call_once

#include "GfxDriverInfo.h"
#include "js/Array.h"               // JS::GetArrayLength, JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_SetElement, JS_SetProperty
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsVersionComparator.h"
#include "mozilla/Services.h"
#include "mozilla/Observer.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "nsIXULAppInfo.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BuildConstants.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/widget/Screen.h"

#include "jsapi.h"

#include "gfxPlatform.h"
#include "gfxConfig.h"
#include "DriverCrashGuard.h"

using namespace mozilla::widget;
using namespace mozilla;
using mozilla::MutexAutoLock;

nsTArray<GfxDriverInfo>* GfxInfoBase::sDriverInfo;
StaticAutoPtr<nsTArray<gfx::GfxInfoFeatureStatus>> GfxInfoBase::sFeatureStatus;
bool GfxInfoBase::sDriverInfoObserverInitialized;
bool GfxInfoBase::sShutdownOccurred;

// Call this when setting sFeatureStatus to a non-null pointer to
// ensure destruction even if the GfxInfo component is never instantiated.
static void InitFeatureStatus(nsTArray<gfx::GfxInfoFeatureStatus>* aPtr) {
  static std::once_flag sOnce;
  std::call_once(sOnce, [] { ClearOnShutdown(&GfxInfoBase::sFeatureStatus); });
  GfxInfoBase::sFeatureStatus = aPtr;
}

// Observes for shutdown so that the child GfxDriverInfo list is freed.
class ShutdownObserver : public nsIObserver {
  virtual ~ShutdownObserver() = default;

 public:
  ShutdownObserver() = default;

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* subject, const char* aTopic,
                     const char16_t* aData) override {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    delete GfxInfoBase::sDriverInfo;
    GfxInfoBase::sDriverInfo = nullptr;

    for (auto& deviceFamily : GfxDriverInfo::sDeviceFamilies) {
      delete deviceFamily;
      deviceFamily = nullptr;
    }

    for (auto& windowProtocol : GfxDriverInfo::sWindowProtocol) {
      delete windowProtocol;
      windowProtocol = nullptr;
    }

    for (auto& deviceVendor : GfxDriverInfo::sDeviceVendors) {
      delete deviceVendor;
      deviceVendor = nullptr;
    }

    for (auto& driverVendor : GfxDriverInfo::sDriverVendors) {
      delete driverVendor;
      driverVendor = nullptr;
    }

    GfxInfoBase::sShutdownOccurred = true;

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver)

static void InitGfxDriverInfoShutdownObserver() {
  if (GfxInfoBase::sDriverInfoObserverInitialized) return;

  GfxInfoBase::sDriverInfoObserverInitialized = true;

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    NS_WARNING("Could not get observer service!");
    return;
  }

  ShutdownObserver* obs = new ShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

using namespace mozilla::widget;
using namespace mozilla::gfx;
using namespace mozilla;

NS_IMPL_ISUPPORTS(GfxInfoBase, nsIGfxInfo, nsIObserver,
                  nsISupportsWeakReference)

#define BLOCKLIST_PREF_BRANCH "gfx.blacklist."
#define SUGGESTED_VERSION_PREF BLOCKLIST_PREF_BRANCH "suggested-driver-version"

static const char* GetPrefNameForFeature(int32_t aFeature) {
  const char* name = nullptr;
  switch (aFeature) {
    case nsIGfxInfo::FEATURE_DIRECT2D:
      name = BLOCKLIST_PREF_BRANCH "direct2d";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS:
      name = BLOCKLIST_PREF_BRANCH "layers.direct3d9";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS:
      name = BLOCKLIST_PREF_BRANCH "layers.direct3d10";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS:
      name = BLOCKLIST_PREF_BRANCH "layers.direct3d10-1";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS:
      name = BLOCKLIST_PREF_BRANCH "layers.direct3d11";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE:
      name = BLOCKLIST_PREF_BRANCH "direct3d11angle";
      break;
    case nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING:
      name = BLOCKLIST_PREF_BRANCH "hardwarevideodecoding";
      break;
    case nsIGfxInfo::FEATURE_OPENGL_LAYERS:
      name = BLOCKLIST_PREF_BRANCH "layers.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_OPENGL:
      name = BLOCKLIST_PREF_BRANCH "webgl.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_ANGLE:
      name = BLOCKLIST_PREF_BRANCH "webgl.angle";
      break;
    case nsIGfxInfo::UNUSED_FEATURE_WEBGL_MSAA:
      name = BLOCKLIST_PREF_BRANCH "webgl.msaa";
      break;
    case nsIGfxInfo::FEATURE_STAGEFRIGHT:
      name = BLOCKLIST_PREF_BRANCH "stagefright";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_H264:
      name = BLOCKLIST_PREF_BRANCH "webrtc.hw.acceleration.h264";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE:
      name = BLOCKLIST_PREF_BRANCH "webrtc.hw.acceleration.encode";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE:
      name = BLOCKLIST_PREF_BRANCH "webrtc.hw.acceleration.decode";
      break;
    case nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION:
      name = BLOCKLIST_PREF_BRANCH "canvas2d.acceleration";
      break;
    case nsIGfxInfo::FEATURE_DX_INTEROP2:
      name = BLOCKLIST_PREF_BRANCH "dx.interop2";
      break;
    case nsIGfxInfo::FEATURE_GPU_PROCESS:
      name = BLOCKLIST_PREF_BRANCH "gpu.process";
      break;
    case nsIGfxInfo::FEATURE_WEBGL2:
      name = BLOCKLIST_PREF_BRANCH "webgl2";
      break;
    case nsIGfxInfo::FEATURE_D3D11_KEYED_MUTEX:
      name = BLOCKLIST_PREF_BRANCH "d3d11.keyed.mutex";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER:
      name = BLOCKLIST_PREF_BRANCH "webrender";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER_COMPOSITOR:
      name = BLOCKLIST_PREF_BRANCH "webrender.compositor";
      break;
    case nsIGfxInfo::FEATURE_DX_NV12:
      name = BLOCKLIST_PREF_BRANCH "dx.nv12";
      break;
    case nsIGfxInfo::FEATURE_DX_P010:
      name = BLOCKLIST_PREF_BRANCH "dx.p010";
      break;
    case nsIGfxInfo::FEATURE_DX_P016:
      name = BLOCKLIST_PREF_BRANCH "dx.p016";
      break;
    case nsIGfxInfo::FEATURE_VP8_HW_DECODE:
      name = BLOCKLIST_PREF_BRANCH "vp8.hw-decode";
      break;
    case nsIGfxInfo::FEATURE_VP9_HW_DECODE:
      name = BLOCKLIST_PREF_BRANCH "vp9.hw-decode";
      break;
    case nsIGfxInfo::FEATURE_GL_SWIZZLE:
      name = BLOCKLIST_PREF_BRANCH "gl.swizzle";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER_SCISSORED_CACHE_CLEARS:
      name = BLOCKLIST_PREF_BRANCH "webrender.scissored_cache_clears";
      break;
    case nsIGfxInfo::FEATURE_ALLOW_WEBGL_OUT_OF_PROCESS:
      name = BLOCKLIST_PREF_BRANCH "webgl.allow-oop";
      break;
    case nsIGfxInfo::FEATURE_THREADSAFE_GL:
      name = BLOCKLIST_PREF_BRANCH "gl.threadsafe";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER_OPTIMIZED_SHADERS:
      name = BLOCKLIST_PREF_BRANCH "webrender.optimized-shaders";
      break;
    case nsIGfxInfo::FEATURE_X11_EGL:
      name = BLOCKLIST_PREF_BRANCH "x11.egl";
      break;
    case nsIGfxInfo::FEATURE_DMABUF:
      name = BLOCKLIST_PREF_BRANCH "dmabuf";
      break;
    case nsIGfxInfo::FEATURE_WEBGPU:
      name = BLOCKLIST_PREF_BRANCH "webgpu";
      break;
    case nsIGfxInfo::FEATURE_VIDEO_OVERLAY:
      name = BLOCKLIST_PREF_BRANCH "video-overlay";
      break;
    case nsIGfxInfo::FEATURE_HW_DECODED_VIDEO_ZERO_COPY:
      name = BLOCKLIST_PREF_BRANCH "hw-video-zero-copy";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER_SHADER_CACHE:
      name = BLOCKLIST_PREF_BRANCH "webrender.program-binary-disk";
      break;
    case nsIGfxInfo::FEATURE_WEBRENDER_PARTIAL_PRESENT:
      name = BLOCKLIST_PREF_BRANCH "webrender.partial-present";
      break;
    case nsIGfxInfo::FEATURE_DMABUF_SURFACE_EXPORT:
      name = BLOCKLIST_PREF_BRANCH "dmabuf.surface-export";
      break;
    case nsIGfxInfo::FEATURE_REUSE_DECODER_DEVICE:
      name = BLOCKLIST_PREF_BRANCH "reuse-decoder-device";
      break;
    case nsIGfxInfo::FEATURE_BACKDROP_FILTER:
      name = BLOCKLIST_PREF_BRANCH "backdrop.filter";
      break;
    case nsIGfxInfo::FEATURE_ACCELERATED_CANVAS2D:
      name = BLOCKLIST_PREF_BRANCH "accelerated-canvas2d";
      break;
    case nsIGfxInfo::FEATURE_H264_HW_DECODE:
      name = BLOCKLIST_PREF_BRANCH "h264.hw-decode";
      break;
    case nsIGfxInfo::FEATURE_AV1_HW_DECODE:
      name = BLOCKLIST_PREF_BRANCH "av1.hw-decode";
      break;
    case nsIGfxInfo::FEATURE_VIDEO_SOFTWARE_OVERLAY:
      name = BLOCKLIST_PREF_BRANCH "video-software-overlay";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_USE_HARDWARE:
      name = BLOCKLIST_PREF_BRANCH "webgl-use-hardware";
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected nsIGfxInfo feature?!");
      break;
  }

  return name;
}

// Returns the value of the pref for the relevant feature in aValue.
// If the pref doesn't exist, aValue is not touched, and returns false.
static bool GetPrefValueForFeature(int32_t aFeature, int32_t& aValue,
                                   nsACString& aFailureId) {
  const char* prefname = GetPrefNameForFeature(aFeature);
  if (!prefname) return false;

  aValue = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  if (!NS_SUCCEEDED(Preferences::GetInt(prefname, &aValue))) {
    return false;
  }

  if (aValue == nsIGfxInfo::FEATURE_DENIED) {
    // We should never see the DENIED status with the downloadable blocklist.
    return false;
  }

  nsCString failureprefname(prefname);
  failureprefname += ".failureid";
  nsAutoCString failureValue;
  nsresult rv = Preferences::GetCString(failureprefname.get(), failureValue);
  if (NS_SUCCEEDED(rv)) {
    aFailureId = failureValue.get();
  } else {
    aFailureId = "FEATURE_FAILURE_BLOCKLIST_PREF";
  }

  return true;
}

static void SetPrefValueForFeature(int32_t aFeature, int32_t aValue,
                                   const nsACString& aFailureId) {
  const char* prefname = GetPrefNameForFeature(aFeature);
  if (!prefname) return;
  if (XRE_IsParentProcess()) {
    GfxInfoBase::sFeatureStatus = nullptr;
  }

  Preferences::SetInt(prefname, aValue);
  if (!aFailureId.IsEmpty()) {
    nsAutoCString failureprefname(prefname);
    failureprefname += ".failureid";
    Preferences::SetCString(failureprefname.get(), aFailureId);
  }
}

static void RemovePrefForFeature(int32_t aFeature) {
  const char* prefname = GetPrefNameForFeature(aFeature);
  if (!prefname) return;

  if (XRE_IsParentProcess()) {
    GfxInfoBase::sFeatureStatus = nullptr;
  }
  Preferences::ClearUser(prefname);
}

static bool GetPrefValueForDriverVersion(nsCString& aVersion) {
  return NS_SUCCEEDED(
      Preferences::GetCString(SUGGESTED_VERSION_PREF, aVersion));
}

static void SetPrefValueForDriverVersion(const nsAString& aVersion) {
  Preferences::SetString(SUGGESTED_VERSION_PREF, aVersion);
}

static void RemovePrefForDriverVersion() {
  Preferences::ClearUser(SUGGESTED_VERSION_PREF);
}

static OperatingSystem BlocklistOSToOperatingSystem(const nsAString& os) {
  if (os.EqualsLiteral("WINNT 6.1")) {
    return OperatingSystem::Windows7;
  }
  if (os.EqualsLiteral("WINNT 6.2")) {
    return OperatingSystem::Windows8;
  }
  if (os.EqualsLiteral("WINNT 6.3")) {
    return OperatingSystem::Windows8_1;
  }
  if (os.EqualsLiteral("WINNT 10.0")) {
    return OperatingSystem::Windows10;
  }
  if (os.EqualsLiteral("Linux")) {
    return OperatingSystem::Linux;
  }
  if (os.EqualsLiteral("Darwin 9")) {
    return OperatingSystem::OSX10_5;
  }
  if (os.EqualsLiteral("Darwin 10")) {
    return OperatingSystem::OSX10_6;
  }
  if (os.EqualsLiteral("Darwin 11")) {
    return OperatingSystem::OSX10_7;
  }
  if (os.EqualsLiteral("Darwin 12")) {
    return OperatingSystem::OSX10_8;
  }
  if (os.EqualsLiteral("Darwin 13")) {
    return OperatingSystem::OSX10_9;
  }
  if (os.EqualsLiteral("Darwin 14")) {
    return OperatingSystem::OSX10_10;
  }
  if (os.EqualsLiteral("Darwin 15")) {
    return OperatingSystem::OSX10_11;
  }
  if (os.EqualsLiteral("Darwin 16")) {
    return OperatingSystem::OSX10_12;
  }
  if (os.EqualsLiteral("Darwin 17")) {
    return OperatingSystem::OSX10_13;
  }
  if (os.EqualsLiteral("Darwin 18")) {
    return OperatingSystem::OSX10_14;
  }
  if (os.EqualsLiteral("Darwin 19")) {
    return OperatingSystem::OSX10_15;
  }
  if (os.EqualsLiteral("Darwin 20")) {
    return OperatingSystem::OSX11_0;
  }
  if (os.EqualsLiteral("Android")) {
    return OperatingSystem::Android;
    // For historical reasons, "All" in blocklist means "All Windows"
  }
  if (os.EqualsLiteral("All")) {
    return OperatingSystem::Windows;
  }
  if (os.EqualsLiteral("Darwin")) {
    return OperatingSystem::OSX;
  }

  return OperatingSystem::Unknown;
}

static GfxDeviceFamily* BlocklistDevicesToDeviceFamily(
    nsTArray<nsCString>& devices) {
  if (devices.Length() == 0) return nullptr;

  // For each device, get its device ID, and return a freshly-allocated
  // GfxDeviceFamily with the contents of that array.
  GfxDeviceFamily* deviceIds = new GfxDeviceFamily;

  for (uint32_t i = 0; i < devices.Length(); ++i) {
    // We make sure we don't add any "empty" device entries to the array, so
    // we don't need to check if devices[i] is empty.
    deviceIds->Append(NS_ConvertUTF8toUTF16(devices[i]));
  }

  return deviceIds;
}

static int32_t BlocklistFeatureToGfxFeature(const nsAString& aFeature) {
  MOZ_ASSERT(!aFeature.IsEmpty());
  if (aFeature.EqualsLiteral("DIRECT2D")) {
    return nsIGfxInfo::FEATURE_DIRECT2D;
  }
  if (aFeature.EqualsLiteral("DIRECT3D_9_LAYERS")) {
    return nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS;
  }
  if (aFeature.EqualsLiteral("DIRECT3D_10_LAYERS")) {
    return nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS;
  }
  if (aFeature.EqualsLiteral("DIRECT3D_10_1_LAYERS")) {
    return nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS;
  }
  if (aFeature.EqualsLiteral("DIRECT3D_11_LAYERS")) {
    return nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS;
  }
  if (aFeature.EqualsLiteral("DIRECT3D_11_ANGLE")) {
    return nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE;
  }
  if (aFeature.EqualsLiteral("HARDWARE_VIDEO_DECODING")) {
    return nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING;
  }
  if (aFeature.EqualsLiteral("OPENGL_LAYERS")) {
    return nsIGfxInfo::FEATURE_OPENGL_LAYERS;
  }
  if (aFeature.EqualsLiteral("WEBGL_OPENGL")) {
    return nsIGfxInfo::FEATURE_WEBGL_OPENGL;
  }
  if (aFeature.EqualsLiteral("WEBGL_ANGLE")) {
    return nsIGfxInfo::FEATURE_WEBGL_ANGLE;
  }
  if (aFeature.EqualsLiteral("WEBGL_MSAA")) {
    return nsIGfxInfo::UNUSED_FEATURE_WEBGL_MSAA;
  }
  if (aFeature.EqualsLiteral("STAGEFRIGHT")) {
    return nsIGfxInfo::FEATURE_STAGEFRIGHT;
  }
  if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION_ENCODE")) {
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE;
  }
  if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION_DECODE")) {
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE;
  }
  if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION_H264")) {
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_H264;
  }
  if (aFeature.EqualsLiteral("CANVAS2D_ACCELERATION")) {
    return nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION;
  }
  if (aFeature.EqualsLiteral("DX_INTEROP2")) {
    return nsIGfxInfo::FEATURE_DX_INTEROP2;
  }
  if (aFeature.EqualsLiteral("GPU_PROCESS")) {
    return nsIGfxInfo::FEATURE_GPU_PROCESS;
  }
  if (aFeature.EqualsLiteral("WEBGL2")) {
    return nsIGfxInfo::FEATURE_WEBGL2;
  }
  if (aFeature.EqualsLiteral("D3D11_KEYED_MUTEX")) {
    return nsIGfxInfo::FEATURE_D3D11_KEYED_MUTEX;
  }
  if (aFeature.EqualsLiteral("WEBRENDER")) {
    return nsIGfxInfo::FEATURE_WEBRENDER;
  }
  if (aFeature.EqualsLiteral("WEBRENDER_COMPOSITOR")) {
    return nsIGfxInfo::FEATURE_WEBRENDER_COMPOSITOR;
  }
  if (aFeature.EqualsLiteral("DX_NV12")) {
    return nsIGfxInfo::FEATURE_DX_NV12;
  }
  if (aFeature.EqualsLiteral("VP8_HW_DECODE")) {
    return nsIGfxInfo::FEATURE_VP8_HW_DECODE;
  }
  if (aFeature.EqualsLiteral("VP9_HW_DECODE")) {
    return nsIGfxInfo::FEATURE_VP9_HW_DECODE;
  }
  if (aFeature.EqualsLiteral("GL_SWIZZLE")) {
    return nsIGfxInfo::FEATURE_GL_SWIZZLE;
  }
  if (aFeature.EqualsLiteral("WEBRENDER_SCISSORED_CACHE_CLEARS")) {
    return nsIGfxInfo::FEATURE_WEBRENDER_SCISSORED_CACHE_CLEARS;
  }
  if (aFeature.EqualsLiteral("ALLOW_WEBGL_OUT_OF_PROCESS")) {
    return nsIGfxInfo::FEATURE_ALLOW_WEBGL_OUT_OF_PROCESS;
  }
  if (aFeature.EqualsLiteral("THREADSAFE_GL")) {
    return nsIGfxInfo::FEATURE_THREADSAFE_GL;
  }
  if (aFeature.EqualsLiteral("X11_EGL")) {
    return nsIGfxInfo::FEATURE_X11_EGL;
  }
  if (aFeature.EqualsLiteral("DMABUF")) {
    return nsIGfxInfo::FEATURE_DMABUF;
  }
  if (aFeature.EqualsLiteral("WEBGPU")) {
    return nsIGfxInfo::FEATURE_WEBGPU;
  }
  if (aFeature.EqualsLiteral("VIDEO_OVERLAY")) {
    return nsIGfxInfo::FEATURE_VIDEO_OVERLAY;
  }
  if (aFeature.EqualsLiteral("HW_DECODED_VIDEO_ZERO_COPY")) {
    return nsIGfxInfo::FEATURE_HW_DECODED_VIDEO_ZERO_COPY;
  }
  if (aFeature.EqualsLiteral("REUSE_DECODER_DEVICE")) {
    return nsIGfxInfo::FEATURE_REUSE_DECODER_DEVICE;
  }
  if (aFeature.EqualsLiteral("WEBRENDER_PARTIAL_PRESENT")) {
    return nsIGfxInfo::FEATURE_WEBRENDER_PARTIAL_PRESENT;
  }
  if (aFeature.EqualsLiteral("BACKDROP_FILTER")) {
    return nsIGfxInfo::FEATURE_BACKDROP_FILTER;
  }
  if (aFeature.EqualsLiteral("ACCELERATED_CANVAS2D")) {
    return nsIGfxInfo::FEATURE_ACCELERATED_CANVAS2D;
  }
  if (aFeature.EqualsLiteral("ALL")) {
    return GfxDriverInfo::allFeatures;
  }
  if (aFeature.EqualsLiteral("OPTIONAL")) {
    return GfxDriverInfo::optionalFeatures;
  }

  // If we don't recognize the feature, it may be new, and something
  // this version doesn't understand.  So, nothing to do.  This is
  // different from feature not being specified at all, in which case
  // this method should not get called and we should continue with the
  // "optional features" blocklisting.
  return 0;
}

static int32_t BlocklistFeatureStatusToGfxFeatureStatus(
    const nsAString& aStatus) {
  if (aStatus.EqualsLiteral("STATUS_OK")) {
    return nsIGfxInfo::FEATURE_STATUS_OK;
  }
  if (aStatus.EqualsLiteral("BLOCKED_DRIVER_VERSION")) {
    return nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
  }
  if (aStatus.EqualsLiteral("BLOCKED_DEVICE")) {
    return nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  }
  if (aStatus.EqualsLiteral("DISCOURAGED")) {
    return nsIGfxInfo::FEATURE_DISCOURAGED;
  }
  if (aStatus.EqualsLiteral("BLOCKED_OS_VERSION")) {
    return nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
  }
  if (aStatus.EqualsLiteral("DENIED")) {
    return nsIGfxInfo::FEATURE_DENIED;
  }
  if (aStatus.EqualsLiteral("ALLOW_QUALIFIED")) {
    return nsIGfxInfo::FEATURE_ALLOW_QUALIFIED;
  }
  if (aStatus.EqualsLiteral("ALLOW_ALWAYS")) {
    return nsIGfxInfo::FEATURE_ALLOW_ALWAYS;
  }

  // Do not allow it to set STATUS_UNKNOWN.  Also, we are not
  // expecting the "mismatch" status showing up here.

  return nsIGfxInfo::FEATURE_STATUS_OK;
}

static VersionComparisonOp BlocklistComparatorToComparisonOp(
    const nsAString& op) {
  if (op.EqualsLiteral("LESS_THAN")) {
    return DRIVER_LESS_THAN;
  }
  if (op.EqualsLiteral("BUILD_ID_LESS_THAN")) {
    return DRIVER_BUILD_ID_LESS_THAN;
  }
  if (op.EqualsLiteral("LESS_THAN_OR_EQUAL")) {
    return DRIVER_LESS_THAN_OR_EQUAL;
  }
  if (op.EqualsLiteral("BUILD_ID_LESS_THAN_OR_EQUAL")) {
    return DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL;
  }
  if (op.EqualsLiteral("GREATER_THAN")) {
    return DRIVER_GREATER_THAN;
  }
  if (op.EqualsLiteral("GREATER_THAN_OR_EQUAL")) {
    return DRIVER_GREATER_THAN_OR_EQUAL;
  }
  if (op.EqualsLiteral("EQUAL")) {
    return DRIVER_EQUAL;
  }
  if (op.EqualsLiteral("NOT_EQUAL")) {
    return DRIVER_NOT_EQUAL;
  }
  if (op.EqualsLiteral("BETWEEN_EXCLUSIVE")) {
    return DRIVER_BETWEEN_EXCLUSIVE;
  }
  if (op.EqualsLiteral("BETWEEN_INCLUSIVE")) {
    return DRIVER_BETWEEN_INCLUSIVE;
  }
  if (op.EqualsLiteral("BETWEEN_INCLUSIVE_START")) {
    return DRIVER_BETWEEN_INCLUSIVE_START;
  }

  return DRIVER_COMPARISON_IGNORED;
}

/*
  Deserialize Blocklist entries from string.
  e.g:
  os:WINNT 6.0\tvendor:0x8086\tdevices:0x2582,0x2782\tfeature:DIRECT3D_10_LAYERS\tfeatureStatus:BLOCKED_DRIVER_VERSION\tdriverVersion:8.52.322.2202\tdriverVersionComparator:LESS_THAN_OR_EQUAL
*/
static bool BlocklistEntryToDriverInfo(const nsACString& aBlocklistEntry,
                                       GfxDriverInfo& aDriverInfo) {
  // If we get an application version to be zero, something is not working
  // and we are not going to bother checking the blocklist versions.
  // See TestGfxWidgets.cpp for how version comparison works.
  // <versionRange minVersion="42.0a1" maxVersion="45.0"></versionRange>
  static mozilla::Version zeroV("0");
  static mozilla::Version appV(GfxInfoBase::GetApplicationVersion().get());
  if (appV <= zeroV) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Invalid application version "
        << GfxInfoBase::GetApplicationVersion().get();
  }

  aDriverInfo.mRuleId = "FEATURE_FAILURE_DL_BLOCKLIST_NO_ID"_ns;

  for (const auto& keyValue : aBlocklistEntry.Split('\t')) {
    nsTArray<nsCString> splitted;
    ParseString(keyValue, ':', splitted);
    if (splitted.Length() != 2) {
      // If we don't recognize the input data, we do not want to proceed.
      gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false))
          << "Unrecognized data " << nsCString(keyValue).get();
      return false;
    }
    const nsCString& key = splitted[0];
    const nsCString& value = splitted[1];
    NS_ConvertUTF8toUTF16 dataValue(value);

    if (value.Length() == 0) {
      // Safety check for empty values.
      gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false))
          << "Empty value for " << key.get();
      return false;
    }

    if (key.EqualsLiteral("blockID")) {
      nsCString blockIdStr = "FEATURE_FAILURE_DL_BLOCKLIST_"_ns + value;
      aDriverInfo.mRuleId = blockIdStr.get();
    } else if (key.EqualsLiteral("os")) {
      aDriverInfo.mOperatingSystem = BlocklistOSToOperatingSystem(dataValue);
    } else if (key.EqualsLiteral("osversion")) {
      aDriverInfo.mOperatingSystemVersion = strtoul(value.get(), nullptr, 10);
    } else if (key.EqualsLiteral("windowProtocol")) {
      aDriverInfo.mWindowProtocol = dataValue;
    } else if (key.EqualsLiteral("vendor")) {
      aDriverInfo.mAdapterVendor = dataValue;
    } else if (key.EqualsLiteral("driverVendor")) {
      aDriverInfo.mDriverVendor = dataValue;
    } else if (key.EqualsLiteral("feature")) {
      aDriverInfo.mFeature = BlocklistFeatureToGfxFeature(dataValue);
      if (aDriverInfo.mFeature == 0) {
        // If we don't recognize the feature, we do not want to proceed.
        gfxWarning() << "Unrecognized feature " << value.get();
        return false;
      }
    } else if (key.EqualsLiteral("featureStatus")) {
      aDriverInfo.mFeatureStatus =
          BlocklistFeatureStatusToGfxFeatureStatus(dataValue);
    } else if (key.EqualsLiteral("driverVersion")) {
      uint64_t version;
      if (ParseDriverVersion(dataValue, &version))
        aDriverInfo.mDriverVersion = version;
    } else if (key.EqualsLiteral("driverVersionMax")) {
      uint64_t version;
      if (ParseDriverVersion(dataValue, &version))
        aDriverInfo.mDriverVersionMax = version;
    } else if (key.EqualsLiteral("driverVersionComparator")) {
      aDriverInfo.mComparisonOp = BlocklistComparatorToComparisonOp(dataValue);
    } else if (key.EqualsLiteral("model")) {
      aDriverInfo.mModel = dataValue;
    } else if (key.EqualsLiteral("product")) {
      aDriverInfo.mProduct = dataValue;
    } else if (key.EqualsLiteral("manufacturer")) {
      aDriverInfo.mManufacturer = dataValue;
    } else if (key.EqualsLiteral("hardware")) {
      aDriverInfo.mHardware = dataValue;
    } else if (key.EqualsLiteral("versionRange")) {
      nsTArray<nsCString> versionRange;
      ParseString(value, ',', versionRange);
      if (versionRange.Length() != 2) {
        gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false))
            << "Unrecognized versionRange " << value.get();
        return false;
      }
      const nsCString& minValue = versionRange[0];
      const nsCString& maxValue = versionRange[1];

      mozilla::Version minV(minValue.get());
      mozilla::Version maxV(maxValue.get());

      if (minV > zeroV && !(appV >= minV)) {
        // The version of the application is less than the minimal version
        // this blocklist entry applies to, so we can just ignore it by
        // returning false and letting the caller deal with it.
        return false;
      }
      if (maxV > zeroV && !(appV <= maxV)) {
        // The version of the application is more than the maximal version
        // this blocklist entry applies to, so we can just ignore it by
        // returning false and letting the caller deal with it.
        return false;
      }
    } else if (key.EqualsLiteral("devices")) {
      nsTArray<nsCString> devices;
      ParseString(value, ',', devices);
      GfxDeviceFamily* deviceIds = BlocklistDevicesToDeviceFamily(devices);
      if (deviceIds) {
        // Get GfxDriverInfo to adopt the devices array we created.
        aDriverInfo.mDeleteDevices = true;
        aDriverInfo.mDevices = deviceIds;
      }
    }
    // We explicitly ignore unknown elements.
  }

  return true;
}

NS_IMETHODIMP
GfxInfoBase::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  if (strcmp(aTopic, "blocklist-data-gfxItems") == 0) {
    nsTArray<GfxDriverInfo> driverInfo;
    NS_ConvertUTF16toUTF8 utf8Data(aData);

    for (const auto& blocklistEntry : utf8Data.Split('\n')) {
      GfxDriverInfo di;
      if (BlocklistEntryToDriverInfo(blocklistEntry, di)) {
        // XXX Changing this to driverInfo.AppendElement(di) causes leaks.
        // Probably some non-standard semantics of the copy/move operations?
        *driverInfo.AppendElement() = di;
        // Prevent di falling out of scope from destroying the devices.
        di.mDeleteDevices = false;
      } else {
        driverInfo.AppendElement();
      }
    }

    EvaluateDownloadedBlocklist(driverInfo);
  }

  return NS_OK;
}

GfxInfoBase::GfxInfoBase() : mScreenPixels(INT64_MAX), mMutex("GfxInfoBase") {}

GfxInfoBase::~GfxInfoBase() = default;

nsresult GfxInfoBase::Init() {
  InitGfxDriverInfoShutdownObserver();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "blocklist-data-gfxItems", true);
  }

  return NS_OK;
}

void GfxInfoBase::GetData() {
  if (mScreenPixels != INT64_MAX) {
    // Already initialized.
    return;
  }

  ScreenManager::GetSingleton().GetTotalScreenPixels(&mScreenPixels);
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureStatus(int32_t aFeature, nsACString& aFailureId,
                              int32_t* aStatus) {
  // Ignore the gfx.blocklist.all pref on release and beta.
#if defined(RELEASE_OR_BETA)
  int32_t blocklistAll = 0;
#else
  int32_t blocklistAll = StaticPrefs::gfx_blocklist_all_AtStartup();
#endif
  if (blocklistAll > 0) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Forcing blocklisting all features";
    *aStatus = FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_BLOCK_ALL";
    return NS_OK;
  }

  if (blocklistAll < 0) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Ignoring any feature blocklisting.";
    *aStatus = FEATURE_STATUS_OK;
    return NS_OK;
  }

  // This is how we evaluate the downloadable blocklist. If there is no pref,
  // then we will fallback to checking the static blocklist.
  if (GetPrefValueForFeature(aFeature, *aStatus, aFailureId)) {
    return NS_OK;
  }

  if (XRE_IsContentProcess() || XRE_IsGPUProcess()) {
    // Use the cached data received from the parent process.
    MOZ_ASSERT(sFeatureStatus);
    bool success = false;
    for (const auto& fs : *sFeatureStatus) {
      if (fs.feature() == aFeature) {
        aFailureId = fs.failureId();
        *aStatus = fs.status();
        success = true;
        break;
      }
    }
    return success ? NS_OK : NS_ERROR_FAILURE;
  }

  nsString version;
  nsTArray<GfxDriverInfo> driverInfo;
  nsresult rv =
      GetFeatureStatusImpl(aFeature, aStatus, version, driverInfo, aFailureId);
  return rv;
}

nsTArray<gfx::GfxInfoFeatureStatus> GfxInfoBase::GetAllFeatures() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  if (!sFeatureStatus) {
    InitFeatureStatus(new nsTArray<gfx::GfxInfoFeatureStatus>());
    for (int32_t i = 1; i <= nsIGfxInfo::FEATURE_MAX_VALUE; ++i) {
      int32_t status = 0;
      nsAutoCString failureId;
      GetFeatureStatus(i, failureId, &status);
      gfx::GfxInfoFeatureStatus gfxFeatureStatus;
      gfxFeatureStatus.feature() = i;
      gfxFeatureStatus.status() = status;
      gfxFeatureStatus.failureId() = failureId;
      sFeatureStatus->AppendElement(gfxFeatureStatus);
    }
  }

  nsTArray<gfx::GfxInfoFeatureStatus> features;
  for (const auto& status : *sFeatureStatus) {
    gfx::GfxInfoFeatureStatus copy = status;
    features.AppendElement(copy);
  }
  return features;
}

inline bool MatchingAllowStatus(int32_t aStatus) {
  switch (aStatus) {
    case nsIGfxInfo::FEATURE_ALLOW_ALWAYS:
    case nsIGfxInfo::FEATURE_ALLOW_QUALIFIED:
      return true;
    default:
      return false;
  }
}

// Matching OS go somewhat beyond the simple equality check because of the
// "All Windows" and "All OS X" variations.
//
// aBlockedOS is describing the system(s) we are trying to block.
// aSystemOS is describing the system we are running on.
//
// aSystemOS should not be "Windows" or "OSX" - it should be set to
// a particular version instead.
// However, it is valid for aBlockedOS to be one of those generic values,
// as we could be blocking all of the versions.
inline bool MatchingOperatingSystems(OperatingSystem aBlockedOS,
                                     OperatingSystem aSystemOS,
                                     uint32_t aSystemOSBuild) {
  MOZ_ASSERT(aSystemOS != OperatingSystem::Windows &&
             aSystemOS != OperatingSystem::OSX);

  // If the block entry OS is unknown, it doesn't match
  if (aBlockedOS == OperatingSystem::Unknown) {
    return false;
  }

#if defined(XP_WIN)
  if (aBlockedOS == OperatingSystem::Windows) {
    // We do want even "unknown" aSystemOS to fall under "all windows"
    return true;
  }

  constexpr uint32_t kMinWin10BuildNumber = 18362;
  if (aBlockedOS == OperatingSystem::RecentWindows10 &&
      aSystemOS == OperatingSystem::Windows10) {
    // For allowlist purposes, we sometimes want to restrict to only recent
    // versions of Windows 10. This is a bit of a kludge but easier than adding
    // complicated blocklist infrastructure for build ID comparisons like driver
    // versions.
    return aSystemOSBuild >= kMinWin10BuildNumber;
  }

  if (aBlockedOS == OperatingSystem::NotRecentWindows10) {
    if (aSystemOS == OperatingSystem::Windows10) {
      return aSystemOSBuild < kMinWin10BuildNumber;
    } else {
      return true;
    }
  }
#endif

#if defined(XP_MACOSX)
  if (aBlockedOS == OperatingSystem::OSX) {
    // We do want even "unknown" aSystemOS to fall under "all OS X"
    return true;
  }
#endif

  return aSystemOS == aBlockedOS;
}

inline bool MatchingBattery(BatteryStatus aBatteryStatus, bool aHasBattery) {
  switch (aBatteryStatus) {
    case BatteryStatus::All:
      return true;
    case BatteryStatus::None:
      return !aHasBattery;
    case BatteryStatus::Present:
      return aHasBattery;
  }

  MOZ_ASSERT_UNREACHABLE("bad battery status");
  return false;
}

inline bool MatchingScreenSize(ScreenSizeStatus aScreenStatus,
                               int64_t aScreenPixels) {
  constexpr int64_t kMaxSmallPixels = 2304000;   // 1920x1200
  constexpr int64_t kMaxMediumPixels = 4953600;  // 3440x1440

  switch (aScreenStatus) {
    case ScreenSizeStatus::All:
      return true;
    case ScreenSizeStatus::Small:
      return aScreenPixels <= kMaxSmallPixels;
    case ScreenSizeStatus::SmallAndMedium:
      return aScreenPixels <= kMaxMediumPixels;
    case ScreenSizeStatus::Medium:
      return aScreenPixels > kMaxSmallPixels &&
             aScreenPixels <= kMaxMediumPixels;
    case ScreenSizeStatus::MediumAndLarge:
      return aScreenPixels > kMaxSmallPixels;
    case ScreenSizeStatus::Large:
      return aScreenPixels > kMaxMediumPixels;
  }

  MOZ_ASSERT_UNREACHABLE("bad screen status");
  return false;
}

int32_t GfxInfoBase::FindBlocklistedDeviceInList(
    const nsTArray<GfxDriverInfo>& info, nsAString& aSuggestedVersion,
    int32_t aFeature, nsACString& aFailureId, OperatingSystem os,
    bool aForAllowing) {
  int32_t status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  // Some properties are not available on all platforms.
  nsAutoString windowProtocol;
  nsresult rv = GetWindowProtocol(windowProtocol);
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    return 0;
  }

  bool hasBattery = false;
  rv = GetHasBattery(&hasBattery);
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    return 0;
  }

  uint32_t osBuild = OperatingSystemBuild();

  // Get the adapters once then reuse below
  nsAutoString adapterVendorID[2];
  nsAutoString adapterDeviceID[2];
  nsAutoString adapterDriverVendor[2];
  nsAutoString adapterDriverVersionString[2];
  bool adapterInfoFailed[2];

  adapterInfoFailed[0] =
      (NS_FAILED(GetAdapterVendorID(adapterVendorID[0])) ||
       NS_FAILED(GetAdapterDeviceID(adapterDeviceID[0])) ||
       NS_FAILED(GetAdapterDriverVendor(adapterDriverVendor[0])) ||
       NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString[0])));
  adapterInfoFailed[1] =
      (NS_FAILED(GetAdapterVendorID2(adapterVendorID[1])) ||
       NS_FAILED(GetAdapterDeviceID2(adapterDeviceID[1])) ||
       NS_FAILED(GetAdapterDriverVendor2(adapterDriverVendor[1])) ||
       NS_FAILED(GetAdapterDriverVersion2(adapterDriverVersionString[1])));
  // No point in going on if we don't have adapter info
  if (adapterInfoFailed[0] && adapterInfoFailed[1]) {
    return 0;
  }

#if defined(XP_WIN) || defined(ANDROID) || defined(MOZ_WIDGET_GTK)
  uint64_t driverVersion[2] = {0, 0};
  if (!adapterInfoFailed[0]) {
    ParseDriverVersion(adapterDriverVersionString[0], &driverVersion[0]);
  }
  if (!adapterInfoFailed[1]) {
    ParseDriverVersion(adapterDriverVersionString[1], &driverVersion[1]);
  }
#endif

  uint32_t i = 0;
  for (; i < info.Length(); i++) {
    // If the status is FEATURE_ALLOW_*, then it is for the allowlist, not
    // blocklisting. Only consider entries for our search mode.
    if (MatchingAllowStatus(info[i].mFeatureStatus) != aForAllowing) {
      continue;
    }

    // If we don't have the info for this GPU, no need to check further.
    // It is unclear that we would ever have a mixture of 1st and 2nd
    // GPU, but leaving the code in for that possibility for now.
    // (Actually, currently mGpu2 will never be true, so this can
    // be optimized out.)
    uint32_t infoIndex = info[i].mGpu2 ? 1 : 0;
    if (adapterInfoFailed[infoIndex]) {
      continue;
    }

    // Do the operating system check first, no point in getting the driver
    // info if we won't need to use it.
    if (!MatchingOperatingSystems(info[i].mOperatingSystem, os, osBuild)) {
      continue;
    }

    if (info[i].mOperatingSystemVersion &&
        info[i].mOperatingSystemVersion != OperatingSystemVersion()) {
      continue;
    }

    if (!MatchingBattery(info[i].mBattery, hasBattery)) {
      continue;
    }

    if (!MatchingScreenSize(info[i].mScreen, mScreenPixels)) {
      continue;
    }

    if (!DoesWindowProtocolMatch(info[i].mWindowProtocol, windowProtocol)) {
      continue;
    }

    if (!DoesVendorMatch(info[i].mAdapterVendor, adapterVendorID[infoIndex])) {
      continue;
    }

    if (!DoesDriverVendorMatch(info[i].mDriverVendor,
                               adapterDriverVendor[infoIndex])) {
      continue;
    }

    if (info[i].mDevices && !info[i].mDevices->IsEmpty()) {
      nsresult rv = info[i].mDevices->Contains(adapterDeviceID[infoIndex]);
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // Not found
        continue;
      }
      if (rv != NS_OK) {
        // Failed to search, allowlist should not match, blocklist should match
        // for safety reasons
        if (aForAllowing) {
          continue;
        }
        break;
      }
    }

    bool match = false;

    if (!info[i].mHardware.IsEmpty() && !info[i].mHardware.Equals(Hardware())) {
      continue;
    }
    if (!info[i].mModel.IsEmpty() && !info[i].mModel.Equals(Model())) {
      continue;
    }
    if (!info[i].mProduct.IsEmpty() && !info[i].mProduct.Equals(Product())) {
      continue;
    }
    if (!info[i].mManufacturer.IsEmpty() &&
        !info[i].mManufacturer.Equals(Manufacturer())) {
      continue;
    }

#if defined(XP_WIN) || defined(ANDROID) || defined(MOZ_WIDGET_GTK)
    switch (info[i].mComparisonOp) {
      case DRIVER_LESS_THAN:
        match = driverVersion[infoIndex] < info[i].mDriverVersion;
        break;
      case DRIVER_BUILD_ID_LESS_THAN:
        match = (driverVersion[infoIndex] & 0xFFFF) < info[i].mDriverVersion;
        break;
      case DRIVER_LESS_THAN_OR_EQUAL:
        match = driverVersion[infoIndex] <= info[i].mDriverVersion;
        break;
      case DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL:
        match = (driverVersion[infoIndex] & 0xFFFF) <= info[i].mDriverVersion;
        break;
      case DRIVER_GREATER_THAN:
        match = driverVersion[infoIndex] > info[i].mDriverVersion;
        break;
      case DRIVER_GREATER_THAN_OR_EQUAL:
        match = driverVersion[infoIndex] >= info[i].mDriverVersion;
        break;
      case DRIVER_EQUAL:
        match = driverVersion[infoIndex] == info[i].mDriverVersion;
        break;
      case DRIVER_NOT_EQUAL:
        match = driverVersion[infoIndex] != info[i].mDriverVersion;
        break;
      case DRIVER_BETWEEN_EXCLUSIVE:
        match = driverVersion[infoIndex] > info[i].mDriverVersion &&
                driverVersion[infoIndex] < info[i].mDriverVersionMax;
        break;
      case DRIVER_BETWEEN_INCLUSIVE:
        match = driverVersion[infoIndex] >= info[i].mDriverVersion &&
                driverVersion[infoIndex] <= info[i].mDriverVersionMax;
        break;
      case DRIVER_BETWEEN_INCLUSIVE_START:
        match = driverVersion[infoIndex] >= info[i].mDriverVersion &&
                driverVersion[infoIndex] < info[i].mDriverVersionMax;
        break;
      case DRIVER_COMPARISON_IGNORED:
        // We don't have a comparison op, so we match everything.
        match = true;
        break;
      default:
        NS_WARNING("Bogus op in GfxDriverInfo");
        break;
    }
#else
    // We don't care what driver version it was. We only check OS version and if
    // the device matches.
    match = true;
#endif

    if (match || info[i].mDriverVersion == GfxDriverInfo::allDriverVersions) {
      if (info[i].mFeature == GfxDriverInfo::allFeatures ||
          info[i].mFeature == aFeature ||
          (info[i].mFeature == GfxDriverInfo::optionalFeatures &&
           OnlyAllowFeatureOnKnownConfig(aFeature))) {
        status = info[i].mFeatureStatus;
        if (!info[i].mRuleId.IsEmpty()) {
          aFailureId = info[i].mRuleId.get();
        } else {
          aFailureId = "FEATURE_FAILURE_DL_BLOCKLIST_NO_ID";
        }
        break;
      }
    }
  }

#if defined(XP_WIN)
  // As a very special case, we block D2D on machines with an NVidia 310M GPU
  // as either the primary or secondary adapter.  D2D is also blocked when the
  // NV 310M is the primary adapter (using the standard blocklisting mechanism).
  // If the primary GPU already matched something in the blocklist then we
  // ignore this special rule.  See bug 1008759.
  if (status == nsIGfxInfo::FEATURE_STATUS_UNKNOWN &&
      (aFeature == nsIGfxInfo::FEATURE_DIRECT2D)) {
    if (!adapterInfoFailed[1]) {
      nsAString& nvVendorID =
          (nsAString&)GfxDriverInfo::GetDeviceVendor(DeviceVendor::NVIDIA);
      const nsString nv310mDeviceId = u"0x0A70"_ns;
      if (nvVendorID.Equals(adapterVendorID[1],
                            nsCaseInsensitiveStringComparator) &&
          nv310mDeviceId.Equals(adapterDeviceID[1],
                                nsCaseInsensitiveStringComparator)) {
        status = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        aFailureId = "FEATURE_FAILURE_D2D_NV310M_BLOCK";
      }
    }
  }

  // Depends on Windows driver versioning. We don't pass a GfxDriverInfo object
  // back to the Windows handler, so we must handle this here.
  if (status == FEATURE_BLOCKED_DRIVER_VERSION) {
    if (info[i].mSuggestedVersion) {
      aSuggestedVersion.AppendPrintf("%s", info[i].mSuggestedVersion);
    } else if (info[i].mComparisonOp == DRIVER_LESS_THAN &&
               info[i].mDriverVersion != GfxDriverInfo::allDriverVersions) {
      aSuggestedVersion.AppendPrintf(
          "%lld.%lld.%lld.%lld",
          (info[i].mDriverVersion & 0xffff000000000000) >> 48,
          (info[i].mDriverVersion & 0x0000ffff00000000) >> 32,
          (info[i].mDriverVersion & 0x00000000ffff0000) >> 16,
          (info[i].mDriverVersion & 0x000000000000ffff));
    }
  }
#endif

  return status;
}

void GfxInfoBase::SetFeatureStatus(nsTArray<gfx::GfxInfoFeatureStatus>&& aFS) {
  MOZ_ASSERT(!sFeatureStatus);
  InitFeatureStatus(new nsTArray<gfx::GfxInfoFeatureStatus>(std::move(aFS)));
}

bool GfxInfoBase::DoesWindowProtocolMatch(
    const nsAString& aBlocklistWindowProtocol,
    const nsAString& aWindowProtocol) {
  return aBlocklistWindowProtocol.Equals(aWindowProtocol,
                                         nsCaseInsensitiveStringComparator) ||
         aBlocklistWindowProtocol.Equals(
             GfxDriverInfo::GetWindowProtocol(WindowProtocol::All),
             nsCaseInsensitiveStringComparator);
}

bool GfxInfoBase::DoesVendorMatch(const nsAString& aBlocklistVendor,
                                  const nsAString& aAdapterVendor) {
  return aBlocklistVendor.Equals(aAdapterVendor,
                                 nsCaseInsensitiveStringComparator) ||
         aBlocklistVendor.Equals(
             GfxDriverInfo::GetDeviceVendor(DeviceVendor::All),
             nsCaseInsensitiveStringComparator);
}

bool GfxInfoBase::DoesDriverVendorMatch(const nsAString& aBlocklistVendor,
                                        const nsAString& aDriverVendor) {
  return aBlocklistVendor.Equals(aDriverVendor,
                                 nsCaseInsensitiveStringComparator) ||
         aBlocklistVendor.Equals(
             GfxDriverInfo::GetDriverVendor(DriverVendor::All),
             nsCaseInsensitiveStringComparator);
}

bool GfxInfoBase::IsFeatureAllowlisted(int32_t aFeature) const {
  return aFeature == nsIGfxInfo::FEATURE_VIDEO_OVERLAY ||
         aFeature == nsIGfxInfo::FEATURE_HW_DECODED_VIDEO_ZERO_COPY;
}

nsresult GfxInfoBase::GetFeatureStatusImpl(
    int32_t aFeature, int32_t* aStatus, nsAString& aSuggestedVersion,
    const nsTArray<GfxDriverInfo>& aDriverInfo, nsACString& aFailureId,
    OperatingSystem* aOS /* = nullptr */) {
  if (aFeature <= 0) {
    gfxWarning() << "Invalid feature <= 0";
    return NS_OK;
  }

  if (*aStatus != nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    // Terminate now with the status determined by the derived type (OS-specific
    // code).
    return NS_OK;
  }

  if (sShutdownOccurred) {
    // This is futile; we've already commenced shutdown and our blocklists have
    // been deleted. We may want to look into resurrecting the blocklist instead
    // but for now, just don't even go there.
    return NS_OK;
  }

  // Ensure any additional initialization required is complete.
  GetData();

  // If an operating system was provided by the derived GetFeatureStatusImpl,
  // grab it here. Otherwise, the OS is unknown.
  OperatingSystem os = (aOS ? *aOS : OperatingSystem::Unknown);

  nsAutoString adapterVendorID;
  nsAutoString adapterDeviceID;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(adapterVendorID)) ||
      NS_FAILED(GetAdapterDeviceID(adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString))) {
    if (OnlyAllowFeatureOnKnownConfig(aFeature)) {
      aFailureId = "FEATURE_FAILURE_CANT_RESOLVE_ADAPTER";
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    } else {
      *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
    }
    return NS_OK;
  }

  // We only check either the given blocklist, or the static list, as given.
  int32_t status;
  if (aDriverInfo.Length()) {
    status =
        FindBlocklistedDeviceInList(aDriverInfo, aSuggestedVersion, aFeature,
                                    aFailureId, os, /* aForAllowing */ false);
  } else {
    if (!sDriverInfo) {
      sDriverInfo = new nsTArray<GfxDriverInfo>();
    }
    status = FindBlocklistedDeviceInList(GetGfxDriverInfo(), aSuggestedVersion,
                                         aFeature, aFailureId, os,
                                         /* aForAllowing */ false);
  }

  if (status == nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    if (IsFeatureAllowlisted(aFeature)) {
      // This feature is actually using the allowlist; that means after we pass
      // the blocklist to prevent us explicitly from getting the feature, we now
      // need to check the allowlist to ensure we are allowed to get it in the
      // first place.
      if (aDriverInfo.Length()) {
        status = FindBlocklistedDeviceInList(aDriverInfo, aSuggestedVersion,
                                             aFeature, aFailureId, os,
                                             /* aForAllowing */ true);
      } else {
        status = FindBlocklistedDeviceInList(
            GetGfxDriverInfo(), aSuggestedVersion, aFeature, aFailureId, os,
            /* aForAllowing */ true);
      }

      if (status == nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
        status = nsIGfxInfo::FEATURE_DENIED;
      }
    } else {
      // It's now done being processed. It's safe to set the status to
      // STATUS_OK.
      status = nsIGfxInfo::FEATURE_STATUS_OK;
    }
  }

  *aStatus = status;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureSuggestedDriverVersion(int32_t aFeature,
                                              nsAString& aVersion) {
  nsCString version;
  if (GetPrefValueForDriverVersion(version)) {
    aVersion = NS_ConvertASCIItoUTF16(version);
    return NS_OK;
  }

  int32_t status;
  nsCString discardFailureId;
  nsTArray<GfxDriverInfo> driverInfo;
  return GetFeatureStatusImpl(aFeature, &status, aVersion, driverInfo,
                              discardFailureId);
}

void GfxInfoBase::EvaluateDownloadedBlocklist(
    nsTArray<GfxDriverInfo>& aDriverInfo) {
  // If the list is empty, then we don't actually want to call
  // GetFeatureStatusImpl since we will use the static list instead. In that
  // case, all we want to do is make sure the pref is removed.
  if (aDriverInfo.IsEmpty()) {
    gfxCriticalNoteOnce << "Evaluate empty downloaded blocklist";
    return;
  }

  OperatingSystem os = GetOperatingSystem();

  // For every feature we know about, we evaluate whether this blocklist has a
  // non-STATUS_OK status. If it does, we set the pref we evaluate in
  // GetFeatureStatus above, so we don't need to hold on to this blocklist
  // anywhere permanent.
  for (int feature = 1; feature <= nsIGfxInfo::FEATURE_MAX_VALUE; ++feature) {
    int32_t status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
    nsCString failureId;
    nsAutoString suggestedVersion;

    // Note that we are careful to call the base class method since we only want
    // to evaluate the downloadable blocklist for these prefs.
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(GfxInfoBase::GetFeatureStatusImpl(
        feature, &status, suggestedVersion, aDriverInfo, failureId, &os)));

    switch (status) {
      default:
        MOZ_FALLTHROUGH_ASSERT("Unhandled feature status!");
      case nsIGfxInfo::FEATURE_STATUS_UNKNOWN:
        // This may be returned during shutdown or for invalid features.
      case nsIGfxInfo::FEATURE_ALLOW_ALWAYS:
      case nsIGfxInfo::FEATURE_ALLOW_QUALIFIED:
      case nsIGfxInfo::FEATURE_DENIED:
        // We cannot use the downloadable blocklist to control the allowlist.
        // If a feature is allowlisted, then we should also ignore DENIED
        // statuses from GetFeatureStatusImpl because we don't check the
        // static list when and this is an expected value. If we wish to
        // override the allowlist, it is as simple as creating a normal
        // blocklist rule with a BLOCKED* status code.
      case nsIGfxInfo::FEATURE_STATUS_OK:
        RemovePrefForFeature(feature);
        break;

      case nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION:
        if (!suggestedVersion.IsEmpty()) {
          SetPrefValueForDriverVersion(suggestedVersion);
        } else {
          RemovePrefForDriverVersion();
        }
        [[fallthrough]];

      case nsIGfxInfo::FEATURE_BLOCKED_MISMATCHED_VERSION:
      case nsIGfxInfo::FEATURE_BLOCKED_DEVICE:
      case nsIGfxInfo::FEATURE_DISCOURAGED:
      case nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION:
      case nsIGfxInfo::FEATURE_BLOCKED_PLATFORM_TEST:
        SetPrefValueForFeature(feature, status, failureId);
        break;
    }
  }
}

NS_IMETHODIMP_(void)
GfxInfoBase::LogFailure(const nsACString& failure) {
  // gfxCriticalError has a mutex lock of its own, so we may not actually
  // need this lock. ::GetFailures() accesses the data but the LogForwarder
  // will not return the copy of the logs unless it can get the same lock
  // that gfxCriticalError uses.  Still, that is so much of an implementation
  // detail that it's nicer to just add an extra lock here and in
  // ::GetFailures()
  MutexAutoLock lock(mMutex);

  // By default, gfxCriticalError asserts; make it not assert in this case.
  gfxCriticalError(CriticalLog::DefaultOptions(false))
      << "(LF) " << failure.BeginReading();
}

NS_IMETHODIMP GfxInfoBase::GetFailures(nsTArray<int32_t>& indices,
                                       nsTArray<nsCString>& failures) {
  MutexAutoLock lock(mMutex);

  LogForwarder* logForwarder = Factory::GetLogForwarder();
  if (!logForwarder) {
    return NS_ERROR_UNEXPECTED;
  }

  // There are two string copies in this method, starting with this one. We are
  // assuming this is not a big deal, as the size of the array should be small
  // and the strings in it should be small as well (the error messages in the
  // code.)  The second copy happens with the AppendElement() calls.
  // Technically, we don't need the mutex lock after the StringVectorCopy()
  // call.
  LoggingRecord loggedStrings = logForwarder->LoggingRecordCopy();
  LoggingRecord::const_iterator it;
  for (it = loggedStrings.begin(); it != loggedStrings.end(); ++it) {
    failures.AppendElement(nsDependentCSubstring(std::get<1>(*it).c_str(),
                                                 std::get<1>(*it).size()));
    indices.AppendElement(std::get<0>(*it));
  }

  return NS_OK;
}

nsTArray<GfxInfoCollectorBase*>* sCollectors;

static void InitCollectors() {
  if (!sCollectors) sCollectors = new nsTArray<GfxInfoCollectorBase*>;
}

nsresult GfxInfoBase::GetInfo(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aResult) {
  InitCollectors();
  InfoObject obj(aCx);

  for (uint32_t i = 0; i < sCollectors->Length(); i++) {
    (*sCollectors)[i]->GetInfo(obj);
  }

  // Some example property definitions
  // obj.DefineProperty("wordCacheSize", gfxTextRunWordCache::Count());
  // obj.DefineProperty("renderer", mRendererIDsString);
  // obj.DefineProperty("five", 5);

  if (!obj.mOk) {
    return NS_ERROR_FAILURE;
  }

  aResult.setObject(*obj.mObj);
  return NS_OK;
}

nsAutoCString gBaseAppVersion;

const nsCString& GfxInfoBase::GetApplicationVersion() {
  static bool versionInitialized = false;
  if (!versionInitialized) {
    // If we fail to get the version, we will not try again.
    versionInitialized = true;

    // Get the version from xpcom/system/nsIXULAppInfo.idl
    nsCOMPtr<nsIXULAppInfo> app = do_GetService("@mozilla.org/xre/app-info;1");
    if (app) {
      app->GetVersion(gBaseAppVersion);
    }
  }
  return gBaseAppVersion;
}

/* static */ bool GfxInfoBase::OnlyAllowFeatureOnKnownConfig(int32_t aFeature) {
  switch (aFeature) {
    // The GPU process doesn't need hardware acceleration and can run on
    // devices that we normally block from not being on our whitelist.
    case nsIGfxInfo::FEATURE_GPU_PROCESS:
      return kIsAndroid;
    // We can mostly assume that ANGLE will work
    case nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE:
    // Remote WebGL is needed for Win32k Lockdown, so it should be enabled
    // regardless of HW support or not
    case nsIGfxInfo::FEATURE_ALLOW_WEBGL_OUT_OF_PROCESS:
    // Backdrop filter should generally work, especially if we fall back to
    // Software WebRender because of an unknown vendor.
    case nsIGfxInfo::FEATURE_BACKDROP_FILTER:
      return false;
    default:
      return true;
  }
}

void GfxInfoBase::AddCollector(GfxInfoCollectorBase* collector) {
  InitCollectors();
  sCollectors->AppendElement(collector);
}

void GfxInfoBase::RemoveCollector(GfxInfoCollectorBase* collector) {
  InitCollectors();
  for (uint32_t i = 0; i < sCollectors->Length(); i++) {
    if ((*sCollectors)[i] == collector) {
      sCollectors->RemoveElementAt(i);
      break;
    }
  }
  if (sCollectors->IsEmpty()) {
    delete sCollectors;
    sCollectors = nullptr;
  }
}

static void AppendMonitor(JSContext* aCx, widget::Screen& aScreen,
                          JS::Handle<JSObject*> aOutArray, int32_t aIndex) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

  auto screenSize = aScreen.GetRect().Size();

  JS::Rooted<JS::Value> screenWidth(aCx, JS::Int32Value(screenSize.width));
  JS_SetProperty(aCx, obj, "screenWidth", screenWidth);

  JS::Rooted<JS::Value> screenHeight(aCx, JS::Int32Value(screenSize.height));
  JS_SetProperty(aCx, obj, "screenHeight", screenHeight);

  // XXX Just preserving behavior since this is exposed to telemetry, but we
  // could consider including this everywhere.
#ifdef XP_MACOSX
  JS::Rooted<JS::Value> scale(
      aCx, JS::NumberValue(aScreen.GetContentsScaleFactor()));
  JS_SetProperty(aCx, obj, "scale", scale);
#endif

#ifdef XP_WIN
  JS::Rooted<JS::Value> refreshRate(aCx,
                                    JS::Int32Value(aScreen.GetRefreshRate()));
  JS_SetProperty(aCx, obj, "refreshRate", refreshRate);

  JS::Rooted<JS::Value> pseudoDisplay(
      aCx, JS::BooleanValue(aScreen.GetIsPseudoDisplay()));
  JS_SetProperty(aCx, obj, "pseudoDisplay", pseudoDisplay);
#endif

  JS::Rooted<JS::Value> element(aCx, JS::ObjectValue(*obj));
  JS_SetElement(aCx, aOutArray, aIndex, element);
}

nsresult GfxInfoBase::FindMonitors(JSContext* aCx,
                                   JS::Handle<JSObject*> aOutArray) {
  int32_t index = 0;
  auto& sm = ScreenManager::GetSingleton();
  for (auto& screen : sm.CurrentScreenList()) {
    AppendMonitor(aCx, *screen, aOutArray, index++);
  }

  if (index == 0) {
    // Ensure we return at least one monitor, this is needed for xpcshell.
    RefPtr<Screen> screen = sm.GetPrimaryScreen();
    AppendMonitor(aCx, *screen, aOutArray, index++);
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetMonitors(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, 0));

  nsresult rv = FindMonitors(aCx, array);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aResult.setObject(*array);
  return NS_OK;
}

static inline bool SetJSPropertyString(JSContext* aCx,
                                       JS::Handle<JSObject*> aObj,
                                       const char* aProp, const char* aString) {
  JS::Rooted<JSString*> str(aCx, JS_NewStringCopyZ(aCx, aString));
  if (!str) {
    return false;
  }

  JS::Rooted<JS::Value> val(aCx, JS::StringValue(str));
  return JS_SetProperty(aCx, aObj, aProp, val);
}

template <typename T>
static inline bool AppendJSElement(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                   const T& aValue) {
  uint32_t index;
  if (!JS::GetArrayLength(aCx, aObj, &index)) {
    return false;
  }
  return JS_SetElement(aCx, aObj, index, aValue);
}

nsresult GfxInfoBase::GetFeatures(JSContext* aCx,
                                  JS::MutableHandle<JS::Value> aOut) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*obj);

  layers::LayersBackend backend =
      gfxPlatform::Initialized()
          ? gfxPlatform::GetPlatform()->GetCompositorBackend()
          : layers::LayersBackend::LAYERS_NONE;
  const char* backendName = layers::GetLayersBackendName(backend);
  SetJSPropertyString(aCx, obj, "compositor", backendName);

  // If graphics isn't initialized yet, just stop now.
  if (!gfxPlatform::Initialized()) {
    return NS_OK;
  }

  DescribeFeatures(aCx, obj);
  return NS_OK;
}

nsresult GfxInfoBase::GetFeatureLog(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aOut) {
  JS::Rooted<JSObject*> containerObj(aCx, JS_NewPlainObject(aCx));
  if (!containerObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*containerObj);

  JS::Rooted<JSObject*> featureArray(aCx, JS::NewArrayObject(aCx, 0));
  if (!featureArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Collect features.
  gfxConfig::ForEachFeature([&](const char* aName, const char* aDescription,
                                FeatureState& aFeature) -> void {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return;
    }
    if (!SetJSPropertyString(aCx, obj, "name", aName) ||
        !SetJSPropertyString(aCx, obj, "description", aDescription) ||
        !SetJSPropertyString(aCx, obj, "status",
                             FeatureStatusToString(aFeature.GetValue()))) {
      return;
    }

    JS::Rooted<JS::Value> log(aCx);
    if (!BuildFeatureStateLog(aCx, aFeature, &log)) {
      return;
    }
    if (!JS_SetProperty(aCx, obj, "log", log)) {
      return;
    }

    if (!AppendJSElement(aCx, featureArray, obj)) {
      return;
    }
  });

  JS::Rooted<JSObject*> fallbackArray(aCx, JS::NewArrayObject(aCx, 0));
  if (!fallbackArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Collect fallbacks.
  gfxConfig::ForEachFallback(
      [&](const char* aName, const char* aMessage) -> void {
        JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
        if (!obj) {
          return;
        }

        if (!SetJSPropertyString(aCx, obj, "name", aName) ||
            !SetJSPropertyString(aCx, obj, "message", aMessage)) {
          return;
        }

        if (!AppendJSElement(aCx, fallbackArray, obj)) {
          return;
        }
      });

  JS::Rooted<JS::Value> val(aCx);

  val = JS::ObjectValue(*featureArray);
  JS_SetProperty(aCx, containerObj, "features", val);

  val = JS::ObjectValue(*fallbackArray);
  JS_SetProperty(aCx, containerObj, "fallbacks", val);

  return NS_OK;
}

bool GfxInfoBase::BuildFeatureStateLog(JSContext* aCx,
                                       const FeatureState& aFeature,
                                       JS::MutableHandle<JS::Value> aOut) {
  JS::Rooted<JSObject*> log(aCx, JS::NewArrayObject(aCx, 0));
  if (!log) {
    return false;
  }
  aOut.setObject(*log);

  aFeature.ForEachStatusChange([&](const char* aType, FeatureStatus aStatus,
                                   const char* aMessage,
                                   const nsCString& aFailureId) -> void {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return;
    }

    if (!SetJSPropertyString(aCx, obj, "type", aType) ||
        !SetJSPropertyString(aCx, obj, "status",
                             FeatureStatusToString(aStatus)) ||
        (!aFailureId.IsEmpty() &&
         !SetJSPropertyString(aCx, obj, "failureId", aFailureId.get())) ||
        (aMessage && !SetJSPropertyString(aCx, obj, "message", aMessage))) {
      return;
    }

    if (!AppendJSElement(aCx, log, obj)) {
      return;
    }
  });

  return true;
}

void GfxInfoBase::DescribeFeatures(JSContext* aCx, JS::Handle<JSObject*> aObj) {
  JS::Rooted<JSObject*> obj(aCx);

  gfx::FeatureState& hwCompositing =
      gfxConfig::GetFeature(gfx::Feature::HW_COMPOSITING);
  InitFeatureObject(aCx, aObj, "hwCompositing", hwCompositing, &obj);

  gfx::FeatureState& gpuProcess =
      gfxConfig::GetFeature(gfx::Feature::GPU_PROCESS);
  InitFeatureObject(aCx, aObj, "gpuProcess", gpuProcess, &obj);

  gfx::FeatureState& webrender = gfxConfig::GetFeature(gfx::Feature::WEBRENDER);
  InitFeatureObject(aCx, aObj, "webrender", webrender, &obj);

  gfx::FeatureState& wrCompositor =
      gfxConfig::GetFeature(gfx::Feature::WEBRENDER_COMPOSITOR);
  InitFeatureObject(aCx, aObj, "wrCompositor", wrCompositor, &obj);

  gfx::FeatureState& openglCompositing =
      gfxConfig::GetFeature(gfx::Feature::OPENGL_COMPOSITING);
  InitFeatureObject(aCx, aObj, "openglCompositing", openglCompositing, &obj);

  gfx::FeatureState& omtp = gfxConfig::GetFeature(gfx::Feature::OMTP);
  InitFeatureObject(aCx, aObj, "omtp", omtp, &obj);
}

bool GfxInfoBase::InitFeatureObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aContainer,
                                    const char* aName,
                                    mozilla::gfx::FeatureState& aFeatureState,
                                    JS::MutableHandle<JSObject*> aOutObj) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  nsCString status = aFeatureState.GetStatusAndFailureIdString();

  JS::Rooted<JSString*> str(aCx, JS_NewStringCopyZ(aCx, status.get()));
  JS::Rooted<JS::Value> val(aCx, JS::StringValue(str));
  JS_SetProperty(aCx, obj, "status", val);

  // Add the feature object to the container.
  {
    JS::Rooted<JS::Value> val(aCx, JS::ObjectValue(*obj));
    JS_SetProperty(aCx, aContainer, aName, val);
  }

  aOutObj.set(obj);
  return true;
}

nsresult GfxInfoBase::GetActiveCrashGuards(JSContext* aCx,
                                           JS::MutableHandle<JS::Value> aOut) {
  JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, 0));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*array);

  DriverCrashGuard::ForEachActiveCrashGuard(
      [&](const char* aName, const char* aPrefName) -> void {
        JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
        if (!obj) {
          return;
        }
        if (!SetJSPropertyString(aCx, obj, "type", aName)) {
          return;
        }
        if (!SetJSPropertyString(aCx, obj, "prefName", aPrefName)) {
          return;
        }
        if (!AppendJSElement(aCx, array, obj)) {
          return;
        }
      });

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetTargetFrameRate(uint32_t* aTargetFrameRate) {
  *aTargetFrameRate = gfxPlatform::TargetFrameRate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetCodecSupportInfo(nsACString& aCodecSupportInfo) {
  aCodecSupportInfo.Assign(gfx::gfxVars::CodecSupportInfo());
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetIsHeadless(bool* aIsHeadless) {
  *aIsHeadless = gfxPlatform::IsHeadless();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetContentBackend(nsAString& aContentBackend) {
  BackendType backend = gfxPlatform::GetPlatform()->GetDefaultContentBackend();
  nsString outStr;

  switch (backend) {
    case BackendType::DIRECT2D1_1: {
      outStr.AppendPrintf("Direct2D 1.1");
      break;
    }
    case BackendType::SKIA: {
      outStr.AppendPrintf("Skia");
      break;
    }
    case BackendType::CAIRO: {
      outStr.AppendPrintf("Cairo");
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }

  aContentBackend.Assign(outStr);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetAzureCanvasBackend(nsAString& aBackend) {
  CopyASCIItoUTF16(mozilla::MakeStringSpan(
                       gfxPlatform::GetPlatform()->GetAzureCanvasBackend()),
                   aBackend);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetAzureContentBackend(nsAString& aBackend) {
  CopyASCIItoUTF16(mozilla::MakeStringSpan(
                       gfxPlatform::GetPlatform()->GetAzureContentBackend()),
                   aBackend);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetUsingGPUProcess(bool* aOutValue) {
  GPUProcessManager* gpu = GPUProcessManager::Get();
  if (!gpu) {
    // Not supported in content processes.
    return NS_ERROR_FAILURE;
  }

  *aOutValue = !!gpu->GetGPUChild();
  return NS_OK;
}

NS_IMETHODIMP_(int32_t)
GfxInfoBase::GetMaxRefreshRate(bool* aMixed) {
  if (aMixed) {
    *aMixed = false;
  }

  int32_t maxRefreshRate = 0;
  for (auto& screen : ScreenManager::GetSingleton().CurrentScreenList()) {
    int32_t refreshRate = screen->GetRefreshRate();
    if (aMixed && maxRefreshRate > 0 && maxRefreshRate != refreshRate) {
      *aMixed = true;
    }
    maxRefreshRate = std::max(maxRefreshRate, refreshRate);
  }

  return maxRefreshRate > 0 ? maxRefreshRate : -1;
}

NS_IMETHODIMP
GfxInfoBase::ControlGPUProcessForXPCShell(bool aEnable, bool* _retval) {
  gfxPlatform::GetPlatform();

  GPUProcessManager* gpm = GPUProcessManager::Get();
  if (aEnable) {
    if (!gfxConfig::IsEnabled(gfx::Feature::GPU_PROCESS)) {
      gfxConfig::UserForceEnable(gfx::Feature::GPU_PROCESS, "xpcshell-test");
    }
    DebugOnly<nsresult> rv = gpm->EnsureGPUReady();
    MOZ_ASSERT(rv != NS_ERROR_ILLEGAL_DURING_SHUTDOWN);
  } else {
    gfxConfig::UserDisable(gfx::Feature::GPU_PROCESS, "xpcshell-test");
    gpm->KillProcess();
  }

  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP GfxInfoBase::KillGPUProcessForTests() {
  GPUProcessManager* gpm = GPUProcessManager::Get();
  if (!gpm) {
    // gfxPlatform has not been initialized.
    return NS_ERROR_NOT_INITIALIZED;
  }

  gpm->KillProcess();
  return NS_OK;
}

NS_IMETHODIMP GfxInfoBase::CrashGPUProcessForTests() {
  GPUProcessManager* gpm = GPUProcessManager::Get();
  if (!gpm) {
    // gfxPlatform has not been initialized.
    return NS_ERROR_NOT_INITIALIZED;
  }

  gpm->CrashProcess();
  return NS_OK;
}

GfxInfoCollectorBase::GfxInfoCollectorBase() {
  GfxInfoBase::AddCollector(this);
}

GfxInfoCollectorBase::~GfxInfoCollectorBase() {
  GfxInfoBase::RemoveCollector(this);
}
