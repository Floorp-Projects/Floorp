/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArray_h__
#  error "Don't include this file directly"
#endif

// NOTE: We don't use MOZ_COUNT_CTOR/MOZ_COUNT_DTOR to perform leak checking of
// nsTArray_base objects intentionally for the following reasons:
// * The leak logging isn't as useful as other types of logging, as
//   nsTArray_base is frequently relocated without invoking a constructor, such
//   as when stored within another nsTArray. This means that
//   XPCOM_MEM_LOG_CLASSES cannot be used to identify specific leaks of nsTArray
//   objects.
// * The nsTArray type is layout compatible with the ThinVec crate with the
//   correct flags, and ThinVec does not currently perform leak logging.
//   This means that if a large number of arrays are transferred between Rust
//   and C++ code using ThinVec, for example within another ThinVec, they
//   will not be logged correctly and might appear as e.g. negative leaks.
// * Leaks which have been found thanks to the leak logging added by this
//   type have often not been significant, and/or have needed to be
//   circumvented using some other mechanism. Most leaks found with this type
//   in them also include other types which will continue to be tracked.

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc, RelocationStrategy>::nsTArray_base() : mHdr(EmptyHdr()) {}

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc, RelocationStrategy>::~nsTArray_base() {
  if (!HasEmptyHeader() && !UsesAutoArrayBuffer()) {
    Alloc::Free(mHdr);
  }
}

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc, RelocationStrategy>::nsTArray_base(const nsTArray_base&)
    : mHdr(EmptyHdr()) {
  // Actual copying happens through nsTArray_CopyEnabler, we just need to do the
  // initialization of mHdr.
}

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc, RelocationStrategy>&
nsTArray_base<Alloc, RelocationStrategy>::operator=(const nsTArray_base&) {
  // Actual copying happens through nsTArray_CopyEnabler, so do nothing here (do
  // not copy mHdr).
  return *this;
}

template <class Alloc, class RelocationStrategy>
const nsTArrayHeader*
nsTArray_base<Alloc, RelocationStrategy>::GetAutoArrayBufferUnsafe(
    size_t aElemAlign) const {
  // Assuming |this| points to an nsAutoArray, we want to get a pointer to
  // mAutoBuf.  So just cast |this| to nsAutoArray* and read &mAutoBuf!

  const void* autoBuf =
      &reinterpret_cast<const AutoTArray<nsTArray<uint32_t>, 1>*>(this)
           ->mAutoBuf;

  // If we're on a 32-bit system and aElemAlign is 8, we need to adjust our
  // pointer to take into account the extra alignment in the auto array.

  static_assert(
      sizeof(void*) != 4 || (MOZ_ALIGNOF(mozilla::AlignedElem<8>) == 8 &&
                             sizeof(AutoTArray<mozilla::AlignedElem<8>, 1>) ==
                                 sizeof(void*) + sizeof(nsTArrayHeader) + 4 +
                                     sizeof(mozilla::AlignedElem<8>)),
      "auto array padding wasn't what we expected");

  // We don't support alignments greater than 8 bytes.
  MOZ_ASSERT(aElemAlign <= 4 || aElemAlign == 8, "unsupported alignment.");
  if (sizeof(void*) == 4 && aElemAlign == 8) {
    autoBuf = reinterpret_cast<const char*>(autoBuf) + 4;
  }

  return reinterpret_cast<const Header*>(autoBuf);
}

template <class Alloc, class RelocationStrategy>
bool nsTArray_base<Alloc, RelocationStrategy>::UsesAutoArrayBuffer() const {
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
  //   * GetAutoArrayBuffer(8) is at most 4 bytes past GetAutoArrayBuffer(4),
  //     and
  //   * sizeof(nsTArrayHeader) > 4.
  //
  // Since AutoTArray always contains an nsTArrayHeader,
  // GetAutoArrayBuffer(8) will always point inside the auto array object,
  // even if it doesn't point at the beginning of the header.
  //
  // Note that this means that we can't store elements with alignment 16 in an
  // nsTArray, because GetAutoArrayBuffer(16) could lie outside the memory
  // owned by this AutoTArray.  We statically assert that elem_type's
  // alignment is 8 bytes or less in AutoTArray.

  static_assert(sizeof(nsTArrayHeader) > 4, "see comment above");

#ifdef DEBUG
  ptrdiff_t diff = reinterpret_cast<const char*>(GetAutoArrayBuffer(8)) -
                   reinterpret_cast<const char*>(GetAutoArrayBuffer(4));
  MOZ_ASSERT(diff >= 0 && diff <= 4,
             "GetAutoArrayBuffer doesn't do what we expect.");
#endif

  return mHdr == GetAutoArrayBuffer(4) || mHdr == GetAutoArrayBuffer(8);
}

// defined in nsTArray.cpp
bool IsTwiceTheRequiredBytesRepresentableAsUint32(size_t aCapacity,
                                                  size_t aElemSize);

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
typename ActualAlloc::ResultTypeProxy
nsTArray_base<Alloc, RelocationStrategy>::ExtendCapacity(size_type aLength,
                                                         size_type aCount,
                                                         size_type aElemSize) {
  mozilla::CheckedInt<size_type> newLength = aLength;
  newLength += aCount;

  if (!newLength.isValid()) {
    return ActualAlloc::FailureResult();
  }

  return this->EnsureCapacity<ActualAlloc>(newLength.value(), aElemSize);
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
typename ActualAlloc::ResultTypeProxy
nsTArray_base<Alloc, RelocationStrategy>::EnsureCapacity(size_type aCapacity,
                                                         size_type aElemSize) {
  // This should be the most common case so test this first
  if (aCapacity <= mHdr->mCapacity) {
    return ActualAlloc::SuccessResult();
  }

  // If the requested memory allocation exceeds size_type(-1)/2, then
  // our doubling algorithm may not be able to allocate it.
  // Additionally, if it exceeds uint32_t(-1) then we couldn't fit in the
  // Header::mCapacity member. Just bail out in cases like that.  We don't want
  // to be allocating 2 GB+ arrays anyway.
  if (!IsTwiceTheRequiredBytesRepresentableAsUint32(aCapacity, aElemSize)) {
    ActualAlloc::SizeTooBig((size_t)aCapacity * aElemSize);
    return ActualAlloc::FailureResult();
  }

  size_t reqSize = sizeof(Header) + aCapacity * aElemSize;

  if (HasEmptyHeader()) {
    // Malloc() new data
    Header* header = static_cast<Header*>(ActualAlloc::Malloc(reqSize));
    if (!header) {
      return ActualAlloc::FailureResult();
    }
    header->mLength = 0;
    header->mCapacity = aCapacity;
    header->mIsAutoArray = 0;
    mHdr = header;

    return ActualAlloc::SuccessResult();
  }

  // We increase our capacity so that the allocated buffer grows exponentially,
  // which gives us amortized O(1) appending. Below the threshold, we use
  // powers-of-two. Above the threshold, we grow by at least 1.125, rounding up
  // to the nearest MiB.
  const size_t slowGrowthThreshold = 8 * 1024 * 1024;

  size_t bytesToAlloc;
  if (reqSize >= slowGrowthThreshold) {
    size_t currSize = sizeof(Header) + Capacity() * aElemSize;
    size_t minNewSize = currSize + (currSize >> 3);  // multiply by 1.125
    bytesToAlloc = reqSize > minNewSize ? reqSize : minNewSize;

    // Round up to the next multiple of MiB.
    const size_t MiB = 1 << 20;
    bytesToAlloc = MiB * ((bytesToAlloc + MiB - 1) / MiB);
  } else {
    // Round up to the next power of two.
    bytesToAlloc = mozilla::RoundUpPow2(reqSize);
  }

  Header* header;
  if (UsesAutoArrayBuffer() || !RelocationStrategy::allowRealloc) {
    // Malloc() and copy
    header = static_cast<Header*>(ActualAlloc::Malloc(bytesToAlloc));
    if (!header) {
      return ActualAlloc::FailureResult();
    }

    RelocationStrategy::RelocateNonOverlappingRegionWithHeader(
        header, mHdr, Length(), aElemSize);

    if (!UsesAutoArrayBuffer()) {
      ActualAlloc::Free(mHdr);
    }
  } else {
    // Realloc() existing data
    header = static_cast<Header*>(ActualAlloc::Realloc(mHdr, bytesToAlloc));
    if (!header) {
      return ActualAlloc::FailureResult();
    }
  }

  // How many elements can we fit in bytesToAlloc?
  size_t newCapacity = (bytesToAlloc - sizeof(Header)) / aElemSize;
  MOZ_ASSERT(newCapacity >= aCapacity, "Didn't enlarge the array enough!");
  header->mCapacity = newCapacity;

  mHdr = header;

  return ActualAlloc::SuccessResult();
}

// We don't need use Alloc template parameter specified here because failure to
// shrink the capacity will leave the array unchanged.
template <class Alloc, class RelocationStrategy>
void nsTArray_base<Alloc, RelocationStrategy>::ShrinkCapacity(
    size_type aElemSize, size_t aElemAlign) {
  if (HasEmptyHeader() || UsesAutoArrayBuffer()) {
    return;
  }

  if (mHdr->mLength >= mHdr->mCapacity) {  // should never be greater than...
    return;
  }

  size_type length = Length();

  if (IsAutoArray() && GetAutoArrayBuffer(aElemAlign)->mCapacity >= length) {
    Header* header = GetAutoArrayBuffer(aElemAlign);

    // Move the data, but don't copy the header to avoid overwriting mCapacity.
    header->mLength = length;
    RelocationStrategy::RelocateNonOverlappingRegion(header + 1, mHdr + 1,
                                                     length, aElemSize);

    nsTArrayFallibleAllocator::Free(mHdr);
    mHdr = header;
    return;
  }

  if (length == 0) {
    MOZ_ASSERT(!IsAutoArray(), "autoarray should have fit 0 elements");
    nsTArrayFallibleAllocator::Free(mHdr);
    mHdr = EmptyHdr();
    return;
  }

  size_type newSize = sizeof(Header) + length * aElemSize;

  Header* newHeader;
  if (!RelocationStrategy::allowRealloc) {
    // Malloc() and copy.
    newHeader =
        static_cast<Header*>(nsTArrayFallibleAllocator::Malloc(newSize));
    if (!newHeader) {
      return;
    }

    RelocationStrategy::RelocateNonOverlappingRegionWithHeader(
        newHeader, mHdr, Length(), aElemSize);

    nsTArrayFallibleAllocator::Free(mHdr);
  } else {
    // Realloc() existing data.
    newHeader =
        static_cast<Header*>(nsTArrayFallibleAllocator::Realloc(mHdr, newSize));
    if (!newHeader) {
      return;
    }
  }

  mHdr = newHeader;
  mHdr->mCapacity = length;
}

template <class Alloc, class RelocationStrategy>
void nsTArray_base<Alloc, RelocationStrategy>::ShrinkCapacityToZero(
    size_type aElemSize, size_t aElemAlign) {
  MOZ_ASSERT(mHdr->mLength == 0);

  if (HasEmptyHeader() || UsesAutoArrayBuffer()) {
    return;
  }

  const bool isAutoArray = IsAutoArray();

  nsTArrayFallibleAllocator::Free(mHdr);

  if (isAutoArray) {
    mHdr = GetAutoArrayBufferUnsafe(aElemAlign);
    mHdr->mLength = 0;
  } else {
    mHdr = EmptyHdr();
  }
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
void nsTArray_base<Alloc, RelocationStrategy>::ShiftData(index_type aStart,
                                                         size_type aOldLen,
                                                         size_type aNewLen,
                                                         size_type aElemSize,
                                                         size_t aElemAlign) {
  if (aOldLen == aNewLen) {
    return;
  }

  // Determine how many elements need to be shifted
  size_type num = mHdr->mLength - (aStart + aOldLen);

  // Compute the resulting length of the array
  mHdr->mLength += aNewLen - aOldLen;
  if (mHdr->mLength == 0) {
    ShrinkCapacityToZero(aElemSize, aElemAlign);
  } else {
    // Maybe nothing needs to be shifted
    if (num == 0) {
      return;
    }
    // Perform shift (change units to bytes first)
    aStart *= aElemSize;
    aNewLen *= aElemSize;
    aOldLen *= aElemSize;
    char* baseAddr = reinterpret_cast<char*>(mHdr + 1) + aStart;
    RelocationStrategy::RelocateOverlappingRegion(
        baseAddr + aNewLen, baseAddr + aOldLen, num, aElemSize);
  }
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
void nsTArray_base<Alloc, RelocationStrategy>::SwapFromEnd(index_type aStart,
                                                           size_type aCount,
                                                           size_type aElemSize,
                                                           size_t aElemAlign) {
  // This method is part of the implementation of
  // nsTArray::SwapRemoveElement{s,}At. For more information, read the
  // documentation on that method.
  if (aCount == 0) {
    return;
  }

  // We are going to be removing aCount elements. Update our length to point to
  // the new end of the array.
  size_type oldLength = mHdr->mLength;
  mHdr->mLength -= aCount;

  if (mHdr->mLength == 0) {
    // If we have no elements remaining in the array, we can free our buffer.
    ShrinkCapacityToZero(aElemSize, aElemAlign);
    return;
  }

  // Determine how many elements we need to move from the end of the array into
  // the now-removed section. This will either be the number of elements which
  // were removed (if there are more elements in the tail of the array), or the
  // entire tail of the array, whichever is smaller.
  size_type relocCount = std::min(aCount, mHdr->mLength - aStart);
  if (relocCount == 0) {
    return;
  }

  // Move the elements which are now stranded after the end of the array back
  // into the now-vacated memory.
  index_type sourceBytes = (oldLength - relocCount) * aElemSize;
  index_type destBytes = aStart * aElemSize;

  // Perform the final copy. This is guaranteed to be a non-overlapping copy
  // as our source contains only still-valid entries, and the destination
  // contains only invalid entries which need to be overwritten.
  MOZ_ASSERT(sourceBytes >= destBytes,
             "The source should be after the destination.");
  MOZ_ASSERT(sourceBytes - destBytes >= relocCount * aElemSize,
             "The range should be nonoverlapping");

  char* baseAddr = reinterpret_cast<char*>(mHdr + 1);
  RelocationStrategy::RelocateNonOverlappingRegion(
      baseAddr + destBytes, baseAddr + sourceBytes, relocCount, aElemSize);
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
typename ActualAlloc::ResultTypeProxy
nsTArray_base<Alloc, RelocationStrategy>::InsertSlotsAt(index_type aIndex,
                                                        size_type aCount,
                                                        size_type aElemSize,
                                                        size_t aElemAlign) {
  if (MOZ_UNLIKELY(aIndex > Length())) {
    InvalidArrayIndex_CRASH(aIndex, Length());
  }

  if (!ActualAlloc::Successful(
          this->ExtendCapacity<ActualAlloc>(Length(), aCount, aElemSize))) {
    return ActualAlloc::FailureResult();
  }

  // Move the existing elements as needed.  Note that this will
  // change our mLength, so no need to call IncrementLength.
  ShiftData<ActualAlloc>(aIndex, 0, aCount, aElemSize, aElemAlign);

  return ActualAlloc::SuccessResult();
}

// nsTArray_base::IsAutoArrayRestorer is an RAII class which takes
// |nsTArray_base &array| in its constructor.  When it's destructed, it ensures
// that
//
//   * array.mIsAutoArray has the same value as it did when we started, and
//   * if array has an auto buffer and mHdr would otherwise point to
//     sEmptyTArrayHeader, array.mHdr points to array's auto buffer.

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc, RelocationStrategy>::IsAutoArrayRestorer::
    IsAutoArrayRestorer(nsTArray_base<Alloc, RelocationStrategy>& aArray,
                        size_t aElemAlign)
    : mArray(aArray), mElemAlign(aElemAlign), mIsAuto(aArray.IsAutoArray()) {}

template <class Alloc, class RelocationStrategy>
nsTArray_base<Alloc,
              RelocationStrategy>::IsAutoArrayRestorer::~IsAutoArrayRestorer() {
  // Careful: We don't want to set mIsAutoArray = 1 on sEmptyTArrayHeader.
  if (mIsAuto && mArray.HasEmptyHeader()) {
    // Call GetAutoArrayBufferUnsafe() because GetAutoArrayBuffer() asserts
    // that mHdr->mIsAutoArray is true, which surely isn't the case here.
    mArray.mHdr = mArray.GetAutoArrayBufferUnsafe(mElemAlign);
    mArray.mHdr->mLength = 0;
  } else if (!mArray.HasEmptyHeader()) {
    mArray.mHdr->mIsAutoArray = mIsAuto;
  }
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc, class Allocator>
typename ActualAlloc::ResultTypeProxy
nsTArray_base<Alloc, RelocationStrategy>::SwapArrayElements(
    nsTArray_base<Allocator, RelocationStrategy>& aOther, size_type aElemSize,
    size_t aElemAlign) {
  // EnsureNotUsingAutoArrayBuffer will set mHdr = sEmptyTArrayHeader even if we
  // have an auto buffer.  We need to point mHdr back to our auto buffer before
  // we return, otherwise we'll forget that we have an auto buffer at all!
  // IsAutoArrayRestorer takes care of this for us.

  IsAutoArrayRestorer ourAutoRestorer(*this, aElemAlign);
  typename nsTArray_base<Allocator, RelocationStrategy>::IsAutoArrayRestorer
      otherAutoRestorer(aOther, aElemAlign);

  // If neither array uses an auto buffer which is big enough to store the
  // other array's elements, then ensure that both arrays use malloc'ed storage
  // and swap their mHdr pointers.
  if ((!UsesAutoArrayBuffer() || Capacity() < aOther.Length()) &&
      (!aOther.UsesAutoArrayBuffer() || aOther.Capacity() < Length())) {
    if (!EnsureNotUsingAutoArrayBuffer<ActualAlloc>(aElemSize) ||
        !aOther.template EnsureNotUsingAutoArrayBuffer<ActualAlloc>(
            aElemSize)) {
      return ActualAlloc::FailureResult();
    }

    Header* temp = mHdr;
    mHdr = aOther.mHdr;
    aOther.mHdr = temp;

    return ActualAlloc::SuccessResult();
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

  if (!ActualAlloc::Successful(
          EnsureCapacity<ActualAlloc>(aOther.Length(), aElemSize)) ||
      !Allocator::Successful(
          aOther.template EnsureCapacity<Allocator>(Length(), aElemSize))) {
    return ActualAlloc::FailureResult();
  }

  // The EnsureCapacity calls above shouldn't have caused *both* arrays to
  // switch from their auto buffers to malloc'ed space.
  MOZ_ASSERT(UsesAutoArrayBuffer() || aOther.UsesAutoArrayBuffer(),
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
  AutoTArray<uint8_t, 64 * sizeof(void*)> temp;
  if (!ActualAlloc::Successful(temp.template EnsureCapacity<ActualAlloc>(
          smallerLength * aElemSize, sizeof(uint8_t)))) {
    return ActualAlloc::FailureResult();
  }

  RelocationStrategy::RelocateNonOverlappingRegion(
      temp.Elements(), smallerElements, smallerLength, aElemSize);
  RelocationStrategy::RelocateNonOverlappingRegion(
      smallerElements, largerElements, largerLength, aElemSize);
  RelocationStrategy::RelocateNonOverlappingRegion(
      largerElements, temp.Elements(), smallerLength, aElemSize);

  // Swap the arrays' lengths.
  MOZ_ASSERT((aOther.Length() == 0 || !HasEmptyHeader()) &&
                 (Length() == 0 || !aOther.HasEmptyHeader()),
             "Don't set sEmptyTArrayHeader's length.");
  size_type tempLength = Length();

  // Avoid writing to EmptyHdr, since it can trigger false
  // positives with TSan.
  if (!HasEmptyHeader()) {
    mHdr->mLength = aOther.Length();
  }
  if (!aOther.HasEmptyHeader()) {
    aOther.mHdr->mLength = tempLength;
  }

  return ActualAlloc::SuccessResult();
}

template <class Alloc, class RelocationStrategy>
template <class Allocator>
void nsTArray_base<Alloc, RelocationStrategy>::MoveInit(
    nsTArray_base<Allocator, RelocationStrategy>& aOther, size_type aElemSize,
    size_t aElemAlign) {
  // This method is similar to SwapArrayElements, but specialized for the case
  // where the target array is empty with no allocated heap storage. It is
  // provided and used to simplify template instantiation and enable better code
  // generation.

  MOZ_ASSERT(Length() == 0);
  MOZ_ASSERT(Capacity() == 0 || (IsAutoArray() && UsesAutoArrayBuffer()));

  // EnsureNotUsingAutoArrayBuffer will set mHdr = sEmptyTArrayHeader even if we
  // have an auto buffer.  We need to point mHdr back to our auto buffer before
  // we return, otherwise we'll forget that we have an auto buffer at all!
  // IsAutoArrayRestorer takes care of this for us.

  IsAutoArrayRestorer ourAutoRestorer(*this, aElemAlign);
  typename nsTArray_base<Allocator, RelocationStrategy>::IsAutoArrayRestorer
      otherAutoRestorer(aOther, aElemAlign);

  // If neither array uses an auto buffer which is big enough to store the
  // other array's elements, then ensure that both arrays use malloc'ed storage
  // and swap their mHdr pointers.
  if ((!IsAutoArray() || Capacity() < aOther.Length()) &&
      !aOther.UsesAutoArrayBuffer()) {
    mHdr = aOther.mHdr;

    aOther.mHdr = EmptyHdr();

    return;
  }

  // Move the data by copying, since at least one has an auto
  // buffer which is large enough to hold all of the aOther's elements.

  EnsureCapacity<nsTArrayInfallibleAllocator>(aOther.Length(), aElemSize);

  // The EnsureCapacity calls above shouldn't have caused *both* arrays to
  // switch from their auto buffers to malloc'ed space.
  MOZ_ASSERT(UsesAutoArrayBuffer() || aOther.UsesAutoArrayBuffer(),
             "One of the arrays should be using its auto buffer.");

  RelocationStrategy::RelocateNonOverlappingRegion(Hdr() + 1, aOther.Hdr() + 1,
                                                   aOther.Length(), aElemSize);

  // Swap the arrays' lengths.
  MOZ_ASSERT((aOther.Length() == 0 || !HasEmptyHeader()) &&
                 (Length() == 0 || !aOther.HasEmptyHeader()),
             "Don't set sEmptyTArrayHeader's length.");

  // Avoid writing to EmptyHdr, since it can trigger false
  // positives with TSan.
  if (!HasEmptyHeader()) {
    mHdr->mLength = aOther.Length();
  }
  if (!aOther.HasEmptyHeader()) {
    aOther.mHdr->mLength = 0;
  }
}

template <class Alloc, class RelocationStrategy>
template <class Allocator>
void nsTArray_base<Alloc, RelocationStrategy>::MoveConstructNonAutoArray(
    nsTArray_base<Allocator, RelocationStrategy>& aOther, size_type aElemSize,
    size_t aElemAlign) {
  // We know that we are not an (Copyable)AutoTArray and we know that we are
  // empty, so don't use SwapArrayElements which doesn't know either of these
  // facts and is very complex.

  if (aOther.IsEmpty()) {
    return;
  }

  // aOther might be an (Copyable)AutoTArray though, and it might use its inline
  // buffer.
  const bool otherUsesAutoArrayBuffer = aOther.UsesAutoArrayBuffer();
  if (otherUsesAutoArrayBuffer) {
    // Use nsTArrayInfallibleAllocator regardless of Alloc because this is
    // called from a move constructor, which cannot report an error to the
    // caller.
    aOther.template EnsureNotUsingAutoArrayBuffer<nsTArrayInfallibleAllocator>(
        aElemSize);
  }

  const bool otherIsAuto = otherUsesAutoArrayBuffer || aOther.IsAutoArray();
  mHdr = aOther.mHdr;
  // We might write to mHdr, so ensure it's not the static empty header. aOther
  // shouldn't have been empty if we get here anyway.
  MOZ_ASSERT(!HasEmptyHeader());

  if (otherIsAuto) {
    mHdr->mIsAutoArray = false;
    aOther.mHdr = aOther.GetAutoArrayBufferUnsafe(aElemAlign);
    aOther.mHdr->mLength = 0;
  } else {
    aOther.mHdr = aOther.EmptyHdr();
  }
}

template <class Alloc, class RelocationStrategy>
template <typename ActualAlloc>
bool nsTArray_base<Alloc, RelocationStrategy>::EnsureNotUsingAutoArrayBuffer(
    size_type aElemSize) {
  if (UsesAutoArrayBuffer()) {
    // If you call this on a 0-length array, we'll set that array's mHdr to
    // sEmptyTArrayHeader, in flagrant violation of the AutoTArray invariants.
    // It's up to you to set it back!  (If you don't, the AutoTArray will
    // forget that it has an auto buffer.)
    if (Length() == 0) {
      mHdr = EmptyHdr();
      return true;
    }

    size_type size = sizeof(Header) + Length() * aElemSize;

    Header* header = static_cast<Header*>(ActualAlloc::Malloc(size));
    if (!header) {
      return false;
    }

    RelocationStrategy::RelocateNonOverlappingRegionWithHeader(
        header, mHdr, Length(), aElemSize);
    header->mCapacity = Length();
    mHdr = header;
  }

  return true;
}
