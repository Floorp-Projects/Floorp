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

nsTArray_base::size_type
nsTArray_base::Capacity() const {
  if (!mData)
    return 0;
  Header *header = NS_STATIC_CAST(Header*, mData) - 1;
  return header->mCapacity;
}

PRBool
nsTArray_base::EnsureCapacity(size_type capacity, size_type elemSize) {
  // If the requested memory allocation exceeds size_type(-1)/2, then our
  // doubling algorithm may not be able to allocate it.  Just bail out in
  // cases like that.  We don't want to be allocating 2 GB+ arrays anyway.
  if (capacity * elemSize > size_type(-1)/2) {
    NS_ERROR("Attempting to allocate excessively large array");
    return PR_FALSE;
  }
  if (!mData) {
    // NS_Alloc new data
    Header *header = NS_STATIC_CAST(Header*,
                         NS_Alloc(sizeof(Header) + capacity * elemSize));
    if (!header)
      return PR_FALSE;
    header->mCapacity = capacity;
    mData = header + 1;
  } else {
    // NS_Realloc existing data
    Header *header = NS_STATIC_CAST(Header*, mData) - 1;
    if (capacity <= header->mCapacity)
      return PR_TRUE;

    // Use doubling algorithm when forced to increase available capacity.
    if (header->mCapacity > 0) {
      size_type temp = header->mCapacity;
      while (temp < capacity)
        temp <<= 1;
      capacity = temp;
    }

    size_type size = sizeof(Header) + capacity * elemSize;
    void *ptr = NS_Realloc(header, size);
    if (!ptr)
      return PR_FALSE;
    header = NS_STATIC_CAST(Header*, ptr);
    header->mCapacity = capacity;
    mData = header + 1;
  }
  return PR_TRUE;
}

void
nsTArray_base::ShrinkCapacity(size_type elemSize) {
  if (!mData)
    return;

  Header *header = NS_STATIC_CAST(Header*, mData) - 1;
  if (mLength >= header->mCapacity)  // should never be greater than...
    return;

  if (mLength == 0) {
    NS_Free(header);
    mData = nsnull;
    return;
  }

  size_type size = sizeof(Header) + mLength * elemSize;
  void *ptr = NS_Realloc(header, size);
  if (!ptr)
    return;
  header = NS_STATIC_CAST(Header*, ptr);
  header->mCapacity = mLength;
  mData = header + 1;
}

void
nsTArray_base::ShiftData(index_type start, size_type oldLen, size_type newLen,
                         size_type elemSize) {
  if (oldLen == newLen)
    return;

  // Determine how many elements need to be shifted
  size_type num = mLength - (start + oldLen);

  // Compute the resulting length of the array
  mLength = (mLength + newLen) - oldLen;
  if (mLength == 0) {
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
    char *base = NS_STATIC_CAST(char*, mData) + start;
    memmove(base + newLen, base + oldLen, num);
  }
}
