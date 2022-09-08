/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintsService.h"
#include "EarlyHintPreloader.h"
#include "mozilla/PreloadHashKey.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsICookieJarSettings.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsString.h"

namespace mozilla::net {

EarlyHintsService::EarlyHintsService()
    : mOngoingEarlyHints(new OngoingEarlyHints()) {}

// implementing the destructor in the .cpp file to allow EarlyHintsService.h
// not to include EarlyHintPreloader.h, decoupling the two files and hopefully
// allow faster compile times
EarlyHintsService::~EarlyHintsService() = default;

void EarlyHintsService::EarlyHint(const nsACString& aLinkHeader,
                                  nsIURI* aBaseURI, nsIChannel* aChannel) {
  mEarlyHintsCount++;
  if (!mFirstEarlyHint) {
    mFirstEarlyHint.emplace(TimeStamp::NowLoRes());
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  // We only follow Early Hints sent on the main document. Make sure that we got
  // the main document channel here.
  if (loadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_DOCUMENT) {
    MOZ_ASSERT(false, "Early Hint on non-document channel");
    return;
  }
  nsCOMPtr<nsIPrincipal> principal;
  // We want to set the top-level document as the triggeringPrincipal for the
  // load of the sub-resources (image, font, fetch, script, style, fetch and in
  // the future maybe more). We can't use the `triggeringPrincipal` of the main
  // document channel, because it is the `systemPrincipal` for user initiated
  // loads. Same for the `LoadInfo::FindPrincipalToInherit(aChannel)`.
  //
  // On 3xx redirects of the main document to cross site locations, all Early
  // Hint preloads get cancelled as specified in the whatwg spec:
  //
  //   Note: Only the first early hint response served during the navigation is
  //   handled, and it is discarded if it is succeeded by a cross-origin
  //   redirect. [1]
  //
  // Therefore the channel doesn't need to change the principal for any reason
  // and has the correct principal for the whole lifetime.
  //
  // [1]: https://html.spec.whatwg.org/multipage/semantics.html#early-hints
  nsresult rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (NS_FAILED(
          loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings)))) {
    return;
  }

  // TODO: find out why LinkHeaderParser uses utf16 and check if it can be
  //       changed to utf8
  auto linkHeaders = ParseLinkHeader(NS_ConvertUTF8toUTF16(aLinkHeader));

  for (auto& linkHeader : linkHeaders) {
    EarlyHintPreloader::MaybeCreateAndInsertPreload(
        mOngoingEarlyHints, linkHeader, aBaseURI, principal, cookieJarSettings);
  }
}

void EarlyHintsService::FinalResponse(uint32_t aResponseStatus) {
  // We will collect telemetry mosly once for a document.
  // In case of a reddirect this will be called multiple times.
  CollectTelemetry(Some(aResponseStatus));
  if (aResponseStatus >= 300 && aResponseStatus < 400) {
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
