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
 * Portions created by the Initial Developer are Copyright (C) 1994-2001
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
/* Thse functions are stub functions which will get replaced with calls through
 * PKCS #11.
 */

#include "pk11func.h"
#include "secmod.h"
#include "secmodi.h"
#include "pkcs11t.h"
#include "pk11pqg.h"
#include "pqgutil.h"
#include "secerr.h"


/* Generate PQGParams and PQGVerify structs.
 * Length of P specified by j.  Length of h will match length of P.
 * Length of SEED in bytes specified in seedBytes.
 * seedBbytes must be in the range [20..255] or an error will result.
 */
extern SECStatus
PK11_PQG_ParamGenSeedLen( unsigned int j, unsigned int seedBytes,
				 PQGParams **pParams, PQGVerify **pVfy)
{
    PK11SlotInfo *slot = NULL;
    CK_ATTRIBUTE genTemplate[5];
    CK_ATTRIBUTE *attrs = genTemplate;
    int count = sizeof(genTemplate)/sizeof(genTemplate[0]);
    CK_MECHANISM mechanism;
    CK_OBJECT_HANDLE objectID = CK_INVALID_HANDLE;
    CK_RV crv;
    CK_ATTRIBUTE pTemplate[] = {
	{ CKA_PRIME, NULL, 0 },
	{ CKA_SUBPRIME, NULL, 0 },
	{ CKA_BASE, NULL, 0 },
    };
    CK_ATTRIBUTE vTemplate[] = {
	{ CKA_NETSCAPE_PQG_COUNTER, NULL, 0 },
	{ CKA_NETSCAPE_PQG_SEED, NULL, 0 },
	{ CKA_NETSCAPE_PQG_H, NULL, 0 },
    };
    int pTemplateCount = sizeof(pTemplate)/sizeof(pTemplate[0]);
    int vTemplateCount = sizeof(vTemplate)/sizeof(vTemplate[0]);
    PRArenaPool *parena = NULL;
    PRArenaPool *varena = NULL;
    PQGParams *params = NULL;
    PQGVerify *verify = NULL;
    CK_ULONG primeBits = j;
    CK_ULONG seedBits = seedBytes*8;

    *pParams = NULL;
    *pVfy =  NULL;

    PK11_SETATTRS(attrs, CKA_PRIME_BITS,&primeBits,sizeof(primeBits)); attrs++;
    if (seedBits != 0) {
    	PK11_SETATTRS(attrs, CKA_NETSCAPE_PQG_SEED_BITS, 
					&seedBits, sizeof(seedBits)); attrs++;
    }
    count = attrs - genTemplate;
    PR_ASSERT(count <= sizeof(genTemplate)/sizeof(CK_ATTRIBUTE));

    slot = PK11_GetInternalSlot();
    if (slot == NULL) {
	/* set error */
	goto loser;
    }

    /* Initialize the Key Gen Mechanism */
    mechanism.mechanism = CKM_DSA_PARAMETER_GEN;
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GenerateKey(slot->session,
			 &mechanism, genTemplate, count, &objectID);
    PK11_ExitSlotMonitor(slot);

    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }

    parena = PORT_NewArena(60);
    crv = PK11_GetAttributes(parena, slot, objectID, pTemplate, pTemplateCount);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }


    params = (PQGParams *)PORT_ArenaAlloc(parena,sizeof(PQGParams));
    if (params == NULL) {
	goto loser;
    }

    /* fill in Params */
    params->arena = parena;
    params->prime.type = siUnsignedInteger;
    params->prime.data = pTemplate[0].pValue;
    params->prime.len = pTemplate[0].ulValueLen;
    params->subPrime.type = siUnsignedInteger;
    params->subPrime.data = pTemplate[1].pValue;
    params->subPrime.len = pTemplate[1].ulValueLen;
    params->base.type = siUnsignedInteger;
    params->base.data = pTemplate[2].pValue;
    params->base.len = pTemplate[2].ulValueLen;


    varena = PORT_NewArena(60);
    crv = PK11_GetAttributes(varena, slot, objectID, vTemplate, vTemplateCount);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }


    verify = (PQGVerify *)PORT_ArenaAlloc(varena,sizeof(PQGVerify));
    if (verify == NULL) {
	goto loser;
    }
    /* fill in Params */
    verify->arena = varena;
    verify->counter = (unsigned int)(*(CK_ULONG*)vTemplate[0].pValue);
    verify->seed.type = siUnsignedInteger;
    verify->seed.data = vTemplate[1].pValue;
    verify->seed.len = vTemplate[1].ulValueLen;
    verify->h.type = siUnsignedInteger;
    verify->h.data = vTemplate[2].pValue;
    verify->h.len = vTemplate[2].ulValueLen;

    PK11_DestroyObject(slot,objectID);
    PK11_FreeSlot(slot);

    *pParams = params;
    *pVfy =  verify;

    return SECSuccess;

loser:
    if (objectID != CK_INVALID_HANDLE) {
	PK11_DestroyObject(slot,objectID);
    }
    if (parena != NULL) {
	PORT_FreeArena(parena,PR_FALSE);
    }
    if (varena != NULL) {
	PORT_FreeArena(varena,PR_FALSE);
    }
    if (slot) {
	PK11_FreeSlot(slot);
    }
    return SECFailure;
}

/* Generate PQGParams and PQGVerify structs.
 * Length of seed and length of h both equal length of P. 
 * All lengths are specified by "j", according to the table above.
 */
extern SECStatus
PK11_PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
    return PK11_PQG_ParamGenSeedLen(j, 0, pParams, pVfy);
}

/*  Test PQGParams for validity as DSS PQG values.
 *  If vfy is non-NULL, test PQGParams to make sure they were generated
 *       using the specified seed, counter, and h values.
 *
 *  Return value indicates whether Verification operation ran succesfully
 *  to completion, but does not indicate if PQGParams are valid or not.
 *  If return value is SECSuccess, then *pResult has these meanings:
 *       SECSuccess: PQGParams are valid.
 *       SECFailure: PQGParams are invalid.
 */

extern SECStatus
PK11_PQG_VerifyParams(const PQGParams *params, const PQGVerify *vfy, 
							SECStatus *result)
{
    CK_ATTRIBUTE keyTempl[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_KEY_TYPE, NULL, 0 },
	{ CKA_PRIME, NULL, 0 },
	{ CKA_SUBPRIME, NULL, 0 },
	{ CKA_BASE, NULL, 0 },
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_NETSCAPE_PQG_COUNTER, NULL, 0 },
	{ CKA_NETSCAPE_PQG_SEED, NULL, 0 },
	{ CKA_NETSCAPE_PQG_H, NULL, 0 },
    };
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS class = CKO_KG_PARAMETERS;
    CK_KEY_TYPE keyType = CKK_DSA;
    SECStatus rv = SECSuccess;
    PK11SlotInfo *slot;
    int keyCount;
    CK_OBJECT_HANDLE objectID;
    CK_ULONG counter;
    CK_RV crv;

    attrs = keyTempl;
    PK11_SETATTRS(attrs, CKA_CLASS, &class, sizeof(class)); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType)); attrs++;
    PK11_SETATTRS(attrs, CKA_PRIME, params->prime.data, 
						params->prime.len); attrs++;
    PK11_SETATTRS(attrs, CKA_SUBPRIME, params->subPrime.data, 
						params->subPrime.len); attrs++;
    PK11_SETATTRS(attrs, CKA_BASE,params->base.data,params->base.len); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckfalse, sizeof(ckfalse)); attrs++;
    if (vfy) {
	counter = vfy->counter;
	PK11_SETATTRS(attrs, CKA_NETSCAPE_PQG_COUNTER, 
			&counter, sizeof(counter)); attrs++;
	PK11_SETATTRS(attrs, CKA_NETSCAPE_PQG_SEED, 
			vfy->seed.data, vfy->seed.len); attrs++;
	PK11_SETATTRS(attrs, CKA_NETSCAPE_PQG_H, 
			vfy->h.data, vfy->h.len); attrs++;
    }

    keyCount = attrs - keyTempl;
    PORT_Assert(keyCount <= sizeof(keyTempl)/sizeof(keyTempl[0]));


    slot = PK11_GetInternalSlot();
    if (slot == NULL) {
	return SECFailure;
    }

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_CreateObject(slot->session, keyTempl, keyCount, 
								&objectID);
    PK11_ExitSlotMonitor(slot);

    /* throw away the keys, we only wanted the return code */
    PK11_DestroyObject(slot,objectID);
    PK11_FreeSlot(slot);

    *result = SECSuccess;
    if (crv == CKR_ATTRIBUTE_VALUE_INVALID) {
	*result = SECFailure;
    } else if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	rv = SECFailure;
    }
    return rv;

}



/**************************************************************************
 *  Free the PQGParams struct and the things it points to.                *
 **************************************************************************/
extern void 
PK11_PQG_DestroyParams(PQGParams *params) {
     PQG_DestroyParams(params);
     return;
}

/**************************************************************************
 *  Free the PQGVerify struct and the things it points to.                *
 **************************************************************************/
extern void
PK11_PQG_DestroyVerify(PQGVerify *vfy) {
    PQG_DestroyVerify(vfy);
    return;
}

/**************************************************************************
 *  Return a pointer to a new PQGParams struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/
extern PQGParams *
PK11_PQG_NewParams(const SECItem * prime, const SECItem * subPrime, 
                                 		const SECItem * base) {
    return PQG_NewParams(prime, subPrime, base);
}


/**************************************************************************
 * Fills in caller's "prime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(prime, PR_FALSE);	
 **************************************************************************/
extern SECStatus 
PK11_PQG_GetPrimeFromParams(const PQGParams *params, SECItem * prime) {
    return PQG_GetPrimeFromParams(params, prime);
}


/**************************************************************************
 * Fills in caller's "subPrime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(subPrime, PR_FALSE);	
 **************************************************************************/
extern SECStatus
PK11_PQG_GetSubPrimeFromParams(const PQGParams *params, SECItem * subPrime) {
    return PQG_GetSubPrimeFromParams(params, subPrime);
}


/**************************************************************************
 * Fills in caller's "base" SECItem with the base value in params.
 * Contents can be freed by calling SECITEM_FreeItem(base, PR_FALSE);	
 **************************************************************************/
extern SECStatus 
PK11_PQG_GetBaseFromParams(const PQGParams *params, SECItem *base) {
    return PQG_GetBaseFromParams(params, base);
}


/**************************************************************************
 *  Return a pointer to a new PQGVerify struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/
extern PQGVerify *
PK11_PQG_NewVerify(unsigned int counter, const SECItem * seed, 
							const SECItem * h) {
    return PQG_NewVerify(counter, seed, h);
}


/**************************************************************************
 * Returns "counter" value from the PQGVerify.
 **************************************************************************/
extern unsigned int 
PK11_PQG_GetCounterFromVerify(const PQGVerify *verify) {
    return PQG_GetCounterFromVerify(verify);
}

/**************************************************************************
 * Fills in caller's "seed" SECItem with the seed value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(seed, PR_FALSE);	
 **************************************************************************/
extern SECStatus 
PK11_PQG_GetSeedFromVerify(const PQGVerify *verify, SECItem *seed) {
    return PQG_GetSeedFromVerify(verify, seed);
}


/**************************************************************************
 * Fills in caller's "h" SECItem with the h value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(h, PR_FALSE);	
 **************************************************************************/
extern SECStatus 
PK11_PQG_GetHFromVerify(const PQGVerify *verify, SECItem * h) {
    return PQG_GetHFromVerify(verify, h);
}
