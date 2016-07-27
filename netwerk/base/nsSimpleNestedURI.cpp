/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsSimpleNestedURI.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

#include "mozilla/ipc/URIUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED(nsSimpleNestedURI, nsSimpleURI, nsINestedURI)

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

    nsCOMPtr<nsISupports> supports;
    rv = aStream->ReadObject(true, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    mInnerURI = do_QueryInterface(supports, &rv);
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

// nsIIPCSerializableURI
void
nsSimpleNestedURI::Serialize(mozilla::ipc::URIParams& aParams)
{
    using namespace mozilla::ipc;

    SimpleNestedURIParams params;
    URIParams simpleParams;

    nsSimpleURI::Serialize(simpleParams);
    params.simpleParams() = simpleParams;

    SerializeURI(mInnerURI, params.innerURI());

    aParams = params;
}

bool
nsSimpleNestedURI::Deserialize(const mozilla::ipc::URIParams& aParams)
{
    using namespace mozilla::ipc;

    if (aParams.type() != URIParams::TSimpleNestedURIParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const SimpleNestedURIParams& params = aParams.get_SimpleNestedURIParams();
    if (!nsSimpleURI::Deserialize(params.simpleParams()))
        return false;

    mInnerURI = DeserializeURI(params.innerURI());

    NS_TryToSetImmutable(mInnerURI);
    return true;
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
nsSimpleNestedURI::StartClone(nsSimpleURI::RefHandlingEnum refHandlingMode,
                              const nsACString& newRef)
{
    NS_ENSURE_TRUE(mInnerURI, nullptr);
    
    nsCOMPtr<nsIURI> innerClone;
    nsresult rv;
    if (refHandlingMode == eHonorRef) {
        rv = mInnerURI->Clone(getter_AddRefs(innerClone));
    } else if (refHandlingMode == eReplaceRef) {
        rv = mInnerURI->CloneWithNewRef(newRef, getter_AddRefs(innerClone));
    } else {
        rv = mInnerURI->CloneIgnoringRef(getter_AddRefs(innerClone));
    }

    if (NS_FAILED(rv)) {
        return nullptr;
    }

    nsSimpleNestedURI* url = new nsSimpleNestedURI(innerClone);
    SetRefOnClone(url, refHandlingMode, newRef);
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

} // namespace net
} // namespace mozilla
