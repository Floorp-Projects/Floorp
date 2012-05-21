/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsServerSocket_h__
#define nsServerSocket_h__

#include "nsIServerSocket.h"
#include "nsSocketTransportService2.h"
#include "mozilla/Mutex.h"

//-----------------------------------------------------------------------------

class nsServerSocket : public nsASocketHandler
                     , public nsIServerSocket
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKET

  // nsASocketHandler methods:
  virtual void OnSocketReady(PRFileDesc *fd, PRInt16 outFlags);
  virtual void OnSocketDetached(PRFileDesc *fd);

  nsServerSocket();

  // This must be public to support older compilers (xlC_r on AIX)
  virtual ~nsServerSocket();

private:
  void OnMsgClose();
  void OnMsgAttach();
  
  // try attaching our socket (mFD) to the STS's poll list.
  nsresult TryAttach();

  // lock protects access to mListener; so it is not cleared while being used.
  mozilla::Mutex                    mLock;
  PRFileDesc                       *mFD;
  PRNetAddr                         mAddr;
  nsCOMPtr<nsIServerSocketListener> mListener;
  nsCOMPtr<nsIEventTarget>          mListenerTarget;
  bool                              mAttached;
};

//-----------------------------------------------------------------------------

#endif // nsServerSocket_h__
