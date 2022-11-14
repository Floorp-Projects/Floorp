/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "nsString.h"
#include "nsUrlClassifierPrefixSet.h"

#include "Common.h"

// This function generate N 4 byte prefixes.
static void RandomPrefixes(uint32_t N, nsTArray<uint32_t>& array) {
  array.Clear();
  array.SetCapacity(N);

  for (uint32_t i = 0; i < N; i++) {
    bool added = false;

    while (!added) {
      nsAutoCString prefix;
      char* dst = prefix.BeginWriting();
      for (uint32_t j = 0; j < 4; j++) {
        dst[j] = static_cast<char>(rand() % 256);
      }

      const char* src = prefix.BeginReading();
      uint32_t data = 0;
      memcpy(&data, src, sizeof(data));

      if (!array.Contains(data)) {
        array.AppendElement(data);
        added = true;
      }
    }
  }

  struct Comparator {
    bool LessThan(const uint32_t& aA, const uint32_t& aB) const {
      return aA < aB;
    }

    bool Equals(const uint32_t& aA, const uint32_t& aB) const {
      return aA == aB;
    }
  };

  array.Sort(Comparator());
}

void RunTest(uint32_t aTestSize) {
  RefPtr<nsUrlClassifierPrefixSet> prefixSet = new nsUrlClassifierPrefixSet();
  nsTArray<uint32_t> array;

  RandomPrefixes(aTestSize, array);

  nsresult rv = prefixSet->SetPrefixes(array.Elements(), array.Length());
  ASSERT_NS_SUCCEEDED(rv);

  for (uint32_t i = 0; i < array.Length(); i++) {
    uint32_t value = 0;
    rv = prefixSet->GetPrefixByIndex(i, &value);
    ASSERT_NS_SUCCEEDED(rv);

    ASSERT_TRUE(value == array[i]);
  }
}

TEST(URLClassifierPrefixSet, GetTargetPrefixWithLargeSet)
{
  // Make sure the delta algorithm will be used.
  static const char prefKey[] = "browser.safebrowsing.prefixset_max_array_size";
  mozilla::Preferences::SetUint(prefKey, 10000);

  // Ideally, we should test more than 512 * 1024 entries. But, it will make the
  // test too long. So, we test 100k entries instead.
  RunTest(100000);
}

TEST(URLClassifierPrefixSet, GetTargetPrefixWithSmallSet)
{
  // Make sure the delta algorithm won't be used.
  static const char prefKey[] = "browser.safebrowsing.prefixset_max_array_size";
  mozilla::Preferences::SetUint(prefKey, 10000);

  RunTest(1000);
}
