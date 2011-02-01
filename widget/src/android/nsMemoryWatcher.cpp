/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsMemoryWatcher.h"
#include "nsComponentManagerUtils.h"
#include "android/log.h"
#include "nsString.h"
#include "nsAppShell.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsXULAppAPI.h"

NS_IMPL_ISUPPORTS1(nsMemoryWatcher, nsITimerCallback)

// These numbers were determined by inspecting the /proc/meminfo
// MemFree values on the Nexus S before there was a system failure.
// Seting these numbers lower we'd see no time to recover (by killing
// the child process).  Setting these numbers hight resulted in not
// being able to load pages on phones with lots of services/apps
// running.
#define DEFAULT_TIMER_INTERVAL 2000
#define DEFAULT_LOW_MEMORY_MARK 5000
#define DEFAULT_HIGH_MEMORY_MARK 10000

nsMemoryWatcher::nsMemoryWatcher()
  : mLowWaterMark(DEFAULT_LOW_MEMORY_MARK)
  , mHighWaterMark(DEFAULT_HIGH_MEMORY_MARK)
  , mLastLowNotification(0)
  , mLastHighNotification(0)
  , mMemInfoFile(nsnull)
{
}

nsMemoryWatcher::~nsMemoryWatcher()
{
  if (mTimer)
    StopWatching();
}

void
nsMemoryWatcher::StartWatching()
{
  if (mTimer)
    return;

  // Prevent this from running in the child process
  if (XRE_GetProcessType() != GeckoProcessType_Default)
      return;

  // Prevent this from running anything but the devices that need it
  nsCOMPtr<nsIPropertyBag2> sysInfo = do_GetService("@mozilla.org/system-info;1");
  if (sysInfo) {
      nsCString deviceType;
      nsresult rv = sysInfo->GetPropertyAsACString(NS_LITERAL_STRING("device"),
                                                       deviceType);
      if (NS_SUCCEEDED(rv)) {
          if (! deviceType.EqualsLiteral("Nexus S"))
              return;
      }
  }

  __android_log_print(ANDROID_LOG_WARN, "Gecko",
                      "!!!!!!!!! Watching Memory....");


  mMemInfoFile = fopen("/proc/meminfo", "r");
  NS_ASSERTION(mMemInfoFile, "Could not open /proc/meminfo for reading.");

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  NS_ASSERTION(mTimer, "Creating of a timer failed.");

  mTimer->InitWithCallback(this, DEFAULT_TIMER_INTERVAL, nsITimer::TYPE_REPEATING_SLACK);
}

void
nsMemoryWatcher::StopWatching()
{
  if (!mTimer)
    return;

  mTimer->Cancel();
  mTimer = nsnull;

  fclose(mMemInfoFile);
  mMemInfoFile = nsnull;
}

NS_IMETHODIMP
nsMemoryWatcher::Notify(nsITimer *aTimer)
{
  NS_ASSERTION(mMemInfoFile, "File* to /proc/meminfo is null");

  rewind(mMemInfoFile);

  long memFree = -1;
  char line[256];

  while (fgets(line, 256, mMemInfoFile)) {
      sscanf(line, "MemFree: %ld kB", &memFree);
  }
  NS_ASSERTION(memFree > 0, "Free memory should be greater than zero");

  if (memFree < mLowWaterMark) {
      __android_log_print(ANDROID_LOG_WARN, "Gecko",
                          "!!!!!!!!! Reached criticial memory level. MemFree = %ld",
                          memFree);

      if (PR_IntervalToSeconds(PR_IntervalNow() - mLastLowNotification) > 5) {
          nsAppShell::gAppShell->NotifyObservers(nsnull,
                                                 "memory-pressure",
                                                 NS_LITERAL_STRING("oom-kill").get());
          mLastLowNotification = PR_IntervalNow();
      }
      return NS_OK;
  }
  
  if (memFree < mHighWaterMark) {
      __android_log_print(ANDROID_LOG_WARN, "Gecko",
                          "!!!!!!!!! Reached low memory level. MemFree = %ld",
                          memFree);
      if (PR_IntervalToSeconds(PR_IntervalNow() - mLastHighNotification) > 5) {
          nsAppShell::gAppShell->NotifyObservers(nsnull,
                                                 "memory-pressure",
                                                 NS_LITERAL_STRING("low-memory").get());
          mLastHighNotification = PR_IntervalNow();
      }
      // we should speed up the timer at this point so that we can more
      // closely watch memory usage
      aTimer->SetDelay(DEFAULT_TIMER_INTERVAL / 10);
      return NS_OK;
  }
  
  // Make sure the delay is set properly.
  aTimer->SetDelay(DEFAULT_TIMER_INTERVAL);
  return NS_OK;
}
