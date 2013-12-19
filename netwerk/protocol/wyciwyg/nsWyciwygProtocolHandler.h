/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwygProtocolHandler_h___
#define nsWyciwygProtocolHandler_h___

#include "nsIProtocolHandler.h"
            
class nsWyciwygProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    nsWyciwygProtocolHandler();
    virtual ~nsWyciwygProtocolHandler();
};

#endif /* nsWyciwygProtocolHandler_h___ */
