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
#include "p7cinfo.h"
#include "ssmerrs.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(ci) (&(ci)->super)

SSMStatus
SSMP7ContentInfo_Create(void *arg, SSMControlConnection * connection, 
                        SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7ContentInfo *ci;
    *res = NULL; /* in case we fail */
    
    ci = (SSMP7ContentInfo *) PR_CALLOC(sizeof(SSMP7ContentInfo));
    if (!ci) goto loser;
    
    SSMRESOURCE(ci)->m_connection = connection;
    rv = SSMP7ContentInfo_Init(ci, connection, (SEC_PKCS7ContentInfo *) arg, 
			       SSM_RESTYPE_PKCS7_CONTENT_INFO);
    if (rv != PR_SUCCESS) goto loser;

    SSMP7ContentInfo_Invariant(ci);

    *res = SSMRESOURCE(ci);
    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (ci) 
    {
        SSMRESOURCE(ci)->m_refCount = 1; /* force destroy */
        SSM_FreeResource(SSMRESOURCE(ci));
    }
        
    return rv;
}

SSMStatus 
SSMP7ContentInfo_Init(SSMP7ContentInfo *ci, SSMControlConnection * conn, 
					  SEC_PKCS7ContentInfo *cinfo,
					  SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;

    /* Initialize superclass */
    rv = SSMResource_Init(conn, SSMRESOURCE(ci), type);
	if (rv != PR_SUCCESS) goto loser;

	/* Fill in the content info */
	PR_ASSERT(cinfo != NULL);
	ci->m_cinfo = cinfo;

    /* Sanity check before returning */
    SSMP7ContentInfo_Invariant(ci);
    
    goto done;
 loser:
    /* member destruct, if any are allocated, will happen in the
       _Destroy method */
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMP7ContentInfo_Destroy(SSMResource *res, PRBool doFree)
{
    SSMP7ContentInfo *ci = (SSMP7ContentInfo *) res;
    SSMP7ContentInfo_Invariant(ci);
    /* Destroy our members. */
    if (ci->m_cinfo)
    {
        SEC_PKCS7DestroyContentInfo(ci->m_cinfo);
        ci->m_cinfo = NULL;
    }

    /* Destroy superclass. */
    SSMResource_Destroy(res, PR_FALSE);
    
    /* Free if asked. */
    if (doFree)
        PR_Free(res);

    return PR_SUCCESS;
}

void 
SSMP7ContentInfo_Invariant(SSMP7ContentInfo *ci)
{
    /* Superclass invariant */
    SSMResource_Invariant(SSMRESOURCE(ci));
    
    SSM_LockResource(SSMRESOURCE(ci));

    /* Class check */
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(ci), SSM_RESTYPE_PKCS7_CONTENT_INFO));

    /* Member check */
	PR_ASSERT(ci->m_cinfo != NULL);
    if (ci->m_signerCert)
        PR_ASSERT(SSM_IsAKindOf(&(ci->m_signerCert->super),SSM_RESTYPE_CERTIFICATE));

    SSM_UnlockResource(SSMRESOURCE(ci));
}

SSMStatus 
SSMP7ContentInfo_GetAttrIDs(SSMResource *res,
                              SSMAttributeID **ids,
                              PRIntn *count)
{
    SSMStatus rv;
    rv = SSMResource_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 3) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_P7CINFO_IS_SIGNED;
    (*ids)[*count++] = SSM_FID_P7CINFO_IS_ENCRYPTED;
    (*ids)[*count++] = SSM_FID_P7CINFO_SIGNER_CERT;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMP7ContentInfo_GetAttr(SSMResource *res,
                         SSMAttributeID attrID, 
                         SSMResourceAttrType attrType,
                         SSMAttributeValue *value)
{
    SSMP7ContentInfo *cinfo = (SSMP7ContentInfo *) res;
    SEC_PKCS7ContentInfo *info;
    SSMStatus rv = SSM_SUCCESS;

    SSMP7ContentInfo_Invariant(cinfo);
    if (!cinfo)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    SSM_LockResource(SSMRESOURCE(cinfo));
    info = cinfo->m_cinfo;
    if (!info) 
    {
        rv = SSM_ERR_ATTRIBUTE_MISSING;
        goto loser;
    }

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_P7CINFO_IS_SIGNED:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = SEC_PKCS7ContentIsSigned(cinfo->m_cinfo);
        rv = SSM_SUCCESS;
        break;
    case SSM_FID_P7CINFO_IS_ENCRYPTED:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = SEC_PKCS7ContentIsEncrypted(cinfo->m_cinfo);
        rv = SSM_SUCCESS;
        break;
    case SSM_FID_P7CINFO_SIGNER_CERT:
        /* Make sure we have a cert to return. */
        rv = SSM_ERR_ATTRIBUTE_MISSING; /* by default */

        /* want signed data */
        if ((info->content.signedData) && 
            /* want signer info */
            (info->content.signedData->signerInfos) && 
            /* 1 and only 1 signer 
               ### mwelch what if we have more than one? */
            (info->content.signedData->signerInfos[0]) && 
            (info->content.signedData->signerInfos[1] == NULL))
        {
            /* Get the cert, wrap it in a resource, and return its ID. */
            CERTCertificate *cert =
                info->content.signedData->signerInfos[0]->cert;
            SSMResourceID certID;

            if (!cert) goto loser; /* we could still have a null cert */

            rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                                    cert, 
                                    SSMRESOURCE(cinfo)->m_connection,
                                    &certID,
                                    (SSMResource **) &cinfo->m_signerCert);
            SSMP7ContentInfo_Invariant(cinfo);

            value->type = SSM_RID_ATTRIBUTE;
            rv = SSM_ClientGetResourceReference(&cinfo->m_signerCert->super,
                                                &certID);
            if (rv != PR_SUCCESS)
                goto loser;
            rv = SSM_SUCCESS;
    	    value->type = SSM_RID_ATTRIBUTE;
	        value->u.numeric = certID;
            SSM_FreeResource(&cinfo->m_signerCert->super);
        }
        
        break;
    default:
        rv = SSMResource_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS) goto loser;
    }

    goto done;
 loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
 done:
    if (cinfo)
        SSM_UnlockResource(SSMRESOURCE(cinfo));
    return rv;
}

SSMStatus SSMP7ContentInfo_VerifyDetachedSignature(SSMP7ContentInfo *ci,
                                                   SECCertUsage usage,
                                                   HASH_HashType hash,
                                                   PRBool keepCerts,
                                                   PRIntn digestLen,
                                                   char *detachedDigest)
{
	SSMStatus rv = PR_FAILURE;
	SECItem digest;

	SSMP7ContentInfo_Invariant(ci);

	digest.len = digestLen;
	digest.data = (unsigned char*) detachedDigest;

	if (ci->m_cinfo)
	{
		if (SEC_PKCS7VerifyDetachedSignature(ci->m_cinfo, usage,
						 &digest, hash, keepCerts))
			rv = SSM_SUCCESS;
	}
	return rv;
}
