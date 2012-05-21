/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUDPSocketProvider_h__
#define nsUDPSocketProvider_h__

#include "nsISocketProvider.h"

class nsUDPSocketProvider : public nsISocketProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETPROVIDER

private:
    ~nsUDPSocketProvider();

};

#endif /* nsUDPSocketProvider_h__ */

