/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXPCOM_h__
#include "nsXPCOM.h"
#endif

#ifndef nsCOMPtr_h__
#include "nsCOMPtr.h"
#endif

#include "nsComponentManagerUtils.h"
#include "nsIServiceManagerUtils.h"

nsresult
nsCreateInstanceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->CreateInstance(mCID, mOuter, aIID, aInstancePtr);
    else if (NS_SUCCEEDED(status))
        status = NS_ERROR_UNEXPECTED;

    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsCreateInstanceByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status;
    if ( mContractID ) {
        nsCOMPtr<nsIComponentManager> compMgr;
        status = NS_GetComponentManager(getter_AddRefs(compMgr));
        if (compMgr)
            status = compMgr->CreateInstanceByContractID(mContractID, mOuter,
                                                         aIID, aInstancePtr);
        else if (NS_SUCCEEDED(status))
            status = NS_ERROR_UNEXPECTED;
    }
    else
        status = NS_ERROR_NULL_POINTER;
    
    if (NS_FAILED(status))
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsCreateInstanceFromFactory::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status;
    if ( mFactory )
        status = mFactory->CreateInstance(mOuter, aIID, aInstancePtr);
    else
        status = NS_ERROR_NULL_POINTER;
    
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}


nsresult
nsGetClassObjectByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->GetClassObject(mCID, aIID, aInstancePtr);
    else if (NS_SUCCEEDED(status))
        status = NS_ERROR_UNEXPECTED;

    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsGetClassObjectByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status;
    if ( mContractID ) {
        nsCOMPtr<nsIComponentManager> compMgr;
        status = NS_GetComponentManager(getter_AddRefs(compMgr));
        if (compMgr)
            status = compMgr->GetClassObjectByContractID(mContractID,
                                                         aIID, aInstancePtr);
        else if (NS_SUCCEEDED(status))
            status = NS_ERROR_UNEXPECTED;
    }
    else
        status = NS_ERROR_NULL_POINTER;
    
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}


nsresult
nsGetServiceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = NS_ERROR_FAILURE;
    if ( mServiceManager ) {
        status = mServiceManager->GetService(mCID, aIID, (void**)aInstancePtr);
    } else {
        nsCOMPtr<nsIServiceManager> mgr;
        NS_GetServiceManager(getter_AddRefs(mgr));
        if (mgr)
            status = mgr->GetService(mCID, aIID, (void**)aInstancePtr);
    }
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsGetServiceByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = NS_ERROR_FAILURE;
    if ( mServiceManager ) {
        status = mServiceManager->GetServiceByContractID(mContractID, aIID, (void**)aInstancePtr);
    } else {
        nsCOMPtr<nsIServiceManager> mgr;
        NS_GetServiceManager(getter_AddRefs(mgr));
        if (mgr)
            status = mgr->GetServiceByContractID(mContractID, aIID, (void**)aInstancePtr);
    }
    
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}



