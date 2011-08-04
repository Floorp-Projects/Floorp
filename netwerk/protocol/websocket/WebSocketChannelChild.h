/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_net_WebSocketChannelChild_h
#define mozilla_net_WebSocketChannelChild_h

#include "mozilla/net/PWebSocketChild.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/BaseWebSocketChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class WebSocketChannelChild : public BaseWebSocketChannel,
                              public PWebSocketChild
{
 public:
  WebSocketChannelChild(bool aSecure);
  ~WebSocketChannelChild();

  NS_DECL_ISUPPORTS

  // nsIWebSocketChannel methods BaseWebSocketChannel didn't implement for us
  //
  NS_SCRIPTABLE NS_IMETHOD AsyncOpen(nsIURI *aURI,
                                     const nsACString &aOrigin,
                                     nsIWebSocketListener *aListener,
                                     nsISupports *aContext);
  NS_SCRIPTABLE NS_IMETHOD Close(PRUint16 code, const nsACString & reason);
  NS_SCRIPTABLE NS_IMETHOD SendMsg(const nsACString &aMsg);
  NS_SCRIPTABLE NS_IMETHOD SendBinaryMsg(const nsACString &aMsg);
  NS_SCRIPTABLE NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo);

  void AddIPDLReference();
  void ReleaseIPDLReference();

 private:
  bool RecvOnStart(const nsCString& aProtocol, const nsCString& aExtensions);
  bool RecvOnStop(const nsresult& aStatusCode);
  bool RecvOnMessageAvailable(const nsCString& aMsg);
  bool RecvOnBinaryMessageAvailable(const nsCString& aMsg);
  bool RecvOnAcknowledge(const PRUint32& aSize);
  bool RecvOnServerClose(const PRUint16& aCode, const nsCString &aReason);
  bool RecvAsyncOpenFailed();

  void OnStart(const nsCString& aProtocol, const nsCString& aExtensions);
  void OnStop(const nsresult& aStatusCode);
  void OnMessageAvailable(const nsCString& aMsg);
  void OnBinaryMessageAvailable(const nsCString& aMsg);
  void OnAcknowledge(const PRUint32& aSize);
  void OnServerClose(const PRUint16& aCode, const nsCString& aReason);
  void AsyncOpenFailed();  

  ChannelEventQueue mEventQ;
  bool mIPCOpen;

  friend class StartEvent;
  friend class StopEvent;
  friend class MessageEvent;
  friend class AcknowledgeEvent;
  friend class ServerCloseEvent;
  friend class AsyncOpenFailedEvent;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WebSocketChannelChild_h
