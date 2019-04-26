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

  static void NotifyChannelClassifierProtectionDisabled(
      nsIChannel* aChannel, uint32_t aAcceptedReason);

  static bool ShouldEnableClassifier(nsIChannel* aChannel);

  static nsresult SetBlockedContent(nsIChannel* channel, nsresult aErrorCode,
                                    const nsACString& aList,
                                    const nsACString& aProvider,
                                    const nsACString& aFullHash);

  static nsresult SetTrackingInfo(nsIChannel* channel,
                                  const nsTArray<nsCString>& aLists,
                                  const nsTArray<nsCString>& aFullHashes);

  // Use this function only when you are looking for a pairwise whitelist uri
  // with the format: http://toplevel.page/?resource=channel.uri.domain
  static nsresult CreatePairwiseWhiteListURI(nsIChannel* aChannel,
                                             nsIURI** aURI);

  static void AnnotateChannel(
      nsIChannel* aChannel,
      AntiTrackingCommon::ContentBlockingAllowListPurpose aPurpose,
      uint32_t aClassificationFlags, uint32_t aLoadingState);

  static bool IsAllowListed(
      nsIChannel* aChannel,
      AntiTrackingCommon::ContentBlockingAllowListPurpose aPurpose);

  static bool IsTrackingClassificationFlag(uint32_t aFlag);

  static bool IsCryptominingClassificationFlag(uint32_t aFlag);

  // Join the table names in 1 single string.
  static void TablesToString(const nsTArray<nsCString>& aList,
                             nsACString& aString);

  struct ClassificationData {
    nsCString mPrefix;
    uint32_t mFlag;
  };

  // Checks if the entries in aList are part of the ClassificationData vector
  // and it returns the corresponding flags. If none of them is found, the
  // default flag is returned.
  static uint32_t TablesToClassificationFlags(
      const nsTArray<nsCString>& aList,
      const std::vector<ClassificationData>& aData, uint32_t aDefaultFlag);

 private:
  // aBlockedReason must be one of the nsIWebProgressListener state.
  static void NotifyChannelBlocked(nsIChannel* aChannel,
                                   nsIURI* aURIBeingLoaded,
                                   unsigned aBlockedReason);

  static uint32_t TableToClassificationFlag(
      const nsACString& aTable, const std::vector<ClassificationData>& aData);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierCommon_h
