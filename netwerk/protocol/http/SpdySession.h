/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef mozilla_net_SpdySession_h
#define mozilla_net_SpdySession_h

// SPDY as defined by
// http://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2

#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsDeque.h"
#include "nsHashKeys.h"
#include "zlib.h"

class nsHttpConnection;
class nsISocketTransport;

namespace mozilla { namespace net {

class SpdyStream;

class SpdySession : public nsAHttpTransaction
                  , public nsAHttpConnection
                  , public nsAHttpSegmentReader
                  , public nsAHttpSegmentWriter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION
  NS_DECL_NSAHTTPCONNECTION
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdySession(nsAHttpTransaction *, nsISocketTransport *, PRInt32);
  ~SpdySession();

  bool AddStream(nsAHttpTransaction *, PRInt32);
  bool CanReuse() { return !mShouldGoAway && !mClosed; }
  void DontReuse();
  bool RoomForMoreStreams();
  PRUint32 RegisterStreamID(SpdyStream *);

  const static PRUint8 kFlag_Control   = 0x80;

  const static PRUint8 kFlag_Data_FIN  = 0x01;
  const static PRUint8 kFlag_Data_UNI  = 0x02;
  const static PRUint8 kFlag_Data_ZLIB = 0x02;
  
  const static PRUint8 kPri00   = 0x00;
  const static PRUint8 kPri01   = 0x40;
  const static PRUint8 kPri02   = 0x80;
  const static PRUint8 kPri03   = 0xC0;

  enum
  {
    CONTROL_TYPE_FIRST = 0,
    CONTROL_TYPE_SYN_STREAM = 1,
    CONTROL_TYPE_SYN_REPLY = 2,
    CONTROL_TYPE_RST_STREAM = 3,
    CONTROL_TYPE_SETTINGS = 4,
    CONTROL_TYPE_NOOP = 5,
    CONTROL_TYPE_PING = 6,
    CONTROL_TYPE_GOAWAY = 7,
    CONTROL_TYPE_HEADERS = 8,
    CONTROL_TYPE_WINDOW_UPDATE = 9,               /* no longer in v2 */
    CONTROL_TYPE_LAST = 10
  };

  enum
  {
    RST_PROTOCOL_ERROR = 1,
    RST_INVALID_STREAM = 2,
    RST_REFUSED_STREAM = 3,
    RST_UNSUPPORTED_VERSION = 4,
    RST_CANCEL = 5,
    RST_INTERNAL_ERROR = 6,
    RST_FLOW_CONTROL_ERROR = 7,
    RST_BAD_ASSOC_STREAM = 8
  };

  enum
  {
    SETTINGS_TYPE_UPLOAD_BW = 1, // kb/s
    SETTINGS_TYPE_DOWNLOAD_BW = 2, // kb/s
    SETTINGS_TYPE_RTT = 3, // ms
    SETTINGS_TYPE_MAX_CONCURRENT = 4, // streams
    SETTINGS_TYPE_CWND = 5, // packets
    SETTINGS_TYPE_DOWNLOAD_RETRANS_RATE = 6, // percentage
    SETTINGS_TYPE_INITIAL_WINDOW = 7  // bytes. Not used in v2.
  };

  // This should be big enough to hold all of your control packets,
  // but if it needs to grow for huge headers it can do so dynamically.
  // About 1% of requests to SPDY google services seem to be > 1000
  // with all less than 2000.
  const static PRUint32 kDefaultBufferSize = 2000;

  const static PRUint32 kDefaultQueueSize =  16000;
  const static PRUint32 kQueueTailRoom    =  4000;
  const static PRUint32 kSendingChunkSize = 4000;
  const static PRUint32 kDefaultMaxConcurrent = 100;
  const static PRUint32 kMaxStreamID = 0x7800000;
  
  static nsresult HandleSynStream(SpdySession *);
  static nsresult HandleSynReply(SpdySession *);
  static nsresult HandleRstStream(SpdySession *);
  static nsresult HandleSettings(SpdySession *);
  static nsresult HandleNoop(SpdySession *);
  static nsresult HandlePing(SpdySession *);
  static nsresult HandleGoAway(SpdySession *);
  static nsresult HandleHeaders(SpdySession *);
  static nsresult HandleWindowUpdate(SpdySession *);

  static void EnsureBuffer(nsAutoArrayPtr<char> &,
                           PRUint32, PRUint32, PRUint32 &);

  // For writing the SPDY data stream to LOG4
  static void LogIO(SpdySession *, SpdyStream *, const char *,
                    const char *, PRUint32);

private:

  enum stateType {
    BUFFERING_FRAME_HEADER,
    BUFFERING_CONTROL_FRAME,
    PROCESSING_DATA_FRAME,
    DISCARD_DATA_FRAME,
    PROCESSING_CONTROL_SYN_REPLY,
    PROCESSING_CONTROL_RST_STREAM
  };

  PRUint32    WriteQueueSize();
  void        ChangeDownstreamState(enum stateType);
  nsresult    DownstreamUncompress(char *, PRUint32);
  void        zlibInit();
  nsresult    FindHeader(nsCString, nsDependentCSubstring &);
  nsresult    ConvertHeaders(nsDependentCSubstring &,
                             nsDependentCSubstring &);
  void        GeneratePing(PRUint32);
  void        GenerateRstStream(PRUint32, PRUint32);
  void        GenerateGoAway();
  void        CleanupStream(SpdyStream *, nsresult);

  void        SetWriteCallbacks(nsAHttpTransaction *);
  void        FlushOutputQueue();

  bool        RoomForMoreConcurrent();
  void        ActivateStream(SpdyStream *);
  void        ProcessPending();

  static PLDHashOperator Shutdown(nsAHttpTransaction *,
                                  nsAutoPtr<SpdyStream> &,
                                  void *);

  // This is intended to be nsHttpConnectionMgr:nsHttpConnectionHandle taken
  // from the first transcation on this session. That object contains the
  // pointer to the real network-level nsHttpConnection object.
  nsRefPtr<nsAHttpConnection> mConnection;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader       *mSegmentReader;
  nsAHttpSegmentWriter       *mSegmentWriter;

  PRUint32          mSendingChunkSize;        /* the transmisison chunk size */
  PRUint32          mNextStreamID;            /* 24 bits */
  PRUint32          mConcurrentHighWater;     /* max parallelism on session */

  stateType         mDownstreamState; /* in frame, between frames, etc..  */

  // Maintain 5 indexes - one by stream ID, one by transaction ptr,
  // one list of streams ready to write, one list of streams that are queued
  // due to max parallelism settings, and one list of streams
  // that must be given priority to write for window updates. The objects
  // are not ref counted - they get destryoed
  // by the nsClassHashtable implementation when they are removed from
  // there.
  nsDataHashtable<nsUint32HashKey, SpdyStream *>      mStreamIDHash;
  nsClassHashtable<nsPtrHashKey<nsAHttpTransaction>,
                   SpdyStream>                        mStreamTransactionHash;
  nsDeque                                             mReadyForWrite;
  nsDeque                                             mQueuedStreams;

  // UrgentForWrite is meant to carry window updates. They were defined in
  // the v2 spec but apparently never implemented so are now scheduled to
  // be removed. But they will be reintroduced for v3, so we will leave
  // this queue in place to ease that transition.
  nsDeque           mUrgentForWrite;

  // If we block while wrting out a frame then this points to the stream
  // that was blocked. When writing again that stream must be the first
  // one to write. It is null if there is not a partial frame.
  SpdyStream        *mPartialFrame;

  // Compression contexts for header transport using deflate.
  // SPDY compresses only HTTP headers and does not reset zlib in between
  // frames.
  z_stream            mDownstreamZlib;
  z_stream            mUpstreamZlib;

  // mFrameBuffer is used to store received control packets and the 8 bytes
  // of header on data packets
  PRUint32             mFrameBufferSize;
  PRUint32             mFrameBufferUsed;
  nsAutoArrayPtr<char> mFrameBuffer;
  
  // mFrameDataSize/Read are used for tracking the amount of data consumed
  // in a data frame. the data itself is not buffered in spdy
  // The frame size is mFrameDataSize + the constant 8 byte header
  PRUint32             mFrameDataSize;
  PRUint32             mFrameDataRead;
  bool                 mFrameDataLast; // This frame was marked FIN

  // When a frame has been received that is addressed to a particular stream
  // (e.g. a data frame after the stream-id has been decoded), this points
  // to the stream.
  SpdyStream          *mFrameDataStream;
  
  // A state variable to cleanup a closed stream after the stack has unwound.
  SpdyStream          *mNeedsCleanup;

  // The CONTROL_TYPE value for a control frame
  PRUint32             mFrameControlType;

  // This reason code in the last processed RESET frame
  PRUint32             mDownstreamRstReason;

  // These are used for decompressing downstream spdy response headers
  // This is done at the session level because sometimes the stream
  // has already been canceled but the decompression still must happen
  // to keep the zlib state correct for the next state of headers.
  PRUint32             mDecompressBufferSize;
  PRUint32             mDecompressBufferUsed;
  nsAutoArrayPtr<char> mDecompressBuffer;

  // for the conversion of downstream http headers into spdy formatted headers
  nsCString            mFlatHTTPResponseHeaders;
  PRUint32             mFlatHTTPResponseHeadersOut;

  // when set, the session will go away when it reaches 0 streams
  bool                 mShouldGoAway;

  // the session has received a nsAHttpTransaction::Close()  call
  bool                 mClosed;

  // the session received a GoAway frame with a valid GoAwayID
  bool                 mCleanShutdown;

  // If a GoAway message was received this is the ID of the last valid
  // stream. 0 otherwise. (0 is never a valid stream id.)
  PRUint32             mGoAwayID;

  // The limit on number of concurrent streams for this session. Normally it
  // is basically unlimited, but the SETTINGS control message from the
  // server might bring it down.
  PRUint32             mMaxConcurrent;

  // The actual number of concurrent streams at this moment. Generally below
  // mMaxConcurrent, but the max can be lowered in real time to a value
  // below the current value
  PRUint32             mConcurrent;

  // The number of server initiated SYN-STREAMS, tracked for telemetry
  PRUint32             mServerPushedResources;

  // This is a output queue of bytes ready to be written to the SSL stream.
  // When that streams returns WOULD_BLOCK on direct write the bytes get
  // coalesced together here. This results in larger writes to the SSL layer.
  // The buffer is not dynamically grown to accomodate stream writes, but
  // does expand to accept infallible session wide frames like GoAway and RST.
  PRUint32             mOutputQueueSize;
  PRUint32             mOutputQueueUsed;
  PRUint32             mOutputQueueSent;
  nsAutoArrayPtr<char> mOutputQueueBuffer;
};

}} // namespace mozilla::net

#endif // mozilla_net_SpdySession_h
