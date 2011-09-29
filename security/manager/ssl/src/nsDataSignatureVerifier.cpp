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
 * The Original Code is mozilla.org code.
 *
 * Contributor(s):
 *  Dave Townsend <dtownsend@oxymoronical.com>
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
        0, NULL, sizeof(CERTSignedData) },
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
    PRArenaPool *arena;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return NS_ERROR_OUT_OF_MEMORY;

    // Base 64 decode the key
    SECItem keyItem;
    PORT_Memset(&keyItem, 0, sizeof(SECItem));
    if (!NSSBase64_DecodeBuffer(arena, &keyItem,
                                nsPromiseFlatCString(aPublicKey).get(),
                                aPublicKey.Length())) {
        PORT_FreeArena(arena, PR_FALSE);
        return NS_ERROR_FAILURE;
    }
    
    // Extract the public key from the data
    CERTSubjectPublicKeyInfo *pki = SECKEY_DecodeDERSubjectPublicKeyInfo(&keyItem);
    if (!pki) {
        PORT_FreeArena(arena, PR_FALSE);
        return NS_ERROR_FAILURE;
    }
    SECKEYPublicKey *publicKey = SECKEY_ExtractPublicKey(pki);
    SECKEY_DestroySubjectPublicKeyInfo(pki);
    pki = nsnull;
    
    if (!publicKey) {
        PORT_FreeArena(arena, PR_FALSE);
        return NS_ERROR_FAILURE;
    }
    
    // Base 64 decode the signature
    SECItem signatureItem;
    PORT_Memset(&signatureItem, 0, sizeof(SECItem));
    if (!NSSBase64_DecodeBuffer(arena, &signatureItem,
                                nsPromiseFlatCString(aSignature).get(),
                                aSignature.Length())) {
        SECKEY_DestroyPublicKey(publicKey);
        PORT_FreeArena(arena, PR_FALSE);
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
        PORT_FreeArena(arena, PR_FALSE);
        return NS_ERROR_FAILURE;
    }
    
    // Perform the final verification
    DER_ConvertBitString(&(sigData.signature));
    ss = VFY_VerifyDataWithAlgorithmID((const unsigned char*)nsPromiseFlatCString(aData).get(),
                                       aData.Length(), publicKey,
                                       &(sigData.signature),
                                       &(sigData.signatureAlgorithm),
                                       NULL, NULL);
    
    // Clean up remaining objects
    SECKEY_DestroyPublicKey(publicKey);
    PORT_FreeArena(arena, PR_FALSE);
    
    *_retval = (ss == SECSuccess);

    return NS_OK;
}
