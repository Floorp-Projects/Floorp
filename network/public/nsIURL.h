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
#include "nsILoadAttribs.h"

class nsIInputStream;
class nsIStreamListener;
class nsString;
class nsIURLGroup;

#define NS_IURL_IID           \
{ 0x6ecb2900, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIURL : public nsISupports {
public:

  /** Equality operator */
  NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL) const = 0;

  // XXX Temporary...
  virtual PRBool operator==(const nsIURL &aURL) const {
    NS_ASSERTION(0, "change your code to use the Equals method");
    return PR_FALSE;
  }

  /** Accessors */
  //@{
  /** @return string originally used to construct the URL */
  NS_IMETHOD GetSpec(const char* *result) const = 0;

  NS_IMETHOD SetSpec(const char* spec) = 0;

  /** @return protocol part of the URL */
  NS_IMETHOD GetProtocol(const char* *result) const = 0;

  NS_IMETHOD SetProtocol(const char* protocol) = 0;

  /** @return host part of the URL */
  NS_IMETHOD GetHost(const char* *result) const = 0;

  NS_IMETHOD SetHost(const char* host) = 0;

  /** @return ref part of the URL */
  NS_IMETHOD GetHostPort(PRUint32 *result) const = 0;

  NS_IMETHOD SetHostPort(PRUint32 port) = 0;

  /** @return file part of the URL */
  NS_IMETHOD GetFile(const char* *result) const = 0;

  NS_IMETHOD SetFile(const char* file) = 0;

  /** @return ref part of the URL */
  NS_IMETHOD GetRef(const char* *result) const = 0;

  NS_IMETHOD SetRef(const char* ref) = 0;

  /** @return search part of the URL */
  NS_IMETHOD GetSearch(const char* *result) const = 0;

  NS_IMETHOD SetSearch(const char* search) = 0;

  NS_IMETHOD GetContainer(nsISupports* *result) const = 0;
  NS_IMETHOD SetContainer(nsISupports* container) = 0;

  NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result) const = 0;
  NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs) = 0;

  NS_IMETHOD GetURLGroup(nsIURLGroup* *result) const = 0;
  NS_IMETHOD SetURLGroup(nsIURLGroup* group) = 0;
  //@}

  NS_IMETHOD SetPostHeader(const char* name, const char* value) = 0;

  NS_IMETHOD SetPostData(nsIInputStream* input) = 0;

  /** Write the URL to aString, overwriting previous contents. */
  NS_IMETHOD ToString(PRUnichar* *aString) const = 0;

};

// XXXwhh (re)move these...

/** Create a new URL, interpreting aSpec as relative to aURL (if non-null). */
extern NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                                 const nsString& aSpec,
                                 const nsIURL* aBaseURL = nsnull,
                                 nsISupports* aContainer = nsnull,
                                 nsIURLGroup* aGroup = nsnull);

/**
 * Utility routine to take a url (may be nsnull) and a base url (may
 * be empty), and a url spec and combine them properly into a new
 * absolute url.
 */
extern NS_NET nsresult NS_MakeAbsoluteURL(nsIURL* aURL,
                                          const nsString& aBaseURL,
                                          const nsString& aSpec,
                                          nsString& aResult);

extern NS_NET nsresult NS_OpenURL(nsIURL* aURL, nsIStreamListener* aConsumer);

extern NS_NET nsresult NS_OpenURL(nsIURL* aURL, nsIInputStream* *aNewStream,
                                  nsIStreamListener* aConsumer = nsnull);

#endif /* nsIURL_h___ */
