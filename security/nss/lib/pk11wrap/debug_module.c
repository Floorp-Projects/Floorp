/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "prlog.h"
#include <stdio.h>
#include "cert.h"  /* for CERT_DerNameToAscii & CERT_Hexify */

static PRLogModuleInfo *modlog = NULL;

static CK_FUNCTION_LIST_PTR module_functions;

static CK_FUNCTION_LIST debug_functions;

static void print_final_statistics(void);

#define STRING static const char 

STRING fmt_flags[]                = "  flags = 0x%x";
STRING fmt_hKey[]                 = "  hKey = 0x%x";
STRING fmt_hObject[]              = "  hObject = 0x%x";
STRING fmt_hSession[]             = "  hSession = 0x%x";
STRING fmt_manufacturerID[]       = "  manufacturerID = \"%.32s\"";
STRING fmt_pData[]                = "  pData = 0x%p";
STRING fmt_pDigest[]              = "  pDigest = 0x%p";
STRING fmt_pEncryptedData[]       = "  pEncryptedData = 0x%p";
STRING fmt_pEncryptedPart[]       = "  pEncryptedPart = 0x%p";
STRING fmt_pInfo[]                = "  pInfo = 0x%p";
STRING fmt_pMechanism[]           = "  pMechanism = 0x%p";
STRING fmt_pOperationState[]      = "  pOperationState = 0x%p";
STRING fmt_pPart[]                = "  pPart = 0x%p";
STRING fmt_pPin[]                 = "  pPin = 0x%p";
STRING fmt_pSignature[]           = "  pSignature = 0x%p";
STRING fmt_pTemplate[]            = "  pTemplate = 0x%p";
STRING fmt_pWrappedKey[]          = "  pWrappedKey = 0x%p";
STRING fmt_phKey[]                = "  phKey = 0x%p";
STRING fmt_phObject[]             = "  phObject = 0x%p";
STRING fmt_pulCount[]             = "  pulCount = 0x%p";
STRING fmt_pulDataLen[]           = "  pulDataLen = 0x%p";
STRING fmt_pulDigestLen[]         = "  pulDigestLen = 0x%p";
STRING fmt_pulEncryptedPartLen[]  = "  pulEncryptedPartLen = 0x%p";
STRING fmt_pulPartLen[]           = "  pulPartLen = 0x%p";
STRING fmt_pulSignatureLen[]      = "  pulSignatureLen = 0x%p";
STRING fmt_slotID[]               = "  slotID = 0x%x";
STRING fmt_sphKey[]               = "  *phKey = 0x%x";
STRING fmt_spulCount[]            = "  *pulCount = 0x%x";
STRING fmt_spulDataLen[]          = "  *pulDataLen = 0x%x";
STRING fmt_spulDigestLen[]        = "  *pulDigestLen = 0x%x";
STRING fmt_spulEncryptedPartLen[] = "  *pulEncryptedPartLen = 0x%x";
STRING fmt_spulPartLen[]          = "  *pulPartLen = 0x%x";
STRING fmt_spulSignatureLen[]     = "  *pulSignatureLen = 0x%x";
STRING fmt_ulAttributeCount[]     = "  ulAttributeCount = %d";
STRING fmt_ulCount[]              = "  ulCount = %d";
STRING fmt_ulDataLen[]            = "  ulDataLen = %d";
STRING fmt_ulEncryptedPartLen[]   = "  ulEncryptedPartLen = %d";
STRING fmt_ulPartLen[]            = "  ulPartLen = %d";
STRING fmt_ulPinLen[]             = "  ulPinLen = %d";
STRING fmt_ulSignatureLen[]       = "  ulSignatureLen = %d";

STRING fmt_fwVersion[]            = "  firmware version: %d.%d";
STRING fmt_hwVersion[]            = "  hardware version: %d.%d";
STRING fmt_s_qsq_d[]              = "    %s = \"%s\" [%d]";
STRING fmt_s_s_d[]                = "    %s = %s [%d]";
STRING fmt_invalid_handle[]       = " (CK_INVALID_HANDLE)";


static void get_attr_type_str(CK_ATTRIBUTE_TYPE atype, char *str, int len)
{
#define CASE(attr) case attr: a = #attr ; break

    const char * a = NULL;

    switch (atype) {
    CASE(CKA_CLASS);
    CASE(CKA_TOKEN);
    CASE(CKA_PRIVATE);
    CASE(CKA_LABEL);
    CASE(CKA_APPLICATION);
    CASE(CKA_VALUE);
    CASE(CKA_OBJECT_ID);
    CASE(CKA_CERTIFICATE_TYPE);
    CASE(CKA_ISSUER);
    CASE(CKA_SERIAL_NUMBER);
    CASE(CKA_AC_ISSUER);
    CASE(CKA_OWNER);
    CASE(CKA_ATTR_TYPES);
    CASE(CKA_TRUSTED);
    CASE(CKA_KEY_TYPE);
    CASE(CKA_SUBJECT);
    CASE(CKA_ID);
    CASE(CKA_SENSITIVE);
    CASE(CKA_ENCRYPT);
    CASE(CKA_DECRYPT);
    CASE(CKA_WRAP);
    CASE(CKA_UNWRAP);
    CASE(CKA_SIGN);
    CASE(CKA_SIGN_RECOVER);
    CASE(CKA_VERIFY);
    CASE(CKA_VERIFY_RECOVER);
    CASE(CKA_DERIVE);
    CASE(CKA_START_DATE);
    CASE(CKA_END_DATE);
    CASE(CKA_MODULUS);
    CASE(CKA_MODULUS_BITS);
    CASE(CKA_PUBLIC_EXPONENT);
    CASE(CKA_PRIVATE_EXPONENT);
    CASE(CKA_PRIME_1);
    CASE(CKA_PRIME_2);
    CASE(CKA_EXPONENT_1);
    CASE(CKA_EXPONENT_2);
    CASE(CKA_COEFFICIENT);
    CASE(CKA_PRIME);
    CASE(CKA_SUBPRIME);
    CASE(CKA_BASE);
    CASE(CKA_PRIME_BITS);
    CASE(CKA_SUB_PRIME_BITS);
    CASE(CKA_VALUE_BITS);
    CASE(CKA_VALUE_LEN);
    CASE(CKA_EXTRACTABLE);
    CASE(CKA_LOCAL);
    CASE(CKA_NEVER_EXTRACTABLE);
    CASE(CKA_ALWAYS_SENSITIVE);
    CASE(CKA_KEY_GEN_MECHANISM);
    CASE(CKA_MODIFIABLE);
    CASE(CKA_ECDSA_PARAMS);
    CASE(CKA_EC_POINT);
    CASE(CKA_SECONDARY_AUTH);
    CASE(CKA_AUTH_PIN_FLAGS);
    CASE(CKA_HW_FEATURE_TYPE);
    CASE(CKA_RESET_ON_INIT);
    CASE(CKA_HAS_RESET);
    CASE(CKA_VENDOR_DEFINED);
    CASE(CKA_NETSCAPE_URL);
    CASE(CKA_NETSCAPE_EMAIL);
    CASE(CKA_NETSCAPE_SMIME_INFO);
    CASE(CKA_NETSCAPE_SMIME_TIMESTAMP);
    CASE(CKA_NETSCAPE_PKCS8_SALT);
    CASE(CKA_NETSCAPE_PASSWORD_CHECK);
    CASE(CKA_NETSCAPE_EXPIRES);
    CASE(CKA_NETSCAPE_KRL);
    CASE(CKA_NETSCAPE_PQG_COUNTER);
    CASE(CKA_NETSCAPE_PQG_SEED);
    CASE(CKA_NETSCAPE_PQG_H);
    CASE(CKA_NETSCAPE_PQG_SEED_BITS);
    CASE(CKA_TRUST);
    CASE(CKA_TRUST_DIGITAL_SIGNATURE);
    CASE(CKA_TRUST_NON_REPUDIATION);
    CASE(CKA_TRUST_KEY_ENCIPHERMENT);
    CASE(CKA_TRUST_DATA_ENCIPHERMENT);
    CASE(CKA_TRUST_KEY_AGREEMENT);
    CASE(CKA_TRUST_KEY_CERT_SIGN);
    CASE(CKA_TRUST_CRL_SIGN);
    CASE(CKA_TRUST_SERVER_AUTH);
    CASE(CKA_TRUST_CLIENT_AUTH);
    CASE(CKA_TRUST_CODE_SIGNING);
    CASE(CKA_TRUST_EMAIL_PROTECTION);
    CASE(CKA_TRUST_IPSEC_END_SYSTEM);
    CASE(CKA_TRUST_IPSEC_TUNNEL);
    CASE(CKA_TRUST_IPSEC_USER);
    CASE(CKA_TRUST_TIME_STAMPING);
    CASE(CKA_CERT_SHA1_HASH);
    CASE(CKA_CERT_MD5_HASH);
    CASE(CKA_NETSCAPE_DB);
    CASE(CKA_NETSCAPE_TRUST);
    default: break;
    }
    if (a)
	PR_snprintf(str, len, "%s", a);
    else
	PR_snprintf(str, len, "0x%p", atype);
}

static void get_obj_class(CK_OBJECT_CLASS objClass, char *str, int len)
{

    const char * a = NULL;

    switch (objClass) {
    CASE(CKO_DATA);
    CASE(CKO_CERTIFICATE);
    CASE(CKO_PUBLIC_KEY);
    CASE(CKO_PRIVATE_KEY);
    CASE(CKO_SECRET_KEY);
    CASE(CKO_HW_FEATURE);
    CASE(CKO_DOMAIN_PARAMETERS);
    CASE(CKO_NETSCAPE_CRL);
    CASE(CKO_NETSCAPE_SMIME);
    CASE(CKO_NETSCAPE_TRUST);
    CASE(CKO_NETSCAPE_BUILTIN_ROOT_LIST);
    default: break;
    }
    if (a)
	PR_snprintf(str, len, "%s", a);
    else
	PR_snprintf(str, len, "0x%p", objClass);
}

static void get_trust_val(CK_TRUST trust, char *str, int len)
{
    const char * a = NULL;

    switch (trust) {
    CASE(CKT_NETSCAPE_TRUSTED);
    CASE(CKT_NETSCAPE_TRUSTED_DELEGATOR);
    CASE(CKT_NETSCAPE_UNTRUSTED);
    CASE(CKT_NETSCAPE_MUST_VERIFY);
    CASE(CKT_NETSCAPE_TRUST_UNKNOWN);
    CASE(CKT_NETSCAPE_VALID);
    CASE(CKT_NETSCAPE_VALID_DELEGATOR);
    default: break;
    }
    if (a)
	PR_snprintf(str, len, "%s", a);
    else
	PR_snprintf(str, len, "0x%p", trust);
}

static void log_rv(CK_RV rv)
{
    const char * a = NULL;

    switch (rv) {
    CASE(CKR_OK);
    CASE(CKR_CANCEL);
    CASE(CKR_HOST_MEMORY);
    CASE(CKR_SLOT_ID_INVALID);
    CASE(CKR_GENERAL_ERROR);
    CASE(CKR_FUNCTION_FAILED);
    CASE(CKR_ARGUMENTS_BAD);
    CASE(CKR_NO_EVENT);
    CASE(CKR_NEED_TO_CREATE_THREADS);
    CASE(CKR_CANT_LOCK);
    CASE(CKR_ATTRIBUTE_READ_ONLY);
    CASE(CKR_ATTRIBUTE_SENSITIVE);
    CASE(CKR_ATTRIBUTE_TYPE_INVALID);
    CASE(CKR_ATTRIBUTE_VALUE_INVALID);
    CASE(CKR_DATA_INVALID);
    CASE(CKR_DATA_LEN_RANGE);
    CASE(CKR_DEVICE_ERROR);
    CASE(CKR_DEVICE_MEMORY);
    CASE(CKR_DEVICE_REMOVED);
    CASE(CKR_ENCRYPTED_DATA_INVALID);
    CASE(CKR_ENCRYPTED_DATA_LEN_RANGE);
    CASE(CKR_FUNCTION_CANCELED);
    CASE(CKR_FUNCTION_NOT_PARALLEL);
    CASE(CKR_FUNCTION_NOT_SUPPORTED);
    CASE(CKR_KEY_HANDLE_INVALID);
    CASE(CKR_KEY_SIZE_RANGE);
    CASE(CKR_KEY_TYPE_INCONSISTENT);
    CASE(CKR_KEY_NOT_NEEDED);
    CASE(CKR_KEY_CHANGED);
    CASE(CKR_KEY_NEEDED);
    CASE(CKR_KEY_INDIGESTIBLE);
    CASE(CKR_KEY_FUNCTION_NOT_PERMITTED);
    CASE(CKR_KEY_NOT_WRAPPABLE);
    CASE(CKR_KEY_UNEXTRACTABLE);
    CASE(CKR_MECHANISM_INVALID);
    CASE(CKR_MECHANISM_PARAM_INVALID);
    CASE(CKR_OBJECT_HANDLE_INVALID);
    CASE(CKR_OPERATION_ACTIVE);
    CASE(CKR_OPERATION_NOT_INITIALIZED);
    CASE(CKR_PIN_INCORRECT);
    CASE(CKR_PIN_INVALID);
    CASE(CKR_PIN_LEN_RANGE);
    CASE(CKR_PIN_EXPIRED);
    CASE(CKR_PIN_LOCKED);
    CASE(CKR_SESSION_CLOSED);
    CASE(CKR_SESSION_COUNT);
    CASE(CKR_SESSION_HANDLE_INVALID);
    CASE(CKR_SESSION_PARALLEL_NOT_SUPPORTED);
    CASE(CKR_SESSION_READ_ONLY);
    CASE(CKR_SESSION_EXISTS);
    CASE(CKR_SESSION_READ_ONLY_EXISTS);
    CASE(CKR_SESSION_READ_WRITE_SO_EXISTS);
    CASE(CKR_SIGNATURE_INVALID);
    CASE(CKR_SIGNATURE_LEN_RANGE);
    CASE(CKR_TEMPLATE_INCOMPLETE);
    CASE(CKR_TEMPLATE_INCONSISTENT);
    CASE(CKR_TOKEN_NOT_PRESENT);
    CASE(CKR_TOKEN_NOT_RECOGNIZED);
    CASE(CKR_TOKEN_WRITE_PROTECTED);
    CASE(CKR_UNWRAPPING_KEY_HANDLE_INVALID);
    CASE(CKR_UNWRAPPING_KEY_SIZE_RANGE);
    CASE(CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT);
    CASE(CKR_USER_ALREADY_LOGGED_IN);
    CASE(CKR_USER_NOT_LOGGED_IN);
    CASE(CKR_USER_PIN_NOT_INITIALIZED);
    CASE(CKR_USER_TYPE_INVALID);
    CASE(CKR_USER_ANOTHER_ALREADY_LOGGED_IN);
    CASE(CKR_USER_TOO_MANY_TYPES);
    CASE(CKR_WRAPPED_KEY_INVALID);
    CASE(CKR_WRAPPED_KEY_LEN_RANGE);
    CASE(CKR_WRAPPING_KEY_HANDLE_INVALID);
    CASE(CKR_WRAPPING_KEY_SIZE_RANGE);
    CASE(CKR_WRAPPING_KEY_TYPE_INCONSISTENT);
    CASE(CKR_RANDOM_SEED_NOT_SUPPORTED);
    CASE(CKR_RANDOM_NO_RNG);
    CASE(CKR_DOMAIN_PARAMS_INVALID);
    CASE(CKR_BUFFER_TOO_SMALL);
    CASE(CKR_SAVED_STATE_INVALID);
    CASE(CKR_INFORMATION_SENSITIVE);
    CASE(CKR_STATE_UNSAVEABLE);
    CASE(CKR_CRYPTOKI_NOT_INITIALIZED);
    CASE(CKR_CRYPTOKI_ALREADY_INITIALIZED);
    CASE(CKR_MUTEX_BAD);
    CASE(CKR_MUTEX_NOT_LOCKED);
    CASE(CKR_FUNCTION_REJECTED);
    CASE(CKR_KEY_PARAMS_INVALID);
    default: break;
    }
    if (a)
	PR_LOG(modlog, 1, ("  rv = %s\n", a));
    else
	PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
}

static void log_state(CK_STATE state)
{
    const char * a = NULL;

    switch (state) {
    CASE(CKS_RO_PUBLIC_SESSION);
    CASE(CKS_RO_USER_FUNCTIONS);
    CASE(CKS_RW_PUBLIC_SESSION);
    CASE(CKS_RW_USER_FUNCTIONS);
    CASE(CKS_RW_SO_FUNCTIONS);
    default: break;
    }
    if (a)
	PR_LOG(modlog, 1, ("  state = %s\n", a));
    else
	PR_LOG(modlog, 1, ("  state = 0x%x\n", state));
}

static void log_handle(int level, const char * format, CK_ULONG handle)
{
    char fmtBuf[80];
    if (handle)
	PR_LOG(modlog, level, (format, handle));
    else {
	PL_strncpyz(fmtBuf, format, sizeof fmtBuf);
	PL_strcatn(fmtBuf, sizeof fmtBuf, fmt_invalid_handle);
	PR_LOG(modlog, level, (fmtBuf, handle));
    }
}

static void print_mechanism(CK_MECHANISM_PTR m)
{

    const char * a = NULL;

    switch (m->mechanism) {
    CASE(CKM_AES_CBC);
    CASE(CKM_AES_CBC_ENCRYPT_DATA);
    CASE(CKM_AES_CBC_PAD);
    CASE(CKM_AES_ECB);
    CASE(CKM_AES_ECB_ENCRYPT_DATA);
    CASE(CKM_AES_KEY_GEN);
    CASE(CKM_AES_MAC);
    CASE(CKM_AES_MAC_GENERAL);
    CASE(CKM_CAMELLIA_CBC);
    CASE(CKM_CAMELLIA_CBC_ENCRYPT_DATA);
    CASE(CKM_CAMELLIA_CBC_PAD);
    CASE(CKM_CAMELLIA_ECB);
    CASE(CKM_CAMELLIA_ECB_ENCRYPT_DATA);
    CASE(CKM_CAMELLIA_KEY_GEN);
    CASE(CKM_CAMELLIA_MAC);
    CASE(CKM_CAMELLIA_MAC_GENERAL);
    CASE(CKM_CDMF_CBC);
    CASE(CKM_CDMF_CBC_PAD);
    CASE(CKM_CDMF_ECB);
    CASE(CKM_CDMF_KEY_GEN);
    CASE(CKM_CDMF_MAC);
    CASE(CKM_CDMF_MAC_GENERAL);
    CASE(CKM_CMS_SIG);
    CASE(CKM_CONCATENATE_BASE_AND_DATA);
    CASE(CKM_CONCATENATE_BASE_AND_KEY);
    CASE(CKM_CONCATENATE_DATA_AND_BASE);
    CASE(CKM_DES2_KEY_GEN);
    CASE(CKM_DES3_CBC);
    CASE(CKM_DES3_CBC_ENCRYPT_DATA);
    CASE(CKM_DES3_CBC_PAD);
    CASE(CKM_DES3_ECB);
    CASE(CKM_DES3_ECB_ENCRYPT_DATA);
    CASE(CKM_DES3_KEY_GEN);
    CASE(CKM_DES3_MAC);
    CASE(CKM_DES3_MAC_GENERAL);
    CASE(CKM_DES_CBC);
    CASE(CKM_DES_CBC_ENCRYPT_DATA);
    CASE(CKM_DES_CBC_PAD);
    CASE(CKM_DES_CFB64);
    CASE(CKM_DES_CFB8);
    CASE(CKM_DES_ECB);
    CASE(CKM_DES_ECB_ENCRYPT_DATA);
    CASE(CKM_DES_KEY_GEN);
    CASE(CKM_DES_MAC);
    CASE(CKM_DES_MAC_GENERAL);
    CASE(CKM_DES_OFB64);
    CASE(CKM_DES_OFB8);
    CASE(CKM_DH_PKCS_DERIVE);
    CASE(CKM_DH_PKCS_KEY_PAIR_GEN);
    CASE(CKM_DH_PKCS_PARAMETER_GEN);
    CASE(CKM_DSA);
    CASE(CKM_DSA_KEY_PAIR_GEN);
    CASE(CKM_DSA_PARAMETER_GEN);
    CASE(CKM_DSA_SHA1);
    CASE(CKM_ECDH1_COFACTOR_DERIVE);
    CASE(CKM_ECDH1_DERIVE);
    CASE(CKM_ECDSA);
    CASE(CKM_ECDSA_SHA1);
    CASE(CKM_ECMQV_DERIVE);
    CASE(CKM_EC_KEY_PAIR_GEN);	     /* also CASE(CKM_ECDSA_KEY_PAIR_GEN); */
    CASE(CKM_EXTRACT_KEY_FROM_KEY);
    CASE(CKM_FASTHASH);
    CASE(CKM_FORTEZZA_TIMESTAMP);
    CASE(CKM_GENERIC_SECRET_KEY_GEN);
    CASE(CKM_IDEA_CBC);
    CASE(CKM_IDEA_CBC_PAD);
    CASE(CKM_IDEA_ECB);
    CASE(CKM_IDEA_KEY_GEN);
    CASE(CKM_IDEA_MAC);
    CASE(CKM_IDEA_MAC_GENERAL);
    CASE(CKM_KEA_KEY_DERIVE);
    CASE(CKM_KEA_KEY_PAIR_GEN);
    CASE(CKM_KEY_WRAP_LYNKS);
    CASE(CKM_KEY_WRAP_SET_OAEP);
    CASE(CKM_MD2);
    CASE(CKM_MD2_HMAC);
    CASE(CKM_MD2_HMAC_GENERAL);
    CASE(CKM_MD2_KEY_DERIVATION);
    CASE(CKM_MD2_RSA_PKCS);
    CASE(CKM_MD5);
    CASE(CKM_MD5_HMAC);
    CASE(CKM_MD5_HMAC_GENERAL);
    CASE(CKM_MD5_KEY_DERIVATION);
    CASE(CKM_MD5_RSA_PKCS);
    CASE(CKM_PBA_SHA1_WITH_SHA1_HMAC);
    CASE(CKM_PBE_MD2_DES_CBC);
    CASE(CKM_PBE_MD5_DES_CBC);
    CASE(CKM_PBE_SHA1_DES2_EDE_CBC);
    CASE(CKM_PBE_SHA1_DES3_EDE_CBC);
    CASE(CKM_PBE_SHA1_RC2_128_CBC);
    CASE(CKM_PBE_SHA1_RC2_40_CBC);
    CASE(CKM_PBE_SHA1_RC4_128);
    CASE(CKM_PBE_SHA1_RC4_40);
    CASE(CKM_PKCS5_PBKD2);
    CASE(CKM_RC2_CBC);
    CASE(CKM_RC2_CBC_PAD);
    CASE(CKM_RC2_ECB);
    CASE(CKM_RC2_KEY_GEN);
    CASE(CKM_RC2_MAC);
    CASE(CKM_RC2_MAC_GENERAL);
    CASE(CKM_RC4);
    CASE(CKM_RC4_KEY_GEN);
    CASE(CKM_RC5_CBC);
    CASE(CKM_RC5_CBC_PAD);
    CASE(CKM_RC5_ECB);
    CASE(CKM_RC5_KEY_GEN);
    CASE(CKM_RC5_MAC);
    CASE(CKM_RC5_MAC_GENERAL);
    CASE(CKM_RIPEMD128);
    CASE(CKM_RIPEMD128_HMAC);
    CASE(CKM_RIPEMD128_HMAC_GENERAL);
    CASE(CKM_RIPEMD128_RSA_PKCS);
    CASE(CKM_RIPEMD160);
    CASE(CKM_RIPEMD160_HMAC);
    CASE(CKM_RIPEMD160_HMAC_GENERAL);
    CASE(CKM_RIPEMD160_RSA_PKCS);
    CASE(CKM_RSA_9796);
    CASE(CKM_RSA_PKCS);
    CASE(CKM_RSA_PKCS_KEY_PAIR_GEN);
    CASE(CKM_RSA_PKCS_OAEP);
    CASE(CKM_RSA_PKCS_PSS);
    CASE(CKM_RSA_X9_31);
    CASE(CKM_RSA_X9_31_KEY_PAIR_GEN);
    CASE(CKM_RSA_X_509);
    CASE(CKM_SHA1_KEY_DERIVATION);
    CASE(CKM_SHA1_RSA_PKCS);
    CASE(CKM_SHA1_RSA_PKCS_PSS);
    CASE(CKM_SHA1_RSA_X9_31);
    CASE(CKM_SHA224);
    CASE(CKM_SHA224_HMAC);
    CASE(CKM_SHA224_HMAC_GENERAL);
    CASE(CKM_SHA224_KEY_DERIVATION);
    CASE(CKM_SHA224_RSA_PKCS);
    CASE(CKM_SHA224_RSA_PKCS_PSS);
    CASE(CKM_SHA256);
    CASE(CKM_SHA256_HMAC);
    CASE(CKM_SHA256_HMAC_GENERAL);
    CASE(CKM_SHA256_KEY_DERIVATION);
    CASE(CKM_SHA256_RSA_PKCS);
    CASE(CKM_SHA256_RSA_PKCS_PSS);
    CASE(CKM_SHA384);
    CASE(CKM_SHA384_HMAC);
    CASE(CKM_SHA384_HMAC_GENERAL);
    CASE(CKM_SHA384_KEY_DERIVATION);
    CASE(CKM_SHA384_RSA_PKCS);
    CASE(CKM_SHA384_RSA_PKCS_PSS);
    CASE(CKM_SHA512);
    CASE(CKM_SHA512_HMAC);
    CASE(CKM_SHA512_HMAC_GENERAL);
    CASE(CKM_SHA512_KEY_DERIVATION);
    CASE(CKM_SHA512_RSA_PKCS);
    CASE(CKM_SHA512_RSA_PKCS_PSS);
    CASE(CKM_SHA_1);
    CASE(CKM_SHA_1_HMAC);
    CASE(CKM_SHA_1_HMAC_GENERAL);
    CASE(CKM_SKIPJACK_CBC64);
    CASE(CKM_SKIPJACK_CFB16);
    CASE(CKM_SKIPJACK_CFB32);
    CASE(CKM_SKIPJACK_CFB64);
    CASE(CKM_SKIPJACK_CFB8);
    CASE(CKM_SKIPJACK_ECB64);
    CASE(CKM_SKIPJACK_KEY_GEN);
    CASE(CKM_SKIPJACK_OFB64);
    CASE(CKM_SKIPJACK_PRIVATE_WRAP);
    CASE(CKM_SKIPJACK_RELAYX);
    CASE(CKM_SKIPJACK_WRAP);
    CASE(CKM_SSL3_KEY_AND_MAC_DERIVE);
    CASE(CKM_SSL3_MASTER_KEY_DERIVE);
    CASE(CKM_SSL3_MASTER_KEY_DERIVE_DH);
    CASE(CKM_SSL3_MD5_MAC);
    CASE(CKM_SSL3_PRE_MASTER_KEY_GEN);
    CASE(CKM_SSL3_SHA1_MAC);
    CASE(CKM_TLS_KEY_AND_MAC_DERIVE);
    CASE(CKM_TLS_MASTER_KEY_DERIVE);
    CASE(CKM_TLS_MASTER_KEY_DERIVE_DH);
    CASE(CKM_TLS_PRE_MASTER_KEY_GEN);
    CASE(CKM_TLS_PRF);
    CASE(CKM_TWOFISH_CBC);
    CASE(CKM_TWOFISH_KEY_GEN);
    CASE(CKM_X9_42_DH_DERIVE);
    CASE(CKM_X9_42_DH_HYBRID_DERIVE);
    CASE(CKM_X9_42_DH_KEY_PAIR_GEN);
    CASE(CKM_X9_42_DH_PARAMETER_GEN);
    CASE(CKM_X9_42_MQV_DERIVE);
    CASE(CKM_XOR_BASE_AND_DATA);
    default: break;
    }
    if (a)
	PR_LOG(modlog, 4, ("      mechanism = %s", a));
    else
	PR_LOG(modlog, 4, ("      mechanism = 0x%p", m->mechanism));
}

static void get_key_type(CK_KEY_TYPE keyType, char *str, int len)
{

    const char * a = NULL;

    switch (keyType) {
    CASE(CKK_AES);
    CASE(CKK_CAMELLIA);
    CASE(CKK_CDMF);
    CASE(CKK_DES);
    CASE(CKK_DES2);
    CASE(CKK_DES3);
    CASE(CKK_DH);
    CASE(CKK_DSA);
    CASE(CKK_EC);		/* also CASE(CKK_ECDSA); */
    CASE(CKK_GENERIC_SECRET);
    CASE(CKK_IDEA);
    CASE(CKK_INVALID_KEY_TYPE);
    CASE(CKK_KEA);
    CASE(CKK_RC2);
    CASE(CKK_RC4);
    CASE(CKK_RC5);
    CASE(CKK_RSA);
    CASE(CKK_SKIPJACK);
    CASE(CKK_TWOFISH);
    CASE(CKK_X9_42_DH);
    default: break;
    }
    if (a)
	PR_snprintf(str, len, "%s", a);
    else
	PR_snprintf(str, len, "0x%p", keyType);
}

static void print_attr_value(CK_ATTRIBUTE_PTR attr)
{
    char atype[48];
    char valstr[49];
    int len;

    get_attr_type_str(attr->type, atype, sizeof atype);
    switch (attr->type) {
    case CKA_ALWAYS_SENSITIVE:
    case CKA_DECRYPT:
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_EXTRACTABLE:
    case CKA_LOCAL:
    case CKA_MODIFIABLE:
    case CKA_NEVER_EXTRACTABLE:
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_TOKEN:
    case CKA_UNWRAP:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_BBOOL tf = *((CK_BBOOL *)attr->pValue);
	    PR_LOG(modlog, 4, (fmt_s_s_d, 
	           atype, tf ? "CK_TRUE" : "CK_FALSE", attr->ulValueLen));
	    break;
	}
    case CKA_CLASS:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_OBJECT_CLASS objClass = *((CK_OBJECT_CLASS *)attr->pValue);
	    get_obj_class(objClass, valstr, sizeof valstr);
	    PR_LOG(modlog, 4, (fmt_s_s_d, 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_TRUST_CLIENT_AUTH:
    case CKA_TRUST_CODE_SIGNING:
    case CKA_TRUST_EMAIL_PROTECTION:
    case CKA_TRUST_SERVER_AUTH:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_TRUST trust = *((CK_TRUST *)attr->pValue);
	    get_trust_val(trust, valstr, sizeof valstr);
	    PR_LOG(modlog, 4, (fmt_s_s_d, 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_KEY_TYPE:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_KEY_TYPE keyType = *((CK_KEY_TYPE *)attr->pValue);
	    get_key_type(keyType, valstr, sizeof valstr);
	    PR_LOG(modlog, 4, (fmt_s_s_d, 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_LABEL:
    case CKA_NETSCAPE_EMAIL:
    case CKA_NETSCAPE_URL:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    len = PR_MIN(attr->ulValueLen + 1, sizeof valstr);
	    PR_snprintf(valstr, len, "%s", attr->pValue);
	    PR_LOG(modlog, 4, (fmt_s_qsq_d, 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_ISSUER:
    case CKA_SUBJECT:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    char * asciiName;
	    SECItem derName;
	    derName.type = siDERNameBuffer;
	    derName.data = attr->pValue;
	    derName.len  = attr->ulValueLen;
	    asciiName = CERT_DerNameToAscii(&derName);
	    if (asciiName) {
		PR_LOG(modlog, 4, (fmt_s_s_d, 
		       atype, asciiName, attr->ulValueLen));
	    	PORT_Free(asciiName);
		break;
	    }
	    /* else fall through and treat like a binary buffer */
	}
    case CKA_ID:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    unsigned char * pV = attr->pValue;
	    for (len = (int)attr->ulValueLen; len > 0; --len) {
		unsigned int ch = *pV++;
		if (ch >= 0x20 && ch < 0x7f) 
		    continue;
		if (!ch && len == 1)  /* will ignore NUL if last character */
		    continue;
		break;
	    }
	    if (!len) {	/* entire string is printable */
		len = PR_MIN(attr->ulValueLen + 1, sizeof valstr);
		PR_snprintf(valstr, len, "%s", attr->pValue);
		PR_LOG(modlog, 4, (fmt_s_qsq_d, 
		       atype, valstr, attr->ulValueLen));
		break;
	    }
	    /* else fall through and treat like a binary buffer */
	}
    case CKA_SERIAL_NUMBER:
    default:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    char * hexBuf;
	    SECItem attrBuf;
	    attrBuf.type = siDERNameBuffer;
	    attrBuf.data = attr->pValue;
	    attrBuf.len  = PR_MIN(attr->ulValueLen, (sizeof valstr)/2);

	    hexBuf = CERT_Hexify(&attrBuf, PR_FALSE);
	    if (hexBuf) {
		PR_LOG(modlog, 4, (fmt_s_s_d, 
		       atype, hexBuf, attr->ulValueLen));
	    	PORT_Free(hexBuf);
		break;
	    }
	    /* else fall through and show only the address. :( */
	}
	PR_LOG(modlog, 4, ("    %s = [0x%p] [%d]", 
	       atype, attr->pValue, attr->ulValueLen));
	break;
    }
}

static void print_template(CK_ATTRIBUTE_PTR templ, CK_ULONG tlen)
{
    CK_ULONG i;
    for (i=0; i<tlen; i++) {
	print_attr_value(&templ[i]);
    }
}

struct nssdbg_prof_str {
    PRUint32 time;
    PRUint32 calls;
    char *function;
};

#define NSSDBG_DEFINE(func) { 0, 0, #func }

struct nssdbg_prof_str nssdbg_prof_data[] = {
#define FUNC_C_INITIALIZE 0
    NSSDBG_DEFINE(C_Initialize),
#define FUNC_C_FINALIZE 1
    NSSDBG_DEFINE(C_Finalize),
#define FUNC_C_GETINFO 2
    NSSDBG_DEFINE(C_GetInfo),
#define FUNC_C_GETFUNCITONLIST 3
    NSSDBG_DEFINE(C_GetFunctionList),
#define FUNC_C_GETSLOTLIST 4
    NSSDBG_DEFINE(C_GetSlotList),
#define FUNC_C_GETSLOTINFO 5
    NSSDBG_DEFINE(C_GetSlotInfo),
#define FUNC_C_GETTOKENINFO 6
    NSSDBG_DEFINE(C_GetTokenInfo),
#define FUNC_C_GETMECHANISMLIST 7
    NSSDBG_DEFINE(C_GetMechanismList),
#define FUNC_C_GETMECHANISMINFO 8
    NSSDBG_DEFINE(C_GetMechanismInfo),
#define FUNC_C_INITTOKEN 9
    NSSDBG_DEFINE(C_InitToken),
#define FUNC_C_INITPIN 10
    NSSDBG_DEFINE(C_InitPIN),
#define FUNC_C_SETPIN 11
    NSSDBG_DEFINE(C_SetPIN),
#define FUNC_C_OPENSESSION 12
    NSSDBG_DEFINE(C_OpenSession),
#define FUNC_C_CLOSESESSION 13
    NSSDBG_DEFINE(C_CloseSession),
#define FUNC_C_CLOSEALLSESSIONS 14
    NSSDBG_DEFINE(C_CloseAllSessions),
#define FUNC_C_GETSESSIONINFO 15
    NSSDBG_DEFINE(C_GetSessionInfo),
#define FUNC_C_GETOPERATIONSTATE 16
    NSSDBG_DEFINE(C_GetOperationState),
#define FUNC_C_SETOPERATIONSTATE 17
    NSSDBG_DEFINE(C_SetOperationState),
#define FUNC_C_LOGIN 18
    NSSDBG_DEFINE(C_Login),
#define FUNC_C_LOGOUT 19
    NSSDBG_DEFINE(C_Logout),
#define FUNC_C_CREATEOBJECT 20
    NSSDBG_DEFINE(C_CreateObject),
#define FUNC_C_COPYOBJECT 21
    NSSDBG_DEFINE(C_CopyObject),
#define FUNC_C_DESTROYOBJECT 22
    NSSDBG_DEFINE(C_DestroyObject),
#define FUNC_C_GETOBJECTSIZE  23
    NSSDBG_DEFINE(C_GetObjectSize),
#define FUNC_C_GETATTRIBUTEVALUE 24
    NSSDBG_DEFINE(C_GetAttributeValue),
#define FUNC_C_SETATTRIBUTEVALUE 25
    NSSDBG_DEFINE(C_SetAttributeValue),
#define FUNC_C_FINDOBJECTSINIT 26
    NSSDBG_DEFINE(C_FindObjectsInit),
#define FUNC_C_FINDOBJECTS 27
    NSSDBG_DEFINE(C_FindObjects),
#define FUNC_C_FINDOBJECTSFINAL 28
    NSSDBG_DEFINE(C_FindObjectsFinal),
#define FUNC_C_ENCRYPTINIT 29
    NSSDBG_DEFINE(C_EncryptInit),
#define FUNC_C_ENCRYPT 30
    NSSDBG_DEFINE(C_Encrypt),
#define FUNC_C_ENCRYPTUPDATE 31
    NSSDBG_DEFINE(C_EncryptUpdate),
#define FUNC_C_ENCRYPTFINAL 32
    NSSDBG_DEFINE(C_EncryptFinal),
#define FUNC_C_DECRYPTINIT 33
    NSSDBG_DEFINE(C_DecryptInit),
#define FUNC_C_DECRYPT 34
    NSSDBG_DEFINE(C_Decrypt),
#define FUNC_C_DECRYPTUPDATE 35
    NSSDBG_DEFINE(C_DecryptUpdate),
#define FUNC_C_DECRYPTFINAL 36
    NSSDBG_DEFINE(C_DecryptFinal),
#define FUNC_C_DIGESTINIT 37
    NSSDBG_DEFINE(C_DigestInit),
#define FUNC_C_DIGEST 38
    NSSDBG_DEFINE(C_Digest),
#define FUNC_C_DIGESTUPDATE 39
    NSSDBG_DEFINE(C_DigestUpdate),
#define FUNC_C_DIGESTKEY 40
    NSSDBG_DEFINE(C_DigestKey),
#define FUNC_C_DIGESTFINAL 41
    NSSDBG_DEFINE(C_DigestFinal),
#define FUNC_C_SIGNINIT 42
    NSSDBG_DEFINE(C_SignInit),
#define FUNC_C_SIGN 43
    NSSDBG_DEFINE(C_Sign),
#define FUNC_C_SIGNUPDATE 44
    NSSDBG_DEFINE(C_SignUpdate),
#define FUNC_C_SIGNFINAL 45
    NSSDBG_DEFINE(C_SignFinal),
#define FUNC_C_SIGNRECOVERINIT 46
    NSSDBG_DEFINE(C_SignRecoverInit),
#define FUNC_C_SIGNRECOVER 47
    NSSDBG_DEFINE(C_SignRecover),
#define FUNC_C_VERIFYINIT 48
    NSSDBG_DEFINE(C_VerifyInit),
#define FUNC_C_VERIFY 49
    NSSDBG_DEFINE(C_Verify),
#define FUNC_C_VERIFYUPDATE 50
    NSSDBG_DEFINE(C_VerifyUpdate),
#define FUNC_C_VERIFYFINAL 51
    NSSDBG_DEFINE(C_VerifyFinal),
#define FUNC_C_VERIFYRECOVERINIT 52
    NSSDBG_DEFINE(C_VerifyRecoverInit),
#define FUNC_C_VERIFYRECOVER 53
    NSSDBG_DEFINE(C_VerifyRecover),
#define FUNC_C_DIGESTENCRYPTUPDATE 54
    NSSDBG_DEFINE(C_DigestEncryptUpdate),
#define FUNC_C_DECRYPTDIGESTUPDATE 55
    NSSDBG_DEFINE(C_DecryptDigestUpdate),
#define FUNC_C_SIGNENCRYPTUPDATE 56
    NSSDBG_DEFINE(C_SignEncryptUpdate),
#define FUNC_C_DECRYPTVERIFYUPDATE 57
    NSSDBG_DEFINE(C_DecryptVerifyUpdate),
#define FUNC_C_GENERATEKEY 58
    NSSDBG_DEFINE(C_GenerateKey),
#define FUNC_C_GENERATEKEYPAIR 59
    NSSDBG_DEFINE(C_GenerateKeyPair),
#define FUNC_C_WRAPKEY 60
    NSSDBG_DEFINE(C_WrapKey),
#define FUNC_C_UNWRAPKEY 61
    NSSDBG_DEFINE(C_UnWrapKey),
#define FUNC_C_DERIVEKEY 62 
    NSSDBG_DEFINE(C_DeriveKey),
#define FUNC_C_SEEDRANDOM 63
    NSSDBG_DEFINE(C_SeedRandom),
#define FUNC_C_GENERATERANDOM 64
    NSSDBG_DEFINE(C_GenerateRandom),
#define FUNC_C_GETFUNCTIONSTATUS 65
    NSSDBG_DEFINE(C_GetFunctionStatus),
#define FUNC_C_CANCELFUNCTION 66
    NSSDBG_DEFINE(C_CancelFunction),
#define FUNC_C_WAITFORSLOTEVENT 67
    NSSDBG_DEFINE(C_WaitForSlotEvent)
};

int nssdbg_prof_size = sizeof(nssdbg_prof_data)/sizeof(nssdbg_prof_data[0]);
    

static void nssdbg_finish_time(PRInt32 fun_number, PRIntervalTime start)
{
    PRIntervalTime ival;
    PRIntervalTime end = PR_IntervalNow();

    ival = end-start;
    /* sigh, lie to PRAtomic add and say we are using signed values */
    PR_ATOMIC_ADD((PRInt32 *)&nssdbg_prof_data[fun_number].time, (PRInt32)ival);
}

static void nssdbg_start_time(PRInt32 fun_number, PRIntervalTime *start)
{
    PR_ATOMIC_INCREMENT((PRInt32 *)&nssdbg_prof_data[fun_number].calls);
    *start = PR_IntervalNow();
}

#define COMMON_DEFINITIONS \
    CK_RV rv; \
    PRIntervalTime start

CK_RV NSSDBGC_Initialize(
  CK_VOID_PTR pInitArgs
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Initialize"));
    PR_LOG(modlog, 3, ("  pInitArgs = 0x%p", pInitArgs));
    nssdbg_start_time(FUNC_C_INITIALIZE,&start);
    rv = module_functions->C_Initialize(pInitArgs);
    nssdbg_finish_time(FUNC_C_INITIALIZE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Finalize(
  CK_VOID_PTR pReserved
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Finalize"));
    PR_LOG(modlog, 3, ("  pReserved = 0x%p", pReserved));
    nssdbg_start_time(FUNC_C_FINALIZE,&start);
    rv = module_functions->C_Finalize(pReserved);
    nssdbg_finish_time(FUNC_C_FINALIZE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetInfo(
  CK_INFO_PTR pInfo
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetInfo"));
    PR_LOG(modlog, 3, (fmt_pInfo, pInfo));
    nssdbg_start_time(FUNC_C_GETINFO,&start);
    rv = module_functions->C_GetInfo(pInfo);
    nssdbg_finish_time(FUNC_C_GETINFO,start);
    if (rv == CKR_OK) {
	PR_LOG(modlog, 4, ("  cryptoki version: %d.%d", 
			   pInfo->cryptokiVersion.major,
			   pInfo->cryptokiVersion.minor));
	PR_LOG(modlog, 4, (fmt_manufacturerID, pInfo->manufacturerID));
	PR_LOG(modlog, 4, ("  library description = \"%.32s\"", 
	                   pInfo->libraryDescription));
	PR_LOG(modlog, 4, ("  library version: %d.%d", 
			   pInfo->libraryVersion.major,
			   pInfo->libraryVersion.minor));
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetFunctionList(
  CK_FUNCTION_LIST_PTR_PTR ppFunctionList
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetFunctionList"));
    PR_LOG(modlog, 3, ("  ppFunctionList = 0x%p", ppFunctionList));
    nssdbg_start_time(FUNC_C_GETFUNCITONLIST,&start);
    rv = module_functions->C_GetFunctionList(ppFunctionList);
    nssdbg_finish_time(FUNC_C_GETFUNCITONLIST,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetSlotList(
  CK_BBOOL       tokenPresent,
  CK_SLOT_ID_PTR pSlotList,
  CK_ULONG_PTR   pulCount
)
{
    COMMON_DEFINITIONS;

    CK_ULONG i;
    PR_LOG(modlog, 1, ("C_GetSlotList"));
    PR_LOG(modlog, 3, ("  tokenPresent = 0x%x", tokenPresent));
    PR_LOG(modlog, 3, ("  pSlotList = 0x%p", pSlotList));
    PR_LOG(modlog, 3, (fmt_pulCount, pulCount));
    nssdbg_start_time(FUNC_C_GETSLOTLIST,&start);
    rv = module_functions->C_GetSlotList(tokenPresent, pSlotList, pulCount);
    nssdbg_finish_time(FUNC_C_GETSLOTLIST,start);
    PR_LOG(modlog, 4, (fmt_spulCount, *pulCount));
    if (pSlotList) {
	for (i=0; i<*pulCount; i++) {
	    PR_LOG(modlog, 4, ("  slotID[%d] = %x", i, pSlotList[i]));
	}
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetSlotInfo(
  CK_SLOT_ID       slotID,
  CK_SLOT_INFO_PTR pInfo
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetSlotInfo"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, (fmt_pInfo, pInfo));
    nssdbg_start_time(FUNC_C_GETSLOTINFO,&start);
    rv = module_functions->C_GetSlotInfo(slotID, pInfo);
    nssdbg_finish_time(FUNC_C_GETSLOTINFO,start);
    if (rv == CKR_OK) {
	PR_LOG(modlog, 4, ("  slotDescription = \"%.64s\"", 
	                   pInfo->slotDescription));
	PR_LOG(modlog, 4, (fmt_manufacturerID, pInfo->manufacturerID));
	PR_LOG(modlog, 4, ("  flags = %s %s %s",
	    pInfo->flags & CKF_HW_SLOT          ? "CKF_HW_SLOT" : "",
	    pInfo->flags & CKF_REMOVABLE_DEVICE ? "CKF_REMOVABLE_DEVICE" : "",
	    pInfo->flags & CKF_TOKEN_PRESENT    ? "CKF_TOKEN_PRESENT" : ""));
	PR_LOG(modlog, 4, (fmt_hwVersion, 
			    pInfo->hardwareVersion.major,
			    pInfo->hardwareVersion.minor));
	PR_LOG(modlog, 4, (fmt_fwVersion, 
			    pInfo->firmwareVersion.major,
			    pInfo->firmwareVersion.minor));
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetTokenInfo(
  CK_SLOT_ID        slotID,
  CK_TOKEN_INFO_PTR pInfo
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetTokenInfo"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, (fmt_pInfo, pInfo));
    nssdbg_start_time(FUNC_C_GETTOKENINFO,&start);
    rv = module_functions->C_GetTokenInfo(slotID, pInfo);
    nssdbg_finish_time(FUNC_C_GETTOKENINFO,start);
    if (rv == CKR_OK) {
    	PR_LOG(modlog, 4, ("  label = \"%.32s\"", pInfo->label));
	PR_LOG(modlog, 4, (fmt_manufacturerID, pInfo->manufacturerID));
    	PR_LOG(modlog, 4, ("  model = \"%.16s\"", pInfo->model));
    	PR_LOG(modlog, 4, ("  serial = \"%.16s\"", pInfo->serialNumber));
	PR_LOG(modlog, 4, ("  flags = %s %s %s %s",
	    pInfo->flags & CKF_RNG             ? "CKF_RNG" : "",
	    pInfo->flags & CKF_WRITE_PROTECTED ? "CKF_WRITE_PROTECTED" : "",
	    pInfo->flags & CKF_LOGIN_REQUIRED  ? "CKF_LOGIN_REQUIRED" : "",
	    pInfo->flags & CKF_USER_PIN_INITIALIZED ? "CKF_USER_PIN_INIT" : ""));
	PR_LOG(modlog, 4, ("  maxSessions = %u, Sessions = %u", 
	                   pInfo->ulMaxSessionCount, pInfo->ulSessionCount));
	PR_LOG(modlog, 4, ("  maxRwSessions = %u, RwSessions = %u", 
	                   pInfo->ulMaxRwSessionCount, 
			   pInfo->ulRwSessionCount));
	/* ignore Max & Min Pin Len, Public and Private Memory */
	PR_LOG(modlog, 4, (fmt_hwVersion, 
			    pInfo->hardwareVersion.major,
			    pInfo->hardwareVersion.minor));
	PR_LOG(modlog, 4, (fmt_fwVersion, 
			    pInfo->firmwareVersion.major,
			    pInfo->firmwareVersion.minor));
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetMechanismList(
  CK_SLOT_ID            slotID,
  CK_MECHANISM_TYPE_PTR pMechanismList,
  CK_ULONG_PTR          pulCount
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetMechanismList"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, ("  pMechanismList = 0x%p", pMechanismList));
    PR_LOG(modlog, 3, (fmt_pulCount, pulCount));
    nssdbg_start_time(FUNC_C_GETMECHANISMLIST,&start);
    rv = module_functions->C_GetMechanismList(slotID,
                                 pMechanismList,
                                 pulCount);
    nssdbg_finish_time(FUNC_C_GETMECHANISMLIST,start);
    PR_LOG(modlog, 4, (fmt_spulCount, *pulCount));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetMechanismInfo(
  CK_SLOT_ID            slotID,
  CK_MECHANISM_TYPE     type,
  CK_MECHANISM_INFO_PTR pInfo
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetMechanismInfo"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, ("  type = 0x%x", type));
    PR_LOG(modlog, 3, (fmt_pInfo, pInfo));
    nssdbg_start_time(FUNC_C_GETMECHANISMINFO,&start);
    rv = module_functions->C_GetMechanismInfo(slotID,
                                 type,
                                 pInfo);
    nssdbg_finish_time(FUNC_C_GETMECHANISMINFO,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_InitToken(
  CK_SLOT_ID  slotID,
  CK_CHAR_PTR pPin,
  CK_ULONG    ulPinLen,
  CK_CHAR_PTR pLabel
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_InitToken"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, (fmt_pPin, pPin));
    PR_LOG(modlog, 3, (fmt_ulPinLen, ulPinLen));
    PR_LOG(modlog, 3, ("  pLabel = 0x%p", pLabel));
    nssdbg_start_time(FUNC_C_INITTOKEN,&start);
    rv = module_functions->C_InitToken(slotID,
                                 pPin,
                                 ulPinLen,
                                 pLabel);
    nssdbg_finish_time(FUNC_C_INITTOKEN,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_InitPIN(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR       pPin,
  CK_ULONG          ulPinLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_InitPIN"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPin, pPin));
    PR_LOG(modlog, 3, (fmt_ulPinLen, ulPinLen));
    nssdbg_start_time(FUNC_C_INITPIN,&start);
    rv = module_functions->C_InitPIN(hSession,
                                 pPin,
                                 ulPinLen);
    nssdbg_finish_time(FUNC_C_INITPIN,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SetPIN(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR       pOldPin,
  CK_ULONG          ulOldLen,
  CK_CHAR_PTR       pNewPin,
  CK_ULONG          ulNewLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SetPIN"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  pOldPin = 0x%p", pOldPin));
    PR_LOG(modlog, 3, ("  ulOldLen = %d", ulOldLen));
    PR_LOG(modlog, 3, ("  pNewPin = 0x%p", pNewPin));
    PR_LOG(modlog, 3, ("  ulNewLen = %d", ulNewLen));
    nssdbg_start_time(FUNC_C_SETPIN,&start);
    rv = module_functions->C_SetPIN(hSession,
                                 pOldPin,
                                 ulOldLen,
                                 pNewPin,
                                 ulNewLen);
    nssdbg_finish_time(FUNC_C_SETPIN,start);
    log_rv(rv);
    return rv;
}

static PRUint32 numOpenSessions = 0;
static PRUint32 maxOpenSessions = 0;

CK_RV NSSDBGC_OpenSession(
  CK_SLOT_ID            slotID,
  CK_FLAGS              flags,
  CK_VOID_PTR           pApplication,
  CK_NOTIFY             Notify,
  CK_SESSION_HANDLE_PTR phSession
)
{
    COMMON_DEFINITIONS;

    PR_ATOMIC_INCREMENT((PRInt32 *)&numOpenSessions);
    maxOpenSessions = PR_MAX(numOpenSessions, maxOpenSessions);
    PR_LOG(modlog, 1, ("C_OpenSession"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    PR_LOG(modlog, 3, (fmt_flags, flags));
    PR_LOG(modlog, 3, ("  pApplication = 0x%p", pApplication));
    PR_LOG(modlog, 3, ("  Notify = 0x%x", Notify));
    PR_LOG(modlog, 3, ("  phSession = 0x%p", phSession));
    nssdbg_start_time(FUNC_C_OPENSESSION,&start);
    rv = module_functions->C_OpenSession(slotID,
                                 flags,
                                 pApplication,
                                 Notify,
                                 phSession);
    nssdbg_finish_time(FUNC_C_OPENSESSION,start);
    log_handle(4, "  *phSession = 0x%x", *phSession);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_CloseSession(
  CK_SESSION_HANDLE hSession
)
{
    COMMON_DEFINITIONS;

    PR_ATOMIC_DECREMENT((PRInt32 *)&numOpenSessions);
    PR_LOG(modlog, 1, ("C_CloseSession"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_CLOSESESSION,&start);
    rv = module_functions->C_CloseSession(hSession);
    nssdbg_finish_time(FUNC_C_CLOSESESSION,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_CloseAllSessions(
  CK_SLOT_ID slotID
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_CloseAllSessions"));
    PR_LOG(modlog, 3, (fmt_slotID, slotID));
    nssdbg_start_time(FUNC_C_CLOSEALLSESSIONS,&start);
    rv = module_functions->C_CloseAllSessions(slotID);
    nssdbg_finish_time(FUNC_C_CLOSEALLSESSIONS,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetSessionInfo(
  CK_SESSION_HANDLE   hSession,
  CK_SESSION_INFO_PTR pInfo
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetSessionInfo"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pInfo, pInfo));
    nssdbg_start_time(FUNC_C_GETSESSIONINFO,&start);
    rv = module_functions->C_GetSessionInfo(hSession,
                                 pInfo);
    nssdbg_finish_time(FUNC_C_GETSESSIONINFO,start);
    if (rv == CKR_OK) {
	PR_LOG(modlog, 4, (fmt_slotID, pInfo->slotID));
	log_state(pInfo->state);
	PR_LOG(modlog, 4, ("  flags = %s %s",
	    pInfo->flags & CKF_RW_SESSION     ? "CKF_RW_SESSION" : "",
	    pInfo->flags & CKF_SERIAL_SESSION ? "CKF_SERIAL_SESSION" : ""));
	PR_LOG(modlog, 4, ("  deviceError = 0x%x", pInfo->ulDeviceError));
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetOperationState(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pOperationState,
  CK_ULONG_PTR      pulOperationStateLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetOperationState"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pOperationState, pOperationState));
    PR_LOG(modlog, 3, ("  pulOperationStateLen = 0x%p", pulOperationStateLen));
    nssdbg_start_time(FUNC_C_GETOPERATIONSTATE,&start);
    rv = module_functions->C_GetOperationState(hSession,
                                 pOperationState,
                                 pulOperationStateLen);
    nssdbg_finish_time(FUNC_C_GETOPERATIONSTATE,start);
    PR_LOG(modlog, 4, ("  *pulOperationStateLen = 0x%x", *pulOperationStateLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SetOperationState(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR      pOperationState,
  CK_ULONG         ulOperationStateLen,
  CK_OBJECT_HANDLE hEncryptionKey,
  CK_OBJECT_HANDLE hAuthenticationKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SetOperationState"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pOperationState, pOperationState));
    PR_LOG(modlog, 3, ("  ulOperationStateLen = %d", ulOperationStateLen));
    log_handle(3, "  hEncryptionKey = 0x%x", hEncryptionKey);
    log_handle(3, "  hAuthenticationKey = 0x%x", hAuthenticationKey);
    nssdbg_start_time(FUNC_C_SETOPERATIONSTATE,&start);
    rv = module_functions->C_SetOperationState(hSession,
                                 pOperationState,
                                 ulOperationStateLen,
                                 hEncryptionKey,
                                 hAuthenticationKey);
    nssdbg_finish_time(FUNC_C_SETOPERATIONSTATE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Login(
  CK_SESSION_HANDLE hSession,
  CK_USER_TYPE      userType,
  CK_CHAR_PTR       pPin,
  CK_ULONG          ulPinLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Login"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  userType = 0x%x", userType));
    PR_LOG(modlog, 3, (fmt_pPin, pPin));
    PR_LOG(modlog, 3, (fmt_ulPinLen, ulPinLen));
    nssdbg_start_time(FUNC_C_LOGIN,&start);
    rv = module_functions->C_Login(hSession,
                                 userType,
                                 pPin,
                                 ulPinLen);
    nssdbg_finish_time(FUNC_C_LOGIN,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Logout(
  CK_SESSION_HANDLE hSession
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Logout"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_LOGOUT,&start);
    rv = module_functions->C_Logout(hSession);
    nssdbg_finish_time(FUNC_C_LOGOUT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_CreateObject(
  CK_SESSION_HANDLE    hSession,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phObject
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_CreateObject"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    PR_LOG(modlog, 3, (fmt_phObject, phObject));
    print_template(pTemplate, ulCount);
    nssdbg_start_time(FUNC_C_CREATEOBJECT,&start);
    rv = module_functions->C_CreateObject(hSession,
                                 pTemplate,
                                 ulCount,
                                 phObject);
    nssdbg_finish_time(FUNC_C_CREATEOBJECT,start);
    log_handle(4, "  *phObject = 0x%x", *phObject);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_CopyObject(
  CK_SESSION_HANDLE    hSession,
  CK_OBJECT_HANDLE     hObject,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phNewObject
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_CopyObject"));
    log_handle(3, fmt_hSession, hSession);
    log_handle(3, fmt_hObject, hObject);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    PR_LOG(modlog, 3, ("  phNewObject = 0x%p", phNewObject));
    print_template(pTemplate, ulCount);
    nssdbg_start_time(FUNC_C_COPYOBJECT,&start);
    rv = module_functions->C_CopyObject(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount,
                                 phNewObject);
    nssdbg_finish_time(FUNC_C_COPYOBJECT,start);
    log_handle(4, "  *phNewObject = 0x%x", *phNewObject);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DestroyObject(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DestroyObject"));
    log_handle(3, fmt_hSession, hSession);
    log_handle(3, fmt_hObject, hObject);
    nssdbg_start_time(FUNC_C_DESTROYOBJECT,&start);
    rv = module_functions->C_DestroyObject(hSession,
                                 hObject);
    nssdbg_finish_time(FUNC_C_DESTROYOBJECT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetObjectSize(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ULONG_PTR      pulSize
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetObjectSize"));
    log_handle(3, fmt_hSession, hSession);
    log_handle(3, fmt_hObject, hObject);
    PR_LOG(modlog, 3, ("  pulSize = 0x%p", pulSize));
    nssdbg_start_time(FUNC_C_GETOBJECTSIZE,&start);
    rv = module_functions->C_GetObjectSize(hSession,
                                 hObject,
                                 pulSize);
    nssdbg_finish_time(FUNC_C_GETOBJECTSIZE,start);
    PR_LOG(modlog, 4, ("  *pulSize = 0x%x", *pulSize));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetAttributeValue(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetAttributeValue"));
    log_handle(3, fmt_hSession, hSession);
    log_handle(3, fmt_hObject, hObject);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    nssdbg_start_time(FUNC_C_GETATTRIBUTEVALUE,&start);
    rv = module_functions->C_GetAttributeValue(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(FUNC_C_GETATTRIBUTEVALUE,start);
    print_template(pTemplate, ulCount);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SetAttributeValue(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SetAttributeValue"));
    log_handle(3, fmt_hSession, hSession);
    log_handle(3, fmt_hObject, hObject);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    print_template(pTemplate, ulCount);
    nssdbg_start_time(FUNC_C_SETATTRIBUTEVALUE,&start);
    rv = module_functions->C_SetAttributeValue(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(FUNC_C_SETATTRIBUTEVALUE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_FindObjectsInit(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_FindObjectsInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    print_template(pTemplate, ulCount);
    nssdbg_start_time(FUNC_C_FINDOBJECTSINIT,&start);
    rv = module_functions->C_FindObjectsInit(hSession,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(FUNC_C_FINDOBJECTSINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_FindObjects(
  CK_SESSION_HANDLE    hSession,
  CK_OBJECT_HANDLE_PTR phObject,
  CK_ULONG             ulMaxObjectCount,
  CK_ULONG_PTR         pulObjectCount
)
{
    COMMON_DEFINITIONS;
    CK_ULONG i;

    PR_LOG(modlog, 1, ("C_FindObjects"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_phObject, phObject));
    PR_LOG(modlog, 3, ("  ulMaxObjectCount = %d", ulMaxObjectCount));
    PR_LOG(modlog, 3, ("  pulObjectCount = 0x%p", pulObjectCount));
    nssdbg_start_time(FUNC_C_FINDOBJECTS,&start);
    rv = module_functions->C_FindObjects(hSession,
                                 phObject,
                                 ulMaxObjectCount,
                                 pulObjectCount);
    nssdbg_finish_time(FUNC_C_FINDOBJECTS,start);
    PR_LOG(modlog, 4, ("  *pulObjectCount = 0x%x", *pulObjectCount));
    for (i=0; i<*pulObjectCount; i++) {
	PR_LOG(modlog, 4, ("  phObject[%d] = 0x%x%s", i, phObject[i],
	       phObject[i] ? "" : fmt_invalid_handle));
    }
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_FindObjectsFinal(
  CK_SESSION_HANDLE hSession
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_FindObjectsFinal"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_FINDOBJECTSFINAL,&start);
    rv = module_functions->C_FindObjectsFinal(hSession);
    nssdbg_finish_time(FUNC_C_FINDOBJECTSFINAL,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_EncryptInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_EncryptInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_ENCRYPTINIT,&start);
    rv = module_functions->C_EncryptInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_ENCRYPTINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Encrypt(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pEncryptedData,
  CK_ULONG_PTR      pulEncryptedDataLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Encrypt"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_ulDataLen, ulDataLen));
    PR_LOG(modlog, 3, (fmt_pEncryptedData, pEncryptedData));
    PR_LOG(modlog, 3, ("  pulEncryptedDataLen = 0x%p", pulEncryptedDataLen));
    nssdbg_start_time(FUNC_C_ENCRYPT,&start);
    rv = module_functions->C_Encrypt(hSession,
                                 pData,
                                 ulDataLen,
                                 pEncryptedData,
                                 pulEncryptedDataLen);
    nssdbg_finish_time(FUNC_C_ENCRYPT,start);
    PR_LOG(modlog, 4, ("  *pulEncryptedDataLen = 0x%x", *pulEncryptedDataLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_EncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_EncryptUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_pulEncryptedPartLen, pulEncryptedPartLen));
    nssdbg_start_time(FUNC_C_ENCRYPTUPDATE,&start);
    rv = module_functions->C_EncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(FUNC_C_ENCRYPTUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulEncryptedPartLen, *pulEncryptedPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_EncryptFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pLastEncryptedPart,
  CK_ULONG_PTR      pulLastEncryptedPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_EncryptFinal"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  pLastEncryptedPart = 0x%p", pLastEncryptedPart));
    PR_LOG(modlog, 3, ("  pulLastEncryptedPartLen = 0x%p", pulLastEncryptedPartLen));
    nssdbg_start_time(FUNC_C_ENCRYPTFINAL,&start);
    rv = module_functions->C_EncryptFinal(hSession,
                                 pLastEncryptedPart,
                                 pulLastEncryptedPartLen);
    nssdbg_finish_time(FUNC_C_ENCRYPTFINAL,start);
    PR_LOG(modlog, 4, ("  *pulLastEncryptedPartLen = 0x%x", *pulLastEncryptedPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DecryptInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DecryptInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_DECRYPTINIT,&start);
    rv = module_functions->C_DecryptInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_DECRYPTINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Decrypt(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedData,
  CK_ULONG          ulEncryptedDataLen,
  CK_BYTE_PTR       pData,
  CK_ULONG_PTR      pulDataLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Decrypt"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pEncryptedData, pEncryptedData));
    PR_LOG(modlog, 3, ("  ulEncryptedDataLen = %d", ulEncryptedDataLen));
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_pulDataLen, pulDataLen));
    nssdbg_start_time(FUNC_C_DECRYPT,&start);
    rv = module_functions->C_Decrypt(hSession,
                                 pEncryptedData,
                                 ulEncryptedDataLen,
                                 pData,
                                 pulDataLen);
    nssdbg_finish_time(FUNC_C_DECRYPT,start);
    PR_LOG(modlog, 4, (fmt_spulDataLen, *pulDataLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DecryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DecryptUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_ulEncryptedPartLen, ulEncryptedPartLen));
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_pulPartLen, pulPartLen));
    nssdbg_start_time(FUNC_C_DECRYPTUPDATE,&start);
    rv = module_functions->C_DecryptUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(FUNC_C_DECRYPTUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulPartLen, *pulPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DecryptFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pLastPart,
  CK_ULONG_PTR      pulLastPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DecryptFinal"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  pLastPart = 0x%p", pLastPart));
    PR_LOG(modlog, 3, ("  pulLastPartLen = 0x%p", pulLastPartLen));
    nssdbg_start_time(FUNC_C_DECRYPTFINAL,&start);
    rv = module_functions->C_DecryptFinal(hSession,
                                 pLastPart,
                                 pulLastPartLen);
    nssdbg_finish_time(FUNC_C_DECRYPTFINAL,start);
    PR_LOG(modlog, 4, ("  *pulLastPartLen = 0x%x", *pulLastPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DigestInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DigestInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_DIGESTINIT,&start);
    rv = module_functions->C_DigestInit(hSession,
                                 pMechanism);
    nssdbg_finish_time(FUNC_C_DIGESTINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Digest(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pDigest,
  CK_ULONG_PTR      pulDigestLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Digest"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_ulDataLen, ulDataLen));
    PR_LOG(modlog, 3, (fmt_pDigest, pDigest));
    PR_LOG(modlog, 3, (fmt_pulDigestLen, pulDigestLen));
    nssdbg_start_time(FUNC_C_DIGEST,&start);
    rv = module_functions->C_Digest(hSession,
                                 pData,
                                 ulDataLen,
                                 pDigest,
                                 pulDigestLen);
    nssdbg_finish_time(FUNC_C_DIGEST,start);
    PR_LOG(modlog, 4, (fmt_spulDigestLen, *pulDigestLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DigestUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DigestUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    nssdbg_start_time(FUNC_C_DIGESTUPDATE,&start);
    rv = module_functions->C_DigestUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(FUNC_C_DIGESTUPDATE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DigestKey(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DigestKey"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_DIGESTKEY,&start);
    rv = module_functions->C_DigestKey(hSession,
                                 hKey);
    nssdbg_finish_time(FUNC_C_DIGESTKEY,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DigestFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pDigest,
  CK_ULONG_PTR      pulDigestLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DigestFinal"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pDigest, pDigest));
    PR_LOG(modlog, 3, (fmt_pulDigestLen, pulDigestLen));
    nssdbg_start_time(FUNC_C_DIGESTFINAL,&start);
    rv = module_functions->C_DigestFinal(hSession,
                                 pDigest,
                                 pulDigestLen);
    nssdbg_finish_time(FUNC_C_DIGESTFINAL,start);
    PR_LOG(modlog, 4, (fmt_spulDigestLen, *pulDigestLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_SIGNINIT,&start);
    rv = module_functions->C_SignInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_SIGNINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Sign(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Sign"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_ulDataLen, ulDataLen));
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_pulSignatureLen, pulSignatureLen));
    nssdbg_start_time(FUNC_C_SIGN,&start);
    rv = module_functions->C_Sign(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(FUNC_C_SIGN,start);
    PR_LOG(modlog, 4, (fmt_spulSignatureLen, *pulSignatureLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    nssdbg_start_time(FUNC_C_SIGNUPDATE,&start);
    rv = module_functions->C_SignUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(FUNC_C_SIGNUPDATE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignFinal"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_pulSignatureLen, pulSignatureLen));
    nssdbg_start_time(FUNC_C_SIGNFINAL,&start);
    rv = module_functions->C_SignFinal(hSession,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(FUNC_C_SIGNFINAL,start);
    PR_LOG(modlog, 4, (fmt_spulSignatureLen, *pulSignatureLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignRecoverInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignRecoverInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_SIGNRECOVERINIT,&start);
    rv = module_functions->C_SignRecoverInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_SIGNRECOVERINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignRecover(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignRecover"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_ulDataLen, ulDataLen));
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_pulSignatureLen, pulSignatureLen));
    nssdbg_start_time(FUNC_C_SIGNRECOVER,&start);
    rv = module_functions->C_SignRecover(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(FUNC_C_SIGNRECOVER,start);
    PR_LOG(modlog, 4, (fmt_spulSignatureLen, *pulSignatureLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_VerifyInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_VerifyInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_VERIFYINIT,&start);
    rv = module_functions->C_VerifyInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_VERIFYINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_Verify(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_Verify"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_ulDataLen, ulDataLen));
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_ulSignatureLen, ulSignatureLen));
    nssdbg_start_time(FUNC_C_VERIFY,&start);
    rv = module_functions->C_Verify(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 ulSignatureLen);
    nssdbg_finish_time(FUNC_C_VERIFY,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_VerifyUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_VerifyUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    nssdbg_start_time(FUNC_C_VERIFYUPDATE,&start);
    rv = module_functions->C_VerifyUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(FUNC_C_VERIFYUPDATE,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_VerifyFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_VerifyFinal"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_ulSignatureLen, ulSignatureLen));
    nssdbg_start_time(FUNC_C_VERIFYFINAL,&start);
    rv = module_functions->C_VerifyFinal(hSession,
                                 pSignature,
                                 ulSignatureLen);
    nssdbg_finish_time(FUNC_C_VERIFYFINAL,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_VerifyRecoverInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_VerifyRecoverInit"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, fmt_hKey, hKey);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_VERIFYRECOVERINIT,&start);
    rv = module_functions->C_VerifyRecoverInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(FUNC_C_VERIFYRECOVERINIT,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_VerifyRecover(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen,
  CK_BYTE_PTR       pData,
  CK_ULONG_PTR      pulDataLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_VerifyRecover"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pSignature, pSignature));
    PR_LOG(modlog, 3, (fmt_ulSignatureLen, ulSignatureLen));
    PR_LOG(modlog, 3, (fmt_pData, pData));
    PR_LOG(modlog, 3, (fmt_pulDataLen, pulDataLen));
    nssdbg_start_time(FUNC_C_VERIFYRECOVER,&start);
    rv = module_functions->C_VerifyRecover(hSession,
                                 pSignature,
                                 ulSignatureLen,
                                 pData,
                                 pulDataLen);
    nssdbg_finish_time(FUNC_C_VERIFYRECOVER,start);
    PR_LOG(modlog, 4, (fmt_spulDataLen, *pulDataLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DigestEncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DigestEncryptUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_pulEncryptedPartLen, pulEncryptedPartLen));
    nssdbg_start_time(FUNC_C_DIGESTENCRYPTUPDATE,&start);
    rv = module_functions->C_DigestEncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(FUNC_C_DIGESTENCRYPTUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulEncryptedPartLen, *pulEncryptedPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DecryptDigestUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DecryptDigestUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_ulEncryptedPartLen, ulEncryptedPartLen));
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_pulPartLen, pulPartLen));
    nssdbg_start_time(FUNC_C_DECRYPTDIGESTUPDATE,&start);
    rv = module_functions->C_DecryptDigestUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(FUNC_C_DECRYPTDIGESTUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulPartLen, *pulPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SignEncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SignEncryptUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_ulPartLen, ulPartLen));
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_pulEncryptedPartLen, pulEncryptedPartLen));
    nssdbg_start_time(FUNC_C_SIGNENCRYPTUPDATE,&start);
    rv = module_functions->C_SignEncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(FUNC_C_SIGNENCRYPTUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulEncryptedPartLen, *pulEncryptedPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DecryptVerifyUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DecryptVerifyUpdate"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pEncryptedPart, pEncryptedPart));
    PR_LOG(modlog, 3, (fmt_ulEncryptedPartLen, ulEncryptedPartLen));
    PR_LOG(modlog, 3, (fmt_pPart, pPart));
    PR_LOG(modlog, 3, (fmt_pulPartLen, pulPartLen));
    nssdbg_start_time(FUNC_C_DECRYPTVERIFYUPDATE,&start);
    rv = module_functions->C_DecryptVerifyUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(FUNC_C_DECRYPTVERIFYUPDATE,start);
    PR_LOG(modlog, 4, (fmt_spulPartLen, *pulPartLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GenerateKey(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GenerateKey"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulCount, ulCount));
    PR_LOG(modlog, 3, (fmt_phKey, phKey));
    print_template(pTemplate, ulCount);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_GENERATEKEY,&start);
    rv = module_functions->C_GenerateKey(hSession,
                                 pMechanism,
                                 pTemplate,
                                 ulCount,
                                 phKey);
    nssdbg_finish_time(FUNC_C_GENERATEKEY,start);
    log_handle(4, fmt_sphKey, *phKey);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GenerateKeyPair(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
  CK_ULONG             ulPublicKeyAttributeCount,
  CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
  CK_ULONG             ulPrivateKeyAttributeCount,
  CK_OBJECT_HANDLE_PTR phPublicKey,
  CK_OBJECT_HANDLE_PTR phPrivateKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GenerateKeyPair"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    PR_LOG(modlog, 3, ("  pPublicKeyTemplate = 0x%p", pPublicKeyTemplate));
    PR_LOG(modlog, 3, ("  ulPublicKeyAttributeCount = %d", ulPublicKeyAttributeCount));
    PR_LOG(modlog, 3, ("  pPrivateKeyTemplate = 0x%p", pPrivateKeyTemplate));
    PR_LOG(modlog, 3, ("  ulPrivateKeyAttributeCount = %d", ulPrivateKeyAttributeCount));
    PR_LOG(modlog, 3, ("  phPublicKey = 0x%p", phPublicKey));
    print_template(pPublicKeyTemplate, ulPublicKeyAttributeCount);
    PR_LOG(modlog, 3, ("  phPrivateKey = 0x%p", phPrivateKey));
    print_template(pPrivateKeyTemplate, ulPrivateKeyAttributeCount);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_GENERATEKEYPAIR,&start);
    rv = module_functions->C_GenerateKeyPair(hSession,
                                 pMechanism,
                                 pPublicKeyTemplate,
                                 ulPublicKeyAttributeCount,
                                 pPrivateKeyTemplate,
                                 ulPrivateKeyAttributeCount,
                                 phPublicKey,
                                 phPrivateKey);
    nssdbg_finish_time(FUNC_C_GENERATEKEYPAIR,start);
    log_handle(4, "  *phPublicKey = 0x%x", *phPublicKey);
    log_handle(4, "  *phPrivateKey = 0x%x", *phPrivateKey);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_WrapKey(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hWrappingKey,
  CK_OBJECT_HANDLE  hKey,
  CK_BYTE_PTR       pWrappedKey,
  CK_ULONG_PTR      pulWrappedKeyLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_WrapKey"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, "  hWrappingKey = 0x%x", hWrappingKey);
    log_handle(3, fmt_hKey, hKey);
    PR_LOG(modlog, 3, (fmt_pWrappedKey, pWrappedKey));
    PR_LOG(modlog, 3, ("  pulWrappedKeyLen = 0x%p", pulWrappedKeyLen));
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_WRAPKEY,&start);
    rv = module_functions->C_WrapKey(hSession,
                                 pMechanism,
                                 hWrappingKey,
                                 hKey,
                                 pWrappedKey,
                                 pulWrappedKeyLen);
    nssdbg_finish_time(FUNC_C_WRAPKEY,start);
    PR_LOG(modlog, 4, ("  *pulWrappedKeyLen = 0x%x", *pulWrappedKeyLen));
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_UnwrapKey(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_OBJECT_HANDLE     hUnwrappingKey,
  CK_BYTE_PTR          pWrappedKey,
  CK_ULONG             ulWrappedKeyLen,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_UnwrapKey"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, "  hUnwrappingKey = 0x%x", hUnwrappingKey);
    PR_LOG(modlog, 3, (fmt_pWrappedKey, pWrappedKey));
    PR_LOG(modlog, 3, ("  ulWrappedKeyLen = %d", ulWrappedKeyLen));
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulAttributeCount, ulAttributeCount));
    PR_LOG(modlog, 3, (fmt_phKey, phKey));
    print_template(pTemplate, ulAttributeCount);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_UNWRAPKEY,&start);
    rv = module_functions->C_UnwrapKey(hSession,
                                 pMechanism,
                                 hUnwrappingKey,
                                 pWrappedKey,
                                 ulWrappedKeyLen,
                                 pTemplate,
                                 ulAttributeCount,
                                 phKey);
    nssdbg_finish_time(FUNC_C_UNWRAPKEY,start);
    log_handle(4, fmt_sphKey, *phKey);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_DeriveKey(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_OBJECT_HANDLE     hBaseKey,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_DeriveKey"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, (fmt_pMechanism, pMechanism));
    log_handle(3, "  hBaseKey = 0x%x", hBaseKey);
    PR_LOG(modlog, 3, (fmt_pTemplate, pTemplate));
    PR_LOG(modlog, 3, (fmt_ulAttributeCount, ulAttributeCount));
    PR_LOG(modlog, 3, (fmt_phKey, phKey));
    print_template(pTemplate, ulAttributeCount);
    print_mechanism(pMechanism);
    nssdbg_start_time(FUNC_C_DERIVEKEY,&start);
    rv = module_functions->C_DeriveKey(hSession,
                                 pMechanism,
                                 hBaseKey,
                                 pTemplate,
                                 ulAttributeCount,
                                 phKey);
    nssdbg_finish_time(FUNC_C_DERIVEKEY,start);
    log_handle(4, fmt_sphKey, *phKey);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_SeedRandom(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSeed,
  CK_ULONG          ulSeedLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_SeedRandom"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  pSeed = 0x%p", pSeed));
    PR_LOG(modlog, 3, ("  ulSeedLen = %d", ulSeedLen));
    nssdbg_start_time(FUNC_C_SEEDRANDOM,&start);
    rv = module_functions->C_SeedRandom(hSession,
                                 pSeed,
                                 ulSeedLen);
    nssdbg_finish_time(FUNC_C_SEEDRANDOM,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GenerateRandom(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       RandomData,
  CK_ULONG          ulRandomLen
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GenerateRandom"));
    log_handle(3, fmt_hSession, hSession);
    PR_LOG(modlog, 3, ("  RandomData = 0x%p", RandomData));
    PR_LOG(modlog, 3, ("  ulRandomLen = %d", ulRandomLen));
    nssdbg_start_time(FUNC_C_GENERATERANDOM,&start);
    rv = module_functions->C_GenerateRandom(hSession,
                                 RandomData,
                                 ulRandomLen);
    nssdbg_finish_time(FUNC_C_GENERATERANDOM,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_GetFunctionStatus(
  CK_SESSION_HANDLE hSession
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_GetFunctionStatus"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_GETFUNCTIONSTATUS,&start);
    rv = module_functions->C_GetFunctionStatus(hSession);
    nssdbg_finish_time(FUNC_C_GETFUNCTIONSTATUS,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_CancelFunction(
  CK_SESSION_HANDLE hSession
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_CancelFunction"));
    log_handle(3, fmt_hSession, hSession);
    nssdbg_start_time(FUNC_C_CANCELFUNCTION,&start);
    rv = module_functions->C_CancelFunction(hSession);
    nssdbg_finish_time(FUNC_C_CANCELFUNCTION,start);
    log_rv(rv);
    return rv;
}

CK_RV NSSDBGC_WaitForSlotEvent(
  CK_FLAGS       flags,
  CK_SLOT_ID_PTR pSlot,
  CK_VOID_PTR    pRserved
)
{
    COMMON_DEFINITIONS;

    PR_LOG(modlog, 1, ("C_WaitForSlotEvent"));
    PR_LOG(modlog, 3, (fmt_flags, flags));
    PR_LOG(modlog, 3, ("  pSlot = 0x%p", pSlot));
    PR_LOG(modlog, 3, ("  pRserved = 0x%p", pRserved));
    nssdbg_start_time(FUNC_C_WAITFORSLOTEVENT,&start);
    rv = module_functions->C_WaitForSlotEvent(flags,
                                 pSlot,
                                 pRserved);
    nssdbg_finish_time(FUNC_C_WAITFORSLOTEVENT,start);
    log_rv(rv);
    return rv;
}

CK_FUNCTION_LIST_PTR nss_InsertDeviceLog(
  CK_FUNCTION_LIST_PTR devEPV
)
{
    module_functions = devEPV;
    modlog = PR_NewLogModule("nss_mod_log");
    debug_functions.C_Initialize = NSSDBGC_Initialize;
    debug_functions.C_Finalize = NSSDBGC_Finalize;
    debug_functions.C_GetInfo = NSSDBGC_GetInfo;
    debug_functions.C_GetFunctionList = NSSDBGC_GetFunctionList;
    debug_functions.C_GetSlotList = NSSDBGC_GetSlotList;
    debug_functions.C_GetSlotInfo = NSSDBGC_GetSlotInfo;
    debug_functions.C_GetTokenInfo = NSSDBGC_GetTokenInfo;
    debug_functions.C_GetMechanismList = NSSDBGC_GetMechanismList;
    debug_functions.C_GetMechanismInfo = NSSDBGC_GetMechanismInfo;
    debug_functions.C_InitToken = NSSDBGC_InitToken;
    debug_functions.C_InitPIN = NSSDBGC_InitPIN;
    debug_functions.C_SetPIN = NSSDBGC_SetPIN;
    debug_functions.C_OpenSession = NSSDBGC_OpenSession;
    debug_functions.C_CloseSession = NSSDBGC_CloseSession;
    debug_functions.C_CloseAllSessions = NSSDBGC_CloseAllSessions;
    debug_functions.C_GetSessionInfo = NSSDBGC_GetSessionInfo;
    debug_functions.C_GetOperationState = NSSDBGC_GetOperationState;
    debug_functions.C_SetOperationState = NSSDBGC_SetOperationState;
    debug_functions.C_Login = NSSDBGC_Login;
    debug_functions.C_Logout = NSSDBGC_Logout;
    debug_functions.C_CreateObject = NSSDBGC_CreateObject;
    debug_functions.C_CopyObject = NSSDBGC_CopyObject;
    debug_functions.C_DestroyObject = NSSDBGC_DestroyObject;
    debug_functions.C_GetObjectSize = NSSDBGC_GetObjectSize;
    debug_functions.C_GetAttributeValue = NSSDBGC_GetAttributeValue;
    debug_functions.C_SetAttributeValue = NSSDBGC_SetAttributeValue;
    debug_functions.C_FindObjectsInit = NSSDBGC_FindObjectsInit;
    debug_functions.C_FindObjects = NSSDBGC_FindObjects;
    debug_functions.C_FindObjectsFinal = NSSDBGC_FindObjectsFinal;
    debug_functions.C_EncryptInit = NSSDBGC_EncryptInit;
    debug_functions.C_Encrypt = NSSDBGC_Encrypt;
    debug_functions.C_EncryptUpdate = NSSDBGC_EncryptUpdate;
    debug_functions.C_EncryptFinal = NSSDBGC_EncryptFinal;
    debug_functions.C_DecryptInit = NSSDBGC_DecryptInit;
    debug_functions.C_Decrypt = NSSDBGC_Decrypt;
    debug_functions.C_DecryptUpdate = NSSDBGC_DecryptUpdate;
    debug_functions.C_DecryptFinal = NSSDBGC_DecryptFinal;
    debug_functions.C_DigestInit = NSSDBGC_DigestInit;
    debug_functions.C_Digest = NSSDBGC_Digest;
    debug_functions.C_DigestUpdate = NSSDBGC_DigestUpdate;
    debug_functions.C_DigestKey = NSSDBGC_DigestKey;
    debug_functions.C_DigestFinal = NSSDBGC_DigestFinal;
    debug_functions.C_SignInit = NSSDBGC_SignInit;
    debug_functions.C_Sign = NSSDBGC_Sign;
    debug_functions.C_SignUpdate = NSSDBGC_SignUpdate;
    debug_functions.C_SignFinal = NSSDBGC_SignFinal;
    debug_functions.C_SignRecoverInit = NSSDBGC_SignRecoverInit;
    debug_functions.C_SignRecover = NSSDBGC_SignRecover;
    debug_functions.C_VerifyInit = NSSDBGC_VerifyInit;
    debug_functions.C_Verify = NSSDBGC_Verify;
    debug_functions.C_VerifyUpdate = NSSDBGC_VerifyUpdate;
    debug_functions.C_VerifyFinal = NSSDBGC_VerifyFinal;
    debug_functions.C_VerifyRecoverInit = NSSDBGC_VerifyRecoverInit;
    debug_functions.C_VerifyRecover = NSSDBGC_VerifyRecover;
    debug_functions.C_DigestEncryptUpdate = NSSDBGC_DigestEncryptUpdate;
    debug_functions.C_DecryptDigestUpdate = NSSDBGC_DecryptDigestUpdate;
    debug_functions.C_SignEncryptUpdate = NSSDBGC_SignEncryptUpdate;
    debug_functions.C_DecryptVerifyUpdate = NSSDBGC_DecryptVerifyUpdate;
    debug_functions.C_GenerateKey = NSSDBGC_GenerateKey;
    debug_functions.C_GenerateKeyPair = NSSDBGC_GenerateKeyPair;
    debug_functions.C_WrapKey = NSSDBGC_WrapKey;
    debug_functions.C_UnwrapKey = NSSDBGC_UnwrapKey;
    debug_functions.C_DeriveKey = NSSDBGC_DeriveKey;
    debug_functions.C_SeedRandom = NSSDBGC_SeedRandom;
    debug_functions.C_GenerateRandom = NSSDBGC_GenerateRandom;
    debug_functions.C_GetFunctionStatus = NSSDBGC_GetFunctionStatus;
    debug_functions.C_CancelFunction = NSSDBGC_CancelFunction;
    debug_functions.C_WaitForSlotEvent = NSSDBGC_WaitForSlotEvent;
    return &debug_functions;
}

/*
 * scale the time factor up accordingly.
 * This routine tries to keep at least 2 significant figures on output.
 *    If the time is 0, then indicate that with a 'z' for units.
 *    If the time is greater than 10 minutes, output the time in minutes.
 *    If the time is less than 10 minutes but greater than 10 seconds output 
 * the time in second.
 *    If the time is less than 10 seconds but greater than 10 milliseconds 
 * output * the time in millisecond.
 *    If the time is less than 10 milliseconds but greater than 0 ticks output 
 * the time in microsecond.
 *
 */
static PRUint32 getPrintTime(PRIntervalTime time ,char **type)
{
	PRUint32 prTime;

        /* detect a programming error by outputting 'bu' to the output stream
	 * rather than crashing */
 	*type = "bug";
	if (time == 0) {
	    *type = "z";
	    return 0;
	}

	prTime = PR_IntervalToSeconds(time);

	if (prTime >= 600) {
	    *type="m";
	    return prTime/60;
	}
        if (prTime >= 10) {
	    *type="s";
	    return prTime;
	} 
	prTime = PR_IntervalToMilliseconds(time);
        if (prTime >= 10) {
	    *type="ms";
	    return prTime;
	} 
 	*type = "us";
	return PR_IntervalToMicroseconds(time);
}

static void print_final_statistics(void)
{
    int total_calls = 0;
    PRIntervalTime total_time = 0;
    PRUint32 pr_total_time;
    char *type;
    char *fname;
    FILE *outfile = NULL;
    int i;

    fname = PR_GetEnv("NSS_OUTPUT_FILE");
    if (fname) {
	/* need to add an optional process id to the filename */
	outfile = fopen(fname,"w+");
    }
    if (!outfile) {
	outfile = stdout;
    }
	

    fprintf(outfile,"%-25s %10s %12s %12s %10s\n", "Function", "# Calls", 
				"Time", "Avg.", "% Time");
    fprintf(outfile,"\n");
    for (i=0; i < nssdbg_prof_size; i++) {
	total_calls += nssdbg_prof_data[i].calls;
	total_time += nssdbg_prof_data[i].time;
    }
    for (i=0; i < nssdbg_prof_size; i++) {
	PRIntervalTime time = nssdbg_prof_data[i].time;
	PRUint32 usTime = PR_IntervalToMicroseconds(time);
	PRUint32 prTime = 0;
	PRUint32 calls = nssdbg_prof_data[i].calls;
	/* don't print out functions that weren't even called */
	if (calls == 0) {
	    continue;
	}

	prTime = getPrintTime(time,&type);

	fprintf(outfile,"%-25s %10d %10d%2s ", nssdbg_prof_data[i].function, 
						calls, prTime, type);
	/* for now always output the average in microseconds */
	fprintf(outfile,"%10.2f%2s", (float)usTime / (float)calls, "us" );
	fprintf(outfile,"%10.2f%%", ((float)time / (float)total_time) * 100);
	fprintf(outfile,"\n");
    }
    fprintf(outfile,"\n");

    pr_total_time = getPrintTime(total_time,&type);

    fprintf(outfile,"%25s %10d %10d%2s\n", "Totals", total_calls, 
							pr_total_time, type);
    fprintf(outfile,"\n\nMaximum number of concurrent open sessions: %d\n\n",
							 maxOpenSessions);
    fflush (outfile);
    if (outfile != stdout) {
	fclose(outfile);
    }
}

