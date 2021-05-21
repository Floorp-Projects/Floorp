/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SpeculativeTransaction_h__
#define SpeculativeTransaction_h__

#include "mozilla/Maybe.h"
#include "NullHttpTransaction.h"

namespace mozilla {
namespace net {

class HTTPSRecordResolver;

class SpeculativeTransaction : public NullHttpTransaction {
 public:
  SpeculativeTransaction(nsHttpConnectionInfo* aConnInfo,
                         nsIInterfaceRequestor* aCallbacks, uint32_t aCaps,
                         std::function<void(bool)>&& aCallback = nullptr);

  already_AddRefed<SpeculativeTransaction> CreateWithNewConnInfo(
      nsHttpConnectionInfo* aConnInfo);

  virtual nsresult FetchHTTPSRR() override;

  virtual nsresult OnHTTPSRRAvailable(
      nsIDNSHTTPSSVCRecord* aHTTPSSVCRecord,
      nsISVCBRecord* aHighestPriorityRecord) override;

  void SetParallelSpeculativeConnectLimit(uint32_t aLimit) {
    mParallelSpeculativeConnectLimit.emplace(aLimit);
  }
  void SetIgnoreIdle(bool aIgnoreIdle) { mIgnoreIdle.emplace(aIgnoreIdle); }
  void SetIsFromPredictor(bool aIsFromPredictor) {
    mIsFromPredictor.emplace(aIsFromPredictor);
  }
  void SetAllow1918(bool aAllow1918) { mAllow1918.emplace(aAllow1918); }

  const Maybe<uint32_t>& ParallelSpeculativeConnectLimit() {
    return mParallelSpeculativeConnectLimit;
  }
  const Maybe<bool>& IgnoreIdle() { return mIgnoreIdle; }
  const Maybe<bool>& IsFromPredictor() { return mIsFromPredictor; }
  const Maybe<bool>& Allow1918() { return mAllow1918; }

  void Close(nsresult aReason) override;
  nsresult ReadSegments(nsAHttpSegmentReader* aReader, uint32_t aCount,
                        uint32_t* aCountRead) override;
  void InvokeCallback();

 protected:
  virtual ~SpeculativeTransaction();

 private:
  Maybe<uint32_t> mParallelSpeculativeConnectLimit;
  Maybe<bool> mIgnoreIdle;
  Maybe<bool> mIsFromPredictor;
  Maybe<bool> mAllow1918;

  bool mTriedToWrite = false;
  std::function<void(bool)> mCloseCallback;
};

}  // namespace net
}  // namespace mozilla

#endif  // SpeculativeTransaction_h__
