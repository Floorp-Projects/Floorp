/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void WINAPI SvcMain(DWORD dwArgc, LPWSTR* lpszArgv);
void SvcInit(DWORD dwArgc, LPWSTR* lpszArgv);
void WINAPI SvcCtrlHandler(DWORD dwCtrl);
void ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                     DWORD dwWaitHint);
