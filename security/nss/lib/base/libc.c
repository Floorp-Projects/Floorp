/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * libc.c
 *
 * This file contains our wrappers/reimplementations for "standard" 
 * libc functions.  Things like "memcpy."  We add to this as we need 
 * it.  Oh, and let's keep it in alphabetical order, should it ever 
 * get large.  Most string/character stuff should be in utf8.c, not 
 * here.  This file (and maybe utf8.c) should be the only ones in
 * NSS to include files with angle brackets.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#include <string.h> /* memcpy, memset */

/*
 * nsslibc_memcpy
 * nsslibc_memset
 * nsslibc_offsetof
 * nsslibc_memequal
 */

/*
 * nsslibc_memcpy
 *
 * Errors:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  NULL on error
 *  The destination pointer on success
 */

NSS_IMPLEMENT void *
nsslibc_memcpy
(
  void *dest,
  const void *source,
  PRUint32 n
)
{
#ifdef NSSDEBUG
  if( ((void *)NULL == dest) || ((const void *)NULL == source) ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (void *)NULL;
  }
#endif /* NSSDEBUG */

  return memcpy(dest, source, (size_t)n);
}

/*
 * nsslibc_memset
 *
 * Errors:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  NULL on error
 *  The destination pointer on success
 */

NSS_IMPLEMENT void *
nsslibc_memset
(
  void *dest,
  PRUint8 byte,
  PRUint32 n
)
{
#ifdef NSSDEBUG
  if( ((void *)NULL == dest) ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (void *)NULL;
  }
#endif /* NSSDEBUG */

  return memset(dest, (int)byte, (size_t)n);
}

/*
 * nsslibc_memequal
 *
 * Errors:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_TRUE if they match
 *  PR_FALSE if they don't
 *  PR_FALSE upon error
 */

NSS_IMPLEMENT PRBool
nsslibc_memequal
(
  const void *a,
  const void *b,
  PRUint32 len,
  PRStatus *statusOpt
)
{
#ifdef NSSDEBUG
  if( (((void *)NULL == a) || ((void *)NULL == b)) ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    if( (PRStatus *)NULL != statusOpt ) {
      *statusOpt = PR_FAILURE;
    }
    return PR_FALSE;
  }
#endif /* NSSDEBUG */

  if( (PRStatus *)NULL != statusOpt ) {
    *statusOpt = PR_SUCCESS;
  }

  if( 0 == memcmp(a, b, len) ) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

/*
 * nsslibc_memcmp
 */

NSS_IMPLEMENT PRInt32
nsslibc_memcmp
(
  const void *a,
  const void *b,
  PRUint32 len,
  PRStatus *statusOpt
)
{
  int v;

#ifdef NSSDEBUG
  if( (((void *)NULL == a) || ((void *)NULL == b)) ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    if( (PRStatus *)NULL != statusOpt ) {
      *statusOpt = PR_FAILURE;
    }
    return -2;
  }
#endif /* NSSDEBUG */

  if( (PRStatus *)NULL != statusOpt ) {
    *statusOpt = PR_SUCCESS;
  }

  v = memcmp(a, b, len);
  return (PRInt32)v;
}

/*
 * offsetof is a preprocessor definition
 */
