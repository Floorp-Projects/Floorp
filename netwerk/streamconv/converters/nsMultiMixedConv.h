/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsmultimixedconv__h__
#define __nsmultimixedconv__h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIByteRangeRequest.h"
#include "nsIMultiPartChannel.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

#define NS_MULTIMIXEDCONVERTER_CID                         \
{ /* 7584CE90-5B25-11d3-A175-0050041CAF44 */         \
    0x7584ce90,                                      \
    0x5b25,                                          \
    0x11d3,                                          \
    {0xa1, 0x75, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44}       \
}

//
// nsPartChannel is a "dummy" channel which represents an individual part of
// a multipart/mixed stream...
//
// Instances on this channel are passed out to the consumer through the
// nsIStreamListener interface.
//
class nsPartChannel MOZ_FINAL : public nsIChannel,
                                public nsIByteRangeRequest,
                                public nsIMultiPartChannel
{
public:
  nsPartChannel(nsIChannel *aMultipartChannel, PRUint32 aPartID,
                nsIStreamListener* aListener);

  void InitializeByteRange(PRInt64 aStart, PRInt64 aEnd);
  void SetIsLastPart() { mIsLastPart = true; }
  nsresult SendOnStartRequest(nsISupports* aContext);
  nsresult SendOnDataAvailable(nsISupports* aContext, nsIInputStream* aStream,
                               PRUint32 aOffset, PRUint32 aLen);
  nsresult SendOnStopRequest(nsISupports* aContext, nsresult aStatus);
  /* SetContentDisposition expects the full value of the Content-Disposition
   * header */
  void SetContentDisposition(const nsACString& aContentDispositionHeader);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIBYTERANGEREQUEST
  NS_DECL_NSIMULTIPARTCHANNEL

protected:
  ~nsPartChannel();

protected:
  nsCOMPtr<nsIChannel>    mMultipartChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  
  nsresult                mStatus;
  nsLoadFlags             mLoadFlags;

  nsCOMPtr<nsILoadGroup>  mLoadGroup;

  nsCString               mContentType;
  nsCString               mContentCharset;
  PRUint32                mContentDisposition;
  nsString                mContentDispositionFilename;
  nsCString               mContentDispositionHeader;
  PRUint64                mContentLength;

  bool                    mIsByteRangeRequest;
  PRInt64                 mByteRangeStart;
  PRInt64                 mByteRangeEnd;

  PRUint32                mPartID; // unique ID that can be used to identify
                                   // this part of the multipart document
  bool                    mIsLastPart;
};

// The nsMultiMixedConv stream converter converts a stream of type "multipart/x-mixed-replace"
// to it's subparts. There was some debate as to whether or not the functionality desired
// when HTTP confronted this type required a stream converter. After all, this type really
// prompts various viewer related actions rather than stream conversion. There simply needs
// to be a piece in place that can strip out the multiple parts of a stream of this type, and 
// "display" them accordingly.
//
// With that said, this "stream converter" spends more time packaging up the sub parts of the
// main stream and sending them off the destination stream listener, than doing any real
// stream parsing/converting.
//
// WARNING: This converter requires that it's destination stream listener be able to handle
//   multiple OnStartRequest(), OnDataAvailable(), and OnStopRequest() call combinations.
//   Each series represents the beginning, data production, and ending phase of each sub-
//   part of the original stream.
//
// NOTE: this MIME-type is used by HTTP, *not* SMTP, or IMAP.
//
// NOTE: For reference, a general description of how this MIME type should be handled via
//   HTTP, see http://home.netscape.com/assist/net_sites/pushpull.html . Note that
//   real world server content deviates considerably from this overview.
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
// boundary tokens are considered part of the data. BoundaryToken
// is any opaque string.
//  
//

class nsMultiMixedConv : public nsIStreamConverter {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    nsMultiMixedConv();
    virtual ~nsMultiMixedConv();

protected:
    nsresult SendStart(nsIChannel *aChannel);
    nsresult SendStop(nsresult aStatus);
    nsresult SendData(char *aBuffer, PRUint32 aLen);
    nsresult ParseHeaders(nsIChannel *aChannel, char *&aPtr,
                          PRUint32 &aLen, bool *_retval);
    PRInt32  PushOverLine(char *&aPtr, PRUint32 &aLen);
    char *FindToken(char *aCursor, PRUint32 aLen);
    nsresult BufferData(char *aData, PRUint32 aLen);

    // member data
    bool                mNewPart;        // Are we processing the beginning of a part?
    bool                mProcessingHeaders;
    nsCOMPtr<nsIStreamListener> mFinalListener; // this guy gets the converted data via his OnDataAvailable()

    nsCString           mToken;
    PRUint32            mTokenLen;

    nsRefPtr<nsPartChannel> mPartChannel;   // the channel for the given part we're processing.
                                        // one channel per part.
    nsCOMPtr<nsISupports> mContext;
    nsCString           mContentType;
    nsCString           mContentDisposition;
    PRUint64            mContentLength;
    
    char                *mBuffer;
    PRUint32            mBufLen;
    PRUint64            mTotalSent;
    bool                mFirstOnData;   // used to determine if we're in our first OnData callback.

    // The following members are for tracking the byte ranges in
    // multipart/mixed content which specified the 'Content-Range:'
    // header...
    PRInt64             mByteRangeStart;
    PRInt64             mByteRangeEnd;
    bool                mIsByteRangeRequest;

    PRUint32            mCurrentPartID;
};

#endif /* __nsmultimixedconv__h__ */
