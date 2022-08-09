/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintsService_h
#define mozilla_net_EarlyHintsService_h

#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsStringFwd.h"
#include "mozilla/RefPtr.h"

class nsIChannel;
class nsIURI;

namespace mozilla::net {

class OngoingEarlyHints;

class EarlyHintsService {
 public:
  EarlyHintsService();
  ~EarlyHintsService();
  void EarlyHint(const nsACString& aLinkHeader, nsIURI* aBaseURI,
                 nsIChannel* aChannel);
  void FinalResponse(uint32_t aResponseStatus);
  void Cancel();

 private:
  void CollectTelemetry(Maybe<uint32_t> aResponseStatus);

  Maybe<TimeStamp> mFirstEarlyHint;
  uint32_t mEarlyHintsCount{0};
  bool mCanceled{false};

  RefPtr<OngoingEarlyHints> mOngoingEarlyHints;
};

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintsService_h
