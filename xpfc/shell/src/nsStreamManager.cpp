/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include "nscore.h"

#include "nsISupports.h"
#include "nsStreamManager.h"
#include "nsxpfcCIID.h"
#include "nsIContentSink.h"
#include "nsUrlParser.h"
#include "nspr.h"
#include "nsParserCIID.h"
#include "nsXPFCXMLContentSink.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCStreamManager, NS_STREAM_MANAGER_CID);

nsStreamManager::nsStreamManager()
{
  NS_INIT_REFCNT();
  mUrl = nsnull;
  mParser = nsnull;
  mDTD = nsnull;
  mSink = nsnull;
}

nsStreamManager::~nsStreamManager()
{
  NS_IF_RELEASE(mUrl);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mSink);
}

NS_DEFINE_IID(kIStreamManagerIID, NS_ISTREAM_MANAGER_IID);
NS_IMPL_ISUPPORTS(nsStreamManager,kIStreamManagerIID);

nsresult nsStreamManager::Init()
{
  return NS_OK;
}

nsresult nsStreamManager::LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                                  const nsString& aURLSpec, 
                                  nsIStreamListener* aListener, 
                                  nsIPostData * aPostData,
                                  nsIID *aDTDIID,
                                  nsIID *aSinkIID)
{
/*
 * If we can find the file, then use it, else use the HARDCODE.
 * We'll need to change the way we deal with this since the file
 * could be gotten over the network.
 */

  static NS_DEFINE_IID(kIDTDIID,          NS_IDTD_IID);
  static NS_DEFINE_IID(kIContentSinkIID,  NS_ICONTENT_SINK_IID);

  static NS_DEFINE_IID(kCCalXPFCXMLDTD,            NS_IXPFCXML_DTD_IID);
  static NS_DEFINE_IID(kCCalXPFCXMLContentSinkCID, NS_XPFCXMLCONTENTSINK_IID); 

  nsIID * iid_dtd  = aDTDIID;
  nsIID * iid_sink = aSinkIID;

  if (iid_dtd == nsnull)
    iid_dtd = (nsIID*)&kCCalXPFCXMLDTD;
  if (iid_sink == nsnull)
    iid_sink = (nsIID*)&kCCalXPFCXMLContentSinkCID;

  nsresult rv = NS_OK;

  char * pUI = aURLSpec.ToNewCString();

  /*
   * Create a nsIURL representing the interface ...
   */

  nsresult res = NS_OK;
  nsUrlParser urlParser(pUI);
  
  if (urlParser.IsLocalFile() == PR_TRUE) {
    char * pURL = urlParser.LocalFileToURL();
    res = NS_NewURL(&mUrl, pURL);
  } else {
    res = NS_NewURL(&mUrl, pUI);
  }



  if (urlParser.IsLocalFile() == PR_TRUE)
  {
    PRStatus status = PR_Access(pUI,PR_ACCESS_EXISTS);

  } else {

    char * file = urlParser.ToLocalFile();

    PRStatus status = PR_Access(file,PR_ACCESS_EXISTS);

  }


  /*
   *  Create the Parser
   */
  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  rv = nsRepository::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&mParser);


  if (NS_OK != res) {
      return res;
  }

  /*
   * Create the DTD and Sink
   */

  res = nsRepository::CreateInstance(*iid_dtd, 
                                     nsnull, 
                                     kIDTDIID,
                                     (void**) &mDTD);

  if (NS_OK != res) {
      return res;
  }


  res = nsRepository::CreateInstance(*iid_sink, 
                                     nsnull, 
                                     kIContentSinkIID,
                                     (void**) &mSink);

  if (NS_OK != res) {
      return res;
  }

  nsIXPFCXMLContentSink * sink ;

  static NS_DEFINE_IID(kIXPFCXMLContentSinkIID,  NS_IXPFC_XML_CONTENT_SINK_IID); 

  res = mSink->QueryInterface(kIXPFCXMLContentSinkIID,(void**)&sink);

  if (NS_OK == res)
  {
    sink->SetViewerContainer(aWebViewerContainer);
    NS_RELEASE(sink);
  }

  /*
   * Register the DTD
   */

  mParser->RegisterDTD(mDTD);


  /*
   * Register the Context Sink, Parser, etc...
   */

  mParser->SetContentSink(mSink);


  mDTD->SetContentSink(mSink);
  mDTD->SetParser(mParser);


  /*
   * Open the URL
   */

  res = mUrl->Open(aListener);


  /*
   * We want to parse when the Stream has data?
   */

  mParser->Parse(mUrl);

  delete pUI;

  return rv;

}