/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwygProtocolHandler_h___
#define nsWyciwygProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
            
class nsWyciwygProtocolHandler : public nsIProtocolHandler
                               , public nsIObserver
                               , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER

    nsWyciwygProtocolHandler();
    virtual ~nsWyciwygProtocolHandler();

    nsresult Init();

    static void GetCacheSessionName(uint32_t aAppId,
                                    bool aInBrowser,
                                    bool aPrivateBrowsing,
                                    nsACString& aSessionName);
};

#endif /* nsWyciwygProtocolHandler_h___ */
