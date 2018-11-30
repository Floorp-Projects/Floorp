/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSAXXMLReader_h__
#define nsSAXXMLReader_h__

#include "nsCOMPtr.h"
#include "nsIContentSink.h"
#include "nsIExpatSink.h"
#include "nsIParser.h"
#include "nsIURI.h"
#include "nsISAXXMLReader.h"
#include "nsISAXContentHandler.h"
#include "nsISAXErrorHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/NotNull.h"

#define NS_SAXXMLREADER_CONTRACTID "@mozilla.org/saxparser/xmlreader;1"
#define NS_SAXXMLREADER_CID                          \
  {                                                  \
    0xab1da296, 0x6125, 0x40ba, {                    \
      0x96, 0xd0, 0x47, 0xa8, 0x28, 0x2a, 0xe3, 0xdb \
    }                                                \
  }

class nsSAXXMLReader final : public nsISAXXMLReader,
                             public nsIExpatSink,
                             public nsIContentSink {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsSAXXMLReader, nsISAXXMLReader)
  NS_DECL_NSIEXPATSINK
  NS_DECL_NSISAXXMLREADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsSAXXMLReader();

  // nsIContentSink
  NS_IMETHOD WillParse() override { return NS_OK; }

  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  NS_IMETHOD SetParser(nsParserBase *aParser) override;

  NS_IMETHOD WillInterrupt() override { return NS_OK; }

  NS_IMETHOD WillResume() override { return NS_OK; }

  virtual void FlushPendingNotifications(mozilla::FlushType aType) override {}

  virtual void SetDocumentCharset(
      NotNull<const Encoding *> aEncoding) override {}

  virtual nsISupports *GetTarget() override { return nullptr; }

 private:
  ~nsSAXXMLReader() {}

  nsCOMPtr<nsISAXContentHandler> mContentHandler;
  nsCOMPtr<nsISAXErrorHandler> mErrorHandler;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIRequestObserver> mParserObserver;
  bool mIsAsyncParse;
  static bool TryChannelCharset(nsIChannel *aChannel, int32_t &aCharsetSource,
                                NotNull<const Encoding *> &aEncoding);
  nsresult EnsureBaseURI();
  nsresult InitParser(nsIRequestObserver *aListener, nsIChannel *aChannel);
  nsresult SplitExpatName(const char16_t *aExpatName, nsString &aURI,
                          nsString &aLocalName, nsString &aQName);
};

#endif  // nsSAXXMLReader_h__
