/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): Akkana Peck.
 */

#include "nsIStreamConverter.h"
#include "nsString.h"
#include "nsIFactory.h"

///////////////////////////////////////////////
// TextConverter
#define NS_TEXTCONVERTER_CID                         \
 { /* a6cf9109-15b3-11d2-932e-00805f8add32 */        \
    0xa6cf9109,                                      \
    0x15b3,                                          \
    0x11d2,                                          \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} \
  }

static NS_DEFINE_CID(kTextConverterCID,          NS_TEXTCONVERTER_CID);

class nsTextConverter : public nsIStreamConverter
{
 public:
  NS_DECL_ISUPPORTS

  nsTextConverter();
  virtual ~nsTextConverter();

  // nsIStreamConverter methods
  NS_IMETHOD Convert(nsIInputStream *aFromStream, const PRUnichar *aFromType, 
                     const PRUnichar *aToType, nsISupports *ctxt, nsIInputStream **_retval);


  NS_IMETHOD AsyncConvertData(const PRUnichar *aFromType,
                              const PRUnichar *aToType, 
                              nsIStreamListener *aListener, nsISupports *ctxt);

  // nsIStreamListener method
  NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                             nsIInputStream *inStr, 
                             PRUint32 sourceOffset, PRUint32 count);

  // nsIStreamObserver methods
  NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt);

  NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                           nsresult status, const PRUnichar *errorMsg);

  // member data
  nsIStreamListener *mListener;
  nsString mFromType;
  nsString mToType;
};

//////////////////////////////////////////////////
// FACTORY
class nsTextConverterFactory : public nsIFactory
{
public:
  nsTextConverterFactory(const nsCID &aClass, const char* className,
                         const char* contractID);

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

protected:
  virtual ~nsTextConverterFactory();

protected:
  nsCID       mClassID;
  const char* mClassName;
  const char* mContractID;
};
