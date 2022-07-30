/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "double-conversion/double-conversion.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Printf.h"
#include "mozilla/ResultExtensions.h"

#include "nsASCIIMask.h"
#include "nsCharTraits.h"
#include "nsISupports.h"
#include "nsString.h"

#ifdef DEBUG
#  include "nsStringStats.h"
#else
#  define STRING_STAT_INCREMENT(_s)
#endif

// It's not worthwhile to reallocate the buffer and memcpy the
// contents over when the size difference isn't large. With
// power-of-two allocation buckets and 64 as the typical inline
// capacity, considering that above 1000 there performance aspects
// of realloc and memcpy seem to be absorbed, relative to the old
// code, by the performance benefits of the new code being exact,
// we need to choose which transitions of 256 to 128, 512 to 256
// and 1024 to 512 to allow. As a guess, let's pick the middle
// one as the the largest potential transition that we forgo. So
// we'll shrink from 1024 bucket to 512 bucket but not from 512
// bucket to 256 bucket. We'll decide by comparing the difference
// of capacities. As bucket differences, the differences are 256
// and 512. Since the capacities have various overheads, we
// can't compare with 256 or 512 exactly but it's easier to
// compare to some number that's between the two, so it's
// far away from either to ignore the overheads.
const uint32_t kNsStringBufferShrinkingThreshold = 384;

using double_conversion::DoubleToStringConverter;

// ---------------------------------------------------------------------------

static const char16_t gNullChar = 0;

char* const nsCharTraits<char>::sEmptyBuffer =
    (char*)const_cast<char16_t*>(&gNullChar);
char16_t* const nsCharTraits<char16_t>::sEmptyBuffer =
    const_cast<char16_t*>(&gNullChar);

// ---------------------------------------------------------------------------

static void ReleaseData(void* aData, nsAString::DataFlags aFlags) {
  if (aFlags & nsAString::DataFlags::REFCOUNTED) {
    nsStringBuffer::FromData(aData)->Release();
  } else if (aFlags & nsAString::DataFlags::OWNED) {
    free(aData);
    STRING_STAT_INCREMENT(AdoptFree);
    // Treat this as destruction of a "StringAdopt" object for leak
    // tracking purposes.
    MOZ_LOG_DTOR(aData, "StringAdopt", 1);
  }
  // otherwise, nothing to do.
}

// ---------------------------------------------------------------------------

#ifdef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
template <typename T>
nsTSubstring<T>::nsTSubstring(char_type* aData, size_type aLength,
                              DataFlags aDataFlags, ClassFlags aClassFlags)
    : ::mozilla::detail::nsTStringRepr<T>(aData, aLength, aDataFlags,
                                          aClassFlags) {
  AssertValid();

  if (aDataFlags & DataFlags::OWNED) {
    STRING_STAT_INCREMENT(Adopt);
    MOZ_LOG_CTOR(this->mData, "StringAdopt", 1);
  }
}
#endif /* XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE */

/**
 * helper function for down-casting a nsTSubstring to an nsTAutoString.
 */
template <typename T>
inline const nsTAutoString<T>* AsAutoString(const nsTSubstring<T>* aStr) {
  return static_cast<const nsTAutoString<T>*>(aStr);
}

template <typename T>
mozilla::Result<mozilla::BulkWriteHandle<T>, nsresult>
nsTSubstring<T>::BulkWrite(size_type aCapacity, size_type aPrefixToPreserve,
                           bool aAllowShrinking) {
  auto r = StartBulkWriteImpl(aCapacity, aPrefixToPreserve, aAllowShrinking);
  if (MOZ_UNLIKELY(r.isErr())) {
    return r.propagateErr();
  }
  return mozilla::BulkWriteHandle<T>(this, r.unwrap());
}

template <typename T>
auto nsTSubstring<T>::StartBulkWriteImpl(size_type aCapacity,
                                         size_type aPrefixToPreserve,
                                         bool aAllowShrinking,
                                         size_type aSuffixLength,
                                         size_type aOldSuffixStart,
                                         size_type aNewSuffixStart)
    -> mozilla::Result<size_type, nsresult> {
  // Note! Capacity does not include room for the terminating null char.

  MOZ_ASSERT(aPrefixToPreserve <= aCapacity,
             "Requested preservation of an overlong prefix.");
  MOZ_ASSERT(aNewSuffixStart + aSuffixLength <= aCapacity,
             "Requesed move of suffix to out-of-bounds location.");
  // Can't assert aOldSuffixStart, because mLength may not be valid anymore,
  // since this method allows itself to be called more than once.

  // If zero capacity is requested, set the string to the special empty
  // string.
  if (MOZ_UNLIKELY(!aCapacity)) {
    ReleaseData(this->mData, this->mDataFlags);
    SetToEmptyBuffer();
    return 0;
  }

  // Note! Capacity() returns 0 when the string is immutable.
  const size_type curCapacity = Capacity();

  bool shrinking = false;

  // We've established that aCapacity > 0.
  // |curCapacity == 0| means that the buffer is immutable or 0-sized, so we
  // need to allocate a new buffer. We cannot use the existing buffer even
  // though it might be large enough.

  if (aCapacity <= curCapacity) {
    if (aAllowShrinking) {
      shrinking = true;
    } else {
      char_traits::move(this->mData + aNewSuffixStart,
                        this->mData + aOldSuffixStart, aSuffixLength);
      if (aSuffixLength) {
        char_traits::uninitialize(this->mData + aPrefixToPreserve,
                                  XPCOM_MIN(aNewSuffixStart - aPrefixToPreserve,
                                            kNsStringBufferMaxPoison));
        char_traits::uninitialize(
            this->mData + aNewSuffixStart + aSuffixLength,
            XPCOM_MIN(curCapacity + 1 - aNewSuffixStart - aSuffixLength,
                      kNsStringBufferMaxPoison));
      } else {
        char_traits::uninitialize(this->mData + aPrefixToPreserve,
                                  XPCOM_MIN(curCapacity + 1 - aPrefixToPreserve,
                                            kNsStringBufferMaxPoison));
      }
      return curCapacity;
    }
  }

  char_type* oldData = this->mData;
  DataFlags oldFlags = this->mDataFlags;

  char_type* newData;
  DataFlags newDataFlags;
  size_type newCapacity;

  // If this is an nsTAutoStringN, it's possible that we can use the inline
  // buffer.
  if ((this->mClassFlags & ClassFlags::INLINE) &&
      (aCapacity <= AsAutoString(this)->mInlineCapacity)) {
    newCapacity = AsAutoString(this)->mInlineCapacity;
    newData = (char_type*)AsAutoString(this)->mStorage;
    newDataFlags = DataFlags::TERMINATED | DataFlags::INLINE;
  } else {
    // If |aCapacity > kMaxCapacity|, then our doubling algorithm may not be
    // able to allocate it.  Just bail out in cases like that.  We don't want
    // to be allocating 2GB+ strings anyway.
    static_assert((sizeof(nsStringBuffer) & 0x1) == 0,
                  "bad size for nsStringBuffer");
    if (MOZ_UNLIKELY(!this->CheckCapacity(aCapacity))) {
      return mozilla::Err(NS_ERROR_OUT_OF_MEMORY);
    }

    // We increase our capacity so that the allocated buffer grows
    // exponentially, which gives us amortized O(1) appending. Below the
    // threshold, we use powers-of-two. Above the threshold, we grow by at
    // least 1.125, rounding up to the nearest MiB.
    const size_type slowGrowthThreshold = 8 * 1024 * 1024;

    // nsStringBuffer allocates sizeof(nsStringBuffer) + passed size, and
    // storageSize below wants extra 1 * sizeof(char_type).
    const size_type neededExtraSpace =
        sizeof(nsStringBuffer) / sizeof(char_type) + 1;

    size_type temp;
    if (aCapacity >= slowGrowthThreshold) {
      size_type minNewCapacity =
          curCapacity + (curCapacity >> 3);  // multiply by 1.125
      temp = XPCOM_MAX(aCapacity, minNewCapacity) + neededExtraSpace;

      // Round up to the next multiple of MiB, but ensure the expected
      // capacity doesn't include the extra space required by nsStringBuffer
      // and null-termination.
      const size_t MiB = 1 << 20;
      temp = (MiB * ((temp + MiB - 1) / MiB)) - neededExtraSpace;
    } else {
      // Round up to the next power of two.
      temp =
          mozilla::RoundUpPow2(aCapacity + neededExtraSpace) - neededExtraSpace;
    }

    newCapacity = XPCOM_MIN(temp, base_string_type::kMaxCapacity);
    MOZ_ASSERT(newCapacity >= aCapacity,
               "should have hit the early return at the top");
    // Avoid shrinking if the new buffer size is close to the old. Note that
    // unsigned underflow is defined behavior.
    if ((curCapacity - newCapacity) <= kNsStringBufferShrinkingThreshold &&
        (this->mDataFlags & DataFlags::REFCOUNTED)) {
      MOZ_ASSERT(aAllowShrinking, "How come we didn't return earlier?");
      // We're already close enough to the right size.
      newData = oldData;
      newCapacity = curCapacity;
    } else {
      size_type storageSize = (newCapacity + 1) * sizeof(char_type);
      // Since we allocate only by powers of 2 we always fit into a full
      // mozjemalloc bucket, it's not useful to use realloc, which may spend
      // time uselessly copying too much.
      nsStringBuffer* newHdr = nsStringBuffer::Alloc(storageSize).take();
      if (newHdr) {
        newData = (char_type*)newHdr->Data();
      } else if (shrinking) {
        // We're still in a consistent state.
        //
        // Since shrinking is just a memory footprint optimization, we
        // don't propagate OOM if we tried to shrink in order to avoid
        // OOM crashes from infallible callers. If we're lucky, soon enough
        // a fallible caller reaches OOM and is able to deal or we end up
        // disposing of this string before reaching OOM again.
        newData = oldData;
        newCapacity = curCapacity;
      } else {
        return mozilla::Err(NS_ERROR_OUT_OF_MEMORY);
      }
    }
    newDataFlags = DataFlags::TERMINATED | DataFlags::REFCOUNTED;
  }

  this->mData = newData;
  this->mDataFlags = newDataFlags;

  if (oldData == newData) {
    char_traits::move(newData + aNewSuffixStart, oldData + aOldSuffixStart,
                      aSuffixLength);
    if (aSuffixLength) {
      char_traits::uninitialize(this->mData + aPrefixToPreserve,
                                XPCOM_MIN(aNewSuffixStart - aPrefixToPreserve,
                                          kNsStringBufferMaxPoison));
      char_traits::uninitialize(
          this->mData + aNewSuffixStart + aSuffixLength,
          XPCOM_MIN(newCapacity + 1 - aNewSuffixStart - aSuffixLength,
                    kNsStringBufferMaxPoison));
    } else {
      char_traits::uninitialize(this->mData + aPrefixToPreserve,
                                XPCOM_MIN(newCapacity + 1 - aPrefixToPreserve,
                                          kNsStringBufferMaxPoison));
    }
  } else {
    char_traits::copy(newData, oldData, aPrefixToPreserve);
    char_traits::copy(newData + aNewSuffixStart, oldData + aOldSuffixStart,
                      aSuffixLength);
    ReleaseData(oldData, oldFlags);
  }

  return newCapacity;
}

template <typename T>
void nsTSubstring<T>::FinishBulkWriteImpl(size_type aLength) {
  if (aLength) {
    FinishBulkWriteImplImpl(aLength);
  } else {
    ReleaseData(this->mData, this->mDataFlags);
    SetToEmptyBuffer();
  }
  AssertValid();
}

template <typename T>
void nsTSubstring<T>::Finalize() {
  ReleaseData(this->mData, this->mDataFlags);
  // this->mData, this->mLength, and this->mDataFlags are purposefully left
  // dangling
}

template <typename T>
bool nsTSubstring<T>::ReplacePrep(index_type aCutStart, size_type aCutLength,
                                  size_type aNewLength) {
  aCutLength = XPCOM_MIN(aCutLength, this->mLength - aCutStart);

  mozilla::CheckedInt<size_type> newTotalLen = this->Length();
  newTotalLen += aNewLength;
  newTotalLen -= aCutLength;
  if (!newTotalLen.isValid()) {
    return false;
  }

  if (aCutStart == this->mLength && Capacity() > newTotalLen.value()) {
    this->mDataFlags &= ~DataFlags::VOIDED;
    this->mData[newTotalLen.value()] = char_type(0);
    this->mLength = newTotalLen.value();
    return true;
  }

  return ReplacePrepInternal(aCutStart, aCutLength, aNewLength,
                             newTotalLen.value());
}

template <typename T>
bool nsTSubstring<T>::ReplacePrepInternal(index_type aCutStart,
                                          size_type aCutLen, size_type aFragLen,
                                          size_type aNewLen) {
  size_type newSuffixStart = aCutStart + aFragLen;
  size_type oldSuffixStart = aCutStart + aCutLen;
  size_type suffixLength = this->mLength - oldSuffixStart;

  mozilla::Result<size_type, nsresult> r = StartBulkWriteImpl(
      aNewLen, aCutStart, false, suffixLength, oldSuffixStart, newSuffixStart);
  if (r.isErr()) {
    return false;
  }
  FinishBulkWriteImpl(aNewLen);
  return true;
}

template <typename T>
typename nsTSubstring<T>::size_type nsTSubstring<T>::Capacity() const {
  // return 0 to indicate an immutable or 0-sized buffer

  size_type capacity;
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    // if the string is readonly, then we pretend that it has no capacity.
    nsStringBuffer* hdr = nsStringBuffer::FromData(this->mData);
    if (hdr->IsReadonly()) {
      capacity = 0;
    } else {
      capacity = (size_t(hdr->StorageSize()) / sizeof(char_type)) - 1;
    }
  } else if (this->mDataFlags & DataFlags::INLINE) {
    MOZ_ASSERT(this->mClassFlags & ClassFlags::INLINE);
    capacity = AsAutoString(this)->mInlineCapacity;
  } else if (this->mDataFlags & DataFlags::OWNED) {
    // we don't store the capacity of an adopted buffer because that would
    // require an additional member field.  the best we can do is base the
    // capacity on our length.  remains to be seen if this is the right
    // trade-off.
    capacity = this->mLength;
  } else {
    capacity = 0;
  }

  return capacity;
}

template <typename T>
bool nsTSubstring<T>::EnsureMutable(size_type aNewLen) {
  if (aNewLen == size_type(-1) || aNewLen == this->mLength) {
    if (this->mDataFlags & (DataFlags::INLINE | DataFlags::OWNED)) {
      return true;
    }
    if ((this->mDataFlags & DataFlags::REFCOUNTED) &&
        !nsStringBuffer::FromData(this->mData)->IsReadonly()) {
      return true;
    }

    aNewLen = this->mLength;
  }
  return SetLength(aNewLen, mozilla::fallible);
}

// ---------------------------------------------------------------------------

// This version of Assign is optimized for single-character assignment.
template <typename T>
void nsTSubstring<T>::Assign(char_type aChar) {
  if (MOZ_UNLIKELY(!Assign(aChar, mozilla::fallible))) {
    AllocFailed(1);
  }
}

template <typename T>
bool nsTSubstring<T>::Assign(char_type aChar, const fallible_t&) {
  auto r = StartBulkWriteImpl(1, 0, true);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  *this->mData = aChar;
  FinishBulkWriteImpl(1);
  return true;
}

template <typename T>
void nsTSubstring<T>::Assign(const char_type* aData, size_type aLength) {
  if (MOZ_UNLIKELY(!Assign(aData, aLength, mozilla::fallible))) {
    AllocFailed(aLength == size_type(-1) ? char_traits::length(aData)
                                         : aLength);
  }
}

template <typename T>
bool nsTSubstring<T>::Assign(const char_type* aData,
                             const fallible_t& aFallible) {
  return Assign(aData, size_type(-1), aFallible);
}

template <typename T>
bool nsTSubstring<T>::Assign(const char_type* aData, size_type aLength,
                             const fallible_t& aFallible) {
  if (!aData || aLength == 0) {
    Truncate();
    return true;
  }

  if (MOZ_UNLIKELY(aLength == size_type(-1))) {
    aLength = char_traits::length(aData);
  }

  if (MOZ_UNLIKELY(this->IsDependentOn(aData, aData + aLength))) {
    return Assign(string_type(aData, aLength), aFallible);
  }

  auto r = StartBulkWriteImpl(aLength, 0, true);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  char_traits::copy(this->mData, aData, aLength);
  FinishBulkWriteImpl(aLength);
  return true;
}

template <typename T>
void nsTSubstring<T>::AssignASCII(const char* aData, size_type aLength) {
  if (MOZ_UNLIKELY(!AssignASCII(aData, aLength, mozilla::fallible))) {
    AllocFailed(aLength);
  }
}

template <typename T>
bool nsTSubstring<T>::AssignASCII(const char* aData, size_type aLength,
                                  const fallible_t& aFallible) {
  MOZ_ASSERT(aLength != size_type(-1));

  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
  if constexpr (std::is_same_v<T, char>) {
    if (this->IsDependentOn(aData, aData + aLength)) {
      return Assign(string_type(aData, aLength), aFallible);
    }
  }

  auto r = StartBulkWriteImpl(aLength, 0, true);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  char_traits::copyASCII(this->mData, aData, aLength);
  FinishBulkWriteImpl(aLength);
  return true;
}

template <typename T>
void nsTSubstring<T>::AssignLiteral(const char_type* aData, size_type aLength) {
  ReleaseData(this->mData, this->mDataFlags);
  SetData(const_cast<char_type*>(aData), aLength,
          DataFlags::TERMINATED | DataFlags::LITERAL);
}

template <typename T>
void nsTSubstring<T>::Assign(const self_type& aStr) {
  if (!Assign(aStr, mozilla::fallible)) {
    AllocFailed(aStr.Length());
  }
}

template <typename T>
bool nsTSubstring<T>::Assign(const self_type& aStr,
                             const fallible_t& aFallible) {
  // |aStr| could be sharable. We need to check its flags to know how to
  // deal with it.

  if (&aStr == this) {
    return true;
  }

  if (!aStr.mLength) {
    Truncate();
    this->mDataFlags |= aStr.mDataFlags & DataFlags::VOIDED;
    return true;
  }

  if (aStr.mDataFlags & DataFlags::REFCOUNTED) {
    // nice! we can avoid a string copy :-)

    // |aStr| should be null-terminated
    NS_ASSERTION(aStr.mDataFlags & DataFlags::TERMINATED,
                 "shared, but not terminated");

    ReleaseData(this->mData, this->mDataFlags);

    SetData(aStr.mData, aStr.mLength,
            DataFlags::TERMINATED | DataFlags::REFCOUNTED);

    // get an owning reference to the this->mData
    nsStringBuffer::FromData(this->mData)->AddRef();
    return true;
  } else if (aStr.mDataFlags & DataFlags::LITERAL) {
    MOZ_ASSERT(aStr.mDataFlags & DataFlags::TERMINATED, "Unterminated literal");

    AssignLiteral(aStr.mData, aStr.mLength);
    return true;
  }

  // else, treat this like an ordinary assignment.
  return Assign(aStr.Data(), aStr.Length(), aFallible);
}

template <typename T>
void nsTSubstring<T>::Assign(self_type&& aStr) {
  if (!Assign(std::move(aStr), mozilla::fallible)) {
    AllocFailed(aStr.Length());
  }
}

template <typename T>
void nsTSubstring<T>::AssignOwned(self_type&& aStr) {
  MOZ_ASSERT(aStr.mDataFlags & (DataFlags::REFCOUNTED | DataFlags::OWNED),
             "neither shared nor owned");

  // If they have a REFCOUNTED or OWNED buffer, we can avoid a copy - so steal
  // their buffer and reset them to the empty string.

  // |aStr| should be null-terminated
  MOZ_ASSERT(aStr.mDataFlags & DataFlags::TERMINATED,
             "shared or owned, but not terminated");

  ReleaseData(this->mData, this->mDataFlags);

  SetData(aStr.mData, aStr.mLength, aStr.mDataFlags);
  aStr.SetToEmptyBuffer();
}

template <typename T>
bool nsTSubstring<T>::Assign(self_type&& aStr, const fallible_t& aFallible) {
  // We're moving |aStr| in this method, so we need to try to steal the data,
  // and in the fallback perform a copy-assignment followed by a truncation of
  // the original string.

  if (&aStr == this) {
    NS_WARNING("Move assigning a string to itself?");
    return true;
  }

  if (aStr.mDataFlags & (DataFlags::REFCOUNTED | DataFlags::OWNED)) {
    AssignOwned(std::move(aStr));
    return true;
  }

  // Otherwise treat this as a normal assignment, and truncate the moved string.
  // We don't truncate the source string if the allocation failed.
  if (!Assign(aStr, aFallible)) {
    return false;
  }
  aStr.Truncate();
  return true;
}

template <typename T>
void nsTSubstring<T>::Assign(const substring_tuple_type& aTuple) {
  if (!Assign(aTuple, mozilla::fallible)) {
    AllocFailed(aTuple.Length());
  }
}

template <typename T>
bool nsTSubstring<T>::AssignNonDependent(const substring_tuple_type& aTuple,
                                         size_type aTupleLength,
                                         const mozilla::fallible_t& aFallible) {
  NS_ASSERTION(aTuple.Length() == aTupleLength, "wrong length passed");

  auto r = StartBulkWriteImpl(aTupleLength);
  if (r.isErr()) {
    return false;
  }

  aTuple.WriteTo(this->mData, aTupleLength);

  FinishBulkWriteImpl(aTupleLength);
  return true;
}

template <typename T>
bool nsTSubstring<T>::Assign(const substring_tuple_type& aTuple,
                             const fallible_t& aFallible) {
  const auto [isDependentOnThis, tupleLength] =
      aTuple.IsDependentOnWithLength(this->mData, this->mData + this->mLength);
  if (isDependentOnThis) {
    string_type temp;
    self_type& tempSubstring = temp;
    if (!tempSubstring.AssignNonDependent(aTuple, tupleLength, aFallible)) {
      return false;
    }
    AssignOwned(std::move(temp));
    return true;
  }

  return AssignNonDependent(aTuple, tupleLength, aFallible);
}

template <typename T>
void nsTSubstring<T>::Adopt(char_type* aData, size_type aLength) {
  if (aData) {
    ReleaseData(this->mData, this->mDataFlags);

    if (aLength == size_type(-1)) {
      aLength = char_traits::length(aData);
    }

    SetData(aData, aLength, DataFlags::TERMINATED | DataFlags::OWNED);

    STRING_STAT_INCREMENT(Adopt);
    // Treat this as construction of a "StringAdopt" object for leak
    // tracking purposes.
    MOZ_LOG_CTOR(this->mData, "StringAdopt", 1);
  } else {
    SetIsVoid(true);
  }
}

// This version of Replace is optimized for single-character replacement.
template <typename T>
void nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                              char_type aChar) {
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (ReplacePrep(aCutStart, aCutLength, 1)) {
    this->mData[aCutStart] = aChar;
  }
}

template <typename T>
bool nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                              char_type aChar, const fallible_t&) {
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (!ReplacePrep(aCutStart, aCutLength, 1)) {
    return false;
  }

  this->mData[aCutStart] = aChar;

  return true;
}

template <typename T>
void nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                              const char_type* aData, size_type aLength) {
  if (!Replace(aCutStart, aCutLength, aData, aLength, mozilla::fallible)) {
    AllocFailed(this->Length() - aCutLength + 1);
  }
}

template <typename T>
bool nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                              const char_type* aData, size_type aLength,
                              const fallible_t& aFallible) {
  // unfortunately, some callers pass null :-(
  if (!aData) {
    aLength = 0;
  } else {
    if (aLength == size_type(-1)) {
      aLength = char_traits::length(aData);
    }

    if (this->IsDependentOn(aData, aData + aLength)) {
      nsTAutoString<T> temp(aData, aLength);
      return Replace(aCutStart, aCutLength, temp, aFallible);
    }
  }

  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  bool ok = ReplacePrep(aCutStart, aCutLength, aLength);
  if (!ok) {
    return false;
  }

  if (aLength > 0) {
    char_traits::copy(this->mData + aCutStart, aData, aLength);
  }

  return true;
}

template <typename T>
void nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                              const substring_tuple_type& aTuple) {
  const auto [isDependentOnThis, tupleLength] =
      aTuple.IsDependentOnWithLength(this->mData, this->mData + this->mLength);

  if (isDependentOnThis) {
    nsTAutoString<T> temp;
    if (!temp.AssignNonDependent(aTuple, tupleLength, mozilla::fallible)) {
      AllocFailed(tupleLength);
    }
    Replace(aCutStart, aCutLength, temp);
    return;
  }

  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (ReplacePrep(aCutStart, aCutLength, tupleLength) && tupleLength > 0) {
    aTuple.WriteTo(this->mData + aCutStart, tupleLength);
  }
}

template <typename T>
void nsTSubstring<T>::ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                                     const char_type* aData,
                                     size_type aLength) {
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (!aCutStart && aCutLength == this->Length() &&
      !(this->mDataFlags & DataFlags::REFCOUNTED)) {
    // Check for REFCOUNTED above to avoid undoing the effect of
    // SetCapacity().
    AssignLiteral(aData, aLength);
  } else if (ReplacePrep(aCutStart, aCutLength, aLength) && aLength > 0) {
    char_traits::copy(this->mData + aCutStart, aData, aLength);
  }
}

template <typename T>
void nsTSubstring<T>::Append(char_type aChar) {
  if (MOZ_UNLIKELY(!Append(aChar, mozilla::fallible))) {
    AllocFailed(this->mLength + 1);
  }
}

template <typename T>
bool nsTSubstring<T>::Append(char_type aChar, const fallible_t& aFallible) {
  size_type oldLen = this->mLength;
  size_type newLen = oldLen + 1;  // Can't overflow
  auto r = StartBulkWriteImpl(newLen, oldLen, false);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  this->mData[oldLen] = aChar;
  FinishBulkWriteImpl(newLen);
  return true;
}

template <typename T>
void nsTSubstring<T>::Append(const char_type* aData, size_type aLength) {
  if (MOZ_UNLIKELY(!Append(aData, aLength, mozilla::fallible))) {
    AllocFailed(this->mLength + (aLength == size_type(-1)
                                     ? char_traits::length(aData)
                                     : aLength));
  }
}

template <typename T>
bool nsTSubstring<T>::Append(const char_type* aData, size_type aLength,
                             const fallible_t& aFallible) {
  if (MOZ_UNLIKELY(aLength == size_type(-1))) {
    aLength = char_traits::length(aData);
  }

  if (MOZ_UNLIKELY(!aLength)) {
    // Avoid undoing the effect of SetCapacity() if both
    // mLength and aLength are zero.
    return true;
  }

  if (MOZ_UNLIKELY(this->IsDependentOn(aData, aData + aLength))) {
    return Append(string_type(aData, aLength), mozilla::fallible);
  }
  size_type oldLen = this->mLength;
  mozilla::CheckedInt<size_type> newLen(oldLen);
  newLen += aLength;
  if (MOZ_UNLIKELY(!newLen.isValid())) {
    return false;
  }
  auto r = StartBulkWriteImpl(newLen.value(), oldLen, false);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  char_traits::copy(this->mData + oldLen, aData, aLength);
  FinishBulkWriteImpl(newLen.value());
  return true;
}

template <typename T>
void nsTSubstring<T>::AppendASCII(const char* aData, size_type aLength) {
  if (MOZ_UNLIKELY(!AppendASCII(aData, aLength, mozilla::fallible))) {
    AllocFailed(this->mLength +
                (aLength == size_type(-1) ? strlen(aData) : aLength));
  }
}

template <typename T>
bool nsTSubstring<T>::AppendASCII(const char* aData,
                                  const fallible_t& aFallible) {
  return AppendASCII(aData, size_type(-1), aFallible);
}

template <typename T>
bool nsTSubstring<T>::AppendASCII(const char* aData, size_type aLength,
                                  const fallible_t& aFallible) {
  if (MOZ_UNLIKELY(aLength == size_type(-1))) {
    aLength = strlen(aData);
  }

  if (MOZ_UNLIKELY(!aLength)) {
    // Avoid undoing the effect of SetCapacity() if both
    // mLength and aLength are zero.
    return true;
  }

  if constexpr (std::is_same_v<T, char>) {
    // 16-bit string can't depend on an 8-bit buffer
    if (MOZ_UNLIKELY(this->IsDependentOn(aData, aData + aLength))) {
      return Append(string_type(aData, aLength), mozilla::fallible);
    }
  }

  size_type oldLen = this->mLength;
  mozilla::CheckedInt<size_type> newLen(oldLen);
  newLen += aLength;
  if (MOZ_UNLIKELY(!newLen.isValid())) {
    return false;
  }
  auto r = StartBulkWriteImpl(newLen.value(), oldLen, false);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  char_traits::copyASCII(this->mData + oldLen, aData, aLength);
  FinishBulkWriteImpl(newLen.value());
  return true;
}

template <typename T>
void nsTSubstring<T>::Append(const self_type& aStr) {
  if (MOZ_UNLIKELY(!Append(aStr, mozilla::fallible))) {
    AllocFailed(this->mLength + aStr.Length());
  }
}

template <typename T>
bool nsTSubstring<T>::Append(const self_type& aStr,
                             const fallible_t& aFallible) {
  // Check refcounted to avoid undoing the effects of SetCapacity().
  if (MOZ_UNLIKELY(!this->mLength &&
                   !(this->mDataFlags & DataFlags::REFCOUNTED))) {
    return Assign(aStr, mozilla::fallible);
  }
  return Append(aStr.BeginReading(), aStr.Length(), mozilla::fallible);
}

template <typename T>
void nsTSubstring<T>::Append(const substring_tuple_type& aTuple) {
  if (MOZ_UNLIKELY(!Append(aTuple, mozilla::fallible))) {
    AllocFailed(this->mLength + aTuple.Length());
  }
}

template <typename T>
bool nsTSubstring<T>::Append(const substring_tuple_type& aTuple,
                             const fallible_t& aFallible) {
  const auto [isDependentOnThis, tupleLength] =
      aTuple.IsDependentOnWithLength(this->mData, this->mData + this->mLength);

  if (MOZ_UNLIKELY(!tupleLength)) {
    // Avoid undoing the effect of SetCapacity() if both
    // mLength and tupleLength are zero.
    return true;
  }

  if (MOZ_UNLIKELY(isDependentOnThis)) {
    return Append(string_type(aTuple), aFallible);
  }

  size_type oldLen = this->mLength;
  mozilla::CheckedInt<size_type> newLen(oldLen);
  newLen += tupleLength;
  if (MOZ_UNLIKELY(!newLen.isValid())) {
    return false;
  }
  auto r = StartBulkWriteImpl(newLen.value(), oldLen, false);
  if (MOZ_UNLIKELY(r.isErr())) {
    return false;
  }
  aTuple.WriteTo(this->mData + oldLen, tupleLength);
  FinishBulkWriteImpl(newLen.value());
  return true;
}

template <typename T>
void nsTSubstring<T>::SetCapacity(size_type aCapacity) {
  if (!SetCapacity(aCapacity, mozilla::fallible)) {
    AllocFailed(aCapacity);
  }
}

template <typename T>
bool nsTSubstring<T>::SetCapacity(size_type aCapacity, const fallible_t&) {
  size_type length = this->mLength;
  // This method can no longer be used to shorten the
  // logical length.
  size_type capacity = XPCOM_MAX(aCapacity, length);

  auto r = StartBulkWriteImpl(capacity, length, true);
  if (r.isErr()) {
    return false;
  }

  if (MOZ_UNLIKELY(!capacity)) {
    // Zero capacity was requested on a zero-length
    // string. In this special case, we are pointing
    // to the special empty buffer, which is already
    // zero-terminated and not writable, so we must
    // not attempt to zero-terminate it.
    AssertValid();
    return true;
  }

  // FinishBulkWriteImpl with argument zero releases
  // the heap-allocated buffer. However, SetCapacity()
  // is a special case that allows mLength to be zero
  // while a heap-allocated buffer exists.
  // By calling FinishBulkWriteImplImpl, we skip the
  // zero case handling that's inappropriate in the
  // SetCapacity() case.
  FinishBulkWriteImplImpl(length);
  return true;
}

template <typename T>
void nsTSubstring<T>::SetLength(size_type aLength) {
  if (!SetLength(aLength, mozilla::fallible)) {
    AllocFailed(aLength);
  }
}

template <typename T>
bool nsTSubstring<T>::SetLength(size_type aLength,
                                const fallible_t& aFallible) {
  size_type preserve = XPCOM_MIN(aLength, this->Length());
  auto r = StartBulkWriteImpl(aLength, preserve, true);
  if (r.isErr()) {
    return false;
  }

  FinishBulkWriteImpl(aLength);

  return true;
}

template <typename T>
void nsTSubstring<T>::Truncate() {
  ReleaseData(this->mData, this->mDataFlags);
  SetToEmptyBuffer();
  AssertValid();
}

template <typename T>
void nsTSubstring<T>::SetIsVoid(bool aVal) {
  if (aVal) {
    Truncate();
    this->mDataFlags |= DataFlags::VOIDED;
  } else {
    this->mDataFlags &= ~DataFlags::VOIDED;
  }
}

namespace mozilla {
namespace detail {

template <typename T>
typename nsTStringRepr<T>::char_type nsTStringRepr<T>::First() const {
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|First()| called on an empty string");
  return this->mData[0];
}

template <typename T>
typename nsTStringRepr<T>::char_type nsTStringRepr<T>::Last() const {
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|Last()| called on an empty string");
  return this->mData[this->mLength - 1];
}

template <typename T>
bool nsTStringRepr<T>::Equals(const self_type& aStr) const {
  return this->mLength == aStr.mLength &&
         char_traits::compare(this->mData, aStr.mData, this->mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const self_type& aStr,
                              comparator_type aComp) const {
  return this->mLength == aStr.mLength &&
         aComp(this->mData, aStr.mData, this->mLength, aStr.mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple) const {
  return Equals(substring_type(aTuple));
}

template <typename T>
bool nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple,
                              comparator_type aComp) const {
  return Equals(substring_type(aTuple), aComp);
}

template <typename T>
bool nsTStringRepr<T>::Equals(const char_type* aData) const {
  // unfortunately, some callers pass null :-(
  if (!aData) {
    MOZ_ASSERT_UNREACHABLE("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length &&
         char_traits::compare(this->mData, aData, this->mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const char_type* aData,
                              comparator_type aComp) const {
  // unfortunately, some callers pass null :-(
  if (!aData) {
    MOZ_ASSERT_UNREACHABLE("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length &&
         aComp(this->mData, aData, this->mLength, length) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsASCII(const char* aData, size_type aLen) const {
  return this->mLength == aLen &&
         char_traits::compareASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsASCII(const char* aData) const {
  return char_traits::compareASCIINullTerminated(this->mData, this->mLength,
                                                 aData) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsLatin1(const char* aData,
                                    const size_type aLength) const {
  return (this->mLength == aLength) &&
         char_traits::equalsLatin1(this->mData, aData, aLength);
}

template <typename T>
bool nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData,
                                            size_type aLen) const {
  return this->mLength == aLen &&
         char_traits::compareLowerCaseToASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData) const {
  return char_traits::compareLowerCaseToASCIINullTerminated(
             this->mData, this->mLength, aData) == 0;
}

template <typename T>
typename nsTStringRepr<T>::size_type nsTStringRepr<T>::CountChar(
    char_type aChar) const {
  const char_type* start = this->mData;
  const char_type* end = this->mData + this->mLength;

  return NS_COUNT(start, end, aChar);
}

template <typename T>
int32_t nsTStringRepr<T>::FindChar(char_type aChar, index_type aOffset) const {
  if (aOffset < this->mLength) {
    const char_type* result = char_traits::find(this->mData + aOffset,
                                                this->mLength - aOffset, aChar);
    if (result) {
      return result - this->mData;
    }
  }
  return -1;
}

template <typename T>
bool nsTStringRepr<T>::Contains(char_type aChar) const {
  return FindChar(aChar) != kNotFound;
}

}  // namespace detail
}  // namespace mozilla

template <typename T>
void nsTSubstring<T>::StripChar(char_type aChar) {
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) {  // XXX do this lazily?
    AllocFailed(this->mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to = this->mData;
  char_type* from = this->mData;
  char_type* end = this->mData + this->mLength;

  while (from < end) {
    char_type theChar = *from++;
    if (aChar != theChar) {
      *to++ = theChar;
    }
  }
  *to = char_type(0);  // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void nsTSubstring<T>::StripChars(const char_type* aChars) {
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) {  // XXX do this lazily?
    AllocFailed(this->mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to = this->mData;
  char_type* from = this->mData;
  char_type* end = this->mData + this->mLength;

  while (from < end) {
    char_type theChar = *from++;
    const char_type* test = aChars;

    for (; *test && *test != theChar; ++test)
      ;

    if (!*test) {
      // Not stripped, copy this char.
      *to++ = theChar;
    }
  }
  *to = char_type(0);  // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void nsTSubstring<T>::StripTaggedASCII(const ASCIIMaskArray& aToStrip) {
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) {
    AllocFailed(this->mLength);
  }

  char_type* to = this->mData;
  char_type* from = this->mData;
  char_type* end = this->mData + this->mLength;

  while (from < end) {
    uint32_t theChar = (uint32_t)*from++;
    // Replacing this with a call to ASCIIMask::IsMasked
    // regresses performance somewhat, so leaving it inlined.
    if (!mozilla::ASCIIMask::IsMasked(aToStrip, theChar)) {
      // Not stripped, copy this char.
      *to++ = (char_type)theChar;
    }
  }
  *to = char_type(0);  // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void nsTSubstring<T>::StripCRLF() {
  // Expanding this call to copy the code from StripTaggedASCII
  // instead of just calling it does somewhat help with performance
  // but it is not worth it given the duplicated code.
  StripTaggedASCII(mozilla::ASCIIMask::MaskCRLF());
}

template <typename T>
struct MOZ_STACK_CLASS PrintfAppend : public mozilla::PrintfTarget {
  explicit PrintfAppend(nsTSubstring<T>* aString) : mString(aString) {}

  bool append(const char* aStr, size_t aLen) override {
    if (aLen == 0) {
      return true;
    }

    mString->AppendASCII(aStr, aLen);
    return true;
  }

 private:
  nsTSubstring<T>* mString;
};

template <typename T>
void nsTSubstring<T>::AppendPrintf(const char* aFormat, ...) {
  PrintfAppend<T> appender(this);
  va_list ap;
  va_start(ap, aFormat);
  bool r = appender.vprint(aFormat, ap);
  if (!r) {
    MOZ_CRASH("Allocation or other failure in PrintfTarget::print");
  }
  va_end(ap);
}

template <typename T>
void nsTSubstring<T>::AppendVprintf(const char* aFormat, va_list aAp) {
  PrintfAppend<T> appender(this);
  bool r = appender.vprint(aFormat, aAp);
  if (!r) {
    MOZ_CRASH("Allocation or other failure in PrintfTarget::print");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntDec(int32_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntDec(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntDec(uint32_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntDec(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntOct(uint32_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntOct(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntHex(uint32_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntHex(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntDec(int64_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntDec(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntDec(uint64_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntDec(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntOct(uint64_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntOct(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

template <typename T>
void nsTSubstring<T>::AppendIntHex(uint64_t aInteger) {
  PrintfAppend<T> appender(this);
  bool r = appender.appendIntHex(aInteger);
  if (MOZ_UNLIKELY(!r)) {
    MOZ_CRASH("Allocation or other failure while appending integers");
  }
}

// Returns the length of the formatted aDouble in aBuf.
static int FormatWithoutTrailingZeros(char (&aBuf)[40], double aDouble,
                                      int aPrecision) {
  static const DoubleToStringConverter converter(
      DoubleToStringConverter::UNIQUE_ZERO |
          DoubleToStringConverter::NO_TRAILING_ZERO |
          DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN,
      "Infinity", "NaN", 'e', -6, 21, 6, 1);
  double_conversion::StringBuilder builder(aBuf, sizeof(aBuf));
  converter.ToPrecision(aDouble, aPrecision, &builder);
  int length = builder.position();
  builder.Finalize();
  return length;
}

template <typename T>
void nsTSubstring<T>::AppendFloat(float aFloat) {
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 6);
  AppendASCII(buf, length);
}

template <typename T>
void nsTSubstring<T>::AppendFloat(double aFloat) {
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 15);
  AppendASCII(buf, length);
}

template <typename T>
size_t nsTSubstring<T>::SizeOfExcludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    return nsStringBuffer::FromData(this->mData)
        ->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }
  if (this->mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(this->mData);
  }

  // If we reach here, exactly one of the following must be true:
  // - DataFlags::VOIDED is set, and this->mData points to sEmptyBuffer;
  // - DataFlags::INLINE is set, and this->mData points to a buffer within a
  //   string object (e.g. nsAutoString);
  // - None of DataFlags::REFCOUNTED, DataFlags::OWNED, DataFlags::INLINE is
  //   set, and this->mData points to a buffer owned by something else.
  //
  // In all three cases, we don't measure it.
  return 0;
}

template <typename T>
size_t nsTSubstring<T>::SizeOfExcludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // This is identical to SizeOfExcludingThisIfUnshared except for the
  // DataFlags::REFCOUNTED case.
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    return nsStringBuffer::FromData(this->mData)
        ->SizeOfIncludingThisEvenIfShared(aMallocSizeOf);
  }
  if (this->mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(this->mData);
  }
  return 0;
}

template <typename T>
size_t nsTSubstring<T>::SizeOfIncludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

template <typename T>
size_t nsTSubstring<T>::SizeOfIncludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
}

template <typename T>
nsTSubstringSplitter<T> nsTSubstring<T>::Split(const char_type aChar) const {
  return nsTSubstringSplitter<T>(
      nsTCharSeparatedTokenizerTemplate<
          NS_TokenizerIgnoreNothing, T,
          nsTokenizerFlags::IncludeEmptyTokenAtEnd>(*this, aChar));
}

// Common logic for nsTSubstring<T>::ToInteger and nsTSubstring<T>::ToInteger64.
template <typename T, typename int_type>
int_type ToIntegerCommon(const nsTSubstring<T>& aSrc, nsresult* aErrorCode,
                         uint32_t aRadix) {
  MOZ_ASSERT(aRadix == 10 || aRadix == 16);

  // Initial value, override if we find an integer.
  *aErrorCode = NS_ERROR_ILLEGAL_VALUE;

  // Begin by skipping over leading chars that shouldn't be part of the number.
  auto cp = aSrc.BeginReading();
  auto endcp = aSrc.EndReading();
  bool negate = false;
  bool done = false;

  // NB: For backwards compatibility I'm not going to change this logic but
  //     it seems really odd. Previously there was logic to auto-detect the
  //     radix if kAutoDetect was passed in. In practice this value was never
  //     used, so it pretended to auto detect and skipped some preceding
  //     letters (excluding valid hex digits) but never used the result.
  //
  //     For example if you pass in "Get the number: 10", aRadix = 10 we'd
  //     skip the 'G', and then fail to parse "et the number: 10". If aRadix =
  //     16 we'd skip the 'G', and parse just 'e' returning 14.
  while ((cp < endcp) && (!done)) {
    switch (*cp++) {
      // clang-format off
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        done = true;
        break;
      // clang-format on
      case '-':
        negate = true;
        break;
      default:
        break;
    }
  }

  if (!done) {
    // No base 16 or base 10 digits were found.
    return 0;
  }

  // Step back.
  cp--;

  mozilla::CheckedInt<int_type> result;

  // Now iterate the numeric chars and build our result.
  while (cp < endcp) {
    auto theChar = *cp++;
    if (('0' <= theChar) && (theChar <= '9')) {
      result = (aRadix * result) + (theChar - '0');
    } else if ((theChar >= 'A') && (theChar <= 'F')) {
      if (10 == aRadix) {
        // Invalid base 10 digit, error out.
        return 0;
      } else {
        result = (aRadix * result) + ((theChar - 'A') + 10);
      }
    } else if ((theChar >= 'a') && (theChar <= 'f')) {
      if (10 == aRadix) {
        // Invalid base 10 digit, error out.
        return 0;
      } else {
        result = (aRadix * result) + ((theChar - 'a') + 10);
      }
    } else if ((('X' == theChar) || ('x' == theChar)) && result == 0) {
      // For some reason we support a leading 'x' regardless of radix. For
      // example: "000000x500", aRadix = 10 would be parsed as 500 rather
      // than 0.
      continue;
    } else {
      // We've encountered a char that's not a legal number or sign and we can
      // terminate processing.
      break;
    }

    if (!result.isValid()) {
      // Overflow!
      return 0;
    }
  }

  // Integer found.
  *aErrorCode = NS_OK;

  if (negate) {
    result = -result;
  }

  return result.value();
}

template <typename T>
int32_t nsTSubstring<T>::ToInteger(nsresult* aErrorCode,
                                   uint32_t aRadix) const {
  return ToIntegerCommon<T, int32_t>(*this, aErrorCode, aRadix);
}

/**
 * nsTSubstring::ToInteger64
 */
template <typename T>
int64_t nsTSubstring<T>::ToInteger64(nsresult* aErrorCode,
                                     uint32_t aRadix) const {
  return ToIntegerCommon<T, int64_t>(*this, aErrorCode, aRadix);
}
