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
#include "resource.h"
#include "hashtbl.h"
#include "collectn.h"
#include "connect.h"
#include "ctrlconn.h"
#include "dataconn.h"
#include "sslconn.h"
#include "sslskst.h"
#include "certres.h"
#include "keyres.h"
#include "crmfres.h"
#include "p7dconn.h"
#include "p7cinfo.h"
#include "p7econn.h"
#include "hashconn.h"
#include "kgenctxt.h"
#include "servimpl.h"
#include "ssmerrs.h"
#include "nlsutil.h"
#include "advisor.h"
#include "p12res.h"
#include "signtextres.h"
#include "sdrres.h"

#include <stdarg.h>

/* Type registration */
struct SSMResourceClass
{
    /* The type of resource. */
    SSMResourceType        m_classType;
    SSMResourceType        m_superclass;

    /* Name. */
    char *                 m_className;

    /* Data pertaining to each class. */
    SSMClientDestroyAction m_clientDest;
    
    /* Accessor functions. */
    SSMResourceCreateFunc  m_create_func;
    SSMResourceDestroyFunc m_destroy_func;
    SSMResourceShutdownFunc m_shutdown_func;
    SSMResourceGetAttrIDsFunc m_getids_func;
    SSMResourceGetAttrFunc m_get_func;
    SSMResourceSetAttrFunc m_set_func;
    SSMResourcePickleFunc m_pickle_func;
    SSMResourceUnpickleFunc m_unpickle_func;
    SSMResourceHTMLFunc m_html_func;
    SSMResourcePrintFunc m_print_func;
    SSMSubmitHandlerFunc m_submit_func;
};

SSMHashTable *ssm_classRegistry = NULL;


/* Declarations to keep the compiler happy */
SSMStatus SSM_FindResourceClass(SSMResourceType type, SSMResourceClass **cls);

SSMStatus
SSM_ResourceInit()
{
    SSMStatus rv = PR_SUCCESS;
    
    if (ssm_classRegistry == NULL)
        {
        rv = SSM_HashCreate(&ssm_classRegistry);
        PR_ASSERT(ssm_classRegistry != NULL);

        /* Register well-known types. 
           Connection classes fall into this category. */
        SSM_RegisterResourceType("Resource",
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_RESTYPE_NULL,
                                 SSM_CLIENTDEST_NOTHING,
                                 SSMResource_Create,
                                 SSMResource_Destroy,
                                 SSMResource_Shutdown,
                                 SSMResource_GetAttrIDs,
                                 SSMResource_GetAttr,
                                 SSMResource_SetAttr,
                                 SSMResource_Pickle,
                                 SSMResource_Unpickle,
                                 SSMResource_HTML,
                                 SSMResource_Print,
                                 SSMResource_FormSubmitHandler);
        SSM_RegisterResourceType("Connection",
                                 SSM_RESTYPE_CONNECTION,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMConnection_Create,
                                 SSMConnection_Destroy,
                                 SSMConnection_Shutdown,
                                 SSMConnection_GetAttrIDs,
                                 SSMConnection_GetAttr,
                                 SSMConnection_SetAttr, 
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("Control connection",
                                 SSM_RESTYPE_CONTROL_CONNECTION,
                                 SSM_RESTYPE_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMControlConnection_Create,
                                 SSMControlConnection_Destroy,
                                 SSMControlConnection_Shutdown,
                                 SSMControlConnection_GetAttrIDs,
                                 SSMControlConnection_GetAttr,
                                 NULL, 
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMControlConnection_FormSubmitHandler);
        SSM_RegisterResourceType("Data connection",
                                 SSM_RESTYPE_DATA_CONNECTION,
                                 SSM_RESTYPE_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMDataConnection_Create,
                                 SSMDataConnection_Destroy,
                                 SSMDataConnection_Shutdown,
                                 SSMDataConnection_GetAttrIDs,
                                 SSMDataConnection_GetAttr,
                                 NULL, 
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("SSL connection",
                                 SSM_RESTYPE_SSL_DATA_CONNECTION,
                                 SSM_RESTYPE_DATA_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMSSLDataConnection_Create,
                                 SSMSSLDataConnection_Destroy,
                                 SSMSSLDataConnection_Shutdown,
                                 SSMSSLDataConnection_GetAttrIDs,
                                 SSMSSLDataConnection_GetAttr,
                                 SSMSSLDataConnection_SetAttr,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMSSLDataConnection_FormSubmitHandler);
        SSM_RegisterResourceType("Hash connection",
                                 SSM_RESTYPE_HASH_CONNECTION,
                                 SSM_RESTYPE_DATA_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMHashConnection_Create,
                                 SSMHashConnection_Destroy,
                                 SSMHashConnection_Shutdown,
                                 SSMHashConnection_GetAttrIDs,
                                 SSMHashConnection_GetAttr,
                                 NULL,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("PKCS7 decode connection",
                                 SSM_RESTYPE_PKCS7_DECODE_CONNECTION,
                                 SSM_RESTYPE_DATA_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMP7DecodeConnection_Create,
                                 SSMP7DecodeConnection_Destroy,
                                 SSMP7DecodeConnection_Shutdown,
                                 SSMP7DecodeConnection_GetAttrIDs,
                                 SSMP7DecodeConnection_GetAttr,
                                 NULL,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("PKCS7 encode connection",
                                 SSM_RESTYPE_PKCS7_ENCODE_CONNECTION,
                                 SSM_RESTYPE_DATA_CONNECTION,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMP7EncodeConnection_Create,
                                 SSMP7EncodeConnection_Destroy,
                                 SSMP7EncodeConnection_Shutdown,
                                 NULL,
                                 SSMP7EncodeConnection_GetAttr,
                                 SSMP7EncodeConnection_SetAttr,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("PKCS7 content info",
                                 SSM_RESTYPE_PKCS7_CONTENT_INFO,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE, /* client destroy */
                                 SSMP7ContentInfo_Create,
                                 SSMP7ContentInfo_Destroy,
                                 NULL,
                                 SSMP7ContentInfo_GetAttrIDs,
                                 SSMP7ContentInfo_GetAttr,
                                 NULL,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("SSL socket status object",
                                 SSM_RESTYPE_SSL_SOCKET_STATUS,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE, /* client destroy */
                                 SSMSSLSocketStatus_Create,
                                 SSMSSLSocketStatus_Destroy,
                                 NULL,
                                 SSMSSLSocketStatus_GetAttrIDs,
                                 SSMSSLSocketStatus_GetAttr,
                                 NULL,
                                 SSMSSLSocketStatus_Pickle, 
                                 SSMSSLSocketStatus_Unpickle,
                                 SSMSSLSocketStatus_HTML,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("Certificate",
                                 SSM_RESTYPE_CERTIFICATE,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE, /* client destroy */
                                 SSMResourceCert_Create,
                                 SSMResourceCert_Destroy,
                                 NULL,
                                 SSMResourceCert_GetAttrIDs,
                                 SSMResourceCert_GetAttr,
                                 NULL,
                                 SSMResourceCert_Pickle, 
                                 SSMResourceCert_Unpickle,
                                 SSMResourceCert_HTML,
                                 NULL,
                                 SSMResourceCert_FormSubmitHandler);
        SSM_RegisterResourceType("Key pair",
                                 SSM_RESTYPE_KEY_PAIR,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE, /* client destroy */
                                 SSMKeyPair_Create,
                                 SSMKeyPair_Destroy,
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMKeyPair_SetAttr,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("CRMF request",
                                 SSM_RESTYPE_CRMF_REQUEST,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE, /* client destroy */
                                 SSMCRMFRequest_Create,
                                 SSMCRMFRequest_Destroy,
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMCRMFRequest_SetAttr,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
        SSM_RegisterResourceType("Key gen context",
                                 SSM_RESTYPE_KEYGEN_CONTEXT, /* type */
                                 SSM_RESTYPE_RESOURCE, /* superclass */
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 SSMKeyGenContext_Create, /* create */
                                 SSMKeyGenContext_Destroy, /* destroy */
                                 SSMKeyGenContext_Shutdown, /* shutdown */
                                 NULL, /* get attr IDs */
                                 SSMKeyGenContext_GetAttr, /* get attr */
                                 SSMKeyGenContext_SetAttr,
                                 NULL, /* pickle */
                                 NULL, /* unpickle */
                                 NULL, /* HTML */
                                 SSMKeyGenContext_Print, /* Print */
                                 SSMKeyGenContext_FormSubmitHandler);
        SSM_RegisterResourceType("Security advisor context",
                                 SSM_RESTYPE_SECADVISOR_CONTEXT,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_SHUTDOWN, /* client destroy */
                                 NULL,
                                 SSMSecurityAdvisorContext_Destroy,
                                 NULL, 
                                 SSMSecurityAdvisorContext_GetAttrIDs,
                                 SSMSecurityAdvisorContext_GetAttr,
                                 SSMSecurityAdvisorContext_SetAttr,
                                 NULL, 
                                 NULL,
                                 NULL,
                                 SSMSecurityAdvisorContext_Print,
                                 SSMSecurityAdvisorContext_FormSubmitHandler);
        SSM_RegisterResourceType("PKCS12 Context",
                                 SSM_RESTYPE_PKCS12_CONTEXT,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE,
                                 SSMPKCS12Context_Create,
                                 SSMPKCS12Context_Destroy,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMPKCS12Context_Print,
                                 SSMPKCS12Context_FormSubmitHandler);
        SSM_RegisterResourceType("SignText Context",
                                 SSM_RESTYPE_SIGNTEXT,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE,
                                 SSMSignTextResource_Create,
                                 SSMSignTextResource_Destroy,
                                 SSMSignTextResource_Shutdown,
                                 NULL,
                                 SSMSignTextResource_GetAttr,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMSignTextResource_Print,
                                 SSMSignTextResource_FormSubmitHandler);
        SSM_RegisterResourceType("SDR Context",
                                 SSM_RESTYPE_SDR_CONTEXT,
                                 SSM_RESTYPE_RESOURCE,
                                 SSM_CLIENTDEST_FREE,
                                 SSMSDRContext_Create,
                                 SSMSDRContext_Destroy,
                                 NULL,  /* Shutdown */
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 SSMSDRContext_FormSubmitHandler);
    }
    
    return rv;
}

SSMStatus
SSMResource_Destroy(SSMResource *res, PRBool doFree)
{
    SSMStatus rv;
    SSMResourceID tmpID;

    /* We should have been called from a subclass destructor. */
    PR_ASSERT(doFree == PR_FALSE);

    SSM_DEBUG("Destroying %s (rid %ld).\n", res->m_class->m_className, 
              (long) res->m_id);

    rv = SSM_HashRemove(res->m_connection->m_resourceDB, res->m_id,
                        (void **) &res);
    if (rv != PR_SUCCESS)
        return PR_FAILURE;

    rv = SSM_HashRemove(res->m_connection->m_resourceIdDB, (SSMHashKey)res,
                        (void **) &tmpID);
    if (rv != PR_SUCCESS)
        return PR_FAILURE;

    if (res->m_lock)
    {
        PR_DestroyMonitor(res->m_lock);
        res->m_lock = NULL;
    }
    
    if (res->m_UILock)
    {
        PR_DestroyMonitor(res->m_UILock);
        res->m_UILock = NULL;
    }
    if (res->m_formName) {
        PR_Free(res->m_formName);
        res->m_formName = NULL;
    }
    
    return PR_SUCCESS;
}

SSMStatus
SSMResource_Init(SSMControlConnection * conn, SSMResource *res, 
                 SSMResourceType type)
{
    SSMResourceClass *cls;
    SSMStatus rv;

    rv = SSM_FindResourceClass(type, &cls);
    if (rv != PR_SUCCESS)
        goto loser;

    res->m_class = cls;
    res->m_classType = type;
    res->m_buttonType = SSM_BUTTON_NONE;
    res->m_clientDest = cls->m_clientDest;
    res->m_destroy_func = cls->m_destroy_func;
    res->m_shutdown_func = cls->m_shutdown_func;
    res->m_getids_func = cls->m_getids_func;
    res->m_get_func = cls->m_get_func;
    res->m_set_func = cls->m_set_func;
    res->m_pickle_func = cls->m_pickle_func;
    res->m_html_func = cls->m_html_func;
    res->m_print_func = cls->m_print_func;
    res->m_submit_func = cls->m_submit_func;
    res->m_connection = conn;
    res->m_lock = PR_NewMonitor();
    if (!res->m_lock)
        goto loser;

    if (conn != NULL)
        res->m_id = SSMControlConnection_GenerateResourceID(conn);

    /* will create this monitor when needed */
    res->m_UILock = NULL;
    res->m_formName = NULL;
    res->m_fileName = NULL;
    res->m_clientContext.len = 0;
    res->m_clientContext.data = NULL;
    
    res->m_clientCount = 0;
    res->m_refCount = 1;
    res->m_resourceShutdown = PR_FALSE;

    SSM_DEBUG("Created %s with rid %ld.\n", cls->m_className,
              (long)res->m_id);

    if (SSMControlConnection_AddResource(res, res->m_id) != PR_SUCCESS)
        goto loser;
    
    return PR_SUCCESS;
 loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
    return rv;
}

SSMStatus
SSM_FindResourceClass(SSMResourceType type, SSMResourceClass **cls)
{
    SSMStatus rv = PR_SUCCESS;

    *cls = NULL; /* in case we fail */
    PR_ASSERT(ssm_classRegistry != NULL);

    rv = SSM_HashFind(ssm_classRegistry, type, (void **) cls);
    return rv;
}

PRBool
SSM_IsA(SSMResource *res, SSMResourceType type)
{
    return (res && (res->m_classType == type));
}

PRBool
SSM_IsAKindOf(SSMResource *res, SSMResourceType type)
{
    PRBool result = PR_FALSE;
    SSMResourceClass *cls;

    if (!res) return PR_FALSE;

    SSM_FindResourceClass( res->m_classType, &cls);
    while (cls && (cls->m_classType != type))
        SSM_FindResourceClass(cls->m_superclass, &cls);
    result = (cls != NULL);
    return result;
}

SSMStatus
SSM_RegisterResourceType(char *name,
                         SSMResourceType type,
                         SSMResourceType superClass,
                         SSMClientDestroyAction destAction,
                         SSMResourceCreateFunc createFunc,
                         SSMResourceDestroyFunc destFunc,
                         SSMResourceShutdownFunc shutFunc,
                         SSMResourceGetAttrIDsFunc getIDsFunc,
                         SSMResourceGetAttrFunc getFunc,
                         SSMResourceSetAttrFunc setFunc,
                         SSMResourcePickleFunc  pickleFunc, 
                         SSMResourceUnpickleFunc unpickleFunc,
                         SSMResourceHTMLFunc    htmlFunc,
                         SSMResourcePrintFunc   printFunc,
                         SSMSubmitHandlerFunc   submitFunc)
{
    SSMResourceClass *cls, *super = NULL;
    SSMStatus rv = PR_SUCCESS;

    /* If a superclass was specified, find it. */
    if (superClass)
    {
        rv = SSM_FindResourceClass(superClass, &super);
        if (rv != PR_SUCCESS) goto loser;
    }
        
    /* Create a new entry for the class. */
    cls = (SSMResourceClass *) PR_CALLOC(sizeof(SSMResourceClass));
    if (!cls) goto loser;

    /* Start with the superclass' methods, then overlay the new ones. */
    if (super)
        (void) memcpy(cls, super, sizeof(SSMResourceClass));

    cls->m_className = name;
    cls->m_classType = type;
    cls->m_superclass = superClass;
    cls->m_clientDest = destAction;

    if (createFunc != NULL)
        cls->m_create_func = createFunc;
    if (destFunc != NULL)
        cls->m_destroy_func = destFunc;
    if (shutFunc != NULL)
        cls->m_shutdown_func = shutFunc;
    if (getIDsFunc != NULL)
        cls->m_getids_func = getIDsFunc;
    if (getFunc != NULL)
        cls->m_get_func = getFunc;
    if (setFunc != NULL)
        cls->m_set_func = setFunc;
    if (pickleFunc != NULL)
        cls->m_pickle_func = pickleFunc;
    if (unpickleFunc != NULL) 
        cls->m_unpickle_func = unpickleFunc;
    if (htmlFunc != NULL)
        cls->m_html_func = htmlFunc;
    if (printFunc != NULL)
        cls->m_print_func = printFunc;
    if (submitFunc != NULL)
        cls->m_submit_func = submitFunc;

    rv = SSM_HashInsert(ssm_classRegistry, type, cls);
    if (rv != PR_SUCCESS) goto loser;

    return PR_SUCCESS;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    if (cls) PR_Free(cls);
    return rv;
}

SSMStatus
SSM_CreateResource(SSMResourceType type, void *arg, 
                   SSMControlConnection * connection,
                   SSMResourceID *resID, SSMResource **result)
{
    SSMStatus rv;
    SSMResourceClass *cls = NULL;
    SSMResource *res = NULL;
    
    rv = SSM_FindResourceClass(type, &cls);
    if (rv != PR_SUCCESS) goto loser;

    PR_ASSERT(cls != NULL);
    
    /* Call the create function in this class. */
    rv = (*cls->m_create_func)(arg, connection, &res);
    if ((rv != PR_SUCCESS) && (rv != SSM_ERR_DEFER_RESPONSE))
        goto loser;

    *resID = res->m_id;

    *result = res;
    return rv;

 loser:
    if (res) 
    {
        res->m_refCount = 1; /* so that it is destroyed */
        SSM_FreeResource(res);
    }
    return rv;
}

SSMStatus
SSM_GetResourceReference(SSMResource *res)
{
    SSMResource_Invariant(res);

    SSM_LockResource(res);
    res->m_refCount++;
    SSM_DEBUG("Get ref - rsrcid: %ld ++refcnt: %ld\n", res->m_id, res->m_refCount);
    switch (res->m_classType) {
    case SSM_RESTYPE_CONTROL_CONNECTION:
        SSM_DEBUG("Getting a reference for the control connection\n");
        break;
    default:
        break;
    }
    SSM_UnlockResource(res);

    return PR_SUCCESS;
}

SSMStatus
SSM_FreeResource(SSMResource *res)
{
    PRIntn refcnt;

    PR_ASSERT(res != NULL);

    SSMResource_Invariant(res);
	PR_ASSERT(res->m_refCount > 0);

    SSM_LockResource(res);
    refcnt = --(res->m_refCount);
    SSM_DEBUG("Free ref - rsrcid: %ld --refcnt: %ld\n", res->m_id, res->m_refCount);
    res->m_refCount = refcnt;
    switch (res->m_classType) {
    case SSM_RESTYPE_CONTROL_CONNECTION:
        SSM_DEBUG("Giving up a reference to the control connection\n");
        break;
    default:
        break;
    }
    SSM_UnlockResource(res);

    /* need to handle race condition on destroy */
    if (refcnt <= 0)
    {
        SSM_DEBUG("Destroying resource.\n", 
                                   refcnt);
        return (*res->m_destroy_func)(res, PR_TRUE);
    }

    return PR_SUCCESS;
}

SSMStatus
SSM_GetResAttribute(SSMResource *res, SSMAttributeID fieldID,
                    SSMResourceAttrType attrType, SSMAttributeValue *value)
{
    PR_ASSERT(res != NULL);
    SSMResource_Invariant(res);
    return (*res->m_get_func)(res,fieldID,attrType,value);
}

SSMStatus
SSM_SetResAttribute(SSMResource *res, SSMAttributeID fieldID,
                    SSMAttributeValue *value)
{
    PR_ASSERT(res != NULL);
    SSMResource_Invariant(res);
    return (*res->m_set_func)(res,fieldID,value);
}

SSMStatus 
SSM_PickleResource(SSMResource * res, PRIntn * len, void ** value)
{
    PR_ASSERT(res != NULL);
    return(*res->m_pickle_func)(res, len, value);
}

SSMStatus 
SSM_HTMLResource(SSMResource *res, PRIntn *len, void **value)
{
    PR_ASSERT(res != NULL);
    return(*res->m_html_func)(res, len, value);
}

SSMStatus
SSM_ClientGetResourceReference(SSMResource *res, SSMResourceID *id)
{
    SSM_LockResource(res);
    res->m_clientCount++;
    SSM_DEBUG("Get client ref - rsrcid: %ld ++clientcnt: %ld\n",
              res->m_id, res->m_clientCount);
    SSM_GetResourceReference(res);
    if (id)
        *id = res->m_id;
    SSM_UnlockResource(res);
    return PR_SUCCESS;
}
  
/*
 * Unpickle function is similar to create function. Need the switch statement
 * to call  
 * Unpickle function for different resources.  
 */  
SSMStatus 
SSM_UnpickleResource(SSMResource ** res, SSMResourceType type, 
                     SSMControlConnection * connection,
                     PRIntn len, void * value)
{
    SSMStatus rv = PR_SUCCESS;

    PR_ASSERT(res != NULL);
    if (!res || !value) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }
    *res = NULL; /* in case we fail */
    switch (type) { 
    case SSM_RESTYPE_CERTIFICATE:
        rv = SSMResourceCert_Unpickle(res, connection, len, value);
        break;
    case SSM_RESTYPE_SSL_SOCKET_STATUS:
        rv = SSMSSLSocketStatus_Unpickle(res, connection, len, value);
        break;
    case SSM_RESTYPE_HASH_CONNECTION:
    case SSM_RESTYPE_PKCS7_DECODE_CONNECTION:
    case SSM_RESTYPE_SSL_DATA_CONNECTION:
    default:
        rv = PR_FAILURE;
    }
    goto done;
loser:
    if (rv == PR_SUCCESS) 
        rv = PR_FAILURE;
    if (res && *res) {
        PR_Free(*res);
        *res = NULL;
    }
done:
    return rv;
}


SSMStatus
SSM_ShutdownResource(SSMResource *res, SSMStatus status)
{
    PR_ASSERT(res->m_shutdown_func);
    if (res->m_shutdown_func)
        return (*res->m_shutdown_func)(res, status);
    else
        return PR_FAILURE;
}

SSMStatus 
SSM_ClientDestroyResource(SSMControlConnection * connection, 
                          SSMResourceID rid, SSMResourceType objType)
{
    SSMStatus rv;
    SSMResource *res = NULL;
    PRIntn clientCount;

    rv = SSMControlConnection_GetResource(connection, rid, &res);
    if (rv != PR_SUCCESS)
        goto done;

    PR_ASSERT(res);
    /* If the object is of the right type, free the reference. */
    if (!SSM_IsAKindOf(res, objType))
    {
        rv = SSM_ERR_BAD_RESOURCE_TYPE;
        goto done;
    }

    SSM_LockResource(res);

    clientCount = --(res->m_clientCount);
    SSM_DEBUG("Free client ref - rsrcid: %ld --clientcnt: %ld\n",
              res->m_id, res->m_clientCount);
    /* Also release the resource that was added when the client got
     * its resource
     */
    SSM_FreeResource(res);
    PR_ASSERT(clientCount >= 0);
    if((clientCount == 0)  && (res->m_threadCount > 0))
    {
        SSM_DEBUG("ClientDestroy: Shutting down resource %ld.\n",
                  (long)res->m_id);
        rv = SSM_ShutdownResource(res, SSM_ERR_CLIENT_DESTROY);
        if (rv != PR_SUCCESS)
            goto locked_done;
    }

locked_done:
    SSM_UnlockResource(res);
done:
    if (res != NULL)
        rv = SSM_FreeResource(res);
    SSM_DEBUG("ClientDestroy returning status %d.\n", rv);
    return rv;
}

SSMStatus 
SSM_MessageFormatResource(SSMResource *res,
                          char *fmt,
                          PRIntn numParams,
			  char ** value,
                          char **resultStr)
{
    SSMStatus rv = SSM_FAILURE;

    if (res->m_print_func)
        rv = (*(res->m_print_func))(res, fmt, numParams, value, resultStr);
    return rv;
}

SSMStatus
SSMResource_Shutdown(SSMResource *res, SSMStatus status)
{
    SSMStatus rv = PR_SUCCESS;
    SSM_LockResource(res);
    if ((res->m_status == PR_SUCCESS) || (res->m_status == SSM_ERR_CLIENT_DESTROY))
    {
        SSM_DEBUG("Shutting down %s with rid %ld. Status == %d.\n", 
                  res->m_class->m_className, 
                  (long) res->m_id, status);
        res->m_status = status;

        /* If there is a thread waiting for us to shut down, notify it. */
        if (res->m_waitThread)
            SSM_NotifyResource(res);
    }
    else
        rv = SSM_ERR_ALREADY_SHUT_DOWN;
    res->m_resourceShutdown = PR_TRUE;
    SSM_UnlockResource(res);

    return rv;
}

void
SSMResource_Invariant(SSMResource *res)
{
#ifdef DEBUG
    if (res)
    {
        PR_ASSERT(res->m_id != 0);
        PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_RESOURCE));
        PR_ASSERT(res->m_refCount >= 0);
        PR_ASSERT(res->m_destroy_func != NULL);
        PR_ASSERT(res->m_shutdown_func != NULL);
        PR_ASSERT(res->m_getids_func != NULL);
        PR_ASSERT(res->m_get_func != NULL);
        PR_ASSERT(res->m_set_func != NULL);
        PR_ASSERT(res->m_connection != NULL);
    }
#endif
}

SSMStatus
SSMResource_Create(void *arg, SSMControlConnection * connection, 
                   SSMResource **res)
{
    /* never instantiate a bare SSMResource */
    *res = NULL;
    return PR_FAILURE;
}

SSMStatus
SSMResource_GetAttr(SSMResource *res,
                    SSMAttributeID fieldID, 
                    SSMResourceAttrType attrType,
                    SSMAttributeValue *value)
{
    return SSM_ERR_BAD_ATTRIBUTE_ID;
}

SSMStatus
SSMResource_GetAttrIDs(SSMResource *res,
                       SSMAttributeID **ids,
                       PRIntn *count)
{
    /* return 0 attributes */
    *ids = (SSMAttributeID *) PR_CALLOC(0);
    *count = 0;
    return PR_SUCCESS;
}

SSMStatus
SSMResource_SetAttr(SSMResource *res,
                    SSMAttributeID attrID,
                    SSMAttributeValue *value)
{
    return PR_FAILURE; /* nothing client-accessible in the base class */
}

SSMStatus
SSMResource_Pickle(SSMResource *res, PRIntn * len, void **value)
{
    return PR_FAILURE;
}

SSMStatus
SSMResource_Unpickle(SSMResource ** res, SSMControlConnection *conn,
                     PRIntn len, void * value)
{
    return PR_FAILURE;
}

SSMStatus
SSMResource_HTML(SSMResource *res, PRIntn * len, void ** value)
{
    return PR_FAILURE;
}

SSMStatus 
SSMResource_Print(SSMResource   *res, char *fmt, PRIntn numParams, 
		  char ** value, char **resultStr)
{
    PR_ASSERT(res != NULL && fmt != NULL && resultStr != NULL);
    if (numParams > 3) 
      SSM_DEBUG("SSMResource_Print: too many parameters!\n");
    switch (numParams) {
      case (3): 
        *resultStr = PR_smprintf(fmt, res->m_id, *value, *(value+1),*(value+2));
        break;
      case (2): 
	*resultStr = PR_smprintf(fmt, res->m_id, *value, *(value+1));
        break;
      case (1):
        *resultStr = PR_smprintf(fmt, res->m_id, *value);
        break;
      default:
        *resultStr = PR_smprintf(fmt, res->m_id);
    }
    if (*resultStr == NULL) {
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

SSMStatus
SSMResource_FormSubmitHandler(SSMResource *res,
                              HTTPRequest *req)
{
    return SSM_HTTPDefaultCommandHandler(req);
}

void
SSM_LockResource(SSMResource *res)
{
    PR_EnterMonitor(res->m_lock);
}

SSMStatus
SSM_UnlockResource(SSMResource *res)
{
    return PR_ExitMonitor(res->m_lock);
}

SSMStatus
SSM_WaitResource(SSMResource *res, PRIntervalTime ticks)
{
    return PR_Wait(res->m_lock, ticks);
}

SSMStatus
SSM_NotifyResource(SSMResource *res)
{
    return PR_Notify(res->m_lock);
}

/* 
   Wait for a resource to shut down. 
   ### mwelch We use WaitResource() in another context, namely where
   data connections hand over a newly opened port number to their
   control connections. Can we guard against the case where we have an
   initializing data connection upon which a thread decides to wait for 
   shutdown?
*/
SSMStatus
SSM_WaitForResourceShutdown(SSMResource *res)
{
    SSMStatus rv = PR_SUCCESS;
    SSM_LockResource(res);
    PR_ASSERT(res->m_waitThread == NULL);

    if (res->m_waitThread)
        rv = PR_FAILURE;
    else if (!res->m_resourceShutdown)
    {
        res->m_waitThread = PR_GetCurrentThread();
        do 
            SSM_WaitResource(res, PR_INTERVAL_NO_TIMEOUT);
        while (res->m_threadCount > 0);
        res->m_waitThread = NULL;
    } 
    else 
    {
        rv = SSM_ERR_ALREADY_SHUT_DOWN;
    }
    
    SSM_UnlockResource(res);
    return rv;
}

char * 
SSM_ResourceClassName(SSMResource *res)
{
    return res->m_class->m_className;
}

void SSM_LockUIEvent(SSMResource * res)
{
  if (res->m_UILock == NULL)
    res->m_UILock = PR_NewMonitor();
  PR_ASSERT(res->m_UILock);
  PR_EnterMonitor(res->m_UILock);
}

SSMStatus SSM_UnlockUIEvent(SSMResource *res)
{
  PR_ASSERT(res->m_UILock);
  if (res->m_UILock != NULL) {
    return PR_ExitMonitor(res->m_UILock);
  }
  return PR_FAILURE;
}

SSMStatus SSM_WaitUIEvent(SSMResource * res, PRIntervalTime ticks)
{ 
  PR_ASSERT(res->m_UILock);
  if (res->m_UILock)
    {
     res->m_UIBoolean = PR_FALSE;
     while (!res->m_UIBoolean)
       PR_Wait(res->m_UILock, ticks);
     return PR_ExitMonitor(res->m_UILock);
    }
  /* if we used UILock once, chances are we'll need it again? */
  return PR_FAILURE;
}

SSMStatus SSM_NotifyUIEvent(SSMResource * res)
{
  SSMStatus rv = PR_FAILURE;

  PR_ASSERT(res->m_UILock);
  if (res->m_UILock)
    {
     PR_EnterMonitor(res->m_UILock);
     res->m_UIBoolean = PR_TRUE;
     rv =PR_Notify(res->m_UILock);
     PR_ExitMonitor(res->m_UILock);
    }
  return rv;
}



SSMStatus SSM_WaitForOKCancelEvent(SSMResource *res, PRIntervalTime ticks)
{
  PRStatus rv;
  if (res->m_UILock == NULL) {
    /* We might have been waken up by a spurious notify,
     * so we don't allocate a new monitor every time.
     */
    res->m_UILock = PR_NewMonitor();
  }
  PR_EnterMonitor(res->m_UILock);
  rv = PR_Wait(res->m_UILock, ticks);
  PR_ExitMonitor(res->m_UILock);
  return rv;
}

SSMStatus SSM_NotifyOKCancelEvent(SSMResource *res)
{
  SSMStatus rv = PR_FAILURE;

  if (res->m_UILock) {
    PR_EnterMonitor(res->m_UILock);
    rv = PR_Notify(res->m_UILock);
    PR_ExitMonitor(res->m_UILock);
  }
  return rv;
}

SSMStatus
SSM_HandlePromptReply(SSMResource *res, char *reply)
{
    /* Currently, only SSMPKCS12Context supports this, 
     * perhaps in the future we'll add more general support.
     */
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_PKCS12_CONTEXT)) {
        return PR_FAILURE;
    }
    return SSMPKCS12Context_ProcessPromptReply(res, reply);
}

PRThread *
SSM_CreateThread(SSMResource *res, void (*func)(void *arg))
{
    PRThread *thd = NULL;

    PR_ASSERT(res != NULL);
    PR_ASSERT(func != NULL);

    SSM_LockResource(res);
    thd = SSM_CreateAndRegisterThread(PR_USER_THREAD, 
                          func, 
                          (void *)res, 
                          PR_PRIORITY_NORMAL,
                          PR_LOCAL_THREAD,
                          PR_UNJOINABLE_THREAD, 0);
    if (thd) {
        res->m_threadCount++;
    }
    SSM_UnlockResource(res);

    return thd;
}

