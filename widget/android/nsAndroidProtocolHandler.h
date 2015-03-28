/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAndroidProtocolHandler_h___
#define nsAndroidProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

#define NS_ANDROIDPROTOCOLHANDLER_CID                 \
{ /* e9cd2b7f-8386-441b-aaf5-0b371846bfd0 */         \
    0xe9cd2b7f,                                      \
    0x8386,                                          \
    0x441b,                                          \
    {0x0b, 0x37, 0x18, 0x46, 0xbf, 0xd0}             \
}

class nsAndroidProtocolHandler final : public nsIProtocolHandler,
                                       public nsSupportsWeakReference
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsAndroidProtocolHandler methods:
    nsAndroidProtocolHandler() {}

private:
    ~nsAndroidProtocolHandler() {}
};

#endif /* nsAndroidProtocolHandler_h___ */
