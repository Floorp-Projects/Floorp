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
 * X.509 v3 Subject Key Usage Extension 
 *
 */

#include "prtypes.h"
#include "mcom_db.h"
#include "seccomon.h"
#include "secdert.h"
#include "secoidt.h"
#include "secasn1t.h"
#include "secasn1.h"
#include "secport.h"
#include "certt.h"  
#include "genname.h"
#include "secerr.h"

   
const SEC_ASN1Template CERTAuthKeyIDTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CERTAuthKeyID) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(CERTAuthKeyID,keyID), SEC_OctetStringTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC  | 1,
          offsetof(CERTAuthKeyID, DERAuthCertIssuer), CERT_GeneralNamesTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(CERTAuthKeyID,authCertSerialNumber), SEC_IntegerTemplate},
    { 0 }
};



SECStatus CERT_EncodeAuthKeyID (PRArenaPool *arena, CERTAuthKeyID *value, SECItem *encodedValue)
{
    SECStatus rv = SECFailure;
 
    PORT_Assert (value);
    PORT_Assert (arena);
    PORT_Assert (value->DERAuthCertIssuer == NULL);
    PORT_Assert (encodedValue);

    do {
	
	/* If both of the authCertIssuer and the serial number exist, encode
	   the name first.  Otherwise, it is an error if one exist and the other
	   is not.
	 */
	if (value->authCertIssuer) {
	    if (!value->authCertSerialNumber.data) {
		PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
		break;
	    }

	    value->DERAuthCertIssuer = cert_EncodeGeneralNames
		(arena, value->authCertIssuer);
	    if (!value->DERAuthCertIssuer) {
		PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
		break;
	    }
	}
	else if (value->authCertSerialNumber.data) {
		PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
		break;
	}

	if (SEC_ASN1EncodeItem (arena, encodedValue, value,
				CERTAuthKeyIDTemplate) == NULL)
	    break;
	rv = SECSuccess;

    } while (0);
     return(rv);
}

CERTAuthKeyID *
CERT_DecodeAuthKeyID (PRArenaPool *arena, SECItem *encodedValue)
{
    CERTAuthKeyID * value = NULL;
    SECStatus       rv    = SECFailure;
    void *          mark;
    SECItem         newEncodedValue;

    PORT_Assert (arena);
   
    do {
	mark = PORT_ArenaMark (arena);
        value = (CERTAuthKeyID*)PORT_ArenaZAlloc (arena, sizeof (*value));
	value->DERAuthCertIssuer = NULL;
	if (value == NULL)
	    break;
        /* copy the DER into the arena, since Quick DER returns data that points
           into the DER input, which may get freed by the caller */
        rv = SECITEM_CopyItem(arena, &newEncodedValue, encodedValue);
        if ( rv != SECSuccess ) {
	    break;
        }

        rv = SEC_QuickDERDecodeItem
	     (arena, value, CERTAuthKeyIDTemplate, &newEncodedValue);
	if (rv != SECSuccess)
	    break;

        value->authCertIssuer = cert_DecodeGeneralNames (arena, value->DERAuthCertIssuer);
	if (value->authCertIssuer == NULL)
	    break;
	
	/* what if the general name contains other format but not URI ?
	   hl
	 */
	if ((value->authCertSerialNumber.data && !value->authCertIssuer) ||
	    (!value->authCertSerialNumber.data && value->authCertIssuer)){
	    PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
	    break;
	}
    } while (0);

    if (rv != SECSuccess) {
	PORT_ArenaRelease (arena, mark);
	return ((CERTAuthKeyID *)NULL);	    
    } 
    PORT_ArenaUnmark(arena, mark);
    return (value);
}
