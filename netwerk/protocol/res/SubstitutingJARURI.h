/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SubstitutingJARURI_h
#define SubstitutingJARURI_h

#include "nsIStandardURL.h"
#include "nsIURL.h"
#include "nsJARURI.h"
#include "nsISerializable.h"

namespace mozilla {
namespace net {

#define NS_SUBSTITUTINGJARURI_IMPL_CID               \
  { /* 8f8c54ed-aba7-4ebf-ba6f-e58aec0aba4c */       \
    0x8f8c54ed, 0xaba7, 0x4ebf, {                    \
      0xba, 0x6f, 0xe5, 0x8a, 0xec, 0x0a, 0xba, 0x4c \
    }                                                \
  }

// Provides a Substituting URI for resource://-like substitutions which
// allows consumers to access the underlying jar resource.
class SubstitutingJARURI : public nsIJARURI,
                           public nsIStandardURL,
                           public nsISerializable {
 protected:
  // Contains the resource://-like URI to be mapped. nsIURI and nsIURL will
  // forward to this.
  nsCOMPtr<nsIURL> mSource;
  // Contains the resolved jar resource, nsIJARURI forwards to this to let
  // consumer acccess the underlying jar resource.
  nsCOMPtr<nsIJARURI> mResolved;
  virtual ~SubstitutingJARURI() = default;

 public:
  SubstitutingJARURI(nsIURL* source, nsIJARURI* resolved);
  // For deserializing
  explicit SubstitutingJARURI() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISERIALIZABLE

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SUBSTITUTINGJARURI_IMPL_CID)

  NS_FORWARD_SAFE_NSIURL(mSource)
  NS_FORWARD_SAFE_NSIJARURI(mResolved)

  // enum used in a few places to specify how .ref attribute should be handled
  enum RefHandlingEnum { eIgnoreRef, eHonorRef };

  // We cannot forward Equals* methods to |mSource| so we override them here
  NS_IMETHOD EqualsExceptRef(nsIURI* aOther, bool* aResult) override;
  NS_IMETHOD Equals(nsIURI* aOther, bool* aResult) override;

  // Helper to share code between Equals methods.
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult);

  // Forward the rest of nsIURI to mSource
  NS_IMETHOD GetSpec(nsACString& aSpec) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetSpec(aSpec);
  }
  NS_IMETHOD GetPrePath(nsACString& aPrePath) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetPrePath(aPrePath);
  }
  NS_IMETHOD GetScheme(nsACString& aScheme) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetScheme(aScheme);
  }
  NS_IMETHOD GetUserPass(nsACString& aUserPass) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetUserPass(aUserPass);
  }
  NS_IMETHOD GetUsername(nsACString& aUsername) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetUsername(aUsername);
  }
  NS_IMETHOD GetPassword(nsACString& aPassword) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetPassword(aPassword);
  }
  NS_IMETHOD GetHostPort(nsACString& aHostPort) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetHostPort(aHostPort);
  }
  NS_IMETHOD GetHost(nsACString& aHost) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetHost(aHost);
  }
  NS_IMETHOD GetPort(int32_t* aPort) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetPort(aPort);
  }
  NS_IMETHOD GetPathQueryRef(nsACString& aPathQueryRef) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetPathQueryRef(aPathQueryRef);
  }
  NS_IMETHOD SchemeIs(const char* scheme, bool* _retval) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->SchemeIs(scheme, _retval);
  }
  NS_IMETHOD Resolve(const nsACString& relativePath,
                     nsACString& _retval) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->Resolve(relativePath, _retval);
  }
  NS_IMETHOD GetAsciiSpec(nsACString& aAsciiSpec) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetAsciiSpec(aAsciiSpec);
  }
  NS_IMETHOD GetAsciiHostPort(nsACString& aAsciiHostPort) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetAsciiHostPort(aAsciiHostPort);
  }
  NS_IMETHOD GetAsciiHost(nsACString& aAsciiHost) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetAsciiHost(aAsciiHost);
  }
  NS_IMETHOD GetRef(nsACString& aRef) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetRef(aRef);
  }
  NS_IMETHOD GetSpecIgnoringRef(nsACString& aSpecIgnoringRef) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetSpecIgnoringRef(aSpecIgnoringRef);
  }
  NS_IMETHOD GetHasRef(bool* aHasRef) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetHasRef(aHasRef);
  }
  NS_IMETHOD GetHasUserPass(bool* aHasUserPass) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetHasUserPass(aHasUserPass);
  }
  NS_IMETHOD GetHasQuery(bool* aHasQuery) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetHasQuery(aHasQuery);
  }
  NS_IMETHOD GetFilePath(nsACString& aFilePath) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetFilePath(aFilePath);
  }
  NS_IMETHOD GetQuery(nsACString& aQuery) override {
    return !mSource ? NS_ERROR_NULL_POINTER : mSource->GetQuery(aQuery);
  }
  NS_IMETHOD GetDisplayHost(nsACString& aDisplayHost) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetDisplayHost(aDisplayHost);
  }
  NS_IMETHOD GetDisplayHostPort(nsACString& aDisplayHostPort) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetDisplayHostPort(aDisplayHostPort);
  }
  NS_IMETHOD GetDisplaySpec(nsACString& aDisplaySpec) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetDisplaySpec(aDisplaySpec);
  }
  NS_IMETHOD GetDisplayPrePath(nsACString& aDisplayPrePath) override {
    return !mSource ? NS_ERROR_NULL_POINTER
                    : mSource->GetDisplayPrePath(aDisplayPrePath);
  }
  NS_IMETHOD Mutate(nsIURIMutator** _retval) override;
  NS_IMETHOD_(void) Serialize(mozilla::ipc::URIParams& aParams) override;

 private:
  nsresult Clone(nsIURI** aURI);
  nsresult SetSpecInternal(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetScheme(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetUserPass(const nsACString& aInput);
  nsresult SetUsername(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetPassword(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetHostPort(const nsACString& aValue) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetHost(const nsACString& input) { return NS_ERROR_NOT_IMPLEMENTED; }
  nsresult SetPort(int32_t aPort);
  nsresult SetPathQueryRef(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetRef(const nsACString& input) { return NS_ERROR_NOT_IMPLEMENTED; }
  nsresult SetFilePath(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetQuery(const nsACString& input) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult SetQueryWithEncoding(const nsACString& input,
                                const mozilla::Encoding* encoding) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  bool Deserialize(const mozilla::ipc::URIParams& aParams);
  nsresult ReadPrivate(nsIObjectInputStream* aStream);

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<SubstitutingJARURI>,
                        public nsISerializable {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    // nsISerializable overrides
    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    explicit Mutator() = default;

   private:
    virtual ~Mutator() = default;

    friend SubstitutingJARURI;
  };

  friend BaseURIMutator<SubstitutingJARURI>;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SubstitutingJARURI,
                              NS_SUBSTITUTINGJARURI_IMPL_CID)

}  // namespace net
}  // namespace mozilla

#endif /* SubstitutingJARURI_h */
