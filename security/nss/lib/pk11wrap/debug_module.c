/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "prlog.h"
#include <stdio.h>

static PRLogModuleInfo *modlog = NULL;

static CK_FUNCTION_LIST_PTR module_functions;

static CK_FUNCTION_LIST debug_functions;

static void print_final_statistics(void);

/* The AIX 64-bit compiler chokes on large switch statements (see
 * bug #63815).  I tried the trick recommended there, using -O2 in
 * debug builds, and it didn't work.  Instead, I'll suppress some of
 * the verbose output and just dump values.
 */

static void get_attr_type_str(CK_ATTRIBUTE_TYPE atype, char *str, int len)
{
#define SETA(attr) \
    PR_snprintf(str, len, "%s", attr); break;
    switch (atype) {
#ifndef AIX_64BIT
    case CKA_CLASS: SETA("CKA_CLASS");
    case CKA_TOKEN: SETA("CKA_TOKEN");
    case CKA_PRIVATE: SETA("CKA_PRIVATE");
    case CKA_LABEL: SETA("CKA_LABEL");
    case CKA_APPLICATION: SETA("CKA_APPLICATION");
    case CKA_VALUE: SETA("CKA_VALUE");
    case CKA_OBJECT_ID: SETA("CKA_OBJECT_ID");
    case CKA_CERTIFICATE_TYPE: SETA("CKA_CERTIFICATE_TYPE");
    case CKA_ISSUER: SETA("CKA_ISSUER");
    case CKA_SERIAL_NUMBER: SETA("CKA_SERIAL_NUMBER");
    case CKA_AC_ISSUER: SETA("CKA_AC_ISSUER");
    case CKA_OWNER: SETA("CKA_OWNER");
    case CKA_ATTR_TYPES: SETA("CKA_ATTR_TYPES");
    case CKA_TRUSTED: SETA("CKA_TRUSTED");
    case CKA_KEY_TYPE: SETA("CKA_KEY_TYPE");
    case CKA_SUBJECT: SETA("CKA_SUBJECT");
    case CKA_ID: SETA("CKA_ID");
    case CKA_SENSITIVE: SETA("CKA_SENSITIVE");
    case CKA_ENCRYPT: SETA("CKA_ENCRYPT");
    case CKA_DECRYPT: SETA("CKA_DECRYPT");
    case CKA_WRAP: SETA("CKA_WRAP");
    case CKA_UNWRAP: SETA("CKA_UNWRAP");
    case CKA_SIGN: SETA("CKA_SIGN");
    case CKA_SIGN_RECOVER: SETA("CKA_SIGN_RECOVER");
    case CKA_VERIFY: SETA("CKA_VERIFY");
    case CKA_VERIFY_RECOVER: SETA("CKA_VERIFY_RECOVER");
    case CKA_DERIVE: SETA("CKA_DERIVE");
    case CKA_START_DATE: SETA("CKA_START_DATE");
    case CKA_END_DATE: SETA("CKA_END_DATE");
    case CKA_MODULUS: SETA("CKA_MODULUS");
    case CKA_MODULUS_BITS: SETA("CKA_MODULUS_BITS");
    case CKA_PUBLIC_EXPONENT: SETA("CKA_PUBLIC_EXPONENT");
    case CKA_PRIVATE_EXPONENT: SETA("CKA_PRIVATE_EXPONENT");
    case CKA_PRIME_1: SETA("CKA_PRIME_1");
    case CKA_PRIME_2: SETA("CKA_PRIME_2");
    case CKA_EXPONENT_1: SETA("CKA_EXPONENT_1");
    case CKA_EXPONENT_2: SETA("CKA_EXPONENT_2");
    case CKA_COEFFICIENT: SETA("CKA_COEFFICIENT");
    case CKA_PRIME: SETA("CKA_PRIME");
    case CKA_SUBPRIME: SETA("CKA_SUBPRIME");
    case CKA_BASE: SETA("CKA_BASE");
    case CKA_PRIME_BITS: SETA("CKA_PRIME_BITS");
    case CKA_SUB_PRIME_BITS: SETA("CKA_SUB_PRIME_BITS");
    case CKA_VALUE_BITS: SETA("CKA_VALUE_BITS");
    case CKA_VALUE_LEN: SETA("CKA_VALUE_LEN");
    case CKA_EXTRACTABLE: SETA("CKA_EXTRACTABLE");
    case CKA_LOCAL: SETA("CKA_LOCAL");
    case CKA_NEVER_EXTRACTABLE: SETA("CKA_NEVER_EXTRACTABLE");
    case CKA_ALWAYS_SENSITIVE: SETA("CKA_ALWAYS_SENSITIVE");
    case CKA_KEY_GEN_MECHANISM: SETA("CKA_KEY_GEN_MECHANISM");
    case CKA_MODIFIABLE: SETA("CKA_MODIFIABLE");
    case CKA_ECDSA_PARAMS: SETA("CKA_ECDSA_PARAMS");
    case CKA_EC_POINT: SETA("CKA_EC_POINT");
    case CKA_SECONDARY_AUTH: SETA("CKA_SECONDARY_AUTH");
    case CKA_AUTH_PIN_FLAGS: SETA("CKA_AUTH_PIN_FLAGS");
    case CKA_HW_FEATURE_TYPE: SETA("CKA_HW_FEATURE_TYPE");
    case CKA_RESET_ON_INIT: SETA("CKA_RESET_ON_INIT");
    case CKA_HAS_RESET: SETA("CKA_HAS_RESET");
    case CKA_VENDOR_DEFINED: SETA("CKA_VENDOR_DEFINED");
    case CKA_NETSCAPE_URL: SETA("CKA_NETSCAPE_URL");
    case CKA_NETSCAPE_EMAIL: SETA("CKA_NETSCAPE_EMAIL");
    case CKA_NETSCAPE_SMIME_INFO: SETA("CKA_NETSCAPE_SMIME_INFO");
    case CKA_NETSCAPE_SMIME_TIMESTAMP: SETA("CKA_NETSCAPE_SMIME_TIMESTAMP");
    case CKA_NETSCAPE_PKCS8_SALT: SETA("CKA_NETSCAPE_PKCS8_SALT");
    case CKA_NETSCAPE_PASSWORD_CHECK: SETA("CKA_NETSCAPE_PASSWORD_CHECK");
    case CKA_NETSCAPE_EXPIRES: SETA("CKA_NETSCAPE_EXPIRES");
    case CKA_NETSCAPE_KRL: SETA("CKA_NETSCAPE_KRL");
    case CKA_NETSCAPE_PQG_COUNTER: SETA("CKA_NETSCAPE_PQG_COUNTER");
    case CKA_NETSCAPE_PQG_SEED: SETA("CKA_NETSCAPE_PQG_SEED");
    case CKA_NETSCAPE_PQG_H: SETA("CKA_NETSCAPE_PQG_H");
    case CKA_NETSCAPE_PQG_SEED_BITS: SETA("CKA_NETSCAPE_PQG_SEED_BITS");
    case CKA_TRUST: SETA("CKA_TRUST");
    case CKA_TRUST_DIGITAL_SIGNATURE: SETA("CKA_TRUST_DIGITAL_SIGNATURE");
    case CKA_TRUST_NON_REPUDIATION: SETA("CKA_TRUST_NON_REPUDIATION");
    case CKA_TRUST_KEY_ENCIPHERMENT: SETA("CKA_TRUST_KEY_ENCIPHERMENT");
    case CKA_TRUST_DATA_ENCIPHERMENT: SETA("CKA_TRUST_DATA_ENCIPHERMENT");
    case CKA_TRUST_KEY_AGREEMENT: SETA("CKA_TRUST_KEY_AGREEMENT");
    case CKA_TRUST_KEY_CERT_SIGN: SETA("CKA_TRUST_KEY_CERT_SIGN");
    case CKA_TRUST_CRL_SIGN: SETA("CKA_TRUST_CRL_SIGN");
    case CKA_TRUST_SERVER_AUTH: SETA("CKA_TRUST_SERVER_AUTH");
    case CKA_TRUST_CLIENT_AUTH: SETA("CKA_TRUST_CLIENT_AUTH");
    case CKA_TRUST_CODE_SIGNING: SETA("CKA_TRUST_CODE_SIGNING");
    case CKA_TRUST_EMAIL_PROTECTION: SETA("CKA_TRUST_EMAIL_PROTECTION");
    case CKA_TRUST_IPSEC_END_SYSTEM: SETA("CKA_TRUST_IPSEC_END_SYSTEM");
    case CKA_TRUST_IPSEC_TUNNEL: SETA("CKA_TRUST_IPSEC_TUNNEL");
    case CKA_TRUST_IPSEC_USER: SETA("CKA_TRUST_IPSEC_USER");
    case CKA_TRUST_TIME_STAMPING: SETA("CKA_TRUST_TIME_STAMPING");
    case CKA_CERT_SHA1_HASH: SETA("CKA_CERT_SHA1_HASH");
    case CKA_CERT_MD5_HASH: SETA("CKA_CERT_MD5_HASH");
    case CKA_NETSCAPE_DB: SETA("CKA_NETSCAPE_DB");
    case CKA_NETSCAPE_TRUST: SETA("CKA_NETSCAPE_TRUST");
#endif
    default: PR_snprintf(str, len, "0x%p", atype); break;
    }
}

static void get_obj_class(CK_OBJECT_CLASS class, char *str, int len)
{
#define SETO(objc) \
    PR_snprintf(str, len, "%s", objc); break;
    switch (class) {
#ifndef AIX_64BIT
    case CKO_DATA: SETO("CKO_DATA");
    case CKO_CERTIFICATE: SETO("CKO_CERTIFICATE");
    case CKO_PUBLIC_KEY: SETO("CKO_PUBLIC_KEY");
    case CKO_PRIVATE_KEY: SETO("CKO_PRIVATE_KEY");
    case CKO_SECRET_KEY: SETO("CKO_SECRET_KEY");
    case CKO_HW_FEATURE: SETO("CKO_HW_FEATURE");
    case CKO_DOMAIN_PARAMETERS: SETO("CKO_DOMAIN_PARAMETERS");
    case CKO_NETSCAPE_CRL: SETO("CKO_NETSCAPE_CRL");
    case CKO_NETSCAPE_SMIME: SETO("CKO_NETSCAPE_SMIME");
    case CKO_NETSCAPE_TRUST: SETO("CKO_NETSCAPE_TRUST");
    case CKO_NETSCAPE_BUILTIN_ROOT_LIST: SETO("CKO_NETSCAPE_BUILTIN_ROOT_LIST");
#endif
    default: PR_snprintf(str, len, "0x%p", class); break;
    }
}

static void get_trust_val(CK_TRUST trust, char *str, int len)
{
#define SETT(objc) \
    PR_snprintf(str, len, "%s", objc); break;
    switch (trust) {
#ifndef AIX_64BIT
    case CKT_NETSCAPE_TRUSTED: SETT("CKT_NETSCAPE_TRUSTED");
    case CKT_NETSCAPE_TRUSTED_DELEGATOR: SETT("CKT_NETSCAPE_TRUSTED_DELEGATOR");
    case CKT_NETSCAPE_UNTRUSTED: SETT("CKT_NETSCAPE_UNTRUSTED");
    case CKT_NETSCAPE_MUST_VERIFY: SETT("CKT_NETSCAPE_MUST_VERIFY");
    case CKT_NETSCAPE_TRUST_UNKNOWN: SETT("CKT_NETSCAPE_TRUST_UNKNOWN");
    case CKT_NETSCAPE_VALID: SETT("CKT_NETSCAPE_VALID");
    case CKT_NETSCAPE_VALID_DELEGATOR: SETT("CKT_NETSCAPE_VALID_DELEGATOR");
#endif
    default: PR_snprintf(str, len, "0x%p", trust); break;
    }
}

static void print_attr_value(CK_ATTRIBUTE_PTR attr)
{
    char atype[48];
    char valstr[48];
    int len;
    get_attr_type_str(attr->type, atype, sizeof atype);
    switch (attr->type) {
    case CKA_TOKEN:
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ENCRYPT:
    case CKA_DECRYPT:
    case CKA_WRAP:
    case CKA_UNWRAP:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_DERIVE:
    case CKA_EXTRACTABLE:
    case CKA_LOCAL:
    case CKA_NEVER_EXTRACTABLE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_MODIFIABLE:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_BBOOL tf = *((CK_BBOOL *)attr->pValue);
	    len = sizeof(valstr);
	    PR_snprintf(valstr, len, "%s", tf ? "CK_TRUE" : "CK_FALSE");
	    PR_LOG(modlog, 4, ("    %s = %s [%d]", 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_CLASS:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_OBJECT_CLASS class = *((CK_OBJECT_CLASS *)attr->pValue);
	    get_obj_class(class, valstr, sizeof valstr);
	    PR_LOG(modlog, 4, ("    %s = %s [%d]", 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_TRUST_SERVER_AUTH:
    case CKA_TRUST_CLIENT_AUTH:
    case CKA_TRUST_CODE_SIGNING:
    case CKA_TRUST_EMAIL_PROTECTION:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    CK_TRUST trust = *((CK_TRUST *)attr->pValue);
	    get_trust_val(trust, valstr, sizeof valstr);
	    PR_LOG(modlog, 4, ("    %s = %s [%d]", 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    case CKA_LABEL:
    case CKA_NETSCAPE_EMAIL:
    case CKA_NETSCAPE_URL:
	if (attr->ulValueLen > 0 && attr->pValue) {
	    len = PR_MIN(attr->ulValueLen + 1, sizeof valstr);
	    PR_snprintf(valstr, len, "%s", attr->pValue);
	    PR_LOG(modlog, 4, ("    %s = %s [%d]", 
	           atype, valstr, attr->ulValueLen));
	    break;
	}
    default:
	PR_LOG(modlog, 4, ("    %s = 0x%p [%d]", 
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

static void print_mechanism(CK_MECHANISM_PTR m)
{
    PR_LOG(modlog, 4, ("      mechanism = 0x%p", m->mechanism));
}

#define MAX_UINT32 0xffffffff

static void nssdbg_finish_time(PRInt32 *counter, PRIntervalTime start)
{
    PRIntervalTime ival;
    PRIntervalTime end = PR_IntervalNow();

    if (end >= start) {
	ival = PR_IntervalToMilliseconds(end-start);
    } else {
	/* the interval timer rolled over. presume it only tripped once */
	ival = PR_IntervalToMilliseconds(MAX_UINT32-start) +
			PR_IntervalToMilliseconds(end);
    }
    PR_AtomicAdd(counter, ival);
}

static PRInt32 counter_C_Initialize = 0;
static PRInt32 calls_C_Initialize = 0;
CK_RV NSSDBGC_Initialize(
  CK_VOID_PTR pInitArgs
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Initialize);
    PR_LOG(modlog, 1, ("C_Initialize"));
    PR_LOG(modlog, 3, ("  pInitArgs = 0x%p", pInitArgs));
    start = PR_IntervalNow();
    rv = module_functions->C_Initialize(pInitArgs);
    nssdbg_finish_time(&counter_C_Initialize,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Finalize = 0;
static PRInt32 calls_C_Finalize = 0;
CK_RV NSSDBGC_Finalize(
  CK_VOID_PTR pReserved
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Finalize);
    PR_LOG(modlog, 1, ("C_Finalize"));
    PR_LOG(modlog, 3, ("  pReserved = 0x%p", pReserved));
    start = PR_IntervalNow();
    rv = module_functions->C_Finalize(pReserved);
    nssdbg_finish_time(&counter_C_Finalize,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetInfo = 0;
static PRInt32 calls_C_GetInfo = 0;
CK_RV NSSDBGC_GetInfo(
  CK_INFO_PTR pInfo
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetInfo);
    PR_LOG(modlog, 1, ("C_GetInfo"));
    PR_LOG(modlog, 3, ("  pInfo = 0x%p", pInfo));
    start = PR_IntervalNow();
    rv = module_functions->C_GetInfo(pInfo);
    nssdbg_finish_time(&counter_C_GetInfo,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetFunctionList = 0;
static PRInt32 calls_C_GetFunctionList = 0;
CK_RV NSSDBGC_GetFunctionList(
  CK_FUNCTION_LIST_PTR_PTR ppFunctionList
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetFunctionList);
    PR_LOG(modlog, 1, ("C_GetFunctionList"));
    PR_LOG(modlog, 3, ("  ppFunctionList = 0x%p", ppFunctionList));
    start = PR_IntervalNow();
    rv = module_functions->C_GetFunctionList(ppFunctionList);
    nssdbg_finish_time(&counter_C_GetFunctionList,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetSlotList = 0;
static PRInt32 calls_C_GetSlotList = 0;
CK_RV NSSDBGC_GetSlotList(
  CK_BBOOL       tokenPresent,
  CK_SLOT_ID_PTR pSlotList,
  CK_ULONG_PTR   pulCount
)
{
    CK_RV rv;
    PRIntervalTime start;
    CK_ULONG i;
    PR_AtomicIncrement(&calls_C_GetSlotList);
    PR_LOG(modlog, 1, ("C_GetSlotList"));
    PR_LOG(modlog, 3, ("  tokenPresent = 0x%x", tokenPresent));
    PR_LOG(modlog, 3, ("  pSlotList = 0x%p", pSlotList));
    PR_LOG(modlog, 3, ("  pulCount = 0x%p", pulCount));
    start = PR_IntervalNow();
    rv = module_functions->C_GetSlotList(tokenPresent,
                                 pSlotList,
                                 pulCount);
    nssdbg_finish_time(&counter_C_GetSlotList,start);
    PR_LOG(modlog, 4, ("  *pulCount = 0x%x", *pulCount));
    if (pSlotList) {
	for (i=0; i<*pulCount; i++) {
	    PR_LOG(modlog, 4, ("  slotID[%d] = %x", i, pSlotList[i]));
	}
    }
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetSlotInfo = 0;
static PRInt32 calls_C_GetSlotInfo = 0;
CK_RV NSSDBGC_GetSlotInfo(
  CK_SLOT_ID       slotID,
  CK_SLOT_INFO_PTR pInfo
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetSlotInfo);
    PR_LOG(modlog, 1, ("C_GetSlotInfo"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  pInfo = 0x%p", pInfo));
    start = PR_IntervalNow();
    rv = module_functions->C_GetSlotInfo(slotID,
                                 pInfo);
    nssdbg_finish_time(&counter_C_GetSlotInfo,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetTokenInfo = 0;
static PRInt32 calls_C_GetTokenInfo = 0;
CK_RV NSSDBGC_GetTokenInfo(
  CK_SLOT_ID        slotID,
  CK_TOKEN_INFO_PTR pInfo
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetTokenInfo);
    PR_LOG(modlog, 1, ("C_GetTokenInfo"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  pInfo = 0x%p", pInfo));
    start = PR_IntervalNow();
    rv = module_functions->C_GetTokenInfo(slotID,
                                 pInfo);
    nssdbg_finish_time(&counter_C_GetTokenInfo,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetMechanismList = 0;
static PRInt32 calls_C_GetMechanismList = 0;
CK_RV NSSDBGC_GetMechanismList(
  CK_SLOT_ID            slotID,
  CK_MECHANISM_TYPE_PTR pMechanismList,
  CK_ULONG_PTR          pulCount
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetMechanismList);
    PR_LOG(modlog, 1, ("C_GetMechanismList"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  pMechanismList = 0x%p", pMechanismList));
    PR_LOG(modlog, 3, ("  pulCount = 0x%p", pulCount));
    start = PR_IntervalNow();
    rv = module_functions->C_GetMechanismList(slotID,
                                 pMechanismList,
                                 pulCount);
    nssdbg_finish_time(&counter_C_GetMechanismList,start);
    PR_LOG(modlog, 4, ("  *pulCount = 0x%x", *pulCount));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetMechanismInfo = 0;
static PRInt32 calls_C_GetMechanismInfo = 0;
CK_RV NSSDBGC_GetMechanismInfo(
  CK_SLOT_ID            slotID,
  CK_MECHANISM_TYPE     type,
  CK_MECHANISM_INFO_PTR pInfo
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetMechanismInfo);
    PR_LOG(modlog, 1, ("C_GetMechanismInfo"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  type = 0x%x", type));
    PR_LOG(modlog, 3, ("  pInfo = 0x%p", pInfo));
    start = PR_IntervalNow();
    rv = module_functions->C_GetMechanismInfo(slotID,
                                 type,
                                 pInfo);
    nssdbg_finish_time(&counter_C_GetMechanismInfo,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_InitToken = 0;
static PRInt32 calls_C_InitToken = 0;
CK_RV NSSDBGC_InitToken(
  CK_SLOT_ID  slotID,
  CK_CHAR_PTR pPin,
  CK_ULONG    ulPinLen,
  CK_CHAR_PTR pLabel
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_InitToken);
    PR_LOG(modlog, 1, ("C_InitToken"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  pPin = 0x%p", pPin));
    PR_LOG(modlog, 3, ("  ulPinLen = %d", ulPinLen));
    PR_LOG(modlog, 3, ("  pLabel = 0x%p", pLabel));
    start = PR_IntervalNow();
    rv = module_functions->C_InitToken(slotID,
                                 pPin,
                                 ulPinLen,
                                 pLabel);
    nssdbg_finish_time(&counter_C_InitToken,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_InitPIN = 0;
static PRInt32 calls_C_InitPIN = 0;
CK_RV NSSDBGC_InitPIN(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR       pPin,
  CK_ULONG          ulPinLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_InitPIN);
    PR_LOG(modlog, 1, ("C_InitPIN"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPin = 0x%p", pPin));
    PR_LOG(modlog, 3, ("  ulPinLen = %d", ulPinLen));
    start = PR_IntervalNow();
    rv = module_functions->C_InitPIN(hSession,
                                 pPin,
                                 ulPinLen);
    nssdbg_finish_time(&counter_C_InitPIN,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SetPIN = 0;
static PRInt32 calls_C_SetPIN = 0;
CK_RV NSSDBGC_SetPIN(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR       pOldPin,
  CK_ULONG          ulOldLen,
  CK_CHAR_PTR       pNewPin,
  CK_ULONG          ulNewLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SetPIN);
    PR_LOG(modlog, 1, ("C_SetPIN"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pOldPin = 0x%p", pOldPin));
    PR_LOG(modlog, 3, ("  ulOldLen = %d", ulOldLen));
    PR_LOG(modlog, 3, ("  pNewPin = 0x%p", pNewPin));
    PR_LOG(modlog, 3, ("  ulNewLen = %d", ulNewLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SetPIN(hSession,
                                 pOldPin,
                                 ulOldLen,
                                 pNewPin,
                                 ulNewLen);
    nssdbg_finish_time(&counter_C_SetPIN,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_OpenSession = 0;
static PRInt32 calls_C_OpenSession = 0;
static PRInt32 numOpenSessions = 0;
static PRInt32 maxOpenSessions = 0;
CK_RV NSSDBGC_OpenSession(
  CK_SLOT_ID            slotID,
  CK_FLAGS              flags,
  CK_VOID_PTR           pApplication,
  CK_NOTIFY             Notify,
  CK_SESSION_HANDLE_PTR phSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_OpenSession);
    PR_AtomicIncrement(&numOpenSessions);
    maxOpenSessions = PR_MAX(numOpenSessions, maxOpenSessions);
    PR_LOG(modlog, 1, ("C_OpenSession"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    PR_LOG(modlog, 3, ("  flags = 0x%x", flags));
    PR_LOG(modlog, 3, ("  pApplication = 0x%p", pApplication));
    PR_LOG(modlog, 3, ("  Notify = 0x%x", Notify));
    PR_LOG(modlog, 3, ("  phSession = 0x%p", phSession));
    start = PR_IntervalNow();
    rv = module_functions->C_OpenSession(slotID,
                                 flags,
                                 pApplication,
                                 Notify,
                                 phSession);
    nssdbg_finish_time(&counter_C_OpenSession,start);
    PR_LOG(modlog, 4, ("  *phSession = 0x%x", *phSession));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_CloseSession = 0;
static PRInt32 calls_C_CloseSession = 0;
CK_RV NSSDBGC_CloseSession(
  CK_SESSION_HANDLE hSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_CloseSession);
    PR_AtomicDecrement(&numOpenSessions);
    PR_LOG(modlog, 1, ("C_CloseSession"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    start = PR_IntervalNow();
    rv = module_functions->C_CloseSession(hSession);
    nssdbg_finish_time(&counter_C_CloseSession,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_CloseAllSessions = 0;
static PRInt32 calls_C_CloseAllSessions = 0;
CK_RV NSSDBGC_CloseAllSessions(
  CK_SLOT_ID slotID
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_CloseAllSessions);
    PR_LOG(modlog, 1, ("C_CloseAllSessions"));
    PR_LOG(modlog, 3, ("  slotID = 0x%x", slotID));
    start = PR_IntervalNow();
    rv = module_functions->C_CloseAllSessions(slotID);
    nssdbg_finish_time(&counter_C_CloseAllSessions,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetSessionInfo = 0;
static PRInt32 calls_C_GetSessionInfo = 0;
CK_RV NSSDBGC_GetSessionInfo(
  CK_SESSION_HANDLE   hSession,
  CK_SESSION_INFO_PTR pInfo
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetSessionInfo);
    PR_LOG(modlog, 1, ("C_GetSessionInfo"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pInfo = 0x%p", pInfo));
    start = PR_IntervalNow();
    rv = module_functions->C_GetSessionInfo(hSession,
                                 pInfo);
    nssdbg_finish_time(&counter_C_GetSessionInfo,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetOperationState = 0;
static PRInt32 calls_C_GetOperationState = 0;
CK_RV NSSDBGC_GetOperationState(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pOperationState,
  CK_ULONG_PTR      pulOperationStateLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetOperationState);
    PR_LOG(modlog, 1, ("C_GetOperationState"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pOperationState = 0x%p", pOperationState));
    PR_LOG(modlog, 3, ("  pulOperationStateLen = 0x%p", pulOperationStateLen));
    start = PR_IntervalNow();
    rv = module_functions->C_GetOperationState(hSession,
                                 pOperationState,
                                 pulOperationStateLen);
    nssdbg_finish_time(&counter_C_GetOperationState,start);
    PR_LOG(modlog, 4, ("  *pulOperationStateLen = 0x%x", *pulOperationStateLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SetOperationState = 0;
static PRInt32 calls_C_SetOperationState = 0;
CK_RV NSSDBGC_SetOperationState(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR      pOperationState,
  CK_ULONG         ulOperationStateLen,
  CK_OBJECT_HANDLE hEncryptionKey,
  CK_OBJECT_HANDLE hAuthenticationKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SetOperationState);
    PR_LOG(modlog, 1, ("C_SetOperationState"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pOperationState = 0x%p", pOperationState));
    PR_LOG(modlog, 3, ("  ulOperationStateLen = %d", ulOperationStateLen));
    PR_LOG(modlog, 3, ("  hEncryptionKey = 0x%x", hEncryptionKey));
    PR_LOG(modlog, 3, ("  hAuthenticationKey = 0x%x", hAuthenticationKey));
    start = PR_IntervalNow();
    rv = module_functions->C_SetOperationState(hSession,
                                 pOperationState,
                                 ulOperationStateLen,
                                 hEncryptionKey,
                                 hAuthenticationKey);
    nssdbg_finish_time(&counter_C_SetOperationState,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Login = 0;
static PRInt32 calls_C_Login = 0;
CK_RV NSSDBGC_Login(
  CK_SESSION_HANDLE hSession,
  CK_USER_TYPE      userType,
  CK_CHAR_PTR       pPin,
  CK_ULONG          ulPinLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Login);
    PR_LOG(modlog, 1, ("C_Login"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  userType = 0x%x", userType));
    PR_LOG(modlog, 3, ("  pPin = 0x%p", pPin));
    PR_LOG(modlog, 3, ("  ulPinLen = %d", ulPinLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Login(hSession,
                                 userType,
                                 pPin,
                                 ulPinLen);
    nssdbg_finish_time(&counter_C_Login,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Logout = 0;
static PRInt32 calls_C_Logout = 0;
CK_RV NSSDBGC_Logout(
  CK_SESSION_HANDLE hSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Logout);
    PR_LOG(modlog, 1, ("C_Logout"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    start = PR_IntervalNow();
    rv = module_functions->C_Logout(hSession);
    nssdbg_finish_time(&counter_C_Logout,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_CreateObject = 0;
static PRInt32 calls_C_CreateObject = 0;
CK_RV NSSDBGC_CreateObject(
  CK_SESSION_HANDLE    hSession,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phObject
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_CreateObject);
    PR_LOG(modlog, 1, ("C_CreateObject"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    PR_LOG(modlog, 3, ("  phObject = 0x%p", phObject));
    print_template(pTemplate, ulCount);
    start = PR_IntervalNow();
    rv = module_functions->C_CreateObject(hSession,
                                 pTemplate,
                                 ulCount,
                                 phObject);
    nssdbg_finish_time(&counter_C_CreateObject,start);
    PR_LOG(modlog, 4, ("  *phObject = 0x%x", *phObject));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_CopyObject = 0;
static PRInt32 calls_C_CopyObject = 0;
CK_RV NSSDBGC_CopyObject(
  CK_SESSION_HANDLE    hSession,
  CK_OBJECT_HANDLE     hObject,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phNewObject
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_CopyObject);
    PR_LOG(modlog, 1, ("C_CopyObject"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hObject = 0x%x", hObject));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    PR_LOG(modlog, 3, ("  phNewObject = 0x%p", phNewObject));
    print_template(pTemplate, ulCount);
    start = PR_IntervalNow();
    rv = module_functions->C_CopyObject(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount,
                                 phNewObject);
    nssdbg_finish_time(&counter_C_CopyObject,start);
    PR_LOG(modlog, 4, ("  *phNewObject = 0x%x", *phNewObject));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DestroyObject = 0;
static PRInt32 calls_C_DestroyObject = 0;
CK_RV NSSDBGC_DestroyObject(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DestroyObject);
    PR_LOG(modlog, 1, ("C_DestroyObject"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hObject = 0x%x", hObject));
    start = PR_IntervalNow();
    rv = module_functions->C_DestroyObject(hSession,
                                 hObject);
    nssdbg_finish_time(&counter_C_DestroyObject,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetObjectSize = 0;
static PRInt32 calls_C_GetObjectSize = 0;
CK_RV NSSDBGC_GetObjectSize(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ULONG_PTR      pulSize
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetObjectSize);
    PR_LOG(modlog, 1, ("C_GetObjectSize"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hObject = 0x%x", hObject));
    PR_LOG(modlog, 3, ("  pulSize = 0x%p", pulSize));
    start = PR_IntervalNow();
    rv = module_functions->C_GetObjectSize(hSession,
                                 hObject,
                                 pulSize);
    nssdbg_finish_time(&counter_C_GetObjectSize,start);
    PR_LOG(modlog, 4, ("  *pulSize = 0x%x", *pulSize));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetAttributeValue = 0;
static PRInt32 calls_C_GetAttributeValue = 0;
CK_RV NSSDBGC_GetAttributeValue(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetAttributeValue);
    PR_LOG(modlog, 1, ("C_GetAttributeValue"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hObject = 0x%x", hObject));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    start = PR_IntervalNow();
    rv = module_functions->C_GetAttributeValue(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(&counter_C_GetAttributeValue,start);
    print_template(pTemplate, ulCount);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SetAttributeValue = 0;
static PRInt32 calls_C_SetAttributeValue = 0;
CK_RV NSSDBGC_SetAttributeValue(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hObject,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SetAttributeValue);
    PR_LOG(modlog, 1, ("C_SetAttributeValue"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hObject = 0x%x", hObject));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    print_template(pTemplate, ulCount);
    start = PR_IntervalNow();
    rv = module_functions->C_SetAttributeValue(hSession,
                                 hObject,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(&counter_C_SetAttributeValue,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_FindObjectsInit = 0;
static PRInt32 calls_C_FindObjectsInit = 0;
CK_RV NSSDBGC_FindObjectsInit(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR  pTemplate,
  CK_ULONG          ulCount
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_FindObjectsInit);
    PR_LOG(modlog, 1, ("C_FindObjectsInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    print_template(pTemplate, ulCount);
    start = PR_IntervalNow();
    rv = module_functions->C_FindObjectsInit(hSession,
                                 pTemplate,
                                 ulCount);
    nssdbg_finish_time(&counter_C_FindObjectsInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_FindObjects = 0;
static PRInt32 calls_C_FindObjects = 0;
CK_RV NSSDBGC_FindObjects(
  CK_SESSION_HANDLE    hSession,
  CK_OBJECT_HANDLE_PTR phObject,
  CK_ULONG             ulMaxObjectCount,
  CK_ULONG_PTR         pulObjectCount
)
{
    CK_RV rv;
    CK_ULONG i;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_FindObjects);
    PR_LOG(modlog, 1, ("C_FindObjects"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  phObject = 0x%p", phObject));
    PR_LOG(modlog, 3, ("  ulMaxObjectCount = %d", ulMaxObjectCount));
    PR_LOG(modlog, 3, ("  pulObjectCount = 0x%p", pulObjectCount));
    start = PR_IntervalNow();
    rv = module_functions->C_FindObjects(hSession,
                                 phObject,
                                 ulMaxObjectCount,
                                 pulObjectCount);
    nssdbg_finish_time(&counter_C_FindObjects,start);
    PR_LOG(modlog, 4, ("  *pulObjectCount = 0x%x", *pulObjectCount));
    for (i=0; i<*pulObjectCount; i++) {
	PR_LOG(modlog, 4, ("  phObject[%d] = 0x%x", i, phObject[i]));
    }
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_FindObjectsFinal = 0;
static PRInt32 calls_C_FindObjectsFinal = 0;
CK_RV NSSDBGC_FindObjectsFinal(
  CK_SESSION_HANDLE hSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_FindObjectsFinal);
    PR_LOG(modlog, 1, ("C_FindObjectsFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    start = PR_IntervalNow();
    rv = module_functions->C_FindObjectsFinal(hSession);
    nssdbg_finish_time(&counter_C_FindObjectsFinal,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_EncryptInit = 0;
static PRInt32 calls_C_EncryptInit = 0;
CK_RV NSSDBGC_EncryptInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_EncryptInit);
    PR_LOG(modlog, 1, ("C_EncryptInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_EncryptInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_EncryptInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Encrypt = 0;
static PRInt32 calls_C_Encrypt = 0;
CK_RV NSSDBGC_Encrypt(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pEncryptedData,
  CK_ULONG_PTR      pulEncryptedDataLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Encrypt);
    PR_LOG(modlog, 1, ("C_Encrypt"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  ulDataLen = %d", ulDataLen));
    PR_LOG(modlog, 3, ("  pEncryptedData = 0x%p", pEncryptedData));
    PR_LOG(modlog, 3, ("  pulEncryptedDataLen = 0x%p", pulEncryptedDataLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Encrypt(hSession,
                                 pData,
                                 ulDataLen,
                                 pEncryptedData,
                                 pulEncryptedDataLen);
    nssdbg_finish_time(&counter_C_Encrypt,start);
    PR_LOG(modlog, 4, ("  *pulEncryptedDataLen = 0x%x", *pulEncryptedDataLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_EncryptUpdate = 0;
static PRInt32 calls_C_EncryptUpdate = 0;
CK_RV NSSDBGC_EncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_EncryptUpdate);
    PR_LOG(modlog, 1, ("C_EncryptUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  pulEncryptedPartLen = 0x%p", pulEncryptedPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_EncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(&counter_C_EncryptUpdate,start);
    PR_LOG(modlog, 4, ("  *pulEncryptedPartLen = 0x%x", *pulEncryptedPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_EncryptFinal = 0;
static PRInt32 calls_C_EncryptFinal = 0;
CK_RV NSSDBGC_EncryptFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pLastEncryptedPart,
  CK_ULONG_PTR      pulLastEncryptedPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_EncryptFinal);
    PR_LOG(modlog, 1, ("C_EncryptFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pLastEncryptedPart = 0x%p", pLastEncryptedPart));
    PR_LOG(modlog, 3, ("  pulLastEncryptedPartLen = 0x%p", pulLastEncryptedPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_EncryptFinal(hSession,
                                 pLastEncryptedPart,
                                 pulLastEncryptedPartLen);
    nssdbg_finish_time(&counter_C_EncryptFinal,start);
    PR_LOG(modlog, 4, ("  *pulLastEncryptedPartLen = 0x%x", *pulLastEncryptedPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DecryptInit = 0;
static PRInt32 calls_C_DecryptInit = 0;
CK_RV NSSDBGC_DecryptInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DecryptInit);
    PR_LOG(modlog, 1, ("C_DecryptInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_DecryptInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_DecryptInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Decrypt = 0;
static PRInt32 calls_C_Decrypt = 0;
CK_RV NSSDBGC_Decrypt(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedData,
  CK_ULONG          ulEncryptedDataLen,
  CK_BYTE_PTR       pData,
  CK_ULONG_PTR      pulDataLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Decrypt);
    PR_LOG(modlog, 1, ("C_Decrypt"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pEncryptedData = 0x%p", pEncryptedData));
    PR_LOG(modlog, 3, ("  ulEncryptedDataLen = %d", ulEncryptedDataLen));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  pulDataLen = 0x%p", pulDataLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Decrypt(hSession,
                                 pEncryptedData,
                                 ulEncryptedDataLen,
                                 pData,
                                 pulDataLen);
    nssdbg_finish_time(&counter_C_Decrypt,start);
    PR_LOG(modlog, 4, ("  *pulDataLen = 0x%x", *pulDataLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DecryptUpdate = 0;
static PRInt32 calls_C_DecryptUpdate = 0;
CK_RV NSSDBGC_DecryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DecryptUpdate);
    PR_LOG(modlog, 1, ("C_DecryptUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  ulEncryptedPartLen = %d", ulEncryptedPartLen));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  pulPartLen = 0x%p", pulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DecryptUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(&counter_C_DecryptUpdate,start);
    PR_LOG(modlog, 4, ("  *pulPartLen = 0x%x", *pulPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DecryptFinal = 0;
static PRInt32 calls_C_DecryptFinal = 0;
CK_RV NSSDBGC_DecryptFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pLastPart,
  CK_ULONG_PTR      pulLastPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DecryptFinal);
    PR_LOG(modlog, 1, ("C_DecryptFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pLastPart = 0x%p", pLastPart));
    PR_LOG(modlog, 3, ("  pulLastPartLen = 0x%p", pulLastPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DecryptFinal(hSession,
                                 pLastPart,
                                 pulLastPartLen);
    nssdbg_finish_time(&counter_C_DecryptFinal,start);
    PR_LOG(modlog, 4, ("  *pulLastPartLen = 0x%x", *pulLastPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DigestInit = 0;
static PRInt32 calls_C_DigestInit = 0;
CK_RV NSSDBGC_DigestInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DigestInit);
    PR_LOG(modlog, 1, ("C_DigestInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_DigestInit(hSession,
                                 pMechanism);
    nssdbg_finish_time(&counter_C_DigestInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Digest = 0;
static PRInt32 calls_C_Digest = 0;
CK_RV NSSDBGC_Digest(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pDigest,
  CK_ULONG_PTR      pulDigestLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Digest);
    PR_LOG(modlog, 1, ("C_Digest"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  ulDataLen = %d", ulDataLen));
    PR_LOG(modlog, 3, ("  pDigest = 0x%p", pDigest));
    PR_LOG(modlog, 3, ("  pulDigestLen = 0x%p", pulDigestLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Digest(hSession,
                                 pData,
                                 ulDataLen,
                                 pDigest,
                                 pulDigestLen);
    nssdbg_finish_time(&counter_C_Digest,start);
    PR_LOG(modlog, 4, ("  *pulDigestLen = 0x%x", *pulDigestLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DigestUpdate = 0;
static PRInt32 calls_C_DigestUpdate = 0;
CK_RV NSSDBGC_DigestUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DigestUpdate);
    PR_LOG(modlog, 1, ("C_DigestUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DigestUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(&counter_C_DigestUpdate,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DigestKey = 0;
static PRInt32 calls_C_DigestKey = 0;
CK_RV NSSDBGC_DigestKey(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DigestKey);
    PR_LOG(modlog, 1, ("C_DigestKey"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    start = PR_IntervalNow();
    rv = module_functions->C_DigestKey(hSession,
                                 hKey);
    nssdbg_finish_time(&counter_C_DigestKey,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DigestFinal = 0;
static PRInt32 calls_C_DigestFinal = 0;
CK_RV NSSDBGC_DigestFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pDigest,
  CK_ULONG_PTR      pulDigestLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DigestFinal);
    PR_LOG(modlog, 1, ("C_DigestFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pDigest = 0x%p", pDigest));
    PR_LOG(modlog, 3, ("  pulDigestLen = 0x%p", pulDigestLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DigestFinal(hSession,
                                 pDigest,
                                 pulDigestLen);
    nssdbg_finish_time(&counter_C_DigestFinal,start);
    PR_LOG(modlog, 4, ("  *pulDigestLen = 0x%x", *pulDigestLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignInit = 0;
static PRInt32 calls_C_SignInit = 0;
CK_RV NSSDBGC_SignInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignInit);
    PR_LOG(modlog, 1, ("C_SignInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_SignInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_SignInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Sign = 0;
static PRInt32 calls_C_Sign = 0;
CK_RV NSSDBGC_Sign(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Sign);
    PR_LOG(modlog, 1, ("C_Sign"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  ulDataLen = %d", ulDataLen));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  pulSignatureLen = 0x%p", pulSignatureLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Sign(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(&counter_C_Sign,start);
    PR_LOG(modlog, 4, ("  *pulSignatureLen = 0x%x", *pulSignatureLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignUpdate = 0;
static PRInt32 calls_C_SignUpdate = 0;
CK_RV NSSDBGC_SignUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignUpdate);
    PR_LOG(modlog, 1, ("C_SignUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SignUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(&counter_C_SignUpdate,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignFinal = 0;
static PRInt32 calls_C_SignFinal = 0;
CK_RV NSSDBGC_SignFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignFinal);
    PR_LOG(modlog, 1, ("C_SignFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  pulSignatureLen = 0x%p", pulSignatureLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SignFinal(hSession,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(&counter_C_SignFinal,start);
    PR_LOG(modlog, 4, ("  *pulSignatureLen = 0x%x", *pulSignatureLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignRecoverInit = 0;
static PRInt32 calls_C_SignRecoverInit = 0;
CK_RV NSSDBGC_SignRecoverInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignRecoverInit);
    PR_LOG(modlog, 1, ("C_SignRecoverInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_SignRecoverInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_SignRecoverInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignRecover = 0;
static PRInt32 calls_C_SignRecover = 0;
CK_RV NSSDBGC_SignRecover(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG_PTR      pulSignatureLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignRecover);
    PR_LOG(modlog, 1, ("C_SignRecover"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  ulDataLen = %d", ulDataLen));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  pulSignatureLen = 0x%p", pulSignatureLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SignRecover(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 pulSignatureLen);
    nssdbg_finish_time(&counter_C_SignRecover,start);
    PR_LOG(modlog, 4, ("  *pulSignatureLen = 0x%x", *pulSignatureLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_VerifyInit = 0;
static PRInt32 calls_C_VerifyInit = 0;
CK_RV NSSDBGC_VerifyInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_VerifyInit);
    PR_LOG(modlog, 1, ("C_VerifyInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_VerifyInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_VerifyInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_Verify = 0;
static PRInt32 calls_C_Verify = 0;
CK_RV NSSDBGC_Verify(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pData,
  CK_ULONG          ulDataLen,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_Verify);
    PR_LOG(modlog, 1, ("C_Verify"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  ulDataLen = %d", ulDataLen));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  ulSignatureLen = %d", ulSignatureLen));
    start = PR_IntervalNow();
    rv = module_functions->C_Verify(hSession,
                                 pData,
                                 ulDataLen,
                                 pSignature,
                                 ulSignatureLen);
    nssdbg_finish_time(&counter_C_Verify,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_VerifyUpdate = 0;
static PRInt32 calls_C_VerifyUpdate = 0;
CK_RV NSSDBGC_VerifyUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_VerifyUpdate);
    PR_LOG(modlog, 1, ("C_VerifyUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_VerifyUpdate(hSession,
                                 pPart,
                                 ulPartLen);
    nssdbg_finish_time(&counter_C_VerifyUpdate,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_VerifyFinal = 0;
static PRInt32 calls_C_VerifyFinal = 0;
CK_RV NSSDBGC_VerifyFinal(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_VerifyFinal);
    PR_LOG(modlog, 1, ("C_VerifyFinal"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  ulSignatureLen = %d", ulSignatureLen));
    start = PR_IntervalNow();
    rv = module_functions->C_VerifyFinal(hSession,
                                 pSignature,
                                 ulSignatureLen);
    nssdbg_finish_time(&counter_C_VerifyFinal,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_VerifyRecoverInit = 0;
static PRInt32 calls_C_VerifyRecoverInit = 0;
CK_RV NSSDBGC_VerifyRecoverInit(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_VerifyRecoverInit);
    PR_LOG(modlog, 1, ("C_VerifyRecoverInit"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_VerifyRecoverInit(hSession,
                                 pMechanism,
                                 hKey);
    nssdbg_finish_time(&counter_C_VerifyRecoverInit,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_VerifyRecover = 0;
static PRInt32 calls_C_VerifyRecover = 0;
CK_RV NSSDBGC_VerifyRecover(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSignature,
  CK_ULONG          ulSignatureLen,
  CK_BYTE_PTR       pData,
  CK_ULONG_PTR      pulDataLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_VerifyRecover);
    PR_LOG(modlog, 1, ("C_VerifyRecover"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pSignature = 0x%p", pSignature));
    PR_LOG(modlog, 3, ("  ulSignatureLen = %d", ulSignatureLen));
    PR_LOG(modlog, 3, ("  pData = 0x%p", pData));
    PR_LOG(modlog, 3, ("  pulDataLen = 0x%p", pulDataLen));
    start = PR_IntervalNow();
    rv = module_functions->C_VerifyRecover(hSession,
                                 pSignature,
                                 ulSignatureLen,
                                 pData,
                                 pulDataLen);
    nssdbg_finish_time(&counter_C_VerifyRecover,start);
    PR_LOG(modlog, 4, ("  *pulDataLen = 0x%x", *pulDataLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DigestEncryptUpdate = 0;
static PRInt32 calls_C_DigestEncryptUpdate = 0;
CK_RV NSSDBGC_DigestEncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DigestEncryptUpdate);
    PR_LOG(modlog, 1, ("C_DigestEncryptUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  pulEncryptedPartLen = 0x%p", pulEncryptedPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DigestEncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(&counter_C_DigestEncryptUpdate,start);
    PR_LOG(modlog, 4, ("  *pulEncryptedPartLen = 0x%x", *pulEncryptedPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DecryptDigestUpdate = 0;
static PRInt32 calls_C_DecryptDigestUpdate = 0;
CK_RV NSSDBGC_DecryptDigestUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DecryptDigestUpdate);
    PR_LOG(modlog, 1, ("C_DecryptDigestUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  ulEncryptedPartLen = %d", ulEncryptedPartLen));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  pulPartLen = 0x%p", pulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DecryptDigestUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(&counter_C_DecryptDigestUpdate,start);
    PR_LOG(modlog, 4, ("  *pulPartLen = 0x%x", *pulPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SignEncryptUpdate = 0;
static PRInt32 calls_C_SignEncryptUpdate = 0;
CK_RV NSSDBGC_SignEncryptUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pPart,
  CK_ULONG          ulPartLen,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG_PTR      pulEncryptedPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SignEncryptUpdate);
    PR_LOG(modlog, 1, ("C_SignEncryptUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  ulPartLen = %d", ulPartLen));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  pulEncryptedPartLen = 0x%p", pulEncryptedPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SignEncryptUpdate(hSession,
                                 pPart,
                                 ulPartLen,
                                 pEncryptedPart,
                                 pulEncryptedPartLen);
    nssdbg_finish_time(&counter_C_SignEncryptUpdate,start);
    PR_LOG(modlog, 4, ("  *pulEncryptedPartLen = 0x%x", *pulEncryptedPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DecryptVerifyUpdate = 0;
static PRInt32 calls_C_DecryptVerifyUpdate = 0;
CK_RV NSSDBGC_DecryptVerifyUpdate(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pEncryptedPart,
  CK_ULONG          ulEncryptedPartLen,
  CK_BYTE_PTR       pPart,
  CK_ULONG_PTR      pulPartLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DecryptVerifyUpdate);
    PR_LOG(modlog, 1, ("C_DecryptVerifyUpdate"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pEncryptedPart = 0x%p", pEncryptedPart));
    PR_LOG(modlog, 3, ("  ulEncryptedPartLen = %d", ulEncryptedPartLen));
    PR_LOG(modlog, 3, ("  pPart = 0x%p", pPart));
    PR_LOG(modlog, 3, ("  pulPartLen = 0x%p", pulPartLen));
    start = PR_IntervalNow();
    rv = module_functions->C_DecryptVerifyUpdate(hSession,
                                 pEncryptedPart,
                                 ulEncryptedPartLen,
                                 pPart,
                                 pulPartLen);
    nssdbg_finish_time(&counter_C_DecryptVerifyUpdate,start);
    PR_LOG(modlog, 4, ("  *pulPartLen = 0x%x", *pulPartLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GenerateKey = 0;
static PRInt32 calls_C_GenerateKey = 0;
CK_RV NSSDBGC_GenerateKey(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GenerateKey);
    PR_LOG(modlog, 1, ("C_GenerateKey"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulCount = %d", ulCount));
    PR_LOG(modlog, 3, ("  phKey = 0x%p", phKey));
    print_template(pTemplate, ulCount);
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_GenerateKey(hSession,
                                 pMechanism,
                                 pTemplate,
                                 ulCount,
                                 phKey);
    nssdbg_finish_time(&counter_C_GenerateKey,start);
    PR_LOG(modlog, 4, ("  *phKey = 0x%x", *phKey));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GenerateKeyPair = 0;
static PRInt32 calls_C_GenerateKeyPair = 0;
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
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GenerateKeyPair);
    PR_LOG(modlog, 1, ("C_GenerateKeyPair"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  pPublicKeyTemplate = 0x%p", pPublicKeyTemplate));
    PR_LOG(modlog, 3, ("  ulPublicKeyAttributeCount = %d", ulPublicKeyAttributeCount));
    PR_LOG(modlog, 3, ("  pPrivateKeyTemplate = 0x%p", pPrivateKeyTemplate));
    PR_LOG(modlog, 3, ("  ulPrivateKeyAttributeCount = %d", ulPrivateKeyAttributeCount));
    PR_LOG(modlog, 3, ("  phPublicKey = 0x%p", phPublicKey));
    PR_LOG(modlog, 3, ("  phPrivateKey = 0x%p", phPrivateKey));
    print_template(pPublicKeyTemplate, ulPublicKeyAttributeCount);
    print_template(pPrivateKeyTemplate, ulPrivateKeyAttributeCount);
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_GenerateKeyPair(hSession,
                                 pMechanism,
                                 pPublicKeyTemplate,
                                 ulPublicKeyAttributeCount,
                                 pPrivateKeyTemplate,
                                 ulPrivateKeyAttributeCount,
                                 phPublicKey,
                                 phPrivateKey);
    nssdbg_finish_time(&counter_C_GenerateKeyPair,start);
    PR_LOG(modlog, 4, ("  *phPublicKey = 0x%x", *phPublicKey));
    PR_LOG(modlog, 4, ("  *phPrivateKey = 0x%x", *phPrivateKey));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_WrapKey = 0;
static PRInt32 calls_C_WrapKey = 0;
CK_RV NSSDBGC_WrapKey(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR  pMechanism,
  CK_OBJECT_HANDLE  hWrappingKey,
  CK_OBJECT_HANDLE  hKey,
  CK_BYTE_PTR       pWrappedKey,
  CK_ULONG_PTR      pulWrappedKeyLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_WrapKey);
    PR_LOG(modlog, 1, ("C_WrapKey"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hWrappingKey = 0x%x", hWrappingKey));
    PR_LOG(modlog, 3, ("  hKey = 0x%x", hKey));
    PR_LOG(modlog, 3, ("  pWrappedKey = 0x%p", pWrappedKey));
    PR_LOG(modlog, 3, ("  pulWrappedKeyLen = 0x%p", pulWrappedKeyLen));
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_WrapKey(hSession,
                                 pMechanism,
                                 hWrappingKey,
                                 hKey,
                                 pWrappedKey,
                                 pulWrappedKeyLen);
    nssdbg_finish_time(&counter_C_WrapKey,start);
    PR_LOG(modlog, 4, ("  *pulWrappedKeyLen = 0x%x", *pulWrappedKeyLen));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_UnwrapKey = 0;
static PRInt32 calls_C_UnwrapKey = 0;
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
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_UnwrapKey);
    PR_LOG(modlog, 1, ("C_UnwrapKey"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hUnwrappingKey = 0x%x", hUnwrappingKey));
    PR_LOG(modlog, 3, ("  pWrappedKey = 0x%p", pWrappedKey));
    PR_LOG(modlog, 3, ("  ulWrappedKeyLen = %d", ulWrappedKeyLen));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulAttributeCount = %d", ulAttributeCount));
    PR_LOG(modlog, 3, ("  phKey = 0x%p", phKey));
    print_template(pTemplate, ulAttributeCount);
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_UnwrapKey(hSession,
                                 pMechanism,
                                 hUnwrappingKey,
                                 pWrappedKey,
                                 ulWrappedKeyLen,
                                 pTemplate,
                                 ulAttributeCount,
                                 phKey);
    nssdbg_finish_time(&counter_C_UnwrapKey,start);
    PR_LOG(modlog, 4, ("  *phKey = 0x%x", *phKey));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_DeriveKey = 0;
static PRInt32 calls_C_DeriveKey = 0;
CK_RV NSSDBGC_DeriveKey(
  CK_SESSION_HANDLE    hSession,
  CK_MECHANISM_PTR     pMechanism,
  CK_OBJECT_HANDLE     hBaseKey,
  CK_ATTRIBUTE_PTR     pTemplate,
  CK_ULONG             ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_DeriveKey);
    PR_LOG(modlog, 1, ("C_DeriveKey"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pMechanism = 0x%p", pMechanism));
    PR_LOG(modlog, 3, ("  hBaseKey = 0x%x", hBaseKey));
    PR_LOG(modlog, 3, ("  pTemplate = 0x%p", pTemplate));
    PR_LOG(modlog, 3, ("  ulAttributeCount = %d", ulAttributeCount));
    PR_LOG(modlog, 3, ("  phKey = 0x%p", phKey));
    print_template(pTemplate, ulAttributeCount);
    print_mechanism(pMechanism);
    start = PR_IntervalNow();
    rv = module_functions->C_DeriveKey(hSession,
                                 pMechanism,
                                 hBaseKey,
                                 pTemplate,
                                 ulAttributeCount,
                                 phKey);
    nssdbg_finish_time(&counter_C_DeriveKey,start);
    PR_LOG(modlog, 4, ("  *phKey = 0x%x", *phKey));
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_SeedRandom = 0;
static PRInt32 calls_C_SeedRandom = 0;
CK_RV NSSDBGC_SeedRandom(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       pSeed,
  CK_ULONG          ulSeedLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_SeedRandom);
    PR_LOG(modlog, 1, ("C_SeedRandom"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  pSeed = 0x%p", pSeed));
    PR_LOG(modlog, 3, ("  ulSeedLen = %d", ulSeedLen));
    start = PR_IntervalNow();
    rv = module_functions->C_SeedRandom(hSession,
                                 pSeed,
                                 ulSeedLen);
    nssdbg_finish_time(&counter_C_SeedRandom,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GenerateRandom = 0;
static PRInt32 calls_C_GenerateRandom = 0;
CK_RV NSSDBGC_GenerateRandom(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR       RandomData,
  CK_ULONG          ulRandomLen
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GenerateRandom);
    PR_LOG(modlog, 1, ("C_GenerateRandom"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    PR_LOG(modlog, 3, ("  RandomData = 0x%p", RandomData));
    PR_LOG(modlog, 3, ("  ulRandomLen = %d", ulRandomLen));
    start = PR_IntervalNow();
    rv = module_functions->C_GenerateRandom(hSession,
                                 RandomData,
                                 ulRandomLen);
    nssdbg_finish_time(&counter_C_GenerateRandom,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_GetFunctionStatus = 0;
static PRInt32 calls_C_GetFunctionStatus = 0;
CK_RV NSSDBGC_GetFunctionStatus(
  CK_SESSION_HANDLE hSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_GetFunctionStatus);
    PR_LOG(modlog, 1, ("C_GetFunctionStatus"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    start = PR_IntervalNow();
    rv = module_functions->C_GetFunctionStatus(hSession);
    nssdbg_finish_time(&counter_C_GetFunctionStatus,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_CancelFunction = 0;
static PRInt32 calls_C_CancelFunction = 0;
CK_RV NSSDBGC_CancelFunction(
  CK_SESSION_HANDLE hSession
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_CancelFunction);
    PR_LOG(modlog, 1, ("C_CancelFunction"));
    PR_LOG(modlog, 3, ("  hSession = 0x%x", hSession));
    start = PR_IntervalNow();
    rv = module_functions->C_CancelFunction(hSession);
    nssdbg_finish_time(&counter_C_CancelFunction,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
    return rv;
}

static PRInt32 counter_C_WaitForSlotEvent = 0;
static PRInt32 calls_C_WaitForSlotEvent = 0;
CK_RV NSSDBGC_WaitForSlotEvent(
  CK_FLAGS       flags,
  CK_SLOT_ID_PTR pSlot,
  CK_VOID_PTR    pRserved
)
{
    CK_RV rv;
    PRIntervalTime start;
    PR_AtomicIncrement(&calls_C_WaitForSlotEvent);
    PR_LOG(modlog, 1, ("C_WaitForSlotEvent"));
    PR_LOG(modlog, 3, ("  flags = 0x%x", flags));
    PR_LOG(modlog, 3, ("  pSlot = 0x%p", pSlot));
    PR_LOG(modlog, 3, ("  pRserved = 0x%p", pRserved));
    start = PR_IntervalNow();
    rv = module_functions->C_WaitForSlotEvent(flags,
                                 pSlot,
                                 pRserved);
    nssdbg_finish_time(&counter_C_WaitForSlotEvent,start);
    PR_LOG(modlog, 1, ("  rv = 0x%x\n", rv));
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

static void print_final_statistics(void)
{
    int total_calls = 0;
    PRInt32 total_time = 0;
    char *fname;
    FILE *outfile = NULL;

    fname = PR_GetEnv("NSS_OUTPUT_FILE");
    if (fname) {
	outfile = fopen(fname,"w+");
    }
    if (!outfile) {
	outfile = stdout;
    }
	

    fprintf(outfile,"%-25s %10s %11s %10s %10s\n", "Function", "# Calls", "Time (ms)", "Avg. (ms)", "% Time");
    fprintf(outfile,"\n");
    total_calls += calls_C_CancelFunction;
    total_time += counter_C_CancelFunction;
    total_calls += calls_C_CloseAllSessions;
    total_time += counter_C_CloseAllSessions;
    total_calls += calls_C_CloseSession;
    total_time += counter_C_CloseSession;
    total_calls += calls_C_CopyObject;
    total_time += counter_C_CopyObject;
    total_calls += calls_C_CreateObject;
    total_time += counter_C_CreateObject;
    total_calls += calls_C_Decrypt;
    total_time += counter_C_Decrypt;
    total_calls += calls_C_DecryptDigestUpdate;
    total_time += counter_C_DecryptDigestUpdate;
    total_calls += calls_C_DecryptFinal;
    total_time += counter_C_DecryptFinal;
    total_calls += calls_C_DecryptInit;
    total_time += counter_C_DecryptInit;
    total_calls += calls_C_DecryptUpdate;
    total_time += counter_C_DecryptUpdate;
    total_calls += calls_C_DecryptVerifyUpdate;
    total_time += counter_C_DecryptVerifyUpdate;
    total_calls += calls_C_DeriveKey;
    total_time += counter_C_DeriveKey;
    total_calls += calls_C_DestroyObject;
    total_time += counter_C_DestroyObject;
    total_calls += calls_C_Digest;
    total_time += counter_C_Digest;
    total_calls += calls_C_DigestEncryptUpdate;
    total_time += counter_C_DigestEncryptUpdate;
    total_calls += calls_C_DigestFinal;
    total_time += counter_C_DigestFinal;
    total_calls += calls_C_DigestInit;
    total_time += counter_C_DigestInit;
    total_calls += calls_C_DigestKey;
    total_time += counter_C_DigestKey;
    total_calls += calls_C_DigestUpdate;
    total_time += counter_C_DigestUpdate;
    total_calls += calls_C_Encrypt;
    total_time += counter_C_Encrypt;
    total_calls += calls_C_EncryptFinal;
    total_time += counter_C_EncryptFinal;
    total_calls += calls_C_EncryptInit;
    total_time += counter_C_EncryptInit;
    total_calls += calls_C_EncryptUpdate;
    total_time += counter_C_EncryptUpdate;
    total_calls += calls_C_Finalize;
    total_time += counter_C_Finalize;
    total_calls += calls_C_FindObjects;
    total_time += counter_C_FindObjects;
    total_calls += calls_C_FindObjectsFinal;
    total_time += counter_C_FindObjectsFinal;
    total_calls += calls_C_FindObjectsInit;
    total_time += counter_C_FindObjectsInit;
    total_calls += calls_C_GenerateKey;
    total_time += counter_C_GenerateKey;
    total_calls += calls_C_GenerateKeyPair;
    total_time += counter_C_GenerateKeyPair;
    total_calls += calls_C_GenerateRandom;
    total_time += counter_C_GenerateRandom;
    total_calls += calls_C_GetAttributeValue;
    total_time += counter_C_GetAttributeValue;
    total_calls += calls_C_GetFunctionList;
    total_time += counter_C_GetFunctionList;
    total_calls += calls_C_GetFunctionStatus;
    total_time += counter_C_GetFunctionStatus;
    total_calls += calls_C_GetInfo;
    total_time += counter_C_GetInfo;
    total_calls += calls_C_GetMechanismInfo;
    total_time += counter_C_GetMechanismInfo;
    total_calls += calls_C_GetMechanismList;
    total_time += counter_C_GetMechanismList;
    total_calls += calls_C_GetObjectSize;
    total_time += counter_C_GetObjectSize;
    total_calls += calls_C_GetOperationState;
    total_time += counter_C_GetOperationState;
    total_calls += calls_C_GetSessionInfo;
    total_time += counter_C_GetSessionInfo;
    total_calls += calls_C_GetSlotInfo;
    total_time += counter_C_GetSlotInfo;
    total_calls += calls_C_GetSlotList;
    total_time += counter_C_GetSlotList;
    total_calls += calls_C_GetTokenInfo;
    total_time += counter_C_GetTokenInfo;
    total_calls += calls_C_InitPIN;
    total_time += counter_C_InitPIN;
    total_calls += calls_C_InitToken;
    total_time += counter_C_InitToken;
    total_calls += calls_C_Initialize;
    total_time += counter_C_Initialize;
    total_calls += calls_C_Login;
    total_time += counter_C_Login;
    total_calls += calls_C_Logout;
    total_time += counter_C_Logout;
    total_calls += calls_C_OpenSession;
    total_time += counter_C_OpenSession;
    total_calls += calls_C_SeedRandom;
    total_time += counter_C_SeedRandom;
    total_calls += calls_C_SetAttributeValue;
    total_time += counter_C_SetAttributeValue;
    total_calls += calls_C_SetOperationState;
    total_time += counter_C_SetOperationState;
    total_calls += calls_C_SetPIN;
    total_time += counter_C_SetPIN;
    total_calls += calls_C_Sign;
    total_time += counter_C_Sign;
    total_calls += calls_C_SignEncryptUpdate;
    total_time += counter_C_SignEncryptUpdate;
    total_calls += calls_C_SignFinal;
    total_time += counter_C_SignFinal;
    total_calls += calls_C_SignInit;
    total_time += counter_C_SignInit;
    total_calls += calls_C_SignRecover;
    total_time += counter_C_SignRecover;
    total_calls += calls_C_SignRecoverInit;
    total_time += counter_C_SignRecoverInit;
    total_calls += calls_C_SignUpdate;
    total_time += counter_C_SignUpdate;
    total_calls += calls_C_UnwrapKey;
    total_time += counter_C_UnwrapKey;
    total_calls += calls_C_Verify;
    total_time += counter_C_Verify;
    total_calls += calls_C_VerifyFinal;
    total_time += counter_C_VerifyFinal;
    total_calls += calls_C_VerifyInit;
    total_time += counter_C_VerifyInit;
    total_calls += calls_C_VerifyRecover;
    total_time += counter_C_VerifyRecover;
    total_calls += calls_C_VerifyRecoverInit;
    total_time += counter_C_VerifyRecoverInit;
    total_calls += calls_C_VerifyUpdate;
    total_time += counter_C_VerifyUpdate;
    total_calls += calls_C_WaitForSlotEvent;
    total_time += counter_C_WaitForSlotEvent;
    total_calls += calls_C_WrapKey;
    total_time += counter_C_WrapKey;
    fprintf(outfile,"%-25s %10d %10d ", "C_CancelFunction", calls_C_CancelFunction, counter_C_CancelFunction);
    if (calls_C_CancelFunction > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_CancelFunction / (float)calls_C_CancelFunction);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_CancelFunction / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_CloseAllSessions", calls_C_CloseAllSessions, counter_C_CloseAllSessions);
    if (calls_C_CloseAllSessions > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_CloseAllSessions / (float)calls_C_CloseAllSessions);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_CloseAllSessions / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_CloseSession", calls_C_CloseSession, counter_C_CloseSession);
    if (calls_C_CloseSession > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_CloseSession / (float)calls_C_CloseSession);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_CloseSession / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_CopyObject", calls_C_CopyObject, counter_C_CopyObject);
    if (calls_C_CopyObject > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_CopyObject / (float)calls_C_CopyObject);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_CopyObject / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_CreateObject", calls_C_CreateObject, counter_C_CreateObject);
    if (calls_C_CreateObject > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_CreateObject / (float)calls_C_CreateObject);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_CreateObject / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Decrypt", calls_C_Decrypt, counter_C_Decrypt);
    if (calls_C_Decrypt > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Decrypt / (float)calls_C_Decrypt);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Decrypt / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DecryptDigestUpdate", calls_C_DecryptDigestUpdate, counter_C_DecryptDigestUpdate);
    if (calls_C_DecryptDigestUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DecryptDigestUpdate / (float)calls_C_DecryptDigestUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DecryptDigestUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DecryptFinal", calls_C_DecryptFinal, counter_C_DecryptFinal);
    if (calls_C_DecryptFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DecryptFinal / (float)calls_C_DecryptFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DecryptFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DecryptInit", calls_C_DecryptInit, counter_C_DecryptInit);
    if (calls_C_DecryptInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DecryptInit / (float)calls_C_DecryptInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DecryptInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DecryptUpdate", calls_C_DecryptUpdate, counter_C_DecryptUpdate);
    if (calls_C_DecryptUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DecryptUpdate / (float)calls_C_DecryptUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DecryptUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DecryptVerifyUpdate", calls_C_DecryptVerifyUpdate, counter_C_DecryptVerifyUpdate);
    if (calls_C_DecryptVerifyUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DecryptVerifyUpdate / (float)calls_C_DecryptVerifyUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DecryptVerifyUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DeriveKey", calls_C_DeriveKey, counter_C_DeriveKey);
    if (calls_C_DeriveKey > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DeriveKey / (float)calls_C_DeriveKey);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DeriveKey / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DestroyObject", calls_C_DestroyObject, counter_C_DestroyObject);
    if (calls_C_DestroyObject > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DestroyObject / (float)calls_C_DestroyObject);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DestroyObject / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Digest", calls_C_Digest, counter_C_Digest);
    if (calls_C_Digest > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Digest / (float)calls_C_Digest);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Digest / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DigestEncryptUpdate", calls_C_DigestEncryptUpdate, counter_C_DigestEncryptUpdate);
    if (calls_C_DigestEncryptUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DigestEncryptUpdate / (float)calls_C_DigestEncryptUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DigestEncryptUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DigestFinal", calls_C_DigestFinal, counter_C_DigestFinal);
    if (calls_C_DigestFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DigestFinal / (float)calls_C_DigestFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DigestFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DigestInit", calls_C_DigestInit, counter_C_DigestInit);
    if (calls_C_DigestInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DigestInit / (float)calls_C_DigestInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DigestInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DigestKey", calls_C_DigestKey, counter_C_DigestKey);
    if (calls_C_DigestKey > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DigestKey / (float)calls_C_DigestKey);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DigestKey / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_DigestUpdate", calls_C_DigestUpdate, counter_C_DigestUpdate);
    if (calls_C_DigestUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_DigestUpdate / (float)calls_C_DigestUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_DigestUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Encrypt", calls_C_Encrypt, counter_C_Encrypt);
    if (calls_C_Encrypt > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Encrypt / (float)calls_C_Encrypt);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Encrypt / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_EncryptFinal", calls_C_EncryptFinal, counter_C_EncryptFinal);
    if (calls_C_EncryptFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_EncryptFinal / (float)calls_C_EncryptFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_EncryptFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_EncryptInit", calls_C_EncryptInit, counter_C_EncryptInit);
    if (calls_C_EncryptInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_EncryptInit / (float)calls_C_EncryptInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_EncryptInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_EncryptUpdate", calls_C_EncryptUpdate, counter_C_EncryptUpdate);
    if (calls_C_EncryptUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_EncryptUpdate / (float)calls_C_EncryptUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_EncryptUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Finalize", calls_C_Finalize, counter_C_Finalize);
    if (calls_C_Finalize > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Finalize / (float)calls_C_Finalize);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Finalize / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_FindObjects", calls_C_FindObjects, counter_C_FindObjects);
    if (calls_C_FindObjects > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_FindObjects / (float)calls_C_FindObjects);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_FindObjects / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_FindObjectsFinal", calls_C_FindObjectsFinal, counter_C_FindObjectsFinal);
    if (calls_C_FindObjectsFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_FindObjectsFinal / (float)calls_C_FindObjectsFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_FindObjectsFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_FindObjectsInit", calls_C_FindObjectsInit, counter_C_FindObjectsInit);
    if (calls_C_FindObjectsInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_FindObjectsInit / (float)calls_C_FindObjectsInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_FindObjectsInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GenerateKey", calls_C_GenerateKey, counter_C_GenerateKey);
    if (calls_C_GenerateKey > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GenerateKey / (float)calls_C_GenerateKey);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GenerateKey / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GenerateKeyPair", calls_C_GenerateKeyPair, counter_C_GenerateKeyPair);
    if (calls_C_GenerateKeyPair > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GenerateKeyPair / (float)calls_C_GenerateKeyPair);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GenerateKeyPair / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GenerateRandom", calls_C_GenerateRandom, counter_C_GenerateRandom);
    if (calls_C_GenerateRandom > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GenerateRandom / (float)calls_C_GenerateRandom);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GenerateRandom / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetAttributeValue", calls_C_GetAttributeValue, counter_C_GetAttributeValue);
    if (calls_C_GetAttributeValue > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetAttributeValue / (float)calls_C_GetAttributeValue);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetAttributeValue / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetFunctionList", calls_C_GetFunctionList, counter_C_GetFunctionList);
    if (calls_C_GetFunctionList > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetFunctionList / (float)calls_C_GetFunctionList);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetFunctionList / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetFunctionStatus", calls_C_GetFunctionStatus, counter_C_GetFunctionStatus);
    if (calls_C_GetFunctionStatus > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetFunctionStatus / (float)calls_C_GetFunctionStatus);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetFunctionStatus / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetInfo", calls_C_GetInfo, counter_C_GetInfo);
    if (calls_C_GetInfo > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetInfo / (float)calls_C_GetInfo);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetInfo / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetMechanismInfo", calls_C_GetMechanismInfo, counter_C_GetMechanismInfo);
    if (calls_C_GetMechanismInfo > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetMechanismInfo / (float)calls_C_GetMechanismInfo);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetMechanismInfo / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetMechanismList", calls_C_GetMechanismList, counter_C_GetMechanismList);
    if (calls_C_GetMechanismList > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetMechanismList / (float)calls_C_GetMechanismList);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetMechanismList / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetObjectSize", calls_C_GetObjectSize, counter_C_GetObjectSize);
    if (calls_C_GetObjectSize > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetObjectSize / (float)calls_C_GetObjectSize);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetObjectSize / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetOperationState", calls_C_GetOperationState, counter_C_GetOperationState);
    if (calls_C_GetOperationState > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetOperationState / (float)calls_C_GetOperationState);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetOperationState / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetSessionInfo", calls_C_GetSessionInfo, counter_C_GetSessionInfo);
    if (calls_C_GetSessionInfo > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetSessionInfo / (float)calls_C_GetSessionInfo);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetSessionInfo / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetSlotInfo", calls_C_GetSlotInfo, counter_C_GetSlotInfo);
    if (calls_C_GetSlotInfo > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetSlotInfo / (float)calls_C_GetSlotInfo);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetSlotInfo / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetSlotList", calls_C_GetSlotList, counter_C_GetSlotList);
    if (calls_C_GetSlotList > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetSlotList / (float)calls_C_GetSlotList);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetSlotList / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_GetTokenInfo", calls_C_GetTokenInfo, counter_C_GetTokenInfo);
    if (calls_C_GetTokenInfo > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_GetTokenInfo / (float)calls_C_GetTokenInfo);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_GetTokenInfo / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_InitPIN", calls_C_InitPIN, counter_C_InitPIN);
    if (calls_C_InitPIN > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_InitPIN / (float)calls_C_InitPIN);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_InitPIN / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_InitToken", calls_C_InitToken, counter_C_InitToken);
    if (calls_C_InitToken > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_InitToken / (float)calls_C_InitToken);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_InitToken / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Initialize", calls_C_Initialize, counter_C_Initialize);
    if (calls_C_Initialize > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Initialize / (float)calls_C_Initialize);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Initialize / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Login", calls_C_Login, counter_C_Login);
    if (calls_C_Login > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Login / (float)calls_C_Login);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Login / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Logout", calls_C_Logout, counter_C_Logout);
    if (calls_C_Logout > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Logout / (float)calls_C_Logout);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Logout / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_OpenSession", calls_C_OpenSession, counter_C_OpenSession);
    if (calls_C_OpenSession > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_OpenSession / (float)calls_C_OpenSession);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_OpenSession / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SeedRandom", calls_C_SeedRandom, counter_C_SeedRandom);
    if (calls_C_SeedRandom > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SeedRandom / (float)calls_C_SeedRandom);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SeedRandom / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SetAttributeValue", calls_C_SetAttributeValue, counter_C_SetAttributeValue);
    if (calls_C_SetAttributeValue > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SetAttributeValue / (float)calls_C_SetAttributeValue);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SetAttributeValue / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SetOperationState", calls_C_SetOperationState, counter_C_SetOperationState);
    if (calls_C_SetOperationState > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SetOperationState / (float)calls_C_SetOperationState);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SetOperationState / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SetPIN", calls_C_SetPIN, counter_C_SetPIN);
    if (calls_C_SetPIN > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SetPIN / (float)calls_C_SetPIN);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SetPIN / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Sign", calls_C_Sign, counter_C_Sign);
    if (calls_C_Sign > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Sign / (float)calls_C_Sign);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Sign / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignEncryptUpdate", calls_C_SignEncryptUpdate, counter_C_SignEncryptUpdate);
    if (calls_C_SignEncryptUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignEncryptUpdate / (float)calls_C_SignEncryptUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignEncryptUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignFinal", calls_C_SignFinal, counter_C_SignFinal);
    if (calls_C_SignFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignFinal / (float)calls_C_SignFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignInit", calls_C_SignInit, counter_C_SignInit);
    if (calls_C_SignInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignInit / (float)calls_C_SignInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignRecover", calls_C_SignRecover, counter_C_SignRecover);
    if (calls_C_SignRecover > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignRecover / (float)calls_C_SignRecover);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignRecover / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignRecoverInit", calls_C_SignRecoverInit, counter_C_SignRecoverInit);
    if (calls_C_SignRecoverInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignRecoverInit / (float)calls_C_SignRecoverInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignRecoverInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_SignUpdate", calls_C_SignUpdate, counter_C_SignUpdate);
    if (calls_C_SignUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_SignUpdate / (float)calls_C_SignUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_SignUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_UnwrapKey", calls_C_UnwrapKey, counter_C_UnwrapKey);
    if (calls_C_UnwrapKey > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_UnwrapKey / (float)calls_C_UnwrapKey);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_UnwrapKey / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_Verify", calls_C_Verify, counter_C_Verify);
    if (calls_C_Verify > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_Verify / (float)calls_C_Verify);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_Verify / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_VerifyFinal", calls_C_VerifyFinal, counter_C_VerifyFinal);
    if (calls_C_VerifyFinal > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_VerifyFinal / (float)calls_C_VerifyFinal);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_VerifyFinal / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_VerifyInit", calls_C_VerifyInit, counter_C_VerifyInit);
    if (calls_C_VerifyInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_VerifyInit / (float)calls_C_VerifyInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_VerifyInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_VerifyRecover", calls_C_VerifyRecover, counter_C_VerifyRecover);
    if (calls_C_VerifyRecover > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_VerifyRecover / (float)calls_C_VerifyRecover);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_VerifyRecover / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_VerifyRecoverInit", calls_C_VerifyRecoverInit, counter_C_VerifyRecoverInit);
    if (calls_C_VerifyRecoverInit > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_VerifyRecoverInit / (float)calls_C_VerifyRecoverInit);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_VerifyRecoverInit / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_VerifyUpdate", calls_C_VerifyUpdate, counter_C_VerifyUpdate);
    if (calls_C_VerifyUpdate > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_VerifyUpdate / (float)calls_C_VerifyUpdate);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_VerifyUpdate / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_WaitForSlotEvent", calls_C_WaitForSlotEvent, counter_C_WaitForSlotEvent);
    if (calls_C_WaitForSlotEvent > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_WaitForSlotEvent / (float)calls_C_WaitForSlotEvent);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_WaitForSlotEvent / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"%-25s %10d %10d ", "C_WrapKey", calls_C_WrapKey, counter_C_WrapKey);
    if (calls_C_WrapKey > 0) {
        fprintf(outfile,"%10.2f", (float)counter_C_WrapKey / (float)calls_C_WrapKey);
    } else {
        fprintf(outfile,"%10.2f", 0.0);
    }
    fprintf(outfile,"%10.2f", (float)counter_C_WrapKey / (float)total_time * 100);
    fprintf(outfile,"\n");
    fprintf(outfile,"\n");

    fprintf(outfile,"%25s %10d %10d\n", "Totals", total_calls, total_time);
    fprintf(outfile,"\n\nMaximum number of concurrent open sessions: %d\n\n", maxOpenSessions);
    fflush (outfile);
    if (outfile != stdout) {
	fclose(outfile);
    }
}

