/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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

#ifndef _NSSSLSOCKETPROVIDER_H_
#define _NSSSLSOCKETPROVIDER_H_

#include "nsISSLSocketProvider.h"


/* 274418d0-5437-11d3-bbc8-0000861d1237 */         
#define NS_SSLSOCKETPROVIDER_CID   { 0x274418d0, 0x5437, 0x11d3, {0xbb, 0xc8, 0x00, 0x00, 0x86, 0x1d, 0x12, 0x37}}


class nsSSLSocketProvider : public nsISSLSocketProvider
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSISOCKETPROVIDER

  NS_DECL_NSISSLSOCKETPROVIDER

  // nsSSLSocketProvider methods:
  nsSSLSocketProvider();
  virtual ~nsSSLSocketProvider();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  nsresult Init();

protected:
};

#endif /* _NSSSLSOCKETPROVIDER_H_ */
