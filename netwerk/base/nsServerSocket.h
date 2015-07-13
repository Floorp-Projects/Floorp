/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsServerSocket_h__
#define nsServerSocket_h__

#include "prio.h"
#include "nsASocketHandler.h"
#include "nsIServerSocket.h"
#include "mozilla/Mutex.h"

//-----------------------------------------------------------------------------

class nsIEventTarget;
namespace mozilla { namespace net {
union NetAddr;
} // namespace net
} // namespace mozilla

class nsServerSocket : public nsASocketHandler
                     , public nsIServerSocket
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISERVERSOCKET

  // nsASocketHandler methods:
  virtual void OnSocketReady(PRFileDesc *fd, int16_t outFlags) override;
  virtual void OnSocketDetached(PRFileDesc *fd) override;
  virtual void IsLocal(bool *aIsLocal) override;
  virtual void KeepWhenOffline(bool *aKeepWhenOffline) override;

  virtual uint64_t ByteCountSent() override { return 0; }
  virtual uint64_t ByteCountReceived() override { return 0; }
  nsServerSocket();

  virtual void CreateClientTransport(PRFileDesc* clientFD,
                                     const mozilla::net::NetAddr& clientAddr);
  virtual nsresult SetSocketDefaults() { return NS_OK; }
  virtual nsresult OnSocketListen() { return NS_OK; }

protected:
  virtual ~nsServerSocket();
  PRFileDesc*                       mFD;
  nsCOMPtr<nsIServerSocketListener> mListener;

private:
  void OnMsgClose();
  void OnMsgAttach();

  // try attaching our socket (mFD) to the STS's poll list.
  nsresult TryAttach();

  // lock protects access to mListener; so it is not cleared while being used.
  mozilla::Mutex                    mLock;
  PRNetAddr                         mAddr;
  nsCOMPtr<nsIEventTarget>          mListenerTarget;
  bool                              mAttached;
  bool                              mKeepWhenOffline;
};

//-----------------------------------------------------------------------------

#endif // nsServerSocket_h__
