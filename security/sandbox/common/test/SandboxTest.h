/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef mozilla_SandboxTest_h
#define mozilla_SandboxTest_h

#include "SandboxTestingParent.h"
#include "mozISandboxTest.h"

#if !defined(MOZ_DEBUG) || !defined(ENABLE_TESTS)
#  error "This file should not be used outside of debug with tests"
#endif

namespace mozilla {
class SandboxTest : public mozISandboxTest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXTEST

  SandboxTest() : mSandboxTestingParents{nullptr} {};

 private:
  virtual ~SandboxTest() = default;
  static constexpr size_t NumProcessTypes =
      static_cast<size_t>(GeckoProcessType_End);
  SandboxTestingParent* mSandboxTestingParents[NumProcessTypes];
};

}  // namespace mozilla
#endif  // mozilla_SandboxTest_h
