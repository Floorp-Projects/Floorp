/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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



