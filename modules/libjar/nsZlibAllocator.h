/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

/* 
 * Allocator for zlib optimized for mozilla's use
 *
 */

#define NBUCKETS 6
#define BY4ALLOC_ITEMS 320

class nsZlibAllocator;
extern nsZlibAllocator *gZlibAllocator;

class nsZlibAllocator {
 public:
  struct nsMemBucket {
    void *ptr;
    PRUint32 size;

    // Is this bucket available ?
    // 1 : free
    // 0 or negative : in use
    // This is an int because we use atomic inc/dec to operate on it
    PRInt32 available;
  };

  nsMemBucket mMemBucket[NBUCKETS];

  nsZlibAllocator() {
    memset(mMemBucket, 0, sizeof(mMemBucket));
    for (PRUint32 i = 0; i < NBUCKETS; i++)
      mMemBucket[i].available = 1;
    return;
  }

  ~nsZlibAllocator() {
    for (PRUint32 i = 0; i < NBUCKETS; i++)
    {
      PRBool claimed = Claim(i);
      
      // ASSERT that we claimed the bucked. If we cannot, then the bucket is in use.
      // We dont attempt to free this ptr.
      // This will most likely cause a leak of this memory.
      PR_ASSERT(claimed);
      
      // Free bucket memory if not in use
      if (claimed && mMemBucket[i].ptr)
        free(mMemBucket[i].ptr);
    }
  }

  // zlib allocators
  void* zAlloc(PRUint32 items, PRUint32 size);
  void  zFree(void *ptr);

  // Bucket handling.
  PRBool Claim(PRUint32 i) {
    PRBool claimed = (PR_AtomicDecrement(&mMemBucket[i].available) == 0);
    // Undo the claim, if claim failed
    if (!claimed)
      Unclaim(i);

    return claimed;
  }

  void Unclaim(PRUint32 i) { PR_AtomicIncrement(&mMemBucket[i].available); }
};

