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
#include "string.h"
#include "protocol.h"
#include "protocolshr.h"
#include "messages.h"

/* Forward ref */
static void encrypt(CMTItem *data);
static void decrypt(CMTItem *data);

const char *kPrefix = "Encrypted";

/* encryption request */
CMTStatus
CMT_DoEncryptionRequest(CMTItem *message)
{
  CMTStatus rv = CMTSuccess;
  EncryptRequestMessage request;
  EncryptReplyMessage reply;
  CMUint32 pLen = strlen(kPrefix);

  /* Initialize */
  request.keyid.data = 0;
  request.data.data = 0;
  reply.item.data = 0;

  /* Decode incoming message */
  rv = CMT_DecodeMessage(EncryptRequestTemplate, &request, message);
  if (rv != CMTSuccess) goto loser;  /* Protocol error */

  /* Free incoming message */
  free(message->data);
  message->data = NULL;

  /* "Encrypt" by prefixing the data */
  reply.item.len = request.data.len + pLen;
  reply.item.data = calloc(reply.item.len, 1);
  if (!reply.item.data) {
	rv = CMTFailure;
	goto loser;
  }

  if (pLen) memcpy(reply.item.data, kPrefix, pLen);
  encrypt(&request.data);
  memcpy(&reply.item.data[pLen], request.data.data, request.data.len);
  
  /* Generate response */
  message->type = SSM_SDR_ENCRYPT_REPLY;
  rv = CMT_EncodeMessage(EncryptReplyTemplate, message, &reply);
  if (rv != CMTSuccess) goto loser;  /* Unknown error */

loser:
  if (request.keyid.data) free(request.keyid.data);
  if (request.data.data) free(request.data.data);
  if (request.ctx.data) free(request.ctx.data);
  if (reply.item.data) free(reply.item.data);

  return rv;
}

/* decryption request */
CMTStatus
CMT_DoDecryptionRequest(CMTItem *message)
{
  CMTStatus rv = CMTSuccess;
  DecryptRequestMessage request;
  DecryptReplyMessage reply;
  CMUint32 pLen = strlen(kPrefix);

  /* Initialize */
  request.data.data = 0;
  request.ctx.data = 0;
  reply.item.data = 0;

  /* Decode the message */
  rv = CMT_DecodeMessage(DecryptRequestTemplate, &request, message);
  if (rv != CMTSuccess) goto loser;

  /* Free incoming message */
  free(message->data);
  message->data = NULL;

  /* "Decrypt" the message by removing the key */
  if (pLen && memcmp(request.data.data, kPrefix, pLen) != 0) {
    rv = CMTFailure;  /* Invalid format */
    goto loser;
  }

  reply.item.len = request.data.len - pLen;
  reply.item.data = calloc(reply.item.len, 1);
  if (!reply.item.data) { rv = CMTFailure;  goto loser; }

  memcpy(reply.item.data, &request.data.data[pLen], reply.item.len);
  decrypt(&reply.item);

  /* Create reply message */
  message->type = SSM_SDR_DECRYPT_REPLY;
  rv = CMT_EncodeMessage(DecryptReplyTemplate, message, &reply);
  if (rv != CMTSuccess) goto loser;

loser:
  if (request.data.data) free(request.data.data);
  if (request.ctx.data) free(request.ctx.data);
  if (reply.item.data) free(reply.item.data);

  return rv;
}

/* "encrypt" */
static unsigned char mask[64] = {
 0x73, 0x46, 0x1a, 0x05, 0x24, 0x65, 0x43, 0xb4, 0x24, 0xee, 0x79, 0xc1, 0xcc,
 0x49, 0xc7, 0x27, 0x11, 0x91, 0x2e, 0x8f, 0xaa, 0xf7, 0x62, 0x75, 0x41, 0x7e,
 0xb2, 0x42, 0xde, 0x1b, 0x42, 0x7b, 0x1f, 0x33, 0x49, 0xca, 0xd1, 0x6a, 0x85,
 0x05, 0x6c, 0xf9, 0x0e, 0x3e, 0x72, 0x02, 0xf2, 0xd8, 0x9d, 0xa1, 0xb8, 0x6e,
 0x03, 0x18, 0x3e, 0x82, 0x86, 0x34, 0x1a, 0x61, 0xd9, 0x65, 0xb6, 0x7f
};

static void
encrypt(CMTItem *data)
{
  unsigned int i, j;

  j = 0;
  for(i = 0;i < data->len;i++)
  {
    data->data[i] ^= mask[j];

    if (++j >= 64) j = 0;
  }
}

static void
decrypt(CMTItem *data)
{
  encrypt(data);
}


