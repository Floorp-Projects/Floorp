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
 * X.509 Extension Encoding  
 */

#include "prtypes.h"
#include "seccomon.h"
#include "secdert.h"
#include "secoidt.h"
#include "secasn1t.h"
#include "secasn1.h"
#include "cert.h"
#include "secder.h"
#include "prprf.h"
#include "xconst.h"
#include "genname.h"
#include "secasn1.h"
#include "secerr.h"


static const SEC_ASN1Template CERTSubjectKeyIDTemplate[] = {
    { SEC_ASN1_OCTET_STRING }
};


static const SEC_ASN1Template CERTIA5TypeTemplate[] = {
    { SEC_ASN1_IA5_STRING }
};

SEC_ASN1_MKSUB(SEC_GeneralizedTimeTemplate)

static const SEC_ASN1Template CERTPrivateKeyUsagePeriodTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(CERTPrivKeyUsagePeriod) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC  | SEC_ASN1_XTRN | 0,
	  offsetof(CERTPrivKeyUsagePeriod, notBefore), 
	  SEC_ASN1_SUB(SEC_GeneralizedTimeTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC  | SEC_ASN1_XTRN | 1,
	  offsetof(CERTPrivKeyUsagePeriod, notAfter), 
	  SEC_ASN1_SUB(SEC_GeneralizedTimeTemplate)},
    { 0, } 
};


const SEC_ASN1Template CERTAltNameTemplate[] = {
    { SEC_ASN1_CONSTRUCTED, offsetof(CERTAltNameEncodedContext, encodedGenName), 
      CERT_GeneralNamesTemplate}
};

const SEC_ASN1Template CERTAuthInfoAccessItemTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(CERTAuthInfoAccess) },
    { SEC_ASN1_OBJECT_ID,
      offsetof(CERTAuthInfoAccess, method) },
    { SEC_ASN1_ANY,
      offsetof(CERTAuthInfoAccess, derLocation) },
    { 0, }
};

const SEC_ASN1Template CERTAuthInfoAccessTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, CERTAuthInfoAccessItemTemplate }
};


SECStatus 
CERT_EncodeSubjectKeyID(PRArenaPool *arena, const SECItem* srcString,
                        SECItem *encodedValue)
{
    SECStatus rv = SECSuccess;

    if (!srcString) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (SEC_ASN1EncodeItem (arena, encodedValue, srcString,
			    CERTSubjectKeyIDTemplate) == NULL) {
	rv = SECFailure;
    }
    
    return(rv);
}


SECStatus
CERT_EncodePrivateKeyUsagePeriod(PRArenaPool *arena, 
                                CERTPrivKeyUsagePeriod *pkup, 
				SECItem *encodedValue)
{
    SECStatus rv = SECSuccess;

    if (SEC_ASN1EncodeItem (arena, encodedValue, pkup,
			    CERTPrivateKeyUsagePeriodTemplate) == NULL) {
	rv = SECFailure;
    }
    return(rv);
}

CERTPrivKeyUsagePeriod *
CERT_DecodePrivKeyUsagePeriodExtension(PLArenaPool *arena, SECItem *extnValue)
{
    SECStatus rv;
    CERTPrivKeyUsagePeriod *pPeriod;
    SECItem newExtnValue;

    /* allocate the certificate policies structure */
    pPeriod = PORT_ArenaZNew(arena, CERTPrivKeyUsagePeriod);
    if ( pPeriod == NULL ) {
	goto loser;
    }
    
    pPeriod->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newExtnValue, extnValue);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, pPeriod, 
                                CERTPrivateKeyUsagePeriodTemplate,
			        &newExtnValue);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    return pPeriod;
    
loser:
    return NULL;
}


SECStatus 
CERT_EncodeIA5TypeExtension(PRArenaPool *arena, char *value, SECItem *encodedValue)
{
    SECItem encodeContext;
    SECStatus rv = SECSuccess;


    PORT_Memset (&encodeContext, 0, sizeof (encodeContext));
    
    if (value != NULL) {
	encodeContext.data = (unsigned char *)value;
	encodeContext.len = strlen(value);
    }
    if (SEC_ASN1EncodeItem (arena, encodedValue, &encodeContext,
			    CERTIA5TypeTemplate) == NULL) {
	rv = SECFailure;
    }
    
    return(rv);
}

SECStatus
CERT_EncodeAltNameExtension(PRArenaPool *arena,  CERTGeneralName  *value, SECItem *encodedValue)
{
    SECItem                **encodedGenName;
    SECStatus              rv = SECSuccess;

    encodedGenName = cert_EncodeGeneralNames(arena, value);
    if (SEC_ASN1EncodeItem (arena, encodedValue, &encodedGenName,
			    CERT_GeneralNamesTemplate) == NULL) {
	rv = SECFailure;
    }

    return rv;
}

CERTGeneralName *
CERT_DecodeAltNameExtension(PRArenaPool *reqArena, SECItem *EncodedAltName)
{
    SECStatus                  rv = SECSuccess;
    CERTAltNameEncodedContext  encodedContext;
    SECItem*                   newEncodedAltName;

    if (!reqArena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    newEncodedAltName = SECITEM_ArenaDupItem(reqArena, EncodedAltName);
    if (!newEncodedAltName) {
        return NULL;
    }

    encodedContext.encodedGenName = NULL;
    PORT_Memset(&encodedContext, 0, sizeof(CERTAltNameEncodedContext));
    rv = SEC_QuickDERDecodeItem (reqArena, &encodedContext,
                                 CERT_GeneralNamesTemplate, newEncodedAltName);
    if (rv == SECFailure) {
	goto loser;
    }
    if (encodedContext.encodedGenName && encodedContext.encodedGenName[0])
	return cert_DecodeGeneralNames(reqArena,
                                       encodedContext.encodedGenName);
    /* Extension contained an empty GeneralNames sequence */
    /* Treat as extension not found */
    PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
loser:
    return NULL;
}


SECStatus
CERT_EncodeNameConstraintsExtension(PRArenaPool          *arena, 
				    CERTNameConstraints  *value,
				    SECItem              *encodedValue)
{
    SECStatus     rv = SECSuccess;
    
    rv = cert_EncodeNameConstraints(value, arena, encodedValue);
    return rv;
}


CERTNameConstraints *
CERT_DecodeNameConstraintsExtension(PRArenaPool          *arena,
				    SECItem              *encodedConstraints)
{
    return cert_DecodeNameConstraints(arena, encodedConstraints);
}


CERTAuthInfoAccess **
CERT_DecodeAuthInfoAccessExtension(PRArenaPool *reqArena,
				   SECItem     *encodedExtension)
{
    CERTAuthInfoAccess **info = NULL;
    SECStatus rv;
    int i;
    SECItem* newEncodedExtension;

    if (!reqArena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    newEncodedExtension = SECITEM_ArenaDupItem(reqArena, encodedExtension);
    if (!newEncodedExtension) {
        return NULL;
    }

    rv = SEC_QuickDERDecodeItem(reqArena, &info, CERTAuthInfoAccessTemplate, 
			    newEncodedExtension);
    if (rv != SECSuccess || info == NULL) {
	return NULL;
    }

    for (i = 0; info[i] != NULL; i++) {
	info[i]->location = CERT_DecodeGeneralName(reqArena,
						   &(info[i]->derLocation),
						   NULL);
    }
    return info;
}

SECStatus
CERT_EncodeInfoAccessExtension(PRArenaPool *arena,
				   CERTAuthInfoAccess **info,
				   SECItem *dest)
{
    SECItem *dummy;
    int i;

    PORT_Assert(info != NULL);
    PORT_Assert(dest != NULL);
    if (info == NULL || dest == NULL) {
	return SECFailure;
    }

    for (i = 0; info[i] != NULL; i++) {
	if (CERT_EncodeGeneralName(info[i]->location, &(info[i]->derLocation),
				   arena) == NULL)
	    /* Note that this may leave some of the locations filled in. */
	    return SECFailure;
    }
    dummy = SEC_ASN1EncodeItem(arena, dest, &info,
			       CERTAuthInfoAccessTemplate);
    if (dummy == NULL) {
	return SECFailure;
    }
    return SECSuccess;
}
