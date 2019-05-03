/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentSignatureVerifier.h"

#include "BRNameMatchingPolicy.h"
#include "CryptoTask.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "mozilla/Base64.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsPromiseFlatString.h"
#include "nsSecurityHeaderParser.h"
#include "nsWhitespaceTokenizer.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixtypes.h"
#include "secerr.h"

NS_IMPL_ISUPPORTS(ContentSignatureVerifier, nsIContentSignatureVerifier)

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::psm;
using dom::Promise;

static LazyLogModule gCSVerifierPRLog("ContentSignatureVerifier");
#define CSVerifier_LOG(args) MOZ_LOG(gCSVerifierPRLog, LogLevel::Debug, args)

// Content-Signature prefix
const unsigned char kPREFIX[] = {'C', 'o', 'n', 't', 'e', 'n', 't',
                                 '-', 'S', 'i', 'g', 'n', 'a', 't',
                                 'u', 'r', 'e', ':', 0};

class VerifyContentSignatureTask : public CryptoTask {
 public:
  VerifyContentSignatureTask(const nsACString& aData,
                             const nsACString& aCSHeader,
                             const nsACString& aCertChain,
                             const nsACString& aHostname,
                             RefPtr<Promise>& aPromise)
      : mData(aData),
        mCSHeader(aCSHeader),
        mCertChain(aCertChain),
        mHostname(aHostname),
        mSignatureVerified(false),
        mPromise(aPromise) {}

 private:
  virtual nsresult CalculateResult() override;
  virtual void CallCallback(nsresult rv) override;

  nsCString mData;
  nsCString mCSHeader;
  nsCString mCertChain;
  nsCString mHostname;
  bool mSignatureVerified;
  RefPtr<Promise> mPromise;
};

NS_IMETHODIMP
ContentSignatureVerifier::AsyncVerifyContentSignature(
    const nsACString& aData, const nsACString& aCSHeader,
    const nsACString& aCertChain, const nsACString& aHostname, JSContext* aCx,
    Promise** aPromise) {
  NS_ENSURE_ARG_POINTER(aCx);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  RefPtr<VerifyContentSignatureTask> task(new VerifyContentSignatureTask(
      aData, aCSHeader, aCertChain, aHostname, promise));
  nsresult rv = task->Dispatch("ContentSig");
  if (NS_FAILED(rv)) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

static nsresult VerifyContentSignatureInternal(
    const nsACString& aData, const nsACString& aCSHeader,
    const nsACString& aCertChain, const nsACString& aHostname,
    /* out */
    mozilla::Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS&
        aErrorLabel,
    /* out */ nsACString& aCertFingerprint, /* out */ uint32_t& aErrorValue);
static nsresult ParseContentSignatureHeader(
    const nsACString& aContentSignatureHeader,
    /* out */ nsCString& aSignature);

nsresult VerifyContentSignatureTask::CalculateResult() {
  // 3 is the default, non-specific, "something failed" error.
  Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS errorLabel =
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err3;
  nsAutoCString certFingerprint;
  uint32_t errorValue = 3;
  nsresult rv =
      VerifyContentSignatureInternal(mData, mCSHeader, mCertChain, mHostname,
                                     errorLabel, certFingerprint, errorValue);
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: Signature verification failed"));
    if (certFingerprint.Length() > 0) {
      Telemetry::AccumulateCategoricalKeyed(certFingerprint, errorLabel);
    }
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, errorValue);
    if (rv == NS_ERROR_INVALID_SIGNATURE) {
      return NS_OK;
    }
    return rv;
  }

  mSignatureVerified = true;
  Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 0);

  return NS_OK;
}

void VerifyContentSignatureTask::CallCallback(nsresult rv) {
  if (NS_FAILED(rv)) {
    mPromise->MaybeReject(rv);
  } else {
    mPromise->MaybeResolve(mSignatureVerified);
  }
}

bool IsNewLine(char16_t c) { return c == '\n' || c == '\r'; }

nsresult ReadChainIntoCertList(const nsACString& aCertChain,
                               CERTCertList* aCertList) {
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
        nsAutoCString derString;
        nsresult rv = Base64Decode(blockData, derString);
        if (NS_FAILED(rv)) {
          CSVerifier_LOG(("CSVerifier: decoding the signature failed"));
          return rv;
        }
        SECItem der = {
            siBuffer,
            BitwiseCast<unsigned char*, const char*>(derString.get()),
            derString.Length(),
        };
        UniqueCERTCertificate tmpCert(CERT_NewTempCertificate(
            CERT_GetDefaultCertDB(), &der, nullptr, false, true));
        if (!tmpCert) {
          return NS_ERROR_FAILURE;
        }
        // if adding tmpCert succeeds, tmpCert will now be owned by aCertList
        SECStatus res = CERT_AddCertToListTail(aCertList, tmpCert.get());
        if (res != SECSuccess) {
          return MapSECStatus(res);
        }
        Unused << tmpCert.release();
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
    CSVerifier_LOG(("CSVerifier: supplied chain contains bad data"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// Given data to verify, a content signature header value, a string representing
// a list of PEM-encoded certificates, and a hostname to validate the
// certificates against, this function attempts to validate the certificate
// chain, extract the signature from the header, and verify the data using the
// key in the end-entity certificate from the chain. Returns NS_OK if everything
// is satisfactory and a failing nsresult otherwise. The output parameters are
// filled with telemetry data to report in the case of failures.
static nsresult VerifyContentSignatureInternal(
    const nsACString& aData, const nsACString& aCSHeader,
    const nsACString& aCertChain, const nsACString& aHostname,
    /* out */
    Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS& aErrorLabel,
    /* out */ nsACString& aCertFingerprint,
    /* out */ uint32_t& aErrorValue) {
  UniqueCERTCertList certCertList(CERT_NewCertList());
  if (!certCertList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = ReadChainIntoCertList(aCertChain, certCertList.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  CERTCertListNode* node = CERT_LIST_HEAD(certCertList.get());
  if (!node || CERT_LIST_END(node, certCertList.get()) || !node->cert) {
    return NS_ERROR_FAILURE;
  }

  SECItem* certSecItem = &node->cert->derCert;

  Input certDER;
  mozilla::pkix::Result result =
      certDER.Init(BitwiseCast<uint8_t*, unsigned char*>(certSecItem->data),
                   certSecItem->len);
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }

  // Get EE certificate fingerprint for telemetry.
  unsigned char fingerprint[SHA256_LENGTH] = {0};
  SECStatus srv = PK11_HashBuf(SEC_OID_SHA256, fingerprint, certSecItem->data,
                               AssertedCast<int32_t>(certSecItem->len));
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  SECItem fingerprintItem = {siBuffer, fingerprint, SHA256_LENGTH};
  UniquePORTString tmpFingerprintString(CERT_Hexify(&fingerprintItem, 0));
  if (!tmpFingerprintString) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aCertFingerprint.Assign(tmpFingerprintString.get());

  // Check the signerCert chain is good
  CSTrustDomain trustDomain(certCertList);
  result = BuildCertChain(
      trustDomain, certDER, Now(), EndEntityOrCA::MustBeEndEntity,
      KeyUsage::noParticularKeyUsageRequired, KeyPurposeId::id_kp_codeSigning,
      CertPolicyId::anyPolicy, nullptr /*stapledOCSPResponse*/);
  if (result != Success) {
    // if there was a library error, return an appropriate error
    if (IsFatalError(result)) {
      return NS_ERROR_FAILURE;
    }
    // otherwise, assume the signature was invalid
    if (result == mozilla::pkix::Result::ERROR_EXPIRED_CERTIFICATE) {
      aErrorLabel =
          Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err4;
      aErrorValue = 4;
    } else if (result ==
               mozilla::pkix::Result::ERROR_NOT_YET_VALID_CERTIFICATE) {
      aErrorLabel =
          Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err5;
      aErrorValue = 5;
    } else {
      // Building cert chain failed for some other reason.
      aErrorLabel =
          Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err6;
      aErrorValue = 6;
    }
    CSVerifier_LOG(("CSVerifier: The supplied chain is bad (%s)",
                    MapResultToName(result)));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Check the SAN
  Input hostnameInput;

  result = hostnameInput.Init(
      BitwiseCast<const uint8_t*, const char*>(aHostname.BeginReading()),
      aHostname.Length());
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }

  BRNameMatchingPolicy nameMatchingPolicy(BRNameMatchingPolicy::Mode::Enforce);
  result = CheckCertHostname(certDER, hostnameInput, nameMatchingPolicy);
  if (result != Success) {
    // EE cert isnot valid for the given host name.
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err7;
    aErrorValue = 7;
    return NS_ERROR_INVALID_SIGNATURE;
  }

  mozilla::UniqueSECKEYPublicKey key(CERT_ExtractPublicKey(node->cert));
  // in case we were not able to extract a key
  if (!key) {
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err8;
    aErrorValue = 8;
    CSVerifier_LOG(("CSVerifier: unable to extract a key"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  nsAutoCString signature;
  rv = ParseContentSignatureHeader(aCSHeader, signature);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Base 64 decode the signature
  nsAutoCString rawSignature;
  rv = Base64Decode(signature, rawSignature);
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: decoding the signature failed"));
    return rv;
  }

  // get signature object
  ScopedAutoSECItem signatureItem;
  SECItem rawSignatureItem = {
      siBuffer,
      BitwiseCast<unsigned char*, const char*>(rawSignature.get()),
      rawSignature.Length(),
  };
  // We have a raw ecdsa signature r||s so we have to DER-encode it first
  // Note that we have to check rawSignatureItem->len % 2 here as
  // DSAU_EncodeDerSigWithLen asserts this
  if (rawSignatureItem.len == 0 || rawSignatureItem.len % 2 != 0) {
    CSVerifier_LOG(("CSVerifier: signature length is bad"));
    return NS_ERROR_FAILURE;
  }
  if (DSAU_EncodeDerSigWithLen(&signatureItem, &rawSignatureItem,
                               rawSignatureItem.len) != SECSuccess) {
    CSVerifier_LOG(("CSVerifier: encoding the signature failed"));
    return NS_ERROR_FAILURE;
  }

  // this is the only OID we support for now
  SECOidTag oid = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;
  mozilla::UniqueVFYContext cx(
      VFY_CreateContext(key.get(), &signatureItem, oid, nullptr));
  if (!cx) {
    // Creating context failed.
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err9;
    aErrorValue = 9;
    return NS_ERROR_INVALID_SIGNATURE;
  }

  if (VFY_Begin(cx.get()) != SECSuccess) {
    // Creating context failed.
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err9;
    aErrorValue = 9;
    return NS_ERROR_INVALID_SIGNATURE;
  }
  if (VFY_Update(cx.get(), kPREFIX, sizeof(kPREFIX)) != SECSuccess) {
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err1;
    aErrorValue = 1;
    return NS_ERROR_INVALID_SIGNATURE;
  }
  if (VFY_Update(cx.get(),
                 reinterpret_cast<const unsigned char*>(aData.BeginReading()),
                 aData.Length()) != SECSuccess) {
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err1;
    aErrorValue = 1;
    return NS_ERROR_INVALID_SIGNATURE;
  }
  if (VFY_End(cx.get()) != SECSuccess) {
    aErrorLabel = Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err1;
    aErrorValue = 1;
    return NS_ERROR_INVALID_SIGNATURE;
  }

  return NS_OK;
}

static nsresult ParseContentSignatureHeader(
    const nsACString& aContentSignatureHeader,
    /* out */ nsCString& aSignature) {
  // We only support p384 ecdsa.
  NS_NAMED_LITERAL_CSTRING(signature_var, "p384ecdsa");

  aSignature.Truncate();

  const nsCString& flatHeader = PromiseFlatCString(aContentSignatureHeader);
  nsSecurityHeaderParser parser(flatHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: could not parse ContentSignature header"));
    return NS_ERROR_FAILURE;
  }
  LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    CSVerifier_LOG(
        ("CSVerifier: found directive '%s'", directive->mName.get()));
    if (directive->mName.Length() == signature_var.Length() &&
        directive->mName.EqualsIgnoreCase(signature_var.get(),
                                          signature_var.Length())) {
      if (!aSignature.IsEmpty()) {
        CSVerifier_LOG(("CSVerifier: found two ContentSignatures"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSVerifier_LOG(("CSVerifier: found a ContentSignature directive"));
      aSignature.Assign(directive->mValue);
    }
  }

  // we have to ensure that we found a signature at this point
  if (aSignature.IsEmpty()) {
    CSVerifier_LOG(
        ("CSVerifier: got a Content-Signature header but didn't find a "
         "signature."));
    return NS_ERROR_FAILURE;
  }

  // Bug 769521: We have to change b64 url to regular encoding as long as we
  // don't have a b64 url decoder. This should change soon, but in the meantime
  // we have to live with this.
  aSignature.ReplaceChar('-', '+');
  aSignature.ReplaceChar('_', '/');

  return NS_OK;
}
