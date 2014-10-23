/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <stdlib.h>
#include "cryptox.h"

#if defined(MAR_NSS)

/** 
 * Loads the public key for the specified cert name from the NSS store.
 * 
 * @param certData  The DER-encoded X509 certificate to extract the key from.
 * @param certDataSize The size of certData.
 * @param publicKey Out parameter for the public key to use.
 * @return CryptoX_Success on success, CryptoX_Error on error.
*/
CryptoX_Result
NSS_LoadPublicKey(const unsigned char *certData, unsigned int certDataSize,
                  SECKEYPublicKey **publicKey)
{
  CERTCertificate * cert;
  SECItem certDataItem = { siBuffer, (unsigned char*) certData, certDataSize };

  if (!certData || !publicKey) {
    return CryptoX_Error;
  }

  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &certDataItem, NULL,
                                 PR_FALSE, PR_TRUE);
  /* Get the cert and embedded public key out of the database */
  if (!cert) {
    return CryptoX_Error;
  }
  *publicKey = CERT_ExtractPublicKey(cert);
  CERT_DestroyCertificate(cert);

  if (!*publicKey) {
    return CryptoX_Error;
  }
  return CryptoX_Success;
}

CryptoX_Result
NSS_VerifyBegin(VFYContext **ctx, 
                SECKEYPublicKey * const *publicKey)
{
  SECStatus status;
  if (!ctx || !publicKey || !*publicKey) {
    return CryptoX_Error;
  }

  /* Check that the key length is large enough for our requirements */
  if ((SECKEY_PublicKeyStrength(*publicKey) * 8) < 
      XP_MIN_SIGNATURE_LEN_IN_BYTES) {
    fprintf(stderr, "ERROR: Key length must be >= %d bytes\n", 
            XP_MIN_SIGNATURE_LEN_IN_BYTES);
    return CryptoX_Error;
  }

  *ctx = VFY_CreateContext(*publicKey, NULL, 
                           SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE, NULL);
  if (*ctx == NULL) {
    return CryptoX_Error;
  }

  status = VFY_Begin(*ctx);
  return SECSuccess == status ? CryptoX_Success : CryptoX_Error;
}

/**
 * Verifies if a verify context matches the passed in signature.
 *
 * @param ctx          The verify context that the signature should match.
 * @param signature    The signature to match.
 * @param signatureLen The length of the signature.
 * @return CryptoX_Success on success, CryptoX_Error on error.
*/
CryptoX_Result
NSS_VerifySignature(VFYContext * const *ctx, 
                    const unsigned char *signature, 
                    unsigned int signatureLen)
{
  SECItem signedItem;
  SECStatus status;
  if (!ctx || !signature || !*ctx) {
    return CryptoX_Error;
  }

  signedItem.len = signatureLen;
  signedItem.data = (unsigned char*)signature;
  status = VFY_EndWithSignature(*ctx, &signedItem);
  return SECSuccess == status ? CryptoX_Success : CryptoX_Error;
}

#elif defined(XP_WIN)
/**
 * Verifies if a signature + public key matches a hash context.
 *
 * @param hash      The hash context that the signature should match.
 * @param pubKey    The public key to use on the signature.
 * @param signature The signature to check.
 * @param signatureLen The length of the signature.
 * @return CryptoX_Success on success, CryptoX_Error on error.
*/
CryptoX_Result
CyprtoAPI_VerifySignature(HCRYPTHASH *hash, 
                          HCRYPTKEY *pubKey,
                          const BYTE *signature, 
                          DWORD signatureLen)
{
  DWORD i;
  BOOL result;
/* Windows APIs expect the bytes in the signature to be in little-endian 
 * order, but we write the signature in big-endian order.  Other APIs like 
 * NSS and OpenSSL expect big-endian order.
 */
  BYTE *signatureReversed;
  if (!hash || !pubKey || !signature || signatureLen < 1) {
    return CryptoX_Error;
  }

  signatureReversed = malloc(signatureLen);
  if (!signatureReversed) {
    return CryptoX_Error;
  }

  for (i = 0; i < signatureLen; i++) {
    signatureReversed[i] = signature[signatureLen - 1 - i]; 
  }
  result = CryptVerifySignature(*hash, signatureReversed,
                                signatureLen, *pubKey, NULL, 0);
  free(signatureReversed);
  return result ? CryptoX_Success : CryptoX_Error;
}

/** 
 * Obtains the public key for the passed in cert data
 * 
 * @param provider       The cyrto provider
 * @param certData       Data of the certificate to extract the public key from
 * @param sizeOfCertData The size of the certData buffer
 * @param certStore      Pointer to the handle of the certificate store to use
 * @param CryptoX_Success on success
*/
CryptoX_Result
CryptoAPI_LoadPublicKey(HCRYPTPROV provider, 
                        BYTE *certData,
                        DWORD sizeOfCertData,
                        HCRYPTKEY *publicKey)
{
  CRYPT_DATA_BLOB blob;
  CERT_CONTEXT *context;
  if (!provider || !certData || !publicKey) {
    return CryptoX_Error;
  }

  blob.cbData = sizeOfCertData;
  blob.pbData = certData;
  if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &blob, 
                        CERT_QUERY_CONTENT_FLAG_CERT, 
                        CERT_QUERY_FORMAT_FLAG_BINARY, 
                        0, NULL, NULL, NULL, 
                        NULL, NULL, (const void **)&context)) {
    return CryptoX_Error;
  }

  if (!CryptImportPublicKeyInfo(provider, 
                                PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                &context->pCertInfo->SubjectPublicKeyInfo,
                                publicKey)) {
    CertFreeCertificateContext(context);
    return CryptoX_Error;
  }

  CertFreeCertificateContext(context);
  return CryptoX_Success;
}

/* Try to acquire context in this way:
  * 1. Enhanced provider without creating a new key set
  * 2. Enhanced provider with creating a new key set
  * 3. Default provider without creating a new key set
  * 4. Default provider without creating a new key set
  * #2 and #4 should not be needed because of the CRYPT_VERIFYCONTEXT, 
  * but we add it just in case.
  *
  * @param provider Out parameter containing the provider handle.
  * @return CryptoX_Success on success, CryptoX_Error on error.
 */
CryptoX_Result
CryptoAPI_InitCryptoContext(HCRYPTPROV *provider)
{
  if (!CryptAcquireContext(provider, 
                           NULL, 
                           MS_ENHANCED_PROV, 
                           PROV_RSA_FULL, 
                           CRYPT_VERIFYCONTEXT)) {
    if (!CryptAcquireContext(provider, 
                             NULL, 
                             MS_ENHANCED_PROV, 
                             PROV_RSA_FULL, 
                             CRYPT_NEWKEYSET | CRYPT_VERIFYCONTEXT)) {
      if (!CryptAcquireContext(provider, 
                               NULL, 
                               NULL, 
                               PROV_RSA_FULL, 
                               CRYPT_VERIFYCONTEXT)) {
        if (!CryptAcquireContext(provider, 
                                 NULL, 
                                 NULL, 
                                 PROV_RSA_FULL, 
                                 CRYPT_NEWKEYSET | CRYPT_VERIFYCONTEXT)) {
          *provider = CryptoX_InvalidHandleValue;
          return CryptoX_Error;
        }
      }
    }
  }
  return CryptoX_Success;
}

/** 
  * Begins a signature verification hash context
  *
  * @param provider The crypt provider to use
  * @param hash     Out parameter for a handle to the hash context
  * @return CryptoX_Success on success, CryptoX_Error on error.
*/
CryptoX_Result
CryptoAPI_VerifyBegin(HCRYPTPROV provider, HCRYPTHASH* hash)
{
  BOOL result;
  if (!provider || !hash) {
    return CryptoX_Error;
  }

  *hash = (HCRYPTHASH)NULL;
  result = CryptCreateHash(provider, CALG_SHA1,
                           0, 0, hash);
  return result ? CryptoX_Success : CryptoX_Error;
}

/** 
  * Updates a signature verification hash context
  *
  * @param hash The hash context to udpate
  * @param buf  The buffer to update the hash context with
  * @param len The size of the passed in buffer
  * @return CryptoX_Success on success, CryptoX_Error on error.
*/
CryptoX_Result
CryptoAPI_VerifyUpdate(HCRYPTHASH* hash, BYTE *buf, DWORD len)
{
  BOOL result;
  if (!hash || !buf) {
    return CryptoX_Error;
  }

  result = CryptHashData(*hash, buf, len, 0);
  return result ? CryptoX_Success : CryptoX_Error;
}

#endif



