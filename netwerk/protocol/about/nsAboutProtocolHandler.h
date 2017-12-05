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
    nsAboutProtocolHandler() {}

private:
    virtual ~nsAboutProtocolHandler() {}
};

class nsSafeAboutProtocolHandler final : public nsIProtocolHandler
                                       , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsSafeAboutProtocolHandler methods:
    nsSafeAboutProtocolHandler() {}

private:
    ~nsSafeAboutProtocolHandler() {}
};


// Class to allow us to propagate the base URI to about:blank correctly
class nsNestedAboutURI : public nsSimpleNestedURI {
public:
    nsNestedAboutURI(nsIURI* aInnerURI, nsIURI* aBaseURI)
        : nsSimpleNestedURI(aInnerURI)
        , mBaseURI(aBaseURI)
    {}

    // For use only from deserialization
    nsNestedAboutURI() : nsSimpleNestedURI() {}

    virtual ~nsNestedAboutURI() {}

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

public:
    class Mutator
        : public nsIURIMutator
        , public BaseURIMutator<nsNestedAboutURI>
    {
        NS_DECL_ISUPPORTS
        NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
        NS_DEFINE_NSIMUTATOR_COMMON

        explicit Mutator() { }
    private:
        virtual ~Mutator() { }

        friend class nsNestedAboutURI;
    };
};

} // namespace net
} // namespace mozilla

#endif /* nsAboutProtocolHandler_h___ */
