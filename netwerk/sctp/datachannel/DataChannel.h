/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_

#ifdef MOZ_WEBRTC_SIGNALING
#  define SCTP_DTLS_SUPPORTED 1
#endif

#include <memory>
#include <string>
#include <vector>
#include <errno.h>
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/Mutex.h"
#include "DataChannelProtocol.h"
#include "DataChannelListener.h"
#include "mozilla/net/NeckoTargetHolder.h"
#include "DataChannelLog.h"

#ifdef SCTP_DTLS_SUPPORTED
#  include "mtransport/sigslot.h"
#  include "mtransport/transportlayer.h"  // For TransportLayer::State
#endif

#ifndef EALREADY
#  define EALREADY WSAEALREADY
#endif

extern "C" {
struct socket;
struct sctp_rcvinfo;
}

namespace mozilla {

class DataChannelConnection;
class DataChannel;
class DataChannelOnMessageAvailable;
class MediaPacket;
class MediaTransportHandler;

// For sending outgoing messages.
// This class only holds a reference to the data and the info structure but does
// not copy it.
class OutgoingMsg {
 public:
  OutgoingMsg(struct sctp_sendv_spa& info, const uint8_t* data, size_t length);
  ~OutgoingMsg() = default;
  ;
  void Advance(size_t offset);
  struct sctp_sendv_spa& GetInfo() {
    return *mInfo;
  };
  size_t GetLength() { return mLength; };
  size_t GetLeft() { return mLength - mPos; };
  const uint8_t* GetData() { return (const uint8_t*)(mData + mPos); };

 protected:
  OutgoingMsg()  // Use this for inheritance only
      : mLength(0), mData(nullptr), mInfo(nullptr), mPos(0){};
  size_t mLength;
  const uint8_t* mData;
  struct sctp_sendv_spa* mInfo;
  size_t mPos;
};

// For queuing outgoing messages
// This class copies data of an outgoing message.
class BufferedOutgoingMsg : public OutgoingMsg {
 public:
  explicit BufferedOutgoingMsg(OutgoingMsg& message);
  ~BufferedOutgoingMsg();
};

// for queuing incoming data messages before the Open or
// external negotiation is indicated to us
class QueuedDataMessage {
 public:
  QueuedDataMessage(uint16_t stream, uint32_t ppid, int flags, const void* data,
                    uint32_t length)
      : mStream(stream), mPpid(ppid), mFlags(flags), mLength(length) {
    mData = static_cast<uint8_t*>(moz_xmalloc((size_t)length));  // infallible
    memcpy(mData, data, (size_t)length);
  }

  ~QueuedDataMessage() { free(mData); }

  uint16_t mStream;
  uint32_t mPpid;
  int mFlags;
  uint32_t mLength;
  uint8_t* mData;
};

// One per PeerConnection
class DataChannelConnection final : public net::NeckoTargetHolder
#ifdef SCTP_DTLS_SUPPORTED
    ,
                                    public sigslot::has_slots<>
#endif
{
  friend class DataChannel;
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnectRunnable;

  virtual ~DataChannelConnection();

 public:
  enum {
    PENDING_NONE = 0U,  // No outgoing messages are pending
    PENDING_DCEP = 1U,  // Outgoing DCEP messages are pending
    PENDING_DATA = 2U,  // Outgoing data channel messages are pending
  };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannelConnection)

  class DataConnectionListener
      : public SupportsWeakPtr<DataConnectionListener> {
   public:
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(
        DataChannelConnection::DataConnectionListener)
    virtual ~DataConnectionListener() = default;

    // Called when a new DataChannel has been opened by the other side.
    virtual void NotifyDataChannel(already_AddRefed<DataChannel> channel) = 0;
  };

  // Create a new DataChannel Connection
  // Must be called on Main thread
  static Maybe<RefPtr<DataChannelConnection>> Create(
      DataConnectionListener* aListener, nsIEventTarget* aTarget,
      MediaTransportHandler* aHandler, const uint16_t aLocalPort,
      const uint16_t aNumStreams, const Maybe<uint64_t>& aMaxMessageSize);

  void Destroy();  // So we can spawn refs tied to runnables in shutdown
  // Finish Destroy on STS to avoid SCTP race condition with ABORT from far end
  void DestroyOnSTS(struct socket* aMasterSocket, struct socket* aSocket);
  void DestroyOnSTSFinal();

  void SetMaxMessageSize(bool aMaxMessageSizeSet, uint64_t aMaxMessageSize);
  uint64_t GetMaxMessageSize();

#ifdef ALLOW_DIRECT_SCTP_LISTEN_CONNECT
  // These block; they require something to decide on listener/connector
  // (though you can do simultaneous Connect()).  Do not call these from
  // the main thread!
  bool Listen(unsigned short port);
  bool Connect(const char* addr, unsigned short port);
#endif

#ifdef SCTP_DTLS_SUPPORTED
  bool ConnectToTransport(const std::string& aTransportId, const bool aClient,
                          const uint16_t aLocalPort,
                          const uint16_t aRemotePort);
  void TransportStateChange(const std::string& aTransportId,
                            TransportLayer::State aState);
  void CompleteConnect();
  void SetSignals(const std::string& aTransportId);
#endif

  typedef enum {
    RELIABLE = 0,
    PARTIAL_RELIABLE_REXMIT = 1,
    PARTIAL_RELIABLE_TIMED = 2
  } Type;

  [[nodiscard]] already_AddRefed<DataChannel> Open(
      const nsACString& label, const nsACString& protocol, Type type,
      bool inOrder, uint32_t prValue, DataChannelListener* aListener,
      nsISupports* aContext, bool aExternalNegotiated, uint16_t aStream);

  void Stop();
  void Close(DataChannel* aChannel);
  // CloseInt() must be called with mLock held
  void CloseInt(DataChannel* aChannel);
  void CloseAll();

  // Returns a POSIX error code.
  int SendMsg(uint16_t stream, const nsACString& aMsg) {
    return SendDataMsgCommon(stream, aMsg, false);
  }

  // Returns a POSIX error code.
  int SendBinaryMsg(uint16_t stream, const nsACString& aMsg) {
    return SendDataMsgCommon(stream, aMsg, true);
  }

  // Returns a POSIX error code.
  int SendBlob(uint16_t stream, nsIInputStream* aBlob);

  // Called on data reception from the SCTP library
  // must(?) be public so my c->c++ trampoline can call it
  int ReceiveCallback(struct socket* sock, void* data, size_t datalen,
                      struct sctp_rcvinfo rcv, int flags);

  // Find out state
  enum { CONNECTING = 0U, OPEN = 1U, CLOSING = 2U, CLOSED = 3U };

  Mutex mLock;

  void ReadBlob(already_AddRefed<DataChannelConnection> aThis, uint16_t aStream,
                nsIInputStream* aBlob);

  bool SendDeferredMessages();

 protected:
  // Avoid cycles with PeerConnectionImpl
  // Use from main thread only as WeakPtr is not threadsafe
  WeakPtr<DataConnectionListener> mListener;

 private:
  DataChannelConnection(DataConnectionListener* aListener,
                        nsIEventTarget* aTarget,
                        MediaTransportHandler* aHandler);

  bool Init(const uint16_t aLocalPort, const uint16_t aNumStreams,
            const Maybe<uint64_t>& aMaxMessageSize);

  // Caller must hold mLock
  uint16_t GetReadyState() const {
    mLock.AssertCurrentThreadOwns();

    return mState;
  }

  // Caller must hold mLock
  void SetReadyState(const uint16_t aState);

#ifdef SCTP_DTLS_SUPPORTED
  static void DTLSConnectThread(void* data);
  void SendPacket(std::unique_ptr<MediaPacket>&& packet);
  void SctpDtlsInput(const std::string& aTransportId,
                     const MediaPacket& packet);
  static int SctpDtlsOutput(void* addr, void* buffer, size_t length,
                            uint8_t tos, uint8_t set_df);
#endif
  DataChannel* FindChannelByStream(uint16_t stream);
  uint16_t FindFreeStream();
  bool RequestMoreStreams(int32_t aNeeded = 16);
  uint32_t UpdateCurrentStreamIndex();
  uint32_t GetCurrentStreamIndex();
  int SendControlMessage(const uint8_t* data, uint32_t len, uint16_t stream);
  int SendOpenAckMessage(uint16_t stream);
  int SendOpenRequestMessage(const nsACString& label,
                             const nsACString& protocol, uint16_t stream,
                             bool unordered, uint16_t prPolicy,
                             uint32_t prValue);
  bool SendBufferedMessages(nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer,
                            size_t* aWritten);
  int SendMsgInternal(OutgoingMsg& msg, size_t* aWritten);
  int SendMsgInternalOrBuffer(nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer,
                              OutgoingMsg& msg, bool& buffered,
                              size_t* aWritten);
  int SendDataMsgInternalOrBuffer(DataChannel& channel, const uint8_t* data,
                                  size_t len, uint32_t ppid);
  int SendDataMsg(DataChannel& channel, const uint8_t* data, size_t len,
                  uint32_t ppidPartial, uint32_t ppidFinal);
  int SendDataMsgCommon(uint16_t stream, const nsACString& aMsg, bool isBinary);

  void DeliverQueuedData(uint16_t stream);

  already_AddRefed<DataChannel> OpenFinish(
      already_AddRefed<DataChannel>&& aChannel);

  void ProcessQueuedOpens();
  void ClearResets();
  void SendOutgoingStreamReset();
  void ResetOutgoingStream(uint16_t stream);
  void HandleOpenRequestMessage(
      const struct rtcweb_datachannel_open_request* req, uint32_t length,
      uint16_t stream);
  void HandleOpenAckMessage(const struct rtcweb_datachannel_ack* ack,
                            uint32_t length, uint16_t stream);
  void HandleUnknownMessage(uint32_t ppid, uint32_t length, uint16_t stream);
  uint8_t BufferMessage(nsACString& recvBuffer, const void* data,
                        uint32_t length, uint32_t ppid, int flags);
  void HandleDataMessage(const void* buffer, size_t length, uint32_t ppid,
                         uint16_t stream, int flags);
  void HandleDCEPMessage(const void* buffer, size_t length, uint32_t ppid,
                         uint16_t stream, int flags);
  void HandleMessage(const void* buffer, size_t length, uint32_t ppid,
                     uint16_t stream, int flags);
  void HandleAssociationChangeEvent(const struct sctp_assoc_change* sac);
  void HandlePeerAddressChangeEvent(const struct sctp_paddr_change* spc);
  void HandleRemoteErrorEvent(const struct sctp_remote_error* sre);
  void HandleShutdownEvent(const struct sctp_shutdown_event* sse);
  void HandleAdaptationIndication(const struct sctp_adaptation_event* sai);
  void HandlePartialDeliveryEvent(const struct sctp_pdapi_event* spde);
  void HandleSendFailedEvent(const struct sctp_send_failed_event* ssfe);
  void HandleStreamResetEvent(const struct sctp_stream_reset_event* strrst);
  void HandleStreamChangeEvent(const struct sctp_stream_change_event* strchg);
  void HandleNotification(const union sctp_notification* notif, size_t n);

#ifdef SCTP_DTLS_SUPPORTED
  bool IsSTSThread() {
    bool on = false;
    if (mSTS) {
      mSTS->IsOnCurrentThread(&on);
    }
    return on;
  }
#endif

  class Channels {
   public:
    Channels() : mMutex("DataChannelConnection::Channels::mMutex") {}
    void Insert(const RefPtr<DataChannel>& aChannel);
    bool Remove(const RefPtr<DataChannel>& aChannel);
    RefPtr<DataChannel> Get(uint16_t aId) const;
    typedef AutoTArray<RefPtr<DataChannel>, 16> ChannelArray;
    ChannelArray GetAll() const {
      MutexAutoLock lock(mMutex);
      return mChannels.Clone();
    }
    RefPtr<DataChannel> GetNextChannel(uint16_t aCurrentId) const;

   private:
    struct IdComparator {
      bool Equals(const RefPtr<DataChannel>& aChannel, uint16_t aId) const;
      bool LessThan(const RefPtr<DataChannel>& aChannel, uint16_t aId) const;
      bool Equals(const RefPtr<DataChannel>& a1,
                  const RefPtr<DataChannel>& a2) const;
      bool LessThan(const RefPtr<DataChannel>& a1,
                    const RefPtr<DataChannel>& a2) const;
    };
    mutable Mutex mMutex;
    ChannelArray mChannels;
  };

  bool mSendInterleaved = false;
  bool mMaxMessageSizeSet = false;
  uint64_t mMaxMessageSize = 0;
  // Main thread only
  Maybe<bool> mAllocateEven;
  // Data:
  // NOTE: while this container will auto-expand, increases in the number of
  // channels available from the stack must be negotiated!
  // Accessed from both main and sts, API is threadsafe
  Channels mChannels;
  // STS only
  uint32_t mCurrentStream = 0;
  nsDeque mPending;  // Holds addref'ed DataChannel's -- careful!
  // STS and main
  size_t mNegotiatedIdLimit = 0;  // GUARDED_BY(mConnection->mLock)
  uint8_t mPendingType = PENDING_NONE;
  // holds data that's come in before a channel is open
  nsTArray<UniquePtr<QueuedDataMessage>> mQueuedData;
  // holds outgoing control messages
  nsTArray<UniquePtr<BufferedOutgoingMsg>>
      mBufferedControl;  // GUARDED_BY(mConnection->mLock)

  // Streams pending reset. Accessed from main and STS.
  AutoTArray<uint16_t, 4> mStreamsResetting;  // GUARDED_BY(mConnection->mLock)
  // accessed from STS thread
  struct socket* mMasterSocket = nullptr;
  // cloned from mMasterSocket on successful Connect on STS thread
  struct socket* mSocket = nullptr;
  uint16_t mState = CLOSED;  // Protected with mLock

#ifdef SCTP_DTLS_SUPPORTED
  std::string mTransportId;
  RefPtr<MediaTransportHandler> mTransportHandler;
  nsCOMPtr<nsIEventTarget> mSTS;
#endif
  uint16_t mLocalPort = 0;  // Accessed from connect thread
  uint16_t mRemotePort = 0;

  nsCOMPtr<nsIThread> mInternalIOThread = nullptr;
  nsCString mRecvBuffer;

  // Workaround to prevent a message from being received on main before the
  // sender sees the decrease in bufferedAmount.
  bool mDeferSend = false;
  std::vector<std::unique_ptr<MediaPacket>> mDeferredSend;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool mShutdown;
#endif
};

#define ENSURE_DATACONNECTION \
  do {                        \
    MOZ_ASSERT(mConnection);  \
    if (!mConnection) {       \
      return;                 \
    }                         \
  } while (0)

class DataChannel {
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnection;

 public:
  enum { CONNECTING = 0U, OPEN = 1U, CLOSING = 2U, CLOSED = 3U };

  DataChannel(DataChannelConnection* connection, uint16_t stream,
              uint16_t state, const nsACString& label,
              const nsACString& protocol, uint16_t policy, uint32_t value,
              bool ordered, bool negotiated, DataChannelListener* aListener,
              nsISupports* aContext)
      : mListener(aListener),
        mContext(aContext),
        mConnection(connection),
        mLabel(label),
        mProtocol(protocol),
        mReadyState(state),
        mStream(stream),
        mPrPolicy(policy),
        mPrValue(value),
        mNegotiated(negotiated),
        mOrdered(ordered),
        mFlags(0),
        mIsRecvBinary(false),
        mBufferedThreshold(0),  // default from spec
        mBufferedAmount(0),
        mMainThreadEventTarget(connection->GetNeckoTarget()) {
    NS_ASSERTION(mConnection, "NULL connection");
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
  void SetListener(DataChannelListener* aListener, nsISupports* aContext);

  // Helper for send methods that converts POSIX error codes to an ErrorResult.
  static void SendErrnoToErrorResult(int error, size_t aMessageSize,
                                     ErrorResult& aRv);

  // Send a string
  void SendMsg(const nsACString& aMsg, ErrorResult& aRv);

  // Send a binary message (TypedArray)
  void SendBinaryMsg(const nsACString& aMsg, ErrorResult& aRv);

  // Send a binary blob
  void SendBinaryBlob(dom::Blob& aBlob, ErrorResult& aRv);

  uint16_t GetType() { return mPrPolicy; }

  dom::Nullable<uint16_t> GetMaxPacketLifeTime() const;

  dom::Nullable<uint16_t> GetMaxRetransmits() const;

  bool GetNegotiated() { return mNegotiated; }

  bool GetOrdered() { return mOrdered; }

  void IncrementBufferedAmount(uint32_t aSize, ErrorResult& aRv);
  void DecrementBufferedAmount(uint32_t aSize);

  // Amount of data buffered to send
  uint32_t GetBufferedAmount() {
    MOZ_ASSERT(NS_IsMainThread());
    return mBufferedAmount;
  }

  // Trigger amount for generating BufferedAmountLow events
  uint32_t GetBufferedAmountLowThreshold();
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);

  void AnnounceOpen();
  // TODO(bug 843625): Optionally pass an error here.
  void AnnounceClosed();

  // Find out state
  uint16_t GetReadyState() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mReadyState;
  }

  // Set ready state
  void SetReadyState(const uint16_t aState);

  void GetLabel(nsAString& aLabel) { CopyUTF8toUTF16(mLabel, aLabel); }
  void GetProtocol(nsAString& aProtocol) {
    CopyUTF8toUTF16(mProtocol, aProtocol);
  }
  uint16_t GetStream() { return mStream; }

  void SendOrQueue(DataChannelOnMessageAvailable* aMessage);

 protected:
  // These are both mainthread only
  DataChannelListener* mListener;
  nsCOMPtr<nsISupports> mContext;

 private:
  nsresult AddDataToBinaryMsg(const char* data, uint32_t size);
  bool EnsureValidStream(ErrorResult& aRv);

  RefPtr<DataChannelConnection> mConnection;
  nsCString mLabel;
  nsCString mProtocol;
  // This is mainthread only
  uint16_t mReadyState;
  uint16_t mStream;
  uint16_t mPrPolicy;
  uint32_t mPrValue;
  // Accessed on main and STS
  const bool mNegotiated;
  const bool mOrdered;
  uint32_t mFlags;
  bool mIsRecvBinary;
  size_t mBufferedThreshold;
  // Read/written on main only. Decremented via message-passing, because the
  // spec requires us to queue a task for this.
  size_t mBufferedAmount;
  nsCString mRecvBuffer;
  nsTArray<UniquePtr<BufferedOutgoingMsg>>
      mBufferedData;  // GUARDED_BY(mConnection->mLock)
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

// used to dispatch notifications of incoming data to the main thread
// Patterned on CallOnMessageAvailable in WebSockets
// Also used to proxy other items to MainThread
class DataChannelOnMessageAvailable : public Runnable {
 public:
  enum {
    ON_CONNECTION,
    ON_DISCONNECTED,
    ON_CHANNEL_CREATED,
    ON_DATA_STRING,
    ON_DATA_BINARY,
  }; /* types */

  DataChannelOnMessageAvailable(
      int32_t aType, DataChannelConnection* aConnection, DataChannel* aChannel,
      nsCString& aData)  // XXX this causes inefficiency
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection),
        mData(aData) {}

  DataChannelOnMessageAvailable(int32_t aType, DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel) {}
  // XXX is it safe to leave mData uninitialized?  This should only be
  // used for notifications that don't use them, but I'd like more
  // bulletproof compile-time checking.

  DataChannelOnMessageAvailable(int32_t aType,
                                DataChannelConnection* aConnection,
                                DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection) {}

  // for ON_CONNECTION/ON_DISCONNECTED
  DataChannelOnMessageAvailable(int32_t aType,
                                DataChannelConnection* aConnection)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mConnection(aConnection) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    // Note: calling the listeners can indirectly cause the listeners to be
    // made available for GC (by removing event listeners), especially for
    // OnChannelClosed().  We hold a ref to the Channel and the listener
    // while calling this.
    switch (mType) {
      case ON_DATA_STRING:
      case ON_DATA_BINARY:
        if (!mChannel->mListener) {
          DC_ERROR(("DataChannelOnMessageAvailable (%d) with null Listener!",
                    mType));
          return NS_OK;
        }

        if (mChannel->GetReadyState() == DataChannel::CLOSED ||
            mChannel->GetReadyState() == DataChannel::CLOSING) {
          // Closed by JS, probably
          return NS_OK;
        }

        if (mType == ON_DATA_STRING) {
          mChannel->mListener->OnMessageAvailable(mChannel->mContext, mData);
        } else {
          mChannel->mListener->OnBinaryMessageAvailable(mChannel->mContext,
                                                        mData);
        }
        break;
      case ON_DISCONNECTED:
        // If we've disconnected, make sure we close all the streams - from
        // mainthread!
        mConnection->CloseAll();
        break;
      case ON_CHANNEL_CREATED:
        if (!mConnection->mListener) {
          DC_ERROR(("DataChannelOnMessageAvailable (%d) with null Listener!",
                    mType));
          return NS_OK;
        }

        // important to give it an already_AddRefed pointer!
        mConnection->mListener->NotifyDataChannel(mChannel.forget());
        break;
      case ON_CONNECTION:
        // TODO: Notify someday? How? What does the spec say about this?
        break;
    }
    return NS_OK;
  }

 private:
  ~DataChannelOnMessageAvailable() = default;

  int32_t mType;
  // XXX should use union
  RefPtr<DataChannel> mChannel;
  RefPtr<DataChannelConnection> mConnection;
  nsCString mData;
};

}  // namespace mozilla

#endif  // NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
