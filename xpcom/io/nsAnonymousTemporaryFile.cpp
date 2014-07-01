/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/dom/ContentChild.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prio.h"
#include "private/pprio.h"

#ifdef XP_WIN
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsIIdleService.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"
#include "nsCRT.h"

#endif

using namespace mozilla;

// We store the temp files in the system temp dir.
//
// On Windows systems in particular we use a sub-directory of the temp
// directory, because:
//   1. DELETE_ON_CLOSE is unreliable on Windows, in particular if we power
//      cycle (and perhaps if we crash) the files are not deleted. We store
//      the temporary files in a known sub-dir so that we can find and delete
//      them easily and quickly.
//   2. On Windows NT the system temp dir is in the user's $HomeDir/AppData,
//      so we can be sure the user always has write privileges to that directory;
//      if the sub-dir for our temp files was in some shared location and
//      was created by a privileged user, it's possible that other users
//      wouldn't have write access to that sub-dir. (Non-Windows systems
//      don't store their temp files in a sub-dir, so this isn't an issue on
//      those platforms).
//   3. Content processes can access the system temp dir
//      (NS_GetSpecialDirectory fails on NS_APP_USER_PROFILE_LOCAL_50_DIR
//      for content process for example, which is where we previously stored
//      temp files on Windows). This argument applies to all platforms, not
//      just Windows.
static nsresult
GetTempDir(nsIFile** aTempDir)
{
  if (NS_WARN_IF(!aTempDir)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef XP_WIN
  // On windows DELETE_ON_CLOSE is unreliable, so we store temporary files
  // in a subdir of the temp dir and delete that in an idle service observer
  // to ensure it's been cleared.
  rv = tmpFile->AppendNative(nsDependentCString("mozilla-temp-files"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

  tmpFile.forget(aTempDir);

  return NS_OK;
}

nsresult
NS_OpenAnonymousTemporaryFile(PRFileDesc** aOutFileDesc)
{
  if (NS_WARN_IF(!aOutFileDesc)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (dom::ContentChild* child = dom::ContentChild::GetSingleton()) {
    ipc::FileDescriptor fd;
    DebugOnly<bool> succeeded = child->SendOpenAnonymousTemporaryFile(&fd);
    // The child process should already have been terminated if the
    // IPC had failed.
    MOZ_ASSERT(succeeded);
    *aOutFileDesc = PR_ImportFile(PROsfd(fd.PlatformHandle()));
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> tmpFile;
  rv = GetTempDir(getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Give the temp file a name with a random element. CreateUnique will also
  // append a counter to the name if it encounters a name collision. Adding
  // a random element to the name reduces the likelihood of a name collision,
  // so that CreateUnique() doesn't end up trying a lot of name variants in
  // its "try appending an incrementing counter" loop, as file IO can be
  // expensive on some mobile flash drives.
  nsAutoCString name("mozilla-temp-");
  name.AppendInt(rand());

  rv = tmpFile->AppendNative(name);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0700);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->OpenNSPRFileDesc(PR_RDWR | nsIFile::DELETE_ON_CLOSE,
                                 PR_IRWXU, aOutFileDesc);

  return rv;
}

#ifdef XP_WIN

// On Windows we have an idle service observer that runs some time after
// startup and deletes any stray anonymous temporary files...

// Duration of idle time before we'll get a callback whereupon we attempt to
// remove any stray and unused anonymous temp files.
#define TEMP_FILE_IDLE_TIME_S 30

// The nsAnonTempFileRemover is created in a timer, which sets an idle observer.
// This is expiration time (in ms) which initial timer is set for (3 minutes).
#define SCHEDULE_TIMEOUT_MS 3 * 60 * 1000

#define XPCOM_SHUTDOWN_TOPIC "xpcom-shutdown"

// This class adds itself as an idle observer. When the application has
// been idle for about 30 seconds we'll get a notification, whereupon we'll
// attempt to delete ${TempDir}/mozilla-temp-files/. This is to ensure all
// temp files that were supposed to be deleted on application exit were actually
// deleted, as they may not be if we previously crashed. See bugs 572579 and
// 785662. This is only needed on some versions of Windows,
// nsIFile::DELETE_ON_CLOSE works on other platforms.
// This class adds itself as a shutdown observer so that it can cancel the
// idle observer and its timer on shutdown. Note: the observer and idle
// services hold references to instances of this object, and those references
// are what keep this object alive.
class nsAnonTempFileRemover MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  nsAnonTempFileRemover()
  {
    MOZ_COUNT_CTOR(nsAnonTempFileRemover);
  }

  ~nsAnonTempFileRemover()
  {
    MOZ_COUNT_DTOR(nsAnonTempFileRemover);
  }

  nsresult Init()
  {
    // We add the idle observer in a timer, so that the app has enough
    // time to start up before we add the idle observer. If we register the
    // idle observer too early, it will be registered before the fake idle
    // service is installed when running in xpcshell, and this interferes with
    // the fake idle service, causing xpcshell-test failures.
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (NS_WARN_IF(!mTimer)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = mTimer->Init(this,
                               SCHEDULE_TIMEOUT_MS,
                               nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Register shutdown observer so we can cancel the timer if we shutdown before
    // the timer runs.
    nsCOMPtr<nsIObserverService> obsSrv = services::GetObserverService();
    if (NS_WARN_IF(!obsSrv)) {
      return NS_ERROR_FAILURE;
    }
    return obsSrv->AddObserver(this, XPCOM_SHUTDOWN_TOPIC, false);
  }

  void Cleanup()
  {
    // Cancel timer.
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    // Remove idle service observer.
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (idleSvc) {
      idleSvc->RemoveIdleObserver(this, TEMP_FILE_IDLE_TIME_S);
    }
    // Remove shutdown observer.
    nsCOMPtr<nsIObserverService> obsSrv = services::GetObserverService();
    if (obsSrv) {
      obsSrv->RemoveObserver(this, XPCOM_SHUTDOWN_TOPIC);
    }
  }

  NS_IMETHODIMP Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData)
  {
    if (nsCRT::strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0 &&
        NS_FAILED(RegisterIdleObserver())) {
      Cleanup();
    } else if (nsCRT::strcmp(aTopic, OBSERVER_TOPIC_IDLE) == 0) {
      // The user has been idle for a while, clean up the temp files.
      // The idle service will drop its reference to this object after
      // we exit, destroying this object.
      RemoveAnonTempFileFiles();
      Cleanup();
    } else if (nsCRT::strcmp(aTopic, XPCOM_SHUTDOWN_TOPIC) == 0) {
      Cleanup();
    }
    return NS_OK;
  }

  nsresult RegisterIdleObserver()
  {
    // Add this as an idle observer. When we've been idle for
    // TEMP_FILE_IDLE_TIME_S seconds, we'll get a notification, and we'll then
    // try to delete any stray temp files.
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (!idleSvc) {
      return NS_ERROR_FAILURE;
    }
    return idleSvc->AddIdleObserver(this, TEMP_FILE_IDLE_TIME_S);
  }

  void RemoveAnonTempFileFiles()
  {
    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = GetTempDir(getter_AddRefs(tmpDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // Remove the directory recursively.
    tmpDir->Remove(true);
  }

private:
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(nsAnonTempFileRemover, nsIObserver)

nsresult
CreateAnonTempFileRemover()
{
  // Create a temp file remover. If Init() succeeds, the temp file remover is kept
  // alive by a reference held by the observer service, since the temp file remover
  // is a shutdown observer. We only create the temp file remover if we're running
  // in the main process; there's no point in doing the temp file removal multiple
  // times per startup.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return NS_OK;
  }
  nsRefPtr<nsAnonTempFileRemover> tempRemover = new nsAnonTempFileRemover();
  return tempRemover->Init();
}

#endif

