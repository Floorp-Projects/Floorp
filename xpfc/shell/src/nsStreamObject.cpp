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
#include "nsStreamObject.h"
#include "nsxpfcCIID.h"
#include "nsIContentSink.h"
#include "nsUrlParser.h"
#include "nspr.h"
#include "nsParserCIID.h"
#include "nsXPFCXMLContentSink.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCStreamObject, NS_STREAM_OBJECT_CID);
static NS_DEFINE_IID(kIStreamListenerIID,  NS_ISTREAMLISTENER_IID);

nsStreamObject::nsStreamObject()
{
  NS_INIT_REFCNT();
  mUrl = nsnull;
  mParser = nsnull;
  mDTD = nsnull;
  mSink = nsnull;
  mStreamListener = nsnull;
  mParentCanvas = nsnull;
}

nsStreamObject::~nsStreamObject()
{
  NS_IF_RELEASE(mUrl);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mSink);
  //NS_IF_RELEASE(mDTD);
  NS_IF_RELEASE(mStreamListener);
}

NS_DEFINE_IID(kIStreamObjectIID,   NS_ISTREAM_OBJECT_IID);

NS_IMPL_ADDREF(nsStreamObject)
NS_IMPL_RELEASE(nsStreamObject)

nsresult nsStreamObject::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIStreamObject*)(this);                                        
  }
  else if(aIID.Equals(kIStreamObjectIID)) {  //do this class...
    *aInstancePtr = (nsStreamObject*)(this);                                        
  }                 
  else if(aIID.Equals(kIStreamListenerIID)) {  //do this class...
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

nsresult nsStreamObject::Init()
{
  if (mParser != nsnull)
  {
    nsresult res = mParser->QueryInterface(kIStreamListenerIID, (void **)&mStreamListener);

    if (NS_OK != res)
      mStreamListener = nsnull;
  }
  return NS_OK;
}


nsresult nsStreamObject::GetBindInfo(nsIURL * aURL)
{
  return NS_OK;
}

nsresult nsStreamObject::OnDataAvailable(nsIURL * aURL,
					      nsIInputStream *aIStream, 
                                              PRInt32 aLength)
{
  return (mStreamListener->OnDataAvailable(aURL, aIStream, aLength));
}

nsresult nsStreamObject::OnStartBinding(nsIURL * aURL, 
					     const char *aContentType)
{
  return (mStreamListener->OnStartBinding(aURL,aContentType));
}

nsresult nsStreamObject::OnStopBinding(nsIURL * aURL, PRInt32 aStatus, const nsString &aMsg)
{
  return (mStreamListener->OnStopBinding(aURL, aStatus, aMsg));
}

nsresult nsStreamObject::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

nsresult nsStreamObject::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_OK;
}


