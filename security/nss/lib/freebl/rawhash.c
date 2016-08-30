/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "nspr.h"
#include "hasht.h"
#include "blapi.h" /* below the line */
#include "secerr.h"

static void *
null_hash_new_context(void)
{
    return NULL;
}

static void *
null_hash_clone_context(void *v)
{
    PORT_Assert(v == NULL);
    return NULL;
}

static void
null_hash_begin(void *v)
{
}

static void
null_hash_update(void *v, const unsigned char *input, unsigned int length)
{
}

static void
null_hash_end(void *v, unsigned char *output, unsigned int *outLen,
              unsigned int maxOut)
{
    *outLen = 0;
}

static void
null_hash_destroy_context(void *v, PRBool b)
{
    PORT_Assert(v == NULL);
}

const SECHashObject SECRawHashObjects[] = {
    { 0,
      (void *(*)(void))null_hash_new_context,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))null_hash_destroy_context,
      (void (*)(void *))null_hash_begin,
      (void (*)(void *, const unsigned char *, unsigned int))null_hash_update,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))null_hash_end,
      0,
      HASH_AlgNULL,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))null_hash_end },
    {
        MD2_LENGTH,
        (void *(*)(void))MD2_NewContext,
        (void *(*)(void *))null_hash_clone_context,
        (void (*)(void *, PRBool))MD2_DestroyContext,
        (void (*)(void *))MD2_Begin,
        (void (*)(void *, const unsigned char *, unsigned int))MD2_Update,
        (void (*)(void *, unsigned char *, unsigned int *, unsigned int))MD2_End,
        MD2_BLOCK_LENGTH,
        HASH_AlgMD2,
        NULL /* end_raw */
    },
    { MD5_LENGTH,
      (void *(*)(void))MD5_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))MD5_DestroyContext,
      (void (*)(void *))MD5_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))MD5_Update,
      (void (*)(void *, unsigned char *, unsigned int *, unsigned int))MD5_End,
      MD5_BLOCK_LENGTH,
      HASH_AlgMD5,
      (void (*)(void *, unsigned char *, unsigned int *, unsigned int))MD5_EndRaw },
    { SHA1_LENGTH,
      (void *(*)(void))SHA1_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))SHA1_DestroyContext,
      (void (*)(void *))SHA1_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))SHA1_Update,
      (void (*)(void *, unsigned char *, unsigned int *, unsigned int))SHA1_End,
      SHA1_BLOCK_LENGTH,
      HASH_AlgSHA1,
      (void (*)(void *, unsigned char *, unsigned int *, unsigned int))
          SHA1_EndRaw },
    { SHA256_LENGTH,
      (void *(*)(void))SHA256_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))SHA256_DestroyContext,
      (void (*)(void *))SHA256_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))SHA256_Update,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA256_End,
      SHA256_BLOCK_LENGTH,
      HASH_AlgSHA256,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA256_EndRaw },
    { SHA384_LENGTH,
      (void *(*)(void))SHA384_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))SHA384_DestroyContext,
      (void (*)(void *))SHA384_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))SHA384_Update,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA384_End,
      SHA384_BLOCK_LENGTH,
      HASH_AlgSHA384,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA384_EndRaw },
    { SHA512_LENGTH,
      (void *(*)(void))SHA512_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))SHA512_DestroyContext,
      (void (*)(void *))SHA512_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))SHA512_Update,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA512_End,
      SHA512_BLOCK_LENGTH,
      HASH_AlgSHA512,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA512_EndRaw },
    { SHA224_LENGTH,
      (void *(*)(void))SHA224_NewContext,
      (void *(*)(void *))null_hash_clone_context,
      (void (*)(void *, PRBool))SHA224_DestroyContext,
      (void (*)(void *))SHA224_Begin,
      (void (*)(void *, const unsigned char *, unsigned int))SHA224_Update,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA224_End,
      SHA224_BLOCK_LENGTH,
      HASH_AlgSHA224,
      (void (*)(void *, unsigned char *, unsigned int *,
                unsigned int))SHA224_EndRaw },
};

const SECHashObject *
HASH_GetRawHashObject(HASH_HashType hashType)
{
    if (hashType <= HASH_AlgNULL || hashType >= HASH_AlgTOTAL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return &SECRawHashObjects[hashType];
}
