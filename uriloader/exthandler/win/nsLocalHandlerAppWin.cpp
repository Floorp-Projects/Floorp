/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLocalHandlerAppWin.h"
#include "nsString.h"
#include "nsIWindowsRegKey.h"
#include "nsILocalFileWin.h"
#include "nsComponentManagerUtils.h"

static nsresult GetPrettyNameFromFileDescription(
    const nsCOMPtr<nsILocalFileWin>& executableOnWindows,
    const nsString& assignedName, nsString& aName) {
  nsresult result = NS_ERROR_FAILURE;

  if (executableOnWindows) {
    result = executableOnWindows->GetVersionInfoField("FileDescription", aName);

    if (NS_FAILED(result) || aName.IsEmpty()) {
      if (!assignedName.IsEmpty()) {
        aName = assignedName;
      } else {
        result = executableOnWindows->GetLeafName(aName);
      }

      if (!aName.IsEmpty()) {
        result = NS_OK;
      } else {
        result = NS_ERROR_FAILURE;
      }
    }
  }

  return result;
}

static nsresult GetValueFromRegistry(nsString& aName,
                                     const nsCOMPtr<nsIWindowsRegKey>& appKey,
                                     const nsString& registryPath,
                                     const nsString& valueName) {
  nsresult rv =
      appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, registryPath,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);

  if (NS_SUCCEEDED(rv)) {
    nsAutoString applicationName;
    if (NS_SUCCEEDED(appKey->ReadStringValue(valueName, applicationName))) {
      aName = applicationName;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
};

std::function<nsresult(nsString&)>
nsLocalHandlerAppWin::GetPrettyNameOnNonMainThreadCallback() {
  // Make a copy of executable so that we don't have to worry about any other
  // threads
  nsCOMPtr<nsIFile> executable;
  mExecutable->Clone(getter_AddRefs(executable));

  // Get the windows interface to the file
  nsCOMPtr<nsILocalFileWin> executableOnWindows(do_QueryInterface(executable));
  auto appIdOrName = mAppIdOrName;
  auto assignedName = mName;

  std::function<nsresult(nsString&)> callback =
      [assignedName, appIdOrName,
       executableOnWindows =
           std::move(executableOnWindows)](nsString& aName) -> nsresult {
    // On all platforms, we want a human readable name for an application.
    // For example: msedge -> Microsoft Edge Browser
    //
    // This is generated on mac directly in nsLocalHandlerAppMac::GetName.
    // The auto-test coverage for GetName isn't thorough enough to be
    // confident that changing GetName on Windows won't cause problems.
    //
    // Besides that, this is a potentially slow thing to execute, and making
    // it asynchronous is preferable. There's a fallback to GetName() in the
    // nsLocalHandlerApp::PrettyNameAsync to cover Mac and Linux.

    if (appIdOrName.IsEmpty()) {
      return GetPrettyNameFromFileDescription(executableOnWindows, assignedName,
                                              aName);
    }

    nsCOMPtr<nsIWindowsRegKey> appKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1");
    if (!appKey) {
      return GetPrettyNameFromFileDescription(executableOnWindows, assignedName,
                                              aName);
    }

    // Check for ApplicationName first. Path:
    // HKEY_CLASSES_ROOT\${APP_ID}\Application, Value entry: ApplicationName
    nsresult rv =
        GetValueFromRegistry(aName, appKey, appIdOrName + u"\\Application"_ns,
                             u"ApplicationName"_ns);
    if (NS_SUCCEEDED(rv) && !aName.IsEmpty()) {
      return rv;
    }

    // Check for the default on the Applications entry next.
    // Path: HKEY_CLASSES_ROOT\Applications\${APP_ID}, Value entry: ""
    // (default)
    rv = GetValueFromRegistry(aName, appKey, u"Applications\\"_ns + appIdOrName,
                              u""_ns);
    if (NS_SUCCEEDED(rv) && !aName.IsEmpty()) {
      return rv;
    }

    // Fallthrough to getting the name from the file description
    return GetPrettyNameFromFileDescription(executableOnWindows, assignedName,
                                            aName);
  };

  return callback;
}
