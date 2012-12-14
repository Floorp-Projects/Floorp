/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_

#ifdef MOZ_WEBRTC_SIGNALING
#define SCTP_DTLS_SUPPORTED 1
#endif

#include <string>
#include <errno.h>
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "nsIInputStream.h"
#include "nsITimer.h"
#include "mozilla/Mutex.h"
#include "DataChannelProtocol.h"
#ifdef SCTP_DTLS_SUPPORTED
#include "mtransport/sigslot.h"
#include "mtransport/transportflow.h"
#include "mtransport/transportlayer.h"
#include "mtransport/transportlayerprsock.h"
#endif

#ifndef EALREADY
#define EALREADY  WSAEALREADY
#endif

extern "C" {
  struct socket;
  struct sctp_rcvinfo;
}

namespace mozilla {

class DTLSConnection;
class DataChannelConnection;
class DataChannel;
class DataChannelOnMessageAvailable;

class BufferedMsg
{
public:
  BufferedMsg(struct sctp_sendv_spa &spa,const char *data,
              uint32_t length);
  ~BufferedMsg();

  struct sctp_sendv_spa *mSpa;
  const char *mData;
  uint32_t mLength;
};

// Implemented by consumers of a Channel to receive messages.
// Can't nest it in DataChannelConnection because C++ doesn't allow forward
// refs to embedded classes
class DataChannelListener {
public:
  virtual ~DataChannelListener() {}

  // Called when a DOMString message is received.
  virtual nsresult OnMessageAvailable(nsISupports *aContext,
                                      const nsACString& message) = 0;

  // Called when a binary message is received.
  virtual nsresult OnBinaryMessageAvailable(nsISupports *aContext,
                                            const nsACString& message) = 0;

  // Called when the channel is connected
  virtual nsresult OnChannelConnected(nsISupports *aContext) = 0;

  // Called when the channel is closed
  virtual nsresult OnChannelClosed(nsISupports *aContext) = 0;
};


// One per PeerConnection
class DataChannelConnection: public nsITimerCallback
#ifdef SCTP_DTLS_SUPPORTED
                             , public sigslot::has_slots<>
#endif
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  class DataConnectionListener {
  public:
    virtual ~DataConnectionListener() {}

    // Called when a the connection is open
    virtual void NotifyConnection() = 0;

    // Called when a the connection is lost/closed
    virtual void NotifyClosedConnection() = 0;

    // Called when a new DataChannel has been opened by the other side.
    virtual void NotifyDataChannel(DataChannel *channel) = 0;
  };

  DataChannelConnection(DataConnectionListener *listener);
  virtual ~DataChannelConnection();

  bool Init(unsigned short aPort, uint16_t aNumStreams, bool aUsingDtls);
  void Destroy(); // So we can spawn refs tied to runnables in shutdown

  // These block; they require something to decide on listener/connector
  // (though you can do simultaneous Connect()).  Do not call these from
  // the main thread!
  bool Listen(unsigned short port);
  bool Connect(const char *addr, unsigned short port);

#ifdef SCTP_DTLS_SUPPORTED
  // Connect using a TransportFlow (DTLS) channel
  bool ConnectDTLS(TransportFlow *aFlow, uint16_t localport, uint16_t remoteport);
#endif

  typedef enum {
    RELIABLE=0,
    PARTIAL_RELIABLE_REXMIT = 1,
    PARTIAL_RELIABLE_TIMED = 2
  } Type;
    
  already_AddRefed<DataChannel> Open(const nsACString& label,
                                     Type type, bool inOrder,
                                     uint32_t prValue,
                                     DataChannelListener *aListener,
                                     nsISupports *aContext);

  void Close(uint16_t stream);
  void CloseAll();

  int32_t SendMsg(uint16_t stream, const nsACString &aMsg)
    {
      return SendMsgCommon(stream, aMsg, false);
    }
  int32_t SendBinaryMsg(uint16_t stream, const nsACString &aMsg)
    {
      return SendMsgCommon(stream, aMsg, true);
    }
  int32_t SendBlob(uint16_t stream, nsIInputStream *aBlob);

  // Called on data reception from the SCTP library
  // must(?) be public so my c->c++ tramploine can call it
  int ReceiveCallback(struct socket* sock, void *data, size_t datalen, 
                      struct sctp_rcvinfo rcv, int32_t flags);

  // Find out state
  enum {
    CONNECTING = 0U,
    OPEN = 1U,
    CLOSING = 2U,
    CLOSED = 3U
  };
  uint16_t GetReadyState() { return mState; }

  friend class DataChannel;
  Mutex  mLock;

protected:
  friend class DataChannelOnMessageAvailable;
  DataConnectionListener *mListener;

private:
  friend class DataChannelConnectRunnable;

#ifdef SCTP_DTLS_SUPPORTED
  static void DTLSConnectThread(void *data);
  int SendPacket(const unsigned char* data, size_t len, bool release);
  void SctpDtlsInput(TransportFlow *flow, const unsigned char *data, size_t len);
  static int SctpDtlsOutput(void *addr, void *buffer, size_t length, uint8_t tos, uint8_t set_df);
#endif
  DataChannel* FindChannelByStreamIn(uint16_t streamIn);
  DataChannel* FindChannelByStreamOut(uint16_t streamOut);
  uint16_t FindFreeStreamOut();
  bool RequestMoreStreamsOut(int32_t aNeeded = 16);
  int32_t SendControlMessage(void *msg, uint32_t len, uint16_t streamOut);
  int32_t SendOpenRequestMessage(const nsACString& label,uint16_t streamOut,
                                 bool unordered, uint16_t prPolicy, uint32_t prValue);
  int32_t SendOpenResponseMessage(uint16_t streamOut, uint16_t streamIn);
  int32_t SendOpenAckMessage(uint16_t streamOut);
  int32_t SendMsgInternal(DataChannel *channel, const char *data,
                          uint32_t length, uint32_t ppid);
  int32_t SendBinary(DataChannel *channel, const char *data,
                     uint32_t len);
  int32_t SendMsgCommon(uint16_t stream, const nsACString &aMsg, bool isBinary);

  already_AddRefed<DataChannel> OpenFinish(already_AddRefed<DataChannel> channel);

  void StartDefer();
  bool SendDeferredMessages();
  void SendOutgoingStreamReset();
  void ResetOutgoingStream(uint16_t streamOut);
  void HandleOpenRequestMessage(const struct rtcweb_datachannel_open_request *req,
                                size_t length,
                                uint16_t streamIn);
  void OpenResponseFinish(already_AddRefed<DataChannel> channel);
  void HandleOpenResponseMessage(const struct rtcweb_datachannel_open_response *rsp,
                                 size_t length, uint16_t streamIn);
  void HandleOpenAckMessage(const struct rtcweb_datachannel_ack *ack,
                            size_t length, uint16_t streamIn);
  void HandleUnknownMessage(uint32_t ppid, size_t length, uint16_t streamIn);
  void HandleDataMessage(uint32_t ppid, const void *buffer, size_t length, uint16_t streamIn);
  void HandleMessage(const void *buffer, size_t length, uint32_t ppid, uint16_t streamIn);
  void HandleAssociationChangeEvent(const struct sctp_assoc_change *sac);
  void HandlePeerAddressChangeEvent(const struct sctp_paddr_change *spc);
  void HandleRemoteErrorEvent(const struct sctp_remote_error *sre);
  void HandleShutdownEvent(const struct sctp_shutdown_event *sse);
  void HandleAdaptationIndication(const struct sctp_adaptation_event *sai);
  void HandleSendFailedEvent(const struct sctp_send_failed_event *ssfe);
  void HandleStreamResetEvent(const struct sctp_stream_reset_event *strrst);
  void HandleStreamChangeEvent(const struct sctp_stream_change_event *strchg);
  void HandleNotification(const union sctp_notification *notif, size_t n);

#ifdef SCTP_DTLS_SUPPORTED
  bool IsSTSThread() {
    bool on = false;
    if (mSTS) {
      mSTS->IsOnCurrentThread(&on);
    }
    return on;
  }
#endif

  // NOTE: while these arrays will auto-expand, increases in the number of
  // channels available from the stack must be negotiated!
  nsAutoTArray<nsRefPtr<DataChannel>,16> mStreamsOut;
  nsAutoTArray<nsRefPtr<DataChannel>,16> mStreamsIn;
  nsDeque mPending; // Holds already_AddRefed<DataChannel>s -- careful!

  // Streams pending reset
  nsAutoTArray<uint16_t,4> mStreamsResetting;

  struct socket *mMasterSocket; // accessed from connect thread
  struct socket *mSocket; // cloned from mMasterSocket on successful Connect on connect thread
  uint16_t mState; // modified on connect thread (to OPEN)

#ifdef SCTP_DTLS_SUPPORTED
  nsRefPtr<TransportFlow> mTransportFlow;
  nsCOMPtr<nsIEventTarget> mSTS;
#endif
  uint16_t mLocalPort; // Accessed from connect thread
  uint16_t mRemotePort;

  // Timer to control when we try to resend blocked messages
  nsCOMPtr<nsITimer> mDeferredTimer;
  uint32_t mDeferTimeout; // in ms
  bool mTimerRunning;

  // Thread used for connections
  nsCOMPtr<nsIThread> mConnectThread;
};

class DataChannel {
public:
  enum {
    CONNECTING = 0U,
    OPEN = 1U,
    CLOSING = 2U,
    CLOSED = 3U,
    WAITING_TO_OPEN = 4U
  };

  DataChannel(DataChannelConnection *connection,
              uint16_t streamOut, uint16_t streamIn, 
              uint16_t state,
              const nsACString& label,
              uint16_t policy, uint32_t value,
              uint32_t flags,
              DataChannelListener *aListener,
              nsISupports *aContext)
    : mListener(aListener)
    , mConnection(connection)
    , mLabel(label)
    , mState(state)
    , mReady(false)
    , mStreamOut(streamOut)
    , mStreamIn(streamIn)
    , mPrPolicy(policy)
    , mPrValue(value)
    , mFlags(0)
    , mContext(aContext)
    {
      NS_ASSERTION(mConnection,"NULL connection");
    }

  ~DataChannel();
  void Destroy(); // when we disconnect from the connection after stream RESET

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannel)

  // Close this DataChannel.  Can be called multiple times.
  void Close();

  // Set the listener (especially for channels created from the other side)
  // Note: The Listener and Context should only be set once
  void SetListener(DataChannelListener *aListener, nsISupports *aContext);

  // Send a string
  bool SendMsg(const nsACString &aMsg)
    {
      if (mStreamOut != INVALID_STREAM)
        return (mConnection->SendMsg(mStreamOut, aMsg) > 0);
      else
        return false;
    }

  // Send a binary message (TypedArray)
  bool SendBinaryMsg(const nsACString &aMsg)
    {
      if (mStreamOut != INVALID_STREAM)
        return (mConnection->SendBinaryMsg(mStreamOut, aMsg) > 0);
      else
        return false;
    }

  // Send a binary blob
  bool SendBinaryStream(nsIInputStream *aBlob, uint32_t msgLen)
    {
      if (mStreamOut != INVALID_STREAM)
        return (mConnection->SendBlob(mStreamOut, aBlob) > 0);
      else
        return false;
    }

  uint16_t GetType() { return mPrPolicy; }

  bool GetOrdered() { return !(mFlags & DATA_CHANNEL_FLAG_OUT_OF_ORDER_ALLOWED); }

  // Amount of data buffered to send
  uint32_t GetBufferedAmount();

  // Find out state
  uint16_t GetReadyState()
    {
      if (mState == WAITING_TO_OPEN)
        return CONNECTING;
      return mState;
    }

  void SetReadyState(uint16_t aState) { mState = aState; }

  void GetLabel(nsAString& aLabel) { CopyUTF8toUTF16(mLabel, aLabel); }

  void AppReady();

  void SendOrQueue(DataChannelOnMessageAvailable *aMessage);

protected:
  DataChannelListener *mListener;

private:
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnection;

  nsresult AddDataToBinaryMsg(const char *data, uint32_t size);

  nsRefPtr<DataChannelConnection> mConnection;
  nsCString mLabel;
  uint16_t mState;
  bool     mReady;
  uint16_t mStreamOut;
  uint16_t mStreamIn;
  uint16_t mPrPolicy;
  uint32_t mPrValue;
  uint32_t mFlags;
  uint32_t mId;
  nsCOMPtr<nsISupports> mContext;
  nsCString mBinaryBuffer;
  nsTArray<nsAutoPtr<BufferedMsg> > mBufferedData;
  nsTArray<nsCOMPtr<nsIRunnable> > mQueuedMessages;
};

// used to dispatch notifications of incoming data to the main thread
// Patterned on CallOnMessageAvailable in WebSockets
// Also used to proxy other items to MainThread
class DataChannelOnMessageAvailable : public nsRunnable
{
public:
  enum {
    ON_CONNECTION,
    ON_DISCONNECTED,
    ON_CHANNEL_CREATED,
    ON_CHANNEL_OPEN,
    ON_CHANNEL_CLOSED,
    ON_DATA,
    START_DEFER,
  };  /* types */

  DataChannelOnMessageAvailable(int32_t     aType,
                                DataChannelConnection *aConnection,
                                DataChannel *aChannel,
                                nsCString   &aData,  // XXX this causes inefficiency
                                int32_t     aLen)
    : mType(aType),
      mChannel(aChannel),
      mConnection(aConnection), 
      mData(aData),
      mLen(aLen) {}

  DataChannelOnMessageAvailable(int32_t     aType,
                                DataChannel *aChannel)
    : mType(aType),
      mChannel(aChannel) {}
  // XXX is it safe to leave mData/mLen uninitialized?  This should only be
  // used for notifications that don't use them, but I'd like more
  // bulletproof compile-time checking.

  DataChannelOnMessageAvailable(int32_t     aType,
                                DataChannelConnection *aConnection,
                                DataChannel *aChannel)
    : mType(aType),
      mChannel(aChannel),
      mConnection(aConnection) {}

  // for ON_CONNECTION
  DataChannelOnMessageAvailable(int32_t     aType,
                                DataChannelConnection *aConnection,
                                bool aResult)
    : mType(aType),
      mConnection(aConnection),
      mResult(aResult) {}

  NS_IMETHOD Run()
  {
    switch (mType) {
      case ON_DATA:
      case ON_CHANNEL_OPEN:
      case ON_CHANNEL_CLOSED:
        if (!mChannel->mListener)
          return NS_OK;
        break;
      case ON_CHANNEL_CREATED:
      case ON_CONNECTION:
      case ON_DISCONNECTED:
        if (!mConnection->mListener)
          return NS_OK;
        break;
      case START_DEFER:
        break;
    }
    switch (mType) {
      case ON_DATA:
        if (mLen < 0) {
          mChannel->mListener->OnMessageAvailable(mChannel->mContext, mData);
        } else {
          mChannel->mListener->OnBinaryMessageAvailable(mChannel->mContext, mData);
        }
        break;
      case ON_CHANNEL_OPEN:
        mChannel->mListener->OnChannelConnected(mChannel->mContext);
        break;
      case ON_CHANNEL_CLOSED:
        mChannel->mListener->OnChannelClosed(mChannel->mContext);
        break;
      case ON_CHANNEL_CREATED:
        mConnection->mListener->NotifyDataChannel(mChannel);
        break;
      case ON_CONNECTION:
        if (mResult) {
          mConnection->mListener->NotifyConnection();
        }
        mConnection->mConnectThread = nullptr; // kill the connection thread
        break;
      case ON_DISCONNECTED:
        mConnection->mListener->NotifyClosedConnection();
        break;
      case START_DEFER:
        mConnection->StartDefer();
        break;
    }
    return NS_OK;
  }

private:
  ~DataChannelOnMessageAvailable() {}

  int32_t                           mType;
  // XXX should use union
  nsRefPtr<DataChannel>             mChannel;
  nsRefPtr<DataChannelConnection>   mConnection;
  nsCString                         mData;
  int32_t                           mLen;
  bool                              mResult;
};

}

#endif  // NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
