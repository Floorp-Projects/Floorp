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
#include "nsStreamObject.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCStreamManager, NS_STREAM_MANAGER_CID);

static NS_DEFINE_IID(kIDTDIID,          NS_IDTD_IID);
static NS_DEFINE_IID(kIContentSinkIID,  NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kCCalXPFCXMLDTD,            NS_IXPFCXML_DTD_IID);
static NS_DEFINE_IID(kCCalXPFCXMLContentSinkCID, NS_XPFCXMLCONTENTSINK_IID); 

static NS_DEFINE_IID(kIStreamObjectIID,  NS_ISTREAM_OBJECT_IID);
static NS_DEFINE_IID(kCStreamObjectCID,  NS_STREAM_OBJECT_CID);
static NS_DEFINE_IID(kIStreamListenerIID,  NS_ISTREAMLISTENER_IID);

nsStreamManager::nsStreamManager()
{
  NS_INIT_REFCNT();
}

nsStreamManager::~nsStreamManager()
{
  if (mStreamObjects != nsnull) {

	  nsIIterator * iterator;

	  mStreamObjects->CreateIterator(&iterator);
	  iterator->Init();

    nsStreamObject * item;

	  while(!(iterator->IsDone()))
	  {
		  item = (nsStreamObject *) iterator->CurrentItem();
		  NS_RELEASE(item);
		  iterator->Next();
	  }
	  NS_RELEASE(iterator);

    mStreamObjects->RemoveAll();
    NS_RELEASE(mStreamObjects);
  }

}

NS_DEFINE_IID(kIStreamManagerIID, NS_ISTREAM_MANAGER_IID);
NS_IMPL_ISUPPORTS(nsStreamManager,kIStreamManagerIID);

nsresult nsStreamManager::Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                              nsnull, 
                                              kCVectorCID, 
                                              (void **)&mStreamObjects);

  if (NS_OK != res)
    return res;

  mStreamObjects->Init();

  return NS_OK;
}

nsresult nsStreamManager::LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                                  nsIXPFCCanvas * aParentCanvas,
                                  const nsString& aURLSpec, 
                                  nsIPostData * aPostData,
                                  nsIID *aDTDIID,
                                  nsIID *aSinkIID)
{
  nsIID * iid_dtd  = aDTDIID;
  nsIID * iid_sink = aSinkIID;

  if (iid_dtd == nsnull)
    iid_dtd = (nsIID*)&kCCalXPFCXMLDTD;
  if (iid_sink == nsnull)
    iid_sink = (nsIID*)&kCCalXPFCXMLContentSinkCID;

  char * pUI = aURLSpec.ToNewCString();

  nsStreamObject * stream_object = nsnull;
  nsresult res = NS_OK;

  res = nsRepository::CreateInstance(kCStreamObjectCID, 
                                     nsnull, 
                                     kIStreamObjectIID,
                                     (void**) &stream_object);

  if (NS_OK != res) {
      return res;
  }

  stream_object->Init();

  mStreamObjects->Append(stream_object);

  /*
   * Create a nsIURL representing the interface ...
   */

  nsUrlParser urlParser(pUI);
  

  /*
   * Create a StreamObject
   */

  if (urlParser.IsLocalFile() == PR_TRUE) {
    char * pURL = urlParser.LocalFileToURL();
    res = NS_NewURL(&(stream_object->mUrl), pURL);
  } else {
    res = NS_NewURL(&(stream_object->mUrl), pUI);
  }

  /*
   *  Create the Parser
   */
  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  res = nsRepository::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&(stream_object->mParser));


  if (NS_OK != res) {
      return res;
  }

  res = stream_object->mParser->QueryInterface(kIStreamListenerIID, (void **)&(stream_object->mStreamListener));

  /*
   * Create the DTD and Sink
   */

  res = nsRepository::CreateInstance(*iid_dtd, 
                                     nsnull, 
                                     kIDTDIID,
                                     (void**) &(stream_object->mDTD));

  if (NS_OK != res) {
      return res;
  }


  res = nsRepository::CreateInstance(*iid_sink, 
                                     nsnull, 
                                     kIContentSinkIID,
                                     (void**) &(stream_object->mSink));

  if (NS_OK != res) {
      return res;
  }

  nsIXPFCXMLContentSink * sink ;

  static NS_DEFINE_IID(kIXPFCXMLContentSinkIID,  NS_IXPFC_XML_CONTENT_SINK_IID); 

  res = stream_object->mSink->QueryInterface(kIXPFCXMLContentSinkIID,(void**)&sink);

  if (NS_OK == res)
  {
    sink->SetViewerContainer(aWebViewerContainer);

    /*
     * Push Top of stack
     */

    if (nsnull != aParentCanvas)
    {
      sink->SetRootCanvas(aParentCanvas);

    }

    NS_RELEASE(sink);
  }

  /*
   * Register the DTD
   */

  stream_object->mParser->RegisterDTD(stream_object->mDTD);


  /*
   * Register the Context Sink, Parser, etc...
   */

  stream_object->mParser->SetContentSink(stream_object->mSink);


  stream_object->mDTD->SetContentSink(stream_object->mSink);
  stream_object->mDTD->SetParser(stream_object->mParser);

  /*
   * Open the URL
   */

  res = stream_object->mUrl->Open(stream_object->mStreamListener);


  /*
   * We want to parse when the Stream has data?
   */

  stream_object->mParser->Parse(stream_object->mUrl);

  delete pUI;

  return res;

}