/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/WeakPtr.h"
#include "mozilla/UniquePtr.h"
#include <thread>

using namespace mozilla;

struct C : public SupportsWeakPtr {
  int mNum{0};
};

struct HasWeakPtrToC {
  HasWeakPtrToC(C* c) : mPtr(c) {}

  MainThreadWeakPtr<C> mPtr;

  ~HasWeakPtrToC() {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread(), "Should be released OMT");
  }
};

TEST(MFBT_MainThreadWeakPtr, Basic)
{
  auto c = MakeUnique<C>();
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  auto weakRef = MakeUnique<HasWeakPtrToC>(c.get());

  std::thread t([weakRef = std::move(weakRef)] {});

  MOZ_RELEASE_ASSERT(!weakRef);
  c = nullptr;

  t.join();
}

