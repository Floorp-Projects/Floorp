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
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

    // Override StartClone(), the nsISerializable methods, and
    // GetClassIDNoAlloc; this last is needed to make our nsISerializable impl
    // work right.
    virtual nsSimpleURI* StartClone(RefHandlingEnum aRefHandlingMode,
                                    const nsACString& newRef);
    NS_IMETHOD Read(nsIObjectInputStream* aStream);
    NS_IMETHOD Write(nsIObjectOutputStream* aStream);
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc);

    nsIURI* GetBaseURI() const {
        return mBaseURI;
    }

protected:
    nsCOMPtr<nsIURI> mBaseURI;
};

} // namespace net
} // namespace mozilla

#endif /* nsAboutProtocolHandler_h___ */
