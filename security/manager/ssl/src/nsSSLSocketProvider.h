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
 *  Brian Ryner <bryner@brianryner.com>
 */

#ifndef _NSSSLSOCKETPROVIDER_H_
#define _NSSSLSOCKETPROVIDER_H_

#include "nsISSLSocketProvider.h"

/* 217d014a-1dd2-11b2-999c-b0c4df79b324 */
#define NS_SSLSOCKETPROVIDER_CID   \
{ 0x217d014a, 0x1dd2, 0x11b2, {0x99, 0x9c, 0xb0, 0xc4, 0xdf, 0x79, 0xb3, 0x24}}


class nsSSLSocketProvider : public nsISSLSocketProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDER
  NS_DECL_NSISSLSOCKETPROVIDER
  
  // nsSSLSocketProvider methods:
  nsSSLSocketProvider();
  virtual ~nsSSLSocketProvider();
};

#endif /* _NSSSLSOCKETPROVIDER_H_ */
