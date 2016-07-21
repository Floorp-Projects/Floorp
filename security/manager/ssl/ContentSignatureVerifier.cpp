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
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsISupportsPriority.h"
#include "nsIURI.h"
#include "nsNSSComponent.h"
#include "nsSecurityHeaderParser.h"
#include "nsStreamUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsXPCOMStrings.h"
#include "nssb64.h"
#include "pkix/pkix.h"
#include "pkix/pkixtypes.h"
#include "secerr.h"

NS_IMPL_ISUPPORTS(ContentSignatureVerifier,
                  nsIContentSignatureVerifier,
                  nsIInterfaceRequestor,
                  nsIStreamListener)

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

nsresult
ContentSignatureVerifier::CreateContextInternal(const nsACString& aData,
                                                const nsACString& aCertChain,
                                                const nsACString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
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
    // if there was a library error, return an appropriate error
    if (IsFatalError(result)) {
      return NS_ERROR_FAILURE;
    }
    // otherwise, assume the signature was invalid
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

  rv = UpdateInternal(kPREFIX, locker);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // add data if we got any
  return UpdateInternal(aData, locker);
}

nsresult
ContentSignatureVerifier::DownloadCertChain()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mCertChainURL.IsEmpty()) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  nsCOMPtr<nsIURI> certChainURI;
  nsresult rv = NS_NewURI(getter_AddRefs(certChainURI), mCertChainURL);
  if (NS_FAILED(rv) || !certChainURI) {
    return rv;
  }

  // If the address is not https, fail.
  bool isHttps = false;
  rv = certChainURI->SchemeIs("https", &isHttps);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isHttps) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  rv = NS_NewChannel(getter_AddRefs(mChannel), certChainURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // we need this chain soon
  nsCOMPtr<nsISupportsPriority> priorityChannel = do_QueryInterface(mChannel);
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }

  rv = mChannel->AsyncOpen2(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

// Create a context for content signature verification using CreateContext below.
// This function doesn't require a cert chain to be passed, but instead aCSHeader
// must contain an x5u value that is then used to download the cert chain.
NS_IMETHODIMP
ContentSignatureVerifier::CreateContextWithoutCertChain(
  nsIContentSignatureReceiverCallback *aCallback, const nsACString& aCSHeader,
  const nsACString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);
  if (mInitialised) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialised = true;

  // we get the raw content-signature header here, so first parse aCSHeader
  nsresult rv = ParseContentSignatureHeader(aCSHeader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mCallback = aCallback;
  mName.Assign(aName);

  // We must download the cert chain now.
  // This is async and blocks createContextInternal calls.
  return DownloadCertChain();
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
  if (mInitialised) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialised = true;
  // The cert chain is given in aCertChain so we don't have to download anything.
  mHasCertChain = true;

  // we get the raw content-signature header here, so first parse aCSHeader
  nsresult rv = ParseContentSignatureHeader(aCSHeader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return CreateContextInternal(aData, aCertChain, aName);
}

nsresult
ContentSignatureVerifier::UpdateInternal(
  const nsACString& aData, const nsNSSShutDownPreventionLock& /*proofOfLock*/)
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
  MOZ_ASSERT(NS_IsMainThread());
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
  }

  // If we didn't create the context yet, bail!
  if (!mHasCertChain) {
    MOZ_ASSERT_UNREACHABLE(
      "Someone called ContentSignatureVerifier::Update before "
      "downloading the cert chain.");
    return NS_ERROR_FAILURE;
  }

  return UpdateInternal(aData, locker);
}

/**
 * Finish signature verification and return the result in _retval.
 */
NS_IMETHODIMP
ContentSignatureVerifier::End(bool* _retval)
{
  NS_ENSURE_ARG(_retval);
  MOZ_ASSERT(NS_IsMainThread());
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    CSVerifier_LOG(("CSVerifier: nss is already shutdown\n"));
    return NS_ERROR_FAILURE;
  }

  // If we didn't create the context yet, bail!
  if (!mHasCertChain) {
    MOZ_ASSERT_UNREACHABLE(
      "Someone called ContentSignatureVerifier::End before "
      "downloading the cert chain.");
    return NS_ERROR_FAILURE;
  }

  *_retval = (VFY_End(mCx.get()) == SECSuccess);

  return NS_OK;
}

nsresult
ContentSignatureVerifier::ParseContentSignatureHeader(
  const nsACString& aContentSignatureHeader)
{
  MOZ_ASSERT(NS_IsMainThread());
  // We only support p384 ecdsa according to spec
  NS_NAMED_LITERAL_CSTRING(signature_var, "p384ecdsa");
  NS_NAMED_LITERAL_CSTRING(certChainURL_var, "x5u");

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
    if (directive->mName.Length() == certChainURL_var.Length() &&
        directive->mName.EqualsIgnoreCase(certChainURL_var.get(),
                                          certChainURL_var.Length())) {
      if (!mCertChainURL.IsEmpty()) {
        CSVerifier_LOG(("CSVerifier: found two x5u values\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSVerifier_LOG(("CSVerifier: found an x5u directive\n"));
      mCertChainURL = directive->mValue;
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

/* nsIStreamListener implementation */

NS_IMETHODIMP
ContentSignatureVerifier::OnStartRequest(nsIRequest* aRequest,
                                         nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::OnStopRequest(nsIRequest* aRequest,
                                        nsISupports* aContext, nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIContentSignatureReceiverCallback> callback;
  callback.swap(mCallback);
  nsresult rv;

  // Check HTTP status code and return if it's not 200.
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest, &rv);
  uint32_t httpResponseCode;
  if (NS_FAILED(rv) || NS_FAILED(http->GetResponseStatus(&httpResponseCode)) ||
      httpResponseCode != 200) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  if (NS_FAILED(aStatus)) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  nsAutoCString certChain;
  for (uint32_t i = 0; i < mCertChain.Length(); ++i) {
    certChain.Append(mCertChain[i]);
  }

  // We got the cert chain now. Let's create the context.
  rv = CreateContextInternal(NS_LITERAL_CSTRING(""), certChain, mName);
  if (NS_FAILED(rv)) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  mHasCertChain = true;
  callback->ContextCreated(true);
  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::OnDataAvailable(nsIRequest* aRequest,
                                          nsISupports* aContext,
                                          nsIInputStream* aInputStream,
                                          uint64_t aOffset, uint32_t aCount)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString buffer;

  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mCertChain.AppendElement(buffer, fallible)) {
    mCertChain.TruncateLength(0);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::GetInterface(const nsIID& uuid, void** result)
{
  return QueryInterface(uuid, result);
}
