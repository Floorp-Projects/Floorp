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
 * Portions created by the Initial Developer are Copyright (C) 2012
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

#ifndef mozilla_net_SpdyStream3_h
#define mozilla_net_SpdyStream3_h

#include "nsAHttpTransaction.h"

namespace mozilla { namespace net {

class SpdyStream3 : public nsAHttpSegmentReader
                  , public nsAHttpSegmentWriter
{
public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdyStream3(nsAHttpTransaction *,
             SpdySession3 *, nsISocketTransport *,
             PRUint32, z_stream *, PRInt32);

  PRUint32 StreamID() { return mStreamID; }

  nsresult ReadSegments(nsAHttpSegmentReader *,  PRUint32, PRUint32 *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *);

  bool RequestBlockedOnRead()
  {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  // returns false if called more than once
  bool GetFullyOpen() {return mFullyOpen;}
  void SetFullyOpen() 
  {
    NS_ABORT_IF_FALSE(!mFullyOpen, "SetFullyOpen already open");
    mFullyOpen = 1;
  }

  nsAHttpTransaction *Transaction()
  {
    return mTransaction;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus) { mRecvdFin = aStatus ? 1 : 0; }
  bool RecvdFin() { return mRecvdFin; }

  void SetRecvdData(bool aStatus) { mReceivedData = aStatus ? 1 : 0; }
  bool RecvdData() { return mReceivedData; }

  void UpdateTransportSendEvents(PRUint32 count);
  void UpdateTransportReadEvents(PRUint32 count);

  // The zlib header compression dictionary defined by SPDY,
  // and hooks to the mozilla allocator for zlib to use.
  static const unsigned char kDictionary[1423];
  static void *zlib_allocator(void *, uInt, uInt);
  static void zlib_destructor(void *, void *);

  nsresult Uncompress(z_stream *, char *, PRUint32);
  nsresult ConvertHeaders(nsACString &);

  void UpdateRemoteWindow(PRInt32 delta) { mRemoteWindow += delta; }
  PRInt64 RemoteWindow() { return mRemoteWindow; }

  void DecrementLocalWindow(PRUint32 delta) {
    mLocalWindow -= delta;
    mLocalUnacked += delta;
  }

  void IncrementLocalWindow(PRUint32 delta) {
    mLocalWindow += delta;
    mLocalUnacked -= delta;
  }

  PRUint64 LocalUnAcked() { return mLocalUnacked; }
  bool     BlockedOnRwin() { return mBlockedOnRwin; }

private:

  // a SpdyStream3 object is only destroyed by being removed from the
  // SpdySession3 mStreamTransactionHash - make the dtor private to
  // just the AutoPtr implementation needed for that hash.
  friend class nsAutoPtr<SpdyStream3>;
  ~SpdyStream3();

  enum stateType {
    GENERATING_SYN_STREAM,
    SENDING_SYN_STREAM,
    GENERATING_REQUEST_BODY,
    SENDING_REQUEST_BODY,
    SENDING_FIN_STREAM,
    UPSTREAM_COMPLETE
  };

  static PLDHashOperator hdrHashEnumerate(const nsACString &,
                                          nsAutoPtr<nsCString> &,
                                          void *);

  void     ChangeState(enum stateType);
  nsresult ParseHttpRequestHeaders(const char *, PRUint32, PRUint32 *);
  nsresult TransmitFrame(const char *, PRUint32 *);
  void     GenerateDataFrameHeader(PRUint32, bool);

  void     CompressToFrame(const nsACString &);
  void     CompressToFrame(const nsACString *);
  void     CompressToFrame(const char *, PRUint32);
  void     CompressToFrame(PRUint32);
  void     CompressFlushFrame();
  void     ExecuteCompress(PRUint32);
  nsresult FindHeader(nsCString, nsDependentCSubstring &);
  
  // Each stream goes from syn_stream to upstream_complete, perhaps
  // looping on multiple instances of generating_request_body and
  // sending_request_body for each SPDY chunk in the upload.
  enum stateType mUpstreamState;

  // The underlying HTTP transaction. This pointer is used as the key
  // in the SpdySession3 mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
  nsRefPtr<nsAHttpTransaction> mTransaction;

  // The session that this stream is a subset of
  SpdySession3                *mSession;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader        *mSegmentReader;
  nsAHttpSegmentWriter        *mSegmentWriter;

  // The 24 bit SPDY stream ID
  PRUint32                    mStreamID;

  // The quanta upstream data frames are chopped into
  PRUint32                    mChunkSize;

  // Flag is set when all http request headers have been read
  PRUint32                     mSynFrameComplete     : 1;

  // Flag is set when the HTTP processor has more data to send
  // but has blocked in doing so.
  PRUint32                     mRequestBlockedOnRead : 1;

  // Flag is set when a FIN has been placed on a data or syn packet
  // (i.e after the client has closed)
  PRUint32                     mSentFinOnData        : 1;

  // Flag is set after the response frame bearing the fin bit has
  // been processed. (i.e. after the server has closed).
  PRUint32                     mRecvdFin             : 1;

  // Flag is set after syn reply received
  PRUint32                     mFullyOpen            : 1;

  // Flag is set after the WAITING_FOR Transport event has been generated
  PRUint32                     mSentWaitingFor       : 1;

  // Flag is set after 1st DATA frame has been passed to stream, after
  // which additional HEADERS data is invalid
  PRUint32                     mReceivedData         : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  nsAutoArrayPtr<char>         mTxInlineFrame;
  PRUint32                     mTxInlineFrameSize;
  PRUint32                     mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  PRUint32                     mTxStreamFrameSize;

  // Compression context and buffer for request header compression.
  // This is a copy of SpdySession3::mUpstreamZlib because it needs
  //  to remain the same in all streams of a session.
  z_stream                     *mZlib;
  nsCString                    mFlatHttpRequestHeaders;

  // These are used for decompressing downstream spdy response headers
  PRUint32             mDecompressBufferSize;
  PRUint32             mDecompressBufferUsed;
  PRUint32             mDecompressedBytes;
  nsAutoArrayPtr<char> mDecompressBuffer;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers.
  PRInt64                      mRequestBodyLenRemaining;

  // based on nsISupportsPriority definitions
  PRInt32                      mPriority;

  // mLocalWindow, mRemoteWindow, and mLocalUnacked are for flow control.
  // *window are signed because they race conditions in asynchronous SETTINGS
  // messages can force them temporarily negative.

  // LocalWindow is how much data the server will send without getting a
  //   window update
  PRInt64                      mLocalWindow;

  // RemoteWindow is how much data the client is allowed to send without
  //   getting a window update
  PRInt64                      mRemoteWindow;

  // LocalUnacked is the number of bytes received by the client but not
  //   yet reflected in a window update. Sending that update will increment
  //   LocalWindow
  PRUint64                     mLocalUnacked;

  // True when sending is suspended becuase the remote flow control window is
  //   <= 0
  bool                         mBlockedOnRwin;

  // For Progress Events
  PRUint64                     mTotalSent;
  PRUint64                     mTotalRead;
};

}} // namespace mozilla::net

#endif // mozilla_net_SpdyStream3_h
