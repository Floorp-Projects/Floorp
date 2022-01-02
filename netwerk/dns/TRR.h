/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRR_h
#define mozilla_net_TRR_h

#include "mozilla/net/DNSByTypeRecord.h"
#include "mozilla/Assertions.h"
#include "nsClassHashtable.h"
#include "nsIChannel.h"
#include "nsIHttpPushListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "DNSPacket.h"
#include "TRRSkippedReason.h"

class AHostResolver;
class nsHostRecord;

namespace mozilla {
namespace net {

class TRRService;
class TRRServiceChannel;

class TRR : public Runnable,
            public nsITimerCallback,
            public nsIHttpPushListener,
            public nsIInterfaceRequestor,
            public nsIStreamListener {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIHTTPPUSHLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK

  // Number of "steps" we follow CNAME chains
  static const unsigned int kCnameChaseMax = 64;

  // when firing off a normal A or AAAA query
  explicit TRR(AHostResolver* aResolver, nsHostRecord* aRec,
               enum TrrType aType);
  // when following CNAMEs
  explicit TRR(AHostResolver* aResolver, nsHostRecord* aRec, nsCString& aHost,
               enum TrrType& aType, unsigned int aLoopCount, bool aPB);
  // used on push
  explicit TRR(AHostResolver* aResolver, bool aPB);
  // to verify a domain
  explicit TRR(AHostResolver* aResolver, nsACString& aHost, enum TrrType aType,
               const nsACString& aOriginSuffix, bool aPB,
               bool aUseFreshConnection);

  NS_IMETHOD Run() override;
  void Cancel(nsresult aStatus);
  enum TrrType Type() { return mType; }
  nsCString mHost;
  RefPtr<nsHostRecord> mRec;
  RefPtr<AHostResolver> mHostResolver;

  void SetTimeout(uint32_t aTimeoutMs) { mTimeoutMs = aTimeoutMs; }

  nsresult ChannelStatus() { return mChannelStatus; }

  enum RequestPurpose {
    Resolve,
    Confirmation,
    Blocklist,
  };

  RequestPurpose Purpose() { return mPurpose; }
  void SetPurpose(RequestPurpose aPurpose) { mPurpose = aPurpose; }

 protected:
  virtual ~TRR() = default;
  virtual DNSPacket* GetOrCreateDNSPacket();
  virtual nsresult CreateQueryURI(nsIURI** aOutURI);
  virtual const char* ContentType() const { return "application/dns-message"; }
  virtual DNSResolverType ResolverType() const { return DNSResolverType::TRR; }
  virtual bool MaybeBlockRequest();
  virtual void RecordProcessingTime(nsIChannel* aChannel);
  virtual void ReportStatus(nsresult aStatusCode);
  virtual void HandleTimeout();
  virtual void HandleEncodeError(nsresult aStatusCode) {}
  virtual void HandleDecodeError(nsresult aStatusCode);
  nsresult SendHTTPRequest();
  nsresult ReturnData(nsIChannel* aChannel);

  // FailData() must be called to signal that the asynch TRR resolve is
  // completed. For failed name resolves ("no such host"), the 'error' it
  // passses on in its argument must be NS_ERROR_UNKNOWN_HOST. Other errors
  // (if host was blacklisted, there as a bad content-type received, etc)
  // other error codes must be used. This distinction is important for the
  // subsequent logic to separate the error reasons.
  nsresult FailData(nsresult error);
  static nsresult DohDecodeQuery(const nsCString& query, nsCString& host,
                                 enum TrrType& type);
  nsresult ReceivePush(nsIHttpChannel* pushed, nsHostRecord* pushedRec);
  nsresult On200Response(nsIChannel* aChannel);
  nsresult FollowCname(nsIChannel* aChannel);

  bool UseDefaultServer();
  void SaveAdditionalRecords(
      const nsClassHashtable<nsCStringHashKey, DOHresp>& aRecords);

  friend class TRRServiceChannel;
  static nsresult SetupTRRServiceChannelInternal(
      nsIHttpChannel* aChannel, bool aUseGet, const nsACString& aContentType);

  void StoreIPHintAsDNSRecord(const struct SVCB& aSVCBRecord);

  nsCOMPtr<nsIChannel> mChannel;
  enum TrrType mType { TRRTYPE_A };
  UniquePtr<DNSPacket> mPacket;
  bool mFailed = false;
  bool mPB = false;
  DOHresp mDNS;
  nsresult mChannelStatus = NS_OK;

  RequestPurpose mPurpose = Resolve;
  Atomic<bool, Relaxed> mCancelled{false};

  // The request timeout in milliseconds. If 0 we will use the default timeout
  // we get from the prefs.
  uint32_t mTimeoutMs = 0;
  nsCOMPtr<nsITimer> mTimeout;
  nsCString mCname;
  uint32_t mCnameLoop = kCnameChaseMax;  // loop detection counter

  uint32_t mTTL = UINT32_MAX;
  TypeRecordResultType mResult = mozilla::AsVariant(Nothing());

  TRRSkippedReason mTRRSkippedReason = TRRSkippedReason::TRR_UNSET;
  void RecordReason(TRRSkippedReason reason) {
    if (mTRRSkippedReason == TRRSkippedReason::TRR_UNSET) {
      mTRRSkippedReason = reason;
    }
  }

  // keep a copy of the originSuffix for the cases where mRec == nullptr */
  const nsCString mOriginSuffix;

  // If true, we set LOAD_FRESH_CONNECTION on our channel's load flags.
  bool mUseFreshConnection = false;
};

}  // namespace net
}  // namespace mozilla

#endif  // include guard
