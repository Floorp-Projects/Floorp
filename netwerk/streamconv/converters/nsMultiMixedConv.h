/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsmultimixedconv__h__
#define __nsmultimixedconv__h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIByteRangeRequest.h"
#include "nsIMultiPartChannel.h"
#include "mozilla/Attributes.h"
#include "mozilla/IncrementalTokenizer.h"
#include "nsHttpResponseHead.h"
#include "mozilla/UniquePtr.h"

#define NS_MULTIMIXEDCONVERTER_CID                 \
  { /* 7584CE90-5B25-11d3-A175-0050041CAF44 */     \
    0x7584ce90, 0x5b25, 0x11d3, {                  \
      0xa1, 0x75, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 \
    }                                              \
  }

//
// nsPartChannel is a "dummy" channel which represents an individual part of
// a multipart/mixed stream...
//
// Instances on this channel are passed out to the consumer through the
// nsIStreamListener interface.
//
class nsPartChannel final : public nsIChannel,
                            public nsIByteRangeRequest,
                            public nsIMultiPartChannel {
 public:
  nsPartChannel(nsIChannel* aMultipartChannel, uint32_t aPartID,
                bool aIsFirstPart, nsIStreamListener* aListener);

  void InitializeByteRange(int64_t aStart, int64_t aEnd);
  void SetIsLastPart() { mIsLastPart = true; }
  nsresult SendOnStartRequest(nsISupports* aContext);
  nsresult SendOnDataAvailable(nsISupports* aContext, nsIInputStream* aStream,
                               uint64_t aOffset, uint32_t aLen);
  nsresult SendOnStopRequest(nsISupports* aContext, nsresult aStatus);
  /* SetContentDisposition expects the full value of the Content-Disposition
   * header */
  void SetContentDisposition(const nsACString& aContentDispositionHeader);
  // TODO(ER): This appears to be dead code
  void SetResponseHead(mozilla::net::nsHttpResponseHead* head) {
    mResponseHead.reset(head);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIBYTERANGEREQUEST
  NS_DECL_NSIMULTIPARTCHANNEL

 protected:
  ~nsPartChannel() = default;

 protected:
  nsCOMPtr<nsIChannel> mMultipartChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  mozilla::UniquePtr<mozilla::net::nsHttpResponseHead> mResponseHead;

  nsresult mStatus{NS_OK};
  nsLoadFlags mLoadFlags{0};

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsCString mContentType;
  nsCString mContentCharset;
  uint32_t mContentDisposition{0};
  nsString mContentDispositionFilename;
  nsCString mContentDispositionHeader;
  uint64_t mContentLength{UINT64_MAX};

  bool mIsByteRangeRequest{false};
  int64_t mByteRangeStart{0};
  int64_t mByteRangeEnd{0};

  uint32_t mPartID;  // unique ID that can be used to identify
                     // this part of the multipart document
  bool mIsFirstPart;
  bool mIsLastPart{false};
};

// The nsMultiMixedConv stream converter converts a stream of type
// "multipart/x-mixed-replace" to it's subparts. There was some debate as to
// whether or not the functionality desired when HTTP confronted this type
// required a stream converter. After all, this type really prompts various
// viewer related actions rather than stream conversion. There simply needs to
// be a piece in place that can strip out the multiple parts of a stream of this
// type, and "display" them accordingly.
//
// With that said, this "stream converter" spends more time packaging up the sub
// parts of the main stream and sending them off the destination stream
// listener, than doing any real stream parsing/converting.
//
// WARNING: This converter requires that it's destination stream listener be
//   able to handle multiple OnStartRequest(), OnDataAvailable(), and
//   OnStopRequest() call combinations.  Each series represents the beginning,
//   data production, and ending phase of each sub- part of the original
//   stream.
//
// NOTE: this MIME-type is used by HTTP, *not* SMTP, or IMAP.
//
// NOTE: For reference, a general description of how this MIME type should be
//   handled via HTTP, see
//   http://home.netscape.com/assist/net_sites/pushpull.html . Note that real
//   world server content deviates considerably from this overview.
//
// Implementation assumptions:
//  Assumed structue:
//  --BoundaryToken[\r]\n
//  content-type: foo/bar[\r]\n
//  ... (other headers if any)
//  [\r]\n (second line feed to delimit end of headers)
//  data
//  --BoundaryToken-- (end delimited by final "--")
//
// linebreaks can be either CRLF or LFLF. linebreaks preceding
// boundary tokens are NOT considered part of the data. BoundaryToken
// is any opaque string.
//
//

class nsMultiMixedConv : public nsIStreamConverter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMCONVERTER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  explicit nsMultiMixedConv();

 protected:
  using Token = mozilla::IncrementalTokenizer::Token;

  virtual ~nsMultiMixedConv() = default;

  nsresult SendStart();
  void AccumulateData(Token const& aToken);
  nsresult SendData();
  nsresult SendStop(nsresult aStatus);

  // member data
  nsCOMPtr<nsIStreamListener> mFinalListener;  // this guy gets the converted
                                               // data via his OnDataAvailable()

  nsCOMPtr<nsIChannel>
      mChannel;  // The channel as we get in in OnStartRequest call
  RefPtr<nsPartChannel> mPartChannel;  // the channel for the given part we're
                                       // processing. one channel per part.
  nsCOMPtr<nsISupports> mContext;
  nsCString mContentType;
  nsCString mContentDisposition;
  nsCString mContentSecurityPolicy;
  nsCString mRootContentSecurityPolicy;
  uint64_t mContentLength{UINT64_MAX};
  uint64_t mTotalSent{0};

  // The following members are for tracking the byte ranges in
  // multipart/mixed content which specified the 'Content-Range:'
  // header...
  int64_t mByteRangeStart{0};
  int64_t mByteRangeEnd{0};
  bool mIsByteRangeRequest{false};
  // This flag is set first time we create a part channel.
  // We use it to prevent duplicated OnStopRequest call on the listener
  // when we fail from some reason to ever create a part channel that
  // ensures correct notifications.
  bool mRequestListenerNotified{false};

  uint32_t mCurrentPartID{0};

  // Flag preventing reenter of OnDataAvailable in case the target listener
  // ends up spinning the event loop.
  bool mInOnDataAvailable{false};

  // Current state of the incremental parser
  enum EParserState {
    PREAMBLE,
    BOUNDARY_CRLF,
    HEADER_NAME,
    HEADER_SEP,
    HEADER_VALUE,
    BODY_INIT,
    BODY,
    TRAIL_DASH1,
    TRAIL_DASH2,
    EPILOGUE,

    INIT = PREAMBLE
  } mParserState{INIT};

  // Response part header value, valid when we find a header name
  // we recognize.
  enum EHeader : uint32_t {
    HEADER_FIRST,
    HEADER_CONTENT_TYPE = HEADER_FIRST,
    HEADER_CONTENT_LENGTH,
    HEADER_CONTENT_DISPOSITION,
    HEADER_SET_COOKIE,
    HEADER_CONTENT_RANGE,
    HEADER_RANGE,
    HEADER_CONTENT_SECURITY_POLICY,
    HEADER_UNKNOWN
  } mResponseHeader{HEADER_UNKNOWN};
  // Cumulated value of a response header.
  nsCString mResponseHeaderValue;

  nsCString mBoundary;
  mozilla::IncrementalTokenizer mTokenizer;

  // When in the "body parsing" mode, see below, we cumulate raw data
  // incrementally to mainly avoid any unnecessary granularity.
  // mRawData points to the first byte in the tokenizer buffer where part
  // body data begins or continues.  mRawDataLength is a cumulated length
  // of that data during a single tokenizer input feed.  This is always
  // flushed right after we fed the tokenizer.
  nsACString::const_char_iterator mRawData{nullptr};
  nsACString::size_type mRawDataLength{0};

  // At the start we don't know if the server will be sending boundary with
  // or without the leading dashes.
  Token mBoundaryToken;
  Token mBoundaryTokenWithDashes;
  // We need these custom tokens to allow finding CRLF when in the binary mode.
  // CRLF before boundary is considered part of the boundary and not part of
  // the data.
  Token mLFToken;
  Token mCRLFToken;
  // Custom tokens for each of the response headers we recognize.
  Token mHeaderTokens[HEADER_UNKNOWN];

  // Resets values driven by part headers, like content type, to their defaults,
  // called at the start of every part processing.
  void HeadersToDefault();
  // Processes captured value of mResponseHeader header.
  nsresult ProcessHeader();
  // Switches the parser and tokenizer state to "binary mode" which only
  // searches for the 'CRLF boundary' delimiter.
  void SwitchToBodyParsing();
  // Switches to the default mode, we are in this mode when parsing headers and
  // control data around the boundary delimiters.
  void SwitchToControlParsing();
  // Turns on or off recognition of the headers we recognize in part heads.
  void SetHeaderTokensEnabled(bool aEnable);

  // The main parser callback called by the IncrementalTokenizer
  // instance from OnDataAvailable or OnStopRequest.
  nsresult ConsumeToken(Token const& token);
};

#endif /* __nsmultimixedconv__h__ */
