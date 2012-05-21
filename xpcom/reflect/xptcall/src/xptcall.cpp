/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* entry point wrappers. */

#include "xptcprivate.h"
#include "xptiprivate.h"

NS_IMETHODIMP
nsXPTCStubBase::QueryInterface(REFNSIID aIID,
                               void **aInstancePtr)
{
    if (aIID.Equals(mEntry->IID())) {
        NS_ADDREF_THIS();
        *aInstancePtr = static_cast<nsISupports*>(this);
        return NS_OK;
    }

    return mOuter->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt)
nsXPTCStubBase::AddRef()
{
    return mOuter->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsXPTCStubBase::Release()
{
    return mOuter->Release();
}

EXPORT_XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface* *aResult)
{
    NS_ENSURE_ARG(aOuter && aResult);

    xptiInterfaceInfoManager *iim =
        xptiInterfaceInfoManager::GetSingleton();
    NS_ENSURE_TRUE(iim, NS_ERROR_NOT_INITIALIZED);

    xptiInterfaceEntry *iie = iim->GetInterfaceEntryForIID(&aIID);
    if (!iie || !iie->EnsureResolved() || iie->GetBuiltinClassFlag())
        return NS_ERROR_FAILURE;

    nsXPTCStubBase* newbase = new nsXPTCStubBase(aOuter, iie);
    if (!newbase)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = newbase;
    return NS_OK;
}

EXPORT_XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub)
{
    nsXPTCStubBase* stub = static_cast<nsXPTCStubBase*>(aStub);
    delete(stub);
}
