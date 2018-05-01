/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutProtocolHandler_h___
#define nsAboutProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsSimpleNestedURI.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"
#include "nsIURIMutator.h"

class nsIURI;

namespace mozilla {
namespace net {

class nsAboutProtocolHandler : public nsIProtocolHandlerWithDynamicFlags
                             , public nsIProtocolHandler
                             , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS

    // nsAboutProtocolHandler methods:
    nsAboutProtocolHandler() = default;

private:
    virtual ~nsAboutProtocolHandler() = default;
};

class nsSafeAboutProtocolHandler final : public nsIProtocolHandler
                                       , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsSafeAboutProtocolHandler methods:
    nsSafeAboutProtocolHandler() = default;

private:
    ~nsSafeAboutProtocolHandler() = default;
};


// Class to allow us to propagate the base URI to about:blank correctly
class nsNestedAboutURI final
    : public nsSimpleNestedURI
{
private:
    nsNestedAboutURI(nsIURI* aInnerURI, nsIURI* aBaseURI)
        : nsSimpleNestedURI(aInnerURI)
        , mBaseURI(aBaseURI)
    {}
    nsNestedAboutURI() : nsSimpleNestedURI() {}
    virtual ~nsNestedAboutURI() = default;

public:
    // Override QI so we can QI to our CID as needed
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

    // Override StartClone(), the nsISerializable methods, and
    // GetClassIDNoAlloc; this last is needed to make our nsISerializable impl
    // work right.
    virtual nsSimpleURI* StartClone(RefHandlingEnum aRefHandlingMode,
                                    const nsACString& newRef) override;
    NS_IMETHOD Mutate(nsIURIMutator * *_retval) override;

    NS_IMETHOD Read(nsIObjectInputStream* aStream) override;
    NS_IMETHOD Write(nsIObjectOutputStream* aStream) override;
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc) override;

    nsIURI* GetBaseURI() const {
        return mBaseURI;
    }

protected:
    nsCOMPtr<nsIURI> mBaseURI;
    nsresult ReadPrivate(nsIObjectInputStream *stream);

public:
    class Mutator final
        : public nsIURIMutator
        , public BaseURIMutator<nsNestedAboutURI>
        , public nsISerializable
        , public nsINestedAboutURIMutator
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
            mURI->mMutable = false;
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
        InitWithBase(nsIURI* aInnerURI, nsIURI* aBaseURI) override
        {
            mURI = new nsNestedAboutURI(aInnerURI, aBaseURI);
            return NS_OK;
        }

        void ResetMutable()
        {
            if (mURI) {
                mURI->mMutable = true;
            }
        }

        friend class nsNestedAboutURI;
    };

    friend BaseURIMutator<nsNestedAboutURI>;
};

} // namespace net
} // namespace mozilla

#endif /* nsAboutProtocolHandler_h___ */
