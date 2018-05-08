/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRR_h
#define mozilla_net_TRR_h

#include "nsIChannel.h"
#include "nsIHttpPushListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"

namespace mozilla { namespace net {

// the values map to RFC1035 type identifiers
enum TrrType {
  TRRTYPE_A = 1,
  TRRTYPE_NS = 2,
  TRRTYPE_CNAME = 5,
  TRRTYPE_AAAA = 28,
};

class DOHaddr : public LinkedListElement<DOHaddr> {
public:
  NetAddr mNet;
  uint32_t mTtl;
};

class TRRService;
extern TRRService *gTRRService;

class DOHresp {
public:
  ~DOHresp() {
    DOHaddr *el;
    while ((el = mAddresses.popLast())) {
      delete el;
    }
  }
  nsresult Add(uint32_t TTL, unsigned char *dns, int index, uint16_t len,
               bool aLocalAllowed);
  LinkedList<DOHaddr> mAddresses;
};

class TRR
  : public Runnable
  , public nsITimerCallback
  , public nsIHttpPushListener
  , public nsIInterfaceRequestor
  , public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIHTTPPUSHLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK

  // Never accept larger DOH responses than this as that would indicate
  // something is wrong. Typical ones are much smaller.
  static const unsigned int kMaxSize = 3200;

  // Number of "steps" we follow CNAME chains
  static const unsigned int kCnameChaseMax = 64;

  // when firing off a normal A or AAAA query
  explicit TRR(AHostResolver *aResolver,
               nsHostRecord *aRec,
               enum TrrType aType)
    : mozilla::Runnable("TRR")
    , mRec(aRec)
    , mHostResolver(aResolver)
    , mType(aType)
    , mBodySize(0)
    , mFailed(false)
    , mCnameLoop(kCnameChaseMax)
    , mAllowRFC1918(false)
  {
    mHost = aRec->host;
    mPB = aRec->pb;
  }

  // when following CNAMEs
  explicit TRR(AHostResolver *aResolver,
               nsHostRecord *aRec,
               nsCString &aHost,
               enum TrrType & aType,
               unsigned int aLoopCount,
               bool aPB)
    : mozilla::Runnable("TRR")
    , mHost(aHost)
    , mRec(aRec)
    , mHostResolver(aResolver)
    , mType(aType)
    , mBodySize(0)
    , mFailed(false)
    , mPB(aPB)
    , mCnameLoop(aLoopCount)
    , mAllowRFC1918(false)
  {

  }

  // used on push
  explicit TRR(AHostResolver *aResolver, bool aPB)
    : mozilla::Runnable("TRR")
    , mHostResolver(aResolver)
    , mBodySize(0)
    , mFailed(false)
    , mPB(aPB)
    , mCnameLoop(kCnameChaseMax)
    , mAllowRFC1918(false)
  { }

  // to verify a domain
  explicit TRR(AHostResolver *aResolver,
               nsACString &aHost,
               enum TrrType aType,
               bool aPB)
    : mozilla::Runnable("TRR")
    , mHost(aHost)
    , mHostResolver(aResolver)
    , mType(aType)
    , mBodySize(0)
    , mFailed(false)
    , mPB(aPB)
    , mCnameLoop(kCnameChaseMax)
    , mAllowRFC1918(false)
  { }

  NS_IMETHOD Run() override;
  void Cancel();
  enum TrrType Type() { return mType; }
  nsCString mHost;
  RefPtr<nsHostRecord> mRec;
  RefPtr<AHostResolver> mHostResolver;

private:
  ~TRR() = default;
  nsresult SendHTTPRequest();
  nsresult DohEncode(nsCString &target);
  nsresult PassQName(unsigned int &index);
  nsresult GetQname(nsAutoCString &aQname, unsigned int &aIndex);
  nsresult DohDecode(nsCString &aHost);
  nsresult ReturnData();
  nsresult FailData();
  nsresult DohDecodeQuery(const nsCString &query,
                          nsCString &host, enum TrrType &type);
  nsresult ReceivePush(nsIHttpChannel *pushed, nsHostRecord *pushedRec);
  nsresult On200Response();

  nsCOMPtr<nsIChannel> mChannel;
  enum TrrType mType;
  TimeStamp mStartTime;
  unsigned char mResponse[kMaxSize];
  unsigned int mBodySize;
  bool mFailed;
  bool mPB;
  DOHresp mDNS;
  nsCOMPtr<nsITimer> mTimeout;
  nsCString mCname;
  uint32_t mCnameLoop; // loop detection counter
  bool mAllowRFC1918;
};

} // namespace net
} // namespace mozilla

#endif // include guard
