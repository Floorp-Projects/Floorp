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
 *   Sun Microsystems
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
/*
 * pkix_pl_httpcertstore.h
 *
 * HTTPCertstore Object Type Definition
 *
 */

#ifndef _PKIX_PL_HTTPCERTSTORE_H
#define _PKIX_PL_HTTPCERTSTORE_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RCVBUFSIZE              512

typedef SECStatus (*sec_pkcs7_cipher_function) (void *,
						unsigned char *,
						unsigned *,
						unsigned int,
						const unsigned char *,
						unsigned int);
typedef SECStatus (*sec_pkcs7_cipher_destroy) (void *, PRBool);

#define BLOCK_SIZE 4096

struct sec_pkcs7_cipher_object {
    void *cx;
    sec_pkcs7_cipher_function doit;
    sec_pkcs7_cipher_destroy destroy;
    PRBool encrypt;
    int block_size;
    int pad_size;
    int pending_count;
    unsigned char pending_buf[BLOCK_SIZE];
};

typedef struct sec_pkcs7_cipher_object sec_PKCS7CipherObject;

struct sec_pkcs7_decoder_worker {
    int depth;
    int digcnt;
    void **digcxs;
    void **   /* const SECHashObject **/ digobjs;
    void *   /* sec_PKCS7CipherObject * */ decryptobj;
    PRBool saw_contents;
};

struct SEC_PKCS7DecoderContextStr {
    SEC_ASN1DecoderContext *dcx;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7DecoderContentCallback cb;
    void *cb_arg;
    SECKEYGetPasswordKey pwfn;
    void *pwfn_arg;
    struct sec_pkcs7_decoder_worker worker;
    PRArenaPool *tmp_poolp;
    int error;
    SEC_PKCS7GetDecryptKeyCallback dkcb;
    void *dkcb_arg;
    SEC_PKCS7DecryptionAllowedCallback decrypt_allowed_cb;
};

struct PKIX_PL_HttpCertStoreContextStruct {
        const SEC_HttpClientFcn *client;
        SEC_HTTP_SERVER_SESSION serverSession;
        SEC_HTTP_REQUEST_SESSION requestSession;
	char *path;
};

/* see source file for function documentation */

PKIX_Error *pkix_pl_HttpCertStoreContext_RegisterSelf(void *plContext);

PKIX_Error *
pkix_pl_HttpCertStore_CreateWithAsciiName(
        PKIX_PL_HttpClient *client,
	char *locationAscii,
        PKIX_CertStore **pCertStore,
        void *plContext);

PKIX_Error *
pkix_HttpCertStore_FindSocketConnection(
        PRIntervalTime timeout,
        char *hostname,
        PRUint16 portnum,
        PRErrorCode *pStatus,
        PKIX_PL_Socket **pSocket,
        void *plContext);

PKIX_Error *
pkix_pl_HttpCertStore_ProcessCertResponse(
	PRUint16 responseCode,
	const char *responseContentType,
	const char *responseData,
        PRUint32 responseDataLen,
	PKIX_List **pCertList,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_HTTPCERTSTORE_H */
