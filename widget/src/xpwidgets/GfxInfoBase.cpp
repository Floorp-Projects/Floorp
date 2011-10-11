/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "GfxInfoBase.h"

#include "GfxInfoWebGL.h"
#include "GfxDriverInfo.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/Services.h"
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

using namespace mozilla;

extern "C" {
  void StoreSpline(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy);
  void CrashSpline(double tolerance, int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy);
}

static int crash_ax;
static int crash_ay;
static int crash_bx;
static int crash_by;
static int crash_cx;
static int crash_cy;
static int crash_dx;
static int crash_dy;

void
StoreSpline(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy) {
    crash_ax = ax;
    crash_ay = ay;
    crash_bx = bx;
    crash_by = by;
    crash_cx = cx;
    crash_cy = cy;
    crash_dx = dx;
    crash_dy = dy;
}

void
CrashSpline(double tolerance, int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy) {
#if defined(MOZ_CRASHREPORTER)
  static bool annotated;

  if (!annotated) {
    nsCAutoString note;

    note.AppendPrintf("curve ");
    note.AppendPrintf("%x ", crash_ax);
    note.AppendPrintf("%x, ", crash_ay);
    note.AppendPrintf("%x ", crash_bx);
    note.AppendPrintf("%x, ", crash_by);
    note.AppendPrintf("%x ", crash_cx);
    note.AppendPrintf("%x, ", crash_cy);
    note.AppendPrintf("%x ", crash_dx);
    note.AppendPrintf("%x\n", crash_dy);
    note.AppendPrintf("crv-crash(%f): ", tolerance);
    note.AppendPrintf("%x ", ax);
    note.AppendPrintf("%x, ", ay);
    note.AppendPrintf("%x ", bx);
    note.AppendPrintf("%x, ", by);
    note.AppendPrintf("%x ", cx);
    note.AppendPrintf("%x, ", cy);
    note.AppendPrintf("%x ", dx);
    note.AppendPrintf("%x\n", dy);

    CrashReporter::AppendAppNotesToCrashReport(note);
    annotated = true;
  }
#endif
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
  const char* name = nsnull;
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

  aValue = PR_FALSE;
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
  if (os == NS_LITERAL_STRING("WINNT 5.0"))
    return DRIVER_OS_WINDOWS_2000;
  else if (os == NS_LITERAL_STRING("WINNT 5.1"))
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
  else if (os == NS_LITERAL_STRING("All"))
    return DRIVER_OS_ALL;

  return DRIVER_OS_UNKNOWN;
}

static PRUint32
BlacklistHexToInt(const nsAString& aHex)
{
  PRInt32 err;
  // nsAString doesn't have .ToInteger() :(
  nsAutoString hex(aHex);
  PRInt32 value = hex.ToInteger(&err, 16);
  if (NS_FAILED(err))
    return 0;
  return (PRUint32) value;
}

static PRUint32*
BlacklistDevicesToDeviceFamily(nsIDOMNodeList* aDevices)
{
  PRUint32 length;
  if (NS_FAILED(aDevices->GetLength(&length)))
    return nsnull;

  // For each <device>, get its device ID, and return a freshly-allocated array
  // with the contents of that array.
  nsAutoArrayPtr<PRUint32> deviceIds(new PRUint32[length + 1]);
  memset(deviceIds, 0, sizeof(PRUint32) * (length + 1));

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> node;
    if (NS_FAILED(aDevices->Item(i, getter_AddRefs(node))) || !node)
      continue;

    nsAutoString deviceValue;
    if (!BlacklistNodeToTextValue(node, deviceValue))
      continue;

    deviceIds[i] = BlacklistHexToInt(deviceValue);
  }

  return deviceIds.forget();
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

  return DRIVER_UNKNOWN_COMPARISON;
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
    aDriverInfo.mAdapterVendor = BlacklistHexToInt(dataValue);
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
        PRUint32* deviceIds = BlacklistDevicesToDeviceFamily(devices);
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

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> blacklistEntry;
    if (NS_SUCCEEDED(aBlacklistEntries->Item(i,
                                             getter_AddRefs(blacklistEntry))) &&
        blacklistEntry) {
      GfxDriverInfo di;
      if (BlacklistEntryToDriverInfo(blacklistEntry, di)) {
        aDriverInfo.AppendElement(di);
      }
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
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "blocklist-data-gfxItems", PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureStatus(PRInt32 aFeature, PRInt32* aStatus NS_OUTPARAM)
{
  if (GetPrefValueForFeature(aFeature, *aStatus))
    return NS_OK;

  nsString version;
  return GetFeatureStatusImpl(aFeature, aStatus, version);
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureSuggestedDriverVersion(PRInt32 aFeature,
                                              nsAString& aVersion NS_OUTPARAM)
{
  nsCString version;
  if (GetPrefValueForDriverVersion(version)) {
    aVersion = NS_ConvertASCIItoUTF16(version);
    return NS_OK;
  }

  PRInt32 status;
  return GetFeatureStatusImpl(aFeature, &status, aVersion);
}


NS_IMETHODIMP
GfxInfoBase::GetWebGLParameter(const nsAString& aParam,
                               nsAString& aResult NS_OUTPARAM)
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
    0
  };

  // GetFeatureStatusImpl wants a zero-GfxDriverInfo terminated array. So, we
  // append that to our list.
  aDriverInfo.AppendElement(GfxDriverInfo());

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
                                          aDriverInfo.Elements()))) {
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
NS_IMETHODIMP GfxInfoBase::GetFailures(PRUint32 *failureCount NS_OUTPARAM, char ***failures NS_OUTPARAM)
{

  NS_ENSURE_ARG_POINTER(failureCount);
  NS_ENSURE_ARG_POINTER(failures);

  *failures = nsnull;
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
