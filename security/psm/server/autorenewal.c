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
#include "ctrlconn.h"
#include "cert.h"
#include "secasn1.h"
#include "secder.h"
#include "xconst.h"
#include "secerr.h"
#include "newproto.h"
#include "messages.h"
#include "certres.h"
#include "pk11func.h"
#include "secoid.h"

typedef struct CERTRenewalWindow {
	SECItem begin;
	SECItem end;
} CERTRenewalWindow;

const static SEC_ASN1Template CERT_RenewalWindowTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
		0, NULL, sizeof(CERTRenewalWindow) },
	{ SEC_ASN1_GENERALIZED_TIME,
	offsetof(CERTRenewalWindow,begin) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_GENERALIZED_TIME,
	offsetof(CERTRenewalWindow,end) },
	{ 0 }
};

PRBool CERT_HasRenewalExtension(CERTCertificate *cert)
{
	int rv;
	SECItem *derWindow = NULL;
	PRBool ret = PR_FALSE;

	/* Allocate from the heap, so that we can free eveything later */
	derWindow = SECITEM_AllocItem(NULL, NULL, 0);
	if (derWindow == NULL) {
		goto loser;
	}

	/* Get the renewal window extension */
	rv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME,
		derWindow);
	if (rv == SECSuccess) {
		ret = PR_TRUE;
	}

loser:
	if (derWindow) {
		SECITEM_FreeItem(derWindow, PR_TRUE);
	}
	return ret;
}

PRBool CERT_InRenewalWindow(CERTCertificate *cert, int64 t)
{
	int rv;
	int64 begin, end;
	CERTRenewalWindow *window = NULL;
	SECItem *derWindow = NULL;
	PRArenaPool *arena = NULL;
	PRBool ret = PR_FALSE;

	/* Allocate from the heap, so that we can free eveything later */
	derWindow = SECITEM_AllocItem(NULL, NULL, 0);
	if (derWindow == NULL) {
		goto loser;
	}

	/* Get the renewal window extension */
	rv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME,
		derWindow);
	if (rv == SECFailure) {
		goto loser;
	}

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (arena == NULL)
		goto loser;

	window = (CERTRenewalWindow*)PORT_ArenaZAlloc(arena,
								sizeof(CERTRenewalWindow));
	if (window == NULL) {
		goto loser;
	}

	/* Decode the extension */
	rv = SEC_ASN1DecodeItem(arena, window, CERT_RenewalWindowTemplate, derWindow);
	if (rv) {
		goto loser;
	}

	/* Decode the times */
	rv = DER_GeneralizedTimeToTime(&begin, &window->begin);
	if (window->end.data) {
		rv = DER_GeneralizedTimeToTime(&end, &window->end);
	} else {
		rv = DER_UTCTimeToTime(&end, &cert->validity.notAfter);
	}

	/* Is this cert in the renewal window */
	if (LL_CMP(t,>,begin) && LL_CMP(t,<,end)) {
		ret = PR_TRUE;
	} else {
		ret = PR_FALSE;
	}
loser:
	if (arena) {
		PORT_FreeArena(arena, PR_FALSE);
	}
	if (derWindow) {
		SECITEM_FreeItem(derWindow, PR_TRUE);
	}
	return ret;
}

PRBool CERT_RenewalWindowExpired(CERTCertificate *cert, int64 t)
{
	int rv;
	int64 begin, end;
	CERTRenewalWindow *window = NULL;
	SECItem *derWindow = NULL;
	PRArenaPool *arena = NULL;
	PRBool ret = PR_FALSE;

	/* Allocate from the heap, so that we can free eveything later */
	derWindow = SECITEM_AllocItem(NULL, NULL, 0);
	if (derWindow == NULL) {
		goto loser;
	}

	/* Get the renewal window extension */
	rv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME,
		derWindow);
	if (rv == SECFailure) {
		goto loser;
	}

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (arena == NULL)
		goto loser;

	window = (CERTRenewalWindow*)PORT_ArenaZAlloc(arena,
								sizeof(CERTRenewalWindow));
	if (window == NULL) {
		goto loser;
	}

	/* Decode the extension */
	rv = SEC_ASN1DecodeItem(arena, window, CERT_RenewalWindowTemplate, derWindow);
	if (rv) {
		goto loser;
	}

	/* Decode the times */
	rv = DER_GeneralizedTimeToTime(&begin, &window->begin);
	if (window->end.data) {
		rv = DER_GeneralizedTimeToTime(&end, &window->end);
	} else {
		rv = DER_UTCTimeToTime(&end, &cert->validity.notAfter);
	}

	/* Has the renewal window expired */
	if (LL_CMP(t,>,end)) {
		ret = PR_TRUE;
	} else {
		ret = PR_FALSE;
	}
loser:
	if (arena) {
		PORT_FreeArena(arena, PR_FALSE);
	}
	if (derWindow) {
		SECITEM_FreeItem(derWindow, PR_TRUE);
	}
	return ret;
}

char * CERT_GetAuthorityInfoAccessLocation(CERTCertificate *cert, int method)
{
    CERTGeneralName *locname = NULL;
    SECItem *location = NULL;
    SECItem *encodedAuthInfoAccess = NULL;
    CERTAuthInfoAccess **authInfoAccess = NULL;
    char *locURI = NULL;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    int i;

    /*
     * Allocate this one from the heap because it will get filled in
     * by CERT_FindCertExtension which will also allocate from the heap,
     * and we can free the entire thing on our way out.
     */
    encodedAuthInfoAccess = SECITEM_AllocItem(NULL, NULL, 0);
    if (encodedAuthInfoAccess == NULL)
	goto loser;

    rv = CERT_FindCertExtension(cert, SEC_OID_X509_AUTH_INFO_ACCESS,
				encodedAuthInfoAccess);
    if (rv == SECFailure)
	goto loser;

    /*
     * The rest of the things allocated in the routine will come out of
     * this arena, which is temporary just for us to decode and get at the
     * AIA extension.  The whole thing will be destroyed on our way out,
     * after we have copied the location string (url) itself (if found).
     */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	goto loser;

    authInfoAccess = cert_DecodeAuthInfoAccessExtension(arena,
							encodedAuthInfoAccess);
    if (authInfoAccess == NULL)
	goto loser;

    for (i = 0; authInfoAccess[i] != NULL; i++) {
	if (SECOID_FindOIDTag(&authInfoAccess[i]->method) == method)
	    locname = authInfoAccess[i]->location;
    }

    /*
     * If we found an AIA extension, but it did not include an OCSP method,
     * that should look to our caller as if we did not find the extension
     * at all, because it is only an OCSP method that we care about.
     * So set the same error that would be set if the AIA extension was
     * not there at all.
     */
    if (locname == NULL) {
	PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
	goto loser;
    }

    /*
     * The following is just a pointer back into locname (i.e. not a copy);
     * thus it should not be freed.
     */
    location = CERT_GetGeneralNameByType(locname, certURI, PR_FALSE);
    if (location == NULL) {
	/*
	 * XXX Appears that CERT_GetGeneralNameByType does not set an
	 * error if there is no name by that type.  For lack of anything
	 * better, act as if the extension was not found.  In the future
	 * this should probably be something more like the extension was
	 * badly formed.
	 */
	PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
	goto loser;
    }

    /*
     * That location is really a string, but it has a specified length
     * without a null-terminator.  We need a real string that does have
     * a null-terminator, and we need a copy of it anyway to return to
     * our caller -- so allocate and copy.
     */
    locURI = PORT_Alloc(location->len + 1);
    if (locURI == NULL) {
	goto loser;
    }
    PORT_Memcpy(locURI, location->data, location->len);
    locURI[location->len] = '\0';

loser:
    if (arena != NULL)
	PORT_FreeArena(arena, PR_FALSE);

    if (encodedAuthInfoAccess != NULL)
	SECITEM_FreeItem(encodedAuthInfoAccess, PR_TRUE);

    return locURI;
}

SSMStatus 
SendRenewalUIEvent(SSMControlConnection *conn,
                                 char *url)
{
    SECItem event;
    SSMStatus rv = PR_SUCCESS;
    SSMResourceID rid = 0;
    UIEvent reply;

    if (!conn->m_doesUI) {
        return SSM_FAILURE;
    }

    /* Generate the actual message to send to the client. */
    reply.resourceID = rid;
    reply.width = 715;
    reply.height = 545;
	reply.isModal = 3;
    reply.url = url;
    reply.clientContext.len = 0;
    reply.clientContext.data = NULL;

    if (CMT_EncodeMessage(UIEventTemplate, (CMTItem*)&event, &reply) != CMTSuccess) {
        goto loser;
    }

    /* Post the message on the outgoing control channel. */
    rv = SSM_SendQMessage(conn->m_controlOutQ, SSM_PRIORITY_NORMAL, 
                          SSM_EVENT_MESSAGE | SSM_UI_EVENT,
                          (int) event.len, (char *) event.data, PR_FALSE);
    if (rv != PR_SUCCESS) goto loser;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    PR_FREEIF(event.data);
    return rv;
}

SECStatus CheckCertificate(CERTCertificate * cert, void * arg)
{
	char *url = NULL;
	SSMControlConnection *ctrl = (SSMControlConnection *)arg;
    SSMStatus rv;
    SSMResourceID certID;
    SSMResourceCert * certRes = NULL;
	CERTCertificate *otherCert = NULL;
	CERTCertList *certs = NULL;
	CERTCertListNode *node;
	
	/* Is this cert in the renewal window */
	if (CERT_InRenewalWindow(cert, PR_Now()) == PR_FALSE) {
		goto done;
	}

	/* Get the AIA */
	url = CERT_GetAuthorityInfoAccessLocation(cert, SEC_OID_CERT_RENEWAL_LOCATOR);
	if (!url) {
		goto done;
	}

	/* Should we renew this cert? Check the case where it has already been renewed */
	/* but the old cert is still there waith a valid renewal window */
	certs = PK11_FindCertsFromNickname(cert->nickname, ctrl);
	if (!cert) {
		goto done;
	}

	/* Interate through the certs */
	node = CERT_LIST_HEAD(certs);

	while (!CERT_LIST_END(node, certs)) {
		otherCert = node->cert;

		/* This is the cert we are renewing */
		if (otherCert == cert) {
			goto endloop;
		}

		/* This cert has expired */
		if (CERT_CheckCertValidTimes(otherCert, PR_Now(), PR_FALSE) != secCertTimeValid) {
			goto endloop;
		}

		/* This cert has different key usage */
		if (cert->keyUsage != otherCert->keyUsage) {
			goto endloop;
		}

		/* This cert renewal window has not expired - don't renew our cert */
		if (CERT_HasRenewalExtension(otherCert) && !CERT_RenewalWindowExpired(otherCert, PR_Now())) {
			goto done;
		}
endloop:
		node = CERT_LIST_NEXT(node);
	}

	/* Create a resource for this cert and get an id */
	rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                            cert,
                            ctrl,
                            &certID,
                            (SSMResource**)&certRes);
	if (rv != PR_SUCCESS) {
            goto done;
    }

	SSM_LockUIEvent((SSMResource*)certRes);

	/* Send a UI event to client */
	rv = SSMControlConnection_SendUIEvent(ctrl, "get", 
				     "cert_renewal", 
				     &(certRes->super), NULL,
				     &(&(ctrl->super.super))->m_clientContext, 
				     PR_TRUE);
	if (rv != SSM_SUCCESS) {
		goto loser;
    }

	SSM_WaitUIEvent((SSMResource*)certRes, PR_INTERVAL_NO_TIMEOUT);

	if ((certRes->super.m_buttonType == SSM_BUTTON_OK) && (certRes->m_renewCert == PR_TRUE)) {
		/* Send a UI event asking about renewal */
		/* Send UI prompting the user */
		/* SendRenewalUIEvent(ctrl, url); */
	}
loser:
done:
	if (certs) {
		CERT_DestroyCertList(certs);
	}
	if (certRes) {
		SSM_FreeResource(certRes);
	}
	PR_FREEIF(url);
	return SECSuccess;
}

void SSM_CertificateRenewalThread(void * arg)
{
	int series, slotSeries[255], i = 0;
	PK11SlotList * slots = NULL;
	PK11SlotListElement * le = NULL;
	SSMControlConnection *ctrl = (SSMControlConnection *)arg;

	/* Initialize slot series array to zero */
	memset(slotSeries, 0, sizeof(slotSeries));

	/* Get a list of tokens installed */
	slots = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, ctrl);
	if (!slots) {
		return;
	}

	while (1) {
		/* Interate over the list of slots */
		for (le = slots->head, i= 0; le; le = le->next, i++) {

			/* If there is no token present then we are no interested */
			if (!PK11_IsPresent(le->slot)) {
				continue;
			}

			/* If token not logged in, then we are not interested */
			if (!PK11_IsLoggedIn(le->slot, ctrl)) {
				continue;
			}

			/* If token is read-only, then we are not interested */
			if (PK11_IsReadOnly(le->slot)) {
				continue;
			}

			/* Get the series and mark as checked */
			series = PK11_GetSlotSeries(le->slot);
			if (slotSeries[i] == series) {
				continue;
			}
			slotSeries[i] = series;

			/* Interate all the certs on the token */
			PK11_TraverseCertsInSlot(le->slot, CheckCertificate, ctrl);
		}

		/* Sleep for one minute */
		PR_Sleep(PR_TicksPerSecond()*60);
	}

	if (slots) {
		PK11_FreeSlotList(slots);
	}
}

SSMStatus SSM_RenewalCertInfoHandler(SSMTextGenContext* cx)
{
    SSMStatus rv;
    SSMResource* target = NULL;
    SSMResourceCert* renewalCertRes = NULL;
    CERTCertificate* renewalCert = NULL;
	char *url = NULL;
	char *fmt = NULL, *issuerCN = NULL;
	char *validNotBefore = NULL, *validNotAfter = NULL;

    PR_ASSERT(cx != NULL);
    PR_ASSERT(cx->m_request != NULL);
    PR_ASSERT(cx->m_params != NULL);
    PR_ASSERT(cx->m_result != NULL);

    /* retrieve the renewal cert */
    target = SSMTextGen_GetTargetObject(cx);
    renewalCertRes = (SSMResourceCert*)target;
    
    renewalCert = renewalCertRes->cert;
    if (renewalCert == NULL) {
		goto loser;
    }

	/* Get the info */
	/* This is: Name, issuer, valid from and valid to */
    issuerCN = CERT_GetCommonName(&renewalCert->issuer),
    validNotBefore = DER_UTCDayToAscii(&renewalCert->validity.notBefore);
    validNotAfter = DER_UTCDayToAscii(&renewalCert->validity.notAfter);
	url = CERT_GetAuthorityInfoAccessLocation(renewalCert, SEC_OID_CERT_RENEWAL_LOCATOR);

	rv = SSM_GetAndExpandTextKeyedByString(cx, "cert_renewal_cert_info", &fmt);
	if (rv != SSM_SUCCESS) {
		goto loser;
	}

	cx->m_result = PR_smprintf(fmt, renewalCert->nickname, validNotBefore, validNotAfter, issuerCN, url);

	PR_FREEIF(fmt);
	PR_FREEIF(issuerCN);
	PR_FREEIF(validNotBefore);
	PR_FREEIF(validNotAfter);
	PR_FREEIF(url);
	return SSM_SUCCESS;

loser:
	PR_FREEIF(fmt);
	PR_FREEIF(issuerCN);
	PR_FREEIF(validNotBefore);
	PR_FREEIF(validNotAfter);
	return SSM_FAILURE;
}

