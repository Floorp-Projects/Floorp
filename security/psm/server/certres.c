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
#include "certres.h"
#include "sechash.h"
#include "ssmerrs.h"
#include "ctrlconn.h"
#include "certlist.h"
#include "resource.h"
#include "ssldlgs.h"
#include "certsearch.h"
#include "secpkcs7.h"
#include "secerr.h"
#include "advisor.h"

typedef enum 
{ myCert = 0,
  othersCert,
  webCert,
  caCert,
  badCert
} certPane;

enum {
  USER_CERT = (PRIntn) 0, 
  EMAIL_CERT, 
  CA_CERT, 
  WEBSITE_CERT
};

enum { 
  NICKNAME = (PRIntn) 0,
  EMAILADDR
};

SSMStatus SSM_DeleteCertificate(SSMResourceCert * resource);
SECItem * unhexify(char * hex);
SSMStatus
ssm_select_cert(SSMTextGenContext * cx, char ** result, char * fmt, 
	        PRIntn type, PRIntn key, char * nickname);
SSMStatus
ssm_create_select_cert_entry(SSMTextGenContext * cx, CERTCertificate * cert, 
			     char **result, char *fmt, char * checked);
SSMStatus
ssm_cert_belongs_type(CERTCertificate * cert, PRIntn type);
static
certPane SSMUI_CertBelongs(CERTCertificate * cert);

/* ### mwelch Defined in libcert. Should we be using this? */
SEC_BEGIN_PROTOS
extern SECStatus cert_GetKeyID(CERTCertificate *cert);
SEC_END_PROTOS

CERTCertificate * FindCertByKeyIDAndNickname(SSMControlConnection * ctrl, 
					     char *nickname, SECItem *keyID, 
					     char * serial);

CERTCertList * 
SSMControlConnection_CreateCertListByNickname(SSMControlConnection * ctrl, 
					      char * nick, PRBool email);
CERTCertificate * 
SSMControlConnection_FindCertByNickname(SSMControlConnection * ctrl, 
					char * nick, PRBool email);
SSMStatus SSM_RefreshRefererPage(HTTPRequest * req);
SSMStatus SSM_ProcessCertResourceUIAction(HTTPRequest * req, 
					  SSMResourceCert * certres);
extern SSMStatus httpparse_count_params(HTTPRequest * req);
extern SSMStatus httpparse_parse_params(HTTPRequest * req);

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(ss) (&(ss)->super)

SSMStatus SSMResourceCert_GetAttr(SSMResource * resource, 
				  SSMAttributeID attrib,
				  SSMResourceAttrType attrType,
				  SSMAttributeValue *value)
{
  SSMResourceCert * res = (SSMResourceCert *)resource;
  SECItem secitem;
  SSMStatus rv;
  char *tmpstr = NULL;

  if (!res || !res->cert || !value )
    goto loser;

  /* Access fields in the certificate and pass up in the data field */
  switch (attrib) {
  case SSM_FID_CERT_SUBJECT_NAME:
    if (!res->cert->subjectName) 
      goto loser;
    value->type = SSM_STRING_ATTRIBUTE;
    value->u.string.len = strlen(res->cert->subjectName);
    value->u.string.data = (unsigned char *) strdup(res->cert->subjectName);
    break;
    
  case SSM_FID_CERT_ISSUER_NAME:
    if (!res->cert->issuerName) 
      goto loser;
    value->type = SSM_STRING_ATTRIBUTE;
    value->u.string.len = strlen(res->cert->issuerName);
    value->u.string.data = (unsigned char *) strdup(res->cert->issuerName);
    break;
    
  case SSM_FID_CERT_SERIAL_NUMBER:
    value->type = SSM_STRING_ATTRIBUTE;
    tmpstr = CERT_Hexify(&(res->cert->serialNumber), 1);
    if (tmpstr == NULL)
      goto loser;
    value->u.string.len = strlen(tmpstr);
    value->u.string.data = (unsigned char *) strdup(tmpstr);
    break;

  case SSM_FID_CERT_EXP_DATE:
    value->type = SSM_STRING_ATTRIBUTE;
    tmpstr = DER_UTCDayToAscii(&(res->cert->validity.notAfter));
    if (tmpstr == NULL)
      goto loser;
    value->u.string.len = strlen(tmpstr);
    value->u.string.data = (unsigned char *) strdup(tmpstr);
    break;

  case SSM_FID_CERT_FINGERPRINT:
    {
      unsigned char buf[MD5_LENGTH];
      value->type = SSM_STRING_ATTRIBUTE;
      MD5_HashBuf(buf, res->cert->derCert.data, res->cert->derCert.len);
      secitem.data = buf;
      secitem.len = 16;
      tmpstr = CERT_Hexify(&secitem, 1);
      if (tmpstr == NULL)
	    goto loser;
      value->u.string.len = strlen(tmpstr);
      value->u.string.data = (unsigned char *) strdup(tmpstr);
      break;
    }
    
  case SSM_FID_CERT_COMMON_NAME:
    value->type = SSM_STRING_ATTRIBUTE;
    tmpstr = CERT_GetCommonName(&(res->cert->subject));
    if (tmpstr == NULL)
      goto loser;
    value->u.string.len = strlen(tmpstr);
    value->u.string.data = (unsigned char *) strdup(tmpstr);
    break;
    
  case SSM_FID_CERT_NICKNAME:
    if (!res->cert->nickname) 
      goto loser;
    value->type = SSM_STRING_ATTRIBUTE;
    value->u.string.len = strlen(res->cert->nickname);
    value->u.string.data = (unsigned char *) strdup(res->cert->nickname);
    break;
    
  case SSM_FID_CERT_ORG_NAME:
    value->type = SSM_STRING_ATTRIBUTE;
    tmpstr = CERT_GetOrgName(&(res->cert->subject));
    if (tmpstr == NULL)
      goto loser;
    value->u.string.len = strlen(tmpstr);
    value->u.string.data = (unsigned char *) strdup(tmpstr);
    break;

  case SSM_FID_CERT_EMAIL_ADDRESS:
    value->type = SSM_STRING_ATTRIBUTE;
    tmpstr = PL_strdup(CERT_GetCertificateEmailAddress(res->cert));
    if (tmpstr == NULL)
      goto loser;
    value->u.string.len = PL_strlen(tmpstr);
    value->u.string.data = (unsigned char *) PL_strdup(tmpstr);
    break;
    
  case SSM_FID_CERT_PICKLE_CERT:
    value->type = SSM_STRING_ATTRIBUTE;
    if (!(value->u.string.data = (unsigned char *) PR_Malloc(res->cert->derCert.len))) {
        goto loser;
    }
    memcpy(value->u.string.data, res->cert->derCert.data, res->cert->derCert.len);
    value->u.string.len = res->cert->derCert.len;
    break;

  case SSM_FID_CERT_HTML_CERT:
    value->type = SSM_STRING_ATTRIBUTE;
    rv = (*resource->m_html_func)(resource, NULL, (void **)&tmpstr);
    if (rv != PR_SUCCESS)
      goto loser;
    value->u.string.len = PL_strlen(tmpstr);
    value->u.string.data = (unsigned char *) PL_strdup(tmpstr);
    break;

  case SSM_FID_CERT_CERTKEY:
      value->type = SSM_STRING_ATTRIBUTE;
      if (!(value->u.string.data = (unsigned char *) PR_Malloc(res->cert->certKey.len))) {
          goto loser;
      }
      memcpy(value->u.string.data, res->cert->certKey.data, res->cert->certKey.len);
      value->u.string.len = res->cert->certKey.len;
      break;

  case SSM_FID_CERT_FIND_CERT_ISSUER:
      {
        SSMResourceCert *certRes;
        SSMResourceID certID;
        SSMStatus rv;
        CERTCertificate *cert;

        /* Make sure we have a cert to return. */
        cert = CERT_FindCertIssuer(res->cert, PR_Now(), certUsageObjectSigner);
        if (!cert) {
            goto loser;
        }

        if ((cert->certKey.len != res->cert->certKey.len) ||
            (memcmp(cert->certKey.data, 
                    res->cert->certKey.data, 
                    cert->certKey.len)))
        {
            rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
				    cert, resource->m_connection, &certID,
				    (SSMResource**)&certRes);
	    if (rv != PR_SUCCESS)
	      goto loser;
	    rv = SSM_ClientGetResourceReference(SSMRESOURCE(certRes), &certID);
	    SSM_FreeResource(SSMRESOURCE(certRes));
	    if (rv != PR_SUCCESS)
	      goto loser;
        }
        else
        {
	    rv = SSM_ClientGetResourceReference(SSMRESOURCE(res), &certID);
	    if (rv != PR_SUCCESS)
	      goto loser;
        }

        value->type = SSM_RID_ATTRIBUTE;
	value->u.rid = certID;

        rv = PR_SUCCESS;
        break;
        }
    case SSM_FID_CERT_ISPERM:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = res->cert->isperm;
        rv = PR_SUCCESS;
        break;

  default: 
    goto loser;
  }

  if (tmpstr != NULL)
    PR_Free(tmpstr);
  return PR_SUCCESS;
  
loser:
  value->type = SSM_NO_ATTRIBUTE;
  if (tmpstr != NULL)
    PR_Free(tmpstr);
  return PR_FAILURE;
}


SSMStatus SSMResourceCert_Pickle(SSMResource * res, 
			PRIntn * len,
			void ** data)
{
  SSMAttributeValue value;
  SSMStatus rv;

  if (!res || !((SSMResourceCert *)res)->cert || !data || !len ) 
    return PR_FAILURE;

  rv =  ((*(SSMResourceCert *)res).super.m_get_func)(res,
						     SSM_FID_CERT_PICKLE_CERT,
						     SSM_STRING_ATTRIBUTE,
						     &value);
  if (rv != PR_SUCCESS)
    goto loser;

  *data = PR_Malloc(value.u.string.len);
  if (!*data) { 
      goto loser;
  }
  memcpy(*data, value.u.string.data, value.u.string.len);
  *len = value.u.string.len;
  SSM_DestroyAttrValue(&value, PR_FALSE);
loser:
  return rv;
}


SSMStatus SSMResourceCert_GetAttrIDs(SSMResource *res,
				    SSMAttributeID **ids,
				    PRIntn *count)
{
  int i=-1;
  
  if (!res || !ids || !count) 
    goto loser;
  
  /* all certificate attributes accessible to client */
  *count = SSM_FID_CERT_ORG_NAME - SSM_FID_CERT_SUBJECT_NAME + 1;
  *ids = (SSMAttributeID *)PORT_ZAlloc(*count * (sizeof(SSMAttributeID)));
  if (!*ids) goto loser;

  while (i < *count) {
    (*ids)[i] = (SSMAttributeID) ((int) SSM_FID_CERT_SUBJECT_NAME + i);
    i++;
  }
  return PR_SUCCESS;
  
loser:
  return PR_FAILURE;
}


SSMStatus
SSMResourceCert_Create(void *arg, SSMControlConnection * connection, 
		       SSMResource **res)
{
  SSMStatus rv = PR_SUCCESS;
  SSMResourceCert * certResource = NULL;

  if (!res) 
    goto loser;
  *res = NULL;
  
  if (!arg)
    goto loser;
  
  /* before creating a cert, make sure we don't have it already */
  SSMControlConnection_CertLookUp(connection, arg, res);
  if (*res) 
    goto done; /* found cert resource! */

  certResource = (SSMResourceCert *)PR_CALLOC(sizeof(SSMResourceCert));
  if (!certResource) 
    goto loser;
  rv = SSMResourceCert_Init(certResource, connection, arg);
  if (rv != PR_SUCCESS) goto loser;
  
  *res = (SSMResource *)&certResource->super;
  /* enter cert into cert db */
  rv = SSM_HashInsert(connection->m_certIdDB, (SSMHashKey) arg, (void *)*res);
  if (rv != PR_SUCCESS) 
    goto loser;

done:
  return PR_SUCCESS;
loser:
  if (certResource) 
    (*((SSMRESOURCE(certResource))->m_destroy_func))
      ((SSMResource *)certResource, PR_TRUE);
  else 
    if (arg) 
      PR_Free(arg); 
  return PR_FAILURE;
}

SSMStatus SSMResourceCert_Init(SSMResourceCert * certResource,
			      SSMControlConnection * conn,
			      void * arg)
     
{
  SSMResource_Init(conn, SSMRESOURCE(certResource), SSM_RESTYPE_CERTIFICATE);
  certResource->cert = (CERTCertificate *)arg;
  if (!arg) 
    return SSM_FAILURE;

  if (certResource->cert->slot) 
    PK11_ReferenceSlot(certResource->cert->slot);
  certResource->m_verify_func = &SSMResourceCert_Verify;
  certResource->m_deletecert_func = &SSMResourceCert_DeleteCert;
  certResource->m_markedForDeletion = PR_FALSE;
  return SSM_SUCCESS;
}

SSMStatus SSMResourceCert_Unpickle(SSMResource ** resource, 
				  SSMControlConnection * connection, 
				  PRInt32 len, 
				  void * value)
{
  SSMStatus rv = PR_SUCCESS;
  SSMResourceID resID;
  CERTCertificate * cert;
  SECItem certArg;

  if (!resource || !value) 
     goto loser;
  certArg.len = len;
  certArg.data = (unsigned char *) value;
  
  cert = CERT_NewTempCertificate(connection->m_certdb,(SECItem *)&certArg, 
                                 NULL, PR_FALSE, PR_TRUE);
  if (!cert) {
    rv = (SSMStatus) PR_GetError();
    goto loser;
  }
  rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, (void *)cert, connection, 
			  &resID, resource); 
  if (rv != PR_SUCCESS) goto loser;
  goto done;

loser:

  if (resource && *resource) {
    SSM_DEBUG("Error unpickling cert: %d.\n", rv);
    ((*resource)->m_destroy_func)(*resource, PR_TRUE);
    PR_Free(*resource);
  }
  SSM_DEBUG("Error unpickling cert: %d.\n", rv);
done:
  return rv;
}



SSMStatus SSMResourceCert_Destroy(SSMResource * resource, PRBool doFree)
{
  SSMResourceCert * res = (SSMResourceCert *)resource;
  SSMResource *tmpRes;

  if (!res) goto loser;
  SSM_DEBUG("Trying to remove cert with id: %d\n", resource->m_id);
  /* remove cert from the cert resource db */
  SSM_HashRemove(resource->m_connection->m_certIdDB, (SSMHashKey) res->cert, 
  	(void**)&tmpRes);
  PR_ASSERT(tmpRes == resource); 

  if (res->cert != NULL) {
      if (res->m_markedForDeletion) {
	  SSM_DEBUG("Cert id %d is being deleted.\n", resource->m_id);
	  if (SSMUI_CertBelongs(res->cert) == myCert) {
	      PK11_DeleteTokenCertAndKey(res->cert, res->super.m_connection);
	  }
	  else {
	      SEC_DeletePermCertificate(res->cert);
	  }
      }
      else {
	  CERT_DestroyCertificate(res->cert);
      }
      res->cert = NULL;
  }

  /* Destroy superclass */
  SSMResource_Destroy(SSMRESOURCE(res), PR_FALSE);
  
  if (doFree)
    PR_Free(res);

  return PR_SUCCESS;
loser:
  return PR_FAILURE;
}

SSMStatus SSM_VerifyCert(SSMResourceCert * resource, 
				SECCertUsage certUsage)
{
  return (*resource->m_verify_func)(resource, certUsage);
}

SSMStatus SSM_DeleteCertificate(SSMResourceCert * resource) 
{
  return (*resource->m_deletecert_func)(resource);
}

SSMStatus SSMResourceCert_DeleteCert(SSMResourceCert * res)
{

  PR_ASSERT(SSM_IsAKindOf((SSMResource *)res, SSM_RESTYPE_CERTIFICATE));  
  if (!res || !res->cert) {
    SSM_DEBUG("DeleteCert: bad argument!\n");
    goto done;
  }

  SSM_DEBUG("Cert id %d is marked for deletion.\n", ((SSMResource*)res)->m_id);
  SSM_LockResource((SSMResource*)res);
  res->m_markedForDeletion = PR_TRUE; /* delete it from disk */
  SSM_UnlockResource((SSMResource*)res);

  /* this will get the reference count right */
  SSM_FreeResource((SSMResource*)res);

 done:
  return SSM_SUCCESS;
}


SSMStatus SSMResourceCert_Verify(SSMResourceCert  * resource,
				SECCertUsage certUsage)
{
  if (!resource || !resource->cert)
    goto loser;

  if (CERT_VerifyCertNow(SSMRESOURCE(resource)->m_connection->m_certdb, 
			 resource->cert, PR_TRUE, 
			 certUsage, 
			 SSMRESOURCE(resource)->m_connection) != SECSuccess) 
    goto loser;
  return PR_SUCCESS;
loser:
  return PR_FAILURE;
}

SSMStatus SSMResourceCert_HTML(SSMResource *res, PRIntn * len, void ** value)
{
  SSMStatus rv = PR_SUCCESS;
  SSMResourceCert * resource = (SSMResourceCert *)res;

  if (!resource || !resource->cert || !value) {
    rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  *value = NULL;
  if (len) *len = 0;
    
  *value = CERT_HTMLCertInfo(resource->cert, PR_FALSE, PR_TRUE);
  if (!*value) { 
    rv = (SSMStatus) PR_GetError();
    goto loser;
  }
  
  if (len)
    *len = strlen((char *)*value);
  goto done;

loser:
  if (len && *len) 
    *len = 0;
  if (value && *value) 
    *value = NULL;

done:
  return rv;
}

SSMStatus
SSM_CertRenewalHandler(HTTPRequest * req)
{
	SSMResourceCert * target = (SSMResourceCert *)req->target;
	char * value;
	SSMStatus rv;
  
	/* Renew cert action */
	rv = SSM_HTTPParamValue(req, "action", &value);
	if (rv != SSM_SUCCESS) 
		goto done;
	if (PL_strcmp(value, "now") == 0) {
		target->m_renewCert = PR_TRUE;
	} else {
		target->m_renewCert = PR_FALSE;
	}
done:
	SSM_NotifyUIEvent((SSMResource *)target);
	return SSM_SUCCESS; 
}

SSMStatus
SSMResourceCert_FormSubmitHandler(SSMResource *res, HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  char* tmpStr = NULL;
  
  /* make sure you got the right baseRef */
  rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
  if (rv != SSM_SUCCESS ||
      PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
    goto loser;
  }
  
  rv = SSM_HTTPCloseAndSleep(req);
  if (rv != SSM_SUCCESS)
    SSM_DEBUG("Errors closing window in FormSubmitHandler: %d\n", rv);
  
  if (!res->m_formName)
    goto loser;
  if (PL_strcmp(res->m_formName, "cert_edit") == 0)
    rv = SSM_EditCertificateTrustHandler(req);
  else if (PL_strcmp(res->m_formName, "trust_new_ca") == 0)
    rv = SSM_CertCAImportCommandHandler2(req);
  else if (PL_strcmp(res->m_formName, "cert_renewal") == 0) {
	rv = SSM_CertRenewalHandler(req);
  } else {
    SSM_DEBUG("CertResource_FormsubmitHandler: bad formName %s\n", res->m_formName);
  }
    
loser:
  return rv;
}

static
certPane SSMUI_CertBelongs(CERTCertificate * cert)
{
  CERTCertTrust * trust;
  certPane owner = badCert;

  if (!cert || !cert->trust) 
    goto done;
  trust = cert->trust;

  if ((trust->sslFlags & CERTDB_USER) ||
      (trust->emailFlags & CERTDB_USER) ||
      (trust->objectSigningFlags & CERTDB_USER))
    owner = myCert;
  else if ((trust->sslFlags & CERTDB_VALID_CA) ||
	   (trust->emailFlags & CERTDB_VALID_CA) ||
	   (trust->objectSigningFlags & CERTDB_VALID_CA))
    owner = caCert;
  else if (trust->sslFlags & CERTDB_VALID_PEER) 
    owner = webCert;
  else if (trust->emailFlags & CERTDB_VALID_PEER) 
    owner = othersCert;
 done:
  return owner;
}

/* 
 * Find correct help target for the kind of cert we're deleting.
 */
SSMStatus SSM_DeleteCertHelpKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  certPane kind = badCert;
  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  PR_ASSERT(SSM_IsAKindOf(cx->m_request->target, SSM_RESTYPE_CERTIFICATE));

  kind = SSMUI_CertBelongs(((SSMResourceCert *)cx->m_request->target)->cert);
  switch (kind) {
  case caCert:
    rv = SSM_GetAndExpandText(cx, "help_delete_ca", &cx->m_result);
    break;
  case myCert:
    rv = SSM_GetAndExpandText(cx, "help_delete_mine", &cx->m_result);
    break;
  case webCert:
    rv = SSM_GetAndExpandText(cx, "help_delete_websites", &cx->m_result);
    break;
  case othersCert:
    rv = SSM_GetAndExpandText(cx, "help_delete_others", &cx->m_result);
    break;
  default:
    SSM_DEBUG("DeleteCertHelpKeyword: can't figure out cert type!\n");
  }
  return rv;
}

SSMStatus SSM_DeleteCertWarnKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  certPane kind = badCert;

  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  PR_ASSERT(SSM_IsAKindOf(cx->m_request->target, SSM_RESTYPE_CERTIFICATE));

  kind = SSMUI_CertBelongs(((SSMResourceCert *)cx->m_request->target)->cert);
  switch (kind)  {
  case othersCert:
    rv = SSM_GetAndExpandText(cx, "delete_cert_warning_others", &cx->m_result);
    break;
  case myCert:
    rv = SSM_GetAndExpandText(cx, "delete_cert_warning_mine", &cx->m_result);
    break;
  case caCert:
    rv = SSM_GetAndExpandText(cx, "delete_cert_warning_ca", &cx->m_result);
    break;
  case webCert:
    rv = SSM_GetAndExpandText(cx, "delete_cert_warning_web", &cx->m_result);
    break;
  default:
    SSM_DEBUG("DeleteCertWarnKeyword: can't figure out cert type!\n");  
  }
  return rv;
}
  


SSMStatus SSM_DeleteCertHandler(HTTPRequest * req)
{
  SSMStatus rv;
  char * value = NULL;
  char * nickname = NULL;
  
  /* if this brakes, we're in deep trouble */
  PR_ASSERT(SSM_IsAKindOf(req->target, SSM_RESTYPE_CERTIFICATE));

  /* close the window */
  rv = SSM_HTTPCloseAndSleep(req);
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("DeleteCertHandler: Problem closing DeleteCertificateWindow!\n");
 
  /* check which button was clicked */
  rv = SSM_HTTPParamValue(req, "do_cancel", &value);
  if (rv == SSM_SUCCESS) {
    req->ctrlconn->super.super.m_buttonType = SSM_BUTTON_CANCEL;
    goto done;
  }
  
  rv = SSM_HTTPParamValue(req, "do_ok", &value);
  if (rv == SSM_SUCCESS) {
    if (((SSMResourceCert *)req->target)->cert->nickname)
      nickname = PL_strdup(((SSMResourceCert *)req->target)->cert->nickname);
    else nickname = PL_strdup(((SSMResourceCert *)req->target)->cert->emailAddr);
    rv = (SSMStatus) SSM_DeleteCertificate((SSMResourceCert *)req->target);
    /* delete this cert from Security Advisor hashtable */
    rv = SSM_ChangeCertSecAdvisorList(req, nickname, (ssmCertHashAction)-1);
    PR_Free(nickname);
    goto done;
  }
  SSM_DEBUG("DeleteCertHandler: can't figure out which button was clicked in DeleteCert dialog!\n");
    
done:
  SSM_NotifyUIEvent(&req->ctrlconn->super.super);
  return rv;
}

SSMStatus SSM_ProcessCertDeleteButton(HTTPRequest * req)
{
  SSMResource * target;
  char * params = NULL;
  char * certNickname = NULL, * formName = NULL;
  char * page = NULL, * outPage = NULL;
  SSMStatus rv = SSM_FAILURE;

  rv = SSM_HTTPParamValue(req, "selectCert", &certNickname);
  if (rv != SSM_SUCCESS) 
    goto loser;
  
  rv = SSM_HTTPParamValue(req, "formName", &formName);
  if (rv != SSM_SUCCESS) 
    goto loser;
  
  /* Get the target resource. */
  target = (req->target ? req->target : (SSMResource *) req->ctrlconn);
  PR_ASSERT(target);
  
  params = PR_smprintf("action=delete_cert&nick=%s&formName=%s",certNickname, 
		       formName);
  SSM_LockUIEvent(&req->ctrlconn->super.super);
  rv = SSMControlConnection_SendUIEvent(req->ctrlconn, "cert", "delete_cert", 
					target, params, 
					&target->m_clientContext, PR_TRUE);
  SSM_WaitUIEvent(&req->ctrlconn->super.super, PR_INTERVAL_NO_TIMEOUT);
  /* See if the user canceled, if so send back HTTP_NO_CONTENT
   * so security advisor doesn't redraw the same content.
   */
  if (req->ctrlconn->super.super.m_buttonType == SSM_BUTTON_CANCEL) {
    SSM_HTTPReportError(req, HTTP_NO_CONTENT);
    goto done;
  }  
 /* tell the secadvisor page to reload */
  rv = SSM_RefreshRefererPage(req);
  goto done;
  
loser:
  if (rv == SSM_SUCCESS) 
    rv = SSM_FAILURE;
  SSM_HTTPReportSpecificError(req, 
			      "ProcessDeleteCert: can't send/process delete cert UIEvent", rv);

done:
  PR_FREEIF(params);
  PR_FREEIF(page);
  PR_FREEIF(outPage);
  return rv;
}

SSMStatus SSM_RefreshRefererPage(HTTPRequest * req)
{
  SSMTextGenContext * cx = NULL;
  SSMStatus rv = SSM_FAILURE;
  char * page = NULL, * outPage = NULL, * ptr = NULL;

  if (!req) 
    goto done;
  
  /* ptr will point the last '/' in the referer URL. We are interested 
   * in everything AFTER the last '/'.
   */
  ptr = strrchr(req->referer, '/');
  
  rv = SSMTextGen_NewTopLevelContext(req, &cx);
  rv = SSM_GetAndExpandText(cx, "refresh_window_content", &page);
  SSMTextGen_DestroyContext(cx);
  outPage = PR_smprintf(page, ptr+1);
  req->sentResponse = PR_TRUE;
  rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
  rv = SSM_HTTPSendUTF8String(req, outPage);

 done:
  return rv;
}


SSMStatus SSM_HTTPCertListHandler(HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  char * nick = NULL, * action = NULL, *target, * page=NULL, *outPage = NULL;
  char * nickhtml = NULL, * formName = NULL;
  CERTCertificate * cert;
  CERTCertList * certList = NULL;
  SSMTextGenContext * cx; 
  PRBool emailCert = PR_FALSE;
  char * certres;
  SSMResource * certresource;
  SSMResourceCert * certRes;
  SSM_DEBUG("In cert_list handler\n");

  /* figure out the certificate  */

  /* this is a cert identified by resource id */
  rv = SSM_HTTPParamValue(req, "certresource", &certres);
  if (rv == SSM_SUCCESS) {
    rv = SSM_RIDTextToResource(req, certres, &certresource);
    if (rv != SSM_SUCCESS || !certresource 
	|| !(SSM_IsAKindOf(certresource, SSM_RESTYPE_CERTIFICATE))) {
      SSM_DEBUG("certListHandler:can't find cert by resource ID %s!\n", certres);
	  goto loser;
	}

    certRes = ((SSMResourceCert *)certresource);
    rv = SSM_ProcessCertResourceUIAction(req, certRes);
    goto done;
  }

  rv = SSM_HTTPParamValue(req, "target", &target);
  if (rv != SSM_SUCCESS) 
    /* can't find target */
    goto loser;
  
  rv = SSM_HTTPParamValue(req, "nick", &nick);
  if (rv != SSM_SUCCESS || PL_strcmp(nick,"undefined")==0) {
    /* can't find cert selection */
    SSM_DEBUG("certListHandler: can't find cert nick!\n");
    goto loser;
  }

  nickhtml = SSM_ConvertStringToHTMLString(nick);
  if (!nickhtml) {
    SSM_DEBUG("HTTPCertListHandler: error in ConvertStringToHTMLString\n");
    goto loser;
  }
  rv = SSM_HTTPParamValue(req, "formName", &formName);
  if (rv != SSM_SUCCESS || !formName) {
    SSM_DEBUG("certListHandler: no originating formName found!\n");
    goto loser;
  }
  
  /* look for certs by nickname, unless it's "others", 
   * then check email address
   */
  if (strstr(formName,"_others")) 
    emailCert = PR_TRUE;

  certList = SSMControlConnection_CreateCertListByNickname(req->ctrlconn, 
							   nick, 
							   emailCert);
  if (certList && SSM_CertListCount(certList) > 1)
    { /* more than one cert under the same nickname */
             
      /* get current values */
      rv = SSM_HTTPParamValue(req, "action", &action);
      if (rv != SSM_SUCCESS) {
	/* can't find action selection */
	goto loser;
      }

      rv = SSMTextGen_NewTopLevelContext(req, &cx);
      rv = SSM_GetAndExpandText(cx, "choose_cert_content", &page);
      SSMTextGen_DestroyContext(cx);
      outPage = PR_smprintf(page, atoi(target), action, nickhtml, formName);
      rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
      rv = SSM_HTTPSendUTF8String(req, outPage);
      
    }
  else 
    {
      /* get selected certificate */
      cert = SSMControlConnection_FindCertByNickname(req->ctrlconn, nick, 
						     emailCert);
      if (!cert) {
	SSM_DEBUG("HTTPCertListHandler: can't find cert with nick %s!\n",nick);
	goto loser;
      }
      rv = SSM_ProcessCertUIAction(req, cert);
    }  
done:
  PR_FREEIF(page);
  PR_FREEIF(outPage);
  PR_FREEIF(nickhtml);
  if (certList) 
    CERT_DestroyCertList(certList);
 
  return rv;

loser:
  if (certList) 
    CERT_DestroyCertList(certList);
  
  /* kill the window */
  rv = SSMTextGen_NewTopLevelContext(req, &cx);
  rv = SSM_GetAndExpandText(cx, "windowclose_doclose_js_content", &page);
  SSMTextGen_DestroyContext(cx);
  req->sentResponse = PR_TRUE;
  rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
  rv = SSM_HTTPSendUTF8String(req, page);
  /* notify owner if this a UI event */
  if (((SSMResource *)req->ctrlconn)->m_UILock) 
    SSM_NotifyUIEvent((SSMResource *)req->ctrlconn);
  
  return SSM_SUCCESS;

}

SSMStatus SSM_ProcessCertResourceUIAction(HTTPRequest * req, 
					  SSMResourceCert * certres)
{
  SSMResourceID resID;
  SSMStatus rv = SSM_FAILURE;
  char * page = NULL, *outPage = NULL;
  char * action, *ref;
  SSMTextGenContext * cx = NULL;

  /* Free reference to the previous target
   */
  if (req->target) {
    SSM_FreeResource(req->target);
  }
  req->target = &certres->super;
  /*
   * Sigh, this is so hacked up that the only way I can prevent a crash now
   * is to get an extra reference here so the target doesn't disappear.
   * -javi
   */
  SSM_GetResourceReference(req->target);
  resID = certres->super.m_id;
  /* figure out what action was requested */
  rv = SSM_HTTPParamValue(req, "action", &action);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("ProcessCertUIAction: no action parameter found!\n");
    SSM_HTTPReportError(req, HTTP_NO_CONTENT);
    goto done;
  }
  ref = PR_smprintf("%s_content", action);
  rv = SSMTextGen_NewTopLevelContext(req, &cx);
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("ProcessUIAction: can't get new TopLevelContext\n");
  rv = SSM_GetAndExpandText(cx, ref, &page);
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("ProcessUIAction:can't get wrapper for %s\n", action);
  SSMTextGen_DestroyContext(cx);
  outPage = PR_smprintf(page, resID, action, "bogus");
  rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
  rv = SSM_HTTPSendUTF8String(req, outPage);

 done:
  PR_FREEIF(page);
  PR_FREEIF(outPage);
  return rv;
}    


SSMStatus
SSM_ProcessCertUIAction(HTTPRequest * req, CERTCertificate * cert)
{
  SSMResourceCert * certres = NULL;
  SSMStatus rv;
  SSMResourceID resID;

  if (!cert) {
    SSM_DEBUG("ProcessCertUIAction: no cert. Either user hit Cancel or smth is wrong.\n");
    SSM_DEBUG("Close the window \n");    
    goto done;
  }

  /* get a resource for this cert */
  rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, (void *)cert, 
			  req->ctrlconn, &resID, (SSMResource**)&certres); 
  if (rv != SSM_SUCCESS) 
    goto done;  
  /*
   * XXX This is a gross hack.  But, iff the previous target was another
   *     certificate, release the reference so that reference counts will
   *     stay in sync.
   */
  if (SSM_IsAKindOf(req->target, SSM_RESTYPE_CERTIFICATE)) {
    SSM_FreeResource(req->target);
  }
  return SSM_ProcessCertResourceUIAction(req, certres);
 done:
  return rv;
}


char * digits = "0123456789ABCDEF";
#define HEX_DIGIT(x)  (strchr(digits, *x) - digits)

SECItem *
unhexify(char * hex) 
{
  SECItem * result = NULL;
  char * data = NULL;
  char * ptr, *ch;
  PRIntn len;
  
  if (!hex)
    return NULL;

  result = (SECItem *) PORT_ZAlloc(sizeof(SECItem));

  ch = hex;
  len = strlen(hex)/2;
  if (strlen(hex)&1) 
    len++;

  ptr = data = (char *) PORT_ZAlloc(len);
  if (strlen(hex)&1) {
    *ptr = HEX_DIGIT(ch);
    ptr++;
    ch++;
  }


  while (*ch) { 
    *ptr = (HEX_DIGIT(ch) << 4) + HEX_DIGIT((ch+1));
    ch += 2;
    ptr++;
  }
  result->data = (unsigned char *) data;
  result->len = len;
  return result;
}
 
/*
 * Here's how we do unhexify -jp
 * Number[J]=HexDigit(*P)*16+HexDigit(*(P+1));
 * J++;
 * P+=2;
 * char *Digits="012345679ABCDEF"
 *return(strchr(Digits,toupper(Char))-Digits)
 */ 

SSMStatus SSM_ChooseCertUsageHandler(HTTPRequest * req)
{
  SSMResource * target = NULL, *origTarget;
  char * value = NULL, * nick = NULL, *serialNum;
  char * tmp;
  SSMStatus rv = SSM_FAILURE;
  SECItem * keyID;
  CERTCertificate * cert = NULL;
  SSMTextGenContext * cx;
  char * urlStr=NULL, *action=NULL, * redirectHTML=NULL, * outStr=NULL;
  SSMResourceID resID;
  char * windowName = NULL, * params = NULL;

  /* Get the target resource. */
  target = (req->target ? req->target : (SSMResource *) req->ctrlconn);
  PR_ASSERT(target);
  origTarget = target;
  rv = SSM_HTTPParamValue(req, "selectItem", &value);
  if (rv != SSM_SUCCESS) 
    goto loser;

  rv = SSM_HTTPParamValue(req, "nick", &nick);
  if (rv != SSM_SUCCESS)
    goto loser;
  
  rv = SSM_HTTPParamValue(req, "baseRef", &action);
  if (rv != SSM_SUCCESS)
    goto loser;
  
  tmp = PL_strdup(value);
  value = strtok(tmp, ":");
  serialNum = strtok(NULL, ":");
  keyID = unhexify(value);
  cert = FindCertByKeyIDAndNickname(req->ctrlconn, nick, keyID, serialNum);
  SECITEM_FreeItem(keyID,PR_TRUE);
  PR_Free(tmp);
  
  SSM_DEBUG("ChooseCertUsageHandler: found certificate %lx!\n", cert);

  req->ctrlconn->super.super.m_uiData = (void *)cert;
  
  /* set the window name:
   * View is done through http thread, window name is popup
   * Delete is done with UIEvent, window name is PSM
   */
  if (PL_strcmp(action, "cert_view") == 0 || 
      PL_strcmp(action, "ca_policy_view") == 0) 
    windowName = PR_smprintf("popup");
  else if (PL_strcmp(action, "delete_cert") == 0)
    windowName = PR_smprintf("PSM");
  else if (PL_strcmp(action, "backup") == 0)
    windowName = NULL;
  else if (PL_strcmp(action, "cert_edit") == 0)
    windowName = PR_smprintf("popup");
  else {
    SSM_DEBUG("ChooseCertUsageHandler: bad action baseRef = %s", action);
    goto loser;
  }
  
  /* make sure the next URL takes up the whole window */
  rv = SSMTextGen_NewTopLevelContext(req, &cx);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("ChooseCertUsageHandler: can't get new textGen context\n");
    goto loser;
  }
  rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, (void *)cert, 
			  req->ctrlconn, &resID, &target);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("ChooseCertUsageHandler: can't create cert resource \n");
    goto loser;
  }
  if (!MIN_STRCMP(action, "backup")) {
    rv = SSM_GetAndExpandText(cx, "windowclose_doclose_js_content", &outStr);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    SSM_NotifyUIEvent(origTarget);
  } else {

    /* policy view requires extra parameters */
    if (PL_strcmp(action, "ca_policy_view") == 0)
      params = PR_smprintf("&certresource=%d&bogus=bogus", resID);
    
    urlStr = PR_smprintf("get?baseRef=%s&target=%d%s", action, resID, params?params:"");
 
    rv = SSM_GetAndExpandText(cx, "refresh_frameset_content", &redirectHTML);
    if (rv != SSM_SUCCESS)
      SSM_DEBUG("ChooseCertUsageHandler: can't create redirectHTML");
    outStr = PR_smprintf(redirectHTML, urlStr, windowName);
    SSMTextGen_DestroyContext(cx);
    cx = NULL;
    PR_Free(urlStr);
    PR_Free(redirectHTML);
  }
  req->sentResponse = PR_TRUE;
  rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("ChooseCertUsageHandler: error sending OKHeaders\n");
  rv = SSM_HTTPSendUTF8String(req, outStr);
  if (rv != SSM_SUCCESS)
    SSM_DEBUG("ChooseCertUsageHandler: error sending <%s>\n", outStr);
  PR_FREEIF(windowName);
  PR_Free(outStr);
  if (cx != NULL)
    SSMTextGen_DestroyContext(cx);
  return rv;
loser:  
  if (cx != NULL)
    SSMTextGen_DestroyContext(cx);
  if (rv == SSM_SUCCESS)
    rv = SSM_FAILURE;
  SSM_DEBUG("ChooseCertUsageHandler: somethings is very wrong!\n");
  return SSM_HTTPCloseWindow(req);
 }

CERTCertificate *
FindCertByKeyIDAndNickname(SSMControlConnection * ctrl, char *nickname, 
			   SECItem *keyID, char * serial)
{
  CERTCertificate *cert = NULL;
  CERTCertList *certList = NULL;
  CERTCertListNode *node;
  char *hexSerial=NULL;

  certList = SSMControlConnection_CreateCertListByNickname(ctrl, nickname, 
							   PR_FALSE);
  if (!certList)  /* could not find certs with this nick, try email address */
    certList = SSMControlConnection_CreateCertListByNickname(ctrl, nickname, 
							     PR_TRUE);  
  if (!certList) 
    goto loser;

  node = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(node, certList)) {
    if (cert_GetKeyID(node->cert) != SECSuccess) 
      goto loser;
    hexSerial = CERT_Hexify(&node->cert->serialNumber, 0);
    if ( SECITEM_CompareItem(keyID, &node->cert->subjectKeyID) == SECEqual &&
	 (hexSerial == NULL ||  
	  strcmp(hexSerial, serial) == 0)) { 
      PR_FREEIF(hexSerial);   
      goto found;
    }
    PR_FREEIF(hexSerial);
    node = CERT_LIST_NEXT(node);
  }
  
  SSM_DEBUG("FindCertByKeyIDAndNickname: could not find certificate!\n");
  goto loser;

found:
  cert = CERT_DupCertificate(node->cert);

loser:
  if (certList) 
    CERT_DestroyCertList(certList);
  return cert;

}

static SSMStatus
SSM_DoOCSPError(SSMTextGenContext *cx, CERTCertificate *cert,
		const char* textKey)
{
    SSMStatus rv;
    char *tmp=NULL, *url=NULL;

    rv = SSM_GetAndExpandText(cx, textKey, &tmp);
    if (rv == SSM_SUCCESS) {
        PR_FREEIF(cx->m_result);
        url = SSM_GetOCSPURL(cert, cx->m_request->target->m_connection->m_prefs);
        PR_ASSERT(url != NULL);
	cx->m_result = PR_smprintf(tmp, url);
        PR_Free(url); 
    }
    PR_FREEIF(tmp);
    return rv;
}

static SSMStatus
SSM_BuildErrorMessage(SSMTextGenContext *cx, SSMResourceCert *certres)
{
    PRErrorCode err;
    SECCertUsage certUsage;
    SSMStatus rv;

    if (ssm_cert_belongs_type(certres->cert, USER_CERT) == SSM_SUCCESS) {
      certUsage = certUsageEmailRecipient;
    } else 
    if (ssm_cert_belongs_type(certres->cert, EMAIL_CERT) == SSM_SUCCESS) {
      certUsage = certUsageEmailRecipient;
    } else 
    if (ssm_cert_belongs_type(certres->cert, CA_CERT) == SSM_SUCCESS) {
      certUsage = certUsageSSLCA;
    } else 
    if (ssm_cert_belongs_type(certres->cert, WEBSITE_CERT) == SSM_SUCCESS) {
      certUsage = certUsageSSLServer;
    }
    
    if (SSM_VerifyCert(certres, certUsage) != SECSuccess) {
      err = PR_GetError();
      switch (err) {
      case SEC_ERROR_EXPIRED_CERTIFICATE:
	rv = SSM_GetAndExpandText(cx, "not_verified_expired_cert_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_REVOKED_CERTIFICATE:
	rv = SSM_GetAndExpandText(cx, "not_verified_revoked_cert_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_UNKNOWN_ISSUER:
	rv = SSM_GetAndExpandText(cx, "not_verified_unknown_issuer_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_CA_CERT_INVALID:
	rv = SSM_GetAndExpandText(cx, "not_verified_ca_invalid_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_UNTRUSTED_ISSUER:
	rv = SSM_GetAndExpandText(cx, "not_verified_untrusted_issuer_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
	rv = SSM_GetAndExpandText(cx, "not_verified_expired_issuer_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_UNTRUSTED_CERT:
	rv = SSM_GetAndExpandText(cx, "not_verified_untrusted_cert_text", 
				  &cx->m_result);
	break;
      case SEC_ERROR_CERT_BAD_ACCESS_LOCATION:
	rv = SSM_DoOCSPError(cx, certres->cert, "not_verified_bad_ocsp_url");
	break;
      case SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE:
	rv = SSM_DoOCSPError(cx, certres->cert, 
			     "not_verified_unknown_ocsp_response_type");
	break;
      case SEC_ERROR_OCSP_BAD_HTTP_RESPONSE:
	rv = SSM_DoOCSPError(cx, certres->cert,
			     "bad_ocsp_http_response");
	break;
      case SEC_ERROR_OCSP_SERVER_ERROR:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_server_error");
	break;
      case SEC_ERROR_OCSP_MALFORMED_REQUEST:
	rv = SSM_DoOCSPError(cx, certres->cert, "bad_ocsp_der");
	break;
      case SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_unauthorized_responder");
	break;
      case SEC_ERROR_OCSP_TRY_SERVER_LATER:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_try_server_later");
	break;
      case SEC_ERROR_OCSP_REQUEST_NEEDS_SIG:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_response_needs_sig");
	break;
      case SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_unauthorized_request");
	break;
      case SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_unknown_status");
	break;
      case SEC_ERROR_OCSP_UNKNOWN_CERT:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_unknown_cert");
	break;
      case SEC_ERROR_OCSP_NOT_ENABLED:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_not_enabled");
	break;
      case SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_no_default_responder");
	break;
      case SEC_ERROR_OCSP_MALFORMED_RESPONSE:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_malformed_response");
	break;
      case SEC_ERROR_OCSP_FUTURE_RESPONSE:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_future_response");
	break;
      case SEC_ERROR_OCSP_OLD_RESPONSE:
	rv = SSM_DoOCSPError(cx, certres->cert, "ocsp_old_response");
	break;
      case PR_DIRECTORY_LOOKUP_ERROR:
	/* Add some extra logic because NSS doesn't always set error 
	 * codes for OCSP failures.
	 */
	if (SSM_IsOCSPEnabled(certres->super.m_connection)) {
	    rv = SSM_DoOCSPError(cx, certres->cert, 
				 "not_verified_bad_ocsp_url");
	    break;
	}
      default:
	rv = SSM_GetAndExpandText(cx, "not_verified_unknown_error_text", 
				  &cx->m_result);
      }
    } else {
      rv = SSM_GetAndExpandText(cx, "not_verified_unknown_error_text", 
				&cx->m_result);
    }
    return rv;
}

SSMStatus
SSM_VerifyCertKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  SSMResourceCert * certres = NULL;
  /* All the certUsage values currently defined in NSS */
  /* All of these strings should come from the properties files. */
  char * formatKey = NULL, * fmt = NULL;
  PRBool verified[12];
  char * result = NULL;
  PRInt32 i, j;
  PRBool valid = PR_FALSE;

  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  PR_ASSERT(SSM_IsAKindOf(cx->m_request->target, SSM_RESTYPE_CERTIFICATE));

  /* get certificate resource */
  certres = (SSMResourceCert *)cx->m_request->target;

  for (i = 0, j= 0; i < certUsageAnyCA + 1; i++) {
    /* UserCertImport, ProtectedObjectSigner, AnyCA, VerifyCA certUsages 
     * cause NSS to panic, make sure we don't try to verify it.
     */
    if (i == certUsageUserCertImport ||
	i == certUsageProtectedObjectSigner ||
	i == certUsageVerifyCA ||
	i == certUsageAnyCA) {
      verified[i] = PR_FALSE;
      continue;
    }
    if (SSM_VerifyCert(certres, (SECCertUsage) i) == SECSuccess) {
      verified[i] = PR_TRUE;
      valid = PR_TRUE;
    }
    else verified[i] = PR_FALSE;
  }
  
  if (valid) {
    rv = SSM_GetAndExpandText(cx, "verified_prefix", 
			      &cx->m_result);
  } else {
    rv = SSM_BuildErrorMessage(cx, certres);
  }
  formatKey = (char *) SSM_At(cx->m_params, (PRIntn)0);
  rv = SSM_GetAndExpandTextKeyedByString(cx, formatKey, &fmt);
  if (rv != SSM_SUCCESS) 
    goto loser;
  result = PR_smprintf(fmt, verified[0], verified[1], verified[2],
		       verified[3], verified[4], verified[5],
		       verified[6], verified[7], verified[8],
		       verified[9], verified[10], verified[11], valid);
  rv = SSM_ConcatenateUTF8String(&cx->m_result, result);
  PR_Free(result);
  rv = SSM_SUCCESS;
  goto done;
  
 loser:
  SSM_DEBUG("VerifyCertKeywordHandler: something is wrong!\n");
  if (rv == SSM_SUCCESS) 
    rv = SSM_FAILURE;
  if (cx->m_result) 
    PR_Free(cx->m_result);
  cx->m_result = NULL;
 done:
  PR_FREEIF(fmt);
  return rv;
}


SSMStatus 
SSM_EditCertKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv;
  SSMResourceCert * target = (SSMResourceCert *)SSMTextGen_GetTargetObject(cx);
  CERTCertTrust * trust;
  char * tmpStr = NULL, *checked = NULL, *notAvailable = NULL, * donot= NULL;
  PRBool trusted=PR_FALSE, trustca = PR_FALSE;
  PRBool emailtrust=PR_FALSE, signtrust=PR_FALSE, ssltrust = PR_FALSE; 
/* edit cert dialog help targets */
  char * emailCertHelpTarget = "1036027";
  char * sslCertHelpTarget   = "1035916";
  char * caCertHelpTarget    = "1036857";
 

  PR_ASSERT(target && SSM_IsAKindOf((SSMResource *)target, SSM_RESTYPE_CERTIFICATE));
  PR_ASSERT(target->cert);
  
  rv = SSM_GetAndExpandTextKeyedByString(cx, "text_checked", &checked);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("EditCertKeywordHandler: can't get text for 'checked'\n");
    goto done;
  }
  rv = SSM_GetAndExpandTextKeyedByString(cx, "trust_do_not", &donot);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("EditCertKeywordHandler: can't get text for 'trust_do_not'\n");
    goto done;
  }
  
  rv = SSM_GetAndExpandTextKeyedByString(cx, "text_not_available", 
					 &notAvailable);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("EditCertKeywordHandler: can't get text for notAvailable\n");
    goto done;
  }
  if ((trust = target->cert->trust) == NULL) {
    SSM_DEBUG("EditCertKeywordHandler: cert trust object is NULL!\n");
    rv = SSM_FAILURE;
    goto done;
  }
  
  /* website cert */
  if (trust->sslFlags & CERTDB_VALID_PEER) { 
    CERTCertificate * issuer = CERT_FindCertIssuer(target->cert, PR_Now(), 
						 certUsageAnyCA);
    if (issuer && issuer->trust && 
	(issuer->trust->sslFlags & CERTDB_TRUSTED_CA))
      trustca = PR_TRUE;

    rv = SSM_GetAndExpandTextKeyedByString(cx, "edit_cert_website", &tmpStr);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("EditCertKeywordHandler: can't find edit_cert_website \n");
      goto done;
    }
    /* check first button if trusted, second if not */
    if (trust->sslFlags & CERTDB_TRUSTED)
      trusted = PR_TRUE;
    PR_FREEIF(cx->m_result); 
    cx->m_result = PR_smprintf(tmpStr, target->cert->nickname, 
			       CERT_GetCommonName(&target->cert->issuer),
			       issuer?issuer->nickname:notAvailable,
			       trusted?checked:"", 
			       trusted?"":checked,
			       sslCertHelpTarget,
			       trustca?"":donot,
			       trustca?"":donot);
    
  } else if ( (trust->sslFlags & CERTDB_VALID_CA)    ||
	      (trust->emailFlags & CERTDB_VALID_CA ) ||
	      ( trust->objectSigningFlags & CERTDB_VALID_CA )) {
 
   /* security advisor in 4.x does this, should we? */
    if ( ! ( target->cert->nsCertType & NS_CERT_TYPE_CA ) ) 
      target->cert->nsCertType = NS_CERT_TYPE_CA;
    
    rv = SSM_GetAndExpandTextKeyedByString(cx, "edit_cert_authority", &tmpStr);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("EditCertKeywordHandler: can't get edit_cert_authority wrapper\n");
      goto done;
    }
    
    /* check first button if trusted, second if not */
    if (trust->sslFlags & CERTDB_TRUSTED_CA)
      ssltrust = PR_TRUE;
    if (trust->emailFlags & CERTDB_TRUSTED_CA)
      emailtrust = PR_TRUE;
    if (trust->objectSigningFlags & CERTDB_TRUSTED_CA ) 
      signtrust = PR_TRUE;
    
    PR_FREEIF(cx->m_result);
    cx->m_result = PR_smprintf(tmpStr, ssltrust?checked:"", 
			       emailtrust?checked:"",
			       signtrust?checked:"", 
			       target->cert->nickname,
			       caCertHelpTarget);
  } else if (trust->emailFlags & CERTDB_VALID_PEER) {
    CERTCertificate * issuer = CERT_FindCertIssuer(target->cert, PR_Now(), 
						 certUsageAnyCA);
    char *CN = NULL;
    if (issuer && issuer->trust && 
	(issuer->trust->emailFlags & CERTDB_TRUSTED_CA))
      trustca = PR_TRUE;

    rv = SSM_GetAndExpandTextKeyedByString(cx, "edit_cert_others", &tmpStr);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("EditCertKeywordHandler: can't get edit_cert_authority wrapper\n");
      goto done;
    }
    if (trust->emailFlags & CERTDB_TRUSTED) 
      trusted = PR_TRUE;
    PR_FREEIF(cx->m_result);
    CN = CERT_GetCommonName(&target->cert->issuer);
    cx->m_result = PR_smprintf(tmpStr, target->cert->emailAddr, 
			       CN,
			       issuer?issuer->nickname:notAvailable,
			       trusted?checked:"", 
			       trusted?"":checked,
			       emailCertHelpTarget,
			       trustca?"":donot,
			       trustca?"":donot);
    PR_FREEIF(CN);
  } else {
    PR_FREEIF(cx->m_result);
    cx->m_result = PR_smprintf("Bad certificate type, don't know how to edit.\n");
  }
    
done:
  PR_FREEIF(tmpStr);
  PR_FREEIF(checked);
  PR_FREEIF(donot);
  return rv;
}

SSMStatus
SSM_EditCertificateTrustHandler(HTTPRequest * req)
{
  SSMResourceCert * target = (SSMResourceCert *)req->target;
  CERTCertTrust trust; 
  char * value;
  SSMStatus rv;
  
  PR_ASSERT(target && target->cert);
  rv = (SSMStatus) CERT_GetCertTrust(target->cert, &trust);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("EditCertificateTrustHandler: can't get cert trust object\n");
    goto done;
  }
  
  if (trust.sslFlags & CERTDB_VALID_PEER) {
    /* this is website cert we're editing */
    rv = SSM_HTTPParamValue(req, "trustoption", &value);
    if (rv != SSM_SUCCESS) 
      goto done;
    if (strcmp(value, "trust") == 0)
      trust.sslFlags |= CERTDB_TRUSTED;
    else trust.sslFlags &= (~CERTDB_TRUSTED);
  } 
  else 
    if ( (trust.sslFlags & CERTDB_VALID_CA)    ||
	 (trust.emailFlags & CERTDB_VALID_CA ) ||
	 ( trust.objectSigningFlags & CERTDB_VALID_CA ) 
	 ) {
      /* security advisor in 4.x does this, should we? */
      if ( ! ( target->cert->nsCertType & NS_CERT_TYPE_CA ) ) 
	target->cert->nsCertType = NS_CERT_TYPE_CA;
      
      if (target->cert->nsCertType & NS_CERT_TYPE_SSL_CA)
	trust.sslFlags |= CERTDB_VALID_CA;
      if (target->cert->nsCertType & NS_CERT_TYPE_OBJECT_SIGNING_CA)
	trust.objectSigningFlags |= CERTDB_VALID_CA;
      if (target->cert->nsCertType & NS_CERT_TYPE_EMAIL_CA)
	trust.emailFlags |= CERTDB_VALID_CA;

      rv = SSM_HTTPParamValue(req, "networksite", &value);
      if (rv == SSM_SUCCESS) 
	trust.sslFlags |= CERTDB_TRUSTED_CA;
      else trust.sslFlags &= (~CERTDB_TRUSTED_CA);
      
      rv = SSM_HTTPParamValue(req, "emailuser", &value);
      if (rv == SSM_SUCCESS) 
	trust.emailFlags |= CERTDB_TRUSTED_CA;
      else trust.emailFlags &= (~CERTDB_TRUSTED_CA);
      
      rv = SSM_HTTPParamValue(req, "software", &value);
      if (rv == SSM_SUCCESS) 
	trust.objectSigningFlags |= CERTDB_TRUSTED_CA;
      else trust.objectSigningFlags &= (~CERTDB_TRUSTED_CA);
    }
  else if (trust.emailFlags & CERTDB_VALID_PEER) 
    { 
      /* edit email cert */
      rv = SSM_HTTPParamValue(req, "trustoption", &value);
      if (rv != SSM_SUCCESS) 
	goto done;
      if (PL_strcmp(value, "trust") == 0)
	trust.emailFlags |= CERTDB_TRUSTED;
      else trust.emailFlags &= (~CERTDB_TRUSTED);
    }
  else 
    {
      SSM_DEBUG("EditCertificateTrustHandler: trying to edit bad cert\n");
      goto done;
    }
   rv = (SSMStatus) CERT_ChangeCertTrust(req->ctrlconn->m_certdb, target->cert, &trust);
   SSL_ClearSessionCache();

done:
   return SSM_SUCCESS; 
}

SSMStatus
SSM_SelectCertKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_SUCCESS;
  char * value = NULL, *formName = NULL;
  char * fmt = NULL;
  PRIntn PARAM_FORMAT = (PRIntn) 0;
  PRIntn certType, certKey;

  /* Check for parameter validity */
  PR_ASSERT(cx);
  PR_ASSERT(cx->m_request);
  PR_ASSERT(cx->m_params);
  PR_ASSERT(cx->m_result);
  if (!cx || !cx->m_request || !cx->m_params || !cx->m_result)
    {
      rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
      goto loser; 
    }

  rv = SSM_HTTPParamValue(cx->m_request, "origin", &formName);
  if (rv != SSM_SUCCESS)
    goto loser;
  
  /* figure which certs we're looking for */
  if (strstr(formName, "_mine")) {
    certType = USER_CERT;
    certKey = NICKNAME;
  } else if (strstr(formName, "_others")) {
    certType = EMAIL_CERT;
    certKey = EMAILADDR;
  } else if (strstr(formName, "_websites")) {
    certType = WEBSITE_CERT;
    certKey = NICKNAME;
  } else if (strstr(formName, "_authorities")) {
    certType = CA_CERT;
    certKey = NICKNAME;
  } else 
    SSM_DEBUG("SelectCertKeywordHandler: Bad request\n");
  
  /* get format */
  fmt = (char *) SSM_At(cx->m_params, PARAM_FORMAT);
  rv = SSM_HTTPParamValue(cx->m_request, "content", &value);
  if (!fmt || !value) {
    SSM_DEBUG("SelectCertKeywordHandler: can't get format or nickname!\n");
    goto loser;
  }

  rv = SSM_GetAndExpandTextKeyedByString(cx, fmt, &fmt);
  if (rv != SSM_SUCCESS) 
    goto loser;
  SSM_DebugUTF8String("cert selection format <%s>", fmt);

  
  rv = ssm_select_cert(cx, &cx->m_result, fmt, certType, certKey, value);
  goto done;
  
loser:
  PR_FREEIF(cx->m_result);
  cx->m_result = NULL;
  if (rv == SSM_SUCCESS) 
    rv = SSM_FAILURE;
done:  
  return rv;
}

SSMStatus
ssm_select_cert(SSMTextGenContext * cx, char ** result, char * fmt, 
		PRIntn type, PRIntn key,char *nickname) 
{
  CERTCertList * certList;
  CERTCertListNode * node;
  CERTCertificate * cert;
  char * tmpStr = NULL, * checked = NULL;
  SSMStatus rv = SSM_FAILURE;
  PRBool first = PR_TRUE;
  
  PR_ASSERT(result);
  PR_ASSERT(fmt);
  PR_ASSERT(nickname);
  
  if (!result || !fmt || !nickname) {
    SSM_DEBUG("Bad parameters in ssm_select_cert_nick\n");
    goto loser;
  }
  *result = NULL;
 
  /* get a list of certs with the given nick or email address*/
  switch (key) {
  case NICKNAME:
    certList = SSMControlConnection_CreateCertListByNickname(cx->m_request->ctrlconn, 
							     nickname, 
							     PR_FALSE);
    break;
  case EMAILADDR:
    certList = SSMControlConnection_CreateCertListByNickname(cx->m_request->ctrlconn, nickname, 
							     PR_TRUE);
    break;
  default:
    SSM_DEBUG("select_cert: bad cert key, must be NICKNAME or EMAILADDR\n");
    goto loser;
  }
  if (!certList) {
    SSM_DEBUG("select_cert: no certificate with nick %s in the db\n", 
	      nickname);
    goto done;
  }

  rv = SSM_GetAndExpandTextKeyedByString(cx, "text_checked", &checked);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("select_cert: can't get text for checked attribute\n");
    goto done;
  }

  /* traverse the list */
  node = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(node, certList))
    {
      cert = node->cert;
      node = CERT_LIST_NEXT(node);
      /* make sure it's correct type of cert */
      if (ssm_cert_belongs_type(cert, type) != SSM_SUCCESS) 
	continue;
      
      rv = ssm_create_select_cert_entry(cx, cert, &tmpStr, fmt, first?checked:""); 
      if (rv != SSM_SUCCESS) { 
	SSM_DEBUG("Could not create select_cert_entry\n");
	goto loser;
      }
      rv = SSM_ConcatenateUTF8String(result, tmpStr);
      if (rv != SSM_SUCCESS)
	goto loser;
      PR_Free(tmpStr);
      tmpStr = NULL;
      first = PR_FALSE;
    }/* end of loop on certs in the certList */
  goto done;
  
loser:
  if (rv == SSM_SUCCESS)
    rv = SSM_FAILURE;
  if (result && *result)
    PR_Free(*result);
  *result = NULL;
  
done:
  if (certList)
    CERT_DestroyCertList(certList);
  if (tmpStr) 
    PR_Free(tmpStr);
  return rv;
}

SSMStatus
ssm_create_select_cert_entry(SSMTextGenContext * cx, CERTCertificate * cert, 
			     char **result, char *fmt, char *checked) 
  {
    char * validNotBefore = NULL, * validNotAfter = NULL;
    char * purposeStr = NULL;
    char * valueStr = NULL, * serialNum = NULL;
    SSMStatus rv = SSM_FAILURE;
    
    PR_ASSERT(result);
    PR_ASSERT(fmt);
    PR_ASSERT(cert);
    if (!result || !fmt || !cert) {
      SSM_DEBUG("Bad params in ssm_create_select_cert_entry!\n");
      return SSM_FAILURE;
    }
    *result = NULL;
    /* now find info about this certificate */
    validNotBefore = DER_UTCDayToAscii(&cert->validity.notBefore);
    validNotAfter = DER_UTCDayToAscii(&cert->validity.notAfter);
    
    if ((cert->keyUsage & KU_KEY_ENCIPHERMENT) == KU_KEY_ENCIPHERMENT) 
      SSM_GetUTF8Text(cx, "key_encipherment", &purposeStr);
    else if ((cert->keyUsage & KU_DIGITAL_SIGNATURE) == KU_DIGITAL_SIGNATURE)
      SSM_GetUTF8Text(cx, "digital_signature", &purposeStr);
    else if ((cert->keyUsage & KU_KEY_AGREEMENT) == KU_KEY_AGREEMENT) 
      SSM_GetUTF8Text(cx, "key_agreement", &purposeStr);
    else {
      SSM_DEBUG("Can't find certificate usage!\n");
      goto loser;
    }
    rv = (SSMStatus) cert_GetKeyID(cert);
    if (rv != SECSuccess) 
      goto loser;
    valueStr = PR_smprintf("%s", CERT_Hexify(&(cert->subjectKeyID),
					     PR_FALSE));
    serialNum = CERT_Hexify(&cert->serialNumber, 0);
    if (!serialNum) 
      serialNum = PL_strdup("--");

    /* string ? 
     * <input type="radio" name="selectItem" value=keyusage> keyusage, etc.
     */
    *result = PR_smprintf(fmt, valueStr, purposeStr, validNotBefore, 
			  validNotAfter, checked, serialNum);
    goto done;
  loser:
    if (*result)
      PR_Free(result);
    *result = NULL;
  done:
    if (purposeStr) 
      PR_Free(purposeStr);
    if (valueStr)
      PR_Free(valueStr);
    if (validNotBefore)
      PR_Free(validNotBefore);
    if (validNotAfter)
      PR_Free(validNotAfter);
    PR_FREEIF(serialNum);
    return rv;
  }

SSMStatus
ssm_cert_belongs_type(CERTCertificate * cert, PRIntn type)
  {
    PR_ASSERT(cert);
    /* cert is not in perm database */
    if (!cert->trust) 
      goto loser;

    switch (type) {
    case USER_CERT:
      if ((cert->trust->sslFlags & CERTDB_USER) ||
	  (cert->trust->emailFlags & CERTDB_USER) ||
	  (cert->trust->objectSigningFlags & CERTDB_USER) )
        goto done;
      break;
    case EMAIL_CERT:
      if (cert->emailAddr && 
	  (ssm_cert_belongs_type(cert, USER_CERT) != SSM_SUCCESS ) &&
	  (cert->trust->emailFlags & CERTDB_VALID_PEER))
	goto done;
      break;
    case WEBSITE_CERT:
      if (cert->trust->sslFlags & CERTDB_VALID_PEER) 
	goto done;
      break;
    case CA_CERT:
      if ((cert->trust->sslFlags & CERTDB_VALID_CA) ||
	  (cert->trust->emailFlags & CERTDB_VALID_CA) ||
	  (cert->trust->objectSigningFlags & CERTDB_VALID_CA)) {
	/* it is a CA cert, make sure it's not invisible cert */
	if (!((cert->trust->sslFlags & CERTDB_INVISIBLE_CA) ||
	      (cert->trust->emailFlags & CERTDB_INVISIBLE_CA) || 
	      (cert->trust->objectSigningFlags & CERTDB_INVISIBLE_CA)))
	  goto done;
      }
      break;
    default:
      PR_ASSERT(0);
    }
    goto loser;
done:  
    return SSM_SUCCESS;
loser: 
    return SSM_FAILURE;
  }



SSMStatus
SSM_ViewCertInfoKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  char* pattern = NULL;
  char* key = NULL;
  char * style = NULL, *commentString = NULL;
  const PRIntn CERT_WRAPPER = (PRIntn)1;
  const PRIntn CERT_WRAPPER_NO_COMMENT = (PRIntn)2;
  const PRIntn STYLE_PARAM = (PRIntn)0;
  PRIntn wrapper;
  CERTCertificate * cert = NULL;
	
  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  PR_ASSERT(SSM_IsAKindOf(cx->m_request->target, SSM_RESTYPE_CERTIFICATE));
 
  if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
      cx->m_result == NULL) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    goto loser;
  }
  /* get cert */
  cert = ((SSMResourceCert *)cx->m_request->target)->cert;
  if (!cert)
    goto loser;

  /* get the correct wrapper */
  commentString = CERT_GetCertCommentString(cert);
  if (commentString)
    wrapper = CERT_WRAPPER;
  else wrapper = CERT_WRAPPER_NO_COMMENT;
  key = (char *) SSM_At(cx->m_params, wrapper);
  PR_FREEIF(commentString);
  
  /* second, grab and expand the keyword objects */
  rv = SSM_GetAndExpandTextKeyedByString(cx, key, &pattern);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  SSM_DebugUTF8String("ca cert info pattern <%s>", pattern);
  
  style = (char *) SSM_At(cx->m_params, STYLE_PARAM);
  PR_FREEIF(cx->m_result);
  if (!strcmp(style, "pretty")) 
    rv = SSM_PrettyFormatCert(cert, pattern, &cx->m_result, PR_TRUE);
  else
    rv = SSM_FormatCert(cert, pattern, &cx->m_result);
  goto done;

 loser:
  if (rv == SSM_SUCCESS)
    rv = SSM_FAILURE;
 done:
  PR_FREEIF(pattern);

  return rv;
}



int
SSM_CertListCount(CERTCertList *certList)
{
  int numCerts = 0;
  CERTCertListNode *node;
  
  node = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(node, certList)) {
    numCerts++;
    node = CERT_LIST_NEXT(node);
  }
  return numCerts;
}

/* ### mwelch - PKCS11 private function? Need prototype for Mac */
#ifdef XP_MAC
CERTCertList *
PK11_FindCertsFromNickname(char *nickname, void *wincx);
#endif

CERTCertList * 
SSMControlConnection_CreateCertListByNickname(SSMControlConnection * ctrl, 
					      char * nick, PRBool email)
{
  CERTCertList * certListDB = NULL, * certListExternal = NULL;

  certListDB = CERT_NewCertList();
  if (email) 
    certListDB = CERT_CreateEmailAddrCertList(certListDB,ctrl->m_certdb,
					    nick, PR_Now(), PR_FALSE);
  else 
    certListDB = CERT_CreateNicknameCertList(certListDB, ctrl->m_certdb,
					     nick, PR_Now(), PR_FALSE);
  if (!certListDB && !email) 
    certListExternal = PK11_FindCertsFromNickname(nick, ctrl);

  if (certListExternal) 
    return certListExternal;
  else 
    return certListDB;
}
      
CERTCertificate * 
SSMControlConnection_FindCertByNickname(SSMControlConnection * ctrl, 
					char * nick, PRBool email)
{
  CERTCertificate * cert = NULL;
  
  if (email) 
    cert = CERT_FindCertByEmailAddr(ctrl->m_certdb, nick);
  else 
    cert = CERT_FindCertByNickname(ctrl->m_certdb, nick);

  if (!cert && !email) 
    cert = PK11_FindCertFromNickname(nick, ctrl);

  return cert;
}

SSMStatus
SSM_OCSPOptionsKeywordHandler(SSMTextGenContext *cx)
{
  SSMStatus rv;
  PRBool prefBool;
  char *fmt = NULL, *ocspURL;
  char *noOCSP = NULL, *noDefaultResponder = NULL, *useDefaultResponder = NULL;
  PrefSet *prefs;

  rv =  SSM_GetAndExpandTextKeyedByString(cx, "ocsp_options_template",
					  &fmt);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  prefs = cx->m_request->ctrlconn->m_prefs;
  rv = PREF_GetStringPref(prefs, "security.OCSP.URL", &ocspURL);
  if (rv != SSM_SUCCESS) {
    ocspURL = "";
  }
  rv = PREF_GetBoolPref(prefs, "security.OCSP.enabled", &prefBool);
  /*
   * Since the CHECKED is part of the HTML parsed by the broswer,
   * We don't have to localize it.  If the user were going to see
   * it, then we would have to localize it.
   */
  if (rv != SSM_SUCCESS || !prefBool) {
    noOCSP              = "CHECKED";
    noDefaultResponder  = "";
    useDefaultResponder = "";
  } else {
    rv = PREF_GetBoolPref(prefs, "security.OCSP.useDefaultResponder", 
			  &prefBool);
    if (rv != SSM_SUCCESS) {
      noOCSP              = "CHECKED";
      noDefaultResponder  = "";
      useDefaultResponder = "";
    } else if (prefBool) {
      noOCSP              = "";
      noDefaultResponder  = "";
      useDefaultResponder = "CHECKED";
    } else {
      noOCSP              = "";
      noDefaultResponder  = "CHECKED";
      useDefaultResponder = "";
    }
  }
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf(fmt, noOCSP,noDefaultResponder,
			     useDefaultResponder, ocspURL);
  PR_Free(fmt);
  return SSM_SUCCESS;
 loser:
  PR_FREEIF(fmt);
  return SSM_FAILURE;
}

SSMStatus
SSM_OCSPDefaultResponderKeywordHandler(SSMTextGenContext *cx)
{
  SSMStatus rv;
  char *defaultResponder = NULL, *fmt = NULL;
  
  rv = PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs, 
			  "security.OCSP.signingCA",
			  &defaultResponder);
  if (rv != SSM_SUCCESS) {
    goto loser;
  } 
  rv = SSM_GetAndExpandTextKeyedByString(cx, "default_responder_template", 
					 &fmt);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf(fmt, defaultResponder);
  if (cx->m_result == NULL) {
    goto loser;
  }
  PR_Free(fmt);
  return SSM_SUCCESS;
  
 loser:
  cx->m_result = PL_strdup("");
  PR_FREEIF(fmt);
  return SSM_SUCCESS;
}

SSMStatus 
SSM_ObtainNewCertSite(SSMTextGenContext * cx)
{
  char * newCertURL = NULL;
  SSMStatus rv = SSM_FAILURE;

  rv = PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs, "obtainCertURL",
			  &newCertURL);
  if (rv == SSM_SUCCESS) 
    goto done;

  rv = SSM_GetAndExpandText(cx, "new_cert_URL", &newCertURL);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("NewCertSite: can't find URL for obtaining new certs!\n");
    goto loser;
  }
  SSM_DEBUG("NewCertSite: no customized URL provided using default:%s", 
	    newCertURL);
  
 done:
  PR_FREEIF(cx->m_result);
  cx->m_result = newCertURL;
  newCertURL = NULL;
 loser:
  return rv;
}

SSMStatus SSM_ProcessLDAPWindow(HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  SSMResource * target = NULL;
  char **ldap_servers, **ptr, *params = NULL;
  SSMHTTPParamMultValues emailAddresses={NULL, NULL, 0};
  int i;

  if (!req || !req->ctrlconn) 
    goto loser;

  rv = PREF_CreateChildList(req->ctrlconn->m_prefs, 
			    "ldap_2.servers", &ldap_servers);
  if (rv != SSM_SUCCESS || !ldap_servers) {
    SSMControlConnection_SendUIEvent(req->ctrlconn, "get", 
                                     "show_followup", NULL, 
				     "result=no_ldap_setup", 
                                     &((SSMResource *)req->ctrlconn)->m_clientContext,
				     PR_TRUE);
    goto loser;
  }
    
  target = (req->target ? req->target : (SSMResource *) req->ctrlconn);
  /*
   * Let's see if there were some email addresses passed along to this
   * dialog that we should pre-fill the window with.
   */
  rv  = SSM_HTTPParamValueMultiple(req, "emailAddresses", &emailAddresses);
  if (rv == SSM_SUCCESS && emailAddresses.numValues > 0) {
    char *tmp;

    /* There are some email addresses that need to be passed on. */
    for (i=0; i<emailAddresses.numValues; i++) {
      tmp = PR_smprintf("%s%s%s", (params)?params:"", (params)?",":"",
			           emailAddresses.values[i]);
      PR_FREEIF(params);
      params = tmp;
    }
    tmp = SSM_ConvertStringToHTMLString(params);
    PR_Free(params);
    params = PR_smprintf("emailAddresses=%s", tmp);
    PR_Free(tmp);
  }
  /* send UI event to bring up the dialog */
  SSM_LockUIEvent(target);
  rv = SSMControlConnection_SendUIEvent(req->ctrlconn, "get", 
					"ldap_request", target, 
					params, &target->m_clientContext, 
					PR_TRUE);
  if (rv != SSM_SUCCESS) { 
    SSM_UnlockUIEvent(target);
    goto loser;
  }
  SSM_WaitUIEvent(target, PR_INTERVAL_NO_TIMEOUT);
  /*  if (req->ctrlconn->super.super.m_buttonType == SSM_BUTTON_CANCEL) {
      SSM_HTTPReportError(req, HTTP_NO_CONTENT);
      goto loser;
      }
  */
  /* free memory from ChildList */
  ptr = ldap_servers;
  while (*ptr) { 
    PR_Free(*ptr);
    ptr++;
  }
  PR_Free(ldap_servers);
  
 loser:
  if (req) {
    if (req->target->m_buttonType == SSM_BUTTON_CANCEL) {
      SSM_HTTPReportError(req, HTTP_NO_CONTENT);
    } else {
      SSM_RefreshRefererPage(req);
    }
  }
  /* Don't free the values stored in the individual array because
   * those are owned by the request structure.
   */
  PR_FREEIF(emailAddresses.values);
  return rv;
}

SSMStatus SSM_ProcessLDAPRequestHandler(HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  char * tmpStr = NULL, *emailaddr, *ldapserver;
  char* key = NULL, *currEmail, *cursor;
  PRBool updateList = PR_FALSE;

  /* make sure you got the right baseRef */
  rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
  if (rv != SSM_SUCCESS ||
      PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
    goto loser;
  }
  /* Close the window */
  rv = SSM_HTTPDefaultCommandHandler(req);
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("UI_ProcessLDAPRequest: can't close the window !\n");


  rv = SSM_HTTPParamValue(req, "do_cancel", &tmpStr);
  if (rv == SSM_SUCCESS && tmpStr) {
    req->target->m_buttonType = SSM_BUTTON_CANCEL;
    goto loser;
  }
  req->target->m_buttonType = SSM_BUTTON_OK;

  rv = SSM_HTTPParamValue(req, "emailaddress", &emailaddr);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("UI_ProcessLDAPRequest: no email address supplied!\n");
    goto loser;
  }

  rv = SSM_HTTPParamValue(req, "ldapServer", &ldapserver);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("UI_ProcessLDAPRequest: can't find ldap server parameter!\n");
    goto loser;
  }
  
  /* create a complete key part */
  key = PR_smprintf("ldap_2.servers.%s", ldapserver);
  if (key == NULL) {
      goto loser;
  }
  /*
   * Now let's loop over the each entry looking for the certs.
   */
  cursor = currEmail = emailaddr;
  while (cursor != NULL && *cursor != '\0') {
    char oldCursorVal;

    cursor = PL_strpbrk(currEmail, " ,");
    if (cursor != NULL) {
      oldCursorVal = *cursor;
      *cursor = '\0';
    }
    rv = SSM_CompleteLDAPLookup(req->ctrlconn, key, currEmail);
    if (rv == SSM_SUCCESS) {
      updateList = PR_TRUE;
    }
    if (cursor != NULL) {
      *cursor = oldCursorVal;
      cursor++;
    }
    /*
     * Jump over all of the white space and commas.
     */
    while (cursor && *cursor && isspace(*cursor))
      cursor++;
    while (cursor && *cursor == ',') {
      cursor++;
      while (cursor && *cursor && isspace(*cursor))
	cursor++;
    }
    currEmail = cursor;
  }
  if (updateList) 
    SSM_ChangeCertSecAdvisorList(req, emailaddr, certHashAdd);
  else 
    SSM_DEBUG("UI_ProcessLDAPRequest: can't import new cert into the db!\n");
  
 loser:
  SSM_NotifyUIEvent(req->target);
  PR_FREEIF(key);
  return rv;
}


SSMStatus 
SSM_CompleteLDAPLookup(SSMControlConnection *ctrl, char * ldapserver, 
		       char *emailaddr)
{
  SSMStatus rv = SSM_FAILURE;
  char * tmpStr, * servername, *baseDN, *mailAttribs;
  SECItem newCert = { siBuffer, NULL, 0};
  CERTCertificate * cert = NULL;
  SECStatus secrv; 
  int port = 0;
  char cert_attribs[] = "userSMIMECertificate,usercertificate;binary";
  cert_struct * certs[2] = {NULL, NULL};	/* one for ea cert_attrib */
  
  if (!ctrl || !ldapserver) 
    goto loser;
  PR_ASSERT(SSM_IsA((SSMResource *)ctrl, SSM_RESTYPE_CONTROL_CONNECTION));
  
  tmpStr = PR_smprintf("%s.serverName", ldapserver);
  rv = PREF_GetStringPref(ctrl->m_prefs,tmpStr,
			  &servername);
  PR_FREEIF(tmpStr);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("CompleteLDAPLookup: can't find LDAP server %s!\n",ldapserver);
    goto loser;
  }
  
  /* DN, mail attribs and port are not supplied from UI, look up in prefs */
  tmpStr = PR_smprintf("%s.searchBase",ldapserver);
  rv = PREF_GetStringPref(ctrl->m_prefs, tmpStr, &baseDN);
  PR_FREEIF(tmpStr);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("CompleteLDAPLookup: can't find baseDN for %s!\n",ldapserver);
    goto loser;
  }

  tmpStr = PR_smprintf("%s.attributes.mail",ldapserver);
  rv = PREF_GetStringPref(ctrl->m_prefs, tmpStr, &mailAttribs);
  PR_FREEIF(tmpStr);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("CompleteLDAPLookup:can't find mail attributes for %s!\n",
	      ldapserver);
    goto loser;
  }

  tmpStr = PR_smprintf("%s.port",ldapserver);
  rv = PREF_GetIntPref(ctrl->m_prefs, tmpStr, &port);
  PR_FREEIF(tmpStr);
  if (rv != SSM_SUCCESS)
    port = 0;

#ifndef XP_MAC
  rv = LDAPCertSearch(emailaddr, servername, baseDN, port, 1, NULL, NULL, 
		      NULL, mailAttribs, cert_attribs, certs);
#else
  rv = SSM_FAILURE; /* don't yet support LDAP on the Mac */
#endif

  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("CompleteLDAPLookup: ldap search did not find anything!\n");
    goto loser;
  }

  /* Go thru the possible multiple Certs retrieved from LDAP */
  rv = SSM_FAILURE;	/* default case - no good Certs found */

  /* first check any userSMIMECerts found */
  if (certs[0]) {
	  PRBool ret;
	  SEC_PKCS7ContentInfo *ci;
	  SECItem digest;
	  unsigned char nullsha1[SHA1_LENGTH];
      struct cert_struct_def * cert_ptr;

      cert_ptr = certs[0];
      while (cert_ptr->cert_len) {
          newCert.len = cert_ptr->cert_len;
          newCert.data = (unsigned char *) cert_ptr->cert;
		  ci = SEC_PKCS7DecodeItem(&newCert, NULL, NULL,
								     NULL, NULL, NULL, NULL, NULL);
		  if ( ci != NULL ) {
			  if ( SEC_PKCS7ContentIsSigned(ci) ) {
				  rv = SHA1_HashBuf(nullsha1, nullsha1, 0);
				  if ( rv != SECSuccess ) {
					  break;
				  }
				  digest.len = SHA1_LENGTH;
				  digest.data = nullsha1;

				  ret = SEC_PKCS7VerifyDetachedSignature(ci,
									certUsageEmailRecipient,
									&digest,
									HASH_AlgSHA1,
									PR_TRUE);
				  if (ret == PR_TRUE) {
					  rv = SSM_SUCCESS;
				  }
			  }
		  }

          PR_Free(cert_ptr->cert);
          *cert_ptr++;
      }
      PR_Free(certs[0]);
  }

  /* If no valid Certs found yet, try userCertificate;binary */
  if (rv == SSM_FAILURE && certs[1]) {
      struct cert_struct_def * cert_ptr;

      cert_ptr = certs[1];
      while (cert_ptr->cert_len) {
          newCert.len = cert_ptr->cert_len;
          newCert.data = (unsigned char *) cert_ptr->cert;
/*        memcpy(newCert.data, cert_ptr->cert, newCert.len); */

          /* Okay, got a Cert - so try to store in CertDB */
          cert = CERT_NewTempCertificate(ctrl->m_certdb, &newCert, NULL,
                                         PR_FALSE, PR_TRUE);
          if (cert != NULL) {
              secrv = CERT_SaveImportedCert(cert, certUsageEmailRecipient,
                                            PR_FALSE, NULL);
              if (secrv == SECSuccess) {
                  secrv = CERT_SaveSMimeProfile(cert, NULL, NULL);
                  rv = SSM_SUCCESS;
              }
          }
          /* and free LDAP cert memory for this cert */
          PR_Free(cert_ptr->cert);
          *cert_ptr++;
      }
      PR_Free(certs[1]);
  }

#if 0 
  /* Store the cert in the certdb */
  cert = CERT_NewTempCertificate(ctrl->m_certdb, &newCert,
				 NULL, PR_FALSE, PR_TRUE);
  if ( cert != NULL ) {
    secrv = CERT_SaveImportedCert(cert, certUsageEmailRecipient,
				  PR_FALSE, NULL);
    if ( secrv == SECSuccess ) 
      secrv = CERT_SaveSMimeProfile(cert, NULL, NULL);
  } else {
    SSM_DEBUG("CompleteLDAPLookup: couldn't import cert into DB!\n");
    rv = SSM_FAILURE;
  }
#endif

 loser:
    return rv;
}


SSMStatus SSM_LDAPServerListKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  char * tmpStr = NULL, * desc_key = NULL;
  char ** ldap_servers = NULL, ** ptr, * server = NULL;
  char * wrapper = NULL;

  cx->m_result = NULL;
  /* get all the ldap servers available and present them in a list */
  rv = PREF_CreateChildList(cx->m_request->ctrlconn->m_prefs, 
			    "ldap_2.servers", &ldap_servers);
  if (rv != SSM_SUCCESS || !ldap_servers)
    goto done;
  
  rv = SSM_GetAndExpandTextKeyedByString(cx, "ldap_server_list_prefix", 
					 &cx->m_result);
  if (rv != SSM_SUCCESS)
    goto done;
  rv = SSM_GetAndExpandTextKeyedByString(cx, "ldap_server_list_item", 
					 &wrapper);
  if (rv != SSM_SUCCESS) 
    goto done;
  ptr = ldap_servers;
  while (*ptr) {
    desc_key = PR_smprintf("ldap_2.servers.%s.description", *ptr);
    rv = PREF_GetStringPref(cx->m_request->ctrlconn->m_prefs,
			    desc_key, &server);
    PR_FREEIF(desc_key); desc_key = NULL;
    if (rv != SSM_SUCCESS)
      goto done;
    tmpStr = PR_smprintf(wrapper, *ptr, server);
    PR_Free(*ptr); 
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    PR_FREEIF(tmpStr); tmpStr = NULL;
    if (rv != SSM_SUCCESS) 
      goto done;
    ptr++;
  }
  PR_Free(ldap_servers); ldap_servers = NULL;
  SSM_GetAndExpandTextKeyedByString(cx, "ldap_server_list_suffix",
				    &tmpStr);
  rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
 done:
  PR_FREEIF(wrapper);
  PR_FREEIF(tmpStr);
  if (rv != SSM_SUCCESS)
    PR_FREEIF(cx->m_result);
  return rv;
}

char * SSM_GetCAPolicyString(char * org, unsigned long noticeNum, void * arg)
{
  SSMControlConnection * ctrl = (SSMControlConnection *)arg;
  char * alphaorg = NULL, * ptr, *aptr, * key = NULL, * value = NULL;
  int c;

  if (!ctrl || !org) 
    goto loser;
  PR_ASSERT(ctrl && SSM_IsAKindOf((SSMResource *)ctrl,SSM_RESTYPE_CONTROL_CONNECTION));

  alphaorg = (char *) PORT_ZAlloc(sizeof(org) + 1);
  if (!alphaorg) 
    goto loser;
  
  ptr = org; aptr = alphaorg;
  while ( (c=*ptr) != '\0') {
    if ( isalpha(c) ) {
      * aptr = c;
      aptr++;
    }
    ptr++;
  }
  *aptr = '\0';
  
  key = PR_smprintf("security.canotices.%s.notice%ld", alphaorg, noticeNum);
  if (!key)
    goto loser;
  PREF_GetStringPref(ctrl->m_prefs, key, &value);
  
 loser:
  PR_FREEIF(alphaorg);
  PR_FREEIF(key);
  return value?PL_strdup(value):NULL;
}

SSMStatus
SSM_FillTextWithEmails(SSMTextGenContext *cx)
{
  SSMStatus rv;
  char *emails;

  rv = SSM_HTTPParamValue(cx->m_request, "emailAddresses", &emails);
  PR_FREEIF(cx->m_result);
  if (rv == SSM_SUCCESS) {
    cx->m_result = PL_strdup(emails);
  } else {
    cx->m_result = PL_strdup("");
  }
  return SSM_SUCCESS;
}

SSMStatus
SSM_GetWindowOffset(SSMTextGenContext *cx)
{
  char *offset=NULL;
  SSMStatus rv;
  int offsetVal;
  
  rv = SSM_HTTPParamValue(cx->m_request, "windowOffset", &offset);
  if (rv != SSM_SUCCESS) {
    offsetVal = 0;
  } else {
    offsetVal = atoi (offset);
  }
  offsetVal += 20;
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf("%d", offsetVal);
  return SSM_SUCCESS;
}

SSMStatus
SSM_MakeUniqueNameForIssuerWindow(SSMTextGenContext *cx)
{
  SSMResourceCert *certres;
  CERTCertificate *issuer, *serverCert;
  char *certHex=NULL, *issuerHex=NULL;
  SSMSSLDataConnection *sslConn;

  if (SSM_IsAKindOf(cx->m_request->target, SSM_RESTYPE_CERTIFICATE)) {
      certres = (SSMResourceCert*)cx->m_request->target;
      serverCert = certres->cert;
  } else if (SSM_IsAKindOf(cx->m_request->target, 
			   SSM_RESTYPE_SSL_DATA_CONNECTION)) {
      sslConn = (SSMSSLDataConnection*)cx->m_request->target;
      serverCert = SSL_PeerCertificate(sslConn->socketSSL);
      if (serverCert == NULL) {
	goto loser;
      }
  } else {
      goto loser;
  }
  issuer = CERT_FindCertIssuer(serverCert,PR_Now(),certUsageAnyCA);
  certHex = CERT_Hexify(&serverCert->serialNumber,0);
  if (issuer != NULL) {
    issuerHex = CERT_Hexify(&issuer->serialNumber,0);
    CERT_DestroyCertificate(issuer);
  }
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf("%s%s", certHex, (issuerHex) ? issuerHex : "");
  PR_FREEIF(issuerHex);
  PR_Free(certHex);
  return SSM_SUCCESS;
 loser:
  return SSM_FAILURE;
}

SSMStatus 
SSM_ShowCertIssuer(HTTPRequest *req)
{
  SSMResourceCert *certres;
  SSMResource *issuerRes=NULL;
  CERTCertificate *issuer=NULL;
  char *tmpl = NULL, *finalText=NULL;
  SSMTextGenContext *cx=NULL;
  SSMStatus rv;
  SSMResourceID issuerResID;
  char *htmlString=NULL, *params = NULL, *offsetString;
  int offset;

  if (!SSM_IsAKindOf(req->target, SSM_RESTYPE_CERTIFICATE)) {
    goto loser;
  }
  certres = (SSMResourceCert*)req->target;
  issuer = CERT_FindCertIssuer(certres->cert, PR_Now(), certUsageAnyCA);
  if (issuer == NULL) {
    /*
     * Instead of bailing out entirely, pop up a dialog that let's the
     * user know the issuer was not found.
     */
    
    params = PR_smprintf("get?baseRef=issuer_not_found&target=%d",
			 req->target->m_id);
  } else {
    
    rv = (SSMStatus) SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, (void*)issuer,
					req->ctrlconn, &issuerResID, &issuerRes);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }

    rv = SSM_HTTPParamValue(req, "windowOffset", &offsetString);
    if (rv != SSM_SUCCESS) {
      offset = 0;
    } else {
      offset = atoi (offsetString);
    }
    
    /*  Now let's build up the URL*/
    htmlString = SSM_ConvertStringToHTMLString(issuer->nickname);
    params = PR_smprintf("get?baseRef=cert_view&target=%1$d&windowOffset=%2$d", 
			 issuerResID, offset);
    PR_Free(htmlString);
    htmlString = NULL;
  } 
  rv =SSMTextGen_NewTopLevelContext(req, &cx);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  rv = SSM_GetAndExpandText(cx, "refresh_window_content", &tmpl);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  finalText = PR_smprintf(tmpl, params);
  SSM_HTTPSendOKHeader(req, NULL, "text/html");
  SSM_HTTPSendUTF8String(req, finalText);
  PR_FREEIF(tmpl);
  PR_Free(finalText);
  SSMTextGen_DestroyContext(cx);
  req->sentResponse = PR_TRUE;
  return SSM_SUCCESS;
 loser:
  PR_FREEIF(tmpl);
  PR_FREEIF(finalText);
  if (cx != NULL) {
    SSMTextGen_DestroyContext(cx);
  }
  return SSM_FAILURE;
}

SSMStatus SSM_PrettyPrintCert(HTTPRequest *req)
{
    SSMResource *res;
    SSMResourceCert *certres;
    SSMStatus rv;
    char *prefix=NULL, *suffix=NULL, *certName;
    SSMTextGenContext *cx=NULL;

    res = req->target;
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_CERTIFICATE)) {
        return SSM_FAILURE;
    }
    certres = (SSMResourceCert*)res;
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_GetAndExpandText(cx, "certPrettyPrefix", &prefix);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    rv = SSM_GetAndExpandText(cx, "certPrettySuffix", &suffix);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    rv = SSM_HTTPSendUTF8String(req, prefix);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    certName = (certres->cert->nickname) ? certres->cert->nickname :
                                           certres->cert->emailAddr;
    SSM_HTTPSendUTF8String(req, certName);
    SSM_HTTPSendUTF8String(req, "\n\n");
    SSM_PrettyPrintDER(req->sock, &certres->cert->derCert, 0);
    SSM_HTTPSendUTF8String(req, suffix);
    req->sentResponse = PR_TRUE;
    return SSM_SUCCESS;
 loser:
    if (cx != NULL) {
      SSMTextGen_DestroyContext(cx);
    }
    PR_FREEIF(prefix);
    PR_FREEIF(suffix);
    return SSM_FAILURE; 
}

