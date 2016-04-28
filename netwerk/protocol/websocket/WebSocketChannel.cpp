/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrame.h"
#include "WebSocketLog.h"
#include "WebSocketChannel.h"

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Endian.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/net/WebSocketEventService.h"

#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsICryptoHash.h"
#include "nsIRunnable.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsICancelable.h"
#include "nsIClassOfService.h"
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
#include "nsINetworkLinkService.h"
#include "nsIObserverService.h"

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
#include "nsINode.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsSocketTransportService2.h"

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
                  nsIThreadRetargetableRequest,
                  nsIObserver)

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
              LOG(("WebSocket: delaying websocket [this=%p] by %lu ms, changing"
                   " state to CONNECTING_DELAYED", ws,
                   (unsigned long)remainingDelay));
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
    ws->BeginOpen(true);
  }

  // Remove() also deletes all expired entries as it iterates: better for
  // battery life than using a periodic timer.
  void Remove(nsCString &address, int32_t port)
  {
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
    LOG(("Websocket: ConditionallyConnect: [this=%p]", ws));
    MOZ_ASSERT(NS_IsMainThread(), "not main thread");
    MOZ_ASSERT(ws->mConnecting == NOT_CONNECTING, "opening state");

    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }

    // If there is already another WS channel connecting to this IP address,
    // defer BeginOpen and mark as waiting in queue.
    bool found = (sManager->IndexOf(ws->mAddress) >= 0);

    // Always add ourselves to queue, even if we'll connect immediately
    nsOpenConn *newdata = new nsOpenConn(ws->mAddress, ws);
    LOG(("Websocket: adding conn %p to the queue", newdata));
    sManager->mQueue.AppendElement(newdata);

    if (found) {
      LOG(("Websocket: some other channel is connecting, changing state to "
           "CONNECTING_QUEUED"));
      ws->mConnecting = CONNECTING_QUEUED;
    } else {
      sManager->mFailures.DelayOrBegin(ws);
    }
  }

  static void OnConnected(WebSocketChannel *aChannel)
  {
    LOG(("Websocket: OnConnected: [this=%p]", aChannel));

    MOZ_ASSERT(NS_IsMainThread(), "not main thread");
    MOZ_ASSERT(aChannel->mConnecting == CONNECTING_IN_PROGRESS,
               "Channel completed connect, but not connecting?");

    StaticMutexAutoLock lock(sLock);
    if (!sManager) {
      return;
    }

    LOG(("Websocket: changing state to NOT_CONNECTING"));
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
    LOG(("Websocket: OnStopSession: [this=%p, reason=0x%08x]", aChannel,
         aReason));

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
      MOZ_ASSERT(NS_IsMainThread(), "not main thread");

      // Only way a connecting channel may get here w/o failing is if it was
      // closed with GOING_AWAY (1001) because of navigation, tab close, etc.
      MOZ_ASSERT(NS_FAILED(aReason) ||
                 aChannel->mScriptCloseCode == CLOSE_GOING_AWAY,
                 "websocket closed while connecting w/o failing?");

      sManager->RemoveFromQueue(aChannel);

      bool wasNotQueued = (aChannel->mConnecting != CONNECTING_QUEUED);
      LOG(("Websocket: changing state to NOT_CONNECTING"));
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
    MOZ_ASSERT(NS_IsMainThread(), "not main thread");

    int32_t index = IndexOf(hostName);
    if (index >= 0) {
      WebSocketChannel *chan = mQueue[index]->mChannel;

      MOZ_ASSERT(chan->mConnecting == CONNECTING_QUEUED,
                 "transaction not queued but in queue");
      LOG(("WebSocket: ConnectNext: found channel [this=%p] in queue", chan));

      mFailures.DelayOrBegin(chan);
    }
  }

  void RemoveFromQueue(WebSocketChannel *aChannel)
  {
    LOG(("Websocket: RemoveFromQueue: [this=%p]", aChannel));
    int32_t index = IndexOf(aChannel);
    MOZ_ASSERT(index >= 0, "connection to remove not in queue");
    if (index >= 0) {
      nsOpenConn *olddata = mQueue[index];
      mQueue.RemoveElementAt(index);
      LOG(("Websocket: removing conn %p from the queue", olddata));
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

class CallOnMessageAvailable final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnMessageAvailable(WebSocketChannel* aChannel,
                         nsACString& aData,
                         int32_t aLen)
    : mChannel(aChannel),
      mListenerMT(aChannel->mListenerMT),
      mData(aData),
      mLen(aLen) {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mChannel->IsOnTargetThread());

    if (mListenerMT) {
      if (mLen < 0) {
        mListenerMT->mListener->OnMessageAvailable(mListenerMT->mContext,
                                                   mData);
      } else {
        mListenerMT->mListener->OnBinaryMessageAvailable(mListenerMT->mContext,
                                                         mData);
      }
    }

    return NS_OK;
  }

private:
  ~CallOnMessageAvailable() {}

  RefPtr<WebSocketChannel> mChannel;
  RefPtr<BaseWebSocketChannel::ListenerAndContextContainer> mListenerMT;
  nsCString mData;
  int32_t mLen;
};
NS_IMPL_ISUPPORTS(CallOnMessageAvailable, nsIRunnable)

//-----------------------------------------------------------------------------
// CallOnStop
//-----------------------------------------------------------------------------

class CallOnStop final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnStop(WebSocketChannel* aChannel,
             nsresult aReason)
    : mChannel(aChannel),
      mListenerMT(mChannel->mListenerMT),
      mReason(aReason)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mChannel->IsOnTargetThread());

    if (mListenerMT) {
      mListenerMT->mListener->OnStop(mListenerMT->mContext, mReason);
      mChannel->mListenerMT = nullptr;
    }

    return NS_OK;
  }

private:
  ~CallOnStop() {}

  RefPtr<WebSocketChannel> mChannel;
  RefPtr<BaseWebSocketChannel::ListenerAndContextContainer> mListenerMT;
  nsresult mReason;
};
NS_IMPL_ISUPPORTS(CallOnStop, nsIRunnable)

//-----------------------------------------------------------------------------
// CallOnServerClose
//-----------------------------------------------------------------------------

class CallOnServerClose final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CallOnServerClose(WebSocketChannel* aChannel,
                    uint16_t aCode,
                    nsACString& aReason)
    : mChannel(aChannel),
      mListenerMT(mChannel->mListenerMT),
      mCode(aCode),
      mReason(aReason) {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mChannel->IsOnTargetThread());

    if (mListenerMT) {
      mListenerMT->mListener->OnServerClose(mListenerMT->mContext, mCode,
                                            mReason);
    }
    return NS_OK;
  }

private:
  ~CallOnServerClose() {}

  RefPtr<WebSocketChannel> mChannel;
  RefPtr<BaseWebSocketChannel::ListenerAndContextContainer> mListenerMT;
  uint16_t mCode;
  nsCString mReason;
};
NS_IMPL_ISUPPORTS(CallOnServerClose, nsIRunnable)

//-----------------------------------------------------------------------------
// CallAcknowledge
//-----------------------------------------------------------------------------

class CallAcknowledge final : public CancelableRunnable
{
public:
  CallAcknowledge(WebSocketChannel* aChannel,
                  uint32_t aSize)
    : mChannel(aChannel),
      mListenerMT(mChannel->mListenerMT),
      mSize(aSize) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(mChannel->IsOnTargetThread());

    LOG(("WebSocketChannel::CallAcknowledge: Size %u\n", mSize));
    if (mListenerMT) {
      mListenerMT->mListener->OnAcknowledge(mListenerMT->mContext, mSize);
    }
    return NS_OK;
  }

private:
  ~CallAcknowledge() {}

  RefPtr<WebSocketChannel> mChannel;
  RefPtr<BaseWebSocketChannel::ListenerAndContextContainer> mListenerMT;
  uint32_t mSize;
};

//-----------------------------------------------------------------------------
// CallOnTransportAvailable
//-----------------------------------------------------------------------------

class CallOnTransportAvailable final : public nsIRunnable
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

  NS_IMETHOD Run() override
  {
    LOG(("WebSocketChannel::CallOnTransportAvailable %p\n", this));
    return mChannel->OnTransportAvailable(mTransport, mSocketIn, mSocketOut);
  }

private:
  ~CallOnTransportAvailable() {}

  RefPtr<WebSocketChannel>     mChannel;
  nsCOMPtr<nsISocketTransport>   mTransport;
  nsCOMPtr<nsIAsyncInputStream>  mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
};
NS_IMPL_ISUPPORTS(CallOnTransportAvailable, nsIRunnable)

//-----------------------------------------------------------------------------
// PMCECompression
//-----------------------------------------------------------------------------

class PMCECompression
{
public:
  PMCECompression(bool aNoContextTakeover,
                  int32_t aClientMaxWindowBits,
                  int32_t aServerMaxWindowBits)
    : mActive(false)
    , mNoContextTakeover(aNoContextTakeover)
    , mResetDeflater(false)
    , mMessageDeflated(false)
  {
    MOZ_COUNT_CTOR(PMCECompression);

    mDeflater.zalloc = mInflater.zalloc = Z_NULL;
    mDeflater.zfree  = mInflater.zfree  = Z_NULL;
    mDeflater.opaque = mInflater.opaque = Z_NULL;

    if (deflateInit2(&mDeflater, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     -aClientMaxWindowBits, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
      if (inflateInit2(&mInflater, -aServerMaxWindowBits) == Z_OK) {
        mActive = true;
      } else {
        deflateEnd(&mDeflater);
      }
    }
  }

  ~PMCECompression()
  {
    MOZ_COUNT_DTOR(PMCECompression);

    if (mActive) {
      inflateEnd(&mInflater);
      deflateEnd(&mDeflater);
    }
  }

  bool Active()
  {
    return mActive;
  }

  void SetMessageDeflated()
  {
    MOZ_ASSERT(!mMessageDeflated);
    mMessageDeflated = true;
  }
  bool IsMessageDeflated()
  {
    return mMessageDeflated;
  }

  bool UsingContextTakeover()
  {
    return !mNoContextTakeover;
  }

  nsresult Deflate(uint8_t *data, uint32_t dataLen, nsACString &_retval)
  {
    if (mResetDeflater || mNoContextTakeover) {
      if (deflateReset(&mDeflater) != Z_OK) {
        return NS_ERROR_UNEXPECTED;
      }
      mResetDeflater = false;
    }

    mDeflater.avail_out = kBufferLen;
    mDeflater.next_out = mBuffer;
    mDeflater.avail_in = dataLen;
    mDeflater.next_in = data;

    while (true) {
      int zerr = deflate(&mDeflater, Z_SYNC_FLUSH);

      if (zerr != Z_OK) {
        mResetDeflater = true;
        return NS_ERROR_UNEXPECTED;
      }

      uint32_t deflated = kBufferLen - mDeflater.avail_out;
      if (deflated > 0) {
        _retval.Append(reinterpret_cast<char *>(mBuffer), deflated);
      }

      mDeflater.avail_out = kBufferLen;
      mDeflater.next_out = mBuffer;

      if (mDeflater.avail_in > 0) {
        continue; // There is still some data to deflate
      }

      if (deflated == kBufferLen) {
        continue; // There was not enough space in the buffer
      }

      break;
    }

    if (_retval.Length() < 4) {
      MOZ_ASSERT(false, "Expected trailing not found in deflated data!");
      mResetDeflater = true;
      return NS_ERROR_UNEXPECTED;
    }

    _retval.Truncate(_retval.Length() - 4);

    return NS_OK;
  }

  nsresult Inflate(uint8_t *data, uint32_t dataLen, nsACString &_retval)
  {
    mMessageDeflated = false;

    Bytef trailingData[] = { 0x00, 0x00, 0xFF, 0xFF };
    bool trailingDataUsed = false;

    mInflater.avail_out = kBufferLen;
    mInflater.next_out = mBuffer;
    mInflater.avail_in = dataLen;
    mInflater.next_in = data;

    while (true) {
      int zerr = inflate(&mInflater, Z_NO_FLUSH);

      if (zerr == Z_STREAM_END) {
        Bytef *saveNextIn = mInflater.next_in;
        uint32_t saveAvailIn = mInflater.avail_in;
        Bytef *saveNextOut = mInflater.next_out;
        uint32_t saveAvailOut = mInflater.avail_out;

        inflateReset(&mInflater);

        mInflater.next_in = saveNextIn;
        mInflater.avail_in = saveAvailIn;
        mInflater.next_out = saveNextOut;
        mInflater.avail_out = saveAvailOut;
      } else if (zerr != Z_OK && zerr != Z_BUF_ERROR) {
        return NS_ERROR_INVALID_CONTENT_ENCODING;
      }

      uint32_t inflated = kBufferLen - mInflater.avail_out;
      if (inflated > 0) {
        _retval.Append(reinterpret_cast<char *>(mBuffer), inflated);
      }

      mInflater.avail_out = kBufferLen;
      mInflater.next_out = mBuffer;

      if (mInflater.avail_in > 0) {
        continue; // There is still some data to inflate
      }

      if (inflated == kBufferLen) {
        continue; // There was not enough space in the buffer
      }

      if (!trailingDataUsed) {
        trailingDataUsed = true;
        mInflater.avail_in = sizeof(trailingData);
        mInflater.next_in = trailingData;
        continue;
      }

      return NS_OK;
    }
  }

private:
  bool                  mActive;
  bool                  mNoContextTakeover;
  bool                  mResetDeflater;
  bool                  mMessageDeflated;
  z_stream              mDeflater;
  z_stream              mInflater;
  const static uint32_t kBufferLen = 4096;
  uint8_t               mBuffer[kBufferLen];
};

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
    : mMsgType(type), mDeflated(false), mOrigLength(0)
  {
    MOZ_COUNT_CTOR(OutboundMessage);
    mMsg.pString.mValue = str;
    mMsg.pString.mOrigValue = nullptr;
    mLength = str ? str->Length() : 0;
  }

  OutboundMessage(nsIInputStream *stream, uint32_t length)
    : mMsgType(kMsgTypeStream), mLength(length), mDeflated(false)
    , mOrigLength(0)
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
        delete mMsg.pString.mValue;
        if (mMsg.pString.mOrigValue)
          delete mMsg.pString.mOrigValue;
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
  int32_t OrigLength() const { return mDeflated ? mOrigLength : mLength; }

  uint8_t* BeginWriting() {
    MOZ_ASSERT(mMsgType != kMsgTypeStream,
               "Stream should have been converted to string by now");
    return (uint8_t *)(mMsg.pString.mValue ? mMsg.pString.mValue->BeginWriting() : nullptr);
  }

  uint8_t* BeginReading() {
    MOZ_ASSERT(mMsgType != kMsgTypeStream,
               "Stream should have been converted to string by now");
    return (uint8_t *)(mMsg.pString.mValue ? mMsg.pString.mValue->BeginReading() : nullptr);
  }

  uint8_t* BeginOrigReading() {
    MOZ_ASSERT(mMsgType != kMsgTypeStream,
               "Stream should have been converted to string by now");
    if (!mDeflated)
      return BeginReading();
    return (uint8_t *)(mMsg.pString.mOrigValue ? mMsg.pString.mOrigValue->BeginReading() : nullptr);
  }

  nsresult ConvertStreamToString()
  {
    MOZ_ASSERT(mMsgType == kMsgTypeStream, "Not a stream!");

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
    mMsg.pString.mValue = temp.forget();
    mMsg.pString.mOrigValue = nullptr;
    mMsgType = kMsgTypeBinaryString;

    return NS_OK;
  }

  bool DeflatePayload(PMCECompression *aCompressor)
  {
    MOZ_ASSERT(mMsgType != kMsgTypeStream,
               "Stream should have been converted to string by now");
    MOZ_ASSERT(!mDeflated);

    nsresult rv;

    if (mLength == 0) {
      // Empty message
      return false;
    }

    nsAutoPtr<nsCString> temp(new nsCString());
    rv = aCompressor->Deflate(BeginReading(), mLength, *temp);
    if (NS_FAILED(rv)) {
      LOG(("WebSocketChannel::OutboundMessage: Deflating payload failed "
           "[rv=0x%08x]\n", rv));
      return false;
    }

    if (!aCompressor->UsingContextTakeover() && temp->Length() > mLength) {
      // When "client_no_context_takeover" was negotiated, do not send deflated
      // payload if it's larger that the original one. OTOH, it makes sense
      // to send the larger deflated payload when the sliding window is not
      // reset between messages because if we would skip some deflated block
      // we would need to empty the sliding window which could affect the
      // compression of the subsequent messages.
      LOG(("WebSocketChannel::OutboundMessage: Not deflating message since the "
           "deflated payload is larger than the original one [deflated=%d, "
           "original=%d]", temp->Length(), mLength));
      return false;
    }

    mOrigLength = mLength;
    mDeflated = true;
    mLength = temp->Length();
    mMsg.pString.mOrigValue = mMsg.pString.mValue;
    mMsg.pString.mValue = temp.forget();
    return true;
  }

private:
  union {
    struct {
      nsCString *mValue;
      nsCString *mOrigValue;
    } pString;
    nsIInputStream *pStream;
  }                           mMsg;
  WsMsgType                   mMsgType;
  uint32_t                    mLength;
  bool                        mDeflated;
  uint32_t                    mOrigLength;
};

//-----------------------------------------------------------------------------
// OutboundEnqueuer
//-----------------------------------------------------------------------------

class OutboundEnqueuer final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  OutboundEnqueuer(WebSocketChannel *aChannel, OutboundMessage *aMsg)
    : mChannel(aChannel), mMessage(aMsg) {}

  NS_IMETHOD Run() override
  {
    mChannel->EnqueueOutgoingMessage(mChannel->mOutgoingMessages, mMessage);
    return NS_OK;
  }

private:
  ~OutboundEnqueuer() {}

  RefPtr<WebSocketChannel>  mChannel;
  OutboundMessage            *mMessage;
};
NS_IMPL_ISUPPORTS(OutboundEnqueuer, nsIRunnable)


//-----------------------------------------------------------------------------
// WebSocketChannel
//-----------------------------------------------------------------------------

WebSocketChannel::WebSocketChannel() :
  mPort(0),
  mCloseTimeout(20000),
  mOpenTimeout(20000),
  mConnecting(NOT_CONNECTING),
  mMaxConcurrentConnections(200),
  mGotUpgradeOK(0),
  mRecvdHttpUpgradeTransport(0),
  mAutoFollowRedirects(0),
  mAllowPMCE(1),
  mPingOutstanding(0),
  mReleaseOnTransmit(0),
  mDataStarted(0),
  mRequestedClose(0),
  mClientClosed(0),
  mServerClosed(0),
  mStopped(0),
  mCalledOnStop(0),
  mTCPClosed(0),
  mOpenedHttpChannel(0),
  mIncrementedSessionCount(0),
  mDecrementedSessionCount(0),
  mMaxMessageSize(INT32_MAX),
  mStopOnClose(NS_OK),
  mServerCloseCode(CLOSE_ABNORMAL),
  mScriptCloseCode(0),
  mFragmentOpcode(nsIWebSocketFrame::OPCODE_CONTINUATION),
  mFragmentAccumulator(0),
  mBuffered(0),
  mBufferSize(kIncomingBufferInitialSize),
  mCurrentOut(nullptr),
  mCurrentOutSent(0),
  mDynamicOutputSize(0),
  mDynamicOutput(nullptr),
  mPrivateBrowsing(false),
  mConnectionLogService(nullptr),
  mCountRecv(0),
  mCountSent(0),
  mAppId(NECKO_NO_APP_ID),
  mIsInIsolatedMozBrowser(false)
{
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannel::WebSocketChannel() %p\n", this));

  nsWSAdmissionManager::Init();

  mFramePtr = mBuffer = static_cast<uint8_t *>(moz_xmalloc(mBufferSize));

  nsresult rv;
  mConnectionLogService = do_GetService("@mozilla.org/network/dashboard;1",&rv);
  if (NS_FAILED(rv))
    LOG(("Failed to initiate dashboard service."));

  mService = WebSocketEventService::GetOrCreate();
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

  free(mBuffer);
  free(mDynamicOutput);
  delete mCurrentOut;

  while ((mCurrentOut = (OutboundMessage *) mOutgoingPingMessages.PopFront()))
    delete mCurrentOut;
  while ((mCurrentOut = (OutboundMessage *) mOutgoingPongMessages.PopFront()))
    delete mCurrentOut;
  while ((mCurrentOut = (OutboundMessage *) mOutgoingMessages.PopFront()))
    delete mCurrentOut;

  NS_ReleaseOnMainThread(mURI.forget());
  NS_ReleaseOnMainThread(mOriginalURI.forget());

  mListenerMT = nullptr;

  NS_ReleaseOnMainThread(mLoadGroup.forget());
  NS_ReleaseOnMainThread(mLoadInfo.forget());
  NS_ReleaseOnMainThread(mService.forget());
}

NS_IMETHODIMP
WebSocketChannel::Observe(nsISupports *subject,
                          const char *topic,
                          const char16_t *data)
{
  LOG(("WebSocketChannel::Observe [topic=\"%s\"]\n", topic));

  if (strcmp(topic, NS_NETWORK_LINK_TOPIC) == 0) {
    nsCString converted = NS_ConvertUTF16toUTF8(data);
    const char *state = converted.get();

    if (strcmp(state, NS_NETWORK_LINK_DATA_CHANGED) == 0) {
      LOG(("WebSocket: received network CHANGED event"));

      if (!mSocketThread) {
        // there has not been an asyncopen yet on the object and then we need
        // no ping.
        LOG(("WebSocket: early object, no ping needed"));
      } else {
        // Next we check mDataStarted, which we need to do on mTargetThread.
        if (!IsOnTargetThread()) {
          mTargetThread->Dispatch(
            NewRunnableMethod(this, &WebSocketChannel::OnNetworkChanged),
            NS_DISPATCH_NORMAL);
        } else {
          OnNetworkChanged();
        }
      }
    }
  }

  return NS_OK;
}

nsresult
WebSocketChannel::OnNetworkChanged()
{
  if (IsOnTargetThread()) {
    LOG(("WebSocketChannel::OnNetworkChanged() - on target thread %p", this));

    if (!mDataStarted) {
      LOG(("WebSocket: data not started yet, no ping needed"));
      return NS_OK;
    }

    return mSocketThread->Dispatch(
      NewRunnableMethod(this, &WebSocketChannel::OnNetworkChanged),
      NS_DISPATCH_NORMAL);
  }

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  LOG(("WebSocketChannel::OnNetworkChanged() - on socket thread %p", this));

  if (mPingOutstanding) {
    // If there's an outstanding ping that's expected to get a pong back
    // we let that do its thing.
    LOG(("WebSocket: pong already pending"));
    return NS_OK;
  }

  if (mPingForced) {
    // avoid more than one
    LOG(("WebSocket: forced ping timer already fired"));
    return NS_OK;
  }

  LOG(("nsWebSocketChannel:: Generating Ping as network changed\n"));

  if (!mPingTimer) {
    // The ping timer is only conditionally running already. If it wasn't
    // already created do it here.
    nsresult rv;
    mPingTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      LOG(("WebSocket: unable to create ping timer!"));
      NS_WARNING("unable to create ping timer!");
      return rv;
    }
  }
  // Trigger the ping timeout asap to fire off a new ping. Wait just
  // a little bit to better avoid multi-triggers.
  mPingForced = 1;
  mPingTimer->InitWithCallback(this, 200, nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}

void
WebSocketChannel::Shutdown()
{
  nsWSAdmissionManager::Shutdown();
}

bool
WebSocketChannel::IsOnTargetThread()
{
  MOZ_ASSERT(mTargetThread);
  bool isOnTargetThread = false;
  nsresult rv = mTargetThread->IsOnCurrentThread(&isOnTargetThread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_FAILED(rv) ? false : isOnTargetThread;
}

void
WebSocketChannel::GetEffectiveURL(nsAString& aEffectiveURL) const
{
  aEffectiveURL = mEffectiveURL;
}

bool
WebSocketChannel::IsEncrypted() const
{
  return mEncrypted;
}

void
WebSocketChannel::BeginOpen(bool aCalledFromAdmissionManager)
{
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannel::BeginOpen() %p\n", this));

  // Important that we set CONNECTING_IN_PROGRESS before any call to
  // AbortSession here: ensures that any remaining queued connection(s) are
  // scheduled in OnStopSession
  LOG(("Websocket: changing state to CONNECTING_IN_PROGRESS"));
  mConnecting = CONNECTING_IN_PROGRESS;

  if (aCalledFromAdmissionManager) {
    // When called from nsWSAdmissionManager post an event to avoid potential
    // re-entering of nsWSAdmissionManager and its lock.
    NS_DispatchToMainThread(
      NewRunnableMethod(this, &WebSocketChannel::BeginOpenInternal),
                           NS_DISPATCH_NORMAL);
  } else {
    BeginOpenInternal();
  }
}

void
WebSocketChannel::BeginOpenInternal()
{
  LOG(("WebSocketChannel::BeginOpenInternal() %p\n", this));

  nsresult rv;

  if (mRedirectCallback) {
    LOG(("WebSocketChannel::BeginOpenInternal: Resuming Redirect\n"));
    rv = mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
    mRedirectCallback = nullptr;
    return;
  }

  nsCOMPtr<nsIChannel> localChannel = do_QueryInterface(mChannel, &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpenInternal: cannot async open\n"));
    AbortSession(NS_ERROR_UNEXPECTED);
    return;
  }

  if (localChannel) {
    NS_GetAppInfo(localChannel, &mAppId, &mIsInIsolatedMozBrowser);
  }

#ifdef MOZ_WIDGET_GONK
  if (mAppId != NECKO_NO_APP_ID) {
    nsCOMPtr<nsINetworkInfo> activeNetworkInfo;
    GetActiveNetworkInfo(activeNetworkInfo);
    mActiveNetworkInfo =
      new nsMainThreadPtrHolder<nsINetworkInfo>(activeNetworkInfo);
  }
#endif

  rv = localChannel->AsyncOpen(this, mHttpChannel);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpenInternal: cannot async open\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return;
  }
  mOpenedHttpChannel = 1;

  mOpenTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpenInternal: cannot create open timer\n"));
    AbortSession(NS_ERROR_UNEXPECTED);
    return;
  }

  rv = mOpenTimer->InitWithCallback(this, mOpenTimeout,
                                    nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpenInternal: cannot initialize open "
         "timer\n"));
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

  MOZ_ASSERT(IsPersistentFramePtr(), "update read buffer bad mFramePtr");
  MOZ_ASSERT(mFramePtr - accumulatedFragments >= mBuffer,
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
    mBuffer = (uint8_t *)realloc(mBuffer, mBufferSize);
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
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  nsresult rv;

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
    int64_t payloadLength64 = mFramePtr[1] & kPayloadLengthBitsMask;
    uint8_t finBit  = mFramePtr[0] & kFinalFragBit;
    uint8_t rsvBits = mFramePtr[0] & kRsvBitsMask;
    uint8_t rsvBit1 = mFramePtr[0] & kRsv1Bit;
    uint8_t rsvBit2 = mFramePtr[0] & kRsv2Bit;
    uint8_t rsvBit3 = mFramePtr[0] & kRsv3Bit;
    uint8_t opcode  = mFramePtr[0] & kOpcodeBitsMask;
    uint8_t maskBit = mFramePtr[1] & kMaskBit;
    uint32_t mask = 0;

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

      mask = NetworkEndian::readUint32(payload - 4);
      ApplyMask(mask, payload, payloadLength);
    }

    // Control codes are required to have the fin bit set
    if (!finBit && (opcode & kControlFrameMask)) {
      LOG(("WebSocketChannel:: fragmented control frame code %d\n", opcode));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (rsvBits) {
      // PMCE sets RSV1 bit in the first fragment when the non-control frame
      // is deflated
      if (mPMCECompressor && rsvBits == kRsv1Bit && mFragmentAccumulator == 0 &&
          !(opcode & kControlFrameMask)) {
        mPMCECompressor->SetMessageDeflated();
        LOG(("WebSocketChannel::ProcessInput: received deflated frame\n"));
      } else {
        LOG(("WebSocketChannel::ProcessInput: unexpected reserved bits %x\n",
             rsvBits));
        return NS_ERROR_ILLEGAL_VALUE;
      }
    }

    if (!finBit || opcode == nsIWebSocketFrame::OPCODE_CONTINUATION) {
      // This is part of a fragment response

      // Only the first frame has a non zero op code: Make sure we don't see a
      // first frame while some old fragments are open
      if ((mFragmentAccumulator != 0) &&
          (opcode != nsIWebSocketFrame::OPCODE_CONTINUATION)) {
        LOG(("WebSocketChannel:: nested fragments\n"));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      LOG(("WebSocketChannel:: Accumulating Fragment %ld\n", payloadLength));

      if (opcode == nsIWebSocketFrame::OPCODE_CONTINUATION) {

        // Make sure this continuation fragment isn't the first fragment
        if (mFragmentOpcode == nsIWebSocketFrame::OPCODE_CONTINUATION) {
          LOG(("WebSocketHeandler:: continuation code in first fragment\n"));
          return NS_ERROR_ILLEGAL_VALUE;
        }

        // For frag > 1 move the data body back on top of the headers
        // so we have contiguous stream of data
        MOZ_ASSERT(mFramePtr + framingLength == payload,
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
        mFragmentOpcode = nsIWebSocketFrame::OPCODE_CONTINUATION;
      } else {
        opcode = nsIWebSocketFrame::OPCODE_CONTINUATION;
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
    } else if (opcode == nsIWebSocketFrame::OPCODE_TEXT) {
      bool isDeflated = mPMCECompressor && mPMCECompressor->IsMessageDeflated();
      LOG(("WebSocketChannel:: %stext frame received\n",
           isDeflated ? "deflated " : ""));

      if (mListenerMT) {
        nsCString utf8Data;

        if (isDeflated) {
          rv = mPMCECompressor->Inflate(payload, payloadLength, utf8Data);
          if (NS_FAILED(rv)) {
            return rv;
          }
          LOG(("WebSocketChannel:: message successfully inflated "
               "[origLength=%d, newLength=%d]\n", payloadLength,
               utf8Data.Length()));
        } else {
          if (!utf8Data.Assign((const char *)payload, payloadLength,
                               mozilla::fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
        }

        // Section 8.1 says to fail connection if invalid utf-8 in text message
        if (!IsUTF8(utf8Data, false)) {
          LOG(("WebSocketChannel:: text frame invalid utf-8\n"));
          return NS_ERROR_CANNOT_CONVERT_DATA;
        }

        RefPtr<WebSocketFrame> frame =
          mService->CreateFrameIfNeeded(finBit, rsvBit1, rsvBit2, rsvBit3,
                                        opcode, maskBit, mask, utf8Data);

        if (frame) {
          mService->FrameReceived(mSerial, mInnerWindowID, frame.forget());
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

      RefPtr<WebSocketFrame> frame =
        mService->CreateFrameIfNeeded(finBit, rsvBit1, rsvBit2, rsvBit3,
                                      opcode, maskBit, mask, payload,
                                      payloadLength);

      if (opcode == nsIWebSocketFrame::OPCODE_CLOSE) {
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

        if (frame) {
          // We send the frame immediately becuase we want to have it dispatched
          // before the CallOnServerClose.
          mService->FrameReceived(mSerial, mInnerWindowID, frame.forget());
        }

        if (mListenerMT) {
          mTargetThread->Dispatch(new CallOnServerClose(this, mServerCloseCode,
                                                        mServerCloseReason),
                                  NS_DISPATCH_NORMAL);
        }

        if (mClientClosed)
          ReleaseSession();
      } else if (opcode == nsIWebSocketFrame::OPCODE_PING) {
        LOG(("WebSocketChannel:: ping received\n"));
        GeneratePong(payload, payloadLength);
      } else if (opcode == nsIWebSocketFrame::OPCODE_PONG) {
        // opcode OPCODE_PONG: the mere act of receiving the packet is all we
        // need to do for the pong to trigger the activity timers
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
        MOZ_ASSERT(mFramePtr + framingLength == payload,
                   "payload offset from frameptr wrong");
        ::memmove(mFramePtr, payload + payloadLength, avail - payloadLength);
        payload = mFramePtr;
        avail -= payloadLength;
        if (mBuffered)
          mBuffered -= framingLength + payloadLength;
        payloadLength = 0;
      }

      if (frame) {
        mService->FrameReceived(mSerial, mInnerWindowID, frame.forget());
      }
    } else if (opcode == nsIWebSocketFrame::OPCODE_BINARY) {
      bool isDeflated = mPMCECompressor && mPMCECompressor->IsMessageDeflated();
      LOG(("WebSocketChannel:: %sbinary frame received\n",
           isDeflated ? "deflated " : ""));

      if (mListenerMT) {
        nsCString binaryData;

        if (isDeflated) {
          rv = mPMCECompressor->Inflate(payload, payloadLength, binaryData);
          if (NS_FAILED(rv)) {
            return rv;
          }
          LOG(("WebSocketChannel:: message successfully inflated "
               "[origLength=%d, newLength=%d]\n", payloadLength,
               binaryData.Length()));
        } else {
          if (!binaryData.Assign((const char *)payload, payloadLength,
                                 mozilla::fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
        }

        RefPtr<WebSocketFrame> frame =
          mService->CreateFrameIfNeeded(finBit, rsvBit1, rsvBit2, rsvBit3,
                                        opcode, maskBit, mask, binaryData);
        if (frame) {
          mService->FrameReceived(mSerial, mInnerWindowID, frame.forget());
        }

        mTargetThread->Dispatch(
          new CallOnMessageAvailable(this, binaryData, binaryData.Length()),
          NS_DISPATCH_NORMAL);
        // To add the header to 'Networking Dashboard' log
        if (mConnectionLogService && !mPrivateBrowsing) {
          mConnectionLogService->NewMsgReceived(mHost, mSerial, count);
          LOG(("Added new received msg for %s", mHost.get()));
        }
      }
    } else if (opcode != nsIWebSocketFrame::OPCODE_CONTINUATION) {
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
      free(mBuffer);
      mBuffer = (uint8_t *)moz_xmalloc(mBufferSize);
    }
  }
  return NS_OK;
}

/* static */ void
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
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");

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
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  MOZ_ASSERT(!mCurrentOut, "Current message in progress");

  nsresult rv = NS_OK;

  mCurrentOut = (OutboundMessage *)mOutgoingPongMessages.PopFront();
  if (mCurrentOut) {
    MOZ_ASSERT(mCurrentOut->GetMsgType() == kMsgTypePong,
               "Not pong message!");
  } else {
    mCurrentOut = (OutboundMessage *)mOutgoingPingMessages.PopFront();
    if (mCurrentOut)
      MOZ_ASSERT(mCurrentOut->GetMsgType() == kMsgTypePing,
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
    mOutHeader[0] = kFinalFragBit | nsIWebSocketFrame::OPCODE_CLOSE;
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
          MOZ_ASSERT(mScriptCloseReason.Length() <= 123,
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
      mOutHeader[0] = kFinalFragBit | nsIWebSocketFrame::OPCODE_PONG;
      break;
    case kMsgTypePing:
      mOutHeader[0] = kFinalFragBit | nsIWebSocketFrame::OPCODE_PING;
      break;
    case kMsgTypeString:
      mOutHeader[0] = kFinalFragBit | nsIWebSocketFrame::OPCODE_TEXT;
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
      MOZ_FALLTHROUGH;

    case kMsgTypeBinaryString:
      mOutHeader[0] = kFinalFragBit | nsIWebSocketFrame::OPCODE_BINARY;
      break;
    case kMsgTypeFin:
      MOZ_ASSERT(false, "unreachable");  // avoid compiler warning
      break;
    }

    // deflate the payload if PMCE is negotiated
    if (mPMCECompressor &&
        (msgType == kMsgTypeString || msgType == kMsgTypeBinaryString)) {
      if (mCurrentOut->DeflatePayload(mPMCECompressor)) {
        // The payload was deflated successfully, set RSV1 bit
        mOutHeader[0] |= kRsv1Bit;

        LOG(("WebSocketChannel::PrimeNewOutgoingMessage %p current msg %p was "
             "deflated [origLength=%d, newLength=%d].\n", this, mCurrentOut,
             mCurrentOut->OrigLength(), mCurrentOut->Length()));
      }
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

  MOZ_ASSERT(payload, "payload offset not found");

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
    free(buffer);
  } while (!mask);
  NetworkEndian::writeUint32(payload - sizeof(uint32_t), mask);

  LOG(("WebSocketChannel::PrimeNewOutgoingMessage() using mask %08x\n", mask));

  // We don't mask the framing, but occasionally we stick a little payload
  // data in the buffer used for the framing. Close frames are the current
  // example. This data needs to be masked, but it is never more than a
  // handful of bytes and might rotate the mask, so we can just do it locally.
  // For real data frames we ship the bulk of the payload off to ApplyMask()

   RefPtr<WebSocketFrame> frame =
     mService->CreateFrameIfNeeded(
                           mOutHeader[0] & WebSocketChannel::kFinalFragBit,
                           mOutHeader[0] & WebSocketChannel::kRsv1Bit,
                           mOutHeader[0] & WebSocketChannel::kRsv2Bit,
                           mOutHeader[0] & WebSocketChannel::kRsv3Bit,
                           mOutHeader[0] & WebSocketChannel::kOpcodeBitsMask,
                           mOutHeader[1] & WebSocketChannel::kMaskBit,
                           mask,
                           payload, mHdrOutToSend - (payload - mOutHeader),
                           mCurrentOut->BeginOrigReading(),
                           mCurrentOut->OrigLength());

  if (frame) {
    mService->FrameSent(mSerial, mInnerWindowID, frame.forget());
  }

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

  // Transmitting begins - mHdrOutToSend bytes from mOutHeader and
  // mCurrentOut->Length() bytes from mCurrentOut. The latter may be
  // coaleseced into the former for small messages or as the result of the
  // compression process.
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

namespace {

class RemoveObserverRunnable : public Runnable
{
  RefPtr<WebSocketChannel> mChannel;

public:
  explicit RemoveObserverRunnable(WebSocketChannel* aChannel)
    : mChannel(aChannel)
  {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService) {
      NS_WARNING("failed to get observer service");
      return NS_OK;
    }

    observerService->RemoveObserver(mChannel, NS_NETWORK_LINK_TOPIC);
    return NS_OK;
  }
};

} // namespace

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

  // This method can run in any thread, but the observer has to be removed on
  // the main-thread.
  NS_DispatchToMainThread(new RemoveObserverRunnable(this));

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

  mPMCECompressor = nullptr;

  if (!mCalledOnStop) {
    mCalledOnStop = 1;

    nsWSAdmissionManager::OnStopSession(this, reason);

    RefPtr<CallOnStop> runnable = new CallOnStop(this, reason);
    mTargetThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }
}

void
WebSocketChannel::AbortSession(nsresult reason)
{
  LOG(("WebSocketChannel::AbortSession() %p [reason %x] stopped = %d\n",
       this, reason, !!mStopped));

  // normally this should be called on socket thread, but it is ok to call it
  // from the main thread before StartWebsocketData() has completed

  // When we are failing we need to close the TCP connection immediately
  // as per 7.1.1
  mTCPClosed = true;

  if (mLingeringCloseTimer) {
    MOZ_ASSERT(mStopped, "Lingering without Stop");
    LOG(("WebSocketChannel:: Cleanup connection based on TCP Close"));
    CleanupConnection();
    return;
  }

  if (mStopped)
    return;
  mStopped = 1;

  if (mTransport && reason != NS_BASE_STREAM_CLOSED && !mRequestedClose &&
      !mClientClosed && !mServerClosed && mConnecting == NOT_CONNECTING) {
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
       this, !!mStopped));
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");

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

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  rv = mHttpChannel->GetResponseHeader(
    NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"), extensions);
  if (NS_SUCCEEDED(rv)) {
    LOG(("WebSocketChannel::HandleExtensions: received "
         "Sec-WebSocket-Extensions header: %s\n", extensions.get()));

    extensions.CompressWhitespace();

    if (!extensions.IsEmpty()) {
      if (StringBeginsWith(extensions,
                           NS_LITERAL_CSTRING("permessage-deflate"))) {
        if (!mAllowPMCE) {
          LOG(("WebSocketChannel::HandleExtensions: "
               "Recvd permessage-deflate which wasn't offered\n"));
          AbortSession(NS_ERROR_ILLEGAL_VALUE);
          return NS_ERROR_ILLEGAL_VALUE;
        }

        bool skipExtensionName = false;
        bool clientNoContextTakeover = false;
        bool serverNoContextTakeover = false;
        int32_t clientMaxWindowBits = -1;
        int32_t serverMaxWindowBits = -1;

        while (!extensions.IsEmpty()) {
          nsAutoCString paramName;
          nsAutoCString paramValue;

          int32_t delimPos = extensions.FindChar(';');
          if (delimPos != kNotFound) {
            paramName = Substring(extensions, 0, delimPos);
            extensions = Substring(extensions, delimPos + 1);
          } else {
            paramName = extensions;
            extensions.Truncate();
          }
          paramName.CompressWhitespace();
          extensions.CompressWhitespace();

          delimPos = paramName.FindChar('=');
          if (delimPos != kNotFound) {
            paramValue = Substring(paramName, delimPos + 1);
            paramName.Truncate(delimPos);
          }

          if (!skipExtensionName) {
            skipExtensionName = true;
            if (!paramName.EqualsLiteral("permessage-deflate") ||
                !paramValue.IsEmpty()) {
              LOG(("WebSocketChannel::HandleExtensions: HTTP "
                   "Sec-WebSocket-Extensions negotiated unknown extension\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
          } else if (paramName.EqualsLiteral("client_no_context_takeover")) {
            if (!paramValue.IsEmpty()) {
              LOG(("WebSocketChannel::HandleExtensions: parameter "
                   "client_no_context_takeover must not have value, found %s\n",
                   paramValue.get()));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
            if (clientNoContextTakeover) {
              LOG(("WebSocketChannel::HandleExtensions: found multiple "
                   "parameters client_no_context_takeover\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
            clientNoContextTakeover = true;
          } else if (paramName.EqualsLiteral("server_no_context_takeover")) {
            if (!paramValue.IsEmpty()) {
              LOG(("WebSocketChannel::HandleExtensions: parameter "
                   "server_no_context_takeover must not have value, found %s\n",
                   paramValue.get()));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
            if (serverNoContextTakeover) {
              LOG(("WebSocketChannel::HandleExtensions: found multiple "
                   "parameters server_no_context_takeover\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
            serverNoContextTakeover = true;
          } else if (paramName.EqualsLiteral("client_max_window_bits")) {
            if (clientMaxWindowBits != -1) {
              LOG(("WebSocketChannel::HandleExtensions: found multiple "
                   "parameters client_max_window_bits\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }

            nsresult errcode;
            clientMaxWindowBits = paramValue.ToInteger(&errcode);
            if (NS_FAILED(errcode) || clientMaxWindowBits < 8 ||
                clientMaxWindowBits > 15) {
              LOG(("WebSocketChannel::HandleExtensions: found invalid "
                   "parameter client_max_window_bits %s\n", paramValue.get()));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
          } else if (paramName.EqualsLiteral("server_max_window_bits")) {
            if (serverMaxWindowBits != -1) {
              LOG(("WebSocketChannel::HandleExtensions: found multiple "
                   "parameters server_max_window_bits\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }

            nsresult errcode;
            serverMaxWindowBits = paramValue.ToInteger(&errcode);
            if (NS_FAILED(errcode) || serverMaxWindowBits < 8 ||
                serverMaxWindowBits > 15) {
              LOG(("WebSocketChannel::HandleExtensions: found invalid "
                   "parameter server_max_window_bits %s\n", paramValue.get()));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }
          } else {
            LOG(("WebSocketChannel::HandleExtensions: found unknown "
                 "parameter %s\n", paramName.get()));
            AbortSession(NS_ERROR_ILLEGAL_VALUE);
            return NS_ERROR_ILLEGAL_VALUE;
          }
        }

        if (clientMaxWindowBits == -1) {
          clientMaxWindowBits = 15;
        }
        if (serverMaxWindowBits == -1) {
          serverMaxWindowBits = 15;
        }

        mPMCECompressor = new PMCECompression(clientNoContextTakeover,
                                              clientMaxWindowBits,
                                              serverMaxWindowBits);
        if (mPMCECompressor->Active()) {
          LOG(("WebSocketChannel::HandleExtensions: PMCE negotiated, %susing "
               "context takeover, clientMaxWindowBits=%d, "
               "serverMaxWindowBits=%d\n",
               clientNoContextTakeover ? "NOT " : "", clientMaxWindowBits,
               serverMaxWindowBits));

          mNegotiatedExtensions = "permessage-deflate";
        } else {
          LOG(("WebSocketChannel::HandleExtensions: Cannot init PMCE "
               "compression object\n"));
          mPMCECompressor = nullptr;
          AbortSession(NS_ERROR_UNEXPECTED);
          return NS_ERROR_UNEXPECTED;
        }
      } else {
        LOG(("WebSocketChannel::HandleExtensions: "
             "HTTP Sec-WebSocket-Extensions negotiated unknown value %s\n",
             extensions.get()));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }
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
                                  nsIRequest::LOAD_BYPASS_CACHE |
                                  nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
  NS_ENSURE_SUCCESS(rv, rv);

  // we never let websockets be blocked by head CSS/JS loads to avoid
  // potential deadlock where server generation of CSS/JS requires
  // an XHR signal.
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(mChannel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }

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

  if (mAllowPMCE)
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"),
                                   NS_LITERAL_CSTRING("permessage-deflate"),
                                   false);

  uint8_t      *secKey;
  nsAutoCString secKeyString;

  rv = mRandomGenerator->GenerateRandomBytes(16, &secKey);
  NS_ENSURE_SUCCESS(rv, rv);
  char* b64 = PL_Base64Encode((const char *)secKey, 16, nullptr);
  free(secKey);
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

  nsresult rv;
  rv = pps->AsyncResolve(mHttpChannel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                         nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         this, getter_AddRefs(mCancelable));
  NS_ASSERTION(NS_FAILED(rv) || mCancelable,
               "nsIProtocolProxyService::AsyncResolve succeeded but didn't "
               "return a cancelable object!");
  return rv;
}

// Called after both OnStartRequest and OnTransportAvailable have
// executed. This essentially ends the handshake and starts the websockets
// protocol state machine.
nsresult
WebSocketChannel::StartWebsocketData()
{
  nsresult rv;

  if (!IsOnTargetThread()) {
    return mTargetThread->Dispatch(
      NewRunnableMethod(this, &WebSocketChannel::StartWebsocketData),
      NS_DISPATCH_NORMAL);
  }

  LOG(("WebSocketChannel::StartWebsocketData() %p", this));
  MOZ_ASSERT(!mDataStarted, "StartWebsocketData twice");
  mDataStarted = 1;

  rv = mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::StartWebsocketData mSocketIn->AsyncWait() failed "
         "with error 0x%08x", rv));
    return mSocketThread->Dispatch(
      NewRunnableMethod<nsresult>(this,
                                  &WebSocketChannel::AbortSession,
                                  rv),
      NS_DISPATCH_NORMAL);
  }

  if (mPingInterval) {
    rv = mSocketThread->Dispatch(
      NewRunnableMethod(this, &WebSocketChannel::StartPinging),
      NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      LOG(("WebSocketChannel::StartWebsocketData Could not start pinging, "
           "rv=0x%08x", rv));
      return rv;
    }
  }

  LOG(("WebSocketChannel::StartWebsocketData Notifying Listener %p",
       mListenerMT ? mListenerMT->mListener.get() : nullptr));

  if (mListenerMT) {
    mListenerMT->mListener->OnStart(mListenerMT->mContext);
  }

  return NS_OK;
}

nsresult
WebSocketChannel::StartPinging()
{
  LOG(("WebSocketChannel::StartPinging() %p", this));
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  MOZ_ASSERT(mPingInterval);
  MOZ_ASSERT(!mPingTimer);

  nsresult rv;
  mPingTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to create ping timer. Carrying on.");
  } else {
    LOG(("WebSocketChannel will generate ping after %d ms of receive silence\n",
         mPingInterval));
    mPingTimer->InitWithCallback(this, mPingInterval, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
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

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

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
WebSocketChannel::OnProxyAvailable(nsICancelable *aRequest, nsIChannel *aChannel,
                                   nsIProxyInfo *pi, nsresult status)
{
  if (mStopped) {
    LOG(("WebSocketChannel::OnProxyAvailable: [%p] Request Already Stopped\n", this));
    mCancelable = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(!mCancelable || (aRequest == mCancelable));
  mCancelable = nullptr;

  nsAutoCString type;
  if (NS_SUCCEEDED(status) && pi &&
      NS_SUCCEEDED(pi->GetType(type)) &&
      !type.EqualsLiteral("direct")) {
    LOG(("WebSocket OnProxyAvailable [%p] Proxy found skip DNS lookup\n", this));
    // call DNS callback directly without DNS resolver
    OnLookupComplete(nullptr, nullptr, NS_ERROR_FAILURE);
  } else {
    LOG(("WebSocketChannel::OnProxyAvailable[%p] checking DNS resolution\n", this));
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

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

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

    if (!(flags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                   nsIChannelEventSink::REDIRECT_STS_UPGRADE))) {
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
    MOZ_ASSERT(mClientClosed, "Close Timeout without local close");
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread,
               "not socket thread");

    mCloseTimer = nullptr;
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    LOG(("WebSocketChannel:: Expecting Server Close - Timed Out\n"));
    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mOpenTimer) {
    MOZ_ASSERT(!mGotUpgradeOK,
               "Open Timer after open complete");
    MOZ_ASSERT(NS_IsMainThread(), "not main thread");

    mOpenTimer = nullptr;
    LOG(("WebSocketChannel:: Connection Timed Out\n"));
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mReconnectDelayTimer) {
    MOZ_ASSERT(mConnecting == CONNECTING_DELAYED,
               "woke up from delay w/o being delayed?");
    MOZ_ASSERT(NS_IsMainThread(), "not main thread");

    mReconnectDelayTimer = nullptr;
    LOG(("WebSocketChannel: connecting [this=%p] after reconnect delay", this));
    BeginOpen(false);
  } else if (timer == mPingTimer) {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread,
               "not socket thread");

    if (mClientClosed || mServerClosed || mRequestedClose) {
      // no point in worrying about ping now
      mPingTimer = nullptr;
      return NS_OK;
    }

    if (!mPingOutstanding) {
      // Ping interval must be non-null or PING was forced by OnNetworkChanged()
      MOZ_ASSERT(mPingInterval || mPingForced);
      LOG(("nsWebSocketChannel:: Generating Ping\n"));
      mPingOutstanding = 1;
      mPingForced = 0;
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
    MOZ_ASSERT(0, "Unknown Timer");
  }

  return NS_OK;
}

// nsIWebSocketChannel

NS_IMETHODIMP
WebSocketChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG(("WebSocketChannel::GetSecurityInfo() %p\n", this));
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  if (mTransport) {
    if (NS_FAILED(mTransport->GetSecurityInfo(aSecurityInfo)))
      *aSecurityInfo = nullptr;
  }
  return NS_OK;
}


NS_IMETHODIMP
WebSocketChannel::AsyncOpen(nsIURI *aURI,
                            const nsACString &aOrigin,
                            uint64_t aInnerWindowID,
                            nsIWebSocketListener *aListener,
                            nsISupports *aContext)
{
  LOG(("WebSocketChannel::AsyncOpen() %p\n", this));

  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "not main thread");
    LOG(("WebSocketChannel::AsyncOpen() called off the main thread"));
    return NS_ERROR_UNEXPECTED;
  }

  if (!aURI || !aListener) {
    LOG(("WebSocketChannel::AsyncOpen() Uri or Listener null"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mListenerMT || mWasOpened)
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
    rv = prefService->GetBoolPref("network.websocket.extensions.permessage-deflate",
                                  &boolpref);
    if (NS_SUCCEEDED(rv)) {
      mAllowPMCE = boolpref ? 1 : 0;
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
  mInnerWindowID = aInnerWindowID;

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

  // Ideally we'd call newChannelFromURIWithLoadInfo here, but that doesn't
  // allow setting proxy uri/flags
  rv = io2->NewChannelFromURIWithProxyFlags2(
              localURI,
              mURI,
              nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
              nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
              mLoadInfo->LoadingNode() ?
                mLoadInfo->LoadingNode()->AsDOMNode() : nullptr,
              mLoadInfo->LoadingPrincipal(),
              mLoadInfo->TriggeringPrincipal(),
              mLoadInfo->GetSecurityFlags(),
              mLoadInfo->InternalContentPolicyType(),
              getter_AddRefs(localChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Please note that we still call SetLoadInfo on the channel because
  // we want the same instance of the loadInfo to be set on the channel.
  rv = localChannel->SetLoadInfo(mLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Pass most GetInterface() requests through to our instantiator, but handle
  // nsIChannelEventSink in this object in order to deal with redirects
  localChannel->SetNotificationCallbacks(this);

  class MOZ_STACK_CLASS CleanUpOnFailure
  {
  public:
    explicit CleanUpOnFailure(WebSocketChannel* aWebSocketChannel)
      : mWebSocketChannel(aWebSocketChannel)
    {}

    ~CleanUpOnFailure()
    {
      if (!mWebSocketChannel->mWasOpened) {
        mWebSocketChannel->mChannel = nullptr;
        mWebSocketChannel->mHttpChannel = nullptr;
      }
    }

    WebSocketChannel *mWebSocketChannel;
  };

  CleanUpOnFailure cuof(this);

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

  // Register for prefs change notifications
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    NS_WARNING("failed to get observer service");
    return NS_ERROR_FAILURE;
  }

  rv = observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Only set these if the open was successful:
  //
  mWasOpened = 1;
  mListenerMT = new ListenerAndContextContainer(aListener, aContext);
  IncrementSessionCount();

  return rv;
}

NS_IMETHODIMP
WebSocketChannel::Close(uint16_t code, const nsACString & reason)
{
  LOG(("WebSocketChannel::Close() %p\n", this));
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

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

  if (!mTransport || mConnecting != NOT_CONNECTING) {
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
  MOZ_ASSERT(IsOnTargetThread(), "not target thread");

  if (!mDataStarted) {
    LOG(("WebSocketChannel:: Error: data not started yet\n"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mRequestedClose) {
    LOG(("WebSocketChannel:: Error: send when closed\n"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mStopped) {
    LOG(("WebSocketChannel:: Error: send when stopped\n"));
    return NS_ERROR_NOT_CONNECTED;
  }

  MOZ_ASSERT(mMaxMessageSize >= 0, "max message size negative");
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

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  MOZ_ASSERT(!mRecvdHttpUpgradeTransport, "OTA duplicated");
  MOZ_ASSERT(aSocketIn, "OTA with invalid socketIn");

  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;

  nsresult rv;
  rv = mTransport->SetEventSink(nullptr, nullptr);
  if (NS_FAILED(rv)) return rv;
  rv = mTransport->SetSecurityCallbacks(this);
  if (NS_FAILED(rv)) return rv;

  mRecvdHttpUpgradeTransport = 1;
  if (mGotUpgradeOK) {
    // We're now done CONNECTING, which means we can now open another,
    // perhaps parallel, connection to the same host if one
    // is pending
    nsWSAdmissionManager::OnConnected(this);

    return StartWebsocketData();
  }

  return NS_OK;
}

// nsIRequestObserver (from nsIStreamListener)

NS_IMETHODIMP
WebSocketChannel::OnStartRequest(nsIRequest *aRequest,
                                 nsISupports *aContext)
{
  LOG(("WebSocketChannel::OnStartRequest(): %p [%p %p] recvdhttpupgrade=%d\n",
       this, aRequest, aContext, mRecvdHttpUpgradeTransport));
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  MOZ_ASSERT(!mGotUpgradeOK, "OTA duplicated");

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

  // Update mEffectiveURL for off main thread URI access.
  nsCOMPtr<nsIURI> uri = mURI ? mURI : mOriginalURI;
  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  CopyUTF8toUTF16(spec, mEffectiveURL);

  mGotUpgradeOK = 1;
  if (mRecvdHttpUpgradeTransport) {
    // We're now done CONNECTING, which means we can now open another,
    // perhaps parallel, connection to the same host if one
    // is pending
    nsWSAdmissionManager::OnConnected(this);

    return StartWebsocketData();
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannel::OnStopRequest(nsIRequest *aRequest,
                                nsISupports *aContext,
                                nsresult aStatusCode)
{
  LOG(("WebSocketChannel::OnStopRequest() %p [%p %p %x]\n",
       this, aRequest, aContext, aStatusCode));
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

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
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  if (!mSocketIn) // did we we clean up the socket after scheduling InputReady?
    return NS_OK;

  // this is after the http upgrade - so we are speaking websockets
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

    rv = ProcessInput((uint8_t *)buffer, count);
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
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread, "not socket thread");
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
        mSocketOut->AsyncWait(this, 0, 0, mSocketThread);
      }
    } else {
      if (amtSent == toSend) {
        if (!mStopped) {
          mTargetThread->Dispatch(
            new CallAcknowledge(this, mCurrentOut->OrigLength()),
            NS_DISPATCH_NORMAL);
        }
        DeleteCurrentOutGoingMessage();
        PrimeNewOutgoingMessage();
      } else {
        mCurrentOutSent += amtSent;
        mSocketOut->AsyncWait(this, 0, 0, mSocketThread);
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

  // This is the HTTP OnDataAvailable Method, which means this is http data in
  // response to the upgrade request and there should be no http response body
  // if the upgrade succeeded. This generally should be caught by a non 101
  // response code in OnStartRequest().. so we can ignore the data here

  LOG(("WebSocketChannel::OnDataAvailable: HTTP data unexpected len>=%u\n",
         aCount));

  return NS_OK;
}

nsresult
WebSocketChannel::SaveNetworkStats(bool enforce)
{
#ifdef MOZ_WIDGET_GONK
  // Check if the active network and app id are valid.
  if(!mActiveNetworkInfo || mAppId == NECKO_NO_APP_ID) {
    return NS_OK;
  }

  uint64_t countRecv = 0;
  uint64_t countSent = 0;

  mCountRecv.exchange(countRecv);
  mCountSent.exchange(countSent);

  if (countRecv == 0 && countSent == 0) {
    // There is no traffic, no need to save.
    return NS_OK;
  }

  // If |enforce| is false, the traffic amount is saved
  // only when the total amount exceeds the predefined
  // threshold.
  uint64_t totalBytes = countRecv + countSent;
  if (!enforce && totalBytes < NETWORK_STATS_THRESHOLD) {
    return NS_OK;
  }

  // Create the event to save the network statistics.
  // the event is then dispatched to the main thread.
  RefPtr<Runnable> event =
    new SaveNetworkStatsEvent(mAppId, mIsInIsolatedMozBrowser, mActiveNetworkInfo,
                              countRecv, countSent, false);
  NS_DispatchToMainThread(event);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

} // namespace net
} // namespace mozilla

#undef CLOSE_GOING_AWAY
