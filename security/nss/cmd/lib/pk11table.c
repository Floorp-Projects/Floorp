/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11table.h"

const char *_valueString[] = {
    "None",
    "Variable",
    "CK_ULONG",
    "Data",
    "UTF8",
    "CK_INFO",
    "CK_SLOT_INFO",
    "CK_TOKEN_INFO",
    "CK_SESSION_INFO",
    "CK_ATTRIBUTE",
    "CK_MECHANISM",
    "CK_MECHANISM_INFO",
    "CK_C_INITIALIZE_ARGS",
    "CK_FUNCTION_LIST"
};

const char **valueString = &_valueString[0];
const int valueCount = sizeof(_valueString) / sizeof(_valueString[0]);

const char *_constTypeString[] = {
    "None",
    "Bool",
    "InfoFlags",
    "SlotFlags",
    "TokenFlags",
    "SessionFlags",
    "MechanismFlags",
    "InitializeFlags",
    "Users",
    "SessionState",
    "Object",
    "Hardware",
    "KeyType",
    "CertificateType",
    "Attribute",
    "Mechanism",
    "Result",
    "Trust",
    "AvailableSizes",
    "CurrentSize"
};

const char **constTypeString = &_constTypeString[0];
const int constTypeCount = sizeof(_constTypeString) / sizeof(_constTypeString[0]);

#define mkEntry(x, t)              \
    {                              \
        #x, x, Const##t, ConstNone \
    }
#define mkEntry2(x, t, t2)         \
    {                              \
        #x, x, Const##t, Const##t2 \
    }

const Constant _consts[] = {
    mkEntry(CK_FALSE, Bool),
    mkEntry(CK_TRUE, Bool),

    mkEntry(CKF_TOKEN_PRESENT, SlotFlags),
    mkEntry(CKF_REMOVABLE_DEVICE, SlotFlags),
    mkEntry(CKF_HW_SLOT, SlotFlags),

    mkEntry(CKF_RNG, TokenFlags),
    mkEntry(CKF_WRITE_PROTECTED, TokenFlags),
    mkEntry(CKF_LOGIN_REQUIRED, TokenFlags),
    mkEntry(CKF_USER_PIN_INITIALIZED, TokenFlags),
    mkEntry(CKF_RESTORE_KEY_NOT_NEEDED, TokenFlags),
    mkEntry(CKF_CLOCK_ON_TOKEN, TokenFlags),
    mkEntry(CKF_PROTECTED_AUTHENTICATION_PATH, TokenFlags),
    mkEntry(CKF_DUAL_CRYPTO_OPERATIONS, TokenFlags),
    mkEntry(CKF_TOKEN_INITIALIZED, TokenFlags),
    mkEntry(CKF_SECONDARY_AUTHENTICATION, TokenFlags),
    mkEntry(CKF_USER_PIN_COUNT_LOW, TokenFlags),
    mkEntry(CKF_USER_PIN_FINAL_TRY, TokenFlags),
    mkEntry(CKF_USER_PIN_LOCKED, TokenFlags),
    mkEntry(CKF_USER_PIN_TO_BE_CHANGED, TokenFlags),
    mkEntry(CKF_SO_PIN_COUNT_LOW, TokenFlags),
    mkEntry(CKF_SO_PIN_FINAL_TRY, TokenFlags),
    mkEntry(CKF_SO_PIN_LOCKED, TokenFlags),
    mkEntry(CKF_SO_PIN_TO_BE_CHANGED, TokenFlags),

    mkEntry(CKF_RW_SESSION, SessionFlags),
    mkEntry(CKF_SERIAL_SESSION, SessionFlags),

    mkEntry(CKF_HW, MechanismFlags),
    mkEntry(CKF_ENCRYPT, MechanismFlags),
    mkEntry(CKF_DECRYPT, MechanismFlags),
    mkEntry(CKF_DIGEST, MechanismFlags),
    mkEntry(CKF_SIGN, MechanismFlags),
    mkEntry(CKF_SIGN_RECOVER, MechanismFlags),
    mkEntry(CKF_VERIFY, MechanismFlags),
    mkEntry(CKF_VERIFY_RECOVER, MechanismFlags),
    mkEntry(CKF_GENERATE, MechanismFlags),
    mkEntry(CKF_GENERATE_KEY_PAIR, MechanismFlags),
    mkEntry(CKF_WRAP, MechanismFlags),
    mkEntry(CKF_UNWRAP, MechanismFlags),
    mkEntry(CKF_DERIVE, MechanismFlags),
    mkEntry(CKF_EC_FP, MechanismFlags),
    mkEntry(CKF_EC_F_2M, MechanismFlags),
    mkEntry(CKF_EC_ECPARAMETERS, MechanismFlags),
    mkEntry(CKF_EC_NAMEDCURVE, MechanismFlags),
    mkEntry(CKF_EC_UNCOMPRESS, MechanismFlags),
    mkEntry(CKF_EC_COMPRESS, MechanismFlags),

    mkEntry(CKF_LIBRARY_CANT_CREATE_OS_THREADS, InitializeFlags),
    mkEntry(CKF_OS_LOCKING_OK, InitializeFlags),

    mkEntry(CKU_SO, Users),
    mkEntry(CKU_USER, Users),

    mkEntry(CKS_RO_PUBLIC_SESSION, SessionState),
    mkEntry(CKS_RO_USER_FUNCTIONS, SessionState),
    mkEntry(CKS_RW_PUBLIC_SESSION, SessionState),
    mkEntry(CKS_RW_USER_FUNCTIONS, SessionState),
    mkEntry(CKS_RW_SO_FUNCTIONS, SessionState),

    mkEntry(CKO_DATA, Object),
    mkEntry(CKO_CERTIFICATE, Object),
    mkEntry(CKO_PUBLIC_KEY, Object),
    mkEntry(CKO_PRIVATE_KEY, Object),
    mkEntry(CKO_SECRET_KEY, Object),
    mkEntry(CKO_HW_FEATURE, Object),
    mkEntry(CKO_DOMAIN_PARAMETERS, Object),
    mkEntry(CKO_KG_PARAMETERS, Object),
    mkEntry(CKO_NSS_CRL, Object),
    mkEntry(CKO_NSS_SMIME, Object),
    mkEntry(CKO_NSS_TRUST, Object),
    mkEntry(CKO_NSS_BUILTIN_ROOT_LIST, Object),

    mkEntry(CKH_MONOTONIC_COUNTER, Hardware),
    mkEntry(CKH_CLOCK, Hardware),

    mkEntry(CKK_RSA, KeyType),
    mkEntry(CKK_DSA, KeyType),
    mkEntry(CKK_DH, KeyType),
    mkEntry(CKK_ECDSA, KeyType),
    mkEntry(CKK_EC, KeyType),
    mkEntry(CKK_X9_42_DH, KeyType),
    mkEntry(CKK_KEA, KeyType),
    mkEntry(CKK_GENERIC_SECRET, KeyType),
    mkEntry(CKK_RC2, KeyType),
    mkEntry(CKK_RC4, KeyType),
    mkEntry(CKK_DES, KeyType),
    mkEntry(CKK_DES2, KeyType),
    mkEntry(CKK_DES3, KeyType),
    mkEntry(CKK_CAST, KeyType),
    mkEntry(CKK_CAST3, KeyType),
    mkEntry(CKK_CAST5, KeyType),
    mkEntry(CKK_CAST128, KeyType),
    mkEntry(CKK_RC5, KeyType),
    mkEntry(CKK_IDEA, KeyType),
    mkEntry(CKK_SKIPJACK, KeyType),
    mkEntry(CKK_BATON, KeyType),
    mkEntry(CKK_JUNIPER, KeyType),
    mkEntry(CKK_CDMF, KeyType),
    mkEntry(CKK_AES, KeyType),
    mkEntry(CKK_CAMELLIA, KeyType),
    mkEntry(CKK_NSS_PKCS8, KeyType),

    mkEntry(CKC_X_509, CertType),
    mkEntry(CKC_X_509_ATTR_CERT, CertType),

    mkEntry2(CKA_CLASS, Attribute, Object),
    mkEntry2(CKA_TOKEN, Attribute, Bool),
    mkEntry2(CKA_PRIVATE, Attribute, Bool),
    mkEntry2(CKA_LABEL, Attribute, None),
    mkEntry2(CKA_APPLICATION, Attribute, None),
    mkEntry2(CKA_VALUE, Attribute, None),
    mkEntry2(CKA_OBJECT_ID, Attribute, None),
    mkEntry2(CKA_CERTIFICATE_TYPE, Attribute, CertType),
    mkEntry2(CKA_ISSUER, Attribute, None),
    mkEntry2(CKA_SERIAL_NUMBER, Attribute, None),
    mkEntry2(CKA_AC_ISSUER, Attribute, None),
    mkEntry2(CKA_OWNER, Attribute, None),
    mkEntry2(CKA_ATTR_TYPES, Attribute, None),
    mkEntry2(CKA_TRUSTED, Attribute, Bool),
    mkEntry2(CKA_KEY_TYPE, Attribute, KeyType),
    mkEntry2(CKA_SUBJECT, Attribute, None),
    mkEntry2(CKA_ID, Attribute, None),
    mkEntry2(CKA_SENSITIVE, Attribute, Bool),
    mkEntry2(CKA_ENCRYPT, Attribute, Bool),
    mkEntry2(CKA_DECRYPT, Attribute, Bool),
    mkEntry2(CKA_WRAP, Attribute, Bool),
    mkEntry2(CKA_UNWRAP, Attribute, Bool),
    mkEntry2(CKA_SIGN, Attribute, Bool),
    mkEntry2(CKA_SIGN_RECOVER, Attribute, Bool),
    mkEntry2(CKA_VERIFY, Attribute, Bool),
    mkEntry2(CKA_VERIFY_RECOVER, Attribute, Bool),
    mkEntry2(CKA_DERIVE, Attribute, Bool),
    mkEntry2(CKA_START_DATE, Attribute, None),
    mkEntry2(CKA_END_DATE, Attribute, None),
    mkEntry2(CKA_MODULUS, Attribute, None),
    mkEntry2(CKA_MODULUS_BITS, Attribute, None),
    mkEntry2(CKA_PUBLIC_EXPONENT, Attribute, None),
    mkEntry2(CKA_PRIVATE_EXPONENT, Attribute, None),
    mkEntry2(CKA_PRIME_1, Attribute, None),
    mkEntry2(CKA_PRIME_2, Attribute, None),
    mkEntry2(CKA_EXPONENT_1, Attribute, None),
    mkEntry2(CKA_EXPONENT_2, Attribute, None),
    mkEntry2(CKA_COEFFICIENT, Attribute, None),
    mkEntry2(CKA_PRIME, Attribute, None),
    mkEntry2(CKA_SUBPRIME, Attribute, None),
    mkEntry2(CKA_BASE, Attribute, None),
    mkEntry2(CKA_PRIME_BITS, Attribute, None),
    mkEntry2(CKA_SUB_PRIME_BITS, Attribute, None),
    mkEntry2(CKA_VALUE_BITS, Attribute, None),
    mkEntry2(CKA_VALUE_LEN, Attribute, None),
    mkEntry2(CKA_EXTRACTABLE, Attribute, Bool),
    mkEntry2(CKA_LOCAL, Attribute, Bool),
    mkEntry2(CKA_NEVER_EXTRACTABLE, Attribute, Bool),
    mkEntry2(CKA_ALWAYS_SENSITIVE, Attribute, Bool),
    mkEntry2(CKA_KEY_GEN_MECHANISM, Attribute, Mechanism),
    mkEntry2(CKA_MODIFIABLE, Attribute, Bool),
    mkEntry2(CKA_ECDSA_PARAMS, Attribute, None),
    mkEntry2(CKA_EC_PARAMS, Attribute, None),
    mkEntry2(CKA_EC_POINT, Attribute, None),
    mkEntry2(CKA_SECONDARY_AUTH, Attribute, None),
    mkEntry2(CKA_AUTH_PIN_FLAGS, Attribute, None),
    mkEntry2(CKA_HW_FEATURE_TYPE, Attribute, Hardware),
    mkEntry2(CKA_RESET_ON_INIT, Attribute, Bool),
    mkEntry2(CKA_HAS_RESET, Attribute, Bool),
    mkEntry2(CKA_NSS_URL, Attribute, None),
    mkEntry2(CKA_NSS_EMAIL, Attribute, None),
    mkEntry2(CKA_NSS_SMIME_INFO, Attribute, None),
    mkEntry2(CKA_NSS_SMIME_TIMESTAMP, Attribute, None),
    mkEntry2(CKA_NSS_PKCS8_SALT, Attribute, None),
    mkEntry2(CKA_NSS_PASSWORD_CHECK, Attribute, None),
    mkEntry2(CKA_NSS_EXPIRES, Attribute, None),
    mkEntry2(CKA_NSS_KRL, Attribute, None),
    mkEntry2(CKA_NSS_PQG_COUNTER, Attribute, None),
    mkEntry2(CKA_NSS_PQG_SEED, Attribute, None),
    mkEntry2(CKA_NSS_PQG_H, Attribute, None),
    mkEntry2(CKA_NSS_PQG_SEED_BITS, Attribute, None),
    mkEntry2(CKA_TRUST_DIGITAL_SIGNATURE, Attribute, Trust),
    mkEntry2(CKA_TRUST_NON_REPUDIATION, Attribute, Trust),
    mkEntry2(CKA_TRUST_KEY_ENCIPHERMENT, Attribute, Trust),
    mkEntry2(CKA_TRUST_DATA_ENCIPHERMENT, Attribute, Trust),
    mkEntry2(CKA_TRUST_KEY_AGREEMENT, Attribute, Trust),
    mkEntry2(CKA_TRUST_KEY_CERT_SIGN, Attribute, Trust),
    mkEntry2(CKA_TRUST_CRL_SIGN, Attribute, Trust),
    mkEntry2(CKA_TRUST_SERVER_AUTH, Attribute, Trust),
    mkEntry2(CKA_TRUST_CLIENT_AUTH, Attribute, Trust),
    mkEntry2(CKA_TRUST_CODE_SIGNING, Attribute, Trust),
    mkEntry2(CKA_TRUST_EMAIL_PROTECTION, Attribute, Trust),
    mkEntry2(CKA_TRUST_IPSEC_END_SYSTEM, Attribute, Trust),
    mkEntry2(CKA_TRUST_IPSEC_TUNNEL, Attribute, Trust),
    mkEntry2(CKA_TRUST_IPSEC_USER, Attribute, Trust),
    mkEntry2(CKA_TRUST_TIME_STAMPING, Attribute, Trust),
    mkEntry2(CKA_CERT_SHA1_HASH, Attribute, None),
    mkEntry2(CKA_CERT_MD5_HASH, Attribute, None),
    mkEntry2(CKA_NETSCAPE_DB, Attribute, None),
    mkEntry2(CKA_NETSCAPE_TRUST, Attribute, Trust),

    mkEntry(CKM_RSA_PKCS, Mechanism),
    mkEntry(CKM_RSA_9796, Mechanism),
    mkEntry(CKM_RSA_X_509, Mechanism),
    mkEntry(CKM_RSA_PKCS_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_MD2_RSA_PKCS, Mechanism),
    mkEntry(CKM_MD5_RSA_PKCS, Mechanism),
    mkEntry(CKM_SHA1_RSA_PKCS, Mechanism),
    mkEntry(CKM_RIPEMD128_RSA_PKCS, Mechanism),
    mkEntry(CKM_RIPEMD160_RSA_PKCS, Mechanism),
    mkEntry(CKM_RSA_PKCS_OAEP, Mechanism),
    mkEntry(CKM_RSA_X9_31_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_RSA_X9_31, Mechanism),
    mkEntry(CKM_SHA1_RSA_X9_31, Mechanism),
    mkEntry(CKM_DSA_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_DSA, Mechanism),
    mkEntry(CKM_DSA_SHA1, Mechanism),
    mkEntry(CKM_DH_PKCS_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_DH_PKCS_DERIVE, Mechanism),
    mkEntry(CKM_X9_42_DH_DERIVE, Mechanism),
    mkEntry(CKM_X9_42_DH_HYBRID_DERIVE, Mechanism),
    mkEntry(CKM_X9_42_MQV_DERIVE, Mechanism),
    mkEntry(CKM_SHA256_RSA_PKCS, Mechanism),
    mkEntry(CKM_SHA384_RSA_PKCS, Mechanism),
    mkEntry(CKM_SHA512_RSA_PKCS, Mechanism),
    mkEntry(CKM_RC2_KEY_GEN, Mechanism),
    mkEntry(CKM_RC2_ECB, Mechanism),
    mkEntry(CKM_RC2_CBC, Mechanism),
    mkEntry(CKM_RC2_MAC, Mechanism),
    mkEntry(CKM_RC2_MAC_GENERAL, Mechanism),
    mkEntry(CKM_RC2_CBC_PAD, Mechanism),
    mkEntry(CKM_RC4_KEY_GEN, Mechanism),
    mkEntry(CKM_RC4, Mechanism),
    mkEntry(CKM_DES_KEY_GEN, Mechanism),
    mkEntry(CKM_DES_ECB, Mechanism),
    mkEntry(CKM_DES_CBC, Mechanism),
    mkEntry(CKM_DES_MAC, Mechanism),
    mkEntry(CKM_DES_MAC_GENERAL, Mechanism),
    mkEntry(CKM_DES_CBC_PAD, Mechanism),
    mkEntry(CKM_DES2_KEY_GEN, Mechanism),
    mkEntry(CKM_DES3_KEY_GEN, Mechanism),
    mkEntry(CKM_DES3_ECB, Mechanism),
    mkEntry(CKM_DES3_CBC, Mechanism),
    mkEntry(CKM_DES3_MAC, Mechanism),
    mkEntry(CKM_DES3_MAC_GENERAL, Mechanism),
    mkEntry(CKM_DES3_CBC_PAD, Mechanism),
    mkEntry(CKM_CDMF_KEY_GEN, Mechanism),
    mkEntry(CKM_CDMF_ECB, Mechanism),
    mkEntry(CKM_CDMF_CBC, Mechanism),
    mkEntry(CKM_CDMF_MAC, Mechanism),
    mkEntry(CKM_CDMF_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CDMF_CBC_PAD, Mechanism),
    mkEntry(CKM_MD2, Mechanism),
    mkEntry(CKM_MD2_HMAC, Mechanism),
    mkEntry(CKM_MD2_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_MD5, Mechanism),
    mkEntry(CKM_MD5_HMAC, Mechanism),
    mkEntry(CKM_MD5_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_SHA_1, Mechanism),
    mkEntry(CKM_SHA_1_HMAC, Mechanism),
    mkEntry(CKM_SHA_1_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_RIPEMD128, Mechanism),
    mkEntry(CKM_RIPEMD128_HMAC, Mechanism),
    mkEntry(CKM_RIPEMD128_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_RIPEMD160, Mechanism),
    mkEntry(CKM_RIPEMD160_HMAC, Mechanism),
    mkEntry(CKM_RIPEMD160_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_SHA256, Mechanism),
    mkEntry(CKM_SHA256_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_SHA256_HMAC, Mechanism),
    mkEntry(CKM_SHA384, Mechanism),
    mkEntry(CKM_SHA384_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_SHA384_HMAC, Mechanism),
    mkEntry(CKM_SHA512, Mechanism),
    mkEntry(CKM_SHA512_HMAC_GENERAL, Mechanism),
    mkEntry(CKM_SHA512_HMAC, Mechanism),
    mkEntry(CKM_AES_CMAC, Mechanism),
    mkEntry(CKM_AES_CMAC_GENERAL, Mechanism),
    mkEntry(CKM_CAST_KEY_GEN, Mechanism),
    mkEntry(CKM_CAST_ECB, Mechanism),
    mkEntry(CKM_CAST_CBC, Mechanism),
    mkEntry(CKM_CAST_MAC, Mechanism),
    mkEntry(CKM_CAST_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CAST_CBC_PAD, Mechanism),
    mkEntry(CKM_CAST3_KEY_GEN, Mechanism),
    mkEntry(CKM_CAST3_ECB, Mechanism),
    mkEntry(CKM_CAST3_CBC, Mechanism),
    mkEntry(CKM_CAST3_MAC, Mechanism),
    mkEntry(CKM_CAST3_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CAST3_CBC_PAD, Mechanism),
    mkEntry(CKM_CAST5_KEY_GEN, Mechanism),
    mkEntry(CKM_CAST128_KEY_GEN, Mechanism),
    mkEntry(CKM_CAST5_ECB, Mechanism),
    mkEntry(CKM_CAST128_ECB, Mechanism),
    mkEntry(CKM_CAST5_CBC, Mechanism),
    mkEntry(CKM_CAST128_CBC, Mechanism),
    mkEntry(CKM_CAST5_MAC, Mechanism),
    mkEntry(CKM_CAST128_MAC, Mechanism),
    mkEntry(CKM_CAST5_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CAST128_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CAST5_CBC_PAD, Mechanism),
    mkEntry(CKM_CAST128_CBC_PAD, Mechanism),
    mkEntry(CKM_RC5_KEY_GEN, Mechanism),
    mkEntry(CKM_RC5_ECB, Mechanism),
    mkEntry(CKM_RC5_CBC, Mechanism),
    mkEntry(CKM_RC5_MAC, Mechanism),
    mkEntry(CKM_RC5_MAC_GENERAL, Mechanism),
    mkEntry(CKM_RC5_CBC_PAD, Mechanism),
    mkEntry(CKM_IDEA_KEY_GEN, Mechanism),
    mkEntry(CKM_IDEA_ECB, Mechanism),
    mkEntry(CKM_IDEA_CBC, Mechanism),
    mkEntry(CKM_IDEA_MAC, Mechanism),
    mkEntry(CKM_IDEA_MAC_GENERAL, Mechanism),
    mkEntry(CKM_IDEA_CBC_PAD, Mechanism),
    mkEntry(CKM_GENERIC_SECRET_KEY_GEN, Mechanism),
    mkEntry(CKM_CONCATENATE_BASE_AND_KEY, Mechanism),
    mkEntry(CKM_CONCATENATE_BASE_AND_DATA, Mechanism),
    mkEntry(CKM_CONCATENATE_DATA_AND_BASE, Mechanism),
    mkEntry(CKM_XOR_BASE_AND_DATA, Mechanism),
    mkEntry(CKM_EXTRACT_KEY_FROM_KEY, Mechanism),
    mkEntry(CKM_SSL3_PRE_MASTER_KEY_GEN, Mechanism),
    mkEntry(CKM_SSL3_MASTER_KEY_DERIVE, Mechanism),
    mkEntry(CKM_SSL3_KEY_AND_MAC_DERIVE, Mechanism),
    mkEntry(CKM_SSL3_MASTER_KEY_DERIVE_DH, Mechanism),
    mkEntry(CKM_TLS_PRE_MASTER_KEY_GEN, Mechanism),
    mkEntry(CKM_TLS_MASTER_KEY_DERIVE, Mechanism),
    mkEntry(CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256, Mechanism),
    mkEntry(CKM_TLS_KEY_AND_MAC_DERIVE, Mechanism),
    mkEntry(CKM_NSS_TLS_KEY_AND_MAC_DERIVE_SHA256, Mechanism),
    mkEntry(CKM_TLS_MASTER_KEY_DERIVE_DH, Mechanism),
    mkEntry(CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256, Mechanism),
    mkEntry(CKM_SSL3_MD5_MAC, Mechanism),
    mkEntry(CKM_SSL3_SHA1_MAC, Mechanism),
    mkEntry(CKM_MD5_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_MD2_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_SHA1_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_SHA256_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_SHA384_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_SHA512_KEY_DERIVATION, Mechanism),
    mkEntry(CKM_PBE_MD2_DES_CBC, Mechanism),
    mkEntry(CKM_PBE_MD5_DES_CBC, Mechanism),
    mkEntry(CKM_PBE_MD5_CAST_CBC, Mechanism),
    mkEntry(CKM_PBE_MD5_CAST3_CBC, Mechanism),
    mkEntry(CKM_PBE_MD5_CAST5_CBC, Mechanism),
    mkEntry(CKM_PBE_MD5_CAST128_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_CAST5_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_CAST128_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_RC4_128, Mechanism),
    mkEntry(CKM_PBE_SHA1_RC4_40, Mechanism),
    mkEntry(CKM_PBE_SHA1_DES3_EDE_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_DES2_EDE_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_RC2_128_CBC, Mechanism),
    mkEntry(CKM_PBE_SHA1_RC2_40_CBC, Mechanism),
    mkEntry(CKM_PKCS5_PBKD2, Mechanism),
    mkEntry(CKM_PBA_SHA1_WITH_SHA1_HMAC, Mechanism),
    mkEntry(CKM_KEY_WRAP_LYNKS, Mechanism),
    mkEntry(CKM_KEY_WRAP_SET_OAEP, Mechanism),
    mkEntry(CKM_SKIPJACK_KEY_GEN, Mechanism),
    mkEntry(CKM_SKIPJACK_ECB64, Mechanism),
    mkEntry(CKM_SKIPJACK_CBC64, Mechanism),
    mkEntry(CKM_SKIPJACK_OFB64, Mechanism),
    mkEntry(CKM_SKIPJACK_CFB64, Mechanism),
    mkEntry(CKM_SKIPJACK_CFB32, Mechanism),
    mkEntry(CKM_SKIPJACK_CFB16, Mechanism),
    mkEntry(CKM_SKIPJACK_CFB8, Mechanism),
    mkEntry(CKM_SKIPJACK_WRAP, Mechanism),
    mkEntry(CKM_SKIPJACK_PRIVATE_WRAP, Mechanism),
    mkEntry(CKM_SKIPJACK_RELAYX, Mechanism),
    mkEntry(CKM_KEA_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_KEA_KEY_DERIVE, Mechanism),
    mkEntry(CKM_FORTEZZA_TIMESTAMP, Mechanism),
    mkEntry(CKM_BATON_KEY_GEN, Mechanism),
    mkEntry(CKM_BATON_ECB128, Mechanism),
    mkEntry(CKM_BATON_ECB96, Mechanism),
    mkEntry(CKM_BATON_CBC128, Mechanism),
    mkEntry(CKM_BATON_COUNTER, Mechanism),
    mkEntry(CKM_BATON_SHUFFLE, Mechanism),
    mkEntry(CKM_BATON_WRAP, Mechanism),
    mkEntry(CKM_ECDSA_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_EC_KEY_PAIR_GEN, Mechanism),
    mkEntry(CKM_ECDSA, Mechanism),
    mkEntry(CKM_ECDSA_SHA1, Mechanism),
    mkEntry(CKM_ECDH1_DERIVE, Mechanism),
    mkEntry(CKM_ECDH1_COFACTOR_DERIVE, Mechanism),
    mkEntry(CKM_ECMQV_DERIVE, Mechanism),
    mkEntry(CKM_JUNIPER_KEY_GEN, Mechanism),
    mkEntry(CKM_JUNIPER_ECB128, Mechanism),
    mkEntry(CKM_JUNIPER_CBC128, Mechanism),
    mkEntry(CKM_JUNIPER_COUNTER, Mechanism),
    mkEntry(CKM_JUNIPER_SHUFFLE, Mechanism),
    mkEntry(CKM_JUNIPER_WRAP, Mechanism),
    mkEntry(CKM_FASTHASH, Mechanism),
    mkEntry(CKM_AES_KEY_GEN, Mechanism),
    mkEntry(CKM_AES_ECB, Mechanism),
    mkEntry(CKM_AES_CBC, Mechanism),
    mkEntry(CKM_AES_MAC, Mechanism),
    mkEntry(CKM_AES_MAC_GENERAL, Mechanism),
    mkEntry(CKM_AES_CBC_PAD, Mechanism),
    mkEntry(CKM_CAMELLIA_KEY_GEN, Mechanism),
    mkEntry(CKM_CAMELLIA_ECB, Mechanism),
    mkEntry(CKM_CAMELLIA_CBC, Mechanism),
    mkEntry(CKM_CAMELLIA_MAC, Mechanism),
    mkEntry(CKM_CAMELLIA_MAC_GENERAL, Mechanism),
    mkEntry(CKM_CAMELLIA_CBC_PAD, Mechanism),
    mkEntry(CKM_SEED_KEY_GEN, Mechanism),
    mkEntry(CKM_SEED_ECB, Mechanism),
    mkEntry(CKM_SEED_CBC, Mechanism),
    mkEntry(CKM_SEED_MAC, Mechanism),
    mkEntry(CKM_SEED_MAC_GENERAL, Mechanism),
    mkEntry(CKM_SEED_CBC_PAD, Mechanism),
    mkEntry(CKM_SEED_ECB_ENCRYPT_DATA, Mechanism),
    mkEntry(CKM_SEED_CBC_ENCRYPT_DATA, Mechanism),
    mkEntry(CKM_DSA_PARAMETER_GEN, Mechanism),
    mkEntry(CKM_DH_PKCS_PARAMETER_GEN, Mechanism),
    mkEntry(CKM_NSS_AES_KEY_WRAP, Mechanism),
    mkEntry(CKM_NSS_AES_KEY_WRAP_PAD, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_DES_CBC, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN, Mechanism),
    mkEntry(CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN, Mechanism),
    mkEntry(CKM_TLS_PRF_GENERAL, Mechanism),
    mkEntry(CKM_NSS_TLS_PRF_GENERAL_SHA256, Mechanism),

    mkEntry(CKR_OK, Result),
    mkEntry(CKR_CANCEL, Result),
    mkEntry(CKR_HOST_MEMORY, Result),
    mkEntry(CKR_SLOT_ID_INVALID, Result),
    mkEntry(CKR_GENERAL_ERROR, Result),
    mkEntry(CKR_FUNCTION_FAILED, Result),
    mkEntry(CKR_ARGUMENTS_BAD, Result),
    mkEntry(CKR_NO_EVENT, Result),
    mkEntry(CKR_NEED_TO_CREATE_THREADS, Result),
    mkEntry(CKR_CANT_LOCK, Result),
    mkEntry(CKR_ATTRIBUTE_READ_ONLY, Result),
    mkEntry(CKR_ATTRIBUTE_SENSITIVE, Result),
    mkEntry(CKR_ATTRIBUTE_TYPE_INVALID, Result),
    mkEntry(CKR_ATTRIBUTE_VALUE_INVALID, Result),
    mkEntry(CKR_DATA_INVALID, Result),
    mkEntry(CKR_DATA_LEN_RANGE, Result),
    mkEntry(CKR_DEVICE_ERROR, Result),
    mkEntry(CKR_DEVICE_MEMORY, Result),
    mkEntry(CKR_DEVICE_REMOVED, Result),
    mkEntry(CKR_ENCRYPTED_DATA_INVALID, Result),
    mkEntry(CKR_ENCRYPTED_DATA_LEN_RANGE, Result),
    mkEntry(CKR_FUNCTION_CANCELED, Result),
    mkEntry(CKR_FUNCTION_NOT_PARALLEL, Result),
    mkEntry(CKR_FUNCTION_NOT_SUPPORTED, Result),
    mkEntry(CKR_KEY_HANDLE_INVALID, Result),
    mkEntry(CKR_KEY_SIZE_RANGE, Result),
    mkEntry(CKR_KEY_TYPE_INCONSISTENT, Result),
    mkEntry(CKR_KEY_NOT_NEEDED, Result),
    mkEntry(CKR_KEY_CHANGED, Result),
    mkEntry(CKR_KEY_NEEDED, Result),
    mkEntry(CKR_KEY_INDIGESTIBLE, Result),
    mkEntry(CKR_KEY_FUNCTION_NOT_PERMITTED, Result),
    mkEntry(CKR_KEY_NOT_WRAPPABLE, Result),
    mkEntry(CKR_KEY_UNEXTRACTABLE, Result),
    mkEntry(CKR_KEY_PARAMS_INVALID, Result),
    mkEntry(CKR_MECHANISM_INVALID, Result),
    mkEntry(CKR_MECHANISM_PARAM_INVALID, Result),
    mkEntry(CKR_OBJECT_HANDLE_INVALID, Result),
    mkEntry(CKR_OPERATION_ACTIVE, Result),
    mkEntry(CKR_OPERATION_NOT_INITIALIZED, Result),
    mkEntry(CKR_PIN_INCORRECT, Result),
    mkEntry(CKR_PIN_INVALID, Result),
    mkEntry(CKR_PIN_LEN_RANGE, Result),
    mkEntry(CKR_PIN_EXPIRED, Result),
    mkEntry(CKR_PIN_LOCKED, Result),
    mkEntry(CKR_SESSION_CLOSED, Result),
    mkEntry(CKR_SESSION_COUNT, Result),
    mkEntry(CKR_SESSION_HANDLE_INVALID, Result),
    mkEntry(CKR_SESSION_PARALLEL_NOT_SUPPORTED, Result),
    mkEntry(CKR_SESSION_READ_ONLY, Result),
    mkEntry(CKR_SESSION_EXISTS, Result),
    mkEntry(CKR_SESSION_READ_ONLY_EXISTS, Result),
    mkEntry(CKR_SESSION_READ_WRITE_SO_EXISTS, Result),
    mkEntry(CKR_SIGNATURE_INVALID, Result),
    mkEntry(CKR_SIGNATURE_LEN_RANGE, Result),
    mkEntry(CKR_TEMPLATE_INCOMPLETE, Result),
    mkEntry(CKR_TEMPLATE_INCONSISTENT, Result),
    mkEntry(CKR_TOKEN_NOT_PRESENT, Result),
    mkEntry(CKR_TOKEN_NOT_RECOGNIZED, Result),
    mkEntry(CKR_TOKEN_WRITE_PROTECTED, Result),
    mkEntry(CKR_UNWRAPPING_KEY_HANDLE_INVALID, Result),
    mkEntry(CKR_UNWRAPPING_KEY_SIZE_RANGE, Result),
    mkEntry(CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT, Result),
    mkEntry(CKR_USER_ALREADY_LOGGED_IN, Result),
    mkEntry(CKR_USER_NOT_LOGGED_IN, Result),
    mkEntry(CKR_USER_PIN_NOT_INITIALIZED, Result),
    mkEntry(CKR_USER_TYPE_INVALID, Result),
    mkEntry(CKR_USER_ANOTHER_ALREADY_LOGGED_IN, Result),
    mkEntry(CKR_USER_TOO_MANY_TYPES, Result),
    mkEntry(CKR_WRAPPED_KEY_INVALID, Result),
    mkEntry(CKR_WRAPPED_KEY_LEN_RANGE, Result),
    mkEntry(CKR_WRAPPING_KEY_HANDLE_INVALID, Result),
    mkEntry(CKR_WRAPPING_KEY_SIZE_RANGE, Result),
    mkEntry(CKR_WRAPPING_KEY_TYPE_INCONSISTENT, Result),
    mkEntry(CKR_RANDOM_SEED_NOT_SUPPORTED, Result),
    mkEntry(CKR_RANDOM_NO_RNG, Result),
    mkEntry(CKR_DOMAIN_PARAMS_INVALID, Result),
    mkEntry(CKR_BUFFER_TOO_SMALL, Result),
    mkEntry(CKR_SAVED_STATE_INVALID, Result),
    mkEntry(CKR_INFORMATION_SENSITIVE, Result),
    mkEntry(CKR_STATE_UNSAVEABLE, Result),
    mkEntry(CKR_CRYPTOKI_NOT_INITIALIZED, Result),
    mkEntry(CKR_CRYPTOKI_ALREADY_INITIALIZED, Result),
    mkEntry(CKR_MUTEX_BAD, Result),
    mkEntry(CKR_MUTEX_NOT_LOCKED, Result),
    mkEntry(CKR_VENDOR_DEFINED, Result),

    mkEntry(CKT_NSS_TRUSTED, Trust),
    mkEntry(CKT_NSS_TRUSTED_DELEGATOR, Trust),
    mkEntry(CKT_NSS_NOT_TRUSTED, Trust),
    mkEntry(CKT_NSS_MUST_VERIFY_TRUST, Trust),
    mkEntry(CKT_NSS_TRUST_UNKNOWN, Trust),
    mkEntry(CKT_NSS_VALID_DELEGATOR, Trust),

    mkEntry(CK_EFFECTIVELY_INFINITE, AvailableSizes),
    mkEntry(CK_UNAVAILABLE_INFORMATION, CurrentSize),
};

const Constant *consts = &_consts[0];
const unsigned int constCount = sizeof(_consts) / sizeof(_consts[0]);

const Commands _commands[] = {
    { "C_Initialize",
      F_C_Initialize,
      "C_Initialize pInitArgs\n\n"
      "C_Initialize initializes the PKCS #11 library.\n"
      "  pInitArgs  if this is not NULL_PTR it gets cast to and dereferenced\n",
      { ArgInitializeArgs, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_Finalize",
      F_C_Finalize,
      "C_Finalize pReserved\n\n"
      "C_Finalize indicates that an application is done with the PKCS #11 library.\n"
      " pReserved  reserved. Should be NULL_PTR\n",
      { ArgInitializeArgs, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_GetInfo",
      F_C_GetInfo,
      "C_GetInfo pInfo\n\n"
      "C_GetInfo returns general information about PKCS #11.\n"
      " pInfo   location that receives information\n",
      { ArgInfo | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_GetFunctionList",
      F_C_GetFunctionList,
      "C_GetFunctionList ppFunctionList\n\n"
      "C_GetFunctionList returns the function list.\n"
      " ppFunctionList   receives pointer to function list\n",
      { ArgFunctionList | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_GetSlotList",
      F_C_GetSlotList,
      "C_GetSlotList tokenPresent pSlotList pulCount\n\n"
      "C_GetSlotList obtains a list of slots in the system.\n"
      " tokenPresent    only slots with tokens?\n"
      " pSlotList       receives array of slot IDs\n"
      " pulCount        receives number of slots\n",
      { ArgULong, ArgULong | ArgArray | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_GetSlotInfo",
      F_C_GetSlotInfo,
      "C_GetSlotInfo slotID pInfo\n\n"
      "C_GetSlotInfo obtains information about a particular slot in the system.\n"
      " slotID    the ID of the slot\n"
      " pInfo     receives the slot information\n",
      { ArgULong, ArgSlotInfo | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_GetTokenInfo",
      F_C_GetTokenInfo,
      "C_GetTokenInfo slotID pInfo\n\n"
      "C_GetTokenInfo obtains information about a particular token in the system.\n"
      " slotID    ID of the token's slot\n"
      " pInfo     receives the token information\n",
      { ArgULong, ArgTokenInfo | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_GetMechanismList",
      F_C_GetMechanismList,
      "C_GetMechanismList slotID pMechanismList pulCount\n\n"
      "C_GetMechanismList obtains a list of mechanism types supported by a token.\n"
      " slotID            ID of token's slot\n"
      " pMechanismList    gets mech. array\n"
      " pulCount          gets # of mechs.\n",
      { ArgULong, ArgULong | ArgArray | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_GetMechanismInfo",
      F_C_GetMechanismInfo,
      "C_GetMechanismInfo slotID type pInfo\n\n"
      "C_GetMechanismInfo obtains information about a particular mechanism possibly\n"
      "supported by a token.\n"
      " slotID    ID of the token's slot\n"
      " type      type of mechanism\n"
      " pInfo     receives mechanism info\n",
      { ArgULong, ArgULong, ArgMechanismInfo | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_InitToken",
      F_C_InitToken,
      "C_InitToken slotID pPin ulPinLen pLabel\n\n"
      "C_InitToken initializes a token.\n"
      " slotID      ID of the token's slot\n"
      " pPin        the SO's initial PIN\n"
      " ulPinLen    length in bytes of the PIN\n"
      " pLabel      32-byte token label (blank padded)\n",
      { ArgULong, ArgUTF8, ArgULong, ArgUTF8, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_InitPIN",
      F_C_InitPIN,
      "C_InitPIN hSession pPin ulPinLen\n\n"
      "C_InitPIN initializes the normal user's PIN.\n"
      " hSession    the session's handle\n"
      " pPin        the normal user's PIN\n"
      " ulPinLen    length in bytes of the PIN\n",
      { ArgULong, ArgUTF8, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_SetPIN",
      F_C_SetPIN,
      "C_SetPIN hSession pOldPin ulOldLen pNewPin ulNewLen\n\n"
      "C_SetPIN modifies the PIN of the user who is logged in.\n"
      " hSession    the session's handle\n"
      " pOldPin     the old PIN\n"
      " ulOldLen    length of the old PIN\n"
      " pNewPin     the new PIN\n"
      " ulNewLen    length of the new PIN\n",
      { ArgULong, ArgUTF8, ArgULong, ArgUTF8, ArgULong, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_OpenSession",
      F_C_OpenSession,
      "C_OpenSession slotID flags phSession\n\n"
      "C_OpenSession opens a session between an application and a token.\n"
      " slotID          the slot's ID\n"
      " flags           from\n"
      " phSession       gets session handle\n",
      { ArgULong, ArgULong, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_CloseSession",
      F_C_CloseSession,
      "C_CloseSession hSession\n\n"
      "C_CloseSession closes a session between an application and a token.\n"
      " hSession   the session's handle\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_CloseAllSessions",
      F_C_CloseAllSessions,
      "C_CloseAllSessions slotID\n\n"
      "C_CloseAllSessions closes all sessions with a token.\n"
      " slotID   the token's slot\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_GetSessionInfo",
      F_C_GetSessionInfo,
      "C_GetSessionInfo hSession pInfo\n\n"
      "C_GetSessionInfo obtains information about the session.\n"
      " hSession    the session's handle\n"
      " pInfo       receives session info\n",
      { ArgULong, ArgSessionInfo | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_GetOperationState",
      F_C_GetOperationState,
      "C_GetOperationState hSession pOpState pulOpStateLen\n\n"
      "C_GetOperationState obtains the state of the cryptographic operation in a\n"
      "session.\n"
      " hSession        session's handle\n"
      " pOpState        gets state\n"
      " pulOpStateLen   gets state length\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_SetOperationState",
      F_C_SetOperationState,
      "C_SetOperationState hSession pOpState ulOpStateLen hEncKey hAuthKey\n\n"
      "C_SetOperationState restores the state of the cryptographic operation in a\n"
      "session.\n"
      " hSession        session's handle\n"
      " pOpState        holds state\n"
      " ulOpStateLen    holds state length\n"
      " hEncKey         en/decryption key\n"
      " hAuthnKey       sign/verify key\n",
      { ArgULong, ArgChar | ArgOut, ArgULong, ArgULong, ArgULong, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_Login",
      F_C_Login,
      "C_Login hSession userType pPin ulPinLen\n\n"
      "C_Login logs a user into a token.\n"
      " hSession    the session's handle\n"
      " userType    the user type\n"
      " pPin        the user's PIN\n"
      " ulPinLen    the length of the PIN\n",
      { ArgULong, ArgULong, ArgVar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_Logout",
      F_C_Logout,
      "C_Logout hSession\n\n"
      "C_Logout logs a user out from a token.\n"
      " hSession   the session's handle\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_CreateObject",
      F_C_CreateObject,
      "C_CreateObject hSession pTemplate ulCount phObject\n\n"
      "C_CreateObject creates a new object.\n"
      " hSession      the session's handle\n"
      " pTemplate     the object's template\n"
      " ulCount       attributes in template\n"
      " phObject   gets new object's handle.\n",
      { ArgULong, ArgAttribute | ArgArray, ArgULong, ArgULong | ArgOut, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_CopyObject",
      F_C_CopyObject,
      "C_CopyObject hSession hObject pTemplate ulCount phNewObject\n\n"
      "C_CopyObject copies an object creating a new object for the copy.\n"
      " hSession      the session's handle\n"
      " hObject       the object's handle\n"
      " pTemplate     template for new object\n"
      " ulCount       attributes in template\n"
      " phNewObject   receives handle of copy\n",
      { ArgULong, ArgULong, ArgAttribute | ArgArray, ArgULong, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DestroyObject",
      F_C_DestroyObject,
      "C_DestroyObject hSession hObject\n\n"
      "C_DestroyObject destroys an object.\n"
      " hSession    the session's handle\n"
      " hObject     the object's handle\n",
      { ArgULong, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_GetObjectSize",
      F_C_GetObjectSize,
      "C_GetObjectSize hSession hObject pulSize\n\n"
      "C_GetObjectSize gets the size of an object in bytes.\n"
      " hSession    the session's handle\n"
      " hObject     the object's handle\n"
      " pulSize     receives size of object\n",
      { ArgULong, ArgULong, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_GetAttributeValue",
      F_C_GetAttributeValue,
      "C_GetAttributeValue hSession hObject pTemplate ulCount\n\n"
      "C_GetAttributeValue obtains the value of one or more object attributes.\n"
      " hSession     the session's handle\n"
      " hObject      the object's handle\n"
      " pTemplate    specifies attrs; gets vals\n"
      " ulCount      attributes in template\n",
      { ArgULong, ArgULong, ArgAttribute | ArgArray, ArgULong, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_SetAttributeValue",
      F_C_SetAttributeValue,
      "C_SetAttributeValue hSession hObject pTemplate ulCount\n\n"
      "C_SetAttributeValue modifies the value of one or more object attributes\n"
      " hSession     the session's handle\n"
      " hObject      the object's handle\n"
      " pTemplate    specifies attrs and values\n"
      " ulCount      attributes in template\n",
      { ArgULong, ArgULong, ArgAttribute | ArgArray, ArgULong, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_FindObjectsInit",
      F_C_FindObjectsInit,
      "C_FindObjectsInit hSession pTemplate ulCount\n\n"
      "C_FindObjectsInit initializes a search for token and session objects that\n"
      "match a template.\n"
      " hSession     the session's handle\n"
      " pTemplate    attribute values to match\n"
      " ulCount      attrs in search template\n",
      { ArgULong, ArgAttribute | ArgArray, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_FindObjectsFinal",
      F_C_FindObjectsFinal,
      "C_FindObjectsFinal hSession\n\n"
      "C_FindObjectsFinal finishes a search for token and session objects.\n"
      " hSession   the session's handle\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_FindObjects",
      F_C_FindObjects,
      "C_FindObjects hSession phObject ulMaxObjectCount pulObjectCount\n\n"
      "C_FindObjects continues a search for token and session objects that match\n"
      "a template obtaining additional object handles.\n"
      " hSession            session's handle\n"
      " phObject            gets obj. handles\n"
      " ulMaxObjectCount    max handles to get\n"
      " pulObjectCount      actual # returned\n",
      { ArgULong, ArgULong | ArgOut, ArgULong, ArgULong | ArgOut, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_EncryptInit",
      F_C_EncryptInit,
      "C_EncryptInit hSession pMechanism hKey\n\n"
      "C_EncryptInit initializes an encryption operation.\n"
      " hSession      the session's handle\n"
      " pMechanism    the encryption mechanism\n"
      " hKey          handle of encryption key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_EncryptUpdate",
      F_C_EncryptUpdate,
      "C_EncryptUpdate hSession pPart ulPartLen pEncryptedPart pulEncryptedPartLen\n"
      "\n"
      "C_EncryptUpdate continues a multiple-part encryption operation.\n"
      " hSession             session's handle\n"
      " pPart                the plaintext data\n"
      " ulPartLen            plaintext data len\n"
      " pEncryptedPart       gets ciphertext\n"
      " pulEncryptedPartLen  gets c-text size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_EncryptFinal",
      F_C_EncryptFinal,
      "C_EncryptFinal hSession pLastEncryptedPart pulLastEncryptedPartLen\n\n"
      "C_EncryptFinal finishes a multiple-part encryption operation.\n"
      " hSession                  session handle\n"
      " pLastEncryptedPart        last c-text\n"
      " pulLastEncryptedPartLen   gets last size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_Encrypt",
      F_C_Encrypt,
      "C_Encrypt hSession pData ulDataLen pEncryptedData pulEncryptedDataLen\n\n"
      "C_Encrypt encrypts single-part data.\n"
      " hSession              session's handle\n"
      " pData                 the plaintext data\n"
      " ulDataLen             bytes of plaintext\n"
      " pEncryptedData        gets ciphertext\n"
      " pulEncryptedDataLen   gets c-text size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DecryptInit",
      F_C_DecryptInit,
      "C_DecryptInit hSession pMechanism hKey\n\n"
      "C_DecryptInit initializes a decryption operation.\n"
      " hSession      the session's handle\n"
      " pMechanism    the decryption mechanism\n"
      " hKey          handle of decryption key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_DecryptUpdate",
      F_C_DecryptUpdate,
      "C_DecryptUpdate hSession pEncryptedPart ulEncryptedPartLen pPart pulPartLen\n"
      "\n"
      "C_DecryptUpdate continues a multiple-part decryption operation.\n"
      " hSession              session's handle\n"
      " pEncryptedPart        encrypted data\n"
      " ulEncryptedPartLen    input length\n"
      " pPart                 gets plaintext\n"
      " pulPartLen            p-text size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DecryptFinal",
      F_C_DecryptFinal,
      "C_DecryptFinal hSession pLastPart pulLastPartLen\n\n"
      "C_DecryptFinal finishes a multiple-part decryption operation.\n"
      " hSession         the session's handle\n"
      " pLastPart        gets plaintext\n"
      " pulLastPartLen   p-text size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_Decrypt",
      F_C_Decrypt,
      "C_Decrypt hSession pEncryptedData ulEncryptedDataLen pData pulDataLen\n\n"
      "C_Decrypt decrypts encrypted data in a single part.\n"
      " hSession             session's handle\n"
      " pEncryptedData       ciphertext\n"
      " ulEncryptedDataLen   ciphertext length\n"
      " pData                gets plaintext\n"
      " pulDataLen           gets p-text size\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DigestInit",
      F_C_DigestInit,
      "C_DigestInit hSession pMechanism\n\n"
      "C_DigestInit initializes a message-digesting operation.\n"
      " hSession     the session's handle\n"
      " pMechanism   the digesting mechanism\n",
      { ArgULong, ArgMechanism, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_DigestUpdate",
      F_C_DigestUpdate,
      "C_DigestUpdate hSession pPart ulPartLen\n\n"
      "C_DigestUpdate continues a multiple-part message-digesting operation.\n"
      " hSession    the session's handle\n"
      " pPart       data to be digested\n"
      " ulPartLen   bytes of data to be digested\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DigestKey",
      F_C_DigestKey,
      "C_DigestKey hSession hKey\n\n"
      "C_DigestKey continues a multi-part message-digesting operation by digesting\n"
      "the value of a secret key as part of the data already digested.\n"
      " hSession    the session's handle\n"
      " hKey        secret key to digest\n",
      { ArgULong, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_DigestFinal",
      F_C_DigestFinal,
      "C_DigestFinal hSession pDigest pulDigestLen\n\n"
      "C_DigestFinal finishes a multiple-part message-digesting operation.\n"
      " hSession       the session's handle\n"
      " pDigest        gets the message digest\n"
      " pulDigestLen   gets byte count of digest\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_Digest",
      F_C_Digest,
      "C_Digest hSession pData ulDataLen pDigest pulDigestLen\n\n"
      "C_Digest digests data in a single part.\n"
      " hSession       the session's handle\n"
      " pData          data to be digested\n"
      " ulDataLen      bytes of data to digest\n"
      " pDigest        gets the message digest\n"
      " pulDigestLen   gets digest length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_SignInit",
      F_C_SignInit,
      "C_SignInit hSession pMechanism hKey\n\n"
      "C_SignInit initializes a signature (private key encryption operation where\n"
      "the signature is (will be) an appendix to the data and plaintext cannot be\n"
      "recovered from the signature.\n"
      " hSession      the session's handle\n"
      " pMechanism    the signature mechanism\n"
      " hKey          handle of signature key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_SignUpdate",
      F_C_SignUpdate,
      "C_SignUpdate hSession pPart ulPartLen\n\n"
      "C_SignUpdate continues a multiple-part signature operation where the\n"
      "signature is (will be) an appendix to the data and plaintext cannot be\n"
      "recovered from the signature.\n"
      " hSession    the session's handle\n"
      " pPart       the data to sign\n"
      " ulPartLen   count of bytes to sign\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_SignFinal",
      F_C_SignFinal,
      "C_SignFinal hSession pSignature pulSignatureLen\n\n"
      "C_SignFinal finishes a multiple-part signature operation returning the\n"
      "signature.\n"
      " hSession          the session's handle\n"
      " pSignature        gets the signature\n"
      " pulSignatureLen   gets signature length\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_SignRecoverInit",
      F_C_SignRecoverInit,
      "C_SignRecoverInit hSession pMechanism hKey\n\n"
      "C_SignRecoverInit initializes a signature operation where the data can be\n"
      "recovered from the signature.\n"
      " hSession     the session's handle\n"
      " pMechanism   the signature mechanism\n"
      " hKey         handle of the signature key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_SignRecover",
      F_C_SignRecover,
      "C_SignRecover hSession pData ulDataLen pSignature pulSignatureLen\n\n"
      "C_SignRecover signs data in a single operation where the data can be\n"
      "recovered from the signature.\n"
      " hSession          the session's handle\n"
      " pData             the data to sign\n"
      " ulDataLen         count of bytes to sign\n"
      " pSignature        gets the signature\n"
      " pulSignatureLen   gets signature length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_Sign",
      F_C_Sign,
      "C_Sign hSession pData ulDataLen pSignature pulSignatureLen\n\n"
      "C_Sign signs (encrypts with private key) data in a single part where the\n"
      "signature is (will be) an appendix to the data and plaintext cannot be\n"
      "recovered from the signature.\n"
      " hSession          the session's handle\n"
      " pData             the data to sign\n"
      " ulDataLen         count of bytes to sign\n"
      " pSignature        gets the signature\n"
      " pulSignatureLen   gets signature length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_VerifyInit",
      F_C_VerifyInit,
      "C_VerifyInit hSession pMechanism hKey\n\n"
      "C_VerifyInit initializes a verification operation where the signature is an\n"
      "appendix to the data and plaintext cannot cannot be recovered from the\n"
      "signature (e.g. DSA).\n"
      " hSession      the session's handle\n"
      " pMechanism    the verification mechanism\n"
      " hKey          verification key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_VerifyUpdate",
      F_C_VerifyUpdate,
      "C_VerifyUpdate hSession pPart ulPartLen\n\n"
      "C_VerifyUpdate continues a multiple-part verification operation where the\n"
      "signature is an appendix to the data and plaintext cannot be recovered from\n"
      "the signature.\n"
      " hSession    the session's handle\n"
      " pPart       signed data\n"
      " ulPartLen   length of signed data\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_VerifyFinal",
      F_C_VerifyFinal,
      "C_VerifyFinal hSession pSignature ulSignatureLen\n\n"
      "C_VerifyFinal finishes a multiple-part verification operation checking the\n"
      "signature.\n"
      " hSession         the session's handle\n"
      " pSignature       signature to verify\n"
      " ulSignatureLen   signature length\n",
      { ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "C_VerifyRecoverInit",
      F_C_VerifyRecoverInit,
      "C_VerifyRecoverInit hSession pMechanism hKey\n\n"
      "C_VerifyRecoverInit initializes a signature verification operation where the\n"
      "data is recovered from the signature.\n"
      " hSession      the session's handle\n"
      " pMechanism    the verification mechanism\n"
      " hKey          verification key\n",
      { ArgULong, ArgMechanism, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "C_VerifyRecover",
      F_C_VerifyRecover,
      "C_VerifyRecover hSession pSignature ulSignatureLen pData pulDataLen\n\n"
      "C_VerifyRecover verifies a signature in a single-part operation where the\n"
      "data is recovered from the signature.\n"
      " hSession          the session's handle\n"
      " pSignature        signature to verify\n"
      " ulSignatureLen    signature length\n"
      " pData             gets signed data\n"
      " pulDataLen        gets signed data len\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_Verify",
      F_C_Verify,
      "C_Verify hSession pData ulDataLen pSignature ulSignatureLen\n\n"
      "C_Verify verifies a signature in a single-part operation where the signature\n"
      "is an appendix to the data and plaintext cannot be recovered from the\n"
      "signature.\n"
      " hSession         the session's handle\n"
      " pData            signed data\n"
      " ulDataLen        length of signed data\n"
      " pSignature       signature\n"
      " ulSignatureLen   signature length*/\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DigestEncryptUpdate",
      F_C_DigestEncryptUpdate,
      "C_DigestEncryptUpdate hSession pPart ulPartLen pEncryptedPart \\\n"
      "    pulEncryptedPartLen\n\n"
      "C_DigestEncryptUpdate continues a multiple-part digesting and encryption\n"
      "operation.\n"
      " hSession              session's handle\n"
      " pPart                 the plaintext data\n"
      " ulPartLen             plaintext length\n"
      " pEncryptedPart        gets ciphertext\n"
      " pulEncryptedPartLen   gets c-text length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DecryptDigestUpdate",
      F_C_DecryptDigestUpdate,
      "C_DecryptDigestUpdate hSession pEncryptedPart ulEncryptedPartLen pPart \\\n"
      "    pulPartLen\n\n"
      "C_DecryptDigestUpdate continues a multiple-part decryption and digesting\n"
      "operation.\n"
      " hSession              session's handle\n"
      " pEncryptedPart        ciphertext\n"
      " ulEncryptedPartLen    ciphertext length\n"
      " pPart                 gets plaintext\n"
      " pulPartLen            gets plaintext len\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_SignEncryptUpdate",
      F_C_SignEncryptUpdate,
      "C_SignEncryptUpdate hSession pPart ulPartLen pEncryptedPart \\\n"
      "    pulEncryptedPartLen\n\n"
      "C_SignEncryptUpdate continues a multiple-part signing and encryption\n"
      "operation.\n"
      " hSession              session's handle\n"
      " pPart                 the plaintext data\n"
      " ulPartLen             plaintext length\n"
      " pEncryptedPart        gets ciphertext\n"
      " pulEncryptedPartLen   gets c-text length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_DecryptVerifyUpdate",
      F_C_DecryptVerifyUpdate,
      "C_DecryptVerifyUpdate hSession pEncryptedPart ulEncryptedPartLen pPart \\\n"
      "    pulPartLen\n\n"
      "C_DecryptVerifyUpdate continues a multiple-part decryption and verify\n"
      "operation.\n"
      " hSession              session's handle\n"
      " pEncryptedPart        ciphertext\n"
      " ulEncryptedPartLen    ciphertext length\n"
      " pPart                 gets plaintext\n"
      " pulPartLen            gets p-text length\n",
      { ArgULong, ArgChar, ArgULong, ArgChar | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_GenerateKeyPair",
      F_C_GenerateKeyPair,
      "C_GenerateKeyPair hSession pMechanism pPublicKeyTemplate \\\n"
      "    ulPublicKeyAttributeCount pPrivateKeyTemplate ulPrivateKeyAttributeCount \\\n"
      "    phPublicKey phPrivateKey\n\n"
      "C_GenerateKeyPair generates a public-key/private-key pair creating new key\n"
      "objects.\n"
      " hSession                      sessionhandle\n"
      " pMechanism                    key-genmech.\n"
      " pPublicKeyTemplate            templatefor pub. key\n"
      " ulPublicKeyAttributeCount     # pub. attrs.\n"
      " pPrivateKeyTemplate           templatefor priv. key\n"
      " ulPrivateKeyAttributeCount    # priv. attrs.\n"
      " phPublicKey                   gets pub. keyhandle\n"
      " phPrivateKey                  getspriv. keyhandle\n",
      { ArgULong, ArgMechanism, ArgAttribute | ArgArray, ArgULong,
        ArgAttribute | ArgArray, ArgULong, ArgULong | ArgOut, ArgULong | ArgOut, ArgNone,
        ArgNone } },
    { "C_GenerateKey",
      F_C_GenerateKey,
      "C_GenerateKey hSession pMechanism pTemplate ulCount phKey\n\n"
      "C_GenerateKey generates a secret key creating a new key object.\n"
      " hSession      the session's handle\n"
      " pMechanism    key generation mech.\n"
      " pTemplate     template for new key\n"
      " ulCount       # of attrs in template\n"
      " phKey         gets handle of new key\n",
      { ArgULong, ArgMechanism, ArgAttribute | ArgArray, ArgULong, ArgULong | ArgOut,
        ArgNone, ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_WrapKey",
      F_C_WrapKey,
      "C_WrapKey hSession pMechanism hWrappingKey hKey pWrappedKey pulWrappedKeyLen\n\n"
      "C_WrapKey wraps (i.e. encrypts) a key.\n"
      " hSession          the session's handle\n"
      " pMechanism        the wrapping mechanism\n"
      " hWrappingKey      wrapping key\n"
      " hKey              key to be wrapped\n"
      " pWrappedKey       gets wrapped key\n"
      " pulWrappedKeyLen  gets wrapped key size\n",
      { ArgULong, ArgMechanism, ArgULong, ArgULong, ArgULong, ArgChar | ArgOut,
        ArgULong | ArgOut, ArgNone, ArgNone, ArgNone } },
    { "C_UnwrapKey",
      F_C_UnwrapKey,
      "C_UnwrapKey hSession pMechanism hUnwrappingKey pWrappedKey ulWrappedKeyLen \\\n"
      "    pTemplate ulAttributeCount phKey\n\n"
      "C_UnwrapKey unwraps (decrypts) a wrapped key creating a new key object.\n"
      " hSession            session's handle\n"
      " pMechanism          unwrapping mech.\n"
      " hUnwrappingKey      unwrapping key\n"
      " pWrappedKey         the wrapped key\n"
      " ulWrappedKeyLen     wrapped key len\n"
      " pTemplate           new key template\n"
      " ulAttributeCount    template length\n"
      " phKey               gets new handle\n",
      { ArgULong, ArgMechanism, ArgULong, ArgChar, ArgULong, ArgAttribute | ArgArray,
        ArgULong, ArgULong | ArgOut, ArgNone, ArgNone } },
    { "C_DeriveKey",
      F_C_DeriveKey,
      "C_DeriveKey hSession pMechanism hBaseKey pTemplate ulAttributeCount phKey\n\n"
      "C_DeriveKey derives a key from a base key creating a new key object.\n"
      " hSession            session's handle\n"
      " pMechanism          key deriv. mech.\n"
      " hBaseKey            base key\n"
      " pTemplate           new key template\n"
      " ulAttributeCount    template length\n"
      " phKey               gets new handle\n",
      { ArgULong, ArgMechanism, ArgULong, ArgAttribute | ArgArray, ArgULong,
        ArgULong | ArgOut, ArgNone, ArgNone, ArgNone, ArgNone } },
    { "C_SeedRandom",
      F_C_SeedRandom,
      "C_SeedRandom hSession pSeed ulSeedLen\n\n"
      "C_SeedRandom mixes additional seed material into the token's random number\n"
      "generator.\n"
      " hSession    the session's handle\n"
      " pSeed       the seed material\n"
      " ulSeedLen   length of seed material\n",
      { ArgULong, ArgChar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_GenerateRandom",
      F_C_GenerateRandom,
      "C_GenerateRandom hSession RandomData ulRandomLen\n\n"
      "C_GenerateRandom generates random data.\n"
      " hSession      the session's handle\n"
      " RandomData    receives the random data\n"
      " ulRandomLen   # of bytes to generate\n",
      { ArgULong, ArgChar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_GetFunctionStatus",
      F_C_GetFunctionStatus,
      "C_GetFunctionStatus hSession\n\n"
      "C_GetFunctionStatus is a legacy function; it obtains an updated status of\n"
      "a function running in parallel with an application.\n"
      " hSession   the session's handle\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_CancelFunction",
      F_C_CancelFunction,
      "C_CancelFunction hSession\n\n"
      "C_CancelFunction is a legacy function; it cancels a function running in\n"
      "parallel.\n"
      " hSession   the session's handle\n",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "C_WaitForSlotEvent",
      F_C_WaitForSlotEvent,
      "C_WaitForSlotEvent flags pSlot pRserved\n\n"
      "C_WaitForSlotEvent waits for a slot event (token insertion removal etc.)\n"
      "to occur.\n"
      " flags          blocking/nonblocking flag\n"
      " pSlot    location that receives the slot ID\n"
      " pRserved    reserved.  Should be NULL_PTR\n",
      { ArgULong, ArgULong | ArgArray, ArgVar, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "NewArray",
      F_NewArray,
      "NewArray varName varType array size\n\n"
      "Creates a new array variable.\n"
      " varName     variable name of the new array\n"
      " varType     data type of the new array\n"
      " size        number of elements in the array\n",
      { ArgVar | ArgNew, ArgVar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "NewInitArg",
      F_NewInitializeArgs,
      "NewInitArg varName flags string\n\n"
      "Creates a new init variable.\n"
      " varName     variable name of the new initArg\n"
      " flags       value to set the flags field\n"
      " string      string parameter for init arg\n",
      { ArgVar | ArgNew, ArgULong, ArgVar | ArgNew, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "NewTemplate",
      F_NewTemplate,
      "NewTemplate varName attributeList\n\n"
      "Create a new empty template and populate the attribute list\n"
      " varName        variable name of the new template\n"
      " attributeList  comma separated list of CKA_ATTRIBUTE types\n",
      { ArgVar | ArgNew, ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "NewMechanism",
      F_NewMechanism,
      "NewMechanism varName mechanismType\n\n"
      "Create a new CK_MECHANISM object with type NULL parameters and specified type\n"
      " varName        variable name of the new mechansim\n"
      " mechanismType  CKM_ mechanism type value to set int the type field\n",
      { ArgVar | ArgNew, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "BuildTemplate",
      F_BuildTemplate,
      "BuildTemplate template\n\n"
      "Allocates space for the value in a template which has the sizes filled in,\n"
      "but no values allocated yet.\n"
      " template        variable name of the template\n",
      { ArgAttribute, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "SetTemplate",
      F_SetTemplate,
      "SetTemplate template index value\n\n"
      "Sets a particular element of a template to a CK_ULONG\n"
      " template        variable name of the template\n"
      " index           index into the template to the element to change\n"
      " value           32 bit value to set in the template\n",
      { ArgAttribute, ArgULong, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "SetString",
      F_SetStringVar,
      "SetString varName string\n\n"
      "Sets a particular variable to a string value\n"
      " variable        variable name of new string\n"
      " string          String to set the variable to\n",
      { ArgVar | ArgNew, ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Set",
      F_SetVar,
      "Set varName value\n\n"
      "Sets a particular variable to CK_ULONG\n"
      " variable        name of the new variable\n"
      " value           32 bit value to set variable to\n",
      { ArgVar | ArgNew, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Print",
      F_Print,
      "Print varName\n\n"
      "prints a variable\n"
      " variable        name of the variable to print\n",
      { ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "Delete",
      F_Delete,
      "Delete varName\n\n"
      "delete a variable\n"
      " variable        name of the variable to delete\n",
      { ArgVar | ArgNew, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Load",
      F_Load,
      "load libraryName\n\n"
      "load a pkcs #11 module\n"
      " libraryName        Name of a shared library\n",
      { ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "Save",
      F_SaveVar,
      "Save filename variable\n\n"
      "Saves the binary value of 'variable' in file 'filename'\n"
      " fileName        target file to save the variable in\n"
      " variable        variable to save\n",
      { ArgVar | ArgNew, ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Restore",
      F_RestoreVar,
      "Restore filename variable\n\n"
      "Restores a variable from a file\n"
      " fileName        target file to restore the variable from\n"
      " variable        variable to restore\n",
      { ArgVar | ArgNew, ArgVar, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Increment",
      F_Increment,
      "Increment variable value\n\n"
      "Increment a variable by value\n",
      { ArgVar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "Decrement",
      F_Decrement,
      "Decrement variable value\n\n"
      "Decrement a variable by value\n",
      { ArgVar, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "List",
      F_List,
      "List all the variables\n",
      { ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "Unload",
      F_Unload,
      "Unload the currrently loaded PKCS #11 library\n",
      { ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "Run",
      F_Run,
      "Run filename\n\n"
      "reads filename as script of commands to execute\n",
      { ArgVar | ArgNew, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "Time",
      F_Time,
      "Time pkcs11 command\n\n"
      "Execute a pkcs #11 command and time the results\n",
      { ArgVar | ArgFull, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "System",
      F_System,
      "Set System Flag",
      { ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
    { "LoopRun",
      F_Loop,
      "LoopRun filename var start end step\n\n"
      "Run in a loop. Loop exit if scrip does and explicit quit (Quit QuitIf etc.)",
      { ArgVar | ArgNew, ArgVar | ArgNew, ArgULong, ArgULong, ArgULong, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone } },
    { "Help",
      F_Help,
      "Help [command]\n\n"
      "print general help, or help for a specific command\n",
      { ArgVar | ArgOpt, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "QuitIf",
      F_QuitIf,
      "QuitIf arg1 comparator arg2\n\n"
      "Exit from this program if Condition is valid, valid comparators:\n"
      "  < > <= >= = !=\n",
      { ArgULong, ArgVar | ArgNew, ArgULong, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone } },
    { "QuitIfString",
      F_QuitIfString,
      "QuitIfString arg1 comparator arg2\n\n"
      "Exit from this program if Condition is valid, valid comparators:\n"
      "  = !=\n",
      { ArgVar | ArgNew, ArgVar | ArgNew, ArgVar | ArgNew, ArgNone, ArgNone, ArgNone,
        ArgNone, ArgNone, ArgNone, ArgNone } },
    { "Quit",
      F_Quit,
      "Exit from this program",
      { ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone, ArgNone,
        ArgNone } },
};

const Commands *commands = &_commands[0];
const int commandCount = sizeof(_commands) / sizeof(_commands[0]);

const Topics _topics[] = {
    { "variables",
      "Variables are random strings of characters. These should begin with alpha\n"
      " characters, and should not contain any spaces, nor should they match any\n"
      " built-in constants. There is some checking in the code for these things,\n"
      " but it's not 100% and using invalid variable names can cause problems.\n"
      " Variables are created by any 'OUT' parameter. If the variable does not\n"
      " exist, it will be created. For in parameters variables must already exist.\n" },
    { "constants",
      "pk11util recognizes *lots* of constants. All CKA_, CKF_, CKO_, CKU_, CKS_,\n"
      " CKC_, CKK_, CKH_, CKM_, CKT_ values from the PKCS #11 spec are recognized.\n"
      " Constants can be specified with their fully qualified CK?_ value, or the\n"
      " prefix can be dropped. Constants are matched case insensitve.\n" },
    { "arrays",
      "Arrays are special variables which represent 'C' arrays. Each array \n"
      " variable can be referenced as a group (using just the name), or as \n"
      " individual elements (with the [int] operator). Example:\n"
      "      print myArray    # prints the full array.\n"
      "      print myArray[3] # prints the 3rd elemement of the array \n" },
    { "sizes",
      "Size operaters returns the size in bytes of a variable, or the number of\n"
      " elements in an array.\n"
      "    size(var) and sizeof(var) return the size of var in bytes.\n"
      "    sizea(var) and sizeofarray(var) return the number of elements in var.\n"
      "       If var is not an array, sizea(var) returns 1.\n" },
};

const Topics *topics = &_topics[0];
const int topicCount = sizeof(_topics) / sizeof(_topics[0]);

const char *
getName(CK_ULONG value, ConstType type)
{
    unsigned int i;

    for (i = 0; i < constCount; i++) {
        if (consts[i].type == type && consts[i].value == value) {
            return consts[i].name;
        }
        if (type == ConstNone && consts[i].value == value) {
            return consts[i].name;
        }
    }

    return NULL;
}

const char *
getNameFromAttribute(CK_ATTRIBUTE_TYPE type)
{
    return getName(type, ConstAttribute);
}

unsigned int
totalKnownType(ConstType type)
{
    unsigned int count = 0;
    unsigned int i;

    for (i = 0; i < constCount; i++) {
        if (consts[i].type == type)
            count++;
    }
    return count;
}
