/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SOFTOKEN_KEM_H
#define SOFTOKEN_KEM_H

#include "pkcs11.h"

KyberParams sftk_kyber_PK11ParamToInternal(CK_NSS_KEM_PARAMETER_SET_TYPE pk11ParamSet);

SECItem* sftk_kyber_AllocPubKeyItem(KyberParams params, SECItem* pubkey);
SECItem* sftk_kyber_AllocPrivKeyItem(KyberParams params, SECItem* privkey);
SECItem* sftk_kyber_AllocCiphertextItem(KyberParams params, SECItem* ciphertext);

CK_RV NSC_Encapsulate(CK_SESSION_HANDLE hSession,
                      CK_MECHANISM_PTR pMechanism,
                      CK_OBJECT_HANDLE hPublicKey,
                      CK_ATTRIBUTE_PTR pTemplate,
                      CK_ULONG ulAttributeCount,
                      CK_OBJECT_HANDLE_PTR phKey,
                      CK_BYTE_PTR pCiphertext,
                      CK_ULONG_PTR pulCiphertextLen);

CK_RV NSC_Decapsulate(CK_SESSION_HANDLE hSession,
                      CK_MECHANISM_PTR pMechanism,
                      CK_OBJECT_HANDLE hPrivateKey,
                      CK_BYTE_PTR pCiphertext,
                      CK_ULONG ulCiphertextLen,
                      CK_ATTRIBUTE_PTR pTemplate,
                      CK_ULONG ulAttributeCount,
                      CK_OBJECT_HANDLE_PTR phKey);

#endif // SOFTOKEN_KEM_H
