/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsISizeOf.h"
#include "nsIURIMutator.h"
#include "nsISimpleURIMutator.h"

namespace mozilla {
namespace net {

#define NS_THIS_SIMPLEURI_IMPLEMENTATION_CID         \
  { /* 0b9bb0c2-fee6-470b-b9b9-9fd9462b5e19 */       \
    0x0b9bb0c2, 0xfee6, 0x470b, {                    \
      0xb9, 0xb9, 0x9f, 0xd9, 0x46, 0x2b, 0x5e, 0x19 \
    }                                                \
  }

class nsSimpleURI : public nsIURI, public nsISerializable, public nsISizeOf {
 protected:
  nsSimpleURI() = default;
  virtual ~nsSimpleURI() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSISERIALIZABLE

  static already_AddRefed<nsSimpleURI> From(nsIURI* aURI);

  // nsSimpleURI methods:

  bool Equals(nsSimpleURI* aOther) { return EqualsInternal(aOther, eHonorRef); }

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
  enum RefHandlingEnum { eIgnoreRef, eHonorRef, eReplaceRef };

  virtual nsresult Clone(nsIURI** result);
  virtual nsresult SetSpecInternal(const nsACString& aSpec,
                                   bool aStripWhitespace = false);
  virtual nsresult SetScheme(const nsACString& input);
  virtual nsresult SetUserPass(const nsACString& input);
  nsresult SetUsername(const nsACString& input);
  virtual nsresult SetPassword(const nsACString& input);
  virtual nsresult SetHostPort(const nsACString& aValue);
  virtual nsresult SetHost(const nsACString& input);
  virtual nsresult SetPort(int32_t port);
  virtual nsresult SetPathQueryRef(const nsACString& aPath);
  virtual nsresult SetRef(const nsACString& aRef);
  virtual nsresult SetFilePath(const nsACString& aFilePath);
  virtual nsresult SetQuery(const nsACString& aQuery);
  virtual nsresult SetQueryWithEncoding(const nsACString& aQuery,
                                        const Encoding* encoding);
  nsresult ReadPrivate(nsIObjectInputStream* stream);

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
                                 const nsACString& newRef, nsIURI** result);

  void TrimTrailingCharactersFromPath();
  nsresult EscapeAndSetPathQueryRef(const nsACString& aPath);
  nsresult SetPathQueryRefInternal(const nsACString& aPath);

  bool Deserialize(const mozilla::ipc::URIParams&);

  nsCString mScheme;
  nsCString mPath;  // NOTE: mPath does not include ref, as an optimization
  nsCString mRef;   // so that URIs with different refs can share string data.
  nsCString
      mQuery;  // so that URLs with different querys can share string data.
  bool mIsRefValid{false};  // To distinguish between empty-ref and no-ref.
  // To distinguish between empty-query and no-query.
  bool mIsQueryValid{false};

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<nsSimpleURI>,
                        public nsISimpleURIMutator,
                        public nsISerializable {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    [[nodiscard]] NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    [[nodiscard]] NS_IMETHOD SetSpecAndFilterWhitespace(
        const nsACString& aSpec, nsIURIMutator** aMutator) override {
      if (aMutator) {
        *aMutator = do_AddRef(this).take();
      }

      nsresult rv = NS_OK;
      RefPtr<nsSimpleURI> uri = new nsSimpleURI();
      rv = uri->SetSpecInternal(aSpec, /* filterWhitespace */ true);
      if (NS_FAILED(rv)) {
        return rv;
      }
      mURI = std::move(uri);
      return NS_OK;
    }

    explicit Mutator() = default;

   private:
    virtual ~Mutator() = default;

    friend class nsSimpleURI;
  };

  friend BaseURIMutator<nsSimpleURI>;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsSimpleURI_h__
