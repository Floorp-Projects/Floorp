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
/*************************************************************************
 * Only server-side message functions are provided: Parse functions for 
 * request messages and Pack functions for reply messages. 
 *
 * Parse functions accept a ptr to the "blob" of data received from the 
 * network and return fields of the message, numerals in host-order, 
 * strings as C-style character strings. If everything goes well,
 * blob of data is freed and SUCCESS is returned, otherwise an error returned
 * and data left intact. Caller may pass NULL ptrs for the fields s/he is
 * not interested in. 
 *
 * Pack functions take all the info to construct a message and fill in a 
 * ptr to the "blob" of data to be sent. Return size of the "blob" or 0
 * if something goes wrong. All fields of the message must be supplied, 
 * otherwise an error is returned.
 *
 * All functions set NSPR errors when necessary. 
 * Caller is responsible for freeing returned values.
 ************************************************************************/

#include "protocolf.h"
#include "rsrcids.h"
#include "nspr.h"


/* Utility functions to handle generic requests/replies */
SSMPRStatus SSM_ParseSingleNumRequest(void *request,
                                      SSMPRUint32 *result)
{
    PRStatus rv = PR_SUCCESS;
    
    if (!request) 
        rv = PR_INVALID_ARGUMENT_ERROR;
    else if (result) 
        *result = SSMPR_ntohl(*(SSMPRUint32 *)request);

    /* ### mwelch Don't free this here, because the message-specific
       handlers will free this for us. */
    /*    if (request)
        PR_Free(request);*/
    return rv;
}

SSMPRInt32 SSM_PackSingleNumReply(void **reply, SSMPRUint32 num)
{
    SSMPRStatus rv = PR_SUCCESS;
    if (!reply)
        rv = SSMPR_INVALID_ARGUMENT_ERROR;
    else
    {
        /* allocate memory for blob of data */
        *reply = (void *)SSMPORT_ZAlloc(sizeof(SSMPRUint32));
        if (!*reply) 
            rv = SSMPR_OUT_OF_MEMORY_ERROR;
    }

    if (rv == PR_SUCCESS)
    {
        *(SSMPRInt32 *)*reply = SSMPR_htonl(num);
        return sizeof(SSMPRUint32);
    }
    else
    {
        SSMPORT_SetError(rv);
        return 0;
    }    
}


/* Initial messages - establishing connection */

SSMPRStatus SSM_ParseHelloRequest(void * helloRequest, 
                                  SSMPRUint32 * version, 
                                  PRBool *doesUI,
                                  PRInt32 * policyType,
                                  SSMPRUint32 * profileLen, 
                                  char ** profile)
{
  void * curptr = helloRequest;
  SSMPRStatus rv = SSMPR_SUCCESS;

  if (!helloRequest) {
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  
  /* read and store protocol version */
  if (version) 
    *version = SSMPR_ntohl(*((SSMPRUint32 *)curptr));
  curptr = (SSMPRUint32 *)curptr + 1;
  if (policyType)
      *policyType = PR_ntohl(*((PRInt32 *)curptr));
  curptr = (SSMPRUint32 *)curptr + 1;
  if (doesUI)
      *doesUI = PR_ntohl(*((SSMPRInt32 *)curptr));
  curptr = (SSMPRUint32 *)curptr + 1;
  if (profile) { 
    *profile = NULL;
    /* read and store profile name that client uses */
    rv = SSM_SSMStringToString(profile, NULL, (SSMString *)curptr);
    if (rv != SSMPR_SUCCESS && *profile)
      SSMPORT_Free(*profile);
  }
loser:
  if (helloRequest)
  SSMPORT_Free(helloRequest);
  return rv;
}


SSMPRInt32 SSM_PackHelloReply(void ** helloReply, SSMPRInt32 result, 
			      SSMPRUint32 sessionID, SSMPRUint32 version, 
			      SSMPRUint32 httpPort, 
			      SSMPRUint32 nonceLength, char * nonce,
			      SSMPolicyType policy)
{
  SSMPRInt32 blobSize;
  void * curptr, * nonceStr;
  SSMPRStatus rv;

  if (!helloReply || !nonce || !*nonce || policy == ssmUnknownPolicy) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  blobSize = sizeof(SSMPRInt32)+ sizeof(SSMPRUint32) * 5 + 
    SSMSTRING_PADDED_LENGTH(strlen(nonce));
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    rv = PR_OUT_OF_MEMORY_ERROR;
    *helloReply = NULL;
    goto loser;
  }
  *helloReply = curptr;

  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  curptr = (SSMPRInt32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(sessionID); 
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(version); 
  curptr = (SSMPRUint32 *)curptr + 1;
   *(SSMPRUint32 *)curptr = SSMPR_htonl(httpPort); 
  curptr = (SSMPRUint32 *)curptr + 1;
   *(SSMPRUint32 *)curptr = SSMPR_htonl(policy); 
  curptr = (SSMPRUint32 *)curptr + 1;
  
  rv = SSM_StringToSSMString((SSMString **)&nonceStr, 0, nonce);
  if (rv != SSMPR_SUCCESS) { 
    if (nonceStr) SSMPORT_Free(nonceStr); /* free string */
    goto loser;
 }
  memcpy(curptr, nonceStr, SSM_SIZEOF_STRING(*(SSMString *)nonceStr));
  SSMPORT_Free(nonceStr);
  
  return blobSize;
loser:
  if (*helloReply)
	PR_Free(*helloReply);
  return 0;
}


/* Handle data connection messages */
/* SSL data connection request */
SSMPRStatus SSM_ParseSSLDataConnectionRequest(void *sslRequest, 
					      SSMPRUint32 * flags, 
					      SSMPRUint32 * port, 
					      SSMPRUint32 * hostIPLen, 
					      char ** hostIP, 
					      SSMPRUint32 * hostNameLen,
					      char ** hostName)
{
  SSMPRStatus rv = SSMPR_SUCCESS;
  void * curptr;
  
  if (!sslRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  curptr = sslRequest;
  if (flags) *flags = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  if (port) *port = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  
  if (hostIP) { 
    *hostIP = NULL;
    /* read and store hostIP */
    rv = SSM_SSMStringToString(hostIP,  NULL, (SSMString *)curptr);
    if (rv != SSMPR_SUCCESS)  
      goto loser; 
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);

  if (hostName) { 
    *hostName = NULL;
    /* read and store hostName */
    rv = SSM_SSMStringToString(hostName,  NULL, (SSMString *)curptr);
    if (rv != SSMPR_SUCCESS)  
         goto loser;
  }
goto done;
loser:
    if (hostName && *hostName) 
	PR_Free(*hostName);
    if (hostIP && *hostIP)
	PR_Free(*hostIP);
done:
  if (sslRequest)
     SSMPORT_Free(sslRequest);
  return rv;
}

/* Hash stream data connection request */
SSMPRStatus SSM_ParseHashStreamRequest(void * hashStreamRequest, 
				       SSMPRUint32 * type)
{
 PRStatus rv = PR_SUCCESS;

  if (!hashStreamRequest) {
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (type) *type = SSMPR_ntohl(*(SSMPRUint32 *)hashStreamRequest);

 loser:
  if (hashStreamRequest) 
     SSMPORT_Free(hashStreamRequest);
  return rv;
}

/* Messages to initiate PKCS7 data connection */
/* PKCS7DecodeRequest message has no data */ 

/* Create PKCS7 encode connection.  Needs a content info */
SSMPRStatus SSM_ParseP7EncodeConnectionRequest(void *request,
					       SSMPRUint32 *ciRID)
{
    return SSM_ParseSingleNumRequest(request, ciRID);
}

/* Create data connection reply - same for all types of data connections */
SSMPRInt32 SSM_PackDataConnectionReply(void ** sslReply, 
				       SSMPRInt32 result, 
				       SSMPRUint32 connID, 
				       SSMPRUint32 port)
{
  void * curptr = NULL;
  SSMPRInt32 blobSize;

  if (!sslReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  
  blobSize = sizeof(SSMPRInt32) + sizeof(SSMPRUint32)*2;
  /* allocate space for the SSLDataConnectionReply "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    *sslReply = NULL;
    return 0;
  }
  *sslReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  curptr = (SSMPRInt32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(connID);
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(port);

  return blobSize;
}


/* Messages to create SocketStatus resource */
SSMPRStatus SSM_ParseSSLSocketStatusRequest(void * statusRequest, 
					    SSMPRUint32 * connID)
{ 
  PRStatus rv = PR_SUCCESS;

  if (!statusRequest ) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (connID) *connID = SSMPR_ntohl(*((SSMPRUint32 *)statusRequest));
loser:
  if (statusRequest)
    SSMPORT_Free(statusRequest);
  return rv;
}   

SSMPRInt32 SSM_PackSSLSocketStatusReply(void ** statusReply, 
					SSMPRInt32 result, 
					SSMPRUint32 resourceID)
{
  SSMPRInt32 blobSize;
  void * curptr;
  
  if (!statusReply){ 
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  /* allocate memory for blob of data */
  blobSize = sizeof(SSMPRUint32) + sizeof(SSMPRInt32);
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    *statusReply = NULL;
    return 0;
  }
  *statusReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  curptr = (SSMPRInt32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(resourceID); 
  
  return blobSize;
}


/* UI event message - server initiated */
SSMPRInt32 SSM_PackUIEvent(void ** eventData, SSMPRUint32 resourceID, 
			   SSMPRUint32 width, SSMPRUint32 height, 
			   SSMPRUint32 urlLen, char * url)
{
  void * curptr, * tmpStr;
  SSMPRInt32 blobSize;
  SSMPRStatus rv;

  if (!eventData){ 
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  /* allocate memory for blob of data */
  blobSize = sizeof(SSMPRUint32) * 4 + SSMSTRING_PADDED_LENGTH(strlen(url));

  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    *eventData = NULL;
    return 0;
  }
  *eventData = curptr;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(resourceID);
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(width);
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(height);
  curptr = (SSMPRUint32 *)curptr + 1;
  
  /* copy URL string into the blob */
  rv = SSM_StringToSSMString((SSMString **)&tmpStr, 0, url);
  if (rv != SSMPR_SUCCESS) { 
    if (tmpStr) SSMPORT_Free(tmpStr); /* free string */
    SSMPORT_Free(*eventData);            /* free blob   */
    *eventData = NULL;
    return 0;
  }
  memcpy(curptr, tmpStr, SSM_SIZEOF_STRING(*(SSMString *)tmpStr));
  SSMPORT_Free(tmpStr);
  
  return blobSize;
}

SSMPRInt32 SSM_PackTaskCompletedEvent(void **event, SSMPRUint32 resourceID, 
                                      SSMPRUint32 numTasks, 
                                      SSMPRUint32 result)
{
  void * curptr;
  SSMPRInt32 blobSize;

  if (!event){ 
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  /* allocate memory for blob of data */
  blobSize = sizeof(SSMPRUint32) * 3;
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    *event = NULL;
    return 0;
  }
  *event = curptr;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(resourceID);
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(numTasks);
  curptr = (SSMPRUint32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(result);
  
  return blobSize;
}

/* Handle verify signature messages */
SSMPRStatus SSM_ParseVerifyRawSigRequest(void * verifyRawSigRequest, 
					 SSMPRUint32 * algorithmID,
					 SSMPRUint32 *paramsLen,
					 unsigned char ** params,
					 SSMPRUint32 * pubKeyLen,
					 unsigned char ** pubKey,
					 SSMPRUint32 * hashLen,
					 unsigned char ** hash,
					 SSMPRUint32 * signatureLen,
					 unsigned char ** signature)
{
  void * curptr = verifyRawSigRequest;
  SSMPRStatus rv = SSMPR_SUCCESS;

  if (!verifyRawSigRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  if (algorithmID) *algorithmID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
 
  if (params) { 
     *params = NULL;
     /* read and store cipher */
     rv = SSM_SSMStringToString((char **)params,  NULL, (SSMString *)curptr);
     if (rv != SSMPR_SUCCESS) 
       goto loser;
  }  
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);

  if (pubKey) { 
     *pubKey = NULL;
     /* read and store cipher */
     rv = SSM_SSMStringToString((char **)pubKey,  NULL, (SSMString *)curptr);
     if (rv != SSMPR_SUCCESS) 
       goto loser;
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);
  if (signature) { 
    *signature = NULL;
    /* read and store cipher */
    rv = SSM_SSMStringToString((char **)signature,  NULL, (SSMString *)curptr);
    if (rv != SSMPR_SUCCESS) 
      goto loser;  
  }
 goto done;
 
loser:
  if (params && *params) 
    SSMPORT_Free(*params);
  if (pubKey && *pubKey) 
    SSMPORT_Free(*pubKey);
  if (signature && *signature) 
    SSMPORT_Free(*signature);
done:
  if (verifyRawSigRequest)
     PR_Free(verifyRawSigRequest); 
  return rv;
}

SSMPRInt32 SSM_PackVerifyRawSigReply(void ** verifyRawSigReply, 
				     SSMPRInt32 result)
{
  SSMPRInt32 blobSize;
  void * curptr;

  if (!verifyRawSigReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  
  *verifyRawSigReply = NULL;
  blobSize = sizeof(SSMPRInt32);
  /* allocate space for the verifyRawSigReply "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  *verifyRawSigReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  
  return blobSize;
}

SSMPRStatus SSM_ParseVerifyDetachedSigRequest(void * verifyDetachedSigRequest,
					      SSMPRInt32 * pkcs7ContentID,
					      SSMPRInt32 * certUsage,
					      SSMPRInt32 * hashAlgID,
					      SSMPRUint32 * keepCert,
					      SSMPRUint32 * hashLen,
					      unsigned char ** hash)
{
  void * curptr = verifyDetachedSigRequest;
  SSMPRStatus rv = SSMPR_SUCCESS;

  if (!verifyDetachedSigRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  if (pkcs7ContentID) *pkcs7ContentID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  
  if (certUsage) *certUsage = SSMPR_ntohl(*(SSMPRInt32 *)curptr);
  curptr = (SSMPRInt32 *)curptr + 1;
  
  if (hashAlgID) *hashAlgID = SSMPR_ntohl(*(SSMPRInt32 *)curptr);
  curptr = (SSMPRInt32 *)curptr + 1;
  
  if (keepCert) *keepCert = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;  
 
  if (hash) { 
     *hash = NULL;
     /* read and store cipher */
     rv = SSM_SSMStringToString((char **)hash,  (PRInt32 *)hashLen, (SSMString *)curptr);
     if (rv != SSMPR_SUCCESS)  
         goto loser;
  }
  goto done;

loser:
  if (hash && *hash)
     PR_Free(*hash); 
done: 
  if (verifyDetachedSigRequest) 
     PR_Free(verifyDetachedSigRequest);
  return rv;
}


SSMPRInt32 SSM_PackVerifyDetachedSigReply(void ** verifyDetachedSigReply, 
				     SSMPRInt32 result)
{
  SSMPRInt32 blobSize;
  void * curptr;

  if (!verifyDetachedSigReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }  
  *verifyDetachedSigReply = NULL;
  blobSize = sizeof(SSMPRInt32);
  /* allocate space for the helloRequest "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  *verifyDetachedSigReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  
  return blobSize;
}

/* Messages for the resource management */


SSMPRStatus SSM_ParseCreateSignedRequest(void *request,
                                         SSMPRInt32 *scertRID,
                                         SSMPRInt32 *ecertRID,
                                         SSMPRUint32 *dig_alg,
                                         SECItem **digest)
{
    unsigned char *curPtr = (unsigned char *) request;
    SSMPRStatus rv;

    if (!request) {
        SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
        return SSMPR_FAILURE;
    }
    *scertRID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    *ecertRID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    *dig_alg = SSMPR_ntohl(*(SSMPRUint32*)curPtr);
    curPtr += sizeof(SSMPRUint32);
    *digest = PR_NEWZAP(SECItem);
    if (*digest == NULL)
        goto loser;
    rv = SSM_SSMStringToString(&(*digest)->data, &(*digest)->len,
                               (SSMString *) curPtr);
    if (rv != SSMPR_SUCCESS)
        goto loser;
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMPRInt32 SSM_PackCreateSignedReply(void **reply, SSMPRInt32 ciRID,
                                     SSMPRUint32 result)
{
    SSMPRInt32     blobSize;
    unsigned char *curPtr;
    
    blobSize = sizeof (SSMPRInt32) + sizeof(SSMPRUint32);
    *reply = curPtr = PORT_ZAlloc(blobSize);
    if (curPtr == NULL) {
        SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
        return 0;
    }
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(ciRID);
    curPtr += sizeof(SSMPRInt32);
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(result);
    
    return blobSize;
}

SSMPRStatus SSM_ParseCreateEncryptedRequest(void *request,
                                            SSMPRInt32 *scertRID,
                                            SSMPRInt32 *nrcerts,
                                            SSMPRInt32 **rcertRIDs)
{
    unsigned char *curPtr = (unsigned char *) request;
    SSMPRStatus rv;
    SSMPRInt32 ncerts;
    int i;

    if (!request) {
        SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
        return SSMPR_FAILURE;
    }
    *scertRID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    ncerts = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    *rcertRIDs = PR_Calloc(ncerts+1, sizeof(SSMPRInt32));
    curPtr += sizeof(SSMPRInt32);
    for (i = 0; i < ncerts; i++) {
        (*rcertRIDs)[i] = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
        curPtr += sizeof(SSMPRInt32);
    }
    *nrcerts = ncerts;
    return SSMPR_SUCCESS;
}

SSMPRInt32 SSM_PackCreateEncryptedReply(void **reply, SSMPRInt32 ciRID,
                                        SSMPRUint32 result)
{
    SSMPRInt32     blobSize;
    unsigned char *curPtr;
    
    blobSize = sizeof (SSMPRInt32) + sizeof(SSMPRUint32);
    *reply = curPtr = PORT_ZAlloc(blobSize);
    if (curPtr == NULL) {
        SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
        return 0;
    }
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(ciRID);
    curPtr += sizeof(SSMPRInt32);
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(result);
    
    return blobSize;
}

SSMPRStatus SSM_ParseCreateResourceRequest(void *request,
                                           SSMPRUint32 *type,
                                           unsigned char **params,
                                           SSMPRUint32 *paramLen)
{
    unsigned char *curPtr = (unsigned char*)request;
    SSMPRStatus    rv     = SSMPR_SUCCESS;

    if (!request)
    {
        SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
        return SSMPR_FAILURE;
    }

    /* Get stuff out. */
    if (type)
        *type = SSMPR_ntohl(*((SSMPRInt32*)curPtr));
    curPtr += sizeof(SSMPRInt32);

    if (params)
        rv = SSM_SSMStringToString((char **) params, (int*) paramLen,
                                   (SSMString *) curPtr);

    return rv;
}

SSMPRStatus SSM_PackCreateResourceReply(void **reply, SSMPRStatus rv,
                                        SSMPRUint32 resID)
{
    SSMPRInt32     blobSize;
    unsigned char *curPtr;
    
    blobSize = sizeof (SSMPRInt32) + sizeof(SSMPRUint32);
    *reply = curPtr = PORT_ZAlloc(blobSize);
    if (curPtr == NULL) {
        SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
        return 0;
    }
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(rv);
    curPtr += sizeof(SSMPRInt32);
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(resID);
    
    return blobSize;
}

SSMPRStatus SSM_ParseDuplicateResourceRequest(void *request,
					      SSMPRUint32 *resourceID)
{
    return SSM_ParseSingleNumRequest(request, resourceID);
}
 
SSMPRInt32 SSM_PackDuplicateResourceReply(void ** reply, SSMPRInt32 result,
					  SSMPRUint32 resID)
{
    SSMPRInt32     blobSize;
    unsigned char *curPtr;
    
    blobSize = sizeof (SSMPRInt32) + sizeof(SSMPRUint32);
    *reply = curPtr = PORT_ZAlloc(blobSize);
    if (curPtr == NULL) {
        SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
        return 0;
    }
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(result);
    curPtr += sizeof(SSMPRInt32);
    *((SSMPRInt32*)curPtr) = SSMPR_htonl(resID);
    
    return blobSize;
}

SSMPRStatus SSM_ParseGetAttribRequest(void * getAttribRequest, 
				      SSMPRUint32 * resourceID, 
				      SSMPRUint32 * fieldID)
{ 
  void * curptr = getAttribRequest; 
  PRStatus rv = PR_SUCCESS;

  if (!getAttribRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (resourceID) *resourceID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  
  if (fieldID) *fieldID = SSMPR_ntohl(*(SSMPRInt32 *)curptr);
  curptr = (SSMPRInt32 *)curptr + 1;
  
loser:
  if (getAttribRequest)
     PR_Free(getAttribRequest);
  return rv;
}

SSMPRInt32 SSM_PackGetAttribReply(void **getAttribReply,
				  SSMPRInt32 result,
				  SSMAttributeValue *value)
{
  SSMPRInt32 blobSize, fieldlength;
  void * curptr;

  if (!getAttribReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  *getAttribReply = NULL;
  
  /* Calculate length of the field value. Binary length (fielddatalen)
	takes precedence over using strlen(). */
  fieldlength = (value->type == SSM_STRING_ATTRIBUTE) ? 
	  PR_ntohl(SSMSTRING_PADDED_LENGTH(value->u.string->m_length)) : 0;
  fieldlength += sizeof(SSMPRUint32);

  blobSize = sizeof(SSMPRInt32) + fieldlength;
  /* allocate space for the getAttribReply "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  * getAttribReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  curptr = (SSMPRInt32 *)curptr + 1;

  switch(value->type) {
   case SSM_STRING_ATTRIBUTE:
      /* This value is stored in network byte order, no need to switch
       * it back
       */
      *(SSMPRUint32 *)curptr = value->u.string->m_length;
      curptr = (SSMPRInt32 *)curptr + 1;
      memcpy(curptr, &value->u.string->m_data, 
             PR_ntohl(value->u.string->m_length));
      break;
   case SSM_RID_ATTRIBUTE:
      *(SSMPRUint32 *)curptr = SSMPR_htonl(value->u.rid);
      break;
   case SSM_NUMERIC_ATTRIBUTE:
      *(SSMPRUint32 *)curptr = SSMPR_htonl(value->u.numeric);
      break;
  }
  
  return blobSize;
}

SSMPRStatus
SSM_ParseSetAttribRequest(SECItem             *msg,
			  SSMPRInt32          *resourceID,
			  SSMPRInt32          *fieldID,
			  SSMAttributeValue   *value)
{
  unsigned char *curPtr;
  SSMPRInt32 strLen;

  if (!msg      || !msg->data || !resourceID || !fieldID || !value){
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return SSMPR_FAILURE;
  }
  curPtr = msg->data;
  value->type = msg->type & SSM_SPECIFIC_MASK;

  switch (value->type) {
  case SSM_NUMERIC_ATTRIBUTE:
    *resourceID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    *fieldID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    value->u.numeric = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    break;
  case SSM_RID_ATTRIBUTE:
    *resourceID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    *fieldID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    value->u.rid = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    break;
  case SSM_STRING_ATTRIBUTE:
    *resourceID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    *fieldID = SSMPR_ntohl(*(SSMPRInt32*)curPtr);
    curPtr += sizeof(SSMPRInt32);
    strLen = msg->len - (curPtr - msg->data);
    value->u.string = SSMPORT_ZAlloc(strLen);
    memcpy (value->u.string, curPtr, strLen);
    break;
  default:
    return SSMPR_FAILURE;
  }
  
  return SSMPR_SUCCESS;
}


/* Messages to pickle and unpickle a resource. */
SSMPRStatus SSM_ParsePickleResourceRequest(void * pickleResourceRequest, 
					  SSMPRUint32 * resourceID)
{ 
  void * curptr = pickleResourceRequest;
  PRStatus rv = PR_SUCCESS;

  if (!pickleResourceRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (resourceID) *resourceID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  
loser:
  if (pickleResourceRequest)
     SSMPORT_Free(pickleResourceRequest);
  return rv;
}

SSMPRInt32 SSM_PackPickleResourceReply(void ** pickleResourceReply, 
				       SSMPRInt32 result,
				       SSMPRUint32 resourceLen, 
				       void * resource)
{ 
    SSMPRInt32 blobSize;
    void * curptr, *tmpStr = NULL;
    PRStatus rv;
    
    if (!pickleResourceReply) 
      goto loser;
    
    *pickleResourceReply = NULL;
    blobSize = sizeof(SSMPRInt32) + sizeof(SSMPRUint32) + 
	SSMSTRING_PADDED_LENGTH(resourceLen); 
    
    /* allocate space for the helloRequest "blob" */
    curptr = (void *)SSMPORT_ZAlloc(blobSize);
    if (!curptr)  
      goto loser;
    
    *pickleResourceReply = curptr;
    *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
    curptr = (SSMPRInt32 *)curptr + 1;
    
    rv = SSM_StringToSSMString((SSMString **)&tmpStr, resourceLen, resource);
    if (rv != SSMPR_SUCCESS) 
      goto loser;
    memcpy(curptr, tmpStr, SSM_SIZEOF_STRING(*(SSMString *)tmpStr));
    goto done;

loser:
    if (pickleResourceReply && *pickleResourceReply) 
      PR_Free(*pickleResourceReply);
    if (tmpStr) 
      PR_Free(tmpStr);
done:
    return blobSize;
}


SSMPRStatus SSM_ParseUnpickleResourceRequest(void * unpickleResourceRequest, 
					     SSMPRUint32 blobSize,
					     SSMPRUint32 * resourceType,
					     SSMPRUint32 * resourceLen, 
					     void ** resource)
{
    void * curptr = unpickleResourceRequest;
    SSMPRStatus rv = PR_SUCCESS;
    
    if (!unpickleResourceRequest) { 
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    if (resourceType)  
        *resourceType = PR_ntohl(*(SSMPRUint32 *)curptr);
    curptr = (SSMPRUint32 *)curptr + 1;
    
    if (resource) {
        rv = SSM_SSMStringToString((char **)resource,  (SSMPRInt32 *)resourceLen, 
                                   (SSMString *)curptr);
        if (rv != SSMPR_SUCCESS)  
            goto loser; 
    }
    goto done;
loser:
    if (resource && *resource)
      PR_Free(*resource);
done: 
    if (unpickleResourceRequest)
      PR_Free(unpickleResourceRequest); 
    return rv;
}

SSMPRInt32 SSM_PackUnpickleResourceReply(void ** unpickleResourceReply, 
					 SSMPRInt32 result, 
					 SSMPRUint32 resourceID)
{
  SSMPRInt32 blobSize;
  void * curptr;
  
  if (!unpickleResourceReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }  
  *unpickleResourceReply = NULL;
  blobSize = sizeof(SSMPRInt32) + sizeof(SSMPRUint32);

  /* allocate space for the helloRequest "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  *unpickleResourceReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  curptr = (SSMPRInt32 *)curptr + 1;
  *(SSMPRUint32 *)curptr = SSMPR_htonl(resourceID);

  return blobSize;
}

/* Destroy resource messages */
SSMPRStatus SSM_ParseDestroyResourceRequest(void * destroyResourceRequest, 
					    SSMPRUint32 * resourceID, 
					    SSMPRUint32 * resourceType)
{ 
  void * curptr = destroyResourceRequest;
  PRStatus rv = PR_SUCCESS;

  if (!destroyResourceRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (resourceID) *resourceID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  if (resourceType) *resourceType = SSMPR_ntohl(*(SSMPRUint32 *)curptr);

loser:
  if (destroyResourceRequest)
     SSMPORT_Free(destroyResourceRequest);
  return rv;
}
 
SSMPRInt32 SSM_PackDestroyResourceReply(void ** destroyResourceReply, 
					SSMPRInt32 result)
{
    return SSM_PackSingleNumReply(destroyResourceReply, result);
}

SSMPRStatus SSM_ParseVerifyCertRequest(void * verifyCertRequest, 
                                       SSMPRUint32 * resourceID, 
                                       SSMPRInt32 * certUsage)
{
  void * curptr = verifyCertRequest;
  PRStatus rv = PR_SUCCESS;

  if (!verifyCertRequest) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  if (resourceID) *resourceID = SSMPR_ntohl(*(SSMPRUint32 *)curptr);
  curptr = (SSMPRUint32 *)curptr + 1;
  if (certUsage) *certUsage = SSMPR_ntohl(*(SSMPRInt32 *)curptr);

loser:
  if (verifyCertRequest)
  SSMPORT_Free(verifyCertRequest);
  return rv;
}

SSMPRInt32 SSM_PackVerifyCertReply(void ** verifyCertReply, 
                                   SSMPRInt32 result)
{
  SSMPRInt32 blobSize;
  void * curptr;
  
  if (!verifyCertReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }
  *verifyCertReply = NULL;
  blobSize = sizeof(SSMPRInt32);
  /* allocate space for the helloRequest "blob" */
  curptr = (void *)SSMPORT_ZAlloc(blobSize);
  if (!curptr) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  *verifyCertReply = curptr;
  *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
  
  return blobSize;
}

SSMPRStatus 
SSM_ParseImportCertRequest(void * importCertRequest, 
			   SSMPRUint32 * blobLen, 
			   void ** certBlob)
{
  PRStatus rv;
  void * curptr = importCertRequest;
  PRUint32 length = 0;

  if (!importCertRequest) {
     rv = PR_INVALID_ARGUMENT_ERROR;
     goto loser;
  }
  
  /* in case we fail */
  if (certBlob) *certBlob = NULL; 
  if (blobLen) 
    *blobLen = 0;

  if (certBlob) {
    rv = SSM_SSMStringToString((char **)certBlob,  (SSMPRInt32 *)&length, 
			       (SSMString *)curptr);
    if (rv != SSMPR_SUCCESS)  
       goto loser;   
  } 
  if (blobLen) 
    *blobLen = length;
  goto done;

loser:
   if (certBlob && *certBlob)
	PR_Free(*certBlob);
done:
   if (importCertRequest)
      SSMPORT_Free(importCertRequest);
   return rv;
}

  
SSMPRInt32 
SSM_PackImportCertReply(void ** importCertReply, SSMPRInt32 result, 
			SSMPRUint32 resourceID)
  {
    SSMPRInt32 blobSize;
    void * curptr;
    
    if (!importCertReply) 
      goto loser;
    
    *importCertReply = NULL;
    blobSize = sizeof(SSMPRInt32) + sizeof(SSMPRUint32);
    
    /* allocate space for the "blob" */
    curptr = (void *)SSMPORT_ZAlloc(blobSize);
    if (!curptr) 
      goto loser;

    *importCertReply = curptr;
    *(SSMPRInt32 *)curptr = SSMPR_htonl(result);
    curptr = (SSMPRInt32 *)curptr + 1;
    *(SSMPRUint32 *)curptr = SSMPR_htonl(resourceID);
    
    return blobSize;
  loser:
    if (importCertReply && *importCertReply)
      PR_Free(*importCertReply);
    return 0;
  }

PRStatus
SSM_ParseFindCertByNicknameRequest(void *request, char ** nickname)
{
    unsigned char * curPtr = request;
    PRStatus rv = PR_SUCCESS;

    /* Do some basic parameter checking */
    if (!request || !nickname) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Get the certificate nickname */
    rv = SSM_SSMStringToString(nickname, NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    goto done;
loser:
done:
    if (request) {
        PR_Free(request);
    }
    return rv;
}

PRInt32
SSM_PackFindCertByNicknameReply(void ** reply, PRUint32 resourceID)
{
    unsigned char *curPtr;
    PRInt32 replyLength;

    /* Do some basic parameter checking */
    if (!reply) {
        goto loser;
    }

    /* Calculate the message length */
    replyLength = sizeof(PRUint32);

    curPtr = *reply = PR_Malloc(replyLength);
    if (!curPtr) {
        goto loser;
    }

    *(PRUint32*)curPtr = PR_htonl(resourceID);

    return replyLength;
loser:
    if (reply && *reply) {
        PR_Free(*reply);
    }
    return 0;
}

PRStatus
SSM_ParseFindCertByKeyRequest(void *request, SECItem ** key)
{
    unsigned char *curPtr = request;
    PRStatus rv = PR_SUCCESS;

    /* Do some basic parameter checking */
    if (!request || !key) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Allocate the key */
    *key = PR_NEWZAP(SECItem);
    if (!(*key)) {
        goto loser;
    }

    /* Get the key */
    rv = SSM_SSMStringToString(&((*key)->data), &((*key)->len),
			       (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    goto done;
loser:
    if (*key) {
        if ((*key)->data) {
            PR_Free((*key)->data);
        }
        PR_Free(*key);
    }
done:
    if (request) {
        PR_Free(request);
    }
    return rv;
}

PRInt32
SSM_PackFindCertByKeyReply(void ** reply, PRUint32 resourceID)
{
    unsigned char *curPtr;
    PRInt32 replyLength;

    /* Do some basic parameter checking */
    if (!reply) {
        goto loser;
    }

    /* Calculate the message length */
    replyLength = sizeof(PRUint32);

    curPtr = *reply = PR_Malloc(replyLength);
    if (!curPtr) {
        goto loser;
    }

    *(PRUint32*)curPtr = PR_htonl(resourceID);

    return replyLength;
loser:
    if (reply && *reply) {
        PR_Free(*reply);
    }
    return 0;
}

PRStatus
SSM_ParseFindCertByEmailAddrRequest(void *request, char ** emailAddr)
{
    unsigned char * curPtr = request;
    PRStatus rv = PR_SUCCESS;

    /* Do some basic parameter checking */
    if (!request || !emailAddr) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Get the certificate nickname */
    rv = SSM_SSMStringToString(emailAddr, NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    goto done;
loser:
done:
    if (request) {
        PR_Free(request);
    }
    return rv;
}

PRInt32
SSM_PackFindCertByEmailAddrReply(void ** reply, PRUint32 resourceID)
{
    unsigned char *curPtr;
    PRInt32 replyLength;

    /* Do some basic parameter checking */
    if (!reply) {
        goto loser;
    }

    /* Calculate the message length */
    replyLength = sizeof(PRUint32);

    curPtr = *reply = PR_Malloc(replyLength);
    if (!curPtr) {
        goto loser;
    }

    *(PRUint32*)curPtr = PR_htonl(resourceID);

    return replyLength;
loser:
    if (reply && *reply) {
        PR_Free(*reply);
    }
    return 0;
}

PRStatus
SSM_ParseAddTempCertToDBRequest(void *request, PRUint32 *resourceID, char ** nickname, PRInt32 *ssl, PRInt32 *email, PRInt32 *objectSigning)
{
    unsigned char * curPtr = request;
    PRStatus rv = PR_SUCCESS;

    /* Do some basic parameter checking */
    if (!request || !resourceID || !nickname) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Get the resource ID */
    *resourceID = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    /* Get the nickname */
    rv = SSM_SSMStringToString(nickname, NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* SSL */
    *ssl = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    /* Email */
    *email = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    /* Object signing */
    *objectSigning = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    goto done;
loser:
    if (nickname && *nickname) {
        PR_Free(*nickname);
    }
done:
    if (request) {
        PR_Free(request);
    }
    return rv;
}

PRInt32
SSM_PackAddTempCertToDBReply(void ** reply)
{
    unsigned char *curPtr;
    PRInt32 replyLength;

    /* Do some basic parameter checking */
    if (!reply) {
        goto loser;
    }

    /* Calculate the message length */
    replyLength = sizeof(PRInt32)*3;

    curPtr = *reply = PR_Malloc(replyLength);
    if (!curPtr) {
        goto loser;
    }

    return replyLength;
loser:
    if (reply && *reply) {
        PR_Free(*reply);
    }
    return 0;
}

PRStatus SSM_ParseMatchUserCertRequest(void *request, MatchUserCertRequestData** data)
{
    MatchUserCertRequestData * requestData;
    char *curPtr = request;
    PRStatus rv = PR_SUCCESS;
    int i;

    /* Do some basic parameter checking */
    if (!request || !data) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Allocate the reply structure */
    requestData = PR_NEWZAP(MatchUserCertRequestData);
    if (NULL == requestData) {
        rv = PR_FAILURE;
        goto loser;
    }

    /* Get the cert type */
    requestData->certType = PR_ntohl(*(PRUint32*)curPtr);
    curPtr += sizeof(PRUint32);

    /* Get the number of CAs */
    requestData->numCANames = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    /* Get the CA names */
    for (i = 0; i < requestData->numCANames; i++) {
        rv = SSM_SSMStringToString(&(requestData->caNames[i]), NULL, (SSMString*)curPtr);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);
    }

    *data = requestData;

    goto done;
loser:
    if (requestData) {
        PR_Free(requestData);
    }
done:
    if (request) {
        PR_Free(request);
    }
    return rv;
}

PRInt32 SSM_PackMatchUserCertReply(void **reply, SSMCertList * certList)
{
    unsigned char *curPtr;
    PRInt32 replyLength;
    int i;
    SSMCertListElement *head;

    /* Do some basic parameter checking */
    if (!reply) {
        goto loser;
    }

    /* Calculate the message length */
    replyLength = sizeof(PRInt32) + (certList->count)*sizeof(PRUint32);

    curPtr = *reply = PR_Malloc(replyLength);
    if (!curPtr) {
        goto loser;
    }

    /* Count */
    *((PRInt32*)curPtr) = PR_htonl(certList->count);
    curPtr += sizeof(PRInt32);

    /* Get the first element */
    head = SSM_CERT_LIST_ELEMENT_PTR(certList->certs.next);

    for (i = 0; i < certList->count; i++) {
        *((PRUint32*)curPtr) = PR_htonl(head->certResID);
        curPtr += sizeof(PRUint32);
        head = SSM_CERT_LIST_ELEMENT_PTR(head->links.next);
    }

    return replyLength;
loser:
    /* XXX Free memory here */
    return 0;
}

SSMPRInt32
SSM_PackErrorMessage(void ** errorReply, SSMPRInt32 result)
{
  SSMPRInt32 blobSize;
  
  if (!errorReply) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return 0;
  }  
  *errorReply = NULL;
  blobSize = sizeof(SSMPRInt32) + sizeof(SSMPRUint32);

  /* allocate space for the "blob" */
  *errorReply = (void *)SSMPORT_ZAlloc(blobSize);
  if (!*errorReply) { 
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    return 0;
  }
  *(SSMPRInt32 *)(*errorReply) = SSMPR_htonl(result);
  return blobSize;
}

SSMPRStatus
SSM_ParseKeyPairGenRequest(void *keyPairGenRequest, SSMPRInt32 requestLen,
                           SSMPRUint32 *keyGenCtxtID,
			   SSMPRUint32 *genMechanism,  
			   SSMPRUint32 *keySize, unsigned char **params,
			   SSMPRUint32 *paramLen)
{
  unsigned char *curPtr = (unsigned char*)keyPairGenRequest;
  SSMPRStatus    rv     = SSMPR_SUCCESS;

  if (!keyPairGenRequest) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return SSMPR_FAILURE;
  }

  /* Now fetch all of the stuff out */
  if (keyGenCtxtID)
    *keyGenCtxtID =SSMPR_ntohl(*((SSMPRInt32*)curPtr));
  curPtr += sizeof(SSMPRInt32);

  if (genMechanism)
    *genMechanism =SSMPR_ntohl(*((SSMPRInt32*)curPtr));
  curPtr += sizeof(SSMPRInt32);

  if (keySize)
    *keySize = SSMPR_ntohl(*((SSMPRInt32*)curPtr));
  curPtr += sizeof(SSMPRInt32);

  rv = SSM_SSMStringToString((char**)params, (int*)paramLen, 
			     (SSMString*)curPtr);
  return rv;
}

SSMPRInt32
SSM_PackKeyPairGenResponse(void ** keyPairGenResponse, SSMPRUint32 keyPairId)
{
  SSMPRInt32     blobSize;
  unsigned char *curPtr;

  blobSize = sizeof (SSMPRInt32);
  *keyPairGenResponse = curPtr = PORT_ZAlloc(blobSize);
  if (curPtr == NULL) {
      SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
      return 0;
  }
  *((SSMPRInt32*)curPtr) = SSMPR_htonl(keyPairId);
  return blobSize;
}

PRStatus
SSM_ParseFinishKeyGenRequest(void    *finishKeyGenRequest,
                             PRInt32  requestLen,
                             PRInt32 *keyGenContext)
{
  if (!finishKeyGenRequest || !keyGenContext) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return PR_FAILURE;
  }
  *keyGenContext = PR_ntohl(*((PRInt32*)finishKeyGenRequest));
  PR_ASSERT(requestLen == sizeof(PRInt32));
  return PR_SUCCESS;
}

SSMPRStatus 
SSM_ParseCreateCRMFReqRequest(void        *crmfReqRequest,
			      SSMPRInt32   requestLen,
			      SSMPRUint32 *keyPairId)
{
  if (!crmfReqRequest || !keyPairId) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return SSMPR_FAILURE;
  }
  *keyPairId = SSMPR_ntohl(*((SSMPRInt32*)crmfReqRequest));
  return SSMPR_SUCCESS;
}


SSMPRInt32
SSM_PackCreateCRMFReqReply(void        **crmfReqReply,
			   SSMPRUint32   crmfReqId)
{
  SSMPRInt32 blobSize;
  unsigned char *curPtr;

  blobSize = sizeof(SSMPRInt32);
  *crmfReqReply = curPtr = (unsigned char *) PORT_ZAlloc(blobSize);
  *((SSMPRInt32*)curPtr) = SSMPR_htonl(crmfReqId);
  return blobSize;
}

SSMPRStatus
SSM_ParseEncodeCRMFReqRequest(void         *encodeReq,
			      SSMPRInt32    requestLen,
			      SSMPRUint32 **crmfReqIds,
			      SSMPRInt32   *numRequests)
{
  unsigned char *curPtr = (unsigned char*)encodeReq;
  SSMPRInt32 i;
  SSMPRUint32 *reqIdArr;
  if (!encodeReq || !crmfReqIds || !numRequests) {
    SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
    return SSMPR_FAILURE;
  }
  *numRequests = SSMPR_ntohl(*((SSMPRInt32*)encodeReq));
  curPtr += sizeof(SSMPRInt32);
  *crmfReqIds = reqIdArr = SSMPORT_ZNewArray(SSMPRUint32, *numRequests);
  if (reqIdArr == NULL) {
    return SSMPR_FAILURE;
  }
  for (i=0; i<*numRequests;i++) {
    reqIdArr[i] = SSMPR_ntohl(*((SSMPRUint32*)curPtr));
    curPtr += sizeof(SSMPRUint32);
  } 
  return SSMPR_SUCCESS;
}

SSMPRInt32
SSM_PackEncodeCRMFReqReply(void        **encodeReply,
			   char         *crmfDER,
			   SSMPRUint32   derLen)
{
  char       *reply;
  SSMPRInt32  blobSize;

  blobSize = SSMSTRING_PADDED_LENGTH(derLen)+sizeof(SSMPRUint32);
  *encodeReply = reply = (char *) PORT_ZAlloc(blobSize);
  *((SSMPRUint32*)reply) = SSMPR_ntohl(derLen);
  reply += sizeof (SSMPRUint32);
  memcpy(reply, crmfDER, derLen);
  reply += derLen;
  memset(reply, 0 , blobSize - (reply - (*(char**)encodeReply)));
  return blobSize;
}

SSMPRStatus
SSM_ParseCMMFCertResponse(void        *encodedRes,
			  SSMPRInt32   encodeLen,
			  char       **nickname,
			  char       **base64Der,
			  PRBool      *doBackup)
{
  SSMPRStatus  rv;
  char        *curPtr;

  if (encodedRes == NULL || nickname == NULL || 
      base64Der  == NULL || doBackup == NULL) {
      return SSMPR_FAILURE;
  }
  curPtr = encodedRes;
  PR_ASSERT(*nickname == NULL && *base64Der == NULL);
  *nickname = *base64Der = NULL;
  rv = SSM_SSMStringToString(nickname, NULL, (SSMString*)curPtr);
  if (rv != SSMPR_SUCCESS) {
      goto loser;
  }
  curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);
  rv = SSM_SSMStringToString(base64Der, NULL, (SSMString*)curPtr);
  if (rv != SSMPR_SUCCESS) {
      goto loser;
  }
  curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);
  *doBackup = (SSMPR_ntohl(*(SSMPRUint32*)curPtr) == 0) ? PR_FALSE : PR_TRUE;
  return SSMPR_SUCCESS;
 loser:
  if (nickname && *nickname) {
      PR_Free(*nickname);
  }
  if (base64Der && *base64Der) {
      PR_Free(*base64Der);
  }
  return SSMPR_FAILURE;
}

PRStatus SSM_ParsePOPChallengeRequest(void     *challenge,
				      PRInt32   len,
				      char    **responseString)
{
  if (challenge == NULL || responseString == NULL) {
    return PR_FAILURE;
  }
  *responseString = NULL;
  return  SSM_SSMStringToString(responseString, NULL, (SSMString*)challenge);
    
}

PRInt32 SSM_PackPOPChallengeResponse(void   **response,
				     char    *responseString,
				     PRInt32  responseStringLen)
{
  PRInt32  blobSize;

  blobSize = SSMSTRING_PADDED_LENGTH(responseStringLen)+sizeof(PRInt32);
  *response = SSMPORT_ZNewArray(char, blobSize);
  if (SSM_StringToSSMString((SSMString**)response, responseStringLen,
			    responseString) != PR_SUCCESS) {
    return 0;
  }
  return blobSize;
}

PRInt32 SSM_PackPasswdRequest(void ** passwdRequest, PRInt32 tokenID, 
			      char * prompt, PRInt32 promptLen)
{
  void * curptr, * tmpStr = NULL;
  PRInt32 blobSize;
  PRStatus rv = PR_SUCCESS;

  if (!passwdRequest || !prompt || tokenID == 0 || promptLen == 0)
    {
      SSMPORT_SetError(SSMPR_INVALID_ARGUMENT_ERROR);
      goto loser;
    }
  *passwdRequest = NULL; /* in case we fail */

  blobSize = sizeof(PRInt32)*2 + SSMSTRING_PADDED_LENGTH(promptLen);
  curptr = *passwdRequest = PORT_ZAlloc(blobSize);
  if (!*passwdRequest) {
    SSMPORT_SetError(SSMPR_OUT_OF_MEMORY_ERROR);
    goto loser;
  }
    
  *(PRInt32 *)curptr = PR_htonl(tokenID);
  curptr = (PRInt32 *)curptr + 1;
  
  rv = SSM_StringToSSMString((SSMString **)&tmpStr, promptLen, prompt);
  if (rv != PR_SUCCESS) 
    goto loser;
  memcpy(curptr, tmpStr, SSM_SIZEOF_STRING(*(SSMString *)tmpStr));
  goto done;

loser:
  if (passwdRequest && *passwdRequest) 
    PR_Free(passwdRequest);

done:
  if (tmpStr) 
    PR_Free(tmpStr);
  return blobSize;
}

PRStatus SSM_ParsePasswordReply(void * passwdReply, PRInt32 * result, 
                                PRInt32 * tokenID,
				char ** passwd, PRInt32 * passwdLen)
{
  PRStatus rv = PR_SUCCESS;
  void * curptr = passwdReply; 
  
  if (!passwdReply) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
 
  if (result) 
    *result = PR_ntohl(*(PRInt32 *)curptr);
  curptr = (PRInt32 *)curptr + 1;
   
  if (tokenID) 
    *tokenID = PR_ntohl(*(PRInt32 *)curptr);
  curptr = (PRInt32 *)curptr + 1;
  
  if (passwd) {
    *passwd = NULL;
    rv = SSM_SSMStringToString(passwd, passwdLen, (SSMString *)curptr);
    if (rv != PR_SUCCESS && *passwd) {
      PR_Free(*passwd);
      *passwd = NULL;
      passwdLen = 0;
      goto loser;
    }
  }
  goto done;
loser:
  if (rv == PR_SUCCESS) 
    rv = PR_FAILURE;
  if (passwd && *passwd) {
    PR_Free(*passwd);
    *passwd = NULL;
  }
  if (tokenID) 
    *tokenID = 0;
  if (passwdLen)
    *passwdLen = 0;

done: 
  return rv;
}

void SSM_DestroyAttrValue(SSMAttributeValue *value, PRBool freeit)
{
  if (value->type == SSM_STRING_ATTRIBUTE)
    PR_Free(value->u.string);
  value->type = 0;
  if (freeit)
    PR_Free(value);
}

/* Sign Text functions */

PRStatus SSM_ParseSignTextRequest(void* signTextRequest, PRInt32 len, PRUint32* resID, signTextRequestData ** data)
{
    unsigned char *curPtr = (unsigned char*)signTextRequest;
    signTextRequestData *signTextData = NULL;
    PRStatus rv;
    int i;

    /* Do some basic parameter checking */
    if (!signTextRequest || !resID || !data) {
        goto loser;
    }

    /* Allocate the reply structure */
    signTextData = PR_NEWZAP(signTextRequestData);
    if (NULL == signTextData) {
        goto loser;
    }

    /* Resource ID */
    *resID = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    /* String to sign */
    rv = SSM_SSMStringToString(&(signTextData->stringToSign), NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);

    /* Host name */
    rv = SSM_SSMStringToString(&(signTextData->hostName), NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);

    /* CA option */
    rv = SSM_SSMStringToString(&(signTextData->caOption), NULL, (SSMString*)curPtr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);

    /* Number of CAs */
    signTextData->numCAs = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);

    signTextData->caNames = PR_Malloc(sizeof(char*)*(signTextData->numCAs));

    for (i = 0; i < signTextData->numCAs; i++) {
        rv = SSM_SSMStringToString(&(signTextData->caNames[i]), NULL, (SSMString*)curPtr);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);
    }

    *data = signTextData;

    /* Free the incoming data buffer */
    PR_Free(signTextRequest);
    
    return PR_SUCCESS;
loser:
    if (signTextData) {
        if (signTextData->stringToSign) {
            PR_Free(signTextData->stringToSign);
        }
        if (signTextData->hostName) {
            PR_Free(signTextData->hostName);
        }
        if (signTextData->numCAs) {
            /* XXX Free the CA Names */
        }
        PR_Free(signTextData);
    }
    return PR_FAILURE;
}

PRStatus SSM_ParseGetLocalizedTextRequest(void               *data,
                                          SSMLocalizedString *whichString)
{
    return SSM_ParseSingleNumRequest(data, (SSMPRUint32*)whichString);
} 

PRInt32 SSM_PackGetLocalizedTextResponse(void               **data,
					 SSMLocalizedString   whichString,
					 char                *retString)
{
    char *tmpPtr;
    PRInt32 replyLen;
    int retStrLen;

    retStrLen = strlen(retString);
    replyLen = SSMSTRING_PADDED_LENGTH(retStrLen)+(2*sizeof(PRInt32));
    
    tmpPtr = SSMPORT_ZNewArray(char, replyLen);
    if (tmpPtr == NULL) {
      *data = NULL;
      return 0;
    }
    *data = tmpPtr;
    *(PRUint32*)tmpPtr = PR_htonl(whichString);
    tmpPtr += sizeof(PRUint32);
    *(PRUint32*)tmpPtr = PR_htonl(retStrLen);
    tmpPtr += sizeof(PRUint32);
    memcpy(tmpPtr, retString, retStrLen);
    return replyLen;
}

PRStatus SSM_ParseAddNewSecurityModuleRequest(void          *data, 
					      char         **moduleName,
					      char         **libraryPath, 
					      unsigned long *pubMechFlags,
					      unsigned long *pubCipherFlags)
{
    char *tmpPtr;
    PRStatus rv;

    if (data == NULL) {
       return PR_FAILURE;
    }
    if (moduleName != NULL) {
        *moduleName = NULL;
    }
    if (libraryPath != NULL) {
        *libraryPath = NULL;
    }
    tmpPtr = data;
    if (moduleName) {
      rv = SSM_SSMStringToString(moduleName, NULL, (SSMString*)tmpPtr);
      if (rv != PR_SUCCESS) {
	  goto loser;
      }
    }
    tmpPtr += SSM_SIZEOF_STRING(*(SSMString*)tmpPtr);
    if (libraryPath) {
      rv =SSM_SSMStringToString(libraryPath, NULL, (SSMString*)tmpPtr);
      if (rv != PR_SUCCESS) {
	  goto loser;
      }
    }
    tmpPtr += SSM_SIZEOF_STRING(*(SSMString*)tmpPtr);
    *pubMechFlags = PR_ntohl(*(unsigned long*)tmpPtr);
    tmpPtr += sizeof(unsigned long);
    *pubCipherFlags = PR_ntohl(*(unsigned long*)tmpPtr);
    return PR_SUCCESS;
 loser:
    if (moduleName && *moduleName) {
        PR_Free(moduleName);
    }
    if (libraryPath && *libraryPath) {
        PR_Free(libraryPath);
    }
    return PR_FAILURE;
}

PRInt32 SSM_PackAddNewModuleResponse(void **data, PRInt32 rv)
{
    return SSM_PackSingleNumReply(data, rv);
}

PRStatus SSM_ParseDeleteSecurityModuleRequest(void *data, char **moduleName)
{
    PRStatus rv = PR_FAILURE;

    if (data == NULL) {
        goto done;
    }
    if (moduleName) {
        *moduleName = NULL;
	rv = SSM_SSMStringToString(moduleName, NULL, (SSMString*)data);
    }
 done:
    return rv;
}

PRInt32 SSM_PackDeleteModuleResponse(void **data, PRInt32 moduleType)
{
    return SSM_PackSingleNumReply(data, moduleType);
}

/* messages for importing certs *the traditional way* */
PRInt32 SSM_PackDecodeCertReply(void ** data, PRInt32 certID)
{
 return SSM_PackSingleNumReply(data, certID);
}

PRStatus SSM_ParseDecodeCertRequest(void * data, PRInt32 * len,
                                        char ** buffer)
{
  if (!data) 
    goto loser;

  if (buffer) {
    return SSM_SSMStringToString(buffer, len, (SSMString*)data); 
  }
loser:
  return PR_FAILURE;
}

PRStatus SSM_ParseDecodeAndCreateTempCertRequest(void * data,
                        char ** certbuf, PRUint32 * certlen, int * certClass)
{
  PRStatus rv = PR_FAILURE;

  if (!data) 
    goto loser;

  *certClass = PR_ntohl(*(int *)data);
  rv = SSM_SSMStringToString(certbuf, certlen, (SSMString *)((char *)data + sizeof(int)));
  if (rv != PR_SUCCESS)
    goto loser;

loser:  
  if (data)
    PR_Free(data);
  return rv;
} 
  

PRStatus SSM_ParseGetKeyChoiceListRequest(void * data, PRUint32 dataLen,
                                          char ** type, PRUint32 *typeLen,
                                          char ** pqgString, PRUint32 * pqgLen)
{
  PRStatus  rv = PR_SUCCESS;
  void * curptr = data;
  char * field = NULL, * value;
  PRUint32 len;

  /* in fact, this is perfectly OK, loser is just an exit tag */
  if (!data)
    goto loser;

  if (type) *type = NULL;
  if (pqgString) *pqgString = NULL;
  
  while  ((long)curptr < (long)data + dataLen)  {
    rv = SSM_SSMStringToString(&field, &len, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
    curptr = (char *)curptr + SSMSTRING_PADDED_LENGTH(len) + sizeof(PRInt32);
    rv = SSM_SSMStringToString(&value, &len, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
    curptr = (char *)curptr + SSMSTRING_PADDED_LENGTH(len) + sizeof(PRInt32);

    if (type &&  PORT_Strcmp(field, "type")==0) {
      *type = value;
      if (typeLen) *typeLen = len;
    } else if (pqgString && PORT_Strcmp(field, "pqg")==0) {
      *pqgString = value;
      if (pqgLen) *pqgLen = len;
    }
    if (field) PR_Free(field);
  } 

loser:
  if (data)
    PR_Free(data);
  return rv;
} 
  

PRInt32 SSM_PackGetKeyChoiceListReply(void ** data, char ** list)
{
  PRInt32 len = 0, i=0, oldlen;
  char * tmpString = NULL, * tmp = NULL;
  PRStatus rv = PR_FAILURE;

  *data = NULL;

  while (list[i] != 0) {
    oldlen = len;
    len = len + SSMSTRING_PADDED_LENGTH(strlen(list[i])) + sizeof(PRInt32);
    if (tmp)  
      tmp = PR_REALLOC(tmp, len);
    else 
      tmp = PORT_ZAlloc(len);
    if (!tmp)
      goto loser;
    rv = SSM_StringToSSMString((SSMString **)&tmpString, 0, list[i]); 
    if (rv != PR_SUCCESS)  
      goto loser;
    memcpy(tmp+oldlen, tmpString, len-oldlen);
    i++;
  }

  *data = PORT_ZAlloc(len + sizeof(i));
  oldlen = PR_htonl(i);
  *(PRInt32 *)*data = oldlen; /* number of strings */
  memcpy((char *)*data + sizeof(i), tmp, len);

  return len+sizeof(i);

loser:
   /* scream out loud, should not be breaking here 
   SSM_DEBUG("Error in packGetKeyChoiceListReply!\n");
   */
   *data = NULL;
   return 0;
}


PRStatus SSM_ParseGenKeyOldStyleRequest(void * data, PRUint32 datalen,
                                        char ** choiceString,
                                        char ** challenge,
                                        char ** typeString,
                                        char ** pqgString)
{
  char * curptr = (char *)data;
  PRStatus rv; 
   
  if (!data)
    goto loser;
  
  if (choiceString) {
    rv = SSM_SSMStringToString(choiceString, NULL, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);

  if (challenge) {
    rv = SSM_SSMStringToString(challenge, NULL, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr); 

  if (typeString) {
    rv = SSM_SSMStringToString(typeString, NULL, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);

  if (pqgString) {
    rv = SSM_SSMStringToString(pqgString, NULL, (SSMString *)curptr);
    if (rv != PR_SUCCESS)
      goto loser;
  }
  curptr = (char *)curptr + SSM_SIZEOF_STRING(*(SSMString *)curptr);
  goto done;

loser:
  if (pqgString && *pqgString) {
    PR_Free(*pqgString);
    *pqgString = NULL;
  }
  if (typeString && *typeString) {
    PR_Free(*typeString);
    *typeString = NULL;
  }
  if (challenge && *challenge) {
    PR_Free(*challenge);
    *challenge = NULL;
  }
  if (choiceString && *choiceString) {
    PR_Free(*choiceString);
    *choiceString = NULL;
  }
  if (rv == PR_SUCCESS)
    rv = PR_FAILURE;
done:
  if (data) 
    PR_Free(data);
  return rv;
}

 
PRInt32 SSM_PackGenKeyOldStyleReply(void ** data, char * keydata)
{
  PRStatus rv;
  char * curptr;

  if (!data)
    return 0;

  rv = SSM_StringToSSMString((SSMString **)&curptr, 0, keydata);
  if (rv != PR_SUCCESS)
    return 0;
   
  *data = curptr;
  return SSM_SIZEOF_STRING(*(SSMString *)curptr);
}


PRInt32 SSM_PackFilePathRequest(void **data, PRInt32 resID, char *prompt,
                                PRBool shouldFileExist, char *fileSuffix)
{
    PRInt32 reqLen;
    char *request;
    PRInt32 promptLen, fileSufLen;

    promptLen = strlen(prompt);
    fileSufLen = strlen(fileSuffix);
    reqLen = SSMSTRING_PADDED_LENGTH(promptLen) + 
             SSMSTRING_PADDED_LENGTH(fileSufLen) + (4*sizeof(PRInt32));
    request = SSMPORT_ZNewArray(char, reqLen);
    if (request == NULL) {
        *data = NULL;
        return 0;
    }
    *data = request;
    *(PRInt32*)request = PR_htonl(resID);
    request += sizeof(PRInt32);
    *(PRInt32*)request = PR_htonl(shouldFileExist);
    request += sizeof(PRInt32);
    *(PRInt32*)request = PR_htonl(promptLen);
    request += sizeof(PRInt32);
    memcpy(request, prompt, promptLen);
    request += SSMSTRING_PADDED_LENGTH(promptLen);
    *(PRInt32*)request = PR_htonl(fileSufLen);
    request += sizeof(PRInt32);
    memcpy(request, fileSuffix, fileSufLen);
    return reqLen;
}

PRStatus SSM_ParseFilePathReply(void *message, char **filePath,
                                PRInt32 *rid)
{
    unsigned char *curPtr = message;

    *rid = PR_ntohl(*(PRInt32*)curPtr);
    curPtr += sizeof(PRInt32);
    if (filePath != NULL) {
        *filePath = NULL;
        SSM_SSMStringToString(filePath, NULL, (SSMString*)curPtr);
    }
    curPtr += SSM_SIZEOF_STRING(*(SSMString*)curPtr);
    return PR_SUCCESS;
}

PRInt32 SSM_PackPromptRequestEvent(void **data, PRInt32 resID, char *prompt)
{
    PRInt32 promptLen;
    PRInt32 reqLen;
    char *request;

    promptLen = strlen(prompt);
    reqLen = SSMSTRING_PADDED_LENGTH(promptLen) + (2*sizeof(PRInt32));
    *data = request = SSMPORT_ZNewArray(char, reqLen);
    if (request == NULL) {
        *data = NULL;
        return 0;
    }
    *(PRInt32*)request = PR_htonl(resID);
    request += sizeof(PRInt32); 
    *(PRInt32*)request = PR_htonl(promptLen);
    request += sizeof(PRInt32);
    memcpy(request, prompt, promptLen);
    return reqLen;
}

PRStatus 
SSM_ParsePasswordPromptReply(void *data, PRInt32 *resID, char **reply)
{
    /* The Message formats are the same, so I can do this. */
    return SSM_ParseFilePathReply(data, reply, resID);
}
