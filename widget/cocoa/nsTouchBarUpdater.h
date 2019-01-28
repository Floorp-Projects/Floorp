/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTouchBarUpdater_h_
#define nsTouchBarUpdater_h_

#include "nsITouchBarUpdater.h"

class nsTouchBarUpdater : public nsITouchBarUpdater {
 public:
  nsTouchBarUpdater() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSITOUCHBARUPDATER

 protected:
  virtual ~nsTouchBarUpdater() {}
};

#endif  // nsTouchBarUpdater_h_
