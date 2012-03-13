/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BloomFilter.h"
#include "TestHarness.h"

#include <stdio.h>

using namespace mozilla;

class FilterChecker {
public:
  FilterChecker(uint32_t hash) :
    mHash(hash)
  {}
  
  uint32_t hash() const { return mHash; }

private:
  uint32_t mHash;
};

int main()
{
  BloomFilter<12, FilterChecker> *filter = new BloomFilter<12, FilterChecker>();

  FilterChecker one(1);
  FilterChecker two(0x20000);
  FilterChecker many(0x10000);
  FilterChecker multiple(0x20001);

  filter->add(&one);
  if (!filter->mightContain(&one)) {
    fail("Filter should contain 'one'");
    return -1;
  }

  if (filter->mightContain(&multiple)) {
    fail("Filter claims to contain 'multiple' when it should not");
    return -1;
  }

  if (!filter->mightContain(&many)) {
    fail("Filter should contain 'many' (false positive)");
    return -1;
  }

  filter->add(&two);
  if (!filter->mightContain(&multiple)) {
    fail("Filter should contain 'multiple' (false positive)");
    return -1;
  }

  // Test basic removals
  filter->remove(&two);
  if (filter->mightContain(&multiple)) {
    fail("Filter claims to contain 'multiple' when it should not after two was "
         "removed");
    return -1;
  }

  // Test multiple addition/removal
  const unsigned FILTER_SIZE = 255;
  for (unsigned i = 0; i < FILTER_SIZE - 1; ++i) {
    filter->add(&two);
  }
  if (!filter->mightContain(&multiple)) {
    fail("Filter should contain 'multiple' after 'two' added lots of times "
         "(false positive)");
    return -1;
  }
  for (unsigned i = 0; i < FILTER_SIZE - 1; ++i) {
    filter->remove(&two);
  }
  if (filter->mightContain(&multiple)) {
    fail("Filter claims to contain 'multiple' when it should not after two was "
         "removed lots of times");
    return -1;
  }

  // Test overflowing the filter buckets
  for (unsigned i = 0; i < FILTER_SIZE + 1; ++i) {
    filter->add(&two);
  }
  if (!filter->mightContain(&multiple)) {
    fail("Filter should contain 'multiple' after 'two' added lots more times "
         "(false positive)");
    return -1;
  }
  for (unsigned i = 0; i < FILTER_SIZE + 1; ++i) {
    filter->remove(&two);
  }
  if (!filter->mightContain(&multiple)) {
    fail("Filter claims to not contain 'multiple' even though we should have "
         "run out of space in the buckets (false positive)");
    return -1;
  }
  if (!filter->mightContain(&two)) {
    fail("Filter claims to not contain 'two' even though we should have run "
         "out of space in the buckets (false positive)");
    return -1;
  }

  filter->remove(&one);
  if (filter->mightContain(&one)) {
    fail("Filter should not contain 'one', because we didn't overflow its "
         "bucket");
    return -1;
  }
  
  filter->clear();
  if (filter->mightContain(&multiple)) {
    fail("clear() failed to work");
    return -1;
  }

  return 0;
}
