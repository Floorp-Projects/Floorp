/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnknownDecoder_h__
#define nsUnknownDecoder_h__

#include "nsIStreamConverter.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIContentSniffer.h"
#include "mozilla/Mutex.h"
#include "mozilla/Atomics.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_UNKNOWNDECODER_CID                        \
  { /* 7d7008a0-c49a-11d3-9b22-0080c7cb1080 */       \
    0x7d7008a0, 0xc49a, 0x11d3, {                    \
      0x9b, 0x22, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80 \
    }                                                \
  }

class nsUnknownDecoder : public nsIStreamConverter,
                         public nsIContentSniffer,
                         public nsIThreadRetargetableStreamListener {
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

  // nsIThreadRetargetableStreamListener methods
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  explicit nsUnknownDecoder(nsIStreamListener* aListener = nullptr);

 protected:
  virtual ~nsUnknownDecoder();

  virtual void DetermineContentType(nsIRequest* aRequest);
  nsresult FireListenerNotifications(nsIRequest* request, nsISupports* aCtxt);

  class ConvertedStreamListener : public nsIStreamListener {
   public:
    explicit ConvertedStreamListener(nsUnknownDecoder* aDecoder);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

   private:
    virtual ~ConvertedStreamListener() = default;
    static nsresult AppendDataToString(nsIInputStream* inputStream,
                                       void* closure, const char* rawSegment,
                                       uint32_t toOffset, uint32_t count,
                                       uint32_t* writeCount);
    nsUnknownDecoder* mDecoder;
  };

 protected:
  nsCOMPtr<nsIStreamListener> mNextListener;

  // Various sniffer functions.  Returning true means that a type
  // was determined; false means no luck.
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
    using TypeSniffFunc = bool (nsUnknownDecoder::*)(nsIRequest*);

    const char* mBytes;
    uint32_t mByteLen;

    // Exactly one of mMimeType and mContentTypeSniffer should be set non-null
    const char* mMimeType;
    TypeSniffFunc mContentTypeSniffer;
  };

#define SNIFFER_ENTRY(_bytes, _type) \
  { _bytes, sizeof(_bytes) - 1, _type, nullptr }

#define SNIFFER_ENTRY_WITH_FUNC(_bytes, _func) \
  { _bytes, sizeof(_bytes) - 1, nullptr, _func }

  static nsSnifferEntry sSnifferEntries[];
  static uint32_t sSnifferEntryNum;

  // We guarantee in order delivery of OnStart, OnStop and OnData, therefore
  // we do not need proper locking for mBuffer.
  mozilla::Atomic<char*> mBuffer;
  mozilla::Atomic<uint32_t> mBufferLen;

  nsCString mContentType;

  // This mutex syncs: mContentType, mDecodedData and mNextListener.
  mutable mozilla::Mutex mMutex MOZ_UNANNOTATED;

 protected:
  nsresult ConvertEncodedData(nsIRequest* request, const char* data,
                              uint32_t length);
  nsCString mDecodedData;  // If data are encoded this will be uncompress data.
};

#define NS_BINARYDETECTOR_CID                        \
  { /* a2027ec6-ba0d-4c72-805d-148233f5f33c */       \
    0xa2027ec6, 0xba0d, 0x4c72, {                    \
      0x80, 0x5d, 0x14, 0x82, 0x33, 0xf5, 0xf3, 0x3c \
    }                                                \
  }

/**
 * Class that detects whether a data stream is text or binary.  This reuses
 * most of nsUnknownDecoder except the actual content-type determination logic
 * -- our overridden DetermineContentType simply calls LastDitchSniff and sets
 * the type to APPLICATION_GUESS_FROM_EXT if the data is detected as binary.
 */
class nsBinaryDetector : public nsUnknownDecoder {
 protected:
  virtual void DetermineContentType(nsIRequest* aRequest) override;
};

#endif /* nsUnknownDecoder_h__ */
