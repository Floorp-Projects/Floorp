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

#ifndef nsTransportService_h___
#define nsTransportService_h___

#include "nsITransportService.h"

class nsTransportService : public nsITransportService
{
public:
    NS_DECL_ISUPPORTS

    // nsITransportService methods:
    NS_IMETHOD GetFileTransport(PLEventQueue* appEventQueue,
                                nsIStreamListener* listener,
                                const char* path, 
                                nsITransport* *result);
    NS_IMETHOD GetSocketTransport(PLEventQueue* appEventQueue,
                                  nsIStreamListener* listener,
                                  const char* host, PRInt32 port,
                                  nsITransport* *result);
    NS_IMETHOD GetJarTransport(PLEventQueue* appEventQueue,
                               nsIStreamListener* listener,
                               const char* jarFilePath,
                               const char* jarEntryPath,
                               nsITransport* *result);
    NS_IMETHOD HasActiveTransports();

    // nsTransportService methods:
    nsTransportService();
    virtual ~nsTransportService();
    
    nsresult Init();

protected:

};

#endif /* nsTransportService_h___ */
