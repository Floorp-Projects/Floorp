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
#if defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#ifdef XP_MAC
#else /* windows */
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include <errno.h>
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include "rsrcids.h"



CMTStatus CMT_OpenSSLConnection(PCMT_CONTROL control, CMTSocket sock,
                                SSMSSLConnectionRequestType flags, 
                                CMUint32 port, char * hostIP, 
                                char * hostName, CMBool forceHandshake, void* clientContext)
{
	CMTItem message;
    SSLDataConnectionRequest request;
    DataConnectionReply reply;
    CMUint32 sent;
	
	/* Do some parameter checking */
	if (!control || !hostIP || !hostName) {
		goto loser;
	}

    request.flags = flags;
    request.port = port;
    request.hostIP = hostIP;
    request.hostName = hostName;
    request.forceHandshake = forceHandshake;
    request.clientContext = CMT_CopyPtrToItem(clientContext);

    /* Encode message */
    if (CMT_EncodeMessage(SSLDataConnectionRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | SSM_SSL_CONNECTION;
    
	/* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | SSM_SSL_CONNECTION)) {
        goto loser;
    }

    /* Decode message */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

	/* Success */
	if (reply.result == 0) {
	  if (control->sockFuncs.connect(sock, reply.port, NULL) != CMTSuccess) {
            goto loser;
	  }
	  sent = CMT_WriteThisMany(control, sock, control->nonce.data, 
                               control->nonce.len);
	  if (sent != control->nonce.len) {
            goto loser;
	  }
	  
	  /* Save connection info */
	  if (CMT_AddDataConnection(control, sock, reply.connID)
	      != CMTSuccess) {
	    goto loser;
	  }

	  return CMTSuccess;
	} 
	
loser:
	return CMTFailure;
}

CMTStatus CMT_GetSSLDataErrorCode(PCMT_CONTROL control, CMTSocket sock,
                                  CMInt32* errorCode)
{
    CMUint32 connID;

    if (!control || !errorCode) {
        goto loser;
    }

    /* get the data connection */
    if (CMT_GetDataConnectionID(control, sock, &connID) != CMTSuccess) {
        goto loser;
    }

    /* get the PR error */
    if (CMT_GetNumericAttribute(control, connID, SSM_FID_SSLDATA_ERROR_VALUE,
                                errorCode) != CMTSuccess) {
        goto loser;
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_ReleaseSSLSocketStatus(PCMT_CONTROL control, CMTSocket sock)
{
    CMUint32 connectionID;

    if (!control || !sock) {
        goto loser;
    }
    if (CMT_GetDataConnectionID(control, sock, &connectionID) != CMTSuccess) {
        goto loser;
    }
    if (CMT_SetNumericAttribute(control, connectionID, 
                                SSM_FID_SSLDATA_DISCARD_SOCKET_STATUS,
                                0) != CMTSuccess) {
        goto loser;
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus CMT_GetSSLSocketStatus(PCMT_CONTROL control, CMTSocket sock, 
                                 CMTItem* pickledStatus, CMInt32* level)
{
    CMUint32 connectionID;
    SingleNumMessage request;
    CMTItem message;
    PickleSecurityStatusReply reply;

    if (!control || !pickledStatus || !level) {
        goto loser;
    }

	/* get the data connection */
    if (CMT_GetDataConnectionID(control, sock, &connectionID) != CMTSuccess) {
		goto loser;
	}

    /* set up the request */
    request.value = connectionID;

    /* encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | 
        SSM_CONSERVE_RESOURCE | SSM_PICKLE_SECURITY_STATUS;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION |
                         SSM_CONSERVE_RESOURCE | SSM_PICKLE_SECURITY_STATUS)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(PickleSecurityStatusReplyTemplate, &reply, &message)
        != CMTSuccess) {
        goto loser;
    }

    /* success */
    if (reply.result == 0) {
        *pickledStatus = reply.blob;
        *level = reply.securityLevel;
        return CMTSuccess;
    }

loser:
    return CMTFailure;
}


CMTStatus CMT_OpenTLSConnection(PCMT_CONTROL control, CMTSocket sock,
                                CMUint32 port, char* hostIP, char* hostName)
{
    TLSDataConnectionRequest request;
    CMTItem message;
    DataConnectionReply reply;
    CMUint32 sent;

    /* do some parameter checking */
    if (!control || !hostIP || !hostName) {
        goto loser;
    }

    request.port = port;
    request.hostIP = hostIP;
    request.hostName = hostName;

    /* encode the message */
    if (CMT_EncodeMessage(TLSDataConnectionRequestTemplate, &message, &request)
        != CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | 
        SSM_TLS_CONNECTION;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | 
                         SSM_TLS_CONNECTION)) {
        goto loser;
    }

    /* decode the message */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* success */
    if (reply.result == 0) {
        if (control->sockFuncs.connect(sock, reply.port, NULL) != CMTSuccess) {
            goto loser;
        }
        sent = CMT_WriteThisMany(control, sock, control->nonce.data, 
                                 control->nonce.len);
        if (sent != control->nonce.len) {
            goto loser;
        }

        /* save connection info */
        if (CMT_AddDataConnection(control, sock, reply.connID) != CMTSuccess) {
            goto loser;
        }

        return CMTSuccess;
    }
loser:
    return CMTFailure;
}


CMTStatus CMT_TLSStepUp(PCMT_CONTROL control, CMTSocket sock, 
                        void* clientContext)
{
    TLSStepUpRequest request;
    SingleNumMessage reply;
    CMTItem message;
    CMUint32 connectionID;

    /* check arguments */
    if (!control || !sock) {
        goto loser;
    }

    /* get the data connection ID */
    if (CMT_GetDataConnectionID(control, sock, &connectionID) != CMTSuccess) {
        goto loser;
    }

    /* set up the request */
    request.connID = connectionID;
    request.clientContext = CMT_CopyPtrToItem(clientContext);

    /* encode the request */
    if (CMT_EncodeMessage(TLSStepUpRequestTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_TLS_STEPUP;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION |
                         SSM_TLS_STEPUP)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    return (CMTStatus) reply.value;
loser:
    return CMTFailure;
}

CMTStatus CMT_OpenSSLProxyConnection(PCMT_CONTROL control, CMTSocket sock,
                                     CMUint32 port, char* hostIP, 
                                     char* hostName)
{
    TLSDataConnectionRequest request;
    CMTItem message;
    DataConnectionReply reply;
    CMUint32 sent;

    /* do some parameter checking */
    if (!control || !hostIP || !hostName) {
        goto loser;
    }

    request.port = port;
    request.hostIP = hostIP;
    request.hostName = hostName;

    /* encode the message */
    if (CMT_EncodeMessage(TLSDataConnectionRequestTemplate, &message, &request)
        != CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | 
        SSM_PROXY_CONNECTION;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | 
                         SSM_PROXY_CONNECTION)) {
        goto loser;
    }

    /* decode the message */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* success */
    if (reply.result == 0) {
        if (control->sockFuncs.connect(sock, reply.port, NULL) != CMTSuccess) {
            goto loser;
        }
        sent = CMT_WriteThisMany(control, sock, control->nonce.data, 
                                 control->nonce.len);
        if (sent != control->nonce.len) {
            goto loser;
        }

        /* save connection info */
        if (CMT_AddDataConnection(control, sock, reply.connID) != CMTSuccess) {
            goto loser;
        }

        return CMTSuccess;
    }
loser:
    return CMTFailure;
}


CMTStatus CMT_ProxyStepUp(PCMT_CONTROL control, CMTSocket sock, 
                        void* clientContext, char* remoteUrl)
{
    ProxyStepUpRequest request;
    SingleNumMessage reply;
    CMTItem message;
    CMUint32 connectionID;

    /* check arguments */
    if (!control || !sock || !remoteUrl) {
        goto loser;
    }

    /* get the data connection ID */
    if (CMT_GetDataConnectionID(control, sock, &connectionID) != CMTSuccess) {
        goto loser;
    }

    /* set up the request */
    request.connID = connectionID;
    request.clientContext = CMT_CopyPtrToItem(clientContext);
    request.url = remoteUrl;

    /* encode the request */
    if (CMT_EncodeMessage(ProxyStepUpRequestTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | 
        SSM_PROXY_STEPUP;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION |
                         SSM_PROXY_STEPUP)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    return (CMTStatus) reply.value;
loser:
    return CMTFailure;
}
