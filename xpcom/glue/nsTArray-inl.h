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
bool
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

  // We increase our capacity so |capacity * elemSize + sizeof(Header)| is the
  // next power of two, if this value is less than pageSize bytes, or otherwise
  // so it's the next multiple of pageSize.
  const PRUint32 pageSizeBytes = 12;
  const PRUint32 pageSize = 1 << pageSizeBytes;

  PRUint32 minBytes = capacity * elemSize + sizeof(Header);
  PRUint32 bytesToAlloc;
  if (minBytes >= pageSize) {
    // Round up to the next multiple of pageSize.
    bytesToAlloc = pageSize * ((minBytes + pageSize - 1) / pageSize);
  }
  else {
    // Round up to the next power of two.  See
    // http://graphics.stanford.edu/~seander/bithacks.html
    bytesToAlloc = minBytes - 1;
    bytesToAlloc |= bytesToAlloc >> 1;
    bytesToAlloc |= bytesToAlloc >> 2;
    bytesToAlloc |= bytesToAlloc >> 4;
    bytesToAlloc |= bytesToAlloc >> 8;
    bytesToAlloc |= bytesToAlloc >> 16;
    bytesToAlloc++;

    NS_ASSERTION((bytesToAlloc & (bytesToAlloc - 1)) == 0,
                 "nsTArray's allocation size should be a power of two!");
  }

  Header *header;
  if (UsesAutoArrayBuffer()) {
    // Malloc() and copy
    header = static_cast<Header*>(Alloc::Malloc(bytesToAlloc));
    if (!header)
      return PR_FALSE;

    memcpy(header, mHdr, sizeof(Header) + Length() * elemSize);
  } else {
    // Realloc() existing data
    header = static_cast<Header*>(Alloc::Realloc(mHdr, bytesToAlloc));
    if (!header)
      return PR_FALSE;
  }

  // How many elements can we fit in bytesToAlloc?
  PRUint32 newCapacity = (bytesToAlloc - sizeof(Header)) / elemSize;
  NS_ASSERTION(newCapacity >= capacity, "Didn't enlarge the array enough!");
  header->mCapacity = newCapacity;

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
bool
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

// nsTArray_base::IsAutoArrayRestorer is an RAII class which takes
// |nsTArray_base &array| in its constructor.  When it's destructed, it ensures
// that
//
//   * array.mIsAutoArray has the same value as it did when we started, and
//   * if array has an auto buffer and mHdr would otherwise point to sEmptyHdr,
//     array.mHdr points to array's auto buffer.

template<class Alloc>
nsTArray_base<Alloc>::IsAutoArrayRestorer::IsAutoArrayRestorer(
  nsTArray_base<Alloc> &array) 
  : mArray(array),
    mIsAuto(array.IsAutoArray())
{
}

template<class Alloc>
nsTArray_base<Alloc>::IsAutoArrayRestorer::~IsAutoArrayRestorer() {
  // Careful: We don't want to set mIsAutoArray = 1 on sEmptyHdr.
  if (mIsAuto && mArray.mHdr == mArray.EmptyHdr()) {
    // Call GetAutoArrayBufferUnsafe() because GetAutoArrayBuffer() asserts
    // that mHdr->mIsAutoArray is true, which surely isn't the case here.
    mArray.mHdr = mArray.GetAutoArrayBufferUnsafe();
    mArray.mHdr->mLength = 0;
  }
  else {
    mArray.mHdr->mIsAutoArray = mIsAuto;
  }
}

template<class Alloc>
template<class Allocator>
bool
nsTArray_base<Alloc>::SwapArrayElements(nsTArray_base<Allocator>& other,
                                        size_type elemSize) {

  // EnsureNotUsingAutoArrayBuffer will set mHdr = sEmptyHdr even if we have an
  // auto buffer.  We need to point mHdr back to our auto buffer before we
  // return, otherwise we'll forget that we have an auto buffer at all!
  // IsAutoArrayRestorer takes care of this for us.

  IsAutoArrayRestorer ourAutoRestorer(*this);
  typename nsTArray_base<Allocator>::IsAutoArrayRestorer otherAutoRestorer(other);

  // If neither array uses an auto buffer which is big enough to store the
  // other array's elements, then ensure that both arrays use malloc'ed storage
  // and swap their mHdr pointers.
  if ((!UsesAutoArrayBuffer() || Capacity() < other.Length()) &&
      (!other.UsesAutoArrayBuffer() || other.Capacity() < Length())) {

    if (!EnsureNotUsingAutoArrayBuffer(elemSize) ||
        !other.EnsureNotUsingAutoArrayBuffer(elemSize)) {
      return PR_FALSE;
    }

    Header *temp = mHdr;
    mHdr = other.mHdr;
    other.mHdr = temp;

    return PR_TRUE;
  }

  // Swap the two arrays using memcpy, since at least one is using an auto
  // buffer which is large enough to hold all of the other's elements.  We'll
  // copy the shorter array into temporary storage.
  //
  // (We could do better than this in some circumstances.  Suppose we're
  // swapping arrays X and Y.  X has space for 2 elements in its auto buffer,
  // but currently has length 4, so it's using malloc'ed storage.  Y has length
  // 2.  When we swap X and Y, we don't need to use a temporary buffer; we can
  // write Y straight into X's auto buffer, write X's malloc'ed buffer on top
  // of Y, and then switch X to using its auto buffer.)

  if (!EnsureCapacity(other.Length(), elemSize) ||
      !other.EnsureCapacity(Length(), elemSize)) {
    return PR_FALSE;
  }

  // The EnsureCapacity calls above shouldn't have caused *both* arrays to
  // switch from their auto buffers to malloc'ed space.
  NS_ABORT_IF_FALSE(UsesAutoArrayBuffer() || other.UsesAutoArrayBuffer(),
                    "One of the arrays should be using its auto buffer.");

  size_type smallerLength = NS_MIN(Length(), other.Length());
  size_type largerLength = NS_MAX(Length(), other.Length());
  void *smallerElements, *largerElements;
  if (Length() <= other.Length()) {
    smallerElements = Hdr() + 1;
    largerElements = other.Hdr() + 1;
  }
  else {
    smallerElements = other.Hdr() + 1;
    largerElements = Hdr() + 1;
  }

  // Allocate temporary storage for the smaller of the two arrays.  We want to
  // allocate this space on the stack, unless it's very large.  Sounds like a
  // job for AutoTArray!  (One of the two arrays we're swapping is using an
  // auto buffer, so we're likely not allocating a lot of space here.  But one
  // could, in theory, allocate a huge AutoTArray on the heap.)
  nsAutoTArray<PRUint8, 8192, Alloc> temp;
  if (!temp.SetCapacity(smallerLength * elemSize)) {
    return PR_FALSE;
  }

  memcpy(temp.Elements(), smallerElements, smallerLength * elemSize);
  memcpy(smallerElements, largerElements, largerLength * elemSize);
  memcpy(largerElements, temp.Elements(), smallerLength * elemSize);

  // Swap the arrays' lengths.
  NS_ABORT_IF_FALSE((other.Length() == 0 || mHdr != EmptyHdr()) &&
                    (Length() == 0 || other.mHdr != EmptyHdr()),
                    "Don't set sEmptyHdr's length.");
  size_type tempLength = Length();
  mHdr->mLength = other.Length();
  other.mHdr->mLength = tempLength;

  return PR_TRUE;
}

template<class Alloc>
bool
nsTArray_base<Alloc>::EnsureNotUsingAutoArrayBuffer(size_type elemSize) {
  if (UsesAutoArrayBuffer()) {

    // If you call this on a 0-length array, we'll set that array's mHdr to
    // sEmptyHdr, in flagrant violation of the nsAutoTArray invariants.  It's
    // up to you to set it back!  (If you don't, the nsAutoTArray will forget
    // that it has an auto buffer.)
    if (Length() == 0) {
      mHdr = EmptyHdr();
      return PR_TRUE;
    }

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
