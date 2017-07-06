/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * URI class to be used for cases when a simple URI actually resolves to some
 * other sort of URI, with the latter being what's loaded when the load
 * happens.  All objects of this class should always be immutable, so that the
 * inner URI and this URI don't get out of sync.  The Clone() implementation
 * will guarantee this for the clone, but it's up to the protocol handlers
 * creating these URIs to ensure that in the first place.  The innerURI passed
 * to this URI will be set immutable if possible.
 */

#ifndef nsSimpleNestedURI_h__
#define nsSimpleNestedURI_h__

#include "nsCOMPtr.h"
#include "nsSimpleURI.h"
#include "nsINestedURI.h"

#include "nsIIPCSerializableURI.h"

namespace mozilla {
namespace net {

class nsSimpleNestedURI : public nsSimpleURI,
                          public nsINestedURI
{
protected:
    ~nsSimpleNestedURI() {}

public:
    // To be used by deserialization only.  Leaves this object in an
    // uninitialized state that will throw on most accesses.
    nsSimpleNestedURI()
    {
    }

    // Constructor that should generally be used when constructing an object of
    // this class with |operator new|.
    explicit nsSimpleNestedURI(nsIURI* innerURI);

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINESTEDURI

    // Overrides for various methods nsSimpleURI implements follow.

    // nsSimpleURI overrides
    virtual nsresult EqualsInternal(nsIURI* other,
                                    RefHandlingEnum refHandlingMode,
                                    bool* result) override;
    virtual nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode,
                                    const nsACString& newRef) override;

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
};

} // namespace net
} // namespace mozilla

#endif /* nsSimpleNestedURI_h__ */
