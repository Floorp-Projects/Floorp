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
#include "ctrlconn.h"
#include "serv.h"
#include "servimpl.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(ss) (&(ss)->super)

/* implemented in resource.c: this should belong to resource.h! */
SSMStatus SSM_UnpickleResource(SSMResource** res, SSMResourceType type, 
                              SSMControlConnection* connection,
                              PRIntn len, void* value);

SSMStatus
SSMSSLSocketStatus_Create(void *arg, SSMControlConnection * connection, 
                          SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSSLSocketStatus *ss;
    *res = NULL; /* in case we fail */
    
    ss = (SSMSSLSocketStatus *) PR_CALLOC(sizeof(SSMSSLSocketStatus));
    if (!ss) goto loser;

    rv = SSMSSLSocketStatus_Init(ss, connection, 
				 (PRFileDesc *) arg, 
                                 SSM_RESTYPE_SSL_SOCKET_STATUS);
    if (rv != PR_SUCCESS) goto loser;

    SSMSSLSocketStatus_Invariant(ss);

    *res = &ss->super;
    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (ss) 
    {
        SSMRESOURCE(ss)->m_refCount = 1; /* force destroy */
        SSM_FreeResource(SSMRESOURCE(ss));
    }
        
    return rv;
}

SSMStatus 
SSMSSLSocketStatus_Init(SSMSSLSocketStatus *ss, 
 			SSMControlConnection * connection, 
                        PRFileDesc *fd, SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    int keySize, secretKeySize, level;
    SSMResource *certObj;
    SSMResourceID certRID;
    CERTCertificate *cert;

    /* Initialize superclass */
    SSMResource_Init(connection, SSMRESOURCE(ss), type);

    /* Get the peer cert and import it into an NSM object */
    cert = SSL_PeerCertificate(fd);
    if (!cert) goto loser;
    rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, 
                            SSMRESOURCE(ss)->m_connection, &certRID, 
                            &certObj); 
    if (rv != PR_SUCCESS) goto loser;
    ss->m_cert = (SSMResourceCert*)certObj;

    /* Get the other socket status information */
    rv = SSL_SecurityStatus(fd, &level, &ss->m_cipherName,
                            &keySize,
                            &secretKeySize,
                            NULL, NULL);
    if (rv != PR_SUCCESS) goto loser;

    ss->m_keySize = keySize;
    ss->m_secretKeySize = secretKeySize;
    ss->m_level = level;

    /* Sanity check before returning */
    SSMSSLSocketStatus_Invariant(ss);
    
    goto done;
loser:
    /* member destruct, if any are allocated, will happen in the
       _Destroy method */
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
done:
    return rv;
}

SSMStatus 
SSMSSLSocketStatus_Destroy(SSMResource *res, PRBool doFree)
{
    SSMSSLSocketStatus *ss = (SSMSSLSocketStatus *) res;
    SSMSSLSocketStatus_Invariant(ss);
    /* Destroy our members. */
    if (ss->m_cert)
    {
        SSM_FreeResource(&ss->m_cert->super);
        ss->m_cert = NULL;
    }
    if (ss->m_cipherName)
    {
        PORT_Free(ss->m_cipherName);
        ss->m_cipherName = NULL;
    }

    /* Destroy superclass. */
    SSMResource_Destroy(res, PR_FALSE);
    
    /* Free if asked. */
    if (doFree)
        PR_Free(res);

    return PR_SUCCESS;
}

void 
SSMSSLSocketStatus_Invariant(SSMSSLSocketStatus *ss)
{
    /* Superclass invariant */
    SSMResource_Invariant(SSMRESOURCE(ss));
    
    /* Class check */
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(ss), SSM_RESTYPE_SSL_SOCKET_STATUS));

    /* Member check */
#if 0
    /* Make sure we used a cert to initialize ourselves */
    if (ss->m_cert)
        PR_ASSERT(ss->m_cert != NULL);
#endif
}

SSMStatus 
SSMSSLSocketStatus_GetAttrIDs(SSMResource *res,
                              SSMAttributeID **ids,
                              PRIntn *count)
{
    SSMStatus rv;

    rv = SSMResource_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 4) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_SSS_KEYSIZE;
    (*ids)[*count++] = SSM_FID_SSS_SECRET_KEYSIZE;
    (*ids)[*count++] = SSM_FID_SSS_CERT_ID;
    (*ids)[*count++] = SSM_FID_SSS_CIPHER_NAME;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMSSLSocketStatus_GetAttr(SSMResource *res,
                           SSMAttributeID attrID,
                           SSMResourceAttrType attrType,
                           SSMAttributeValue *value)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSSLSocketStatus *ss = (SSMSSLSocketStatus *) res;
    char *tmpstr = NULL;

    SSM_DEBUG("socket status get attr is called.\n");
    SSMSSLSocketStatus_Invariant(ss);

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_SSS_KEYSIZE: 
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = ss->m_keySize;
        break;
    case SSM_FID_SSS_SECRET_KEYSIZE:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = ss->m_secretKeySize;
        break;
    case SSM_FID_SSS_CERT_ID:
        value->type = SSM_RID_ATTRIBUTE;
        value->u.numeric = ss->m_cert->super.m_id;
        break;
    case SSM_FID_SSS_SECURITY_LEVEL:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = ss->m_level;
        break;
    case SSM_FID_SSS_CIPHER_NAME:
        value->type = SSM_STRING_ATTRIBUTE;
        value->u.string.len = PL_strlen(ss->m_cipherName);
        value->u.string.data = (unsigned char *) PL_strdup(ss->m_cipherName);
        break;
    case SSM_FID_SSS_HTML_STATUS:
        value->type = SSM_STRING_ATTRIBUTE;
        rv = (*res->m_html_func)(res, NULL, (void**)&tmpstr);

        value->u.string.len = PL_strlen(tmpstr);
        value->u.string.data = (unsigned char *) PL_strdup(tmpstr);
        break;
	case SSM_FID_SSS_CA_NAME:
		{
			char * caName = NULL;

		value->type = SSM_STRING_ATTRIBUTE;
		caName = CERT_GetOrgName(&ss->m_cert->cert->issuer);
		if (caName == NULL) {
			caName = PL_strdup("");
		}
		value->u.string.len = PL_strlen(caName);
		value->u.string.data = (unsigned char *)PL_strdup(caName);
		PR_FREEIF(caName);
		}
		break;
    default:
        rv = SSMResource_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS)
            goto loser;
    }

    goto done;
 loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    if (tmpstr != NULL)
        PR_Free(tmpstr);
    return rv;
}

SSMStatus SSMSSLSocketStatus_Pickle(SSMResource *res, PRIntn * len,
                                   void **value)
{ 
    int blobSize, certSize;
    SSMStatus rv;
    SSMSSLSocketStatus * resource = (SSMSSLSocketStatus *)res;
    void * certBlob, *curptr, *tmpStr = NULL;

    if (!res || !value) {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    /* in case we fail */
    *value = NULL;
    if (len) *len = 0;
#if 0    
    /* first, pickle cert */
    rv = SSMControlConnection_GetResource(res->m_connection, 
                                          resource->m_certID,
                                          (SSMResource **)&certObj);
    if (rv != PR_SUCCESS) goto loser;
#endif
    
    rv = SSM_PickleResource(&resource->m_cert->super, &certSize, &certBlob); 
    if (rv != PR_SUCCESS) 
        goto loser;
    
    /* allocate memory for the pickled blob */
    blobSize = certSize + sizeof(PRUint32)*5 + 
        SSMSTRING_PADDED_LENGTH(strlen(resource->m_cipherName));
    curptr = PORT_ZAlloc(blobSize);
    if (!curptr) {
        rv = PR_OUT_OF_MEMORY_ERROR;
        goto loser;
    }
    *value = curptr;
    *(PRUint32 *)curptr = resource->m_keySize;
    curptr = (PRUint32 *)curptr + 1;
    *(PRUint32 *)curptr = resource->m_secretKeySize;
    curptr = (PRUint32 *)curptr + 1;
	*(PRInt32*)curptr = resource->m_level;
	curptr = (PRInt32 *)curptr + 1;
	*(PRInt32*)curptr = resource->m_error;
	curptr = (PRInt32 *)curptr + 1;
	rv = SSM_StringToSSMString((SSMString **)&tmpStr, 0, resource->m_cipherName);
    if (rv != PR_SUCCESS) 
        goto loser;
    memcpy(curptr, tmpStr, SSM_SIZEOF_STRING(*(SSMString *)tmpStr));
    curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)tmpStr);
    PR_Free(tmpStr);
    tmpStr = NULL;
    /* copy cert into the blob */
    memcpy(curptr, certBlob, certSize);

    if (len) *len = blobSize;
        
    return rv;
    
loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
    if (value && *value)
        PR_Free(*value);
    if (len) 
        *len = 0;
    if (tmpStr) 
        PR_Free(tmpStr);

    return rv;
}

SSMStatus SSMSSLSocketStatus_Unpickle(SSMResource ** res, 
                                     SSMControlConnection * connection, 
                                     PRInt32 len, 
                                     void *value)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSSLSocketStatus *ss;
    void * curptr = NULL;
    PRUint32 strLength = 0, certLength = 0;
    SSMResource * certResource = NULL;

    if (!res || !value) {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    *res = NULL; /* in case we fail */
    
    ss = (SSMSSLSocketStatus *) PR_CALLOC(sizeof(SSMSSLSocketStatus));
    if (!ss) goto loser;
    ss->m_cipherName = NULL;
    ss->m_cert = NULL;

    /* Initialize superclass */
    SSMResource_Init(connection, SSMRESOURCE(ss), 
		     SSM_RESTYPE_SSL_SOCKET_STATUS);
    /* Unpickle socket status data */
    curptr = value;
    
    ss->m_keySize = *(PRUint32 *)curptr;
    curptr =(PRUint32 *)curptr + 1;  
    ss->m_secretKeySize = *(PRUint32 *)curptr;
    curptr =(PRUint32 *)curptr + 1; 
    ss->m_level = *(PRInt32 *)curptr;
	curptr = (PRInt32 *)curptr + 1;
	ss->m_error = *(PRInt32 *)curptr;
	curptr = (PRInt32 *)curptr + 1;

    /* Fix this */
    rv = SSM_SSMStringToString(&ss->m_cipherName, (int *) &strLength, 
                               (SSMString *)curptr);
    if (rv != PR_SUCCESS || ss->m_cipherName == NULL || strLength == 0) 
        goto loser;
    curptr = (char *)curptr + SSMSTRING_PADDED_LENGTH(strLength) + 
        sizeof(PRUint32);
    
    /* Unpickle cert */
    certLength = len - ((unsigned long)curptr - (unsigned long)value);
    rv = SSM_UnpickleResource(&certResource, SSM_RESTYPE_CERTIFICATE, 
                              connection, certLength, curptr);
    if (rv != PR_SUCCESS || !certResource)
        goto loser;
#if 0
    ss->m_certID = certResource->m_id;
#endif
    ss->m_cert = (SSMResourceCert*)certResource; 
    /* Sanity check before returning */
    SSMSSLSocketStatus_Invariant(ss);
    
    *res = SSMRESOURCE(ss);
    goto done;
    
loser:
    if (ss) {
        if (ss->m_cipherName)
            PR_Free(ss->m_cipherName);
        if (ss->m_cert) {
            SSM_FreeResource(&ss->m_cert->super);
#if 0
            (*(certResource->m_destroy_func))(certResource, PR_TRUE);
#endif
        }
        PR_Free(certResource);
    }
    if (rv == PR_SUCCESS) 
        rv = PR_FAILURE;
done: 
    return rv;
}

SSMStatus SSMSSLSocketStatus_HTML(SSMResource *res, PRIntn *len, void ** value)
{ 
    SSMStatus rv = PR_SUCCESS;
    SSMSSLSocketStatus * resource = (SSMSSLSocketStatus *)res;
    char * line = NULL;
 
    if (!res || !value) {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    if (len) *len = 0;
    *value = NULL;
    
    switch (resource->m_level) {
    case SSL_SECURITY_STATUS_ON_HIGH:
#ifdef FORTEZZA
    case SSL_SECURITY_STATUS_FORTEZZA:
#endif
        line = PR_smprintf("%s", SECURITY_HIGH_MESSAGE);
        break;  
    case SSL_SECURITY_STATUS_ON_LOW:
        line = PR_smprintf("%s", SECURITY_LOW_MESSAGE);
        break;
    default:
        *value = PR_smprintf("%s", SECURITY_NO_MESSAGE);
        goto done;
    }
 

    if (resource->m_keySize != resource->m_secretKeySize) {
        *value = PR_smprintf("%s (%s, %d bit with %d secret).", line,
                             resource->m_cipherName, 
                             resource->m_keySize, 
                             resource->m_secretKeySize);
    } else {
        *value = PR_smprintf("%s (%s, %d bit).", line, 
                             resource->m_cipherName,
                             resource->m_keySize);
    }
    
    goto done;
loser:
    if (value && *value)
        PR_Free(*value);
    *value = NULL;
    if (len && *len)
        *len = 0;
    if (rv == PR_SUCCESS) 
        rv = PR_FAILURE;
done:
    if (value && *value && len) 
        *len = strlen((char *)*value)+1;
    if (line) 
        PR_Free(line);
    return rv;
}


SSMStatus SSMSSLSocketStatus_UpdateSecurityStatus(SSMSSLSocketStatus* ss,
                                                 PRFileDesc* socket)
{
    SSMStatus rv = PR_SUCCESS;
    int keySize, secretKeySize, level;

    /* ss should not be NULL when this function is called */
    PR_ASSERT(ss != NULL);

    /* Get the security status information */
    if (ss->m_cipherName != NULL) {
        PR_Free(ss->m_cipherName);
        ss->m_cipherName = NULL;
    }
    rv = SSL_SecurityStatus(socket, &level, &ss->m_cipherName, &keySize, 
                            &secretKeySize, NULL, NULL);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    ss->m_keySize = keySize;
    ss->m_secretKeySize = secretKeySize;
    ss->m_level = level;

    /* Sanity check before returning */
    SSMSSLSocketStatus_Invariant(ss);
    
    goto done;
loser:
    if (rv == PR_SUCCESS) {
        rv = PR_FAILURE;
    }
    if (ss->m_cipherName != NULL) {
        PR_Free(ss->m_cipherName);
        ss->m_cipherName = NULL;
    }
done:
    return rv;
}
