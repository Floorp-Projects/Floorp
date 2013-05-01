/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS message methods.
 */

#include "cmslocal.h"

#include "cert.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"

/*
 * NSS_CMSMessage_Create - create a CMS message object
 *
 * "poolp" - arena to allocate memory from, or NULL if new arena should be created
 */
NSSCMSMessage *
NSS_CMSMessage_Create(PLArenaPool *poolp)
{
    void *mark = NULL;
    NSSCMSMessage *cmsg;
    PRBool poolp_is_ours = PR_FALSE;

    if (poolp == NULL) {
	poolp = PORT_NewArena (1024);           /* XXX what is right value? */
	if (poolp == NULL)
	    return NULL;
	poolp_is_ours = PR_TRUE;
    } 

    if (!poolp_is_ours)
	mark = PORT_ArenaMark(poolp);

    cmsg = (NSSCMSMessage *)PORT_ArenaZAlloc (poolp, sizeof(NSSCMSMessage));
    if (cmsg == NULL) {
	if (!poolp_is_ours) {
	    if (mark) {
		PORT_ArenaRelease(poolp, mark);
	    }
	} else
	    PORT_FreeArena(poolp, PR_FALSE);
	return NULL;
    }
    NSS_CMSContentInfo_Private_Init(&(cmsg->contentInfo));

    cmsg->poolp = poolp;
    cmsg->poolp_is_ours = poolp_is_ours;
    cmsg->refCount = 1;

    if (mark)
	PORT_ArenaUnmark(poolp, mark);

    return cmsg;
}

/*
 * NSS_CMSMessage_SetEncodingParams - set up a CMS message object for encoding or decoding
 *
 * "cmsg" - message object
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 * "detached_digestalgs", "detached_digests" - digests from detached content
 */
void
NSS_CMSMessage_SetEncodingParams(NSSCMSMessage *cmsg,
			PK11PasswordFunc pwfn, void *pwfn_arg,
			NSSCMSGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
			SECAlgorithmID **detached_digestalgs, SECItem **detached_digests)
{
    if (pwfn)
	PK11_SetPasswordFunc(pwfn);
    cmsg->pwfn_arg = pwfn_arg;
    cmsg->decrypt_key_cb = decrypt_key_cb;
    cmsg->decrypt_key_cb_arg = decrypt_key_cb_arg;
    cmsg->detached_digestalgs = detached_digestalgs;
    cmsg->detached_digests = detached_digests;
}

/*
 * NSS_CMSMessage_Destroy - destroy a CMS message and all of its sub-pieces.
 */
void
NSS_CMSMessage_Destroy(NSSCMSMessage *cmsg)
{
    PORT_Assert (cmsg->refCount > 0);
    if (cmsg->refCount <= 0)	/* oops */
	return;

    cmsg->refCount--;		/* thread safety? */
    if (cmsg->refCount > 0)
	return;

    NSS_CMSContentInfo_Destroy(&(cmsg->contentInfo));

    /* if poolp is not NULL, cmsg is the owner of its arena */
    if (cmsg->poolp_is_ours)
	PORT_FreeArena (cmsg->poolp, PR_FALSE);	/* XXX clear it? */
}

/*
 * NSS_CMSMessage_Copy - return a copy of the given message. 
 *
 * The copy may be virtual or may be real -- either way, the result needs
 * to be passed to NSS_CMSMessage_Destroy later (as does the original).
 */
NSSCMSMessage *
NSS_CMSMessage_Copy(NSSCMSMessage *cmsg)
{
    if (cmsg == NULL)
	return NULL;

    PORT_Assert (cmsg->refCount > 0);

    cmsg->refCount++; /* XXX chrisk thread safety? */
    return cmsg;
}

/*
 * NSS_CMSMessage_GetArena - return a pointer to the message's arena pool
 */
PLArenaPool *
NSS_CMSMessage_GetArena(NSSCMSMessage *cmsg)
{
    return cmsg->poolp;
}

/*
 * NSS_CMSMessage_GetContentInfo - return a pointer to the top level contentInfo
 */
NSSCMSContentInfo *
NSS_CMSMessage_GetContentInfo(NSSCMSMessage *cmsg)
{
    return &(cmsg->contentInfo);
}

/*
 * Return a pointer to the actual content. 
 * In the case of those types which are encrypted, this returns the *plain* content.
 * In case of nested contentInfos, this descends and retrieves the innermost content.
 */
SECItem *
NSS_CMSMessage_GetContent(NSSCMSMessage *cmsg)
{
    /* this is a shortcut */
    NSSCMSContentInfo * cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    SECItem           * pItem = NSS_CMSContentInfo_GetInnerContent(cinfo);
    return pItem;
}

/*
 * NSS_CMSMessage_ContentLevelCount - count number of levels of CMS content objects in this message
 *
 * CMS data content objects do not count.
 */
int
NSS_CMSMessage_ContentLevelCount(NSSCMSMessage *cmsg)
{
    int count = 0;
    NSSCMSContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; ) {
	count++;
	cinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo);
    }
    return count;
}

/*
 * NSS_CMSMessage_ContentLevel - find content level #n
 *
 * CMS data content objects do not count.
 */
NSSCMSContentInfo *
NSS_CMSMessage_ContentLevel(NSSCMSMessage *cmsg, int n)
{
    int count = 0;
    NSSCMSContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL && count < n; cinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo)) {
	count++;
    }

    return cinfo;
}

/*
 * NSS_CMSMessage_ContainsCertsOrCrls - see if message contains certs along the way
 */
PRBool
NSS_CMSMessage_ContainsCertsOrCrls(NSSCMSMessage *cmsg)
{
    NSSCMSContentInfo *cinfo;

    /* descend into CMS message */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo)) {
	if (!NSS_CMSType_IsData(NSS_CMSContentInfo_GetContentTypeTag(cinfo)))
	    continue;	/* next level */
	
	if (NSS_CMSSignedData_ContainsCertsOrCrls(cinfo->content.signedData))
	    return PR_TRUE;
	/* callback here for generic wrappers? */
    }
    return PR_FALSE;
}

/*
 * NSS_CMSMessage_IsEncrypted - see if message contains a encrypted submessage
 */
PRBool
NSS_CMSMessage_IsEncrypted(NSSCMSMessage *cmsg)
{
    NSSCMSContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo))
    {
	switch (NSS_CMSContentInfo_GetContentTypeTag(cinfo)) {
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    return PR_TRUE;
	default:
	    /* callback here for generic wrappers? */
	    break;
	}
    }
    return PR_FALSE;
}

/*
 * NSS_CMSMessage_IsSigned - see if message contains a signed submessage
 *
 * If the CMS message has a SignedData with a signature (not just a SignedData)
 * return true; false otherwise.  This can/should be called before calling
 * VerifySignature, which will always indicate failure if no signature is
 * present, but that does not mean there even was a signature!
 * Note that the content itself can be empty (detached content was sent
 * another way); it is the presence of the signature that matters.
 */
PRBool
NSS_CMSMessage_IsSigned(NSSCMSMessage *cmsg)
{
    NSSCMSContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo))
    {
	switch (NSS_CMSContentInfo_GetContentTypeTag(cinfo)) {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    if (!NSS_CMSArray_IsEmpty((void **)cinfo->content.signedData->signerInfos))
		return PR_TRUE;
	    break;
	default:
	    /* callback here for generic wrappers? */
	    break;
	}
    }
    return PR_FALSE;
}

/*
 * NSS_CMSMessage_IsContentEmpty - see if content is empty
 *
 * returns PR_TRUE is innermost content length is < minLen
 * XXX need the encrypted content length (why?)
 */
PRBool
NSS_CMSMessage_IsContentEmpty(NSSCMSMessage *cmsg, unsigned int minLen)
{
    SECItem *item = NULL;

    if (cmsg == NULL)
	return PR_TRUE;

    item = NSS_CMSContentInfo_GetContent(NSS_CMSMessage_GetContentInfo(cmsg));

    if (!item) {
	return PR_TRUE;
    } else if(item->len <= minLen) {
	return PR_TRUE;
    }

    return PR_FALSE;
}
