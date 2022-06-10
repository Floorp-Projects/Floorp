/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringStats_h
#define nsStringStats_h

#include "mozilla/Atomics.h"

class nsStringStats {
 public:
  nsStringStats() = default;

  ~nsStringStats();

  using AtomicInt = mozilla::Atomic<int32_t, mozilla::SequentiallyConsistent>;

  AtomicInt mAllocCount{0};
  AtomicInt mReallocCount{0};
  AtomicInt mFreeCount{0};
  AtomicInt mShareCount{0};
  AtomicInt mAdoptCount{0};
  AtomicInt mAdoptFreeCount{0};
};

extern nsStringStats gStringStats;

#define STRING_STAT_INCREMENT(_s) (gStringStats.m##_s##Count)++

#endif  // nsStringStats_h
