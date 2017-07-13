/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsASCIIMask.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/double-conversion.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Printf.h"

using double_conversion::DoubleToStringConverter;

const nsTSubstring_CharT::size_type nsTSubstring_CharT::kMaxCapacity =
    (nsTSubstring_CharT::size_type(-1) /
        2 - sizeof(nsStringBuffer)) /
    sizeof(nsTSubstring_CharT::char_type) - 2;

#ifdef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
nsTSubstring_CharT::nsTSubstring_CharT(char_type* aData, size_type aLength,
                                       DataFlags aDataFlags,
                                       ClassFlags aClassFlags)
  : nsTStringRepr_CharT(aData, aLength, aDataFlags, aClassFlags)
{
  MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "String is too large.");

  if (aDataFlags & DataFlags::OWNED) {
    STRING_STAT_INCREMENT(Adopt);
    MOZ_LOG_CTOR(mData, "StringAdopt", 1);
  }
}
#endif /* XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE */

/**
 * helper function for down-casting a nsTSubstring to a nsTFixedString.
 */
inline const nsTFixedString_CharT*
AsFixedString(const nsTSubstring_CharT* aStr)
{
  return static_cast<const nsTFixedString_CharT*>(aStr);
}

/**
 * this function is called to prepare mData for writing.  the given capacity
 * indicates the required minimum storage size for mData, in sizeof(char_type)
 * increments.  this function returns true if the operation succeeds.  it also
 * returns the old data and old flags members if mData is newly allocated.
 * the old data must be released by the caller.
 */
bool
nsTSubstring_CharT::MutatePrep(size_type aCapacity, char_type** aOldData,
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
      mDataFlags &= ~DataFlags::VOIDED;  // mutation clears voided flag
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
  //  (1) we have a shared buffer (mDataFlags & DataFlags::SHARED)
  //  (2) we have an owned buffer (mDataFlags & DataFlags::OWNED)
  //  (3) we have a fixed buffer (mDataFlags & DataFlags::FIXED)
  //  (4) we have a readonly buffer
  //
  // requiring that we in some cases preserve the data before creating
  // a new buffer complicates things just a bit ;-)
  //

  size_type storageSize = (aCapacity + 1) * sizeof(char_type);

  // case #1
  if (mDataFlags & DataFlags::SHARED) {
    nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
    if (!hdr->IsReadonly()) {
      nsStringBuffer* newHdr = nsStringBuffer::Realloc(hdr, storageSize);
      if (!newHdr) {
        return false;  // out-of-memory (original header left intact)
      }

      hdr = newHdr;
      mData = (char_type*)hdr->Data();
      mDataFlags &= ~DataFlags::VOIDED;  // mutation clears voided flag
      return true;
    }
  }

  char_type* newData;
  DataFlags newDataFlags;

  // if we have a fixed buffer of sufficient size, then use it.  this helps
  // avoid heap allocations.
  if ((mClassFlags & ClassFlags::FIXED) &&
      (aCapacity < AsFixedString(this)->mFixedCapacity)) {
    newData = AsFixedString(this)->mFixedBuf;
    newDataFlags = DataFlags::TERMINATED | DataFlags::FIXED;
  } else {
    // if we reach here then, we must allocate a new buffer.  we cannot
    // make use of our DataFlags::OWNED or DataFlags::FIXED buffers because they are not
    // large enough.

    nsStringBuffer* newHdr =
      nsStringBuffer::Alloc(storageSize).take();
    if (!newHdr) {
      return false;  // we are still in a consistent state
    }

    newData = (char_type*)newHdr->Data();
    newDataFlags = DataFlags::TERMINATED | DataFlags::SHARED;
  }

  // save old data and flags
  *aOldData = mData;
  *aOldDataFlags = mDataFlags;

  mData = newData;
  mDataFlags = newDataFlags;

  // mLength does not change

  // though we are not necessarily terminated at the moment, now is probably
  // still the best time to set DataFlags::TERMINATED.

  return true;
}

void
nsTSubstring_CharT::Finalize()
{
  ::ReleaseData(mData, mDataFlags);
  // mData, mLength, and mDataFlags are purposefully left dangling
}

bool
nsTSubstring_CharT::ReplacePrep(index_type aCutStart,
                                size_type aCutLength,
                                size_type aNewLength)
{
  aCutLength = XPCOM_MIN(aCutLength, mLength - aCutStart);

  mozilla::CheckedInt<size_type> newTotalLen = mLength;
  newTotalLen += aNewLength;
  newTotalLen -= aCutLength;
  if (!newTotalLen.isValid()) {
    return false;
  }

  if (aCutStart == mLength && Capacity() > newTotalLen.value()) {
    mDataFlags &= ~DataFlags::VOIDED;
    mData[newTotalLen.value()] = char_type(0);
    mLength = newTotalLen.value();
    return true;
  }

  return ReplacePrepInternal(aCutStart, aCutLength, aNewLength,
                             newTotalLen.value());
}

bool
nsTSubstring_CharT::ReplacePrepInternal(index_type aCutStart, size_type aCutLen,
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
      char_traits::copy(mData, oldData, aCutStart);
    }

    if (aCutStart + aCutLen < mLength) {
      // copy suffix from old string to new offset
      size_type from = aCutStart + aCutLen;
      size_type fromLen = mLength - from;
      uint32_t to = aCutStart + aFragLen;
      char_traits::copy(mData + to, oldData + from, fromLen);
    }

    ::ReleaseData(oldData, oldFlags);
  } else {
    // original data remains intact

    // determine whether or not we need to move part of the existing string
    // to make room for the requested hole.
    if (aFragLen != aCutLen && aCutStart + aCutLen < mLength) {
      uint32_t from = aCutStart + aCutLen;
      uint32_t fromLen = mLength - from;
      uint32_t to = aCutStart + aFragLen;
      char_traits::move(mData + to, mData + from, fromLen);
    }
  }

  // add null terminator (mutable mData always has room for the null-
  // terminator).
  mData[aNewLen] = char_type(0);
  mLength = aNewLen;

  return true;
}

nsTSubstring_CharT::size_type
nsTSubstring_CharT::Capacity() const
{
  // return 0 to indicate an immutable or 0-sized buffer

  size_type capacity;
  if (mDataFlags & DataFlags::SHARED) {
    // if the string is readonly, then we pretend that it has no capacity.
    nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
    if (hdr->IsReadonly()) {
      capacity = 0;
    } else {
      capacity = (hdr->StorageSize() / sizeof(char_type)) - 1;
    }
  } else if (mDataFlags & DataFlags::FIXED) {
    capacity = AsFixedString(this)->mFixedCapacity;
  } else if (mDataFlags & DataFlags::OWNED) {
    // we don't store the capacity of an adopted buffer because that would
    // require an additional member field.  the best we can do is base the
    // capacity on our length.  remains to be seen if this is the right
    // trade-off.
    capacity = mLength;
  } else {
    capacity = 0;
  }

  return capacity;
}

bool
nsTSubstring_CharT::EnsureMutable(size_type aNewLen)
{
  if (aNewLen == size_type(-1) || aNewLen == mLength) {
    if (mDataFlags & (DataFlags::FIXED | DataFlags::OWNED)) {
      return true;
    }
    if ((mDataFlags & DataFlags::SHARED) &&
        !nsStringBuffer::FromData(mData)->IsReadonly()) {
      return true;
    }

    aNewLen = mLength;
  }
  return SetLength(aNewLen, mozilla::fallible);
}

// ---------------------------------------------------------------------------

// This version of Assign is optimized for single-character assignment.
void
nsTSubstring_CharT::Assign(char_type aChar)
{
  if (!ReplacePrep(0, mLength, 1)) {
    AllocFailed(mLength);
  }

  *mData = aChar;
}

bool
nsTSubstring_CharT::Assign(char_type aChar, const fallible_t&)
{
  if (!ReplacePrep(0, mLength, 1)) {
    return false;
  }

  *mData = aChar;
  return true;
}

void
nsTSubstring_CharT::Assign(const char_type* aData)
{
  if (!Assign(aData, mozilla::fallible)) {
    AllocFailed(char_traits::length(aData));
  }
}

bool
nsTSubstring_CharT::Assign(const char_type* aData, const fallible_t&)
{
  return Assign(aData, size_type(-1), mozilla::fallible);
}

void
nsTSubstring_CharT::Assign(const char_type* aData, size_type aLength)
{
  if (!Assign(aData, aLength, mozilla::fallible)) {
    AllocFailed(aLength == size_type(-1) ? char_traits::length(aData)
                                         : aLength);
  }
}

bool
nsTSubstring_CharT::Assign(const char_type* aData, size_type aLength,
                           const fallible_t& aFallible)
{
  if (!aData || aLength == 0) {
    Truncate();
    return true;
  }

  if (aLength == size_type(-1)) {
    aLength = char_traits::length(aData);
  }

  if (IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), aFallible);
  }

  if (!ReplacePrep(0, mLength, aLength)) {
    return false;
  }

  char_traits::copy(mData, aData, aLength);
  return true;
}

void
nsTSubstring_CharT::AssignASCII(const char* aData, size_type aLength)
{
  if (!AssignASCII(aData, aLength, mozilla::fallible)) {
    AllocFailed(aLength);
  }
}

bool
nsTSubstring_CharT::AssignASCII(const char* aData, size_type aLength,
                                const fallible_t& aFallible)
{
  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), aFallible);
  }
#endif

  if (!ReplacePrep(0, mLength, aLength)) {
    return false;
  }

  char_traits::copyASCII(mData, aData, aLength);
  return true;
}

void
nsTSubstring_CharT::AssignLiteral(const char_type* aData, size_type aLength)
{
  ::ReleaseData(mData, mDataFlags);
  mData = const_cast<char_type*>(aData);
  mLength = aLength;
  mDataFlags = DataFlags::TERMINATED | DataFlags::LITERAL;
}

void
nsTSubstring_CharT::Assign(const self_type& aStr)
{
  if (!Assign(aStr, mozilla::fallible)) {
    AllocFailed(aStr.Length());
  }
}

bool
nsTSubstring_CharT::Assign(const self_type& aStr, const fallible_t& aFallible)
{
  // |aStr| could be sharable. We need to check its flags to know how to
  // deal with it.

  if (&aStr == this) {
    return true;
  }

  if (!aStr.mLength) {
    Truncate();
    mDataFlags |= aStr.mDataFlags & DataFlags::VOIDED;
    return true;
  }

  if (aStr.mDataFlags & DataFlags::SHARED) {
    // nice! we can avoid a string copy :-)

    // |aStr| should be null-terminated
    NS_ASSERTION(aStr.mDataFlags & DataFlags::TERMINATED, "shared, but not terminated");

    ::ReleaseData(mData, mDataFlags);

    mData = aStr.mData;
    mLength = aStr.mLength;
    mDataFlags = DataFlags::TERMINATED | DataFlags::SHARED;

    // get an owning reference to the mData
    nsStringBuffer::FromData(mData)->AddRef();
    return true;
  } else if (aStr.mDataFlags & DataFlags::LITERAL) {
    MOZ_ASSERT(aStr.mDataFlags & DataFlags::TERMINATED, "Unterminated literal");

    AssignLiteral(aStr.mData, aStr.mLength);
    return true;
  }

  // else, treat this like an ordinary assignment.
  return Assign(aStr.Data(), aStr.Length(), aFallible);
}

void
nsTSubstring_CharT::Assign(const substring_tuple_type& aTuple)
{
  if (!Assign(aTuple, mozilla::fallible)) {
    AllocFailed(aTuple.Length());
  }
}

bool
nsTSubstring_CharT::Assign(const substring_tuple_type& aTuple,
                           const fallible_t& aFallible)
{
  if (aTuple.IsDependentOn(mData, mData + mLength)) {
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

  aTuple.WriteTo(mData, length);
  mData[length] = 0;
  mLength = length;
  return true;
}

void
nsTSubstring_CharT::Adopt(char_type* aData, size_type aLength)
{
  if (aData) {
    ::ReleaseData(mData, mDataFlags);

    if (aLength == size_type(-1)) {
      aLength = char_traits::length(aData);
    }

    MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "adopting a too-long string");

    mData = aData;
    mLength = aLength;
    mDataFlags = DataFlags::TERMINATED | DataFlags::OWNED;

    STRING_STAT_INCREMENT(Adopt);
    // Treat this as construction of a "StringAdopt" object for leak
    // tracking purposes.
    MOZ_LOG_CTOR(mData, "StringAdopt", 1);
  } else {
    SetIsVoid(true);
  }
}


// This version of Replace is optimized for single-character replacement.
void
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
                            char_type aChar)
{
  aCutStart = XPCOM_MIN(aCutStart, Length());

  if (ReplacePrep(aCutStart, aCutLength, 1)) {
    mData[aCutStart] = aChar;
  }
}

bool
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
                            char_type aChar,
                            const fallible_t&)
{
  aCutStart = XPCOM_MIN(aCutStart, Length());

  if (!ReplacePrep(aCutStart, aCutLength, 1)) {
    return false;
  }

  mData[aCutStart] = aChar;

  return true;
}

void
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
                            const char_type* aData, size_type aLength)
{
  if (!Replace(aCutStart, aCutLength, aData, aLength,
               mozilla::fallible)) {
    AllocFailed(Length() - aCutLength + 1);
  }
}

bool
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
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

    if (IsDependentOn(aData, aData + aLength)) {
      nsTAutoString_CharT temp(aData, aLength);
      return Replace(aCutStart, aCutLength, temp, aFallible);
    }
  }

  aCutStart = XPCOM_MIN(aCutStart, Length());

  bool ok = ReplacePrep(aCutStart, aCutLength, aLength);
  if (!ok) {
    return false;
  }

  if (aLength > 0) {
    char_traits::copy(mData + aCutStart, aData, aLength);
  }

  return true;
}

void
nsTSubstring_CharT::ReplaceASCII(index_type aCutStart, size_type aCutLength,
                                 const char* aData, size_type aLength)
{
  if (!ReplaceASCII(aCutStart, aCutLength, aData, aLength, mozilla::fallible)) {
    AllocFailed(Length() - aCutLength + 1);
  }
}

bool
nsTSubstring_CharT::ReplaceASCII(index_type aCutStart, size_type aCutLength,
                                 const char* aData, size_type aLength,
                                 const fallible_t& aFallible)
{
  if (aLength == size_type(-1)) {
    aLength = strlen(aData);
  }

  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (IsDependentOn(aData, aData + aLength)) {
    nsTAutoString_CharT temp(aData, aLength);
    return Replace(aCutStart, aCutLength, temp, aFallible);
  }
#endif

  aCutStart = XPCOM_MIN(aCutStart, Length());

  bool ok = ReplacePrep(aCutStart, aCutLength, aLength);
  if (!ok) {
    return false;
  }

  if (aLength > 0) {
    char_traits::copyASCII(mData + aCutStart, aData, aLength);
  }

  return true;
}

void
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
                            const substring_tuple_type& aTuple)
{
  if (aTuple.IsDependentOn(mData, mData + mLength)) {
    nsTAutoString_CharT temp(aTuple);
    Replace(aCutStart, aCutLength, temp);
    return;
  }

  size_type length = aTuple.Length();

  aCutStart = XPCOM_MIN(aCutStart, Length());

  if (ReplacePrep(aCutStart, aCutLength, length) && length > 0) {
    aTuple.WriteTo(mData + aCutStart, length);
  }
}

void
nsTSubstring_CharT::ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                                   const char_type* aData, size_type aLength)
{
  aCutStart = XPCOM_MIN(aCutStart, Length());

  if (!aCutStart && aCutLength == Length()) {
    AssignLiteral(aData, aLength);
  } else if (ReplacePrep(aCutStart, aCutLength, aLength) && aLength > 0) {
    char_traits::copy(mData + aCutStart, aData, aLength);
  }
}

void
nsTSubstring_CharT::SetCapacity(size_type aCapacity)
{
  if (!SetCapacity(aCapacity, mozilla::fallible)) {
    AllocFailed(aCapacity);
  }
}

bool
nsTSubstring_CharT::SetCapacity(size_type aCapacity, const fallible_t&)
{
  // capacity does not include room for the terminating null char

  // if our capacity is reduced to zero, then free our buffer.
  if (aCapacity == 0) {
    ::ReleaseData(mData, mDataFlags);
    mData = char_traits::sEmptyBuffer;
    mLength = 0;
    mDataFlags = DataFlags::TERMINATED;
    return true;
  }

  char_type* oldData;
  DataFlags oldFlags;
  if (!MutatePrep(aCapacity, &oldData, &oldFlags)) {
    return false;  // out-of-memory
  }

  // compute new string length
  size_type newLen = XPCOM_MIN(mLength, aCapacity);

  if (oldData) {
    // preserve old data
    if (mLength > 0) {
      char_traits::copy(mData, oldData, newLen);
    }

    ::ReleaseData(oldData, oldFlags);
  }

  // adjust mLength if our buffer shrunk down in size
  if (newLen < mLength) {
    mLength = newLen;
  }

  // always null-terminate here, even if the buffer got longer.  this is
  // for backwards compat with the old string implementation.
  mData[aCapacity] = char_type(0);

  return true;
}

void
nsTSubstring_CharT::SetLength(size_type aLength)
{
  SetCapacity(aLength);
  mLength = aLength;
}

bool
nsTSubstring_CharT::SetLength(size_type aLength, const fallible_t& aFallible)
{
  if (!SetCapacity(aLength, aFallible)) {
    return false;
  }

  mLength = aLength;
  return true;
}

void
nsTSubstring_CharT::SetIsVoid(bool aVal)
{
  if (aVal) {
    Truncate();
    mDataFlags |= DataFlags::VOIDED;
  } else {
    mDataFlags &= ~DataFlags::VOIDED;
  }
}

namespace mozilla {
namespace detail {

nsTStringRepr_CharT::char_type
nsTStringRepr_CharT::First() const
{
  MOZ_RELEASE_ASSERT(mLength > 0, "|First()| called on an empty string");
  return mData[0];
}

nsTStringRepr_CharT::char_type
nsTStringRepr_CharT::Last() const
{
  MOZ_RELEASE_ASSERT(mLength > 0, "|Last()| called on an empty string");
  return mData[mLength - 1];
}

bool
nsTStringRepr_CharT::Equals(const self_type& aStr) const
{
  return mLength == aStr.mLength &&
         char_traits::compare(mData, aStr.mData, mLength) == 0;
}

bool
nsTStringRepr_CharT::Equals(const self_type& aStr,
                            const comparator_type& aComp) const
{
  return mLength == aStr.mLength &&
         aComp(mData, aStr.mData, mLength, aStr.mLength) == 0;
}

bool
nsTStringRepr_CharT::Equals(const substring_tuple_type& aTuple) const
{
  return Equals(substring_type(aTuple));
}

bool
nsTStringRepr_CharT::Equals(const substring_tuple_type& aTuple,
                            const comparator_type& aComp) const
{
  return Equals(substring_type(aTuple), aComp);
}

bool
nsTStringRepr_CharT::Equals(const char_type* aData) const
{
  // unfortunately, some callers pass null :-(
  if (!aData) {
    NS_NOTREACHED("null data pointer");
    return mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return mLength == length &&
         char_traits::compare(mData, aData, mLength) == 0;
}

bool
nsTStringRepr_CharT::Equals(const char_type* aData,
                            const comparator_type& aComp) const
{
  // unfortunately, some callers pass null :-(
  if (!aData) {
    NS_NOTREACHED("null data pointer");
    return mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return mLength == length && aComp(mData, aData, mLength, length) == 0;
}

bool
nsTStringRepr_CharT::EqualsASCII(const char* aData, size_type aLen) const
{
  return mLength == aLen &&
         char_traits::compareASCII(mData, aData, aLen) == 0;
}

bool
nsTStringRepr_CharT::EqualsASCII(const char* aData) const
{
  return char_traits::compareASCIINullTerminated(mData, mLength, aData) == 0;
}

bool
nsTStringRepr_CharT::LowerCaseEqualsASCII(const char* aData,
                                          size_type aLen) const
{
  return mLength == aLen &&
         char_traits::compareLowerCaseToASCII(mData, aData, aLen) == 0;
}

bool
nsTStringRepr_CharT::LowerCaseEqualsASCII(const char* aData) const
{
  return char_traits::compareLowerCaseToASCIINullTerminated(mData,
                                                            mLength,
                                                            aData) == 0;
}

nsTStringRepr_CharT::size_type
nsTStringRepr_CharT::CountChar(char_type aChar) const
{
  const char_type* start = mData;
  const char_type* end   = mData + mLength;

  return NS_COUNT(start, end, aChar);
}

int32_t
nsTStringRepr_CharT::FindChar(char_type aChar, index_type aOffset) const
{
  if (aOffset < mLength) {
    const char_type* result = char_traits::find(mData + aOffset,
                                                mLength - aOffset, aChar);
    if (result) {
      return result - mData;
    }
  }
  return -1;
}

} // namespace detail
} // namespace mozilla

void
nsTSubstring_CharT::StripChar(char_type aChar)
{
  if (mLength == 0) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    AllocFailed(mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = mData;
  char_type* from = mData;
  char_type* end  = mData + mLength;

  while (from < end) {
    char_type theChar = *from++;
    if (aChar != theChar) {
      *to++ = theChar;
    }
  }
  *to = char_type(0); // add the null
  mLength = to - mData;
}

void
nsTSubstring_CharT::StripChars(const char_type* aChars)
{
  if (mLength == 0) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    AllocFailed(mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = mData;
  char_type* from = mData;
  char_type* end  = mData + mLength;

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
  mLength = to - mData;
}

void
nsTSubstring_CharT::StripTaggedASCII(const ASCIIMaskArray& aToStrip)
{
  if (mLength == 0) {
    return;
  }

  if (!EnsureMutable()) {
    AllocFailed(mLength);
  }

  char_type* to   = mData;
  char_type* from = mData;
  char_type* end  = mData + mLength;

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
  mLength = to - mData;
}

void
nsTSubstring_CharT::StripCRLF()
{
  // Expanding this call to copy the code from StripTaggedASCII
  // instead of just calling it does somewhat help with performance
  // but it is not worth it given the duplicated code.
  StripTaggedASCII(mozilla::ASCIIMask::MaskCRLF());
}

struct MOZ_STACK_CLASS PrintfAppend_CharT : public mozilla::PrintfTarget
{
  explicit PrintfAppend_CharT(nsTSubstring_CharT* aString)
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

  nsTSubstring_CharT* mString;
};

void
nsTSubstring_CharT::AppendPrintf(const char* aFormat, ...)
{
  PrintfAppend_CharT appender(this);
  va_list ap;
  va_start(ap, aFormat);
  bool r = appender.vprint(aFormat, ap);
  if (!r) {
    MOZ_CRASH("Allocation or other failure in PrintfTarget::print");
  }
  va_end(ap);
}

void
nsTSubstring_CharT::AppendPrintf(const char* aFormat, va_list aAp)
{
  PrintfAppend_CharT appender(this);
  bool r = appender.vprint(aFormat, aAp);
  if (!r) {
    MOZ_CRASH("Allocation or other failure in PrintfTarget::print");
  }
}

/* hack to make sure we define FormatWithoutTrailingZeros only once */
#ifdef CharT_is_PRUnichar
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
#endif /* CharT_is_PRUnichar */

void
nsTSubstring_CharT::AppendFloat(float aFloat)
{
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 6);
  AppendASCII(buf, length);
}

void
nsTSubstring_CharT::AppendFloat(double aFloat)
{
  char buf[40];
  int length = FormatWithoutTrailingZeros(buf, aFloat, 15);
  AppendASCII(buf, length);
}

size_t
nsTSubstring_CharT::SizeOfExcludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  if (mDataFlags & DataFlags::SHARED) {
    return nsStringBuffer::FromData(mData)->
      SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }
  if (mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(mData);
  }

  // If we reach here, exactly one of the following must be true:
  // - DataFlags::VOIDED is set, and mData points to sEmptyBuffer;
  // - DataFlags::FIXED is set, and mData points to a buffer within a string
  //   object (e.g. nsAutoString);
  // - None of DataFlags::SHARED, DataFlags::OWNED, DataFlags::FIXED is set, and mData points to a buffer
  //   owned by something else.
  //
  // In all three cases, we don't measure it.
  return 0;
}

size_t
nsTSubstring_CharT::SizeOfExcludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  // This is identical to SizeOfExcludingThisIfUnshared except for the
  // DataFlags::SHARED case.
  if (mDataFlags & DataFlags::SHARED) {
    return nsStringBuffer::FromData(mData)->
      SizeOfIncludingThisEvenIfShared(aMallocSizeOf);
  }
  if (mDataFlags & DataFlags::OWNED) {
    return aMallocSizeOf(mData);
  }
  return 0;
}

size_t
nsTSubstring_CharT::SizeOfIncludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
nsTSubstring_CharT::SizeOfIncludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
}

inline
nsTSubstringSplitter_CharT::nsTSubstringSplitter_CharT(
    const nsTSubstring_CharT* aStr, char_type aDelim)
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
  mArray.reset(new nsTDependentSubstring_CharT[mArraySize]);

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

nsTSubstringSplitter_CharT
nsTSubstring_CharT::Split(const char_type aChar) const
{
  return nsTSubstringSplitter_CharT(this, aChar);
}

const nsTDependentSubstring_CharT&
nsTSubstringSplitter_CharT::nsTSubstringSplit_Iter::operator* () const
{
   return mObj.Get(mPos);
}
