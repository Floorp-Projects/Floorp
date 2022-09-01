/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketChannelChild_h
#define mozilla_net_WebSocketChannelChild_h

#include "mozilla/net/NeckoTargetHolder.h"
#include "mozilla/net/PWebSocketChild.h"
#include "mozilla/net/BaseWebSocketChannel.h"
#include "nsString.h"

namespace mozilla {

namespace net {

class ChannelEvent;
class ChannelEventQueue;
class MessageEvent;

class WebSocketChannelChild final : public BaseWebSocketChannel,
                                    public PWebSocketChild,
                                    public NeckoTargetHolder {
  friend class PWebSocketChild;

 public:
  explicit WebSocketChannelChild(bool aEncrypted);

  NS_DECL_THREADSAFE_ISUPPORTS

  // nsIWebSocketChannel methods BaseWebSocketChannel didn't implement for us
  //
  NS_IMETHOD AsyncOpen(nsIURI* aURI, const nsACString& aOrigin,
                       JS::Handle<JS::Value> aOriginAttributes,
                       uint64_t aInnerWindowID, nsIWebSocketListener* aListener,
                       nsISupports* aContext, JSContext* aCx) override;
  NS_IMETHOD AsyncOpenNative(nsIURI* aURI, const nsACString& aOrigin,
                             const OriginAttributes& aOriginAttributes,
                             uint64_t aInnerWindowID,
                             nsIWebSocketListener* aListener,
                             nsISupports* aContext) override;
  NS_IMETHOD Close(uint16_t code, const nsACString& reason) override;
  NS_IMETHOD SendMsg(const nsACString& aMsg) override;
  NS_IMETHOD SendBinaryMsg(const nsACString& aMsg) override;
  NS_IMETHOD SendBinaryStream(nsIInputStream* aStream,
                              uint32_t aLength) override;
  NS_IMETHOD GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) override;

  void AddIPDLReference();
  void ReleaseIPDLReference();

  // Off main thread URI access.
  void GetEffectiveURL(nsAString& aEffectiveURL) const override;
  bool IsEncrypted() const override;

 private:
  ~WebSocketChannelChild();

  mozilla::ipc::IPCResult RecvOnStart(const nsACString& aProtocol,
                                      const nsACString& aExtensions,
                                      const nsAString& aEffectiveURL,
                                      const bool& aEncrypted,
                                      const uint64_t& aHttpChannelId);
  mozilla::ipc::IPCResult RecvOnStop(const nsresult& aStatusCode);
  mozilla::ipc::IPCResult RecvOnMessageAvailable(const nsACString& aMsg,
                                                 const bool& aMoreData);
  mozilla::ipc::IPCResult RecvOnBinaryMessageAvailable(const nsACString& aMsg,
                                                       const bool& aMoreData);
  mozilla::ipc::IPCResult RecvOnAcknowledge(const uint32_t& aSize);
  mozilla::ipc::IPCResult RecvOnServerClose(const uint16_t& aCode,
                                            const nsACString& aReason);

  void OnStart(const nsACString& aProtocol, const nsACString& aExtensions,
               const nsAString& aEffectiveURL, const bool& aEncrypted,
               const uint64_t& aHttpChannelId);
  void OnStop(const nsresult& aStatusCode);
  void OnMessageAvailable(const nsACString& aMsg);
  void OnBinaryMessageAvailable(const nsACString& aMsg);
  void OnAcknowledge(const uint32_t& aSize);
  void OnServerClose(const uint16_t& aCode, const nsACString& aReason);
  void AsyncOpenFailed();

  void MaybeReleaseIPCObject();

  // This function tries to get a labeled event target for |mNeckoTarget|.
  void SetupNeckoTarget();

  bool RecvOnMessageAvailableInternal(const nsACString& aMsg, bool aMoreData,
                                      bool aBinary);

  void OnError();

  RefPtr<ChannelEventQueue> mEventQ;
  nsString mEffectiveURL;
  nsCString mReceivedMsgBuffer;

  // This variable is protected by mutex.
  enum { Opened, Closing, Closed } mIPCState;

  mozilla::Mutex mMutex MOZ_UNANNOTATED;

  friend class StartEvent;
  friend class StopEvent;
  friend class MessageEvent;
  friend class AcknowledgeEvent;
  friend class ServerCloseEvent;
  friend class AsyncOpenFailedEvent;
  friend class OnErrorEvent;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketChannelChild_h
