/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsAnonymousTemporaryFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAppDirectoryServiceDefs.h"

#ifdef XP_WIN
#include "nsIObserver.h"
#include "nsIIdleService.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"
#endif


// We store the temp files in the system temp dir on non-Windows systems.
// On Windows systems we use a subdirectory of the profile dir, because:
//   1. DELETE_ON_CLOSE is unreliable on Windows, in particular if we power
//      cycle (and perhaps if we crash) the files are not deleted. We store
//      the temporary files in a known subdir so that we can find and delete
//      them easily and quickly.
//   2. We can be sure the user always has write privileges to their own
//      profile directory; if the subdir was in the system temp directory and
//      was created by a privileged user it's possible that other users
//      wouldn't have write access to this folder.
#ifdef XP_WIN
#define ANON_TEMP_DIR NS_APP_USER_PROFILE_LOCAL_50_DIR
#else
#define ANON_TEMP_DIR NS_OS_TEMP_DIR
#endif

nsresult
NS_OpenAnonymousTemporaryFile(PRFileDesc** aOutFileDesc)
{
  NS_ENSURE_ARG(aOutFileDesc);
  nsresult rv;

  nsCOMPtr<nsIFile> tmpFile;
  rv = NS_GetSpecialDirectory(ANON_TEMP_DIR, getter_AddRefs(tmpFile));
  NS_ENSURE_SUCCESS(rv,rv);

#ifdef XP_WIN
  // On windows DELETE_ON_CLOSE is unreliable, so we store temporary files
  // in a subdir of the temp dir and delete that in an idle service observer
  // to ensure it's been cleared.
  rv = tmpFile->AppendNative(nsDependentCString("mozilla-temp-files"));
  NS_ENSURE_SUCCESS(rv,rv);    
  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  NS_ENSURE_TRUE(rv == NS_ERROR_FILE_ALREADY_EXISTS || NS_SUCCEEDED(rv), rv);
#endif

  // Give the temp file a name with a random element. CreateUnique will also
  // append a counter to the name if it encounters a name collision. Adding
  // a random element to the name reduces the likelihood of a name collision,
  // so that CreateUnique() doesn't end up trying a lot of name variants in
  // its "try appending an incrementing counter" loop, as file IO can be
  // expensive on some mobile flash drives.
  nsAutoString name(NS_LITERAL_STRING("mozilla-temp-"));
  name.AppendInt(rand());

  rv = tmpFile->Append(name);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0700);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = tmpFile->OpenNSPRFileDesc(PR_RDWR | nsIFile::DELETE_ON_CLOSE,
                                 PR_IRWXU, aOutFileDesc);

  return rv;    
}

#ifdef XP_WIN

// On Windows we have an idle service observer that runs some time after
// startup and deletes any stray anonymous temporary files...

// Duration of idle time before we'll get a callback whereupon we attempt to
// remove any stray and unused anonymous temp files.
#define TEMP_FILE_IDLE_TIME 30

// The nsAnonTempFileRemover is created in a timer, which sets an idle observer.
// This is expiration time (in ms) which initial timer is set for (3 minutes).
#define SCHEDULE_TIMEOUT 3 * 60 * 1000

// This class adds itself as an idle observer. When the application has
// been idle for about 30 seconds we'll get a notification, whereupon we'll
// attempt to delete ${TempDir}/mozilla-temp-files/. This is to ensure all
// temp files that were supposed to be deleted on application exit were actually
// deleted as they may not be if we previously crashed. See bugs 572579 and
// 785662. This is only needed on some versions of Windows,
// nsIFile::DELETE_ON_CLOSE works on other platforms.
class nsAnonTempFileRemover : public nsIObserver {
public:
  NS_DECL_ISUPPORTS

  nsAnonTempFileRemover() {
    MOZ_COUNT_CTOR(nsAnonTempFileRemover);
  }

  ~nsAnonTempFileRemover() {
    MOZ_COUNT_DTOR(nsAnonTempFileRemover);
  }

  NS_IMETHODIMP Observe(nsISupports *subject,
                        const char *topic,
                        const PRUnichar *data)
  {
    if (strcmp(topic, "idle") == 0) {
      // The user has been idle for a while, clean up the temp files.
      // The idle service will drop its reference to this object after
      // we exit, destroying this object.
      RemoveAnonTempFileFiles();
    }
    return NS_OK;
  }

  nsresult RegisterIdleObserver() {
    // Add this as an idle observer. When we've been idle for 
    // TEMP_FILE_IDLE_TIME seconds, we'll get a notification, and we'll then
    // try to delete any stray temp files.
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (!idleSvc)
      return NS_ERROR_FAILURE;
    return idleSvc->AddIdleObserver(this, TEMP_FILE_IDLE_TIME);
  }

  void RemoveAnonTempFileFiles() {
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (idleSvc)
      idleSvc->RemoveIdleObserver(this, TEMP_FILE_IDLE_TIME);

    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = NS_GetSpecialDirectory(ANON_TEMP_DIR,
                                         getter_AddRefs(tmpDir));
    if (NS_FAILED(rv))
      return;

    rv = tmpDir->AppendNative(nsDependentCString("mozilla-temp-files"));
    if (NS_FAILED(rv))
      return;

    // Remove the directory recursively.  
    tmpDir->Remove(true);
  }
};

NS_IMPL_ISUPPORTS1(nsAnonTempFileRemover, nsIObserver)

// Stores a reference to the timer we use to trigger CreateAnonTempFileRemover.
// We need to keep a reference to it to stop the timer being released before
// it fires.
static nsITimer* sAnonTempFileTimer = nullptr;

void CreateAnonTempFileRemover(nsITimer* aTimer, void* aClosure) {
  // Create a new nsAnonTempFileRemover, and register it as an idle observer.
  // If it is successfully registered as an idle observer, its owning reference
  // will be held by the idle service, otherwise it will be destroyed by the
  // refptr here when it goes out of scope.
  nsRefPtr<nsAnonTempFileRemover> t = new nsAnonTempFileRemover();
  t->RegisterIdleObserver();
  NS_IF_RELEASE(sAnonTempFileTimer);
  sAnonTempFileTimer = nullptr;
}

nsresult ScheduleAnonTempFileRemover() {
  NS_ASSERTION(sAnonTempFileTimer == nullptr, "Don't init timer twice!");
  // We create the nsAnonTempFileRemover in a timer, so that the app has enough
  // time to start up before we add the idle observer. If we register the
  // idle observer too early, it will be registered before the fake idle
  // service is installed when running in xpcshell, and this interferes with
  // the fake idle service, causing xpcshell-test failures.
  nsCOMPtr<nsITimer> t = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (NS_SUCCEEDED(t->InitWithFuncCallback(CreateAnonTempFileRemover,
                                           nullptr,
                                           SCHEDULE_TIMEOUT,
                                           nsITimer::TYPE_ONE_SHOT))) {
    t.forget(&sAnonTempFileTimer);
  }
  return NS_OK;
}

#endif

