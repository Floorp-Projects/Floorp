/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIURL_h___
#define nsIURL_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsILoadAttribs.h"

class nsIInputStream;
class nsIStreamListener;
class nsString;
class nsILoadGroup;

#define NS_IURL_IID           \
{ 0x6ecb2900, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIURI : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IURL_IID; return iid; }

  /** Equality operator */
  NS_IMETHOD_(PRBool) Equals(const nsIURI *aURL) const = 0;

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

  NS_IMETHOD GetLoadGroup(nsILoadGroup* *result) const = 0;
  NS_IMETHOD SetLoadGroup(nsILoadGroup* group) = 0;
  //@}

  NS_IMETHOD SetPostHeader(const char* name, const char* value) = 0;

  NS_IMETHOD SetPostData(nsIInputStream* input) = 0;
  
  NS_IMETHOD GetContentLength(PRInt32 *len) = 0;

  NS_IMETHOD GetServerStatus(PRInt32 *status) = 0;

  /** Write the URL to aString, overwriting previous contents. */
  NS_IMETHOD ToString(PRUnichar* *aString) const = 0;
};

// XXXwhh (re)move these...

/** Create a new URL, interpreting aSpec as relative to aURL (if non-null). */
extern NS_NET nsresult NS_NewURL(nsIURI** aInstancePtrResult,
                                 const nsString& aSpec,
                                 const nsIURI* aBaseURL = nsnull,
                                 nsISupports* aContainer = nsnull,
                                 nsILoadGroup* aGroup = nsnull);

/**
 * Utility routine to take a url (may be nsnull) and a base url (may
 * be empty), and a url spec and combine them properly into a new
 * absolute url.
 */
extern NS_NET nsresult NS_MakeAbsoluteURL(nsIURI* aURL,
                                          const nsString& aBaseURL,
                                          const nsString& aSpec,
                                          nsString& aResult);

extern NS_NET nsresult NS_OpenURL(nsIURI* aURL, nsIStreamListener* aConsumer);

extern NS_NET nsresult NS_OpenURL(nsIURI* aURL, nsIInputStream* *aNewStream,
                                  nsIStreamListener* aConsumer = nsnull);

#endif /* nsIURL_h___ */
