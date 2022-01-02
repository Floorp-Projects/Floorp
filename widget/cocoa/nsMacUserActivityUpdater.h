/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacUserActivityUpdater_h_
#define nsMacUserActivityUpdater_h_

#include "nsIMacUserActivityUpdater.h"
#include "nsCocoaWindow.h"

class nsMacUserActivityUpdater : public nsIMacUserActivityUpdater {
 public:
  nsMacUserActivityUpdater() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACUSERACTIVITYUPDATER

 protected:
  virtual ~nsMacUserActivityUpdater() {}
  BaseWindow* GetCocoaWindow(nsIBaseWindow* aWindow);
};

#endif  // nsMacUserActivityUpdater_h_
