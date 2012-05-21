/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

BOOL LaunchWinPostProcess(const WCHAR *installationDir,
                          const WCHAR *updateInfoDir, 
                          bool forceSync,
                          HANDLE userToken);
BOOL StartServiceUpdate(int argc, LPWSTR *argv);
BOOL GetUpdateDirectoryPath(LPWSTR path);
DWORD LaunchServiceSoftwareUpdateCommand(int argc, LPCWSTR *argv);
BOOL WriteStatusFailure(LPCWSTR updateDirPath, int errorCode);
BOOL WriteStatusPending(LPCWSTR updateDirPath);
DWORD WaitForServiceStop(LPCWSTR serviceName, DWORD maxWaitSeconds);
DWORD WaitForProcessExit(LPCWSTR filename, DWORD maxSeconds);
BOOL DoesFallbackKeyExist();
BOOL IsLocalFile(LPCWSTR file, BOOL &isLocal);

#define SVC_NAME L"MozillaMaintenance"
