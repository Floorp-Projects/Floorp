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

/*
 * CMS digesting.
 *
 * $Id: cmsdigest.c,v 1.4 2003/12/04 00:32:18 nelsonb%netscape.com Exp $
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"

/*  #define CMS_FIND_LEAK_MULTIPLE 1 */
#ifdef CMS_FIND_LEAK_MULTIPLE
static int stop_on_err = 1;
static int num_digests = 0;
#endif

struct NSSCMSDigestContextStr {
    PRBool		saw_contents;
    int			digcnt;
    void **		digcxs;
const SECHashObject **	digobjs;
};

/*
 * NSS_CMSDigestContext_StartMultiple - start digest calculation using all the
 *  digest algorithms in "digestalgs" in parallel.
 */
NSSCMSDigestContext *
NSS_CMSDigestContext_StartMultiple(SECAlgorithmID **digestalgs)
{
    NSSCMSDigestContext *cmsdigcx;
    int digcnt;
    int i;

#ifdef CMS_FIND_LEAK_MULTIPLE
    PORT_Assert(num_digests == 0 || !stop_on_err);
#endif

    digcnt = (digestalgs == NULL) ? 0 : NSS_CMSArray_Count((void **)digestalgs);

    cmsdigcx = PORT_New(NSSCMSDigestContext);
    if (cmsdigcx == NULL)
	return NULL;

    if (digcnt > 0) {
	cmsdigcx->digcxs  = PORT_NewArray(void *, digcnt);
	cmsdigcx->digobjs = PORT_NewArray(const SECHashObject *, digcnt);
	if (cmsdigcx->digcxs == NULL || cmsdigcx->digobjs == NULL)
	    goto loser;
    } else {
	cmsdigcx->digcxs  = NULL;
	cmsdigcx->digobjs = NULL;
    }

    cmsdigcx->digcnt = 0;

    /*
     * Create a digest object context for each algorithm.
     */
    for (i = 0; i < digcnt; i++) {
	const SECHashObject *digobj;
	void *digcx;

	digobj = NSS_CMSUtil_GetHashObjByAlgID(digestalgs[i]);
	/*
	 * Skip any algorithm we do not even recognize; obviously,
	 * this could be a problem, but if it is critical then the
	 * result will just be that the signature does not verify.
	 * We do not necessarily want to error out here, because
	 * the particular algorithm may not actually be important,
	 * but we cannot know that until later.
	 */
	if (digobj == NULL)
	    continue;

	digcx = (*digobj->create)();
	if (digcx != NULL) {
	    (*digobj->begin) (digcx);
	    cmsdigcx->digobjs[cmsdigcx->digcnt] = digobj;
	    cmsdigcx->digcxs[cmsdigcx->digcnt] = digcx;
	    cmsdigcx->digcnt++;
#ifdef CMS_FIND_LEAK_MULTIPLE
	    num_digests++;
#endif
	}
    }

    cmsdigcx->saw_contents = PR_FALSE;

    return cmsdigcx;

loser:
    if (cmsdigcx) {
	if (cmsdigcx->digobjs)
	    PORT_Free((void *)cmsdigcx->digobjs); /* cast away const */
	if (cmsdigcx->digcxs)
	    PORT_Free(cmsdigcx->digcxs);
	PORT_Free(cmsdigcx);
    }
    return NULL;
}

/*
 * NSS_CMSDigestContext_StartSingle - same as 
 * NSS_CMSDigestContext_StartMultiple, but only one algorithm.
 */
NSSCMSDigestContext *
NSS_CMSDigestContext_StartSingle(SECAlgorithmID *digestalg)
{
    SECAlgorithmID *digestalgs[] = { NULL, NULL };		/* fake array */

    digestalgs[0] = digestalg;
    return NSS_CMSDigestContext_StartMultiple(digestalgs);
}

/*
 * NSS_CMSDigestContext_Update - feed more data into the digest machine
 */
void
NSS_CMSDigestContext_Update(NSSCMSDigestContext *cmsdigcx, 
                            const unsigned char *data, int len)
{
    int i;

    cmsdigcx->saw_contents = PR_TRUE;

    for (i = 0; i < cmsdigcx->digcnt; i++)
	(*cmsdigcx->digobjs[i]->update)(cmsdigcx->digcxs[i], data, len);
}

/*
 * NSS_CMSDigestContext_Cancel - cancel digesting operation
 */
void
NSS_CMSDigestContext_Cancel(NSSCMSDigestContext *cmsdigcx)
{
    int i;

    for (i = 0; i < cmsdigcx->digcnt; i++) {
	(*cmsdigcx->digobjs[i]->destroy)(cmsdigcx->digcxs[i], PR_TRUE);
#ifdef CMS_FIND_LEAK_MULTIPLE
	--num_digests;
#endif
    }
#ifdef CMS_FIND_LEAK_MULTIPLE
    PORT_Assert(num_digests == 0 || !stop_on_err);
#endif
    if (cmsdigcx->digobjs) {
	PORT_Free((void *)cmsdigcx->digobjs); /* cast away const */
	cmsdigcx->digobjs = NULL;
    }
    if (cmsdigcx->digcxs) {
	PORT_Free(cmsdigcx->digcxs);
	cmsdigcx->digcxs = NULL;
    }
    PORT_Free(cmsdigcx);
}

/*
 * NSS_CMSDigestContext_FinishMultiple - finish the digests and put them
 *  into an array of SECItems (allocated on poolp)
 */
SECStatus
NSS_CMSDigestContext_FinishMultiple(NSSCMSDigestContext *cmsdigcx, 
                                    PLArenaPool *poolp,
			            SECItem ***digestsp)
{
    const SECHashObject *digobj;
    void *digcx;
    SECItem **digests = NULL, *digest;
    int i;
    void *mark;
    SECStatus rv = SECFailure;

    /* no contents? do not update digests */
    if (digestsp == NULL || !cmsdigcx->saw_contents) {
	for (i = 0; i < cmsdigcx->digcnt; i++) {
	    (*cmsdigcx->digobjs[i]->destroy)(cmsdigcx->digcxs[i], PR_TRUE);
#ifdef CMS_FIND_LEAK_MULTIPLE
	    --num_digests;
#endif
	}
	rv = SECSuccess;
	goto cleanup;
    }

    mark = PORT_ArenaMark (poolp);

    /* allocate digest array & SECItems on arena */
    digests = PORT_ArenaNewArray( poolp, SECItem *, cmsdigcx->digcnt + 1);
    digest  = PORT_ArenaZNewArray(poolp, SECItem,   cmsdigcx->digcnt    );
    if (digests == NULL || digest == NULL) {
	rv = SECFailure;
    } else {
	rv = SECSuccess;
    }

    for (i = 0; i < cmsdigcx->digcnt; i++, digest++) {
	digcx = cmsdigcx->digcxs[i];
	digobj = cmsdigcx->digobjs[i];

	if (rv != SECSuccess) {
	    /* skip it */
	} else {
	    digest->data = 
	        (unsigned char*)PORT_ArenaAlloc(poolp, digobj->length);
	    if (digest->data != NULL) {
		digest->len = digobj->length;
		(* digobj->end)(digcx, digest->data, &(digest->len), 
		                       digest->len);
		digests[i] = digest;
	    } else {
		rv = SECFailure;
	    }
	}
	(* digobj->destroy)(digcx, PR_TRUE);
#ifdef CMS_FIND_LEAK_MULTIPLE
	--num_digests;
#endif
    }
    if (rv == SECSuccess) {
	digests[i] = NULL;
	PORT_ArenaUnmark(poolp, mark);
    } else
	PORT_ArenaRelease(poolp, mark);

cleanup:
#ifdef CMS_FIND_LEAK_MULTIPLE
    PORT_Assert( num_digests == 0 || !stop_on_err);
#endif
    if (cmsdigcx->digobjs)
	PORT_Free((void *)cmsdigcx->digobjs); /* cast away const */
    if (cmsdigcx->digcxs)
	PORT_Free(cmsdigcx->digcxs);
    PORT_Free(cmsdigcx);
    if (rv == SECSuccess && digestsp) {
	*digestsp = digests;
    }
    return rv;
}

/*
 * NSS_CMSDigestContext_FinishSingle - same as 
 * NSS_CMSDigestContext_FinishMultiple, but for one digest.
 */
SECStatus
NSS_CMSDigestContext_FinishSingle(NSSCMSDigestContext *cmsdigcx, 
                                  PLArenaPool *poolp,
			          SECItem *digest)
{
    SECStatus rv = SECFailure;
    SECItem **dp;
    PLArenaPool *arena = NULL;

    if ((arena = PORT_NewArena(1024)) == NULL)
	goto loser;

    /* get the digests into arena, then copy the first digest into poolp */
    rv = NSS_CMSDigestContext_FinishMultiple(cmsdigcx, arena, &dp);
    if (rv == SECSuccess) {
	/* now copy it into poolp */
	rv = SECITEM_CopyItem(poolp, digest, dp[0]);
    }
loser:
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);

    return rv;
}
