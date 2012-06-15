/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSAXXMLReader_h__
#define nsSAXXMLReader_h__

#include "nsCOMPtr.h"
#include "nsIContentSink.h"
#include "nsIExtendedExpatSink.h"
#include "nsIParser.h"
#include "nsIURI.h"
#include "nsISAXXMLReader.h"
#include "nsISAXContentHandler.h"
#include "nsISAXDTDHandler.h"
#include "nsISAXErrorHandler.h"
#include "nsISAXLexicalHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

#define NS_SAXXMLREADER_CONTRACTID "@mozilla.org/saxparser/xmlreader;1"
#define NS_SAXXMLREADER_CLASSNAME "SAX XML Reader"
#define NS_SAXXMLREADER_CID  \
{ 0xab1da296, 0x6125, 0x40ba, \
{ 0x96, 0xd0, 0x47, 0xa8, 0x28, 0x2a, 0xe3, 0xdb} }

class nsSAXXMLReader MOZ_FINAL : public nsISAXXMLReader,
                                 public nsIExtendedExpatSink,
                                 public nsIContentSink
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsSAXXMLReader, nsISAXXMLReader)
  NS_DECL_NSIEXPATSINK
  NS_DECL_NSIEXTENDEDEXPATSINK
  NS_DECL_NSISAXXMLREADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsSAXXMLReader();

  //nsIContentSink
  NS_IMETHOD WillParse()
  {
    return NS_OK;
  }

  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
  NS_IMETHOD DidBuildModel(bool aTerminated);
  NS_IMETHOD SetParser(nsParserBase* aParser);
  
  NS_IMETHOD WillInterrupt()
  {
    return NS_OK;
  }

  NS_IMETHOD WillResume()
  {
    return NS_OK;
  }
  
  virtual void FlushPendingNotifications(mozFlushType aType)
  {
  }
  
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset)
  {
    return NS_OK;
  }
  
  virtual nsISupports *GetTarget()
  {
    return nsnull;
  }

private:
  nsCOMPtr<nsISAXContentHandler> mContentHandler;
  nsCOMPtr<nsISAXDTDHandler> mDTDHandler;
  nsCOMPtr<nsISAXErrorHandler> mErrorHandler;
  nsCOMPtr<nsISAXLexicalHandler> mLexicalHandler;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIRequestObserver> mParserObserver;
  bool mIsAsyncParse;
  static bool TryChannelCharset(nsIChannel *aChannel,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);
  nsresult EnsureBaseURI();
  nsresult InitParser(nsIRequestObserver *aListener, nsIChannel *aChannel);
  nsresult SplitExpatName(const PRUnichar *aExpatName,
                          nsString &aURI,
                          nsString &aLocalName,
                          nsString &aQName);
  nsString mPublicId;
  nsString mSystemId;
};

#endif // nsSAXXMLReader_h__
