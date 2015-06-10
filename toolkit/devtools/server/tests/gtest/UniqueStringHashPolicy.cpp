/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bug 1171226 - Test UniqueStringHashPolicy::match

#include "DevTools.h"
#include "mozilla/devtools/HeapSnapshot.h"

using mozilla::devtools::UniqueString;
using mozilla::devtools::UniqueStringHashPolicy;

DEF_TEST(UniqueStringHashPolicy_match, {
    //                                               1
    //                                     01234567890123456
    UniqueString str1(NS_strdup(MOZ_UTF16("some long string and a tail")));
    ASSERT_TRUE(!!str1);

    UniqueStringHashPolicy::Lookup lookup(MOZ_UTF16("some long string with same prefix"), 16);

    // str1 is longer than Lookup.length, so they shouldn't match, even though
    // the first 16 chars are equal!
    ASSERT_FALSE(UniqueStringHashPolicy::match(str1, lookup));
  });
