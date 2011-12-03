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
 * The Initial Developer of the Original Code is Mozilla Fonudation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "seccomon.h"
#include "secerr.h"
#include "blapi.h"
#include "pkcs11i.h"
#include "softoken.h"

static CK_RV
jpake_mapStatus(SECStatus rv, CK_RV invalidArgsMapping) {
    int err;
    if (rv == SECSuccess)
        return CKR_OK;
    err = PORT_GetError();
    switch (err) {
        /* XXX: SEC_ERROR_INVALID_ARGS might be caused by invalid template
            parameters. */
        case SEC_ERROR_INVALID_ARGS:  return invalidArgsMapping;
        case SEC_ERROR_BAD_SIGNATURE: return CKR_SIGNATURE_INVALID;
        case SEC_ERROR_NO_MEMORY:     return CKR_HOST_MEMORY;
    }
    return CKR_FUNCTION_FAILED;
}

/* If key is not NULL then the gx value will be stored as an attribute with
   the type given by the gxAttrType parameter. */
static CK_RV
jpake_Sign(PLArenaPool * arena, const PQGParams * pqg, HASH_HashType hashType,
           const SECItem * signerID, const SECItem * x,
           CK_NSS_JPAKEPublicValue * out)
{
    SECItem gx, gv, r;
    CK_RV crv;

    PORT_Assert(arena != NULL);
    
    gx.data = NULL;
    gv.data = NULL;
    r.data = NULL;
    crv = jpake_mapStatus(JPAKE_Sign(arena, pqg, hashType, signerID, x, NULL,
                                     NULL, &gx, &gv, &r),
                          CKR_MECHANISM_PARAM_INVALID);
    if (crv == CKR_OK) {
        if ((out->pGX != NULL && out->ulGXLen >= gx.len) ||
            (out->pGV != NULL && out->ulGVLen >= gv.len) ||
            (out->pR  != NULL && out->ulRLen >= r.len)) {
            PORT_Memcpy(out->pGX, gx.data, gx.len); 
            PORT_Memcpy(out->pGV, gv.data, gv.len); 
            PORT_Memcpy(out->pR, r.data, r.len);
            out->ulGXLen = gx.len;
            out->ulGVLen = gv.len;
            out->ulRLen = r.len;
        } else {
            crv = CKR_MECHANISM_PARAM_INVALID;
        }
    } 
    return crv;
}

static CK_RV
jpake_Verify(PLArenaPool * arena, const PQGParams * pqg,
             HASH_HashType hashType, const SECItem * signerID,
             const CK_BYTE * peerIDData, CK_ULONG peerIDLen,
             const CK_NSS_JPAKEPublicValue * publicValueIn)
{
    SECItem peerID, gx, gv, r;
    peerID.data = (unsigned char *) peerIDData; peerID.len = peerIDLen;
    gx.data = publicValueIn->pGX; gx.len = publicValueIn->ulGXLen;
    gv.data = publicValueIn->pGV; gv.len = publicValueIn->ulGVLen;
    r.data = publicValueIn->pR;   r.len = publicValueIn->ulRLen;
    return jpake_mapStatus(JPAKE_Verify(arena, pqg, hashType, signerID, &peerID,
                                        &gx, &gv, &r),
                           CKR_MECHANISM_PARAM_INVALID);
}

#define NUM_ELEM(x) (sizeof (x) / sizeof (x)[0])

/* If the template has the key type set, ensure that it was set to the correct
 * value. If the template did not have the key type set, set it to the
 * correct value.
 */
static CK_RV
jpake_enforceKeyType(SFTKObject * key, CK_KEY_TYPE keyType) {
    CK_RV crv;
    SFTKAttribute * keyTypeAttr = sftk_FindAttribute(key, CKA_KEY_TYPE);
    if (keyTypeAttr != NULL) {
        crv = *(CK_KEY_TYPE *)keyTypeAttr->attrib.pValue == keyType
            ? CKR_OK
            : CKR_TEMPLATE_INCONSISTENT;
        sftk_FreeAttribute(keyTypeAttr);
    } else {
        crv = sftk_forceAttribute(key, CKA_KEY_TYPE, &keyType, sizeof keyType);
    }
    return crv;
}

static CK_RV
jpake_MultipleSecItem2Attribute(SFTKObject * key, const SFTKItemTemplate * attrs,
                                size_t attrsCount)
{
    size_t i;
    
    for (i = 0; i < attrsCount; ++i) {
        CK_RV crv = sftk_forceAttribute(key, attrs[i].type, attrs[i].item->data,
                                        attrs[i].item->len);
        if (crv != CKR_OK)
            return crv;
    }
    return CKR_OK;
}

CK_RV
jpake_Round1(HASH_HashType hashType, CK_NSS_JPAKERound1Params * params,
             SFTKObject * key)
{
    CK_RV crv;
    PQGParams pqg;
    PLArenaPool * arena;
    SECItem signerID;
    SFTKItemTemplate templateAttrs[] = {
        { CKA_PRIME, &pqg.prime },
        { CKA_SUBPRIME, &pqg.subPrime },
        { CKA_BASE, &pqg.base },
        { CKA_NSS_JPAKE_SIGNERID, &signerID }
    };
    SECItem x2, gx1, gx2;
    const SFTKItemTemplate generatedAttrs[] = {
        { CKA_NSS_JPAKE_X2,  &x2  },
        { CKA_NSS_JPAKE_GX1, &gx1 },
        { CKA_NSS_JPAKE_GX2, &gx2 },
    };
    SECItem x1;

    PORT_Assert(params != NULL);
    PORT_Assert(key != NULL);

    arena = PORT_NewArena(NSS_SOFTOKEN_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        crv = CKR_HOST_MEMORY;

    crv = sftk_MultipleAttribute2SecItem(arena, key, templateAttrs,
                                         NUM_ELEM(templateAttrs));

    if (crv == CKR_OK && (signerID.data == NULL || signerID.len == 0))
        crv = CKR_TEMPLATE_INCOMPLETE;

    /* generate x1, g^x1 and the proof of knowledge of x1 */
    if (crv == CKR_OK) {
        x1.data = NULL;
        crv = jpake_mapStatus(DSA_NewRandom(arena, &pqg.subPrime, &x1),
                              CKR_TEMPLATE_INCONSISTENT);
    }
    if (crv == CKR_OK)
        crv = jpake_Sign(arena, &pqg, hashType, &signerID, &x1, &params->gx1);

    /* generate x2, g^x2 and the proof of knowledge of x2 */
    if (crv == CKR_OK) {
        x2.data = NULL;
        crv = jpake_mapStatus(DSA_NewRandom(arena, &pqg.subPrime, &x2),
                              CKR_TEMPLATE_INCONSISTENT);
    }
    if (crv == CKR_OK)
        crv = jpake_Sign(arena, &pqg, hashType, &signerID, &x2, &params->gx2);

    /* Save the values needed for round 2 into CKA_VALUE */
    if (crv == CKR_OK) {
        gx1.data = params->gx1.pGX;
        gx1.len = params->gx1.ulGXLen;
        gx2.data = params->gx2.pGX;
        gx2.len = params->gx2.ulGXLen;
        crv = jpake_MultipleSecItem2Attribute(key, generatedAttrs, 
                                              NUM_ELEM(generatedAttrs));
    }

    PORT_FreeArena(arena, PR_TRUE);
    return crv;
}

CK_RV
jpake_Round2(HASH_HashType hashType, CK_NSS_JPAKERound2Params * params,
             SFTKObject * sourceKey, SFTKObject * key)
{
    CK_RV crv;
    PLArenaPool * arena;
    PQGParams pqg;
    SECItem signerID, x2, gx1, gx2;
    SFTKItemTemplate sourceAttrs[] = { 
        { CKA_PRIME, &pqg.prime },
        { CKA_SUBPRIME, &pqg.subPrime },
        { CKA_BASE, &pqg.base },
        { CKA_NSS_JPAKE_SIGNERID, &signerID },
        { CKA_NSS_JPAKE_X2,  &x2 },
        { CKA_NSS_JPAKE_GX1, &gx1 },
        { CKA_NSS_JPAKE_GX2, &gx2 },
    };
    SECItem x2s, gx3, gx4;
    const SFTKItemTemplate copiedAndGeneratedAttrs[] = {
        { CKA_NSS_JPAKE_SIGNERID, &signerID },
        { CKA_PRIME, &pqg.prime },
        { CKA_SUBPRIME, &pqg.subPrime },
        { CKA_NSS_JPAKE_X2,  &x2  },
        { CKA_NSS_JPAKE_X2S, &x2s },
        { CKA_NSS_JPAKE_GX1, &gx1 },
        { CKA_NSS_JPAKE_GX2, &gx2 },
        { CKA_NSS_JPAKE_GX3, &gx3 },
        { CKA_NSS_JPAKE_GX4, &gx4 }
    };
    SECItem peerID;

    PORT_Assert(params != NULL);
    PORT_Assert(sourceKey != NULL);
    PORT_Assert(key != NULL);

    arena = PORT_NewArena(NSS_SOFTOKEN_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        crv = CKR_HOST_MEMORY;

    /* TODO: check CKK_NSS_JPAKE_ROUND1 */

    crv = sftk_MultipleAttribute2SecItem(arena, sourceKey, sourceAttrs,
                                         NUM_ELEM(sourceAttrs));

    /* Get the peer's ID out of the template and sanity-check it. */
    if (crv == CKR_OK)
        crv = sftk_Attribute2SecItem(arena, &peerID, key,
                                     CKA_NSS_JPAKE_PEERID);
    if (crv == CKR_OK && (peerID.data == NULL || peerID.len == 0))
        crv = CKR_TEMPLATE_INCOMPLETE;
    if (crv == CKR_OK && SECITEM_CompareItem(&signerID, &peerID) == SECEqual)
        crv = CKR_TEMPLATE_INCONSISTENT;

    /* Verify zero-knowledge proofs for g^x3 and g^x4 */
    if (crv == CKR_OK)
        crv = jpake_Verify(arena, &pqg, hashType, &signerID,
                           peerID.data, peerID.len, &params->gx3);
    if (crv == CKR_OK)
        crv = jpake_Verify(arena, &pqg, hashType, &signerID,
                           peerID.data, peerID.len, &params->gx4);

    /* Calculate the base and x2s for A=base^x2s */
    if (crv == CKR_OK) {
        SECItem s;
        s.data = params->pSharedKey;
        s.len = params->ulSharedKeyLen;
        gx3.data = params->gx3.pGX;
        gx3.len = params->gx3.ulGXLen;
        gx4.data = params->gx4.pGX;
        gx4.len = params->gx4.ulGXLen;
        pqg.base.data = NULL;
        x2s.data = NULL;
        crv = jpake_mapStatus(JPAKE_Round2(arena, &pqg.prime, &pqg.subPrime,
                                           &gx1, &gx3, &gx4, &pqg.base, 
                                           &x2, &s, &x2s),
                              CKR_MECHANISM_PARAM_INVALID);
    }

    /* Generate A=base^x2s and its zero-knowledge proof. */
    if (crv == CKR_OK)
        crv = jpake_Sign(arena, &pqg, hashType, &signerID, &x2s, &params->A);

    /* Copy P and Q from the ROUND1 key to the ROUND2 key and save the values
       needed for the final key material derivation into CKA_VALUE. */
    if (crv == CKR_OK)
        crv = sftk_forceAttribute(key, CKA_PRIME, pqg.prime.data,
                                  pqg.prime.len);
    if (crv == CKR_OK)
        crv = sftk_forceAttribute(key, CKA_SUBPRIME, pqg.subPrime.data,
                                  pqg.subPrime.len);
    if (crv == CKR_OK) {
        crv = jpake_MultipleSecItem2Attribute(key, copiedAndGeneratedAttrs,
                                              NUM_ELEM(copiedAndGeneratedAttrs));
    }

    if (crv == CKR_OK)
        crv = jpake_enforceKeyType(key, CKK_NSS_JPAKE_ROUND2);

    PORT_FreeArena(arena, PR_TRUE);
    return crv;
}

CK_RV
jpake_Final(HASH_HashType hashType, const CK_NSS_JPAKEFinalParams * param,
            SFTKObject * sourceKey, SFTKObject * key)
{
    PLArenaPool * arena;
    SECItem K;
    PQGParams pqg;
    CK_RV crv;
    SECItem peerID, signerID, x2s, x2, gx1, gx2, gx3, gx4;
    SFTKItemTemplate sourceAttrs[] = {
        { CKA_NSS_JPAKE_PEERID, &peerID },
        { CKA_NSS_JPAKE_SIGNERID, &signerID },
        { CKA_PRIME, &pqg.prime },
        { CKA_SUBPRIME, &pqg.subPrime },
        { CKA_NSS_JPAKE_X2,  &x2  },
        { CKA_NSS_JPAKE_X2S, &x2s },
        { CKA_NSS_JPAKE_GX1, &gx1 },
        { CKA_NSS_JPAKE_GX2, &gx2 },
        { CKA_NSS_JPAKE_GX3, &gx3 },
        { CKA_NSS_JPAKE_GX4, &gx4 }
    };

    PORT_Assert(param != NULL);
    PORT_Assert(sourceKey != NULL);
    PORT_Assert(key != NULL);

    arena = PORT_NewArena(NSS_SOFTOKEN_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        crv = CKR_HOST_MEMORY;
    
    /* TODO: verify key type CKK_NSS_JPAKE_ROUND2 */

    crv = sftk_MultipleAttribute2SecItem(arena, sourceKey, sourceAttrs,
                                         NUM_ELEM(sourceAttrs));

    /* Calculate base for B=base^x4s */
    if (crv == CKR_OK) {
        pqg.base.data = NULL;
        crv = jpake_mapStatus(JPAKE_Round2(arena, &pqg.prime, &pqg.subPrime,
                                           &gx1, &gx2, &gx3, &pqg.base,
                                           NULL, NULL, NULL),
                              CKR_MECHANISM_PARAM_INVALID);
    }

    /* Verify zero-knowledge proof for B */
    if (crv == CKR_OK)
        crv = jpake_Verify(arena, &pqg, hashType, &signerID,
                           peerID.data, peerID.len, &param->B);
    if (crv == CKR_OK) {
        SECItem B;
        B.data = param->B.pGX;
        B.len = param->B.ulGXLen;
        K.data = NULL;
        crv = jpake_mapStatus(JPAKE_Final(arena, &pqg.prime, &pqg.subPrime,
                                          &x2, &gx4, &x2s, &B, &K),
                              CKR_MECHANISM_PARAM_INVALID);
    }

    /* Save key material into CKA_VALUE. */
    if (crv == CKR_OK)
        crv = sftk_forceAttribute(key, CKA_VALUE, K.data, K.len);

    if (crv == CKR_OK)
        crv = jpake_enforceKeyType(key, CKK_GENERIC_SECRET);

    PORT_FreeArena(arena, PR_TRUE);
    return crv;
}
