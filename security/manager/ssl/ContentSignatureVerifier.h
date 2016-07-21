/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef ContentSignatureVerifier_h
#define ContentSignatureVerifier_h

#include "cert.h"
#include "CSTrustDomain.h"
#include "nsIContentSignatureVerifier.h"
#include "nsIStreamListener.h"
#include "nsNSSShutDown.h"
#include "ScopedNSSTypes.h"

// 45a5fe2f-c350-4b86-962d-02d5aaaa955a
#define NS_CONTENTSIGNATUREVERIFIER_CID \
  { 0x45a5fe2f, 0xc350, 0x4b86, \
    { 0x96, 0x2d, 0x02, 0xd5, 0xaa, 0xaa, 0x95, 0x5a } }
#define NS_CONTENTSIGNATUREVERIFIER_CONTRACTID \
    "@mozilla.org/security/contentsignatureverifier;1"

class ContentSignatureVerifier final : public nsIContentSignatureVerifier
                                     , public nsIStreamListener
                                     , public nsNSSShutDownObject
                                     , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSIGNATUREVERIFIER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  ContentSignatureVerifier()
    : mCx(nullptr)
    , mInitialised(false)
    , mHasCertChain(false)
  {
  }

  // nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override
  {
    destructorSafeDestroyNSSReference();
  }

private:
  ~ContentSignatureVerifier();

  nsresult UpdateInternal(const nsACString& aData,
                          const nsNSSShutDownPreventionLock& /*proofOfLock*/);
  nsresult DownloadCertChain();
  nsresult CreateContextInternal(const nsACString& aData,
                                 const nsACString& aCertChain,
                                 const nsACString& aName);

  void destructorSafeDestroyNSSReference()
  {
    mCx = nullptr;
    mKey = nullptr;
  }

  nsresult ParseContentSignatureHeader(const nsACString& aContentSignatureHeader);

  // verifier context for incremental verifications
  mozilla::UniqueVFYContext mCx;
  bool mInitialised;
  // Indicates whether we hold a cert chain to verify the signature or not.
  // It's set by default in CreateContext or when the channel created in
  // DownloadCertChain finished. Update and End must only be called after
  // mHashCertChain is set.
  bool mHasCertChain;
  // signature to verify
  nsCString mSignature;
  // x5u (X.509 URL) value pointing to pem cert chain
  nsCString mCertChainURL;
  // the downloaded cert chain to verify against
  FallibleTArray<nsCString> mCertChain;
  // verification key
  mozilla::UniqueSECKEYPublicKey mKey;
  // name of the verifying context
  nsCString mName;
  // callback to notify when finished
  nsCOMPtr<nsIContentSignatureReceiverCallback> mCallback;
  // channel to download the cert chain
  nsCOMPtr<nsIChannel> mChannel;
};

#endif // ContentSignatureVerifier_h
