/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfileUnlockerWin.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsXPCOM.h"

namespace mozilla {

/**
 * RAII class to obtain and manage a handle to a Restart Manager session.
 * It opens a new handle upon construction and releases it upon destruction.
 */
class MOZ_STACK_CLASS ScopedRestartManagerSession
{
public:
  explicit ScopedRestartManagerSession(ProfileUnlockerWin& aUnlocker)
    : mError(ERROR_INVALID_HANDLE)
    , mHandle((DWORD)-1) // 0 is a valid restart manager handle
    , mUnlocker(aUnlocker)
  {
    mError = mUnlocker.StartSession(mHandle);
  }

  ~ScopedRestartManagerSession()
  {
    if (mError == ERROR_SUCCESS) {
      mUnlocker.EndSession(mHandle);
    }
  }

  /**
   * @return true if the handle is a valid Restart Ranager handle.
   */
  inline bool
  ok()
  {
    return mError == ERROR_SUCCESS;
  }

  /**
   * @return the Restart Manager handle to pass to other Restart Manager APIs.
   */
  inline DWORD
  handle()
  {
    return mHandle;
  }

private:
  DWORD               mError;
  DWORD               mHandle;
  ProfileUnlockerWin& mUnlocker;
};

ProfileUnlockerWin::ProfileUnlockerWin(const nsAString& aFileName)
  : mRmStartSession(nullptr)
  , mRmRegisterResources(nullptr)
  , mRmGetList(nullptr)
  , mRmEndSession(nullptr)
  , mQueryFullProcessImageName(nullptr)
  , mFileName(aFileName)
{
}

ProfileUnlockerWin::~ProfileUnlockerWin()
{
}

NS_IMPL_ISUPPORTS(ProfileUnlockerWin, nsIProfileUnlocker)

nsresult
ProfileUnlockerWin::Init()
{
  MOZ_ASSERT(!mRestartMgrModule);
  if (mFileName.IsEmpty()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsModuleHandle module(::LoadLibraryW(MOZ_UTF16("Rstrtmgr.dll")));
  if (!module) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mRmStartSession =
    reinterpret_cast<RMSTARTSESSION>(::GetProcAddress(module, "RmStartSession"));
  if (!mRmStartSession) {
    return NS_ERROR_UNEXPECTED;
  }
  mRmRegisterResources =
    reinterpret_cast<RMREGISTERRESOURCES>(::GetProcAddress(module,
                                                       "RmRegisterResources"));
  if (!mRmRegisterResources) {
    return NS_ERROR_UNEXPECTED;
  }
  mRmGetList = reinterpret_cast<RMGETLIST>(::GetProcAddress(module,
                                                            "RmGetList"));
  if (!mRmGetList) {
    return NS_ERROR_UNEXPECTED;
  }
  mRmEndSession = reinterpret_cast<RMENDSESSION>(::GetProcAddress(module,
                                                                  "RmEndSession"));
  if (!mRmEndSession) {
    return NS_ERROR_UNEXPECTED;
  }

  mQueryFullProcessImageName =
    reinterpret_cast<QUERYFULLPROCESSIMAGENAME>(::GetProcAddress(
                                  ::GetModuleHandleW(MOZ_UTF16("kernel32.dll")),
                                  "QueryFullProcessImageNameW"));
  if (!mQueryFullProcessImageName) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mRestartMgrModule.steal(module);
  return NS_OK;
}

DWORD
ProfileUnlockerWin::StartSession(DWORD& aHandle)
{
  WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = {0};
  return mRmStartSession(&aHandle, 0, sessionKey);
}

void
ProfileUnlockerWin::EndSession(DWORD aHandle)
{
  mRmEndSession(aHandle);
}

NS_IMETHODIMP
ProfileUnlockerWin::Unlock(uint32_t aSeverity)
{
  if (!mRestartMgrModule) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aSeverity != FORCE_QUIT) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  ScopedRestartManagerSession session(*this);
  if (!session.ok()) {
    return NS_ERROR_FAILURE;
  }

  LPCWSTR resources[] = { mFileName.get() };
  DWORD error = mRmRegisterResources(session.handle(), 1, resources, 0, nullptr,
                                     0, nullptr);
  if (error != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  // Using a nsAutoTArray here because we expect the required size to be 1.
  nsAutoTArray<RM_PROCESS_INFO, 1> info;
  UINT numEntries;
  UINT numEntriesNeeded = 1;
  error = ERROR_MORE_DATA;
  DWORD reason = RmRebootReasonNone;
  while (error == ERROR_MORE_DATA) {
    info.SetLength(numEntriesNeeded);
    numEntries = numEntriesNeeded;
    error = mRmGetList(session.handle(), &numEntriesNeeded, &numEntries,
                       &info[0], &reason);
  }
  if (error != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  for (UINT i = 0; i < numEntries; ++i) {
    rv = TryToTerminate(info[i].Process);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }

  // If nothing could be unlocked then we return the error code of the last
  // failure that was returned.
  return rv;
}

nsresult
ProfileUnlockerWin::TryToTerminate(RM_UNIQUE_PROCESS& aProcess)
{
  DWORD accessRights = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE;
  nsAutoHandle otherProcess(::OpenProcess(accessRights, FALSE,
                                          aProcess.dwProcessId));
  if (!otherProcess) {
    return NS_ERROR_FAILURE;
  }

  FILETIME creationTime, exitTime, kernelTime, userTime;
  if (!::GetProcessTimes(otherProcess, &creationTime, &exitTime, &kernelTime,
                         &userTime)) {
    return NS_ERROR_FAILURE;
  }
  if (::CompareFileTime(&aProcess.ProcessStartTime, &creationTime)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  WCHAR imageName[MAX_PATH];
  DWORD imageNameLen = MAX_PATH;
  if (!mQueryFullProcessImageName(otherProcess, 0, imageName, &imageNameLen)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIFile> otherProcessImageName;
  if (NS_FAILED(NS_NewLocalFile(nsDependentString(imageName, imageNameLen),
                                false, getter_AddRefs(otherProcessImageName)))) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString otherProcessLeafName;
  if (NS_FAILED(otherProcessImageName->GetLeafName(otherProcessLeafName))) {
    return NS_ERROR_FAILURE;
  }

  imageNameLen = MAX_PATH;
  if (!mQueryFullProcessImageName(::GetCurrentProcess(), 0, imageName,
        &imageNameLen)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIFile> thisProcessImageName;
  if (NS_FAILED(NS_NewLocalFile(nsDependentString(imageName, imageNameLen),
                                false, getter_AddRefs(thisProcessImageName)))) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString thisProcessLeafName;
  if (NS_FAILED(thisProcessImageName->GetLeafName(thisProcessLeafName))) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the image leaf names match
  if (_wcsicmp(otherProcessLeafName.get(), thisProcessLeafName.get())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We know that another process holds the lock and that it shares the same
  // image name as our process. Let's kill it.
  if (!::TerminateProcess(otherProcess, 1)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace mozilla

