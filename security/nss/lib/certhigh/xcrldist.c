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
 * Code for dealing with x.509 v3 CRL Distribution Point extension.
 */
#include "genname.h"
#include "certt.h"
#include "secerr.h"

SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(SEC_BitStringTemplate)

extern void PrepareBitStringForEncoding (SECItem *bitMap, SECItem *value);

static const SEC_ASN1Template FullNameTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 0,
	offsetof (CRLDistributionPoint,derFullName), 
	CERT_GeneralNamesTemplate}
};

static const SEC_ASN1Template RelativeNameTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 1, 
	offsetof (CRLDistributionPoint,distPoint.relativeName), 
	CERT_RDNTemplate}
};

static const SEC_ASN1Template DistributionPointNameTemplate[] = {
    { SEC_ASN1_CHOICE,
	offsetof(CRLDistributionPoint, distPointType), NULL,
	sizeof(CRLDistributionPoint) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 0,
	offsetof (CRLDistributionPoint, derFullName), 
	CERT_GeneralNamesTemplate, generalName },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 1, 
	offsetof (CRLDistributionPoint, distPoint.relativeName), 
	CERT_RDNTemplate, relativeDistinguishedName },
    { 0 }
};

static const SEC_ASN1Template CRLDistributionPointTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRLDistributionPoint) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	    SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_XTRN | 0,
	    offsetof(CRLDistributionPoint,derDistPoint),
            SEC_ASN1_SUB(SEC_AnyTemplate)},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
	    offsetof(CRLDistributionPoint,bitsmap),
            SEC_ASN1_SUB(SEC_BitStringTemplate) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	    SEC_ASN1_CONSTRUCTED | 2,
	    offsetof(CRLDistributionPoint, derCrlIssuer), 
	    CERT_GeneralNamesTemplate},
    { 0 }
};

const SEC_ASN1Template CERTCRLDistributionPointsTemplate[] = {
    {SEC_ASN1_SEQUENCE_OF, 0, CRLDistributionPointTemplate}
};

SECStatus
CERT_EncodeCRLDistributionPoints (PLArenaPool *arena, 
				  CERTCrlDistributionPoints *value,
				  SECItem *derValue)
{
    CRLDistributionPoint **pointList, *point;
    PLArenaPool *ourPool = NULL;
    SECStatus rv = SECSuccess;

    PORT_Assert (derValue);
    PORT_Assert (value && value->distPoints);

    do {
	ourPool = PORT_NewArena (SEC_ASN1_DEFAULT_ARENA_SIZE);
	if (ourPool == NULL) {
	    rv = SECFailure;
	    break;
	}    
	
	pointList = value->distPoints;
	while (*pointList) {
	    point = *pointList;
	    point->derFullName = NULL;
	    point->derDistPoint.data = NULL;

	    switch (point->distPointType) {
	    case generalName:
		point->derFullName = cert_EncodeGeneralNames
		    (ourPool, point->distPoint.fullName);
		
		if (!point->derFullName ||
		    !SEC_ASN1EncodeItem (ourPool, &point->derDistPoint,
			  point, FullNameTemplate))
		    rv = SECFailure;
		break;

	    case relativeDistinguishedName:
		if (!SEC_ASN1EncodeItem(ourPool, &point->derDistPoint, 
		      point, RelativeNameTemplate)) 
		    rv = SECFailure;
		break;

	    /* distributionPointName is omitted */
	    case 0: break;

	    default:
		PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
		rv = SECFailure;
		break;
	    }

	    if (rv != SECSuccess)
		break;

	    if (point->reasons.data)
		PrepareBitStringForEncoding (&point->bitsmap, &point->reasons);

	    if (point->crlIssuer) {
		point->derCrlIssuer = cert_EncodeGeneralNames
		    (ourPool, point->crlIssuer);
		if (!point->derCrlIssuer) {
		    rv = SECFailure;
		    break;
	    	}
	    }
	    ++pointList;
	}
	if (rv != SECSuccess)
	    break;
	if (!SEC_ASN1EncodeItem(arena, derValue, value, 
		CERTCRLDistributionPointsTemplate)) {
	    rv = SECFailure;
	    break;
	}
    } while (0);
    PORT_FreeArena (ourPool, PR_FALSE);
    return rv;
}

CERTCrlDistributionPoints *
CERT_DecodeCRLDistributionPoints (PLArenaPool *arena, SECItem *encodedValue)
{
   CERTCrlDistributionPoints *value = NULL;    
   CRLDistributionPoint **pointList, *point;    
   SECStatus rv = SECSuccess;
   SECItem newEncodedValue;

   PORT_Assert (arena);
   do {
	value = PORT_ArenaZNew(arena, CERTCrlDistributionPoints);
	if (value == NULL) {
	    rv = SECFailure;
	    break;
	}

        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newEncodedValue, encodedValue);
        if (rv != SECSuccess)
	    break;

	rv = SEC_QuickDERDecodeItem(arena, &value->distPoints, 
		CERTCRLDistributionPointsTemplate, &newEncodedValue);
	if (rv != SECSuccess)
	    break;

	pointList = value->distPoints;
	while (NULL != (point = *pointList)) {

	    /* get the data if the distributionPointName is not omitted */
	    if (point->derDistPoint.data != NULL) {
		rv = SEC_QuickDERDecodeItem(arena, point, 
			DistributionPointNameTemplate, &(point->derDistPoint));
		if (rv != SECSuccess)
		    break;

		switch (point->distPointType) {
		case generalName:
		    point->distPoint.fullName = 
			cert_DecodeGeneralNames(arena, point->derFullName);
		    rv = point->distPoint.fullName ? SECSuccess : SECFailure;
		    break;

		case relativeDistinguishedName:
		    break;

		default:
		    PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
		    rv = SECFailure;
		    break;
		} /* end switch */
		if (rv != SECSuccess)
		    break;
	    } /* end if */

	    /* Get the reason code if it's not omitted in the encoding */
	    if (point->bitsmap.data != NULL) {
	    	SECItem bitsmap = point->bitsmap;
		DER_ConvertBitString(&bitsmap);
		rv = SECITEM_CopyItem(arena, &point->reasons, &bitsmap);
		if (rv != SECSuccess)
		    break;
	    }

	    /* Get the crl issuer name if it's not omitted in the encoding */
	    if (point->derCrlIssuer != NULL) {
		point->crlIssuer = cert_DecodeGeneralNames(arena, 
			           point->derCrlIssuer);
		if (!point->crlIssuer)
		    break;
	    }
	    ++pointList;
	} /* end while points remain */
   } while (0);
   return (rv == SECSuccess ? value : NULL);
}
