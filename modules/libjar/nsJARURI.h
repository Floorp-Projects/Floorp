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
#include "nsIURIMutator.h"

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

#define NS_JARURIMUTATOR_CID                         \
{ /* 19d9161b-a2a9-4518-b2c9-fcb8296d6dcd */         \
    0x19d9161b,                                      \
    0xa2a9,                                          \
    0x4518,                                          \
    {0xb2, 0xc9, 0xfc, 0xb8, 0x29, 0x6d, 0x6d, 0xcd} \
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
    NS_DECL_NSIURL
    NS_DECL_NSIJARURI
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSINESTEDURI
    NS_DECL_NSIIPCSERIALIZABLEURI

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_THIS_JARURI_IMPL_CID)

    // nsJARURI
    nsresult FormatSpec(const nsACString &entryPath, nsACString &result,
                        bool aIncludeScheme = true);
    nsresult CreateEntryURL(const nsACString& entryFilename,
                            const char* charset,
                            nsIURL** url);

protected:
    nsJARURI();
    virtual ~nsJARURI();
    nsresult SetJAREntry(const nsACString &entryPath);
    nsresult Init(const char *charsetHint);
    nsresult SetSpecWithBase(const nsACString& aSpec, nsIURI* aBaseURL);

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

private:
    nsresult Clone(nsIURI** aURI);
    nsresult SetSpecInternal(const nsACString &input);
    nsresult SetScheme(const nsACString &input);
    nsresult SetUserPass(const nsACString &input);
    nsresult SetUsername(const nsACString &input);
    nsresult SetPassword(const nsACString &input);
    nsresult SetHostPort(const nsACString &aValue);
    nsresult SetHost(const nsACString &input);
    nsresult SetPort(int32_t port);
    nsresult SetPathQueryRef(const nsACString &input);
    nsresult SetRef(const nsACString &input);
    nsresult SetFilePath(const nsACString &input);
    nsresult SetQuery(const nsACString &input);
    nsresult SetQueryWithEncoding(const nsACString &input, const mozilla::Encoding* encoding);
    bool Deserialize(const mozilla::ipc::URIParams&);
    nsresult ReadPrivate(nsIObjectInputStream *aStream);

    nsresult SetFileNameInternal(const nsACString& fileName);
    nsresult SetFileBaseNameInternal(const nsACString& fileBaseName);
    nsresult SetFileExtensionInternal(const nsACString& fileExtension);

public:
    class Mutator final
        : public nsIURIMutator
        , public BaseURIMutator<nsJARURI>
        , public nsIURLMutator
        , public nsISerializable
        , public nsIJARURIMutator
    {
        NS_DECL_ISUPPORTS
        NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
        NS_DEFINE_NSIMUTATOR_COMMON
        NS_DECL_NSIURLMUTATOR

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

        NS_IMETHOD
        SetSpecBaseCharset(const nsACString& aSpec,
                           nsIURI* aBaseURI,
                           const char* aCharset) override
        {
            RefPtr<nsJARURI> uri;
            if (mURI) {
                mURI.swap(uri);
            } else {
                uri = new nsJARURI();
            }

            nsresult rv = uri->Init(aCharset);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = uri->SetSpecWithBase(aSpec, aBaseURI);
            if (NS_FAILED(rv)) {
                return rv;
            }

            mURI.swap(uri);
            return NS_OK;
        }

        explicit Mutator() { }
    private:
        virtual ~Mutator() { }

        friend class nsJARURI;
    };

    friend BaseURIMutator<nsJARURI>;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsJARURI, NS_THIS_JARURI_IMPL_CID)

#endif // nsJARURI_h__
