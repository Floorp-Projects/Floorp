/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/InitializerList.h"

int sum(std::initializer_list<int> p)
{
  int result = 0;
  for (auto i : p) {
    result += i;
  }
  return result;
}

int
main()
{
  MOZ_RELEASE_ASSERT(sum({1, 2, 3, 4, 5, 6}) == 7 * 3);
  return 0;
}
