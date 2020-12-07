/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppStartupNotifier_h___
#define nsAppStartupNotifier_h___

#include "nsIAppStartupNotifier.h"

class nsAppStartupNotifier final {
 public:
  static nsresult NotifyObservers(const char* aTopic);
};

#endif /* nsAppStartupNotifier_h___ */
