/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARURI_h__
#define nsJARURI_h__

#include "nsIJARURI.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsINestedURI.h"
#include "nsIIPCSerializableURI.h"

#define NS_THIS_JARURI_IMPL_CID                      \
{ /* 9a55f629-730b-4d08-b75b-fa7d9570a691 */         \
    0x9a55f629,                                      \
    0x730b,                                          \
    0x4d08,                                          \
    {0xb7, 0x5b, 0xfa, 0x7d, 0x95, 0x70, 0xa6, 0x91} \
}

#define NS_JARURI_CID                                \
{ /* 245abae2-b947-4ded-a46d-9829d3cca462 */         \
    0x245abae2,                                      \
    0xb947,                                          \
    0x4ded,                                          \
    {0xa4, 0x6d, 0x98, 0x29, 0xd3, 0xcc, 0xa4, 0x62} \
}


class nsJARURI final : public nsIJARURI,
                       public nsISerializable,
                       public nsIClassInfo,
                       public nsINestedURI,
                       public nsIIPCSerializableURI
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURIWITHQUERY
    NS_DECL_NSIURL
    NS_DECL_NSIJARURI
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSINESTEDURI
    NS_DECL_NSIIPCSERIALIZABLEURI

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_THIS_JARURI_IMPL_CID)

    // nsJARURI
    nsJARURI();
   
    nsresult Init(const char *charsetHint);
    nsresult FormatSpec(const nsACString &entryPath, nsACString &result,
                        bool aIncludeScheme = true);
    nsresult CreateEntryURL(const nsACString& entryFilename,
                            const char* charset,
                            nsIURL** url);
    nsresult SetSpecWithBase(const nsACString& aSpec, nsIURI* aBaseURL);

protected:
    virtual ~nsJARURI();

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

    // Helpers to share code between Clone methods.
    nsresult CloneWithJARFileInternal(nsIURI *jarFile,
                                      RefHandlingEnum refHandlingMode,
                                      nsIJARURI **result);
    nsresult CloneWithJARFileInternal(nsIURI *jarFile,
                                      RefHandlingEnum refHandlingMode,
                                      const nsACString& newRef,
                                      nsIJARURI **result);
    nsCOMPtr<nsIURI> mJARFile;
    // mJarEntry stored as a URL so that we can easily access things
    // like extensions, refs, etc.
    nsCOMPtr<nsIURL> mJAREntry;
    nsCString        mCharsetHint;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsJARURI, NS_THIS_JARURI_IMPL_CID)

#endif // nsJARURI_h__
