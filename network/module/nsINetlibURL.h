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

#ifndef nsINetlibURL_h___
#define nsINetlibURL_h___

#include "nsIURL.h"
#include "net.h"

/* forward declaration */
struct URL_Struct_;

#define NS_INETLIBURL_IID                            \
{ /* bcf62ef0-3267-11d2-8163-006008119d7a */         \
    0xbcf62ef0,                                      \
    0x3267,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * The nsINetlibURL interface provides browser-private access to URL
 * internals and integration with netlib without exposing the 
 * implementation.
 */
class nsINetlibURL : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_INETLIBURL_IID; return iid; }

  NS_IMETHOD GetURLInfo(URL_Struct_ **aResult) const = 0;
  NS_IMETHOD SetURLInfo(URL_Struct_ *URL_s) = 0;

};

#endif /* nsINetlibURL_h___ */
