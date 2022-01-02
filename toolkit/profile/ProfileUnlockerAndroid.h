/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileUnlockerAndroid_h
#define ProfileUnlockerAndroid_h

#include "nsIProfileUnlocker.h"

namespace mozilla {

class ProfileUnlockerAndroid final : public nsIProfileUnlocker {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILEUNLOCKER

  explicit ProfileUnlockerAndroid(const pid_t aPid);

 private:
  ~ProfileUnlockerAndroid();
  pid_t mPid;
};

}  // namespace mozilla

#endif  // ProfileUnlockerAndroid_h
