/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "nspr.h"
#include "sechash.h"
#include "blapi.h"	/* below the line */
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
    (void * (*)(void)) null_hash_new_context,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) null_hash_destroy_context,
    (void (*)(void *)) null_hash_begin,
    (void (*)(void *, const unsigned char *, unsigned int)) null_hash_update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) null_hash_end,
    0,
    HASH_AlgNULL
  },
  { MD2_LENGTH,
    (void * (*)(void)) MD2_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) MD2_DestroyContext,
    (void (*)(void *)) MD2_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) MD2_Update,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) MD2_End,
    MD2_BLOCK_LENGTH,
    HASH_AlgMD2
  },
  { MD5_LENGTH,
    (void * (*)(void)) MD5_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) MD5_DestroyContext,
    (void (*)(void *)) MD5_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) MD5_Update,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) MD5_End,
    MD5_BLOCK_LENGTH,
    HASH_AlgMD5
  },
  { SHA1_LENGTH,
    (void * (*)(void)) SHA1_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) SHA1_DestroyContext,
    (void (*)(void *)) SHA1_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) SHA1_Update,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) SHA1_End,
    SHA1_BLOCK_LENGTH,
    HASH_AlgSHA1
  },
  { SHA256_LENGTH,
    (void * (*)(void)) SHA256_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) SHA256_DestroyContext,
    (void (*)(void *)) SHA256_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) SHA256_Update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) SHA256_End,
    SHA256_BLOCK_LENGTH,
    HASH_AlgSHA256
  },
  { SHA384_LENGTH,
    (void * (*)(void)) SHA384_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) SHA384_DestroyContext,
    (void (*)(void *)) SHA384_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) SHA384_Update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) SHA384_End,
    SHA384_BLOCK_LENGTH,
    HASH_AlgSHA384
  },
  { SHA512_LENGTH,
    (void * (*)(void)) SHA512_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) SHA512_DestroyContext,
    (void (*)(void *)) SHA512_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) SHA512_Update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) SHA512_End,
    SHA512_BLOCK_LENGTH,
    HASH_AlgSHA512
  },
  { SHA224_LENGTH,
    (void * (*)(void)) SHA224_NewContext,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) SHA224_DestroyContext,
    (void (*)(void *)) SHA224_Begin,
    (void (*)(void *, const unsigned char *, unsigned int)) SHA224_Update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) SHA224_End,
    SHA224_BLOCK_LENGTH,
    HASH_AlgSHA224
  },
};

const SECHashObject *
HASH_GetRawHashObject(HASH_HashType hashType)
{
    if (hashType < HASH_AlgNULL || hashType >= HASH_AlgTOTAL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    return &SECRawHashObjects[hashType];
}
