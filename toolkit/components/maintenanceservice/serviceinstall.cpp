/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <aclapi.h>
#include <stdlib.h>
#include <shlwapi.h>

// Used for DNLEN and UNLEN
#include <lm.h>

#include <nsWindowsHelpers.h>
#include "mozilla/UniquePtr.h"

#include "serviceinstall.h"
#include "servicebase.h"
#include "updatehelper.h"
#include "shellapi.h"
#include "readstrings.h"
#include "updatererrors.h"
#include "commonupdatedir.h"

#pragma comment(lib, "version.lib")

// This uninstall key is defined originally in maintenanceservice_installer.nsi
#define MAINT_UNINSTALL_KEY                                                    \
  L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MozillaMaintenan" \
  L"ceService"

static BOOL UpdateUninstallerVersionString(LPWSTR versionString) {
  HKEY uninstallKey;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, MAINT_UNINSTALL_KEY, 0,
                    KEY_WRITE | KEY_WOW64_32KEY,
                    &uninstallKey) != ERROR_SUCCESS) {
    return FALSE;
  }

  LONG rv = RegSetValueExW(uninstallKey, L"DisplayVersion", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(versionString),
                           (wcslen(versionString) + 1) * sizeof(WCHAR));
  RegCloseKey(uninstallKey);
  return rv == ERROR_SUCCESS;
}

/**
 * A wrapper function to read strings for the maintenance service.
 *
 * @param path    The path of the ini file to read from
 * @param results The maintenance service strings that were read
 * @return OK on success
 */
static int ReadMaintenanceServiceStrings(
    LPCWSTR path, MaintenanceServiceStringTable* results) {
  // Read in the maintenance service description string if specified.
  const unsigned int kNumStrings = 1;
  const char* kServiceKeys = "MozillaMaintenanceDescription\0";
  char serviceStrings[kNumStrings][MAX_TEXT_LEN];
  int result = ReadStrings(path, kServiceKeys, kNumStrings, serviceStrings);
  if (result != OK) {
    serviceStrings[0][0] = '\0';
  }
  strncpy(results->serviceDescription, serviceStrings[0], MAX_TEXT_LEN - 1);
  results->serviceDescription[MAX_TEXT_LEN - 1] = '\0';
  return result;
}

/**
 * Obtains the version number from the specified PE file's version information
 * Version Format: A.B.C.D (Example 10.0.0.300)
 *
 * @param  path The path of the file to check the version on
 * @param  A    The first part of the version number
 * @param  B    The second part of the version number
 * @param  C    The third part of the version number
 * @param  D    The fourth part of the version number
 * @return TRUE if successful
 */
static BOOL GetVersionNumberFromPath(LPWSTR path, DWORD& A, DWORD& B, DWORD& C,
                                     DWORD& D) {
  DWORD fileVersionInfoSize = GetFileVersionInfoSizeW(path, 0);
  mozilla::UniquePtr<char[]> fileVersionInfo(new char[fileVersionInfoSize]);
  if (!GetFileVersionInfoW(path, 0, fileVersionInfoSize,
                           fileVersionInfo.get())) {
    LOG_WARN(
        ("Could not obtain file info of old service.  (%d)", GetLastError()));
    return FALSE;
  }

  VS_FIXEDFILEINFO* fixedFileInfo =
      reinterpret_cast<VS_FIXEDFILEINFO*>(fileVersionInfo.get());
  UINT size;
  if (!VerQueryValueW(fileVersionInfo.get(), L"\\",
                      reinterpret_cast<LPVOID*>(&fixedFileInfo), &size)) {
    LOG_WARN(("Could not query file version info of old service.  (%d)",
              GetLastError()));
    return FALSE;
  }

  A = HIWORD(fixedFileInfo->dwFileVersionMS);
  B = LOWORD(fixedFileInfo->dwFileVersionMS);
  C = HIWORD(fixedFileInfo->dwFileVersionLS);
  D = LOWORD(fixedFileInfo->dwFileVersionLS);
  return TRUE;
}

/**
 * Updates the service description with what is stored in updater.ini
 * at the same path as the currently executing module binary.
 *
 * @param serviceHandle A handle to an opened service with
 *                      SERVICE_CHANGE_CONFIG access right
 * @param TRUE on succcess.
 */
BOOL UpdateServiceDescription(SC_HANDLE serviceHandle) {
  WCHAR updaterINIPath[MAX_PATH + 1];
  if (!GetModuleFileNameW(nullptr, updaterINIPath,
                          sizeof(updaterINIPath) / sizeof(updaterINIPath[0]))) {
    LOG_WARN(
        ("Could not obtain module filename when attempting to "
         "modify service description.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (!PathRemoveFileSpecW(updaterINIPath)) {
    LOG_WARN(
        ("Could not remove file spec when attempting to "
         "modify service description.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (!PathAppendSafe(updaterINIPath, L"updater.ini")) {
    LOG_WARN(
        ("Could not append updater.ini filename when attempting to "
         "modify service description.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (GetFileAttributesW(updaterINIPath) == INVALID_FILE_ATTRIBUTES) {
    LOG_WARN(
        ("updater.ini file does not exist, will not modify "
         "service description.  (%d)",
         GetLastError()));
    return FALSE;
  }

  MaintenanceServiceStringTable serviceStrings;
  int rv = ReadMaintenanceServiceStrings(updaterINIPath, &serviceStrings);
  if (rv != OK || !strlen(serviceStrings.serviceDescription)) {
    LOG_WARN(
        ("updater.ini file does not contain a maintenance "
         "service description."));
    return FALSE;
  }

  WCHAR serviceDescription[MAX_TEXT_LEN];
  if (!MultiByteToWideChar(
          CP_UTF8, 0, serviceStrings.serviceDescription, -1, serviceDescription,
          sizeof(serviceDescription) / sizeof(serviceDescription[0]))) {
    LOG_WARN(("Could not convert description to wide string format.  (%d)",
              GetLastError()));
    return FALSE;
  }

  SERVICE_DESCRIPTIONW descriptionConfig;
  descriptionConfig.lpDescription = serviceDescription;
  if (!ChangeServiceConfig2W(serviceHandle, SERVICE_CONFIG_DESCRIPTION,
                             &descriptionConfig)) {
    LOG_WARN(("Could not change service config.  (%d)", GetLastError()));
    return FALSE;
  }

  LOG(("The service description was updated successfully."));
  return TRUE;
}

/**
 * Determines if the MozillaMaintenance service path needs to be updated
 * and fixes it if it is wrong.
 *
 * @param service             A handle to the service to fix.
 * @param currentServicePath  The current (possibly wrong) path that is used.
 * @param servicePathWasWrong Out parameter set to TRUE if a fix was needed.
 * @return TRUE if the service path is now correct.
 */
BOOL FixServicePath(SC_HANDLE service, LPCWSTR currentServicePath,
                    BOOL& servicePathWasWrong) {
  // When we originally upgraded the MozillaMaintenance service we
  // would uninstall the service on each upgrade.  This had an
  // intermittent error which could cause the service to use the file
  // maintenanceservice_tmp.exe as the install path.  Only a small number
  // of Nightly users would be affected by this, but we check for this
  // state here and fix the user if they are affected.
  //
  // We also fix the path in the case of the path not being quoted.
  size_t currentServicePathLen = wcslen(currentServicePath);
  bool doesServiceHaveCorrectPath =
      currentServicePathLen > 2 &&
      !wcsstr(currentServicePath, L"maintenanceservice_tmp.exe") &&
      currentServicePath[0] == L'\"' &&
      currentServicePath[currentServicePathLen - 1] == L'\"';

  if (doesServiceHaveCorrectPath) {
    LOG(("The MozillaMaintenance service path is correct."));
    servicePathWasWrong = FALSE;
    return TRUE;
  }
  // This is a recoverable situation so not logging as a warning
  LOG(("The MozillaMaintenance path is NOT correct. It was: %ls",
       currentServicePath));

  servicePathWasWrong = TRUE;
  WCHAR fixedPath[MAX_PATH + 1] = {L'\0'};
  wcsncpy(fixedPath, currentServicePath, MAX_PATH);
  PathUnquoteSpacesW(fixedPath);
  if (!PathRemoveFileSpecW(fixedPath)) {
    LOG_WARN(("Couldn't remove file spec.  (%d)", GetLastError()));
    return FALSE;
  }
  if (!PathAppendSafe(fixedPath, L"maintenanceservice.exe")) {
    LOG_WARN(("Couldn't append file spec.  (%d)", GetLastError()));
    return FALSE;
  }
  PathQuoteSpacesW(fixedPath);

  if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                            SERVICE_NO_CHANGE, fixedPath, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr)) {
    LOG_WARN(("Could not fix service path.  (%d)", GetLastError()));
    return FALSE;
  }

  LOG(("Fixed service path to: %ls.", fixedPath));
  return TRUE;
}

/**
 * Installs or upgrades the SVC_NAME service.
 * If an existing service is already installed, we replace it with the
 * currently running process.
 *
 * @param  action The action to perform.
 * @return TRUE if the service was installed/upgraded
 */
BOOL SvcInstall(SvcInstallAction action) {
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(
      OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG_WARN(("Could not open service manager.  (%d)", GetLastError()));
    return FALSE;
  }

  WCHAR newServiceBinaryPath[MAX_PATH + 1];
  if (!GetModuleFileNameW(
          nullptr, newServiceBinaryPath,
          sizeof(newServiceBinaryPath) / sizeof(newServiceBinaryPath[0]))) {
    LOG_WARN(
        ("Could not obtain module filename when attempting to "
         "install service.  (%d)",
         GetLastError()));
    return FALSE;
  }

  // Check if we already have the service installed.
  nsAutoServiceHandle schService(
      OpenServiceW(schSCManager, SVC_NAME, SERVICE_ALL_ACCESS));
  DWORD lastError = GetLastError();
  if (!schService && ERROR_SERVICE_DOES_NOT_EXIST != lastError) {
    // The service exists but we couldn't open it
    LOG_WARN(("Could not open service.  (%d)", GetLastError()));
    return FALSE;
  }

  if (schService) {
    // The service exists but it may not have the correct permissions.
    // This could happen if the permissions were not set correctly originally
    // or have been changed after the installation.  This will reset the
    // permissions back to allow limited user accounts.
    if (!SetUserAccessServiceDACL(schService)) {
      LOG_WARN(
          ("Could not reset security ACE on service handle. It might not be "
           "possible to start the service. This error should never "
           "happen.  (%d)",
           GetLastError()));
    }

    // The service exists and we opened it
    DWORD bytesNeeded;
    if (!QueryServiceConfigW(schService, nullptr, 0, &bytesNeeded) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG_WARN(
          ("Could not determine buffer size for query service config.  (%d)",
           GetLastError()));
      return FALSE;
    }

    // Get the service config information, in particular we want the binary
    // path of the service.
    mozilla::UniquePtr<char[]> serviceConfigBuffer(new char[bytesNeeded]);
    if (!QueryServiceConfigW(
            schService,
            reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get()),
            bytesNeeded, &bytesNeeded)) {
      LOG_WARN(("Could open service but could not query service config.  (%d)",
                GetLastError()));
      return FALSE;
    }
    QUERY_SERVICE_CONFIGW& serviceConfig =
        *reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get());

    // Check if we need to fix the service path
    BOOL servicePathWasWrong;
    static BOOL alreadyCheckedFixServicePath = FALSE;
    if (!alreadyCheckedFixServicePath) {
      if (!FixServicePath(schService, serviceConfig.lpBinaryPathName,
                          servicePathWasWrong)) {
        LOG_WARN(("Could not fix service path. This should never happen.  (%d)",
                  GetLastError()));
        // True is returned because the service is pointing to
        // maintenanceservice_tmp.exe so it actually was upgraded to the
        // newest installed service.
        return TRUE;
      } else if (servicePathWasWrong) {
        // Now that the path is fixed we should re-attempt the install.
        // This current process' image path is maintenanceservice_tmp.exe.
        // The service used to point to maintenanceservice_tmp.exe.
        // The service was just fixed to point to maintenanceservice.exe.
        // Re-attempting an install from scratch will work as normal.
        alreadyCheckedFixServicePath = TRUE;
        LOG(("Restarting install action: %d", action));
        return SvcInstall(action);
      }
    }

    // Ensure the service path is not quoted. We own this memory and know it to
    // be large enough for the quoted path, so it is large enough for the
    // unquoted path.  This function cannot fail.
    PathUnquoteSpacesW(serviceConfig.lpBinaryPathName);

    // Obtain the existing maintenanceservice file's version number and
    // the new file's version number.  Versions are in the format of
    // A.B.C.D.
    DWORD existingA, existingB, existingC, existingD;
    DWORD newA, newB, newC, newD;
    BOOL obtainedExistingVersionInfo =
        GetVersionNumberFromPath(serviceConfig.lpBinaryPathName, existingA,
                                 existingB, existingC, existingD);
    if (!GetVersionNumberFromPath(newServiceBinaryPath, newA, newB, newC,
                                  newD)) {
      LOG_WARN(("Could not obtain version number from new path"));
      return FALSE;
    }

    // Check if we need to replace the old binary with the new one
    // If we couldn't get the old version info then we assume we should
    // replace it.
    if (ForceInstallSvc == action || !obtainedExistingVersionInfo ||
        (existingA < newA) || (existingA == newA && existingB < newB) ||
        (existingA == newA && existingB == newB && existingC < newC) ||
        (existingA == newA && existingB == newB && existingC == newC &&
         existingD < newD)) {
      // We have a newer updater, so update the description from the INI file.
      UpdateServiceDescription(schService);

      schService.reset();
      if (!StopService()) {
        return FALSE;
      }

      if (!wcscmp(newServiceBinaryPath, serviceConfig.lpBinaryPathName)) {
        LOG(
            ("File is already in the correct location, no action needed for "
             "upgrade.  The path is: \"%ls\"",
             newServiceBinaryPath));
        return TRUE;
      }

      BOOL result = TRUE;

      // Attempt to copy the new binary over top the existing binary.
      // If there is an error we try to move it out of the way and then
      // copy it in.  First try the safest / easiest way to overwrite the file.
      if (!CopyFileW(newServiceBinaryPath, serviceConfig.lpBinaryPathName,
                     FALSE)) {
        LOG_WARN(
            ("Could not overwrite old service binary file. "
             "This should never happen, but if it does the next "
             "upgrade will fix it, the service is not a critical "
             "component that needs to be installed for upgrades "
             "to work.  (%d)",
             GetLastError()));

        // We rename the last 3 filename chars in an unsafe way.  Manually
        // verify there are more than 3 chars for safe failure in MoveFileExW.
        const size_t len = wcslen(serviceConfig.lpBinaryPathName);
        if (len > 3) {
          // Calculate the temp file path that we're moving the file to. This
          // is the same as the proper service path but with a .old extension.
          LPWSTR oldServiceBinaryTempPath = new WCHAR[len + 1];
          memset(oldServiceBinaryTempPath, 0, (len + 1) * sizeof(WCHAR));
          wcsncpy(oldServiceBinaryTempPath, serviceConfig.lpBinaryPathName,
                  len);
          // Rename the last 3 chars to 'old'
          wcsncpy(oldServiceBinaryTempPath + len - 3, L"old", 3);

          // Move the current (old) service file to the temp path.
          if (MoveFileExW(serviceConfig.lpBinaryPathName,
                          oldServiceBinaryTempPath,
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            // The old binary is moved out of the way, copy in the new one.
            if (!CopyFileW(newServiceBinaryPath, serviceConfig.lpBinaryPathName,
                           FALSE)) {
              // It is best to leave the old service binary in this condition.
              LOG_WARN(
                  ("The new service binary could not be copied in."
                   " The service will not be upgraded."));
              result = FALSE;
            } else {
              LOG(
                  ("The new service binary was copied in by first moving the"
                   " old one out of the way."));
            }

            // Attempt to get rid of the old service temp path.
            if (DeleteFileW(oldServiceBinaryTempPath)) {
              LOG(("The old temp service path was deleted: %ls.",
                   oldServiceBinaryTempPath));
            } else {
              // The old temp path could not be removed.  It will be removed
              // the next time the user can't copy the binary in or on
              // uninstall.
              LOG_WARN(("The old temp service path was not deleted."));
            }
          } else {
            // It is best to leave the old service binary in this condition.
            LOG_WARN(
                ("Could not move old service file out of the way from:"
                 " \"%ls\" to \"%ls\". Service will not be upgraded.  (%d)",
                 serviceConfig.lpBinaryPathName, oldServiceBinaryTempPath,
                 GetLastError()));
            result = FALSE;
          }
          delete[] oldServiceBinaryTempPath;
        } else {
          // It is best to leave the old service binary in this condition.
          LOG_WARN(
              ("Service binary path was less than 3, service will"
               " not be updated.  This should never happen."));
          result = FALSE;
        }
      } else {
        WCHAR versionStr[128] = {L'\0'};
        swprintf(versionStr, 128, L"%d.%d.%d.%d", newA, newB, newC, newD);
        if (!UpdateUninstallerVersionString(versionStr)) {
          LOG(("The uninstaller version string could not be updated."));
        }
        LOG(("The new service binary was copied in."));
      }

      // We made a copy of ourselves to the existing location.
      // The tmp file (the process of which we are executing right now) will be
      // left over.  Attempt to delete the file on the next reboot.
      if (MoveFileExW(newServiceBinaryPath, nullptr,
                      MOVEFILE_DELAY_UNTIL_REBOOT)) {
        LOG(("Deleting the old file path on the next reboot: %ls.",
             newServiceBinaryPath));
      } else {
        LOG_WARN(("Call to delete the old file path failed: %ls.",
                  newServiceBinaryPath));
      }

      return result;
    }

    // We don't need to copy ourselves to the existing location.
    // The tmp file (the process of which we are executing right now) will be
    // left over.  Attempt to delete the file on the next reboot.
    MoveFileExW(newServiceBinaryPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    // nothing to do, we already have a newer service installed
    return TRUE;
  }

  // If the service does not exist and we are upgrading, don't install it.
  if (UpgradeSvc == action) {
    // The service does not exist and we are upgrading, so don't install it
    return TRUE;
  }

  // Quote the path only if it contains spaces.
  PathQuoteSpacesW(newServiceBinaryPath);
  // The service does not already exist so create the service as on demand
  schService.own(CreateServiceW(
      schSCManager, SVC_NAME, SVC_DISPLAY_NAME, SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
      newServiceBinaryPath, nullptr, nullptr, nullptr, nullptr, nullptr));
  if (!schService) {
    LOG_WARN(
        ("Could not create Windows service. "
         "This error should never happen since a service install "
         "should only be called when elevated.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (!SetUserAccessServiceDACL(schService)) {
    LOG_WARN(
        ("Could not set security ACE on service handle, the service will not "
         "be able to be started from unelevated processes. "
         "This error should never happen.  (%d)",
         GetLastError()));
  }

  UpdateServiceDescription(schService);

  return TRUE;
}

/**
 * Stops the Maintenance service.
 *
 * @return TRUE if successful.
 */
BOOL StopService() {
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(
      OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG_WARN(("Could not open service manager.  (%d)", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(
      OpenServiceW(schSCManager, SVC_NAME, SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG_WARN(("Could not open service.  (%d)", GetLastError()));
    return FALSE;
  }

  LOG(("Sending stop request..."));
  SERVICE_STATUS status;
  SetLastError(ERROR_SUCCESS);
  if (!ControlService(schService, SERVICE_CONTROL_STOP, &status) &&
      GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
    LOG_WARN(("Error sending stop request.  (%d)", GetLastError()));
  }

  schSCManager.reset();
  schService.reset();

  LOG(("Waiting for service stop..."));
  DWORD lastState = WaitForServiceStop(SVC_NAME, 30);

  // The service can be in a stopped state but the exe still in use
  // so make sure the process is really gone before proceeding
  WaitForProcessExit(L"maintenanceservice.exe", 30);
  LOG(("Done waiting for service stop, last service state: %d", lastState));

  return lastState == SERVICE_STOPPED;
}

/**
 * Uninstalls the Maintenance service.
 *
 * @return TRUE if successful.
 */
BOOL SvcUninstall() {
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(
      OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG_WARN(("Could not open service manager.  (%d)", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(
      OpenServiceW(schSCManager, SVC_NAME, SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG_WARN(("Could not open service.  (%d)", GetLastError()));
    return FALSE;
  }

  // Stop the service so it deletes faster and so the uninstaller
  // can actually delete its EXE.
  DWORD totalWaitTime = 0;
  SERVICE_STATUS status;
  static const int maxWaitTime = 1000 * 60;  // Never wait more than a minute
  if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
    do {
      Sleep(status.dwWaitHint);
      totalWaitTime += (status.dwWaitHint + 10);
      if (status.dwCurrentState == SERVICE_STOPPED) {
        break;
      } else if (totalWaitTime > maxWaitTime) {
        break;
      }
    } while (QueryServiceStatus(schService, &status));
  }

  // Delete the service or mark it for deletion
  BOOL deleted = DeleteService(schService);
  if (!deleted) {
    deleted = (GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE);
  }

  return deleted;
}

/**
 * Sets the access control list for user access for the specified service.
 *
 * @param  hService The service to set the access control list on
 * @return TRUE if successful
 */
BOOL SetUserAccessServiceDACL(SC_HANDLE hService) {
  PACL pNewAcl = nullptr;
  PSECURITY_DESCRIPTOR psd = nullptr;
  DWORD lastError = SetUserAccessServiceDACL(hService, pNewAcl, psd);
  if (pNewAcl) {
    LocalFree((HLOCAL)pNewAcl);
  }
  if (psd) {
    LocalFree((LPVOID)psd);
  }
  return ERROR_SUCCESS == lastError;
}

/**
 * Sets the access control list for user access for the specified service.
 *
 * @param  hService  The service to set the access control list on
 * @param  pNewAcl   The out param ACL which should be freed by caller
 * @param  psd       out param security descriptor, should be freed by caller
 * @return ERROR_SUCCESS if successful
 */
DWORD
SetUserAccessServiceDACL(SC_HANDLE hService, PACL& pNewAcl,
                         PSECURITY_DESCRIPTOR psd) {
  // Get the current security descriptor needed size
  DWORD needed = 0;
  if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, &psd, 0,
                                  &needed)) {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG_WARN(("Could not query service object security size.  (%d)",
                GetLastError()));
      return GetLastError();
    }

    DWORD size = needed;
    psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, size);
    if (!psd) {
      LOG_WARN(
          ("Could not allocate security descriptor.  (%d)", GetLastError()));
      return ERROR_INSUFFICIENT_BUFFER;
    }

    // Get the actual security descriptor now
    if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, psd,
                                    size, &needed)) {
      LOG_WARN(
          ("Could not allocate security descriptor.  (%d)", GetLastError()));
      return GetLastError();
    }
  }

  // Get the current DACL from the security descriptor.
  PACL pacl = nullptr;
  BOOL bDaclPresent = FALSE;
  BOOL bDaclDefaulted = FALSE;
  if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, &bDaclDefaulted)) {
    LOG_WARN(("Could not obtain DACL.  (%d)", GetLastError()));
    return GetLastError();
  }

  PSID sid;
  DWORD SIDSize = SECURITY_MAX_SID_SIZE;
  sid = LocalAlloc(LMEM_FIXED, SIDSize);
  if (!sid) {
    LOG_WARN(("Could not allocate SID memory.  (%d)", GetLastError()));
    return GetLastError();
  }

  if (!CreateWellKnownSid(WinBuiltinUsersSid, nullptr, sid, &SIDSize)) {
    DWORD lastError = GetLastError();
    LOG_WARN(("Could not create well known SID.  (%d)", lastError));
    LocalFree(sid);
    return lastError;
  }

  // Lookup the account name, the function fails if you don't pass in
  // a buffer for the domain name but it's not used since we're using
  // the built in account Sid.
  SID_NAME_USE accountType;
  WCHAR accountName[UNLEN + 1] = {L'\0'};
  WCHAR domainName[DNLEN + 1] = {L'\0'};
  DWORD accountNameSize = UNLEN + 1;
  DWORD domainNameSize = DNLEN + 1;
  if (!LookupAccountSidW(nullptr, sid, accountName, &accountNameSize,
                         domainName, &domainNameSize, &accountType)) {
    LOG_WARN(("Could not lookup account Sid, will try Users.  (%d)",
              GetLastError()));
    wcsncpy(accountName, L"Users", UNLEN);
  }

  // We already have the group name so we can get rid of the SID
  FreeSid(sid);
  sid = nullptr;

  // Build the ACE, BuildExplicitAccessWithName cannot fail so it is not logged.
  EXPLICIT_ACCESS ea;
  BuildExplicitAccessWithNameW(&ea, accountName,
                               SERVICE_START | SERVICE_STOP | GENERIC_READ,
                               SET_ACCESS, NO_INHERITANCE);
  DWORD lastError = SetEntriesInAclW(1, (PEXPLICIT_ACCESS)&ea, pacl, &pNewAcl);
  if (ERROR_SUCCESS != lastError) {
    LOG_WARN(("Could not set entries in ACL.  (%d)", lastError));
    return lastError;
  }

  // Initialize a new security descriptor.
  SECURITY_DESCRIPTOR sd;
  if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    LOG_WARN(
        ("Could not initialize security descriptor.  (%d)", GetLastError()));
    return GetLastError();
  }

  // Set the new DACL in the security descriptor.
  if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE)) {
    LOG_WARN(("Could not set security descriptor DACL.  (%d)", GetLastError()));
    return GetLastError();
  }

  // Set the new security descriptor for the service object.
  if (!SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, &sd)) {
    LOG_WARN(("Could not set object security.  (%d)", GetLastError()));
    return GetLastError();
  }

  // Woohoo, raise the roof
  LOG(("User access was set successfully on the service."));
  return ERROR_SUCCESS;
}
