/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintsPreloader.h"
#include "mozilla/Telemetry.h"

namespace mozilla::net {

void EarlyHintsPreloader::EarlyHint(const nsACString& linkHeader) {
  mEarlyHintsCount++;
  if (!mFirstEarlyHint) {
    mFirstEarlyHint.emplace(TimeStamp::NowLoRes());
  }
}

void EarlyHintsPreloader::FinalResponse(uint32_t aResponseStatus) {
  // We will collect telemetry mosly once for a document.
  // In case of a reddirect this will be called multiple times.
  CollectTelemetry(Some(aResponseStatus));
}

void EarlyHintsPreloader::Cancel() {
  if (!mCanceled) {
    CollectTelemetry(Nothing());
    mCanceled = true;
  }
}

void EarlyHintsPreloader::CollectTelemetry(Maybe<uint32_t> aResponseStatus) {
  // EH_NUM_OF_HINTS_PER_PAGE is only collected for the 2xx responses,
  // regardless of the number of received mEarlyHintsCount.
  // Other telemetry probes are only collected if there was at least one
  // EarlyHins response.
  if (aResponseStatus && (*aResponseStatus <= 299)) {
    Telemetry::Accumulate(Telemetry::EH_NUM_OF_HINTS_PER_PAGE,
                          mEarlyHintsCount);
  }
  if (mEarlyHintsCount == 0) {
    return;
  }

  Telemetry::LABELS_EH_FINAL_RESPONSE label =
      Telemetry::LABELS_EH_FINAL_RESPONSE::Cancel;
  if (aResponseStatus) {
    if (*aResponseStatus <= 299) {
      label = Telemetry::LABELS_EH_FINAL_RESPONSE::R2xx;

      MOZ_ASSERT(mFirstEarlyHint);
      Telemetry::AccumulateTimeDelta(Telemetry::EH_TIME_TO_FINAL_RESPONSE,
                                     *mFirstEarlyHint, TimeStamp::NowLoRes());
    } else if (*aResponseStatus <= 399) {
      label = Telemetry::LABELS_EH_FINAL_RESPONSE::R3xx;
    } else if (*aResponseStatus <= 499) {
      label = Telemetry::LABELS_EH_FINAL_RESPONSE::R4xx;
    } else {
      label = Telemetry::LABELS_EH_FINAL_RESPONSE::Other;
    }
  }

  Telemetry::AccumulateCategorical(label);

  // Reset telemetry counters and timestamps.
  mEarlyHintsCount = 0;
  mFirstEarlyHint = Nothing();
}

}  // namespace mozilla::net
