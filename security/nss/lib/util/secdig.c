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
/* $Id: secdig.c,v 1.10 2010/08/18 05:56:55 emaldona%redhat.com Exp $ */
#include "secdig.h"

#include "secoid.h"
#include "secasn1.h" 
#include "secerr.h"

/*
 * XXX Want to have a SGN_DecodeDigestInfo, like:
 *	SGNDigestInfo *SGN_DecodeDigestInfo(SECItem *didata);
 * that creates a pool and allocates from it and decodes didata into
 * the newly allocated DigestInfo structure.  Then fix secvfy.c (it
 * will no longer need an arena itself) to call this and then call
 * DestroyDigestInfo when it is done, then can remove the old template
 * above and keep our new template static and "hidden".
 */

/*
 * XXX It might be nice to combine the following two functions (create
 * and encode).  I think that is all anybody ever wants to do anyway.
 */

SECItem *
SGN_EncodeDigestInfo(PRArenaPool *poolp, SECItem *dest, SGNDigestInfo *diginfo)
{
    return SEC_ASN1EncodeItem (poolp, dest, diginfo, sgn_DigestInfoTemplate);
}

SGNDigestInfo *
SGN_CreateDigestInfo(SECOidTag algorithm, unsigned char *sig, unsigned len)
{
    SGNDigestInfo *di;
    SECStatus rv;
    PRArenaPool *arena;
    SECItem *null_param;
    SECItem dummy_value;

    switch (algorithm) {
      case SEC_OID_MD2:
      case SEC_OID_MD5:
      case SEC_OID_SHA1:
      case SEC_OID_SHA224:
      case SEC_OID_SHA256:
      case SEC_OID_SHA384:
      case SEC_OID_SHA512:
	break;
      default:
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return NULL;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return NULL;
    }

    di = (SGNDigestInfo *) PORT_ArenaZAlloc(arena, sizeof(SGNDigestInfo));
    if (di == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }

    di->arena = arena;

    /*
     * PKCS #1 specifies that the AlgorithmID must have a NULL parameter
     * (as opposed to no parameter at all).
     */
    dummy_value.data = NULL;
    dummy_value.len = 0;
    null_param = SEC_ASN1EncodeItem(NULL, NULL, &dummy_value, SEC_NullTemplate);
    if (null_param == NULL) {
	goto loser;
    }

    rv = SECOID_SetAlgorithmID(arena, &di->digestAlgorithm, algorithm,
			       null_param);

    SECITEM_FreeItem(null_param, PR_TRUE);

    if (rv != SECSuccess) {
	goto loser;
    }

    di->digest.data = (unsigned char *) PORT_ArenaAlloc(arena, len);
    if (di->digest.data == NULL) {
	goto loser;
    }

    di->digest.len = len;
    PORT_Memcpy(di->digest.data, sig, len);
    return di;

  loser:
    SGN_DestroyDigestInfo(di);
    return NULL;
}

SGNDigestInfo *
SGN_DecodeDigestInfo(SECItem *didata)
{
    PRArenaPool *arena;
    SGNDigestInfo *di;
    SECStatus rv = SECFailure;
    SECItem      diCopy   = {siBuffer, NULL, 0};

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(arena == NULL)
	return NULL;

    rv = SECITEM_CopyItem(arena, &diCopy, didata);
    if (rv != SECSuccess) {
	PORT_FreeArena(arena, PR_FALSE);
    	return NULL;
    }

    di = (SGNDigestInfo *)PORT_ArenaZAlloc(arena, sizeof(SGNDigestInfo));
    if (di != NULL) {
	di->arena = arena;
	rv = SEC_QuickDERDecodeItem(arena, di, sgn_DigestInfoTemplate, &diCopy);
    }
	
    if ((di == NULL) || (rv != SECSuccess)) {
	PORT_FreeArena(arena, PR_FALSE);
	di = NULL;
    }

    return di;
}

void
SGN_DestroyDigestInfo(SGNDigestInfo *di)
{
    if (di && di->arena) {
	PORT_FreeArena(di->arena, PR_FALSE);
    }

    return;
}

SECStatus 
SGN_CopyDigestInfo(PRArenaPool *poolp, SGNDigestInfo *a, SGNDigestInfo *b)
{
    SECStatus rv;
    void *mark;

    if((poolp == NULL) || (a == NULL) || (b == NULL))
	return SECFailure;

    mark = PORT_ArenaMark(poolp);
    a->arena = poolp;
    rv = SECOID_CopyAlgorithmID(poolp, &a->digestAlgorithm, 
	&b->digestAlgorithm);
    if (rv == SECSuccess)
	rv = SECITEM_CopyItem(poolp, &a->digest, &b->digest);

    if (rv != SECSuccess) {
	PORT_ArenaRelease(poolp, mark);
    } else {
	PORT_ArenaUnmark(poolp, mark);
    }

    return rv;
}

SECComparison
SGN_CompareDigestInfo(SGNDigestInfo *a, SGNDigestInfo *b)
{
    SECComparison rv;

    /* Check signature algorithm's */
    rv = SECOID_CompareAlgorithmID(&a->digestAlgorithm, &b->digestAlgorithm);
    if (rv) return rv;

    /* Compare signature block length's */
    rv = SECITEM_CompareItem(&a->digest, &b->digest);
    return rv;
}
