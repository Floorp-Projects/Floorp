/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   
 */

#ifndef _NSSOCKS4SOCKETPROVIDER_H_
#define _NSSOCKS4SOCKETPROVIDER_H_

#include "nsISOCKS4SocketProvider.h"

/* F7C9F5F4-4451-41c3-A28A-5BA2447FBACE */
#define NS_SOCKS4SOCKETPROVIDER_CID { 0xf7c9f5f4, 0x4451, 0x41c3, { 0xa2, 0x8a, 0x5b, 0xa2, 0x44, 0x7f, 0xba, 0xce } }

class nsSOCKS4SocketProvider : public nsISOCKS4SocketProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETPROVIDER
    NS_DECL_NSISOCKS4SOCKETPROVIDER
    
    // nsSOCKS4SocketProvider methods:
    nsSOCKS4SocketProvider();
    virtual ~nsSOCKS4SocketProvider();
    
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
    
    nsresult Init();
    
protected:
};

#endif /* _NSSOCKS4SOCKETPROVIDER_H_ */
