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
/*
  cmtsdr.c -- Support for the Secret Decoder Ring, which provides
      encryption and decryption using stored keys.

  Created by thayes 18 April 2000
 */
#include "stddef.h"
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include "protocolshr.h"
#include "rsrcids.h"
#include <string.h>

#undef PROCESS_LOCALLY

/* Encryption result - contains the key id and the resulting data */
/* An empty key id indicates that NO encryption was performed */
typedef struct EncryptionResult
{
  CMTItem keyid;
  CMTItem data;
} EncryptionResult;

/* Constants for testing */
static const char *kPrefix = "Encrypted:";

static CMTItem
CMT_CopyDataToItem(const unsigned char *data, CMUint32 len)
{
  CMTItem item;

  item.data = (unsigned char*) calloc(len, 1);
  item.len = len;
  memcpy(item.data, data, len);

  return item;
}


static CMTStatus
tmp_SendMessage(PCMT_CONTROL control, CMTItem *message)
{
#ifndef PROCESS_LOCALLY
  return CMT_SendMessage(control, message);
#else
  if (message->type == SSM_SDR_ENCRYPT_REQUEST) 
    return CMT_DoEncryptionRequest(message);
  else if (message->type == SSM_SDR_DECRYPT_REQUEST)
    return CMT_DoDecryptionRequest(message);

  return CMTFailure;
#endif
}
/* End test code */

CMTStatus
CMT_SDREncrypt(PCMT_CONTROL control, void *ctx,
               const unsigned char *key, CMUint32 keyLen,
               const unsigned char *data, CMUint32 dataLen,
               unsigned char **result, CMUint32 *resultLen)
{
  CMTStatus rv = CMTSuccess;
  CMTItem message;
  EncryptRequestMessage request;
  SingleItemMessage reply;

  /* Fill in the request */
  request.keyid = CMT_CopyDataToItem(key, keyLen);
  request.data = CMT_CopyDataToItem(data, dataLen);
  request.ctx = CMT_CopyPtrToItem(ctx);

  reply.item.data = 0;
  reply.item.len = 0;
  message.data = 0;
  message.len = 0;

  /* Encode */
  rv = CMT_EncodeMessage(EncryptRequestTemplate, &message, &request);
  if (rv != CMTSuccess) {
    goto loser;
  }

  message.type = SSM_SDR_ENCRYPT_REQUEST;

  /* Send */
  /* if (CMT_SendMessage(control, &message) != CMTSuccess) goto loser; */
  rv = tmp_SendMessage(control, &message);
  if (rv != CMTSuccess) goto loser;

  if (message.type != SSM_SDR_ENCRYPT_REPLY) { rv = CMTFailure; goto loser; }

  rv = CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message);
  if (rv != CMTSuccess)
    goto loser;

  *result = reply.item.data;
  *resultLen = reply.item.len;

  reply.item.data = 0;

loser:
  if (message.data) free(message.data);
  if (request.keyid.data) free(request.keyid.data);
  if (request.data.data) free(request.data.data);
  if (request.ctx.data) free(request.ctx.data);
  if (reply.item.data) free(reply.item.data);

  return rv; /* need return value */
}

CMTStatus
CMT_SDRDecrypt(PCMT_CONTROL control, void *ctx,
               const unsigned char *data, CMUint32 dataLen,
               unsigned char **result, CMUint32 *resultLen)
{
  CMTStatus rv;
  CMTItem message;
  DecryptRequestMessage request;
  SingleItemMessage reply;

  /* Fill in the request */
  request.data = CMT_CopyDataToItem(data, dataLen);
  request.ctx = CMT_CopyPtrToItem(ctx);

  reply.item.data = 0;
  reply.item.len = 0;
  message.data = 0;
  message.len = 0;

  /* Encode */
  rv = CMT_EncodeMessage(DecryptRequestTemplate, &message, &request);
  if (rv != CMTSuccess) {
    goto loser;
  }

  message.type = SSM_SDR_DECRYPT_REQUEST;

  /* Send */
  /* if (CMT_SendMessage(control, &message) != CMTSuccess) goto loser; */
  rv = tmp_SendMessage(control, &message);
  if (rv != CMTSuccess) goto loser;

  if (message.type != SSM_SDR_DECRYPT_REPLY) { rv = CMTFailure; goto loser; }

  rv = CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message);
  if (rv != CMTSuccess)
    goto loser;

  *result = reply.item.data;
  *resultLen = reply.item.len;

  reply.item.data = 0;

loser:
  if (message.data) free(message.data);
  if (request.data.data) free(request.data.data);
  if (request.ctx.data) free(request.ctx.data);
  if (reply.item.data) free(reply.item.data);

  return rv; /* need return value */
}

CMTStatus
CMT_SDRChangePassword(PCMT_CONTROL control, void *ctx)
{
  CMTStatus rv = CMTSuccess;
  CMTItem message;
  SingleItemMessage request;
  SingleNumMessage reply;

  /* Fill in the request */
  request.item = CMT_CopyPtrToItem(ctx);

  message.data = 0;
  message.len = 0;

  /* Encode */
  rv = CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request);
  if (rv != CMTSuccess) {
    goto loser;
  }

  message.type = (SSM_REQUEST_MESSAGE|SSM_MISC_ACTION|SSM_MISC_UI|SSM_UI_CHANGE_PASSWORD);

  /* Send */
  rv = CMT_SendMessage(control, &message);
  if (rv != CMTSuccess) goto loser;

  if (message.type != 
     (SSM_REPLY_OK_MESSAGE|SSM_MISC_ACTION|SSM_MISC_UI|SSM_UI_CHANGE_PASSWORD)) { 
    rv = CMTFailure;
    goto loser; 
  }

  rv = CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message);
  if (rv != CMTSuccess)
    goto loser;

loser:
  if (request.item.data) free(request.item.data);
  if (message.data) free(message.data);

  return rv; /* need return value */
}
