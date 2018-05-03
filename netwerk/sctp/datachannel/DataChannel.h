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
#include "mozilla/WeakPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "nsIInputStream.h"
#include "mozilla/Mutex.h"
#include "DataChannelProtocol.h"
#include "DataChannelListener.h"
#include "mozilla/net/NeckoTargetHolder.h"
#ifdef SCTP_DTLS_SUPPORTED
#include "mtransport/sigslot.h"
#include "mtransport/transportflow.h"
#include "mtransport/transportlayer.h"
#include "mtransport/transportlayerdtls.h"
#endif

#ifndef DATACHANNEL_LOG
#define DATACHANNEL_LOG(args)
#endif

#ifndef EALREADY
#define EALREADY  WSAEALREADY
#endif

extern "C" {
  struct socket;
  struct sctp_rcvinfo;
}

namespace mozilla {

class DataChannelConnection;
class DataChannel;
class DataChannelOnMessageAvailable;

// For sending outgoing messages.
// This class only holds a reference to the data and the info structure but does
// not copy it.
class OutgoingMsg
{
public:
  OutgoingMsg(struct sctp_sendv_spa &info, const uint8_t *data,
              size_t length);
  ~OutgoingMsg() = default;;
  void Advance(size_t offset);
  struct sctp_sendv_spa &GetInfo() { return *mInfo; };
  size_t GetLength() { return mLength; };
  size_t GetLeft() { return mLength - mPos; };
  const uint8_t *GetData() { return (const uint8_t *)(mData + mPos); };

protected:
  OutgoingMsg() = default;; // Use this for inheritance only
  size_t mLength;
  const uint8_t *mData;
  struct sctp_sendv_spa *mInfo;
  size_t mPos;
};

// For queuing outgoing messages
// This class copies data of an outgoing message.
class BufferedOutgoingMsg : public OutgoingMsg
{
public:
  explicit BufferedOutgoingMsg(OutgoingMsg &message);
  ~BufferedOutgoingMsg();
};

// for queuing incoming data messages before the Open or
// external negotiation is indicated to us
class QueuedDataMessage
{
public:
  QueuedDataMessage(uint16_t stream, uint32_t ppid, int flags,
                    const void *data, uint32_t length)
    : mStream(stream)
    , mPpid(ppid)
    , mFlags(flags)
    , mLength(length)
  {
    mData = static_cast<uint8_t *>(moz_xmalloc((size_t)length)); // infallible
    memcpy(mData, data, (size_t)length);
  }

  ~QueuedDataMessage()
  {
    free(mData);
  }

  uint16_t mStream;
  uint32_t mPpid;
  int      mFlags;
  uint32_t mLength;
  uint8_t  *mData;
};

// One per PeerConnection
class DataChannelConnection final
  : public net::NeckoTargetHolder
#ifdef SCTP_DTLS_SUPPORTED
  , public sigslot::has_slots<>
#endif
{
  virtual ~DataChannelConnection();

public:
  enum {
      PENDING_NONE = 0U, // No outgoing messages are pending
      PENDING_DCEP = 1U, // Outgoing DCEP messages are pending
      PENDING_DATA = 2U, // Outgoing data channel messages are pending
  };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannelConnection)

  class DataConnectionListener : public SupportsWeakPtr<DataConnectionListener>
  {
  public:
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(DataChannelConnection::DataConnectionListener)
    virtual ~DataConnectionListener() = default;

    // Called when a new DataChannel has been opened by the other side.
    virtual void NotifyDataChannel(already_AddRefed<DataChannel> channel) = 0;
  };

  explicit DataChannelConnection(DataConnectionListener *listener,
                                 nsIEventTarget *aTarget);

  bool Init(unsigned short aPort, uint16_t aNumStreams, bool aMaxMessageSizeSet,
            uint64_t aMaxMessageSize);

  void Destroy(); // So we can spawn refs tied to runnables in shutdown
  // Finish Destroy on STS to avoid SCTP race condition with ABORT from far end
  void DestroyOnSTS(struct socket *aMasterSocket,
                    struct socket *aSocket);
  void DestroyOnSTSFinal();

  void SetMaxMessageSize(bool aMaxMessageSizeSet, uint64_t aMaxMessageSize);
  uint64_t GetMaxMessageSize();

#ifdef ALLOW_DIRECT_SCTP_LISTEN_CONNECT
  // These block; they require something to decide on listener/connector
  // (though you can do simultaneous Connect()).  Do not call these from
  // the main thread!
  bool Listen(unsigned short port);
  bool Connect(const char *addr, unsigned short port);
#endif

#ifdef SCTP_DTLS_SUPPORTED
  // Connect using a TransportFlow (DTLS) channel
  void SetEvenOdd();
  bool ConnectViaTransportFlow(TransportFlow *aFlow, uint16_t localport, uint16_t remoteport);
  void CompleteConnect(TransportFlow *flow, TransportLayer::State state);
  void SetSignals();
#endif

  typedef enum {
    RELIABLE=0,
    PARTIAL_RELIABLE_REXMIT = 1,
    PARTIAL_RELIABLE_TIMED = 2
  } Type;

  MOZ_MUST_USE
  already_AddRefed<DataChannel> Open(const nsACString& label,
                                     const nsACString& protocol,
                                     Type type, bool inOrder,
                                     uint32_t prValue,
                                     DataChannelListener *aListener,
                                     nsISupports *aContext,
                                     bool aExternalNegotiated,
                                     uint16_t aStream);

  void Stop();
  void Close(DataChannel *aChannel);
  // CloseInt() must be called with mLock held
  void CloseInt(DataChannel *aChannel);
  void CloseAll();

  // Returns a POSIX error code.
  int SendMsg(uint16_t stream, const nsACString &aMsg)
    {
      return SendDataMsgCommon(stream, aMsg, false);
    }

  // Returns a POSIX error code.
  int SendBinaryMsg(uint16_t stream, const nsACString &aMsg)
    {
      return SendDataMsgCommon(stream, aMsg, true);
    }

  // Returns a POSIX error code.
  int SendBlob(uint16_t stream, nsIInputStream *aBlob);

  // Called on data reception from the SCTP library
  // must(?) be public so my c->c++ trampoline can call it
  int ReceiveCallback(struct socket* sock, void *data, size_t datalen,
                      struct sctp_rcvinfo rcv, int flags);

  // Find out state
  enum {
    CONNECTING = 0U,
    OPEN = 1U,
    CLOSING = 2U,
    CLOSED = 3U
  };
  uint16_t GetReadyState() { MutexAutoLock lock(mLock); return mState; }

  friend class DataChannel;
  Mutex  mLock;

  void ReadBlob(already_AddRefed<DataChannelConnection> aThis, uint16_t aStream, nsIInputStream* aBlob);

  void GetStreamIds(std::vector<uint16_t>* aStreamList);

  bool SendDeferredMessages();

protected:
  friend class DataChannelOnMessageAvailable;
  // Avoid cycles with PeerConnectionImpl
  // Use from main thread only as WeakPtr is not threadsafe
  WeakPtr<DataConnectionListener> mListener;

private:
  friend class DataChannelConnectRunnable;

#ifdef SCTP_DTLS_SUPPORTED
  static void DTLSConnectThread(void *data);
  int SendPacket(unsigned char data[], size_t len, bool release);
  void SctpDtlsInput(TransportFlow *flow, const unsigned char *data, size_t len);
  static int SctpDtlsOutput(void *addr, void *buffer, size_t length, uint8_t tos, uint8_t set_df);
#endif
  DataChannel* FindChannelByStream(uint16_t stream);
  uint16_t FindFreeStream();
  bool RequestMoreStreams(int32_t aNeeded = 16);
  uint32_t UpdateCurrentStreamIndex();
  uint32_t GetCurrentStreamIndex();
  int SendControlMessage(const uint8_t *data, uint32_t len, uint16_t stream);
  int SendOpenAckMessage(uint16_t stream);
  int SendOpenRequestMessage(const nsACString& label, const nsACString& protocol, uint16_t stream,
                             bool unordered, uint16_t prPolicy, uint32_t prValue);
  bool SendBufferedMessages(nsTArray<nsAutoPtr<BufferedOutgoingMsg>> &buffer);
  int SendMsgInternal(OutgoingMsg &msg);
  int SendMsgInternalOrBuffer(nsTArray<nsAutoPtr<BufferedOutgoingMsg>> &buffer, OutgoingMsg &msg,
                              bool &buffered);
  int SendDataMsgInternalOrBuffer(DataChannel &channel, const uint8_t *data, size_t len,
                                  uint32_t ppid);
  int SendDataMsg(DataChannel &channel, const uint8_t *data, size_t len, uint32_t ppidPartial,
                  uint32_t ppidFinal);
  int SendDataMsgCommon(uint16_t stream, const nsACString &aMsg, bool isBinary);

  void DeliverQueuedData(uint16_t stream);

  already_AddRefed<DataChannel> OpenFinish(already_AddRefed<DataChannel>&& aChannel);

  void ProcessQueuedOpens();
  void ClearResets();
  void SendOutgoingStreamReset();
  void ResetOutgoingStream(uint16_t stream);
  void HandleOpenRequestMessage(const struct rtcweb_datachannel_open_request *req,
                                uint32_t length, uint16_t stream);
  void HandleOpenAckMessage(const struct rtcweb_datachannel_ack *ack,
                            uint32_t length, uint16_t stream);
  void HandleUnknownMessage(uint32_t ppid, uint32_t length, uint16_t stream);
  uint8_t BufferMessage(nsACString& recvBuffer, const void *data, uint32_t length, uint32_t ppid,
                        int flags);
  void HandleDataMessage(const void *buffer, size_t length, uint32_t ppid, uint16_t stream,
                         int flags);
  void HandleDCEPMessage(const void *buffer, size_t length, uint32_t ppid, uint16_t stream,
                         int flags);
  void HandleMessage(const void *buffer, size_t length, uint32_t ppid, uint16_t stream, int flags);
  void HandleAssociationChangeEvent(const struct sctp_assoc_change *sac);
  void HandlePeerAddressChangeEvent(const struct sctp_paddr_change *spc);
  void HandleRemoteErrorEvent(const struct sctp_remote_error *sre);
  void HandleShutdownEvent(const struct sctp_shutdown_event *sse);
  void HandleAdaptationIndication(const struct sctp_adaptation_event *sai);
  void HandlePartialDeliveryEvent(const struct sctp_pdapi_event *spde);
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

  // Exists solely for proxying release of the TransportFlow to the STS thread
  static void ReleaseTransportFlow(const RefPtr<TransportFlow>& aFlow) {}

  bool mSendInterleaved;
  bool mPpidFragmentation;
  bool mMaxMessageSizeSet;
  uint64_t mMaxMessageSize;

  // Data:
  // NOTE: while this array will auto-expand, increases in the number of
  // channels available from the stack must be negotiated!
  bool mAllocateEven;
  AutoTArray<RefPtr<DataChannel>,16> mStreams;
  uint32_t mCurrentStream;
  nsDeque mPending; // Holds addref'ed DataChannel's -- careful!
  // holds data that's come in before a channel is open
  nsTArray<nsAutoPtr<QueuedDataMessage>> mQueuedData;
  // holds outgoing control messages
  nsTArray<nsAutoPtr<BufferedOutgoingMsg>> mBufferedControl; // GUARDED_BY(mConnection->mLock)

  // Streams pending reset
  AutoTArray<uint16_t,4> mStreamsResetting;

  struct socket *mMasterSocket; // accessed from STS thread
  struct socket *mSocket; // cloned from mMasterSocket on successful Connect on STS thread
  uint16_t mState; // Protected with mLock

#ifdef SCTP_DTLS_SUPPORTED
  RefPtr<TransportFlow> mTransportFlow;
  nsCOMPtr<nsIEventTarget> mSTS;
#endif
  uint16_t mLocalPort; // Accessed from connect thread
  uint16_t mRemotePort;

  nsCOMPtr<nsIThread> mInternalIOThread;
  uint8_t mPendingType;
  nsCString mRecvBuffer;
};

#define ENSURE_DATACONNECTION \
  do { MOZ_ASSERT(mConnection); if (!mConnection) { return; } } while (0)

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
              uint16_t stream,
              uint16_t state,
              const nsACString& label,
              const nsACString& protocol,
              uint16_t policy, uint32_t value,
              uint32_t flags,
              DataChannelListener *aListener,
              nsISupports *aContext)
    : mListenerLock("netwerk::sctp::DataChannel")
    , mListener(aListener)
    , mContext(aContext)
    , mConnection(connection)
    , mLabel(label)
    , mProtocol(protocol)
    , mState(state)
    , mStream(stream)
    , mPrPolicy(policy)
    , mPrValue(value)
    , mFlags(flags)
    , mIsRecvBinary(false)
    , mBufferedThreshold(0) // default from spec
    , mMainThreadEventTarget(connection->GetNeckoTarget())
    {
      NS_ASSERTION(mConnection,"NULL connection");
    }

private:
  ~DataChannel();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannel)

  // when we disconnect from the connection after stream RESET
  void StreamClosedLocked();

  // Complete dropping of the link between DataChannel and the connection.
  // After this, except for a few methods below listed to be safe, you can't
  // call into DataChannel.
  void ReleaseConnection();

  // Close this DataChannel.  Can be called multiple times.  MUST be called
  // before destroying the DataChannel (state must be CLOSED or CLOSING).
  void Close();

  // Set the listener (especially for channels created from the other side)
  void SetListener(DataChannelListener *aListener, nsISupports *aContext);

  // Helper for send methods that converts POSIX error codes to an ErrorResult.
  static void SendErrnoToErrorResult(int error, ErrorResult& aRv);

  // Send a string
  void SendMsg(const nsACString &aMsg, ErrorResult& aRv);

  // Send a binary message (TypedArray)
  void SendBinaryMsg(const nsACString &aMsg, ErrorResult& aRv);

  // Send a binary blob
  void SendBinaryStream(nsIInputStream *aBlob, ErrorResult& aRv);

  uint16_t GetType() { return mPrPolicy; }

  bool GetOrdered() { return !(mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED); }

  // Amount of data buffered to send
  uint32_t GetBufferedAmount()
  {
    if (!mConnection) {
      return 0;
    }

    MutexAutoLock lock(mConnection->mLock);
    size_t buffered = GetBufferedAmountLocked();

#if (SIZE_MAX > UINT32_MAX)
    if (buffered > UINT32_MAX) { // paranoia - >4GB buffered is very very unlikely
      buffered = UINT32_MAX;
    }
#endif

    return buffered;
  }


  // Trigger amount for generating BufferedAmountLow events
  uint32_t GetBufferedAmountLowThreshold();
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);

  // Find out state
  uint16_t GetReadyState()
    {
      if (mConnection) {
        MutexAutoLock lock(mConnection->mLock);
        if (mState == WAITING_TO_OPEN)
          return CONNECTING;
        return mState;
      }
      return CLOSED;
    }

  void GetLabel(nsAString& aLabel) { CopyUTF8toUTF16(mLabel, aLabel); }
  void GetProtocol(nsAString& aProtocol) { CopyUTF8toUTF16(mProtocol, aProtocol); }
  uint16_t GetStream() { return mStream; }

  void AppReady();

  void SendOrQueue(DataChannelOnMessageAvailable *aMessage);

protected:
  Mutex mListenerLock; // protects mListener and mContext
  DataChannelListener *mListener;
  nsCOMPtr<nsISupports> mContext;

private:
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnection;

  nsresult AddDataToBinaryMsg(const char *data, uint32_t size);
  size_t GetBufferedAmountLocked() const;
  bool EnsureValidStream(ErrorResult& aRv);

  RefPtr<DataChannelConnection> mConnection;
  nsCString mLabel;
  nsCString mProtocol;
  uint16_t mState;
  uint16_t mStream;
  uint16_t mPrPolicy;
  uint32_t mPrValue;
  uint32_t mFlags;
  uint32_t mId;
  bool mIsRecvBinary;
  size_t mBufferedThreshold;
  nsCString mRecvBuffer;
  nsTArray<nsAutoPtr<BufferedOutgoingMsg>> mBufferedData; // GUARDED_BY(mConnection->mLock)
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedMessages;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

// used to dispatch notifications of incoming data to the main thread
// Patterned on CallOnMessageAvailable in WebSockets
// Also used to proxy other items to MainThread
class DataChannelOnMessageAvailable : public Runnable
{
public:
  enum {
    ON_CONNECTION,
    ON_DISCONNECTED,
    ON_CHANNEL_CREATED,
    ON_CHANNEL_OPEN,
    ON_CHANNEL_CLOSED,
    ON_DATA_STRING,
    ON_DATA_BINARY,
    BUFFER_LOW_THRESHOLD,
    NO_LONGER_BUFFERED,
  };  /* types */

  DataChannelOnMessageAvailable(
    int32_t aType,
    DataChannelConnection* aConnection,
    DataChannel* aChannel,
    nsCString& aData) // XXX this causes inefficiency
    : Runnable("DataChannelOnMessageAvailable")
    , mType(aType)
    , mChannel(aChannel)
    , mConnection(aConnection)
    , mData(aData)
  {
  }

  DataChannelOnMessageAvailable(int32_t aType, DataChannel* aChannel)
    : Runnable("DataChannelOnMessageAvailable")
    , mType(aType)
    , mChannel(aChannel)
  {
  }
  // XXX is it safe to leave mData uninitialized?  This should only be
  // used for notifications that don't use them, but I'd like more
  // bulletproof compile-time checking.

  DataChannelOnMessageAvailable(int32_t aType,
                                DataChannelConnection* aConnection,
                                DataChannel* aChannel)
    : Runnable("DataChannelOnMessageAvailable")
    , mType(aType)
    , mChannel(aChannel)
    , mConnection(aConnection)
  {
  }

  // for ON_CONNECTION/ON_DISCONNECTED
  DataChannelOnMessageAvailable(int32_t aType,
                                DataChannelConnection* aConnection)
    : Runnable("DataChannelOnMessageAvailable")
    , mType(aType)
    , mConnection(aConnection)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Note: calling the listeners can indirectly cause the listeners to be
    // made available for GC (by removing event listeners), especially for
    // OnChannelClosed().  We hold a ref to the Channel and the listener
    // while calling this.
    switch (mType) {
      case ON_DATA_STRING:
      case ON_DATA_BINARY:
      case ON_CHANNEL_OPEN:
      case ON_CHANNEL_CLOSED:
      case BUFFER_LOW_THRESHOLD:
      case NO_LONGER_BUFFERED:
        {
          MutexAutoLock lock(mChannel->mListenerLock);
          if (!mChannel->mListener) {
            DATACHANNEL_LOG(("DataChannelOnMessageAvailable (%d) with null Listener!",mType));
            return NS_OK;
          }

          switch (mType) {
            case ON_DATA_STRING:
              mChannel->mListener->OnMessageAvailable(mChannel->mContext, mData);
              break;
            case ON_DATA_BINARY:
              mChannel->mListener->OnBinaryMessageAvailable(mChannel->mContext, mData);
              break;
            case ON_CHANNEL_OPEN:
              mChannel->mListener->OnChannelConnected(mChannel->mContext);
              break;
            case ON_CHANNEL_CLOSED:
              mChannel->mListener->OnChannelClosed(mChannel->mContext);
              break;
            case BUFFER_LOW_THRESHOLD:
              mChannel->mListener->OnBufferLow(mChannel->mContext);
              break;
            case NO_LONGER_BUFFERED:
              mChannel->mListener->NotBuffered(mChannel->mContext);
              break;
          }
          break;
        }
      case ON_DISCONNECTED:
        // If we've disconnected, make sure we close all the streams - from mainthread!
        mConnection->CloseAll();
        MOZ_FALLTHROUGH;
      case ON_CHANNEL_CREATED:
      case ON_CONNECTION:
        // WeakPtr - only used/modified/nulled from MainThread so we can use a WeakPtr here
        if (!mConnection->mListener) {
          DATACHANNEL_LOG(("DataChannelOnMessageAvailable (%d) with null Listener",mType));
          return NS_OK;
        }
        switch (mType) {
          case ON_CHANNEL_CREATED:
            // important to give it an already_AddRefed pointer!
            mConnection->mListener->NotifyDataChannel(mChannel.forget());
            break;
          default:
            break;
        }
        break;
    }
    return NS_OK;
  }

private:
  ~DataChannelOnMessageAvailable() = default;

  int32_t                         mType;
  // XXX should use union
  RefPtr<DataChannel>             mChannel;
  RefPtr<DataChannelConnection>   mConnection;
  nsCString                       mData;
};

}

#endif  // NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
