/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Support for various policy related extensions
 *
 * $Id: polcyxtn.c,v 1.14 2012/04/25 14:49:27 gerv%gerv.net Exp $
 */

#include "seccomon.h"
#include "secport.h"
#include "secder.h"
#include "cert.h"
#include "secoid.h"
#include "secasn1.h"
#include "secerr.h"
#include "nspr.h"

SEC_ASN1_MKSUB(SEC_IntegerTemplate)
SEC_ASN1_MKSUB(SEC_ObjectIDTemplate)

const SEC_ASN1Template CERT_DisplayTextTypeTemplate[] = {
    { SEC_ASN1_CHOICE, offsetof(SECItem, type), 0, sizeof(SECItem) },
    { SEC_ASN1_IA5_STRING, 0, 0, siAsciiString},
    { SEC_ASN1_VISIBLE_STRING , 0, 0, siVisibleString},
    { SEC_ASN1_BMP_STRING  , 0, 0, siBMPString },
    { SEC_ASN1_UTF8_STRING , 0, 0, siUTF8String },
    { 0 }
};

const SEC_ASN1Template CERT_NoticeReferenceTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTNoticeReference) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTNoticeReference, organization),
           CERT_DisplayTextTypeTemplate, 0 },
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_XTRN,
           offsetof(CERTNoticeReference, noticeNumbers),
           SEC_ASN1_SUB(SEC_IntegerTemplate) }, 
    { 0 }
};

const SEC_ASN1Template CERT_UserNoticeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTUserNotice) },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL,
	  offsetof(CERTUserNotice, noticeReference),
           CERT_NoticeReferenceTemplate, 0 },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL,
	  offsetof(CERTUserNotice, displayText),
           CERT_DisplayTextTypeTemplate, 0 }, 
    { 0 }
};

const SEC_ASN1Template CERT_PolicyQualifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTPolicyQualifier) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyQualifier, qualifierID) },
    { SEC_ASN1_ANY,
	  offsetof(CERTPolicyQualifier, qualifierValue) },
    { 0 }
};

const SEC_ASN1Template CERT_PolicyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTPolicyInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyInfo, policyID) },
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_OPTIONAL,
	  offsetof(CERTPolicyInfo, policyQualifiers),
	  CERT_PolicyQualifierTemplate },
    { 0 }
};

const SEC_ASN1Template CERT_CertificatePoliciesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCertificatePolicies, policyInfos),
	  CERT_PolicyInfoTemplate, sizeof(CERTCertificatePolicies)  }
};

const SEC_ASN1Template CERT_PolicyMapTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTPolicyMap) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyMap, issuerDomainPolicy) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyMap, subjectDomainPolicy) },
    { 0 }
};

const SEC_ASN1Template CERT_PolicyMappingsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCertificatePolicyMappings, policyMaps),
	  CERT_PolicyMapTemplate, sizeof(CERTPolicyMap)  }
};

const SEC_ASN1Template CERT_PolicyConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CERTCertificatePolicyConstraints) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(CERTCertificatePolicyConstraints, explicitPolicySkipCerts),
	  SEC_ASN1_SUB(SEC_IntegerTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
	  offsetof(CERTCertificatePolicyConstraints, inhibitMappingSkipCerts),
	  SEC_ASN1_SUB(SEC_IntegerTemplate) },
    { 0 }
};

const SEC_ASN1Template CERT_InhibitAnyTemplate[] = {
    { SEC_ASN1_INTEGER,
	  offsetof(CERTCertificateInhibitAny, inhibitAnySkipCerts),
	  NULL, sizeof(CERTCertificateInhibitAny)  }
};

static void
breakLines(char *string)
{
    char *tmpstr;
    char *lastspace = NULL;
    int curlen = 0;
    int c;
    
    tmpstr = string;

    while ( ( c = *tmpstr ) != '\0' ) {
	switch ( c ) {
	  case ' ':
	    lastspace = tmpstr;
	    break;
	  case '\n':
	    lastspace = NULL;
	    curlen = 0;
	    break;
	}
	
	if ( ( curlen >= 55 ) && ( lastspace != NULL ) ) {
	    *lastspace = '\n';
	    curlen = ( tmpstr - lastspace );
	    lastspace = NULL;
	}
	
	curlen++;
	tmpstr++;
    }
    
    return;
}

CERTCertificatePolicies *
CERT_DecodeCertificatePoliciesExtension(SECItem *extnValue)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    CERTCertificatePolicies *policies;
    CERTPolicyInfo **policyInfos, *policyInfo;
    CERTPolicyQualifier **policyQualifiers, *policyQualifier;
    SECItem newExtnValue;
    
    /* make a new arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	goto loser;
    }

    /* allocate the certificate policies structure */
    policies = (CERTCertificatePolicies *)
	PORT_ArenaZAlloc(arena, sizeof(CERTCertificatePolicies));
    
    if ( policies == NULL ) {
	goto loser;
    }
    
    policies->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newExtnValue, extnValue);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* decode the policy info */
    rv = SEC_QuickDERDecodeItem(arena, policies, CERT_CertificatePoliciesTemplate,
			    &newExtnValue);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* initialize the oid tags */
    policyInfos = policies->policyInfos;
    while (*policyInfos != NULL ) {
	policyInfo = *policyInfos;
	policyInfo->oid = SECOID_FindOIDTag(&policyInfo->policyID);
	policyQualifiers = policyInfo->policyQualifiers;
	while ( policyQualifiers != NULL && *policyQualifiers != NULL ) {
	    policyQualifier = *policyQualifiers;
	    policyQualifier->oid =
		SECOID_FindOIDTag(&policyQualifier->qualifierID);
	    policyQualifiers++;
	}
	policyInfos++;
    }

    return(policies);
    
loser:
    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

void
CERT_DestroyCertificatePoliciesExtension(CERTCertificatePolicies *policies)
{
    if ( policies != NULL ) {
	PORT_FreeArena(policies->arena, PR_FALSE);
    }
    return;
}

CERTCertificatePolicyMappings *
CERT_DecodePolicyMappingsExtension(SECItem *extnValue)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    CERTCertificatePolicyMappings *mappings;
    SECItem newExtnValue;
    
    /* make a new arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
        goto loser;
    }

    /* allocate the policy mappings structure */
    mappings = (CERTCertificatePolicyMappings *)
        PORT_ArenaZAlloc(arena, sizeof(CERTCertificatePolicyMappings));
    if ( mappings == NULL ) {
        goto loser;
    }
    mappings->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newExtnValue, extnValue);
    if ( rv != SECSuccess ) {
        goto loser;
    }

    /* decode the policy mappings */
    rv = SEC_QuickDERDecodeItem
        (arena, mappings, CERT_PolicyMappingsTemplate, &newExtnValue);
    if ( rv != SECSuccess ) {
        goto loser;
    }

    return(mappings);
    
loser:
    if ( arena != NULL ) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

SECStatus
CERT_DestroyPolicyMappingsExtension(CERTCertificatePolicyMappings *mappings)
{
    if ( mappings != NULL ) {
        PORT_FreeArena(mappings->arena, PR_FALSE);
    }
    return SECSuccess;
}

SECStatus
CERT_DecodePolicyConstraintsExtension
                             (CERTCertificatePolicyConstraints *decodedValue,
                              SECItem *encodedValue)
{
    CERTCertificatePolicyConstraints decodeContext;
    PRArenaPool *arena = NULL;
    SECStatus rv = SECSuccess;

    /* initialize so we can tell when an optional component is omitted */
    PORT_Memset(&decodeContext, 0, sizeof(decodeContext));

    /* make a new arena */
    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (!arena) {
        return SECFailure;
    }

    do {
        /* decode the policy constraints */
        rv = SEC_QuickDERDecodeItem(arena,
                &decodeContext, CERT_PolicyConstraintsTemplate, encodedValue);

        if ( rv != SECSuccess ) {
            break;
        }

        if (decodeContext.explicitPolicySkipCerts.len == 0) {
            *(PRInt32 *)decodedValue->explicitPolicySkipCerts.data = -1;
        } else {
            *(PRInt32 *)decodedValue->explicitPolicySkipCerts.data =
                    DER_GetInteger(&decodeContext.explicitPolicySkipCerts);
        }

        if (decodeContext.inhibitMappingSkipCerts.len == 0) {
            *(PRInt32 *)decodedValue->inhibitMappingSkipCerts.data = -1;
        } else {
            *(PRInt32 *)decodedValue->inhibitMappingSkipCerts.data =
                    DER_GetInteger(&decodeContext.inhibitMappingSkipCerts);
        }

        if ((*(PRInt32 *)decodedValue->explicitPolicySkipCerts.data ==
                PR_INT32_MIN) ||
            (*(PRInt32 *)decodedValue->explicitPolicySkipCerts.data ==
                PR_INT32_MAX) ||
            (*(PRInt32 *)decodedValue->inhibitMappingSkipCerts.data ==
                PR_INT32_MIN) ||
            (*(PRInt32 *)decodedValue->inhibitMappingSkipCerts.data ==
                PR_INT32_MAX)) {
            rv = SECFailure;
        }
    
    } while (0);

    PORT_FreeArena(arena, PR_FALSE);
    return(rv);
}

SECStatus CERT_DecodeInhibitAnyExtension
        (CERTCertificateInhibitAny *decodedValue, SECItem *encodedValue)
{
    CERTCertificateInhibitAny decodeContext;
    PRArenaPool *arena = NULL;
    SECStatus rv = SECSuccess;

    /* make a new arena */
    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if ( !arena ) {
        return SECFailure;
    }

    do {

        /* decode the policy mappings */
        decodeContext.inhibitAnySkipCerts.type = siUnsignedInteger;
        rv = SEC_QuickDERDecodeItem(arena,
                &decodeContext, CERT_InhibitAnyTemplate, encodedValue);

        if ( rv != SECSuccess ) {
            break;
        }

        *(PRInt32 *)decodedValue->inhibitAnySkipCerts.data =
                DER_GetInteger(&decodeContext.inhibitAnySkipCerts);

    } while (0);

    PORT_FreeArena(arena, PR_FALSE);
    return(rv);
}

CERTUserNotice *
CERT_DecodeUserNotice(SECItem *noticeItem)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    CERTUserNotice *userNotice;
    SECItem newNoticeItem;
    
    /* make a new arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	goto loser;
    }

    /* allocate the userNotice structure */
    userNotice = (CERTUserNotice *)PORT_ArenaZAlloc(arena,
						    sizeof(CERTUserNotice));
    
    if ( userNotice == NULL ) {
	goto loser;
    }
    
    userNotice->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newNoticeItem, noticeItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* decode the user notice */
    rv = SEC_QuickDERDecodeItem(arena, userNotice, CERT_UserNoticeTemplate, 
			    &newNoticeItem);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    if (userNotice->derNoticeReference.data != NULL) {

        rv = SEC_QuickDERDecodeItem(arena, &userNotice->noticeReference,
                                    CERT_NoticeReferenceTemplate,
                                    &userNotice->derNoticeReference);
        if (rv == SECFailure) {
            goto loser;
    	}
    }

    return(userNotice);
    
loser:
    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

void
CERT_DestroyUserNotice(CERTUserNotice *userNotice)
{
    if ( userNotice != NULL ) {
	PORT_FreeArena(userNotice->arena, PR_FALSE);
    }
    return;
}

static CERTPolicyStringCallback policyStringCB = NULL;
static void *policyStringCBArg = NULL;

void
CERT_SetCAPolicyStringCallback(CERTPolicyStringCallback cb, void *cbarg)
{
    policyStringCB = cb;
    policyStringCBArg = cbarg;
    return;
}

char *
stringFromUserNotice(SECItem *noticeItem)
{
    SECItem *org;
    unsigned int len, headerlen;
    char *stringbuf;
    CERTUserNotice *userNotice;
    char *policystr;
    char *retstr = NULL;
    SECItem *displayText;
    SECItem **noticeNumbers;
    unsigned int strnum;
    
    /* decode the user notice */
    userNotice = CERT_DecodeUserNotice(noticeItem);
    if ( userNotice == NULL ) {
	return(NULL);
    }
    
    org = &userNotice->noticeReference.organization;
    if ( (org->len != 0 ) && ( policyStringCB != NULL ) ) {
	/* has a noticeReference */

	/* extract the org string */
	len = org->len;
	stringbuf = (char*)PORT_Alloc(len + 1);
	if ( stringbuf != NULL ) {
	    PORT_Memcpy(stringbuf, org->data, len);
	    stringbuf[len] = '\0';

	    noticeNumbers = userNotice->noticeReference.noticeNumbers;
	    while ( *noticeNumbers != NULL ) {
		/* XXX - only one byte integers right now*/
		strnum = (*noticeNumbers)->data[0];
		policystr = (* policyStringCB)(stringbuf,
					       strnum,
					       policyStringCBArg);
		if ( policystr != NULL ) {
		    if ( retstr != NULL ) {
			retstr = PR_sprintf_append(retstr, "\n%s", policystr);
		    } else {
			retstr = PR_sprintf_append(retstr, "%s", policystr);
		    }

		    PORT_Free(policystr);
		}
		
		noticeNumbers++;
	    }

	    PORT_Free(stringbuf);
	}
    }

    if ( retstr == NULL ) {
	if ( userNotice->displayText.len != 0 ) {
	    displayText = &userNotice->displayText;

	    if ( displayText->len > 2 ) {
		if ( displayText->data[0] == SEC_ASN1_VISIBLE_STRING ) {
		    headerlen = 2;
		    if ( displayText->data[1] & 0x80 ) {
			/* multibyte length */
			headerlen += ( displayText->data[1] & 0x7f );
		    }

		    len = displayText->len - headerlen;
		    retstr = (char*)PORT_Alloc(len + 1);
		    if ( retstr != NULL ) {
			PORT_Memcpy(retstr, &displayText->data[headerlen],len);
			retstr[len] = '\0';
		    }
		}
	    }
	}
    }
    
    CERT_DestroyUserNotice(userNotice);
    
    return(retstr);
}

char *
CERT_GetCertCommentString(CERTCertificate *cert)
{
    char *retstring = NULL;
    SECStatus rv;
    SECItem policyItem;
    CERTCertificatePolicies *policies = NULL;
    CERTPolicyInfo **policyInfos;
    CERTPolicyQualifier **policyQualifiers, *qualifier;

    policyItem.data = NULL;
    
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_CERTIFICATE_POLICIES,
				&policyItem);
    if ( rv != SECSuccess ) {
	goto nopolicy;
    }

    policies = CERT_DecodeCertificatePoliciesExtension(&policyItem);
    if ( policies == NULL ) {
	goto nopolicy;
    }

    policyInfos = policies->policyInfos;
    /* search through policyInfos looking for the verisign policy */
    while (*policyInfos != NULL ) {
	if ( (*policyInfos)->oid == SEC_OID_VERISIGN_USER_NOTICES ) {
	    policyQualifiers = (*policyInfos)->policyQualifiers;
	    /* search through the policy qualifiers looking for user notice */
	    while ( policyQualifiers != NULL && *policyQualifiers != NULL ) {
		qualifier = *policyQualifiers;
		if ( qualifier->oid == SEC_OID_PKIX_USER_NOTICE_QUALIFIER ) {
		    retstring =
			stringFromUserNotice(&qualifier->qualifierValue);
		    break;
		}

		policyQualifiers++;
	    }
	    break;
	}
	policyInfos++;
    }

nopolicy:
    if ( policyItem.data != NULL ) {
	PORT_Free(policyItem.data);
    }

    if ( policies != NULL ) {
	CERT_DestroyCertificatePoliciesExtension(policies);
    }
    
    if ( retstring == NULL ) {
	retstring = CERT_FindNSStringExtension(cert,
					       SEC_OID_NS_CERT_EXT_COMMENT);
    }
    
    if ( retstring != NULL ) {
	breakLines(retstring);
    }
    
    return(retstring);
}


const SEC_ASN1Template CERT_OidSeqTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_XTRN,
	  offsetof(CERTOidSequence, oids),
	  SEC_ASN1_SUB(SEC_ObjectIDTemplate) }
};

CERTOidSequence *
CERT_DecodeOidSequence(SECItem *seqItem)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    CERTOidSequence *oidSeq;
    SECItem newSeqItem;
    
    /* make a new arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	goto loser;
    }

    /* allocate the userNotice structure */
    oidSeq = (CERTOidSequence *)PORT_ArenaZAlloc(arena,
						 sizeof(CERTOidSequence));
    
    if ( oidSeq == NULL ) {
	goto loser;
    }
    
    oidSeq->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newSeqItem, seqItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* decode the user notice */
    rv = SEC_QuickDERDecodeItem(arena, oidSeq, CERT_OidSeqTemplate, &newSeqItem);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    return(oidSeq);
    
loser:
    return(NULL);
}


void
CERT_DestroyOidSequence(CERTOidSequence *oidSeq)
{
    if ( oidSeq != NULL ) {
	PORT_FreeArena(oidSeq->arena, PR_FALSE);
    }
    return;
}

PRBool
CERT_GovtApprovedBitSet(CERTCertificate *cert)
{
    SECStatus rv;
    SECItem extItem;
    CERTOidSequence *oidSeq = NULL;
    PRBool ret;
    SECItem **oids;
    SECItem *oid;
    SECOidTag oidTag;
    
    extItem.data = NULL;
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_EXT_KEY_USAGE, &extItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    oidSeq = CERT_DecodeOidSequence(&extItem);
    if ( oidSeq == NULL ) {
	goto loser;
    }

    oids = oidSeq->oids;
    while ( oids != NULL && *oids != NULL ) {
	oid = *oids;
	
	oidTag = SECOID_FindOIDTag(oid);
	
	if ( oidTag == SEC_OID_NS_KEY_USAGE_GOVT_APPROVED ) {
	    goto success;
	}
	
	oids++;
    }

loser:
    ret = PR_FALSE;
    goto done;
success:
    ret = PR_TRUE;
done:
    if ( oidSeq != NULL ) {
	CERT_DestroyOidSequence(oidSeq);
    }
    if (extItem.data != NULL) {
	PORT_Free(extItem.data);
    }
    return(ret);
}


SECStatus
CERT_EncodePolicyConstraintsExtension(PRArenaPool *arena,
                                      CERTCertificatePolicyConstraints *constr,
                                      SECItem *dest)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(constr != NULL && dest != NULL);
    if (constr == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem (arena, dest, constr,
                            CERT_PolicyConstraintsTemplate) == NULL) {
	rv = SECFailure;
    }
    return(rv);
}

SECStatus
CERT_EncodePolicyMappingExtension(PRArenaPool *arena,
                                  CERTCertificatePolicyMappings *mapping,
                                  SECItem *dest)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(mapping != NULL && dest != NULL);
    if (mapping == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem (arena, dest, mapping,
                            CERT_PolicyMappingsTemplate) == NULL) {
	rv = SECFailure;
    }
    return(rv);
}



SECStatus
CERT_EncodeCertPoliciesExtension(PRArenaPool *arena,
                                 CERTPolicyInfo **info,
                                 SECItem *dest)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(info != NULL && dest != NULL);
    if (info == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem (arena, dest, info,
                            CERT_CertificatePoliciesTemplate) == NULL) {
	rv = SECFailure;
    }
    return(rv);
}

SECStatus
CERT_EncodeUserNotice(PRArenaPool *arena,
                      CERTUserNotice *notice,
                      SECItem *dest)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(notice != NULL && dest != NULL);
    if (notice == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem(arena, dest,
                           notice, CERT_UserNoticeTemplate) == NULL) {
	rv = SECFailure;
    }

    return(rv);
}

SECStatus
CERT_EncodeNoticeReference(PRArenaPool *arena,
                           CERTNoticeReference *reference,
                           SECItem *dest)
{
    SECStatus rv = SECSuccess;
    
    PORT_Assert(reference != NULL && dest != NULL);
    if (reference == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem (arena, dest, reference,
                            CERT_NoticeReferenceTemplate) == NULL) {
	rv = SECFailure;
    }

    return(rv);
}

SECStatus
CERT_EncodeInhibitAnyExtension(PRArenaPool *arena,
                               CERTCertificateInhibitAny *certInhibitAny,
                               SECItem *dest)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(certInhibitAny != NULL && dest != NULL);
    if (certInhibitAny == NULL || dest == NULL) {
	return SECFailure;
    }

    if (SEC_ASN1EncodeItem (arena, dest, certInhibitAny,
                            CERT_InhibitAnyTemplate) == NULL) {
	rv = SECFailure;
    }
    return(rv);
}
