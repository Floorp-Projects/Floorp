/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * URI class to be used for cases when a simple URI actually resolves to some
 * other sort of URI, with the latter being what's loaded when the load
 * happens.
 */

#ifndef nsSimpleNestedURI_h__
#define nsSimpleNestedURI_h__

#include "nsCOMPtr.h"
#include "nsSimpleURI.h"
#include "nsINestedURI.h"
#include "nsIURIMutator.h"

#include "nsIIPCSerializableURI.h"

namespace mozilla {
namespace net {

class nsSimpleNestedURI : public nsSimpleURI,
                          public nsINestedURI
{
protected:
    nsSimpleNestedURI() = default;
    explicit nsSimpleNestedURI(nsIURI* innerURI);

    ~nsSimpleNestedURI() = default;
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINESTEDURI

    // Overrides for various methods nsSimpleURI implements follow.

    // nsSimpleURI overrides
    virtual nsresult EqualsInternal(nsIURI* other,
                                    RefHandlingEnum refHandlingMode,
                                    bool* result) override;
    virtual nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode,
                                    const nsACString& newRef) override;
    NS_IMETHOD Mutate(nsIURIMutator * *_retval) override;

    // nsISerializable overrides
    NS_IMETHOD Read(nsIObjectInputStream* aStream) override;
    NS_IMETHOD Write(nsIObjectOutputStream* aStream) override;

    // nsIIPCSerializableURI overrides
    NS_DECL_NSIIPCSERIALIZABLEURI

    // Override the nsIClassInfo method GetClassIDNoAlloc to make sure our
    // nsISerializable impl works right.
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc) override;

protected:
    nsCOMPtr<nsIURI> mInnerURI;

    nsresult SetPathQueryRef(const nsACString &aPathQueryRef) override;
    nsresult SetQuery(const nsACString &aQuery) override;
    nsresult SetRef(const nsACString &aRef) override;
    bool Deserialize(const mozilla::ipc::URIParams&);
    nsresult ReadPrivate(nsIObjectInputStream *stream);

public:
    class Mutator final
        : public nsIURIMutator
        , public BaseURIMutator<nsSimpleNestedURI>
        , public nsISerializable
        , public nsINestedURIMutator
    {
        NS_DECL_ISUPPORTS
        NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)

        explicit Mutator() = default;
    private:
        virtual ~Mutator() = default;

        MOZ_MUST_USE NS_IMETHOD
        Deserialize(const mozilla::ipc::URIParams& aParams) override
        {
            return InitFromIPCParams(aParams);
        }

        NS_IMETHOD
        Write(nsIObjectOutputStream *aOutputStream) override
        {
            return NS_ERROR_NOT_IMPLEMENTED;
        }

        MOZ_MUST_USE NS_IMETHOD
        Read(nsIObjectInputStream* aStream) override
        {
            return InitFromInputStream(aStream);
        }

        MOZ_MUST_USE NS_IMETHOD
        Finalize(nsIURI** aURI) override
        {
            mURI.forget(aURI);
            return NS_OK;
        }

        MOZ_MUST_USE NS_IMETHOD
        SetSpec(const nsACString& aSpec, nsIURIMutator** aMutator) override
        {
            if (aMutator) {
                NS_ADDREF(*aMutator = this);
            }
            return InitFromSpec(aSpec);
        }

        MOZ_MUST_USE NS_IMETHOD
        Init(nsIURI* innerURI) override
        {
            mURI = new nsSimpleNestedURI(innerURI);
            return NS_OK;
        }

        friend class nsSimpleNestedURI;
    };

    friend BaseURIMutator<nsSimpleNestedURI>;
};

} // namespace net
} // namespace mozilla

#endif /* nsSimpleNestedURI_h__ */
