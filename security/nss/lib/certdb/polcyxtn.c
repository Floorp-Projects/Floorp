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
 * Support for various policy related extensions
 *
 * $Id: polcyxtn.c,v 1.6 2004/04/25 15:03:03 gerv%gerv.net Exp $
 */

#include "seccomon.h"
#include "secport.h"
#include "secder.h"
#include "cert.h"
#include "secoid.h"
#include "secasn1.h"
#include "secerr.h"
#include "nspr.h"

const SEC_ASN1Template CERT_NoticeReferenceTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTNoticeReference) },
/* NOTE: this should be a choice */
    { SEC_ASN1_IA5_STRING,
	  offsetof(CERTNoticeReference, organization) },
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTNoticeReference, noticeNumbers),
	  SEC_IntegerTemplate }, 
    { 0 }
};

/* this template can not be encoded because of the option inline */
const SEC_ASN1Template CERT_UserNoticeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTUserNotice) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED,
	  offsetof(CERTUserNotice, derNoticeReference) }, 
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(CERTUserNotice, displayText) }, 
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

    /* allocate the certifiate policies structure */
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
	/* sigh, the asn1 parser stripped the sequence encoding, re add it
	 * before we decode.
	 */
	SECItem tmpbuf;
	int	newBytes;

	newBytes = SEC_ASN1LengthLength(userNotice->derNoticeReference.len)+1;
	tmpbuf.len = newBytes + userNotice->derNoticeReference.len;
	tmpbuf.data = PORT_ArenaZAlloc(arena, tmpbuf.len);
	if (tmpbuf.data == NULL) {
	    goto loser;
	}
	tmpbuf.data[0] = SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED;
	SEC_ASN1EncodeLength(&tmpbuf.data[1],userNotice->derNoticeReference.len);
	PORT_Memcpy(&tmpbuf.data[newBytes],userNotice->derNoticeReference.data,
				userNotice->derNoticeReference.len);

	/* OK, no decode it */
    	rv = SEC_QuickDERDecodeItem(arena, &userNotice->noticeReference, 
	    CERT_NoticeReferenceTemplate, &tmpbuf);

	PORT_Free(tmpbuf.data); tmpbuf.data = NULL;
    	if ( rv != SECSuccess ) {
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
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTOidSequence, oids),
	  SEC_ObjectIDTemplate }
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
