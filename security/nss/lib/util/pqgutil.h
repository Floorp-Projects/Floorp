/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef _PQGUTIL_H_
#define _PQGUTIL_H_ 1

#include "blapi.h"

/**************************************************************************
 *  Return a pointer to a new PQGParams struct that is a duplicate of     *
 *  the one passed as an argument.                                        *
 *  Return NULL on failure, or if NULL was passed in.                     *
 **************************************************************************/
extern PQGParams * PQG_DupParams(const PQGParams *src);


/**************************************************************************
 *  Return a pointer to a new PQGParams struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/
extern PQGParams * PQG_NewParams(const SECItem * prime, 
                                 const SECItem * subPrime, 
                                 const SECItem * base);


/**************************************************************************
 * Fills in caller's "prime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(prime, PR_FALSE);	
 **************************************************************************/
extern SECStatus PQG_GetPrimeFromParams(const PQGParams *params, 
                                        SECItem * prime);


/**************************************************************************
 * Fills in caller's "subPrime" SECItem with the prime value in params.
 * Contents can be freed by calling SECITEM_FreeItem(subPrime, PR_FALSE);	
 **************************************************************************/
extern SECStatus PQG_GetSubPrimeFromParams(const PQGParams *params, 
                                           SECItem * subPrime);


/**************************************************************************
 * Fills in caller's "base" SECItem with the base value in params.
 * Contents can be freed by calling SECITEM_FreeItem(base, PR_FALSE);	
 **************************************************************************/
extern SECStatus PQG_GetBaseFromParams(const PQGParams *params, SECItem *base);


/**************************************************************************
 *  Free the PQGParams struct and the things it points to.                *
 **************************************************************************/
extern void PQG_DestroyParams(PQGParams *params);


/**************************************************************************
 *  Return a pointer to a new PQGVerify struct that is a duplicate of     *
 *  the one passed as an argument.                                        *
 *  Return NULL on failure, or if NULL was passed in.                     *
 **************************************************************************/
extern PQGVerify * PQG_DupVerify(const PQGVerify *src);


/**************************************************************************
 *  Return a pointer to a new PQGVerify struct that is constructed from   *
 *  copies of the arguments passed in.                                    *
 *  Return NULL on failure.                                               *
 **************************************************************************/
extern PQGVerify * PQG_NewVerify(unsigned int counter, const SECItem * seed, 
                                 const SECItem * h);


/**************************************************************************
 * Returns "counter" value from the PQGVerify.
 **************************************************************************/
extern unsigned int PQG_GetCounterFromVerify(const PQGVerify *verify);

/**************************************************************************
 * Fills in caller's "seed" SECItem with the seed value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(seed, PR_FALSE);	
 **************************************************************************/
extern SECStatus PQG_GetSeedFromVerify(const PQGVerify *verify, SECItem *seed);


/**************************************************************************
 * Fills in caller's "h" SECItem with the h value in verify.
 * Contents can be freed by calling SECITEM_FreeItem(h, PR_FALSE);	
 **************************************************************************/
extern SECStatus PQG_GetHFromVerify(const PQGVerify *verify, SECItem * h);


/**************************************************************************
 *  Free the PQGVerify struct and the things it points to.                *
 **************************************************************************/
extern void PQG_DestroyVerify(PQGVerify *vfy);


#endif
