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
#ifndef nsIURL_h___
#define nsIURL_h___

#include "nscore.h"
#include "nsISupports.h"

class nsIInputStream;
struct nsIStreamListener;
class nsString;

#define NS_IURL_IID           \
{ 0x6ecb2900, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/**
 * TEMPORARY INTERFACE; this <B><EM><STRONG>will</STRONG></EM></B> will be
 * going away
 */
class nsIURL : public nsISupports {
public:
  /** Open the url for reading and return a new input stream for the
   *  url. The caller must release the input stream when done with it.
   */
  virtual nsIInputStream* Open(PRInt32* aErrorCode) = 0;
  virtual nsresult Open(nsIStreamListener *) = 0;

  /** Equality operator */
  virtual PRBool operator==(const nsIURL& aURL) const = 0;


  virtual nsresult Set(const char *aNewSpec) = 0;

  /** Accessors */
  //@{
  /** 
      @return protocol part of the URL */
  virtual const char* GetProtocol() const = 0;

  /** @return host part of the URL */
  virtual const char* GetHost() const = 0;

  /** @return file part of the URL */
  virtual const char* GetFile() const = 0;

  /** @return ref part of the URL */
  virtual const char* GetRef() const = 0;

  /** @return string originally used to construct the URL */
  virtual const char* GetSpec() const = 0;

  /** @return ref part of the URL */
  virtual PRInt32 GetPort() const = 0;
  //@}

  /** Write the URL to aString, overwriting previous contents. */
  virtual void ToString(nsString& aString) const = 0;
};

/** Create a new URL from aSpec. */
extern NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                                 const nsString& aSpec);

/** Create a new URL, interpreting aSpec as relative to aURL. */
extern NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                                 const nsIURL* aURL,
                                 const nsString& aSpec);

/**
 * Utility routine to take a url (may be nsnull) and a base url (may
 * be empty), and a url spec and combine them properly into a new
 * absolute url.
 */
extern NS_NET nsresult NS_MakeAbsoluteURL(nsIURL* aURL,
                                          const nsString& aBaseURL,
                                          const nsString& aSpec,
                                          nsString& aResult);

#endif /* nsIURL_h___ */
