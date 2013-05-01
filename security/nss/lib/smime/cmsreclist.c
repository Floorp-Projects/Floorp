/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS recipient list functions
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"

static int
nss_cms_recipients_traverse(NSSCMSRecipientInfo **recipientinfos, NSSCMSRecipient **recipient_list)
{
    int count = 0;
    int rlindex = 0;
    int i, j;
    NSSCMSRecipient *rle;
    NSSCMSRecipientInfo *ri;
    NSSCMSRecipientEncryptedKey *rek;

    for (i = 0; recipientinfos[i] != NULL; i++) {
	ri = recipientinfos[i];
	switch (ri->recipientInfoType) {
	case NSSCMSRecipientInfoID_KeyTrans:
	    if (recipient_list) {
		NSSCMSRecipientIdentifier *recipId =
		   &ri->ri.keyTransRecipientInfo.recipientIdentifier;

		if (recipId->identifierType != NSSCMSRecipientID_IssuerSN &&
                    recipId->identifierType != NSSCMSRecipientID_SubjectKeyID) {
		    PORT_SetError(SEC_ERROR_INVALID_ARGS);
		    return -1;
		}                
		/* alloc one & fill it out */
		rle = (NSSCMSRecipient *)PORT_ZAlloc(sizeof(NSSCMSRecipient));
		if (!rle)
		    return -1;
		
		rle->riIndex = i;
		rle->subIndex = -1;
		switch (recipId->identifierType) {
		case NSSCMSRecipientID_IssuerSN:
		    rle->kind = RLIssuerSN;
		    rle->id.issuerAndSN = recipId->id.issuerAndSN;
		    break;
		case NSSCMSRecipientID_SubjectKeyID:
		    rle->kind = RLSubjKeyID;
		    rle->id.subjectKeyID = recipId->id.subjectKeyID;
		    break;
		default: /* we never get here because of identifierType check
                            we done before. Leaving it to kill compiler warning */
		    break;
		}
		recipient_list[rlindex++] = rle;
	    } else {
		count++;
	    }
	    break;
	case NSSCMSRecipientInfoID_KeyAgree:
	    if (ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys == NULL)
		break;
	    for (j=0; ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[j] != NULL; j++) {
		if (recipient_list) {
		    rek = ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[j];
		    /* alloc one & fill it out */
		    rle = (NSSCMSRecipient *)PORT_ZAlloc(sizeof(NSSCMSRecipient));
		    if (!rle)
			return -1;
		    
		    rle->riIndex = i;
		    rle->subIndex = j;
		    switch (rek->recipientIdentifier.identifierType) {
		    case NSSCMSKeyAgreeRecipientID_IssuerSN:
			rle->kind = RLIssuerSN;
			rle->id.issuerAndSN = rek->recipientIdentifier.id.issuerAndSN;
			break;
		    case NSSCMSKeyAgreeRecipientID_RKeyID:
			rle->kind = RLSubjKeyID;
			rle->id.subjectKeyID = rek->recipientIdentifier.id.recipientKeyIdentifier.subjectKeyIdentifier;
			break;
		    }
		    recipient_list[rlindex++] = rle;
		} else {
		    count++;
		}
	    }
	    break;
	case NSSCMSRecipientInfoID_KEK:
	    /* KEK is not implemented */
	    break;
	}
    }
    /* if we have a recipient list, we return on success (-1, above, on failure) */
    /* otherwise, we return the count. */
    if (recipient_list) {
	recipient_list[rlindex] = NULL;
	return 0;
    } else {
	return count;
    }
}

NSSCMSRecipient **
nss_cms_recipient_list_create(NSSCMSRecipientInfo **recipientinfos)
{
    int count, rv;
    NSSCMSRecipient **recipient_list;

    /* count the number of recipient identifiers */
    count = nss_cms_recipients_traverse(recipientinfos, NULL);
    if (count <= 0) {
	/* no recipients? */
	PORT_SetError(SEC_ERROR_BAD_DATA);
#if 0
	PORT_SetErrorString("Cannot find recipient data in envelope.");
#endif
	return NULL;
    }

    /* allocate an array of pointers */
    recipient_list = (NSSCMSRecipient **)
		    PORT_ZAlloc((count + 1) * sizeof(NSSCMSRecipient *));
    if (recipient_list == NULL)
	return NULL;

    /* now fill in the recipient_list */
    rv = nss_cms_recipients_traverse(recipientinfos, recipient_list);
    if (rv < 0) {
	nss_cms_recipient_list_destroy(recipient_list);
	return NULL;
    }
    return recipient_list;
}

void
nss_cms_recipient_list_destroy(NSSCMSRecipient **recipient_list)
{
    int i;
    NSSCMSRecipient *recipient;

    for (i=0; recipient_list[i] != NULL; i++) {
	recipient = recipient_list[i];
	if (recipient->cert)
	    CERT_DestroyCertificate(recipient->cert);
	if (recipient->privkey)
	    SECKEY_DestroyPrivateKey(recipient->privkey);
	if (recipient->slot)
	    PK11_FreeSlot(recipient->slot);
	PORT_Free(recipient);
    }
    PORT_Free(recipient_list);
}

NSSCMSRecipientEncryptedKey *
NSS_CMSRecipientEncryptedKey_Create(PLArenaPool *poolp)
{
    return (NSSCMSRecipientEncryptedKey *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSRecipientEncryptedKey));
}
