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

#ifndef _SECOID_H_
#define _SECOID_H_
/*
 * secoid.h - public data structures and prototypes for ASN.1 OID functions
 *
 * $Id: secoid.h,v 1.4 2001/08/24 18:34:34 relyea%netscape.com Exp $
 */

#include "plarena.h"

#include "seccomon.h"
#include "secoidt.h"
#include "secasn1t.h"

SEC_BEGIN_PROTOS

extern const SEC_ASN1Template SECOID_AlgorithmIDTemplate[];

/* This functions simply returns the address of the above-declared template. */
SEC_ASN1_CHOOSER_DECLARE(SECOID_AlgorithmIDTemplate)

/*
 * OID handling routines
 */
extern SECOidData *SECOID_FindOID(SECItem *oid);
extern SECOidTag SECOID_FindOIDTag(SECItem *oid);
extern SECOidData *SECOID_FindOIDByTag(SECOidTag tagnum);
extern SECOidData *SECOID_FindOIDByMechanism(unsigned long mechanism);

/****************************************/
/*
** Algorithm id handling operations
*/

/*
** Fill in an algorithm-ID object given a tag and some parameters.
** 	"aid" where the DER encoded algorithm info is stored (memory
**	   is allocated)
**	"tag" the tag defining the algorithm (SEC_OID_*)
**	"params" if not NULL, the parameters to go with the algorithm
*/
extern SECStatus SECOID_SetAlgorithmID(PRArenaPool *arena, SECAlgorithmID *aid,
				   SECOidTag tag, SECItem *params);

/*
** Copy the "src" object to "dest". Memory is allocated in "dest" for
** each of the appropriate sub-objects. Memory in "dest" is not freed
** before memory is allocated (use SECOID_DestroyAlgorithmID(dest, PR_FALSE)
** to do that).
*/
extern SECStatus SECOID_CopyAlgorithmID(PRArenaPool *arena, SECAlgorithmID *dest,
				    SECAlgorithmID *src);

/*
** Get the SEC_OID_* tag for the given algorithm-id object.
*/
extern SECOidTag SECOID_GetAlgorithmTag(SECAlgorithmID *aid);

/*
** Destroy an algorithm-id object.
**	"aid" the certificate-request to destroy
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void SECOID_DestroyAlgorithmID(SECAlgorithmID *aid, PRBool freeit);

/*
** Compare two algorithm-id objects, returning the difference between
** them.
*/
extern SECComparison SECOID_CompareAlgorithmID(SECAlgorithmID *a,
					   SECAlgorithmID *b);

extern PRBool SECOID_KnownCertExtenOID (SECItem *extenOid);

/* Given a SEC_OID_* tag, return a string describing it.
 */
extern const char *SECOID_FindOIDTagDescription(SECOidTag tagnum);

/*
 * free up the oid data structures.
 */
extern SECStatus SECOID_Shutdown(void);


SEC_END_PROTOS

#endif /* _SECOID_H_ */
