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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/X520Name/Attic/PCreate.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:38 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXX520Name_Create
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING_TYPE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSPKIXX520Name *
nssPKIXX520Name_Create
(
  NSSArena *arenaOpt,
  nssStringType type,
  NSSItem *data
)
{
  NSSPKIXX520Name *rv = (NSSPKIXX520Name *)NULL;
  nssArenaMark *mark = (nssArenaMark *)NULL;

#ifdef NSSDEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSPKIXX520Name *)NULL;
    }
  }

  if( PR_SUCCESS != nssItem_verifyPointer(data) ) {
    return (NSSPKIXX520Name *)NULL;
  }
#endif /* NSSDEBUG */

  switch( type ) {
  case nssStringType_TeletexString:
  case nssStringType_PrintableString:
  case nssStringType_UniversalString:
  case nssStringType_UTF8String:
  case nssStringType_BMPString:
    break;
  default:
    nss_SetError(NSS_ERROR_INVALID_STRING_TYPE);
    goto loser;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    mark = nssArena_Mark(arenaOpt);
    if( (nssArenaMark *)NULL == mark ) {
      goto loser;
    }
  }

  rv = nss_ZNEW(arenaOpt, NSSPKIXX520Name);
  if( (NSSPKIXX520Name *)NULL == rv ) {
    goto loser;
  }

  rv->utf8 = nssUTF8_Create(arenaOpt, type, data->data, data->size);
  if( (NSSUTF8 *)NULL == rv->utf8 ) {
    goto loser;
  }

  if( nssStringType_PrintableString == type ) {
    rv->wasPrintable = PR_TRUE;
  }

  rv->der = nssUTF8_GetDEREncoding(arenaOpt, type, rv->utf8);
  if( (NSSDER *)NULL == rv->der ) {
    goto loser;
  }

  rv->string.size = rv->der->size;
  rv->string.data = nss_ZAlloc(arenaOpt, rv->string.size);
  if( (void *)NULL == rv->string.data ) {
    goto loser;
  }

  (void)nsslibc_memcpy(rv->string.data, rv->der->data, rv->string.size);

  if( (NSSArena *)NULL != arenaOpt ) {
    rv->inArena = PR_TRUE;
  }

  if( (nssArenaMark *)NULL != mark ) {
    if( PR_SUCCESS != nssArena_Unmark(arenaOpt, mark) ) {
      goto loser;
    }
  }

#ifdef DEBUG
  if( PR_SUCCESS != nss_pkix_X520Name_add_pointer(rv) ) {
    goto loser;
  }

  if( PR_SUCCESS != nssArena_registerDestructor(arena, 
        nss_pkix_X520Name_remove_pointer, rv) ) {
    (void)nss_pkix_X520Name_remove_pointer(rv);
    goto loser;
  }
#endif /* DEBUG */

  return rv;

 loser:
  if( (nssArenaMark *)NULL != mark ) {
    (void)nssArena_Release(arenaOpt, mark);
  }

  if( (NSSArena *)NULL == arenaOpt ) {
    if( (NSSPKIXX520Name *)NULL != rv ) {
      if( (NSSDER *)NULL != rv->der ) {
        nss_ZFreeIf(rv->der->data);
        nss_ZFreeIf(rv->der);
      }

      nss_ZFreeIf(rv->string.data);
      nss_ZFreeIf(rv->utf8);
      nss_ZFreeIf(rv);
    }
  }

  return (NSSPKIXX520Name *)NULL;
}
