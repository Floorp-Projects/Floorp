/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/ScopeExit.h"

using mozilla::MakeScopeExit;

#define CHECK(c)                                       \
  do {                                                 \
    bool cond = !!(c);                                 \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
    if (!cond) {                                       \
      return false;                                    \
    }                                                  \
  } while (false)

static bool Test() {
  int a = 1;
  int b = 1;
  int c = 1;

  {
    a++;
    auto guardA = MakeScopeExit([&] { a--; });

    b++;
    auto guardB = MakeScopeExit([&] { b--; });

    guardB.release();

    c++;
    auto guardC = MakeScopeExit([&] { c--; });

    { auto guardC_ = std::move(guardC); }

    CHECK(c == 1);
  }

  CHECK(a == 1);
  CHECK(b == 2);
  CHECK(c == 1);

  return true;
}

int main() {
  if (!Test()) {
    return 1;
  }
  return 0;
}
