/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintsService.h"
#include "EarlyHintPreloader.h"
#include "mozilla/PreloadHashKey.h"
#include "mozilla/Telemetry.h"
#include "nsICookieJarSettings.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "nsILoadInfo.h"

namespace mozilla::net {

EarlyHintsService::EarlyHintsService()
    : mOngoingEarlyHints(new OngoingEarlyHints()) {}

// implementing the destructor in the .cpp file to allow EarlyHintsService.h
// not to include EarlyHintPreloader.h, decoupling the two files and hopefully
// allow faster compile times
EarlyHintsService::~EarlyHintsService() = default;

void EarlyHintsService::EarlyHint(const nsACString& aLinkHeader,
                                  nsIURI* aBaseURI, nsILoadInfo* aLoadInfo) {
  mEarlyHintsCount++;
  if (!mFirstEarlyHint) {
    mFirstEarlyHint.emplace(TimeStamp::NowLoRes());
  }

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aLoadInfo->TriggeringPrincipal();

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (NS_FAILED(
          aLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings)))) {
    return;
  }

  // TODO: find out why LinkHeaderParser uses utf16 and check if it can be
  //       changed to utf8
  auto linkHeaders = ParseLinkHeader(NS_ConvertUTF8toUTF16(aLinkHeader));

  for (auto& linkHeader : linkHeaders) {
    EarlyHintPreloader::MaybeCreateAndInsertPreload(
        mOngoingEarlyHints, linkHeader, aBaseURI, triggeringPrincipal,
        cookieJarSettings);
  }
}

void EarlyHintsService::FinalResponse(uint32_t aResponseStatus) {
  // We will collect telemetry mosly once for a document.
  // In case of a reddirect this will be called multiple times.
  CollectTelemetry(Some(aResponseStatus));
  if (aResponseStatus >= 300) {
    mOngoingEarlyHints->CancelAllOngoingPreloads();
    mCanceled = true;
  }
}

void EarlyHintsService::Cancel() {
  if (!mCanceled) {
    CollectTelemetry(Nothing());
    mOngoingEarlyHints->CancelAllOngoingPreloads();
    mCanceled = true;
  }
}

void EarlyHintsService::CollectTelemetry(Maybe<uint32_t> aResponseStatus) {
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
