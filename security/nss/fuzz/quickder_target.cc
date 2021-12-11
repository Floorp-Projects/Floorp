/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "asn1_mutators.h"
#include "shared.h"

const std::vector<const SEC_ASN1Template *> templates = {
    CERT_AttributeTemplate,
    CERT_CertExtensionTemplate,
    CERT_CertificateRequestTemplate,
    CERT_CertificateTemplate,
    CERT_CrlTemplate,
    CERT_IssuerAndSNTemplate,
    CERT_NameTemplate,
    CERT_PublicKeyAndChallengeTemplate,
    CERT_RDNTemplate,
    CERT_SequenceOfCertExtensionTemplate,
    CERT_SetOfAttributeTemplate,
    CERT_SetOfSignedCrlTemplate,
    CERT_SignedCrlTemplate,
    CERT_SignedDataTemplate,
    CERT_SubjectPublicKeyInfoTemplate,
    CERT_TimeChoiceTemplate,
    CERT_ValidityTemplate,
    SEC_AnyTemplate,
    SEC_BitStringTemplate,
    SEC_BMPStringTemplate,
    SEC_BooleanTemplate,
    SEC_CertSequenceTemplate,
    SEC_EnumeratedTemplate,
    SEC_GeneralizedTimeTemplate,
    SEC_IA5StringTemplate,
    SEC_IntegerTemplate,
    SEC_NullTemplate,
    SEC_ObjectIDTemplate,
    SEC_OctetStringTemplate,
    SEC_PointerToAnyTemplate,
    SEC_PointerToEnumeratedTemplate,
    SEC_PointerToGeneralizedTimeTemplate,
    SEC_PointerToOctetStringTemplate,
    SEC_PrintableStringTemplate,
    SEC_SetOfAnyTemplate,
    SEC_SetOfEnumeratedTemplate,
    SEC_SequenceOfAnyTemplate,
    SEC_SequenceOfObjectIDTemplate,
    SEC_SignedCertificateTemplate,
    SEC_SkipTemplate,
    SEC_T61StringTemplate,
    SEC_UniversalStringTemplate,
    SEC_UTCTimeTemplate,
    SEC_UTF8StringTemplate,
    SEC_VisibleStringTemplate,
    SECKEY_DHParamKeyTemplate,
    SECKEY_DHPublicKeyTemplate,
    SECKEY_DSAPrivateKeyExportTemplate,
    SECKEY_DSAPublicKeyTemplate,
    SECKEY_PQGParamsTemplate,
    SECKEY_PrivateKeyInfoTemplate,
    SECKEY_RSAPSSParamsTemplate,
    SECKEY_RSAPublicKeyTemplate,
    SECOID_AlgorithmIDTemplate};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  char *dest[2048];

  for (auto tpl : templates) {
    PORTCheapArenaPool pool;
    SECItem buf = {siBuffer, const_cast<unsigned char *>(Data),
                   static_cast<unsigned int>(Size)};

    PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);
    (void)SEC_QuickDERDecodeItem(&pool.arena, dest, tpl, &buf);
    PORT_DestroyCheapArena(&pool);
  }

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t max_size, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1MutatorFlipConstructed, ASN1MutatorChangeType}), data, size,
      max_size, seed);
}
