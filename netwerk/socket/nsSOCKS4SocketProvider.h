/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
