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

#include "secoid.h"
#include "secder.h"	/* XXX remove this when remove the DERTemplate */
#include "secasn1.h"
#include "secitem.h"
#include "secerr.h"

SECOidTag
SECOID_GetAlgorithmTag(SECAlgorithmID *id)
{
    if (id == NULL || id->algorithm.data == NULL)
	return SEC_OID_UNKNOWN;

    return SECOID_FindOIDTag (&(id->algorithm));
}

SECStatus
SECOID_SetAlgorithmID(PRArenaPool *arena, SECAlgorithmID *id, SECOidTag which,
		      SECItem *params)
{
    SECOidData *oiddata;
    PRBool add_null_param;

    oiddata = SECOID_FindOIDByTag(which);
    if ( !oiddata ) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }

    if (SECITEM_CopyItem(arena, &id->algorithm, &oiddata->oid))
	return SECFailure;

    switch (which) {
      case SEC_OID_MD2:
      case SEC_OID_MD4:
      case SEC_OID_MD5:
      case SEC_OID_SHA1:
      case SEC_OID_SHA256:
      case SEC_OID_SHA384:
      case SEC_OID_SHA512:
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
	add_null_param = PR_TRUE;
	break;
      default:
	add_null_param = PR_FALSE;
	break;
    }

    if (params) {
	/*
	 * I am specifically *not* enforcing the following assertion
	 * (by following it up with an error and a return of failure)
	 * because I do not want to introduce any change in the current
	 * behavior.  But I do want for us to notice if the following is
	 * ever true, because I do not think it should be so and probably
	 * signifies an error/bug somewhere.
	 */
	PORT_Assert(!add_null_param || (params->len == 2
					&& params->data[0] == SEC_ASN1_NULL
					&& params->data[1] == 0)); 
	if (SECITEM_CopyItem(arena, &id->parameters, params)) {
	    return SECFailure;
	}
    } else {
	/*
	 * Again, this is not considered an error.  But if we assume
	 * that nobody tries to set the parameters field themselves
	 * (but always uses this routine to do that), then we should
	 * not hit the following assertion.  Unless they forgot to zero
	 * the structure, which could also be a bad (and wrong) thing.
	 */
	PORT_Assert(id->parameters.data == NULL);

	if (add_null_param) {
	    (void) SECITEM_AllocItem(arena, &id->parameters, 2);
	    if (id->parameters.data == NULL) {
		return SECFailure;
	    }
	    id->parameters.data[0] = SEC_ASN1_NULL;
	    id->parameters.data[1] = 0;
	}
    }

    return SECSuccess;
}

SECStatus
SECOID_CopyAlgorithmID(PRArenaPool *arena, SECAlgorithmID *to, SECAlgorithmID *from)
{
    SECStatus rv;

    rv = SECITEM_CopyItem(arena, &to->algorithm, &from->algorithm);
    if (rv) return rv;
    rv = SECITEM_CopyItem(arena, &to->parameters, &from->parameters);
    return rv;
}

void SECOID_DestroyAlgorithmID(SECAlgorithmID *algid, PRBool freeit)
{
    SECITEM_FreeItem(&algid->parameters, PR_FALSE);
    SECITEM_FreeItem(&algid->algorithm, PR_FALSE);
    if(freeit == PR_TRUE)
        PORT_Free(algid);
}

SECComparison
SECOID_CompareAlgorithmID(SECAlgorithmID *a, SECAlgorithmID *b)
{
    SECComparison rv;

    rv = SECITEM_CompareItem(&a->algorithm, &b->algorithm);
    if (rv) return rv;
    rv = SECITEM_CompareItem(&a->parameters, &b->parameters);
    return rv;
}
