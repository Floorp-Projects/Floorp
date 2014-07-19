/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketChannel.h"

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Endian.h"
#include "mozilla/MathAlgorithms.h"

#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsICryptoHash.h"
#include "nsIRunnable.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsICancelable.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsIStreamConverterService.h"
#include "nsIIOService2.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyInfo.h"
#include "nsIProxiedChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIDashboardEventNotifier.h"
#include "nsIEventTarget.h"
#include "nsIHttpChannel.h"
#include "nsILoadGroup.h"
#include "nsIProtocolHandler.h"
#include "nsIRandomGenerator.h"
#include "nsISocketTransport.h"
#include "nsThreadUtils.h"

#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsError.h"
#include "nsStringStream.h"
#include "nsAlgorithm.h"
#include "nsProxyRelease.h"
#include "nsNetUtil.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

#include "plbase64.h"
#include "prmem.h"
#include "prnetdb.h"
#include "zlib.h"
#include <algorithm>

#ifdef MOZ_WIDGET_GONK
#include "NetStatistics.h"
#endif

// rather than slurp up all of nsIWebSocket.idl, which lives outside necko, just
// dupe one constant we need from it
#define CLOSE_GOING_AWAY 1001

extern PRThread *gSocketThread;

using namespace mozilla;
using namespace mozilla::net;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(WebSocketChannel,
                  nsIWebSocketChannel,
                  nsIHttpUpgradeListener,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIProtocolHandler,
                  nsIInputStreamCallback,
                  nsIOutputStreamCallback,
                  nsITimerCallback,
                  nsIDNSListener,
                  nsIProtocolProxyCallback,
                  nsIInterfaceRequestor,
                  nsIChannelEventSink,
                  nsIThreadRetargetableRequest)

// We implement RFC 6455, which uses Sec-WebSocket-Version: 13 on the wire.
#define SEC_WEBSOCKET_VERSION "13"

/*
 * About SSL unsigned certificates
 *
 * wss will not work to a host using an unsigned certificate unless there
 * is already an exception (i.e. it cannot popup a dialog asking for
 * a security exception). This is similar to how an inlined img will
 * fail without a dialog if fails for the same reason. This should not
 * be a problem in practice as it is expected the websocket javascript
 * is served from the same host as the websocket server (or of course,
 * a valid cert could just be provided).
 *
 */

// some helper classes

//-----------------------------------------------------------------------------
// FailDelayManager
//
// Stores entries (searchable by {host, port}) of connections that have recently
// failed, so we can do delay of reconnects per RFC 6455 Section 7.2.3
//-----------------------------------------------------------------------------


// Initial reconnect delay is randomly chosen between 200-400 ms.
// This is a gentler backoff than the 0-5 seconds the spec offhandedly suggests.
const uint32_t kWSReconnectInitialBaseDelay     = 200;
const uint32_t kWSReconnectInitialRandomDelay   = 200;

// Base lifetime (in ms) of a FailDelay: kept longer if more failures occur
const uint32_t kWSReconnectBaseLifeTime         = 60 * 1000;
// Maximum reconnect delay (in ms)
const uint32_t kWSReconnectMaxDelay             = 60 * 1000;

// hold record of failed connections, and calculates needed delay for reconnects
// to same host/port.
class FailDelay
{
public:
  FailDelay(nsCString address, int32_t port)
    : mAddress(address), mPort(port)
  {
    mLastFailure = TimeStamp::Now();
    mNextDelay = kWSReconnectInitialBaseDelay +
                 (rand() % kWSReconnectInitialRandomDelay);
  }

  // Called to update settings when connection fails again.
  void FailedAgain()
  {
    mLastFailure = TimeStamp::Now();
    // We use a truncated exponential backoff as suggested by RFC 6455,
    // but multiply by 1.5 instead of 2 to be more gradual.
    mNextDelay = static_cast<uint32_t>(
      std::min<double>(kWSReconnectMaxDelay, mNextDelay * 1.5));
    LOG(("WebSocket: FailedAgain: host=%s, port=%d: incremented delay to %lu",
         mAddress.get(), mPort, mNextDelay));
  }

  // returns 0 if there is no need to delay (i.e. delay interval is over)
  uint32_t RemainingDelay(TimeStamp rightNow)
  {
    TimeDuration dur = rightNow - mLastFailure;
    uint32_t sinceFail = (uint32_t) dur.ToMilliseconds();
    if (sinceFail > mNextDelay)
      return 0;

    return mNextDelay - sinceFail;
  }

  bool IsExpired(TimeStamp rightNow)
  {
    return (mLastFailure +
            TimeDuration::FromMilliseconds(kWSReconnectBaseLifeTime + mNextDelay))
            <= rightNow;
  }

  nsCString  mAddress;     // IP address (or hostname if using proxy)
  int32_t    mPort;

private:
  TimeStamp  mLastFailure; // Time of last failed attempt
  // mLastFailure + mNextDelay is the soonest we'll allow a reconnect
  uint32_t   mNextDelay;   // milliseconds
};

class FailDelayManager
{
public:
  FailDelayManager()
  {
    MOZ_COUNT_CTOR(FailDelayManager);

    mDelaysDisabled = false;

    nsCOMPtr<nsIPrefBranch> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
    bool boolpref = true;
    nsresult rv;
    rv = prefService->GetBoolPref("network.websocket.delay-failed-reconnects",
                                  &boolpref);
    if (NS_SUCCEEDED(rv) && !boolpref) {
      mDelaysDisabled = true;
    }
  }

  ~FailDelayManager()
  {
    MOZ_COUNT_DTOR(FailDelayManager);
    for (uint32_t i = 0; i < mEntries.Length(); i++) {
      delete mEntries[i];
    }
  }

  void Add(nsCString &address, int32_t port)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    if (mDelaysDisabled)
      return;

    FailDelay *record = new FailDelay(address, port);
    mEntries.AppendElement(record);
  }

  // Element returned may not be valid after next main thread event: don't keep
  // pointer to it around
  FailDelay* Lookup(nsCString &address, int32_t port,
                    uint32_t *outIndex = nullptr)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    if (mDelaysDisabled)
      return nullptr;

    FailDelay *result = nullptr;
    TimeStamp rightNow = TimeStamp::Now();

    // We also remove expired entries during search: iterate from end to make
    // indexing simpler
    for (int32_t i = mEntries.Length() - 1; i >= 0; --i) {
      FailDelay *fail = mEntries[i];
      if (fail->mAddress.Equals(address) && fail->mPort == port) {
        if (outIndex)
          *outIndex = i;
        result = fail;
        // break here: removing more entries would mess up *outIndex.
        // Any remaining expired entries will be deleted next time Lookup
        // finds nothing, which is the most common case anyway.
        break;
      } else if (fail->IsExpired(rightNow)) {
        mEntries.RemoveElementAt(i);
        delete fail;
      }
    }
    return result;
  }

  // returns true if channel connects immediately, or false if it's delayed
  void DelayOrBegin(WebSocketChannel *ws)
  {
    if (!mDelaysDisabled) {
      uint32_t failIndex = 0;
      FailDelay *fail = Lookup(ws->mAddress, ws->mPort, &failIndex);

      if (fail) {
        TimeStamp rightNow = TimeStamp::Now();

        uint32_t remainingDelay = fail->RemainingDelay(rightNow);
        if (remainingDelay) {
          // reconnecting within delay interval: delay by remaining time
          nsresult rv;
          ws->mReconnectDelayTimer =
            do_CreateInstance("@mozilla.org/timer;1", &rv);
          if (NS_SUCCEEDED(rv)) {
            rv = ws->mReconnectDelayTimer->InitWithCallback(
                          ws, remainingDelay, nsITimer::TYPE_ONE_SHOT);
            if (NS_SUCCEEDED(rv)) {
              LOG(("WebSocket: delaying websocket [this=%p] by %lu ms",
                   ws, (unsigned long)remainingDelay));
              ws->mConnecting = CONNECTING_DELAYED;
              return;
            }
          }
          // if timer fails (which is very unlikely), drop down to BeginOpen call
        } else if (fail->IsExpired(rightNow)) {
          mEntries.RemoveElementAt(failIndex);
          delete fail;
        }
      }
    }

    // Delays disabled, or no previous failure, or we're reconnecting after scheduled
    // delay interval has passed: connect.
    ws->BeginOpen();
  }

  // Remove() also deletes all expired entries as it iterates: better for
  // battery life than using a periodic timer.
  void Remove(nsCString &address, int32_t port)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    TimeStamp rightNow = TimeStamp::Now();

    // iterate from end, to make deletion indexing easier
    for (int32_t i = mEntries.Length() - 1; i >= 0; --i) {
      FailDelay *entry = mEntries[i];
      if ((entry->mAddress.Equals(address) && entry->mPort == port) ||
          entry->IsExpired(rightNow)) {
        mEntries.RemoveElementAt(i);
        delete entry;
      }
    }
  }

private:
  nsTArray<FailDelay *> mEntries;
  bool                  mDelaysDisabled;
};

//-----------------------------------------------------------------------------
// nsWSAdmissionManager
//
// 1) Ensures that only one websocket at a time is CONNECTING to a given IP
//    address (or hostname, if using proxy), per RFC 6455 Section 4.1.
// 2) Delays reconnects to IP/host after connection failure, per Section 7.2.3
//-----------------------------------------------------------------------------

class nsWSAdmissionManager
{
public:
  static void Init()
  {
    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      sManager = new nsWSAdmissionManager();
    }
  }

  static void Shutdown()
  {
    StaticMutexAutoLock lock(sLock);
    delete sManager;
    sManager = nullptr;
  }

  // Determine if we will open connection immediately (returns true), or
  // delay/queue the connection (returns false)
  static void ConditionallyConnect(WebSocketChannel *ws)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
    NS_ABORT_IF_FALSE(ws->mConnecting == NOT_CONNECTING, "opening state");

    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }

    // If there is already another WS channel connecting to this IP address,
    // defer BeginOpen and mark as waiting in queue.
    bool found = (sManager->IndexOf(ws->mAddress) >= 0);

    // Always add ourselves to queue, even if we'll connect immediately
    nsOpenConn *newdata = new nsOpenConn(ws->mAddress, ws);
    sManager->mQueue.AppendElement(newdata);

    if (found) {
      ws->mConnecting = CONNECTING_QUEUED;
    } else {
      sManager->mFailures.DelayOrBegin(ws);
    }
  }

  static void OnConnected(WebSocketChannel *aChannel)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
    NS_ABORT_IF_FALSE(aChannel->mConnecting == CONNECTING_IN_PROGRESS,
                      "Channel completed connect, but not connecting?");

    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }

    aChannel->mConnecting = NOT_CONNECTING;

    // Remove from queue
    sManager->RemoveFromQueue(aChannel);

    // Connection succeeded, so stop keeping track of any previous failures
    sManager->mFailures.Remove(aChannel->mAddress, aChannel->mPort);

    // Check for queued connections to same host.
    // Note: still need to check for failures, since next websocket with same
    // host may have different port
    sManager->ConnectNext(aChannel->mAddress);
  }

  // Called every time a websocket channel ends its session (including going away
  // w/o ever successfully creating a connection)
  static void OnStopSession(WebSocketChannel *aChannel, nsresult aReason)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }

    if (NS_FAILED(aReason)) {
      // Have we seen this failure before?
      FailDelay *knownFailure = sManager->mFailures.Lookup(aChannel->mAddress,
                                                           aChannel->mPort);
      if (knownFailure) {
        if (aReason == NS_ERROR_NOT_CONNECTED) {
          // Don't count close() before connection as a network error
          LOG(("Websocket close() before connection to %s, %d completed"
               " [this=%p]", aChannel->mAddress.get(), (int)aChannel->mPort,
               aChannel));
        } else {
          // repeated failure to connect: increase delay for next connection
          knownFailure->FailedAgain();
        }
      } else {
        // new connection failure: record it.
        LOG(("WebSocket: connection to %s, %d failed: [this=%p]",
              aChannel->mAddress.get(), (int)aChannel->mPort, aChannel));
        sManager->mFailures.Add(aChannel->mAddress, aChannel->mPort);
      }
    }

    if (aChannel->mConnecting) {
      // Only way a connecting channel may get here w/o failing is if it was
      // closed with GOING_AWAY (1001) because of navigation, tab close, etc.
      NS_ABORT_IF_FALSE(NS_FAILED(aReason) ||
                        aChannel->mScriptCloseCode == CLOSE_GOING_AWAY,
                        "websocket closed while connecting w/o failing?");

      sManager->RemoveFromQueue(aChannel);

      bool wasNotQueued = (aChannel->mConnecting != CONNECTING_QUEUED);
      aChannel->mConnecting = NOT_CONNECTING;
      if (wasNotQueued) {
        sManager->ConnectNext(aChannel->mAddress);
      }
    }
  }

  static void IncrementSessionCount()
  {
    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }
    sManager->mSessionCount++;
  }

  static void DecrementSessionCount()
  {
    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }
    sManager->mSessionCount--;
  }

  static void GetSessionCount(int32_t &aSessionCount)
  {
    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }
    aSessionCount = sManager->mSessionCount;
  }

private:
  nsWSAdmissionManager() : mSessionCount(0)
  {
    MOZ_COUNT_CTOR(nsWSAdmissionManager);
  }

  ~nsWSAdmissionManager()
  {
    MOZ_COUNT_DTOR(nsWSAdmissionManager);
    for (uint32_t i = 0; i < mQueue.Length(); i++)
      delete mQueue[i];
  }

  class nsOpenConn
  {
  public:
    nsOpenConn(nsCString &addr, WebSocketChannel *channel)
      : mAddress(addr), mChannel(channel) { MOZ_COUNT_CTOR(nsOpenConn); }
    ~nsOpenConn() { MOZ_COUNT_DTOR(nsOpenConn); }

    nsCString mAddress;
    WebSocketChannel *mChannel;
  };

  void ConnectNext(nsCString &hostName)
  {
    int32_t index = IndexOf(hostName);
    if (index >= 0) {
      WebSocketChannel *chan = mQueue[index]->mChannel;

      NS_ABORT_IF_FALSE(chan->mConnecting == CONNECTING_QUEUED,
                        "transaction not queued but in queue");
      LOG(("WebSocket: ConnectNext: found channel [this=%p] in queue", chan));

      mFailures.DelayOrBegin(chan);
    }
  }

  void RemoveFromQueue(WebSocketChannel *aChannel)
  {
    int32_t index = IndexOf(aChannel);
    NS_ABORT_IF_FALSE(index >= 0, "connection to remove not in queue");
    if (index >= 0) {
      nsOpenConn *olddata = mQueue[index];
      mQueue.RemoveElementAt(index);
      delete olddata;
    }
  }

  int32_t IndexOf(nsCString &aStr)
  {
    for (uint32_t i = 0; i < mQueue.Length(); i++)
      if (aStr == (mQueue[i])->mAddress)
        return i;
    return -1;
  }

  int32_t IndexOf(WebSocketChannel *aChannel)
  {
    for (uint32_t i = 0; i < mQueue.Length(); i++)
      if (aChannel == (mQueue[i])->mChannel)
        return i;
    return -1;
  }

  // SessionCount might be decremented from the main or the socket
  // thread, so manage it with atomic counters
  Atomic<int32_t>               mSessionCount;

  // Queue for websockets that have not completed connecting yet.
  // The first nsOpenConn with a given address will be either be
  // CONNECTING_IN_PROGRESS or CONNECTING_DELAYED.  Later ones with the same
  // hostname must be CONNECTING_QUEUED.
  //
  // We could hash hostnames instead of using a single big vector here, but the
  // dataset is expected to be small.
  nsTArray<nsOpenConn *> mQueue;

  FailDelayManager       mFailures;

  static nsWSAdmissionManager *sManager;
  static StaticMutex           sLock;
};

nsWSAdmissionManager *nsWSAdmissionManager::sManager;
StaticMutex           nsWSAdmissionManager::sLock;

//-----------------------------------------------------------------------------
// CallOnMessageAvailable
//-----------------------------------------------------------------------------

class CallOnMessageAvailable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnMessageAvailable(WebSocketChannel *aChannel,
                         nsCString        &aData,
                         int32_t           aLen)
    : mChannel(aChannel),
      mData(aData),
      mLen(aLen) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mChannel->mTargetThread);

    if (mLen < 0)
      mChannel->mListener->OnMessageAvailable(mChannel->mContext, mData);
    else
      mChannel->mListener->OnBinaryMessageAvailable(mChannel->mContext, mData);
    return NS_OK;
  }

private:
  ~CallOnMessageAvailable() {}

  nsRefPtr<WebSocketChannel>        mChannel;
  nsCString                         mData;
  int32_t                           mLen;
};
NS_IMPL_ISUPPORTS(CallOnMessageAvailable, nsIRunnable)

//-----------------------------------------------------------------------------
// CallOnStop
//-----------------------------------------------------------------------------

class CallOnStop MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnStop(WebSocketChannel *aChannel,
             nsresult          aReason)
    : mChannel(aChannel),
      mReason(aReason) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mChannel->mTargetThread);

    nsWSAdmissionManager::OnStopSession(mChannel, mReason);

    if (mChannel->mListener) {
      mChannel->mListener->OnStop(mChannel->mContext, mReason);
      mChannel->mListener = nullptr;
      mChannel->mContext = nullptr;
    }
    return NS_OK;
  }

private:
  ~CallOnStop() {}

  nsRefPtr<WebSocketChannel>        mChannel;
  nsresult                          mReason;
};
NS_IMPL_ISUPPORTS(CallOnStop, nsIRunnable)

//-----------------------------------------------------------------------------
// CallOnServerClose
//-----------------------------------------------------------------------------

class CallOnServerClose MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnServerClose(WebSocketChannel *aChannel,
                    uint16_t          aCode,
                    nsCString        &aReason)
    : mChannel(aChannel),
      mCode(aCode),
      mReason(aReason) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mChannel->mTargetThread);

    mChannel->mListener->OnServerClose(mChannel->mContext, mCode, mReason);
    return NS_OK;
  }

private:
  ~CallOnServerClose() {}

  nsRefPtr<WebSocketChannel>        mChannel;
  uint16_t                          mCode;
  nsCString                         mReason;
};
NS_IMPL_ISUPPORTS(CallOnServerClose, nsIRunnable)

//-----------------------------------------------------------------------------
// CallAcknowledge
//-----------------------------------------------------------------------------

class CallAcknowledge MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallAcknowledge(WebSocketChannel *aChannel,
                  uint32_t          aSize)
    : mChannel(aChannel),
      mSize(aSize) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mChannel->mTargetThread);

    LOG(("WebSocketChannel::CallAcknowledge: Size %u\n", mSize));
    mChannel->mListener->OnAcknowledge(mChannel->mContext, mSize);
    return NS_OK;
  }

private:
  ~CallAcknowledge() {}

  nsRefPtr<WebSocketChannel>        mChannel;
  uint32_t                          mSize;
};
NS_IMPL_ISUPPORTS(CallAcknowledge, nsIRunnable)

//-----------------------------------------------------------------------------
// CallOnTransportAvailable
//-----------------------------------------------------------------------------

class CallOnTransportAvailable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnTransportAvailable(WebSocketChannel *aChannel,
                           nsISocketTransport *aTransport,
                           nsIAsyncInputStream *aSocketIn,
                           nsIAsyncOutputStream *aSocketOut)
    : mChannel(aChannel),
      mTransport(aTransport),
      mSocketIn(aSocketIn),
      mSocketOut(aSocketOut) {}

  NS_IMETHOD Run()
  {
    LOG(("WebSocketChannel::CallOnTransportAvailable %p\n", this));
    return mChannel->OnTransportAvailable(mTransport, mSocketIn, mSocketOut);
  }

private:
  ~CallOnTransportAvailable() {}

  nsRefPtr<WebSocketChannel>     mChannel;
  nsCOMPtr<nsISocketTransport>   mTransport;
  nsCOMPtr<nsIAsyncInputStream>  mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
};
NS_IMPL_ISUPPORTS(CallOnTransportAvailable, nsIRunnable)

//-----------------------------------------------------------------------------
// OutboundMessage
//-----------------------------------------------------------------------------

enum WsMsgType {
  kMsgTypeString = 0,
  kMsgTypeBinaryString,
  kMsgTypeStream,
  kMsgTypePing,
  kMsgTypePong,
  kMsgTypeFin
};

static const char* msgNames[] = {
  "text",
  "binaryString",
  "binaryStream",
  "ping",
  "pong",
  "close"
};

class OutboundMessage
{
public:
  OutboundMessage(WsMsgType type, nsCString *str)
    : mMsgType(type)
  {
    MOZ_COUNT_CTOR(OutboundMessage);
    mMsg.pString = str;
    mLength = str ? str->Length() : 0;
  }

  OutboundMessage(nsIInputStream *stream, uint32_t length)
    : mMsgType(kMsgTypeStream), mLength(length)
  {
    MOZ_COUNT_CTOR(OutboundMessage);
    mMsg.pStream = stream;
    mMsg.pStream->AddRef();
  }

 ~OutboundMessage() {
    MOZ_COUNT_DTOR(OutboundMessage);
    switch (mMsgType) {
      case kMsgTypeString:
      case kMsgTypeBinaryString:
      case kMsgTypePing:
      case kMsgTypePong:
        delete mMsg.pString;
        break;
      case kMsgTypeStream:
        // for now this only gets hit if msg deleted w/o being sent
        if (mMsg.pStream) {
          mMsg.pStream->Close();
          mMsg.pStream->Release();
        }
        break;
      case kMsgTypeFin:
        break;    // do-nothing: avoid compiler warning
    }
  }

  WsMsgType GetMsgType() const { return mMsgType; }
  int32_t Length() const { return mLength; }

  uint8_t* BeginWriting() {
    NS_ABORT_IF_FALSE(mMsgType != kMsgTypeStream,
                      "Stream should have been converted to string by now");
    return (uint8_t *)(mMsg.pString ? mMsg.pString->BeginWriting() : nullptr);
  }

  uint8_t* BeginReading() {
    NS_ABORT_IF_FALSE(mMsgType != kMsgTypeStream,
                      "Stream should have been converted to string by now");
    return (uint8_t *)(mMsg.pString ? mMsg.pString->BeginReading() : nullptr);
  }

  nsresult ConvertStreamToString()
  {
    NS_ABORT_IF_FALSE(mMsgType == kMsgTypeStream, "Not a stream!");

#ifdef DEBUG
    // Make sure we got correct length from Blob
    uint64_t bytes;
    mMsg.pStream->Available(&bytes);
    NS_ASSERTION(bytes == mLength, "Stream length != blob length!");
#endif

    nsAutoPtr<nsCString> temp(new nsCString());
    nsresult rv = NS_ReadInputStreamToString(mMsg.pStream, *temp, mLength);

    NS_ENSURE_SUCCESS(rv, rv);

    mMsg.pStream->Close();
    mMsg.pStream->Release();
    mMsg.pString = temp.forget();
    mMsgType = kMsgTypeBinaryString;

    return NS_OK;
  }

private:
  union {
    nsCString      *pString;
    nsIInputStream *pStream;
  }                           mMsg;
  WsMsgType                   mMsgType;
  uint32_t                    mLength;
};

//-----------------------------------------------------------------------------
// OutboundEnqueuer
//-----------------------------------------------------------------------------

class OutboundEnqueuer MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  OutboundEnqueuer(WebSocketChannel *aChannel, OutboundMessage *aMsg)
    : mChannel(aChannel), mMessage(aMsg) {}

  NS_IMETHOD Run()
  {
    mChannel->EnqueueOutgoingMessage(mChannel->mOutgoingMessages, mMessage);
    return NS_OK;
  }

private:
  ~OutboundEnqueuer() {}

  nsRefPtr<WebSocketChannel>  mChannel;
  OutboundMessage            *mMessage;
};
NS_IMPL_ISUPPORTS(OutboundEnqueuer, nsIRunnable)

//-----------------------------------------------------------------------------
// nsWSCompression
//
// similar to nsDeflateConverter except for the mandatory FLUSH calls
// required by websocket and the absence of the deflate termination
// block which is appropriate because it would create data bytes after
// sending the websockets CLOSE message.
//-----------------------------------------------------------------------------

class nsWSCompression
{
public:
  nsWSCompression(nsIStreamListener *aListener,
                  nsISupports *aContext)
    : mActive(false),
      mContext(aContext),
      mListener(aListener)
  {
    MOZ_COUNT_CTOR(nsWSCompression);

    mZlib.zalloc = allocator;
    mZlib.zfree = destructor;
    mZlib.opaque = Z_NULL;

    // Initialize the compressor - these are all the normal zlib
    // defaults except window size is set to -15 instead of +15.
    // This is the zlib way of specifying raw RFC 1951 output instead
    // of the zlib rfc 1950 format which has a 2 byte header and
    // adler checksum as a trailer

    nsresult rv;
    mStream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && aContext && aListener &&
      deflateInit2(&mZlib, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                   Z_DEFAULT_STRATEGY) == Z_OK) {
      mActive = true;
    }
  }

  ~nsWSCompression()
  {
    MOZ_COUNT_DTOR(nsWSCompression);

    if (mActive)
      deflateEnd(&mZlib);
  }

  bool Active()
  {
    return mActive;
  }

  nsresult Deflate(uint8_t *buf1, uint32_t buf1Len,
                   uint8_t *buf2, uint32_t buf2Len)
  {
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread,
                          "not socket thread");
    NS_ABORT_IF_FALSE(mActive, "not active");

    mZlib.avail_out = kBufferLen;
    mZlib.next_out = mBuffer;
    mZlib.avail_in = buf1Len;
    mZlib.next_in = buf1;

    nsresult rv;

    while (mZlib.avail_in > 0) {
      deflate(&mZlib, (buf2Len > 0) ? Z_NO_FLUSH : Z_SYNC_FLUSH);
      rv = PushData();
      if (NS_FAILED(rv))
        return rv;
      mZlib.avail_out = kBufferLen;
      mZlib.next_out = mBuffer;
    }

    mZlib.avail_in = buf2Len;
    mZlib.next_in = buf2;

    while (mZlib.avail_in > 0) {
      deflate(&mZlib, Z_SYNC_FLUSH);
      rv = PushData();
      if (NS_FAILED(rv))
        return rv;
      mZlib.avail_out = kBufferLen;
      mZlib.next_out = mBuffer;
    }

    return NS_OK;
  }

private:

  // use zlib data types
  static void *allocator(void *opaque, uInt items, uInt size)
  {
    return moz_xmalloc(items * size);
  }

  static void destructor(void *opaque, void *addr)
  {
    moz_free(addr);
  }

  nsresult PushData()
  {
    uint32_t bytesToWrite = kBufferLen - mZlib.avail_out;
    if (bytesToWrite > 0) {
      mStream->ShareData(reinterpret_cast<char *>(mBuffer), bytesToWrite);
      nsresult rv =
        mListener->OnDataAvailable(nullptr, mContext, mStream, 0, bytesToWrite);
      if (NS_FAILED(rv))
        return rv;
    }
    return NS_OK;
  }

  bool                            mActive;
  z_stream                        mZlib;
  nsCOMPtr<nsIStringInputStream>  mStream;

  nsISupports                    *mContext;     /* weak ref */
  nsIStreamListener              *mListener;    /* weak ref */

  const static int32_t            kBufferLen = 4096;
  uint8_t                         mBuffer[kBufferLen];
};

//-----------------------------------------------------------------------------
// WebSocketChannel
//-----------------------------------------------------------------------------

uint32_t WebSocketChannel::sSerialSeed = 0;

WebSocketChannel::WebSocketChannel() :
  mPort(0),
  mCloseTimeout(20000),
  mOpenTimeout(20000),
  mConnecting(NOT_CONNECTING),
  mMaxConcurrentConnections(200),
  mGotUpgradeOK(0),
  mRecvdHttpUpgradeTransport(0),
  mRequestedClose(0),
  mClientClosed(0),
  mServerClosed(0),
  mStopped(0),
  mCalledOnStop(0),
  mPingOutstanding(0),
  mAllowCompression(1),
  mAutoFollowRedirects(0),
  mReleaseOnTransmit(0),
  mTCPClosed(0),
  mOpenedHttpChannel(0),
  mDataStarted(0),
  mIncrementedSessionCount(0),
  mDecrementedSessionCount(0),
  mMaxMessageSize(INT32_MAX),
  mStopOnClose(NS_OK),
  mServerCloseCode(CLOSE_ABNORMAL),
  mScriptCloseCode(0),
  mFragmentOpcode(kContinuation),
  mFragmentAccumulator(0),
  mBuffered(0),
  mBufferSize(kIncomingBufferInitialSize),
  mCurrentOut(nullptr),
  mCurrentOutSent(0),
  mCompressor(nullptr),
  mDynamicOutputSize(0),
  mDynamicOutput(nullptr),
  mPrivateBrowsing(false),
  mConnectionLogService(nullptr),
  mCountRecv(0),
  mCountSent(0),
  mAppId(NECKO_NO_APP_ID)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannel::WebSocketChannel() %p\n", this));

  nsWSAdmissionManager::Init();

  mFramePtr = mBuffer = static_cast<uint8_t *>(moz_xmalloc(mBufferSize));

  nsresult rv;
  mConnectionLogService = do_GetService("@mozilla.org/network/dashboard;1",&rv);
  if (NS_FAILED(rv))
    LOG(("Failed to initiate dashboard service."));

  mSerial = sSerialSeed++;
}

WebSocketChannel::~WebSocketChannel()
{
  LOG(("WebSocketChannel::~WebSocketChannel() %p\n", this));

  if (mWasOpened) {
    MOZ_ASSERT(mCalledOnStop, "WebSocket was opened but OnStop was not called");
    MOZ_ASSERT(mStopped, "WebSocket was opened but never stopped");
  }
  MOZ_ASSERT(!mCancelable, "DNS/Proxy Request still alive at destruction");
  MOZ_ASSERT(!mConnecting, "Should not be connecting in destructor");

  moz_free(mBuffer);
  moz_free(mDynamicOutput);
  delete mCompressor;
  delete mCurrentOut;

  while ((mCurrentOut = (OutboundMessage *) mOutgoingPingMessages.PopFront()))
    delete mCurrentOut;
  while ((mCurrentOut = (OutboundMessage *) mOutgoingPongMessages.PopFront()))
    delete mCurrentOut;
  while ((mCurrentOut = (OutboundMessage *) mOutgoingMessages.PopFront()))
    delete mCurrentOut;

  nsCOMPtr<nsIThread> mainThread;
  nsIURI *forgettable;
  NS_GetMainThread(getter_AddRefs(mainThread));

  if (mURI) {
    mURI.forget(&forgettable);
    NS_ProxyRelease(mainThread, forgettable, false);
  }

  if (mOriginalURI) {
    mOriginalURI.forget(&forgettable);
    NS_ProxyRelease(mainThread, forgettable, false);
  }

  if (mListener) {
    nsIWebSocketListener *forgettableListener;
    mListener.forget(&forgettableListener);
    NS_ProxyRelease(mainThread, forgettableListener, false);
  }

  if (mContext) {
    nsISupports *forgettableContext;
    mContext.forget(&forgettableContext);
    NS_ProxyRelease(mainThread, forgettableContext, false);
  }

  if (mLoadGroup) {
    nsILoadGroup *forgettableGroup;
    mLoadGroup.forget(&forgettableGroup);
    NS_ProxyRelease(mainThread, forgettableGroup, false);
  }
}

void
WebSocketChannel::Shutdown()
{
  nsWSAdmissionManager::Shutdown();
}

void
WebSocketChannel::BeginOpen()
{
  LOG(("WebSocketChannel::BeginOpen() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  nsresult rv;

  // Important that we set CONNECTING_IN_PROGRESS before any call to
  // AbortSession here: ensures that any remaining queued connection(s) are
  // scheduled in OnStopSession
  mConnecting = CONNECTING_IN_PROGRESS;

  if (mRedirectCallback) {
    LOG(("WebSocketChannel::BeginOpen: Resuming Redirect\n"));
    rv = mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
    mRedirectCallback = nullptr;
    return;
  }

  nsCOMPtr<nsIChannel> localChannel = do_QueryInterface(mChannel, &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot async open\n"));
    AbortSession(NS_ERROR_UNEXPECTED);
    return;
  }

  if (localChannel) {
    bool isInBrowser;
    NS_GetAppInfo(localChannel, &mAppId, &isInBrowser);
  }

#ifdef MOZ_WIDGET_GONK
  if (mAppId != NECKO_NO_APP_ID) {
    nsCOMPtr<nsINetworkInterface> activeNetwork;
    GetActiveNetworkInterface(activeNetwork);
    mActiveNetwork =
      new nsMainThreadPtrHolder<nsINetworkInterface>(activeNetwork);
  }
#endif

  rv = localChannel->AsyncOpen(this, mHttpChannel);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot async open\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return;
  }
  mOpenedHttpChannel = 1;

  mOpenTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot create open timer\n"));
    AbortSession(NS_ERROR_UNEXPECTED);
    return;
  }

  rv = mOpenTimer->InitWithCallback(this, mOpenTimeout,
                                    nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot initialize open timer\n"));
    AbortSession(NS_ERROR_UNEXPECTED);
    return;
  }
}

bool
WebSocketChannel::IsPersistentFramePtr()
{
  return (mFramePtr >= mBuffer && mFramePtr < mBuffer + mBufferSize);
}

// Extends the internal buffer by count and returns the total
// amount of data available for read
//
// Accumulated fragment size is passed in instead of using the member
// variable beacuse when transitioning from the stack to the persistent
// read buffer we want to explicitly include them in the buffer instead
// of as already existing data.
bool
WebSocketChannel::UpdateReadBuffer(uint8_t *buffer, uint32_t count,
                                   uint32_t accumulatedFragments,
                                   uint32_t *available)
{
  LOG(("WebSocketChannel::UpdateReadBuffer() %p [%p %u]\n",
         this, buffer, count));

  if (!mBuffered)
    mFramePtr = mBuffer;

  NS_ABORT_IF_FALSE(IsPersistentFramePtr(), "update read buffer bad mFramePtr");
  NS_ABORT_IF_FALSE(mFramePtr - accumulatedFragments >= mBuffer,
                    "reserved FramePtr bad");

  if (mBuffered + count <= mBufferSize) {
    // append to existing buffer
    LOG(("WebSocketChannel: update read buffer absorbed %u\n", count));
  } else if (mBuffered + count - 
             (mFramePtr - accumulatedFragments - mBuffer) <= mBufferSize) {
    // make room in existing buffer by shifting unused data to start
    mBuffered -= (mFramePtr - mBuffer - accumulatedFragments);
    LOG(("WebSocketChannel: update read buffer shifted %u\n", mBuffered));
    ::memmove(mBuffer, mFramePtr - accumulatedFragments, mBuffered);
    mFramePtr = mBuffer + accumulatedFragments;
  } else {
    // existing buffer is not sufficient, extend it
    mBufferSize += count + 8192 + mBufferSize/3;
    LOG(("WebSocketChannel: update read buffer extended to %u\n", mBufferSize));
    uint8_t *old = mBuffer;
    mBuffer = (uint8_t *)moz_realloc(mBuffer, mBufferSize);
    if (!mBuffer) {
      mBuffer = old;
      return false;
    }
    mFramePtr = mBuffer + (mFramePtr - old);
  }

  ::memcpy(mBuffer + mBuffered, buffer, count);
  mBuffered += count;

  if (available)
    *available = mBuffered - (mFramePtr - mBuffer);

  return true;
}

nsresult
WebSocketChannel::ProcessInput(uint8_t *buffer, uint32_t count)
{
  LOG(("WebSocketChannel::ProcessInput %p [%d %d]\n", this, count, mBuffered));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  // The purpose of ping/pong is to actively probe the peer so that an
  // unreachable peer is not mistaken for a period of idleness. This
  // implementation accepts any application level read activity as a sign of
  // life, it does not necessarily have to be a pong.
  ResetPingTimer();

  uint32_t avail;

  if (!mBuffered) {
    // Most of the time we can process right off the stack buffer without
    // having to accumulate anything
    mFramePtr = buffer;
    avail = count;
  } else {
    if (!UpdateReadBuffer(buffer, count, mFragmentAccumulator, &avail)) {
      return NS_ERROR_FILE_TOO_BIG;
    }
  }

  uint8_t *payload;
  uint32_t totalAvail = avail;

  while (avail >= 2) {
    int64_t payloadLength64 = mFramePtr[1] & 0x7F;
    uint8_t finBit  = mFramePtr[0] & kFinalFragBit;
    uint8_t rsvBits = mFramePtr[0] & 0x70;
    uint8_t maskBit = mFramePtr[1] & kMaskBit;
    uint8_t opcode  = mFramePtr[0] & 0x0F;

    uint32_t framingLength = 2;
    if (maskBit)
      framingLength += 4;

    if (payloadLength64 < 126) {
      if (avail < framingLength)
        break;
    } else if (payloadLength64 == 126) {
      // 16 bit length field
      framingLength += 2;
      if (avail < framingLength)
        break;

      payloadLength64 = mFramePtr[2] << 8 | mFramePtr[3];
    } else {
      // 64 bit length
      framingLength += 8;
      if (avail < framingLength)
        break;

      if (mFramePtr[2] & 0x80) {
        // Section 4.2 says that the most significant bit MUST be
        // 0. (i.e. this is really a 63 bit value)
        LOG(("WebSocketChannel:: high bit of 64 bit length set"));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      // copy this in case it is unaligned
      payloadLength64 = NetworkEndian::readInt64(mFramePtr + 2);
    }

    payload = mFramePtr + framingLength;
    avail -= framingLength;

    LOG(("WebSocketChannel::ProcessInput: payload %lld avail %lu\n",
         payloadLength64, avail));

    if (payloadLength64 + mFragmentAccumulator > mMaxMessageSize) {
      return NS_ERROR_FILE_TOO_BIG;
    }
    uint32_t payloadLength = static_cast<uint32_t>(payloadLength64);

    if (avail < payloadLength)
      break;

    LOG(("WebSocketChannel::ProcessInput: Frame accumulated - opcode %d\n",
         opcode));

    if (maskBit) {
      // This is unexpected - the server does not generally send masked
      // frames to the client, but it is allowed
      LOG(("WebSocketChannel:: Client RECEIVING masked frame."));

      uint32_t mask = NetworkEndian::readUint32(payload - 4);
      ApplyMask(mask, payload, payloadLength);
    }

    // Control codes are required to have the fin bit set
    if (!finBit && (opcode & kControlFrameMask)) {
      LOG(("WebSocketChannel:: fragmented control frame code %d\n", opcode));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (rsvBits) {
      LOG(("WebSocketChannel:: unexpected reserved bits %x\n", rsvBits));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (!finBit || opcode == kContinuation) {
      // This is part of a fragment response

      // Only the first frame has a non zero op code: Make sure we don't see a
      // first frame while some old fragments are open
      if ((mFragmentAccumulator != 0) && (opcode != kContinuation)) {
        LOG(("WebSocketChannel:: nested fragments\n"));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      LOG(("WebSocketChannel:: Accumulating Fragment %ld\n", payloadLength));

      if (opcode == kContinuation) {

        // Make sure this continuation fragment isn't the first fragment
        if (mFragmentOpcode == kContinuation) {
          LOG(("WebSocketHeandler:: continuation code in first fragment\n"));
          return NS_ERROR_ILLEGAL_VALUE;
        }

        // For frag > 1 move the data body back on top of the headers
        // so we have contiguous stream of data
        NS_ABORT_IF_FALSE(mFramePtr + framingLength == payload,
                          "payload offset from frameptr wrong");
        ::memmove(mFramePtr, payload, avail);
        payload = mFramePtr;
        if (mBuffered)
          mBuffered -= framingLength;
      } else {
        mFragmentOpcode = opcode;
      }

      if (finBit) {
        LOG(("WebSocketChannel:: Finalizing Fragment\n"));
        payload -= mFragmentAccumulator;
        payloadLength += mFragmentAccumulator;
        avail += mFragmentAccumulator;
        mFragmentAccumulator = 0;
        opcode = mFragmentOpcode;
        // reset to detect if next message illegally starts with continuation
        mFragmentOpcode = kContinuation;
      } else {
        opcode = kContinuation;
        mFragmentAccumulator += payloadLength;
      }
    } else if (mFragmentAccumulator != 0 && !(opcode & kControlFrameMask)) {
      // This frame is not part of a fragment sequence but we
      // have an open fragment.. it must be a control code or else
      // we have a problem
      LOG(("WebSocketChannel:: illegal fragment sequence\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (mServerClosed) {
      LOG(("WebSocketChannel:: ignoring read frame code %d after close\n",
                 opcode));
      // nop
    } else if (mStopped) {
      LOG(("WebSocketChannel:: ignoring read frame code %d after completion\n",
           opcode));
    } else if (opcode == kText) {
      LOG(("WebSocketChannel:: text frame received\n"));
      if (mListener) {
        nsCString utf8Data;
        if (!utf8Data.Assign((const char *)payload, payloadLength,
                             mozilla::fallible_t()))
          return NS_ERROR_OUT_OF_MEMORY;

        // Section 8.1 says to fail connection if invalid utf-8 in text message
        if (!IsUTF8(utf8Data, false)) {
          LOG(("WebSocketChannel:: text frame invalid utf-8\n"));
          return NS_ERROR_CANNOT_CONVERT_DATA;
        }

        mTargetThread->Dispatch(new CallOnMessageAvailable(this, utf8Data, -1),
                                NS_DISPATCH_NORMAL);
        if (mConnectionLogService && !mPrivateBrowsing) {
          mConnectionLogService->NewMsgReceived(mHost, mSerial, count);
          LOG(("Added new msg received for %s", mHost.get()));
        }
      }
    } else if (opcode & kControlFrameMask) {
      // control frames
      if (payloadLength > 125) {
        LOG(("WebSocketChannel:: bad control frame code %d length %d\n",
             opcode, payloadLength));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (opcode == kClose) {
        LOG(("WebSocketChannel:: close received\n"));
        mServerClosed = 1;

        mServerCloseCode = CLOSE_NO_STATUS;
        if (payloadLength >= 2) {
          mServerCloseCode = NetworkEndian::readUint16(payload);
          LOG(("WebSocketChannel:: close recvd code %u\n", mServerCloseCode));
          uint16_t msglen = static_cast<uint16_t>(payloadLength - 2);
          if (msglen > 0) {
            mServerCloseReason.SetLength(msglen);
            memcpy(mServerCloseReason.BeginWriting(),
                   (const char *)payload + 2, msglen);

            // section 8.1 says to replace received non utf-8 sequences
            // (which are non-conformant to send) with u+fffd,
            // but secteam feels that silently rewriting messages is
            // inappropriate - so we will fail the connection instead.
            if (!IsUTF8(mServerCloseReason, false)) {
              LOG(("WebSocketChannel:: close frame invalid utf-8\n"));
              return NS_ERROR_CANNOT_CONVERT_DATA;
            }

            LOG(("WebSocketChannel:: close msg %s\n",
                 mServerCloseReason.get()));
          }
        }

        if (mCloseTimer) {
          mCloseTimer->Cancel();
          mCloseTimer = nullptr;
        }
        if (mListener) {
          mTargetThread->Dispatch(new CallOnServerClose(this, mServerCloseCode,
                                                        mServerCloseReason),
                                  NS_DISPATCH_NORMAL);
        }

        if (mClientClosed)
          ReleaseSession();
      } else if (opcode == kPing) {
        LOG(("WebSocketChannel:: ping received\n"));
        GeneratePong(payload, payloadLength);
      } else if (opcode == kPong) {
        // opcode kPong: the mere act of receiving the packet is all we need
        // to do for the pong to trigger the activity timers
        LOG(("WebSocketChannel:: pong received\n"));
      } else {
        /* unknown control frame opcode */
        LOG(("WebSocketChannel:: unknown control op code %d\n", opcode));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (mFragmentAccumulator) {
        // Remove the control frame from the stream so we have a contiguous
        // data buffer of reassembled fragments
        LOG(("WebSocketChannel:: Removing Control From Read buffer\n"));
        NS_ABORT_IF_FALSE(mFramePtr + framingLength == payload,
                          "payload offset from frameptr wrong");
        ::memmove(mFramePtr, payload + payloadLength, avail - payloadLength);
        payload = mFramePtr;
        avail -= payloadLength;
        if (mBuffered)
          mBuffered -= framingLength + payloadLength;
        payloadLength = 0;
      }
    } else if (opcode == kBinary) {
      LOG(("WebSocketChannel:: binary frame received\n"));
      if (mListener) {
        nsCString binaryData((const char *)payload, payloadLength);
        mTargetThread->Dispatch(new CallOnMessageAvailable(this, binaryData,
                                                           payloadLength),
                                NS_DISPATCH_NORMAL);
        // To add the header to 'Networking Dashboard' log
        if (mConnectionLogService && !mPrivateBrowsing) {
          mConnectionLogService->NewMsgReceived(mHost, mSerial, count);
          LOG(("Added new received msg for %s", mHost.get()));
        }
      }
    } else if (opcode != kContinuation) {
      /* unknown opcode */
      LOG(("WebSocketChannel:: unknown op code %d\n", opcode));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    mFramePtr = payload + payloadLength;
    avail -= payloadLength;
    totalAvail = avail;
  }

  // Adjust the stateful buffer. If we were operating off the stack and
  // now have a partial message then transition to the buffer, or if
  // we were working off the buffer but no longer have any active state
  // then transition to the stack
  if (!IsPersistentFramePtr()) {
    mBuffered = 0;

    if (mFragmentAccumulator) {
      LOG(("WebSocketChannel:: Setup Buffer due to fragment"));

      if (!UpdateReadBuffer(mFramePtr - mFragmentAccumulator,
                            totalAvail + mFragmentAccumulator, 0, nullptr)) {
        return NS_ERROR_FILE_TOO_BIG;
      }

      // UpdateReadBuffer will reset the frameptr to the beginning
      // of new saved state, so we need to skip past processed framgents
      mFramePtr += mFragmentAccumulator;
    } else if (totalAvail) {
      LOG(("WebSocketChannel:: Setup Buffer due to partial frame"));
      if (!UpdateReadBuffer(mFramePtr, totalAvail, 0, nullptr)) {
        return NS_ERROR_FILE_TOO_BIG;
      }
    }
  } else if (!mFragmentAccumulator && !totalAvail) {
    // If we were working off a saved buffer state and there is no partial
    // frame or fragment in process, then revert to stack behavior
    LOG(("WebSocketChannel:: Internal buffering not needed anymore"));
    mBuffered = 0;

    // release memory if we've been processing a large message
    if (mBufferSize > kIncomingBufferStableSize) {
      mBufferSize = kIncomingBufferStableSize;
      moz_free(mBuffer);
      mBuffer = (uint8_t *)moz_xmalloc(mBufferSize);
    }
  }
  return NS_OK;
}

void
WebSocketChannel::ApplyMask(uint32_t mask, uint8_t *data, uint64_t len)
{
  if (!data || len == 0)
    return;

  // Optimally we want to apply the mask 32 bits at a time,
  // but the buffer might not be alligned. So we first deal with
  // 0 to 3 bytes of preamble individually

  while (len && (reinterpret_cast<uintptr_t>(data) & 3)) {
    *data ^= mask >> 24;
    mask = RotateLeft(mask, 8);
    data++;
    len--;
  }

  // perform mask on full words of data

  uint32_t *iData = (uint32_t *) data;
  uint32_t *end = iData + (len / 4);
  NetworkEndian::writeUint32(&mask, mask);
  for (; iData < end; iData++)
    *iData ^= mask;
  mask = NetworkEndian::readUint32(&mask);
  data = (uint8_t *)iData;
  len  = len % 4;

  // There maybe up to 3 trailing bytes that need to be dealt with
  // individually 

  while (len) {
    *data ^= mask >> 24;
    mask = RotateLeft(mask, 8);
    data++;
    len--;
  }
}

void
WebSocketChannel::GeneratePing()
{
  nsCString *buf = new nsCString();
  buf->AssignLiteral("PING");
  EnqueueOutgoingMessage(mOutgoingPingMessages,
                         new OutboundMessage(kMsgTypePing, buf));
}

void
WebSocketChannel::GeneratePong(uint8_t *payload, uint32_t len)
{
  nsCString *buf = new nsCString();
  buf->SetLength(len);
  if (buf->Length() < len) {
    LOG(("WebSocketChannel::GeneratePong Allocation Failure\n"));
    delete buf;
    return;
  }

  memcpy(buf->BeginWriting(), payload, len);
  EnqueueOutgoingMessage(mOutgoingPongMessages,
                         new OutboundMessage(kMsgTypePong, buf));
}

void
WebSocketChannel::EnqueueOutgoingMessage(nsDeque &aQueue,
                                         OutboundMessage *aMsg)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  LOG(("WebSocketChannel::EnqueueOutgoingMessage %p "
       "queueing msg %p [type=%s len=%d]\n",
       this, aMsg, msgNames[aMsg->GetMsgType()], aMsg->Length()));

  aQueue.Push(aMsg);
  OnOutputStreamReady(mSocketOut);
}


uint16_t
WebSocketChannel::ResultToCloseCode(nsresult resultCode)
{
  if (NS_SUCCEEDED(resultCode))
    return CLOSE_NORMAL;

  switch (resultCode) {
    case NS_ERROR_FILE_TOO_BIG:
    case NS_ERROR_OUT_OF_MEMORY:
      return CLOSE_TOO_LARGE;
    case NS_ERROR_CANNOT_CONVERT_DATA:
      return CLOSE_INVALID_PAYLOAD;
    case NS_ERROR_UNEXPECTED:
      return CLOSE_INTERNAL_ERROR;
    default:
      return CLOSE_PROTOCOL_ERROR;
  }
}

void
WebSocketChannel::PrimeNewOutgoingMessage()
{
  LOG(("WebSocketChannel::PrimeNewOutgoingMessage() %p\n", this));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  NS_ABORT_IF_FALSE(!mCurrentOut, "Current message in progress");

  nsresult rv = NS_OK;

  mCurrentOut = (OutboundMessage *)mOutgoingPongMessages.PopFront();
  if (mCurrentOut) {
    NS_ABORT_IF_FALSE(mCurrentOut->GetMsgType() == kMsgTypePong,
                     "Not pong message!");
  } else {
    mCurrentOut = (OutboundMessage *)mOutgoingPingMessages.PopFront();
    if (mCurrentOut)
      NS_ABORT_IF_FALSE(mCurrentOut->GetMsgType() == kMsgTypePing,
                        "Not ping message!");
    else
      mCurrentOut = (OutboundMessage *)mOutgoingMessages.PopFront();
  }

  if (!mCurrentOut)
    return;

  WsMsgType msgType = mCurrentOut->GetMsgType();

  LOG(("WebSocketChannel::PrimeNewOutgoingMessage "
       "%p found queued msg %p [type=%s len=%d]\n",
       this, mCurrentOut, msgNames[msgType], mCurrentOut->Length()));

  mCurrentOutSent = 0;
  mHdrOut = mOutHeader;

  uint8_t *payload = nullptr;

  if (msgType == kMsgTypeFin) {
    // This is a demand to create a close message
    if (mClientClosed) {
      DeleteCurrentOutGoingMessage();
      PrimeNewOutgoingMessage();
      return;
    }

    mClientClosed = 1;
    mOutHeader[0] = kFinalFragBit | kClose;
    mOutHeader[1] = kMaskBit;

    // payload is offset 6 including 4 for the mask
    payload = mOutHeader + 6;

    // The close reason code sits in the first 2 bytes of payload
    // If the channel user provided a code and reason during Close()
    // and there isn't an internal error, use that.
    if (NS_SUCCEEDED(mStopOnClose)) {
      if (mScriptCloseCode) {
        NetworkEndian::writeUint16(payload, mScriptCloseCode);
        mOutHeader[1] += 2;
        mHdrOutToSend = 8;
        if (!mScriptCloseReason.IsEmpty()) {
          NS_ABORT_IF_FALSE(mScriptCloseReason.Length() <= 123,
                            "Close Reason Too Long");
          mOutHeader[1] += mScriptCloseReason.Length();
          mHdrOutToSend += mScriptCloseReason.Length();
          memcpy (payload + 2,
                  mScriptCloseReason.BeginReading(),
                  mScriptCloseReason.Length());
        }
      } else {
        // No close code/reason, so payload length = 0.  We must still send mask
        // even though it's not used.  Keep payload offset so we write mask
        // below.
        mHdrOutToSend = 6;
      }
    } else {
      NetworkEndian::writeUint16(payload, ResultToCloseCode(mStopOnClose));
      mOutHeader[1] += 2;
      mHdrOutToSend = 8;
    }

    if (mServerClosed) {
      /* bidi close complete */
      mReleaseOnTransmit = 1;
    } else if (NS_FAILED(mStopOnClose)) {
      /* result of abort session - give up */
      StopSession(mStopOnClose);
    } else {
      /* wait for reciprocal close from server */
      mCloseTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        mCloseTimer->InitWithCallback(this, mCloseTimeout,
                                      nsITimer::TYPE_ONE_SHOT);
      } else {
        StopSession(rv);
      }
    }
  } else {
    switch (msgType) {
    case kMsgTypePong:
      mOutHeader[0] = kFinalFragBit | kPong;
      break;
    case kMsgTypePing:
      mOutHeader[0] = kFinalFragBit | kPing;
      break;
    case kMsgTypeString:
      mOutHeader[0] = kFinalFragBit | kText;
      break;
    case kMsgTypeStream:
      // HACK ALERT:  read in entire stream into string.
      // Will block socket transport thread if file is blocking.
      // TODO: bug 704447:  don't block socket thread!
      rv = mCurrentOut->ConvertStreamToString();
      if (NS_FAILED(rv)) {
        AbortSession(NS_ERROR_FILE_TOO_BIG);
        return;
      }
      // Now we're a binary string
      msgType = kMsgTypeBinaryString;

      // no break: fall down into binary string case

    case kMsgTypeBinaryString:
      mOutHeader[0] = kFinalFragBit | kBinary;
      break;
    case kMsgTypeFin:
      NS_ABORT_IF_FALSE(false, "unreachable");  // avoid compiler warning
      break;
    }

    if (mCurrentOut->Length() < 126) {
      mOutHeader[1] = mCurrentOut->Length() | kMaskBit;
      mHdrOutToSend = 6;
    } else if (mCurrentOut->Length() <= 0xffff) {
      mOutHeader[1] = 126 | kMaskBit;
      NetworkEndian::writeUint16(mOutHeader + sizeof(uint16_t),
                                 mCurrentOut->Length());
      mHdrOutToSend = 8;
    } else {
      mOutHeader[1] = 127 | kMaskBit;
      NetworkEndian::writeUint64(mOutHeader + 2, mCurrentOut->Length());
      mHdrOutToSend = 14;
    }
    payload = mOutHeader + mHdrOutToSend;
  }

  NS_ABORT_IF_FALSE(payload, "payload offset not found");

  // Perform the sending mask. Never use a zero mask
  uint32_t mask;
  do {
    uint8_t *buffer;
    nsresult rv = mRandomGenerator->GenerateRandomBytes(4, &buffer);
    if (NS_FAILED(rv)) {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage(): "
           "GenerateRandomBytes failure %x\n", rv));
      StopSession(rv);
      return;
    }
    mask = * reinterpret_cast<uint32_t *>(buffer);
    NS_Free(buffer);
  } while (!mask);
  NetworkEndian::writeUint32(payload - sizeof(uint32_t), mask);

  LOG(("WebSocketChannel::PrimeNewOutgoingMessage() using mask %08x\n", mask));

  // We don't mask the framing, but occasionally we stick a little payload
  // data in the buffer used for the framing. Close frames are the current
  // example. This data needs to be masked, but it is never more than a
  // handful of bytes and might rotate the mask, so we can just do it locally.
  // For real data frames we ship the bulk of the payload off to ApplyMask()

  while (payload < (mOutHeader + mHdrOutToSend)) {
    *payload ^= mask >> 24;
    mask = RotateLeft(mask, 8);
    payload++;
  }

  // Mask the real message payloads

  ApplyMask(mask, mCurrentOut->BeginWriting(), mCurrentOut->Length());

  int32_t len = mCurrentOut->Length();

  // for small frames, copy it all together for a contiguous write
  if (len && len <= kCopyBreak) {
    memcpy(mOutHeader + mHdrOutToSend, mCurrentOut->BeginWriting(), len);
    mHdrOutToSend += len;
    mCurrentOutSent = len;
  }

  if (len && mCompressor) {
    // assume a 1/3 reduction in size for sizing the buffer
    // the buffer is used multiple times if necessary
    uint32_t currentHeaderSize = mHdrOutToSend;
    mHdrOutToSend = 0;

    EnsureHdrOut(32 + (currentHeaderSize + len - mCurrentOutSent) / 2 * 3);
    mCompressor->Deflate(mOutHeader, currentHeaderSize,
                         mCurrentOut->BeginReading() + mCurrentOutSent,
                         len - mCurrentOutSent);

    // All of the compressed data now resides in {mHdrOut, mHdrOutToSend}
    // so do not send the body again
    mCurrentOutSent = len;
  }

  // Transmitting begins - mHdrOutToSend bytes from mOutHeader and
  // mCurrentOut->Length() bytes from mCurrentOut. The latter may be
  // coaleseced into the former for small messages or as the result of the
  // compression process,
}

void
WebSocketChannel::DeleteCurrentOutGoingMessage()
{
  delete mCurrentOut;
  mCurrentOut = nullptr;
  mCurrentOutSent = 0;
}

void
WebSocketChannel::EnsureHdrOut(uint32_t size)
{
  LOG(("WebSocketChannel::EnsureHdrOut() %p [%d]\n", this, size));

  if (mDynamicOutputSize < size) {
    mDynamicOutputSize = size;
    mDynamicOutput =
      (uint8_t *) moz_xrealloc(mDynamicOutput, mDynamicOutputSize);
  }

  mHdrOut = mDynamicOutput;
}

void
WebSocketChannel::CleanupConnection()
{
  LOG(("WebSocketChannel::CleanupConnection() %p", this));

  if (mLingeringCloseTimer) {
    mLingeringCloseTimer->Cancel();
    mLingeringCloseTimer = nullptr;
  }

  if (mSocketIn) {
    mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
    mSocketIn = nullptr;
  }

  if (mSocketOut) {
    mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
    mSocketOut = nullptr;
  }

  if (mTransport) {
    mTransport->SetSecurityCallbacks(nullptr);
    mTransport->SetEventSink(nullptr, nullptr);
    mTransport->Close(NS_BASE_STREAM_CLOSED);
    mTransport = nullptr;
  }

  if (mConnectionLogService && !mPrivateBrowsing) {
    mConnectionLogService->RemoveHost(mHost, mSerial);
  }

  DecrementSessionCount();
}

void
WebSocketChannel::StopSession(nsresult reason)
{
  LOG(("WebSocketChannel::StopSession() %p [%x]\n", this, reason));

  // normally this should be called on socket thread, but it is ok to call it
  // from OnStartRequest before the socket thread machine has gotten underway

  mStopped = 1;

  if (!mOpenedHttpChannel) {
    // The HTTP channel information will never be used in this case
    mChannel = nullptr;
    mHttpChannel = nullptr;
    mLoadGroup = nullptr;
    mCallbacks = nullptr;
  }

  if (mCloseTimer) {
    mCloseTimer->Cancel();
    mCloseTimer = nullptr;
  }

  if (mOpenTimer) {
    mOpenTimer->Cancel();
    mOpenTimer = nullptr;
  }

  if (mReconnectDelayTimer) {
    mReconnectDelayTimer->Cancel();
    mReconnectDelayTimer = nullptr;
  }

  if (mPingTimer) {
    mPingTimer->Cancel();
    mPingTimer = nullptr;
  }

  if (mSocketIn && !mTCPClosed) {
    // Drain, within reason, this socket. if we leave any data
    // unconsumed (including the tcp fin) a RST will be generated
    // The right thing to do here is shutdown(SHUT_WR) and then wait
    // a little while to see if any data comes in.. but there is no
    // reason to delay things for that when the websocket handshake
    // is supposed to guarantee a quiet connection except for that fin.

    char     buffer[512];
    uint32_t count = 0;
    uint32_t total = 0;
    nsresult rv;
    do {
      total += count;
      rv = mSocketIn->Read(buffer, 512, &count);
      if (rv != NS_BASE_STREAM_WOULD_BLOCK &&
        (NS_FAILED(rv) || count == 0))
        mTCPClosed = true;
    } while (NS_SUCCEEDED(rv) && count > 0 && total < 32000);
  }

  int32_t sessionCount = kLingeringCloseThreshold;
  nsWSAdmissionManager::GetSessionCount(sessionCount);

  if (!mTCPClosed && mTransport && sessionCount < kLingeringCloseThreshold) {

    // 7.1.1 says that the client SHOULD wait for the server to close the TCP
    // connection. This is so we can reuse port numbers before 2 MSL expires,
    // which is not really as much of a concern for us as the amount of state
    // that might be accrued by keeping this channel object around waiting for
    // the server. We handle the SHOULD by waiting a short time in the common
    // case, but not waiting in the case of high concurrency.
    //
    // Normally this will be taken care of in AbortSession() after mTCPClosed
    // is set when the server close arrives without waiting for the timeout to
    // expire.

    LOG(("WebSocketChannel::StopSession: Wait for Server TCP close"));

    nsresult rv;
    mLingeringCloseTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv))
      mLingeringCloseTimer->InitWithCallback(this, kLingeringCloseTimeout,
                                             nsITimer::TYPE_ONE_SHOT);
    else
      CleanupConnection();
  } else {
    CleanupConnection();
  }

  if (mCancelable) {
    mCancelable->Cancel(NS_ERROR_UNEXPECTED);
    mCancelable = nullptr;
  }

  mInflateReader = nullptr;
  mInflateStream = nullptr;

  delete mCompressor;
  mCompressor = nullptr;

  if (!mCalledOnStop) {
    mCalledOnStop = 1;
    mTargetThread->Dispatch(new CallOnStop(this, reason),
                            NS_DISPATCH_NORMAL);
  }

  return;
}

void
WebSocketChannel::AbortSession(nsresult reason)
{
  LOG(("WebSocketChannel::AbortSession() %p [reason %x] stopped = %d\n",
       this, reason, mStopped));

  // normally this should be called on socket thread, but it is ok to call it
  // from the main thread before StartWebsocketData() has completed

  // When we are failing we need to close the TCP connection immediately
  // as per 7.1.1
  mTCPClosed = true;

  if (mLingeringCloseTimer) {
    NS_ABORT_IF_FALSE(mStopped, "Lingering without Stop");
    LOG(("WebSocketChannel:: Cleanup connection based on TCP Close"));
    CleanupConnection();
    return;
  }

  if (mStopped)
    return;
  mStopped = 1;

  if (mTransport && reason != NS_BASE_STREAM_CLOSED &&
      !mRequestedClose && !mClientClosed && !mServerClosed) {
    mRequestedClose = 1;
    mStopOnClose = reason;
    mSocketThread->Dispatch(
      new OutboundEnqueuer(this, new OutboundMessage(kMsgTypeFin, nullptr)),
                           nsIEventTarget::DISPATCH_NORMAL);
  } else {
    StopSession(reason);
  }
}

// ReleaseSession is called on orderly shutdown
void
WebSocketChannel::ReleaseSession()
{
  LOG(("WebSocketChannel::ReleaseSession() %p stopped = %d\n",
       this, mStopped));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  if (mStopped)
    return;
  StopSession(NS_OK);
}

void
WebSocketChannel::IncrementSessionCount()
{
  if (!mIncrementedSessionCount) {
    nsWSAdmissionManager::IncrementSessionCount();
    mIncrementedSessionCount = 1;
  }
}

void
WebSocketChannel::DecrementSessionCount()
{
  // Make sure we decrement session count only once, and only if we incremented it.
  // This code is thread-safe: sWebSocketAdmissions->DecrementSessionCount is
  // atomic, and mIncrementedSessionCount/mDecrementedSessionCount are set at
  // times when they'll never be a race condition for checking/setting them.
  if (mIncrementedSessionCount && !mDecrementedSessionCount) {
    nsWSAdmissionManager::DecrementSessionCount();
    mDecrementedSessionCount = 1;
  }
}

nsresult
WebSocketChannel::HandleExtensions()
{
  LOG(("WebSocketChannel::HandleExtensions() %p\n", this));

  nsresult rv;
  nsAutoCString extensions;

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  rv = mHttpChannel->GetResponseHeader(
    NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"), extensions);
  if (NS_SUCCEEDED(rv)) {
    if (!extensions.IsEmpty()) {
      if (!extensions.EqualsLiteral("deflate-stream")) {
        LOG(("WebSocketChannel::OnStartRequest: "
             "HTTP Sec-WebSocket-Exensions negotiated unknown value %s\n",
             extensions.get()));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (!mAllowCompression) {
        LOG(("WebSocketChannel::HandleExtensions: "
             "Recvd Compression Extension that wasn't offered\n"));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }

      nsCOMPtr<nsIStreamConverterService> serv =
        do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        LOG(("WebSocketChannel:: Cannot find compression service\n"));
        AbortSession(NS_ERROR_UNEXPECTED);
        return NS_ERROR_UNEXPECTED;
      }

      rv = serv->AsyncConvertData("deflate", "uncompressed", this, nullptr,
                                  getter_AddRefs(mInflateReader));

      if (NS_FAILED(rv)) {
        LOG(("WebSocketChannel:: Cannot find inflate listener\n"));
        AbortSession(NS_ERROR_UNEXPECTED);
        return NS_ERROR_UNEXPECTED;
      }

      mInflateStream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);

      if (NS_FAILED(rv)) {
        LOG(("WebSocketChannel:: Cannot find inflate stream\n"));
        AbortSession(NS_ERROR_UNEXPECTED);
        return NS_ERROR_UNEXPECTED;
      }

      mCompressor = new nsWSCompression(this, mSocketOut);
      if (!mCompressor->Active()) {
        LOG(("WebSocketChannel:: Cannot init deflate object\n"));
        delete mCompressor;
        mCompressor = nullptr;
        AbortSession(NS_ERROR_UNEXPECTED);
        return NS_ERROR_UNEXPECTED;
      }
      mNegotiatedExtensions = extensions;
    }
  }

  return NS_OK;
}

nsresult
WebSocketChannel::SetupRequest()
{
  LOG(("WebSocketChannel::SetupRequest() %p\n", this));

  nsresult rv;

  if (mLoadGroup) {
    rv = mHttpChannel->SetLoadGroup(mLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mHttpChannel->SetLoadFlags(nsIRequest::LOAD_BACKGROUND |
                                  nsIRequest::INHIBIT_CACHING |
                                  nsIRequest::LOAD_BYPASS_CACHE);
  NS_ENSURE_SUCCESS(rv, rv);

  // we never let websockets be blocked by head CSS/JS loads to avoid
  // potential deadlock where server generation of CSS/JS requires
  // an XHR signal.
  rv = mChannel->SetLoadUnblocked(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // draft-ietf-hybi-thewebsocketprotocol-07 illustrates Upgrade: websocket
  // in lower case, so go with that. It is technically case insensitive.
  rv = mChannel->HTTPUpgrade(NS_LITERAL_CSTRING("websocket"), this);
  NS_ENSURE_SUCCESS(rv, rv);

  mHttpChannel->SetRequestHeader(
    NS_LITERAL_CSTRING("Sec-WebSocket-Version"),
    NS_LITERAL_CSTRING(SEC_WEBSOCKET_VERSION), false);

  if (!mOrigin.IsEmpty())
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Origin"), mOrigin,
                                   false);

  if (!mProtocol.IsEmpty())
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"),
                                   mProtocol, true);

  if (mAllowCompression)
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"),
                                   NS_LITERAL_CSTRING("deflate-stream"),
                                   false);

  uint8_t      *secKey;
  nsAutoCString secKeyString;

  rv = mRandomGenerator->GenerateRandomBytes(16, &secKey);
  NS_ENSURE_SUCCESS(rv, rv);
  char* b64 = PL_Base64Encode((const char *)secKey, 16, nullptr);
  NS_Free(secKey);
  if (!b64)
    return NS_ERROR_OUT_OF_MEMORY;
  secKeyString.Assign(b64);
  PR_Free(b64);
  mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Key"),
                                 secKeyString, false);
  LOG(("WebSocketChannel::SetupRequest: client key %s\n", secKeyString.get()));

  // prepare the value we expect to see in
  // the sec-websocket-accept response header
  secKeyString.AppendLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  nsCOMPtr<nsICryptoHash> hasher =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Init(nsICryptoHash::SHA1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Update((const uint8_t *) secKeyString.BeginWriting(),
                      secKeyString.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Finish(true, mHashedSecret);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("WebSocketChannel::SetupRequest: expected server key %s\n",
       mHashedSecret.get()));

  return NS_OK;
}

nsresult
WebSocketChannel::DoAdmissionDNS()
{
  nsresult rv;

  nsCString hostName;
  rv = mURI->GetHost(hostName);
  NS_ENSURE_SUCCESS(rv, rv);
  mAddress = hostName;
  rv = mURI->GetPort(&mPort);
  NS_ENSURE_SUCCESS(rv, rv);
  if (mPort == -1)
    mPort = (mEncrypted ? kDefaultWSSPort : kDefaultWSPort);
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  MOZ_ASSERT(!mCancelable);
  return dns->AsyncResolve(hostName, 0, this, mainThread, getter_AddRefs(mCancelable));
}

nsresult
WebSocketChannel::ApplyForAdmission()
{
  LOG(("WebSocketChannel::ApplyForAdmission() %p\n", this));

  // Websockets has a policy of 1 session at a time being allowed in the
  // CONNECTING state per server IP address (not hostname)

  // Check to see if a proxy is being used before making DNS call
  nsCOMPtr<nsIProtocolProxyService> pps =
    do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID);

  if (!pps) {
    // go straight to DNS
    // expect the callback in ::OnLookupComplete
    LOG(("WebSocketChannel::ApplyForAdmission: checking for concurrent open\n"));
    return DoAdmissionDNS();
  }

  MOZ_ASSERT(!mCancelable);

  return pps->AsyncResolve(mURI,
                           nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                           nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                           this, getter_AddRefs(mCancelable));
}

// Called after both OnStartRequest and OnTransportAvailable have
// executed. This essentially ends the handshake and starts the websockets
// protocol state machine.
nsresult
WebSocketChannel::StartWebsocketData()
{
  LOG(("WebSocketChannel::StartWebsocketData() %p", this));
  NS_ABORT_IF_FALSE(!mDataStarted, "StartWebsocketData twice");
  mDataStarted = 1;

  // We're now done CONNECTING, which means we can now open another,
  // perhaps parallel, connection to the same host if one
  // is pending
  nsWSAdmissionManager::OnConnected(this);

  LOG(("WebSocketChannel::StartWebsocketData Notifying Listener %p\n",
       mListener.get()));

  if (mListener)
    mListener->OnStart(mContext);

  // Start keepalive ping timer, if we're using keepalive.
  if (mPingInterval) {
    nsresult rv;
    mPingTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("unable to create ping timer. Carrying on.");
    } else {
      LOG(("WebSocketChannel will generate ping after %d ms of receive silence\n",
           mPingInterval));
      mPingTimer->SetTarget(mSocketThread);
      mPingTimer->InitWithCallback(this, mPingInterval, nsITimer::TYPE_ONE_SHOT);
    }
  }

  return mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
}

void
WebSocketChannel::ReportConnectionTelemetry()
{ 
  // 3 bits are used. high bit is for wss, middle bit for failed,
  // and low bit for proxy..
  // 0 - 7 : ws-ok-plain, ws-ok-proxy, ws-failed-plain, ws-failed-proxy,
  //         wss-ok-plain, wss-ok-proxy, wss-failed-plain, wss-failed-proxy

  bool didProxy = false;

  nsCOMPtr<nsIProxyInfo> pi;
  nsCOMPtr<nsIProxiedChannel> pc = do_QueryInterface(mChannel);
  if (pc)
    pc->GetProxyInfo(getter_AddRefs(pi));
  if (pi) {
    nsAutoCString proxyType;
    pi->GetType(proxyType);
    if (!proxyType.IsEmpty() &&
        !proxyType.EqualsLiteral("direct"))
      didProxy = true;
  }

  uint8_t value = (mEncrypted ? (1 << 2) : 0) | 
    (!mGotUpgradeOK ? (1 << 1) : 0) |
    (didProxy ? (1 << 0) : 0);

  LOG(("WebSocketChannel::ReportConnectionTelemetry() %p %d", this, value));
  Telemetry::Accumulate(Telemetry::WEBSOCKETS_HANDSHAKE_TYPE, value);
}

// nsIDNSListener

NS_IMETHODIMP
WebSocketChannel::OnLookupComplete(nsICancelable *aRequest,
                                   nsIDNSRecord *aRecord,
                                   nsresult aStatus)
{
  LOG(("WebSocketChannel::OnLookupComplete() %p [%p %p %x]\n",
       this, aRequest, aRecord, aStatus));

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (mStopped) {
    LOG(("WebSocketChannel::OnLookupComplete: Request Already Stopped\n"));
    mCancelable = nullptr;
    return NS_OK;
  }

  mCancelable = nullptr;

  // These failures are not fatal - we just use the hostname as the key
  if (NS_FAILED(aStatus)) {
    LOG(("WebSocketChannel::OnLookupComplete: No DNS Response\n"));

    // set host in case we got here without calling DoAdmissionDNS()
    mURI->GetHost(mAddress);
  } else {
    nsresult rv = aRecord->GetNextAddrAsString(mAddress);
    if (NS_FAILED(rv))
      LOG(("WebSocketChannel::OnLookupComplete: Failed GetNextAddr\n"));
  }

  LOG(("WebSocket OnLookupComplete: Proceeding to ConditionallyConnect\n"));
  nsWSAdmissionManager::ConditionallyConnect(this);

  return NS_OK;
}

// nsIProtocolProxyCallback
NS_IMETHODIMP
WebSocketChannel::OnProxyAvailable(nsICancelable *aRequest, nsIURI *aURI,
                                   nsIProxyInfo *pi, nsresult status)
{
  if (mStopped) {
    LOG(("WebSocketChannel::OnProxyAvailable: [%p] Request Already Stopped\n", this));
    mCancelable = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(aRequest == mCancelable);
  mCancelable = nullptr;

  nsAutoCString type;
  if (NS_SUCCEEDED(status) && pi &&
      NS_SUCCEEDED(pi->GetType(type)) &&
      !type.EqualsLiteral("direct")) {
    LOG(("WebSocket OnProxyAvailable [%p] Proxy found skip DNS lookup\n", this));
    // call DNS callback directly without DNS resolver
    OnLookupComplete(nullptr, nullptr, NS_ERROR_FAILURE);
  } else {
    LOG(("WebSocketChannel::OnProxyAvailable[%] checking DNS resolution\n", this));
    nsresult rv = DoAdmissionDNS();
    if (NS_FAILED(rv)) {
      LOG(("WebSocket OnProxyAvailable [%p] DNS lookup failed\n", this));
      // call DNS callback directly without DNS resolver
      OnLookupComplete(nullptr, nullptr, NS_ERROR_FAILURE);
    }
  }

  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
WebSocketChannel::GetInterface(const nsIID & iid, void **result)
{
  LOG(("WebSocketChannel::GetInterface() %p\n", this));

  if (iid.Equals(NS_GET_IID(nsIChannelEventSink)))
    return QueryInterface(iid, result);

  if (mCallbacks)
    return mCallbacks->GetInterface(iid, result);

  return NS_ERROR_FAILURE;
}

// nsIChannelEventSink

NS_IMETHODIMP
WebSocketChannel::AsyncOnChannelRedirect(
                    nsIChannel *oldChannel,
                    nsIChannel *newChannel,
                    uint32_t flags,
                    nsIAsyncVerifyRedirectCallback *callback)
{
  LOG(("WebSocketChannel::AsyncOnChannelRedirect() %p\n", this));
  nsresult rv;

  nsCOMPtr<nsIURI> newuri;
  rv = newChannel->GetURI(getter_AddRefs(newuri));
  NS_ENSURE_SUCCESS(rv, rv);

  // newuri is expected to be http or https
  bool newuriIsHttps = false;
  rv = newuri->SchemeIs("https", &newuriIsHttps);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mAutoFollowRedirects) {
    // Even if redirects configured off, still allow them for HTTP Strict
    // Transport Security (from ws://FOO to https://FOO (mapped to wss://FOO)

    nsCOMPtr<nsIURI> clonedNewURI;
    rv = newuri->Clone(getter_AddRefs(clonedNewURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = clonedNewURI->SetScheme(NS_LITERAL_CSTRING("ws"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> currentURI;
    rv = GetURI(getter_AddRefs(currentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // currentURI is expected to be ws or wss
    bool currentIsHttps = false;
    rv = currentURI->SchemeIs("wss", &currentIsHttps);
    NS_ENSURE_SUCCESS(rv, rv);

    bool uriEqual = false;
    rv = clonedNewURI->Equals(currentURI, &uriEqual);
    NS_ENSURE_SUCCESS(rv, rv);

    // It's only a HSTS redirect if we started with non-secure, are going to
    // secure, and the new URI is otherwise the same as the old one.
    if (!(!currentIsHttps && newuriIsHttps && uriEqual)) {
      nsAutoCString newSpec;
      rv = newuri->GetSpec(newSpec);
      NS_ENSURE_SUCCESS(rv, rv);

      LOG(("WebSocketChannel: Redirect to %s denied by configuration\n",
           newSpec.get()));
      return NS_ERROR_FAILURE;
    }
  }

  if (mEncrypted && !newuriIsHttps) {
    nsAutoCString spec;
    if (NS_SUCCEEDED(newuri->GetSpec(spec)))
      LOG(("WebSocketChannel: Redirect to %s violates encryption rule\n",
           spec.get()));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(newChannel, &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not QI to HTTP\n"));
    return rv;
  }

  nsCOMPtr<nsIHttpChannelInternal> newUpgradeChannel =
    do_QueryInterface(newChannel, &rv);

  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not QI to HTTP Upgrade\n"));
    return rv;
  }

  // The redirect is likely OK

  newChannel->SetNotificationCallbacks(this);

  mEncrypted = newuriIsHttps;
  newuri->Clone(getter_AddRefs(mURI));
  if (mEncrypted)
    rv = mURI->SetScheme(NS_LITERAL_CSTRING("wss"));
  else
    rv = mURI->SetScheme(NS_LITERAL_CSTRING("ws"));

  mHttpChannel = newHttpChannel;
  mChannel = newUpgradeChannel;
  rv = SetupRequest();
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not SetupRequest()\n"));
    return rv;
  }

  // Redirected-to URI may need to be delayed by 1-connecting-per-host and
  // delay-after-fail algorithms.  So hold off calling OnRedirectVerifyCallback
  // until BeginOpen, when we know it's OK to proceed with new channel.
  mRedirectCallback = callback;

  // Mark old channel as successfully connected so we'll clear any FailDelay
  // associated with the old URI.  Note: no need to also call OnStopSession:
  // it's a no-op for successful, already-connected channels.
  nsWSAdmissionManager::OnConnected(this);

  // ApplyForAdmission as if we were starting from fresh...
  mAddress.Truncate();
  mOpenedHttpChannel = 0;
  rv = ApplyForAdmission();
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect failed due to DNS failure\n"));
    mRedirectCallback = nullptr;
    return rv;
  }

  return NS_OK;
}

// nsITimerCallback

NS_IMETHODIMP
WebSocketChannel::Notify(nsITimer *timer)
{
  LOG(("WebSocketChannel::Notify() %p [%p]\n", this, timer));

  if (timer == mCloseTimer) {
    NS_ABORT_IF_FALSE(mClientClosed, "Close Timeout without local close");
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread,
                      "not socket thread");

    mCloseTimer = nullptr;
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    LOG(("WebSocketChannel:: Expecting Server Close - Timed Out\n"));
    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mOpenTimer) {
    NS_ABORT_IF_FALSE(!mGotUpgradeOK,
                      "Open Timer after open complete");
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    mOpenTimer = nullptr;
    LOG(("WebSocketChannel:: Connection Timed Out\n"));
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mReconnectDelayTimer) {
    NS_ABORT_IF_FALSE(mConnecting == CONNECTING_DELAYED,
                      "woke up from delay w/o being delayed?");

    mReconnectDelayTimer = nullptr;
    LOG(("WebSocketChannel: connecting [this=%p] after reconnect delay", this));
    BeginOpen();
  } else if (timer == mPingTimer) {
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread,
                      "not socket thread");

    if (mClientClosed || mServerClosed || mRequestedClose) {
      // no point in worrying about ping now
      mPingTimer = nullptr;
      return NS_OK;
    }

    if (!mPingOutstanding) {
      LOG(("nsWebSocketChannel:: Generating Ping\n"));
      mPingOutstanding = 1;
      GeneratePing();
      mPingTimer->InitWithCallback(this, mPingResponseTimeout,
                                   nsITimer::TYPE_ONE_SHOT);
    } else {
      LOG(("nsWebSocketChannel:: Timed out Ping\n"));
      mPingTimer = nullptr;
      AbortSession(NS_ERROR_NET_TIMEOUT);
    }
  } else if (timer == mLingeringCloseTimer) {
    LOG(("WebSocketChannel:: Lingering Close Timer"));
    CleanupConnection();
  } else {
    NS_ABORT_IF_FALSE(0, "Unknown Timer");
  }

  return NS_OK;
}

// nsIWebSocketChannel

NS_IMETHODIMP
WebSocketChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG(("WebSocketChannel::GetSecurityInfo() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (mTransport) {
    if (NS_FAILED(mTransport->GetSecurityInfo(aSecurityInfo)))
      *aSecurityInfo = nullptr;
  }
  return NS_OK;
}


NS_IMETHODIMP
WebSocketChannel::AsyncOpen(nsIURI *aURI,
                            const nsACString &aOrigin,
                            nsIWebSocketListener *aListener,
                            nsISupports *aContext)
{
  LOG(("WebSocketChannel::AsyncOpen() %p\n", this));

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (!aURI || !aListener) {
    LOG(("WebSocketChannel::AsyncOpen() Uri or Listener null"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mListener || mWasOpened)
    return NS_ERROR_ALREADY_OPENED;

  nsresult rv;

  // Ensure target thread is set.
  if (!mTargetThread) {
    mTargetThread = do_GetMainThread();
  }

  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to continue without socket transport service");
    return rv;
  }

  mRandomGenerator =
    do_GetService("@mozilla.org/security/random-generator;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to continue without random number generator");
    return rv;
  }

  nsCOMPtr<nsIPrefBranch> prefService;
  prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if (prefService) {
    int32_t intpref;
    bool boolpref;
    rv = prefService->GetIntPref("network.websocket.max-message-size", 
                                 &intpref);
    if (NS_SUCCEEDED(rv)) {
      mMaxMessageSize = clamped(intpref, 1024, INT32_MAX);
    }
    rv = prefService->GetIntPref("network.websocket.timeout.close", &intpref);
    if (NS_SUCCEEDED(rv)) {
      mCloseTimeout = clamped(intpref, 1, 1800) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.open", &intpref);
    if (NS_SUCCEEDED(rv)) {
      mOpenTimeout = clamped(intpref, 1, 1800) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.ping.request",
                                 &intpref);
    if (NS_SUCCEEDED(rv) && !mClientSetPingInterval) {
      mPingInterval = clamped(intpref, 0, 86400) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.ping.response",
                                 &intpref);
    if (NS_SUCCEEDED(rv) && !mClientSetPingTimeout) {
      mPingResponseTimeout = clamped(intpref, 1, 3600) * 1000;
    }
    rv = prefService->GetBoolPref("network.websocket.extensions.stream-deflate",
                                  &boolpref);
    if (NS_SUCCEEDED(rv)) {
      mAllowCompression = boolpref ? 1 : 0;
    }
    rv = prefService->GetBoolPref("network.websocket.auto-follow-http-redirects",
                                  &boolpref);
    if (NS_SUCCEEDED(rv)) {
      mAutoFollowRedirects = boolpref ? 1 : 0;
    }
    rv = prefService->GetIntPref
      ("network.websocket.max-connections", &intpref);
    if (NS_SUCCEEDED(rv)) {
      mMaxConcurrentConnections = clamped(intpref, 1, 0xffff);
    }
  }

  int32_t sessionCount = -1;
  nsWSAdmissionManager::GetSessionCount(sessionCount);
  if (sessionCount >= 0) {
    LOG(("WebSocketChannel::AsyncOpen %p sessionCount=%d max=%d\n", this,
         sessionCount, mMaxConcurrentConnections));
  }

  if (sessionCount >= mMaxConcurrentConnections) {
    LOG(("WebSocketChannel: max concurrency %d exceeded (%d)",
         mMaxConcurrentConnections,
         sessionCount));

    // WebSocket connections are expected to be long lived, so return
    // an error here instead of queueing
    return NS_ERROR_SOCKET_CREATE_FAILED;
  }

  mOriginalURI = aURI;
  mURI = mOriginalURI;
  mURI->GetHostPort(mHost);
  mOrigin = aOrigin;

  nsCOMPtr<nsIURI> localURI;
  nsCOMPtr<nsIChannel> localChannel;

  mURI->Clone(getter_AddRefs(localURI));
  if (mEncrypted)
    rv = localURI->SetScheme(NS_LITERAL_CSTRING("https"));
  else
    rv = localURI->SetScheme(NS_LITERAL_CSTRING("http"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ioService;
  ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to continue without io service");
    return rv;
  }

  nsCOMPtr<nsIIOService2> io2 = do_QueryInterface(ioService, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("WebSocketChannel: unable to continue without ioservice2");
    return rv;
  }

  rv = io2->NewChannelFromURIWithProxyFlags(
              localURI,
              mURI,
              nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
              nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
              getter_AddRefs(localChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Pass most GetInterface() requests through to our instantiator, but handle
  // nsIChannelEventSink in this object in order to deal with redirects
  localChannel->SetNotificationCallbacks(this);

  mChannel = do_QueryInterface(localChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mHttpChannel = do_QueryInterface(localChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupRequest();
  if (NS_FAILED(rv))
    return rv;

  mPrivateBrowsing = NS_UsePrivateBrowsing(localChannel);

  if (mConnectionLogService && !mPrivateBrowsing) {
    mConnectionLogService->AddHost(mHost, mSerial,
                                   BaseWebSocketChannel::mEncrypted);
  }

  rv = ApplyForAdmission();
  if (NS_FAILED(rv))
    return rv;

  // Only set these if the open was successful:
  //
  mWasOpened = 1;
  mListener = aListener;
  mContext = aContext;
  IncrementSessionCount();

  return rv;
}

NS_IMETHODIMP
WebSocketChannel::Close(uint16_t code, const nsACString & reason)
{
  LOG(("WebSocketChannel::Close() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  // save the networkstats (bug 855949)
  SaveNetworkStats(true);

  if (mRequestedClose) {
    return NS_OK;
  }

  // The API requires the UTF-8 string to be 123 or less bytes
  if (reason.Length() > 123)
    return NS_ERROR_ILLEGAL_VALUE;

  mRequestedClose = 1;
  mScriptCloseReason = reason;
  mScriptCloseCode = code;

  if (!mTransport) {
    nsresult rv;
    if (code == CLOSE_GOING_AWAY) {
      // Not an error: for example, tab has closed or navigated away
      LOG(("WebSocketChannel::Close() GOING_AWAY without transport."));
      rv = NS_OK;
    } else {
      LOG(("WebSocketChannel::Close() without transport - error."));
      rv = NS_ERROR_NOT_CONNECTED;
    }
    StopSession(rv);
    return rv;
  }

  return mSocketThread->Dispatch(
      new OutboundEnqueuer(this, new OutboundMessage(kMsgTypeFin, nullptr)),
                           nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
WebSocketChannel::SendMsg(const nsACString &aMsg)
{
  LOG(("WebSocketChannel::SendMsg() %p\n", this));

  return SendMsgCommon(&aMsg, false, aMsg.Length());
}

NS_IMETHODIMP
WebSocketChannel::SendBinaryMsg(const nsACString &aMsg)
{
  LOG(("WebSocketChannel::SendBinaryMsg() %p len=%d\n", this, aMsg.Length()));
  return SendMsgCommon(&aMsg, true, aMsg.Length());
}

NS_IMETHODIMP
WebSocketChannel::SendBinaryStream(nsIInputStream *aStream, uint32_t aLength)
{
  LOG(("WebSocketChannel::SendBinaryStream() %p\n", this));

  return SendMsgCommon(nullptr, true, aLength, aStream);
}

nsresult
WebSocketChannel::SendMsgCommon(const nsACString *aMsg, bool aIsBinary,
                                uint32_t aLength, nsIInputStream *aStream)
{
  NS_ABORT_IF_FALSE(NS_GetCurrentThread() == mTargetThread, "not target thread");

  if (mRequestedClose) {
    LOG(("WebSocketChannel:: Error: send when closed\n"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mStopped) {
    LOG(("WebSocketChannel:: Error: send when stopped\n"));
    return NS_ERROR_NOT_CONNECTED;
  }

  NS_ABORT_IF_FALSE(mMaxMessageSize >= 0, "max message size negative");
  if (aLength > static_cast<uint32_t>(mMaxMessageSize)) {
    LOG(("WebSocketChannel:: Error: message too big\n"));
    return NS_ERROR_FILE_TOO_BIG;
  }

  if (mConnectionLogService && !mPrivateBrowsing) {
    mConnectionLogService->NewMsgSent(mHost, mSerial, aLength);
    LOG(("Added new msg sent for %s", mHost.get()));
  }

  return mSocketThread->Dispatch(
    aStream ? new OutboundEnqueuer(this, new OutboundMessage(aStream, aLength))
            : new OutboundEnqueuer(this,
                     new OutboundMessage(aIsBinary ? kMsgTypeBinaryString
                                                   : kMsgTypeString,
                                         new nsCString(*aMsg))),
    nsIEventTarget::DISPATCH_NORMAL);
}

// nsIHttpUpgradeListener

NS_IMETHODIMP
WebSocketChannel::OnTransportAvailable(nsISocketTransport *aTransport,
                                       nsIAsyncInputStream *aSocketIn,
                                       nsIAsyncOutputStream *aSocketOut)
{
  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(new CallOnTransportAvailable(this,
                                                                aTransport,
                                                                aSocketIn,
                                                                aSocketOut));
  }

  LOG(("WebSocketChannel::OnTransportAvailable %p [%p %p %p] rcvdonstart=%d\n",
       this, aTransport, aSocketIn, aSocketOut, mGotUpgradeOK));

  if (mStopped) {
    LOG(("WebSocketChannel::OnTransportAvailable: Already stopped"));
    return NS_OK;
  }

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
  NS_ABORT_IF_FALSE(!mRecvdHttpUpgradeTransport, "OTA duplicated");
  NS_ABORT_IF_FALSE(aSocketIn, "OTA with invalid socketIn");

  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;

  nsresult rv;
  rv = mTransport->SetEventSink(nullptr, nullptr);
  if (NS_FAILED(rv)) return rv;
  rv = mTransport->SetSecurityCallbacks(this);
  if (NS_FAILED(rv)) return rv;

  mRecvdHttpUpgradeTransport = 1;
  if (mGotUpgradeOK)
    return StartWebsocketData();
  return NS_OK;
}

// nsIRequestObserver (from nsIStreamListener)

NS_IMETHODIMP
WebSocketChannel::OnStartRequest(nsIRequest *aRequest,
                                 nsISupports *aContext)
{
  LOG(("WebSocketChannel::OnStartRequest(): %p [%p %p] recvdhttpupgrade=%d\n",
       this, aRequest, aContext, mRecvdHttpUpgradeTransport));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
  NS_ABORT_IF_FALSE(!mGotUpgradeOK, "OTA duplicated");

  if (mOpenTimer) {
    mOpenTimer->Cancel();
    mOpenTimer = nullptr;
  }

  if (mStopped) {
    LOG(("WebSocketChannel::OnStartRequest: Channel Already Done\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return NS_ERROR_CONNECTION_REFUSED;
  }

  nsresult rv;
  uint32_t status;
  char *val, *token;

  rv = mHttpChannel->GetResponseStatus(&status);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::OnStartRequest: No HTTP Response\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return NS_ERROR_CONNECTION_REFUSED;
  }

  LOG(("WebSocketChannel::OnStartRequest: HTTP status %d\n", status));
  if (status != 101) {
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return NS_ERROR_CONNECTION_REFUSED;
  }

  nsAutoCString respUpgrade;
  rv = mHttpChannel->GetResponseHeader(
    NS_LITERAL_CSTRING("Upgrade"), respUpgrade);

  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_ILLEGAL_VALUE;
    if (!respUpgrade.IsEmpty()) {
      val = respUpgrade.BeginWriting();
      while ((token = nsCRT::strtok(val, ", \t", &val))) {
        if (PL_strcasecmp(token, "Websocket") == 0) {
          rv = NS_OK;
          break;
        }
      }
    }
  }

  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::OnStartRequest: "
         "HTTP response header Upgrade: websocket not found\n"));
    AbortSession(NS_ERROR_ILLEGAL_VALUE);
    return rv;
  }

  nsAutoCString respConnection;
  rv = mHttpChannel->GetResponseHeader(
    NS_LITERAL_CSTRING("Connection"), respConnection);

  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_ILLEGAL_VALUE;
    if (!respConnection.IsEmpty()) {
      val = respConnection.BeginWriting();
      while ((token = nsCRT::strtok(val, ", \t", &val))) {
        if (PL_strcasecmp(token, "Upgrade") == 0) {
          rv = NS_OK;
          break;
        }
      }
    }
  }

  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::OnStartRequest: "
         "HTTP response header 'Connection: Upgrade' not found\n"));
    AbortSession(NS_ERROR_ILLEGAL_VALUE);
    return rv;
  }

  nsAutoCString respAccept;
  rv = mHttpChannel->GetResponseHeader(
                       NS_LITERAL_CSTRING("Sec-WebSocket-Accept"),
                       respAccept);

  if (NS_FAILED(rv) ||
    respAccept.IsEmpty() || !respAccept.Equals(mHashedSecret)) {
    LOG(("WebSocketChannel::OnStartRequest: "
         "HTTP response header Sec-WebSocket-Accept check failed\n"));
    LOG(("WebSocketChannel::OnStartRequest: Expected %s received %s\n",
         mHashedSecret.get(), respAccept.get()));
    AbortSession(NS_ERROR_ILLEGAL_VALUE);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // If we sent a sub protocol header, verify the response matches
  // If it does not, set mProtocol to "" so the protocol attribute
  // of the WebSocket JS object reflects that
  if (!mProtocol.IsEmpty()) {
    nsAutoCString respProtocol;
    rv = mHttpChannel->GetResponseHeader(
                         NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"), 
                         respProtocol);
    if (NS_SUCCEEDED(rv)) {
      rv = NS_ERROR_ILLEGAL_VALUE;
      val = mProtocol.BeginWriting();
      while ((token = nsCRT::strtok(val, ", \t", &val))) {
        if (PL_strcasecmp(token, respProtocol.get()) == 0) {
          rv = NS_OK;
          break;
        }
      }

      if (NS_SUCCEEDED(rv)) {
        LOG(("WebsocketChannel::OnStartRequest: subprotocol %s confirmed",
             respProtocol.get()));
        mProtocol = respProtocol;
      } else {
        LOG(("WebsocketChannel::OnStartRequest: "
             "subprotocol [%s] not found - %s returned",
             mProtocol.get(), respProtocol.get()));
        mProtocol.Truncate();
      }
    } else {
      LOG(("WebsocketChannel::OnStartRequest "
                 "subprotocol [%s] not found - none returned",
                 mProtocol.get()));
      mProtocol.Truncate();
    }
  }

  rv = HandleExtensions();
  if (NS_FAILED(rv))
    return rv;

  mGotUpgradeOK = 1;
  if (mRecvdHttpUpgradeTransport)
    return StartWebsocketData();

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannel::OnStopRequest(nsIRequest *aRequest,
                                  nsISupports *aContext,
                                  nsresult aStatusCode)
{
  LOG(("WebSocketChannel::OnStopRequest() %p [%p %p %x]\n",
       this, aRequest, aContext, aStatusCode));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  ReportConnectionTelemetry();

  // This is the end of the HTTP upgrade transaction, the
  // upgraded streams live on

  mChannel = nullptr;
  mHttpChannel = nullptr;
  mLoadGroup = nullptr;
  mCallbacks = nullptr;

  return NS_OK;
}

// nsIInputStreamCallback

NS_IMETHODIMP
WebSocketChannel::OnInputStreamReady(nsIAsyncInputStream *aStream)
{
  LOG(("WebSocketChannel::OnInputStreamReady() %p\n", this));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  if (!mSocketIn) // did we we clean up the socket after scheduling InputReady?
    return NS_OK;
  
  nsRefPtr<nsIStreamListener>    deleteProtector1(mInflateReader);
  nsRefPtr<nsIStringInputStream> deleteProtector2(mInflateStream);

  // this is after the  http upgrade - so we are speaking websockets
  char  buffer[2048];
  uint32_t count;
  nsresult rv;

  do {
    rv = mSocketIn->Read((char *)buffer, 2048, &count);
    LOG(("WebSocketChannel::OnInputStreamReady: read %u rv %x\n", count, rv));

    // accumulate received bytes
    CountRecvBytes(count);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      mTCPClosed = true;
      AbortSession(rv);
      return rv;
    }

    if (count == 0) {
      mTCPClosed = true;
      AbortSession(NS_BASE_STREAM_CLOSED);
      return NS_OK;
    }

    if (mStopped) {
      continue;
    }

    if (mInflateReader) {
      mInflateStream->ShareData(buffer, count);
      rv = mInflateReader->OnDataAvailable(nullptr, mSocketIn, mInflateStream, 
                                           0, count);
    } else {
      rv = ProcessInput((uint8_t *)buffer, count);
    }

    if (NS_FAILED(rv)) {
      AbortSession(rv);
      return rv;
    }
  } while (NS_SUCCEEDED(rv) && mSocketIn);

  return NS_OK;
}


// nsIOutputStreamCallback

NS_IMETHODIMP
WebSocketChannel::OnOutputStreamReady(nsIAsyncOutputStream *aStream)
{
  LOG(("WebSocketChannel::OnOutputStreamReady() %p\n", this));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  nsresult rv;

  if (!mCurrentOut)
    PrimeNewOutgoingMessage();

  while (mCurrentOut && mSocketOut) {
    const char *sndBuf;
    uint32_t toSend;
    uint32_t amtSent;

    if (mHdrOut) {
      sndBuf = (const char *)mHdrOut;
      toSend = mHdrOutToSend;
      LOG(("WebSocketChannel::OnOutputStreamReady: "
           "Try to send %u of hdr/copybreak\n", toSend));
    } else {
      sndBuf = (char *) mCurrentOut->BeginReading() + mCurrentOutSent;
      toSend = mCurrentOut->Length() - mCurrentOutSent;
      if (toSend > 0) {
        LOG(("WebSocketChannel::OnOutputStreamReady: "
             "Try to send %u of data\n", toSend));
      }
    }

    if (toSend == 0) {
      amtSent = 0;
    } else {
      rv = mSocketOut->Write(sndBuf, toSend, &amtSent);
      LOG(("WebSocketChannel::OnOutputStreamReady: write %u rv %x\n",
           amtSent, rv));

      // accumulate sent bytes
      CountSentBytes(amtSent);

      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        mSocketOut->AsyncWait(this, 0, 0, mSocketThread);
        return NS_OK;
      }

      if (NS_FAILED(rv)) {
        AbortSession(rv);
        return NS_OK;
      }
    }

    if (mHdrOut) {
      if (amtSent == toSend) {
        mHdrOut = nullptr;
        mHdrOutToSend = 0;
      } else {
        mHdrOut += amtSent;
        mHdrOutToSend -= amtSent;
      }
    } else {
      if (amtSent == toSend) {
        if (!mStopped) {
          mTargetThread->Dispatch(new CallAcknowledge(this,
                                                      mCurrentOut->Length()),
                                  NS_DISPATCH_NORMAL);
        }
        DeleteCurrentOutGoingMessage();
        PrimeNewOutgoingMessage();
      } else {
        mCurrentOutSent += amtSent;
      }
    }
  }

  if (mReleaseOnTransmit)
    ReleaseSession();
  return NS_OK;
}

// nsIStreamListener

NS_IMETHODIMP
WebSocketChannel::OnDataAvailable(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsIInputStream *aInputStream,
                                    uint64_t aOffset,
                                    uint32_t aCount)
{
  LOG(("WebSocketChannel::OnDataAvailable() %p [%p %p %p %llu %u]\n",
         this, aRequest, aContext, aInputStream, aOffset, aCount));

  if (aContext == mSocketIn) {
    // This is the deflate decoder

    LOG(("WebSocketChannel::OnDataAvailable: Deflate Data %u\n",
             aCount));

    uint8_t  buffer[2048];
    uint32_t maxRead;
    uint32_t count;
    nsresult rv = NS_OK;  // aCount always > 0, so this just avoids warning

    while (aCount > 0) {
      if (mStopped)
        return NS_BASE_STREAM_CLOSED;

      maxRead = std::min(2048U, aCount);
      rv = aInputStream->Read((char *)buffer, maxRead, &count);
      LOG(("WebSocketChannel::OnDataAvailable: InflateRead read %u rv %x\n",
           count, rv));
      if (NS_FAILED(rv) || count == 0) {
        AbortSession(NS_ERROR_UNEXPECTED);
        break;
      }

      aCount -= count;
      rv = ProcessInput(buffer, count);
      if (NS_FAILED(rv)) {
        AbortSession(rv);
        break;
      }
    }
    return rv;
  }

  if (aContext == mSocketOut) {
    // This is the deflate encoder

    uint32_t maxRead;
    uint32_t count;
    nsresult rv;

    while (aCount > 0) {
      if (mStopped)
        return NS_BASE_STREAM_CLOSED;

      maxRead = std::min(2048U, aCount);
      EnsureHdrOut(mHdrOutToSend + aCount);
      rv = aInputStream->Read((char *)mHdrOut + mHdrOutToSend, maxRead, &count);
      LOG(("WebSocketChannel::OnDataAvailable: DeflateWrite read %u rv %x\n", 
           count, rv));
      if (NS_FAILED(rv) || count == 0) {
        AbortSession(rv);
        break;
      }

      mHdrOutToSend += count;
      aCount -= count;
    }
    return NS_OK;
  }


  // Otherwise, this is the HTTP OnDataAvailable Method, which means
  // this is http data in response to the upgrade request and
  // there should be no http response body if the upgrade succeeded

  // This generally should be caught by a non 101 response code in
  // OnStartRequest().. so we can ignore the data here

  LOG(("WebSocketChannel::OnDataAvailable: HTTP data unexpected len>=%u\n",
         aCount));

  return NS_OK;
}

nsresult
WebSocketChannel::SaveNetworkStats(bool enforce)
{
#ifdef MOZ_WIDGET_GONK
  // Check if the active network and app id are valid.
  if(!mActiveNetwork || mAppId == NECKO_NO_APP_ID) {
    return NS_OK;
  }

  if (mCountRecv <= 0 && mCountSent <= 0) {
    // There is no traffic, no need to save.
    return NS_OK;
  }

  // If |enforce| is false, the traffic amount is saved
  // only when the total amount exceeds the predefined
  // threshold.
  uint64_t totalBytes = mCountRecv + mCountSent;
  if (!enforce && totalBytes < NETWORK_STATS_THRESHOLD) {
    return NS_OK;
  }

  // Create the event to save the network statistics.
  // the event is then dispathed to the main thread.
  nsRefPtr<nsRunnable> event =
    new SaveNetworkStatsEvent(mAppId, mActiveNetwork,
                              mCountRecv, mCountSent, false);
  NS_DispatchToMainThread(event);

  // Reset the counters after saving.
  mCountSent = 0;
  mCountRecv = 0;

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

} // namespace mozilla::net
} // namespace mozilla
