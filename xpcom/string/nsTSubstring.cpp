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

#include "nsASCIIMask.h"

using double_conversion::DoubleToStringConverter;

template <typename T>
const typename nsTSubstring<T>::size_type nsTSubstring<T>::kMaxCapacity =
    (nsTSubstring<T>::size_type(-1) /
        2 - sizeof(nsStringBuffer)) /
    sizeof(nsTSubstring<T>::char_type) - 2;

#ifdef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
template <typename T>
nsTSubstring<T>::nsTSubstring(char_type* aData, size_type aLength,
                              DataFlags aDataFlags,
                              ClassFlags aClassFlags)
  : ::mozilla::detail::nsTStringRepr<T>(aData, aLength, aDataFlags, aClassFlags)
{
  AssertValid();
  MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "String is too large.");

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
inline const nsTAutoString<T>*
AsAutoString(const nsTSubstring<T>* aStr)
{
  return static_cast<const nsTAutoString<T>*>(aStr);
}

/**
 * this function is called to prepare mData for writing.  the given capacity
 * indicates the required minimum storage size for mData, in sizeof(char_type)
 * increments.  this function returns true if the operation succeeds.  it also
 * returns the old data and old flags members if mData is newly allocated.
 * the old data must be released by the caller.
 */
template <typename T>
bool
nsTSubstring<T>::MutatePrep(size_type aCapacity, char_type** aOldData,
                            DataFlags* aOldDataFlags)
{
  // initialize to no old data
  *aOldData = nullptr;
  *aOldDataFlags = DataFlags(0);

  size_type curCapacity = Capacity();

  // If |aCapacity > kMaxCapacity|, then our doubling algorithm may not be
  // able to allocate it.  Just bail out in cases like that.  We don't want
  // to be allocating 2GB+ strings anyway.
  static_assert((sizeof(nsStringBuffer) & 0x1) == 0,
                "bad size for nsStringBuffer");
  if (!CheckCapacity(aCapacity)) {
      return false;
  }

  // |curCapacity == 0| means that the buffer is immutable or 0-sized, so we
  // need to allocate a new buffer. We cannot use the existing buffer even
  // though it might be large enough.

  if (curCapacity != 0) {
    if (aCapacity <= curCapacity) {
      this->mDataFlags &= ~DataFlags::VOIDED;  // mutation clears voided flag
      return true;
    }
  }

  if (curCapacity < aCapacity) {
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
      size_type minNewCapacity = curCapacity + (curCapacity >> 3); // multiply by 1.125
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

    MOZ_ASSERT(XPCOM_MIN(temp, kMaxCapacity) >= aCapacity,
               "should have hit the early return at the top");
    aCapacity = XPCOM_MIN(temp, kMaxCapacity);
  }

  //
  // several cases:
  //
  //  (1) we have a refcounted shareable buffer (this->mDataFlags &
  //      DataFlags::REFCOUNTED)
  //  (2) we have an owned buffer (this->mDataFlags & DataFlags::OWNED)
  //  (3) we have an inline buffer (this->mDataFlags & DataFlags::INLINE)
  //  (4) we have a readonly buffer
  //
  // requiring that we in some cases preserve the data before creating
  // a new buffer complicates things just a bit ;-)
  //

  size_type storageSize = (aCapacity + 1) * sizeof(char_type);

  // case #1
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    nsStringBuffer* hdr = nsStringBuffer::FromData(this->mData);
    if (!hdr->IsReadonly()) {
      nsStringBuffer* newHdr = nsStringBuffer::Realloc(hdr, storageSize);
      if (!newHdr) {
        return false;  // out-of-memory (original header left intact)
      }

      hdr = newHdr;
      this->mData = (char_type*)hdr->Data();
      this->mDataFlags &= ~DataFlags::VOIDED;  // mutation clears voided flag
      return true;
    }
  }

  char_type* newData;
  DataFlags newDataFlags;

  // If this is an nsTAutoStringN whose inline buffer is sufficiently large,
  // then use it. This helps avoid heap allocations.
  if ((this->mClassFlags & ClassFlags::INLINE) &&
      (aCapacity < AsAutoString(this)->mInlineCapacity)) {
    newData = (char_type*)AsAutoString(this)->mStorage;
    newDataFlags = DataFlags::TERMINATED | DataFlags::INLINE;
  } else {
    // if we reach here then, we must allocate a new buffer.  we cannot
    // make use of our DataFlags::OWNED or DataFlags::INLINE buffers because
    // they are not large enough.

    nsStringBuffer* newHdr =
      nsStringBuffer::Alloc(storageSize).take();
    if (!newHdr) {
      return false;  // we are still in a consistent state
    }

    newData = (char_type*)newHdr->Data();
    newDataFlags = DataFlags::TERMINATED | DataFlags::REFCOUNTED;
  }

  // save old data and flags
  *aOldData = this->mData;
  *aOldDataFlags = this->mDataFlags;

  // this->mLength does not change
  SetData(newData, this->mLength, newDataFlags);

  // though we are not necessarily terminated at the moment, now is probably
  // still the best time to set DataFlags::TERMINATED.

  return true;
}

template <typename T>
void
nsTSubstring<T>::Finalize()
{
  ::ReleaseData(this->mData, this->mDataFlags);
  // this->mData, this->mLength, and this->mDataFlags are purposefully left dangling
}

template <typename T>
bool
nsTSubstring<T>::ReplacePrep(index_type aCutStart,
                             size_type aCutLength,
                             size_type aNewLength)
{
  aCutLength = XPCOM_MIN(aCutLength, this->mLength - aCutStart);

  mozilla::CheckedInt<size_type> newTotalLen = this->mLength;
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
bool
nsTSubstring<T>::ReplacePrepInternal(index_type aCutStart, size_type aCutLen,
                                     size_type aFragLen, size_type aNewLen)
{
  char_type* oldData;
  DataFlags oldFlags;
  if (!MutatePrep(aNewLen, &oldData, &oldFlags)) {
    return false;  // out-of-memory
  }

  if (oldData) {
    // determine whether or not we need to copy part of the old string
    // over to the new string.

    if (aCutStart > 0) {
      // copy prefix from old string
      char_traits::copy(this->mData, oldData, aCutStart);
    }

    if (aCutStart + aCutLen < this->mLength) {
      // copy suffix from old string to new offset
      size_type from = aCutStart + aCutLen;
      size_type fromLen = this->mLength - from;
      uint32_t to = aCutStart + aFragLen;
      char_traits::copy(this->mData + to, oldData + from, fromLen);
    }

    ::ReleaseData(oldData, oldFlags);
  } else {
    // original data remains intact

    // determine whether or not we need to move part of the existing string
    // to make room for the requested hole.
    if (aFragLen != aCutLen && aCutStart + aCutLen < this->mLength) {
      uint32_t from = aCutStart + aCutLen;
      uint32_t fromLen = this->mLength - from;
      uint32_t to = aCutStart + aFragLen;
      char_traits::move(this->mData + to, this->mData + from, fromLen);
    }
  }

  // add null terminator (mutable this->mData always has room for the null-
  // terminator).
  this->mData[aNewLen] = char_type(0);
  this->mLength = aNewLen;

  return true;
}

template <typename T>
typename nsTSubstring<T>::size_type
nsTSubstring<T>::Capacity() const
{
  // return 0 to indicate an immutable or 0-sized buffer

  size_type capacity;
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    // if the string is readonly, then we pretend that it has no capacity.
    nsStringBuffer* hdr = nsStringBuffer::FromData(this->mData);
    if (hdr->IsReadonly()) {
      capacity = 0;
    } else {
      capacity = (hdr->StorageSize() / sizeof(char_type)) - 1;
    }
  } else if (this->mDataFlags & DataFlags::INLINE) {
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
bool
nsTSubstring<T>::EnsureMutable(size_type aNewLen)
{
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
void
nsTSubstring<T>::Assign(char_type aChar)
{
  if (!ReplacePrep(0, this->mLength, 1)) {
    AllocFailed(this->mLength);
  }

  *this->mData = aChar;
}

template <typename T>
bool
nsTSubstring<T>::Assign(char_type aChar, const fallible_t&)
{
  if (!ReplacePrep(0, this->mLength, 1)) {
    return false;
  }

  *this->mData = aChar;
  return true;
}

template <typename T>
void
nsTSubstring<T>::Assign(const char_type* aData)
{
  if (!Assign(aData, mozilla::fallible)) {
    AllocFailed(char_traits::length(aData));
  }
}

template <typename T>
bool
nsTSubstring<T>::Assign(const char_type* aData, const fallible_t&)
{
  return Assign(aData, size_type(-1), mozilla::fallible);
}

template <typename T>
void
nsTSubstring<T>::Assign(const char_type* aData, size_type aLength)
{
  if (!Assign(aData, aLength, mozilla::fallible)) {
    AllocFailed(aLength == size_type(-1) ? char_traits::length(aData)
                                         : aLength);
  }
}

template <typename T>
bool
nsTSubstring<T>::Assign(const char_type* aData, size_type aLength,
                        const fallible_t& aFallible)
{
  if (!aData || aLength == 0) {
    Truncate();
    return true;
  }

  if (aLength == size_type(-1)) {
    aLength = char_traits::length(aData);
  }

  if (this->IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), aFallible);
  }

  if (!ReplacePrep(0, this->mLength, aLength)) {
    return false;
  }

  char_traits::copy(this->mData, aData, aLength);
  return true;
}

template <typename T>
void
nsTSubstring<T>::AssignASCII(const char* aData, size_type aLength)
{
  if (!AssignASCII(aData, aLength, mozilla::fallible)) {
    AllocFailed(aLength);
  }
}

template <typename T>
bool
nsTSubstring<T>::AssignASCII(const char* aData, size_type aLength,
                                const fallible_t& aFallible)
{
  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (this->IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), aFallible);
  }
#endif

  if (!ReplacePrep(0, this->mLength, aLength)) {
    return false;
  }

  char_traits::copyASCII(this->mData, aData, aLength);
  return true;
}

template <typename T>
void
nsTSubstring<T>::AssignLiteral(const char_type* aData, size_type aLength)
{
  ::ReleaseData(this->mData, this->mDataFlags);
  SetData(const_cast<char_type*>(aData), aLength,
          DataFlags::TERMINATED | DataFlags::LITERAL);
}

template <typename T>
void
nsTSubstring<T>::Assign(const self_type& aStr)
{
  if (!Assign(aStr, mozilla::fallible)) {
    AllocFailed(aStr.Length());
  }
}

template <typename T>
bool
nsTSubstring<T>::Assign(const self_type& aStr, const fallible_t& aFallible)
{
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
    NS_ASSERTION(aStr.mDataFlags & DataFlags::TERMINATED, "shared, but not terminated");

    ::ReleaseData(this->mData, this->mDataFlags);

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
void
nsTSubstring<T>::Assign(self_type&& aStr)
{
  if (!Assign(std::move(aStr), mozilla::fallible)) {
    AllocFailed(aStr.Length());
  }
}

template <typename T>
bool
nsTSubstring<T>::Assign(self_type&& aStr, const fallible_t& aFallible)
{
  // We're moving |aStr| in this method, so we need to try to steal the data,
  // and in the fallback perform a copy-assignment followed by a truncation of
  // the original string.

  if (&aStr == this) {
    NS_WARNING("Move assigning a string to itself?");
    return true;
  }

  if (aStr.mDataFlags & (DataFlags::REFCOUNTED | DataFlags::OWNED)) {
    // If they have a REFCOUNTED or OWNED buffer, we can avoid a copy - so steal
    // their buffer and reset them to the empty string.

    // |aStr| should be null-terminated
    NS_ASSERTION(aStr.mDataFlags & DataFlags::TERMINATED,
                 "shared or owned, but not terminated");

    ::ReleaseData(this->mData, this->mDataFlags);

    SetData(aStr.mData, aStr.mLength, aStr.mDataFlags);
    aStr.SetToEmptyBuffer();
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

// NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
template <typename T>
void
nsTSubstring<T>::Assign(const literalstring_type& aStr)
{
  Assign(aStr.AsString());
}

template <typename T>
void
nsTSubstring<T>::Assign(const substring_tuple_type& aTuple)
{
  if (!Assign(aTuple, mozilla::fallible)) {
    AllocFailed(aTuple.Length());
  }
}

template <typename T>
bool
nsTSubstring<T>::Assign(const substring_tuple_type& aTuple,
                        const fallible_t& aFallible)
{
  if (aTuple.IsDependentOn(this->mData, this->mData + this->mLength)) {
    // take advantage of sharing here...
    return Assign(string_type(aTuple), aFallible);
  }

  size_type length = aTuple.Length();

  // don't use ReplacePrep here because it changes the length
  char_type* oldData;
  DataFlags oldFlags;
  if (!MutatePrep(length, &oldData, &oldFlags)) {
    return false;
  }

  if (oldData) {
    ::ReleaseData(oldData, oldFlags);
  }

  aTuple.WriteTo(this->mData, length);
  this->mData[length] = 0;
  this->mLength = length;
  return true;
}

template <typename T>
void
nsTSubstring<T>::Adopt(char_type* aData, size_type aLength)
{
  if (aData) {
    ::ReleaseData(this->mData, this->mDataFlags);

    if (aLength == size_type(-1)) {
      aLength = char_traits::length(aData);
    }

    MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "adopting a too-long string");

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
void
nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                         char_type aChar)
{
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (ReplacePrep(aCutStart, aCutLength, 1)) {
    this->mData[aCutStart] = aChar;
  }
}

template <typename T>
bool
nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                         char_type aChar,
                         const fallible_t&)
{
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (!ReplacePrep(aCutStart, aCutLength, 1)) {
    return false;
  }

  this->mData[aCutStart] = aChar;

  return true;
}

template <typename T>
void
nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                         const char_type* aData, size_type aLength)
{
  if (!Replace(aCutStart, aCutLength, aData, aLength,
               mozilla::fallible)) {
    AllocFailed(this->Length() - aCutLength + 1);
  }
}

template <typename T>
bool
nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                         const char_type* aData, size_type aLength,
                         const fallible_t& aFallible)
{
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
void
nsTSubstring<T>::ReplaceASCII(index_type aCutStart, size_type aCutLength,
                              const char* aData, size_type aLength)
{
  if (!ReplaceASCII(aCutStart, aCutLength, aData, aLength, mozilla::fallible)) {
    AllocFailed(this->Length() - aCutLength + 1);
  }
}

template <typename T>
bool
nsTSubstring<T>::ReplaceASCII(index_type aCutStart, size_type aCutLength,
                              const char* aData, size_type aLength,
                              const fallible_t& aFallible)
{
  if (aLength == size_type(-1)) {
    aLength = strlen(aData);
  }

  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (this->IsDependentOn(aData, aData + aLength)) {
    nsTAutoString_CharT temp(aData, aLength);
    return Replace(aCutStart, aCutLength, temp, aFallible);
  }
#endif

  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  bool ok = ReplacePrep(aCutStart, aCutLength, aLength);
  if (!ok) {
    return false;
  }

  if (aLength > 0) {
    char_traits::copyASCII(this->mData + aCutStart, aData, aLength);
  }

  return true;
}

template <typename T>
void
nsTSubstring<T>::Replace(index_type aCutStart, size_type aCutLength,
                         const substring_tuple_type& aTuple)
{
  if (aTuple.IsDependentOn(this->mData, this->mData + this->mLength)) {
    nsTAutoString<T> temp(aTuple);
    Replace(aCutStart, aCutLength, temp);
    return;
  }

  size_type length = aTuple.Length();

  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (ReplacePrep(aCutStart, aCutLength, length) && length > 0) {
    aTuple.WriteTo(this->mData + aCutStart, length);
  }
}

template <typename T>
void
nsTSubstring<T>::ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                                const char_type* aData, size_type aLength)
{
  aCutStart = XPCOM_MIN(aCutStart, this->Length());

  if (!aCutStart && aCutLength == this->Length()) {
    AssignLiteral(aData, aLength);
  } else if (ReplacePrep(aCutStart, aCutLength, aLength) && aLength > 0) {
    char_traits::copy(this->mData + aCutStart, aData, aLength);
  }
}

template <typename T>
void
nsTSubstring<T>::SetCapacity(size_type aCapacity)
{
  if (!SetCapacity(aCapacity, mozilla::fallible)) {
    AllocFailed(aCapacity);
  }
}

template <typename T>
bool
nsTSubstring<T>::SetCapacity(size_type aCapacity, const fallible_t&)
{
  // capacity does not include room for the terminating null char

  // if our capacity is reduced to zero, then free our buffer.
  if (aCapacity == 0) {
    ::ReleaseData(this->mData, this->mDataFlags);
    SetToEmptyBuffer();
    return true;
  }

  char_type* oldData;
  DataFlags oldFlags;
  if (!MutatePrep(aCapacity, &oldData, &oldFlags)) {
    return false;  // out-of-memory
  }

  // compute new string length
  size_type newLen = XPCOM_MIN(this->mLength, aCapacity);

  if (oldData) {
    // preserve old data
    if (this->mLength > 0) {
      char_traits::copy(this->mData, oldData, newLen);
    }

    ::ReleaseData(oldData, oldFlags);
  }

  // adjust this->mLength if our buffer shrunk down in size
  if (newLen < this->mLength) {
    this->mLength = newLen;
  }

  // always null-terminate here, even if the buffer got longer.  this is
  // for backwards compat with the old string implementation.
  this->mData[aCapacity] = char_type(0);

  return true;
}

template <typename T>
void
nsTSubstring<T>::SetLength(size_type aLength)
{
  SetCapacity(aLength);
  this->mLength = aLength;
}

template <typename T>
bool
nsTSubstring<T>::SetLength(size_type aLength, const fallible_t& aFallible)
{
  if (!SetCapacity(aLength, aFallible)) {
    return false;
  }

  this->mLength = aLength;
  return true;
}

template <typename T>
void
nsTSubstring<T>::SetIsVoid(bool aVal)
{
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
typename nsTStringRepr<T>::char_type
nsTStringRepr<T>::First() const
{
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|First()| called on an empty string");
  return this->mData[0];
}

template <typename T>
typename nsTStringRepr<T>::char_type
nsTStringRepr<T>::Last() const
{
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|Last()| called on an empty string");
  return this->mData[this->mLength - 1];
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const self_type& aStr) const
{
  return this->mLength == aStr.mLength &&
         char_traits::compare(this->mData, aStr.mData, this->mLength) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const self_type& aStr,
                         const comparator_type& aComp) const
{
  return this->mLength == aStr.mLength &&
         aComp(this->mData, aStr.mData, this->mLength, aStr.mLength) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple) const
{
  return Equals(substring_type(aTuple));
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple,
                         const comparator_type& aComp) const
{
  return Equals(substring_type(aTuple), aComp);
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const char_type* aData) const
{
  // unfortunately, some callers pass null :-(
  if (!aData) {
    NS_NOTREACHED("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length &&
         char_traits::compare(this->mData, aData, this->mLength) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::Equals(const char_type* aData,
                         const comparator_type& aComp) const
{
  // unfortunately, some callers pass null :-(
  if (!aData) {
    NS_NOTREACHED("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length && aComp(this->mData, aData, this->mLength, length) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::EqualsASCII(const char* aData, size_type aLen) const
{
  return this->mLength == aLen &&
         char_traits::compareASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::EqualsASCII(const char* aData) const
{
  return char_traits::compareASCIINullTerminated(this->mData, this->mLength, aData) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData,
                                       size_type aLen) const
{
  return this->mLength == aLen &&
         char_traits::compareLowerCaseToASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool
nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData) const
{
  return char_traits::compareLowerCaseToASCIINullTerminated(this->mData,
                                                            this->mLength,
                                                            aData) == 0;
}

template <typename T>
typename nsTStringRepr<T>::size_type
nsTStringRepr<T>::CountChar(char_type aChar) const
{
  const char_type* start = this->mData;
  const char_type* end   = this->mData + this->mLength;

  return NS_COUNT(start, end, aChar);
}

template <typename T>
int32_t
nsTStringRepr<T>::FindChar(char_type aChar, index_type aOffset) const
{
  if (aOffset < this->mLength) {
    const char_type* result = char_traits::find(this->mData + aOffset,
                                                this->mLength - aOffset, aChar);
    if (result) {
      return result - this->mData;
    }
  }
  return -1;
}

} // namespace detail
} // namespace mozilla

template <typename T>
void
nsTSubstring<T>::StripChar(char_type aChar)
{
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    AllocFailed(this->mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = this->mData;
  char_type* from = this->mData;
  char_type* end  = this->mData + this->mLength;

  while (from < end) {
    char_type theChar = *from++;
    if (aChar != theChar) {
      *to++ = theChar;
    }
  }
  *to = char_type(0); // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void
nsTSubstring<T>::StripChars(const char_type* aChars)
{
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    AllocFailed(this->mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = this->mData;
  char_type* from = this->mData;
  char_type* end  = this->mData + this->mLength;

  while (from < end) {
    char_type theChar = *from++;
    const char_type* test = aChars;

    for (; *test && *test != theChar; ++test);

    if (!*test) {
      // Not stripped, copy this char.
      *to++ = theChar;
    }
  }
  *to = char_type(0); // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void
nsTSubstring<T>::StripTaggedASCII(const ASCIIMaskArray& aToStrip)
{
  if (this->mLength == 0) {
    return;
  }

  if (!EnsureMutable()) {
    AllocFailed(this->mLength);
  }

  char_type* to   = this->mData;
  char_type* from = this->mData;
  char_type* end  = this->mData + this->mLength;

  while (from < end) {
    uint32_t theChar = (uint32_t)*from++;
    // Replacing this with a call to ASCIIMask::IsMasked
    // regresses performance somewhat, so leaving it inlined.
    if (!mozilla::ASCIIMask::IsMasked(aToStrip, theChar)) {
      // Not stripped, copy this char.
      *to++ = (char_type)theChar;
    }
  }
  *to = char_type(0); // add the null
  this->mLength = to - this->mData;
}

template <typename T>
void
nsTSubstring<T>::StripCRLF()
{
  // Expanding this call to copy the code from StripTaggedASCII
  // instead of just calling it does somewhat help with performance
  // but it is not worth it given the duplicated code.
  StripTaggedASCII(mozilla::ASCIIMask::MaskCRLF());
}

template <typename T>
struct MOZ_STACK_CLASS PrintfAppend : public mozilla::PrintfTarget
{
  explicit PrintfAppend(nsTSubstring<T>* aString)
    : mString(aString)
  {
  }

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
void
nsTSubstring<T>::AppendPrintf(const char* aFormat, ...)
{
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
void
nsTSubstring<T>::AppendPrintf(const char* aFormat, va_list aAp)
{
  PrintfAppend<T> appender(this);
  bool r = appender.vprint(aFormat, aAp);
  if (!r) {
    MOZ_CRASH("Allocation or other failure in PrintfTarget::print");
  }
}

// Returns the length of the formatted aDouble in aBuf.
static int
FormatWithoutTrailingZeros(char (&aBuf)[40], double aDouble,
                           int aPrecision)
{
  static const DoubleToStringConverter converter(DoubleToStringConverter::UNIQUE_ZERO |
                                                 DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN,
                                                 "Infinity",
                                                 "NaN",
                                                 'e',
                                                 -6, 21,
                                                 6, 1);
  double_conversion::StringBuilder builder(aBuf, sizeof(aBuf));
  bool exponential_notation = false;
  converter.ToPrecision(aDouble, aPrecision, &exponential_notation, &builder);
  int length = builder.position();
  char* formattedDouble = builder.Finalize();

  // If we have a shorter string than aPrecision, it means we have a special
  // value (NaN or Infinity).  All other numbers will be formatted with at
  // least aPrecision digits.
  if (length <= aPrecision) {
    return length;
  }

  char* end = formattedDouble + length;
  char* decimalPoint = strchr(aBuf, '.');
  // No trailing zeros to remove.
  if (!decimalPoint) {
    return length;
  }

  if (MOZ_UNLIKELY(exponential_notation)) {
    // We need to check for cases like 1.00000e-10 (yes, this is
    // disgusting).
    char* exponent = end - 1;
    for (; ; --exponent) {
      if (*exponent == 'e') {
        break;
      }
    }
    char* zerosBeforeExponent = exponent - 1;
    for (; zerosBeforeExponent != decimalPoint; --zerosBeforeExponent) {
      if (*zerosBeforeExponent != '0') {
        break;
      }
    }
    if (zerosBeforeExponent == decimalPoint) {
      --zerosBeforeExponent;
    }
    // Slide the exponent to the left over the trailing zeros.  Don't
    // worry about copying the trailing NUL character.
    size_t exponentSize = end - exponent;
    memmove(zerosBeforeExponent + 1, exponent, exponentSize);
    length -= exponent - (zerosBeforeExponent + 1);
  } else {
    char* trailingZeros = end - 1;
    for (; trailingZeros != decimalPoint; --trailingZeros) {
      if (*trailingZeros != '0') {
        break;
      }
    }
    if (trailingZeros == decimalPoint) {
      --trailingZeros;
    }
    length -= end - (trailingZeros + 1);
  }

  return length;
}

template <typename T>
void
nsTSubstring<T>::AppendFloat(float aFloat)
{
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 6);
  AppendASCII(buf, length);
}

template <typename T>
void
nsTSubstring<T>::AppendFloat(double aFloat)
{
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 15);
  AppendASCII(buf, length);
}

template <typename T>
size_t
nsTSubstring<T>::SizeOfExcludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    return nsStringBuffer::FromData(this->mData)->
      SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }
  if (this->mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(this->mData);
  }

  // If we reach here, exactly one of the following must be true:
  // - DataFlags::VOIDED is set, and this->mData points to sEmptyBuffer;
  // - DataFlags::INLINE is set, and this->mData points to a buffer within a
  //   string object (e.g. nsAutoString);
  // - None of DataFlags::REFCOUNTED, DataFlags::OWNED, DataFlags::INLINE is set,
  //   and this->mData points to a buffer owned by something else.
  //
  // In all three cases, we don't measure it.
  return 0;
}

template <typename T>
size_t
nsTSubstring<T>::SizeOfExcludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  // This is identical to SizeOfExcludingThisIfUnshared except for the
  // DataFlags::REFCOUNTED case.
  if (this->mDataFlags & DataFlags::REFCOUNTED) {
    return nsStringBuffer::FromData(this->mData)->
      SizeOfIncludingThisEvenIfShared(aMallocSizeOf);
  }
  if (this->mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(this->mData);
  }
  return 0;
}

template <typename T>
size_t
nsTSubstring<T>::SizeOfIncludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

template <typename T>
size_t
nsTSubstring<T>::SizeOfIncludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
}

template <typename T>
inline
nsTSubstringSplitter<T>::nsTSubstringSplitter(
    const nsTSubstring<T>* aStr, char_type aDelim)
  : mStr(aStr)
  , mArray(nullptr)
  , mDelim(aDelim)
{
  if (mStr->IsEmpty()) {
    mArraySize = 0;
    return;
  }

  size_type delimCount = mStr->CountChar(aDelim);
  mArraySize = delimCount + 1;
  mArray.reset(new nsTDependentSubstring<T>[mArraySize]);

  size_t seenParts = 0;
  size_type start = 0;
  do {
    MOZ_ASSERT(seenParts < mArraySize);
    int32_t offset = mStr->FindChar(aDelim, start);
    if (offset != -1) {
      size_type length = static_cast<size_type>(offset) - start;
      mArray[seenParts++].Rebind(mStr->Data() + start, length);
      start = static_cast<size_type>(offset) + 1;
    } else {
      // Get the remainder
      mArray[seenParts++].Rebind(mStr->Data() + start, mStr->Length() - start);
      break;
    }
  } while (start < mStr->Length());
}

template <typename T>
nsTSubstringSplitter<T>
nsTSubstring<T>::Split(const char_type aChar) const
{
  return nsTSubstringSplitter<T>(this, aChar);
}

template <typename T>
const nsTDependentSubstring<T>&
nsTSubstringSplitter<T>::nsTSubstringSplit_Iter::operator* () const
{
   return mObj.Get(mPos);
}

// Common logic for nsTSubstring<T>::ToInteger and nsTSubstring<T>::ToInteger64.
template<typename T, typename int_type>
int_type
ToIntegerCommon(const nsTSubstring<T>& aSrc, nsresult* aErrorCode, uint32_t aRadix)
{
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
int32_t
nsTSubstring<T>::ToInteger(nsresult* aErrorCode, uint32_t aRadix) const
{
  return ToIntegerCommon<T, int32_t>(*this, aErrorCode, aRadix);
}


/**
 * nsTSubstring::ToInteger64
 */
template <typename T>
int64_t
nsTSubstring<T>::ToInteger64(nsresult* aErrorCode, uint32_t aRadix) const
{
  return ToIntegerCommon<T, int64_t>(*this, aErrorCode, aRadix);
}
