/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsSubProgressManager.h"


////////////////////////////////////////////////////////////////////////


nsSubProgressManager::nsSubProgressManager(MWContext* context)
    : nsProgressManager(context)
{
    // Grab a reference to the parent context's progress manager
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (pm)
        pm->AddRef();
}


nsSubProgressManager::~nsSubProgressManager(void)
{
    // Release the reference on the parent context's progress manager
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    if (pm)
        pm->Release();
}


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSubProgressManager::OnStartBinding(const URL_Struct* url)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnStartBinding(url);
}


NS_IMETHODIMP
nsSubProgressManager::OnProgress(const URL_Struct* url,
                                 PRUint32 bytesReceived,
                                 PRUint32 contentLength)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnProgress(url, bytesReceived, contentLength);
}


NS_IMETHODIMP
nsSubProgressManager::OnStatus(const URL_Struct* url, const char* message)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnStatus(url, message);
}


NS_IMETHODIMP
nsSubProgressManager::OnSuspend(const URL_Struct* url)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnSuspend(url);
}


NS_IMETHODIMP
nsSubProgressManager::OnResume(const URL_Struct* url)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnResume(url);
}


NS_IMETHODIMP
nsSubProgressManager::OnStopBinding(const URL_Struct* url,
                                    PRInt32 status,
                                    const char* message)
{
    nsITransferListener* pm = fContext->grid_parent->progressManager;
    PR_ASSERT(pm);
    if (! pm)
        return NS_ERROR_NULL_POINTER;

    return pm->OnStopBinding(url, status, message);
}



