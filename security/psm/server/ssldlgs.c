/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "sslconn.h"
#include "ssldlgs.h"
#include "ctrlconn.h"
#include "textgen.h"
#include "resource.h"
#include "certres.h"
#include "minihttp.h"

#include "cert.h"
#include "certt.h"
#include "certdb.h"
#include "sechash.h"
#include "nlsutil.h"
#include "nlslayer.h"


#define SSMRESOURCE(sslconn) (&(sslconn)->super.super.super)
#define SSMCONTROLCONNECTION(sslconn) ((SSMControlConnection*)(sslconn->super.super.m_parent))


/* private functions */
SSMStatus ssm_client_auth_get_utf8_cert_list(SSMSSLDataConnection* conn, 
                                             char* fmt, 
                                             char** result);
SSMStatus ssm_http_client_auth_handle_ok_button(HTTPRequest* req);
SSMStatus ssm_http_client_auth_handle_cancel_button(HTTPRequest* req);
SECStatus SSM_SSLServerCertResetTrust(CERTCertificate* cert, 
				      SSMBadServerCertAccept accept);
static char* ssm_default_server_nickname(CERTCertificate* cert);
SSMStatus ssm_http_server_auth_handle_ok_button(HTTPRequest* req);
SSMStatus ssm_http_server_auth_handle_cancel_button(HTTPRequest* req);
SSMStatus ssm_http_unknown_issuer_step1_handle_next_button(HTTPRequest* req);


SSMStatus SSM_FormatCert(CERTCertificate* cert, char* fmt, 
                         char** result)
{
    char* hostname = NULL;
    char* orgname = NULL;
    char* issuer = NULL;
    char * notBefore = NULL, * notAfter = NULL;
    SSMStatus rv = SSM_SUCCESS;
    

    /* check arguments */
    if (cert == NULL || fmt == NULL || result == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }
	
    /* retrieve cert information */
    hostname = CERT_GetCommonName(&cert->subject);
    if (hostname == NULL) {
        goto loser;
    }

    orgname = CERT_GetOrgName(&cert->subject);
    if (orgname == NULL) {
        goto loser;
    }

    issuer = CERT_GetOrgName(&cert->issuer);
    if (issuer == NULL)
    {
        goto loser;
    }
    /* ### sjlee: should issuer be a localizable string? */

    notBefore = DER_UTCDayToAscii(&cert->validity.notBefore);
    if (!notBefore) 
        notBefore = PL_strdup("No date found");
    notAfter = DER_UTCDayToAscii(&cert->validity.notAfter);
    if (!notAfter) 
        notAfter = PL_strdup("No date found");
    
    *result = PR_smprintf(fmt, hostname, notBefore, notAfter, issuer, orgname);
    SSM_DebugUTF8String("wrapped cert", *result);
    goto done;
 loser:
    SSM_DEBUG("Formatting cert failed.\n");
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
 done:
    if (hostname != NULL) {
        PR_Free(hostname);
    }
    if (orgname != NULL) {
        PR_Free(orgname);
    }
    if (issuer != NULL) {
        PR_Free(issuer);
    }
    
    PR_FREEIF(notBefore);
    PR_FREEIF(notAfter);
    return rv;
}


SSMStatus SSM_ServerCertKeywordHandler(SSMTextGenContext* cx)
{
    SSMResource* target = NULL;
    SSMSSLDataConnection* sslconn = NULL;
    SSMStatus rv;
    char* pattern = NULL;
    char* key = NULL;
    char* formatKey = NULL;
    char* simpleStr = NULL;
    char* prettyStr = NULL;
    CERTCertificate* serverCert = NULL;
    const PRIntn CERT_FORMAT = (PRIntn)0;
    const PRIntn CERT_WRAPPER = (PRIntn)1;
    const PRIntn CERT_WRAPPER_NO_COMMENT = (PRIntn)2;
	PRIntn wrapper;

    /* check arguments */
    /* ### sjlee: might as well make this a helper function because most
     *		  keyword handlers will use this checking
     */
    PR_ASSERT(cx != NULL);
    PR_ASSERT(cx->m_request != NULL);
    PR_ASSERT(cx->m_params != NULL);
    PR_ASSERT(cx->m_result != NULL);
    if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
	cx->m_result == NULL) {
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	goto loser;
    }
	
    simpleStr = "simple";
    prettyStr = "pretty";

    /* retrieve the server cert */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(SSM_IsA(target, SSM_RESTYPE_SSL_DATA_CONNECTION));
    sslconn = (SSMSSLDataConnection*)target;

    serverCert = SSL_PeerCertificate(sslconn->socketSSL);
    if (serverCert == NULL) {
	/* couldn't get the server cert: what do I do? */
	goto loser;
    }

    /* form the Format object */
    /* first, find the keys */
    formatKey = (char *) SSM_At(cx->m_params, CERT_FORMAT);

    /* only show comments field if there are comments */
    if (CERT_GetCertCommentString(serverCert))
        wrapper = CERT_WRAPPER;
    else 
        wrapper = CERT_WRAPPER_NO_COMMENT;
    key = (char *) SSM_At(cx->m_params, wrapper);
	
    /* second, grab and expand the keyword objects */
    rv = SSM_GetAndExpandTextKeyedByString(cx, key, &pattern);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("server cert info pattern", pattern);

    /* Now let's figure out if we're doing pretty print or simple print. */
    PR_FREEIF(cx->m_result);
    if (formatKey[0] ==  's') {
        rv = SSM_FormatCert(serverCert, pattern, &cx->m_result);
    } else if (formatKey[0] == 'p') {
        rv = SSM_PrettyFormatCert(serverCert, pattern, 
                                  &cx->m_result, PR_FALSE);
    } else {
        SSM_DEBUG("cannot understand the format key.\n");
        rv = SSM_FAILURE;
    }
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    if (serverCert != NULL) {
	CERT_DestroyCertificate(serverCert);
    }
    PR_FREEIF(pattern);
    return rv;
}

SSMStatus SSM_HTTPBadClientAuthButtonHandler(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    char* tmpStr = NULL;

    PR_ASSERT(req->target != NULL);
    conn = (SSMSSLDataConnection*)(req->target);

    /* make sure you got the right baseRef */
    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS || 
	PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
	goto loser;
    }

    return ssm_http_client_auth_handle_cancel_button(req);
loser:
    /* set the predicate to true and unblock the SSL thread */
    SSM_LockResource(req->target);
    conn->m_UIInfo.UICompleted = PR_TRUE;
    conn->m_UIInfo.chosen = -1;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
    return rv;
}


SSMStatus SSM_SSLMakeClientAuthDialog(SSMSSLDataConnection* conn)
{
    SSMStatus rv;

    /* enter the SSL monitor */
    SSM_LockResource(SSMRESOURCE(conn));
    conn->m_UIInfo.UICompleted = PR_FALSE;

    /* fire up the UI */
    rv = SSMControlConnection_SendUIEvent(SSMCONTROLCONNECTION(conn), "get", 
					  "client_auth", SSMRESOURCE(conn), 
					  NULL, &SSMRESOURCE(conn)->m_clientContext, PR_TRUE);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    while (!conn->m_UIInfo.UICompleted) {
	SSM_WaitResource(SSMRESOURCE(conn), PR_INTERVAL_NO_TIMEOUT);
    }

    if (conn->m_UIInfo.chosen < 0) {
	/* "cancel" button clicked: bring up the no cert dialog? */
	if (rv == SSM_SUCCESS) {
	    rv = SSM_ERR_USER_CANCEL;
	}
    }
    /* the user selected a nickname; let's exit in orderly fashion */

loser:
    /* reset this bit to facilitate future UI events */
    conn->m_UIInfo.UICompleted = PR_FALSE;
    SSM_UnlockResource(SSMRESOURCE(conn));

    return rv;
}


SSMStatus SSM_ClientAuthCertListKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource* target = NULL;
    SSMSSLDataConnection* conn;
    char* prefix = NULL;
    char* wrapper = NULL;
    char* suffix = NULL;
    char* tmpStr = NULL;
    char* prefixExp = NULL;
    char* fmt = NULL;
    char* certList = NULL;
    const PRIntn CERT_LIST_PREFIX = (PRIntn)0;
    const PRIntn CERT_LIST_WRAPPER = (PRIntn)1;
    const PRIntn CERT_LIST_SUFFIX = (PRIntn)2;
    const PRIntn CERT_LIST_PARAM_COUNT = (PRIntn)3;

    /* 
     * make sure cx, cx->m_request, cx->m_params, cx->m_result are not
     * NULL
     */
    PR_ASSERT(cx != NULL && cx->m_request != NULL && cx->m_params != NULL &&
	      cx->m_result != NULL);
    if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
	cx->m_result == NULL) {
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	goto loser;
    }

    if (SSM_Count(cx->m_params) != CERT_LIST_PARAM_COUNT) {
	SSM_HTTPReportSpecificError(cx->m_request, "_client_auth_certList: ",
				    "Incorrect number of parameters "
				    " (%d supplied, %d needed).\n",
				    SSM_Count(cx->m_params), 
				    CERT_LIST_PARAM_COUNT);
	goto loser;
    }

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    conn = (SSMSSLDataConnection*)target;

    /* form the MessageFormat object */
    /* find arguments (prefix, wrapper, and suffix) */
    prefix = (char *) SSM_At(cx->m_params, CERT_LIST_PREFIX);
    wrapper = (char *) SSM_At(cx->m_params, CERT_LIST_WRAPPER);
    suffix = (char *) SSM_At(cx->m_params, CERT_LIST_SUFFIX);
    PR_ASSERT(prefix != NULL && wrapper != NULL && suffix != NULL);

    /* grab the prefix and expand it */
    rv = SSM_GetAndExpandTextKeyedByString(cx, prefix, &prefixExp);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    SSM_DebugUTF8String("client cert list prefix", prefixExp);

    /* grab the wrapper */
    rv = SSM_GetAndExpandTextKeyedByString(cx, wrapper, &fmt);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("client cert list wrapper", fmt);

    /* form the wrapped cert list UnicodeString */
    rv = ssm_client_auth_get_utf8_cert_list(conn, fmt, &certList);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    /* grab the suffix and expand it */
    rv = SSM_GetAndExpandTextKeyedByString(cx, suffix, &tmpStr);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("client cert list suffix", tmpStr);

    cx->m_result = PR_smprintf("%s%s%s", prefixExp, certList, tmpStr);
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    PR_FREEIF(tmpStr);
    PR_FREEIF(prefixExp);
    PR_FREEIF(fmt);
    PR_FREEIF(certList);
    return rv;
}


/*
 * Function: SSMStatus ssm_client_auth_get_utf8_cert_list()
 * Purpose: forms the cert list UnicodeString
 *
 * Arguments and return values:
 * - conn: SSL connection object
 * - fmt: cert wrapper UnicodeString format
 * - result: resulting UnicodeString
 *
 * Note: if we include the expired certs, we need to append to the end as
 *       well
 */
SSMStatus ssm_client_auth_get_utf8_cert_list(SSMSSLDataConnection* conn, 
                                             char* fmt, 
                                             char** result)
{
    SSMStatus rv = SSM_SUCCESS;
    char* tmpStr = NULL;
    char* finalStr = NULL;
    int i;

    PR_ASSERT(conn != NULL && fmt != NULL && result != NULL);
    /* in case we fail */
    *result = NULL;
    finalStr = PL_strdup("");
    /* concatenate the string using the nicknames */
    for (i = 0; i < conn->m_UIInfo.numFilteredCerts; i++) {
        tmpStr = PR_smprintf(fmt, i, conn->m_UIInfo.certNicknames[i]);
        if (SSM_ConcatenateUTF8String(&finalStr, tmpStr) != SSM_SUCCESS) {
            goto loser;
        }
        PR_Free(tmpStr);
        tmpStr = NULL;
    }
    
    SSM_DebugUTF8String("client auth: final cert list", finalStr);
    *result = finalStr;
    return SSM_SUCCESS;
loser:
    SSM_DEBUG("client auth: wrapping cert list failed.\n");
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
    PR_FREEIF(finalStr);
    PR_FREEIF(tmpStr);
    return rv;
}


SSMStatus SSM_ClientAuthCertSelectionButtonHandler(HTTPRequest* req)
{
    SSMStatus rv;
    SSMResource* res;
    SSMSSLDataConnection* conn;

    res = (SSMResource*)(req->target);
    conn = (SSMSSLDataConnection*)(req->target);


    if (res->m_buttonType == SSM_BUTTON_OK) {
	/* "OK" button was clicked */
	rv = ssm_http_client_auth_handle_ok_button(req);
	goto done;
    }
    else if (res->m_buttonType == SSM_BUTTON_CANCEL) {
	/* "Cancel" button was clicked */
	rv = ssm_http_client_auth_handle_cancel_button(req);
	goto done;
    }
    else {
	SSM_DEBUG("Don't know which button was clicked.\n");
	rv = SSM_FAILURE;
    }
    SSM_LockResource(res);
    conn->m_UIInfo.chosen = -1;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(res);
    SSM_UnlockResource(res);
done:
    /* this actually includes the error cases returned from individual
     * button handlers
     */
    return rv;
}
    
    
SSMStatus SSM_HTTPClientAuthButtonHandler(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    char* tmpStr = NULL;

    PR_ASSERT(req->target != NULL);
    conn = (SSMSSLDataConnection*)(req->target);

    /* make sure you got the right baseRef */
    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS || 
	PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
	goto loser;
    }

    /* ### sjlee: this is a very ugly way to differentiate among different
     *            buttons
     */
    rv = SSM_HTTPParamValue(req, "do_ok", &tmpStr);
    if (rv == SSM_SUCCESS) {
	/* "OK" button was clicked */
	rv = ssm_http_client_auth_handle_ok_button(req);
	goto done;
    }

    rv = SSM_HTTPParamValue(req, "do_cancel", &tmpStr);
    if (rv == SSM_SUCCESS) {
	/* "Cancel" button was clicked */
	rv = ssm_http_client_auth_handle_cancel_button(req);
	goto done;
    }

    rv = SSM_HTTPParamValue(req, "do_help", &tmpStr);
    if (rv == SSM_SUCCESS) {
	/* "help" button was clicked */
	/* just use cancel handler for now */
	rv = ssm_http_client_auth_handle_cancel_button(req);
	goto done;
    }
loser:
    if (rv == SSM_SUCCESS) {
	SSM_DEBUG("Don't know which button was clicked.\n");
	rv = SSM_FAILURE;
    }
    SSM_LockResource(req->target);
    conn->m_UIInfo.chosen = -1;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);
done:
    /* this actually includes the error cases returned from individual
     * button handlers
     */
    return rv;
}


SSMStatus ssm_http_client_auth_handle_ok_button(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    char* chosen = NULL;
    int i;

    conn = (SSMSSLDataConnection*)(req->target);

    SSM_LockResource(req->target);
    /* retrieve the chosen nickname */
    rv = SSM_HTTPParamValue(req, "chosen", &chosen);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    PR_ASSERT(chosen != NULL);
    PR_sscanf(chosen, "%ld", &i);
    PR_ASSERT(i >= 0 && i < conn->m_UIInfo.numFilteredCerts);

    rv = SSM_HTTPCloseAndSleep(req);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    /* set the predicate to true and unblock the SSL thread */
    conn->m_UIInfo.chosen = i;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;    /* SSM_SUCCESS */
loser:
    /* still we want to unblock the SSL thread: it will see NULL nickname */
    conn->m_UIInfo.chosen = -1;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;
}

SSMStatus ssm_http_client_auth_handle_cancel_button(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;

    conn = (SSMSSLDataConnection*)(req->target);

    SSM_LockResource(req->target);
    /* close the window */
    rv = SSM_HTTPCloseAndSleep(req);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

loser:
    /* set the predicate to true and unblock the SSL thread */
    conn->m_UIInfo.chosen = -1;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;
}



/*
 * Function: SECStatus SSM_SSLMakeCertExpiredDialog()
 * Purpose: dispatch the UI event to create the server cert expired
 *          dialog
 * Arguments and return values
 * - cert: server cert we are dealing with
 * - conn: SSL connection object
 * - returns: SECSuccess if successful *and* the user decides to trust
 *            the cert; appropriate error code otherwise
 */
SECStatus SSM_SSLMakeCertExpiredDialog(CERTCertificate* cert,
                                       SSMSSLDataConnection* conn)
{
    SECStatus rv;
    int64 now;
    int64 notBefore;
    int64 notAfter;
    char* baseRef = NULL;

    /* compare the time and determine which dialog box to bring up */
    now = PR_Now();
    rv = CERT_GetCertTimes(cert, &notBefore, &notAfter);
    if (rv != SECSuccess) {
	return rv;
    }

    if (LL_CMP(now, >, notAfter)) {
	baseRef = PL_strdup("bad_server_cert_expired");
    }
    else {
	baseRef = PL_strdup("bad_server_cert_not_yet_valid");
    }
    if (baseRef == NULL) {
	return SECFailure;
    }

    SSM_LockResource(SSMRESOURCE(conn));
    conn->m_UIInfo.UICompleted = PR_FALSE;
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;

    /* fire up the UI */
    if (SSMControlConnection_SendUIEvent(SSMCONTROLCONNECTION(conn), "get", 
		baseRef, SSMRESOURCE(conn), NULL,&SSMRESOURCE(conn)->m_clientContext, PR_TRUE) != SSM_SUCCESS) {
        rv = SECFailure;
        goto loser;
    }

    /* wait until the UI event is complete */
    while (!conn->m_UIInfo.UICompleted) {
	SSM_WaitResource(SSMRESOURCE(conn), PR_INTERVAL_NO_TIMEOUT);
    }

    if (conn->m_UIInfo.trustBadServerCert == BSCA_NO) {
	/* user did not want to continue.  Cancel here. */
	if (rv == SECSuccess) {
	    rv = SECFailure;
	}
	goto loser;
    }

    /* ### sjlee: ugliness warning! accessing cert fields directly */
    cert->timeOK = PR_TRUE;

#if 0	/* Setting timeOK suffices.  Setting trust flags is unnecessary. */
    rv = SSM_SSLServerCertResetTrust(cert, conn->m_UIInfo.trustBadServerCert);
    if (rv != SECSuccess) {
	goto loser;
    }
#endif

loser:
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_FALSE;
    SSM_UnlockResource(SSMRESOURCE(conn));

    if (baseRef != NULL) {
	PR_Free(baseRef);
    }

    return rv;
}


SECStatus SSM_SSLServerCertResetTrust(CERTCertificate* cert, 
				      SSMBadServerCertAccept accept)
{
    char* nickname = NULL;
    CERTCertTrust trust;
    CERTCertTrust* trustPtr = NULL;
    SECStatus rv;
    
    if (accept == BSCA_NO) {
	/* this usually shouldn't happen */
	goto loser;
    }
    else if (accept == BSCA_PERMANENT) {
	memset((void*)&trust, 0, sizeof(trust));
	/*trust.sslFlags = CERTDB_VALID_PEER | CERTDB_TRUSTED;*/
	/* this resets the SSL flags CERTDB_VALID_PEER | CERTDB_TRUSTED */
	rv = CERT_DecodeTrustString(&trust, "P");
	if (rv != SECSuccess) {
	    goto loser;
	}

	/* XXX not neccessary? */
	/*
	if (mystate->postwarn) {
	    trust.sslFlags |= CERTDB_SEND_WARN;
	    rv = CERT_DecodeTrustString(&trust, "w");
	}
	*/

	nickname = ssm_default_server_nickname(cert);
	if (nickname == NULL) {
	    goto loser;
	}
	
	/* add a temporary certificate to the permanent database */
	rv = CERT_AddTempCertToPerm(cert, nickname, &trust);
	if (nickname != NULL) {
	    PR_Free(nickname);
	}
	if (rv != SECSuccess) {
	    goto loser;
	}

    } else if (accept == BSCA_SESSION) {
	/* bump the ref count on the cert so that it stays around for
	 * the entire session
	 */
	/* ### sjlee: ugliness warning: accessing fields directly */
	cert->keepSession = PR_TRUE;

	if (cert->trust != NULL) {
	    trustPtr = cert->trust;
	}
	else {
	    trustPtr = (CERTCertTrust*)PORT_ArenaZAlloc(cert->arena,
							sizeof(CERTCertTrust));
	    cert->trust = trustPtr;
	}
	if (trustPtr == NULL) {
	    goto loser;
	}

	/* set the trust value */
	/*trustptr->sslFlags |= (CERTDB_VALID_PEER | CERTDB_TRUSTED);*/
	/* reset the trust SSL bit to CERTDB_VALID_PEER | CERTDB_TRUSTED */
	rv = CERT_DecodeTrustString(trustPtr, "P");
	if (rv != SECSuccess) {
	    goto loser;
	}
	/*
	if (mystate->postwarn) {
	    trustptr->sslFlags |= CERTDB_SEND_WARN;
	    rv = CERT_DecodeTrustString(trustptr, "w");
	}
	*/
    }

    return rv;
loser:
    if (rv == SECSuccess) {
	rv = SECFailure;
    }

    return rv;
}


/* This is also called in oldfunc.c when creating default nickname for 
 * a new certificate.
 */
void _ssm_compress_spaces(char* psrc)
{
    char* pdst;
    char c;
    PRBool lastspace = PR_FALSE;
    
    if (psrc == NULL) {
	return;
    }
    
    pdst = psrc;

    while (((c = *psrc) != 0) && isspace(c)) {
	psrc++;
	/* swallow leading spaces */
    }
    
    
    while ((c = *psrc++) != 0) {
	if (isspace(c)) {
	    if (!lastspace) {
		*pdst++ = ' ';
	    }
	    lastspace = PR_TRUE;
	} else {
	    *pdst++ = c;
	    lastspace = PR_FALSE;
	}
    }
    *pdst = '\0';

    /* if the last character is a space, then remove it too */
    pdst--;
    c = *pdst;
    if (isspace(c)) {
	*pdst = '\0';
    }
    
    return;
}


/*
 * Function: char* ssm_default_server_nickname()
 * Purpose: creates a default nickname for a bad server cert in case the
 *          user wanted to continue
 * Arguments and return values
 * - cert: cert in question
 * - returns: the composed nickname; NULL if failure
 *
 * Note: shamelessly copied from the Nova code
 */
static char* ssm_default_server_nickname(CERTCertificate* cert)
{
    char* nickname = NULL;
    int count;
    PRBool conflict;
    char* servername = NULL;
    
    servername = CERT_GetCommonName(&cert->subject);
    if (servername == NULL) {
	goto loser;
    }
    
    count = 1;
    while (1) {
	if (count == 1) {
	    nickname = PR_smprintf("%s", servername);
	}
	else {
	    nickname = PR_smprintf("%s #%d", servername, count);
	}
	if (nickname == NULL) {
	    goto loser;
	}

	_ssm_compress_spaces(nickname);
	
	conflict = SEC_CertNicknameConflict(nickname, &cert->derSubject,
					    cert->dbhandle);	
	if (conflict == PR_SUCCESS) {
	    goto done;
	}

	/* free the nickname */
	PORT_Free(nickname);

	count++;
    }
    
loser:
    if (nickname != NULL) {
	PR_Free(nickname);
	nickname = NULL;
    }
done:
    if (servername != NULL) {
	PR_Free(servername);
    }
    
    return nickname;
}


SSMStatus SSM_CurrentTimeKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv;
    char* wrapper = NULL;
    char* key = NULL;
    void* dfmt = NULL;
    char* utf8Now = NULL;
    int64 now;
    const PRIntn TIME_WRAPPER = (PRIntn)0;
    /* we have one keyword */
	
    /* check arguments */
    /* ### sjlee: might as well make this a helper function because most
     *		  keyword handlers will use this checking
     */
    PR_ASSERT(cx != NULL);
    /*    PR_ASSERT(cx->m_request != NULL);*/
    PR_ASSERT(cx->m_params != NULL);
    PR_ASSERT(cx->m_result != NULL);
    if (cx == NULL || cx->m_params == NULL || cx->m_result == NULL) {
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	goto loser;
    }
	
    /* form the DateFormat object */
	dfmt = nlsNewDateFormat();
	if (!dfmt) {
		goto loser;
	}
	
    /* form the MessageFormat object */
    /* first, find the key (wrapper) */
    key = (char *) SSM_At(cx->m_params, TIME_WRAPPER);
	
    /* second, grab and expand the key word object */
    rv = SSM_GetAndExpandTextKeyedByString(cx, key, &wrapper);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("current time wrapper", wrapper);
    now = PR_Now();
	utf8Now = nslPRTimeToUTF8String(dfmt, now);
    if (!utf8Now) {
		goto loser;
    }
    
    SSMTextGen_UTF8StringClear(&cx->m_result);

    PR_FREEIF(cx->m_result);
    cx->m_result = PR_smprintf(wrapper, utf8Now);
    SSM_DebugUTF8String("result of wrapping", cx->m_result);
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    PR_FREEIF(wrapper);
    if (dfmt != NULL) {
		nlsFreeDateFormat(dfmt);
    }
    return rv;
}



/* this function handles the button clicks for all server auth failure dialogs
 * except the unknown issuers case
 */
SSMStatus SSM_ServerAuthFailureButtonHandler(HTTPRequest* req)
{
    SSMStatus rv;
    SSMResource* res;
    SSMSSLDataConnection* conn;

    res = (SSMResource*)(req->target);
    conn = (SSMSSLDataConnection*)(req->target);


    if (res->m_buttonType == SSM_BUTTON_OK) {
	/* "OK" button was clicked */
	rv = ssm_http_server_auth_handle_ok_button(req);
	goto done;
    }
    else if (res->m_buttonType == SSM_BUTTON_CANCEL) {
	/* "Cancel" button was clicked */
	rv = ssm_http_server_auth_handle_cancel_button(req);
	goto done;
    }
    else {
	SSM_DEBUG("Don't know which button was clicked.\n");
	rv = SSM_FAILURE;
    }
    SSM_LockResource(res);
    conn->m_UIInfo.UICompleted = PR_TRUE;
    /* the default course of action if something goes wrong is not to trust */
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    SSM_NotifyResource(res);
    SSM_UnlockResource(res);
done:
    /* this actually includes the error cases returned from individual
     * button handlers
     */
    return rv;
}


SSMStatus ssm_http_server_auth_handle_ok_button(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    char* accept = NULL;

    conn = (SSMSSLDataConnection*)(req->target);

    SSM_LockResource(req->target);
    /* if the user wanted to go ahead, accept="session" */
    rv = SSM_HTTPParamValue(req, "accept", &accept);
    if (rv != SSM_SUCCESS) {
	/* either the user did not want to continue or something went wrong */
	conn->m_UIInfo.trustBadServerCert = BSCA_NO;
	goto loser;
    }
    PR_ASSERT(accept != NULL);
    if (PL_strcmp(accept, "session") == 0) {
	conn->m_UIInfo.trustBadServerCert = BSCA_SESSION;
    }
    else if (PL_strcmp(accept, "permanent") == 0) {
	conn->m_UIInfo.trustBadServerCert = BSCA_PERMANENT;
    }
    else {
	/* either something went wrong or the user did not want to continue */
	conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    }

loser:
    /* close the window */
    rv = SSM_HTTPCloseAndSleep(req);

    /* set the predicate to true and unblock the SSL thread */
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;
}


SSMStatus ssm_http_server_auth_handle_cancel_button(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    
    conn = (SSMSSLDataConnection*)(req->target);

    SSM_LockResource(req->target);

    /* close the window */
    rv = SSM_HTTPCloseAndSleep(req);

    /* leave the cert untrusted */
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;
}


SSMStatus SSM_ServerAuthDomainNameKeywordHandler(SSMTextGenContext* cx)
{
    SSMResource* target = NULL;
    SSMSSLDataConnection* sslconn = NULL;
    SSMStatus rv;
    char* pattern = NULL;
    char* key = NULL;
    CERTCertificate* serverCert = NULL;
    char* hostname = NULL;
    char* URLHostname = NULL;
    const PRIntn DOMAIN_NAME_FORMAT = (PRIntn)0;
    /* we have one keyword */
	
    /* check arguments */
    /* ### sjlee: might as well make this a helper function because most
     *		  keyword handlers will use this checking
     */
    PR_ASSERT(cx != NULL);
    PR_ASSERT(cx->m_request != NULL);
    PR_ASSERT(cx->m_params != NULL);
    PR_ASSERT(cx->m_result != NULL);
    if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
	cx->m_result == NULL) {
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	goto loser;
    }
	
    /* retrieve the server cert */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(SSM_IsA(target, SSM_RESTYPE_SSL_DATA_CONNECTION));
    sslconn = (SSMSSLDataConnection*)target;

    serverCert = SSL_PeerCertificate(sslconn->socketSSL);
    if (serverCert == NULL) {
	/* couldn't get the server cert: what do I do? */
	goto loser;
    }

    /* get the hostname from the cert */
    hostname = CERT_GetCommonName(&serverCert->subject);
    if (hostname == NULL) {
	goto loser;
    }

    /* get the URL hostname from the socket */
    URLHostname = SSL_RevealURL(sslconn->socketSSL);
    if (URLHostname == NULL) {
	goto loser;
    }

    /* form the MessageFormat object */
    /* first, find the key (format argument) */
    key = (char *) SSM_At(cx->m_params, DOMAIN_NAME_FORMAT);

    /* second, grab and expand the key word object */
    rv = SSM_GetAndExpandTextKeyedByString(cx, key, &pattern);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("domain name string pattern", pattern);

    SSMTextGen_UTF8StringClear(&cx->m_result);
    
    PR_FREEIF(cx->m_result);
    cx->m_result = PR_smprintf(pattern, URLHostname, hostname);
    if (cx->m_result == NULL) {
        goto loser;
    }
    SSM_DebugUTF8String("wrapped domain name string", cx->m_result);

    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    if (serverCert != NULL) {
	CERT_DestroyCertificate(serverCert);
    }
    if (hostname != NULL) {
	PR_Free(hostname);
    }
    if (URLHostname != NULL) {
	PR_Free(URLHostname);
    }
    PR_FREEIF(pattern);
    return rv;
}


/*
 * Function: SECStatus SSM_SSLMakeCertBadDomainDialog()
 * Purpose: dispatch the UI event to create the server cert domain name
 *          mismatch dialog
 * Arguments and return values
 * - cert: server cert we are dealing with
 * - conn: SSL connection object
 * - returns: SECSuccess if successful *and* the user decides to trust
 *            the cert; appropriate error code otherwise
 */
SECStatus SSM_SSLMakeCertBadDomainDialog(CERTCertificate* cert,
                                         SSMSSLDataConnection* conn)
{
	char *           sslHostname = NULL;
    SECStatus        rv          = SECSuccess;

    SSM_LockResource(SSMRESOURCE(conn));
    conn->m_UIInfo.UICompleted = PR_FALSE;
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;

    /* fire up the UI */
    if (SSMControlConnection_SendUIEvent(SSMCONTROLCONNECTION(conn), "get", 
					 "bad_server_cert_domain", 
					 SSMRESOURCE(conn), NULL, 
                     &SSMRESOURCE(conn)->m_clientContext, 
                     PR_TRUE) != SSM_SUCCESS) {
        rv = SECFailure;
        goto loser;
    }

    /* wait until the UI event is complete */
    while (!conn->m_UIInfo.UICompleted) {
	SSM_WaitResource(SSMRESOURCE(conn), PR_INTERVAL_NO_TIMEOUT);
    }

    if (conn->m_UIInfo.trustBadServerCert == BSCA_NO) {
	/* user did not want to continue.  Cancel here. */
	if (rv == SECSuccess) {
	    rv = SECFailure;
	}
	goto loser;
    }

	sslHostname = SSL_RevealURL(conn->socketSSL);
	if (!sslHostname)
		goto loser;
	rv = CERT_AddOKDomainName(cert, sslHostname);
	PORT_Free(sslHostname);

#if 0	/* this is not neccessary, and is wrong (in this case) */
    rv = SSM_SSLServerCertResetTrust(cert, conn->m_UIInfo.trustBadServerCert);
#endif
    if (rv != SECSuccess) {
	goto loser;
    }

loser:
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_FALSE;
    SSM_UnlockResource(SSMRESOURCE(conn));

    return rv;
}

SSMStatus SSM_PrettyFormatCert(CERTCertificate* cert, char* fmt, 
                               char** result,PRBool addIssuerLink)
{
    SSMStatus rv = SSM_SUCCESS;
    char * displayName = NULL, *location=NULL, *state = NULL, *country = NULL;
    char * emailaddr = NULL, * orgName = NULL, *unitName = NULL;
    char* issuer = NULL;
    char* serialNumber = NULL;
    char * notBefore = NULL;
    char * notAfter = NULL;
    char * tmp = NULL;
    unsigned char fingerprint[16];
    SECItem fpItem;
    char* fpStr = NULL;
    char* commentString = NULL;

    /* check arguments */
    if (cert == NULL || fmt == NULL || result == NULL) {
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	goto loser;
    }
    
    /* retrieve cert information */
    
    displayName = CERT_GetCommonName(&cert->subject);
    emailaddr = CERT_GetCertEmailAddress(&cert->subject);
    orgName = CERT_GetOrgName(&cert->subject);
    unitName = CERT_GetOrgUnitName(&cert->subject);
    if (!displayName) 
        displayName = PL_strdup("");
    if (!emailaddr)
        emailaddr = PL_strdup("");
    if (!orgName) 
        orgName = PL_strdup("");
    if (!unitName)
        unitName = PL_strdup("");
    
    location = CERT_GetLocalityName(&cert->subject);
    if (!location) 
      location = PL_strdup("");
    state = CERT_GetStateName(&cert->subject);
    if (!state)
        state = PL_strdup("");
    country = CERT_GetCountryName(&cert->subject);
    if (!country)
        country = PL_strdup("");

    issuer = CERT_GetOrgName(&cert->issuer);
    if (issuer == NULL) {
        issuer = PL_strdup("");
    } else {
        /*
         * Don't add the extra link if this is a self-signed cert.
         */
        if (addIssuerLink &&
            CERT_CompareName(&cert->subject, &cert->issuer) != SECEqual) {
            tmp=PR_smprintf("<a href=\"javascript:openIssuerWindow();\">%s</a>",
                              issuer);
            PR_Free(issuer);
            issuer = tmp;
            tmp = NULL;
        }
    }

    serialNumber = CERT_Hexify(&cert->serialNumber, 1);
    if (serialNumber == NULL) {
	serialNumber = PL_strdup("");
    }

    notBefore = DER_UTCDayToAscii(&cert->validity.notBefore);
    if (!notBefore) 
        notBefore = PL_strdup("");
    notAfter = DER_UTCDayToAscii(&cert->validity.notAfter);
    if (!notAfter) 
        notAfter = PL_strdup("");

    MD5_HashBuf(fingerprint, cert->derCert.data, cert->derCert.len);

    fpItem.data = fingerprint;
    fpItem.len = sizeof(fingerprint);

    fpStr = CERT_Hexify(&fpItem, 1);
    if (fpStr == NULL) {
	fpStr = PL_strdup("");
    }

    commentString = CERT_GetCertCommentString(cert);
    if (commentString == NULL) {
	commentString = PL_strdup(" ");
    }
    /* comments can be NULL */

    *result = PR_smprintf(fmt, displayName, emailaddr, unitName, orgName, 
                          location, state, country, issuer, serialNumber, 
                          notBefore, notAfter, fpStr, commentString);

    if (*result == NULL) {
        goto loser;
    }
    SSM_DebugUTF8String("wrapped view cert string", *result);
    goto done;
loser:
    SSM_DEBUG("Pretty formatting cert failed.\n");
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    if (issuer != NULL) {
	PR_Free(issuer);
    }
    if (serialNumber != NULL) {
	PR_Free(serialNumber);
    }
    if (fpStr != NULL) {
	PR_Free(fpStr);
    }
    if (commentString != NULL) {
	PR_Free(commentString);
    }
    PR_FREEIF(notBefore);
    PR_FREEIF(notAfter);
    PR_FREEIF(displayName);
    PR_FREEIF(emailaddr);
    PR_FREEIF(orgName);
    PR_FREEIF(unitName);
    return rv;
}


SSMStatus SSM_VerifyServerCertKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv;
    SSMResource* target = NULL;
    SSMSSLDataConnection* sslconn = NULL;
    CERTCertDBHandle* handle = NULL;
    CERTCertificate* serverCert = NULL;
    char* nickname = NULL;
    char* pattern = NULL;

    PR_ASSERT(cx != NULL);
    PR_ASSERT(cx->m_request != NULL);
    PR_ASSERT(cx->m_params != NULL);
    PR_ASSERT(cx->m_result != NULL);

    /* retrieve the server cert */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(SSM_IsA(target, SSM_RESTYPE_SSL_DATA_CONNECTION));
    sslconn = (SSMSSLDataConnection*)target;
    handle = SSMCONTROLCONNECTION(sslconn)->m_certdb;
    
    serverCert = SSL_PeerCertificate(sslconn->socketSSL);
    if (serverCert == NULL) {
	goto loser;
    }

    nickname = CERT_GetNickName(serverCert, handle, serverCert->arena);
    if (nickname == NULL) {
        /* nickname was not found: that's still OK, let's do this */
        nickname = PL_strdup("Unknown");
	if (nickname == NULL) {
	    goto loser;
	}
    }
    /* don't free it! */
    /* if we want to verify the cert, we would do something like this...
    srv = CERT_VerifyCertNow(ctrlconn->m_certdb, cert, PR_TRUE, certSSLServer,
			     conn);
    */
    SSMTextGen_UTF8StringClear(&cx->m_result);
    rv = SSM_GetAndExpandTextKeyedByString(cx, "not_verified_text", 
                                           &cx->m_result);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("wrapped verification string %s", cx->m_result);
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    if (serverCert != NULL) {
	CERT_DestroyCertificate(serverCert);
    }
    if (nickname != NULL) {
	PR_Free(nickname);
    }
    PR_FREEIF(pattern);
    return rv;
}


/*
 * Function: SECStatus SSM_SSLMakeUnknownIssuerDialog()
 * Purpose: dispatch the UI event to create the unknown issuer dialog
 * Arguments and return values
 * - cert: server cert we are dealing with
 * - conn: SSL connection object
 * - returns: SECSuccess if successful *and* the user decides to trust
 *            the cert; appropriate error code otherwise
 */
SECStatus SSM_SSLMakeUnknownIssuerDialog(CERTCertificate* cert,
                                         SSMSSLDataConnection* conn)
{
    SECStatus rv = SECSuccess;

    SSM_LockResource(SSMRESOURCE(conn));
    conn->m_UIInfo.UICompleted = PR_FALSE;
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;

    /* fire up the UI */
    if (SSMControlConnection_SendUIEvent(SSMCONTROLCONNECTION(conn), "get", 
					 "bad_server_cert_unknown_issuer1", 
					 SSMRESOURCE(conn), NULL, 
                     &SSMRESOURCE(conn)->m_clientContext, PR_TRUE) != SSM_SUCCESS) {
	rv = SECFailure;
	goto loser;
    }

    /* wait until the UI event is complete */
    while (!conn->m_UIInfo.UICompleted) {
	SSM_WaitResource(SSMRESOURCE(conn), PR_INTERVAL_NO_TIMEOUT);
    }

    if (conn->m_UIInfo.trustBadServerCert == BSCA_NO) {
	/* user did not want to continue.  Cancel here. */
	if (rv == SECSuccess) {
	    rv = SECFailure;
	}
	goto loser;
    }

    /* reset the trust bit for the session and continue */
    rv = SSM_SSLServerCertResetTrust(cert, conn->m_UIInfo.trustBadServerCert);
    if (rv != SECSuccess) {
	goto loser;
    }

loser:
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_FALSE;
    SSM_UnlockResource(SSMRESOURCE(conn));

    return rv;
}


SSMStatus SSM_HTTPUnknownIssuerStep1ButtonHandler(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;
    char* tmpStr = NULL;

    PR_ASSERT(req->target != NULL);
    conn = (SSMSSLDataConnection*)(req->target);

    /* make sure you got the right baseRef */
    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS || 
	PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
	goto loser;
    }

    rv = SSM_HTTPParamValue(req, "do_cancel", &tmpStr);
    if (rv == SSM_SUCCESS) {
	/* cancel button was clicked */
	req->target->m_buttonType = SSM_BUTTON_CANCEL;
    }
    else {
	rv = SSM_HTTPParamValue(req, "do_next", &tmpStr);
	if (rv == SSM_SUCCESS) {
	    /* next button was clicked */
	    req->target->m_buttonType = SSM_BUTTON_OK;    /* close enough */
	}
    }
    if (rv != SSM_SUCCESS) {
	rv = SSM_ERR_NO_BUTTON;
	goto loser;
    }

    switch (req->target->m_buttonType)
    {
    case SSM_BUTTON_CANCEL:
        rv = ssm_http_server_auth_handle_cancel_button(req);
	break;
    case SSM_BUTTON_OK:
        rv = ssm_http_unknown_issuer_step1_handle_next_button(req);
	break;
    default:
        break;
    }

    return rv;    /* error code will be properly set */
loser:
    /* set the predicate to true and unblock the SSL thread */
    SSM_LockResource(req->target);
    conn->m_UIInfo.UICompleted = PR_TRUE;
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
    return rv;
}


SSMStatus ssm_http_unknown_issuer_step1_handle_next_button(HTTPRequest* req)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn;

    conn = (SSMSSLDataConnection*)(req->target);

    SSM_LockResource(req->target);

    /* do away with the first dialog */
    rv = SSM_HTTPCloseAndSleep(req);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    /* fire up the next dialog */
    rv = SSMControlConnection_SendUIEvent(SSMCONTROLCONNECTION(conn), "get", 
					  "bad_server_cert_unknown_issuer2", 
					  SSMRESOURCE(conn), NULL, &SSMRESOURCE(conn)->m_clientContext, PR_TRUE);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    conn->m_UIInfo.UICompleted = PR_FALSE;
    /* the above is redundant but for peace of mind */
    SSM_UnlockResource(req->target);
    return rv;    /* SSM_SUCCESS */
loser:
    /* still we want to unblock the SSL thread: the connection will fail */
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);

    return rv;
}


