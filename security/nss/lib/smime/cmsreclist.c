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

/*
 * CMS recipient list functions
 *
 * $Id: cmsreclist.c,v 1.4 2005/09/16 17:54:31 wtchang%redhat.com Exp $
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
		/* alloc one & fill it out */
		rle = (NSSCMSRecipient *)PORT_ZAlloc(sizeof(NSSCMSRecipient));
		if (rle == NULL)
		    return -1;
		
		rle->riIndex = i;
		rle->subIndex = -1;
		switch (ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType) {
		case NSSCMSRecipientID_IssuerSN:
		    rle->kind = RLIssuerSN;
		    rle->id.issuerAndSN = ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN;
		    break;
		case NSSCMSRecipientID_SubjectKeyID:
		    rle->kind = RLSubjKeyID;
		    rle->id.subjectKeyID = ri->ri.keyTransRecipientInfo.recipientIdentifier.id.subjectKeyID;
		    break;
		default:
		    PORT_SetError(SEC_ERROR_INVALID_ARGS);
		    return -1;
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
		    if (rle == NULL)
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
