/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUDPServerSocket_h__
#define nsUDPServerSocket_h__

#include "nsIUDPServerSocket.h"
#include "nsSocketTransportService2.h"
#include "mozilla/Mutex.h"
#include "nsIOutputStream.h"

//-----------------------------------------------------------------------------

class nsUDPServerSocket : public nsASocketHandler
                        , public nsIUDPServerSocket
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSERVERSOCKET

  // nsASocketHandler methods:
  virtual void OnSocketReady(PRFileDesc* fd, int16_t outFlags);
  virtual void OnSocketDetached(PRFileDesc* fd);
  virtual void IsLocal(bool* aIsLocal);

  uint64_t ByteCountSent() { return mByteWriteCount; }
  uint64_t ByteCountReceived() { return mByteReadCount; }

  void AddOutputBytes(uint64_t aBytes);

  nsUDPServerSocket();

  // This must be public to support older compilers (xlC_r on AIX)
  virtual ~nsUDPServerSocket();

private:
  void OnMsgClose();
  void OnMsgAttach();

  // try attaching our socket (mFD) to the STS's poll list.
  nsresult TryAttach();

  // lock protects access to mListener;
  // so mListener is not cleared while being used/locked.
  mozilla::Mutex                       mLock;
  PRFileDesc                           *mFD;
  mozilla::net::NetAddr                mAddr;
  nsCOMPtr<nsIUDPServerSocketListener> mListener;
  nsCOMPtr<nsIEventTarget>             mListenerTarget;
  bool                                 mAttached;
  nsRefPtr<nsSocketTransportService>   mSts;

  uint64_t   mByteReadCount;
  uint64_t   mByteWriteCount;
};

//-----------------------------------------------------------------------------

class nsUDPMessage : public nsIUDPMessage
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPMESSAGE

  nsUDPMessage(PRNetAddr* aAddr,
               nsIOutputStream* aOutputStream,
               const nsACString& aData);

private:
  virtual ~nsUDPMessage();

  PRNetAddr mAddr;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCString mData;
};


//-----------------------------------------------------------------------------

class nsUDPOutputStream : public nsIOutputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  nsUDPOutputStream(nsUDPServerSocket* aServer,
                    PRFileDesc* aFD,
                    PRNetAddr& aPrClientAddr);
  virtual ~nsUDPOutputStream();

private:
  nsRefPtr<nsUDPServerSocket> mServer;
  PRFileDesc                  *mFD;
  PRNetAddr                   mPrClientAddr;
  bool                        mIsClosed;
};

#endif // nsUDPServerSocket_h__
