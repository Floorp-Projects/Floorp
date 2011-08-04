/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wellington Fernando de Macedo <wfernandom2004@gmail.com> (original author)
 *   Patrick McManus <mcmanus@ducksong.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "WebSocketLog.h"
#include "WebSocketChannel.h"

#include "nsISocketTransportService.h"
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

#include "nsAutoPtr.h"
#include "nsStandardURL.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsNetError.h"
#include "nsStringStream.h"
#include "nsAlgorithm.h"
#include "nsProxyRelease.h"

#include "plbase64.h"
#include "prmem.h"
#include "prnetdb.h"
#include "prbit.h"
#include "zlib.h"

extern PRThread *gSocketThread;

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS11(WebSocketChannel,
                               nsIWebSocketChannel,
                               nsIHttpUpgradeListener,
                               nsIRequestObserver,
                               nsIStreamListener,
                               nsIProtocolHandler,
                               nsIInputStreamCallback,
                               nsIOutputStreamCallback,
                               nsITimerCallback,
                               nsIDNSListener,
                               nsIInterfaceRequestor,
                               nsIChannelEventSink)

// Use this fake ptr so the Fin message stays in sequence in the
// main transmit queue
#define kFinMessage (reinterpret_cast<nsCString *>(0x01))

// An implementation of draft-ietf-hybi-thewebsocketprotocol-08
#define SEC_WEBSOCKET_VERSION "8"

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

class CallOnMessageAvailable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  CallOnMessageAvailable(nsIWebSocketListener *aListener,
                         nsISupports          *aContext,
                         nsCString            &aData,
                         PRInt32               aLen)
    : mListener(aListener),
      mContext(aContext),
      mData(aData),
      mLen(aLen) {}

  NS_SCRIPTABLE NS_IMETHOD Run()
  {
    if (mLen < 0)
      mListener->OnMessageAvailable(mContext, mData);
    else
      mListener->OnBinaryMessageAvailable(mContext, mData);
    return NS_OK;
  }

private:
  ~CallOnMessageAvailable() {}

  nsCOMPtr<nsIWebSocketListener>    mListener;
  nsCOMPtr<nsISupports>             mContext;
  nsCString                         mData;
  PRInt32                           mLen;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(CallOnMessageAvailable, nsIRunnable)

class CallOnStop : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  CallOnStop(nsIWebSocketListener *aListener,
             nsISupports          *aContext,
             nsresult              aData)
    : mListener(aListener),
      mContext(aContext),
      mData(aData) {}

  NS_SCRIPTABLE NS_IMETHOD Run()
  {
    mListener->OnStop(mContext, mData);
    return NS_OK;
  }

private:
  ~CallOnStop() {}

  nsCOMPtr<nsIWebSocketListener>    mListener;
  nsCOMPtr<nsISupports>             mContext;
  nsresult                          mData;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(CallOnStop, nsIRunnable)

class CallOnServerClose : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  CallOnServerClose(nsIWebSocketListener *aListener,
                    nsISupports          *aContext,
                    PRUint16              aCode,
                    nsCString            &aReason)
    : mListener(aListener),
      mContext(aContext),
      mCode(aCode),
      mReason(aReason) {}

  NS_SCRIPTABLE NS_IMETHOD Run()
  {
    mListener->OnServerClose(mContext, mCode, mReason);
    return NS_OK;
  }

private:
  ~CallOnServerClose() {}

  nsCOMPtr<nsIWebSocketListener>    mListener;
  nsCOMPtr<nsISupports>             mContext;
  PRUint16                          mCode;
  nsCString                         mReason;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(CallOnServerClose, nsIRunnable)

class CallAcknowledge : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  CallAcknowledge(nsIWebSocketListener *aListener,
                  nsISupports          *aContext,
                  PRUint32              aSize)
    : mListener(aListener),
      mContext(aContext),
      mSize(aSize) {}

  NS_SCRIPTABLE NS_IMETHOD Run()
  {
    LOG(("WebSocketChannel::CallAcknowledge: Size %u\n", mSize));
    mListener->OnAcknowledge(mContext, mSize);
    return NS_OK;
  }

private:
  ~CallAcknowledge() {}

  nsCOMPtr<nsIWebSocketListener>    mListener;
  nsCOMPtr<nsISupports>             mContext;
  PRUint32                          mSize;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(CallAcknowledge, nsIRunnable)

class nsPostMessage : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsPostMessage(WebSocketChannel *channel,
                nsCString        *aData,
                PRInt32           aDataLen)
    : mChannel(channel),
      mData(aData),
      mDataLen(aDataLen) {}

  NS_SCRIPTABLE NS_IMETHOD Run()
  {
    if (mData)
      mChannel->SendMsgInternal(mData, mDataLen);
    return NS_OK;
  }

private:
  ~nsPostMessage() {}

  nsRefPtr<WebSocketChannel>    mChannel;
  nsCString                    *mData;
  PRInt32                       mDataLen;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(nsPostMessage, nsIRunnable)


// Section 5.1 requires that a client rate limit its connects to a single
// TCP session in the CONNECTING state (i.e. anything before the 101 upgrade
// complete response comes back and an open javascript event is created)

class nsWSAdmissionManager
{
public:
  nsWSAdmissionManager() : mConnectedCount(0)
  {
    MOZ_COUNT_CTOR(nsWSAdmissionManager);
  }

  class nsOpenConn
  {
  public:
    nsOpenConn(nsCString &addr, WebSocketChannel *channel)
      : mAddress(addr), mChannel(channel) { MOZ_COUNT_CTOR(nsOpenConn); }
    ~nsOpenConn() { MOZ_COUNT_DTOR(nsOpenConn); }

    nsCString mAddress;
    nsRefPtr<WebSocketChannel> mChannel;
  };

  ~nsWSAdmissionManager()
  {
    MOZ_COUNT_DTOR(nsWSAdmissionManager);
    for (PRUint32 i = 0; i < mData.Length(); i++)
      delete mData[i];
  }

  PRBool ConditionallyConnect(nsCString &aStr, WebSocketChannel *ws)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    // if aStr is not in mData then we return true, else false.
    // in either case aStr is then added to mData - meaning
    // there will be duplicates when this function has been
    // called with the same parameter multiple times.

    // we could hash this, but the dataset is expected to be
    // small

    PRBool found = (IndexOf(aStr) >= 0);
    nsOpenConn *newdata = new nsOpenConn(aStr, ws);
    mData.AppendElement(newdata);

    if (!found)
      ws->BeginOpen();
    return !found;
  }

  PRBool Complete(nsCString &aStr)
  {
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
    PRInt32 index = IndexOf(aStr);
    NS_ABORT_IF_FALSE(index >= 0, "completed connection not in open list");

    nsOpenConn *olddata = mData[index];
    mData.RemoveElementAt(index);
    delete olddata;

    // are there more of the same address pending dispatch?
    index = IndexOf(aStr);
    if (index >= 0) {
      (mData[index])->mChannel->BeginOpen();
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  void IncrementConnectedCount()
  {
    PR_ATOMIC_INCREMENT(&mConnectedCount);
  }

  void DecrementConnectedCount()
  {
    PR_ATOMIC_DECREMENT(&mConnectedCount);
  }

  PRInt32 ConnectedCount()
  {
    return mConnectedCount;
  }

private:
  nsTArray<nsOpenConn *> mData;

  PRInt32 IndexOf(nsCString &aStr)
  {
    for (PRUint32 i = 0; i < mData.Length(); i++)
      if (aStr == (mData[i])->mAddress)
        return i;
    return -1;
  }

  // ConnectedCount might be decremented from the main or the socket
  // thread, so manage it with atomic counters
  PRInt32 mConnectedCount;
};

// similar to nsDeflateConverter except for the mandatory FLUSH calls
// required by websocket and the absence of the deflate termination
// block which is appropriate because it would create data bytes after
// sending the websockets CLOSE message.

class nsWSCompression
{
public:
  nsWSCompression(nsIStreamListener *aListener,
                  nsISupports *aContext)
    : mActive(PR_FALSE),
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
      mActive = PR_TRUE;
    }
  }

  ~nsWSCompression()
  {
    MOZ_COUNT_DTOR(nsWSCompression);

    if (mActive)
      deflateEnd(&mZlib);
  }

  PRBool Active()
  {
    return mActive;
  }

  nsresult Deflate(PRUint8 *buf1, PRUint32 buf1Len,
                   PRUint8 *buf2, PRUint32 buf2Len)
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
    PRUint32 bytesToWrite = kBufferLen - mZlib.avail_out;
    if (bytesToWrite > 0) {
      mStream->ShareData(reinterpret_cast<char *>(mBuffer), bytesToWrite);
      nsresult rv =
        mListener->OnDataAvailable(nsnull, mContext, mStream, 0, bytesToWrite);
      if (NS_FAILED(rv))
        return rv;
    }
    return NS_OK;
  }

  PRBool                          mActive;
  z_stream                        mZlib;
  nsCOMPtr<nsIStringInputStream>  mStream;

  nsISupports                    *mContext;     /* weak ref */
  nsIStreamListener              *mListener;    /* weak ref */

  const static PRInt32            kBufferLen = 4096;
  PRUint8                         mBuffer[kBufferLen];
};

static nsWSAdmissionManager *sWebSocketAdmissions = nsnull;

// WebSocketChannel

WebSocketChannel::WebSocketChannel() :
  mCloseTimeout(20000),
  mOpenTimeout(20000),
  mPingTimeout(0),
  mPingResponseTimeout(10000),
  mMaxConcurrentConnections(200),
  mRecvdHttpOnStartRequest(0),
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
  mMaxMessageSize(16000000),
  mStopOnClose(NS_OK),
  mServerCloseCode(CLOSE_ABNORMAL),
  mScriptCloseCode(0),
  mFragmentOpcode(0),
  mFragmentAccumulator(0),
  mBuffered(0),
  mBufferSize(16384),
  mCurrentOut(nsnull),
  mCurrentOutSent(0),
  mCompressor(nsnull),
  mDynamicOutputSize(0),
  mDynamicOutput(nsnull)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannel::WebSocketChannel() %p\n", this));

  if (!sWebSocketAdmissions)
    sWebSocketAdmissions = new nsWSAdmissionManager();

  mFramePtr = mBuffer = static_cast<PRUint8 *>(moz_xmalloc(mBufferSize));
}

WebSocketChannel::~WebSocketChannel()
{
  LOG(("WebSocketChannel::~WebSocketChannel() %p\n", this));

  // this stop is a nop if the normal connect/close is followed
  mStopped = 1;
  StopSession(NS_ERROR_UNEXPECTED);

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
    NS_ProxyRelease(mainThread, forgettable, PR_FALSE);
  }

  if (mOriginalURI) {
    mOriginalURI.forget(&forgettable);
    NS_ProxyRelease(mainThread, forgettable, PR_FALSE);
  }

  if (mListener) {
    nsIWebSocketListener *forgettableListener;
    mListener.forget(&forgettableListener);
    NS_ProxyRelease(mainThread, forgettableListener, PR_FALSE);
  }

  if (mContext) {
    nsISupports *forgettableContext;
    mContext.forget(&forgettableContext);
    NS_ProxyRelease(mainThread, forgettableContext, PR_FALSE);
  }

  if (mLoadGroup) {
    nsILoadGroup *forgettableGroup;
    mLoadGroup.forget(&forgettableGroup);
    NS_ProxyRelease(mainThread, forgettableGroup, PR_FALSE);
  }
}

void
WebSocketChannel::Shutdown()
{
  delete sWebSocketAdmissions;
  sWebSocketAdmissions = nsnull;
}

nsresult
WebSocketChannel::BeginOpen()
{
  LOG(("WebSocketChannel::BeginOpen() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  nsresult rv;

  if (mRedirectCallback) {
    LOG(("WebSocketChannel::BeginOpen: Resuming Redirect\n"));
    rv = mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
    mRedirectCallback = nsnull;
    return rv;
  }

  nsCOMPtr<nsIChannel> localChannel = do_QueryInterface(mChannel, &rv);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot async open\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return rv;
  }

  rv = localChannel->AsyncOpen(this, mHttpChannel);
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel::BeginOpen: cannot async open\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return rv;
  }

  mOpenTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_SUCCEEDED(rv))
    mOpenTimer->InitWithCallback(this, mOpenTimeout, nsITimer::TYPE_ONE_SHOT);

  return rv;
}

PRBool
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
PRUint32
WebSocketChannel::UpdateReadBuffer(PRUint8 *buffer, PRUint32 count,
                                   PRUint32 accumulatedFragments)
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
    mBufferSize += count + 8192;
    LOG(("WebSocketChannel: update read buffer extended to %u\n", mBufferSize));
    PRUint8 *old = mBuffer;
    mBuffer = (PRUint8 *)moz_xrealloc(mBuffer, mBufferSize);
    mFramePtr = mBuffer + (mFramePtr - old);
  }

  ::memcpy(mBuffer + mBuffered, buffer, count);
  mBuffered += count;

  return mBuffered - (mFramePtr - mBuffer);
}

nsresult
WebSocketChannel::ProcessInput(PRUint8 *buffer, PRUint32 count)
{
  LOG(("WebSocketChannel::ProcessInput %p [%d %d]\n", this, count, mBuffered));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  // reset the ping timer
  if (mPingTimer) {
    // The purpose of ping/pong is to actively probe the peer so that an
    // unreachable peer is not mistaken for a period of idleness. This
    // implementation accepts any application level read activity as a sign of
    // life, it does not necessarily have to be a pong.
    mPingOutstanding = 0;
    mPingTimer->SetDelay(mPingTimeout);
  }

  PRUint32 avail;

  if (!mBuffered) {
    // Most of the time we can process right off the stack buffer without
    // having to accumulate anything
    mFramePtr = buffer;
    avail = count;
  } else {
    avail = UpdateReadBuffer(buffer, count, mFragmentAccumulator);
  }

  PRUint8 *payload;
  PRUint32 totalAvail = avail;

  while (avail >= 2) {
    PRInt64 payloadLength = mFramePtr[1] & 0x7F;
    PRUint8 finBit        = mFramePtr[0] & kFinalFragBit;
    PRUint8 rsvBits       = mFramePtr[0] & 0x70;
    PRUint8 maskBit       = mFramePtr[1] & kMaskBit;
    PRUint8 opcode        = mFramePtr[0] & 0x0F;

    PRUint32 framingLength = 2;
    if (maskBit)
      framingLength += 4;

    if (payloadLength < 126) {
      if (avail < framingLength)
        break;
    } else if (payloadLength == 126) {
      // 16 bit length field
      framingLength += 2;
      if (avail < framingLength)
        break;

      payloadLength = mFramePtr[2] << 8 | mFramePtr[3];
    } else {
      // 64 bit length
      framingLength += 8;
      if (avail < framingLength)
        break;

      if (mFramePtr[2] & 0x80) {
        // Section 4.2 says that the most significant bit MUST be
        // 0. (i.e. this is really a 63 bit value)
        LOG(("WebSocketChannel:: high bit of 64 bit length set"));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }

      // copy this in case it is unaligned
      PRUint64 tempLen;
      memcpy(&tempLen, mFramePtr + 2, 8);
      payloadLength = PR_ntohll(tempLen);
    }

    payload = mFramePtr + framingLength;
    avail -= framingLength;

    LOG(("WebSocketChannel::ProcessInput: payload %lld avail %lu\n",
         payloadLength, avail));

    // we don't deal in > 31 bit websocket lengths.. and probably
    // something considerably shorter (16MB by default)
    if (payloadLength + mFragmentAccumulator > mMaxMessageSize) {
      AbortSession(NS_ERROR_FILE_TOO_BIG);
      return NS_ERROR_FILE_TOO_BIG;
    }

    if (avail < payloadLength)
      break;

    LOG(("WebSocketChannel::ProcessInput: Frame accumulated - opcode %d\n",
         opcode));

    if (maskBit) {
      // This is unexpected - the server does not generally send masked
      // frames to the client, but it is allowed
      LOG(("WebSocketChannel:: Client RECEIVING masked frame."));

      PRUint32 mask;
      memcpy(&mask, payload - 4, 4);
      mask = PR_ntohl(mask);
      ApplyMask(mask, payload, payloadLength);
    }

    // Control codes are required to have the fin bit set
    if (!finBit && (opcode & kControlFrameMask)) {
      LOG(("WebSocketChannel:: fragmented control frame code %d\n", opcode));
      AbortSession(NS_ERROR_ILLEGAL_VALUE);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (rsvBits) {
      LOG(("WebSocketChannel:: unexpected reserved bits %x\n", rsvBits));
      AbortSession(NS_ERROR_ILLEGAL_VALUE);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (!finBit || opcode == kContinuation) {
      // This is part of a fragment response

      // Only the first frame has a non zero op code: Make sure we don't see a
      // first frame while some old fragments are open
      if ((mFragmentAccumulator != 0) && (opcode != kContinuation)) {
        LOG(("WebSocketHeandler:: nested fragments\n"));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }

      LOG(("WebSocketChannel:: Accumulating Fragment %lld\n", payloadLength));

      if (opcode == kContinuation) {
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
      } else {
        opcode = kContinuation;
        mFragmentAccumulator += payloadLength;
      }
    } else if (mFragmentAccumulator != 0 && !(opcode & kControlFrameMask)) {
      // This frame is not part of a fragment sequence but we
      // have an open fragment.. it must be a control code or else
      // we have a problem
      LOG(("WebSocketChannel:: illegal fragment sequence\n"));
      AbortSession(NS_ERROR_ILLEGAL_VALUE);
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
        nsCString utf8Data((const char *)payload, payloadLength);

        // Section 8.1 says to replace received non utf-8 sequences
        // (which are non-conformant to send) with u+fffd,
        // but secteam feels that silently rewriting messages is
        // inappropriate - so we will fail the connection instead.
        if (!IsUTF8(utf8Data)) {
          LOG(("WebSocketChannel:: text frame invalid utf-8\n"));
          AbortSession(NS_ERROR_ILLEGAL_VALUE);
          return NS_ERROR_ILLEGAL_VALUE;
        }

        NS_DispatchToMainThread(new CallOnMessageAvailable(mListener, mContext,
                                                           utf8Data, -1));
      }
    } else if (opcode & kControlFrameMask) {
      // control frames
      if (payloadLength > 125) {
        LOG(("WebSocketChannel:: bad control frame code %d length %d\n",
             opcode, payloadLength));
        AbortSession(NS_ERROR_ILLEGAL_VALUE);
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (opcode == kClose) {
        LOG(("WebSocketChannel:: close received\n"));
        mServerClosed = 1;

        mServerCloseCode = CLOSE_NO_STATUS;
        if (payloadLength >= 2) {
          memcpy(&mServerCloseCode, payload, 2);
          mServerCloseCode = PR_ntohs(mServerCloseCode);
          LOG(("WebSocketChannel:: close recvd code %u\n", mServerCloseCode));
          PRUint16 msglen = payloadLength - 2;
          if (msglen > 0) {
            mServerCloseReason.SetLength(msglen);
            memcpy(mServerCloseReason.BeginWriting(),
                   (const char *)payload + 2, msglen);

            // section 8.1 says to replace received non utf-8 sequences
            // (which are non-conformant to send) with u+fffd,
            // but secteam feels that silently rewriting messages is
            // inappropriate - so we will fail the connection instead.
            if (!IsUTF8(mServerCloseReason)) {
              LOG(("WebSocketChannel:: close frame invalid utf-8\n"));
              AbortSession(NS_ERROR_ILLEGAL_VALUE);
              return NS_ERROR_ILLEGAL_VALUE;
            }

            LOG(("WebSocketChannel:: close msg %s\n",
                 mServerCloseReason.get()));
          }
        }

        if (mCloseTimer) {
          mCloseTimer->Cancel();
          mCloseTimer = nsnull;
        }
        if (mListener)
          NS_DispatchToMainThread(
            new CallOnServerClose(mListener, mContext,
                                  mServerCloseCode, mServerCloseReason));

        if (mClientClosed)
          ReleaseSession();
      } else if (opcode == kPing) {
        LOG(("WebSocketChannel:: ping received\n"));
        GeneratePong(payload, payloadLength);
      } else {
        // opcode kPong: the mere act of receiving the packet is all we need
        // to do for the pong to trigger the activity timers
        LOG(("WebSocketChannel:: pong received\n"));
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
        payloadLength = 0;
        if (mBuffered)
          mBuffered -= framingLength + payloadLength;
      }
    } else if (opcode == kBinary) {
      LOG(("WebSocketChannel:: binary frame received\n"));
      if (mListener) {
        nsCString binaryData((const char *)payload, payloadLength);
        NS_DispatchToMainThread(new CallOnMessageAvailable(mListener, mContext,
                                                           binaryData,
                                                           payloadLength));
      }
    } else if (opcode != kContinuation) {
      /* unknown opcode */
      LOG(("WebSocketChannel:: unknown op code %d\n", opcode));
      AbortSession(NS_ERROR_ILLEGAL_VALUE);
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

      UpdateReadBuffer(mFramePtr - mFragmentAccumulator,
                       totalAvail + mFragmentAccumulator, 0);

      // UpdateReadBuffer will reset the frameptr to the beginning
      // of new saved state, so we need to skip past processed framgents
      mFramePtr += mFragmentAccumulator;
    } else if (totalAvail) {
      LOG(("WebSocketChannel:: Setup Buffer due to partial frame"));
      UpdateReadBuffer(mFramePtr, totalAvail, 0);
    }
  } else if (!mFragmentAccumulator && !totalAvail) {
    // If we were working off a saved buffer state and there is no partial
    // frame or fragment in process, then revert to stack behavior
    LOG(("WebSocketChannel:: Internal buffering not needed anymore"));
    mBuffered = 0;
  }
  return NS_OK;
}

void
WebSocketChannel::ApplyMask(PRUint32 mask, PRUint8 *data, PRUint64 len)
{
  // Optimally we want to apply the mask 32 bits at a time,
  // but the buffer might not be alligned. So we first deal with
  // 0 to 3 bytes of preamble individually

  while (len && (reinterpret_cast<PRUptrdiff>(data) & 3)) {
    *data ^= mask >> 24;
    mask = PR_ROTATE_LEFT32(mask, 8);
    data++;
    len--;
  }

  // perform mask on full words of data

  PRUint32 *iData = (PRUint32 *) data;
  PRUint32 *end = iData + (len / 4);
  mask = PR_htonl(mask);
  for (; iData < end; iData++)
    *iData ^= mask;
  mask = PR_ntohl(mask);
  data = (PRUint8 *)iData;
  len  = len % 4;

  // There maybe up to 3 trailing bytes that need to be dealt with
  // individually 

  while (len) {
    *data ^= mask >> 24;
    mask = PR_ROTATE_LEFT32(mask, 8);
    data++;
    len--;
  }
}

void
WebSocketChannel::GeneratePing()
{
  LOG(("WebSocketChannel::GeneratePing() %p\n", this));

  nsCString *buf = new nsCString();
  buf->Assign("PING");
  mOutgoingPingMessages.Push(new OutboundMessage(buf));
  OnOutputStreamReady(mSocketOut);
}

void
WebSocketChannel::GeneratePong(PRUint8 *payload, PRUint32 len)
{
  LOG(("WebSocketChannel::GeneratePong() %p [%p %u]\n", this, payload, len));

  nsCString *buf = new nsCString();
  buf->SetLength(len);
  if (buf->Length() < len) {
    LOG(("WebSocketChannel::GeneratePong Allocation Failure\n"));
    delete buf;
    return;
  }

  memcpy(buf->BeginWriting(), payload, len);
  mOutgoingPongMessages.Push(new OutboundMessage(buf));
  OnOutputStreamReady(mSocketOut);
}

void
WebSocketChannel::SendMsgInternal(nsCString *aMsg,
                                    PRInt32 aDataLen)
{
  LOG(("WebSocketChannel::SendMsgInternal %p [%p len=%d]\n", this, aMsg,
       aDataLen));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  if (aMsg == kFinMessage) {
    mOutgoingMessages.Push(new OutboundMessage());
  } else if (aDataLen < 0) {
    mOutgoingMessages.Push(new OutboundMessage(aMsg));
  } else {
    mOutgoingMessages.Push(new OutboundMessage(aMsg, aDataLen));
  }
  OnOutputStreamReady(mSocketOut);
}

PRUint16
WebSocketChannel::ResultToCloseCode(nsresult resultCode)
{
  if (NS_SUCCEEDED(resultCode))
    return CLOSE_NORMAL;
  if (resultCode == NS_ERROR_FILE_TOO_BIG)
    return CLOSE_TOO_LARGE;
  if (resultCode == NS_BASE_STREAM_CLOSED ||
      resultCode == NS_ERROR_NET_TIMEOUT ||
      resultCode == NS_ERROR_CONNECTION_REFUSED) {
    return CLOSE_ABNORMAL;
  }

  return CLOSE_PROTOCOL_ERROR;
}

void
WebSocketChannel::PrimeNewOutgoingMessage()
{
  LOG(("WebSocketChannel::PrimeNewOutgoingMessage() %p\n", this));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");
  NS_ABORT_IF_FALSE(!mCurrentOut, "Current message in progress");

  PRBool isPong = PR_FALSE;
  PRBool isPing = PR_FALSE;

  mCurrentOut = (OutboundMessage *)mOutgoingPongMessages.PopFront();
  if (mCurrentOut) {
    isPong = PR_TRUE;
  } else {
    mCurrentOut = (OutboundMessage *)mOutgoingPingMessages.PopFront();
    if (mCurrentOut)
      isPing = PR_TRUE;
    else
      mCurrentOut = (OutboundMessage *)mOutgoingMessages.PopFront();
  }

  if (!mCurrentOut)
    return;
  mCurrentOutSent = 0;
  mHdrOut = mOutHeader;

  PRUint8 *payload = nsnull;
  if (mCurrentOut->IsControl() && !isPing && !isPong) {
    // This is a demand to create a close message
    if (mClientClosed) {
      PrimeNewOutgoingMessage();
      return;
    }

    LOG(("WebSocketChannel:: PrimeNewOutgoingMessage() found close request\n"));
    mClientClosed = 1;
    mOutHeader[0] = kFinalFragBit | kClose;
    mOutHeader[1] = 0x02; // payload len = 2, maybe more for reason
    mOutHeader[1] |= kMaskBit;

    // payload is offset 6 including 4 for the mask
    payload = mOutHeader + 6;

    // length is 8 plus any reason information
    mHdrOutToSend = 8;

    // The close reason code sits in the first 2 bytes of payload
    // If the channel user provided a code and reason during Close()
    // and there isn't an internal error, use that.
    if (NS_SUCCEEDED(mStopOnClose) && mScriptCloseCode) {
      *((PRUint16 *)payload) = PR_htons(mScriptCloseCode);
      if (!mScriptCloseReason.IsEmpty()) {
        NS_ABORT_IF_FALSE(mScriptCloseReason.Length() <= 123,
                          "Close Reason Too Long");
        mOutHeader[1] += mScriptCloseReason.Length();
        mHdrOutToSend += mScriptCloseReason.Length();
        memcpy (payload + 2,
                mScriptCloseReason.BeginReading(),
                mScriptCloseReason.Length());
      }
    }
    else {
      *((PRUint16 *)payload) = PR_htons(ResultToCloseCode(mStopOnClose));
    }

    if (mServerClosed) {
      /* bidi close complete */
      mReleaseOnTransmit = 1;
    } else if (NS_FAILED(mStopOnClose)) {
      /* result of abort session - give up */
      StopSession(mStopOnClose);
    } else {
      /* wait for reciprocal close from server */
      nsresult rv;
      mCloseTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        mCloseTimer->InitWithCallback(this, mCloseTimeout,
                                      nsITimer::TYPE_ONE_SHOT);
      } else {
        StopSession(rv);
      }
    }
  } else {
    if (isPong) {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage() found pong request\n"));
      mOutHeader[0] = kFinalFragBit | kPong;
    } else if (isPing) {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage() found ping request\n"));
      mOutHeader[0] = kFinalFragBit | kPing;
    } else if (mCurrentOut->BinaryLen() < 0) {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage() "
           "found queued text message len %d\n", mCurrentOut->Length()));
      mOutHeader[0] = kFinalFragBit | kText;
    } else {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage() "
           "found queued binary message len %d\n", mCurrentOut->Length()));
      mOutHeader[0] = kFinalFragBit | kBinary;
    }

    if (mCurrentOut->Length() < 126) {
      mOutHeader[1] = mCurrentOut->Length() | kMaskBit;
      mHdrOutToSend = 6;
    } else if (mCurrentOut->Length() < 0xffff) {
      mOutHeader[1] = 126 | kMaskBit;
      ((PRUint16 *)mOutHeader)[1] =
        PR_htons(mCurrentOut->Length());
      mHdrOutToSend = 8;
    } else {
      mOutHeader[1] = 127 | kMaskBit;
      PRUint64 tempLen = mCurrentOut->Length();
      tempLen = PR_htonll(tempLen);
      memcpy(mOutHeader + 2, &tempLen, 8);
      mHdrOutToSend = 14;
    }
    payload = mOutHeader + mHdrOutToSend;
  }

  NS_ABORT_IF_FALSE(payload, "payload offset not found");

  // Perfom the sending mask. never use a zero mask
  PRUint32 mask;
  do {
    PRUint8 *buffer;
    nsresult rv = mRandomGenerator->GenerateRandomBytes(4, &buffer);
    if (NS_FAILED(rv)) {
      LOG(("WebSocketChannel::PrimeNewOutgoingMessage(): "
           "GenerateRandomBytes failure %x\n", rv));
      StopSession(rv);
      return;
    }
    mask = * reinterpret_cast<PRUint32 *>(buffer);
    NS_Free(buffer);
  } while (!mask);
  *(((PRUint32 *)payload) - 1) = PR_htonl(mask);

  LOG(("WebSocketChannel::PrimeNewOutgoingMessage() using mask %08x\n", mask));

  // We don't mask the framing, but occasionally we stick a little payload
  // data in the buffer used for the framing. Close frames are the current
  // example. This data needs to be masked, but it is never more than a
  // handful of bytes and might rotate the mask, so we can just do it locally.
  // For real data frames we ship the bulk of the payload off to ApplyMask()

  while (payload < (mOutHeader + mHdrOutToSend)) {
    *payload ^= mask >> 24;
    mask = PR_ROTATE_LEFT32(mask, 8);
    payload++;
  }

  // Mask the real message payloads

  ApplyMask(mask, mCurrentOut->BeginWriting(), mCurrentOut->Length());

  // for small frames, copy it all together for a contiguous write
  if (mCurrentOut->Length() <= kCopyBreak) {
    memcpy(mOutHeader + mHdrOutToSend, mCurrentOut->BeginWriting(),
           mCurrentOut->Length());
    mHdrOutToSend += mCurrentOut->Length();
    mCurrentOutSent = mCurrentOut->Length();
  }

  if (mCompressor) {
    // assume a 1/3 reduction in size for sizing the buffer
    // the buffer is used multiple times if necessary
    PRUint32 currentHeaderSize = mHdrOutToSend;
    mHdrOutToSend = 0;

    EnsureHdrOut(32 +
                 (currentHeaderSize + mCurrentOut->Length() - mCurrentOutSent)
                 / 2 * 3);
    mCompressor->Deflate(mOutHeader, currentHeaderSize,
                         mCurrentOut->BeginReading() + mCurrentOutSent,
                         mCurrentOut->Length() - mCurrentOutSent);

    // All of the compressed data now resides in {mHdrOut, mHdrOutToSend}
    // so do not send the body again
    mCurrentOutSent = mCurrentOut->Length();
  }

  // Transmitting begins - mHdrOutToSend bytes from mOutHeader and
  // mCurrentOut->Length() bytes from mCurrentOut. The latter may be
  // coaleseced into the former for small messages or as the result of the
  // compression process,
}

void
WebSocketChannel::EnsureHdrOut(PRUint32 size)
{
  LOG(("WebSocketChannel::EnsureHdrOut() %p [%d]\n", this, size));

  if (mDynamicOutputSize < size) {
    mDynamicOutputSize = size;
    mDynamicOutput =
      (PRUint8 *) moz_xrealloc(mDynamicOutput, mDynamicOutputSize);
  }

  mHdrOut = mDynamicOutput;
}

void
WebSocketChannel::CleanupConnection()
{
  LOG(("WebSocketChannel::CleanupConnection() %p", this));

  if (mLingeringCloseTimer) {
    mLingeringCloseTimer->Cancel();
    mLingeringCloseTimer = nsnull;
  }

  if (mSocketIn) {
    if (sWebSocketAdmissions)
      sWebSocketAdmissions->DecrementConnectedCount();
    mSocketIn->AsyncWait(nsnull, 0, 0, nsnull);
    mSocketIn = nsnull;
  }

  if (mSocketOut) {
    mSocketOut->AsyncWait(nsnull, 0, 0, nsnull);
    mSocketOut = nsnull;
  }

  if (mTransport) {
    mTransport->SetSecurityCallbacks(nsnull);
    mTransport->SetEventSink(nsnull, nsnull);
    mTransport->Close(NS_BASE_STREAM_CLOSED);
    mTransport = nsnull;
  }
}

void
WebSocketChannel::StopSession(nsresult reason)
{
  LOG(("WebSocketChannel::StopSession() %p [%x]\n", this, reason));

  // normally this should be called on socket thread, but it is ok to call it
  // from OnStartRequest before the socket thread machine has gotten underway

  NS_ABORT_IF_FALSE(mStopped,
                    "stopsession() has not transitioned through abort or close");

  if (mCloseTimer) {
    mCloseTimer->Cancel();
    mCloseTimer = nsnull;
  }

  if (mOpenTimer) {
    mOpenTimer->Cancel();
    mOpenTimer = nsnull;
  }

  if (mPingTimer) {
    mPingTimer->Cancel();
    mPingTimer = nsnull;
  }

  if (mSocketIn && !mTCPClosed) {
    // Drain, within reason, this socket. if we leave any data
    // unconsumed (including the tcp fin) a RST will be generated
    // The right thing to do here is shutdown(SHUT_WR) and then wait
    // a little while to see if any data comes in.. but there is no
    // reason to delay things for that when the websocket handshake
    // is supposed to guarantee a quiet connection except for that fin.

    char     buffer[512];
    PRUint32 count = 0;
    PRUint32 total = 0;
    nsresult rv;
    do {
      total += count;
      rv = mSocketIn->Read(buffer, 512, &count);
      if (rv != NS_BASE_STREAM_WOULD_BLOCK &&
        (NS_FAILED(rv) || count == 0))
        mTCPClosed = PR_TRUE;
    } while (NS_SUCCEEDED(rv) && count > 0 && total < 32000);
  }

  if (!mTCPClosed && mTransport && sWebSocketAdmissions &&
    sWebSocketAdmissions->ConnectedCount() < kLingeringCloseThreshold) {

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

  if (mDNSRequest) {
    mDNSRequest->Cancel(NS_ERROR_UNEXPECTED);
    mDNSRequest = nsnull;
  }

  mInflateReader = nsnull;
  mInflateStream = nsnull;

  delete mCompressor;
  mCompressor = nsnull;

  if (!mCalledOnStop) {
    mCalledOnStop = 1;
    if (mListener)
      NS_DispatchToMainThread(new CallOnStop(mListener, mContext, reason));
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
  mTCPClosed = PR_TRUE;

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
    mSocketThread->Dispatch(new nsPostMessage(this, kFinMessage, -1),
                            nsIEventTarget::DISPATCH_NORMAL);
    mStopOnClose = reason;
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
  mStopped = 1;
  StopSession(NS_OK);
}

nsresult
WebSocketChannel::HandleExtensions()
{
  LOG(("WebSocketChannel::HandleExtensions() %p\n", this));

  nsresult rv;
  nsCAutoString extensions;

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  rv = mHttpChannel->GetResponseHeader(
    NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"), extensions);
  if (NS_SUCCEEDED(rv)) {
    if (!extensions.IsEmpty()) {
      if (!extensions.Equals(NS_LITERAL_CSTRING("deflate-stream"))) {
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

      rv = serv->AsyncConvertData("deflate", "uncompressed", this, nsnull,
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
        mCompressor = nsnull;
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

  // draft-ietf-hybi-thewebsocketprotocol-07 illustrates Upgrade: websocket
  // in lower case, so go with that. It is technically case insensitive.
  rv = mChannel->HTTPUpgrade(NS_LITERAL_CSTRING("websocket"), this);
  NS_ENSURE_SUCCESS(rv, rv);

  mHttpChannel->SetRequestHeader(
    NS_LITERAL_CSTRING("Sec-WebSocket-Version"),
    NS_LITERAL_CSTRING(SEC_WEBSOCKET_VERSION), PR_FALSE);

  if (!mOrigin.IsEmpty())
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Origin"),
                                   mOrigin, PR_FALSE);

  if (!mProtocol.IsEmpty())
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"),
                                   mProtocol, PR_TRUE);

  if (mAllowCompression)
    mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"),
                                   NS_LITERAL_CSTRING("deflate-stream"),
                                   PR_FALSE);

  PRUint8      *secKey;
  nsCAutoString secKeyString;

  rv = mRandomGenerator->GenerateRandomBytes(16, &secKey);
  NS_ENSURE_SUCCESS(rv, rv);
  char* b64 = PL_Base64Encode((const char *)secKey, 16, nsnull);
  NS_Free(secKey);
  if (!b64)
    return NS_ERROR_OUT_OF_MEMORY;
  secKeyString.Assign(b64);
  PR_Free(b64);
  mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Sec-WebSocket-Key"),
                                 secKeyString, PR_FALSE);
  LOG(("WebSocketChannel::SetupRequest: client key %s\n", secKeyString.get()));

  // prepare the value we expect to see in
  // the sec-websocket-accept response header
  secKeyString.AppendLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  nsCOMPtr<nsICryptoHash> hasher =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Init(nsICryptoHash::SHA1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Update((const PRUint8 *) secKeyString.BeginWriting(),
                      secKeyString.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Finish(PR_TRUE, mHashedSecret);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("WebSocketChannel::SetupRequest: expected server key %s\n",
       mHashedSecret.get()));

  return NS_OK;
}

nsresult
WebSocketChannel::ApplyForAdmission()
{
  LOG(("WebSocketChannel::ApplyForAdmission() %p\n", this));

  // Websockets has a policy of 1 session at a time being allowed in the
  // CONNECTING state per server IP address (not hostname)

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString hostName;
  rv = mURI->GetHost(hostName);
  NS_ENSURE_SUCCESS(rv, rv);
  mAddress = hostName;

  // expect the callback in ::OnLookupComplete
  LOG(("WebSocketChannel::ApplyForAdmission: checking for concurrent open\n"));
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  dns->AsyncResolve(hostName, 0, this, mainThread, getter_AddRefs(mDNSRequest));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Called after both OnStartRequest and OnTransportAvailable have
// executed. This essentially ends the handshake and starts the websockets
// protocol state machine.
nsresult
WebSocketChannel::StartWebsocketData()
{
  LOG(("WebSocketChannel::StartWebsocketData() %p", this));

  if (sWebSocketAdmissions &&
    sWebSocketAdmissions->ConnectedCount() > mMaxConcurrentConnections) {
    LOG(("WebSocketChannel max concurrency %d exceeded "
         "in OnTransportAvailable()", mMaxConcurrentConnections));
    AbortSession(NS_ERROR_SOCKET_CREATE_FAILED);
    return NS_OK;
  }

  return mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
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
  NS_ABORT_IF_FALSE(aRequest == mDNSRequest, "wrong dns request");

  mDNSRequest = nsnull;

  // These failures are not fatal - we just use the hostname as the key
  if (NS_FAILED(aStatus)) {
    LOG(("WebSocketChannel::OnLookupComplete: No DNS Response\n"));
  } else {
    nsresult rv = aRecord->GetNextAddrAsString(mAddress);
    if (NS_FAILED(rv))
      LOG(("WebSocketChannel::OnLookupComplete: Failed GetNextAddr\n"));
  }

  if (sWebSocketAdmissions->ConditionallyConnect(mAddress, this)) {
    LOG(("WebSocketChannel::OnLookupComplete: Proceeding with Open\n"));
  } else {
    LOG(("WebSocketChannel::OnLookupComplete: Deferring Open\n"));
  }

  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
WebSocketChannel::GetInterface(const nsIID & iid, void **result NS_OUTPARAM)
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
                    PRUint32 flags,
                    nsIAsyncVerifyRedirectCallback *callback)
{
  LOG(("WebSocketChannel::AsyncOnChannelRedirect() %p\n", this));
  nsresult rv;

  nsCOMPtr<nsIURI> newuri;
  rv = newChannel->GetURI(getter_AddRefs(newuri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mAutoFollowRedirects) {
    nsCAutoString spec;
    if (NS_SUCCEEDED(newuri->GetSpec(spec)))
      LOG(("WebSocketChannel: Redirect to %s denied by configuration\n",
            spec.get()));
    callback->OnRedirectVerifyCallback(NS_ERROR_FAILURE);
    return NS_OK;
  }

  PRBool isHttps = PR_FALSE;
  rv = newuri->SchemeIs("https", &isHttps);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEncrypted && !isHttps) {
    nsCAutoString spec;
    if (NS_SUCCEEDED(newuri->GetSpec(spec)))
      LOG(("WebSocketChannel: Redirect to %s violates encryption rule\n",
           spec.get()));
    callback->OnRedirectVerifyCallback(NS_ERROR_FAILURE);
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(newChannel, &rv);

  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not QI to HTTP\n"));
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannelInternal> newUpgradeChannel =
    do_QueryInterface(newChannel, &rv);

  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not QI to HTTP Upgrade\n"));
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  // The redirect is likely OK

  newChannel->SetNotificationCallbacks(this);
  mURI = newuri;
  mHttpChannel = newHttpChannel;
  mChannel = newUpgradeChannel;
  rv = SetupRequest();
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect could not SetupRequest()\n"));
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  // We cannot just tell the callback OK right now due to the 1 connect at a
  // time policy. First we need to complete the old location and then start the
  // lookup chain for the new location - once that is complete and we have been
  // admitted, OnRedirectVerifyCallback(NS_OK) will be called out of BeginOpen()

  sWebSocketAdmissions->Complete(mAddress);
  mAddress.Truncate();
  mRedirectCallback = callback;

  rv = ApplyForAdmission();
  if (NS_FAILED(rv)) {
    LOG(("WebSocketChannel: Redirect failed due to DNS failure\n"));
    callback->OnRedirectVerifyCallback(rv);
    mRedirectCallback = nsnull;
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

    mCloseTimer = nsnull;
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    LOG(("WebSocketChannel:: Expecting Server Close - Timed Out\n"));
    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mOpenTimer) {
    NS_ABORT_IF_FALSE(!mRecvdHttpOnStartRequest,
                      "Open Timer after open complete");
    NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

    mOpenTimer = nsnull;
    LOG(("WebSocketChannel:: Connection Timed Out\n"));
    if (mStopped || mServerClosed)                /* no longer relevant */
      return NS_OK;

    AbortSession(NS_ERROR_NET_TIMEOUT);
  } else if (timer == mPingTimer) {
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread,
                      "not socket thread");

    if (mClientClosed || mServerClosed || mRequestedClose) {
      // no point in worrying about ping now
      mPingTimer = nsnull;
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
      mPingTimer = nsnull;
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


NS_IMETHODIMP
WebSocketChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG(("WebSocketChannel::GetSecurityInfo() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (mTransport) {
    if (NS_FAILED(mTransport->GetSecurityInfo(aSecurityInfo)))
      *aSecurityInfo = nsnull;
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

  if (mListener)
    return NS_ERROR_ALREADY_OPENED;

  nsresult rv;

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
    PRInt32 intpref;
    PRBool boolpref;
    rv = prefService->GetIntPref("network.websocket.max-message-size", 
                                 &intpref);
    if (NS_SUCCEEDED(rv)) {
      mMaxMessageSize = NS_CLAMP(intpref, 1024, 1 << 30);
    }
    rv = prefService->GetIntPref("network.websocket.timeout.close", &intpref);
    if (NS_SUCCEEDED(rv)) {
      mCloseTimeout = NS_CLAMP(intpref, 1, 1800) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.open", &intpref);
    if (NS_SUCCEEDED(rv)) {
      mOpenTimeout = NS_CLAMP(intpref, 1, 1800) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.ping.request",
                                 &intpref);
    if (NS_SUCCEEDED(rv)) {
      mPingTimeout = NS_CLAMP(intpref, 0, 86400) * 1000;
    }
    rv = prefService->GetIntPref("network.websocket.timeout.ping.response",
                                 &intpref);
    if (NS_SUCCEEDED(rv)) {
      mPingResponseTimeout = NS_CLAMP(intpref, 1, 3600) * 1000;
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
      mMaxConcurrentConnections = NS_CLAMP(intpref, 1, 0xffff);
    }
  }

  if (sWebSocketAdmissions &&
      sWebSocketAdmissions->ConnectedCount() >= mMaxConcurrentConnections)
  {
    // Checking this early creates an optimal fast-fail, but it is
    // also a time-of-check-time-of-use problem. So we will check again
    // after the handshake is complete to catch anything that sneaks
    // through the race condition.
    LOG(("WebSocketChannel: max concurrency %d exceeded",
         mMaxConcurrentConnections));

    // WebSocket connections are expected to be long lived, so return
    // an error here instead of queueing
    return NS_ERROR_SOCKET_CREATE_FAILED;
  }

  if (mPingTimeout) {
    mPingTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("unable to create ping timer. Carrying on.");
    } else {
      LOG(("WebSocketChannel will generate ping after %d ms of receive silence\n",
           mPingTimeout));
      mPingTimer->SetTarget(mSocketThread);
      mPingTimer->InitWithCallback(this, mPingTimeout, nsITimer::TYPE_ONE_SHOT);
    }
  }

  mOriginalURI = aURI;
  mURI = mOriginalURI;
  mListener = aListener;
  mContext = aContext;
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
              nsIProtocolProxyService::RESOLVE_PREFER_SOCKS_PROXY |
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

  return ApplyForAdmission();
}

NS_IMETHODIMP
WebSocketChannel::Close(PRUint16 code, const nsACString & reason)
{
  LOG(("WebSocketChannel::Close() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (!mTransport) {
    LOG(("WebSocketChannel::Close() without transport - aborting."));
    AbortSession(NS_ERROR_NOT_CONNECTED);
    return NS_ERROR_NOT_CONNECTED;
  }

  if (mRequestedClose) {
    LOG(("WebSocketChannel:: Double close error\n"));
    return NS_ERROR_UNEXPECTED;
  }

  // The API requires the UTF-8 string to be 123 or less bytes
  if (reason.Length() > 123)
    return NS_ERROR_ILLEGAL_VALUE;

  mRequestedClose = 1;
  mScriptCloseReason = reason;
  mScriptCloseCode = code;
    
  return mSocketThread->Dispatch(new nsPostMessage(this, kFinMessage, -1),
                                 nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
WebSocketChannel::SendMsg(const nsACString &aMsg)
{
  LOG(("WebSocketChannel::SendMsg() %p\n", this));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (mRequestedClose) {
    LOG(("WebSocketChannel:: SendMsg when closed error\n"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mStopped) {
    LOG(("WebSocketChannel:: SendMsg when stopped error\n"));
    return NS_ERROR_NOT_CONNECTED;
  }

  return mSocketThread->Dispatch(
                          new nsPostMessage(this, new nsCString(aMsg), -1),
                          nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
WebSocketChannel::SendBinaryMsg(const nsACString &aMsg)
{
  LOG(("WebSocketChannel::SendBinaryMsg() %p len=%d\n", this, aMsg.Length()));
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  if (mRequestedClose) {
    LOG(("WebSocketChannel:: SendBinaryMsg when closed error\n"));
    return NS_ERROR_UNEXPECTED;
  }

  if (mStopped) {
    LOG(("WebSocketChannel:: SendBinaryMsg when stopped error\n"));
    return NS_ERROR_NOT_CONNECTED;
  }

  return mSocketThread->Dispatch(new nsPostMessage(this, new nsCString(aMsg), 
                                                   aMsg.Length()),
                                 nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
WebSocketChannel::OnTransportAvailable(nsISocketTransport *aTransport,
                                         nsIAsyncInputStream *aSocketIn,
                                         nsIAsyncOutputStream *aSocketOut)
{
  LOG(("WebSocketChannel::OnTransportAvailable %p [%p %p %p] rcvdonstart=%d\n",
       this, aTransport, aSocketIn, aSocketOut, mRecvdHttpOnStartRequest));

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
  NS_ABORT_IF_FALSE(!mRecvdHttpUpgradeTransport, "OTA duplicated");
  NS_ABORT_IF_FALSE(aSocketIn, "OTA with invalid socketIn");

  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;
  if (sWebSocketAdmissions)
    sWebSocketAdmissions->IncrementConnectedCount();

  nsresult rv;
  rv = mTransport->SetEventSink(nsnull, nsnull);
  if (NS_FAILED(rv)) return rv;
  rv = mTransport->SetSecurityCallbacks(mCallbacks);
  if (NS_FAILED(rv)) return rv;

  mRecvdHttpUpgradeTransport = 1;
  if (mRecvdHttpOnStartRequest)
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
  NS_ABORT_IF_FALSE(!mRecvdHttpOnStartRequest, "OTA duplicated");

  // Generating the onStart event will take us out of the
  // CONNECTING state which means we can now open another,
  // perhaps parallel, connection to the same host if one
  // is pending

  if (sWebSocketAdmissions->Complete(mAddress))
    LOG(("WebSocketChannel::OnStartRequest: Starting Pending Open\n"));
  else
    LOG(("WebSocketChannel::OnStartRequest: No More Pending Opens\n"));

  if (mOpenTimer) {
    mOpenTimer->Cancel();
    mOpenTimer = nsnull;
  }

  if (mStopped) {
    LOG(("WebSocketChannel::OnStartRequest: Channel Already Done\n"));
    AbortSession(NS_ERROR_CONNECTION_REFUSED);
    return NS_ERROR_CONNECTION_REFUSED;
  }

  nsresult rv;
  PRUint32 status;
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

  nsCAutoString respUpgrade;
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
    AbortSession(rv);
    return rv;
  }

  nsCAutoString respConnection;
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
    AbortSession(rv);
    return rv;
  }

  nsCAutoString respAccept;
  rv = mHttpChannel->GetResponseHeader(
                       NS_LITERAL_CSTRING("Sec-WebSocket-Accept"),
                       respAccept);

  if (NS_FAILED(rv) ||
    respAccept.IsEmpty() || !respAccept.Equals(mHashedSecret)) {
    LOG(("WebSocketChannel::OnStartRequest: "
         "HTTP response header Sec-WebSocket-Accept check failed\n"));
    LOG(("WebSocketChannel::OnStartRequest: Expected %s recevied %s\n",
         mHashedSecret.get(), respAccept.get()));
    AbortSession(NS_ERROR_ILLEGAL_VALUE);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // If we sent a sub protocol header, verify the response matches
  // If it does not, set mProtocol to "" so the protocol attribute
  // of the WebSocket JS object reflects that
  if (!mProtocol.IsEmpty()) {
    nsCAutoString respProtocol;
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

  LOG(("WebSocketChannel::OnStartRequest: Notifying Listener %p\n",
       mListener.get()));

  if (mListener)
    mListener->OnStart(mContext);

  mRecvdHttpOnStartRequest = 1;
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

  // This is the end of the HTTP upgrade transaction, the
  // upgraded streams live on

  mChannel = nsnull;
  mHttpChannel = nsnull;
  mLoadGroup = nsnull;
  mCallbacks = nsnull;

  return NS_OK;
}

// nsIInputStreamCallback

NS_IMETHODIMP
WebSocketChannel::OnInputStreamReady(nsIAsyncInputStream *aStream)
{
  LOG(("WebSocketChannel::OnInputStreamReady() %p\n", this));
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "not socket thread");

  nsRefPtr<nsIStreamListener>    deleteProtector1(mInflateReader);
  nsRefPtr<nsIStringInputStream> deleteProtector2(mInflateStream);

  // this is after the  http upgrade - so we are speaking websockets
  char  buffer[2048];
  PRUint32 count;
  nsresult rv;

  do {
    rv = mSocketIn->Read((char *)buffer, 2048, &count);
    LOG(("WebSocketChannel::OnInputStreamReady: read %u rv %x\n", count, rv));

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      mTCPClosed = PR_TRUE;
      AbortSession(rv);
      return rv;
    }

    if (count == 0) {
      mTCPClosed = PR_TRUE;
      AbortSession(NS_BASE_STREAM_CLOSED);
      return NS_OK;
    }

    if (mStopped) {
      NS_ABORT_IF_FALSE(mLingeringCloseTimer,
                        "OnInputReady after stop without linger");
      continue;
    }

    if (mInflateReader) {
      mInflateStream->ShareData(buffer, count);
      rv = mInflateReader->OnDataAvailable(nsnull, mSocketIn, mInflateStream, 
                                           0, count);
    } else {
      rv = ProcessInput((PRUint8 *)buffer, count);
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
    PRUint32 toSend;
    PRUint32 amtSent;

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

      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        mSocketOut->AsyncWait(this, 0, 0, nsnull);
        return NS_OK;
      }

      if (NS_FAILED(rv)) {
        AbortSession(rv);
        return NS_OK;
      }
    }

    if (mHdrOut) {
      if (amtSent == toSend) {
        mHdrOut = nsnull;
        mHdrOutToSend = 0;
      } else {
        mHdrOut += amtSent;
        mHdrOutToSend -= amtSent;
      }
    } else {
      if (amtSent == toSend) {
        if (!mStopped) {
          NS_DispatchToMainThread(new CallAcknowledge(mListener, mContext,
                                                      mCurrentOut->Length()));
        }
        delete mCurrentOut;
        mCurrentOut = nsnull;
        mCurrentOutSent = 0;
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
                                    PRUint32 aOffset,
                                    PRUint32 aCount)
{
  LOG(("WebSocketChannel::OnDataAvailable() %p [%p %p %p %u %u]\n",
         this, aRequest, aContext, aInputStream, aOffset, aCount));

  if (aContext == mSocketIn) {
    // This is the deflate decoder

    LOG(("WebSocketChannel::OnDataAvailable: Deflate Data %u\n",
             aCount));

    PRUint8  buffer[2048];
    PRUint32 maxRead;
    PRUint32 count;
    nsresult rv;

    while (aCount > 0) {
      if (mStopped)
        return NS_BASE_STREAM_CLOSED;

      maxRead = NS_MIN(2048U, aCount);
      rv = aInputStream->Read((char *)buffer, maxRead, &count);
      LOG(("WebSocketChannel::OnDataAvailable: InflateRead read %u rv %x\n",
           count, rv));
      if (NS_FAILED(rv) || count == 0) {
        AbortSession(rv);
        break;
      }

      aCount -= count;
      rv = ProcessInput(buffer, count);
    }
    return NS_OK;
  }

  if (aContext == mSocketOut) {
    // This is the deflate encoder

    PRUint32 maxRead;
    PRUint32 count;
    nsresult rv;

    while (aCount > 0) {
      if (mStopped)
        return NS_BASE_STREAM_CLOSED;

      maxRead = NS_MIN(2048U, aCount);
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

} // namespace mozilla::net
} // namespace mozilla
