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
#include "rsrcids.h"
#include <string.h>

// Encrypt request
typedef struct EncryptRequestMessage
{
  CMTItem keyid;  /* May have length 0 for default */
  CMTItem data;
} EncryptRequestMessage;

static CMTMessageTemplate EncryptRequestTemplate[] =
{
  { CMT_DT_ITEM, offsetof(EncryptRequestMessage, keyid) },
  { CMT_DT_ITEM, offsetof(EncryptRequestMessage, data) },
  { CMT_DT_END }
};

/* Encrypt reply message - SingleItemMessage */
/* Decrypt request message - SingleItemMessage */
/* Decrypt reply message - SingleItemMessage */

/* Constants for testing */
static const char *kSuccess = "Success:";
static const char *kFailure = "Failure:";

static CMTItem
CMT_CopyDataToItem(const unsigned char *data, CMUint32 len)
{
  CMTItem item;

  item.data = calloc(len, 1);
  item.len = len;
  memcpy(item.data, data, len);

  return item;
}

static CMTStatus
tmp_SendMessage(PCMT_CONTROL control, CMTItem *message)
{
  CMTStatus rv = CMTSuccess;
  EncryptRequestMessage request;
  SingleItemMessage reply;
  int prefixLen = strlen(kSuccess);

  if (message->type != SSM_SDR_ENCRYPT_REQUEST) {
	rv = CMTFailure;
	goto loser;
  }

  request.keyid.data = 0;
  request.data.data = 0;

  rv = CMT_DecodeMessage(EncryptRequestTemplate, &request, message);
  if (rv != CMTSuccess) goto loser;

  /* Concatinate the prefix with the data */
  reply.item.len = request.data.len + request.keyid.len;
  reply.item.data = calloc(reply.item.len, 1);
  if (!reply.item.data) {
	rv = CMTFailure;
	goto loser;
  }

  message->type = SSM_SDR_ENCRYPT_REPLY;
  if (request.keyid.len) memcpy(reply.item.data, request.keyid.data, request.keyid.len);
  memcpy(&reply.item.data[request.keyid.len], request.data.data, request.data.len);
  
  /* Free old message ?? */
  rv = CMT_EncodeMessage(SingleItemMessageTemplate, message, &reply);
  if (rv != CMTSuccess) goto loser;

loser:
  if (request.keyid.data) free(request.keyid.data);
  if (request.data.data) free(request.data.data);

  return rv;
}
/* End test code */

CMTStatus
CMT_SDREncrypt(PCMT_CONTROL control, const unsigned char *key, CMUint32 keyLen,
               const unsigned char *data, CMUint32 dataLen,
               unsigned char **result, CMUint32 *resultLen)
{
  CMTItem message;
  EncryptRequestMessage request;
  SingleItemMessage reply;

  /* Fill in the request */
  request.keyid = CMT_CopyDataToItem(key, keyLen);
  request.data = CMT_CopyDataToItem(data, dataLen);

  /* Encode */
  if (CMT_EncodeMessage(EncryptRequestTemplate, &message, &request) != CMTSuccess) {
    goto loser;
  }

  message.type = SSM_SDR_ENCRYPT_REQUEST;

  /* Send */
  /* if (CMT_SendMessage(control, &message) != CMTSuccess) goto loser; */
  if (tmp_SendMessage(control, &message) != CMTSuccess) goto loser;

  if (message.type != SSM_SDR_ENCRYPT_REPLY) goto loser;

  if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) != CMTSuccess)
    goto loser;

  *result = reply.item.data;
  *resultLen = reply.item.len;

  reply.item.data = 0;

loser:
  if (message.data) free(message.data);
  if (request.keyid.data) free(request.keyid.data);
  if (request.data.data) free(request.data.data);
  if (reply.item.data) free(reply.item.data);

  return CMTSuccess; /* need return value */
}