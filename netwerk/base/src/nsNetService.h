/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsNetService_h__
#define nsNetService_h__

#include "nsINetService.h"

class nsNetService : public nsINetService
{
public:
    NS_DECL_ISUPPORTS

    // nsINetService methods:
    NS_IMETHOD GetProtocolHandler(nsIUrl* url, nsIProtocolHandler* *result);
    NS_IMETHOD NewConnectionGroup(nsIConnectionGroup* *result);
    NS_IMETHOD NewURL(nsIUrl* *result, 
                      const char* aSpec,
                      const nsIUrl* aBaseURL,
                      nsISupports* aContainer);
    NS_IMETHOD Open(nsIUrl* url, nsISupports* eventSink,
                    nsIConnectionGroup* aGroup,
                    nsIProtocolConnection* *result);
    NS_IMETHOD HasActiveConnections();

    // nsNetService methods:
    nsNetService();
    virtual ~nsNetService();

    nsresult Init();

protected:

};

#endif // nsNetService_h__
