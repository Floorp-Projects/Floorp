/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintsPreload_h
#define mozilla_net_EarlyHintsPreload_h

#include "nsStringFwd.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"

namespace mozilla::net {

class EarlyHintsPreloader {
 public:
  EarlyHintsPreloader() = default;
  ~EarlyHintsPreloader() = default;
  void EarlyHint(const nsACString& linkHeader);
  void FinalResponse(uint32_t aResponseStatus);
  void Cancel();

 private:
  void CollectTelemetry(Maybe<uint32_t> aResponseStatus);

  Maybe<TimeStamp> mFirstEarlyHint;
  uint32_t mEarlyHintsCount{0};
  bool mCanceled{false};
};

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintsPreload_h
