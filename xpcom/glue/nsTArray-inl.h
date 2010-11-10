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

#ifndef nsTArray_h__
#  error "Don't include this file directly"
#endif

template<class Alloc>
nsTArray_base<Alloc>::nsTArray_base()
  : mHdr(EmptyHdr()) {
  MOZ_COUNT_CTOR(nsTArray_base);
}

template<class Alloc>
nsTArray_base<Alloc>::~nsTArray_base() {
  if (mHdr != EmptyHdr() && !UsesAutoArrayBuffer()) {
    Alloc::Free(mHdr);
  }
  MOZ_COUNT_DTOR(nsTArray_base);
}

template<class Alloc>
PRBool
nsTArray_base<Alloc>::EnsureCapacity(size_type capacity, size_type elemSize) {
  // This should be the most common case so test this first
  if (capacity <= mHdr->mCapacity)
    return PR_TRUE;

  // If the requested memory allocation exceeds size_type(-1)/2, then
  // our doubling algorithm may not be able to allocate it.
  // Additionally we couldn't fit in the Header::mCapacity
  // member. Just bail out in cases like that.  We don't want to be
  // allocating 2 GB+ arrays anyway.
  if ((PRUint64)capacity * elemSize > size_type(-1)/2) {
    NS_ERROR("Attempting to allocate excessively large array");
    return PR_FALSE;
  }

  if (mHdr == EmptyHdr()) {
    // Malloc() new data
    Header *header = static_cast<Header*>
                     (Alloc::Malloc(sizeof(Header) + capacity * elemSize));
    if (!header)
      return PR_FALSE;
    header->mLength = 0;
    header->mCapacity = capacity;
    header->mIsAutoArray = 0;
    mHdr = header;

    return PR_TRUE;
  }

  // Use doubling algorithm when forced to increase available
  // capacity.  (Note that mCapacity is only 31 bits wide, so
  // multiplication promotes its type. We use |2u| instead of |2|
  // to make sure it's promoted to unsigned.)
  capacity = PR_MAX(capacity, mHdr->mCapacity * 2u);

  Header *header;
  if (UsesAutoArrayBuffer()) {
    // Malloc() and copy
    header = static_cast<Header*>
             (Alloc::Malloc(sizeof(Header) + capacity * elemSize));
    if (!header)
      return PR_FALSE;

    memcpy(header, mHdr, sizeof(Header) + Length() * elemSize);
  } else {
    // Realloc() existing data
    size_type size = sizeof(Header) + capacity * elemSize;
    header = static_cast<Header*>(Alloc::Realloc(mHdr, size));
    if (!header)
      return PR_FALSE;
  }

  header->mCapacity = capacity;
  mHdr = header;

  return PR_TRUE;
}

template<class Alloc>
void
nsTArray_base<Alloc>::ShrinkCapacity(size_type elemSize) {
  if (mHdr == EmptyHdr() || UsesAutoArrayBuffer())
    return;

  if (mHdr->mLength >= mHdr->mCapacity)  // should never be greater than...
    return;

  size_type length = Length();

  if (IsAutoArray() && GetAutoArrayBuffer()->mCapacity >= length) {
    Header* header = GetAutoArrayBuffer();

    // Copy data, but don't copy the header to avoid overwriting mCapacity
    header->mLength = length;
    memcpy(header + 1, mHdr + 1, length * elemSize);

    Alloc::Free(mHdr);
    mHdr = header;
    return;
  }

  if (length == 0) {
    NS_ASSERTION(!IsAutoArray(), "autoarray should have fit 0 elements");
    Alloc::Free(mHdr);
    mHdr = EmptyHdr();
    return;
  }

  size_type size = sizeof(Header) + length * elemSize;
  void *ptr = Alloc::Realloc(mHdr, size);
  if (!ptr)
    return;
  mHdr = static_cast<Header*>(ptr);
  mHdr->mCapacity = length;
}

template<class Alloc>
void
nsTArray_base<Alloc>::ShiftData(index_type start,
                                size_type oldLen, size_type newLen,
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
    char *base = reinterpret_cast<char*>(mHdr + 1) + start;
    memmove(base + newLen, base + oldLen, num);
  }
}

template<class Alloc>
PRBool
nsTArray_base<Alloc>::InsertSlotsAt(index_type index, size_type count,
                                    size_type elementSize)  {
  NS_ASSERTION(index <= Length(), "Bogus insertion index");
  size_type newLen = Length() + count;

  EnsureCapacity(newLen, elementSize);

  // Check for out of memory conditions
  if (Capacity() < newLen)
    return PR_FALSE;

  // Move the existing elements as needed.  Note that this will
  // change our mLength, so no need to call IncrementLength.
  ShiftData(index, 0, count, elementSize);
      
  return PR_TRUE;
}

template<class Alloc>
template<class Allocator>
PRBool
nsTArray_base<Alloc>::SwapArrayElements(nsTArray_base<Allocator>& other,
                                        size_type elemSize) {
#ifdef DEBUG
  PRBool isAuto = IsAutoArray();
  PRBool otherIsAuto = other.IsAutoArray();
#endif

  if (!EnsureNotUsingAutoArrayBuffer(elemSize) ||
      !other.EnsureNotUsingAutoArrayBuffer(elemSize)) {
    return PR_FALSE;
  }

  NS_ASSERTION(isAuto == IsAutoArray(), "lost auto info");
  NS_ASSERTION(otherIsAuto == other.IsAutoArray(), "lost auto info");
  NS_ASSERTION(!UsesAutoArrayBuffer() && !other.UsesAutoArrayBuffer(),
               "both should be using an alloced buffer now");

  // If the two arrays have different mIsAutoArray values (i.e. one is
  // an autoarray and one is not) then simply switching the buffers is
  // going to make that bit wrong. We therefore adjust these
  // mIsAutoArray bits before switching the buffers so that once the
  // buffers are switched the mIsAutoArray bits are right again.
  // However, we have to watch out so that we don't set the bit on
  // sEmptyHeader. If an array (A) uses the empty header (and the
  // other (B) therefore must be an nsAutoTArray) we make A point to
  // the B's autobuffer so that when the buffers are switched B points
  // to its own autobuffer.

  // Adjust mIsAutoArray flags before swapping the buffers
  if (IsAutoArray() && !other.IsAutoArray()) {
    if (other.mHdr == EmptyHdr()) {
      // Set other to use our built-in buffer so that we use it
      // after the swap below.
      other.mHdr = GetAutoArrayBuffer();
      other.mHdr->mLength = 0;
    }
    else {
      other.mHdr->mIsAutoArray = 1;
    }
    mHdr->mIsAutoArray = 0;
  }
  else if (!IsAutoArray() && other.IsAutoArray()) {
    if (mHdr == EmptyHdr()) {
      // Set us to use other's built-in buffer so that other use it
      // after the swap below.
      mHdr = other.GetAutoArrayBuffer();
      mHdr->mLength = 0;
    }
    else {
      mHdr->mIsAutoArray = 1;
    }
    other.mHdr->mIsAutoArray = 0;
  }

  // Swap the buffers
  Header *h = other.mHdr;
  other.mHdr = mHdr;
  mHdr = h;

  NS_ASSERTION(isAuto == IsAutoArray(), "lost auto info");
  NS_ASSERTION(otherIsAuto == other.IsAutoArray(), "lost auto info");

  return PR_TRUE;
}

template<class Alloc>
PRBool
nsTArray_base<Alloc>::EnsureNotUsingAutoArrayBuffer(size_type elemSize) {
  if (UsesAutoArrayBuffer()) {
    size_type size = sizeof(Header) + Length() * elemSize;

    Header* header = static_cast<Header*>(Alloc::Malloc(size));
    if (!header)
      return PR_FALSE;

    memcpy(header, mHdr, size);
    header->mCapacity = Length();
    mHdr = header;
  }
  
  return PR_TRUE;
}
