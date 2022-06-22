/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/UniquePtr.h"

#include <stddef.h>
#include <stdint.h>

using mozilla::BitBloomFilter;
using mozilla::CountingBloomFilter;

class FilterChecker {
 public:
  explicit FilterChecker(uint32_t aHash) : mHash(aHash) {}

  uint32_t hash() const { return mHash; }

 private:
  uint32_t mHash;
};

void testBitBloomFilter() {
  const mozilla::UniquePtr filter =
      mozilla::MakeUnique<BitBloomFilter<12, FilterChecker>>();
  MOZ_RELEASE_ASSERT(filter);

  FilterChecker one(1);
  FilterChecker two(0x20000);

  filter->add(&one);
  MOZ_RELEASE_ASSERT(filter->mightContain(&one), "Filter should contain 'one'");

  MOZ_RELEASE_ASSERT(!filter->mightContain(&two),
                     "Filter claims to contain 'two' when it should not");

  // Test multiple addition
  filter->add(&two);
  MOZ_RELEASE_ASSERT(filter->mightContain(&two),
                     "Filter should contain 'two' after 'two' is added");
  filter->add(&two);
  MOZ_RELEASE_ASSERT(filter->mightContain(&two),
                     "Filter should contain 'two' after 'two' is added again");

  filter->clear();

  MOZ_RELEASE_ASSERT(!filter->mightContain(&one), "clear() failed to work");
  MOZ_RELEASE_ASSERT(!filter->mightContain(&two), "clear() failed to work");
}

void testCountingBloomFilter() {
  const mozilla::UniquePtr filter =
      mozilla::MakeUnique<CountingBloomFilter<12, FilterChecker>>();
  MOZ_RELEASE_ASSERT(filter);

  FilterChecker one(1);
  FilterChecker two(0x20000);
  FilterChecker many(0x10000);
  FilterChecker multiple(0x20001);

  filter->add(&one);
  MOZ_RELEASE_ASSERT(filter->mightContain(&one), "Filter should contain 'one'");

  MOZ_RELEASE_ASSERT(!filter->mightContain(&multiple),
                     "Filter claims to contain 'multiple' when it should not");

  MOZ_RELEASE_ASSERT(filter->mightContain(&many),
                     "Filter should contain 'many' (false positive)");

  filter->add(&two);
  MOZ_RELEASE_ASSERT(filter->mightContain(&multiple),
                     "Filter should contain 'multiple' (false positive)");

  // Test basic removals
  filter->remove(&two);
  MOZ_RELEASE_ASSERT(
      !filter->mightContain(&multiple),
      "Filter claims to contain 'multiple' when it should not after two "
      "was removed");

  // Test multiple addition/removal
  const size_t FILTER_SIZE = 255;
  for (size_t i = 0; i < FILTER_SIZE - 1; ++i) {
    filter->add(&two);
  }
  MOZ_RELEASE_ASSERT(
      filter->mightContain(&multiple),
      "Filter should contain 'multiple' after 'two' added lots of times "
      "(false positive)");

  for (size_t i = 0; i < FILTER_SIZE - 1; ++i) {
    filter->remove(&two);
  }
  MOZ_RELEASE_ASSERT(
      !filter->mightContain(&multiple),
      "Filter claims to contain 'multiple' when it should not after two "
      "was removed lots of times");

  // Test overflowing the filter buckets
  for (size_t i = 0; i < FILTER_SIZE + 1; ++i) {
    filter->add(&two);
  }
  MOZ_RELEASE_ASSERT(
      filter->mightContain(&multiple),
      "Filter should contain 'multiple' after 'two' added lots more "
      "times (false positive)");

  for (size_t i = 0; i < FILTER_SIZE + 1; ++i) {
    filter->remove(&two);
  }
  MOZ_RELEASE_ASSERT(
      filter->mightContain(&multiple),
      "Filter claims to not contain 'multiple' even though we should "
      "have run out of space in the buckets (false positive)");
  MOZ_RELEASE_ASSERT(
      filter->mightContain(&two),
      "Filter claims to not contain 'two' even though we should have "
      "run out of space in the buckets (false positive)");

  filter->remove(&one);

  MOZ_RELEASE_ASSERT(
      !filter->mightContain(&one),
      "Filter should not contain 'one', because we didn't overflow its "
      "bucket");

  filter->clear();

  MOZ_RELEASE_ASSERT(!filter->mightContain(&multiple),
                     "clear() failed to work");
}

int main() {
  testBitBloomFilter();
  testCountingBloomFilter();

  return 0;
}
