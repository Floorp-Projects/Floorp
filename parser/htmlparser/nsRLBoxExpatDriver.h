/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_RLBOX_EXPAT_DRIVER__
#define NS_RLBOX_EXPAT_DRIVER__

#include "mozilla/RLBoxSandboxPool.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"

class RLBoxExpatSandboxPool : public mozilla::RLBoxSandboxPool {
 public:
  explicit RLBoxExpatSandboxPool(size_t aDelaySeconds)
      : RLBoxSandboxPool(aDelaySeconds) {}

  static mozilla::StaticRefPtr<RLBoxExpatSandboxPool> sSingleton;
  static void Initialize(size_t aDelaySeconds = 10);

 protected:
  mozilla::UniquePtr<mozilla::RLBoxSandboxDataBase> CreateSandboxData(
      uint64_t aSize) override;
  ~RLBoxExpatSandboxPool() = default;
};

#endif
