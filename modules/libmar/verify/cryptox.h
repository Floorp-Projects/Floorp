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
 * The Original Code is cryptographic wrappers for Mozilla archive code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#ifndef CRYPTOX_H
#define CRYPTOX_H

#define XP_MIN_SIGNATURE_LEN_IN_BYTES 256

#define CryptoX_Result int
#define CryptoX_Success 0
#define CryptoX_Error (-1)
#define CryptoX_Succeeded(X) ((X) == CryptoX_Success)
#define CryptoX_Failed(X) ((X) != CryptoX_Success)

#if defined(MAR_NSS)

#include "nss_secutil.h"

CryptoX_Result NSS_LoadPublicKey(const char *certNickname, 
                                 SECKEYPublicKey **publicKey, 
                                 CERTCertificate **cert);
CryptoX_Result NSS_VerifyBegin(VFYContext **ctx, 
                               SECKEYPublicKey * const *publicKey);
CryptoX_Result NSS_VerifySignature(VFYContext * const *ctx , 
                                   const unsigned char *signature, 
                                   unsigned int signatureLen);

#define CryptoX_InvalidHandleValue NULL
#define CryptoX_ProviderHandle void*
#define CryptoX_SignatureHandle VFYContext *
#define CryptoX_PublicKey SECKEYPublicKey *
#define CryptoX_Certificate CERTCertificate *
#define CryptoX_InitCryptoProvider(CryptoHandle) \
  CryptoX_Success
#define CryptoX_VerifyBegin(CryptoHandle, SignatureHandle, PublicKey) \
  NSS_VerifyBegin(SignatureHandle, PublicKey)
#define CryptoX_VerifyUpdate(SignatureHandle, buf, len) \
  VFY_Update(*SignatureHandle, (const unsigned char*)(buf), len)
#define CryptoX_LoadPublicKey(CryptoHandle, certData, dataSize, \
                              publicKey, certName, cert) \
  NSS_LoadPublicKey(certName, publicKey, cert)
#define CryptoX_VerifySignature(hash, publicKey, signedData, len) \
  NSS_VerifySignature(hash, (const unsigned char *)(signedData), len)
#define CryptoX_FreePublicKey(key) \
  SECKEY_DestroyPublicKey(*key)
#define CryptoX_FreeCertificate(cert) \
  CERT_DestroyCertificate(*cert)

#elif defined(XP_WIN) 

#include <windows.h>
#include <wincrypt.h>

CryptoX_Result CryptoAPI_InitCryptoContext(HCRYPTPROV *provider);
CryptoX_Result CryptoAPI_LoadPublicKey(HCRYPTPROV hProv, 
                                       BYTE *certData,
                                       DWORD sizeOfCertData,
                                       HCRYPTKEY *publicKey,
                                       HCERTSTORE *cert);
CryptoX_Result CryptoAPI_VerifyBegin(HCRYPTPROV provider, HCRYPTHASH* hash);
CryptoX_Result CryptoAPI_VerifyUpdate(HCRYPTHASH* hash, 
                                      BYTE *buf, DWORD len);
CryptoX_Result CyprtoAPI_VerifySignature(HCRYPTHASH *hash, 
                                         HCRYPTKEY *pubKey,
                                         const BYTE *signature, 
                                         DWORD signatureLen);

#define CryptoX_InvalidHandleValue ((ULONG_PTR)NULL)
#define CryptoX_ProviderHandle HCRYPTPROV
#define CryptoX_SignatureHandle HCRYPTHASH
#define CryptoX_PublicKey HCRYPTKEY
#define CryptoX_Certificate HCERTSTORE
#define CryptoX_InitCryptoProvider(CryptoHandle) \
  CryptoAPI_InitCryptoContext(CryptoHandle)
#define CryptoX_VerifyBegin(CryptoHandle, SignatureHandle, PublicKey) \
  CryptoAPI_VerifyBegin(CryptoHandle, SignatureHandle)
#define CryptoX_VerifyUpdate(SignatureHandle, buf, len) \
  CryptoAPI_VerifyUpdate(SignatureHandle, (BYTE *)(buf), len)
#define CryptoX_LoadPublicKey(CryptoHandle, certData, dataSize, \
                              publicKey, certName, cert) \
  CryptoAPI_LoadPublicKey(CryptoHandle, (BYTE*)(certData), \
                          dataSize, publicKey, cert)
#define CryptoX_VerifySignature(hash, publicKey, signedData, len) \
  CyprtoAPI_VerifySignature(hash, publicKey, signedData, len)
#define CryptoX_FreePublicKey(key) \
  CryptDestroyKey(*(key))
#define CryptoX_FreeCertificate(cert) \
  CertCloseStore(*(cert), CERT_CLOSE_STORE_FORCE_FLAG);

#else

/* This default implementation is necessary because we don't want to
 * link to NSS from updater code on non Windows platforms.  On Windows
 * we use CyrptoAPI instead of NSS.  We don't call any function as they
 * would just fail, but this simplifies linking.
 */

#define CryptoX_InvalidHandleValue NULL
#define CryptoX_ProviderHandle void*
#define CryptoX_SignatureHandle void*
#define CryptoX_PublicKey void*
#define CryptoX_Certificate void*
#define CryptoX_InitCryptoProvider(CryptoHandle) \
  CryptoX_Error
#define CryptoX_VerifyBegin(CryptoHandle, SignatureHandle, PublicKey) \
  CryptoX_Error
#define CryptoX_VerifyUpdate(SignatureHandle, buf, len) CryptoX_Error
#define CryptoX_LoadPublicKey(CryptoHandle, certData, dataSize, \
                              publicKey, certName, cert) \
  CryptoX_Error
#define CryptoX_VerifySignature(hash, publicKey, signedData, len) CryptoX_Error
#define CryptoX_FreePublicKey(key) CryptoX_Error

#endif

#endif
