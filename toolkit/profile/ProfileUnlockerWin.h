/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileUnlockerWin_h
#define ProfileUnlockerWin_h

#include <windows.h>
#include <restartmanager.h>

#include "nsIProfileUnlocker.h"
#include "nsString.h"
#include "nsWindowsHelpers.h"

namespace mozilla {

class ProfileUnlockerWin final : public nsIProfileUnlocker {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILEUNLOCKER

  explicit ProfileUnlockerWin(const nsAString& aFileName);

  nsresult Init();

  DWORD StartSession(DWORD& aHandle);
  void EndSession(DWORD aHandle);

 private:
  ~ProfileUnlockerWin();
  nsresult TryToTerminate(RM_UNIQUE_PROCESS& aProcess);

 private:
  typedef DWORD(WINAPI* RMSTARTSESSION)(DWORD*, DWORD, WCHAR[]);
  typedef DWORD(WINAPI* RMREGISTERRESOURCES)(DWORD, UINT, LPCWSTR[], UINT,
                                             RM_UNIQUE_PROCESS[], UINT,
                                             LPCWSTR[]);
  typedef DWORD(WINAPI* RMGETLIST)(DWORD, UINT*, UINT*, RM_PROCESS_INFO[],
                                   LPDWORD);
  typedef DWORD(WINAPI* RMENDSESSION)(DWORD);

 private:
  nsModuleHandle mRestartMgrModule;
  RMSTARTSESSION mRmStartSession;
  RMREGISTERRESOURCES mRmRegisterResources;
  RMGETLIST mRmGetList;
  RMENDSESSION mRmEndSession;

  nsString mFileName;
};

}  // namespace mozilla

#endif  // ProfileUnlockerWin_h
