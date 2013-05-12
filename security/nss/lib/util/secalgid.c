/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
SECOID_SetAlgorithmID(PLArenaPool *arena, SECAlgorithmID *id, SECOidTag which,
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
      case SEC_OID_SHA224:
      case SEC_OID_SHA256:
      case SEC_OID_SHA384:
      case SEC_OID_SHA512:
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
      case SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION:
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
SECOID_CopyAlgorithmID(PLArenaPool *arena, SECAlgorithmID *to, SECAlgorithmID *from)
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
