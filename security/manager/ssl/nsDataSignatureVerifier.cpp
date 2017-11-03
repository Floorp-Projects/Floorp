/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDataSignatureVerifier.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Base64.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::psm;

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

NS_IMPL_ISUPPORTS(nsDataSignatureVerifier, nsIDataSignatureVerifier)

const SEC_ASN1Template CERT_SignatureDataTemplate[] =
{
    { SEC_ASN1_SEQUENCE,
        0, nullptr, sizeof(CERTSignedData) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(CERTSignedData,signatureAlgorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate), },
    { SEC_ASN1_BIT_STRING,
        offsetof(CERTSignedData,signature), },
    { 0, }
};

nsDataSignatureVerifier::~nsDataSignatureVerifier()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

NS_IMETHODIMP
nsDataSignatureVerifier::VerifyData(const nsACString& aData,
                                    const nsACString& aSignature,
                                    const nsACString& aPublicKey,
                                    bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Allocate an arena to handle the majority of the allocations
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Base 64 decode the key
  // For compatibility reasons we need to remove all whitespace first, since
  // Base64Decode() will not accept invalid characters.
  nsAutoCString b64KeyNoWhitespace(aPublicKey);
  b64KeyNoWhitespace.StripWhitespace();
  nsAutoCString key;
  nsresult rv = Base64Decode(b64KeyNoWhitespace, key);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Extract the public key from the data
  SECItem keyItem = {
    siBuffer,
    BitwiseCast<unsigned char*, const char*>(key.get()),
    key.Length(),
  };
  UniqueCERTSubjectPublicKeyInfo pki(
    SECKEY_DecodeDERSubjectPublicKeyInfo(&keyItem));
  if (!pki) {
    return NS_ERROR_FAILURE;
  }

  UniqueSECKEYPublicKey publicKey(SECKEY_ExtractPublicKey(pki.get()));
  if (!publicKey) {
    return NS_ERROR_FAILURE;
  }

  // Base 64 decode the signature
  // For compatibility reasons we need to remove all whitespace first, since
  // Base64Decode() will not accept invalid characters.
  nsAutoCString b64SignatureNoWhitespace(aSignature);
  b64SignatureNoWhitespace.StripWhitespace();
  nsAutoCString signature;
  rv = Base64Decode(b64SignatureNoWhitespace, signature);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Decode the signature and algorithm
  CERTSignedData sigData;
  PORT_Memset(&sigData, 0, sizeof(CERTSignedData));
  SECItem signatureItem = {
    siBuffer,
    BitwiseCast<unsigned char*, const char*>(signature.get()),
    signature.Length(),
  };
  SECStatus srv = SEC_QuickDERDecodeItem(arena.get(), &sigData,
                                         CERT_SignatureDataTemplate,
                                         &signatureItem);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Perform the final verification
  DER_ConvertBitString(&(sigData.signature));
  srv = VFY_VerifyDataWithAlgorithmID(
    BitwiseCast<const unsigned char*, const char*>(
      PromiseFlatCString(aData).get()),
    aData.Length(), publicKey.get(), &(sigData.signature),
    &(sigData.signatureAlgorithm), nullptr, nullptr);

  *_retval = (srv == SECSuccess);

  return NS_OK;
}
