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
static const char CVS_ID[] = "@(#) $RCSfile: whatnspr.c,v $ $Revision: 1.4 $ $Date: 2005/01/20 02:25:45 $";
#endif /* DEBUG */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/*
 * This file isolates us from differences in NSPR versions.
 * We have to detect the library with which we're running at
 * runtime, and switch behaviours there.  This lets us do
 * stuff like load cryptoki modules in Communicator.
 *
 * Hey, it's the PORT layer all over again!
 */

static int whatnspr = 0;

static int
set_whatnspr
(
  void
)
{
  /*
   * The only runtime difference I could find was the
   * return value of PR_dtoa.  We can't just look for
   * a symbol in NSPR >=2, because it'll always be
   * found (because we compile against NSPR >=2).
   * Maybe we could look for a symbol merely in NSPR 1?
   *
   */

  char buffer[64];
  int decpt = 0, sign = 0;
  char *rve = (char *)0;
  /* extern int PR_dtoa(double, int, int, int *, int *, char **, char *, int); */
  int r = (int)PR_dtoa((double)1.0, 0, 5, &decpt, &sign, &rve, 
                       buffer, sizeof(buffer));

  switch( r ) {
  case 0:
  case -1:
    whatnspr = 2;
    /*
     * If we needed to, *now* we could look up "libVersionPoint"
     * and get more data there.. except all current NSPR's (up
     * to NSPR 4.x at time of writing) still say 2 in their
     * version structure.
     */
    break;
  default:
    whatnspr = 1;
    break;
  }

  return whatnspr;
}

#define WHATNSPR (whatnspr ? whatnspr : set_whatnspr())

NSS_IMPLEMENT PRStatus
nss_NewThreadPrivateIndex
(
  PRUintn *ip,
  PRThreadPrivateDTOR dtor
)
{
  switch( WHATNSPR ) {
  case 1:
    {
      PRLibrary *l = (PRLibrary *)0;
      void *f = PR_FindSymbolAndLibrary("PR_NewThreadPrivateID", &l);
      typedef PRInt32 (*ntpt)(void);
      ntpt ntp = (ntpt) f;

      PR_ASSERT((void *)0 != f);

      *ip = ntp();
      return PR_SUCCESS;
    }
  case 2:
  default:
    return PR_NewThreadPrivateIndex(ip, dtor);
  }
}

NSS_IMPLEMENT void *
nss_GetThreadPrivate
(
  PRUintn i
)
{
  switch( WHATNSPR ) {
  case 1:
    {
      PRLibrary *l = (PRLibrary *)0;
      void *f = PR_FindSymbolAndLibrary("PR_GetThreadPrivate", &l);
      typedef void *(*gtpt)(PRThread *, PRInt32);
      gtpt gtp = (gtpt) f;

      PR_ASSERT((void *)0 != f);

      return gtp(PR_CurrentThread(), i);
    }
  case 2:
  default:
    return PR_GetThreadPrivate(i);
  }
}

NSS_IMPLEMENT void
nss_SetThreadPrivate
(
  PRUintn i,
  void *v
)
{
  switch( WHATNSPR ) {
  case 1:
    {
      PRLibrary *l = (PRLibrary *)0;
      void *f = PR_FindSymbolAndLibrary("PR_SetThreadPrivate", &l);
      typedef PRStatus (*stpt)(PRThread *, PRInt32, void *);
      stpt stp = (stpt) f;

      PR_ASSERT((void *)0 != f);

      (void)stp(PR_CurrentThread(), i, v);
      return;
    }
  case 2:
  default:
    (void)PR_SetThreadPrivate(i, v);
    return;
  }
}
