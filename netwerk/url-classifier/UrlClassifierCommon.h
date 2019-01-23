/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierCommon_h
#define mozilla_net_UrlClassifierCommon_h

#include "nsString.h"
#include "mozilla/AntiTrackingCommon.h"

class nsIChannel;
class nsIURI;

#define UC_LOG(args) MOZ_LOG(UrlClassifierCommon::sLog, LogLevel::Info, args)
#define UC_LOG_ENABLED() MOZ_LOG_TEST(UrlClassifierCommon::sLog, LogLevel::Info)

namespace mozilla {
namespace net {

class UrlClassifierCommon final {
 public:
  static const nsCString::size_type sMaxSpecLength;

  static LazyLogModule sLog;

  static bool AddonMayLoad(nsIChannel* aChannel, nsIURI* aURI);

  static void NotifyTrackingProtectionDisabled(nsIChannel* aChannel);

  static bool ShouldEnableClassifier(
      nsIChannel* aChannel,
      AntiTrackingCommon::ContentBlockingAllowListPurpose aBlockingPurpose);

  static nsresult SetBlockedContent(nsIChannel* channel, nsresult aErrorCode,
                                    const nsACString& aList,
                                    const nsACString& aProvider,
                                    const nsACString& aFullHash);

  // Use this function only when you are looking for a pairwise whitelist uri
  // with the format: http://toplevel.page/?resource=channel.uri.domain
  static nsresult CreatePairwiseWhiteListURI(nsIChannel* aChannel,
                                             nsIURI** aURI);

 private:
  // aBlockedReason must be one of the nsIWebProgressListener state.
  static void NotifyChannelBlocked(nsIChannel* aChannel,
                                   nsIURI* aURIBeingLoaded,
                                   unsigned aBlockedReason);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierCommon_h
