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
#ifdef XP_MAC
#include "platform.h"
#endif
#include "advisor.h"
#include "nlsutil.h"
#include "minihttp.h"
#include "p12res.h"
#include "textgen.h"
#include "sslskst.h"
#include "certlist.h"
#include "ocsp.h"
#include "secoid.h"
#include "prefs.h"
#include "messages.h"
#include "secerr.h"
#include "sslerr.h"
#include "base64.h"

/*
 * This is the structure used to gather all of the CA's that can be used
 * for OCSP responders.
 */
typedef struct SSMDefaultOCSPRespondersListStr{
    SSMSortedList *respondersWithAIA;
    SSMSortedList *respondersWithoutAIA;
    SSMTextGenContext *cx;
    char *wrapper, *defaultSigner;
} SSMDefaultOCSPRespondersList;

SSMStatus SSM_SetSelectedItemInfo(SSMSecurityAdvisorContext* cx);

#define SSMRESOURCE(object) (&object->super)

#define SSM_NO_INFO						"sa_no_info"
#define SSM_NAVIGATOR_NO_SEC            "sa_navigator_no_sec"
#define SSM_NAVIGATOR_SSL               "sa_navigator_ssl"
#define SSM_NAVIGATOR_BAD_SSL			"sa_navigator_bad_ssl"
#define SSM_MESSAGE						"sa_message"
#define SSM_MESSAGE_NOT_SIGNED			"sa_message_not_signed"
#define SSM_MESSAGE_NOT_ENCRYPTED		"sa_message_not_encrypted"
#define SSM_MESSAGE_SIGNED				"sa_message_signed"
#define SSM_MESSAGE_ENCRYPTED			"sa_message_encrypted"
#define SSM_MESSAGE_BAD_SIGNED			"sa_message_bad_signed"
#define SSM_MESSAGE_BAD_ENCRYPTED		"sa_message_bad_encrypted"

/* A list of User agent strings that we know can do S/MIME
 * and want the Java tab as well.
 */

#define COMMON_TO_SMIME_AND_JAVA "Mozilla/4.7"

const char *kSMimeApps[]  = {COMMON_TO_SMIME_AND_JAVA, NULL};
const char *kJavaJSApps[] = {COMMON_TO_SMIME_AND_JAVA, NULL};

char * SSM_ConvertStringToHTMLString(char * string);
char * SSMUI_GetPKCS12Error(PRIntn error, PRBool isBackup);

PRBool
SSM_IsCRLPresent(SSMControlConnection *ctrl)
{
    SECStatus rv = SECFailure;
    CERTCrlHeadNode *head = NULL;
    PRBool retVal = PR_FALSE;
    
    rv = SEC_LookupCrls(ctrl->m_certdb, &head, -1);
    if (rv != SECSuccess) {
        goto done;
    }
    if (head == NULL) {
        goto done;
    }
    retVal = (head->first == NULL) ? PR_FALSE : PR_TRUE;
    PORT_FreeArena(head->arena, PR_FALSE);
 done:
    return retVal;
}

SSMStatus 
SSMSecurityAdvisorContext_Create(SSMControlConnection *ctrl, 
                                 InfoSecAdvisor *info, 
                                 SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSecurityAdvisorContext *ct;
	int i;

    *res = NULL; /* in case we fail */
    
    ct = (SSMSecurityAdvisorContext *) 
        PR_CALLOC(sizeof(SSMSecurityAdvisorContext));
    if (!ct) 
        goto loser;

    rv = SSMResource_Init(ctrl, &ct->super, SSM_RESTYPE_SECADVISOR_CONTEXT);
    if (rv != PR_SUCCESS) 
        goto loser;
    
    /* this hash will contail list of formatted certs nickname to display */
    ct->m_certhash = NULL;
    ct->m_certsIncluded = 0;
    
    /* register us with ControlConection */
    if (!ctrl->m_secAdvisorList) {
        ctrl->m_secAdvisorList = (SECItem *) PR_Malloc(sizeof(SECItem));
        ctrl->m_secAdvisorList->len = 0;
        ctrl->m_secAdvisorList->data = NULL;
    } 
    ctrl->m_secAdvisorList->len++;
    ctrl->m_secAdvisorList->data = (unsigned char *) PR_REALLOC(ctrl->m_secAdvisorList->data, 
                                               ctrl->m_secAdvisorList->len);
    ctrl->m_secAdvisorList->data[ ctrl->m_secAdvisorList->len - 1 ] = 
        ((SSMResource *)ct)->m_id;

    if (info) {
        ct->infoContext = info->infoContext;
        ct->resID = info->resID;
        ct->hostname = info->hostname ? PL_strdup(info->hostname) : NULL;
		ct->senderAddr = info->senderAddr ? PL_strdup(info->senderAddr) : NULL;
		ct->encryptedP7CInfo = info->encryptedP7CInfo;
		ct->signedP7CInfo = info->signedP7CInfo;
		ct->decodeError = info->decodeError;
		ct->verifyError = info->verifyError;
		ct->encryptthis = info->encryptthis;
		ct->signthis = info->signthis;
		ct->numRecipients = info->numRecipients;
	    if (info->numRecipients > 0) {
			ct->recipients = (char **) PR_CALLOC(sizeof(char*)*(info->numRecipients));
			if (!ct->recipients) {
				goto loser;
			}
	        for (i=0;i<info->numRecipients;i++) {
		        ct->recipients[i] = PL_strdup(info->recipients[i]);
			}
		}

        SSM_SetSelectedItemInfo(ct);
    }

    /* Create a URL for the security advisor window. */
    rv = (SSMStatus) SSM_GenerateURL(ctrl, "get", "secadvisor", 
                         &ct->super, NULL, 
                         &ct->m_width, &ct->m_height, 
                         &ct->m_url);
    if (rv != SSM_SUCCESS)
        goto loser;

    SSMSecurityAdvisorContext_Invariant(ct);

    *res = &ct->super;
    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (ct) 
    {
        ct->super.m_refCount = 1; /* force destroy */
        SSM_FreeResource(&ct->super);
    }
        
    return rv;
}

char * SSMUI_GetPKCS12Error(PRIntn error, PRBool isBackup)
{
    char * responseKey;
    
    switch (error) { 
    case SSM_ERR_NO_PASSWORD:
        responseKey = "pkcs12_bad_portable_password_restore";
        break;
    case SSM_ERR_BAD_DB_PASSWORD:
        responseKey = "pkcs12_bad_db_password";
        break;
    case SSM_ERR_BAD_FILENAME:
        responseKey = "pkcs12_bad_filepath";
        break;
    case SSM_ERR_NEED_USER_INIT_DB:
        responseKey = "pkcs12_need_db_init";
        break;
    case SSM_ERR_CANNOT_DECODE:
        responseKey="pkcs12_cannot_decode";
        break;
    case SSM_PKCS12_CERT_ALREADY_EXISTS:
        responseKey="pkcs12_cert_already_exists";
        break;
    case SSM_ERR_BAD_REQUEST:
    default:
        responseKey = (isBackup) ? "pkcs12_backup_failure" : 
                                   "pkcs12_restore_failure";
    }

    return responseKey;
}

SSMStatus 
SSMSecurityAdvisorContext_Destroy(SSMResource *res, PRBool doFree)
{
    SSMSecurityAdvisorContext *ct = (SSMSecurityAdvisorContext *) res;
    PRIntn i = 0, others = 0;
    if (ct)
    {
        PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_SECADVISOR_CONTEXT));
        SSMResource_Destroy(res, PR_FALSE);
        
        /* Dereference the security info object */
        if (ct->m_infoSource)
        {
            SSM_FreeResource(ct->m_infoSource);
            ct->m_infoSource = NULL;
        }

        /* Free the URL */
        PR_FREEIF(ct->m_url);

        if (ct->m_certhash)
            SSMSortedList_Destroy(ct->m_certhash);
        
        /* deregister with control connection */
        while (i < res->m_connection->m_secAdvisorList->len) {
            if (res->m_connection->m_secAdvisorList->data[i] == res->m_id)
                res->m_connection->m_secAdvisorList->data[i] = 0;
            if (res->m_connection->m_secAdvisorList->data[i])
                others ++;
            i++;
        }
        if (!others) {
            SECITEM_ZfreeItem(res->m_connection->m_secAdvisorList, PR_TRUE);
            res->m_connection->m_secAdvisorList = NULL;
        }

        if (ct->socketStatus) {
            SSM_FreeResource(&ct->socketStatus->super);
        }
        PR_FREEIF(ct->hostname);
        PR_FREEIF(ct->senderAddr);
        if (ct->recipients) {
            for (i=0; i<ct->numRecipients; i++) {
                PR_FREEIF(ct->recipients[i]);
            }
            PR_FREEIF(ct->recipients);
        }
        /* Free if asked */
        if (doFree)
            PR_Free(ct);
    }
    return PR_SUCCESS; /* no way to fail, really */
}

void 
SSMSecurityAdvisorContext_Invariant(SSMSecurityAdvisorContext *ct)
{
    /* Check superclass. */
    SSMResource_Invariant(&ct->super);

    /* Make sure we always have a URL. */
    PR_ASSERT(ct->m_url != NULL);
}


SSMStatus 
SSMSecurityAdvisorContext_GetAttrIDs(SSMResource *res,
                                     SSMAttributeID **ids,
                                     PRIntn *count)
{
    SSMStatus rv;

    rv = SSMResource_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 4) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_SECADVISOR_URL;
    (*ids)[*count++] = SSM_FID_SECADVISOR_WIDTH;
    (*ids)[*count++] = SSM_FID_SECADVISOR_HEIGHT;
    (*ids)[*count++] = SSM_FID_CLIENT_CONTEXT;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMSecurityAdvisorContext_GetAttr(SSMResource *res,
                                  SSMAttributeID attrID,
                                  SSMResourceAttrType attrType,
                                  SSMAttributeValue *value)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSecurityAdvisorContext *ct = (SSMSecurityAdvisorContext *) res;

    SSMSecurityAdvisorContext_Invariant(ct);

    switch(attrID)
    {
    case SSM_FID_SECADVISOR_URL:
        /* Duplicate and return the string. */
        value->type = SSM_STRING_ATTRIBUTE;
        value->u.string.len = PL_strlen(ct->m_url);
        value->u.string.data = (unsigned char *) PL_strdup(ct->m_url);
        break;
    case SSM_FID_SECADVISOR_WIDTH:
    case SSM_FID_SECADVISOR_HEIGHT:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = (attrID == SSM_FID_SECADVISOR_WIDTH) ?
            ct->m_width : ct->m_height;
        break;
    case SSM_FID_CLIENT_CONTEXT:
      SSM_DEBUG("Getting security advisor client context");
      value->type = SSM_STRING_ATTRIBUTE;
      if (!(value->u.string.data = (unsigned char *) PR_Malloc(res->m_clientContext.len))) {
          goto loser;
      }
      memcpy(value->u.string.data, res->m_clientContext.data, res->m_clientContext.len);
      value->u.string.len = res->m_clientContext.len;
      break;

    default:
        rv = SSMResource_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS)
            goto loser;
    }

    goto done;
 loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus SSMSecurityAdvisorContext_SetAttr(SSMResource *res,
                                  SSMAttributeID attrID,
                                  SSMAttributeValue *value)
{
    switch(attrID) {
    case SSM_FID_CLIENT_CONTEXT:
      SSM_DEBUG("Setting security advisor client context\n");
      if (value->type != SSM_STRING_ATTRIBUTE) {
          goto loser;
      }
      if (!(res->m_clientContext.data = (unsigned char *) PR_Malloc(value->u.string.len))) {
          goto loser;
      }
      memcpy(res->m_clientContext.data, value->u.string.data, value->u.string.len);
      res->m_clientContext.len = value->u.string.len;
      break;
    default:
      SSM_DEBUG("Got unknown security advisor Set Attribute Request %d\n", attrID);
      goto loser;
      break;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}


/* Preference keys used in Security Advisor JavaScript.
 * They are used to cache temporary changes the user has made.
 */
#define SSL2_SPK "enable_ssl2"
#define SSL3_SPK "enable_ssl3"
#define TLS_SPK "enable_tls"
#define CLIENT_AUTH_SPK "client_auth_auto_select"
#define EMAIL_CERT_SPK "default_email_cert"
#define WARN_ENTER_SECURE_SPK "warn_entering_secure"
#define WARN_LEAVE_SECURE_SPK "warn_leaving_secure"
#define WARN_VIEW_MIXED_SPK "warn_viewing_mixed"
#define WARN_SUBMIT_INSECURE_SPK "warn_submit_insecure"
#define ENCRYPT_MAIL_SPK "mail_encrypt_outgoing_mail"
#define SIGN_MAIL_SPK "mail_crypto_sign_outgoing_mail"
#define SIGN_NEWS_SPK "mail_crypto_sign_outgoing_news"

/* maximum number of pref items that will be sent back to the client */
#define ITEMS_MAX 11


static SSMStatus SSMSecurityAdvisor_get_bool_value(HTTPRequest* req,
                                                   char* key, PRBool* value)
{
    SSMStatus rv;
    char* tmpStr = NULL;

    rv = SSM_HTTPParamValue(req, key, &tmpStr);
    if (rv != SSM_SUCCESS) {
        return rv;
    }

    if (PL_strcmp(tmpStr, "true") == 0) {
        *value = PR_TRUE;
    }
    else if (PL_strcmp(tmpStr, "false") == 0) {
        *value = PR_FALSE;
    }
    else {
        SSM_DEBUG("I don't understand the value.\n");
        return SSM_FAILURE;
    }
    return rv;
}

static SSMStatus ssm_set_pack_bool_pref(PrefSet* prefs, char* key, 
                                        PRBool value, SetPrefElement* list,
                                        PRIntn* n)
{
    SSMStatus rv;

    /* set the change to memory */
    rv = PREF_SetBoolPref(prefs, key, value);
    if (rv != PR_SUCCESS) {
        return rv;
    }

    /* pack the change */
    list[*n].key = PL_strdup(key);
    list[*n].type = BOOL_PREF;
    if (value == PR_TRUE) {
        list[*n].value = PL_strdup("true");
    }
    else {
        list[*n].value = PL_strdup("false");
    }
    (*n)++;

    return rv;
}

static SSMStatus
SSMSecurityAdvisorContext_SavePrefs(SSMSecurityAdvisorContext* cx,
                                    HTTPRequest* req)
{
    SSMStatus rv;
    SSMControlConnection* ctrl = NULL;
    PrefSet* prefs = NULL;
    PRBool ssl2on;
    PRBool ssl3on;
    PRBool tlson;
    PRBool autoSelect;
    PRBool warnEnterSecure;
    PRBool warnLeaveSecure;
    PRBool warnViewMixed;
    PRBool warnSubmitInsecure;
    PRBool encryptMail;
    PRBool signMail;
    PRBool signNews;
    char* autoStr = NULL;
    char* defaultCert = NULL;
    SetPrefElement list[ITEMS_MAX];
    SetPrefListMessage request;
    PRIntn n = 0;    /* counter */
    int i;
    CMTItem message;

    PR_ASSERT(cx != NULL && cx->super.m_connection != NULL &&
              cx->super.m_connection->m_prefs != NULL);
    ctrl = cx->super.m_connection;
    prefs = ctrl->m_prefs;

    /* retrieve pref values */
    rv = SSMSecurityAdvisor_get_bool_value(req, SSL2_SPK, &ssl2on);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, SSL3_SPK, &ssl3on);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, TLS_SPK, &tlson);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, CLIENT_AUTH_SPK, &autoSelect);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (autoSelect == PR_TRUE) {
        autoStr = "Select Automatically";
    }
    else {
        autoStr = "Ask Every Time";
    }

    rv = SSM_HTTPParamValue(req, EMAIL_CERT_SPK, &defaultCert);
    if (defaultCert[0] == '\0') {
        defaultCert = NULL;
        rv = SSM_SUCCESS;
    }
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, WARN_ENTER_SECURE_SPK,
                                           &warnEnterSecure);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, WARN_LEAVE_SECURE_SPK,
                                           &warnLeaveSecure);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, WARN_VIEW_MIXED_SPK,
                                           &warnViewMixed);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, WARN_SUBMIT_INSECURE_SPK,
                                           &warnSubmitInsecure);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, ENCRYPT_MAIL_SPK, 
                                           &encryptMail);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, SIGN_MAIL_SPK, &signMail);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    rv = SSMSecurityAdvisor_get_bool_value(req, SIGN_NEWS_SPK, &signNews);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    /* commit the changes */
    if (PREF_BoolPrefChanged(prefs, "security.enable_ssl2", ssl2on)) {
        /* value has changed */
        rv = ssm_set_pack_bool_pref(prefs, "security.enable_ssl2", ssl2on,
                                    (SetPrefElement*)list, &n);
        SSL_EnableDefault(SSL_ENABLE_SSL2, ssl2on);
    }

    if (PREF_BoolPrefChanged(prefs, "security.enable_ssl3", ssl3on)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.enable_ssl3", ssl3on,
                                    (SetPrefElement*)list, &n);
        SSL_EnableDefault(SSL_ENABLE_SSL3, ssl3on);
    }

    if (PREF_BoolPrefChanged(prefs, "security.enable_tls", tlson)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.enable_tls", tlson,
                                    (SetPrefElement*)list, &n);
        SSL_EnableDefault(SSL_ENABLE_TLS, tlson);
    }

    if (PREF_StringPrefChanged(prefs, "security.default_personal_cert", 
                               autoStr)) {
        rv = PREF_SetStringPref(prefs, "security.default_personal_cert", 
                                autoStr);
        
        list[n].key = PL_strdup("security.default_personal_cert");
        list[n].type = STRING_PREF;
        list[n].value = PL_strdup(autoStr);
        n++;
    }
    
    if (PREF_StringPrefChanged(prefs, "security.default_mail_cert",
                               defaultCert)) {
        rv = PREF_SetStringPref(prefs, "security.default_mail_cert", 
                                defaultCert);

        list[n].key = PL_strdup("security.default_mail_cert");
        list[n].type = STRING_PREF;
        list[n].value = PL_strdup(defaultCert);
        n++;
    }

    if (PREF_BoolPrefChanged(prefs, "security.warn_entering_secure", 
                             warnEnterSecure)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.warn_entering_secure",
                                    warnEnterSecure, (SetPrefElement*)list,
                                    &n);
    }

    if (PREF_BoolPrefChanged(prefs, "security.warn_leaving_secure", 
                             warnLeaveSecure)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.warn_leaving_secure",
                                    warnLeaveSecure, (SetPrefElement*)list,
                                    &n);
    }

    if (PREF_BoolPrefChanged(prefs, "security.warn_viewing_mixed", 
                             warnViewMixed)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.warn_viewing_mixed",
                                    warnViewMixed, (SetPrefElement*)list, &n);
    }

    if (PREF_BoolPrefChanged(prefs, "security.warn_submit_insecure", 
                             warnSubmitInsecure)) {
        rv = ssm_set_pack_bool_pref(prefs, "security.warn_submit_insecure",
                                    warnSubmitInsecure, (SetPrefElement*)list,
                                    &n);
    }

    if (PREF_BoolPrefChanged(prefs, "mail.encrypt_outgoing_mail", 
                             encryptMail)) {
        rv = ssm_set_pack_bool_pref(prefs, "mail.encrypt_outgoing_mail",
                                    encryptMail, (SetPrefElement*)list, &n);
    }

    if (PREF_BoolPrefChanged(prefs, "mail.crypto_sign_outgoing_mail", 
                             signMail)) {
        rv = ssm_set_pack_bool_pref(prefs, "mail.crypto_sign_outgoing_mail",
                                    signMail, (SetPrefElement*)list, &n);
    }

    if (PREF_BoolPrefChanged(prefs, "mail.crypto_sign_outgoing_news", 
                             signNews)) {
        rv = ssm_set_pack_bool_pref(prefs, "mail.crypto_sign_outgoing_news",
                                    signNews, (SetPrefElement*)list, &n);
    }

    rv = SSM_HTTPDefaultCommandHandler(req);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* finally, send the changes to the plugin so that it can save the
     * changes
     */
    if (n > 0) {
        /* we need to send this event only if prefs changed */
        request.length = n;
        request.list = list;
        message.type = SSM_EVENT_MESSAGE | SSM_SAVE_PREF_EVENT;
        if (CMT_EncodeMessage(SetPrefListMessageTemplate, &message, 
                              &request) != CMTSuccess) {
            goto loser;
        }

        /* send the message through the control out queue */
        SSM_SendQMessage(ctrl->m_controlOutQ, SSM_PRIORITY_NORMAL, 
                         message.type, message.len, (char*)message.data,
                         PR_TRUE);
    }
loser:
    /* clean out list */
    for (i = 0; i < n; i++) {
        if (list[i].key != NULL) {
            PR_Free(list[i].key);
        }
        if (list[i].value != NULL) {
            PR_Free(list[i].value);
        }
    }
    return rv;
}

SSMStatus
SSMSecurityAdvisorContext_DoPKCS12Response(SSMSecurityAdvisorContext *advisor,
                                           HTTPRequest *req,
                                           const char  *responseKey)
{
    SSMTextGenContext *cx = NULL;
    SSMStatus rv = SSM_FAILURE;
    char name[256];
    char *page = "pkcs12_action_followup";
    char *type = NULL, *hdrs = NULL, *content = NULL;
    char *alertMessage = NULL, *out = NULL;

    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS) {
        SSM_HTTPReportSpecificError(req, "DoPKCS12Response: Error%d "
                                    "attempting to create textgen context.",
                                    rv);
        goto loser;
    }
    PR_snprintf(name, 256, "%s_type", page);
    rv = SSM_GetUTF8Text(cx, name, &type);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    PR_snprintf(name, 256, "%s_content", page);
    rv = SSM_GetAndExpandText(cx, name, &content);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_GetUTF8Text(cx, responseKey, &alertMessage);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    out = PR_smprintf(content, alertMessage, advisor->super.m_id);
    rv = SSM_HTTPSendOKHeader(req, hdrs, type);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_HTTPSendUTF8String(req, out);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    req->sentResponse = PR_TRUE;
    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    PR_FREEIF(type);
    PR_FREEIF(hdrs);
    PR_FREEIF(content);
    PR_FREEIF(out);
    PR_FREEIF(alertMessage);
    return rv;
}

static SSMStatus
SSMSecurityAdvisorContext_DoNewDefMailReponse(SSMPKCS12Context *p12Cxt,
                                              HTTPRequest      *req)
{
    SSMTextGenContext *cx = NULL;
    char *fmt=NULL, *content=NULL, *defEmailCert=NULL, *expContent=NULL;
    SSMStatus rv;

    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS) {
        SSM_HTTPReportSpecificError(req, "DoNewDefMailReponse: Failed to "
                                         "create new TextGenContext.");
        goto loser;
    }
    rv = SSM_FindUTF8StringInBundles(cx, "pkcs12_restore_success_new_mail", 
                                     &fmt);
    if (rv != SSM_SUCCESS || fmt == NULL) {
        goto loser;
    }
    
    rv = PREF_GetStringPref(req->ctrlconn->m_prefs, 
                            "security.default_mail_cert", &defEmailCert);
    if (rv != SSM_SUCCESS || defEmailCert == NULL) {
        goto loser;
    }
    content = PR_smprintf(fmt, defEmailCert, req->target->m_id);
    if (content == NULL) {
        goto loser;
    }
    rv = SSMTextGen_SubstituteString(cx, content, &expContent);
    if (rv != SSM_SUCCESS || expContent == NULL) {
        goto loser;
    }
    rv = SSM_HTTPSendOKHeader(req, "", "text/html");
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_HTTPSendUTF8String(req, expContent);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    PR_Free(expContent);
    PR_Free(content);
    PR_Free(fmt);
    req->sentResponse = PR_TRUE;
    return SSM_SUCCESS;
 loser:
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    PR_FREEIF(fmt);
    PR_FREEIF(content);
    PR_FREEIF(expContent);
    return SSM_FAILURE;
}

SSMStatus SSMSecurityAdvisorContext_DoPKCS12Restore(
                                              SSMSecurityAdvisorContext *res,
                                              HTTPRequest               *req)
{
    SSMStatus           rv;
    SSMPKCS12CreateArg  p12Create;
    SSMPKCS12Context   *p12Cxt=NULL;
    SSMResourceID       rid;
    const char         *responseKey;

    p12Create.isExportContext = PR_FALSE;
    rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_PKCS12_CONTEXT, 
                                        (void*)&p12Create,
                                        SSMRESOURCE(res)->m_connection,
                                        &rid, (SSMResource **)(&p12Cxt));
    if (rv != PR_SUCCESS) {
        goto done;
    }

    /* pass along Advisor's client context for window management */
      /* pass along Advisor's client context for window management */
    SSM_CopyCMTItem(&((SSMResource *)p12Cxt)->m_clientContext,
                    &((SSMResource *)res)->m_clientContext);
    
    rv = SSMPKCS12Context_RestoreCertFromPKCS12File(p12Cxt);
    if (rv == SSM_ERR_NEW_DEF_MAIL_CERT) {
        SSM_ChangeCertSecAdvisorList(req, NULL, certHashAdd);
        rv = SSMSecurityAdvisorContext_DoNewDefMailReponse(p12Cxt,req);
    } else {
        if (p12Cxt->super.m_buttonType == SSM_BUTTON_CANCEL){
            rv = SSM_SUCCESS;
            SSM_HTTPReportError(req, HTTP_NO_CONTENT);
        } else if (rv != SSM_SUCCESS) {
            responseKey = SSMUI_GetPKCS12Error(rv, PR_FALSE);
        } else {
            responseKey = "pkcs12_restore_success";
            SSM_ChangeCertSecAdvisorList(req, NULL, certHashAdd);
        }
        rv = SSMSecurityAdvisorContext_DoPKCS12Response(res, req, responseKey);
    }
 done:
    if (p12Cxt != NULL) {
        SSM_FreeResource(SSMRESOURCE(p12Cxt));
    }
    return rv;
}

static CERTCertificate*
SSMSecurityAdvisorContext_FindCertByNickname(SSMSecurityAdvisorContext *cx, 
                                             HTTPRequest *req,
                                             char *certNickname)
{
    CERTCertList      *certList     = NULL;
    CERTCertificate   *cert         = NULL;
    CERTCertListNode  *certListNode = NULL;
    PRInt32            numcerts     = 0;
    SSMTextGenContext *textGenCx    = NULL;
    SSMStatus          rv;
    char              *htmlTemplate = NULL;

    certList = CERT_NewCertList();
    certList = CERT_CreateNicknameCertList(certList, 
                                           cx->super.m_connection->m_certdb,
                                           certNickname, PR_Now(), PR_FALSE);

    if (certList == NULL) {
        certList = PK11_FindCertsFromNickname(certNickname, &cx->super);
        if (certList == NULL) {
            SSM_DEBUG("Could not find a certificate with nick '%s' "
                      "in cert database\n", certNickname);
            goto loser;
        }
    }
    certListNode = CERT_LIST_HEAD(certList);
    while (!CERT_LIST_END(certListNode, certList)) {
        numcerts++;
        certListNode = CERT_LIST_NEXT(certListNode);
    }
    if (numcerts > 1) {
        char * formName = NULL, *params = NULL;
        rv = SSM_HTTPParamValue(req, "formName", &formName);
        if (rv != SSM_SUCCESS || !formName)
            SSM_DEBUG("AdvisorContext_FindCertByNickname:Can't get original form\n");
        params = PR_smprintf("origin=%s",formName);

        cx->m_nickname = PL_strdup(certNickname);
        rv = SSMControlConnection_SendUIEvent(cx->super.m_connection,
                                              "get", 
                                              "choose_cert", 
                                              &cx->super,
                                              params,
                                              &cx->super.m_clientContext,
                                              PR_TRUE);
        /* Now wait until we are notified by the handler that the user 
         * has selected a cert.
         */
        SSM_LockUIEvent(&cx->super);
        SSM_WaitUIEvent(&cx->super, PR_INTERVAL_NO_TIMEOUT);
        cert = (CERTCertificate*)cx->super.m_connection->super.super.m_uiData;

        if (cx->super.m_buttonType != SSM_BUTTON_CANCEL) {
            /* 
             * If we don't sleep for a bit here, we cause Communicator to crash
             * because it tries to re-use a Window that gets killed.  Guess
             * we're just too fast for Communicator.
             */
            PR_Sleep(PR_TicksPerSecond()*1);
        }
        PR_FREEIF(cx->m_nickname);
        cx->m_nickname = NULL;
        PR_FREEIF(params);
    } else {
        cert = CERT_FindCertByNickname(cx->super.m_connection->m_certdb,
                                       certNickname);
        cx->super.m_buttonType = SSM_BUTTON_OK;
    }
    CERT_DestroyCertList(certList);
    return cert;
 loser:
    PR_FREEIF(htmlTemplate);
    if (certList != NULL) {
        CERT_DestroyCertList(certList);
    }
    if (cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    if (textGenCx != NULL) {
        SSMTextGen_DestroyContext(textGenCx);
    }
    return NULL;
}

typedef struct SSMFindMineArgStr {
    CERTCertList *certList;
    SSMControlConnection *ctrl;
} SSMFindMineArg;

static SSMStatus
ssm_find_all_mine(PRIntn index, void *arg, void *key, void *itemdata)
{
    ssmCertData * data = (ssmCertData*)itemdata;
    SSMFindMineArg *findArg = (SSMFindMineArg*) arg;
    char *nick = (char*)key;
    SSMStatus rv = SSM_FAILURE;

    if (data->usage == clAllMine) {
        CERTCertList *tmpList;

        tmpList = CERT_CreateNicknameCertList(findArg->certList,
                                              findArg->ctrl->m_certdb,
                                              nick, PR_Now(), PR_FALSE);

        if (tmpList != NULL) {
            rv = SSM_SUCCESS;
        }
    }
    return rv;
}

SSMStatus
SSMSecurityAdvisorContext_BackupAllMineCerts(SSMSecurityAdvisorContext *cx, 
                                             HTTPRequest               *req)
{
    SSMFindMineArg arg;
    CERTCertList *certList=NULL;
    SSMPKCS12Context *p12Cxt=NULL;
    SSMPKCS12CreateArg p12Create;
    SSMResourceID rid;
    SSMStatus rv;
    CERTCertificate **certArr = NULL;
    int numCerts,i, finalCerts, currIndex;
    CERTCertListNode *node;
    PRIntn numNicks;
    const char *responseKey;

    certList = CERT_NewCertList();
    if (certList == NULL) {
        goto loser;
    }
    arg.certList = certList;
    arg.ctrl     = req->ctrlconn;
    numNicks = SSMSortedList_Enumerate(cx->m_certhash, ssm_find_all_mine, 
                                       &arg);
    if (numNicks <= 0){
        /* No certs to backup */
        SSM_HTTPReportError(req, HTTP_NO_CONTENT);
        goto loser;
    }
    certList = arg.certList;
    p12Create.isExportContext = PR_TRUE;
    rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_PKCS12_CONTEXT,
                                        (void*)&p12Create, req->ctrlconn,
                                        &rid, (SSMResource**)(&p12Cxt));
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    SSM_CopyCMTItem(&p12Cxt->super.m_clientContext, 
                    &cx->super.m_clientContext);
    
    numCerts = SSM_CertListCount(certList);
    certArr = SSM_NEW_ARRAY(CERTCertificate*,numCerts);
    if (certArr == NULL) {
        goto loser;
    }
    node = CERT_LIST_HEAD(certList);
    for (i=0, currIndex=0, finalCerts=numCerts; i<numCerts; i++) {
        if (node->cert->slot == NULL ||
            PK11_IsInternal(node->cert->slot)) {
            certArr[currIndex] = node->cert;
            currIndex++;
        } else {
            finalCerts--;
        }
        node = CERT_LIST_NEXT(node);
    }
    rv = SSMPKCS12Context_CreatePKCS12FileForMultipleCerts(p12Cxt,
                                                           PR_TRUE,
                                                           certArr,
                                                           finalCerts);
    PR_Free(certArr);
    certArr = NULL;
    CERT_DestroyCertList(certList);
    certList = NULL;
    if (rv == SSM_SUCCESS) {
        responseKey = (finalCerts > 1) ? "pkcs12_backup_multiple_success" :
                                         "pkcs12_backup_success";
    } else {
        if (p12Cxt->super.m_buttonType == SSM_BUTTON_CANCEL) {
            goto loser;
        } else {
            responseKey = SSMUI_GetPKCS12Error(rv, PR_TRUE);
        }
    }
    SSM_FreeResource(&p12Cxt->super);
    p12Cxt = NULL;
    if (SSMSecurityAdvisorContext_DoPKCS12Response(cx, req, responseKey)
        != SSM_SUCCESS) {
        goto loser;
    }
    return SSM_SUCCESS;
 loser:
    PR_FREEIF(certArr);
    if (certList != NULL) {
        CERT_DestroyCertList(certList);
    }
    if (p12Cxt != NULL) {
        SSM_FreeResource(&p12Cxt->super);
    }
    SSM_HTTPReportError(req, HTTP_NO_CONTENT);
    return SSM_FAILURE;
}
SSMStatus SSMSecurityAdvisorContext_DoPKCS12Backup(
                                              SSMSecurityAdvisorContext *cx,
                                              HTTPRequest               *req)
{
    SSMStatus rv;
    char *certNickname;
    const char *responseKey;
    SSMPKCS12CreateArg p12Create;
    SSMResourceID rid;
    SSMPKCS12Context *p12Cxt;

    p12Create.isExportContext = PR_TRUE;
    rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_PKCS12_CONTEXT, 
                                        (void*)&p12Create,
                                        SSMRESOURCE(cx)->m_connection,
                                        &rid, (SSMResource **)(&p12Cxt));
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    /* pass along Advisor's client context for window management */
    SSM_CopyCMTItem(&((SSMResource *)p12Cxt)->m_clientContext,
                    &((SSMResource *)cx)->m_clientContext);

    rv = SSM_HTTPParamValue(req, "selectCert", &certNickname);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    
    p12Cxt->m_cert = 
        SSMSecurityAdvisorContext_FindCertByNickname(cx, req, certNickname);

    if (cx->super.m_buttonType == SSM_BUTTON_CANCEL) {
        goto loser;
    }
    if (p12Cxt->m_cert == NULL) {
        goto loser;
    }

    /* p12Cxt->super.m_clientContext = cx->super.m_clientContext; */
    rv = SSMPKCS12Context_CreatePKCS12File(p12Cxt, PR_TRUE);
    if (rv == SSM_SUCCESS) {
        responseKey = "pkcs12_backup_success";
    } else {
        if (p12Cxt->super.m_buttonType == SSM_BUTTON_CANCEL) {
            goto loser;
        } else {
            responseKey = SSMUI_GetPKCS12Error(rv, PR_TRUE);
        }
    }
    if (SSMSecurityAdvisorContext_DoPKCS12Response(cx, req, responseKey)
        != SSM_SUCCESS) {
        goto loser;
    }
    SSM_FreeResource(&p12Cxt->super);
    return SSM_SUCCESS;
 loser:
    if (p12Cxt != NULL) {
        SSM_FreeResource(&p12Cxt->super);
    }
    SSM_HTTPReportError(req, HTTP_NO_CONTENT);
    return SSM_FAILURE;
}

char *
ssm_packb64_name(char *b64Name)
{
    char *htmlString = NULL;
    int numPercentSigns = 0;
    char *cursor, *retString;
    int i, newLen, origHTMLStrLen; 

    htmlString = SSM_ConvertStringToHTMLString(b64Name);
    /*
     * Now let's see if there are any '%' characters that need
     * to be escaped so that printf statements succeed.
     */
    cursor = htmlString;
    while ((cursor = PL_strchr(cursor, '%')) != NULL) {
        numPercentSigns++;
        cursor++;
    }
    if (numPercentSigns == 0) {
        htmlString;
    }
    origHTMLStrLen = PL_strlen(htmlString);
    newLen = origHTMLStrLen + numPercentSigns + 1;
    retString = SSM_NEW_ARRAY(char, newLen);
    for (i=0,cursor=retString; i<origHTMLStrLen+1; i++,cursor++) {
        if (htmlString[i] == '%') {
            char *dollarSign, *placeHolder;
            /*
             * Let's see if this a urlencoded escape or a printf parameter.
             */
            placeHolder = &htmlString[i];
            dollarSign = PL_strchr(placeHolder, '$');
            if (dollarSign && ((dollarSign - placeHolder) < 2)) {
                /*
                 * OK, this is a numbered parameter for printf
                 */
                *cursor = htmlString[i];
            } else {
                /*
                 * This is an escape for url encoding.  Escape it so printf
                 * doesn't blow up.
                 */
                *cursor = '%';
                cursor++;
                *cursor = '%';
            }
        } else {
            *cursor = htmlString[i];
        }
    }
    PR_Free(htmlString);
    return retString;
}

SSMStatus
SSMSecurityAdvisorContext_ProcessCRLDialog (HTTPRequest *req)
{
    SSMHTTPParamMultValues crlNames={NULL, NULL, 0};
    CERTSignedCrl *realCrl;
    SECItem crlDERName;
    PRBool flushSSLCache = PR_FALSE;
    SSMStatus rv;
    SECStatus srv;
    int i, type;

    rv = SSM_HTTPParamValueMultiple(req, "crlNames", &crlNames);
    if (rv != SSM_SUCCESS || crlNames.numValues == 0) {
        goto loser;
    }
    memset (&crlDERName, 0, sizeof(SECItem));
    for (i=0; i<crlNames.numValues; i++) {
        /*
         * The first character in the value string represents the type,
         * either 1 (SEC_CRL_TYPE) or 0 (SEC_KRL_TYPE)
         */
        srv = ATOB_ConvertAsciiToItem(&crlDERName, crlNames.values[i]+1);
        if (srv != SECSuccess) {
            goto loser;
        }
        type = (crlNames.values[i][0] == '1') ? SEC_CRL_TYPE : SEC_KRL_TYPE;
        realCrl = SEC_FindCrlByName(req->ctrlconn->m_certdb, 
                                    &crlDERName, type);
        SECITEM_FreeItem(&crlDERName, PR_FALSE);
        if (realCrl) {
            SEC_DeletePermCRL(realCrl);
            SEC_DestroyCrl(realCrl);
            flushSSLCache = PR_TRUE;            
        }
    }
    if (flushSSLCache) {
        SSL_ClearSessionCache();
    }
    if (!SSM_IsCRLPresent(req->ctrlconn)) {
        /*
         * In this case, there are no more CRLs in the database,
         * so we'll replace the baseRef with one that will cause
         * the security advisor to refresh itself and elminate the
         * "Delete CRLs" button.
         */
        for (i=0; i<req->numParams; i++) {
            char *crlCloseKey = "crlclose_doclose_js";
            if (PL_strcmp(req->paramNames[i], "baseRef") == 0) {
                memcpy (req->paramValues[i], crlCloseKey, 
                        PL_strlen(crlCloseKey)+1);
                break;
            }
        }
    }

    if (SSM_HTTPDefaultCommandHandler(req) != SSM_SUCCESS) {
        goto loser;
    }
    PR_FREEIF(crlNames.values);
    return SSM_SUCCESS;
 loser:
    PR_FREEIF(crlNames.values);
    return SSM_FAILURE;
}

SSMStatus SSMSecurityAdvisorContext_Process_cert_mine_form(
                                                SSMSecurityAdvisorContext *res,
                                                HTTPRequest *req)
{
    SSMStatus  rv= SSM_FAILURE;
    char      *button;
    
    /* Figure out which one of the buttons on the form was pressed. */
    if (SSM_HTTPParamValue(req, "backup", &button) == SSM_SUCCESS) {
      if (button != NULL) {
        rv = SSMSecurityAdvisorContext_DoPKCS12Backup(res, req);
      }
    } else if (SSM_HTTPParamValue(req, "restore", &button) == SSM_SUCCESS) {
      if (button != NULL) {
        rv = SSMSecurityAdvisorContext_DoPKCS12Restore(res, req);
      }
    } else if (SSM_HTTPParamValue(req, "delete", &button) == SSM_SUCCESS) {
        if (button != NULL) {
            rv = SSM_ProcessCertDeleteButton(req);
        }
    } else if (SSM_HTTPParamValue(req, "password", &button) == SSM_SUCCESS) {
        if (button != NULL) {
            rv = SSM_ProcessPasswordWindow(req);
        }
    } else if (SSM_HTTPParamValue(req, "ldap", &button) == SSM_SUCCESS) {
        if (button != NULL) {
            rv = SSM_ProcessLDAPWindow(req);
        }
    } else if (SSM_HTTPParamValue(req, "backup_all", &button) == SSM_SUCCESS) {
        if (button != NULL) {
            rv = SSMSecurityAdvisorContext_BackupAllMineCerts(res, req);
        }
    } else if (SSM_HTTPParamValue(req, "crlButton", &button) == SSM_SUCCESS) {
        if (button != NULL) {
	    rv = SSM_HTTPReportError(req, HTTP_NO_CONTENT);
	}
    }
    return rv;
}

static SSMStatus
SSMSecurityAdvisorContext_SetConfigOCSP(SSMSecurityAdvisorContext *cx, 
                                        HTTPRequest               *req)
{
    char *responderURL = NULL, *caNickname = NULL;
    char *enableOCSP = NULL;
    CERTCertDBHandle  *db;
    SSMStatus rv;
    SECStatus srv;

    db = cx->super.m_connection->m_certdb;
    rv = SSM_HTTPParamValue(req, "enableOCSP", &enableOCSP);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (!strcmp(enableOCSP,"noOCSP")) {
        CERT_DisableOCSPChecking(db);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.enabled", 
                                          PR_FALSE);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.useDefaultResponder", 
                                          PR_FALSE);
        CERT_DisableOCSPChecking(db);
        CERT_DisableOCSPDefaultResponder(db);
    } else if (!strcmp(enableOCSP,"noDefaultResponder")) {
        srv = CERT_EnableOCSPChecking(db);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.enabled", 
                                          PR_TRUE);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.useDefaultResponder", 
                                          PR_FALSE);
        if (srv != SECSuccess) {
            goto loser;
        }
        CERT_DisableOCSPDefaultResponder(db);
    } else if (!strcmp(enableOCSP,"useDefaultResponder")) {
        srv = CERT_EnableOCSPChecking(db);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.enabled", 
                                          PR_TRUE);
        SSMControlConnection_SaveBoolPref(req->ctrlconn, 
                                          "security.OCSP.useDefaultResponder", 
                                          PR_TRUE);
        if (srv != SECSuccess) {
            goto loser;
        }
        rv = SSM_HTTPParamValue(req, "ocspURL", &responderURL);
        if (rv != SSM_SUCCESS) {
            goto loser;
        }
        SSMControlConnection_SaveStringPref(req->ctrlconn, 
                                            "security.OCSP.URL", 
                                            responderURL);
        rv = SSM_HTTPParamValue(req, "selectCert", &caNickname);
        if (rv != SSM_SUCCESS) {
            goto loser;
        }
        SSMControlConnection_SaveStringPref(req->ctrlconn, 
                                            "security.OCSP.signingCA", 
                                            caNickname);
        srv = CERT_SetOCSPDefaultResponder(db, responderURL, caNickname);
        if (srv != SECSuccess) {
            goto loser;
        }
        srv = CERT_EnableOCSPDefaultResponder(db);
        if (srv != SECSuccess) {
            goto loser;
        }
    } else {
        goto loser;
    }
    return SSM_SUCCESS;

 loser:
    return SSM_FAILURE;
}

static SSMStatus
SSMSecurityAdvisorContext_ProcessOCSPForm(SSMSecurityAdvisorContext *cx, 
                                          HTTPRequest               *req)
{
    SSMStatus rv = SSM_SUCCESS;
    /*
     * First, if the Cancel button was pressed, then don't 
     * process the form.
     */
    if (cx->super.m_buttonType == SSM_BUTTON_OK) {
        rv = SSMSecurityAdvisorContext_SetConfigOCSP(cx, req);
    }
    SSM_HTTPDefaultCommandHandler(req);
    return rv;
}

SSMStatus SSMSecurityAdvisorContext_FormSubmitHandler(SSMResource *res,
                                                      HTTPRequest *req)
{
    SSMStatus  rv;
    char      *formName;

    if (!SSM_IsAKindOf(res, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        return SSM_FAILURE;
    }

    /* First figure out which form we're processing. */
    rv = SSM_HTTPParamValue(req, "formName", &formName);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    if (PL_strcmp(formName, "prefs_submit_form") == 0) {
        /* save pref changes and close the Security Advisor */
        rv = SSMSecurityAdvisorContext_SavePrefs
            ((SSMSecurityAdvisorContext*)res, req);
    }
    else if (!strcmp(formName, "cert_mine_form") ||
             !strcmp(formName, "cert_others_form") ||
             !strcmp(formName, "cert_websites_form") ||
             !strcmp(formName, "cert_authorities_form")) {
      rv = SSMSecurityAdvisorContext_Process_cert_mine_form
        ((SSMSecurityAdvisorContext*)res, req);
    } else if (!strcmp(formName, "choose_cert_by_usage")) {
      rv = SSM_ChooseCertUsageHandler(req);
    } else if (!strcmp(formName, "set_db_password")) {
      rv = SSM_SetDBPasswordHandler(req);
    } else if (!strcmp(formName, "configureOCSPForm")){
      rv = SSMSecurityAdvisorContext_ProcessOCSPForm
                                       ((SSMSecurityAdvisorContext*)res, req);
    } else if (!strcmp(formName, "crlDialog")){
        rv = SSMSecurityAdvisorContext_ProcessCRLDialog(req);
    }else {
      rv = SSM_ERR_BAD_REQUEST; 
      SSM_HTTPReportSpecificError(req, "Do not know how to process form %s",
                                  formName);
    }
  loser:
    return rv;
}

SSMStatus
SSMSecurityAdvisorContext_Print(SSMResource *res,
                                char *fmt, PRIntn numParam,
                                char **value, char **resultStr)
{
    SSMSecurityAdvisorContext *cx = (SSMSecurityAdvisorContext*)res;
    SSMStatus rv;

    PR_ASSERT(fmt != NULL && resultStr != NULL);
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        return PR_FAILURE;
    }
    /* We don't use the extra parameters */
    if (cx->m_nickname != NULL) {
        *resultStr = PR_smprintf(fmt, res->m_id, "backup", cx->m_nickname, *value);
        rv = (*resultStr == NULL) ? PR_FAILURE : PR_SUCCESS;
    } else {
        rv = SSMResource_Print(res, fmt, numParam, value, resultStr);
    }
    return rv;
}

SSMStatus SSM_SetSelectedItemInfo(SSMSecurityAdvisorContext* cx)
{
	SSMStatus rv = SSM_SUCCESS;

    switch (cx->infoContext)
    {
        case SSM_NOINFO:
            cx->selectedItemPage = SSM_NO_INFO;
            break;
        case SSM_COMPOSE:
            break;
		case SSM_SNEWS_MESSAGE:
		case SSM_NEWS_MESSAGE:
        case SSM_MAIL_MESSAGE:
            cx->selectedItemPage = SSM_MESSAGE;
			if (cx->encryptedP7CInfo) {
			    /* Get the P7 Content info resource */
				rv = SSMControlConnection_GetResource(SSMRESOURCE(cx)->m_connection, (SSMResourceID)cx->encryptedP7CInfo,
					  (SSMResource**)&cx->encryptedP7CInfoRes);
				if ((rv != PR_SUCCESS) || (cx->encryptedP7CInfoRes == NULL)) {
					goto loser;
				}
			}
			if (cx->signedP7CInfo) {
			    /* Get the P7 Content info resource */
				rv = SSMControlConnection_GetResource(SSMRESOURCE(cx)->m_connection, (SSMResourceID)cx->signedP7CInfo,
					  (SSMResource**)&cx->signedP7CInfoRes);
				if ((rv != PR_SUCCESS) || (cx->signedP7CInfoRes == NULL)) {
					goto loser;
				}
			}

			if (!cx->encryptedP7CInfo &&
				!cx->signedP7CInfo &&
				cx->verifyError &&
				!cx->decodeError) {
				/* Somehow we have the error code backwards */
				cx->decodeError = cx->verifyError;
				cx->verifyError = 0;
			}

			cx->encrypted_b = (cx->decodeError ||
								(cx->encryptedP7CInfo &&
								SEC_PKCS7ContentIsEncrypted(cx->encryptedP7CInfoRes->m_cinfo)) ||
								(cx->signedP7CInfo &&
								SEC_PKCS7ContentIsEncrypted(cx->signedP7CInfoRes->m_cinfo)));
			cx->signed_b = (cx->verifyError ||
								(cx->encryptedP7CInfo &&
								SEC_PKCS7ContentIsSigned(cx->encryptedP7CInfoRes->m_cinfo)) ||
								(cx->signedP7CInfo &&
								SEC_PKCS7ContentIsSigned(cx->signedP7CInfoRes->m_cinfo)));
            break;
        case SSM_BROWSER:
            if (cx->resID == 0) {
                cx->selectedItemPage = SSM_NAVIGATOR_NO_SEC;
            } else {
                cx->selectedItemPage = SSM_NAVIGATOR_SSL;
            }
            break;
        default:
            cx->selectedItemPage = SSM_NO_INFO;
            break;
    }
	return rv;

loser:
	return SSM_FAILURE;
}

SSMStatus sa_noinfo(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *target = NULL;
    SSMSecurityAdvisorContext* res = NULL;
	char *fmt = NULL;

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSecurityAdvisorContext*)target;

	rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_noinfo", &fmt);
	if (rv != SSM_SUCCESS) {
		goto loser;
	}
        PR_FREEIF(cx->m_result);
	cx->m_result = fmt;

	return SSM_SUCCESS;
loser:
	return SSM_FAILURE;
}

SSMStatus sa_navigator(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *target = NULL;
    SSMSecurityAdvisorContext* res = NULL;
	char *fmt = NULL;
	SSMSSLSocketStatus *socketStatusRes = NULL;
	char * encryption_level = NULL;
	char * serverCN = NULL;
	char * issuerName = NULL;
	CERTCertificate *issuerCert = NULL;
	SSMResourceCert *serverCertRes = NULL, *issuerCertRes = NULL;
	int serverCertResID, issuerCertResID;

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSecurityAdvisorContext*)target;

	if (res->resID == 0) {
		rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_no_sec", &fmt);
		if (rv != SSM_SUCCESS) {
			goto loser;
		}
                PR_FREEIF(cx->m_result);
		cx->m_result = PR_smprintf(fmt, res->hostname, res->hostname);
		PR_Free(fmt);
		return SSM_SUCCESS;
	} else {
	    /* Get the socket status resource */
		rv = SSMControlConnection_GetResource(SSMRESOURCE(res)->m_connection, (SSMResourceID)res->resID,
						  (SSMResource**)&socketStatusRes);
		if ((rv != PR_SUCCESS) || (socketStatusRes == NULL)) {
			goto loser;
		}
        /*
         * We inherit the client's reference here.
         */
        res->socketStatus = socketStatusRes;
		/* Do we have an error */
		if (!socketStatusRes->m_error) {
			rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_ssl", &fmt);
			if (rv != SSM_SUCCESS) {
				goto loser;
			}
#if 0
			/* Create a resource for this cert */
			rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
									socketStatusRes->m_cert,
									SSMRESOURCE(res)->m_connection,
									(long *) &serverCertResID,
									(SSMResource**)&serverCertRes);
			if (rv != PR_SUCCESS) {
				goto loser;
			}
#else
            serverCertResID = socketStatusRes->m_cert->super.m_id;
            serverCertRes = socketStatusRes->m_cert;
#endif
			issuerName = CERT_NameToAscii(&socketStatusRes->m_cert->cert->issuer);

			if (socketStatusRes->m_level == SSL_SECURITY_STATUS_ON_HIGH) {
				SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
			} else {
				SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
			}
            PR_FREEIF(cx->m_result);
			cx->m_result = PR_smprintf(fmt, res->hostname, issuerName, target->m_id, serverCertResID,
										encryption_level, socketStatusRes->m_cipherName,
										socketStatusRes->m_secretKeySize);
			PR_Free(issuerName);
			PR_Free(encryption_level);
			PR_Free(fmt);
            SSM_FreeResource(&socketStatusRes->super);
			return SSM_SUCCESS;
		} else {
			if (socketStatusRes->m_error == SEC_ERROR_UNKNOWN_ISSUER ||
				socketStatusRes->m_error == SEC_ERROR_CA_CERT_INVALID ) {
				rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_ssl_unknown_issuer", &fmt);
				if (rv != SSM_SUCCESS) {
					goto loser;
				}

				/* Get the common name of the issuer */
				issuerName = CERT_NameToAscii(&socketStatusRes->m_cert->cert->issuer);
				if (!issuerName) {
					goto loser;
				}

				/* Get the common name of the server cert */
				serverCN = CERT_GetCommonName(&socketStatusRes->m_cert->cert->subject);
				if (!serverCN) {
					goto loser;
				}
#if 0
				/* Create resource for the server cert */
				rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
									socketStatusRes->m_cert,
									SSMRESOURCE(res)->m_connection,
									(long *) &serverCertResID,
									(SSMResource**)&serverCertRes);
				if (rv != PR_SUCCESS) {
					goto loser;
				}
#else 
                serverCertRes = socketStatusRes->m_cert;
                serverCertResID = serverCertRes->super.m_id;
#endif
				if (socketStatusRes->m_level == SSL_SECURITY_STATUS_ON_HIGH) {
					SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
				} else {
					SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
				}
                                PR_FREEIF(cx->m_result);
				cx->m_result = PR_smprintf(fmt, res->hostname, issuerName, target->m_id, serverCertResID,
											encryption_level, socketStatusRes->m_cipherName,
											socketStatusRes->m_secretKeySize);
				PR_Free(fmt);
				PR_Free(issuerName);
				PR_Free(serverCN);
				PR_Free(encryption_level);
                SSM_FreeResource(&socketStatusRes->super);
				return SSM_SUCCESS;
			} else if(socketStatusRes->m_error == SEC_ERROR_UNTRUSTED_ISSUER) {
				rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_ssl_bad_issuer", &fmt);
				if (rv != SSM_SUCCESS) {
					goto loser;
				}

				/* Get the common name of the issuer */
				issuerName = CERT_NameToAscii(&socketStatusRes->m_cert->cert->issuer);
				if (!issuerName) {
					goto loser;
				}

				/* Get the common name of the server cert */
				serverCN = CERT_GetCommonName(&socketStatusRes->m_cert->cert->subject);
				if (!serverCN) {
					goto loser;
				}
#if 0
				/* Create resource for the server cert */
				rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
									socketStatusRes->m_cert,
									SSMRESOURCE(res)->m_connection,
									(long *) &serverCertResID,
									(SSMResource**)&serverCertRes);
				if (rv != PR_SUCCESS) {
					goto loser;
				}
#else 
                serverCertRes = socketStatusRes->m_cert;
                serverCertResID = serverCertRes->super.m_id;
#endif

				/* Create a resource for the issuer cert (if it exists) */
				issuerCert = CERT_FindCertIssuer(socketStatusRes->m_cert->cert,
                                                 PR_Now(), certUsageAnyCA);
				if (issuerCert) {
					/* Create resource for the issuer cert */
					rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
										issuerCert,
										SSMRESOURCE(res)->m_connection,
										(long *) &issuerCertResID,
										(SSMResource**)&issuerCertRes);
					if (rv != PR_SUCCESS) {
						goto loser;
					}
				} else {
					issuerCertResID = 0;
				}

				if (socketStatusRes->m_level == SSL_SECURITY_STATUS_ON_HIGH) {
					SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
				} else {
					SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
				}
                                PR_FREEIF(cx->m_result);
				cx->m_result = PR_smprintf(fmt, res->hostname, issuerName, target->m_id, serverCertResID,
											issuerCertResID, encryption_level, socketStatusRes->m_cipherName,
											socketStatusRes->m_secretKeySize);
				PR_Free(fmt);
				PR_Free(issuerName);
				PR_Free(serverCN);
				PR_Free(encryption_level);
                SSM_FreeResource(&socketStatusRes->super);
				return SSM_SUCCESS;
			} else if (socketStatusRes->m_error == SSL_ERROR_BAD_CERT_DOMAIN) {
					rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_ssl_bad_cert_domain", &fmt);
					if (rv != SSM_SUCCESS) {
						goto loser;
					}

					/* Get the common name of the server cert */
					serverCN = CERT_GetCommonName(&socketStatusRes->m_cert->cert->subject);
					if (!serverCN) {
						goto loser;
					}

					if (socketStatusRes->m_level == SSL_SECURITY_STATUS_ON_HIGH) {
						SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
					} else {
						SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
					}
                    PR_FREEIF(cx->m_result);
					cx->m_result = PR_smprintf(fmt, res->hostname, serverCN, encryption_level, socketStatusRes->m_cipherName,
												socketStatusRes->m_secretKeySize);

					PR_Free(fmt);
					PR_Free(serverCN);
					PR_Free(encryption_level);
                    SSM_FreeResource(&socketStatusRes->super);
					return SSM_SUCCESS;
			} else {
				rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_navigator_ssl_unknown_error", &fmt);
				if (rv != SSM_SUCCESS) {
					goto loser;
				}
#if 0
				/* Create resource for the server cert */
				rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
									socketStatusRes->m_cert,
									SSMRESOURCE(res)->m_connection,
									(long *) &serverCertResID,
									(SSMResource**)&serverCertRes);
				if (rv != PR_SUCCESS) {
					goto loser;
				}
#else
                serverCertRes = socketStatusRes->m_cert;
                serverCertResID = serverCertRes->super.m_id;
#endif

				if (socketStatusRes->m_level == SSL_SECURITY_STATUS_ON_HIGH) {
					SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
				} else {
					SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
				}
                PR_FREEIF(cx->m_result);
				cx->m_result = PR_smprintf(fmt, res->hostname, target->m_id, serverCertResID, encryption_level, socketStatusRes->m_cipherName,
											socketStatusRes->m_secretKeySize);
				PR_Free(fmt);
                SSM_FreeResource(&socketStatusRes->super);
				return SSM_SUCCESS;
			}
		}
	}
loser:
	PR_FREEIF(fmt);
	PR_FREEIF(serverCN);
	PR_FREEIF(issuerName);
    if (socketStatusRes) {
        SSM_FreeResource(&socketStatusRes->super);
    }
	return SSM_FAILURE;
}

static CERTCertificate * get_signer_cert(SSMSecurityAdvisorContext *res)
{
	CERTCertificate * cert = NULL;

	/* Get the signing cert */
	if (res->signedP7CInfoRes ||
		res->encryptedP7CInfoRes) {
		SEC_PKCS7SignerInfo **signerinfos;
		SEC_PKCS7ContentInfo *ci = res->signedP7CInfoRes->m_cinfo;
		if (!ci) ci = res->encryptedP7CInfoRes->m_cinfo;

		/* Finding the signers cert */
		switch(ci->contentTypeTag->offset) {
			default:
			case SEC_OID_PKCS7_DATA:
			case SEC_OID_PKCS7_DIGESTED_DATA:
			case SEC_OID_PKCS7_ENVELOPED_DATA:
			case SEC_OID_PKCS7_ENCRYPTED_DATA:
			/* Could only get here if SEC_PKCS7ContentIsSigned
			* is broken. */
			{
				PORT_Assert (0);
				cert=NULL;
			}
			break;
			case SEC_OID_PKCS7_SIGNED_DATA:
			{
				SEC_PKCS7SignedData *sdp;
				sdp = ci->content.signedData;
				signerinfos = sdp->signerInfos;
				cert = signerinfos[0]->cert;
			}
			break;
			case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
			{
				SEC_PKCS7SignedAndEnvelopedData *saedp;
				saedp = ci->content.signedAndEnvelopedData;
				signerinfos = saedp->signerInfos;
				cert = signerinfos[0]->cert;
			}
			break;
		} /* finding the signer cert */
	}
	return cert;
}

char*
SSM_GetOCSPURL(CERTCertificate *cert, PrefSet *prefs)
{
    SSMStatus rv;
    PRBool boolval = PR_FALSE;
    char *responderURL = NULL;

    /* Is there a default responder installed */
    rv = PREF_GetBoolPref(prefs, "security.OCSP.useDefaultResponder", &boolval);
    if (boolval) {
        PREF_CopyStringPref(prefs, "security.OCSP.URL", &responderURL);
    } else {
	responderURL = CERT_GetOCSPAuthorityInfoAccessLocation(cert);
    }
    return responderURL;
}

static CERTCertificate * get_encryption_cert(SSMSecurityAdvisorContext *res)
{
	return NULL;
}

static char *
sa_get_algorithm_string(SEC_PKCS7ContentInfo *cinfo)
{
	SECAlgorithmID *algid;
	SECOidTag algtag;
	const char *alg_name;
	int key_size;
	if (!cinfo) return 0;
	algid = SEC_PKCS7GetEncryptionAlgorithm(cinfo);
	if (!algid) return 0;
	algtag = SECOID_GetAlgorithmTag(algid);
	alg_name = SECOID_FindOIDTagDescription(algtag);

	key_size = SEC_PKCS7GetKeyLength(cinfo);

	if (!alg_name || !*alg_name)
		return 0;
	else if (key_size > 0)
		return PR_smprintf("%d-bits %s",
			       key_size, alg_name);
	else
		return PL_strdup(alg_name);
}

PRBool
SSM_IsOCSPEnabled(SSMControlConnection *connection) 
{
    SSMStatus rv;
    PRBool isOCSPEnabled = PR_FALSE;

    rv = PREF_GetBoolPref(connection->m_prefs, "security.OCSP.enabled",
                          &isOCSPEnabled);
    return (rv == SSM_SUCCESS) ? isOCSPEnabled : PR_FALSE; 
}

char *
SSM_GetGenericOCSPWarning(SSMControlConnection *ctrl,
                          CERTCertificate *cert)
{
    char *retString = NULL;
    char *responderURL = NULL;
    SSMTextGenContext *cx = NULL;
    SSMStatus rv;

    retString = PL_strdup("");
    if (SSM_IsOCSPEnabled(ctrl)) {
        responderURL = SSM_GetOCSPURL(cert, ctrl->m_prefs);
        if (responderURL == NULL) {
            goto done;
        }
        rv = SSMTextGen_NewTopLevelContext(NULL, &cx);
        if (rv != SSM_SUCCESS) {
            goto done;
        }
        SSM_GetAndExpandTextKeyedByString(cx, "ocsp_fail_message_generic",
                                          &retString);
    } 
 done:
    PR_FREEIF(responderURL);
    if (cx) {
        SSMTextGen_DestroyContext(cx);
    }
    return retString;
}

SSMStatus sa_message(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *target = NULL;
    SSMSecurityAdvisorContext* res = NULL;
	char *fmt = NULL, *fmtSigned = NULL, *fmtEncrypted = NULL;
    char *genericOCSPWarning = NULL;

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSecurityAdvisorContext*)target;

	/* Deal with the signed part first */
	if (!res->signed_b) {
		rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_not_signed", &fmtSigned);
		if (rv != SSM_SUCCESS) {
			goto loser;
		}
	} else {
		if (res->verifyError == 0) {
			char *signer_email;
			CERTCertificate *signerCert = NULL;
			SSMResourceCert *signerCertRes = NULL;
			int signerCertResID;

			rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed", &fmt);
			if (rv != SSM_SUCCESS) {
				goto loser;
			}

			signerCert = get_signer_cert(res);
			if (!signerCert) {
				goto loser;
			}

			/* Get the signers email address */
			if (res->signedP7CInfoRes) {
				signer_email = SEC_PKCS7GetSignerEmailAddress(res->signedP7CInfoRes->m_cinfo);
			}

			if (!signer_email && res->encryptedP7CInfoRes) {
				signer_email = SEC_PKCS7GetSignerEmailAddress(res->encryptedP7CInfoRes->m_cinfo);
			}

			/* Create a cert resource for this certificate */
		    rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
			                        signerCert,
				                    SSMRESOURCE(res)->m_connection,
					                (long *) &signerCertResID,
						            (SSMResource**)&signerCertRes);
			if (rv != PR_SUCCESS) {
	            goto loser;
		    }

			fmtSigned = PR_smprintf(fmt, signer_email, target->m_id, signerCertResID);
			PR_Free(fmt);
		} else {
            CERTCertificate *signerCert;

            /* Get the signing certificate */
            signerCert = get_signer_cert(res);
            if (!signerCert) {
                goto loser;
            }

            genericOCSPWarning = 
                SSM_GetGenericOCSPWarning(target->m_connection,
                                          signerCert);
			switch(res->verifyError) {
				case SEC_ERROR_PKCS7_BAD_SIGNATURE:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_bad_signature", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
                        fmtSigned = PR_smprintf(fmt, genericOCSPWarning);
                        PR_FREEIF(fmt);
					}
					break;

				/* This case handles both expired and not yet valid certs */
				case SEC_ERROR_EXPIRED_CERTIFICATE:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_expired_signing_cert", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
                        fmtSigned = PR_smprintf(fmt, genericOCSPWarning);
                        PR_FREEIF(fmt);
					}
					break;

				case SEC_ERROR_REVOKED_CERTIFICATE:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_revoked_signing_cert", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
                        fmtSigned = PR_smprintf(fmt, genericOCSPWarning);
                        PR_FREEIF(fmt);
					}
					break;

				case SEC_ERROR_UNKNOWN_ISSUER:
					{
						SSMResourceCert *signerCertRes;
						PRUint32 signerCertResID;
						char *fmt;

						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_unknown_issuer", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						/* Create a cert resource for this certificate */
						rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
												signerCert,
												SSMRESOURCE(res)->m_connection,
												(long *) &signerCertResID,
												(SSMResource**)&signerCertRes);
						if (rv != PR_SUCCESS) {
							goto loser;
						}
						fmtSigned = PR_smprintf(fmt, target->m_id, 
                                                signerCertResID, 
                                                genericOCSPWarning);
						PR_Free(fmt);
					}
					break;

				case SEC_ERROR_CA_CERT_INVALID:
				case SEC_ERROR_UNTRUSTED_ISSUER:
					{
						CERTCertificate *issuerCert;
						SSMResourceCert * signerCertRes, issuerCertRes;
						PRInt32 signerCertResID, issuerCertResID;
						char *fmt = NULL;

						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_untrusted_issuer", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						/* Get the isser cert */
						issuerCert = CERT_FindCertIssuer(signerCert, PR_Now(), certUsageAnyCA);
						if (!issuerCert) {
							goto loser;
						}

						/* Create resources for these certs */
						rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
												signerCert,
												SSMRESOURCE(res)->m_connection,
												(long *) &signerCertResID,
												(SSMResource**)&signerCertRes);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
												issuerCert,
												SSMRESOURCE(res)->m_connection,
												(long *) &issuerCertResID,
												(SSMResource**)&issuerCertRes);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						fmtSigned = PR_smprintf(fmt, target->m_id, 
                                                signerCertResID, 
                                                issuerCertResID,
                                                genericOCSPWarning);
						PR_Free(fmt);
					}
					break;

				/* This case handles both expired and not yet valid certs */
				case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_expired_issuer_cert", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
                        fmtSigned = PR_smprintf(fmt, genericOCSPWarning);
                        PR_FREEIF(fmt);
					}
					break;

				/* Cert address mismatch */
				case SEC_ERROR_CERT_ADDR_MISMATCH:
					{
						char * signer_email = NULL;
						char * signerCN = NULL;
						CERTCertificate *signerCert = NULL;
						SSMResourceCert *signerCertRes = NULL;
						PRInt32 signerCertResID;
						SECItem * item = NULL;
						char *signTime = NULL;

						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_addr_mismatch", &fmt);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						/* Get the signer cert */
						signerCert = get_signer_cert(res);
						if (!signerCert) {
							goto loser;
						}

						/* Get the signer common name */
						signerCN = CERT_GetCommonName(&signerCert->subject);

						/* Get the signing time */
						item = SEC_PKCS7GetSigningTime(res->signedP7CInfoRes->m_cinfo);
						signTime = (item ? DER_UTCTimeToAscii(item) : 0);

						/* Create resources for these certs */
						rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
												signerCert,
												SSMRESOURCE(res)->m_connection,
												(long *) &signerCertResID,
												(SSMResource**)&signerCertRes);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}

						/* Get the signers email address */
						if (res->signedP7CInfoRes) {
							signer_email = SEC_PKCS7GetSignerEmailAddress(res->signedP7CInfoRes->m_cinfo);
						}

						if (!signer_email && res->encryptedP7CInfoRes) {
							signer_email = SEC_PKCS7GetSignerEmailAddress(res->encryptedP7CInfoRes->m_cinfo);
						}

						fmtSigned = PR_smprintf(fmt,res->senderAddr,signer_email,signerCN,signTime, target->m_id, signerCertResID);
						PR_Free(fmt);
						PR_FREEIF(signer_email);
						PR_FREEIF(signTime);
					}
					break;
				default:
					{
						CERTCertificate *signerCert = NULL;
						SSMStatus rv = SSM_SUCCESS;
						PrefSet* prefs = NULL;
						char *responderURL = NULL;

						prefs = res->super.m_connection->m_prefs;

						if (SSM_IsOCSPEnabled(res->super.m_connection)) {
						    
						        responderURL = SSM_GetOCSPURL(get_signer_cert(res), prefs);
							rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_ocsp_error", &fmt);
							if (rv != SSM_SUCCESS) {
								goto loser;
							}
							fmtSigned = PR_smprintf(fmt,responderURL,res->verifyError);
							PR_Free(fmt);
							PR_Free(responderURL);
						} else {
							rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_signed_unknown_error", &fmtSigned);
							if (rv != SSM_SUCCESS) {
								goto loser;
							}
						}
					}
					break;
				/* XXX Missing the case where the issuer cert has been revoked XXX */
			}
		}
	}
    PR_FREEIF(genericOCSPWarning);
	/* Now deal with the encrypted part */
	if (!res->encrypted_b) {
		rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_not_encrypted", &fmtEncrypted);
		if (rv != SSM_SUCCESS) {
			goto loser;
		}
	} else {
		if (res->decodeError == 0) {
			SECAlgorithmID *algid;
			SECOidTag algtag;
			const char *alg_name;
			char *encryption_level;
			int key_size;

			rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_encrypted", &fmt);
			if (rv != SSM_SUCCESS) {
				goto loser;
			}
            if (res->encryptedP7CInfoRes == NULL) {
			algid = SEC_PKCS7GetEncryptionAlgorithm(res->signedP7CInfoRes->m_cinfo);
            } else {
			algid = SEC_PKCS7GetEncryptionAlgorithm(res->encryptedP7CInfoRes->m_cinfo);
            }
			if (!algid) {
				goto loser;
			}

			algtag = SECOID_GetAlgorithmTag(algid);
			alg_name = SECOID_FindOIDTagDescription(algtag);
            if (res->encryptedP7CInfoRes) {
			key_size = SEC_PKCS7GetKeyLength(res->encryptedP7CInfoRes->m_cinfo);
            } else {
			key_size = SEC_PKCS7GetKeyLength(res->signedP7CInfoRes->m_cinfo);
            }
			if (key_size == 40) {
				SSM_GetUTF8Text(cx, "low_grade_encryption", &encryption_level);
			} else if (key_size == 56 || key_size == 64) {
				SSM_GetUTF8Text(cx, "medium_grade_encryption", &encryption_level);
			} else {
				SSM_GetUTF8Text(cx, "high_grade_encryption", &encryption_level);
			}

			fmtEncrypted = PR_smprintf(fmt, encryption_level, key_size, alg_name);
			PR_Free(fmt);
		} else {
			switch (res->decodeError) {
				case SEC_ERROR_NOT_A_RECIPIENT:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_encrypted_no_recipient", &fmtEncrypted);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
					}
					break;
				case SEC_ERROR_BAD_PASSWORD:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_encrypted_bad_password", &fmtEncrypted);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
					}
					break;
				/* XXX Missing cases for contents altered and encryption strength mismatch XXX */
				default:
					{
						rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_message_encrypted_unknown_error", &fmtEncrypted);
						if (rv != SSM_SUCCESS) {
							goto loser;
						}
					}
			}
		}
	}

	/* Output the stirngs */
	PR_FREEIF(cx->m_result);
	cx->m_result = PR_smprintf("%s%s", fmtSigned, fmtEncrypted);
	PR_Free(fmtSigned);
	PR_Free(fmtEncrypted);
	return SSM_SUCCESS;

loser:
	PR_FREEIF(fmt);
	PR_FREEIF(fmtSigned);
	PR_FREEIF(fmtEncrypted);
	return SSM_FAILURE;
}

SSMStatus sa_compose(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *target = NULL;
    SSMSecurityAdvisorContext* res = NULL;
	char *fmt = NULL, *fmtSigned = NULL, *fmtEncrypted = NULL;
	CERTCertificate *cert = NULL;
	char *certNickname = NULL;
	int err;
	char ** errCerts = NULL;
	int numErrCerts, i;

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSecurityAdvisorContext*)target;

	/* Get the default email certificate */
	rv = PREF_GetStringPref(target->m_connection->m_prefs, "security.default_mail_cert",
			                 &certNickname);
	if (rv != PR_SUCCESS) {
		goto loser;
	}

	/* Deal with the signing part first */
	if (!res->signthis) {
		rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_not_to_be_signed", &fmtSigned);
		if (rv != SSM_SUCCESS) {
			goto loser;
		}
	} else {
		/* Do we have a default cert installed */
		if (!certNickname) {
			/* We have no signing cert */
			rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_no_cert", &fmtSigned);
			if (rv != SSM_SUCCESS) {
				goto loser;
			}
		} else {
			cert = CERT_FindUserCertByUsage(target->m_connection->m_certdb, 
											certNickname, 
											certUsageEmailSigner,
											PR_FALSE,
											target->m_connection);
			if (!cert){
				/* We have no signing cert */
				rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_no_cert", &fmtSigned);
				if (rv != SSM_SUCCESS) {
					goto loser;
				}
			} else {
				/* Verify the cert */
				if (CERT_VerifyCert(target->m_connection->m_certdb,
									cert,
									PR_TRUE, 
									certUsageEmailSigner,
									PR_Now(),
									target->m_connection,
									NULL) == SECSuccess) {
					rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_can_be_signed", &fmtSigned);
					if (rv != SSM_SUCCESS) {
						goto loser;
					}
				} else {
					err = PR_GetError();
					switch (err) {
						case SEC_ERROR_EXPIRED_CERTIFICATE:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_expired_cert", &fmtSigned);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						case SEC_ERROR_REVOKED_CERTIFICATE:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_revoked_cert", &fmtSigned);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						case SEC_ERROR_CERT_USAGES_INVALID:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_invalid_cert", &fmtSigned);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						default:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_sign_unknown_error", &fmtSigned);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
					}
				}
			}
		}
	}

	/* Now deal with the encryption part */
	if (!res->encryptthis) {
		rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_not_to_be_encrypted", &fmtEncrypted);
		if (rv != SSM_SUCCESS) {
			goto loser;
		}
	} else {
		if (!res->numRecipients) {
			rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_no_recipients", &fmtEncrypted);
			if (rv != SSM_SUCCESS) {
				goto loser;
			}
		} else {
			/* Do we have a default cert installed */
			if (!certNickname) {
				/* We have no cert */
				rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_no_cert", &fmtEncrypted);
				if (rv != SSM_SUCCESS) {
					goto loser;
				}
			} else {
				cert = CERT_FindUserCertByUsage(target->m_connection->m_certdb,
												certNickname,
												certUsageEmailRecipient,
												PR_FALSE,
												target->m_connection);
				if (!cert) {
					/* We have no cert */
					rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_no_cert", &fmtEncrypted);
					if (rv != SSM_SUCCESS) {
						goto loser;
					}
				} else {
					/* Verify the cert */
					if (CERT_VerifyCert(target->m_connection->m_certdb,
										cert,
										PR_TRUE, 
										certUsageEmailRecipient,
										PR_Now(),
										target->m_connection,
										NULL) == SECSuccess) {
						errCerts = (char **) PR_CALLOC(sizeof(char*)*res->numRecipients);
						if (!errCerts) {
							goto loser;
						}
						/* Verify the recipient certs */
						for (i=0,numErrCerts=0; i<res->numRecipients; i++) {
							cert = CERT_FindCertByEmailAddr(target->m_connection->m_certdb, res->recipients[i]);
							if (!cert) {
								errCerts[numErrCerts++] = PL_strdup(res->recipients[i]);
								continue;
							}
							if (CERT_VerifyCertNow(target->m_connection->m_certdb,
									   cert,
									   PR_TRUE,
									   certUsageEmailRecipient,
									   target->m_connection) == SECFailure) {
								errCerts[numErrCerts++] = PL_strdup(res->recipients[i]);
								continue;
							}
							CERT_DestroyCertificate(cert);
						}
						if (!numErrCerts) {
							rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_can_be_encrypted", &fmtEncrypted);
							if (rv != SSM_SUCCESS) {
								goto loser;
							}
						} else {
							char *option = NULL;
							char *out = NULL;

							rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_bad_recipients", &fmt);
							if (rv != SSM_SUCCESS) {
								goto loser;
							}
				
							for (i=0; i < numErrCerts; i++) {
								option = PR_smprintf("<option selected>%s</option>\n", errCerts[i]);
								SSM_ConcatenateUTF8String(&out, option);
							}

							fmtEncrypted = PR_smprintf(fmt, target->m_id, numErrCerts, out);
							PR_Free(fmt);
						}
					} else {
						err = PR_GetError();
						switch (err) {
						case SEC_ERROR_EXPIRED_CERTIFICATE:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_expired_cert", &fmtEncrypted);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						case SEC_ERROR_REVOKED_CERTIFICATE:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_revoked_cert", &fmtEncrypted);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						case SEC_ERROR_CERT_USAGES_INVALID:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_invalid_cert", &fmtEncrypted);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
							break;
						default:
							{
								rv = SSM_GetAndExpandTextKeyedByString(cx, "sa_compose_encrypted_unknown_error", &fmtEncrypted);
								if (rv != SSM_SUCCESS) {
									goto loser;
								}
							}
						}
					}
				}
			}
		}
	}

	/* Output the stirngs */
    PR_FREEIF(cx->m_result);
	cx->m_result = PR_smprintf("%s%s", fmtSigned, fmtEncrypted);
	PR_Free(fmtSigned);
	PR_Free(fmtEncrypted);
	return SSM_SUCCESS;

loser:
	PR_FREEIF(fmt);
	PR_FREEIF(fmtSigned);
	PR_FREEIF(fmtEncrypted);
	return SSM_FAILURE;
}

SSMStatus SSMSecurityAdvisorContext_sa_selected_item(SSMTextGenContext* cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *target = NULL;
    SSMSecurityAdvisorContext* res = NULL;

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSecurityAdvisorContext*)target;

	/* Load the correct page */
	switch (res->infoContext) 
	{
		case SSM_NOINFO:
				rv = sa_noinfo(cx);
				break;
		case SSM_BROWSER:
				rv = sa_navigator(cx);
				break;
		case SSM_SNEWS_MESSAGE:
		case SSM_NEWS_MESSAGE:
		case SSM_MAIL_MESSAGE:
				rv = sa_message(cx);
				break;
		case SSM_COMPOSE:
				rv = sa_compose(cx);
				break;
		default:
				rv = SSM_FAILURE;
				break;
	}
	goto done;
done:
    return rv;
}

SSMStatus 
SSMSecurityAdvisorContext_GetPrefListKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMSecurityAdvisorContext* adv = NULL;
    PrefSet* prefs = NULL;
    char* subboolfmt;
    char* subfmt;
    char* fmt;
    PRBool boolval;
    char* strval = NULL;
    char* str1 = NULL;
    char* str2 = NULL;
    char* str3 = NULL;
    char* str4 = NULL;
    char* str5 = NULL;
    char* str6 = NULL;
    char* str7 = NULL;
    char* str8 = NULL;
    char* str9 = NULL;
    char* str10 = NULL;
    char* str11 = NULL;
	char* str12 = NULL;

    if ((cx == NULL) || (cx->m_request == NULL) || (cx->m_result == NULL)) {
        goto loser;
    }

    adv = (SSMSecurityAdvisorContext*)SSMTextGen_GetTargetObject(cx);
    if ((adv == NULL) ||
        !SSM_IsA((SSMResource*)adv, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        goto loser;
    }

    /* get the prefs object */
    if ((adv->super.m_connection == NULL) || 
        (adv->super.m_connection->m_prefs == NULL)) {
        goto loser;
    }
    prefs = adv->super.m_connection->m_prefs;

    /* strings and formats */
    subboolfmt = "var %1$s = %2$s;\n";
    subfmt = "var %1$s = \"%2$s\";\n";
    /* since nickname may contain ', I really should use " for quotes */
    fmt = "%1$s%2$s%3$s%4$s%5$s%6$s%7$s%8$s%9$s%10$s%11$s%12$s";

    /* make sure all the right default values are enforced */
    rv = PREF_GetBoolPref(prefs, "security.enable_ssl2", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str1 = PR_smprintf(subboolfmt, SSL2_SPK, "false");
    }
    else { /* default */
        str1 = PR_smprintf(subboolfmt, SSL2_SPK, "true");
    }

    rv = PREF_GetBoolPref(prefs, "security.enable_ssl3", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str2 = PR_smprintf(subboolfmt, SSL3_SPK, "false");
    }
    else { /* default */
        str2 = PR_smprintf(subboolfmt, SSL3_SPK, "true");
    }

    rv = PREF_GetStringPref(prefs, "security.default_personal_cert", &strval);
    if ((strval != NULL) && (PL_strcmp(strval, "Select Automatically") == 0)) {
        str3 = PR_smprintf(subboolfmt, CLIENT_AUTH_SPK, "true");
    }
    else { /* default */
        str3 = PR_smprintf(subboolfmt, CLIENT_AUTH_SPK, "false");
    }

    rv = PREF_GetStringPref(prefs, "security.default_mail_cert", &strval);
    if (strval != NULL) {
        str4 = PR_smprintf(subfmt, EMAIL_CERT_SPK, strval);
    }
    else {
        str4 = PR_smprintf(subfmt, EMAIL_CERT_SPK, "");
    }

    rv = PREF_GetBoolPref(prefs, "security.warn_entering_secure", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str5 = PR_smprintf(subboolfmt, WARN_ENTER_SECURE_SPK, "false");
    }
    else { /* default */
        str5 = PR_smprintf(subboolfmt, WARN_ENTER_SECURE_SPK, "true");
    }

    rv = PREF_GetBoolPref(prefs, "security.warn_leaving_secure", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str6 = PR_smprintf(subboolfmt, WARN_LEAVE_SECURE_SPK, "false");
    }
    else { /* default */
        str6 = PR_smprintf(subboolfmt, WARN_LEAVE_SECURE_SPK, "true");
    }

    rv = PREF_GetBoolPref(prefs, "security.warn_viewing_mixed", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str7 = PR_smprintf(subboolfmt, WARN_VIEW_MIXED_SPK, "false");
    }
    else { /* default */
        str7 = PR_smprintf(subboolfmt, WARN_VIEW_MIXED_SPK, "true");
    }

    rv = PREF_GetBoolPref(prefs, "security.warn_submit_insecure", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str8 = PR_smprintf(subboolfmt, WARN_SUBMIT_INSECURE_SPK, "false");
    }
    else { /* default */
        str8 = PR_smprintf(subboolfmt, WARN_SUBMIT_INSECURE_SPK, "true");
    }

    rv = PREF_GetBoolPref(prefs, "mail.encrypt_outgoing_mail", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_TRUE)) {
        str9 = PR_smprintf(subboolfmt, ENCRYPT_MAIL_SPK, "true");
    }
    else { /* default */
        str9 = PR_smprintf(subboolfmt, ENCRYPT_MAIL_SPK, "false");
    }

    rv = PREF_GetBoolPref(prefs, "mail.crypto_sign_outgoing_mail", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_TRUE)) {
        str10 = PR_smprintf(subboolfmt, SIGN_MAIL_SPK, "true");
    }
    else { /* default */
        str10 = PR_smprintf(subboolfmt, SIGN_MAIL_SPK, "false");
    }

    rv = PREF_GetBoolPref(prefs, "mail.crypto_sign_outgoing_news", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_TRUE)) {
        str11 = PR_smprintf(subboolfmt, SIGN_NEWS_SPK, "true");
    }
    else { /* default */
        str11 = PR_smprintf(subboolfmt, SIGN_NEWS_SPK, "false");
    }

    rv = PREF_GetBoolPref(prefs, "security.enable_tls", &boolval);
    if ((rv == SSM_SUCCESS) && (boolval == PR_FALSE)) {
        str12 = PR_smprintf(subboolfmt, TLS_SPK, "false");
    }
    else { /* default */
        str12 = PR_smprintf(subboolfmt, TLS_SPK, "true");
    }

    PR_FREEIF(cx->m_result);
    cx->m_result = PR_smprintf(fmt, str1, str2, str3, str4, str5, str6, str7,
                               str8, str9, str10, str11, str12);
    SSM_DebugUTF8String("security advisor prefs list", cx->m_result);

    goto done;
loser:
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
done:
    PR_FREEIF(str1);
    PR_FREEIF(str2);
    PR_FREEIF(str3);
    PR_FREEIF(str4);
    PR_FREEIF(str5);
    PR_FREEIF(str6);
    PR_FREEIF(str7);
    PR_FREEIF(str8);
    PR_FREEIF(str9);
    PR_FREEIF(str10);
    PR_FREEIF(str11);

    return SSM_SUCCESS;
}

SSMStatus
SSM_FreeTarget(SSMTextGenContext *cx)
{
    SSMResource *res = NULL;

    res = SSMTextGen_GetTargetObject(cx);
    if (res != NULL) {
        SSM_FreeResource(res);
    }
    return SSM_SUCCESS;
}


#if 0
/*---- Functions for handling Java principal certs ----*/
static SSMStatus ssm_parse_token(char* start, char** dest)
{
    char* openQuote = NULL;
    char* closeQuote = NULL;

    *dest = NULL;

    openQuote = strchr(start, '\"');
    if (openQuote == NULL) {
        return SSM_FAILURE;
    }

    closeQuote = strchr(openQuote+1, '\"');
    if (closeQuote == NULL) {
        return SSM_FAILURE;
    }

    *dest = (char*)PR_Calloc(closeQuote-openQuote, sizeof(char));
    if (*dest == NULL) {
        return SSM_FAILURE;
    }

    /* copy the string */
    memcpy(*dest, openQuote+1, closeQuote-openQuote-1);
    
    return SSM_SUCCESS;
}

static SSMStatus ssm_parse_principals(char* principalsData, 
                                      char*** principals, PRIntn* size)
{
    SSMStatus rv;
    PRIntn i = 1;
    char* start = NULL;

    *principals = NULL;

    /* first check whether the data is empty */
    if (*principalsData == '\0') {
        /* this is not a failure */
        *size = 0;
        return SSM_SUCCESS;
    }

    /* data looks like "\"Netscape\", \"AOL\"": tokenize the string */
    /* first determine the number of tokens */
    start = principalsData;
    while ((start = strchr(start+1, ',')) != NULL) {
        i++;
    }

    *principals = (char**)PR_Calloc(i, sizeof(char*));
    if (*principals == NULL) {
        return SSM_FAILURE;
    }
    *size = i;

    /* parse the tokens and store them in principals */
    for (start = principalsData, i = 0; start != NULL; 
         start = strchr(start, ','), i++) {
        rv = ssm_parse_token(start, &((*principals)[i]));
        if (rv != SSM_SUCCESS) {
            goto loser;
        }
    }
    return SSM_SUCCESS;
 loser:
    for (i = 0; i < *size; i++) {
        PR_FREEIF((*principals)[i]);
    }
    return SSM_FAILURE;
}

static SSMStatus ssm_retrieve_principals(SSMSecurityAdvisorContext* adv,
                                         char*** principals, PRIntn* size)
{
    SSMStatus rv;
    SingleNumMessage request;
    CMTItem message;

    /* first, clear data */
    PR_FREEIF(adv->m_principalsData);

    /* pack the request */
    request.value = (PRIntn)adv->super.m_id;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) !=
        CMTSuccess) {
        return SSM_FAILURE;
    }
    message.type = SSM_EVENT_MESSAGE | SSM_GET_JAVA_PRINCIPALS_EVENT;

    /* send the message through the control out queue */
    SSM_SendQMessage(adv->super.m_connection->m_controlOutQ, 
                     SSM_PRIORITY_NORMAL, message.type, message.len,
                     (char*)message.data, PR_TRUE);

    /* awkward way of blocking, but can't be helped */
    SSM_LockUIEvent(SSMRESOURCE(adv));
    while (adv->m_principalsData == NULL) {
        SSM_WaitUIEvent(SSMRESOURCE(adv), PR_INTERVAL_NO_TIMEOUT);
    }
    SSM_UnlockUIEvent(SSMRESOURCE(adv));

    /* parse the date that came from the plugin */
    rv = ssm_parse_principals(adv->m_principalsData, principals, size);

    return rv;
}

SSMStatus SSM_JavaPrincipalsKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv;
    SSMSecurityAdvisorContext* adv = NULL;
    char* prefix = NULL;
    char* wrapper = NULL;
    char* suffix = NULL;
    char* prefixKey = NULL;
    char* wrapperKey = NULL;
    char* suffixKey = NULL;
    char* tmpStr = NULL;
    char* finalStr = NULL;
    const PRIntn CERT_PREFIX = 0;
    const PRIntn CERT_WRAPPER = 1;
    const PRIntn CERT_SUFFIX = 2;
    PRIntn i;
#if 0
    char* principals[5] = {"Macromedia", "NetCenter", "Sun Microsystems",
                           "MindSpring", "Hong Kong Telecom"};
#else
    char** principals = NULL;
    PRIntn size;
#endif

    if ((cx == NULL) || (cx->m_result == NULL)) {
        goto loser;
    }

    cx->m_result = NULL; /* in case we fail */

    adv = (SSMSecurityAdvisorContext*)SSMTextGen_GetTargetObject(cx);
    if ((adv == NULL) ||
        !SSM_IsA((SSMResource*)adv, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        goto loser;
    }

    /* retrieve the principals in string form from the plugin */
    rv = ssm_retrieve_principals(adv, &principals, &size);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    prefixKey = (char*)SSM_At(cx->m_params, CERT_PREFIX);
    wrapperKey = (char*)SSM_At(cx->m_params, CERT_WRAPPER);
    suffixKey = (char*)SSM_At(cx->m_params, CERT_SUFFIX);

    rv = SSM_GetAndExpandTextKeyedByString(cx, prefixKey, &prefix);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    
    rv = SSM_GetAndExpandTextKeyedByString(cx, suffixKey, &suffix);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    finalStr = PL_strdup(prefix);
    if (finalStr == NULL) {
        goto loser;
    }

    /* add individual certs */
    if (principals != NULL) {
        rv = SSM_GetAndExpandTextKeyedByString(cx, wrapperKey, &wrapper);
        if (rv != SSM_SUCCESS) {
            goto loser;
        }

        for (i = 0; i < size; i++) {
            tmpStr = PR_smprintf(wrapper, principals[i]);
            if (SSM_ConcatenateUTF8String(&finalStr, tmpStr) != SSM_SUCCESS) {
                goto loser;
            }
            PR_Free(tmpStr);
            tmpStr = NULL;
        }
    }

    rv = SSM_ConcatenateUTF8String(&finalStr, suffix);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    cx->m_result = finalStr;

    goto done;
loser:
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
    PR_FREEIF(finalStr);
    PR_FREEIF(tmpStr);
done:
    PR_FREEIF(prefix);
    PR_FREEIF(wrapper);
    PR_FREEIF(suffix);
    return rv;
}

void SSM_HandleGetJavaPrincipalsReply(SSMControlConnection* ctrl,
                                      SECItem* message)
{
    SSMStatus rv;
    GetJavaPrincipalsReply reply;
    SSMResource* res = NULL;
    SSMSecurityAdvisorContext* adv = NULL;

    if (CMT_DecodeMessage(GetJavaPrincipalsReplyTemplate, &reply, 
                          (CMTItem*)message) != CMTSuccess) {
        goto loser;
    }

    rv = SSMControlConnection_GetResource(ctrl, reply.resID, &res);
    if (rv != PR_SUCCESS || res == NULL) {
        goto loser;
    }

    /* make sure it is a Security Advisor context */
    if (!SSM_IsA(res, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        goto loser;
    }

    adv = (SSMSecurityAdvisorContext*)res;
    SSM_LockUIEvent(res);
    adv->m_principalsData = reply.principals;
    /* it will be freed by the Security Advisor */
    if (adv->m_principalsData == NULL) {
        adv->m_principalsData = PL_strdup("");
    }
    SSM_NotifyUIEvent(res);
    SSM_UnlockUIEvent(res);

    return;
loser:
    /* XXX we can't really wake up the waiting Security Advisor object 
     *     because we don't know which resource to wake up.  Bad.
     */
    PR_FREEIF(reply.principals);
    return;
}
#endif

/* platform-dependent names for signed.db */
#ifdef XP_UNIX
#define SIGNED "/signedapplet.db"
#else
#ifdef XP_MAC
#define SIGNED ":SignedAppletDB"
#else
#define SIGNED "\\signed.db"
#endif
#endif

SSMStatus SSM_RemovePrivilegesHandler(HTTPRequest* req)
{
    SSMResource* res = NULL;
    char* buttonValue;
    SSMStatus rv;

    res = (req->target) ? req->target : (SSMResource*)req->ctrlconn;

    rv = SSM_HTTPParamValue(req, "OK", &buttonValue);
    if (rv == SSM_SUCCESS) {
        /* do action */
        char* signeddb;

        signeddb = PR_smprintf("%s%s", req->ctrlconn->m_dirRoot, SIGNED);
        /* XXX we delete the signed.db file here, but we do not actively
         *     check the return value.  There may be failures (file does
         *     not exist, file may be already open, etc.) but we do not
         *     have other recourses.  Sigh.
         */
        PR_Delete(signeddb);
        PR_Free(signeddb);
    }
    else {
        rv = SSM_HTTPParamValue(req, "Cancel", &buttonValue);
    }

    if (rv != SSM_SUCCESS) {
        rv = SSM_ERR_NO_BUTTON;
        goto done;
    }
 done:
    rv = SSM_HTTPDefaultCommandHandler(req);
    return rv;
}

static PRBool 
ocsplist_is_default_signer(CERTCertificate *cert,
                           SSMDefaultOCSPRespondersList *potResponders)
{
    if (!cert->nickname) {
        return PR_FALSE;
    }
    return (PL_strcmp(cert->nickname, potResponders->defaultSigner) == 0) ? 
            PR_TRUE : PR_FALSE;
}

static PRBool ocsplist_include_cert(CERTCertificate *cert)
{
    CERTCertTrust *trust;
    char *nickname;

    trust = cert->trust;
    nickname = cert->nickname;
    
    if ( ( ( trust->sslFlags & CERTDB_INVISIBLE_CA ) ||
           (trust->emailFlags & CERTDB_INVISIBLE_CA ) ||
           (trust->objectSigningFlags & CERTDB_INVISIBLE_CA ) ) ||
         nickname == NULL) {
        return PR_FALSE;
    }
    if ((trust->sslFlags & CERTDB_VALID_CA) ||
        (trust->emailFlags & CERTDB_VALID_CA) ||
        (trust->objectSigningFlags & CERTDB_VALID_CA)) {
        return PR_TRUE;
    }
    return PR_FALSE;
}


static SECStatus ssm_get_potential_ocsp_signers(CERTCertificate *cert,
                                                SECItem         *dbKey,
                                                void            *arg)
{
    SSMDefaultOCSPRespondersList *potResponders;
    char *serviceURL, *nickname;

    potResponders = (SSMDefaultOCSPRespondersList*)arg;

    if (!ocsplist_include_cert(cert) ||
        ocsplist_is_default_signer(cert, potResponders)) {
        goto done;
    }
    /*
     * We've got a CA cert, now we need to figure out if it has the AIA
     * extension that points to a responder somewhere. Get our own copy
     * of the nickname.
     */
    nickname = PL_strdup(cert->nickname);
    serviceURL = CERT_GetOCSPAuthorityInfoAccessLocation(cert);
    if (serviceURL != NULL) {
        /*
         * This CA has an AIA extension so we'll bundle it with the group
         * certs that have the 
         */
        SSMSortedList_Insert(potResponders->respondersWithAIA,
                             nickname, serviceURL);
    } else {
        /*
         * This CA doesn't have an AIA extension so we lump with the
         * rest of the CA's.
         */
        SSMSortedList_Insert(potResponders->respondersWithoutAIA,
                             nickname, NULL);
    }
    
 done:
    return SECSuccess;
}

static void
ocsplist_freedata(void *data)
{
    if (data) {
        PR_Free(data);
    }
}

static void
ocsplist_freekey(void *data)
{
    ocsplist_freedata(data);
}

static SSMStatus 
ocsplist_aiacerts_enumerator(PRIntn index, void * arg, void *key,
                             void *data)
{
    SSMDefaultOCSPRespondersList *potResps;
    char *nextChunk;
    SSMStatus rv;

    potResps = (SSMDefaultOCSPRespondersList*)arg;
    nextChunk = PR_smprintf(potResps->wrapper, (char*)key, 
                            (data == NULL) ? "http://" : (char*)data);
    rv = SSM_ConcatenateUTF8String(&potResps->cx->m_result, nextChunk);
    PR_Free(nextChunk);
    return rv;
}

static SSMStatus 
ocsplist_cacerts_enumerator(PRIntn index, void * arg, void *key,
                            void *data)
{
    return ocsplist_aiacerts_enumerator(index, arg, key, data);
}

SSMStatus 
SSM_OCSPResponderList(SSMTextGenContext *cx)
{
    SECStatus srv;
    SSMDefaultOCSPRespondersList potentialResponders = {NULL, NULL, 
                                                        NULL, NULL, NULL};
    SSMSortedListFn funcs;
    char *prefix, *suffix, *tmpStr, *prefService, *prefSigner;
    SSMStatus rv;

    PR_ASSERT(cx != NULL);
    funcs.keyCompare       = certlist_compare_strings;
    funcs.freeListItemData = ocsplist_freedata;
    funcs.freeListItemKey  = ocsplist_freekey;
    potentialResponders.respondersWithAIA    = SSMSortedList_New(&funcs);
    potentialResponders.respondersWithoutAIA = SSMSortedList_New(&funcs);
    PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs,
                       "security.OCSP.signingCA", 
                       &potentialResponders.defaultSigner);
    srv = SEC_TraversePermCerts(cx->m_request->ctrlconn->m_certdb,
                                ssm_get_potential_ocsp_signers,
                                &potentialResponders);
    if (srv == SECSuccess) {
        srv = PK11_TraverseSlotCerts(ssm_get_potential_ocsp_signers,
                                     &potentialResponders, 
                                     cx->m_request->ctrlconn);
    }
    if (srv != SECSuccess) {
        goto loser;
    }
    prefix = (char*)SSM_At(cx->m_params, 0);
    suffix = (char*)SSM_At(cx->m_params, 1);
    PR_ASSERT(prefix);
    PR_ASSERT(suffix);
    if (prefix == NULL || suffix == NULL) {
        goto loser;
    }
    rv = SSM_GetAndExpandTextKeyedByString(cx, prefix, &tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    PR_Free(tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_GetAndExpandTextKeyedByString(cx, "ocsp_list_wrapper",
                                           &potentialResponders.wrapper);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    potentialResponders.cx = cx;
    /*
     * Now it's time to build the list of potential OCSP Responders.
     */
    SSMSortedList_Enumerate(potentialResponders.respondersWithAIA, 
                            ocsplist_aiacerts_enumerator, 
                            &potentialResponders);
    SSMSortedList_Enumerate(potentialResponders.respondersWithoutAIA,
                            ocsplist_cacerts_enumerator,
                            &potentialResponders);
    /*
     * If there was a default responder enabled, let's over-ride whatever
     * the cert said with what the user entered as the appropriate value.
     */
    if (PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs,
                           "security.OCSP.URL", &prefService) == SSM_SUCCESS) {
        if (PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs,
                      "security.OCSP.signingCA", &prefSigner) == SSM_SUCCESS) {
            tmpStr = PR_smprintf("addNewServiceURL(\"%1$s\",\"%2$s\");\n",
                                 prefSigner, prefService);
            rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
            if (rv != SSM_SUCCESS) {
                goto loser;
            }
            PR_Free(tmpStr);
        }
    }
    rv = SSM_GetAndExpandTextKeyedByString(cx, suffix, &tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    PR_Free(tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    PR_Free(potentialResponders.wrapper);
    SSMSortedList_Destroy(potentialResponders.respondersWithAIA);
    SSMSortedList_Destroy(potentialResponders.respondersWithoutAIA);
    return SSM_SUCCESS;
 loser:
    PR_FREEIF(potentialResponders.wrapper);
    if (potentialResponders.respondersWithAIA) {
        SSMSortedList_Destroy(potentialResponders.respondersWithAIA);
    }
    if (potentialResponders.respondersWithoutAIA) {
        SSMSortedList_Destroy(potentialResponders.respondersWithoutAIA);
    }
    return SSM_FAILURE;
}

SSMStatus
SSM_DisplayCRLButton(SSMTextGenContext *cx)
{
    SSMControlConnection *ctrl;
    char *crlHTML = NULL;
    SSMStatus rv;

    ctrl = SSMTextGen_GetControlConnection(cx);
    if (ctrl == NULL) {
        goto loser;
    }
    if (SSM_IsCRLPresent(ctrl)) {
        rv = SSM_GetAndExpandTextKeyedByString(cx, "crlButtonHTML", &crlHTML);
        if (rv == SSM_SUCCESS) {
            PR_FREEIF(cx->m_result);
            cx->m_result = crlHTML;
        }
    }
    return SSM_SUCCESS;
 loser:
    return SSM_FAILURE;
}

SSMStatus
SSM_ListCRLs(SSMTextGenContext *cx)
{
    SECStatus srv;
    CERTCrlHeadNode *head = NULL;
    CERTCrlNode *node;
    SSMControlConnection *ctrl;
    char *retString = NULL;
    char *currString ;
    char *emptyString = "";
    char *currCRLName = NULL;
    char *name = NULL;
    char *b64Name = NULL, *b64HTMLName;
    
    ctrl = SSMTextGen_GetControlConnection(cx);
    srv = SEC_LookupCrls(ctrl->m_certdb, &head, -1);
    if (srv != SECSuccess || head == NULL) {
        goto loser;
    }
    currString = emptyString;
    for (node=head->first; node != NULL; node = node->next) {
        name = CERT_GetCommonName(&(node->crl->crl.name));
        b64Name = BTOA_ConvertItemToAscii(&node->crl->crl.derName);
        b64HTMLName = ssm_packb64_name(b64Name);
        retString = PR_smprintf("%1$s\n<option value=\"%4$d%2$s\">%3$s", 
                                currString, b64HTMLName, name, node->type);
        PR_Free(name);
        PR_Free(b64Name);
        PR_Free(b64HTMLName);
        if (currString != emptyString)
            PR_Free(currString);
        currString = retString;
    }
    PR_FREEIF(cx->m_result);
    cx->m_result = retString;
    return SSM_SUCCESS;
 loser:
    return SSM_FAILURE;
}

SSMStatus
ssm_getStringForAbleAgent(SSMTextGenContext *cx, const char *agents[])
{
    int i;
    SSMStatus rv;
    char *key;
    
    key = SSM_At(cx->m_params, 0);


    for (i=0; agents[i] != NULL; i++) {
        if (PL_strstr(cx->m_request->agent, agents[i]) != NULL) {
            PR_FREEIF(cx->m_result);
            rv = SSM_GetAndExpandText(cx, key, &cx->m_result);
            if (rv != SSM_SUCCESS) {
                return rv;
            }
            break;
        }
    }
    return SSM_SUCCESS;
}

SSMStatus SSM_LayoutSMIMETab(SSMTextGenContext *cx)
{
    return ssm_getStringForAbleAgent(cx, kSMimeApps);
}

SSMStatus SSM_LayoutJavaJSTab(SSMTextGenContext *cx)
{
    return ssm_getStringForAbleAgent(cx, kJavaJSApps);
}

SSMStatus SSM_LayoutOthersTab(SSMTextGenContext *cx)
{
    return ssm_getStringForAbleAgent(cx, kSMimeApps);
}
