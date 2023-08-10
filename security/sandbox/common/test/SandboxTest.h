/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef mozilla_SandboxTest_h
#define mozilla_SandboxTest_h

#include "SandboxTestingParent.h"
#include "mozISandboxTest.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/MozPromise.h"
#include "GMPService.h"
#include "nsTArray.h"

#if !defined(MOZ_DEBUG) || !defined(ENABLE_TESTS)
#  error "This file should not be used outside of debug with tests"
#endif

namespace mozilla {

class SandboxTest : public mozISandboxTest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISANDBOXTEST

  SandboxTest() : mSandboxTestingParents{nullptr} {};

  // We allow nsresult to be rejected with values:
  //  - NS_ERROR_FAILURE in obvious case of error
  //  - NS_OK in case of success to complete the code but missing process (GPU)
  using ProcessPromise =
      MozPromise<RefPtr<SandboxTestingParent>, nsresult, true>;

 private:
  virtual ~SandboxTest() = default;
  nsTArray<RefPtr<SandboxTestingParent>> mSandboxTestingParents;
  RefPtr<gmp::GMPContentParentCloseBlocker> mGMPContentParentWrapper;
#if defined(XP_WIN)
  bool mChromeDirExisted = false;
#endif
};

}  // namespace mozilla
#endif  // mozilla_SandboxTest_h
