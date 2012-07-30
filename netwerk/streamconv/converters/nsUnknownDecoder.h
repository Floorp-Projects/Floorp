/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnknownDecoder_h__
#define nsUnknownDecoder_h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIContentSniffer.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_UNKNOWNDECODER_CID                        \
{ /* 7d7008a0-c49a-11d3-9b22-0080c7cb1080 */         \
    0x7d7008a0,                                      \
    0xc49a,                                          \
    0x11d3,                                          \
    {0x9b, 0x22, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80}       \
}


class nsUnknownDecoder : public nsIStreamConverter, public nsIContentSniffer
{
public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIStreamConverter methods
  NS_DECL_NSISTREAMCONVERTER

  // nsIStreamListener methods
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver methods
  NS_DECL_NSIREQUESTOBSERVER

  // nsIContentSniffer methods
  NS_DECL_NSICONTENTSNIFFER

  nsUnknownDecoder();

protected:
  virtual ~nsUnknownDecoder();

  virtual void DetermineContentType(nsIRequest* aRequest);
  nsresult FireListenerNotifications(nsIRequest* request, nsISupports *aCtxt);

protected:
  nsCOMPtr<nsIStreamListener> mNextListener;

  // Function to use to check whether sniffing some potentially
  // dangerous types (eg HTML) is ok for this request.  We can disable
  // sniffing for local files if needed using this.  Just a security
  // precation thingy... who knows when we suddenly need to flip this
  // pref?
  bool AllowSniffing(nsIRequest* aRequest);
  
  // Various sniffer functions.  Returning true means that a type
  // was determined; false means no luck.
  bool TryContentSniffers(nsIRequest* aRequest);
  bool SniffForHTML(nsIRequest* aRequest);
  bool SniffForXML(nsIRequest* aRequest);

  // SniffURI guesses at the content type based on the URI (typically
  // using the extentsion)
  bool SniffURI(nsIRequest* aRequest);

  // LastDitchSniff guesses at text/plain vs. application/octet-stream
  // by just looking at whether the data contains null bytes, and
  // maybe at the fraction of chars with high bit set.  Use this only
  // as a last-ditch attempt to decide a content type!
  bool LastDitchSniff(nsIRequest* aRequest);

  /**
   * An entry struct for our array of sniffers.  Each entry has either
   * a type associated with it (set these with the SNIFFER_ENTRY macro)
   * or a function to be executed (set these with the
   * SNIFFER_ENTRY_WITH_FUNC macro).  The function should take a single
   * nsIRequest* and returns bool -- true if it sets mContentType,
   * false otherwise
   */
  struct nsSnifferEntry {
    typedef bool (nsUnknownDecoder::*TypeSniffFunc)(nsIRequest* aRequest);
    
    const char* mBytes;
    PRUint32 mByteLen;
    
    // Exactly one of mMimeType and mContentTypeSniffer should be set non-null
    const char* mMimeType;
    TypeSniffFunc mContentTypeSniffer;
  };

#define SNIFFER_ENTRY(_bytes, _type) \
  { _bytes, sizeof(_bytes) - 1, _type, nullptr }

#define SNIFFER_ENTRY_WITH_FUNC(_bytes, _func) \
  { _bytes, sizeof(_bytes) - 1, nullptr, _func }

  static nsSnifferEntry sSnifferEntries[];
  static PRUint32 sSnifferEntryNum;
  
  char *mBuffer;
  PRUint32 mBufferLen;
  bool mRequireHTMLsuffix;

  nsCString mContentType;

};

#define NS_BINARYDETECTOR_CID                        \
{ /* a2027ec6-ba0d-4c72-805d-148233f5f33c */         \
    0xa2027ec6,                                      \
    0xba0d,                                          \
    0x4c72,                                          \
    {0x80, 0x5d, 0x14, 0x82, 0x33, 0xf5, 0xf3, 0x3c} \
}

/**
 * Class that detects whether a data stream is text or binary.  This reuses
 * most of nsUnknownDecoder except the actual content-type determination logic
 * -- our overridden DetermineContentType simply calls LastDitchSniff and sets
 * the type to APPLICATION_GUESS_FROM_EXT if the data is detected as binary.
 */
class nsBinaryDetector : public nsUnknownDecoder
{
protected:
  virtual void DetermineContentType(nsIRequest* aRequest);
};

#define NS_BINARYDETECTOR_CATEGORYENTRY \
  { NS_CONTENT_SNIFFER_CATEGORY, "Binary Detector", NS_BINARYDETECTOR_CONTRACTID }

#endif /* nsUnknownDecoder_h__ */

