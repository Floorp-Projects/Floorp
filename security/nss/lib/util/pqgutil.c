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
#include "pqgutil.h"
#include "prerror.h"
#include "secitem.h"

#define PQG_DEFAULT_CHUNKSIZE 2048	/* bytes */

/**************************************************************************
 *  Return a pointer to a new PQGParams struct that is a duplicate of     *
 *  the one passed as an argument.                                        *
 *  Return NULL on failure, or if NULL was passed in.                     *
 *                                                                        *
 **************************************************************************/

PQGParams *
PQG_DupParams(const PQGParams *src)
{
    PRArenaPool *arena;
    PQGParams *dest;
    SECStatus status;

    if (src == NULL) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return NULL;
    }

    arena = PORT_NewArena(PQG_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	goto loser;

    dest = (PQGParams*)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
    if (dest == NULL)
	goto loser;

    dest->arena = arena;

    status = SECITEM_CopyItem(arena, &dest->prime, &src->prime);
    if (status != SECSuccess)
	goto loser;

    status = SECITEM_CopyItem(arena, &dest->subPrime, &src->subPrime);
    if (status != SECSuccess)
	goto loser;

    status = SECITEM_CopyItem(arena, &dest->base, &src->base);
    if (status != SECSuccess)
	goto loser;

    return dest;

loser:
    if (arena != NULL)
	PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

/**************************************************************************
 *  Return a pointer to a new PQGParams struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/

PQGParams *
PQG_NewParams(const SECItem * prime, const SECItem * subPrime, 
              const SECItem * base)
{
    PQGParams *  dest;
    PQGParams    src;

    src.arena    = NULL;
    src.prime    = *prime;
    src.subPrime = *subPrime;
    src.base     = *base;
    dest         = PQG_DupParams(&src);
    return dest;
}

/**************************************************************************
 * Fills in caller's "prime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(prime, PR_FALSE);	
 **************************************************************************/
SECStatus
PQG_GetPrimeFromParams(const PQGParams *params, SECItem * prime)
{
    return SECITEM_CopyItem(NULL, prime, &params->prime);
}

/**************************************************************************
 * Fills in caller's "subPrime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(subPrime, PR_FALSE);	
 **************************************************************************/
SECStatus
PQG_GetSubPrimeFromParams(const PQGParams *params, SECItem * subPrime)
{
    return SECITEM_CopyItem(NULL, subPrime, &params->subPrime);
}

/**************************************************************************
 * Fills in caller's "base" SECItem with the base value in params.
 * Contents can be freed by calling SECITEM_FreeItem(base, PR_FALSE);	
 **************************************************************************/
SECStatus
PQG_GetBaseFromParams(const PQGParams *params, SECItem * base)
{
    return SECITEM_CopyItem(NULL, base, &params->base);
}

/**************************************************************************
 *  Free the PQGParams struct and the things it points to.                *
 **************************************************************************/
void
PQG_DestroyParams(PQGParams *params)
{
    if (params == NULL) 
    	return;
    if (params->arena != NULL) {
	PORT_FreeArena(params->arena, PR_FALSE);	/* don't zero it */
    } else {
	SECITEM_FreeItem(&params->prime,    PR_FALSE); /* don't free prime */
	SECITEM_FreeItem(&params->subPrime, PR_FALSE); /* don't free subPrime */
	SECITEM_FreeItem(&params->base,     PR_FALSE); /* don't free base */
	PORT_Free(params);
    }
}

/**************************************************************************
 *  Return a pointer to a new PQGVerify struct that is a duplicate of     *
 *  the one passed as an argument.                                        *
 *  Return NULL on failure, or if NULL was passed in.                     *
 **************************************************************************/

PQGVerify *
PQG_DupVerify(const PQGVerify *src)
{
    PRArenaPool *arena;
    PQGVerify *  dest;
    SECStatus    status;

    if (src == NULL) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return NULL;
    }

    arena = PORT_NewArena(PQG_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	goto loser;

    dest = (PQGVerify*)PORT_ArenaZAlloc(arena, sizeof(PQGVerify));
    if (dest == NULL)
	goto loser;

    dest->arena   = arena;
    dest->counter = src->counter;

    status = SECITEM_CopyItem(arena, &dest->seed, &src->seed);
    if (status != SECSuccess)
	goto loser;

    status = SECITEM_CopyItem(arena, &dest->h, &src->h);
    if (status != SECSuccess)
	goto loser;

    return dest;

loser:
    if (arena != NULL)
	PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

/**************************************************************************
 *  Return a pointer to a new PQGVerify struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/

PQGVerify *
PQG_NewVerify(unsigned int counter, const SECItem * seed, const SECItem * h)
{
    PQGVerify *  dest;
    PQGVerify    src;

    src.arena    = NULL;
    src.counter  = counter;
    src.seed     = *seed;
    src.h        = *h;
    dest         = PQG_DupVerify(&src);
    return dest;
}

/**************************************************************************
 * Returns the "counter" value from the PQGVerify.
 **************************************************************************/
unsigned int
PQG_GetCounterFromVerify(const PQGVerify *verify)
{
    return verify->counter;
}

/**************************************************************************
 * Fills in caller's "seed" SECItem with the seed value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(seed, PR_FALSE);	
 **************************************************************************/
SECStatus
PQG_GetSeedFromVerify(const PQGVerify *verify, SECItem * seed)
{
    return SECITEM_CopyItem(NULL, seed, &verify->seed);
}

/**************************************************************************
 * Fills in caller's "h" SECItem with the h value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(h, PR_FALSE);	
 **************************************************************************/
SECStatus
PQG_GetHFromVerify(const PQGVerify *verify, SECItem * h)
{
    return SECITEM_CopyItem(NULL, h, &verify->h);
}

/**************************************************************************
 *  Free the PQGVerify struct and the things it points to.                *
 **************************************************************************/

void
PQG_DestroyVerify(PQGVerify *vfy)
{
    if (vfy == NULL) 
    	return;
    if (vfy->arena != NULL) {
	PORT_FreeArena(vfy->arena, PR_FALSE);	/* don't zero it */
    } else {
	SECITEM_FreeItem(&vfy->seed,   PR_FALSE); /* don't free seed */
	SECITEM_FreeItem(&vfy->h,      PR_FALSE); /* don't free h */
	PORT_Free(vfy);
    }
}
