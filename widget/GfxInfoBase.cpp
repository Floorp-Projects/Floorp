/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "GfxInfoBase.h"

#include "GfxInfoWebGL.h"
#include "GfxDriverInfo.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsVersionComparator.h"
#include "mozilla/Services.h"
#include "mozilla/Observer.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "nsIXULAppInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "MediaPrefs.h"
#include "gfxPrefs.h"
#include "gfxPlatform.h"
#include "gfxConfig.h"
#include "DriverCrashGuard.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif

using namespace mozilla::widget;
using namespace mozilla;
using mozilla::MutexAutoLock;

nsTArray<GfxDriverInfo>* GfxInfoBase::mDriverInfo;
nsTArray<dom::GfxInfoFeatureStatus>* GfxInfoBase::mFeatureStatus;
bool GfxInfoBase::mDriverInfoObserverInitialized;
bool GfxInfoBase::mShutdownOccurred;

// Observes for shutdown so that the child GfxDriverInfo list is freed.
class ShutdownObserver : public nsIObserver
{
  virtual ~ShutdownObserver() {}

public:
  ShutdownObserver() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports *subject, const char *aTopic,
                     const char16_t *aData) override
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    delete GfxInfoBase::mDriverInfo;
    GfxInfoBase::mDriverInfo = nullptr;

    delete GfxInfoBase::mFeatureStatus;
    GfxInfoBase::mFeatureStatus = nullptr;

    for (uint32_t i = 0; i < DeviceFamilyMax; i++)
      delete GfxDriverInfo::mDeviceFamilies[i];

    for (uint32_t i = 0; i < DeviceVendorMax; i++)
      delete GfxDriverInfo::mDeviceVendors[i];

    GfxInfoBase::mShutdownOccurred = true;

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver)

void InitGfxDriverInfoShutdownObserver()
{
  if (GfxInfoBase::mDriverInfoObserverInitialized)
    return;

  GfxInfoBase::mDriverInfoObserverInitialized = true;

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    NS_WARNING("Could not get observer service!");
    return;
  }

  ShutdownObserver *obs = new ShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

using namespace mozilla::widget;
using namespace mozilla::gfx;
using namespace mozilla;

NS_IMPL_ISUPPORTS(GfxInfoBase, nsIGfxInfo, nsIObserver, nsISupportsWeakReference)

#define BLACKLIST_PREF_BRANCH "gfx.blacklist."
#define SUGGESTED_VERSION_PREF BLACKLIST_PREF_BRANCH "suggested-driver-version"
#define BLACKLIST_ENTRY_TAG_NAME "gfxBlacklistEntry"

static const char*
GetPrefNameForFeature(int32_t aFeature)
{
  const char* name = nullptr;
  switch(aFeature) {
    case nsIGfxInfo::FEATURE_DIRECT2D:
      name = BLACKLIST_PREF_BRANCH "direct2d";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d9";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d10";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d10-1";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d11";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE:
      name = BLACKLIST_PREF_BRANCH "direct3d11angle";
      break;
    case nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING:
      name = BLACKLIST_PREF_BRANCH "hardwarevideodecoding";
      break;
    case nsIGfxInfo::FEATURE_OPENGL_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_OPENGL:
      name = BLACKLIST_PREF_BRANCH "webgl.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_ANGLE:
      name = BLACKLIST_PREF_BRANCH "webgl.angle";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_MSAA:
      name = BLACKLIST_PREF_BRANCH "webgl.msaa";
      break;
    case nsIGfxInfo::FEATURE_STAGEFRIGHT:
      name = BLACKLIST_PREF_BRANCH "stagefright";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION:
      name = BLACKLIST_PREF_BRANCH "webrtc.hw.acceleration";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE:
      name = BLACKLIST_PREF_BRANCH "webrtc.hw.acceleration.encode";
      break;
    case nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE:
      name = BLACKLIST_PREF_BRANCH "webrtc.hw.acceleration.decode";
      break;
    case nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION:
      name = BLACKLIST_PREF_BRANCH "canvas2d.acceleration";
      break;
    case nsIGfxInfo::FEATURE_WEBGL2:
      name = BLACKLIST_PREF_BRANCH "webgl2";
      break;
    case nsIGfxInfo::FEATURE_VP8_HW_DECODE:
    case nsIGfxInfo::FEATURE_VP9_HW_DECODE:
    case nsIGfxInfo::FEATURE_DX_INTEROP2:
    case nsIGfxInfo::FEATURE_GPU_PROCESS:
      // We don't provide prefs for these features.
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected nsIGfxInfo feature?!");
      break;
  }

  return name;
}

// Returns the value of the pref for the relevant feature in aValue.
// If the pref doesn't exist, aValue is not touched, and returns false.
static bool
GetPrefValueForFeature(int32_t aFeature, int32_t& aValue, nsACString& aFailureId)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return false;

  aValue = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  if (!NS_SUCCEEDED(Preferences::GetInt(prefname, &aValue))) {
    return false;
  }

  nsCString failureprefname(prefname);
  failureprefname += ".failureid";
  nsAdoptingCString failureValue = Preferences::GetCString(failureprefname.get());
  if (failureValue) {
    aFailureId = failureValue.get();
  } else {
    aFailureId = "FEATURE_FAILURE_BLACKLIST_PREF";
  }

  return true;
}

static void
SetPrefValueForFeature(int32_t aFeature, int32_t aValue, const nsACString& aFailureId)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return;

  Preferences::SetInt(prefname, aValue);
  if (!aFailureId.IsEmpty()) {
    nsCString failureprefname(prefname);
    failureprefname += ".failureid";
    Preferences::SetCString(failureprefname.get(), aFailureId);
  }
}

static void
RemovePrefForFeature(int32_t aFeature)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return;

  Preferences::ClearUser(prefname);
}

static bool
GetPrefValueForDriverVersion(nsCString& aVersion)
{
  return NS_SUCCEEDED(Preferences::GetCString(SUGGESTED_VERSION_PREF,
                                              &aVersion));
}

static void
SetPrefValueForDriverVersion(const nsAString& aVersion)
{
  Preferences::SetString(SUGGESTED_VERSION_PREF, aVersion);
}

static void
RemovePrefForDriverVersion()
{
  Preferences::ClearUser(SUGGESTED_VERSION_PREF);
}


static OperatingSystem
BlacklistOSToOperatingSystem(const nsAString& os)
{
  if (os.EqualsLiteral("WINNT 6.1"))
    return OperatingSystem::Windows7;
  else if (os.EqualsLiteral("WINNT 6.2"))
    return OperatingSystem::Windows8;
  else if (os.EqualsLiteral("WINNT 6.3"))
    return OperatingSystem::Windows8_1;
  else if (os.EqualsLiteral("WINNT 10.0"))
    return OperatingSystem::Windows10;
  else if (os.EqualsLiteral("Linux"))
    return OperatingSystem::Linux;
  else if (os.EqualsLiteral("Darwin 9"))
    return OperatingSystem::OSX10_5;
  else if (os.EqualsLiteral("Darwin 10"))
    return OperatingSystem::OSX10_6;
  else if (os.EqualsLiteral("Darwin 11"))
    return OperatingSystem::OSX10_7;
  else if (os.EqualsLiteral("Darwin 12"))
    return OperatingSystem::OSX10_8;
  else if (os.EqualsLiteral("Darwin 13"))
    return OperatingSystem::OSX10_9;
  else if (os.EqualsLiteral("Darwin 14"))
    return OperatingSystem::OSX10_10;
  else if (os.EqualsLiteral("Darwin 15"))
    return OperatingSystem::OSX10_11;
  else if (os.EqualsLiteral("Darwin 16"))
    return OperatingSystem::OSX10_12;
  else if (os.EqualsLiteral("Android"))
    return OperatingSystem::Android;
  // For historical reasons, "All" in blocklist means "All Windows"
  else if (os.EqualsLiteral("All"))
    return OperatingSystem::Windows;

  return OperatingSystem::Unknown;
}

static GfxDeviceFamily*
BlacklistDevicesToDeviceFamily(nsTArray<nsCString>& devices)
{
  if (devices.Length() == 0)
    return nullptr;

  // For each device, get its device ID, and return a freshly-allocated
  // GfxDeviceFamily with the contents of that array.
  GfxDeviceFamily* deviceIds = new GfxDeviceFamily;

  for (uint32_t i = 0; i < devices.Length(); ++i) {
    // We make sure we don't add any "empty" device entries to the array, so
    // we don't need to check if devices[i] is empty.
    deviceIds->AppendElement(NS_ConvertUTF8toUTF16(devices[i]));
  }

  return deviceIds;
}

static int32_t
BlacklistFeatureToGfxFeature(const nsAString& aFeature)
{
  MOZ_ASSERT(!aFeature.IsEmpty());
  if (aFeature.EqualsLiteral("DIRECT2D"))
    return nsIGfxInfo::FEATURE_DIRECT2D;
  else if (aFeature.EqualsLiteral("DIRECT3D_9_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS;
  else if (aFeature.EqualsLiteral("DIRECT3D_10_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS;
  else if (aFeature.EqualsLiteral("DIRECT3D_10_1_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS;
  else if (aFeature.EqualsLiteral("DIRECT3D_11_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS;
  else if (aFeature.EqualsLiteral("DIRECT3D_11_ANGLE"))
    return nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE;
  else if (aFeature.EqualsLiteral("HARDWARE_VIDEO_DECODING"))
    return nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING;
  else if (aFeature.EqualsLiteral("OPENGL_LAYERS"))
    return nsIGfxInfo::FEATURE_OPENGL_LAYERS;
  else if (aFeature.EqualsLiteral("WEBGL_OPENGL"))
    return nsIGfxInfo::FEATURE_WEBGL_OPENGL;
  else if (aFeature.EqualsLiteral("WEBGL_ANGLE"))
    return nsIGfxInfo::FEATURE_WEBGL_ANGLE;
  else if (aFeature.EqualsLiteral("WEBGL_MSAA"))
    return nsIGfxInfo::FEATURE_WEBGL_MSAA;
  else if (aFeature.EqualsLiteral("STAGEFRIGHT"))
    return nsIGfxInfo::FEATURE_STAGEFRIGHT;
  else if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION_ENCODE"))
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE;
  else if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION_DECODE"))
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE;
  else if (aFeature.EqualsLiteral("WEBRTC_HW_ACCELERATION"))
    return nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION;
  else if (aFeature.EqualsLiteral("CANVAS2D_ACCELERATION"))
      return nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION;
  else if (aFeature.EqualsLiteral("WEBGL2"))
    return nsIGfxInfo::FEATURE_WEBGL2;

  // If we don't recognize the feature, it may be new, and something
  // this version doesn't understand.  So, nothing to do.  This is
  // different from feature not being specified at all, in which case
  // this method should not get called and we should continue with the
  // "all features" blocklisting.
  return -1;
}

static int32_t
BlacklistFeatureStatusToGfxFeatureStatus(const nsAString& aStatus)
{
  if (aStatus.EqualsLiteral("STATUS_OK"))
    return nsIGfxInfo::FEATURE_STATUS_OK;
  else if (aStatus.EqualsLiteral("BLOCKED_DRIVER_VERSION"))
    return nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
  else if (aStatus.EqualsLiteral("BLOCKED_DEVICE"))
    return nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  else if (aStatus.EqualsLiteral("DISCOURAGED"))
    return nsIGfxInfo::FEATURE_DISCOURAGED;
  else if (aStatus.EqualsLiteral("BLOCKED_OS_VERSION"))
    return nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;

  // Do not allow it to set STATUS_UNKNOWN.  Also, we are not
  // expecting the "mismatch" status showing up here.

  return nsIGfxInfo::FEATURE_STATUS_OK;
}

static VersionComparisonOp
BlacklistComparatorToComparisonOp(const nsAString& op)
{
  if (op.EqualsLiteral("LESS_THAN"))
    return DRIVER_LESS_THAN;
  else if (op.EqualsLiteral("BUILD_ID_LESS_THAN"))
    return DRIVER_BUILD_ID_LESS_THAN;
  else if (op.EqualsLiteral("LESS_THAN_OR_EQUAL"))
    return DRIVER_LESS_THAN_OR_EQUAL;
  else if (op.EqualsLiteral("BUILD_ID_LESS_THAN_OR_EQUAL"))
    return DRIVER_BUILD_ID_LESS_THAN_OR_EQUAL;
  else if (op.EqualsLiteral("GREATER_THAN"))
    return DRIVER_GREATER_THAN;
  else if (op.EqualsLiteral("GREATER_THAN_OR_EQUAL"))
    return DRIVER_GREATER_THAN_OR_EQUAL;
  else if (op.EqualsLiteral("EQUAL"))
    return DRIVER_EQUAL;
  else if (op.EqualsLiteral("NOT_EQUAL"))
    return DRIVER_NOT_EQUAL;
  else if (op.EqualsLiteral("BETWEEN_EXCLUSIVE"))
    return DRIVER_BETWEEN_EXCLUSIVE;
  else if (op.EqualsLiteral("BETWEEN_INCLUSIVE"))
    return DRIVER_BETWEEN_INCLUSIVE;
  else if (op.EqualsLiteral("BETWEEN_INCLUSIVE_START"))
    return DRIVER_BETWEEN_INCLUSIVE_START;

  return DRIVER_COMPARISON_IGNORED;
}


/*
  Deserialize Blacklist entries from string.
  e.g:
  os:WINNT 6.0\tvendor:0x8086\tdevices:0x2582,0x2782\tfeature:DIRECT3D_10_LAYERS\tfeatureStatus:BLOCKED_DRIVER_VERSION\tdriverVersion:8.52.322.2202\tdriverVersionComparator:LESS_THAN_OR_EQUAL
*/
static bool
BlacklistEntryToDriverInfo(nsCString& aBlacklistEntry,
                           GfxDriverInfo& aDriverInfo)
{
  // If we get an application version to be zero, something is not working
  // and we are not going to bother checking the blocklist versions.
  // See TestGfxWidgets.cpp for how version comparison works.
  // <versionRange minVersion="42.0a1" maxVersion="45.0"></versionRange>
  static mozilla::Version zeroV("0");
  static mozilla::Version appV(GfxInfoBase::GetApplicationVersion().get());
  if (appV <= zeroV) {
      gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Invalid application version " << GfxInfoBase::GetApplicationVersion().get();
  }

  nsTArray<nsCString> keyValues;
  ParseString(aBlacklistEntry, '\t', keyValues);

  aDriverInfo.mRuleId = NS_LITERAL_CSTRING("FEATURE_FAILURE_DL_BLACKLIST_NO_ID");

  for (uint32_t i = 0; i < keyValues.Length(); ++i) {
    nsCString keyValue = keyValues[i];
    nsTArray<nsCString> splitted;
    ParseString(keyValue, ':', splitted);
    if (splitted.Length() != 2) {
      // If we don't recognize the input data, we do not want to proceed.
      gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false)) << "Unrecognized data " << keyValue.get();
      return false;
    }
    nsCString key = splitted[0];
    nsCString value = splitted[1];
    NS_ConvertUTF8toUTF16 dataValue(value);

    if (value.Length() == 0) {
      // Safety check for empty values.
      gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false)) << "Empty value for " << key.get();
      return false;
    }

    if (key.EqualsLiteral("blockID")) {
       nsCString blockIdStr = NS_LITERAL_CSTRING("FEATURE_FAILURE_DL_BLACKLIST_") + value;
       aDriverInfo.mRuleId = blockIdStr.get();
    } else if (key.EqualsLiteral("os")) {
      aDriverInfo.mOperatingSystem = BlacklistOSToOperatingSystem(dataValue);
    } else if (key.EqualsLiteral("osversion")) {
      aDriverInfo.mOperatingSystemVersion = strtoul(value.get(), nullptr, 10);
    } else if (key.EqualsLiteral("vendor")) {
      aDriverInfo.mAdapterVendor = dataValue;
    } else if (key.EqualsLiteral("feature")) {
      aDriverInfo.mFeature = BlacklistFeatureToGfxFeature(dataValue);
      if (aDriverInfo.mFeature < 0) {
        // If we don't recognize the feature, we do not want to proceed.
        gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false)) << "Unrecognized feature " << value.get();
        return false;
      }
    } else if (key.EqualsLiteral("featureStatus")) {
      aDriverInfo.mFeatureStatus = BlacklistFeatureStatusToGfxFeatureStatus(dataValue);
    } else if (key.EqualsLiteral("driverVersion")) {
      uint64_t version;
      if (ParseDriverVersion(dataValue, &version))
        aDriverInfo.mDriverVersion = version;
    } else if (key.EqualsLiteral("driverVersionMax")) {
      uint64_t version;
      if (ParseDriverVersion(dataValue, &version))
        aDriverInfo.mDriverVersionMax = version;
    } else if (key.EqualsLiteral("driverVersionComparator")) {
      aDriverInfo.mComparisonOp = BlacklistComparatorToComparisonOp(dataValue);
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
        gfxCriticalErrorOnce(CriticalLog::DefaultOptions(false)) << "Unrecognized versionRange " << value.get();
        return false;
      }
      nsCString minValue = versionRange[0];
      nsCString maxValue = versionRange[1];

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
      GfxDeviceFamily* deviceIds = BlacklistDevicesToDeviceFamily(devices);
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

static void
BlacklistEntriesToDriverInfo(nsTArray<nsCString>& aBlacklistEntries,
                             nsTArray<GfxDriverInfo>& aDriverInfo)
{
  aDriverInfo.Clear();
  aDriverInfo.SetLength(aBlacklistEntries.Length());

  for (uint32_t i = 0; i < aBlacklistEntries.Length(); ++i) {
    nsCString blacklistEntry = aBlacklistEntries[i];
    GfxDriverInfo di;
    if (BlacklistEntryToDriverInfo(blacklistEntry, di)) {
      aDriverInfo[i] = di;
      // Prevent di falling out of scope from destroying the devices.
      di.mDeleteDevices = false;
    }
  }
}

NS_IMETHODIMP
GfxInfoBase::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  if (strcmp(aTopic, "blocklist-data-gfxItems") == 0) {
    nsTArray<GfxDriverInfo> driverInfo;
    nsTArray<nsCString> blacklistEntries;
    nsCString utf8Data = NS_ConvertUTF16toUTF8(aData);
    if (utf8Data.Length() > 0) {
      ParseString(utf8Data, '\n', blacklistEntries);
    }
    BlacklistEntriesToDriverInfo(blacklistEntries, driverInfo);
    EvaluateDownloadedBlacklist(driverInfo);
  }

  return NS_OK;
}

GfxInfoBase::GfxInfoBase()
    : mMutex("GfxInfoBase")
{
}

GfxInfoBase::~GfxInfoBase()
{
}

nsresult
GfxInfoBase::Init()
{
  InitGfxDriverInfoShutdownObserver();
  gfxPrefs::GetSingleton();
  MediaPrefs::GetSingleton();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "blocklist-data-gfxItems", true);
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureStatus(int32_t aFeature, nsACString& aFailureId, int32_t* aStatus)
{
  int32_t blocklistAll = gfxPrefs::BlocklistAll();
  if (blocklistAll > 0) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Forcing blocklisting all features";
    *aStatus = FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_BLOCK_ALL";
    return NS_OK;
  } else if (blocklistAll < 0) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Ignoring any feature blocklisting.";
    *aStatus = FEATURE_STATUS_OK;
    return NS_OK;
  }

  if (GetPrefValueForFeature(aFeature, *aStatus, aFailureId)) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    // Use the cached data received from the parent process.
    MOZ_ASSERT(mFeatureStatus);
    bool success = false;
    for (const auto& fs : *mFeatureStatus) {
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
  nsresult rv = GetFeatureStatusImpl(aFeature, aStatus, version, driverInfo, aFailureId);
  return rv;
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
inline bool
MatchingOperatingSystems(OperatingSystem aBlockedOS, OperatingSystem aSystemOS)
{
  MOZ_ASSERT(aSystemOS != OperatingSystem::Windows &&
             aSystemOS != OperatingSystem::OSX);

  // If the block entry OS is unknown, it doesn't match
  if (aBlockedOS == OperatingSystem::Unknown) {
    return false;
  }

#if defined (XP_WIN)
  if (aBlockedOS == OperatingSystem::Windows) {
    // We do want even "unknown" aSystemOS to fall under "all windows"
    return true;
  }
#endif

#if defined (XP_MACOSX)
  if (aBlockedOS == OperatingSystem::OSX) {
    // We do want even "unknown" aSystemOS to fall under "all OS X"
    return true;
  }
#endif

  return aSystemOS == aBlockedOS;
}

int32_t
GfxInfoBase::FindBlocklistedDeviceInList(const nsTArray<GfxDriverInfo>& info,
                                         nsAString& aSuggestedVersion,
                                         int32_t aFeature,
                                         nsACString& aFailureId,
                                         OperatingSystem os)
{
  int32_t status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  // Get the adapters once then reuse below
  nsAutoString adapterVendorID[2];
  nsAutoString adapterDeviceID[2];
  nsAutoString adapterDriverVersionString[2];
  bool adapterInfoFailed[2];

  adapterInfoFailed[0] = (NS_FAILED(GetAdapterVendorID(adapterVendorID[0])) ||
			  NS_FAILED(GetAdapterDeviceID(adapterDeviceID[0])) ||
			  NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString[0])));
  adapterInfoFailed[1] = (NS_FAILED(GetAdapterVendorID2(adapterVendorID[1])) ||
			  NS_FAILED(GetAdapterDeviceID2(adapterDeviceID[1])) ||
			  NS_FAILED(GetAdapterDriverVersion2(adapterDriverVersionString[1])));
  // No point in going on if we don't have adapter info
  if (adapterInfoFailed[0] && adapterInfoFailed[1]) {
    return 0;
  }

#if defined(XP_WIN) || defined(ANDROID)
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
    if (!MatchingOperatingSystems(info[i].mOperatingSystem, os)) {
      continue;
    }

    if (info[i].mOperatingSystemVersion && info[i].mOperatingSystemVersion != OperatingSystemVersion()) {
        continue;
    }

    if (!info[i].mAdapterVendor.Equals(GfxDriverInfo::GetDeviceVendor(VendorAll), nsCaseInsensitiveStringComparator()) &&
        !info[i].mAdapterVendor.Equals(adapterVendorID[infoIndex], nsCaseInsensitiveStringComparator())) {
      continue;
    }

    if (info[i].mDevices != GfxDriverInfo::allDevices && info[i].mDevices->Length()) {
        bool deviceMatches = false;
        for (uint32_t j = 0; j < info[i].mDevices->Length(); j++) {
            if ((*info[i].mDevices)[j].Equals(adapterDeviceID[infoIndex], nsCaseInsensitiveStringComparator())) {
                deviceMatches = true;
                break;
            }
        }

        if (!deviceMatches) {
            continue;
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
    if (!info[i].mManufacturer.IsEmpty() && !info[i].mManufacturer.Equals(Manufacturer())) {
        continue;
    }

#if defined(XP_WIN) || defined(ANDROID)
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
      match = driverVersion[infoIndex] > info[i].mDriverVersion && driverVersion[infoIndex] < info[i].mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE:
      match = driverVersion[infoIndex] >= info[i].mDriverVersion && driverVersion[infoIndex] <= info[i].mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE_START:
      match = driverVersion[infoIndex] >= info[i].mDriverVersion && driverVersion[infoIndex] < info[i].mDriverVersionMax;
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
          info[i].mFeature == aFeature)
      {
        status = info[i].mFeatureStatus;
        if (!info[i].mRuleId.IsEmpty()) {
          aFailureId = info[i].mRuleId.get();
        } else {
          aFailureId = "FEATURE_FAILURE_DL_BLACKLIST_NO_ID";
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
      nsAString &nvVendorID = (nsAString &)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA);
      const nsString nv310mDeviceId = NS_LITERAL_STRING("0x0A70");
      if (nvVendorID.Equals(adapterVendorID[1], nsCaseInsensitiveStringComparator()) &&
        nv310mDeviceId.Equals(adapterDeviceID[1], nsCaseInsensitiveStringComparator())) {
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
               info[i].mDriverVersion != GfxDriverInfo::allDriverVersions)
    {
        aSuggestedVersion.AppendPrintf("%lld.%lld.%lld.%lld",
                                      (info[i].mDriverVersion & 0xffff000000000000) >> 48,
                                      (info[i].mDriverVersion & 0x0000ffff00000000) >> 32,
                                      (info[i].mDriverVersion & 0x00000000ffff0000) >> 16,
                                      (info[i].mDriverVersion & 0x000000000000ffff));
    }
  }
#endif

  return status;
}

void
GfxInfoBase::SetFeatureStatus(const nsTArray<dom::GfxInfoFeatureStatus>& aFS)
{
  MOZ_ASSERT(!mFeatureStatus);
  mFeatureStatus = new nsTArray<dom::GfxInfoFeatureStatus>(aFS);
}

nsresult
GfxInfoBase::GetFeatureStatusImpl(int32_t aFeature,
                                  int32_t* aStatus,
                                  nsAString& aSuggestedVersion,
                                  const nsTArray<GfxDriverInfo>& aDriverInfo,
                                  nsACString& aFailureId,
                                  OperatingSystem* aOS /* = nullptr */)
{
  if (aFeature <= 0) {
    gfxWarning() << "Invalid feature <= 0";
    return NS_OK;
  }

  if (*aStatus != nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    // Terminate now with the status determined by the derived type (OS-specific
    // code).
    return NS_OK;
  }

  if (mShutdownOccurred) {
    // This is futile; we've already commenced shutdown and our blocklists have
    // been deleted. We may want to look into resurrecting the blocklist instead
    // but for now, just don't even go there.
    return NS_OK;
  }

  // If an operating system was provided by the derived GetFeatureStatusImpl,
  // grab it here. Otherwise, the OS is unknown.
  OperatingSystem os = (aOS ? *aOS : OperatingSystem::Unknown);

  nsAutoString adapterVendorID;
  nsAutoString adapterDeviceID;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(adapterVendorID)) ||
      NS_FAILED(GetAdapterDeviceID(adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
  {
    aFailureId = "FEATURE_FAILURE_CANT_RESOLVE_ADAPTER";
    *aStatus = FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

  // Check if the device is blocked from the downloaded blocklist. If not, check
  // the static list after that. This order is used so that we can later escape
  // out of static blocks (i.e. if we were wrong or something was patched, we
  // can back out our static block without doing a release).
  int32_t status;
  if (aDriverInfo.Length()) {
    status = FindBlocklistedDeviceInList(aDriverInfo, aSuggestedVersion, aFeature, aFailureId, os);
  } else {
    if (!mDriverInfo) {
      mDriverInfo = new nsTArray<GfxDriverInfo>();
    }
    status = FindBlocklistedDeviceInList(GetGfxDriverInfo(), aSuggestedVersion, aFeature, aFailureId, os);
  }

  // It's now done being processed. It's safe to set the status to STATUS_OK.
  if (status == nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
  } else {
    *aStatus = status;
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureSuggestedDriverVersion(int32_t aFeature,
                                              nsAString& aVersion)
{
  nsCString version;
  if (GetPrefValueForDriverVersion(version)) {
    aVersion = NS_ConvertASCIItoUTF16(version);
    return NS_OK;
  }

  int32_t status;
  nsCString discardFailureId;
  nsTArray<GfxDriverInfo> driverInfo;
  return GetFeatureStatusImpl(aFeature, &status, aVersion, driverInfo, discardFailureId);
}


NS_IMETHODIMP
GfxInfoBase::GetWebGLParameter(const nsAString& aParam,
                               nsAString& aResult)
{
  return GfxInfoWebGL::GetWebGLParameter(aParam, aResult);
}

void
GfxInfoBase::EvaluateDownloadedBlacklist(nsTArray<GfxDriverInfo>& aDriverInfo)
{
  int32_t features[] = {
    nsIGfxInfo::FEATURE_DIRECT2D,
    nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE,
    nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL,
    nsIGfxInfo::FEATURE_WEBGL_ANGLE,
    nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE,
    nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE,
    nsIGfxInfo::FEATURE_WEBGL_MSAA,
    nsIGfxInfo::FEATURE_STAGEFRIGHT,
    nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION,
    nsIGfxInfo::FEATURE_CANVAS2D_ACCELERATION,
    nsIGfxInfo::FEATURE_WEBGL2,
    0
  };

  // For every feature we know about, we evaluate whether this blacklist has a
  // non-STATUS_OK status. If it does, we set the pref we evaluate in
  // GetFeatureStatus above, so we don't need to hold on to this blacklist
  // anywhere permanent.
  int i = 0;
  while (features[i]) {
    int32_t status;
    nsCString failureId;
    nsAutoString suggestedVersion;
    if (NS_SUCCEEDED(GetFeatureStatusImpl(features[i], &status,
                                          suggestedVersion,
                                          aDriverInfo,
                                          failureId))) {
      switch (status) {
        default:
        case nsIGfxInfo::FEATURE_STATUS_OK:
          RemovePrefForFeature(features[i]);
          break;

        case nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION:
          if (!suggestedVersion.IsEmpty()) {
            SetPrefValueForDriverVersion(suggestedVersion);
          } else {
            RemovePrefForDriverVersion();
          }
          MOZ_FALLTHROUGH;

        case nsIGfxInfo::FEATURE_BLOCKED_MISMATCHED_VERSION:
        case nsIGfxInfo::FEATURE_BLOCKED_DEVICE:
        case nsIGfxInfo::FEATURE_DISCOURAGED:
        case nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION:
          SetPrefValueForFeature(features[i], status, failureId);
          break;
      }
    }

    ++i;
  }
}

NS_IMETHODIMP_(void)
GfxInfoBase::LogFailure(const nsACString &failure)
{
  // gfxCriticalError has a mutex lock of its own, so we may not actually
  // need this lock. ::GetFailures() accesses the data but the LogForwarder
  // will not return the copy of the logs unless it can get the same lock
  // that gfxCriticalError uses.  Still, that is so much of an implementation
  // detail that it's nicer to just add an extra lock here and in ::GetFailures()
  MutexAutoLock lock(mMutex);

  // By default, gfxCriticalError asserts; make it not assert in this case.
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "(LF) " << failure.BeginReading();
}

/* XPConnect method of returning arrays is very ugly. Would not recommend. */
NS_IMETHODIMP GfxInfoBase::GetFailures(uint32_t* failureCount,
                                       int32_t** indices,
                                       char ***failures)
{
  MutexAutoLock lock(mMutex);

  NS_ENSURE_ARG_POINTER(failureCount);
  NS_ENSURE_ARG_POINTER(failures);

  *failures = nullptr;
  *failureCount = 0;

  // indices is "allowed" to be null, the caller may not care about them,
  // although calling from JS doesn't seem to get us there.
  if (indices) *indices = nullptr;

  LogForwarder* logForwarder = Factory::GetLogForwarder();
  if (!logForwarder) {
    return NS_ERROR_UNEXPECTED;
  }

  // There are two stirng copies in this method, starting with this one. We are
  // assuming this is not a big deal, as the size of the array should be small
  // and the strings in it should be small as well (the error messages in the
  // code.)  The second copy happens with the Clone() calls.  Technically,
  // we don't need the mutex lock after the StringVectorCopy() call.
  LoggingRecord loggedStrings = logForwarder->LoggingRecordCopy();
  *failureCount = loggedStrings.size();

  if (*failureCount != 0) {
    *failures = (char**)moz_xmalloc(*failureCount * sizeof(char*));
    if (!(*failures)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (indices) {
      *indices = (int32_t*)moz_xmalloc(*failureCount * sizeof(int32_t));
      if (!(*indices)) {
        free(*failures);
        *failures = nullptr;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    /* copy over the failure messages into the array we just allocated */
    LoggingRecord::const_iterator it;
    uint32_t i=0;
    for(it = loggedStrings.begin() ; it != loggedStrings.end(); ++it, i++) {
      (*failures)[i] = (char*)nsMemory::Clone(Get<1>(*it).c_str(), Get<1>(*it).size() + 1);
      if (indices) (*indices)[i] = Get<0>(*it);

      if (!(*failures)[i]) {
        /* <sarcasm> I'm too afraid to use an inline function... </sarcasm> */
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, (*failures));
        *failureCount = i;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}

nsTArray<GfxInfoCollectorBase*> *sCollectors;

static void
InitCollectors()
{
  if (!sCollectors)
    sCollectors = new nsTArray<GfxInfoCollectorBase*>;
}

nsresult GfxInfoBase::GetInfo(JSContext* aCx, JS::MutableHandle<JS::Value> aResult)
{
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

const nsCString&
GfxInfoBase::GetApplicationVersion()
{
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

void
GfxInfoBase::AddCollector(GfxInfoCollectorBase* collector)
{
  InitCollectors();
  sCollectors->AppendElement(collector);
}

void
GfxInfoBase::RemoveCollector(GfxInfoCollectorBase* collector)
{
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

NS_IMETHODIMP
GfxInfoBase::GetMonitors(JSContext* aCx, JS::MutableHandleValue aResult)
{
  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));

  nsresult rv = FindMonitors(aCx, array);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aResult.setObject(*array);
  return NS_OK;
}

static const char*
GetLayersBackendName(layers::LayersBackend aBackend)
{
  switch (aBackend) {
    case layers::LayersBackend::LAYERS_NONE:
      return "none";
    case layers::LayersBackend::LAYERS_OPENGL:
      return "opengl";
    case layers::LayersBackend::LAYERS_D3D11:
      return "d3d11";
    case layers::LayersBackend::LAYERS_CLIENT:
      return "client";
    case layers::LayersBackend::LAYERS_WR:
      return "webrender";
    case layers::LayersBackend::LAYERS_BASIC:
      return "basic";
    default:
      MOZ_ASSERT_UNREACHABLE("unknown layers backend");
      return "unknown";
  }
}

static inline bool
SetJSPropertyString(JSContext* aCx, JS::Handle<JSObject*> aObj,
                    const char* aProp, const char* aString)
{
  JS::Rooted<JSString*> str(aCx, JS_NewStringCopyZ(aCx, aString));
  if (!str) {
    return false;
  }

  JS::Rooted<JS::Value> val(aCx, JS::StringValue(str));
  return JS_SetProperty(aCx, aObj, aProp, val);
}

template <typename T>
static inline bool
AppendJSElement(JSContext* aCx, JS::Handle<JSObject*> aObj, const T& aValue)
{
  uint32_t index;
  if (!JS_GetArrayLength(aCx, aObj, &index)) {
    return false;
  }
  return JS_SetElement(aCx, aObj, index, aValue);
}

nsresult
GfxInfoBase::GetFeatures(JSContext* aCx, JS::MutableHandle<JS::Value> aOut)
{
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*obj);

  layers::LayersBackend backend = gfxPlatform::Initialized()
                                  ? gfxPlatform::GetPlatform()->GetCompositorBackend()
                                  : layers::LayersBackend::LAYERS_NONE;
  const char* backendName = GetLayersBackendName(backend);
  SetJSPropertyString(aCx, obj, "compositor", backendName);

  // If graphics isn't initialized yet, just stop now.
  if (!gfxPlatform::Initialized()) {
    return NS_OK;
  }

  DescribeFeatures(aCx, obj);
  return NS_OK;
}

nsresult GfxInfoBase::GetFeatureLog(JSContext* aCx, JS::MutableHandle<JS::Value> aOut)
{
  JS::Rooted<JSObject*> containerObj(aCx, JS_NewPlainObject(aCx));
  if (!containerObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*containerObj);

  JS::Rooted<JSObject*> featureArray(aCx, JS_NewArrayObject(aCx, 0));
  if (!featureArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Collect features.
  gfxConfig::ForEachFeature([&](const char* aName,
                                const char* aDescription,
                                FeatureState& aFeature) -> void {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return;
    }
    if (!SetJSPropertyString(aCx, obj, "name", aName) ||
        !SetJSPropertyString(aCx, obj, "description", aDescription) ||
        !SetJSPropertyString(aCx, obj, "status", FeatureStatusToString(aFeature.GetValue())))
    {
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

  JS::Rooted<JSObject*> fallbackArray(aCx, JS_NewArrayObject(aCx, 0));
  if (!fallbackArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Collect fallbacks.
  gfxConfig::ForEachFallback([&](const char* aName, const char* aMessage) -> void {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return;
    }

    if (!SetJSPropertyString(aCx, obj, "name", aName) ||
        !SetJSPropertyString(aCx, obj, "message", aMessage))
    {
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

bool
GfxInfoBase::BuildFeatureStateLog(JSContext* aCx, const FeatureState& aFeature,
                                  JS::MutableHandle<JS::Value> aOut)
{
  JS::Rooted<JSObject*> log(aCx, JS_NewArrayObject(aCx, 0));
  if (!log) {
    return false;
  }
  aOut.setObject(*log);

  aFeature.ForEachStatusChange([&](const char* aType,
                                   FeatureStatus aStatus,
                                   const char* aMessage) -> void {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return;
    }

    if (!SetJSPropertyString(aCx, obj, "type", aType) ||
        !SetJSPropertyString(aCx, obj, "status", FeatureStatusToString(aStatus)) ||
        (aMessage && !SetJSPropertyString(aCx, obj, "message", aMessage)))
    {
      return;
    }

    if (!AppendJSElement(aCx, log, obj)) {
      return;
    }
  });

  return true;
}

void
GfxInfoBase::DescribeFeatures(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  JS::Rooted<JSObject*> obj(aCx);
  gfx::FeatureStatus gpuProcess = gfxConfig::GetValue(Feature::GPU_PROCESS);
  InitFeatureObject(aCx, aObj, "gpuProcess", FEATURE_GPU_PROCESS, Some(gpuProcess), &obj);
}

bool
GfxInfoBase::InitFeatureObject(JSContext* aCx,
                               JS::Handle<JSObject*> aContainer,
                               const char* aName,
                               int32_t aFeature,
                               const Maybe<mozilla::gfx::FeatureStatus>& aFeatureStatus,
                               JS::MutableHandle<JSObject*> aOutObj)
{
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  nsCString failureId = NS_LITERAL_CSTRING("OK");
  int32_t unused;
  if (!NS_SUCCEEDED(GetFeatureStatus(aFeature, failureId, &unused))) {
    return false;
  }

  // Set "status".
  if (aFeatureStatus) {
    const char* status = FeatureStatusToString(aFeatureStatus.value());

    JS::Rooted<JSString*> str(aCx, JS_NewStringCopyZ(aCx, status));
    JS::Rooted<JS::Value> val(aCx, JS::StringValue(str));
    JS_SetProperty(aCx, obj, "status", val);
  }

  // Add the feature object to the container.
  {
    JS::Rooted<JS::Value> val(aCx, JS::ObjectValue(*obj));
    JS_SetProperty(aCx, aContainer, aName, val);
  }

  aOutObj.set(obj);
  return true;
}

nsresult
GfxInfoBase::GetActiveCrashGuards(JSContext* aCx, JS::MutableHandle<JS::Value> aOut)
{
  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setObject(*array);

  DriverCrashGuard::ForEachActiveCrashGuard([&](const char* aName,
                                                const char* aPrefName) -> void {
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
GfxInfoBase::GetWebRenderEnabled(bool* aWebRenderEnabled)
{
  *aWebRenderEnabled = gfxVars::UseWebRender();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetContentBackend(nsAString & aContentBackend)
{
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
GfxInfoBase::GetUsingGPUProcess(bool *aOutValue)
{
  GPUProcessManager* gpu = GPUProcessManager::Get();
  if (!gpu) {
    // Not supported in content processes.
    return NS_ERROR_FAILURE;
  }

  *aOutValue = !!gpu->GetGPUChild();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::ControlGPUProcessForXPCShell(bool aEnable, bool *_retval)
{
  gfxPlatform::GetPlatform();

  GPUProcessManager* gpm = GPUProcessManager::Get();
  if (aEnable) {
    if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
      gfxConfig::UserForceEnable(Feature::GPU_PROCESS, "xpcshell-test");
    }
    gpm->LaunchGPUProcess();
    gpm->EnsureGPUReady();
  } else {
    gpm->KillProcess();
  }

  *_retval = true;
  return NS_OK;
}

GfxInfoCollectorBase::GfxInfoCollectorBase()
{
  GfxInfoBase::AddCollector(this);
}

GfxInfoCollectorBase::~GfxInfoCollectorBase()
{
  GfxInfoBase::RemoveCollector(this);
}
