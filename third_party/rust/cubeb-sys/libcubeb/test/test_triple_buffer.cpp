/*
 * Copyright © 2022 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* cubeb_triple_buffer test  */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include "cubeb/cubeb.h"
#include "cubeb_triple_buffer.h"
#include <atomic>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "common.h"

TEST(cubeb, triple_buffer)
{
  struct AB {
    uint64_t a;
    uint64_t b;
  };
  triple_buffer<AB> buffer;

  std::atomic<bool> finished = {false};

  ASSERT_TRUE(!buffer.updated());

  auto t = std::thread([&finished, &buffer] {
    AB ab;
    ab.a = 0;
    ab.b = UINT64_MAX;
    uint64_t counter = 0;
    do {
      buffer.write(ab);
      ab.a++;
      ab.b--;
    } while (counter++ < 1e6 && ab.a <= UINT64_MAX && ab.b != 0);
    finished.store(true);
  });

  AB ab;
  AB old_ab;
  old_ab.a = 0;
  old_ab.b = UINT64_MAX;

  // Wait to have at least one value produced.
  while (!buffer.updated()) {
  }

  // Check that the values are increasing (resp. descreasing) monotonically.
  while (!finished) {
    ab = buffer.read();
    ASSERT_GE(ab.a, old_ab.a);
    ASSERT_LE(ab.b, old_ab.b);
    old_ab = ab;
  }

  t.join();
}
