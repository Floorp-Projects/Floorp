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
 *   thayes@netscape.com
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
 *
 * PKCS #11 Wrapper functions which handles authenticating to the card's
 * choosing the best cards, etc.
 */

#include "seccomon.h"
#include "secoid.h"
#include "secasn1.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "pk11sdr.h"

/*
 * Data structure and template for encoding the result of an SDR operation
 *  This is temporary.  It should include the algorithm ID of the encryption mechanism
 */
struct SDRResult
{
  SECItem keyid;
  SECAlgorithmID alg;
  SECItem data;
};
typedef struct SDRResult SDRResult;

static SEC_ASN1Template template[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof (SDRResult) },
  { SEC_ASN1_OCTET_STRING, offsetof(SDRResult, keyid) },
  { SEC_ASN1_INLINE, offsetof(SDRResult, alg), SECOID_AlgorithmIDTemplate },
  { SEC_ASN1_OCTET_STRING, offsetof(SDRResult, data) },
  { 0 }
};

/*
 * Temporary fixed DES3 key for testing
 */
static unsigned char keyValue[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
};

static SECItem keyItem = {
  0,
  keyValue,
  sizeof keyValue
};

static unsigned char keyID[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static SECItem keyIDItem = {
  0,
  keyID,
  sizeof keyID
};

/* local utility function for padding an incoming data block
 * to the mechanism block size.
 */
static SECStatus
padBlock(SECItem *data, int blockSize, SECItem *result)
{
  SECStatus rv = SECSuccess;
  int padLength;
  unsigned int i;

  result->data = 0;
  result->len = 0;

  /* This algorithm always adds to the block (to indicate the number
   * of pad bytes).  So allocate a block large enough.
   */
  padLength = blockSize - (data->len % blockSize);
  result->len = data->len + padLength;
  result->data = (unsigned char *)PORT_Alloc(result->len);

  /* Copy the data */
  PORT_Memcpy(result->data, data->data, data->len);

  /* Add the pad values */
  for(i = data->len; i < result->len; i++)
    result->data[i] = (unsigned char)padLength;

  return rv;
}

static SECStatus
unpadBlock(SECItem *data, int blockSize, SECItem *result)
{
  SECStatus rv = SECSuccess;
  int padLength;

  result->data = 0;
  result->len = 0;

  /* Remove the padding from the end if the input data */
  if (data->len == 0 || data->len % blockSize  != 0) { rv = SECFailure; goto loser; }

  padLength = data->data[data->len-1];
  if (padLength > blockSize) { rv = SECFailure; goto loser; }

  result->len = data->len - padLength;
  result->data = (unsigned char *)PORT_Alloc(result->len);
  if (!result->data) { rv = SECFailure; goto loser; }

  PORT_Memcpy(result->data, data->data, result->len);

loser:
  return rv;
}

/*
 * PK11SDR_Encrypt
 *  Encrypt a block of data using the symmetric key identified.  The result
 *  is an ASN.1 (DER) encoded block of keyid, params and data.
 */
SECStatus
PK11SDR_Encrypt(SECItem *keyid, SECItem *data, SECItem *result, void *cx)
{
  SECStatus rv = SECSuccess;
  PK11SlotInfo *slot = 0;
  PK11SymKey *key = 0;
  SECItem *params = 0;
  PK11Context *ctx = 0;
  CK_MECHANISM_TYPE type;
  SDRResult sdrResult;
  SECItem paddedData;
  PLArenaPool *arena = 0;

  /* Initialize */
  paddedData.len = 0;
  paddedData.data = 0;

  arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
  if (!arena) { rv = SECFailure; goto loser; }

  /* 1. Locate the requested keyid, or the default key (which has a keyid)
   * 2. Create an encryption context
   * 3. Encrypt
   * 4. Encode the results (using ASN.1)
   */

  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = SECFailure; goto loser; }

  /* Use triple-DES */
  type = CKM_DES3_CBC;

  /* Keyid value 16*0 (16 bytes of 0) is special, it uses a fixed DES3 key */
  key = PK11_ImportSymKey(slot, type, 0, CKA_ENCRYPT, &keyItem, cx);
  if (!key) { rv = SECFailure; goto loser; }

  params = PK11_GenerateNewParam(type, key);
  if (!params) { rv = SECFailure; goto loser; }

  ctx = PK11_CreateContextBySymKey(type, CKA_ENCRYPT, key, params);
  if (!ctx) { rv = SECFailure; goto loser; }

  rv = padBlock(data, PK11_GetBlockSize(type, 0), &paddedData);
  if (rv != SECSuccess) goto loser;

  sdrResult.data.len = paddedData.len;
  sdrResult.data.data = (unsigned char *)PORT_ArenaAlloc(arena, sdrResult.data.len);

  rv = PK11_CipherOp(ctx, sdrResult.data.data, &sdrResult.data.len, sdrResult.data.len,
                     paddedData.data, paddedData.len);
  if (rv != SECSuccess) goto loser;

  PK11_Finalize(ctx);

  sdrResult.keyid = keyIDItem;

  rv = PK11_ParamToAlgid(SEC_OID_DES_EDE3_CBC, params, arena, &sdrResult.alg);
  if (rv != SECSuccess) goto loser;

  if (!SEC_ASN1EncodeItem(0, result, &sdrResult, template)) { rv = SECFailure; goto loser; }

loser:
  SECITEM_ZfreeItem(&paddedData, PR_FALSE);
  PORT_FreeArena(arena, PR_TRUE);
  PK11_DestroyContext(ctx, PR_TRUE);
  SECITEM_ZfreeItem(params, PR_TRUE);
  PK11_FreeSymKey(key);
  PK11_FreeSlot(slot);

  return rv;
}

/*
 * PK11SDR_Decrypt
 *  Decrypt a block of data produced by PK11SDR_Encrypt.  The key used is identified
 *  by the keyid field within the input.
 */
SECStatus
PK11SDR_Decrypt(SECItem *data, SECItem *result, void *cx)
{
  SECStatus rv = SECSuccess;
  PK11SlotInfo *slot = 0;
  PK11SymKey *key = 0;
  PK11Context *ctx = 0;
  CK_MECHANISM_TYPE type;
  SDRResult sdrResult;
  SECItem *params = 0;
  SECItem paddedResult;
  PLArenaPool *arena = 0;

  paddedResult.len = 0;
  paddedResult.data = 0;

  arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
  if (!arena) { rv = SECFailure; goto loser; }

  /* Decode the incoming data */
  memset(&sdrResult, 0, sizeof sdrResult);
  rv = SEC_ASN1DecodeItem(arena, &sdrResult, template, data);
  if (rv != SECSuccess) goto loser;  /* Invalid format */

  /* Find the slot and key for the given keyid */
  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = SECFailure; goto loser; }

  /* Use triple-DES (Should look up the algorithm) */
  type = CKM_DES3_CBC;

  /* Keyid value 16*0 (16 bytes of 0) is special, it uses a fixed DES3 key */
  key = PK11_ImportSymKey(slot, type, 0, CKA_DECRYPT, &keyItem, cx);
  if (!key) { rv = SECFailure; goto loser; }

  /* Get the parameter values from the data */
  params = PK11_ParamFromAlgid(&sdrResult.alg);
  if (!params) { rv = SECFailure; goto loser; }

  ctx = PK11_CreateContextBySymKey(type, CKA_ENCRYPT, key, params);
  if (!ctx) { rv = SECFailure; goto loser; }

  paddedResult.len = sdrResult.data.len;
  paddedResult.data = PORT_ArenaAlloc(arena, result->len);

  rv = PK11_CipherOp(ctx, paddedResult.data, &paddedResult.len, paddedResult.len,
                     sdrResult.data.data, sdrResult.data.len);
  if (rv != SECSuccess) goto loser;

  PK11_Finalize(ctx);

  /* Remove the padding */
  rv = unpadBlock(&paddedResult, PK11_GetBlockSize(type, 0), result);
  if (rv) goto loser;

loser:
  SECITEM_ZfreeItem(&paddedResult, PR_FALSE);
  PORT_FreeArena(arena, PR_TRUE);
  PK11_DestroyContext(ctx, PR_TRUE);
  PK11_FreeSymKey(key);
  SECITEM_ZfreeItem(params, PR_TRUE);
  PK11_FreeSlot(slot);

  return rv;
}
