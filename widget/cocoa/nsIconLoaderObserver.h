/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIconLoaderObserver_h_
#define nsIconLoaderObserver_h_

#import <Cocoa/Cocoa.h>

#include "nsISupportsImpl.h"

class nsIconLoaderObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(nsIconLoaderObserver)

  /*
   * Called when the nsIconLoaderService finishes loading the icon. To be used
   * as a completion handler for nsIconLoaderService.
   */
  virtual nsresult OnComplete(NSImage* aImage) = 0;

 protected:
  virtual ~nsIconLoaderObserver() {}
};

#endif  // nsIconLoaderObserver_h_
