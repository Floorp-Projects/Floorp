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
#include "nsServiceManagerUtils.h"

#ifdef MOZILLA_STRICT_API

nsresult
CallGetService(const nsCID &aCID, const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIServiceManager> servMgr;
    nsresult status = NS_GetServiceManager(getter_AddRefs(servMgr));
    if (servMgr)
        status = servMgr->GetService(aCID, aIID, aResult);
    return status;
}

nsresult
CallGetService(const char *aContractID, const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIServiceManager> servMgr;
    nsresult status = NS_GetServiceManager(getter_AddRefs(servMgr));
    if (servMgr)
        status = servMgr->GetServiceByContractID(aContractID, aIID, aResult);
    return status;
}

#else

#include "nsComponentManager.h"

nsresult
CallGetService(const nsCID &aCID, const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->nsComponentManagerImpl::GetService(aCID, aIID, aResult);
}

nsresult
CallGetService(const char *aContractID, const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->
        nsComponentManagerImpl::GetServiceByContractID(aContractID,
                                                       aIID, aResult);
}

#endif

#ifdef MOZILLA_STRICT_API

nsresult
CallCreateInstance(const nsCID &aCID, nsISupports *aDelegate,
                   const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->CreateInstance(aCID, aDelegate, aIID, aResult);
    return status;
}

nsresult
CallCreateInstance(const char *aContractID, nsISupports *aDelegate,
                   const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->CreateInstanceByContractID(aContractID, aDelegate,
                                                     aIID, aResult);
    return status;
}

nsresult
CallGetClassObject(const nsCID &aCID, const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->GetClassObject(aCID, aIID, aResult);
    return status;
}

nsresult
CallGetClassObject(const char *aContractID, const nsIID &aIID, void **aResult)
{
    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult status = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (compMgr)
        status = compMgr->GetClassObjectByContractID(aContractID, aIID,
                                                     aResult);
    return status;
}

#else

#include "nsComponentManager.h"

nsresult
CallCreateInstance(const nsCID &aCID, nsISupports *aDelegate,
                   const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->
        nsComponentManagerImpl::CreateInstance(aCID, aDelegate, aIID, aResult);
}

nsresult
CallCreateInstance(const char *aContractID, nsISupports *aDelegate,
                   const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->
        nsComponentManagerImpl::CreateInstanceByContractID(aContractID,
                                                           aDelegate, aIID,
                                                           aResult);
}

nsresult
CallGetClassObject(const nsCID &aCID, const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->
        nsComponentManagerImpl::GetClassObject(aCID, aIID, aResult);
}

nsresult
CallGetClassObject(const char *aContractID, const nsIID &aIID, void **aResult)
{
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    NS_ENSURE_TRUE(compMgr, NS_ERROR_NOT_INITIALIZED);

    return compMgr->
        nsComponentManagerImpl::GetClassObjectByContractID(aContractID, aIID,
                                                           aResult);
}

#endif

nsresult
nsCreateInstanceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallCreateInstance(mCID, mOuter, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsCreateInstanceByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallCreateInstance(mContractID, mOuter, aIID, aInstancePtr);
    if (NS_FAILED(status))
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsCreateInstanceFromFactory::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = mFactory->CreateInstance(mOuter, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}


nsresult
nsGetClassObjectByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetClassObject(mCID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsGetClassObjectByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetClassObject(mContractID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}


nsresult
nsGetServiceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetService(mCID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;

    return status;
}

nsresult
nsGetServiceByCIDWithError::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetService(mCID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;

    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}

nsresult
nsGetServiceByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetService(mContractID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    return status;
}

nsresult
nsGetServiceByContractIDWithError::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = CallGetService(mContractID, aIID, aInstancePtr);
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}
