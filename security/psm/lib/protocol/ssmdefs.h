/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#ifndef __SSMDEFS_H__
#define __SSMDEFS_H__

/* Basic type definitions for both client and server. */
typedef long CMInt32;
typedef unsigned long CMUint32;
typedef long SSMResourceID;

typedef int SSMStatus;

#define PSM_PORT                        11111
#define PSM_DATA_PORT                   11113 /* needs to be removed */
 
typedef enum _CMTStatus {
  CMTFailure = -1,
  CMTSuccess = 0
} CMTStatus;

typedef enum {
   CM_FALSE = 0,
   CM_TRUE = 1
} CMBool;
 
typedef struct CMTItemStr {
  CMUint32 type;
  unsigned char *data;
  unsigned int len;
} CMTItem;

/* A length-encoded string. */
struct _SSMString {
  CMUint32 m_length;
  char m_data;
};
typedef struct _SSMString SSMString;

#define SSM_PROTOCOL_VERSION 0x00000051

#define SSM_INVALID_RESOURCE 0x00000000
#define SSM_GLOBAL_RESOURCE  0x00000001
#define SSM_SESSION_RESOURCE 0x00000002

/* Message category flags */
#define SSM_REQUEST_MESSAGE     0x10000000
#define SSM_REPLY_OK_MESSAGE    0x20000000
#define SSM_REPLY_ERR_MESSAGE   0x30000000
#define SSM_EVENT_MESSAGE       0x40000000

/* Message types */
#define SSM_DATA_CONNECTION   0x00001000
#define SSM_OBJECT_SIGNING    0x00002000
#define SSM_RESOURCE_ACTION   0x00003000
#define SSM_CERT_ACTION       0x00004000
#define SSM_PKCS11_ACTION     0x00005000
#define SSM_CRMF_ACTION       0x00006000
#define SSM_FORMSIGN_ACTION   0x00007000
#define SSM_LOCALIZED_TEXT    0x00008000
#define SSM_HELLO_MESSAGE     0x00009000
#define SSM_SECURITY_ADVISOR  0x0000a000
#define SSM_SEC_CFG_ACTION    0x0000b000
#define SSM_KEYGEN_TAG        0x0000c000
#define SSM_PREF_ACTION       0x0000d000
#define SSM_MISC_ACTION       0x0000f000

/* Data connection messages subtypes */ 
#define SSM_SSL_CONNECTION      0x00000100
#define SSM_PKCS7DECODE_STREAM  0x00000200
#define SSM_PKCS7ENCODE_STREAM  0x00000300
#define SSM_HASH_STREAM         0x00000400
#define SSM_TLS_CONNECTION      0x00000500
#define SSM_PROXY_CONNECTION    0x00000600

/* Object signing message subtypes */
#define SSM_VERIFY_RAW_SIG      0x00000100
#define SSM_VERIFY_DETACHED_SIG 0x00000200
#define SSM_CREATE_SIGNED       0x00000300
#define SSM_CREATE_ENCRYPTED    0x00000400

/* Resource access messages subtypes  */
#define SSM_CREATE_RESOURCE           0x00000100
#define SSM_DESTROY_RESOURCE          0x00000200
#define SSM_GET_ATTRIBUTE             0x00000300
#define SSM_CONSERVE_RESOURCE         0x00000400
#define SSM_DUPLICATE_RESOURCE        0x00000500
#define SSM_SET_ATTRIBUTE             0x00000600
#define SSM_TLS_STEPUP                0x00000700
#define SSM_PROXY_STEPUP              0x00000800

/* Further specification for resource access messages */
#define SSM_SSLSocket_Status    0x00000010

#define SSM_NO_ATTRIBUTE        0x00000000
#define SSM_NUMERIC_ATTRIBUTE   0x00000010
#define SSM_STRING_ATTRIBUTE    0x00000020
#define SSM_RID_ATTRIBUTE       0x00000030

#define SSM_PICKLE_RESOURCE             0x00000010
#define SSM_UNPICKLE_RESOURCE           0x00000020
#define SSM_PICKLE_SECURITY_STATUS      0x00000030

/* Certificate access message subtypes */
#define SSM_IMPORT_CERT         0x00000100
#define SSM_VERIFY_CERT         0x00000200
#define SSM_FIND_BY_NICKNAME    0x00000300
#define SSM_FIND_BY_KEY         0x00000400
#define SSM_FIND_BY_EMAILADDR   0x00000500
#define SSM_ADD_TO_DB           0x00000600
#define SSM_DECODE_CERT         0x00000700
#define SSM_MATCH_USER_CERT     0x00000800
#define SSM_DESTROY_CERT        0x00000900
#define SSM_DECODE_TEMP_CERT    0x00000a00
#define SSM_REDIRECT_COMPARE    0x00000b00
#define SSM_DECODE_CRL          0x00000c00
#define SSM_EXTENSION_VALUE     0x00000d00
#define SSM_HTML_INFO           0x00000e00

/* message subtypes used for KEYGEN form tag */
#define SSM_GET_KEY_CHOICE       0x00000100
#define SSM_KEYGEN_START         0x00000200
#define SSM_KEYGEN_TOKEN         0x00000300
#define SSM_KEYGEN_PASSWORD      0x00000400
#define SSM_KEYGEN_DONE          0x00000500

#define SSM_CREATE_KEY_PAIR    0x00000100
#define SSM_FINISH_KEY_GEN     0x00000200
#define SSM_ADD_NEW_MODULE     0x00000300
#define SSM_DEL_MODULE         0x00000400
#define SSM_LOGOUT_ALL         0x00000500
#define SSM_ENABLED_CIPHERS    0x00000600

#define SSM_CREATE_CRMF_REQ    0x00000100
#define SSM_DER_ENCODE_REQ     0x00000200
#define SSM_PROCESS_CMMF_RESP  0x00000300
#define SSM_CHALLENGE          0x00000400

#define SSM_SIGN_TEXT  0x00000100

/* Security Config subtypes */
#define SSM_ADD_CERT_TO_TEMP_DB     0x00000100
#define SSM_ADD_TEMP_CERT_TO_DB     0x00000200
#define SSM_DELETE_PERM_CERTS       0x00000300
#define SSM_FIND_CERT_KEY           0x00000400
#define SSM_GET_CERT_PROP_BY_KEY    0x00000500
#define SSM_CERT_INDEX_ENUM         0x00000600

/* subcategories for SSM_FIND_CERT_KEY and SSM_CERT_INDEX_ENUM */
#define SSM_FIND_KEY_BY_NICKNAME    0x00000010
#define SSM_FIND_KEY_BY_EMAIL_ADDR  0x00000020
#define SSM_FIND_KEY_BY_DN          0x00000030

/* subcategories for SSM_GET_CERT_PROP_BY_KEY */
#define SSM_SECCFG_GET_NICKNAME      0x00000010
#define SSM_SECCFG_GET_EMAIL_ADDR    0x00000020
#define SSM_SECCFG_GET_DN            0x00000030
#define SSM_SECCFG_GET_TRUST         0x00000040
#define SSM_SECCFG_CERT_IS_PERM      0x00000050
#define SSM_SECCFG_GET_NOT_BEFORE    0x00000060
#define SSM_SECCFG_GET_NOT_AFTER     0x00000070
#define SSM_SECCFG_GET_SERIAL_NO     0x00000080
#define SSM_SECCFG_GET_ISSUER        0x00000090
#define SSM_SECCFG_GET_ISSUER_KEY    0x000000a0
#define SSM_SECCFG_GET_SUBJECT_NEXT  0x000000b0
#define SSM_SECCFG_GET_SUBJECT_PREV  0x000000c0

/* Misc requests */
#define SSM_MISC_GET_RNG_DATA      0x00000100
#define SSM_MISC_PUT_RNG_DATA      0x00000200
#define SSM_MISC_SDR_ENCRYPT       0x00000300
#define SSM_MISC_SDR_DECRYPT       0x00000400
#define SSM_MISC_UI                0x00000500

/* specific UI requests */
#define SSM_UI_CHANGE_PASSWORD     0x00000010

#define SSM_SDR_ENCRYPT_REQUEST \
  (SSM_REQUEST_MESSAGE|SSM_MISC_ACTION|SSM_MISC_SDR_ENCRYPT)
#define SSM_SDR_ENCRYPT_REPLY \
  (SSM_REPLY_OK_MESSAGE|SSM_MISC_ACTION|SSM_MISC_SDR_ENCRYPT)
#define SSM_SDR_DECRYPT_REQUEST \
  (SSM_REQUEST_MESSAGE|SSM_MISC_ACTION|SSM_MISC_SDR_DECRYPT)
#define SSM_SDR_DECRYPT_REPLY \
  (SSM_REPLY_OK_MESSAGE|SSM_MISC_ACTION|SSM_MISC_SDR_DECRYPT)

/* Type masks for message types */
#define SSM_CATEGORY_MASK   0xF0000000
#define SSM_TYPE_MASK       0x0000F000
#define SSM_SUBTYPE_MASK    0x00000F00
#define SSM_SPECIFIC_MASK   0x000000F0

typedef struct SSMAttributeValue {
  CMUint32 type;
  union {
    SSMResourceID rid;
    CMTItem string;
    CMInt32 numeric;
  } u;
} SSMAttributeValue;

typedef enum {
  rsaEnc, rsaDualUse, rsaSign, rsaNonrepudiation, rsaSignNonrepudiation,
  dhEx, dsaSignNonrepudiation, dsaSign, dsaNonrepudiation, invalidKeyGen
} SSMKeyGenType;

typedef enum {
  ssmUnknownPolicy= -1,ssmDomestic=0, ssmExport=1, ssmFrance=2
} SSMPolicyType;

/* These are the localized strings that PSM can feed back to 
 * the plug-in.  These will initially be used by the plug-in for 
 * JavaScript purposes to pop up alert/confirm dialogs that would 
 * cause nightmares to do if we sent UI events.
 */
typedef enum {
  SSM_STRING_BAD_PK11_LIB_PARAM,
  SSM_STRING_BAD_PK11_LIB_PATH,
  SSM_STRING_ADD_MOD_SUCCESS,
  SSM_STRING_DUP_MOD_FAILURE,
  SSM_STRING_ADD_MOD_FAILURE,
  SSM_STRING_BAD_MOD_NAME,
  SSM_STRING_EXT_MOD_DEL,
  SSM_STRING_INT_MOD_DEL,
  SSM_STRING_MOD_DEL_FAIL,
  SSM_STRING_ADD_MOD_WARN,
  SSM_STRING_MOD_PROMPT,
  SSM_STRING_DLL_PROMPT,
  SSM_STRING_DEL_MOD_WARN,
  SSM_STRING_INVALID_CRL,
  SSM_STRING_INVALID_CKL,
  SSM_STRING_ROOT_CKL_CERT_NOT_FOUND,
  SSM_STRING_BAD_CRL_SIGNATURE,
  SSM_STRING_BAD_CKL_SIGNATURE,
  SSM_STRING_ERR_ADD_CRL,
  SSM_STRING_ERR_ADD_CKL,
  SSM_STRING_JAVASCRIPT_DISABLED
} SSMLocalizedString;

/* Event types */
#define SSM_UI_EVENT                    0x00001000
#define SSM_TASK_COMPLETED_EVENT        0x00002000
#define SSM_FILE_PATH_EVENT             0x00003000
#define SSM_PROMPT_EVENT                0x00004000
#define SSM_AUTH_EVENT                  0x00007000
#define SSM_SAVE_PREF_EVENT             0x00008000
#define SSM_MISC_EVENT                  0x0000f000

/* Flags used in Create SSL Data request */
#define SSM_REQUEST_SSL_DATA_SSL         0x00000001
#define SSM_REQUEST_SSL_DATA_PROXY       0x00000002
#define SSM_REQUEST_SSL_CONNECTION_MASK  0x00000003

/* Create typedefs for the various #defines */
typedef CMUint32 SSMMessageCategory;
typedef CMUint32 SSMMessageType;
typedef CMUint32 SSMDataConnectionSType;
typedef CMUint32 SSMObjSignSType;
typedef CMUint32 SSMResourceAccessSType;
typedef CMUint32 SSMCreateResource;
typedef CMUint32 SSMResourceAttrType;
typedef CMUint32 SSMResourceConsv;
typedef CMUint32 SSMCertAccessSType;
typedef CMUint32 SSMKeyGenTagProcessType;
typedef CMUint32 SSMPKCS11Actions;
typedef CMUint32 SSMCRMFAction;
typedef CMUint32 SSMFormSignAction;
typedef CMUint32 SSMSecCfgAction;
typedef CMUint32 SSMSecCfgFindByType;
typedef CMUint32 SSMSecCfgGetCertPropType;
typedef CMUint32 SSMMiscRequestType;
typedef CMUint32 SSMMessageMaskType;
typedef CMUint32 SSMEventType;
typedef CMUint32 SSMSSLConnectionRequestType;

/*
 * This string is version that can be used to assemble any
 * version information by the apllication using the protocol 
 * library.
 */
extern char SSMVersionString[];
#ifdef __cplusplus
extern "C" {
char * SSM_GetVersionString(void);
#endif
#ifdef __cplusplus
}
#endif

/* What type of client */
typedef enum
{
    SSM_NOINFO,
    SSM_COMPOSE,
    SSM_MAIL_MESSAGE,
    SSM_NEWS_MESSAGE,
    SSM_SNEWS_MESSAGE,
    SSM_BROWSER
} SSMClientType;

#endif /* __SSMDEFS_H__ */
