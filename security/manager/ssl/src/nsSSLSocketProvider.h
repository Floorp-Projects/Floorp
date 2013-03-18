/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSSLSOCKETPROVIDER_H_
#define _NSSSLSOCKETPROVIDER_H_

#include "nsISocketProvider.h"

/* 217d014a-1dd2-11b2-999c-b0c4df79b324 */
#define NS_SSLSOCKETPROVIDER_CID   \
{ 0x217d014a, 0x1dd2, 0x11b2, {0x99, 0x9c, 0xb0, 0xc4, 0xdf, 0x79, 0xb3, 0x24}}


class nsSSLSocketProvider : public nsISocketProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDER
  
  // nsSSLSocketProvider methods:
  nsSSLSocketProvider();
  virtual ~nsSSLSocketProvider();
};

#endif /* _NSSSLSOCKETPROVIDER_H_ */
