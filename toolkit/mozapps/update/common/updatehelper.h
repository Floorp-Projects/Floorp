/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

BOOL LaunchWinPostProcess(const WCHAR *installationDir,
                          const WCHAR *updateInfoDir,
                          bool forceSync,
                          HANDLE userToken);
BOOL StartServiceUpdate(LPCWSTR installDir);
BOOL GetUpdateDirectoryPath(LPWSTR path);
DWORD LaunchServiceSoftwareUpdateCommand(int argc, LPCWSTR *argv);
BOOL WriteStatusFailure(LPCWSTR updateDirPath, int errorCode);
BOOL WriteStatusPending(LPCWSTR updateDirPath);
DWORD WaitForServiceStop(LPCWSTR serviceName, DWORD maxWaitSeconds);
DWORD WaitForProcessExit(LPCWSTR filename, DWORD maxSeconds);
BOOL DoesFallbackKeyExist();
BOOL IsLocalFile(LPCWSTR file, BOOL &isLocal);
DWORD StartServiceCommand(int argc, LPCWSTR* argv);
BOOL IsUnpromptedElevation(BOOL &isUnpromptedElevation);

#define SVC_NAME L"MozillaMaintenance"

#define BASE_SERVICE_REG_KEY \
  L"SOFTWARE\\Mozilla\\MaintenanceService"

// The test only fallback key, as its name implies, is only present on machines
// that will use automated tests.  Since automated tests always run from a
// different directory for each test, the presence of this key bypasses the
// "This is a valid installation directory" check.  This key also stores
// the allowed name and issuer for cert checks so that the cert check
// code can still be run unchanged.
#define TEST_ONLY_FALLBACK_KEY_PATH \
  BASE_SERVICE_REG_KEY L"\\3932ecacee736d366d6436db0f55bce4"

