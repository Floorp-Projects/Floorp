/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSimpleURI_h__
#define nsSimpleURI_h__

#include "mozilla/MemoryReporting.h"
#include "nsIURI.h"
#include "nsISerializable.h"
#include "nsString.h"
#include "nsIClassInfo.h"
#include "nsIMutable.h"
#include "nsISizeOf.h"
#include "nsIIPCSerializableURI.h"
#include "nsIURIMutator.h"

namespace mozilla {
namespace net {

#define NS_THIS_SIMPLEURI_IMPLEMENTATION_CID         \
{ /* 0b9bb0c2-fee6-470b-b9b9-9fd9462b5e19 */         \
    0x0b9bb0c2,                                      \
    0xfee6,                                          \
    0x470b,                                          \
    {0xb9, 0xb9, 0x9f, 0xd9, 0x46, 0x2b, 0x5e, 0x19} \
}

class nsSimpleURI
    : public nsIURI
    , public nsISerializable
    , public nsIClassInfo
    , public nsIMutable
    , public nsISizeOf
    , public nsIIPCSerializableURI
{
protected:
    virtual ~nsSimpleURI();

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSIMUTABLE
    NS_DECL_NSIIPCSERIALIZABLEURI

    static already_AddRefed<nsSimpleURI> From(nsIURI* aURI);

    // nsSimpleURI methods:

    nsSimpleURI();

    bool Equals(nsSimpleURI* aOther)
    {
      return EqualsInternal(aOther, eHonorRef);
    }

    // nsISizeOf
    // Among the sub-classes that inherit (directly or indirectly) from
    // nsSimpleURI, measurement of the following members may be added later if
    // DMD finds it is worthwhile:
    // - nsJSURI: mBaseURI
    // - nsSimpleNestedURI: mInnerURI
    // - nsBlobURI: mPrincipal
    virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
    virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
    // enum used in a few places to specify how .ref attribute should be handled
    enum RefHandlingEnum {
        eIgnoreRef,
        eHonorRef,
        eReplaceRef
    };

    // Helper to share code between Equals methods.
    virtual nsresult EqualsInternal(nsIURI* other,
                                    RefHandlingEnum refHandlingMode,
                                    bool* result);

    // Helper to be used by inherited classes who want to test
    // equality given an assumed nsSimpleURI.  This must NOT check
    // the passed-in other for QI to our CID.
    bool EqualsInternal(nsSimpleURI* otherUri, RefHandlingEnum refHandlingMode);

    // Used by StartClone (and versions of StartClone in subclasses) to
    // handle the ref in the right way for clones.
    void SetRefOnClone(nsSimpleURI* url, RefHandlingEnum refHandlingMode,
                       const nsACString& newRef);

    // NOTE: This takes the refHandlingMode as an argument because
    // nsSimpleNestedURI's specialized version needs to know how to clone
    // its inner URI.
    virtual nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode,
                                    const nsACString& newRef);

    // Helper to share code between Clone methods.
    virtual nsresult CloneInternal(RefHandlingEnum refHandlingMode,
                                   const nsACString &newRef,
                                   nsIURI** clone);

    nsresult SetPathQueryRefEscaped(const nsACString &aPath, bool aNeedsEscape);

    nsCString mScheme;
    nsCString mPath; // NOTE: mPath does not include ref, as an optimization
    nsCString mRef;  // so that URIs with different refs can share string data.
    nsCString mQuery;  // so that URLs with different querys can share string data.
    bool mMutable;
    bool mIsRefValid; // To distinguish between empty-ref and no-ref.
    bool mIsQueryValid; // To distinguish between empty-query and no-query.


public:
    class Mutator
        : public nsIURIMutator
        , public BaseURIMutator<nsSimpleURI>
    {
        NS_DECL_ISUPPORTS
        NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
        NS_DEFINE_NSIMUTATOR_COMMON

        explicit Mutator() { }
    private:
        virtual ~Mutator() { }

        friend class nsSimpleURI;
    };
};

} // namespace net
} // namespace mozilla

#endif // nsSimpleURI_h__
