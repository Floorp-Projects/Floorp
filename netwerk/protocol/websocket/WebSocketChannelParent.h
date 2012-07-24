/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketChannelParent_h
#define mozilla_net_WebSocketChannelParent_h

#include "mozilla/net/PWebSocketParent.h"
#include "nsIWebSocketListener.h"
#include "nsIWebSocketChannel.h"
#include "nsILoadContext.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIAuthPromptProvider;

namespace mozilla {
namespace net {

class WebSocketChannelParent : public PWebSocketParent,
                               public nsIWebSocketListener,
                               public nsIInterfaceRequestor,
                               public nsILoadContext
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSOCKETLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSILOADCONTEXT

  WebSocketChannelParent(nsIAuthPromptProvider* aAuthProvider);

 private:
  bool RecvAsyncOpen(const IPC::URI& aURI,
                     const nsCString& aOrigin,
                     const nsCString& aProtocol,
                     const bool& aSecure,
                     const bool& haveLoadContext,
                     const bool& isContent,
                     const bool& usingPrivateBrowsing,
                     const bool& isInBrowserElement,
                     const PRUint32& appId,
                     const nsCString& extendedOrigin);
  bool RecvClose(const PRUint16 & code, const nsCString & reason);
  bool RecvSendMsg(const nsCString& aMsg);
  bool RecvSendBinaryMsg(const nsCString& aMsg);
  bool RecvSendBinaryStream(const InputStream& aStream,
                            const PRUint32& aLength);
  bool RecvDeleteSelf();

  void ActorDestroy(ActorDestroyReason why);

  nsCOMPtr<nsIAuthPromptProvider> mAuthProvider;
  nsCOMPtr<nsIWebSocketChannel> mChannel;
  bool mIPCOpen;

  // fields for impersonating nsILoadContext
  bool mHaveLoadContext             : 1;
  bool mIsContent                   : 1;
  bool mUsePrivateBrowsing          : 1;
  bool mIsInBrowserElement          : 1;

  PRUint32 mAppId;
  nsCString mExtendedOrigin;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WebSocketChannelParent_h
