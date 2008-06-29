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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: oid.c,v $ $Revision: 1.6 $ $Date: 2005/01/20 02:25:49 $";
#endif /* DEBUG */

/*
 * oid.c
 *
 * This file contains the implementation of the basic OID routines.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifndef PKI1_H
#include "pki1.h"
#endif /* PKI1_H */

#include "plhash.h"
#include "plstr.h"

/*
 * NSSOID
 *
 * The public "methods" regarding this "object" are:
 *
 *  NSSOID_CreateFromBER   -- constructor
 *  NSSOID_CreateFromUTF8  -- constructor
 *  (there is no explicit destructor)
 * 
 *  NSSOID_GetDEREncoding
 *  NSSOID_GetUTF8Encoding

 * The non-public "methods" regarding this "object" are:
 *
 *  nssOID_CreateFromBER   -- constructor
 *  nssOID_CreateFromUTF8  -- constructor
 *  (there is no explicit destructor)
 * 
 *  nssOID_GetDEREncoding
 *  nssOID_GetUTF8Encoding
 *
 * In debug builds, the following non-public calls are also available:
 *
 *  nssOID_verifyPointer
 *  nssOID_getExplanation
 *  nssOID_getTaggedUTF8
 */

const NSSOID *NSS_OID_UNKNOWN = (NSSOID *)NULL;

/*
 * First, the public "wrappers"
 */

/*
 * NSSOID_CreateFromBER
 *
 * This routine creates an NSSOID by decoding a BER- or DER-encoded
 * OID.  It may return NULL upon error, in which case it 
 * will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
NSSOID_CreateFromBER
(
  NSSBER *berOid
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  /* 
   * NSSBERs can be created by the user, 
   * so no pointer-tracking can be checked.
   */

  if( (NSSBER *)NULL == berOid ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSOID *)NULL;
  }

  if( (void *)NULL == berOid->data ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSOID *)NULL;
  }
#endif /* DEBUG */
  
  return nssOID_CreateFromBER(berOid);
}

/*
 * NSSOID_CreateFromUTF8
 *
 * This routine creates an NSSOID by decoding a UTF8 string 
 * representation of an OID in dotted-number format.  The string may 
 * optionally begin with an octothorpe.  It may return NULL
 * upon error, in which case it will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
NSSOID_CreateFromUTF8
(
  NSSUTF8 *stringOid
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  /*
   * NSSUTF8s can be created by the user,
   * so no pointer-tracking can be checked.
   */

  if( (NSSUTF8 *)NULL == stringOid ) {
    nss_SetError(NSS_ERROR_INVALID_UTF8);
    return (NSSOID *)NULL;
  }
#endif /* DEBUG */

  return nssOID_CreateFromUTF8(stringOid);
}

/*
 * NSSOID_GetDEREncoding
 *
 * This routine returns the DER encoding of the specified NSSOID.
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return return null upon error, in 
 * which case it will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSOID
 */

NSS_EXTERN NSSDER *
NSSOID_GetDEREncoding
(
  const NSSOID *oid,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSDER *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }
#endif /* DEBUG */

  return nssOID_GetDEREncoding(oid, rvOpt, arenaOpt);
}

/*
 * NSSOID_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing the dotted-number 
 * encoding of the specified NSSOID.  If the optional arena argument 
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return null upon error, in which case it will have created an
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the dotted-digit encoding of 
 *      this NSSOID
 */

NSS_EXTERN NSSUTF8 *
NSSOID_GetUTF8Encoding
(
  const NSSOID *oid,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* DEBUG */

  return nssOID_GetUTF8Encoding(oid, arenaOpt);
}

/*
 * Next, some internal bookkeeping; including the OID "tag" table 
 * and the debug-version pointer tracker.
 */

/*
 * For implementation reasons (so NSSOIDs can be compared with ==),
 * we hash all NSSOIDs.  This is the hash table.
 */

static PLHashTable *oid_hash_table;

/*
 * And this is its lock.
 */

static PZLock *oid_hash_lock;

/*
 * This is the hash function.  We simply XOR the encoded form with
 * itself in sizeof(PLHashNumber)-byte chunks.  Improving this
 * routine is left as an excercise for the more mathematically
 * inclined student.
 */

static PLHashNumber PR_CALLBACK
oid_hash
(
  const void *key
)
{
  const NSSItem *item = (const NSSItem *)key;
  PLHashNumber rv = 0;

  PRUint8 *data = (PRUint8 *)item->data;
  PRUint32 i;
  PRUint8 *rvc = (PRUint8 *)&rv;

  for( i = 0; i < item->size; i++ ) {
    rvc[ i % sizeof(rv) ] ^= *data;
    data++;
  }

  return rv;
}

/*
 * This is the key-compare function.  It simply does a lexical
 * comparison on the encoded OID form.  This does not result in
 * quite the same ordering as the "sequence of numbers" order,
 * but heck it's only used internally by the hash table anyway.
 */

static PRIntn PR_CALLBACK
oid_hash_compare
(
  const void *k1,
  const void *k2
)
{
  PRIntn rv;

  const NSSItem *i1 = (const NSSItem *)k1;
  const NSSItem *i2 = (const NSSItem *)k2;

  PRUint32 size = (i1->size < i2->size) ? i1->size : i2->size;

  rv = (PRIntn)nsslibc_memequal(i1->data, i2->data, size, (PRStatus *)NULL);
  if( 0 == rv ) {
    rv = i1->size - i2->size;
  }

  return !rv;
}

/*
 * The pointer-tracking code
 */

#ifdef DEBUG
extern const NSSError NSS_ERROR_INTERNAL_ERROR;

static nssPointerTracker oid_pointer_tracker;

static PRStatus
oid_add_pointer
(
  const NSSOID *oid
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&oid_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return rv;
  }

  rv = nssPointerTracker_add(&oid_pointer_tracker, oid);
  if( PR_SUCCESS != rv ) {
    NSSError e = NSS_GetError();
    if( NSS_ERROR_NO_MEMORY != e ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    }

    return rv;
  }

  return PR_SUCCESS;
}

#if defined(CAN_DELETE_OIDS)
/*
 * We actually don't define NSSOID deletion, since we keep OIDs
 * in a hash table for easy comparison.  Were we to, this is
 * what the pointer-removal function would look like.
 */

static PRStatus
oid_remove_pointer
(
  const NSSOID *oid
)
{
  PRStatus rv;

  rv = nssPointerTracker_remove(&oid_pointer_tracker, oid);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  }

  return rv;
}
#endif /* CAN_DELETE_OIDS */

#endif /* DEBUG */

/*
 * All dynamically-added OIDs get their memory from one statically-
 * declared arena here, merely so that any cleanup code will have
 * an easier time of it.
 */

static NSSArena *oid_arena;

/*
 * This is the call-once function which initializes the hashtable.
 * It creates it, then prepopulates it with all of the builtin OIDs.
 * It also creates the aforementioned NSSArena.
 */

static PRStatus PR_CALLBACK
oid_once_func
(
  void
)
{
  PRUint32 i;
  
  /* Initialize the arena */
  oid_arena = nssArena_Create();
  if( (NSSArena *)NULL == oid_arena ) {
    goto loser;
  }

  /* Create the hash table lock */
  oid_hash_lock = PZ_NewLock(nssILockOID);
  if( (PZLock *)NULL == oid_hash_lock ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    goto loser;
  }

  /* Create the hash table */
  oid_hash_table = PL_NewHashTable(0, oid_hash, oid_hash_compare,
                                   PL_CompareValues, 
                                   (PLHashAllocOps *)0,
                                   (void *)0);
  if( (PLHashTable *)NULL == oid_hash_table ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    goto loser;
  }

  /* And populate it with all the builtins */
  for( i = 0; i < nss_builtin_oid_count; i++ ) {
    NSSOID *oid = (NSSOID *)&nss_builtin_oids[i];
    PLHashEntry *e = PL_HashTableAdd(oid_hash_table, &oid->data, oid);
    if( (PLHashEntry *)NULL == e ) {
      nss_SetError(NSS_ERROR_NO_MEMORY);
      goto loser;
    }

#ifdef DEBUG
    if( PR_SUCCESS != oid_add_pointer(oid) ) {
      goto loser;
    }
#endif /* DEBUG */
  }

  return PR_SUCCESS;

 loser:
  if( (PLHashTable *)NULL != oid_hash_table ) {
    PL_HashTableDestroy(oid_hash_table);
    oid_hash_table = (PLHashTable *)NULL;
  }

  if( (PZLock *)NULL != oid_hash_lock ) {
    PZ_DestroyLock(oid_hash_lock);
    oid_hash_lock = (PZLock *)NULL;
  }

  if( (NSSArena *)NULL != oid_arena ) {
    (void)nssArena_Destroy(oid_arena);
    oid_arena = (NSSArena *)NULL;
  }

  return PR_FAILURE;
}

/*
 * This is NSPR's once-block.
 */

static PRCallOnceType oid_call_once;

/*
 * And this is our multiply-callable internal init routine, which
 * will call-once our call-once function.
 */

static PRStatus
oid_init
(
  void
)
{
  return PR_CallOnce(&oid_call_once, oid_once_func);
}

#ifdef DEBUG

/*
 * nssOID_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSOID object, 
 * this routine will return PR_SUCCESS.  Otherwise, it will put an 
 * error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssOID_verifyPointer
(
  const NSSOID *oid
)
{
  PRStatus rv;

  rv = oid_init();
  if( PR_SUCCESS != rv ) {
    return PR_FAILURE;
  }

  rv = nssPointerTracker_initialize(&oid_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return PR_FAILURE;
  }

  rv = nssPointerTracker_verify(&oid_pointer_tracker, oid);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INVALID_NSSOID);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}
#endif /* DEBUG */

/*
 * oid_sanity_check_ber
 *
 * This routine merely applies some sanity-checking to the BER-encoded
 * OID.
 */

static PRStatus
oid_sanity_check_ber
(
  NSSBER *berOid
)
{
  PRUint32 i;
  PRUint8 *data = (PRUint8 *)berOid->data;

  /*
   * The size must be longer than zero bytes.
   */

  if( berOid->size <= 0 ) {
    return PR_FAILURE;
  }

  /*
   * In general, we can't preclude any number from showing up
   * someday.  We could probably guess that top-level numbers
   * won't get very big (beyond the current ccitt(0), iso(1),
   * or joint-ccitt-iso(2)).  However, keep in mind that the
   * encoding rules wrap the first two numbers together, as
   *
   *     (first * 40) + second
   *
   * Also, it is noted in the specs that this implies that the
   * second number won't go above forty.
   *
   * 128 encodes 3.8, which seems pretty safe for now.  Let's
   * check that the first byte is less than that.
   *
   * XXX This is a "soft check" -- we may want to exclude it.
   */

  if( data[0] >= 0x80 ) {
    return PR_FAILURE;
  }

  /*
   * In a normalised format, leading 0x80s will never show up.
   * This means that no 0x80 will be preceeded by the final
   * byte of a sequence, which would naturaly be less than 0x80.
   * Our internal encoding for the single-digit OIDs uses 0x80,
   * but the only places we use them (loading the builtin table,
   * and adding a UTF8-encoded OID) bypass this check.
   */

  for( i = 1; i < berOid->size; i++ ) {
    if( (0x80 == data[i]) && (data[i-1] < 0x80) ) {
      return PR_FAILURE;
    }
  }

  /*
   * The high bit of each octet indicates that following octets
   * are included in the current number.  Thus the last byte can't
   * have the high bit set.
   */

  if( data[ berOid->size-1 ] >= 0x80 ) {
    return PR_FAILURE;
  }

  /*
   * Other than that, any byte sequence is legit.
   */
  return PR_SUCCESS;
}

/*
 * nssOID_CreateFromBER
 *
 * This routine creates an NSSOID by decoding a BER- or DER-encoded
 * OID.  It may return NULL upon error, in which case it 
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
nssOID_CreateFromBER
(
  NSSBER *berOid
)
{
  NSSOID *rv;
  PLHashEntry *e;
  
  if( PR_SUCCESS != oid_init() ) {
    return (NSSOID *)NULL;
  }

  if( PR_SUCCESS != oid_sanity_check_ber(berOid) ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSOID *)NULL;
  }

  /*
   * Does it exist?
   */
  PZ_Lock(oid_hash_lock);
  rv = (NSSOID *)PL_HashTableLookup(oid_hash_table, berOid);
  (void)PZ_Unlock(oid_hash_lock);
  if( (NSSOID *)NULL != rv ) {
    /* Found it! */
    return rv;
  }

  /*
   * Doesn't exist-- create it.
   */
  rv = nss_ZNEW(oid_arena, NSSOID);
  if( (NSSOID *)NULL == rv ) {
    return (NSSOID *)NULL;
  }

  rv->data.data = nss_ZAlloc(oid_arena, berOid->size);
  if( (void *)NULL == rv->data.data ) {
    return (NSSOID *)NULL;
  }

  rv->data.size = berOid->size;
  nsslibc_memcpy(rv->data.data, berOid->data, berOid->size);

#ifdef DEBUG
  rv->tag = "<runtime>";
  rv->expl = "(OID registered at runtime)";
#endif /* DEBUG */

  PZ_Lock(oid_hash_lock);
  e = PL_HashTableAdd(oid_hash_table, &rv->data, rv);
  (void)PZ_Unlock(oid_hash_lock);
  if( (PLHashEntry *)NULL == e ) {
    nss_ZFreeIf(rv->data.data);
    nss_ZFreeIf(rv);
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (NSSOID *)NULL;
  }

#ifdef DEBUG
  {
    PRStatus st;
    st = oid_add_pointer(rv);
    if( PR_SUCCESS != st ) {
      PZ_Lock(oid_hash_lock);
      (void)PL_HashTableRemove(oid_hash_table, &rv->data);
      (void)PZ_Unlock(oid_hash_lock);
      (void)nss_ZFreeIf(rv->data.data);
      (void)nss_ZFreeIf(rv);
      return (NSSOID *)NULL;
    }
  }
#endif /* DEBUG */

  return rv;
}

/*
 * oid_sanity_check_utf8
 *
 * This routine merely applies some sanity-checking to the 
 * UTF8-encoded OID.
 */

static PRStatus
oid_sanity_check_utf8
(
  NSSUTF8 *s
)
{
  /*
   * It may begin with an octothorpe, which we skip.
   */

  if( '#' == *s ) {
    s++;
  }

  /*
   * It begins with a number
   */

  if( (*s < '0') || (*s > '9') ) {
    return PR_FAILURE;
  }

  /*
   * First number is only one digit long
   *
   * XXX This is a "soft check" -- we may want to exclude it
   */

  if( (s[1] != '.') && (s[1] != '\0') ) {
    return PR_FAILURE;
  }

  /*
   * Every character is either a digit or a period
   */

  for( ; '\0' != *s; s++ ) {
    if( ('.' != *s) && ((*s < '0') || (*s > '9')) ) {
      return PR_FAILURE;
    }

    /* No two consecutive periods */
    if( ('.' == *s) && ('.' == s[1]) ) {
      return PR_FAILURE;
    }
  }

  /*
   * The last character isn't a period
   */

  if( '.' == *--s ) {
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

static PRUint32
oid_encode_number
(
  PRUint32 n,
  PRUint8 *dp,
  PRUint32 nb
)
{
  PRUint32 a[5];
  PRUint32 i;
  PRUint32 rv;

  a[0] = (n >> 28) & 0x7f;
  a[1] = (n >> 21) & 0x7f;
  a[2] = (n >> 14) & 0x7f;
  a[3] = (n >>  7) & 0x7f;
  a[4] =  n        & 0x7f;

  for( i = 0; i < 5; i++ ) {
    if( 0 != a[i] ) {
      break;
    }
  }

  if( 5 == i ) {
    i--;
  }

  rv = 5-i;
  if( rv > nb ) {
    return rv;
  }

  for( ; i < 4; i++ ) {
    *dp = 0x80 | a[i];
    dp++;
  }

  *dp = a[4];

  return rv;
}

/*
 * oid_encode_huge
 *
 * This routine will convert a huge decimal number into the DER 
 * encoding for oid numbers.  It is not limited to numbers that will
 * fit into some wordsize, like oid_encode_number.  But it's not
 * necessarily very fast, either.  This is here in case some joker
 * throws us an ASCII oid like 1.2.3.99999999999999999999999999.
 */

static PRUint32
oid_encode_huge
(
  NSSUTF8 *s,
  NSSUTF8 *e,
  PRUint8 *dp,
  PRUint32 nb
)
{
  PRUint32 slen = (e-s);
  PRUint32 blen = (slen+1)/2;
  PRUint8 *st = (PRUint8 *)NULL;
  PRUint8 *bd = (PRUint8 *)NULL;
  PRUint32 i;
  PRUint32 bitno;
  PRUint8 *last;
  PRUint8 *first;
  PRUint32 byteno;
  PRUint8 mask;

  /* We'll be munging the data, so duplicate it */
  st = (PRUint8 *)nss_ZAlloc((NSSArena *)NULL, slen);
  if( (PRUint8 *)NULL == st ) {
    return 0;
  }

  /* Don't know ahead of time exactly how long we'll need */
  bd = (PRUint8 *)nss_ZAlloc((NSSArena *)NULL, blen);
  if( (PRUint8 *)NULL == bd ) {
    (void)nss_ZFreeIf(st);
    return 0;
  }

  /* Copy the original, and convert ASCII to numbers */
  for( i = 0; i < slen; i++ ) {
    st[i] = (PRUint8)(s[i] - '0');
  }

  last = &st[slen-1];
  first = &st[0];

  /*
   * The way we create the binary version is by looking at it 
   * bit by bit.  Start with the least significant bit.  If the
   * number is odd, set that bit.  Halve the number (with integer
   * division), and go to the next least significant bit.  Keep 
   * going until the number goes to zero.
   */
  for( bitno = 0; ; bitno++ ) {
    PRUint8 *d;

    byteno = bitno/7;
    mask = (PRUint8)(1 << (bitno%7));

    /* Skip leading zeroes */
    for( ; first < last; first ++ ) {
      if( 0 != *first ) {
        break;
      }
    }

    /* Down to one number and it's a zero?  Done. */
    if( (first == last) && (0 == *last) ) {
      break;
    }

    /* Last digit is odd?  Set the bit */
    if( *last & 1 ) {
      bd[ byteno ] |= mask;
    }


    /* 
     * Divide the number in half.  This is just a matter
     * of going from the least significant digit upwards,
     * halving each one.  If any digit is odd (other than
     * the last, which has already been handled), add five
     * to the digit to its right.
     */
    *last /= 2;

    for( d = &last[-1]; d >= first; d-- ) {
      if( *d & 1 ) {
        d[1] += 5;
      }

      *d /= 2;
    }
  }

  /* Is there room to write the encoded data? */
  if( (byteno+1) > nb ) {
    return (byteno+1);
  }

  /* Trim any leading zero that crept in there */
  for( ; byteno > 0; byteno-- ) {
    if( 0 != bd[ byteno ] ) {
      break;
    }
  }

  /* Copy all but the last, marking the "continue" bit */
  for( i = 0; i < byteno; i++ ) {
    dp[i] = bd[ byteno-i ] | 0x80;
  }
  /* And the last with the "continue" bit clear */
  dp[byteno] = bd[0];

  (void)nss_ZFreeIf(bd);
  (void)nss_ZFreeIf(st);
  return (byteno+1);
}

/*
 * oid_encode_string
 *
 * This routine converts a dotted-number OID into a DER-encoded
 * one.  It assumes we've already sanity-checked the string.
 */

extern const NSSError NSS_ERROR_INTERNAL_ERROR;

static NSSOID *
oid_encode_string
(
  NSSUTF8 *s
)
{
  PRUint32 nn = 0; /* number of numbers */
  PRUint32 nb = 0; /* number of bytes (estimated) */
  NSSUTF8 *t;
  PRUint32 nd = 0; /* number of digits */
  NSSOID *rv;
  PRUint8 *dp;
  PRUint32 a, b;
  PRUint32 inc;

  /* Dump any octothorpe */
  if( '#' == *s ) {
    s++;
  }

  /* Count up the bytes needed */
  for( t = s; '\0' != *t; t++ ) {
    if( '.' == *t ) {
      nb += (nd+1)/2; /* errs on the big side */
      nd = 0;
      nn++;
    } else {
      nd++;
    }
  }
  nb += (nd+1)/2;
  nn++;

  if( 1 == nn ) {
    /*
     * We have our own "denormalised" encoding for these,
     * which is only used internally.
     */
    nb++;
  }

  /* 
   * Allocate.  Note that we don't use the oid_arena here.. this is
   * because there really isn't a "free()" for stuff allocated out of
   * arenas (at least with the current implementation), so this would
   * keep using up memory each time a UTF8-encoded OID were added.
   * If need be (if this is the first time this oid has been seen),
   * we'll copy it.
   */
  rv = nss_ZNEW((NSSArena *)NULL, NSSOID);
  if( (NSSOID *)NULL == rv ) {
    return (NSSOID *)NULL;
  }

  rv->data.data = nss_ZAlloc((NSSArena *)NULL, nb);
  if( (void *)NULL == rv->data.data ) {
    (void)nss_ZFreeIf(rv);
    return (NSSOID *)NULL;
  }

  dp = (PRUint8 *)rv->data.data;

  a = atoi(s);

  if( 1 == nn ) {
    dp[0] = '\x80';
    inc = oid_encode_number(a, &dp[1], nb-1);
    if( inc >= nb ) {
      goto loser;
    }
  } else {
    for( t = s; '.' != *t; t++ ) {
      ;
    }

    t++;
    b = atoi(t);
    inc = oid_encode_number(a*40+b, dp, nb);
    if( inc > nb ) {
      goto loser;
    }
    dp += inc;
    nb -= inc;
    nn -= 2;

    while( nn-- > 0 ) {
      NSSUTF8 *u;

      for( ; '.' != *t; t++ ) {
        ;
      }

      t++;

      for( u = t; ('\0' != *u) && ('.' != *u); u++ ) {
        ;
      }

      if( (u-t > 9) ) {
        /* In the billions.  Rats. */
        inc = oid_encode_huge(t, u, dp, nb);
      } else {
        b = atoi(t);
        inc = oid_encode_number(b, dp, nb);
      }

      if( inc > nb ) {
        goto loser;
      }
      dp += inc;
      nb -= inc;
    }
  }

  return rv;

 loser:
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return (NSSOID *)NULL;
}

/*
 * nssOID_CreateFromUTF8
 *
 * This routine creates an NSSOID by decoding a UTF8 string 
 * representation of an OID in dotted-number format.  The string may 
 * optionally begin with an octothorpe.  It may return NULL
 * upon error, in which case it will have set an error on the error 
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
nssOID_CreateFromUTF8
(
  NSSUTF8 *stringOid
)
{
  NSSOID *rv = (NSSOID *)NULL;
  NSSOID *candidate = (NSSOID *)NULL;
  PLHashEntry *e;

  if( PR_SUCCESS != oid_init() ) {
    return (NSSOID *)NULL;
  }

  if( PR_SUCCESS != oid_sanity_check_utf8(stringOid) ) {
    nss_SetError(NSS_ERROR_INVALID_STRING);
    return (NSSOID *)NULL;
  }

  candidate = oid_encode_string(stringOid);
  if( (NSSOID *)NULL == candidate ) {
    /* Internal error only */
    return rv;
  }

  /*
   * Does it exist?
   */
  PZ_Lock(oid_hash_lock);
  rv = (NSSOID *)PL_HashTableLookup(oid_hash_table, &candidate->data);
  (void)PZ_Unlock(oid_hash_lock);
  if( (NSSOID *)NULL != rv ) {
    /* Already exists.  Delete my copy and return the original. */
    (void)nss_ZFreeIf(candidate->data.data);
    (void)nss_ZFreeIf(candidate);
    return rv;
  }

  /* 
   * Nope.  Add it.  Remember to allocate it out of the oid arena.
   */

  rv = nss_ZNEW(oid_arena, NSSOID);
  if( (NSSOID *)NULL == rv ) {
    goto loser;
  }

  rv->data.data = nss_ZAlloc(oid_arena, candidate->data.size);
  if( (void *)NULL == rv->data.data ) {
    goto loser;
  }

  rv->data.size = candidate->data.size;
  nsslibc_memcpy(rv->data.data, candidate->data.data, rv->data.size);

  (void)nss_ZFreeIf(candidate->data.data);
  (void)nss_ZFreeIf(candidate);

#ifdef DEBUG
  rv->tag = "<runtime>";
  rv->expl = "(OID registered at runtime)";
#endif /* DEBUG */

  PZ_Lock(oid_hash_lock);
  e = PL_HashTableAdd(oid_hash_table, &rv->data, rv);
  (void)PZ_Unlock(oid_hash_lock);
  if( (PLHashEntry *)NULL == e ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    goto loser;
  }

#ifdef DEBUG
  {
    PRStatus st;
    st = oid_add_pointer(rv);
    if( PR_SUCCESS != st ) {
      PZ_Lock(oid_hash_lock);
      (void)PL_HashTableRemove(oid_hash_table, &rv->data);
      (void)PZ_Unlock(oid_hash_lock);
      goto loser;
    }
  }
#endif /* DEBUG */

  return rv;

 loser:
  if( (NSSOID *)NULL != candidate ) {
    (void)nss_ZFreeIf(candidate->data.data);
  }
  (void)nss_ZFreeIf(candidate);

  if( (NSSOID *)NULL != rv ) {
    (void)nss_ZFreeIf(rv->data.data);
  }
  (void)nss_ZFreeIf(rv);

  return (NSSOID *)NULL;
}

/*
 * nssOID_GetDEREncoding
 *
 * This routine returns the DER encoding of the specified NSSOID.
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return return null upon error, in 
 * which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSOID
 */

NSS_EXTERN NSSDER *
nssOID_GetDEREncoding
(
  const NSSOID *oid,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
)
{
  const NSSItem *it;
  NSSDER *rv;

  if( PR_SUCCESS != oid_init() ) {
    return (NSSDER *)NULL;
  }

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSDER *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }
#endif /* NSSDEBUG */

  it = &oid->data;

  if( (NSSDER *)NULL == rvOpt ) {
    rv = nss_ZNEW(arenaOpt, NSSDER);
    if( (NSSDER *)NULL == rv ) {
      return (NSSDER *)NULL;
    }
  } else {
    rv = rvOpt;
  }

  rv->data = nss_ZAlloc(arenaOpt, it->size);
  if( (void *)NULL == rv->data ) {
    if( rv != rvOpt ) {
      (void)nss_ZFreeIf(rv);
    }
    return (NSSDER *)NULL;
  }

  rv->size = it->size;
  nsslibc_memcpy(rv->data, it->data, it->size);

  return rv;
}

/*
 * nssOID_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing the dotted-number 
 * encoding of the specified NSSOID.  If the optional arena argument 
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return null upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the dotted-digit encoding of 
 *      this NSSOID
 */

NSS_EXTERN NSSUTF8 *
nssOID_GetUTF8Encoding
(
  const NSSOID *oid,
  NSSArena *arenaOpt
)
{
  NSSUTF8 *rv;
  PRUint8 *end;
  PRUint8 *d;
  PRUint8 *e;
  char *a;
  char *b;
  PRUint32 len;

  if( PR_SUCCESS != oid_init() ) {
    return (NSSUTF8 *)NULL;
  }

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* NSSDEBUG */

  a = (char *)NULL;

  /* d will point to the next sequence of bytes to decode */
  d = (PRUint8 *)oid->data.data;
  /* end points to one past the legitimate data */
  end = &d[ oid->data.size ];

#ifdef NSSDEBUG
  /*
   * Guarantee that the for(e=d;e<end;e++) loop below will
   * terminate.  Our BER sanity-checking code above will prevent
   * such a BER from being registered, so the only other way one
   * might show up is if our dotted-decimal encoder above screws
   * up or our generated list is wrong.  So I'll wrap it with
   * #ifdef NSSDEBUG and #endif.
   */
  if( end[-1] & 0x80 ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    return (NSSUTF8 *)NULL;
  }
#endif /* NSSDEBUG */

  /*
   * Check for our pseudo-encoded single-digit OIDs
   */
  if( (*d == 0x80) && (2 == oid->data.size) ) {
    /* Funky encoding.  The second byte is the number */
    a = PR_smprintf("%lu", (PRUint32)d[1]);
    if( (char *)NULL == a ) {
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (NSSUTF8 *)NULL;
    }
    goto done;
  }

  for( ; d < end; d = &e[1] ) {
    
    for( e = d; e < end; e++ ) {
      if( 0 == (*e & 0x80) ) {
        break;
      }
    }
    
    if( ((e-d) > 4) || (((e-d) == 4) && (*d & 0x70)) ) {
      /* More than a 32-bit number */
    } else {
      PRUint32 n = 0;
      
      switch( e-d ) {
      case 4:
        n |= ((PRUint32)(e[-4] & 0x0f)) << 28;
      case 3:
        n |= ((PRUint32)(e[-3] & 0x7f)) << 21;
      case 2:
        n |= ((PRUint32)(e[-2] & 0x7f)) << 14;
      case 1:
        n |= ((PRUint32)(e[-1] & 0x7f)) <<  7;
      case 0:
        n |= ((PRUint32)(e[-0] & 0x7f))      ;
      }
      
      if( (char *)NULL == a ) {
        /* This is the first number.. decompose it */
        PRUint32 one = (n/40), two = (n%40);
        
        a = PR_smprintf("%lu.%lu", one, two);
        if( (char *)NULL == a ) {
          nss_SetError(NSS_ERROR_NO_MEMORY);
          return (NSSUTF8 *)NULL;
        }
      } else {
        b = PR_smprintf("%s.%lu", a, n);
        if( (char *)NULL == b ) {
          PR_smprintf_free(a);
          nss_SetError(NSS_ERROR_NO_MEMORY);
          return (NSSUTF8 *)NULL;
        }
        
        PR_smprintf_free(a);
        a = b;
      }
    }
  }

 done:
  /*
   * Even if arenaOpt is NULL, we have to copy the data so that
   * it'll be freed with the right version of free: ours, not
   * PR_smprintf_free's.
   */
  len = PL_strlen(a);
  rv = (NSSUTF8 *)nss_ZAlloc(arenaOpt, len);
  if( (NSSUTF8 *)NULL == rv ) {
    PR_smprintf_free(a);
    return (NSSUTF8 *)NULL;
  }

  nsslibc_memcpy(rv, a, len);
  PR_smprintf_free(a);

  return rv;
}

/*
 * nssOID_getExplanation
 *
 * This method is only present in debug builds.
 *
 * This routine will return a static pointer to a UTF8-encoded string
 * describing (in English) the specified OID.  The memory pointed to
 * by the return value is not owned by the caller, and should not be
 * freed or modified.  Note that explanations are only provided for
 * the OIDs built into the NSS library; there is no way to specify an
 * explanation for dynamically created OIDs.  This routine is intended
 * only for use in debugging tools such as "derdump."  This routine
 * may return null upon error, in which case it will have placed an
 * error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *
 * Return value:
 *  NULL upon error
 *  A static pointer to a readonly, non-caller-owned UTF8-encoded
 *    string explaining the specified OID.
 */

#ifdef DEBUG
NSS_EXTERN const NSSUTF8 *
nssOID_getExplanation
(
  NSSOID *oid
)
{
  if( PR_SUCCESS != oid_init() ) {
    return (const NSSUTF8 *)NULL;
  }

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSUTF8 *)NULL;
  }
#endif /* NSSDEBUG */

  return oid->expl;
}

extern const NSSError NSS_ERROR_INVALID_NSSOID;
#endif /* DEBUG */

/*
 * nssOID_getTaggedUTF8
 *
 * This method is only present in debug builds.
 *
 * This routine will return a pointer to a caller-owned UTF8-encoded
 * string containing a tagged encoding of the specified OID.  Note
 * that OID (component) tags are only provided for the OIDs built 
 * into the NSS library; there is no way to specify tags for 
 * dynamically created OIDs.  This routine is intended for use in
 * debugging tools such as "derdump."   If the optional arena argument
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine 
 * may return return null upon error, in which case it will have set 
 * an error on the error stack.
 *
 * The error may be one of the following values
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the tagged encoding of
 *      this NSSOID
 */

#ifdef DEBUG
NSS_EXTERN NSSUTF8 *
nssOID_getTaggedUTF8
(
  NSSOID *oid,
  NSSArena *arenaOpt
)
{
  NSSUTF8 *rv;
  char *raw;
  char *c;
  char *a = (char *)NULL;
  char *b;
  PRBool done = PR_FALSE;
  PRUint32 len;

  if( PR_SUCCESS != oid_init() ) {
    return (NSSUTF8 *)NULL;
  }

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* NSSDEBUG */

  a = PR_smprintf("{");
  if( (char *)NULL == a ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (NSSUTF8 *)NULL;
  }

  /*
   * What I'm doing here is getting the text version of the OID,
   * e.g. 1.2.12.92, then looking up each set of leading numbers
   * as oids.. e.g. "1," then "1.2," then "1.2.12," etc.  Each of
   * those will have the leaf tag, and I just build up the string.
   * I never said this was the most efficient way of doing it,
   * but hey it's a debug-build thing, and I'm getting really tired
   * of writing this stupid low-level PKI code.
   */

  /* I know it's all ASCII, so I can use char */
  raw = (char *)nssOID_GetUTF8Encoding(oid, (NSSArena *)NULL);
  if( (char *)NULL == raw ) {
    return (NSSUTF8 *)NULL;
  }

  for( c = raw; !done; c++ ) {
    NSSOID *lead;
    char *lastdot;

    for( ; '.' != *c; c++ ) {
      if( '\0' == *c ) {
        done = PR_TRUE;
        break;
      }
    }

    *c = '\0';
    lead = nssOID_CreateFromUTF8((NSSUTF8 *)raw);
    if( (NSSOID *)NULL == lead ) {
      PR_smprintf_free(a);
      nss_ZFreeIf(raw);
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (NSSUTF8 *)NULL;
    }

    lastdot = PL_strrchr(raw, '.');
    if( (char *)NULL == lastdot ) {
      lastdot = raw;
    }

    b = PR_smprintf("%s %s(%s) ", a, lead->tag, &lastdot[1]);
    if( (char *)NULL == b ) {
      PR_smprintf_free(a);
      nss_ZFreeIf(raw);
      /* drop the OID reference on the floor */
      nss_SetError(NSS_ERROR_NO_MEMORY);
      return (NSSUTF8 *)NULL;
    }

    PR_smprintf_free(a);
    a = b;

    if( !done ) {
      *c = '.';
    }
  }

  nss_ZFreeIf(raw);

  b = PR_smprintf("%s }", a);
  if( (char *)NULL == b ) {
    PR_smprintf_free(a);
    nss_SetError(NSS_ERROR_NO_MEMORY);
    return (NSSUTF8 *)NULL;
  }

  len = PL_strlen(b);
  
  rv = (NSSUTF8 *)nss_ZAlloc(arenaOpt, len+1);
  if( (NSSUTF8 *)NULL == rv ) {
    PR_smprintf_free(b);
    return (NSSUTF8 *)NULL;
  }

  nsslibc_memcpy(rv, b, len);
  PR_smprintf_free(b);

  return rv;
}

extern const NSSError NSS_ERROR_INVALID_NSSOID;
extern const NSSError NSS_ERROR_NO_MEMORY;
#endif /* DEBUG */
