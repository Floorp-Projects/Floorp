/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#error nsMediaCacheRemover only needed on Windows.
#endif

#include "nsIObserver.h"
#include "nsIIdleService.h"
#include "nsISimpleEnumerator.h"
#include "nsILocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"

// Duration of idle time before we'll get a callback whereupon we attempt to
// remove any stray and unused media cache temp files.
#define TEMP_FILE_IDLE_TIME 30

// The nsMediaCacheRemover is created in a timer, which sets an idle observer.
// This is expiration time (in ms) which initial timer is set for (3 minutes).
#define SCHEDULE_TIMEOUT 3 * 60 * 1000

// This class adds itself as an idle observer. When the application has
// been idle for about 30 seconds we'll get a notification, whereupon we'll
// attempt to delete ${TempDir}/mozilla-media-cache/. This is to ensure all
// media cache temp files which were supposed to be deleted on application
// exit were actually deleted as they may not be if we previously crashed.
// See bug 572579. This is only needed on some versions of Windows,
// nsILocalFile::DELETE_ON_CLOSE works on other platforms.
class nsMediaCacheRemover : public nsIObserver {
public:
  NS_DECL_ISUPPORTS

  nsMediaCacheRemover() {
    MOZ_COUNT_CTOR(nsMediaCacheRemover);
  }

  ~nsMediaCacheRemover() {
    MOZ_COUNT_DTOR(nsMediaCacheRemover);
  }

  NS_IMETHODIMP Observe(nsISupports *subject,
                        const char *topic,
                        const PRUnichar *data)
  {
    if (strcmp(topic, "idle") == 0) {
      // The user has been idle for a while, clean up the temp files.
      // The idle service will drop its reference to this object after
      // we exit, destroying this object.
      RemoveMediaCacheFiles();
    }
    return NS_OK;
  }

  nsresult RegisterIdleObserver() {
    // Add this as an idle observer. When we've been idle for 
    // TEMP_FILE_IDLE_TIME seconds, we'll get a notification, and we'll then
    // try to delete any stray media cache temp files.
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (!idleSvc)
      return NS_ERROR_FAILURE;
    return idleSvc->AddIdleObserver(this, TEMP_FILE_IDLE_TIME);
  }

  void RemoveMediaCacheFiles() {
    nsCOMPtr<nsIIdleService> idleSvc =
      do_GetService("@mozilla.org/widget/idleservice;1");
    if (idleSvc)
      idleSvc->RemoveIdleObserver(this, TEMP_FILE_IDLE_TIME);

    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                         getter_AddRefs(tmpDir));
    if (NS_FAILED(rv))
      return;

    NS_ABORT_IF_FALSE(XRE_GetProcessType() == GeckoProcessType_Default,
                      "Need to update media cache file location");

    rv = tmpDir->AppendNative(nsDependentCString("mozilla-media-cache"));
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsILocalFile> tmpFile = do_QueryInterface(tmpDir);
    if (!tmpFile)
      return;

    // Remove the directory recursively.  
    tmpFile->Remove(true);
  }
};

NS_IMPL_ISUPPORTS1(nsMediaCacheRemover, nsIObserver)

void CreateMediaCacheRemover(nsITimer* aTimer, void* aClosure) {
  // Create a new nsMediaCacheRemover, and register it as an idle observer.
  // If it is successfully registered as an idle observer, its owning reference
  // will be held by the idle service, otherwise it will be destroyed by the
  // refptr here when it goes out of scope.
  nsRefPtr<nsMediaCacheRemover> t = new nsMediaCacheRemover();
  t->RegisterIdleObserver();
}

nsresult ScheduleMediaCacheRemover() {
  // We create the nsMediaCacheRemover in a timer, so that the app has enough
  // time to start up before we add the idle observer. If we register the
  // idle observer too early, it will be registered before the fake idle
  // service is installed when running in xpcshell, and this interferes with
  // the fake idle service, causing xpcshell-test failures.
  nsCOMPtr<nsITimer> t = do_CreateInstance(NS_TIMER_CONTRACTID);
  nsresult res = t->InitWithFuncCallback(CreateMediaCacheRemover,
                                         0,
                                         SCHEDULE_TIMEOUT,
                                         nsITimer::TYPE_ONE_SHOT);
  return res;
}
