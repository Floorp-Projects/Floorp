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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 */

#ifndef _NSTLSSOCKETPROVIDER_H_
#define _NSTLSSOCKETPROVIDER_H_

#include "nsISSLSocketProvider.h"


#define NS_STARTTLSSOCKETPROVIDER_CID \
{ /* b9507aec-1dd1-11b2-8cd5-c48ee0c50307 */         \
     0xb9507aec,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x8c, 0xd5, 0xc4, 0x8e, 0xe0, 0xc5, 0x03, 0x07} \
}

class nsTLSSocketProvider : public nsISSLSocketProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDER
  NS_DECL_NSISSLSOCKETPROVIDER
  
  // nsTLSSocketProvider methods:
  nsTLSSocketProvider();
  virtual ~nsTLSSocketProvider();
};

#endif /* _NSTLSSOCKETPROVIDER_H_ */
