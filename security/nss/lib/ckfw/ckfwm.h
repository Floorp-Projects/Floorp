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

#ifndef CKFWM_H
#define CKFWM_H

#ifdef DEBUG
static const char CKFWM_CVS_ID[] = "@(#) $RCSfile: ckfwm.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:12 $ $Name:  $";
#endif /* DEBUG */

/*
 * ckfwm.h
 *
 * This file prototypes the module-private calls of the NSS Cryptoki Framework.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef NSSCKFWT_H
#include "nssckfwt.h"
#endif /* NSSCKFWT_H */

/*
 * nssCKFWHash
 *
 *  nssCKFWHash_Create
 *  nssCKFWHash_Destroy
 *  nssCKFWHash_Add
 *  nssCKFWHash_Remove
 *  nssCKFWHash_Count
 *  nssCKFWHash_Exists
 *  nssCKFWHash_Lookup
 *  nssCKFWHash_Iterate
 */

/*
 * nssCKFWHash_Create
 *
 */
NSS_EXTERN nssCKFWHash *
nssCKFWHash_Create
(
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
);

/*
 * nssCKFWHash_Destroy
 *
 */
NSS_EXTERN void
nssCKFWHash_Destroy
(
  nssCKFWHash *hash
);

/*
 * nssCKFWHash_Add
 *
 */
NSS_EXTERN CK_RV
nssCKFWHash_Add
(
  nssCKFWHash *hash,
  const void *key,
  const void *value
);

/*
 * nssCKFWHash_Remove
 *
 */
NSS_EXTERN void
nssCKFWHash_Remove
(
  nssCKFWHash *hash,
  const void *it
);

/*
 * nssCKFWHash_Count
 *
 */
NSS_EXTERN CK_ULONG
nssCKFWHash_Count
(
  nssCKFWHash *hash
);

/*
 * nssCKFWHash_Exists
 *
 */
NSS_EXTERN CK_BBOOL
nssCKFWHash_Exists
(
  nssCKFWHash *hash,
  const void *it
);

/*
 * nssCKFWHash_Lookup
 *
 */
NSS_EXTERN void *
nssCKFWHash_Lookup
(
  nssCKFWHash *hash,
  const void *it
);

/*
 * nssCKFWHash_Iterate
 *
 */
NSS_EXTERN void
nssCKFWHash_Iterate
(
  nssCKFWHash *hash,
  nssCKFWHashIterator fcn,
  void *closure
);

#endif /* CKFWM_H */
