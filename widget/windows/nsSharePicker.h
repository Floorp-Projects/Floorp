/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSharePicker_h__
#define nsSharePicker_h__

#include "nsCOMPtr.h"
#include "nsISharePicker.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"

class nsSharePicker : public nsISharePicker {
  virtual ~nsSharePicker() = default;

 public:
  nsSharePicker() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHAREPICKER

 private:
  bool mInited = false;
  mozIDOMWindowProxy* mOpenerWindow;
};

#endif  // nsSharePicker_h__
