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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert Sayre.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#define NS_SAXXMLREADER_CONTRACTID "@mozilla.org/saxparser/xmlreader;1"
#define NS_SAXXMLREADER_CLASSNAME "SAX XML Reader"
#define NS_SAXXMLREADER_CID  \
{ 0xab1da296, 0x6125, 0x40ba, \
{ 0x96, 0xd0, 0x47, 0xa8, 0x28, 0x2a, 0xe3, 0xdb} }

class nsSAXXMLReader : public nsISAXXMLReader,
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
  NS_IMETHOD DidBuildModel(PRBool aTerminated);
  NS_IMETHOD SetParser(nsIParser* aParser);
  
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
  PRBool mIsAsyncParse;
  static PRBool TryChannelCharset(nsIChannel *aChannel,
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
