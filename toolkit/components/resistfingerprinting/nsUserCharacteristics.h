/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef __nsUserCharacteristics_h__
#define __nsUserCharacteristics_h__

#include "ErrorList.h"

class nsUserCharacteristics {
 public:
  static void MaybeSubmitPing();

  /*
   * These APIs are public only for testing using the gtest
   * When PopulateDataAndEventuallySubmit is called with aTesting = true
   *   it will not submit the data, and SubmitPing must be called explicitly.
   *   This is perfect because that's what we want for the gtest.
   */
  static void PopulateDataAndEventuallySubmit(bool aUpdatePref = true,
                                              bool aTesting = false);
  static void SubmitPing();
};

namespace testing {
extern "C" {  // Needed to call these in the gtest

int MaxTouchPoints();

}  // extern "C"
};  // namespace testing

#endif /*  __nsUserCharacteristics_h__ */
