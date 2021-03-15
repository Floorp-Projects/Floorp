/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileUnlockerAndroid.h"
#include "nsPrintfCString.h"
#include <unistd.h>

namespace mozilla {

ProfileUnlockerAndroid::ProfileUnlockerAndroid(const pid_t aPid) : mPid(aPid) {}

ProfileUnlockerAndroid::~ProfileUnlockerAndroid() {}

NS_IMPL_ISUPPORTS(ProfileUnlockerAndroid, nsIProfileUnlocker)

NS_IMETHODIMP
ProfileUnlockerAndroid::Unlock(uint32_t aSeverity) {
  if (aSeverity != FORCE_QUIT) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_WARNING(nsPrintfCString("Process %d has the profile "
                             "lock, will try to kill it.",
                             mPid)
                 .get());
  if (mPid == getpid()) {
    NS_ERROR("Lock held by current process !?");
    return NS_ERROR_FAILURE;
  }
  if (kill(mPid, SIGKILL) != 0) {
    NS_WARNING("Could not kill process.");
  }
  return NS_OK;
}

}  // namespace mozilla
