/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
