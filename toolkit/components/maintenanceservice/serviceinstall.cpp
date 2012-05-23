/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <aclapi.h>
#include <stdlib.h>
#include <shlwapi.h>

// Used for DNLEN and UNLEN
#include <lm.h>

#include <nsAutoPtr.h>
#include <nsWindowsHelpers.h>
#include <nsMemory.h>

#include "serviceinstall.h"
#include "servicebase.h"
#include "updatehelper.h"
#include "shellapi.h"
#include "readstrings.h"
#include "errors.h"

#pragma comment(lib, "version.lib")

/**
 * A wrapper function to read strings for the maintenance service.
 *
 * @param path    The path of the ini file to read from
 * @param results The maintenance service strings that were read
 * @return OK on success
*/
static int
ReadMaintenanceServiceStrings(LPCWSTR path, 
                              MaintenanceServiceStringTable *results)
{
  // Read in the maintenance service description string if specified.
  const unsigned int kNumStrings = 1;
  const char *kServiceKeys = "MozillaMaintenanceDescription\0";
  char serviceStrings[kNumStrings][MAX_TEXT_LEN];
  int result = ReadStrings(path, kServiceKeys, 
                           kNumStrings, serviceStrings);
  if (result != OK) {
    serviceStrings[0][0] = '\0';
  }
  strncpy(results->serviceDescription, 
          serviceStrings[0], MAX_TEXT_LEN - 1);
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
static BOOL
GetVersionNumberFromPath(LPWSTR path, DWORD &A, DWORD &B, 
                         DWORD &C, DWORD &D) 
{
  DWORD fileVersionInfoSize = GetFileVersionInfoSizeW(path, 0);
  nsAutoArrayPtr<char> fileVersionInfo = new char[fileVersionInfoSize];
  if (!GetFileVersionInfoW(path, 0, fileVersionInfoSize,
                           fileVersionInfo.get())) {
      LOG(("Could not obtain file info of old service.  (%d)\n", 
           GetLastError()));
      return FALSE;
  }

  VS_FIXEDFILEINFO *fixedFileInfo = 
    reinterpret_cast<VS_FIXEDFILEINFO *>(fileVersionInfo.get());
  UINT size;
  if (!VerQueryValueW(fileVersionInfo.get(), L"\\", 
    reinterpret_cast<LPVOID*>(&fixedFileInfo), &size)) {
      LOG(("Could not query file version info of old service.  (%d)\n", 
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
BOOL
UpdateServiceDescription(SC_HANDLE serviceHandle)
{
  WCHAR updaterINIPath[MAX_PATH + 1];
  if (!GetModuleFileNameW(NULL, updaterINIPath, 
                          sizeof(updaterINIPath) /
                          sizeof(updaterINIPath[0]))) {
    LOG(("Could not obtain module filename when attempting to "
         "modify service description. (%d)\n", GetLastError()));
    return FALSE;
  }

  if (!PathRemoveFileSpecW(updaterINIPath)) {
    LOG(("Could not remove file spec when attempting to "
         "modify service description. (%d)\n", GetLastError()));
    return FALSE;
  }

  if (!PathAppendSafe(updaterINIPath, L"updater.ini")) {
    LOG(("Could not append updater.ini filename when attempting to "
         "modify service description. (%d)\n", GetLastError()));
    return FALSE;
  }

  if (GetFileAttributesW(updaterINIPath) == INVALID_FILE_ATTRIBUTES) {
    LOG(("updater.ini file does not exist, will not modify "
         "service description. (%d)\n", GetLastError()));
    return FALSE;
  }
  
  MaintenanceServiceStringTable serviceStrings;
  int rv = ReadMaintenanceServiceStrings(updaterINIPath, &serviceStrings);
  if (rv != OK || !strlen(serviceStrings.serviceDescription)) {
    LOG(("updater.ini file does not contain a maintenance "
         "service description.\n"));
    return FALSE;
  }

  WCHAR serviceDescription[MAX_TEXT_LEN];
  if (!MultiByteToWideChar(CP_UTF8, 0, 
                           serviceStrings.serviceDescription, -1,
                           serviceDescription,
                           sizeof(serviceDescription) / 
                           sizeof(serviceDescription[0]))) {
    LOG(("Could not convert description to wide string format (%d)\n", 
         GetLastError()));
    return FALSE;
  }

  SERVICE_DESCRIPTIONW descriptionConfig;
  descriptionConfig.lpDescription = serviceDescription;
  if (!ChangeServiceConfig2W(serviceHandle, 
                             SERVICE_CONFIG_DESCRIPTION, 
                             &descriptionConfig)) {
    LOG(("Could not change service config (%d)\n", GetLastError()));
    return FALSE;
  }

  LOG(("The service description was updated successfully.\n"));
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
BOOL
SvcInstall(SvcInstallAction action)
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  WCHAR newServiceBinaryPath[MAX_PATH + 1];
  if (!GetModuleFileNameW(NULL, newServiceBinaryPath, 
                          sizeof(newServiceBinaryPath) / 
                          sizeof(newServiceBinaryPath[0]))) {
    LOG(("Could not obtain module filename when attempting to "
         "install service. (%d)\n",
         GetLastError()));
    return FALSE;
  }

  // Check if we already have the service installed.
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, 
                                              SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  DWORD lastError = GetLastError();
  if (!schService && ERROR_SERVICE_DOES_NOT_EXIST != lastError) {
    // The service exists but we couldn't open it
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  }
  
  if (schService) {
    // The service exists but it may not have the correct permissions.
    // This could happen if the permissions were not set correctly originally
    // or have been changed after the installation.  This will reset the 
    // permissions back to allow limited user accounts.
    if (!SetUserAccessServiceDACL(schService)) {
      LOG(("Could not reset security ACE on service handle. It might not be "
           "possible to start the service. This error should never "
           "happen.  (%d)\n", GetLastError()));
    }

    // The service exists and we opened it
    DWORD bytesNeeded;
    if (!QueryServiceConfigW(schService, NULL, 0, &bytesNeeded) && 
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG(("Could not determine buffer size for query service config.  (%d)\n", 
           GetLastError()));
      return FALSE;
    }

    // Get the service config information, in particular we want the binary 
    // path of the service.
    nsAutoArrayPtr<char> serviceConfigBuffer = new char[bytesNeeded];
    if (!QueryServiceConfigW(schService, 
        reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get()), 
        bytesNeeded, &bytesNeeded)) {
      LOG(("Could open service but could not query service config.  (%d)\n", 
           GetLastError()));
      return FALSE;
    }
    QUERY_SERVICE_CONFIGW &serviceConfig = 
      *reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get());

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
      GetVersionNumberFromPath(serviceConfig.lpBinaryPathName, 
                               existingA, existingB, 
                               existingC, existingD);
    if (!GetVersionNumberFromPath(newServiceBinaryPath, newA, 
                                 newB, newC, newD)) {
      LOG(("Could not obtain version number from new path\n"));
      return FALSE;
    }

    // Check if we need to replace the old binary with the new one
    // If we couldn't get the old version info then we assume we should 
    // replace it.
    if (ForceInstallSvc == action ||
        !obtainedExistingVersionInfo || 
        (existingA < newA) ||
        (existingA == newA && existingB < newB) ||
        (existingA == newA && existingB == newB && 
         existingC < newC) ||
        (existingA == newA && existingB == newB && 
         existingC == newC && existingD < newD)) {

      // We have a newer updater, so update the description from the INI file.
      UpdateServiceDescription(schService);

      schService.reset();
      if (!StopService()) {
        return FALSE;
      }

      if (!wcscmp(newServiceBinaryPath, serviceConfig.lpBinaryPathName)) {
        LOG(("File is already in the correct location, no action needed for "
             "upgrade.\n"));
        return TRUE;
      }

      BOOL result = TRUE;

      // Attempt to copy the new binary over top the existing binary.
      // If there is an error we try to move it out of the way and then
      // copy it in.  First try the safest / easiest way to overwrite the file.
      if (!CopyFileW(newServiceBinaryPath, 
                     serviceConfig.lpBinaryPathName, FALSE)) {
        LOG(("Could not overwrite old service binary file. "
             "This should never happen, but if it does the next upgrade will "
             "fix it, the service is not a critical component that needs to be "
             "installed for upgrades to work. (%d)\n", GetLastError()));

        // We rename the last 3 filename chars in an unsafe way.  Manually
        // verify there are more than 3 chars for safe failure in MoveFileExW.
        const size_t len = wcslen(serviceConfig.lpBinaryPathName);
        if (len > 3) {
          // Calculate the temp file path that we're moving the file to. This 
          // is the same as the proper service path but with a .old extension.
          LPWSTR oldServiceBinaryTempPath = 
            new WCHAR[wcslen(serviceConfig.lpBinaryPathName) + 1];
          wcscpy(oldServiceBinaryTempPath, serviceConfig.lpBinaryPathName);
          // Rename the last 3 chars to 'old'
          wcscpy(oldServiceBinaryTempPath + len - 3, L"old");

          // Move the current (old) service file to the temp path.
          if (MoveFileExW(serviceConfig.lpBinaryPathName, 
                          oldServiceBinaryTempPath, 
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            // The old binary is moved out of the way, copy in the new one.
            if (!CopyFileW(newServiceBinaryPath, 
                           serviceConfig.lpBinaryPathName, FALSE)) {
              // It is best to leave the old service binary in this condition.
              LOG(("ERROR: The new service binary could not be copied in."
                   " The service will not be upgraded.\n"));
              result = FALSE;
            } else {
              LOG(("The new service binary was copied in by first moving the"
                   " old one out of the way.\n"));
            }

            // Attempt to get rid of the old service temp path.
            if (DeleteFileW(oldServiceBinaryTempPath)) {
              LOG(("The old temp service path was deleted: %ls.\n", 
                   oldServiceBinaryTempPath));
            } else {
              // The old temp path could not be removed.  It will be removed
              // the next time the user can't copy the binary in or on uninstall.
              LOG(("WARNING: The old temp service path was not deleted.\n"));
            }
          } else {
            // It is best to leave the old service binary in this condition.
            LOG(("ERROR: Could not move old service file out of the way from:"
                 " \"%ls\" to \"%ls\". Service will not be upgraded. (%d)\n", 
                 serviceConfig.lpBinaryPathName, 
                 oldServiceBinaryTempPath, GetLastError()));
            result = FALSE;
          }
          delete[] oldServiceBinaryTempPath;
        } else {
            // It is best to leave the old service binary in this condition.
            LOG(("ERROR: Service binary path was less than 3, service will"
                 " not be updated.  This should never happen.\n"));
            result = FALSE;
        }
      } else {
        LOG(("The new service binary was copied in.\n"));
      }

      // We made a copy of ourselves to the existing location.
      // The tmp file (the process of which we are executing right now) will be
      // left over.  Attempt to delete the file on the next reboot.
      if (MoveFileExW(newServiceBinaryPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) {
        LOG(("Deleting the old file path on the next reboot: %ls.\n", 
             newServiceBinaryPath));
      } else {
        LOG(("Call to delete the old file path failed: %ls.\n", 
             newServiceBinaryPath));
      }
      
      return result;
    }

    // We don't need to copy ourselves to the existing location.
    // The tmp file (the process of which we are executing right now) will be
    // left over.  Attempt to delete the file on the next reboot.
    MoveFileExW(newServiceBinaryPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    
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
  schService.own(CreateServiceW(schSCManager, SVC_NAME, SVC_DISPLAY_NAME,
                                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                newServiceBinaryPath, NULL, NULL, NULL, 
                                NULL, NULL));
  if (!schService) {
    LOG(("Could not create Windows service. "
         "This error should never happen since a service install "
         "should only be called when elevated. (%d)\n", GetLastError()));
    return FALSE;
  } 

  if (!SetUserAccessServiceDACL(schService)) {
    LOG(("Could not set security ACE on service handle, the service will not "
         "be able to be started from unelevated processes. "
         "This error should never happen.  (%d)\n", 
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
BOOL
StopService()
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  } 

  LOG(("Sending stop request...\n"));
  SERVICE_STATUS status;
  SetLastError(ERROR_SUCCESS);
  if (!ControlService(schService, SERVICE_CONTROL_STOP, &status) &&
      GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
    LOG(("Error sending stop request: %d\n", GetLastError()));
  }

  schSCManager.reset();
  schService.reset();

  LOG(("Waiting for service stop...\n"));
  DWORD lastState = WaitForServiceStop(SVC_NAME, 30);

  // The service can be in a stopped state but the exe still in use
  // so make sure the process is really gone before proceeding
  WaitForProcessExit(L"maintenanceservice.exe", 30);
  LOG(("Done waiting for service stop, last service state: %d\n", lastState));

  return lastState == SERVICE_STOPPED;
}

/**
 * Uninstalls the Maintenance service.
 *
 * @return TRUE if successful.
 */
BOOL
SvcUninstall()
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  } 

  //Stop the service so it deletes faster and so the uninstaller
  // can actually delete its EXE.
  DWORD totalWaitTime = 0;
  SERVICE_STATUS status;
  static const int maxWaitTime = 1000 * 60; // Never wait more than a minute
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
BOOL
SetUserAccessServiceDACL(SC_HANDLE hService)
{
  PACL pNewAcl = NULL;
  PSECURITY_DESCRIPTOR psd = NULL;
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
SetUserAccessServiceDACL(SC_HANDLE hService, PACL &pNewAcl, 
                         PSECURITY_DESCRIPTOR psd)
{
  // Get the current security descriptor needed size
  DWORD needed = 0;
  if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, 
                                  &psd, 0, &needed)) {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG(("Warning: Could not query service object security size.  (%d)\n", 
           GetLastError()));
      return GetLastError();
    }

    DWORD size = needed;
    psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, size);
    if (!psd) {
      LOG(("Warning: Could not allocate security descriptor.  (%d)\n", 
           GetLastError()));
      return ERROR_INSUFFICIENT_BUFFER;
    }

    // Get the actual security descriptor now
    if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, 
                                    psd, size, &needed)) {
      LOG(("Warning: Could not allocate security descriptor.  (%d)\n", 
           GetLastError()));
      return GetLastError();
    }
  }

  // Get the current DACL from the security descriptor.
  PACL pacl = NULL;
  BOOL bDaclPresent = FALSE;
  BOOL bDaclDefaulted = FALSE;
  if ( !GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, 
                                  &bDaclDefaulted)) {
    LOG(("Warning: Could not obtain DACL.  (%d)\n", GetLastError()));
    return GetLastError();
  }

  PSID sid;
  DWORD SIDSize = SECURITY_MAX_SID_SIZE;
  sid = LocalAlloc(LMEM_FIXED, SIDSize);
  if (!sid) {
    LOG(("Could not allocate SID memory.  (%d)\n", GetLastError()));
    return GetLastError();
  }

  if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, sid, &SIDSize)) {
    DWORD lastError = GetLastError();
    LOG(("Could not create well known SID.  (%d)\n", lastError));
    LocalFree(sid);
    return lastError;
  }

  // Lookup the account name, the function fails if you don't pass in
  // a buffer for the domain name but it's not used since we're using
  // the built in account Sid.
  SID_NAME_USE accountType;
  WCHAR accountName[UNLEN + 1];
  WCHAR domainName[DNLEN + 1];
  DWORD accountNameSize = UNLEN + 1;
  DWORD domainNameSize = DNLEN + 1;
  if (!LookupAccountSidW(NULL, sid, accountName, 
                         &accountNameSize, 
                         domainName, &domainNameSize, &accountType)) {
    LOG(("Warning: Could not lookup account Sid, will try Users.  (%d)\n",
         GetLastError()));
    wcscpy(accountName, L"Users");
  }

  // We already have the group name so we can get rid of the SID
  FreeSid(sid);
  sid = NULL;

  // Build the ACE, BuildExplicitAccessWithName cannot fail so it is not logged.
  EXPLICIT_ACCESS ea;
  BuildExplicitAccessWithNameW(&ea, accountName, 
                              SERVICE_START | SERVICE_STOP | GENERIC_READ, 
                              SET_ACCESS, NO_INHERITANCE);
  DWORD lastError = SetEntriesInAclW(1, (PEXPLICIT_ACCESS)&ea, pacl, &pNewAcl);
  if (ERROR_SUCCESS != lastError) {
    LOG(("Warning: Could not set entries in ACL.  (%d)\n", lastError));
    return lastError;
  }

  // Initialize a new security descriptor.
  SECURITY_DESCRIPTOR sd;
  if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    LOG(("Warning: Could not initialize security descriptor.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Set the new DACL in the security descriptor.
  if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE)) {
    LOG(("Warning: Could not set security descriptor DACL.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Set the new security descriptor for the service object.
  if (!SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, &sd)) {
    LOG(("Warning: Could not set object security.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Woohoo, raise the roof
  LOG(("User access was set successfully on the service.\n"));
  return ERROR_SUCCESS;
}
