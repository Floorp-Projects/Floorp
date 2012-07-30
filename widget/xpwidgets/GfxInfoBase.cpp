/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "GfxInfoBase.h"

#include "GfxInfoWebGL.h"
#include "GfxDriverInfo.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Observer.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsTArray.h"
#include "mozilla/Preferences.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif

using namespace mozilla::widget;
using namespace mozilla;

nsTArray<GfxDriverInfo>* GfxInfoBase::mDriverInfo;
bool GfxInfoBase::mDriverInfoObserverInitialized;

// Observes for shutdown so that the child GfxDriverInfo list is freed.
class ShutdownObserver : public nsIObserver
{
public:
  ShutdownObserver() {}
  virtual ~ShutdownObserver() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports *subject, const char *aTopic,
                     const PRUnichar *aData)
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    delete GfxInfoBase::mDriverInfo;
    GfxInfoBase::mDriverInfo = nullptr;

    for (PRUint32 i = 0; i < DeviceFamilyMax; i++)
      delete GfxDriverInfo::mDeviceFamilies[i];

    for (PRUint32 i = 0; i < DeviceVendorMax; i++)
      delete GfxDriverInfo::mDeviceVendors[i];

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(ShutdownObserver, nsIObserver);

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
using namespace mozilla;

NS_IMPL_ISUPPORTS3(GfxInfoBase, nsIGfxInfo, nsIObserver, nsISupportsWeakReference)

#define BLACKLIST_PREF_BRANCH "gfx.blacklist."
#define SUGGESTED_VERSION_PREF BLACKLIST_PREF_BRANCH "suggested-driver-version"
#define BLACKLIST_ENTRY_TAG_NAME "gfxBlacklistEntry"

static const char*
GetPrefNameForFeature(PRInt32 aFeature)
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
    default:
      break;
  };

  return name;
}

// Returns the value of the pref for the relevant feature in aValue.
// If the pref doesn't exist, aValue is not touched, and returns false.
static bool
GetPrefValueForFeature(PRInt32 aFeature, PRInt32& aValue)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return false;

  aValue = false;
  return NS_SUCCEEDED(Preferences::GetInt(prefname, &aValue));
}

static void
SetPrefValueForFeature(PRInt32 aFeature, PRInt32 aValue)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return;

  Preferences::SetInt(prefname, aValue);
}

static void
RemovePrefForFeature(PRInt32 aFeature)
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

// <foo>Hello</foo> - "Hello" is stored as a child text node of the foo node.
static bool
BlacklistNodeToTextValue(nsIDOMNode *aBlacklistNode, nsAString& aValue)
{
  nsAutoString value;
  if (NS_FAILED(aBlacklistNode->GetTextContent(value)))
    return false;

  value.Trim(" \t\r\n");
  aValue = value;

  return true;
}

static OperatingSystem
BlacklistOSToOperatingSystem(const nsAString& os)
{
  if (os == NS_LITERAL_STRING("WINNT 5.1"))
    return DRIVER_OS_WINDOWS_XP;
  else if (os == NS_LITERAL_STRING("WINNT 5.2"))
    return DRIVER_OS_WINDOWS_SERVER_2003;
  else if (os == NS_LITERAL_STRING("WINNT 6.0"))
    return DRIVER_OS_WINDOWS_VISTA;
  else if (os == NS_LITERAL_STRING("WINNT 6.1"))
    return DRIVER_OS_WINDOWS_7;
  else if (os == NS_LITERAL_STRING("Linux"))
    return DRIVER_OS_LINUX;
  else if (os == NS_LITERAL_STRING("Darwin 9"))
    return DRIVER_OS_OS_X_10_5;
  else if (os == NS_LITERAL_STRING("Darwin 10"))
    return DRIVER_OS_OS_X_10_6;
  else if (os == NS_LITERAL_STRING("Darwin 11"))
    return DRIVER_OS_OS_X_10_7;
  else if (os == NS_LITERAL_STRING("Android"))
    return DRIVER_OS_ANDROID;
  else if (os == NS_LITERAL_STRING("All"))
    return DRIVER_OS_ALL;

  return DRIVER_OS_UNKNOWN;
}

static GfxDeviceFamily*
BlacklistDevicesToDeviceFamily(nsIDOMNodeList* aDevices)
{
  PRUint32 length;
  if (NS_FAILED(aDevices->GetLength(&length)))
    return nullptr;

  // For each <device>, get its device ID, and return a freshly-allocated
  // GfxDeviceFamily with the contents of that array.
  GfxDeviceFamily* deviceIds = new GfxDeviceFamily;

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> node;
    if (NS_FAILED(aDevices->Item(i, getter_AddRefs(node))) || !node)
      continue;

    nsAutoString deviceValue;
    if (!BlacklistNodeToTextValue(node, deviceValue))
      continue;

    deviceIds->AppendElement(deviceValue);
  }

  return deviceIds;
}

static PRInt32
BlacklistFeatureToGfxFeature(const nsAString& aFeature)
{
  if (aFeature == NS_LITERAL_STRING("DIRECT2D"))
    return nsIGfxInfo::FEATURE_DIRECT2D;
  else if (aFeature == NS_LITERAL_STRING("DIRECT3D_9_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS;
  else if (aFeature == NS_LITERAL_STRING("DIRECT3D_10_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS;
  else if (aFeature == NS_LITERAL_STRING("DIRECT3D_10_1_LAYERS"))
    return nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS;
  else if (aFeature == NS_LITERAL_STRING("OPENGL_LAYERS"))
    return nsIGfxInfo::FEATURE_OPENGL_LAYERS;
  else if (aFeature == NS_LITERAL_STRING("WEBGL_OPENGL"))
    return nsIGfxInfo::FEATURE_WEBGL_OPENGL;
  else if (aFeature == NS_LITERAL_STRING("WEBGL_ANGLE"))
    return nsIGfxInfo::FEATURE_WEBGL_ANGLE;
  else if (aFeature == NS_LITERAL_STRING("WEBGL_MSAA"))
    return nsIGfxInfo::FEATURE_WEBGL_MSAA;

  return 0;
}

static PRInt32
BlacklistFeatureStatusToGfxFeatureStatus(const nsAString& aStatus)
{
  if (aStatus == NS_LITERAL_STRING("NO_INFO"))
    return nsIGfxInfo::FEATURE_NO_INFO;
  else if (aStatus == NS_LITERAL_STRING("BLOCKED_DRIVER_VERSION"))
    return nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
  else if (aStatus == NS_LITERAL_STRING("BLOCKED_DEVICE"))
    return nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  else if (aStatus == NS_LITERAL_STRING("DISCOURAGED"))
    return nsIGfxInfo::FEATURE_DISCOURAGED;
  else if (aStatus == NS_LITERAL_STRING("BLOCKED_OS_VERSION"))
    return nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;

  // Do not allow it to set STATUS_UNKNOWN.

  return nsIGfxInfo::FEATURE_NO_INFO;
}

static VersionComparisonOp
BlacklistComparatorToComparisonOp(const nsAString& op)
{
  if (op == NS_LITERAL_STRING("LESS_THAN"))
    return DRIVER_LESS_THAN;
  else if (op == NS_LITERAL_STRING("LESS_THAN_OR_EQUAL"))
    return DRIVER_LESS_THAN_OR_EQUAL;
  else if (op == NS_LITERAL_STRING("GREATER_THAN"))
    return DRIVER_GREATER_THAN;
  else if (op == NS_LITERAL_STRING("GREATER_THAN_OR_EQUAL"))
    return DRIVER_GREATER_THAN_OR_EQUAL;
  else if (op == NS_LITERAL_STRING("EQUAL"))
    return DRIVER_EQUAL;
  else if (op == NS_LITERAL_STRING("NOT_EQUAL"))
    return DRIVER_NOT_EQUAL;
  else if (op == NS_LITERAL_STRING("BETWEEN_EXCLUSIVE"))
    return DRIVER_BETWEEN_EXCLUSIVE;
  else if (op == NS_LITERAL_STRING("BETWEEN_INCLUSIVE"))
    return DRIVER_BETWEEN_INCLUSIVE;
  else if (op == NS_LITERAL_STRING("BETWEEN_INCLUSIVE_START"))
    return DRIVER_BETWEEN_INCLUSIVE_START;

  return DRIVER_COMPARISON_IGNORED;
}

// Arbitrarily returns the first |tagname| child of |element|.
static bool
BlacklistNodeGetChildByName(nsIDOMElement *element,
                            const nsAString& tagname,
                            nsIDOMNode** firstchild)
{
  nsCOMPtr<nsIDOMNodeList> nodelist;
  if (NS_FAILED(element->GetElementsByTagName(tagname,
                                              getter_AddRefs(nodelist))) ||
      !nodelist) {
    return false;
  }

  nsCOMPtr<nsIDOMNode> node;
  if (NS_FAILED(nodelist->Item(0, getter_AddRefs(node))) || !node)
    return false;

  *firstchild = node.forget().get();
  return true;
}

/*

<gfxBlacklistEntry>
  <os>WINNT 6.0</os>
  <vendor>0x8086</vendor>
  <devices>
    <device>0x2582</device>
    <device>0x2782</device>
  </devices>
  <feature> DIRECT3D_10_LAYERS </feature>
  <featureStatus> BLOCKED_DRIVER_VERSION </featureStatus>
  <driverVersion> 8.52.322.2202 </driverVersion>
  <driverVersionComparator> LESS_THAN_OR_EQUAL </driverVersionComparator>
</gfxBlacklistEntry>

*/
static bool
BlacklistEntryToDriverInfo(nsIDOMNode* aBlacklistEntry,
                           GfxDriverInfo& aDriverInfo)
{
  nsAutoString nodename;
  if (NS_FAILED(aBlacklistEntry->GetNodeName(nodename)) ||
      nodename != NS_LITERAL_STRING(BLACKLIST_ENTRY_TAG_NAME)) {
    return false;
  }

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aBlacklistEntry);
  if (!element)
    return false;

  nsCOMPtr<nsIDOMNode> dataNode;
  nsAutoString dataValue;

  // <os>WINNT 6.0</os>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("os"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    aDriverInfo.mOperatingSystem = BlacklistOSToOperatingSystem(dataValue);
  }

  // <vendor>0x8086</vendor>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("vendor"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    aDriverInfo.mAdapterVendor = dataValue;
  }

  // <devices>
  //   <device>0x2582</device>
  //   <device>0x2782</device>
  // </devices>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("devices"),
                                  getter_AddRefs(dataNode))) {
    nsCOMPtr<nsIDOMElement> devicesElement = do_QueryInterface(dataNode);
    if (devicesElement) {

      // Get only the <device> nodes, because BlacklistDevicesToDeviceFamily
      // assumes it is passed no other nodes.
      nsCOMPtr<nsIDOMNodeList> devices;
      if (NS_SUCCEEDED(devicesElement->GetElementsByTagName(NS_LITERAL_STRING("device"),
                                                            getter_AddRefs(devices)))) {
        GfxDeviceFamily* deviceIds = BlacklistDevicesToDeviceFamily(devices);
        if (deviceIds) {
          // Get GfxDriverInfo to adopt the devices array we created.
          aDriverInfo.mDeleteDevices = true;
          aDriverInfo.mDevices = deviceIds;
        }
      }
    }
  }

  // <feature> DIRECT3D_10_LAYERS </feature>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("feature"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    aDriverInfo.mFeature = BlacklistFeatureToGfxFeature(dataValue);
  }

  // <featureStatus> BLOCKED_DRIVER_VERSION </featureStatus>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("featureStatus"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    aDriverInfo.mFeatureStatus = BlacklistFeatureStatusToGfxFeatureStatus(dataValue);
  }

  // <driverVersion> 8.52.322.2202 </driverVersion>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("driverVersion"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    PRUint64 version;
    if (ParseDriverVersion(dataValue, &version))
      aDriverInfo.mDriverVersion = version;
  }

  // <driverVersionComparator> LESS_THAN_OR_EQUAL </driverVersionComparator>
  if (BlacklistNodeGetChildByName(element, NS_LITERAL_STRING("driverVersionComparator"),
                                  getter_AddRefs(dataNode))) {
    BlacklistNodeToTextValue(dataNode, dataValue);
    aDriverInfo.mComparisonOp = BlacklistComparatorToComparisonOp(dataValue);
  }

  // We explicitly ignore unknown elements.

  return true;
}

static void
BlacklistEntriesToDriverInfo(nsIDOMNodeList* aBlacklistEntries,
                             nsTArray<GfxDriverInfo>& aDriverInfo)
{
  PRUint32 length;
  if (NS_FAILED(aBlacklistEntries->GetLength(&length)))
    return;

  aDriverInfo.Clear();
  aDriverInfo.SetLength(length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> blacklistEntry;
    if (NS_SUCCEEDED(aBlacklistEntries->Item(i,
                                             getter_AddRefs(blacklistEntry))) &&
        blacklistEntry) {
      GfxDriverInfo di;
      if (BlacklistEntryToDriverInfo(blacklistEntry, di)) {
        aDriverInfo[i] = di;
      }
      // Prevent di falling out of scope from destroying the devices.
      di.mDeleteDevices = false;
    }
  }
}

NS_IMETHODIMP
GfxInfoBase::Observe(nsISupports* aSubject, const char* aTopic,
                     const PRUnichar* aData)
{
  if (strcmp(aTopic, "blocklist-data-gfxItems") == 0) {
    nsCOMPtr<nsIDOMElement> gfxItems = do_QueryInterface(aSubject);
    if (gfxItems) {
      nsCOMPtr<nsIDOMNodeList> blacklistEntries;
      if (NS_SUCCEEDED(gfxItems->
            GetElementsByTagName(NS_LITERAL_STRING(BLACKLIST_ENTRY_TAG_NAME),
                                 getter_AddRefs(blacklistEntries))) &&
          blacklistEntries)
      {
        nsTArray<GfxDriverInfo> driverInfo;
        BlacklistEntriesToDriverInfo(blacklistEntries, driverInfo);
        EvaluateDownloadedBlacklist(driverInfo);
      }
    }
  }

  return NS_OK;
}

GfxInfoBase::GfxInfoBase()
    : mFailureCount(0)
{
}

GfxInfoBase::~GfxInfoBase()
{
}

nsresult
GfxInfoBase::Init()
{
  InitGfxDriverInfoShutdownObserver();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "blocklist-data-gfxItems", true);
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureStatus(PRInt32 aFeature, PRInt32* aStatus)
{
  if (GetPrefValueForFeature(aFeature, *aStatus))
    return NS_OK;

  nsString version;
  nsTArray<GfxDriverInfo> driverInfo;
  return GetFeatureStatusImpl(aFeature, aStatus, version, driverInfo);
}

PRInt32
GfxInfoBase::FindBlocklistedDeviceInList(const nsTArray<GfxDriverInfo>& info,
                                         nsAString& aSuggestedVersion,
                                         PRInt32 aFeature,
                                         OperatingSystem os)
{
  PRInt32 status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  nsAutoString adapterVendorID;
  nsAutoString adapterDeviceID;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(adapterVendorID)) ||
      NS_FAILED(GetAdapterDeviceID(adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
  {
    return NS_OK;
  }

  PRUint64 driverVersion;
  ParseDriverVersion(adapterDriverVersionString, &driverVersion);

  PRUint32 i = 0;
  for (; i < info.Length(); i++) {
    if (info[i].mOperatingSystem != DRIVER_OS_ALL &&
        info[i].mOperatingSystem != os)
    {
      continue;
    }

    if (!info[i].mAdapterVendor.Equals(GfxDriverInfo::GetDeviceVendor(VendorAll), nsCaseInsensitiveStringComparator()) &&
        !info[i].mAdapterVendor.Equals(adapterVendorID, nsCaseInsensitiveStringComparator())) {
      continue;
    }

    if (info[i].mDevices != GfxDriverInfo::allDevices && info[i].mDevices->Length()) {
        bool deviceMatches = false;
        for (PRUint32 j = 0; j < info[i].mDevices->Length(); j++) {
            if ((*info[i].mDevices)[j].Equals(adapterDeviceID, nsCaseInsensitiveStringComparator())) {
                deviceMatches = true;
                break;
            }
        }

        if (!deviceMatches) {
            continue;
        }
    }

    bool match = false;

#if defined(XP_WIN) || defined(ANDROID)
    switch (info[i].mComparisonOp) {
    case DRIVER_LESS_THAN:
      match = driverVersion < info[i].mDriverVersion;
      break;
    case DRIVER_LESS_THAN_OR_EQUAL:
      match = driverVersion <= info[i].mDriverVersion;
      break;
    case DRIVER_GREATER_THAN:
      match = driverVersion > info[i].mDriverVersion;
      break;
    case DRIVER_GREATER_THAN_OR_EQUAL:
      match = driverVersion >= info[i].mDriverVersion;
      break;
    case DRIVER_EQUAL:
      match = driverVersion == info[i].mDriverVersion;
      break;
    case DRIVER_NOT_EQUAL:
      match = driverVersion != info[i].mDriverVersion;
      break;
    case DRIVER_BETWEEN_EXCLUSIVE:
      match = driverVersion > info[i].mDriverVersion && driverVersion < info[i].mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE:
      match = driverVersion >= info[i].mDriverVersion && driverVersion <= info[i].mDriverVersionMax;
      break;
    case DRIVER_BETWEEN_INCLUSIVE_START:
      match = driverVersion >= info[i].mDriverVersion && driverVersion < info[i].mDriverVersionMax;
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
        break;
      }
    }
  }

  // Depends on Windows driver versioning. We don't pass a GfxDriverInfo object
  // back to the Windows handler, so we must handle this here.
#if defined(XP_WIN)
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

nsresult
GfxInfoBase::GetFeatureStatusImpl(PRInt32 aFeature,
                                  PRInt32* aStatus,
                                  nsAString& aSuggestedVersion,
                                  const nsTArray<GfxDriverInfo>& aDriverInfo,
                                  OperatingSystem* aOS /* = nullptr */)
{
  if (*aStatus != nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    // Terminate now with the status determined by the derived type (OS-specific
    // code).
    return NS_OK;
  }

  // If an operating system was provided by the derived GetFeatureStatusImpl,
  // grab it here. Otherwise, the OS is unknown.
  OperatingSystem os = DRIVER_OS_UNKNOWN;
  if (aOS)
    os = *aOS;

  nsAutoString adapterVendorID;
  nsAutoString adapterDeviceID;
  nsAutoString adapterDriverVersionString;
  if (NS_FAILED(GetAdapterVendorID(adapterVendorID)) ||
      NS_FAILED(GetAdapterDeviceID(adapterDeviceID)) ||
      NS_FAILED(GetAdapterDriverVersion(adapterDriverVersionString)))
  {
    return NS_OK;
  }

  PRUint64 driverVersion;
  ParseDriverVersion(adapterDriverVersionString, &driverVersion);

  // Check if the device is blocked from the downloaded blocklist. If not, check
  // the static list after that. This order is used so that we can later escape
  // out of static blocks (i.e. if we were wrong or something was patched, we
  // can back out our static block without doing a release).
  PRInt32 status;
  if (aDriverInfo.Length()) {
    status = FindBlocklistedDeviceInList(aDriverInfo, aSuggestedVersion, aFeature, os);
  } else {
    if (!mDriverInfo) {
      mDriverInfo = new nsTArray<GfxDriverInfo>();
    }
    status = FindBlocklistedDeviceInList(GetGfxDriverInfo(), aSuggestedVersion, aFeature, os);
  }

  // It's now done being processed. It's safe to set the status to NO_INFO.
  if (status == nsIGfxInfo::FEATURE_STATUS_UNKNOWN) {
    *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
  } else {
    *aStatus = status;
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureSuggestedDriverVersion(PRInt32 aFeature,
                                              nsAString& aVersion)
{
  nsCString version;
  if (GetPrefValueForDriverVersion(version)) {
    aVersion = NS_ConvertASCIItoUTF16(version);
    return NS_OK;
  }

  PRInt32 status;
  nsTArray<GfxDriverInfo> driverInfo;
  return GetFeatureStatusImpl(aFeature, &status, aVersion, driverInfo);
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
  PRInt32 features[] = {
    nsIGfxInfo::FEATURE_DIRECT2D,
    nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS,
    nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS,
    nsIGfxInfo::FEATURE_OPENGL_LAYERS,
    nsIGfxInfo::FEATURE_WEBGL_OPENGL,
    nsIGfxInfo::FEATURE_WEBGL_ANGLE,
    nsIGfxInfo::FEATURE_WEBGL_MSAA,
    0
  };

  // For every feature we know about, we evaluate whether this blacklist has a
  // non-NO_INFO status. If it does, we set the pref we evaluate in
  // GetFeatureStatus above, so we don't need to hold on to this blacklist
  // anywhere permanent.
  int i = 0;
  while (features[i]) {
    PRInt32 status;
    nsAutoString suggestedVersion;
    if (NS_SUCCEEDED(GetFeatureStatusImpl(features[i], &status,
                                          suggestedVersion,
                                          aDriverInfo))) {
      switch (status) {
        default:
        case nsIGfxInfo::FEATURE_NO_INFO:
          RemovePrefForFeature(features[i]);
          break;

        case nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION:
          if (!suggestedVersion.IsEmpty()) {
            SetPrefValueForDriverVersion(suggestedVersion);
          } else {
            RemovePrefForDriverVersion();
          }
          // FALLTHROUGH

        case nsIGfxInfo::FEATURE_BLOCKED_DEVICE:
        case nsIGfxInfo::FEATURE_DISCOURAGED:
        case nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION:
          SetPrefValueForFeature(features[i], status);
          break;
      }
    }

    ++i;
  }
}

NS_IMETHODIMP_(void)
GfxInfoBase::LogFailure(const nsACString &failure)
{
  /* We only keep the first 9 failures */
  if (mFailureCount < ArrayLength(mFailures)) {
    mFailures[mFailureCount++] = failure;

    /* record it in the crash notes too */
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AppendAppNotesToCrashReport(failure);
#endif
  }

}

/* void getFailures ([optional] out unsigned long failureCount, [array, size_is (failureCount), retval] out string failures); */
/* XPConnect method of returning arrays is very ugly. Would not recommend. Fallable nsMemory::Alloc makes things worse */
NS_IMETHODIMP GfxInfoBase::GetFailures(PRUint32 *failureCount, char ***failures)
{

  NS_ENSURE_ARG_POINTER(failureCount);
  NS_ENSURE_ARG_POINTER(failures);

  *failures = nullptr;
  *failureCount = mFailureCount;

  if (*failureCount != 0) {
    *failures = (char**)nsMemory::Alloc(*failureCount * sizeof(char*));
    if (!failures)
      return NS_ERROR_OUT_OF_MEMORY;

    /* copy over the failure messages into the array we just allocated */
    for (PRUint32 i = 0; i < *failureCount; i++) {
      nsCString& flattenedFailureMessage(mFailures[i]);
      (*failures)[i] = (char*)nsMemory::Clone(flattenedFailureMessage.get(), flattenedFailureMessage.Length() + 1);

      if (!(*failures)[i]) {
        /* <sarcasm> I'm too afraid to use an inline function... </sarcasm> */
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, (*failures));
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

nsresult GfxInfoBase::GetInfo(JSContext* aCx, jsval* aResult)
{
  InitCollectors();
  InfoObject obj(aCx);

  for (PRUint32 i = 0; i < sCollectors->Length(); i++) {
    (*sCollectors)[i]->GetInfo(obj);
  }

  // Some example property definitions
  // obj.DefineProperty("wordCacheSize", gfxTextRunWordCache::Count());
  // obj.DefineProperty("renderer", mRendererIDsString);
  // obj.DefineProperty("five", 5);

  if (!obj.mOk) {
    return NS_ERROR_FAILURE;
  }

  *aResult = OBJECT_TO_JSVAL(obj.mObj);
  return NS_OK;
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
  for (PRUint32 i = 0; i < sCollectors->Length(); i++) {
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

GfxInfoCollectorBase::GfxInfoCollectorBase()
{
  GfxInfoBase::AddCollector(this);
}

GfxInfoCollectorBase::~GfxInfoCollectorBase()
{
  GfxInfoBase::RemoveCollector(this);
}
