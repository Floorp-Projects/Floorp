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
 * CMS digestedData methods.
 *
 * $Id: cmsdigdata.c,v 1.4 2003/12/04 00:35:02 nelsonb%netscape.com Exp $
 */

#include "cmslocal.h"

#include "secitem.h"
#include "secasn1.h"
#include "secoid.h"
#include "secerr.h"

/*
 * NSS_CMSDigestedData_Create - create a digestedData object (presumably for encoding)
 *
 * version will be set by NSS_CMSDigestedData_Encode_BeforeStart
 * digestAlg is passed as parameter
 * contentInfo must be filled by the user
 * digest will be calculated while encoding
 */
NSSCMSDigestedData *
NSS_CMSDigestedData_Create(NSSCMSMessage *cmsg, SECAlgorithmID *digestalg)
{
    void *mark;
    NSSCMSDigestedData *digd;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    digd = (NSSCMSDigestedData *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSDigestedData));
    if (digd == NULL)
	goto loser;

    digd->cmsg = cmsg;

    if (SECOID_CopyAlgorithmID (poolp, &(digd->digestAlg), digestalg) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return digd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * NSS_CMSDigestedData_Destroy - destroy a digestedData object
 */
void
NSS_CMSDigestedData_Destroy(NSSCMSDigestedData *digd)
{
    /* everything's in a pool, so don't worry about the storage */
    NSS_CMSContentInfo_Destroy(&(digd->contentInfo));
    return;
}

/*
 * NSS_CMSDigestedData_GetContentInfo - return pointer to digestedData object's contentInfo
 */
NSSCMSContentInfo *
NSS_CMSDigestedData_GetContentInfo(NSSCMSDigestedData *digd)
{
    return &(digd->contentInfo);
}

/*
 * NSS_CMSDigestedData_Encode_BeforeStart - do all the necessary things to a DigestedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the right version number. The contentInfo's content type must be set up already.
 */
SECStatus
NSS_CMSDigestedData_Encode_BeforeStart(NSSCMSDigestedData *digd)
{
    unsigned long version;
    SECItem *dummy;

    version = NSS_CMS_DIGESTED_DATA_VERSION_DATA;
    if (NSS_CMSContentInfo_GetContentTypeTag(&(digd->contentInfo)) != SEC_OID_PKCS7_DATA)
	version = NSS_CMS_DIGESTED_DATA_VERSION_ENCAP;

    dummy = SEC_ASN1EncodeInteger(digd->cmsg->poolp, &(digd->version), version);
    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * NSS_CMSDigestedData_Encode_BeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
SECStatus
NSS_CMSDigestedData_Encode_BeforeData(NSSCMSDigestedData *digd)
{
    /* set up the digests */
    if (digd->digestAlg.algorithm.len != 0 && digd->digest.len == 0) {
	/* if digest is already there, do nothing */
	digd->contentInfo.digcx = NSS_CMSDigestContext_StartSingle(&(digd->digestAlg));
	if (digd->contentInfo.digcx == NULL)
	    return SECFailure;
    }
    return SECSuccess;
}

/*
 * NSS_CMSDigestedData_Encode_AfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
SECStatus
NSS_CMSDigestedData_Encode_AfterData(NSSCMSDigestedData *digd)
{
    SECStatus rv = SECSuccess;
    /* did we have digest calculation going on? */
    if (digd->contentInfo.digcx) {
	rv = NSS_CMSDigestContext_FinishSingle(digd->contentInfo.digcx,
				               digd->cmsg->poolp, 
					       &(digd->digest));
	/* error has been set by NSS_CMSDigestContext_FinishSingle */
	digd->contentInfo.digcx = NULL;
    }

    return rv;
}

/*
 * NSS_CMSDigestedData_Decode_BeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
SECStatus
NSS_CMSDigestedData_Decode_BeforeData(NSSCMSDigestedData *digd)
{
    /* is there a digest algorithm yet? */
    if (digd->digestAlg.algorithm.len == 0)
	return SECFailure;

    digd->contentInfo.digcx = NSS_CMSDigestContext_StartSingle(&(digd->digestAlg));
    if (digd->contentInfo.digcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * NSS_CMSDigestedData_Decode_AfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
SECStatus
NSS_CMSDigestedData_Decode_AfterData(NSSCMSDigestedData *digd)
{
    SECStatus rv = SECSuccess;
    /* did we have digest calculation going on? */
    if (digd->contentInfo.digcx) {
	rv = NSS_CMSDigestContext_FinishSingle(digd->contentInfo.digcx,
				               digd->cmsg->poolp, 
					       &(digd->cdigest));
	/* error has been set by NSS_CMSDigestContext_FinishSingle */
	digd->contentInfo.digcx = NULL;
    }

    return rv;
}

/*
 * NSS_CMSDigestedData_Decode_AfterEnd - finalize a digestedData.
 *
 * In detail:
 *  - check the digests for equality
 */
SECStatus
NSS_CMSDigestedData_Decode_AfterEnd(NSSCMSDigestedData *digd)
{
    /* did we have digest calculation going on? */
    if (digd->cdigest.len != 0) {
	/* XXX comparision btw digest & cdigest */
	/* XXX set status */
	/* TODO!!!! */
    }

    return SECSuccess;
}
