/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipboardHelper_h__
#define nsClipboardHelper_h__

// interfaces
#include "nsIClipboardHelper.h"

// basics
#include "nsString.h"

/**
 * impl class for nsIClipboardHelper, a helper for common uses of nsIClipboard.
 */

class nsClipboardHelper : public nsIClipboardHelper {
  virtual ~nsClipboardHelper();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARDHELPER

  nsClipboardHelper();
};

#endif  // nsClipboardHelper_h__
