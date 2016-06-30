/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cmmf.h"
#include "cmmfi.h"
#include "secitem.h"
#include "keyhi.h"
#include "secder.h"

CRMFEncryptedKeyChoice
CRMF_EncryptedKeyGetChoice(CRMFEncryptedKey *inEncrKey)
{
    PORT_Assert(inEncrKey != NULL);
    if (inEncrKey == NULL) {
        return crmfNoEncryptedKeyChoice;
    }
    return inEncrKey->encKeyChoice;
}

CRMFEncryptedValue *
CRMF_EncryptedKeyGetEncryptedValue(CRMFEncryptedKey *inEncrKey)
{
    CRMFEncryptedValue *newEncrValue = NULL;
    SECStatus rv;

    PORT_Assert(inEncrKey != NULL);
    if (inEncrKey == NULL ||
        CRMF_EncryptedKeyGetChoice(inEncrKey) != crmfEncryptedValueChoice) {
        goto loser;
    }
    newEncrValue = PORT_ZNew(CRMFEncryptedValue);
    if (newEncrValue == NULL) {
        goto loser;
    }
    rv = crmf_copy_encryptedvalue(NULL, &inEncrKey->value.encryptedValue,
                                  newEncrValue);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newEncrValue;
loser:
    if (newEncrValue != NULL) {
        CRMF_DestroyEncryptedValue(newEncrValue);
    }
    return NULL;
}

static SECItem *
crmf_get_encvalue_bitstring(SECItem *srcItem)
{
    SECItem *newItem = NULL;
    SECStatus rv;

    if (srcItem->data == NULL) {
        return NULL;
    }
    newItem = PORT_ZNew(SECItem);
    if (newItem == NULL) {
        goto loser;
    }
    rv = crmf_make_bitstring_copy(NULL, newItem, srcItem);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newItem;
loser:
    if (newItem != NULL) {
        SECITEM_FreeItem(newItem, PR_TRUE);
    }
    return NULL;
}

SECItem *
CRMF_EncryptedValueGetEncSymmKey(CRMFEncryptedValue *inEncValue)
{
    if (inEncValue == NULL) {
        return NULL;
    }
    return crmf_get_encvalue_bitstring(&inEncValue->encSymmKey);
}

SECItem *
CRMF_EncryptedValueGetEncValue(CRMFEncryptedValue *inEncrValue)
{
    if (inEncrValue == NULL || inEncrValue->encValue.data == NULL) {
        return NULL;
    }
    return crmf_get_encvalue_bitstring(&inEncrValue->encValue);
}

static SECAlgorithmID *
crmf_get_encvalue_algid(SECAlgorithmID *srcAlg)
{
    SECStatus rv;
    SECAlgorithmID *newAlgID;

    if (srcAlg == NULL) {
        return NULL;
    }
    rv = crmf_copy_encryptedvalue_secalg(NULL, srcAlg, &newAlgID);
    if (rv != SECSuccess) {
        return NULL;
    }
    return newAlgID;
}

SECAlgorithmID *
CRMF_EncryptedValueGetIntendedAlg(CRMFEncryptedValue *inEncValue)
{
    if (inEncValue == NULL) {
        return NULL;
    }
    return crmf_get_encvalue_algid(inEncValue->intendedAlg);
}

SECAlgorithmID *
CRMF_EncryptedValueGetKeyAlg(CRMFEncryptedValue *inEncValue)
{
    if (inEncValue == NULL) {
        return NULL;
    }
    return crmf_get_encvalue_algid(inEncValue->keyAlg);
}

SECAlgorithmID *
CRMF_EncryptedValueGetSymmAlg(CRMFEncryptedValue *inEncValue)
{
    if (inEncValue == NULL) {
        return NULL;
    }
    return crmf_get_encvalue_algid(inEncValue->symmAlg);
}

SECItem *
CRMF_EncryptedValueGetValueHint(CRMFEncryptedValue *inEncValue)
{
    if (inEncValue == NULL || inEncValue->valueHint.data == NULL) {
        return NULL;
    }
    return SECITEM_DupItem(&inEncValue->valueHint);
}

SECStatus
CRMF_PKIArchiveOptionsGetArchiveRemGenPrivKey(CRMFPKIArchiveOptions *inOpt,
                                              PRBool *destVal)
{
    if (inOpt == NULL || destVal == NULL ||
        CRMF_PKIArchiveOptionsGetOptionType(inOpt) != crmfArchiveRemGenPrivKey) {
        return SECFailure;
    }
    *destVal = (inOpt->option.archiveRemGenPrivKey.data[0] == hexFalse)
                   ? PR_FALSE
                   : PR_TRUE;
    return SECSuccess;
}

CRMFEncryptedKey *
CRMF_PKIArchiveOptionsGetEncryptedPrivKey(CRMFPKIArchiveOptions *inOpts)
{
    CRMFEncryptedKey *newEncrKey = NULL;
    SECStatus rv;

    PORT_Assert(inOpts != NULL);
    if (inOpts == NULL ||
        CRMF_PKIArchiveOptionsGetOptionType(inOpts) != crmfEncryptedPrivateKey) {
        return NULL;
    }
    newEncrKey = PORT_ZNew(CRMFEncryptedKey);
    if (newEncrKey == NULL) {
        goto loser;
    }
    rv = crmf_copy_encryptedkey(NULL, &inOpts->option.encryptedKey,
                                newEncrKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newEncrKey;
loser:
    if (newEncrKey != NULL) {
        CRMF_DestroyEncryptedKey(newEncrKey);
    }
    return NULL;
}

SECItem *
CRMF_PKIArchiveOptionsGetKeyGenParameters(CRMFPKIArchiveOptions *inOptions)
{
    if (inOptions == NULL ||
        CRMF_PKIArchiveOptionsGetOptionType(inOptions) != crmfKeyGenParameters ||
        inOptions->option.keyGenParameters.data == NULL) {
        return NULL;
    }
    return SECITEM_DupItem(&inOptions->option.keyGenParameters);
}

CRMFPKIArchiveOptionsType
CRMF_PKIArchiveOptionsGetOptionType(CRMFPKIArchiveOptions *inOptions)
{
    PORT_Assert(inOptions != NULL);
    if (inOptions == NULL) {
        return crmfNoArchiveOptions;
    }
    return inOptions->archOption;
}

static SECStatus
crmf_extract_long_from_item(SECItem *intItem, long *destLong)
{
    *destLong = DER_GetInteger(intItem);
    return (*destLong == -1) ? SECFailure : SECSuccess;
}

SECStatus
CRMF_POPOPrivGetKeySubseqMess(CRMFPOPOPrivKey *inKey,
                              CRMFSubseqMessOptions *destOpt)
{
    long value;
    SECStatus rv;

    PORT_Assert(inKey != NULL);
    if (inKey == NULL ||
        inKey->messageChoice != crmfSubsequentMessage) {
        return SECFailure;
    }
    rv = crmf_extract_long_from_item(&inKey->message.subsequentMessage, &value);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    switch (value) {
        case 0:
            *destOpt = crmfEncrCert;
            break;
        case 1:
            *destOpt = crmfChallengeResp;
            break;
        default:
            rv = SECFailure;
    }
    if (rv != SECSuccess) {
        return rv;
    }
    return SECSuccess;
}

CRMFPOPOPrivKeyChoice
CRMF_POPOPrivKeyGetChoice(CRMFPOPOPrivKey *inPrivKey)
{
    PORT_Assert(inPrivKey != NULL);
    if (inPrivKey != NULL) {
        return inPrivKey->messageChoice;
    }
    return crmfNoMessage;
}

SECStatus
CRMF_POPOPrivKeyGetDHMAC(CRMFPOPOPrivKey *inKey, SECItem *destMAC)
{
    PORT_Assert(inKey != NULL);
    if (inKey == NULL || inKey->message.dhMAC.data == NULL) {
        return SECFailure;
    }
    return crmf_make_bitstring_copy(NULL, destMAC, &inKey->message.dhMAC);
}

SECStatus
CRMF_POPOPrivKeyGetThisMessage(CRMFPOPOPrivKey *inKey,
                               SECItem *destString)
{
    PORT_Assert(inKey != NULL);
    if (inKey == NULL ||
        inKey->messageChoice != crmfThisMessage) {
        return SECFailure;
    }

    return crmf_make_bitstring_copy(NULL, destString,
                                    &inKey->message.thisMessage);
}

SECAlgorithmID *
CRMF_POPOSigningKeyGetAlgID(CRMFPOPOSigningKey *inSignKey)
{
    SECAlgorithmID *newAlgId = NULL;
    SECStatus rv;

    PORT_Assert(inSignKey != NULL);
    if (inSignKey == NULL) {
        return NULL;
    }
    newAlgId = PORT_ZNew(SECAlgorithmID);
    if (newAlgId == NULL) {
        goto loser;
    }
    rv = SECOID_CopyAlgorithmID(NULL, newAlgId,
                                inSignKey->algorithmIdentifier);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newAlgId;

loser:
    if (newAlgId != NULL) {
        SECOID_DestroyAlgorithmID(newAlgId, PR_TRUE);
    }
    return NULL;
}

SECItem *
CRMF_POPOSigningKeyGetInput(CRMFPOPOSigningKey *inSignKey)
{
    PORT_Assert(inSignKey != NULL);
    if (inSignKey == NULL || inSignKey->derInput.data == NULL) {
        return NULL;
    }
    return SECITEM_DupItem(&inSignKey->derInput);
}

SECItem *
CRMF_POPOSigningKeyGetSignature(CRMFPOPOSigningKey *inSignKey)
{
    SECItem *newSig = NULL;
    SECStatus rv;

    PORT_Assert(inSignKey != NULL);
    if (inSignKey == NULL) {
        return NULL;
    }
    newSig = PORT_ZNew(SECItem);
    if (newSig == NULL) {
        goto loser;
    }
    rv = crmf_make_bitstring_copy(NULL, newSig, &inSignKey->signature);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newSig;
loser:
    if (newSig != NULL) {
        SECITEM_FreeItem(newSig, PR_TRUE);
    }
    return NULL;
}

static SECStatus
crmf_copy_poposigningkey(PLArenaPool *poolp,
                         CRMFPOPOSigningKey *inPopoSignKey,
                         CRMFPOPOSigningKey *destPopoSignKey)
{
    SECStatus rv;

    /* We don't support use of the POPOSigningKeyInput, so we'll only
     * store away the DER encoding.
     */
    if (inPopoSignKey->derInput.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &destPopoSignKey->derInput,
                              &inPopoSignKey->derInput);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    destPopoSignKey->algorithmIdentifier = (poolp == NULL) ? PORT_ZNew(SECAlgorithmID)
                                                           : PORT_ArenaZNew(poolp, SECAlgorithmID);

    if (destPopoSignKey->algorithmIdentifier == NULL) {
        goto loser;
    }
    rv = SECOID_CopyAlgorithmID(poolp, destPopoSignKey->algorithmIdentifier,
                                inPopoSignKey->algorithmIdentifier);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_make_bitstring_copy(poolp, &destPopoSignKey->signature,
                                  &inPopoSignKey->signature);
    if (rv != SECSuccess) {
        goto loser;
    }
    return SECSuccess;
loser:
    if (poolp == NULL) {
        CRMF_DestroyPOPOSigningKey(destPopoSignKey);
    }
    return SECFailure;
}

static SECStatus
crmf_copy_popoprivkey(PLArenaPool *poolp,
                      CRMFPOPOPrivKey *srcPrivKey,
                      CRMFPOPOPrivKey *destPrivKey)
{
    SECStatus rv;

    destPrivKey->messageChoice = srcPrivKey->messageChoice;
    switch (destPrivKey->messageChoice) {
        case crmfThisMessage:
        case crmfDHMAC:
            /* I've got a union, so taking the address of one, will also give
             * me a pointer to the other (eg, message.dhMAC)
             */
            rv = crmf_make_bitstring_copy(poolp, &destPrivKey->message.thisMessage,
                                          &srcPrivKey->message.thisMessage);
            break;
        case crmfSubsequentMessage:
            rv = SECITEM_CopyItem(poolp, &destPrivKey->message.subsequentMessage,
                                  &srcPrivKey->message.subsequentMessage);
            break;
        default:
            rv = SECFailure;
    }

    if (rv != SECSuccess && poolp == NULL) {
        CRMF_DestroyPOPOPrivKey(destPrivKey);
    }
    return rv;
}

static CRMFProofOfPossession *
crmf_copy_pop(PLArenaPool *poolp, CRMFProofOfPossession *srcPOP)
{
    CRMFProofOfPossession *newPOP;
    SECStatus rv;

    /*
     * Proof Of Possession structures are always part of the Request
     * message, so there will always be an arena for allocating memory.
     */
    if (poolp == NULL) {
        return NULL;
    }
    newPOP = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (newPOP == NULL) {
        return NULL;
    }
    switch (srcPOP->popUsed) {
        case crmfRAVerified:
            newPOP->popChoice.raVerified.data = NULL;
            newPOP->popChoice.raVerified.len = 0;
            break;
        case crmfSignature:
            rv = crmf_copy_poposigningkey(poolp, &srcPOP->popChoice.signature,
                                          &newPOP->popChoice.signature);
            if (rv != SECSuccess) {
                goto loser;
            }
            break;
        case crmfKeyEncipherment:
        case crmfKeyAgreement:
            /* We've got a union, so a pointer to one, is a pointer to the
             * other one.
             */
            rv = crmf_copy_popoprivkey(poolp, &srcPOP->popChoice.keyEncipherment,
                                       &newPOP->popChoice.keyEncipherment);
            if (rv != SECSuccess) {
                goto loser;
            }
            break;
        default:
            goto loser;
    }
    newPOP->popUsed = srcPOP->popUsed;
    return newPOP;

loser:
    return NULL;
}

static CRMFCertReqMsg *
crmf_copy_cert_req_msg(CRMFCertReqMsg *srcReqMsg)
{
    CRMFCertReqMsg *newReqMsg;
    PLArenaPool *poolp;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    newReqMsg = PORT_ArenaZNew(poolp, CRMFCertReqMsg);
    if (newReqMsg == NULL) {
        PORT_FreeArena(poolp, PR_TRUE);
        return NULL;
    }

    newReqMsg->poolp = poolp;
    newReqMsg->certReq = crmf_copy_cert_request(poolp, srcReqMsg->certReq);
    if (newReqMsg->certReq == NULL) {
        goto loser;
    }
    newReqMsg->pop = crmf_copy_pop(poolp, srcReqMsg->pop);
    if (newReqMsg->pop == NULL) {
        goto loser;
    }
    /* None of my set/get routines operate on the regInfo field, so
     * for now, that won't get copied over.
     */
    return newReqMsg;

loser:
    CRMF_DestroyCertReqMsg(newReqMsg);
    return NULL;
}

CRMFCertReqMsg *
CRMF_CertReqMessagesGetCertReqMsgAtIndex(CRMFCertReqMessages *inReqMsgs,
                                         int index)
{
    int numMsgs;

    PORT_Assert(inReqMsgs != NULL && index >= 0);
    if (inReqMsgs == NULL) {
        return NULL;
    }
    numMsgs = CRMF_CertReqMessagesGetNumMessages(inReqMsgs);
    if (index < 0 || index >= numMsgs) {
        return NULL;
    }
    return crmf_copy_cert_req_msg(inReqMsgs->messages[index]);
}

int
CRMF_CertReqMessagesGetNumMessages(CRMFCertReqMessages *inCertReqMsgs)
{
    int numMessages = 0;

    PORT_Assert(inCertReqMsgs != NULL);
    if (inCertReqMsgs == NULL) {
        return 0;
    }
    while (inCertReqMsgs->messages[numMessages] != NULL) {
        numMessages++;
    }
    return numMessages;
}

CRMFCertRequest *
CRMF_CertReqMsgGetCertRequest(CRMFCertReqMsg *inCertReqMsg)
{
    PLArenaPool *poolp = NULL;
    CRMFCertRequest *newCertReq = NULL;

    PORT_Assert(inCertReqMsg != NULL);

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    newCertReq = crmf_copy_cert_request(poolp, inCertReqMsg->certReq);
    if (newCertReq == NULL) {
        goto loser;
    }
    newCertReq->poolp = poolp;
    return newCertReq;
loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus
CRMF_CertReqMsgGetID(CRMFCertReqMsg *inCertReqMsg, long *destID)
{
    PORT_Assert(inCertReqMsg != NULL && destID != NULL);
    if (inCertReqMsg == NULL || inCertReqMsg->certReq == NULL) {
        return SECFailure;
    }
    return crmf_extract_long_from_item(&inCertReqMsg->certReq->certReqId,
                                       destID);
}

SECStatus
CRMF_CertReqMsgGetPOPKeyAgreement(CRMFCertReqMsg *inCertReqMsg,
                                  CRMFPOPOPrivKey **destKey)
{
    PORT_Assert(inCertReqMsg != NULL && destKey != NULL);
    if (inCertReqMsg == NULL || destKey == NULL ||
        CRMF_CertReqMsgGetPOPType(inCertReqMsg) != crmfKeyAgreement) {
        return SECFailure;
    }
    *destKey = PORT_ZNew(CRMFPOPOPrivKey);
    if (*destKey == NULL) {
        return SECFailure;
    }
    return crmf_copy_popoprivkey(NULL,
                                 &inCertReqMsg->pop->popChoice.keyAgreement,
                                 *destKey);
}

SECStatus
CRMF_CertReqMsgGetPOPKeyEncipherment(CRMFCertReqMsg *inCertReqMsg,
                                     CRMFPOPOPrivKey **destKey)
{
    PORT_Assert(inCertReqMsg != NULL && destKey != NULL);
    if (inCertReqMsg == NULL || destKey == NULL ||
        CRMF_CertReqMsgGetPOPType(inCertReqMsg) != crmfKeyEncipherment) {
        return SECFailure;
    }
    *destKey = PORT_ZNew(CRMFPOPOPrivKey);
    if (*destKey == NULL) {
        return SECFailure;
    }
    return crmf_copy_popoprivkey(NULL,
                                 &inCertReqMsg->pop->popChoice.keyEncipherment,
                                 *destKey);
}

SECStatus
CRMF_CertReqMsgGetPOPOSigningKey(CRMFCertReqMsg *inCertReqMsg,
                                 CRMFPOPOSigningKey **destKey)
{
    CRMFProofOfPossession *pop;
    PORT_Assert(inCertReqMsg != NULL);
    if (inCertReqMsg == NULL) {
        return SECFailure;
    }
    pop = inCertReqMsg->pop;
    ;
    if (pop->popUsed != crmfSignature) {
        return SECFailure;
    }
    *destKey = PORT_ZNew(CRMFPOPOSigningKey);
    if (*destKey == NULL) {
        return SECFailure;
    }
    return crmf_copy_poposigningkey(NULL, &pop->popChoice.signature, *destKey);
}

static SECStatus
crmf_copy_name(CERTName *destName, CERTName *srcName)
{
    PLArenaPool *poolp = NULL;
    SECStatus rv;

    if (destName->arena != NULL) {
        poolp = destName->arena;
    } else {
        poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    }
    if (poolp == NULL) {
        return SECFailure;
    }
    /* Need to do this so that CERT_CopyName doesn't free out
     * the arena from underneath us.
     */
    destName->arena = NULL;
    rv = CERT_CopyName(poolp, destName, srcName);
    destName->arena = poolp;
    return rv;
}

SECStatus
CRMF_CertRequestGetCertTemplateIssuer(CRMFCertRequest *inCertReq,
                                      CERTName *destIssuer)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfIssuer)) {
        return crmf_copy_name(destIssuer,
                              inCertReq->certTemplate.issuer);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateIssuerUID(CRMFCertRequest *inCertReq,
                                         SECItem *destIssuerUID)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfIssuerUID)) {
        return crmf_make_bitstring_copy(NULL, destIssuerUID,
                                        &inCertReq->certTemplate.issuerUID);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplatePublicKey(CRMFCertRequest *inCertReq,
                                         CERTSubjectPublicKeyInfo *destPublicKey)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfPublicKey)) {
        return SECKEY_CopySubjectPublicKeyInfo(NULL, destPublicKey,
                                               inCertReq->certTemplate.publicKey);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateSerialNumber(CRMFCertRequest *inCertReq,
                                            long *serialNumber)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfSerialNumber)) {
        return crmf_extract_long_from_item(&inCertReq->certTemplate.serialNumber,
                                           serialNumber);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateSigningAlg(CRMFCertRequest *inCertReq,
                                          SECAlgorithmID *destAlg)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfSigningAlg)) {
        return SECOID_CopyAlgorithmID(NULL, destAlg,
                                      inCertReq->certTemplate.signingAlg);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateSubject(CRMFCertRequest *inCertReq,
                                       CERTName *destSubject)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfSubject)) {
        return crmf_copy_name(destSubject, inCertReq->certTemplate.subject);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateSubjectUID(CRMFCertRequest *inCertReq,
                                          SECItem *destSubjectUID)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfSubjectUID)) {
        return crmf_make_bitstring_copy(NULL, destSubjectUID,
                                        &inCertReq->certTemplate.subjectUID);
    }
    return SECFailure;
}

SECStatus
CRMF_CertRequestGetCertTemplateVersion(CRMFCertRequest *inCertReq,
                                       long *version)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfVersion)) {
        return crmf_extract_long_from_item(&inCertReq->certTemplate.version,
                                           version);
    }
    return SECFailure;
}

static SECStatus
crmf_copy_validity(CRMFGetValidity *destValidity,
                   CRMFOptionalValidity *src)
{
    SECStatus rv;

    destValidity->notBefore = destValidity->notAfter = NULL;
    if (src->notBefore.data != NULL) {
        rv = crmf_create_prtime(&src->notBefore,
                                &destValidity->notBefore);
        if (rv != SECSuccess) {
            return rv;
        }
    }
    if (src->notAfter.data != NULL) {
        rv = crmf_create_prtime(&src->notAfter,
                                &destValidity->notAfter);
        if (rv != SECSuccess) {
            return rv;
        }
    }
    return SECSuccess;
}

SECStatus
CRMF_CertRequestGetCertTemplateValidity(CRMFCertRequest *inCertReq,
                                        CRMFGetValidity *destValidity)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return SECFailure;
    }
    if (CRMF_DoesRequestHaveField(inCertReq, crmfValidity)) {
        return crmf_copy_validity(destValidity,
                                  inCertReq->certTemplate.validity);
    }
    return SECFailure;
}

CRMFControl *
CRMF_CertRequestGetControlAtIndex(CRMFCertRequest *inCertReq, int index)
{
    CRMFControl *newControl, *srcControl;
    int numControls;
    SECStatus rv;

    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return NULL;
    }
    numControls = CRMF_CertRequestGetNumControls(inCertReq);
    if (index >= numControls || index < 0) {
        return NULL;
    }
    newControl = PORT_ZNew(CRMFControl);
    if (newControl == NULL) {
        return NULL;
    }
    srcControl = inCertReq->controls[index];
    newControl->tag = srcControl->tag;
    rv = SECITEM_CopyItem(NULL, &newControl->derTag, &srcControl->derTag);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SECITEM_CopyItem(NULL, &newControl->derValue,
                          &srcControl->derValue);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Copy over the PKIArchiveOptions stuff */
    switch (srcControl->tag) {
        case SEC_OID_PKIX_REGCTRL_REGTOKEN:
        case SEC_OID_PKIX_REGCTRL_AUTHENTICATOR:
            /* No further processing necessary for these types. */
            rv = SECSuccess;
            break;
        case SEC_OID_PKIX_REGCTRL_OLD_CERT_ID:
        case SEC_OID_PKIX_REGCTRL_PKIPUBINFO:
        case SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY:
            /* These aren't supported yet, so no post-processing will
             * be done at this time.  But we don't want to fail in case
             * we read in DER that has one of these options.
             */
            rv = SECSuccess;
            break;
        case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
            rv = crmf_copy_pkiarchiveoptions(NULL,
                                             &newControl->value.archiveOptions,
                                             &srcControl->value.archiveOptions);
            break;
        default:
            rv = SECFailure;
    }
    if (rv != SECSuccess) {
        goto loser;
    }
    return newControl;
loser:
    CRMF_DestroyControl(newControl);
    return NULL;
}

static SECItem *
crmf_copy_control_value(CRMFControl *inControl)
{
    return SECITEM_DupItem(&inControl->derValue);
}

SECItem *
CRMF_ControlGetAuthenticatorControlValue(CRMFControl *inControl)
{
    PORT_Assert(inControl != NULL);
    if (inControl == NULL ||
        CRMF_ControlGetControlType(inControl) != crmfAuthenticatorControl) {
        return NULL;
    }
    return crmf_copy_control_value(inControl);
}

CRMFControlType
CRMF_ControlGetControlType(CRMFControl *inControl)
{
    CRMFControlType retType;

    PORT_Assert(inControl != NULL);
    switch (inControl->tag) {
        case SEC_OID_PKIX_REGCTRL_REGTOKEN:
            retType = crmfRegTokenControl;
            break;
        case SEC_OID_PKIX_REGCTRL_AUTHENTICATOR:
            retType = crmfAuthenticatorControl;
            break;
        case SEC_OID_PKIX_REGCTRL_PKIPUBINFO:
            retType = crmfPKIPublicationInfoControl;
            break;
        case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
            retType = crmfPKIArchiveOptionsControl;
            break;
        case SEC_OID_PKIX_REGCTRL_OLD_CERT_ID:
            retType = crmfOldCertIDControl;
            break;
        case SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY:
            retType = crmfProtocolEncrKeyControl;
            break;
        default:
            retType = crmfNoControl;
    }
    return retType;
}

CRMFPKIArchiveOptions *
CRMF_ControlGetPKIArchiveOptions(CRMFControl *inControl)
{
    CRMFPKIArchiveOptions *newOpt = NULL;
    SECStatus rv;

    PORT_Assert(inControl != NULL);
    if (inControl == NULL ||
        CRMF_ControlGetControlType(inControl) != crmfPKIArchiveOptionsControl) {
        goto loser;
    }
    newOpt = PORT_ZNew(CRMFPKIArchiveOptions);
    if (newOpt == NULL) {
        goto loser;
    }
    rv = crmf_copy_pkiarchiveoptions(NULL, newOpt,
                                     &inControl->value.archiveOptions);
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    if (newOpt != NULL) {
        CRMF_DestroyPKIArchiveOptions(newOpt);
    }
    return NULL;
}

SECItem *
CRMF_ControlGetRegTokenControlValue(CRMFControl *inControl)
{
    PORT_Assert(inControl != NULL);
    if (inControl == NULL ||
        CRMF_ControlGetControlType(inControl) != crmfRegTokenControl) {
        return NULL;
    }
    return crmf_copy_control_value(inControl);
    ;
}

CRMFCertExtension *
CRMF_CertRequestGetExtensionAtIndex(CRMFCertRequest *inCertReq,
                                    int index)
{
    int numExtensions;

    PORT_Assert(inCertReq != NULL);
    numExtensions = CRMF_CertRequestGetNumberOfExtensions(inCertReq);
    if (index >= numExtensions || index < 0) {
        return NULL;
    }
    return crmf_copy_cert_extension(NULL,
                                    inCertReq->certTemplate.extensions[index]);
}
