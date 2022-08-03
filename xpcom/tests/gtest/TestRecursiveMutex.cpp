/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/RecursiveMutex.h"
#include "gtest/gtest.h"

using mozilla::RecursiveMutex;
using mozilla::RecursiveMutexAutoLock;

// Basic test to make sure the underlying implementation of RecursiveMutex is,
// well, actually recursively acquirable.

TEST(RecursiveMutex, SmokeTest)
MOZ_NO_THREAD_SAFETY_ANALYSIS {
  RecursiveMutex mutex("testing mutex");

  RecursiveMutexAutoLock lock1(mutex);
  RecursiveMutexAutoLock lock2(mutex);

  //...and done.
}
