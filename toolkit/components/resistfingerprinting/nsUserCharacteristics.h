/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsUserCharacteristics_h__
#define __nsUserCharacteristics_h__

#include "ErrorList.h"

class nsUserCharacteristics {
 public:
  static void MaybeSubmitPing();

  // Public For testing
  static nsresult PopulateData(bool aTesting = false);
  static nsresult SubmitPing();
};

namespace testing {
extern "C" {  // Needed to call these in the gtest

int MaxTouchPoints();

}  // extern "C"
};  // namespace testing

#endif /*  __nsUserCharacteristics_h__ */
