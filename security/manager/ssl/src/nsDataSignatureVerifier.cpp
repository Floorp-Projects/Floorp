/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDataSignatureVerifier.h"

#include "cms.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "nsCertificatePrincipal.h"
#include "nsCOMPtr.h"
#include "nsNSSComponent.h"
#include "nssb64.h"
#include "pkix/pkixtypes.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"
#include "SharedCertVerifier.h"

using namespace mozilla;
using namespace mozilla::pkix;
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

NS_IMETHODIMP
nsDataSignatureVerifier::VerifyData(const nsACString & aData,
                                    const nsACString & aSignature,
                                    const nsACString & aPublicKey,
                                    bool *_retval)
{
    // Allocate an arena to handle the majority of the allocations
    PLArenaPool *arena;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return NS_ERROR_OUT_OF_MEMORY;

    // Base 64 decode the key
    SECItem keyItem;
    PORT_Memset(&keyItem, 0, sizeof(SECItem));
    if (!NSSBase64_DecodeBuffer(arena, &keyItem,
                                nsPromiseFlatCString(aPublicKey).get(),
                                aPublicKey.Length())) {
        PORT_FreeArena(arena, false);
        return NS_ERROR_FAILURE;
    }
    
    // Extract the public key from the data
    CERTSubjectPublicKeyInfo *pki = SECKEY_DecodeDERSubjectPublicKeyInfo(&keyItem);
    if (!pki) {
        PORT_FreeArena(arena, false);
        return NS_ERROR_FAILURE;
    }
    SECKEYPublicKey *publicKey = SECKEY_ExtractPublicKey(pki);
    SECKEY_DestroySubjectPublicKeyInfo(pki);
    pki = nullptr;
    
    if (!publicKey) {
        PORT_FreeArena(arena, false);
        return NS_ERROR_FAILURE;
    }
    
    // Base 64 decode the signature
    SECItem signatureItem;
    PORT_Memset(&signatureItem, 0, sizeof(SECItem));
    if (!NSSBase64_DecodeBuffer(arena, &signatureItem,
                                nsPromiseFlatCString(aSignature).get(),
                                aSignature.Length())) {
        SECKEY_DestroyPublicKey(publicKey);
        PORT_FreeArena(arena, false);
        return NS_ERROR_FAILURE;
    }
    
    // Decode the signature and algorithm
    CERTSignedData sigData;
    PORT_Memset(&sigData, 0, sizeof(CERTSignedData));
    SECStatus ss = SEC_QuickDERDecodeItem(arena, &sigData, 
                                          CERT_SignatureDataTemplate,
                                          &signatureItem);
    if (ss != SECSuccess) {
        SECKEY_DestroyPublicKey(publicKey);
        PORT_FreeArena(arena, false);
        return NS_ERROR_FAILURE;
    }
    
    // Perform the final verification
    DER_ConvertBitString(&(sigData.signature));
    ss = VFY_VerifyDataWithAlgorithmID((const unsigned char*)nsPromiseFlatCString(aData).get(),
                                       aData.Length(), publicKey,
                                       &(sigData.signature),
                                       &(sigData.signatureAlgorithm),
                                       nullptr, nullptr);
    
    // Clean up remaining objects
    SECKEY_DestroyPublicKey(publicKey);
    PORT_FreeArena(arena, false);
    
    *_retval = (ss == SECSuccess);

    return NS_OK;
}

namespace mozilla {

nsresult
VerifyCMSDetachedSignatureIncludingCertificate(
  const SECItem& buffer, const SECItem& detachedDigest,
  nsresult (*verifyCertificate)(CERTCertificate* cert, void* context,
                                void* pinArg),
  void *verifyCertificateContext, void* pinArg)
{
  // XXX: missing pinArg is tolerated.
  if (NS_WARN_IF(!buffer.data && buffer.len > 0) ||
      NS_WARN_IF(!detachedDigest.data && detachedDigest.len > 0) ||
      (!verifyCertificate) ||
      NS_WARN_IF(!verifyCertificateContext)) {
    return NS_ERROR_INVALID_ARG;
  }

  ScopedPtr<NSSCMSMessage, NSS_CMSMessage_Destroy>
    cmsMsg(NSS_CMSMessage_CreateFromDER(const_cast<SECItem*>(&buffer), nullptr,
                                        nullptr, nullptr, nullptr, nullptr,
                                        nullptr));
  if (!cmsMsg) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }

  if (!NSS_CMSMessage_IsSigned(cmsMsg.get())) {
    return NS_ERROR_CMS_VERIFY_NOT_SIGNED;
  }

  NSSCMSContentInfo* cinfo = NSS_CMSMessage_ContentLevel(cmsMsg.get(), 0);
  if (!cinfo) {
    return NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
  }

  // signedData is non-owning
  NSSCMSSignedData* signedData =
    reinterpret_cast<NSSCMSSignedData*>(NSS_CMSContentInfo_GetContent(cinfo));
  if (!signedData) {
    return NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
  }

  // Set digest value.
  if (NSS_CMSSignedData_SetDigestValue(signedData, SEC_OID_SHA1,
                                       const_cast<SECItem*>(&detachedDigest))) {
    return NS_ERROR_CMS_VERIFY_BAD_DIGEST;
  }

  // Parse the certificates into CERTCertificate objects held in memory so
  // verifyCertificate will be able to find them during path building.
  mozilla::pkix::ScopedCERTCertList certs(CERT_NewCertList());
  if (!certs) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (signedData->rawCerts) {
    for (size_t i = 0; signedData->rawCerts[i]; ++i) {
      mozilla::pkix::ScopedCERTCertificate
        cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                     signedData->rawCerts[i], nullptr, false,
                                     true));
      // Skip certificates that fail to parse
      if (cert) {
        if (CERT_AddCertToListTail(certs.get(), cert.get()) == SECSuccess) {
          cert.release(); // ownership transfered
        } else {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  // Get the end-entity certificate.
  int numSigners = NSS_CMSSignedData_SignerInfoCount(signedData);
  if (NS_WARN_IF(numSigners != 1)) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }
  // signer is non-owning.
  NSSCMSSignerInfo* signer = NSS_CMSSignedData_GetSignerInfo(signedData, 0);
  if (NS_WARN_IF(!signer)) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }
  CERTCertificate* signerCert =
    NSS_CMSSignerInfo_GetSigningCertificate(signer, CERT_GetDefaultCertDB());
  if (!signerCert) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }

  nsresult rv = verifyCertificate(signerCert, verifyCertificateContext, pinArg);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // See NSS_CMSContentInfo_GetContentTypeOID, which isn't exported from NSS.
  SECOidData* contentTypeOidData =
    SECOID_FindOID(&signedData->contentInfo.contentType);
  if (!contentTypeOidData) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }

  return MapSECStatus(NSS_CMSSignerInfo_Verify(signer,
                         const_cast<SECItem*>(&detachedDigest),
                         &contentTypeOidData->oid));
}

} // namespace mozilla

namespace {

struct VerifyCertificateContext
{
  nsCOMPtr<nsICertificatePrincipal> principal;
  mozilla::pkix::ScopedCERTCertList builtChain;
};

static nsresult
VerifyCertificate(CERTCertificate* cert, void* voidContext, void* pinArg)
{
  // XXX: missing pinArg is tolerated
  if (NS_WARN_IF(!cert) || NS_WARN_IF(!voidContext)) {
    return NS_ERROR_INVALID_ARG;
  }

  VerifyCertificateContext* context =
    reinterpret_cast<VerifyCertificateContext*>(voidContext);

  nsCOMPtr<nsIX509Cert> xpcomCert(nsNSSCertificate::Create(cert));
  if (!xpcomCert) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoString fingerprint;
  nsresult rv = xpcomCert->GetSha1Fingerprint(fingerprint);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString orgName;
  rv = xpcomCert->GetOrganization(orgName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString subjectName;
  rv = xpcomCert->GetSubjectName(subjectName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  context->principal =
    new nsCertificatePrincipal(NS_ConvertUTF16toUTF8(fingerprint),
                               NS_ConvertUTF16toUTF8(subjectName),
                               NS_ConvertUTF16toUTF8(orgName),
                               xpcomCert);

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);

  return MapSECStatus(certVerifier->VerifyCert(cert,
                                               certificateUsageObjectSigner,
                                               PR_Now(), pinArg,
                                               nullptr, // hostname
                                               0, // flags
                                               nullptr, // stapledOCSPResponse
                                               &context->builtChain));
}

} // unnamed namespcae

NS_IMETHODIMP
nsDataSignatureVerifier::VerifySignature(const char* aRSABuf,
                                         uint32_t aRSABufLen,
                                         const char* aPlaintext,
                                         uint32_t aPlaintextLen,
                                         int32_t* aErrorCode,
                                         nsICertificatePrincipal** aPrincipal)
{
  if (!aPlaintext || !aPrincipal || !aErrorCode) {
    return NS_ERROR_INVALID_ARG;
  }

  *aErrorCode = VERIFY_ERROR_OTHER;
  *aPrincipal = nullptr;

  nsNSSShutDownPreventionLock locker;

  Digest digest;
  nsresult rv = digest.DigestBuf(SEC_OID_SHA1,
                                 reinterpret_cast<const uint8_t*>(aPlaintext),
                                 aPlaintextLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SECItem buffer = {
    siBuffer,
    reinterpret_cast<uint8_t*>(const_cast<char*>(aRSABuf)),
    aRSABufLen
  };

  VerifyCertificateContext context;
  // XXX: pinArg is missing
  rv = VerifyCMSDetachedSignatureIncludingCertificate(buffer, digest.get(),
                                                      VerifyCertificate,
                                                      &context, nullptr);
  if (NS_SUCCEEDED(rv)) {
    *aErrorCode = VERIFY_OK;
  } else if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_SECURITY) {
    if (rv == GetXPCOMFromNSSError(SEC_ERROR_UNKNOWN_ISSUER)) {
      *aErrorCode = VERIFY_ERROR_UNKNOWN_ISSUER;
    } else {
      *aErrorCode = VERIFY_ERROR_OTHER;
    }
    rv = NS_OK;
  }
  if (rv == NS_OK) {
    context.principal.forget(aPrincipal);
  }

  return rv;
}
