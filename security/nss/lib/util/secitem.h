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

#ifndef _SECITEM_H_
#define _SECITEM_H_
/*
 * secitem.h - public data structures and prototypes for handling
 *	       SECItems
 *
 * $Id: secitem.h,v 1.3 2003/12/06 01:16:49 wchang0222%aol.com Exp $
 */

#include "plarena.h"
#include "plhash.h"
#include "seccomon.h"

SEC_BEGIN_PROTOS

/*
** Allocate an item.  If "arena" is not NULL, then allocate from there,
** otherwise allocate from the heap.  If "item" is not NULL, allocate
** only the data for the item, not the item itself.  The item structure
** is allocated zero-filled; the data buffer is not zeroed.
**
** The resulting item is returned; NULL if any error occurs.
**
** XXX This probably should take a SECItemType, but since that is mostly
** unused and our improved APIs (aka Stan) are looming, I left it out.
*/
extern SECItem *SECITEM_AllocItem(PRArenaPool *arena, SECItem *item,
				  unsigned int len);

/*
** Reallocate the data for the specified "item".  If "arena" is not NULL,
** then reallocate from there, otherwise reallocate from the heap.
** In the case where oldlen is 0, the data is allocated (not reallocated).
** In any case, "item" is expected to be a valid SECItem pointer;
** SECFailure is returned if it is not.  If the allocation succeeds,
** SECSuccess is returned.
*/
extern SECStatus SECITEM_ReallocItem(PRArenaPool *arena, SECItem *item,
				     unsigned int oldlen, unsigned int newlen);

/*
** Compare two items returning the difference between them.
*/
extern SECComparison SECITEM_CompareItem(const SECItem *a, const SECItem *b);

/*
** Compare two items -- if they are the same, return true; otherwise false.
*/
extern PRBool SECITEM_ItemsAreEqual(const SECItem *a, const SECItem *b);

/*
** Copy "from" to "to"
*/
extern SECStatus SECITEM_CopyItem(PRArenaPool *arena, SECItem *to, 
                                  const SECItem *from);

/*
** Allocate an item and copy "from" into it.
*/
extern SECItem *SECITEM_DupItem(const SECItem *from);

/*
** Allocate an item and copy "from" into it.  The item itself and the 
** data it points to are both allocated from the arena.  If arena is
** NULL, this function is equivalent to SECITEM_DupItem.
*/
extern SECItem *SECITEM_ArenaDupItem(PRArenaPool *arena, const SECItem *from);

/*
** Free "zap". If freeit is PR_TRUE then "zap" itself is freed.
*/
extern void SECITEM_FreeItem(SECItem *zap, PRBool freeit);

/*
** Zero and then free "zap". If freeit is PR_TRUE then "zap" itself is freed.
*/
extern void SECITEM_ZfreeItem(SECItem *zap, PRBool freeit);

PLHashNumber PR_CALLBACK SECITEM_Hash ( const void *key);

PRIntn PR_CALLBACK SECITEM_HashCompare ( const void *k1, const void *k2);


SEC_END_PROTOS

#endif /* _SECITEM_H_ */
