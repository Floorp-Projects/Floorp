/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* entry point wrappers. */

#include "xptcprivate.h"
#include "nsPrintfCString.h"

using namespace mozilla;

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

NS_IMETHODIMP_(MozExternalRefCountType)
nsXPTCStubBase::AddRef()
{
    return mOuter->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsXPTCStubBase::Release()
{
    return mOuter->Release();
}

EXPORT_XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface* *aResult)
{
    if (NS_WARN_IF(!aOuter) || NS_WARN_IF(!aResult))
        return NS_ERROR_INVALID_ARG;

    XPTInterfaceInfoManager *iim =
        XPTInterfaceInfoManager::GetSingleton();
    if (NS_WARN_IF(!iim))
        return NS_ERROR_NOT_INITIALIZED;

    xptiInterfaceEntry *iie = iim->GetInterfaceEntryForIID(&aIID);
    if (!iie || !iie->EnsureResolved() || iie->GetBuiltinClassFlag())
        return NS_ERROR_FAILURE;

    if (iie->GetHasNotXPCOMFlag()) {
#ifdef DEBUG
        nsPrintfCString msg("XPTCall will not implement interface %s because of [notxpcom] members.", iie->GetTheName());
        NS_WARNING(msg.get());
#endif
        return NS_ERROR_FAILURE;
    }

    *aResult = new nsXPTCStubBase(aOuter, iie);
    return NS_OK;
}

EXPORT_XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub)
{
    nsXPTCStubBase* stub = static_cast<nsXPTCStubBase*>(aStub);
    delete(stub);
}

EXPORT_XPCOM_API(size_t)
NS_SizeOfIncludingThisXPTCallStub(const nsISomeInterface* aStub,
                                  mozilla::MallocSizeOf aMallocSizeOf)
{
    // We could cast aStub to nsXPTCStubBase, but that class doesn't seem to own anything,
    // so just measure the size of the object itself.
    return aMallocSizeOf(aStub);
}
