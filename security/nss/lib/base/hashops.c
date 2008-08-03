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
static const char CVS_ID[] = "@(#) $RCSfile: hashops.c,v $ $Revision: 1.6 $ $Date: 2005/01/20 02:25:45 $";
#endif /* DEBUG */

/*
 * hashops.c
 *
 * This file includes a set of PLHashAllocOps that use NSSArenas.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

static void * PR_CALLBACK
nss_arena_hash_alloc_table
(
  void *pool,
  PRSize size
)
{
  NSSArena *arena = (NSSArena *)NULL;

#ifdef NSSDEBUG
  if( (void *)NULL != arena ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
      return (void *)NULL;
    }
  }
#endif /* NSSDEBUG */

  return nss_ZAlloc(arena, size);
}

static void PR_CALLBACK
nss_arena_hash_free_table
(
  void *pool, 
  void *item
)
{
  (void)nss_ZFreeIf(item);
}

static PLHashEntry * PR_CALLBACK
nss_arena_hash_alloc_entry
(
  void *pool,
  const void *key
)
{
  NSSArena *arena = NULL;

#ifdef NSSDEBUG
  if( (void *)NULL != arena ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
      return (void *)NULL;
    }
  }
#endif /* NSSDEBUG */

  return nss_ZNEW(arena, PLHashEntry);
}

static void PR_CALLBACK
nss_arena_hash_free_entry
(
  void *pool,
  PLHashEntry *he,
  PRUintn flag
)
{
  if( HT_FREE_ENTRY == flag ) {
    (void)nss_ZFreeIf(he);
  }
}

NSS_IMPLEMENT_DATA PLHashAllocOps 
nssArenaHashAllocOps = {
  nss_arena_hash_alloc_table,
  nss_arena_hash_free_table,
  nss_arena_hash_alloc_entry,
  nss_arena_hash_free_entry
};
