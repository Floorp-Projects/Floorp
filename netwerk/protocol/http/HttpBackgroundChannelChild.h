/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpBackgroundChannelChild_h
#define mozilla_net_HttpBackgroundChannelChild_h

#include "mozilla/net/PHttpBackgroundChannelChild.h"
#include "nsIRunnable.h"
#include "nsTArray.h"

using mozilla::ipc::IPCResult;
using mozilla::dom::ClassifierInfo;

namespace mozilla {
namespace net {

class HttpChannelChild;

class HttpBackgroundChannelChild final : public PHttpBackgroundChannelChild
{
  friend class BackgroundChannelCreateCallback;
public:
  explicit HttpBackgroundChannelChild();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HttpBackgroundChannelChild)

  // Associate this background channel with a HttpChannelChild and
  // initiate the createion of the PBackground IPC channel.
  nsresult Init(HttpChannelChild* aChannelChild);

  // Callback while the associated HttpChannelChild is not going to
  // handle any incoming messages over background channel.
  void OnChannelClosed();

  // Callback when OnStartRequest is received and handled by HttpChannelChild.
  // Enqueued messages in background channel will be flushed.
  void OnStartRequestReceived();

protected:
  IPCResult RecvOnTransportAndData(const nsresult& aChannelStatus,
                                   const nsresult& aTransportStatus,
                                   const uint64_t& aOffset,
                                   const uint32_t& aCount,
                                   const nsCString& aData) override;

  IPCResult RecvOnStopRequest(const nsresult& aChannelStatus,
                              const ResourceTimingStruct& aTiming,
                              const TimeStamp& aLastActiveTabOptHit) override;

  IPCResult RecvOnProgress(const int64_t& aProgress,
                           const int64_t& aProgressMax) override;

  IPCResult RecvOnStatus(const nsresult& aStatus) override;

  IPCResult RecvFlushedForDiversion() override;

  IPCResult RecvDivertMessages() override;

  IPCResult RecvOnStartRequestSent() override;

  IPCResult RecvNotifyTrackingProtectionDisabled() override;

  IPCResult RecvNotifyTrackingResource() override;

  IPCResult RecvSetClassifierMatchedInfo(const ClassifierInfo& info) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  virtual ~HttpBackgroundChannelChild();

  // Initiate the creation of the PBckground IPC channel.
  // Return false if failed.
  bool CreateBackgroundChannel();

  // Check OnStartRequest is sent by parent process over main thread IPC
  // but not yet received on child process.
  // return true before RecvOnStartRequestSent is invoked.
  // return false if RecvOnStartRequestSent is invoked but not
  // OnStartRequestReceived.
  // return true after both RecvOnStartRequestSend and OnStartRequestReceived
  // are invoked.
  bool IsWaitingOnStartRequest();

  // Associated HttpChannelChild for handling the channel events.
  // Will be removed while failed to create background channel,
  // destruction of the background channel, or explicitly dissociation
  // via OnChannelClosed callback.
  RefPtr<HttpChannelChild> mChannelChild;

  // True if OnStartRequest is received by HttpChannelChild.
  // Should only access on STS thread.
  bool mStartReceived = false;

  // True if OnStartRequest is sent by HttpChannelParent.
  // Should only access on STS thread.
  bool mStartSent = false;

  // Store pending messages that require to be handled after OnStartRequest.
  // Should be flushed after OnStartRequest is received and handled.
  // Should only access on STS thread.
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedRunnables;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpBackgroundChannelChild_h
