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
#include "macsocket.h"
#include "string.h"
#else
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include <errno.h>
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include "rsrcids.h"

CMTStatus CMT_HashCreate(PCMT_CONTROL control, CMUint32 algID, 
                         CMUint32 * connID)
{
    CMTItem message;
    SingleNumMessage request;
    DataConnectionReply reply;

    /* Check passed in parameters */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.value = algID;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_DATA_CONNECTION | SSM_HASH_STREAM;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the response */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_DATA_CONNECTION | SSM_HASH_STREAM)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(DataConnectionReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        CMTSocket sock;

        sock = control->sockFuncs.socket(0);
        if(sock == NULL) {
            goto loser;
        }
        
        if (control->sockFuncs.connect(sock, reply.port, NULL) != CMTSuccess) {
            goto loser;
        }
        /* Send the hello message */
		control->sockFuncs.send(sock, control->nonce.data, control->nonce.len);

        /* Save connection info */
        if (CMT_AddDataConnection(control, sock, reply.connID)
		    != CMTSuccess) {
            goto loser;
        }

        /* Set the connection ID */
        *connID = reply.connID;
        return CMTSuccess;
	} 
loser:
    *connID = 0;
	return CMTFailure;
}

CMTStatus CMT_HASH_Destroy(PCMT_CONTROL control, CMUint32 connectionID)
{
    if (!control) {
        goto loser;
    }

    /* Get the cotext implementation data */
    if (CMT_CloseDataConnection(control, connectionID) == CMTFailure) {
        goto loser;
    }

    return CMTSuccess;

loser:

    return CMTFailure;
}

CMTStatus CMT_HASH_Begin(PCMT_CONTROL control, CMUint32 connectionID)
{
    return CMTSuccess;
}

CMTStatus CMT_HASH_Update(PCMT_CONTROL control, CMUint32 connectionID, const unsigned char * buf, CMUint32 len)
{
    CMTSocket sock;
    CMUint32 sent;

    /* Do some parameter checking */
    if (!control || !buf) {
        goto loser;
    }

    /* Get the data socket */
    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    /* Write the data to the socket */
    sent = CMT_WriteThisMany(control, sock, (void*)buf, len);
    if (sent != len) {
        goto loser;
    }

    return CMTSuccess;

loser:

    return CMTFailure;
}

CMTStatus CMT_HASH_End(PCMT_CONTROL control, CMUint32 connectionID, 
                       unsigned char * result, CMUint32 * resultlen, 
                       CMUint32 maxLen)
{
    CMTItem hash = { 0, NULL, 0 };

    /* Do some parameter checking */
    if (!control || !result || !resultlen) {
        goto loser;
    }

    /* Close the connection */
    if (CMT_CloseDataConnection(control, connectionID) == CMTFailure) {
        goto loser;
    }

    /* Get the context info */
    if (CMT_GetStringAttribute(control, connectionID, SSM_FID_HASHCONN_RESULT,
                               &hash) == CMTFailure) {
        goto loser;
    }
    if (!hash.data) {
        goto loser;
    }

    *resultlen = hash.len;
    if (hash.len > maxLen) {
        memcpy(result, hash.data, maxLen);
    } else {
        memcpy(result, hash.data, hash.len);
    }   

    if (hash.data) {
        free(hash.data);
    }

    return CMTSuccess;

loser:
    if (hash.data) {
        free(hash.data);
    }

    return CMTFailure;
}
