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
#include <sys/time.h>
#else
#ifdef XP_MAC
#include "macsocket.h"
#else /* Windows */
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include <errno.h>
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include "rsrcids.h"

typedef struct _CMTP7Private {
    CMTPrivate priv;
    CMTP7ContentCallback cb;
    void *cb_arg;
} CMTP7Private;

CMTStatus CMT_PKCS7DecoderStart(PCMT_CONTROL control, void* clientContext, CMUint32 * connectionID, CMInt32 * result,
                                CMTP7ContentCallback cb, void *cb_arg)
{
    CMTItem message;
    CMTStatus rv;
    CMTP7Private *priv=NULL;
    SingleItemMessage request;
    DataConnectionReply reply;

    /* Check passed in parameters */
    if (!control) {
        goto loser;
    }

    request.item = CMT_CopyPtrToItem(clientContext);

    /* Encode message */
    if (CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | SSM_PKCS7DECODE_STREAM;

    /* Send the message. */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | SSM_PKCS7DECODE_STREAM)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        CMTSocket sock;

        priv = (CMTP7Private *)malloc(sizeof(CMTP7Private));
        if (priv == NULL)
            goto loser;
        priv->priv.dest = (CMTReclaimFunc) free;
        priv->cb = cb;
        priv->cb_arg = cb_arg;
        sock = control->sockFuncs.socket(0);
        if (sock == NULL) {
            goto loser;
        }
 
       if (control->sockFuncs.connect(sock, (short)reply.port, 
                                      NULL) != CMTSuccess) {
            goto loser;
        }
 
        if (control->sockFuncs.send(sock, control->nonce.data, 
                                    control->nonce.len) != control->nonce.len){
            goto loser;
        }

        /* Save connection info */
        if (CMT_AddDataConnection(control, sock, reply.connID)
            != CMTSuccess) {
            goto loser;
        }
        *connectionID = reply.connID;

        rv = CMT_SetPrivate(control, reply.connID, &priv->priv);
        if (rv != CMTSuccess)
            goto loser;

        return CMTSuccess;
    } 

loser:
    if (priv) {
        free(priv);
    }

	*result = reply.result;
    return CMTFailure;
}

CMTStatus CMT_PKCS7DecoderUpdate(PCMT_CONTROL control, CMUint32 connectionID, const char * buf, CMUint32 len)
{
    CMUint32 sent;
    CMTP7Private *priv;
    unsigned long nbytes;
    char read_buf[128];
    CMTSocket sock, ctrlsock, selSock, sockArr[2];

    /* Do some parameter checking */
    if (!control || !buf) {
        goto loser;
    }

    /* Get the data socket */
    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    priv = (CMTP7Private *)CMT_GetPrivate(control, connectionID);
    if (priv == NULL)
        goto loser;

    /* Write the data to the socket */
    sent = CMT_WriteThisMany(control, sock, (void*)buf, len);
    if (sent != len) {
        goto loser;
    }

	ctrlsock = control->sock;
	sockArr[0] = ctrlsock;
	sockArr[1] = sock;
    while ((selSock = control->sockFuncs.select(sockArr,2,1))) 
    {
		if (selSock == ctrlsock) {
			CMT_ProcessEvent(control);
		} else {
	        nbytes = control->sockFuncs.recv(sock, read_buf, sizeof(read_buf));
		    if (nbytes == -1) {
			    goto loser;
			}
			if (nbytes == 0) {
	            break;
		    }
			priv->cb(priv->cb_arg, read_buf, nbytes);
		}
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}
                                                                                
CMTStatus CMT_PKCS7DecoderFinish(PCMT_CONTROL control, CMUint32 connectionID, 
                                 CMUint32 * resourceID)
{
    CMTP7Private *priv;
    long nbytes;
    char buf[128];
    CMTSocket sock, ctrlsock, selSock, sockArr[2];
#ifndef XP_MAC
    int numTries = 0;
#endif

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    priv = (CMTP7Private *)CMT_GetPrivate(control, connectionID);
    if (priv == NULL)
        goto loser;

    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    ctrlsock = control->sock;
    /* drain socket before we close it */
    control->sockFuncs.shutdown(sock);
    sockArr[0] = sock;
    sockArr[1] = ctrlsock;
    /* Let's see if doing a poll first gets rid of a weird bug where we
     * lock up the client.
     * There are some cases where the server doesn't put up data fast 
     * enough, so we should loop on this poll instead of just trying it
     * once.
     */
#ifndef XP_MAC
 poll_sockets:
    if (control->sockFuncs.select(sockArr,2,1) != NULL)
#endif
	{
        while (1) {
            selSock = control->sockFuncs.select(sockArr,2,0);
            if (selSock == ctrlsock) {
                CMT_ProcessEvent(control);
            } else if (selSock == sock) {
                nbytes = control->sockFuncs.recv(sock, buf, sizeof(buf));
                if (nbytes < 0) {
                    goto loser;
                } else if (nbytes == 0) {
                    break;
                }
                if (priv->cb)
                    priv->cb(priv->cb_arg, buf, nbytes);
            }
        }
    }
#ifndef XP_MAC
    else {
#ifdef WIN32
        if (numTries < 20) {
            Sleep(100);
            numTries++;
            goto poll_sockets;
        }
#elif defined(XP_OS2)
        if (numTries < 20) {
            DosSleep(100);
            numTries++;
            goto poll_sockets;
        }
#elif defined(XP_UNIX)
	if (numTries < 25) {
	  numTries += sleep(1);
	  goto poll_sockets;
	}
#endif
    }
#endif
    
    if (CMT_CloseDataConnection(control, connectionID) == CMTFailure) {
        goto loser;
    }

    /* Get the PKCS7 content info */
    if (CMT_GetRIDAttribute(control, connectionID, SSM_FID_P7CONN_CONTENT_INFO,
                            resourceID) == CMTFailure) {
        goto loser;
    }

    return CMTSuccess;

loser:
    if (control) {
        CMT_CloseDataConnection(control, connectionID);
    }
    
    return CMTFailure;
}

CMTStatus CMT_PKCS7DestroyContentInfo(PCMT_CONTROL control, CMUint32 resourceID)
{
    if (!control) {
        goto loser;
    }

    /* Delete the resource */
    if (CMT_DestroyResource(control, resourceID, SSM_FID_P7CONN_CONTENT_INFO) == CMTFailure) {
        goto loser;
    }
    return CMTSuccess;

loser:
    return CMTFailure;
}

CMTStatus CMT_PKCS7VerifyDetachedSignature(PCMT_CONTROL control, CMUint32 resourceID, CMUint32 certUsage, CMUint32 hashAlgID, CMUint32 keepCerts, CMTItem* digest, CMInt32 * result)
{
    CMTItem message;
    VerifyDetachedSigRequest request;
    SingleNumMessage reply;

    /* Do some parameter checking */
    if (!control || !digest || !result) {
        goto loser;
    }

    /* Set the request */
    request.pkcs7ContentID = resourceID;
    request.certUsage = certUsage;
    request.hashAlgID = hashAlgID;
    request.keepCert = (CMBool) keepCerts;
    request.hash = *digest;

    /* Encode the request */
    if (CMT_EncodeMessage(VerifyDetachedSigRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_OBJECT_SIGNING | SSM_VERIFY_DETACHED_SIG;

    /* Send the message */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_OBJECT_SIGNING |SSM_VERIFY_DETACHED_SIG)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *result = reply.value;
    return CMTSuccess;
loser:
	*result = reply.value;
	return CMTFailure;
}

CMTStatus CMT_PKCS7VerifySignature(PCMT_CONTROL control, CMUint32 pubKeyAlgID,
                                   CMTItem *pubKeyParams, CMTItem *signerPubKey,
                                   CMTItem *computedHash, CMTItem *signature,
                                   CMInt32 *result)
{
	return CMTFailure;
}

CMTStatus CMT_CreateSigned(PCMT_CONTROL control, CMUint32 scertRID,
                           CMUint32 ecertRID, CMUint32 dig_alg,
                           CMTItem *digest, CMUint32 *ciRID, CMInt32 *errCode)
{
    CMTItem message;
    CreateSignedRequest request;
    CreateContentInfoReply reply;
    char checkMessageForError = 0;

    /* Do some parameter checking */
    if (!control || !scertRID || !digest || !ciRID) {
        goto loser;
    }

    /* Set the request */
    request.scertRID = scertRID;
    request.ecertRID = ecertRID;
    request.dig_alg = dig_alg;
    request.digest = *digest;

    /* Encode the request */
    if (CMT_EncodeMessage(CreateSignedRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_OBJECT_SIGNING | SSM_CREATE_SIGNED;

    /* Send the message */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }
    checkMessageForError = 1;
    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_OBJECT_SIGNING | SSM_CREATE_SIGNED)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(CreateContentInfoReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *ciRID = reply.ciRID;
    if (reply.result == 0) {
		return CMTSuccess;
	} 

loser:
    if (checkMessageForError && 
	CMT_DecodeMessage(SingleNumMessageTemplate, 
			  &reply, &message) == CMTSuccess) {
        *errCode = reply.errorCode;
    } else {
        *errCode = 0;
    }
	return CMTFailure;
}

CMTStatus CMT_CreateEncrypted(PCMT_CONTROL control, CMUint32 scertRID,
                              CMUint32 *rcertRIDs, CMUint32 *ciRID)
{
    CMTItem message;
    CMInt32 nrcerts;
    CreateEncryptedRequest request;
    CreateContentInfoReply reply;

    /* Do some parameter checking */
    if (!control || !scertRID || !rcertRIDs || !ciRID) {
        goto loser;
    }

    /* Calculate the number of certs */
    for (nrcerts =0; rcertRIDs[nrcerts] != 0; nrcerts++) {
        /* Nothing */
        ;
    }

    /* Set up the request */
    request.scertRID = scertRID;
    request.nrcerts = nrcerts;
    request.rcertRIDs = (long *) rcertRIDs;

    /* Encode the request */
    if (CMT_EncodeMessage(CreateEncryptedRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_OBJECT_SIGNING | SSM_CREATE_ENCRYPTED;

    /* Send the message */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message response type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_OBJECT_SIGNING | SSM_CREATE_ENCRYPTED)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(CreateContentInfoReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *ciRID = reply.ciRID;
    if (reply.result == 0) {
		return CMTSuccess;
	} 
loser:
	return CMTFailure;
}

CMTStatus CMT_PKCS7EncoderStart(PCMT_CONTROL control, CMUint32 ciRID,
                                CMUint32 *connectionID, CMTP7ContentCallback cb,
                                void *cb_arg)
{
    CMTItem message;
    CMTStatus rv;
    CMTP7Private *priv;
    PKCS7DataConnectionRequest request;
    DataConnectionReply reply;

    /* Check passed in parameters */
    if (!control || !ciRID) {
        goto loser;
    }

    /* Set up the request */
    request.resID = ciRID;
    request.clientContext.len = 0;
    request.clientContext.data = NULL;

    /* Encode the request */
    if (CMT_EncodeMessage(PKCS7DataConnectionRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | SSM_PKCS7ENCODE_STREAM;

    /* Send the message */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | SSM_PKCS7ENCODE_STREAM)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        CMTSocket sock;

        priv = (CMTP7Private *)malloc(sizeof(CMTP7Private));
        if (priv == NULL)
            goto loser;
        priv->priv.dest = (CMTReclaimFunc) free;
        priv->cb = cb;
        priv->cb_arg = cb_arg;

        sock = control->sockFuncs.socket(0);
        if (sock == NULL) {
            goto loser;
        }
        if (control->sockFuncs.connect(sock, (short)reply.port, 
                                       NULL) != CMTSuccess) {
            goto loser;
        }
        if (control->sockFuncs.send(sock, control->nonce.data, 
                                   control->nonce.len) != control->nonce.len) {
            goto loser;
        }
        /* Save connection info */
        if (CMT_AddDataConnection(control, sock, reply.connID)
            != CMTSuccess) {
            goto loser;
        }
        *connectionID = reply.connID;

        rv = CMT_SetPrivate(control, reply.connID, &priv->priv);
        if (rv != CMTSuccess)
            goto loser;
        return CMTSuccess;
    } 
loser:
    return CMTFailure;
}

CMTStatus CMT_PKCS7EncoderUpdate(PCMT_CONTROL control, CMUint32 connectionID,
                                 const char *buf, CMUint32 len)
{
    CMUint32 sent;
    CMTP7Private *priv;
    unsigned long nbytes;
    char read_buf[128];
    CMTSocket sock, ctrlsock, sockArr[2], selSock;

    /* Do some parameter checking */
    if (!control || !connectionID || !buf) {
        goto loser;
    }

    /* Get the data socket */
    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    priv = (CMTP7Private *)CMT_GetPrivate(control, connectionID);
    if (priv == NULL)
        goto loser;

    /* Write the data to the socket */
    sent = CMT_WriteThisMany(control, sock, (void*)buf, len);
    if (sent != len) {
        goto loser;
    }
	ctrlsock = control->sock;
	sockArr[0] = ctrlsock;
	sockArr[1] = sock;
    while ((selSock = control->sockFuncs.select(sockArr, 2, 1)) != NULL) 
    {
		if (selSock == ctrlsock) {
			CMT_ProcessEvent(control);
		} else {
	        nbytes = control->sockFuncs.recv(sock, read_buf, sizeof(read_buf));
		    if (nbytes == -1) {
			    goto loser;
			} else if (nbytes == 0) {
	            break;
		    } else {
			    priv->cb(priv->cb_arg, read_buf, nbytes);
			}
		}
    }
    return CMTSuccess;

loser:

    return CMTFailure;
}

CMTStatus CMT_PKCS7EncoderFinish(PCMT_CONTROL control, CMUint32 connectionID)
{
    CMTP7Private *priv;
    CMInt32 nbytes;
    char buf[128];
    CMTSocket sock, ctrlsock, sockArr[2], selSock;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    priv = (CMTP7Private *)CMT_GetPrivate(control, connectionID);
    if (priv == NULL)
        goto loser;

    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    ctrlsock = control->sock;
    sockArr[0] = ctrlsock;
    sockArr[1] = sock;
    control->sockFuncs.shutdown(sock);
    while (1) {
        selSock = control->sockFuncs.select(sockArr, 2, 0);
        if (selSock == ctrlsock) {
            CMT_ProcessEvent(control);
        } else if (selSock == sock) {
            nbytes = control->sockFuncs.recv(sock, buf, sizeof(buf));
            if (nbytes < 0) {
                goto loser;
            } else if (nbytes == 0) {
                break;
            } else {
                priv->cb(priv->cb_arg, buf, nbytes);
            }
        }
    }
    if (CMT_CloseDataConnection(control, connectionID) == CMTFailure) {
        goto loser;
    }

    return CMTSuccess;

loser:
    if (control) {
        CMT_CloseDataConnection(control, connectionID);
    }
    
    return CMTFailure;
}
