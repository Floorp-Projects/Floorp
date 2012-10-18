/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDataSignatureVerifier.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "seccomon.h"
#include "nssb64.h"
#include "certt.h"
#include "keyhi.h"
#include "cryptohi.h"

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

NS_IMPL_ISUPPORTS1(nsDataSignatureVerifier, nsIDataSignatureVerifier)

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
