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
#ifndef __SSM_RSRCIDS_H__
#define __SSM_RSRCIDS_H__

#include "ssmdefs.h"

/*
 * IMPORTANT:
 *
 * To preserve backward compatibility as much as possible, always add new 
 * values to either one of the enumeration tables at the end of the table.
 */

typedef enum 
{
    SSM_RESTYPE_NULL = 0L,
    SSM_RESTYPE_RESOURCE,
    SSM_RESTYPE_CONNECTION,
    SSM_RESTYPE_CONTROL_CONNECTION,
    SSM_RESTYPE_DATA_CONNECTION,
    SSM_RESTYPE_SSL_DATA_CONNECTION,
    SSM_RESTYPE_PKCS7_DECODE_CONNECTION,
    SSM_RESTYPE_PKCS7_ENCODE_CONNECTION,
    SSM_RESTYPE_HASH_CONNECTION,
    
    SSM_RESTYPE_CERTIFICATE,
    SSM_RESTYPE_SSL_SOCKET_STATUS,
    SSM_RESTYPE_PKCS7_CONTENT_INFO,
    SSM_RESTYPE_KEY_PAIR,
    SSM_RESTYPE_CRMF_REQUEST,
    SSM_RESTYPE_KEYGEN_CONTEXT,
    SSM_RESTYPE_SECADVISOR_CONTEXT,
    SSM_RESTYPE_SIGNTEXT,
    SSM_RESTYPE_PKCS12_CONTEXT,

    SSM_RESTYPE_SDR_CONTEXT,  /* Internal - Not used by client protocol */

    SSM_RESTYPE_MAX
} SSMResourceType;

/* Attribute/resource types */

/* Attribute IDs */
typedef enum
{
    SSM_FID_NULL = (CMUint32) 0, /* placeholder */

    /* Connection attributes */
    SSM_FID_CONN_ALIVE,
    SSM_FID_CONN_PARENT,

    /* Data connection attributes */
    SSM_FID_CONN_DATA_PENDING,

    /* SSL data connection attributes */
    SSM_FID_SSLDATA_SOCKET_STATUS,
    SSM_FID_SSLDATA_ERROR_VALUE,

    /* PKCS7 decode connection attributes */
    SSM_FID_P7CONN_CONTENT_INFO,
    SSM_FID_P7CONN_RETURN_VALUE,
    SSM_FID_P7CONN_ERROR_VALUE,

    /* Hash connection attributes */
    SSM_FID_HASHCONN_RESULT,

    /* Certificate attributes */
    SSM_FID_CERT_SUBJECT_NAME,
    SSM_FID_CERT_ISSUER_NAME,
    SSM_FID_CERT_SERIAL_NUMBER,
    SSM_FID_CERT_EXP_DATE,
    SSM_FID_CERT_FINGERPRINT,
    SSM_FID_CERT_COMMON_NAME,
    SSM_FID_CERT_NICKNAME,
    SSM_FID_CERT_ORG_NAME,
    SSM_FID_CERT_HTML_CERT,
    SSM_FID_CERT_PICKLE_CERT,
    SSM_FID_CERT_CERTKEY,
    SSM_FID_CERT_FIND_CERT_ISSUER,
    SSM_FID_CERT_EMAIL_ADDRESS,
    SSM_FID_CERT_ISPERM,

    /* SSL socket status attributes */
    SSM_FID_SSS_KEYSIZE,
    SSM_FID_SSS_SECRET_KEYSIZE,
    SSM_FID_SSS_CERT_ID,
    SSM_FID_SSS_CIPHER_NAME,
    SSM_FID_SSS_SECURITY_LEVEL,
    SSM_FID_SSS_HTML_STATUS,

    /* PKCS7 content info attributes */
    SSM_FID_P7CINFO_IS_SIGNED,
    SSM_FID_P7CINFO_IS_ENCRYPTED,
    SSM_FID_P7CINFO_SIGNER_CERT,

    /* CRMF ID's */
    SSM_FID_CRMFREQ_REGTOKEN,
    SSM_FID_CRMFREQ_AUTHENTICATOR,
    SSM_FID_CRMFREQ_EXTENSIONS,
    SSM_FID_CRMFREQ_KEY_TYPE,
    SSM_FID_CRMFREQ_DN,

    /* Security advisor context */
    SSM_FID_SECADVISOR_URL,
    SSM_FID_SECADVISOR_WIDTH,
    SSM_FID_SECADVISOR_HEIGHT,

    /* Sign Text */
    SSM_FID_SIGNTEXT_RESULT,

    /* Key Gen ID's */
    SSM_FID_KEYGEN_ESCROW_AUTHORITY,

    /* Key Pair ID's */
    SSM_FID_KEYPAIR_KEY_GEN_TYPE,

    /* Session Attributes */
    SSM_FID_DEFAULT_EMAIL_RECIPIENT_CERT,
    SSM_FID_DEFAULT_EMAIL_SIGNER_CERT,

    /* Client Context Attribute */
    SSM_FID_CLIENT_CONTEXT,

	/* Resource Error */
	SSM_FID_RESOURCE_ERROR,

    SSM_FID_KEYGEN_SLOT_NAME,
    SSM_FID_DISABLE_ESCROW_WARN,
    SSM_FID_KEYGEN_TOKEN_NAME,

    SSM_FID_SSLDATA_DISCARD_SOCKET_STATUS,
    SSM_FID_CHOOSE_TOKEN_URL,
    SSM_FID_INIT_DB_URL,
    SSM_FID_SSS_CA_NAME,

    SSM_FID_MAX /* placeholder */
} SSMAttributeID;

#endif
