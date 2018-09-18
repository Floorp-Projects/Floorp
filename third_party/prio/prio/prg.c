/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <blapit.h>
#include <mprio.h>
#include <pk11pub.h>
#include <string.h>

#include "prg.h"
#include "rand.h"
#include "share.h"
#include "util.h"

struct prg
{
  PK11SlotInfo* slot;
  PK11SymKey* key;
  PK11Context* ctx;
};

SECStatus
PrioPRGSeed_randomize(PrioPRGSeed* key)
{
  return rand_bytes((unsigned char*)key, PRG_SEED_LENGTH);
}

PRG
PRG_new(const PrioPRGSeed key_in)
{
  PRG prg = malloc(sizeof(struct prg));
  if (!prg)
    return NULL;
  prg->slot = NULL;
  prg->key = NULL;
  prg->ctx = NULL;

  SECStatus rv = SECSuccess;
  const CK_MECHANISM_TYPE cipher = CKM_AES_CTR;

  P_CHECKA(prg->slot = PK11_GetInternalSlot());

  // Create a mutable copy of the key.
  PrioPRGSeed key_mut;
  memcpy(key_mut, key_in, PRG_SEED_LENGTH);

  SECItem keyItem = { siBuffer, key_mut, PRG_SEED_LENGTH };

  // The IV can be all zeros since we only encrypt once with
  // each AES key.
  CK_AES_CTR_PARAMS param = { 128, {} };
  SECItem paramItem = { siBuffer, (void*)&param, sizeof(CK_AES_CTR_PARAMS) };

  P_CHECKA(prg->key = PK11_ImportSymKey(prg->slot, cipher, PK11_OriginUnwrap,
                                        CKA_ENCRYPT, &keyItem, NULL));

  P_CHECKA(prg->ctx = PK11_CreateContextBySymKey(cipher, CKA_ENCRYPT, prg->key,
                                                 &paramItem));

cleanup:
  if (rv != SECSuccess) {
    PRG_clear(prg);
    prg = NULL;
  }

  return prg;
}

void
PRG_clear(PRG prg)
{
  if (!prg)
    return;

  if (prg->key)
    PK11_FreeSymKey(prg->key);
  if (prg->slot)
    PK11_FreeSlot(prg->slot);
  if (prg->ctx)
    PK11_DestroyContext(prg->ctx, PR_TRUE);

  free(prg);
}

static SECStatus
PRG_get_bytes_internal(void* prg_vp, unsigned char* bytes, size_t len)
{
  PRG prg = (PRG)prg_vp;

  unsigned char in[len];
  memset(in, 0, len);

  int outlen;
  SECStatus rv = PK11_CipherOp(prg->ctx, bytes, &outlen, len, in, len);
  return (rv != SECSuccess || (size_t)outlen != len) ? SECFailure : SECSuccess;
}

SECStatus
PRG_get_bytes(PRG prg, unsigned char* bytes, size_t len)
{
  return PRG_get_bytes_internal((void*)prg, bytes, len);
}

SECStatus
PRG_get_int(PRG prg, mp_int* out, const mp_int* max)
{
  return rand_int_rng(out, max, &PRG_get_bytes_internal, (void*)prg);
}

SECStatus
PRG_get_array(PRG prg, MPArray dst, const mp_int* mod)
{
  SECStatus rv;
  for (int i = 0; i < dst->len; i++) {
    P_CHECK(PRG_get_int(prg, &dst->data[i], mod));
  }

  return SECSuccess;
}

SECStatus
PRG_share_int(PRG prgB, mp_int* shareA, const mp_int* src, const_PrioConfig cfg)
{
  SECStatus rv = SECSuccess;
  mp_int tmp;
  MP_DIGITS(&tmp) = NULL;

  MP_CHECKC(mp_init(&tmp));
  P_CHECKC(PRG_get_int(prgB, &tmp, &cfg->modulus));
  MP_CHECKC(mp_submod(src, &tmp, &cfg->modulus, shareA));

cleanup:
  mp_clear(&tmp);
  return rv;
}

SECStatus
PRG_share_array(PRG prgB, MPArray arrA, const_MPArray src, const_PrioConfig cfg)
{
  SECStatus rv = SECSuccess;
  if (arrA->len != src->len)
    return SECFailure;

  const int len = src->len;

  for (int i = 0; i < len; i++) {
    P_CHECK(PRG_share_int(prgB, &arrA->data[i], &src->data[i], cfg));
  }

  return rv;
}
