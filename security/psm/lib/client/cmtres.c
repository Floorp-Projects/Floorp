/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#else
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include <errno.h>
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include <string.h>

CMTStatus CMT_GetNumericAttribute(PCMT_CONTROL control, CMUint32 resourceID, CMUint32 fieldID, CMInt32 *value)
{
    CMTItem message;
    GetAttribRequest request;
    GetAttribReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resourceID;
    request.fieldID = fieldID;

    /* Encode the request */
    if (CMT_EncodeMessage(GetAttribRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_NUMERIC_ATTRIBUTE;

    /* Send the mesage and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_NUMERIC_ATTRIBUTE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(GetAttribReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *value = reply.value.u.numeric;

    /* Success */
    if (reply.result == 0) {
        return CMTSuccess;
    } 

loser:
    return CMTFailure;
}

CMTStatus CMT_SetNumericAttribute(PCMT_CONTROL control, CMUint32 resourceID,
				  CMUint32 fieldID, CMInt32 value)
{
    CMTItem message;
    SetAttribRequest request;

    if (!control) {
        goto loser;
    }

    /* Set the request */
    request.resID = resourceID;
    request.fieldID = fieldID;
    request.value.type = SSM_NUMERIC_ATTRIBUTE;
    request.value.u.numeric = value;

    /* Encode the message */
    if (CMT_EncodeMessage(SetAttribRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | 
                   SSM_SET_ATTRIBUTE | SSM_NUMERIC_ATTRIBUTE;

    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | 
                              SSM_SET_ATTRIBUTE    | SSM_NUMERIC_ATTRIBUTE)) {
        goto loser;
    }

    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus
CMT_PadStringValue(CMTItem *dest, CMTItem src)
{
    dest->data = NewArray(unsigned char, src.len+1);
    if (dest->data == NULL) {
        return CMTFailure;
    }
    memcpy(dest->data, src.data, src.len);
    dest->data[src.len] = '\0';
    dest->len = src.len;
    free(src.data);
    return CMTSuccess;
}

CMTStatus CMT_GetStringAttribute(PCMT_CONTROL control, CMUint32 resourceID, CMUint32 fieldID, CMTItem *value)
{
    CMTItem message;
    GetAttribRequest request;
    GetAttribReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resourceID;
    request.fieldID = fieldID;

    /* Encode the request */
    if (CMT_EncodeMessage(GetAttribRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_STRING_ATTRIBUTE;
    
    /* Send the mesage and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_STRING_ATTRIBUTE)) {
        goto loser;
    }

    /* Decode the response */
    if (CMT_DecodeMessage(GetAttribReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        return CMT_PadStringValue(value, reply.value.u.string);
    } 
loser:
    return CMTFailure;
}

CMTStatus
CMT_SetStringAttribute(PCMT_CONTROL control, CMUint32 resourceID,
		       CMUint32 fieldID, CMTItem *value)
{
    CMTItem message;
    SetAttribRequest request;
    
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resourceID;
    request.fieldID = fieldID;
    request.value.type = SSM_STRING_ATTRIBUTE;
    request.value.u.string = *value;

    /* Encode the request */
    if (CMT_EncodeMessage(SetAttribRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | 
                   SSM_SET_ATTRIBUTE | SSM_STRING_ATTRIBUTE;

    /* Send the message */
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }

    /* Validate the message request type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | 
                         SSM_SET_ATTRIBUTE    | SSM_STRING_ATTRIBUTE)) {
        goto loser;
    }
    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_DuplicateResource(PCMT_CONTROL control, CMUint32 resourceID,
				CMUint32 *newResID)
{
    CMTItem message;
    SingleNumMessage request;
    DupResourceReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.value = resourceID;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_DUPLICATE_RESOURCE;

    /* Send the mesage */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_DUPLICATE_RESOURCE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(DupResourceReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        *newResID = reply.resID;
        return CMTSuccess;
    } 

loser:
    *newResID = 0;
    return CMTFailure;
}

CMTStatus CMT_DestroyResource(PCMT_CONTROL control, CMUint32 resourceID, CMUint32 resourceType)
{
    CMTItem message;
    DestroyResourceRequest request;
    SingleNumMessage reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resourceID;
    request.resType = resourceType;

    /* Encode the message */
    if (CMT_EncodeMessage(DestroyResourceRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_DESTROY_RESOURCE;

    /* Send the message */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_DESTROY_RESOURCE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.value == 0) {
        return CMTSuccess;
    }
loser:
    return CMTFailure;
}

CMTStatus CMT_PickleResource(PCMT_CONTROL control, CMUint32 resourceID, CMTItem * pickledResource)
{
    CMTItem message;
    SingleNumMessage request;
    PickleResourceReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.value = resourceID;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_CONSERVE_RESOURCE | SSM_PICKLE_RESOURCE;

    /* Send the mesage and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_CONSERVE_RESOURCE | SSM_PICKLE_RESOURCE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(PickleResourceReplyTemplate, &reply,&message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        *pickledResource = reply.blob;
        return CMTSuccess;
    } 

loser:
    return CMTFailure;
}

CMTStatus CMT_UnpickleResource(PCMT_CONTROL control, CMUint32 resourceType, CMTItem pickledResource, CMUint32 * resourceID)
{
    CMTItem message;
    UnpickleResourceRequest request;
    UnpickleResourceReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set up the request */
    request.resourceType = resourceType;
    request.resourceData = pickledResource;

    /* Encode the request */
    if (CMT_EncodeMessage(UnpickleResourceRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_CONSERVE_RESOURCE | SSM_UNPICKLE_RESOURCE;
    
    /* Send the mesage and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_CONSERVE_RESOURCE | SSM_UNPICKLE_RESOURCE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(UnpickleResourceReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        *resourceID = reply.resID;
        return CMTSuccess;
    } 

loser:
    *resourceID = 0;
    return CMTFailure;
}

CMTStatus CMT_GetRIDAttribute(PCMT_CONTROL control, CMUint32 resourceID, CMUint32 fieldID, CMUint32 *value)
{
    CMTItem message;
    GetAttribRequest request;
    GetAttribReply reply;

    /* Do some parameter checking */
    if (!control) {
        goto loser;
    }

    /* Set the request */
    request.resID = resourceID;
    request.fieldID = fieldID;

    /* Encode the message */
    if (CMT_EncodeMessage(GetAttribRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_RID_ATTRIBUTE;

    /* Send the mesage and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message response type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_GET_ATTRIBUTE | SSM_RID_ATTRIBUTE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(GetAttribReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        *value = reply.value.u.rid;
        return CMTSuccess;
    } 
loser:
    return CMTFailure;
}
 
