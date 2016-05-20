/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentSignatureVerifier.h"

#include "BRNameMatchingPolicy.h"
#include "SharedCertVerifier.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "mozilla/Casting.h"
#include "nsCOMPtr.h"
#include "nsNSSComponent.h"
#include "nsSecurityHeaderParser.h"
#include "nsWhitespaceTokenizer.h"
#include "nsXPCOMStrings.h"
#include "nssb64.h"
#include "pkix/pkix.h"
#include "pkix/pkixtypes.h"
#include "secerr.h"

NS_IMPL_ISUPPORTS(ContentSignatureVerifier, nsIContentSignatureVerifier)

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::psm;

static LazyLogModule gCSVerifierPRLog("ContentSignatureVerifier");
#define CSVerifier_LOG(args) MOZ_LOG(gCSVerifierPRLog, LogLevel::Debug, args)

// Content-Signature prefix
const nsLiteralCString kPREFIX = NS_LITERAL_CSTRING("Content-Signature:\x00");

ContentSignatureVerifier::~ContentSignatureVerifier()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

NS_IMETHODIMP
ContentSignatureVerifier::VerifyContentSignature(
  const nsACString& aData, const nsACString& aCSHeader,
  const nsACString& aCertChain, const nsACString& aName, bool* _retval)
{
  NS_ENSURE_ARG(_retval);
  nsresult rv = CreateContext(aData, aCSHeader, aCertChain, aName);
  if (NS_FAILED(rv)) {
    *_retval = false;
    CSVerifier_LOG(("CSVerifier: Signature verification failed\n"));
    if (rv == NS_ERROR_INVALID_SIGNATURE) {
      return NS_OK;
    }
    return rv;
  }

  return End(_retval);
}

bool
IsNewLine(char16_t c)
{
  return c == '\n' || c == '\r';
}

nsresult
ReadChainIntoCertList(const nsACString& aCertChain, CERTCertList* aCertList,
                      const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  bool inBlock = false;
  bool certFound = false;

  const nsCString header = NS_LITERAL_CSTRING("-----BEGIN CERTIFICATE-----");
  const nsCString footer = NS_LITERAL_CSTRING("-----END CERTIFICATE-----");

  nsCWhitespaceTokenizerTemplate<IsNewLine> tokenizer(aCertChain);

  nsAutoCString blockData;
  while (tokenizer.hasMoreTokens()) {
    nsDependentCSubstring token = tokenizer.nextToken();
    if (token.IsEmpty()) {
      continue;
    }
    if (inBlock) {
      if (token.Equals(footer)) {
        inBlock = false;
        certFound = true;
        // base64 decode data, make certs, append to chain
        UniqueSECItem der(::SECITEM_AllocItem(nullptr, nullptr, 0));
        if (!der || !NSSBase64_DecodeBuffer(nullptr, der.get(),
                                            blockData.BeginReading(),
                                            blockData.Length())) {
          CSVerifier_LOG(("CSVerifier: decoding the signature failed\n"));
          return NS_ERROR_FAILURE;
        }
        CERTCertificate* tmpCert =
          CERT_NewTempCertificate(CERT_GetDefaultCertDB(), der.get(), nullptr,
                                  false, true);
        if (!tmpCert) {
          return NS_ERROR_FAILURE;
        }
        // if adding tmpCert succeeds, tmpCert will now be owned by aCertList
        SECStatus res = CERT_AddCertToListTail(aCertList, tmpCert);
        if (res != SECSuccess) {
          CERT_DestroyCertificate(tmpCert);
          return MapSECStatus(res);
        }
      } else {
        blockData.Append(token);
      }
    } else if (token.Equals(header)) {
      inBlock = true;
      blockData = "";
    }
  }
  if (inBlock || !certFound) {
    // the PEM data did not end; bad data.
    CSVerifier_LOG(("CSVerifier: supplied chain contains bad data\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// Create a context for a content signature verification.
// It sets signature, certificate chain and name that should be used to verify
// the data. The data parameter is the first part of the data to verify (this
// can be the empty string).
NS_IMETHODIMP
ContentSignatureVerifier::CreateContext(const nsACString& aData,
                                        const nsACString& aCSHeader,
                                        const nsACString& aCertChain,
                                        const nsACString& aName)
{
  MutexAutoLock lock(mMutex);
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
  }

  if (mCx) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  UniqueCERTCertList certCertList(CERT_NewCertList());
  if (!certCertList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = ReadChainIntoCertList(aCertChain, certCertList.get(), locker);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CERTCertListNode* node = CERT_LIST_HEAD(certCertList.get());
  if (!node || !node->cert) {
    return NS_ERROR_FAILURE;
  }

  SECItem* certSecItem = &node->cert->derCert;

  Input certDER;
  Result result =
    certDER.Init(BitwiseCast<uint8_t*, unsigned char*>(certSecItem->data),
                 certSecItem->len);
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }


  // Check the signerCert chain is good
  CSTrustDomain trustDomain(certCertList);
  result = BuildCertChain(trustDomain, certDER, Now(),
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::noParticularKeyUsageRequired,
                          KeyPurposeId::id_kp_codeSigning,
                          CertPolicyId::anyPolicy,
                          nullptr/*stapledOCSPResponse*/);
  if (result != Success) {
    // the chain is bad
    CSVerifier_LOG(("CSVerifier: The supplied chain is bad\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Check the SAN
  Input hostnameInput;

  result = hostnameInput.Init(uint8_t_ptr_cast(aName.BeginReading()),
                              aName.Length());
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }

  BRNameMatchingPolicy nameMatchingPolicy(BRNameMatchingPolicy::Mode::Enforce);
  result = CheckCertHostname(certDER, hostnameInput, nameMatchingPolicy);
  if (result != Success) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  mKey.reset(CERT_ExtractPublicKey(node->cert));

  // in case we were not able to extract a key
  if (!mKey) {
    CSVerifier_LOG(("CSVerifier: unable to extract a key\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // we get the raw content-signature header here, so first parse aCSHeader
  rv = ParseContentSignatureHeader(aCSHeader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Base 64 decode the signature
  UniqueSECItem rawSignatureItem(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (!rawSignatureItem ||
      !NSSBase64_DecodeBuffer(nullptr, rawSignatureItem.get(),
                              mSignature.get(), mSignature.Length())) {
    CSVerifier_LOG(("CSVerifier: decoding the signature failed\n"));
    return NS_ERROR_FAILURE;
  }

  // get signature object
  UniqueSECItem signatureItem(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (!signatureItem) {
    return NS_ERROR_FAILURE;
  }
  // We have a raw ecdsa signature r||s so we have to DER-encode it first
  // Note that we have to check rawSignatureItem->len % 2 here as
  // DSAU_EncodeDerSigWithLen asserts this
  if (rawSignatureItem->len == 0 || rawSignatureItem->len % 2 != 0) {
    CSVerifier_LOG(("CSVerifier: signature length is bad\n"));
    return NS_ERROR_FAILURE;
  }
  if (DSAU_EncodeDerSigWithLen(signatureItem.get(), rawSignatureItem.get(),
                               rawSignatureItem->len) != SECSuccess) {
    CSVerifier_LOG(("CSVerifier: encoding the signature failed\n"));
    return NS_ERROR_FAILURE;
  }

  // this is the only OID we support for now
  SECOidTag oid = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;

  mCx = UniqueVFYContext(
    VFY_CreateContext(mKey.get(), signatureItem.get(), oid, nullptr));

  if (!mCx) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  if (VFY_Begin(mCx.get()) != SECSuccess) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  rv = UpdateInternal(kPREFIX, lock, locker);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // add data if we got any
  return UpdateInternal(aData, lock, locker);
}

nsresult
ContentSignatureVerifier::UpdateInternal(const nsACString& aData,
  MutexAutoLock& /*proofOfLock*/,
  const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  if (!aData.IsEmpty()) {
    if (VFY_Update(mCx.get(), (const unsigned char*)nsPromiseFlatCString(aData).get(),
                   aData.Length()) != SECSuccess){
      return NS_ERROR_INVALID_SIGNATURE;
    }
  }
  return NS_OK;
}

/**
 * Add data to the context that shold be verified.
 */
NS_IMETHODIMP
ContentSignatureVerifier::Update(const nsACString& aData)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
  }
  MutexAutoLock lock(mMutex);
  return UpdateInternal(aData, lock, locker);
}

/**
 * Finish signature verification and return the result in _retval.
 */
NS_IMETHODIMP
ContentSignatureVerifier::End(bool* _retval)
{
  NS_ENSURE_ARG(_retval);
  MutexAutoLock lock(mMutex);
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
  }

  *_retval = (VFY_End(mCx.get()) == SECSuccess);

  return NS_OK;
}

nsresult
ContentSignatureVerifier::ParseContentSignatureHeader(
  const nsACString& aContentSignatureHeader)
{
  // We only support p384 ecdsa according to spec
  NS_NAMED_LITERAL_CSTRING(signature_var, "p384ecdsa");

  nsSecurityHeaderParser parser(aContentSignatureHeader.BeginReading());
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: could not parse ContentSignature header\n"));
    return NS_ERROR_FAILURE;
  }
  LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    CSVerifier_LOG(("CSVerifier: found directive %s\n", directive->mName.get()));
    if (directive->mName.Length() == signature_var.Length() &&
        directive->mName.EqualsIgnoreCase(signature_var.get(),
                                          signature_var.Length())) {
      if (!mSignature.IsEmpty()) {
        CSVerifier_LOG(("CSVerifier: found two ContentSignatures\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSVerifier_LOG(("CSVerifier: found a ContentSignature directive\n"));
      mSignature = directive->mValue;
    }
  }

  // we have to ensure that we found a signature at this point
  if (mSignature.IsEmpty()) {
    CSVerifier_LOG(("CSVerifier: got a Content-Signature header but didn't find a signature.\n"));
    return NS_ERROR_FAILURE;
  }

  // Bug 769521: We have to change b64 url to regular encoding as long as we
  // don't have a b64 url decoder. This should change soon, but in the meantime
  // we have to live with this.
  mSignature.ReplaceChar('-', '+');
  mSignature.ReplaceChar('_', '/');

  return NS_OK;
}
