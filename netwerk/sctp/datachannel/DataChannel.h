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
#  include "transport/sigslot.h"
#  include "transport/transportlayer.h"  // For TransportLayer::State
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
namespace dom {
struct RTCStatsCollection;
};

enum class DataChannelState { Connecting, Open, Closing, Closed };
enum class DataChannelConnectionState { Connecting, Open, Closed };
enum class DataChannelReliabilityPolicy {
  Reliable,
  LimitedRetransmissions,
  LimitedLifetime
};

// For sending outgoing messages.
// This class only holds a reference to the data and the info structure but does
// not copy it.
class OutgoingMsg {
 public:
  OutgoingMsg(struct sctp_sendv_spa& info, const uint8_t* data, size_t length);
  OutgoingMsg(OutgoingMsg&& other) = default;
  OutgoingMsg& operator=(OutgoingMsg&& other) = default;
  ~OutgoingMsg() = default;

  void Advance(size_t offset);
  struct sctp_sendv_spa& GetInfo() const { return *mInfo; };
  size_t GetLength() const { return mLength; };
  size_t GetLeft() const { return mLength - mPos; };
  const uint8_t* GetData() const { return (const uint8_t*)(mData + mPos); };

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
  explicit BufferedOutgoingMsg(OutgoingMsg& msg);
  BufferedOutgoingMsg(BufferedOutgoingMsg&& other) = default;
  BufferedOutgoingMsg& operator=(BufferedOutgoingMsg&& other) = default;
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
  QueuedDataMessage(QueuedDataMessage&& other) = default;
  QueuedDataMessage& operator=(QueuedDataMessage&& other) = default;
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
  enum class PendingType {
    None,  // No outgoing messages are pending.
    Dcep,  // Outgoing DCEP messages are pending.
    Data,  // Outgoing data channel messages are pending.
  };

  class DataConnectionListener : public SupportsWeakPtr {
   public:
    virtual ~DataConnectionListener() = default;

    // Called when a new DataChannel has been opened by the other side.
    virtual void NotifyDataChannel(already_AddRefed<DataChannel> channel) = 0;

    // Called when a DataChannel transitions to state open
    virtual void NotifyDataChannelOpen(DataChannel* aChannel) = 0;

    // Called when a DataChannel (that was open at some point in the past)
    // transitions to state closed
    virtual void NotifyDataChannelClosed(DataChannel* aChannel) = 0;

    // Called when SCTP connects
    virtual void NotifySctpConnected() = 0;

    // Called when SCTP closes
    virtual void NotifySctpClosed() = 0;
  };

  // Create a new DataChannel Connection
  // Must be called on Main thread
  static Maybe<RefPtr<DataChannelConnection>> Create(
      DataConnectionListener* aListener, nsISerialEventTarget* aTarget,
      MediaTransportHandler* aHandler, const uint16_t aLocalPort,
      const uint16_t aNumStreams, const Maybe<uint64_t>& aMaxMessageSize);

  DataChannelConnection(const DataChannelConnection&) = delete;
  DataChannelConnection& operator=(const DataChannelConnection&) = delete;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannelConnection)

  void Destroy();  // So we can spawn refs tied to runnables in shutdown
  // Finish Destroy on STS to avoid SCTP race condition with ABORT from far end
  void DestroyOnSTS(struct socket* aMasterSocket, struct socket* aSocket);
  void DestroyOnSTSFinal();

  void SetMaxMessageSize(bool aMaxMessageSizeSet, uint64_t aMaxMessageSize);
  uint64_t GetMaxMessageSize();

  void AppendStatsToReport(const UniquePtr<dom::RTCStatsCollection>& aReport,
                           const DOMHighResTimeStamp aTimestamp) const;
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

  [[nodiscard]] already_AddRefed<DataChannel> Open(
      const nsACString& label, const nsACString& protocol,
      DataChannelReliabilityPolicy prPolicy, bool inOrder, uint32_t prValue,
      DataChannelListener* aListener, nsISupports* aContext,
      bool aExternalNegotiated, uint16_t aStream);

  void Stop();
  void Close(DataChannel* aChannel);
  void CloseLocked(DataChannel* aChannel) MOZ_REQUIRES(mLock);
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
  // May be called with (STS thread) or without the lock
  int ReceiveCallback(struct socket* sock, void* data, size_t datalen,
                      struct sctp_rcvinfo rcv, int flags);

  void ReadBlob(already_AddRefed<DataChannelConnection> aThis, uint16_t aStream,
                nsIInputStream* aBlob);

  bool SendDeferredMessages() MOZ_REQUIRES(mLock);

#ifdef SCTP_DTLS_SUPPORTED
  int SctpDtlsOutput(void* addr, void* buffer, size_t length, uint8_t tos,
                     uint8_t set_df);
#endif

  bool InShutdown() const {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return mShutdown;
#else
    return false;
#endif
  }

 private:
  class Channels {
   public:
    using ChannelArray = AutoTArray<RefPtr<DataChannel>, 16>;

    Channels() : mMutex("DataChannelConnection::Channels::mMutex") {}
    Channels(const Channels&) = delete;
    Channels& operator=(const Channels&) = delete;

    void Insert(const RefPtr<DataChannel>& aChannel);
    bool Remove(const RefPtr<DataChannel>& aChannel);
    RefPtr<DataChannel> Get(uint16_t aId) const;
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
    ChannelArray mChannels MOZ_GUARDED_BY(mMutex);
  };

  DataChannelConnection(DataConnectionListener* aListener,
                        nsISerialEventTarget* aTarget,
                        MediaTransportHandler* aHandler);

  bool Init(const uint16_t aLocalPort, const uint16_t aNumStreams,
            const Maybe<uint64_t>& aMaxMessageSize);

  DataChannelConnectionState GetState() const MOZ_REQUIRES(mLock) {
    mLock.AssertCurrentThreadOwns();

    return mState;
  }

  void SetState(DataChannelConnectionState aState) MOZ_REQUIRES(mLock);
  static int OnThresholdEvent(struct socket* sock, uint32_t sb_free,
                              void* ulp_info);

#ifdef SCTP_DTLS_SUPPORTED
  static void DTLSConnectThread(void* data);
  void SendPacket(std::unique_ptr<MediaPacket>&& packet);
  void SctpDtlsInput(const std::string& aTransportId,
                     const MediaPacket& packet);
#endif
  DataChannel* FindChannelByStream(uint16_t stream) MOZ_REQUIRES(mLock);
  uint16_t FindFreeStream() const MOZ_REQUIRES(mLock);
  bool RequestMoreStreams(int32_t aNeeded = 16) MOZ_REQUIRES(mLock);
  uint32_t UpdateCurrentStreamIndex() MOZ_REQUIRES(mLock);
  uint32_t GetCurrentStreamIndex() MOZ_REQUIRES(mLock);
  int SendControlMessage(const uint8_t* data, uint32_t len, uint16_t stream)
      MOZ_REQUIRES(mLock);
  int SendOpenAckMessage(uint16_t stream) MOZ_REQUIRES(mLock);
  int SendOpenRequestMessage(const nsACString& label,
                             const nsACString& protocol, uint16_t stream,
                             bool unordered,
                             DataChannelReliabilityPolicy prPolicy,
                             uint32_t prValue) MOZ_REQUIRES(mLock);
  bool SendBufferedMessages(nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer,
                            size_t* aWritten);
  int SendMsgInternal(OutgoingMsg& msg, size_t* aWritten);
  int SendMsgInternalOrBuffer(nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer,
                              OutgoingMsg& msg, bool& buffered,
                              size_t* aWritten) MOZ_REQUIRES(mLock);
  int SendDataMsgInternalOrBuffer(DataChannel& channel, const uint8_t* data,
                                  size_t len, uint32_t ppid)
      MOZ_REQUIRES(mLock);
  int SendDataMsg(DataChannel& channel, const uint8_t* data, size_t len,
                  uint32_t ppidPartial, uint32_t ppidFinal) MOZ_REQUIRES(mLock);
  int SendDataMsgCommon(uint16_t stream, const nsACString& aMsg, bool isBinary);

  void DeliverQueuedData(uint16_t stream) MOZ_REQUIRES(mLock);

  already_AddRefed<DataChannel> OpenFinish(
      already_AddRefed<DataChannel>&& aChannel) MOZ_REQUIRES(mLock);

  void ProcessQueuedOpens() MOZ_REQUIRES(mLock);
  void ClearResets() MOZ_REQUIRES(mLock);
  void SendOutgoingStreamReset() MOZ_REQUIRES(mLock);
  void ResetOutgoingStream(uint16_t stream) MOZ_REQUIRES(mLock);
  void HandleOpenRequestMessage(
      const struct rtcweb_datachannel_open_request* req, uint32_t length,
      uint16_t stream) MOZ_REQUIRES(mLock);
  void HandleOpenAckMessage(const struct rtcweb_datachannel_ack* ack,
                            uint32_t length, uint16_t stream);
  void HandleUnknownMessage(uint32_t ppid, uint32_t length, uint16_t stream)
      MOZ_REQUIRES(mLock);
  uint8_t BufferMessage(nsACString& recvBuffer, const void* data,
                        uint32_t length, uint32_t ppid, int flags);
  void HandleDataMessage(const void* data, size_t length, uint32_t ppid,
                         uint16_t stream, int flags) MOZ_REQUIRES(mLock);
  void HandleDCEPMessage(const void* buffer, size_t length, uint32_t ppid,
                         uint16_t stream, int flags) MOZ_REQUIRES(mLock);
  void HandleMessage(const void* buffer, size_t length, uint32_t ppid,
                     uint16_t stream, int flags) MOZ_REQUIRES(mLock);
  void HandleAssociationChangeEvent(const struct sctp_assoc_change* sac)
      MOZ_REQUIRES(mLock);
  void HandlePeerAddressChangeEvent(const struct sctp_paddr_change* spc)
      MOZ_REQUIRES(mLock);
  void HandleRemoteErrorEvent(const struct sctp_remote_error* sre)
      MOZ_REQUIRES(mLock);
  void HandleShutdownEvent(const struct sctp_shutdown_event* sse)
      MOZ_REQUIRES(mLock);
  void HandleAdaptationIndication(const struct sctp_adaptation_event* sai)
      MOZ_REQUIRES(mLock);
  void HandlePartialDeliveryEvent(const struct sctp_pdapi_event* spde)
      MOZ_REQUIRES(mLock);
  void HandleSendFailedEvent(const struct sctp_send_failed_event* ssfe)
      MOZ_REQUIRES(mLock);
  void HandleStreamResetEvent(const struct sctp_stream_reset_event* strrst)
      MOZ_REQUIRES(mLock);
  void HandleStreamChangeEvent(const struct sctp_stream_change_event* strchg)
      MOZ_REQUIRES(mLock);
  void HandleNotification(const union sctp_notification* notif, size_t n)
      MOZ_REQUIRES(mLock);

#ifdef SCTP_DTLS_SUPPORTED
  bool IsSTSThread() const {
    bool on = false;
    if (mSTS) {
      mSTS->IsOnCurrentThread(&on);
    }
    return on;
  }
#endif

  mutable Mutex mLock;
  // Avoid cycles with PeerConnectionImpl
  // Use from main thread only as WeakPtr is not threadsafe
  WeakPtr<DataConnectionListener> mListener;
  bool mSendInterleaved MOZ_GUARDED_BY(mLock) = false;
  // MainThread only
  bool mMaxMessageSizeSet = false;
  // mMaxMessageSize is only set on MainThread, but read off-main-thread
  uint64_t mMaxMessageSize MOZ_GUARDED_BY(mLock) = 0;
  // Main thread only
  Maybe<bool> mAllocateEven;
  // Data:
  // NOTE: while this container will auto-expand, increases in the number of
  // channels available from the stack must be negotiated!
  // Accessed from both main and sts, API is threadsafe
  Channels mChannels;
  // STS only
  uint32_t mCurrentStream = 0;
  nsRefPtrDeque<DataChannel> mPending;
  // STS and main
  size_t mNegotiatedIdLimit MOZ_GUARDED_BY(mLock) = 0;
  PendingType mPendingType MOZ_GUARDED_BY(mLock) = PendingType::None;
  // holds data that's come in before a channel is open
  nsTArray<UniquePtr<QueuedDataMessage>> mQueuedData MOZ_GUARDED_BY(mLock);
  // holds outgoing control messages
  nsTArray<UniquePtr<BufferedOutgoingMsg>> mBufferedControl
      MOZ_GUARDED_BY(mLock);

  // Streams pending reset. Accessed from main and STS.
  AutoTArray<uint16_t, 4> mStreamsResetting MOZ_GUARDED_BY(mLock);
  // accessed from STS thread
  struct socket* mMasterSocket = nullptr;
  // cloned from mMasterSocket on successful Connect on STS thread
  struct socket* mSocket = nullptr;
  DataChannelConnectionState mState MOZ_GUARDED_BY(mLock) =
      DataChannelConnectionState::Closed;

#ifdef SCTP_DTLS_SUPPORTED
  std::string mTransportId;
  bool mConnectedToTransportHandler = false;
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
  uintptr_t mId = 0;
};

class DataChannel {
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnection;

 public:
  struct TrafficCounters {
    uint32_t mMessagesSent = 0;
    uint64_t mBytesSent = 0;
    uint32_t mMessagesReceived = 0;
    uint64_t mBytesReceived = 0;
  };

  DataChannel(DataChannelConnection* connection, uint16_t stream,
              DataChannelState state, const nsACString& label,
              const nsACString& protocol, DataChannelReliabilityPolicy policy,
              uint32_t value, bool ordered, bool negotiated,
              DataChannelListener* aListener, nsISupports* aContext)
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
        mIsRecvBinary(false),
        mBufferedThreshold(0),  // default from spec
        mBufferedAmount(0),
        mMainThreadEventTarget(connection->GetNeckoTarget()),
        mStatsLock("netwer::sctp::DataChannel::mStatsLock") {
    NS_ASSERTION(mConnection, "NULL connection");
  }
  DataChannel(const DataChannel&) = delete;
  DataChannel& operator=(const DataChannel&) = delete;

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

  DataChannelReliabilityPolicy GetType() const { return mPrPolicy; }

  dom::Nullable<uint16_t> GetMaxPacketLifeTime() const;

  dom::Nullable<uint16_t> GetMaxRetransmits() const;

  bool GetNegotiated() const { return mNegotiated; }

  bool GetOrdered() const { return mOrdered; }

  void IncrementBufferedAmount(uint32_t aSize, ErrorResult& aRv);
  void DecrementBufferedAmount(uint32_t aSize);

  // Amount of data buffered to send
  uint32_t GetBufferedAmount() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mBufferedAmount;
  }

  // Trigger amount for generating BufferedAmountLow events
  uint32_t GetBufferedAmountLowThreshold() const;
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);

  void AnnounceOpen();
  // TODO(bug 843625): Optionally pass an error here.
  void AnnounceClosed();

  // Find out state
  DataChannelState GetReadyState() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mReadyState;
  }

  // Set ready state
  void SetReadyState(DataChannelState aState);

  void GetLabel(nsAString& aLabel) { CopyUTF8toUTF16(mLabel, aLabel); }
  void GetProtocol(nsAString& aProtocol) {
    CopyUTF8toUTF16(mProtocol, aProtocol);
  }
  uint16_t GetStream() const { return mStream; }

  void SendOrQueue(DataChannelOnMessageAvailable* aMessage);

  TrafficCounters GetTrafficCounters() const;

 private:
  nsresult AddDataToBinaryMsg(const char* data, uint32_t size);
  bool EnsureValidStream(ErrorResult& aRv);
  void WithTrafficCounters(const std::function<void(TrafficCounters&)>&);

  // These are both mainthread only
  DataChannelListener* mListener;
  nsCOMPtr<nsISupports> mContext;

  RefPtr<DataChannelConnection> mConnection;
  // mainthread only
  bool mEverOpened = false;
  const nsCString mLabel;
  const nsCString mProtocol;
  // This is mainthread only
  DataChannelState mReadyState;
  uint16_t mStream;
  const DataChannelReliabilityPolicy mPrPolicy;
  const uint32_t mPrValue;
  // Accessed on main and STS
  const bool mNegotiated;
  const bool mOrdered;
  // The data channel has completed the open procedure and the client has been
  // notified about it.
  bool mHasFinishedOpen = false;
  // The channel has been opened, but the peer has not yet acked - ensures that
  // the messages are sent ordered until this is cleared.
  bool mWaitingForAck = false;
  // A too large message was attempted to be sent - closing data channel.
  bool mClosingTooLarge = false;
  bool mIsRecvBinary;
  size_t mBufferedThreshold;
  // Read/written on main only. Decremented via message-passing, because the
  // spec requires us to queue a task for this.
  size_t mBufferedAmount;
  nsCString mRecvBuffer;
  nsTArray<UniquePtr<BufferedOutgoingMsg>>
      mBufferedData;  // MOZ_GUARDED_BY(mConnection->mLock)
  nsCOMPtr<nsISerialEventTarget> mMainThreadEventTarget;
  mutable Mutex mStatsLock;
  TrafficCounters mTrafficCounters MOZ_GUARDED_BY(mStatsLock);
};

// used to dispatch notifications of incoming data to the main thread
// Patterned on CallOnMessageAvailable in WebSockets
// Also used to proxy other items to MainThread
class DataChannelOnMessageAvailable : public Runnable {
 public:
  enum class EventType {
    OnConnection,
    OnDisconnected,
    OnChannelCreated,
    OnDataString,
    OnDataBinary,
  };

  DataChannelOnMessageAvailable(
      EventType aType, DataChannelConnection* aConnection,
      DataChannel* aChannel,
      nsCString& aData)  // XXX this causes inefficiency
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection),
        mData(aData) {}

  DataChannelOnMessageAvailable(EventType aType, DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel) {}
  // XXX is it safe to leave mData uninitialized?  This should only be
  // used for notifications that don't use them, but I'd like more
  // bulletproof compile-time checking.

  DataChannelOnMessageAvailable(EventType aType,
                                DataChannelConnection* aConnection,
                                DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection) {}

  // for ON_CONNECTION/ON_DISCONNECTED
  DataChannelOnMessageAvailable(EventType aType,
                                DataChannelConnection* aConnection)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mConnection(aConnection) {}
  DataChannelOnMessageAvailable(const DataChannelOnMessageAvailable&) = delete;
  DataChannelOnMessageAvailable& operator=(
      const DataChannelOnMessageAvailable&) = delete;

  NS_IMETHOD Run() override;

 private:
  ~DataChannelOnMessageAvailable() = default;

  EventType mType;
  // XXX should use union
  RefPtr<DataChannel> mChannel;
  RefPtr<DataChannelConnection> mConnection;
  nsCString mData;
};

}  // namespace mozilla

#endif  // NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
