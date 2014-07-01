/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArray_h__
#  error "Don't include this file directly"
#endif

template<class Alloc, class Copy>
nsTArray_base<Alloc, Copy>::nsTArray_base()
  : mHdr(EmptyHdr())
{
  MOZ_COUNT_CTOR(nsTArray_base);
}

template<class Alloc, class Copy>
nsTArray_base<Alloc, Copy>::~nsTArray_base()
{
  if (mHdr != EmptyHdr() && !UsesAutoArrayBuffer()) {
    Alloc::Free(mHdr);
  }
  MOZ_COUNT_DTOR(nsTArray_base);
}

template<class Alloc, class Copy>
const nsTArrayHeader*
nsTArray_base<Alloc, Copy>::GetAutoArrayBufferUnsafe(size_t aElemAlign) const
{
  // Assuming |this| points to an nsAutoArray, we want to get a pointer to
  // mAutoBuf.  So just cast |this| to nsAutoArray* and read &mAutoBuf!

  const void* autoBuf =
    &reinterpret_cast<const nsAutoArrayBase<nsTArray<uint32_t>, 1>*>(this)->mAutoBuf;

  // If we're on a 32-bit system and aElemAlign is 8, we need to adjust our
  // pointer to take into account the extra alignment in the auto array.

  static_assert(sizeof(void*) != 4 ||
                (MOZ_ALIGNOF(mozilla::AlignedElem<8>) == 8 &&
                 sizeof(nsAutoTArray<mozilla::AlignedElem<8>, 1>) ==
                   sizeof(void*) + sizeof(nsTArrayHeader) +
                   4 + sizeof(mozilla::AlignedElem<8>)),
                "auto array padding wasn't what we expected");

  // We don't support alignments greater than 8 bytes.
  NS_ABORT_IF_FALSE(aElemAlign <= 4 || aElemAlign == 8,
                    "unsupported alignment.");
  if (sizeof(void*) == 4 && aElemAlign == 8) {
    autoBuf = reinterpret_cast<const char*>(autoBuf) + 4;
  }

  return reinterpret_cast<const Header*>(autoBuf);
}

template<class Alloc, class Copy>
bool
nsTArray_base<Alloc, Copy>::UsesAutoArrayBuffer() const
{
  if (!mHdr->mIsAutoArray) {
    return false;
  }

  // This is nuts.  If we were sane, we'd pass aElemAlign as a parameter to
  // this function.  Unfortunately this function is called in nsTArray_base's
  // destructor, at which point we don't know elem_type's alignment.
  //
  // We'll fall on our face and return true when we should say false if
  //
  //   * we're not using our auto buffer,
  //   * aElemAlign == 4, and
  //   * mHdr == GetAutoArrayBuffer(8).
  //
  // This could happen if |*this| lives on the heap and malloc allocated our
  // buffer on the heap adjacent to |*this|.
  //
  // However, we can show that this can't happen.  If |this| is an auto array
  // (as we ensured at the beginning of the method), GetAutoArrayBuffer(8)
  // always points to memory owned by |*this|, because (as we assert below)
  //
  //   * GetAutoArrayBuffer(8) is at most 4 bytes past GetAutoArrayBuffer(4), and
  //   * sizeof(nsTArrayHeader) > 4.
  //
  // Since nsAutoTArray always contains an nsTArrayHeader,
  // GetAutoArrayBuffer(8) will always point inside the auto array object,
  // even if it doesn't point at the beginning of the header.
  //
  // Note that this means that we can't store elements with alignment 16 in an
  // nsTArray, because GetAutoArrayBuffer(16) could lie outside the memory
  // owned by this nsAutoTArray.  We statically assert that elem_type's
  // alignment is 8 bytes or less in nsAutoArrayBase.

  static_assert(sizeof(nsTArrayHeader) > 4,
                "see comment above");

#ifdef DEBUG
  ptrdiff_t diff = reinterpret_cast<const char*>(GetAutoArrayBuffer(8)) -
                   reinterpret_cast<const char*>(GetAutoArrayBuffer(4));
  NS_ABORT_IF_FALSE(diff >= 0 && diff <= 4,
                    "GetAutoArrayBuffer doesn't do what we expect.");
#endif

  return mHdr == GetAutoArrayBuffer(4) || mHdr == GetAutoArrayBuffer(8);
}

// defined in nsTArray.cpp
bool IsTwiceTheRequiredBytesRepresentableAsUint32(size_t aCapacity,
                                                  size_t aElemSize);

template<class Alloc, class Copy>
typename Alloc::ResultTypeProxy
nsTArray_base<Alloc, Copy>::EnsureCapacity(size_type aCapacity,
                                           size_type aElemSize)
{
  // This should be the most common case so test this first
  if (aCapacity <= mHdr->mCapacity) {
    return Alloc::SuccessResult();
  }

  // If the requested memory allocation exceeds size_type(-1)/2, then
  // our doubling algorithm may not be able to allocate it.
  // Additionally, if it exceeds uint32_t(-1) then we couldn't fit in the
  // Header::mCapacity member. Just bail out in cases like that.  We don't want
  // to be allocating 2 GB+ arrays anyway.
  if (!IsTwiceTheRequiredBytesRepresentableAsUint32(aCapacity, aElemSize)) {
    Alloc::SizeTooBig((size_t)aCapacity * aElemSize);
    return Alloc::FailureResult();
  }

  if (mHdr == EmptyHdr()) {
    // Malloc() new data
    Header* header =
      static_cast<Header*>(Alloc::Malloc(sizeof(Header) + aCapacity * aElemSize));
    if (!header) {
      return Alloc::FailureResult();
    }
    header->mLength = 0;
    header->mCapacity = aCapacity;
    header->mIsAutoArray = 0;
    mHdr = header;

    return Alloc::SuccessResult();
  }

  // We increase our capacity so |aCapacity * aElemSize + sizeof(Header)| is the
  // next power of two, if this value is less than pageSize bytes, or otherwise
  // so it's the next multiple of pageSize.
  const size_t pageSizeBytes = 12;
  const size_t pageSize = 1 << pageSizeBytes;

  size_t minBytes = aCapacity * aElemSize + sizeof(Header);
  size_t bytesToAlloc;
  if (minBytes >= pageSize) {
    // Round up to the next multiple of pageSize.
    bytesToAlloc = pageSize * ((minBytes + pageSize - 1) / pageSize);
  } else {
    // Round up to the next power of two.  See
    // http://graphics.stanford.edu/~seander/bithacks.html
    bytesToAlloc = minBytes - 1;
    bytesToAlloc |= bytesToAlloc >> 1;
    bytesToAlloc |= bytesToAlloc >> 2;
    bytesToAlloc |= bytesToAlloc >> 4;
    bytesToAlloc |= bytesToAlloc >> 8;
    bytesToAlloc |= bytesToAlloc >> 16;
    bytesToAlloc++;

    MOZ_ASSERT((bytesToAlloc & (bytesToAlloc - 1)) == 0,
               "nsTArray's allocation size should be a power of two!");
  }

  Header* header;
  if (UsesAutoArrayBuffer() || !Copy::allowRealloc) {
    // Malloc() and copy
    header = static_cast<Header*>(Alloc::Malloc(bytesToAlloc));
    if (!header) {
      return Alloc::FailureResult();
    }

    Copy::CopyHeaderAndElements(header, mHdr, Length(), aElemSize);

    if (!UsesAutoArrayBuffer()) {
      Alloc::Free(mHdr);
    }
  } else {
    // Realloc() existing data
    header = static_cast<Header*>(Alloc::Realloc(mHdr, bytesToAlloc));
    if (!header) {
      return Alloc::FailureResult();
    }
  }

  // How many elements can we fit in bytesToAlloc?
  size_t newCapacity = (bytesToAlloc - sizeof(Header)) / aElemSize;
  MOZ_ASSERT(newCapacity >= aCapacity, "Didn't enlarge the array enough!");
  header->mCapacity = newCapacity;

  mHdr = header;

  return Alloc::SuccessResult();
}

template<class Alloc, class Copy>
void
nsTArray_base<Alloc, Copy>::ShrinkCapacity(size_type aElemSize,
                                           size_t aElemAlign)
{
  if (mHdr == EmptyHdr() || UsesAutoArrayBuffer()) {
    return;
  }

  if (mHdr->mLength >= mHdr->mCapacity) { // should never be greater than...
    return;
  }

  size_type length = Length();

  if (IsAutoArray() && GetAutoArrayBuffer(aElemAlign)->mCapacity >= length) {
    Header* header = GetAutoArrayBuffer(aElemAlign);

    // Copy data, but don't copy the header to avoid overwriting mCapacity
    header->mLength = length;
    Copy::CopyElements(header + 1, mHdr + 1, length, aElemSize);

    Alloc::Free(mHdr);
    mHdr = header;
    return;
  }

  if (length == 0) {
    MOZ_ASSERT(!IsAutoArray(), "autoarray should have fit 0 elements");
    Alloc::Free(mHdr);
    mHdr = EmptyHdr();
    return;
  }

  size_type size = sizeof(Header) + length * aElemSize;
  void* ptr = Alloc::Realloc(mHdr, size);
  if (!ptr) {
    return;
  }
  mHdr = static_cast<Header*>(ptr);
  mHdr->mCapacity = length;
}

template<class Alloc, class Copy>
void
nsTArray_base<Alloc, Copy>::ShiftData(index_type aStart,
                                      size_type aOldLen, size_type aNewLen,
                                      size_type aElemSize, size_t aElemAlign)
{
  if (aOldLen == aNewLen) {
    return;
  }

  // Determine how many elements need to be shifted
  size_type num = mHdr->mLength - (aStart + aOldLen);

  // Compute the resulting length of the array
  mHdr->mLength += aNewLen - aOldLen;
  if (mHdr->mLength == 0) {
    ShrinkCapacity(aElemSize, aElemAlign);
  } else {
    // Maybe nothing needs to be shifted
    if (num == 0) {
      return;
    }
    // Perform shift (change units to bytes first)
    aStart *= aElemSize;
    aNewLen *= aElemSize;
    aOldLen *= aElemSize;
    char* base = reinterpret_cast<char*>(mHdr + 1) + aStart;
    Copy::MoveElements(base + aNewLen, base + aOldLen, num, aElemSize);
  }
}

template<class Alloc, class Copy>
bool
nsTArray_base<Alloc, Copy>::InsertSlotsAt(index_type aIndex, size_type aCount,
                                          size_type aElemSize,
                                          size_t aElemAlign)
{
  MOZ_ASSERT(aIndex <= Length(), "Bogus insertion index");
  size_type newLen = Length() + aCount;

  EnsureCapacity(newLen, aElemSize);

  // Check for out of memory conditions
  if (Capacity() < newLen) {
    return false;
  }

  // Move the existing elements as needed.  Note that this will
  // change our mLength, so no need to call IncrementLength.
  ShiftData(aIndex, 0, aCount, aElemSize, aElemAlign);

  return true;
}

// nsTArray_base::IsAutoArrayRestorer is an RAII class which takes
// |nsTArray_base &array| in its constructor.  When it's destructed, it ensures
// that
//
//   * array.mIsAutoArray has the same value as it did when we started, and
//   * if array has an auto buffer and mHdr would otherwise point to sEmptyHdr,
//     array.mHdr points to array's auto buffer.

template<class Alloc, class Copy>
nsTArray_base<Alloc, Copy>::IsAutoArrayRestorer::IsAutoArrayRestorer(
      nsTArray_base<Alloc, Copy>& aArray,
      size_t aElemAlign)
  : mArray(aArray)
  , mElemAlign(aElemAlign)
  , mIsAuto(aArray.IsAutoArray())
{
}

template<class Alloc, class Copy>
nsTArray_base<Alloc, Copy>::IsAutoArrayRestorer::~IsAutoArrayRestorer()
{
  // Careful: We don't want to set mIsAutoArray = 1 on sEmptyHdr.
  if (mIsAuto && mArray.mHdr == mArray.EmptyHdr()) {
    // Call GetAutoArrayBufferUnsafe() because GetAutoArrayBuffer() asserts
    // that mHdr->mIsAutoArray is true, which surely isn't the case here.
    mArray.mHdr = mArray.GetAutoArrayBufferUnsafe(mElemAlign);
    mArray.mHdr->mLength = 0;
  } else if (mArray.mHdr != mArray.EmptyHdr()) {
    mArray.mHdr->mIsAutoArray = mIsAuto;
  }
}

template<class Alloc, class Copy>
template<class Allocator>
typename Alloc::ResultTypeProxy
nsTArray_base<Alloc, Copy>::SwapArrayElements(nsTArray_base<Allocator,
                                                            Copy>& aOther,
                                              size_type aElemSize,
                                              size_t aElemAlign)
{

  // EnsureNotUsingAutoArrayBuffer will set mHdr = sEmptyHdr even if we have an
  // auto buffer.  We need to point mHdr back to our auto buffer before we
  // return, otherwise we'll forget that we have an auto buffer at all!
  // IsAutoArrayRestorer takes care of this for us.

  IsAutoArrayRestorer ourAutoRestorer(*this, aElemAlign);
  typename nsTArray_base<Allocator, Copy>::IsAutoArrayRestorer
    otherAutoRestorer(aOther, aElemAlign);

  // If neither array uses an auto buffer which is big enough to store the
  // other array's elements, then ensure that both arrays use malloc'ed storage
  // and swap their mHdr pointers.
  if ((!UsesAutoArrayBuffer() || Capacity() < aOther.Length()) &&
      (!aOther.UsesAutoArrayBuffer() || aOther.Capacity() < Length())) {

    if (!EnsureNotUsingAutoArrayBuffer(aElemSize) ||
        !aOther.EnsureNotUsingAutoArrayBuffer(aElemSize)) {
      return Alloc::FailureResult();
    }

    Header* temp = mHdr;
    mHdr = aOther.mHdr;
    aOther.mHdr = temp;

    return Alloc::SuccessResult();
  }

  // Swap the two arrays by copying, since at least one is using an auto
  // buffer which is large enough to hold all of the aOther's elements.  We'll
  // copy the shorter array into temporary storage.
  //
  // (We could do better than this in some circumstances.  Suppose we're
  // swapping arrays X and Y.  X has space for 2 elements in its auto buffer,
  // but currently has length 4, so it's using malloc'ed storage.  Y has length
  // 2.  When we swap X and Y, we don't need to use a temporary buffer; we can
  // write Y straight into X's auto buffer, write X's malloc'ed buffer on top
  // of Y, and then switch X to using its auto buffer.)

  if (!Alloc::Successful(EnsureCapacity(aOther.Length(), aElemSize)) ||
      !Allocator::Successful(aOther.EnsureCapacity(Length(), aElemSize))) {
    return Alloc::FailureResult();
  }

  // The EnsureCapacity calls above shouldn't have caused *both* arrays to
  // switch from their auto buffers to malloc'ed space.
  NS_ABORT_IF_FALSE(UsesAutoArrayBuffer() ||
                    aOther.UsesAutoArrayBuffer(),
                    "One of the arrays should be using its auto buffer.");

  size_type smallerLength = XPCOM_MIN(Length(), aOther.Length());
  size_type largerLength = XPCOM_MAX(Length(), aOther.Length());
  void* smallerElements;
  void* largerElements;
  if (Length() <= aOther.Length()) {
    smallerElements = Hdr() + 1;
    largerElements = aOther.Hdr() + 1;
  } else {
    smallerElements = aOther.Hdr() + 1;
    largerElements = Hdr() + 1;
  }

  // Allocate temporary storage for the smaller of the two arrays.  We want to
  // allocate this space on the stack, if it's not too large.  Sounds like a
  // job for AutoTArray!  (One of the two arrays we're swapping is using an
  // auto buffer, so we're likely not allocating a lot of space here.  But one
  // could, in theory, allocate a huge AutoTArray on the heap.)
  nsAutoArrayBase<nsTArray_Impl<uint8_t, Alloc>, 64> temp;
  if (!Alloc::Successful(temp.EnsureCapacity(smallerLength, aElemSize))) {
    return Alloc::FailureResult();
  }

  Copy::CopyElements(temp.Elements(), smallerElements, smallerLength, aElemSize);
  Copy::CopyElements(smallerElements, largerElements, largerLength, aElemSize);
  Copy::CopyElements(largerElements, temp.Elements(), smallerLength, aElemSize);

  // Swap the arrays' lengths.
  NS_ABORT_IF_FALSE((aOther.Length() == 0 || mHdr != EmptyHdr()) &&
                    (Length() == 0 || aOther.mHdr != EmptyHdr()),
                    "Don't set sEmptyHdr's length.");
  size_type tempLength = Length();
  mHdr->mLength = aOther.Length();
  aOther.mHdr->mLength = tempLength;

  return Alloc::SuccessResult();
}

template<class Alloc, class Copy>
bool
nsTArray_base<Alloc, Copy>::EnsureNotUsingAutoArrayBuffer(size_type aElemSize)
{
  if (UsesAutoArrayBuffer()) {

    // If you call this on a 0-length array, we'll set that array's mHdr to
    // sEmptyHdr, in flagrant violation of the nsAutoTArray invariants.  It's
    // up to you to set it back!  (If you don't, the nsAutoTArray will forget
    // that it has an auto buffer.)
    if (Length() == 0) {
      mHdr = EmptyHdr();
      return true;
    }

    size_type size = sizeof(Header) + Length() * aElemSize;

    Header* header = static_cast<Header*>(Alloc::Malloc(size));
    if (!header) {
      return false;
    }

    Copy::CopyHeaderAndElements(header, mHdr, Length(), aElemSize);
    header->mCapacity = Length();
    mHdr = header;
  }

  return true;
}
