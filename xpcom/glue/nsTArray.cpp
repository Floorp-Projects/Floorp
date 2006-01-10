/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is C++ array template.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <string.h>
#include "nsTArray.h"
#include "nsXPCOM.h"
#include "nsDebug.h"

const nsTArray_base::Header nsTArray_base::sEmptyHdr = { 0, 0 };

PRBool
nsTArray_base::EnsureCapacity(size_type capacity, size_type elemSize) {
  // If the requested memory allocation exceeds size_type(-1)/2, then our
  // doubling algorithm may not be able to allocate it.  Just bail out in
  // cases like that.  We don't want to be allocating 2 GB+ arrays anyway.
  if (capacity * elemSize > size_type(-1)/2) {
    NS_ERROR("Attempting to allocate excessively large array");
    return PR_FALSE;
  }
  if (mHdr == &sEmptyHdr) {
    // NS_Alloc new data
    Header *header = NS_STATIC_CAST(Header*,
                         NS_Alloc(sizeof(Header) + capacity * elemSize));
    if (!header)
      return PR_FALSE;
    header->mLength = 0;
    header->mCapacity = capacity;
    mHdr = header;
  } else {
    // NS_Realloc existing data
    if (capacity <= mHdr->mCapacity)
      return PR_TRUE;

    // Use doubling algorithm when forced to increase available capacity.
    if (mHdr->mCapacity > 0) {
      size_type temp = mHdr->mCapacity;
      while (temp < capacity)
        temp <<= 1;
      capacity = temp;
    }

    size_type size = sizeof(Header) + capacity * elemSize;
    void *ptr = NS_Realloc(mHdr, size);
    if (!ptr)
      return PR_FALSE;
    mHdr = NS_STATIC_CAST(Header*, ptr);
    mHdr->mCapacity = capacity;
  }
  return PR_TRUE;
}

void
nsTArray_base::ShrinkCapacity(size_type elemSize) {
  if (mHdr == &sEmptyHdr)
    return;

  if (mHdr->mLength >= mHdr->mCapacity)  // should never be greater than...
    return;

  if (mHdr->mLength == 0) {
    NS_Free(mHdr);
    mHdr = NS_CONST_CAST(Header *, &sEmptyHdr);
    return;
  }

  size_type size = sizeof(Header) + mHdr->mLength * elemSize;
  void *ptr = NS_Realloc(mHdr, size);
  if (!ptr)
    return;
  mHdr = NS_STATIC_CAST(Header*, ptr);
  mHdr->mCapacity = mHdr->mLength;
}

void
nsTArray_base::ShiftData(index_type start, size_type oldLen, size_type newLen,
                         size_type elemSize) {
  if (oldLen == newLen)
    return;

  // Determine how many elements need to be shifted
  size_type num = mHdr->mLength - (start + oldLen);

  // Compute the resulting length of the array
  mHdr->mLength += newLen - oldLen;
  if (mHdr->mLength == 0) {
    ShrinkCapacity(elemSize);
  } else {
    // Maybe nothing needs to be shifted
    if (num == 0)
      return;
    // Perform shift (change units to bytes first)
    start *= elemSize;
    newLen *= elemSize;
    oldLen *= elemSize;
    num *= elemSize;
    char *base = NS_REINTERPRET_CAST(char*, mHdr + 1) + start;
    memmove(base + newLen, base + oldLen, num);
  }
}
