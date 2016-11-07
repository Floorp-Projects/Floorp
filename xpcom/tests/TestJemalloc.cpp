/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Ideally, this test would be in memory/test/.  But I couldn't get it to build
 * there (couldn't find TestHarness.h).  I think memory/ is processed too early
 * in the build.  So it's here.
 */

#include "TestHarness.h"
#include "mozmemory.h"

static inline bool
TestOne(size_t size)
{
    size_t req = size;
    size_t adv = malloc_good_size(req);
    char* p = (char*)malloc(req);
    size_t usable = moz_malloc_usable_size(p);
    if (adv != usable) {
      fail("malloc_good_size(%d) --> %d; "
           "malloc_usable_size(%d) --> %d",
           req, adv, req, usable);
      return false;
    }
    free(p);
    return true;
}

static inline bool
TestThree(size_t size)
{
    return TestOne(size - 1) && TestOne(size) && TestOne(size + 1);
}

static nsresult
TestJemallocUsableSizeInAdvance()
{
  #define K   * 1024
  #define M   * 1024 * 1024

  /*
   * Test every size up to a certain point, then (N-1, N, N+1) triplets for a
   * various sizes beyond that.
   */

  for (size_t n = 0; n < 16 K; n++)
    if (!TestOne(n))
      return NS_ERROR_UNEXPECTED;

  for (size_t n = 16 K; n < 1 M; n += 4 K)
    if (!TestThree(n))
      return NS_ERROR_UNEXPECTED;

  for (size_t n = 1 M; n < 8 M; n += 128 K)
    if (!TestThree(n))
      return NS_ERROR_UNEXPECTED;

  passed("malloc_good_size");

  return NS_OK;
}

int main(int argc, char** argv)
{
  int rv = 0;
  ScopedXPCOM xpcom("jemalloc");
  if (xpcom.failed())
      return 1;

  if (NS_FAILED(TestJemallocUsableSizeInAdvance()))
    rv = 1;

  return rv;
}

