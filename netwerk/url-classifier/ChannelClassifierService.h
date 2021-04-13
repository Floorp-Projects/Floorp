/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ChannelClassifierService_h
#define mozilla_net_ChannelClassifierService_h

#include "nsIChannelClassifierService.h"
#include "mozilla/net/UrlClassifierCommon.h"

namespace mozilla {
namespace net {

enum class ChannelBlockDecision {
  Blocked,
  Replaced,
  Allowed,
};

class UrlClassifierBlockedChannel final
    : public nsIUrlClassifierBlockedChannel {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERBLOCKEDCHANNEL

  explicit UrlClassifierBlockedChannel(nsIChannel* aChannel);

  bool IsUnblocked() const {
    return mDecision != ChannelBlockDecision::Blocked;
  }

  ChannelBlockDecision GetDecision() { return mDecision; };

  void SetReason(const nsACString& aFeatureName, const nsACString& aTableName);

 protected:
  ~UrlClassifierBlockedChannel() = default;

 private:
  nsCOMPtr<nsIChannel> mChannel;
  ChannelBlockDecision mDecision;
  uint8_t mReason;
  nsCString mTables;
};

class ChannelClassifierService final : public nsIChannelClassifierService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNELCLASSIFIERSERVICE

  friend class UrlClassifierBlockedChannel;

  static already_AddRefed<nsIChannelClassifierService> GetSingleton();

  static ChannelBlockDecision OnBeforeBlockChannel(
      nsIChannel* aChannel, const nsACString& aFeatureName,
      const nsACString& aTableName);

  nsresult OnBeforeBlockChannel(nsIChannel* aChannel,
                                const nsACString& aFeatureName,
                                const nsACString& aTableName,
                                ChannelBlockDecision& aDecision);

  bool HasListener() const { return !mListeners.IsEmpty(); }

 private:
  ChannelClassifierService();
  ~ChannelClassifierService() = default;

  nsTArray<nsCOMPtr<nsIObserver>> mListeners;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ChannelClassifierService_h
