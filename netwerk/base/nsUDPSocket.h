/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUDPSocket_h__
#define nsUDPSocket_h__

#include "nsIUDPSocket.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/DNS.h"
#include "nsIOutputStream.h"
#include "nsASocketHandler.h"
#include "nsCycleCollectionParticipant.h"

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

class nsSocketTransportService;

class nsUDPSocket final : public nsASocketHandler, public nsIUDPSocket {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKET

  // nsASocketHandler methods:
  virtual void OnSocketReady(PRFileDesc* fd, int16_t outFlags) override;
  virtual void OnSocketDetached(PRFileDesc* fd) override;
  virtual void IsLocal(bool* aIsLocal) override;
  virtual nsresult GetRemoteAddr(NetAddr* addr) override;

  uint64_t ByteCountSent() override { return mByteWriteCount; }
  uint64_t ByteCountReceived() override { return mByteReadCount; }

  void AddOutputBytes(int32_t aBytes);

  nsUDPSocket();

 private:
  virtual ~nsUDPSocket();

  void OnMsgClose();
  void OnMsgAttach();

  // try attaching our socket (mFD) to the STS's poll list.
  nsresult TryAttach();

  friend class SetSocketOptionRunnable;
  nsresult SetSocketOption(const PRSocketOptionData& aOpt);
  nsresult JoinMulticastInternal(const PRNetAddr& aAddr,
                                 const PRNetAddr& aIface);
  nsresult LeaveMulticastInternal(const PRNetAddr& aAddr,
                                  const PRNetAddr& aIface);
  nsresult SetMulticastInterfaceInternal(const PRNetAddr& aIface);

  void CloseSocket();

  // lock protects access to mListener;
  // so mListener is not cleared while being used/locked.
  Mutex mLock MOZ_UNANNOTATED{"nsUDPSocket.mLock"};
  PRFileDesc* mFD{nullptr};
  NetAddr mAddr;
  OriginAttributes mOriginAttributes;
  nsCOMPtr<nsIUDPSocketListener> mListener;
  nsCOMPtr<nsIUDPSocketSyncListener> mSyncListener;
  nsCOMPtr<nsIEventTarget> mListenerTarget;
  bool mAttached{false};
  RefPtr<nsSocketTransportService> mSts;

  uint64_t mByteReadCount{0};
  uint64_t mByteWriteCount{0};
};

//-----------------------------------------------------------------------------

class nsUDPMessage : public nsIUDPMessage {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsUDPMessage)
  NS_DECL_NSIUDPMESSAGE

  nsUDPMessage(NetAddr* aAddr, nsIOutputStream* aOutputStream,
               FallibleTArray<uint8_t>&& aData);

 private:
  virtual ~nsUDPMessage();

  NetAddr mAddr;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  FallibleTArray<uint8_t> mData;
  JS::Heap<JSObject*> mJsobj;
};

//-----------------------------------------------------------------------------

class nsUDPOutputStream : public nsIOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  nsUDPOutputStream(nsUDPSocket* aSocket, PRFileDesc* aFD,
                    PRNetAddr& aPrClientAddr);

 private:
  virtual ~nsUDPOutputStream() = default;

  RefPtr<nsUDPSocket> mSocket;
  PRFileDesc* mFD;
  PRNetAddr mPrClientAddr;
  bool mIsClosed;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsUDPSocket_h__
