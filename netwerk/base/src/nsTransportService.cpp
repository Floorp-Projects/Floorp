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

#include "nsTransportService.h"

NS_IMPL_ISUPPORTS(nsTransportService, nsITransportService::GetIID());

////////////////////////////////////////////////////////////////////////////////
// nsTransportService methods:

nsTransportService::nsTransportService()
{
    NS_INIT_REFCNT();
}

nsTransportService::~nsTransportService()
{
}
    
nsresult
nsTransportService::Init()
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsITransportService methods:

NS_IMETHODIMP
nsTransportService::GetFileTransport(PLEventQueue* appEventQueue, nsISupports* eventSink,
                                     const char* path, 
                                     nsITransport* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTransportService::GetSocketTransport(PLEventQueue* appEventQueue, nsISupports* eventSink,
                                       const char* host, PRInt32 port,
                                       nsITransport* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTransportService::GetJarTransport(PLEventQueue* appEventQueue, nsISupports* eventSink,
                                    const char* jarFilePath, const char* jarEntryPath,
                                    nsITransport* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTransportService::HasActiveTransports()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////

