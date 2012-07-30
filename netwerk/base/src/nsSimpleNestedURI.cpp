/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCMessageUtils.h"
#include "mozilla/net/NeckoMessageUtils.h"

#include "nsSimpleNestedURI.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsNetUtil.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsSimpleNestedURI, nsSimpleURI, nsINestedURI)

nsSimpleNestedURI::nsSimpleNestedURI(nsIURI* innerURI)
    : mInnerURI(innerURI)
{
    NS_ASSERTION(innerURI, "Must have inner URI");
    NS_TryToSetImmutable(innerURI);
}
    
// nsISerializable

NS_IMETHODIMP
nsSimpleNestedURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = nsSimpleURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(!mMutable, "How did that happen?");

    rv = aStream->ReadObject(true, getter_AddRefs(mInnerURI));
    if (NS_FAILED(rv)) return rv;

    NS_TryToSetImmutable(mInnerURI);

    return rv;
}

NS_IMETHODIMP
nsSimpleNestedURI::Write(nsIObjectOutputStream* aStream)
{
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(mInnerURI);
    if (!serializable) {
        // We can't serialize ourselves
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult rv = nsSimpleURI::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteCompoundObject(mInnerURI, NS_GET_IID(nsIURI),
                                      true);
    return rv;
}

// nsIIPCSerializable

bool
nsSimpleNestedURI::Read(const IPC::Message *aMsg, void **aIter)
{
    if (!nsSimpleURI::Read(aMsg, aIter))
        return false;

    IPC::URI uri;
    if (!ReadParam(aMsg, aIter, &uri))
        return false;

    mInnerURI = uri;

    return true;
}

void
nsSimpleNestedURI::Write(IPC::Message *aMsg)
{
    nsSimpleURI::Write(aMsg);

    IPC::URI uri(mInnerURI);
    WriteParam(aMsg, uri);
}

// nsINestedURI

NS_IMETHODIMP
nsSimpleNestedURI::GetInnerURI(nsIURI** uri)
{
    NS_ENSURE_TRUE(mInnerURI, NS_ERROR_NOT_INITIALIZED);
    
    return NS_EnsureSafeToReturn(mInnerURI, uri);
}

NS_IMETHODIMP
nsSimpleNestedURI::GetInnermostURI(nsIURI** uri)
{
    return NS_ImplGetInnermostURI(this, uri);
}

// nsSimpleURI overrides
/* virtual */ nsresult
nsSimpleNestedURI::EqualsInternal(nsIURI* other,
                                  nsSimpleURI::RefHandlingEnum refHandlingMode,
                                  bool* result)
{
    *result = false;
    NS_ENSURE_TRUE(mInnerURI, NS_ERROR_NOT_INITIALIZED);
    
    if (other) {
        bool correctScheme;
        nsresult rv = other->SchemeIs(mScheme.get(), &correctScheme);
        NS_ENSURE_SUCCESS(rv, rv);

        if (correctScheme) {
            nsCOMPtr<nsINestedURI> nest = do_QueryInterface(other);
            if (nest) {
                nsCOMPtr<nsIURI> otherInner;
                rv = nest->GetInnerURI(getter_AddRefs(otherInner));
                NS_ENSURE_SUCCESS(rv, rv);

                return (refHandlingMode == eHonorRef) ?
                    otherInner->Equals(mInnerURI, result) :
                    otherInner->EqualsExceptRef(mInnerURI, result);
            }
        }
    }

    return NS_OK;
}

/* virtual */ nsSimpleURI*
nsSimpleNestedURI::StartClone(nsSimpleURI::RefHandlingEnum refHandlingMode)
{
    NS_ENSURE_TRUE(mInnerURI, nullptr);
    
    nsCOMPtr<nsIURI> innerClone;
    nsresult rv = refHandlingMode == eHonorRef ?
        mInnerURI->Clone(getter_AddRefs(innerClone)) :
        mInnerURI->CloneIgnoringRef(getter_AddRefs(innerClone));

    if (NS_FAILED(rv)) {
        return nullptr;
    }

    nsSimpleNestedURI* url = new nsSimpleNestedURI(innerClone);
    url->SetMutable(false);

    return url;
}

// nsIClassInfo overrides

NS_IMETHODIMP 
nsSimpleNestedURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    static NS_DEFINE_CID(kSimpleNestedURICID, NS_SIMPLENESTEDURI_CID);

    *aClassIDNoAlloc = kSimpleNestedURICID;
    return NS_OK;
}
