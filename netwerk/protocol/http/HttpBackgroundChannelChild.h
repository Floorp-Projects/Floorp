/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpBackgroundChannelChild_h
#define mozilla_net_HttpBackgroundChannelChild_h

#include "mozilla/net/PHttpBackgroundChannelChild.h"

using mozilla::ipc::IPCResult;

namespace mozilla {
namespace net {

class HttpBackgroundChannelChild final : public PHttpBackgroundChannelChild
{
public:
  explicit HttpBackgroundChannelChild();

  virtual ~HttpBackgroundChannelChild();

protected:
  IPCResult RecvOnTransportAndData(const nsresult& aChannelStatus,
                                   const nsresult& aTransportStatus,
                                   const uint64_t& aOffset,
                                   const uint32_t& aCount,
                                   const nsCString& aData) override;

  IPCResult RecvOnStopRequest(const nsresult& aChannelStatus,
                              const ResourceTimingStruct& aTiming) override;

  IPCResult RecvOnProgress(const int64_t& aProgress,
                           const int64_t& aProgressMax) override;

  IPCResult RecvOnStatus(const nsresult& aStatus) override;

  IPCResult RecvFlushedForDiversion() override;

  IPCResult RecvDivertMessages() override;

  IPCResult RecvOnStartRequestSent() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpBackgroundChannelChild_h
