/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS digesting.
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
static int global_num_digests = 0;
#endif

struct digestPairStr { 
    const SECHashObject * digobj;
    void *                digcx;
};
typedef struct digestPairStr digestPair;

struct NSSCMSDigestContextStr {
    PRBool		saw_contents;
    PLArenaPool *       pool;
    int			digcnt;
    digestPair  *       digPairs;
};


/*
 * NSS_CMSDigestContext_StartMultiple - start digest calculation using all the
 *  digest algorithms in "digestalgs" in parallel.
 */
NSSCMSDigestContext *
NSS_CMSDigestContext_StartMultiple(SECAlgorithmID **digestalgs)
{
    PLArenaPool *        pool;
    NSSCMSDigestContext *cmsdigcx;
    int digcnt;
    int i;

#ifdef CMS_FIND_LEAK_MULTIPLE
    PORT_Assert(global_num_digests == 0 || !stop_on_err);
#endif

    digcnt = (digestalgs == NULL) ? 0 : NSS_CMSArray_Count((void **)digestalgs);
    /* It's OK if digcnt is zero.  We have to allow this for "certs only"
    ** messages.
    */
    pool = PORT_NewArena(2048);
    if (!pool)
    	return NULL;

    cmsdigcx = PORT_ArenaNew(pool, NSSCMSDigestContext);
    if (cmsdigcx == NULL)
	goto loser;

    cmsdigcx->saw_contents = PR_FALSE;
    cmsdigcx->pool   = pool;
    cmsdigcx->digcnt = digcnt;

    cmsdigcx->digPairs = PORT_ArenaZNewArray(pool, digestPair, digcnt);
    if (cmsdigcx->digPairs == NULL) {
	goto loser;
    }

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
	    cmsdigcx->digPairs[i].digobj = digobj;
	    cmsdigcx->digPairs[i].digcx  = digcx;
#ifdef CMS_FIND_LEAK_MULTIPLE
	    global_num_digests++;
#endif
	}
    }
    return cmsdigcx;

loser:
    /* no digest objects have been created, or need to be destroyed. */
    if (pool) {
    	PORT_FreeArena(pool, PR_FALSE);
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
    digestPair *pair = cmsdigcx->digPairs;

    cmsdigcx->saw_contents = PR_TRUE;

    for (i = 0; i < cmsdigcx->digcnt; i++, pair++) {
	if (pair->digcx) {
	    (*pair->digobj->update)(pair->digcx, data, len);
    	}
    }
}

/*
 * NSS_CMSDigestContext_Cancel - cancel digesting operation
 */
void
NSS_CMSDigestContext_Cancel(NSSCMSDigestContext *cmsdigcx)
{
    int i;
    digestPair *pair = cmsdigcx->digPairs;

    for (i = 0; i < cmsdigcx->digcnt; i++, pair++) {
	if (pair->digcx) {
	    (*pair->digobj->destroy)(pair->digcx, PR_TRUE);
#ifdef CMS_FIND_LEAK_MULTIPLE
	    --global_num_digests;
#endif
    	}
    }
#ifdef CMS_FIND_LEAK_MULTIPLE
    PORT_Assert(global_num_digests == 0 || !stop_on_err);
#endif
    PORT_FreeArena(cmsdigcx->pool, PR_FALSE);
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
    SECItem **  digests = NULL;
    digestPair *pair;
    void *      mark;
    int         i;
    SECStatus   rv;

    /* no contents? do not finish digests */
    if (digestsp == NULL || !cmsdigcx->saw_contents) {
	rv = SECSuccess;
	goto cleanup;
    }

    mark = PORT_ArenaMark (poolp);

    /* allocate digest array & SECItems on arena */
    digests = PORT_ArenaNewArray( poolp, SECItem *, cmsdigcx->digcnt + 1);

    rv = ((digests == NULL) ? SECFailure : SECSuccess);
    pair = cmsdigcx->digPairs;
    for (i = 0; rv == SECSuccess && i < cmsdigcx->digcnt; i++, pair++) {
	SECItem digest;
	unsigned char hash[HASH_LENGTH_MAX];

	if (!pair->digcx) {
	    digests[i] = NULL;
	    continue;
	}

	digest.type = siBuffer;
	digest.data = hash;
	digest.len  = pair->digobj->length;
	(* pair->digobj->end)(pair->digcx, hash, &digest.len, digest.len);
	digests[i] = SECITEM_ArenaDupItem(poolp, &digest);
	if (!digests[i]) {
	    rv = SECFailure;
	}
    }
    digests[i] = NULL;
    if (rv == SECSuccess) {
	PORT_ArenaUnmark(poolp, mark);
    } else
	PORT_ArenaRelease(poolp, mark);

cleanup:
    NSS_CMSDigestContext_Cancel(cmsdigcx);
    /* Don't change the caller's digests pointer if we have no digests.
    **  NSS_CMSSignedData_Encode_AfterData depends on this behavior.
    */
    if (rv == SECSuccess && digestsp && digests) {
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
