/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Support for various policy related extensions
 *
 * $Id$
 */

#include "seccomon.h"
#include "secport.h"
#include "secder.h"
#include "cert.h"
#include "secoid.h"
#include "secasn1.h"
#include "secerr.h"
#include "nspr.h"
#include "secutil.h"

/* This implementation is derived from the one in nss/lib/certdb/policyxtn.c .
** The chief difference is the addition of the OPTIONAL flag to many 
** parts.  The idea is to be able to parse and print as much of the 
** policy extension as possible, even if some parts are invalid.
**
** If this approach still is unable to decode policy extensions that
** contain invalid parts, then the next approach will be to parse 
** the PolicyInfos as a SEQUENCE of ANYs, and then parse each of them 
** as PolicyInfos, with the PolicyQualifiers being ANYs, and finally
** parse each of the PolicyQualifiers.
*/

static const SEC_ASN1Template secu_PolicyQualifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTPolicyQualifier) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyQualifier, qualifierID) },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL,
	  offsetof(CERTPolicyQualifier, qualifierValue) },
    { 0 }
};

static const SEC_ASN1Template secu_PolicyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTPolicyInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTPolicyInfo, policyID) },
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_OPTIONAL,
	  offsetof(CERTPolicyInfo, policyQualifiers),
	  secu_PolicyQualifierTemplate },
    { 0 }
};

static const SEC_ASN1Template secu_CertificatePoliciesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCertificatePolicies, policyInfos),
	  secu_PolicyInfoTemplate, sizeof(CERTCertificatePolicies)  }
};


static CERTCertificatePolicies *
secu_DecodeCertificatePoliciesExtension(SECItem *extnValue)
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
    policies = PORT_ArenaZNew(arena, CERTCertificatePolicies);
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
    rv = SEC_QuickDERDecodeItem(arena, policies, 
                                secu_CertificatePoliciesTemplate,
			        &newExtnValue);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* initialize the oid tags */
    policyInfos = policies->policyInfos;
    while (policyInfos != NULL && *policyInfos != NULL ) {
	policyInfo = *policyInfos;
	policyInfo->oid = SECOID_FindOIDTag(&policyInfo->policyID);
	policyQualifiers = policyInfo->policyQualifiers;
	while ( policyQualifiers && *policyQualifiers != NULL ) {
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


static char *
itemToString(SECItem *item)
{
    char *string;

    string = PORT_ZAlloc(item->len+1);
    if (string == NULL) return NULL;
    PORT_Memcpy(string,item->data,item->len);
    string[item->len] = 0;
    return string;
}

static SECStatus
secu_PrintUserNoticeQualifier(FILE *out, SECItem * qualifierValue,
                              char *msg, int level)
{
    CERTUserNotice *userNotice = NULL;
    if (qualifierValue)
	userNotice = CERT_DecodeUserNotice(qualifierValue);
    if (userNotice) {
	if (userNotice->noticeReference.organization.len != 0) {
            char *string = 
	            itemToString(&userNotice->noticeReference.organization);
            SECItem **itemList = userNotice->noticeReference.noticeNumbers;

	    while (itemList && *itemList) {
		SECU_PrintInteger(out,*itemList,string,level+1);
	        itemList++;
	    }
	    PORT_Free(string);
	}
	if (userNotice->displayText.len != 0) {
	    SECU_PrintString(out,&userNotice->displayText,
			     "Display Text", level+1);
	}
	CERT_DestroyUserNotice(userNotice);
	return SECSuccess;
    }
    return SECFailure;	/* caller will print this value */
}

static SECStatus
secu_PrintPolicyQualifier(FILE *out,CERTPolicyQualifier *policyQualifier,
			  char *msg,int level)
{
   SECStatus rv;
   SECItem * qualifierValue = &policyQualifier->qualifierValue;

   SECU_PrintObjectID(out, &policyQualifier->qualifierID , 
					"Policy Qualifier Name", level);
   if (!qualifierValue->data) {
	SECU_Indent(out, level);
	fprintf(out,"Error: missing qualifier\n");
   } else 
   switch (policyQualifier->oid) {
   case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
       rv = secu_PrintUserNoticeQualifier(out, qualifierValue, msg, level);
       if (SECSuccess == rv)
	   break;
       /* fall through on error */
   case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
   default:
	SECU_PrintAny(out, qualifierValue, "Policy Qualifier Data", level);
	break;
   }
   return SECSuccess;
}

static SECStatus
secu_PrintPolicyInfo(FILE *out,CERTPolicyInfo *policyInfo,char *msg,int level)
{
   CERTPolicyQualifier **policyQualifiers;

   policyQualifiers = policyInfo->policyQualifiers;
   SECU_PrintObjectID(out, &policyInfo->policyID , "Policy Name", level);
   
   while (policyQualifiers && *policyQualifiers != NULL) {
	secu_PrintPolicyQualifier(out,*policyQualifiers,"",level+1);
	policyQualifiers++;
   }
   return SECSuccess;
}

void
SECU_PrintPolicy(FILE *out, SECItem *value, char *msg, int level)
{
   CERTCertificatePolicies *policies = NULL;
   CERTPolicyInfo **policyInfos;

   if (msg) {
	SECU_Indent(out, level);
	fprintf(out,"%s: \n",msg);
	level++;
   }
   policies = secu_DecodeCertificatePoliciesExtension(value);
   if (policies == NULL) {
	SECU_PrintAny(out, value, "Invalid Policy Data", level);
	return;
   }

   policyInfos = policies->policyInfos;
   while (policyInfos && *policyInfos != NULL) {
	secu_PrintPolicyInfo(out,*policyInfos,"",level);
	policyInfos++;
   }

   CERT_DestroyCertificatePoliciesExtension(policies);
}


void
SECU_PrintPrivKeyUsagePeriodExtension(FILE *out, SECItem *value, 
			              char *msg, int level)
{
    CERTPrivKeyUsagePeriod * prd;
    PLArenaPool * arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if ( !arena ) {
	goto loser;
    }
    prd = CERT_DecodePrivKeyUsagePeriodExtension(arena, value);
    if (!prd) {
	goto loser;
    }
    if (prd->notBefore.data) {
	SECU_PrintGeneralizedTime(out, &prd->notBefore, "Not Before", level);
    }
    if (prd->notAfter.data) {
	SECU_PrintGeneralizedTime(out, &prd->notAfter,  "Not After ", level);
    }
    if (!prd->notBefore.data && !prd->notAfter.data) {
	SECU_Indent(out, level);
	fprintf(out, "Error: notBefore or notAfter MUST be present.\n");
loser:
	SECU_PrintAny(out, value, msg, level);
    }
    if (arena) {
	PORT_FreeArena(arena, PR_FALSE);
    }
}
