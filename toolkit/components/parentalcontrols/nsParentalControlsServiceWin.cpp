/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsParentalControlsService.h"
#include "nsComponentManagerUtils.h"
#include "nsString.h"
#include "nsIArray.h"
#include "nsIWidget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFile.h"
#include "nsILocalFileWin.h"
#include "nsArrayUtils.h"
#include "nsIXULAppInfo.h"
#include "nsWindowsHelpers.h"
#include "nsIWindowsRegKey.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"

#include <sddl.h>

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsParentalControlsService, nsIParentalControlsService)

// Get the SID string for the user associated with this process's token.
static nsAutoString GetUserSid() {
  nsAutoString ret;
  HANDLE rawToken;
  BOOL success =
      ::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken);
  if (!success) {
    return ret;
  }
  nsAutoHandle token(rawToken);

  DWORD bufLen;
  success = ::GetTokenInformation(token, TokenUser, nullptr, 0, &bufLen);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return ret;
  }

  UniquePtr<char[]> buf = MakeUnique<char[]>(bufLen);
  success = ::GetTokenInformation(token, TokenUser, buf.get(), bufLen, &bufLen);
  MOZ_ASSERT(success);

  if (success) {
    TOKEN_USER* tokenUser = (TOKEN_USER*)(buf.get());
    PSID sid = tokenUser->User.Sid;
    LPWSTR sidStr;
    success = ::ConvertSidToStringSidW(sid, &sidStr);
    if (success) {
      ret = sidStr;
      ::LocalFree(sidStr);
    }
  }
  return ret;
}

nsParentalControlsService::nsParentalControlsService()
    : mEnabled(false), mProvider(0), mPC(nullptr) {
  // On at least some builds of Windows 10, the old parental controls API no
  // longer exists, so we have to pull the info we need out of the registry.
  if (IsWin10OrLater()) {
    nsAutoString regKeyName;
    regKeyName.AppendLiteral(
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Parental Controls\\"
        "Users\\");
    regKeyName.Append(GetUserSid());
    regKeyName.AppendLiteral("\\Web");

    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, regKeyName,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
      return;
    }

    uint32_t filterOn = 0;
    rv = regKey->ReadIntValue(u"Filter On"_ns, &filterOn);
    if (NS_FAILED(rv)) {
      return;
    }

    mEnabled = filterOn != 0;
    return;
  }

  HRESULT hr;
  CoInitialize(nullptr);
  hr = CoCreateInstance(__uuidof(WindowsParentalControls), nullptr,
                        CLSCTX_INPROC, IID_PPV_ARGS(&mPC));
  if (FAILED(hr)) return;

  RefPtr<IWPCSettings> wpcs;
  if (FAILED(mPC->GetUserSettings(nullptr, getter_AddRefs(wpcs)))) {
    // Not available on this os or not enabled for this user account or we're
    // running as admin
    mPC->Release();
    mPC = nullptr;
    return;
  }

  DWORD settings = 0;
  wpcs->GetRestrictions(&settings);

  // If we can't determine specifically whether Web Filtering is on/off (i.e.
  // we're on Windows < 8), then assume it's on unless no restrictions are set.
  bool enable = IsWin8OrLater() ? settings & WPCFLAG_WEB_FILTERED
                                : settings != WPCFLAG_NO_RESTRICTION;

  if (enable) {
    mEnabled = true;
  }
}

nsParentalControlsService::~nsParentalControlsService() {
  if (mPC) mPC->Release();

  if (mProvider) {
    EventUnregister(mProvider);
  }
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsParentalControlsService::GetParentalControlsEnabled(bool* aResult) {
  *aResult = false;

  if (mEnabled) *aResult = true;

  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::GetBlockFileDownloadsEnabled(bool* aResult) {
  *aResult = false;

  if (!mEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If we're on Windows 10 and we don't have the whole API available, then
  // we can't tell if file downloads are allowed, so assume they are to avoid
  // breaking downloads for every single user with parental controls.
  if (!mPC) {
    return NS_OK;
  }

  RefPtr<IWPCWebSettings> wpcws;
  if (SUCCEEDED(mPC->GetWebSettings(nullptr, getter_AddRefs(wpcws)))) {
    DWORD settings = 0;
    wpcws->GetSettings(&settings);
    if (settings == WPCFLAG_WEB_SETTING_DOWNLOADSBLOCKED) *aResult = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::GetLoggingEnabled(bool* aResult) {
  *aResult = false;

  if (!mEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If we're on Windows 10 and we don't have the whole API available, then
  // we don't know how logging should be done, so report that it's disabled.
  if (!mPC) {
    return NS_OK;
  }

  // Check the general purpose logging flag
  RefPtr<IWPCSettings> wpcs;
  if (SUCCEEDED(mPC->GetUserSettings(nullptr, getter_AddRefs(wpcs)))) {
    BOOL enabled = FALSE;
    wpcs->IsLoggingRequired(&enabled);
    if (enabled) *aResult = true;
  }

  return NS_OK;
}

// Post a log event to the system
NS_IMETHODIMP
nsParentalControlsService::Log(int16_t aEntryType, bool blocked,
                               nsIURI* aSource, nsIFile* aTarget) {
  if (!mEnabled) return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(aSource);

  // Confirm we should be logging
  bool enabled;
  GetLoggingEnabled(&enabled);
  if (!enabled) return NS_ERROR_NOT_AVAILABLE;

  // Register a Vista log event provider associated with the parental controls
  // channel.
  if (!mProvider) {
    if (EventRegister(&WPCPROV, nullptr, nullptr, &mProvider) != ERROR_SUCCESS)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  switch (aEntryType) {
    case ePCLog_URIVisit:
      // Not needed, Vista's web content filter handles this for us
      break;
    case ePCLog_FileDownload:
      LogFileDownload(blocked, aSource, aTarget);
      break;
    default:
      break;
  }

  return NS_OK;
}

//------------------------------------------------------------------------

// Sends a file download event to the Vista Event Log
void nsParentalControlsService::LogFileDownload(bool blocked, nsIURI* aSource,
                                                nsIFile* aTarget) {
  nsAutoCString curi;

  // Note, EventDataDescCreate is a macro defined in the headers, not a function

  aSource->GetSpec(curi);
  nsAutoString uri = NS_ConvertUTF8toUTF16(curi);

  // Get the name of the currently running process
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  nsAutoCString asciiAppName;
  if (appInfo) appInfo->GetName(asciiAppName);
  nsAutoString appName = NS_ConvertUTF8toUTF16(asciiAppName);

  static const WCHAR fill[] = L"";

  // See wpcevent.h and msdn for event formats
  EVENT_DATA_DESCRIPTOR eventData[WPC_ARGS_FILEDOWNLOADEVENT_CARGS];
  DWORD dwBlocked = blocked;

  EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_URL],
                      (const void*)uri.get(),
                      ((ULONG)uri.Length() + 1) * sizeof(WCHAR));
  EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_APPNAME],
                      (const void*)appName.get(),
                      ((ULONG)appName.Length() + 1) * sizeof(WCHAR));
  EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_VERSION],
                      (const void*)fill, sizeof(fill));
  EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_BLOCKED],
                      (const void*)&dwBlocked, sizeof(dwBlocked));

  if (aTarget) {  // May be null
    nsAutoString path;
    aTarget->GetPath(path);
    EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_PATH],
                        (const void*)path.get(),
                        ((ULONG)path.Length() + 1) * sizeof(WCHAR));
  } else {
    EventDataDescCreate(&eventData[WPC_ARGS_FILEDOWNLOADEVENT_PATH],
                        (const void*)fill, sizeof(fill));
  }

  EventWrite(mProvider, &WPCEVENT_WEB_FILEDOWNLOAD, ARRAYSIZE(eventData),
             eventData);
}

NS_IMETHODIMP
nsParentalControlsService::IsAllowed(int16_t aAction, nsIURI* aUri,
                                     bool* _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}
