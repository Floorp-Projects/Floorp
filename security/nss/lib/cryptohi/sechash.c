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
#include "sechash.h"
#include "secoidt.h"
#include "secerr.h"
#include "blapi.h"
#include "pk11func.h"	/* for the PK11_ calls below. */

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


static void *
md2_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_MD2);
}

static void *
md5_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_MD5);
}

static void *
sha1_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_SHA1);
}

static void *
sha256_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_SHA256);
}

static void *
sha384_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_SHA384);
}

static void *
sha512_NewContext(void) {
	return (void *) PK11_CreateDigestContext(SEC_OID_SHA512);
}

const SECHashObject SECHashObjects[] = {
  { 0,
    (void * (*)(void)) null_hash_new_context,
    (void * (*)(void *)) null_hash_clone_context,
    (void (*)(void *, PRBool)) null_hash_destroy_context,
    (void (*)(void *)) null_hash_begin,
    (void (*)(void *, const unsigned char *, unsigned int)) null_hash_update,
    (void (*)(void *, unsigned char *, unsigned int *,
	      unsigned int)) null_hash_end
  },
  { MD2_LENGTH,
    (void * (*)(void)) md2_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
  { MD5_LENGTH,
    (void * (*)(void)) md5_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
  { SHA1_LENGTH,
    (void * (*)(void)) sha1_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
  { SHA256_LENGTH,
    (void * (*)(void)) sha256_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
  { SHA384_LENGTH,
    (void * (*)(void)) sha384_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
  { SHA512_LENGTH,
    (void * (*)(void)) sha512_NewContext,
    (void * (*)(void *)) PK11_CloneContext,
    (void (*)(void *, PRBool)) PK11_DestroyContext,
    (void (*)(void *)) PK11_DigestBegin,
    (void (*)(void *, const unsigned char *, unsigned int)) PK11_DigestOp,
    (void (*)(void *, unsigned char *, unsigned int *, unsigned int)) 
							PK11_DigestFinal
  },
};

const SECHashObject * 
HASH_GetHashObject(HASH_HashType type)
{
    return &SECHashObjects[type];
}

HASH_HashType
HASH_GetHashTypeByOidTag(SECOidTag hashOid)
{
    HASH_HashType ht	= HASH_AlgNULL;

    switch(hashOid) {
    case SEC_OID_MD2:	 ht = HASH_AlgMD2;    break;
    case SEC_OID_MD5:	 ht = HASH_AlgMD5;    break;
    case SEC_OID_SHA1:	 ht = HASH_AlgSHA1;   break;
    case SEC_OID_SHA256: ht = HASH_AlgSHA256; break;
    case SEC_OID_SHA384: ht = HASH_AlgSHA384; break;
    case SEC_OID_SHA512: ht = HASH_AlgSHA512; break;
    default:             ht = HASH_AlgNULL;   
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	break;
    }
    return ht;
}

const SECHashObject * 
HASH_GetHashObjectByOidTag(SECOidTag hashOid)
{
    HASH_HashType ht	= HASH_GetHashTypeByOidTag(hashOid);

    return (ht == HASH_AlgNULL) ? NULL : &SECHashObjects[ht];
}

/* returns zero for unknown hash OID */
unsigned int
HASH_ResultLenByOidTag(SECOidTag hashOid)
{
    const SECHashObject * hashObject = HASH_GetHashObjectByOidTag(hashOid);
    unsigned int          resultLen = 0;

    if (hashObject)
    	resultLen = hashObject->length;
    return resultLen;
}

/* returns zero if hash type invalid. */
unsigned int
HASH_ResultLen(HASH_HashType type)
{
    if ( ( type < HASH_AlgNULL ) || ( type >= HASH_AlgTOTAL ) ) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return(0);
    }
    
    return(SECHashObjects[type].length);
}

unsigned int
HASH_ResultLenContext(HASHContext *context)
{
    return(context->hashobj->length);
}



SECStatus
HASH_HashBuf(HASH_HashType type,
	     unsigned char *dest,
	     unsigned char *src,
	     uint32 src_len)
{
    HASHContext *cx;
    unsigned int part;
    
    if ( ( type < HASH_AlgNULL ) || ( type >= HASH_AlgTOTAL ) ) {
	return(SECFailure);
    }
    
    cx = HASH_Create(type);
    if ( cx == NULL ) {
	return(SECFailure);
    }
    HASH_Begin(cx);
    HASH_Update(cx, src, src_len);
    HASH_End(cx, dest, &part, HASH_ResultLenContext(cx));
    HASH_Destroy(cx);

    return(SECSuccess);
}

HASHContext *
HASH_Create(HASH_HashType type)
{
    void *hash_context = NULL;
    HASHContext *ret = NULL;
    
    if ( ( type < HASH_AlgNULL ) || ( type >= HASH_AlgTOTAL ) ) {
	return(NULL);
    }
    
    hash_context = (* SECHashObjects[type].create)();
    if ( hash_context == NULL ) {
	goto loser;
    }

    ret = (HASHContext *)PORT_Alloc(sizeof(HASHContext));
    if ( ret == NULL ) {
	goto loser;
    }

    ret->hash_context = hash_context;
    ret->hashobj = &SECHashObjects[type];
    
    return(ret);
    
loser:
    if ( hash_context != NULL ) {
	(* SECHashObjects[type].destroy)(hash_context, PR_TRUE);
    }
    
    return(NULL);
}


HASHContext *
HASH_Clone(HASHContext *context)
{
    void *hash_context = NULL;
    HASHContext *ret = NULL;
    
    hash_context = (* context->hashobj->clone)(context->hash_context);
    if ( hash_context == NULL ) {
	goto loser;
    }

    ret = (HASHContext *)PORT_Alloc(sizeof(HASHContext));
    if ( ret == NULL ) {
	goto loser;
    }

    ret->hash_context = hash_context;
    ret->hashobj = context->hashobj;
    
    return(ret);
    
loser:
    if ( hash_context != NULL ) {
	(* context->hashobj->destroy)(hash_context, PR_TRUE);
    }
    
    return(NULL);

}

void
HASH_Destroy(HASHContext *context)
{
    (* context->hashobj->destroy)(context->hash_context, PR_TRUE);
    PORT_Free(context);
    return;
}


void
HASH_Begin(HASHContext *context)
{
    (* context->hashobj->begin)(context->hash_context);
    return;
}


void
HASH_Update(HASHContext *context,
	    const unsigned char *src,
	    unsigned int len)
{
    (* context->hashobj->update)(context->hash_context, src, len);
    return;
}

void
HASH_End(HASHContext *context,
	 unsigned char *result,
	 unsigned int *result_len,
	 unsigned int max_result_len)
{
    (* context->hashobj->end)(context->hash_context, result, result_len,
			      max_result_len);
    return;
}



