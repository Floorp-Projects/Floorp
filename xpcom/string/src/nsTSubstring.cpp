/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/MemoryReporting.h"
#include "double-conversion.h"

using double_conversion::DoubleToStringConverter;

#ifdef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
nsTSubstring_CharT::nsTSubstring_CharT(char_type* aData, size_type aLength,
                                       uint32_t aFlags)
  : mData(aData),
    mLength(aLength),
    mFlags(aFlags)
{
  if (aFlags & F_OWNED) {
    STRING_STAT_INCREMENT(Adopt);
#ifdef NS_BUILD_REFCNT_LOGGING
    NS_LogCtor(mData, "StringAdopt", 1);
#endif
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
                               uint32_t* aOldFlags)
{
  // initialize to no old data
  *aOldData = nullptr;
  *aOldFlags = 0;

  size_type curCapacity = Capacity();

  // If |aCapacity > kMaxCapacity|, then our doubling algorithm may not be
  // able to allocate it.  Just bail out in cases like that.  We don't want
  // to be allocating 2GB+ strings anyway.
  PR_STATIC_ASSERT((sizeof(nsStringBuffer) & 0x1) == 0);
  const size_type kMaxCapacity =
    (size_type(-1) / 2 - sizeof(nsStringBuffer)) / sizeof(char_type) - 2;
  if (aCapacity > kMaxCapacity) {
    // Also assert for |aCapacity| equal to |size_type(-1)|, since we used to
    // use that value to flag immutability.
    NS_ASSERTION(aCapacity != size_type(-1), "Bogus capacity");
    return false;
  }

  // |curCapacity == 0| means that the buffer is immutable or 0-sized, so we
  // need to allocate a new buffer. We cannot use the existing buffer even
  // though it might be large enough.

  if (curCapacity != 0) {
    if (aCapacity <= curCapacity) {
      mFlags &= ~F_VOIDED;  // mutation clears voided flag
      return true;
    }

    // Use doubling algorithm when forced to increase available capacity.
    size_type temp = curCapacity;
    while (temp < aCapacity) {
      temp <<= 1;
    }
    NS_ASSERTION(XPCOM_MIN(temp, kMaxCapacity) >= aCapacity,
                 "should have hit the early return at the top");
    aCapacity = XPCOM_MIN(temp, kMaxCapacity);
  }

  //
  // several cases:
  //
  //  (1) we have a shared buffer (mFlags & F_SHARED)
  //  (2) we have an owned buffer (mFlags & F_OWNED)
  //  (3) we have a fixed buffer (mFlags & F_FIXED)
  //  (4) we have a readonly buffer
  //
  // requiring that we in some cases preserve the data before creating
  // a new buffer complicates things just a bit ;-)
  //

  size_type storageSize = (aCapacity + 1) * sizeof(char_type);

  // case #1
  if (mFlags & F_SHARED) {
    nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
    if (!hdr->IsReadonly()) {
      nsStringBuffer* newHdr = nsStringBuffer::Realloc(hdr, storageSize);
      if (!newHdr) {
        return false;  // out-of-memory (original header left intact)
      }

      hdr = newHdr;
      mData = (char_type*)hdr->Data();
      mFlags &= ~F_VOIDED;  // mutation clears voided flag
      return true;
    }
  }

  char_type* newData;
  uint32_t newDataFlags;

  // if we have a fixed buffer of sufficient size, then use it.  this helps
  // avoid heap allocations.
  if ((mFlags & F_CLASS_FIXED) &&
      (aCapacity < AsFixedString(this)->mFixedCapacity)) {
    newData = AsFixedString(this)->mFixedBuf;
    newDataFlags = F_TERMINATED | F_FIXED;
  } else {
    // if we reach here then, we must allocate a new buffer.  we cannot
    // make use of our F_OWNED or F_FIXED buffers because they are not
    // large enough.

    nsStringBuffer* newHdr =
      nsStringBuffer::Alloc(storageSize).take();
    if (!newHdr) {
      return false;  // we are still in a consistent state
    }

    newData = (char_type*)newHdr->Data();
    newDataFlags = F_TERMINATED | F_SHARED;
  }

  // save old data and flags
  *aOldData = mData;
  *aOldFlags = mFlags;

  mData = newData;
  SetDataFlags(newDataFlags);

  // mLength does not change

  // though we are not necessarily terminated at the moment, now is probably
  // still the best time to set F_TERMINATED.

  return true;
}

void
nsTSubstring_CharT::Finalize()
{
  ::ReleaseData(mData, mFlags);
  // mData, mLength, and mFlags are purposefully left dangling
}

bool
nsTSubstring_CharT::ReplacePrepInternal(index_type aCutStart, size_type aCutLen,
                                        size_type aFragLen, size_type aNewLen)
{
  char_type* oldData;
  uint32_t oldFlags;
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
  if (mFlags & F_SHARED) {
    // if the string is readonly, then we pretend that it has no capacity.
    nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
    if (hdr->IsReadonly()) {
      capacity = 0;
    } else {
      capacity = (hdr->StorageSize() / sizeof(char_type)) - 1;
    }
  } else if (mFlags & F_FIXED) {
    capacity = AsFixedString(this)->mFixedCapacity;
  } else if (mFlags & F_OWNED) {
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
    if (mFlags & (F_FIXED | F_OWNED)) {
      return true;
    }
    if ((mFlags & F_SHARED) &&
        !nsStringBuffer::FromData(mData)->IsReadonly()) {
      return true;
    }

    aNewLen = mLength;
  }
  return SetLength(aNewLen, fallible_t());
}

// ---------------------------------------------------------------------------

// This version of Assign is optimized for single-character assignment.
void
nsTSubstring_CharT::Assign(char_type aChar)
{
  if (!ReplacePrep(0, mLength, 1)) {
    NS_ABORT_OOM(mLength);
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
  if (!Assign(aData, size_type(-1), fallible_t())) {
    NS_ABORT_OOM(char_traits::length(aData));
  }
}

void
nsTSubstring_CharT::Assign(const char_type* aData, size_type aLength)
{
  if (!Assign(aData, aLength, fallible_t())) {
    NS_ABORT_OOM(aLength);
  }
}

bool
nsTSubstring_CharT::Assign(const char_type* aData, size_type aLength,
                           const fallible_t&)
{
  if (!aData || aLength == 0) {
    Truncate();
    return true;
  }

  if (aLength == size_type(-1)) {
    aLength = char_traits::length(aData);
  }

  if (IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), fallible_t());
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
  if (!AssignASCII(aData, aLength, fallible_t())) {
    NS_ABORT_OOM(aLength);
  }
}

bool
nsTSubstring_CharT::AssignASCII(const char* aData, size_type aLength,
                                const fallible_t&)
{
  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (IsDependentOn(aData, aData + aLength)) {
    return Assign(string_type(aData, aLength), fallible_t());
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
  ::ReleaseData(mData, mFlags);
  mData = const_cast<char_type*>(aData);
  mLength = aLength;
  SetDataFlags(F_TERMINATED | F_LITERAL);
}

void
nsTSubstring_CharT::Assign(const self_type& aStr)
{
  if (!Assign(aStr, fallible_t())) {
    NS_ABORT_OOM(aStr.Length());
  }
}

bool
nsTSubstring_CharT::Assign(const self_type& aStr, const fallible_t&)
{
  // |aStr| could be sharable. We need to check its flags to know how to
  // deal with it.

  if (&aStr == this) {
    return true;
  }

  if (!aStr.mLength) {
    Truncate();
    mFlags |= aStr.mFlags & F_VOIDED;
    return true;
  }

  if (aStr.mFlags & F_SHARED) {
    // nice! we can avoid a string copy :-)

    // |aStr| should be null-terminated
    NS_ASSERTION(aStr.mFlags & F_TERMINATED, "shared, but not terminated");

    ::ReleaseData(mData, mFlags);

    mData = aStr.mData;
    mLength = aStr.mLength;
    SetDataFlags(F_TERMINATED | F_SHARED);

    // get an owning reference to the mData
    nsStringBuffer::FromData(mData)->AddRef();
    return true;
  } else if (aStr.mFlags & F_LITERAL) {
    NS_ABORT_IF_FALSE(aStr.mFlags & F_TERMINATED, "Unterminated literal");

    AssignLiteral(aStr.mData, aStr.mLength);
    return true;
  }

  // else, treat this like an ordinary assignment.
  return Assign(aStr.Data(), aStr.Length(), fallible_t());
}

void
nsTSubstring_CharT::Assign(const substring_tuple_type& aTuple)
{
  if (!Assign(aTuple, fallible_t())) {
    NS_ABORT_OOM(aTuple.Length());
  }
}

bool
nsTSubstring_CharT::Assign(const substring_tuple_type& aTuple,
                           const fallible_t&)
{
  if (aTuple.IsDependentOn(mData, mData + mLength)) {
    // take advantage of sharing here...
    return Assign(string_type(aTuple), fallible_t());
  }

  size_type length = aTuple.Length();

  // don't use ReplacePrep here because it changes the length
  char_type* oldData;
  uint32_t oldFlags;
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
    ::ReleaseData(mData, mFlags);

    if (aLength == size_type(-1)) {
      aLength = char_traits::length(aData);
    }

    mData = aData;
    mLength = aLength;
    SetDataFlags(F_TERMINATED | F_OWNED);

    STRING_STAT_INCREMENT(Adopt);
#ifdef NS_BUILD_REFCNT_LOGGING
    // Treat this as construction of a "StringAdopt" object for leak
    // tracking purposes.
    NS_LogCtor(mData, "StringAdopt", 1);
#endif // NS_BUILD_REFCNT_LOGGING
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
                            const mozilla::fallible_t&)
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
               mozilla::fallible_t())) {
    NS_ABORT_OOM(Length() - aCutLength + 1);
  }
}

bool
nsTSubstring_CharT::Replace(index_type aCutStart, size_type aCutLength,
                            const char_type* aData, size_type aLength,
                            const mozilla::fallible_t&)
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
      return Replace(aCutStart, aCutLength, temp, mozilla::fallible_t());
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
  if (aLength == size_type(-1)) {
    aLength = strlen(aData);
  }

  // A Unicode string can't depend on an ASCII string buffer,
  // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
  if (IsDependentOn(aData, aData + aLength)) {
    nsTAutoString_CharT temp(aData, aLength);
    Replace(aCutStart, aCutLength, temp);
    return;
  }
#endif

  aCutStart = XPCOM_MIN(aCutStart, Length());

  if (ReplacePrep(aCutStart, aCutLength, aLength) && aLength > 0) {
    char_traits::copyASCII(mData + aCutStart, aData, aLength);
  }
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
  if (!SetCapacity(aCapacity, fallible_t())) {
    NS_ABORT_OOM(aCapacity);
  }
}

bool
nsTSubstring_CharT::SetCapacity(size_type aCapacity, const fallible_t&)
{
  // capacity does not include room for the terminating null char

  // if our capacity is reduced to zero, then free our buffer.
  if (aCapacity == 0) {
    ::ReleaseData(mData, mFlags);
    mData = char_traits::sEmptyBuffer;
    mLength = 0;
    SetDataFlags(F_TERMINATED);
    return true;
  }

  char_type* oldData;
  uint32_t oldFlags;
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
nsTSubstring_CharT::SetLength(size_type aLength, const fallible_t&)
{
  if (!SetCapacity(aLength, fallible_t())) {
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
    mFlags |= F_VOIDED;
  } else {
    mFlags &= ~F_VOIDED;
  }
}

bool
nsTSubstring_CharT::Equals(const self_type& aStr) const
{
  return mLength == aStr.mLength &&
         char_traits::compare(mData, aStr.mData, mLength) == 0;
}

bool
nsTSubstring_CharT::Equals(const self_type& aStr,
                           const comparator_type& aComp) const
{
  return mLength == aStr.mLength &&
         aComp(mData, aStr.mData, mLength, aStr.mLength) == 0;
}

bool
nsTSubstring_CharT::Equals(const char_type* aData) const
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
nsTSubstring_CharT::Equals(const char_type* aData,
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
nsTSubstring_CharT::EqualsASCII(const char* aData, size_type aLen) const
{
  return mLength == aLen &&
         char_traits::compareASCII(mData, aData, aLen) == 0;
}

bool
nsTSubstring_CharT::EqualsASCII(const char* aData) const
{
  return char_traits::compareASCIINullTerminated(mData, mLength, aData) == 0;
}

bool
nsTSubstring_CharT::LowerCaseEqualsASCII(const char* aData,
                                         size_type aLen) const
{
  return mLength == aLen &&
         char_traits::compareLowerCaseToASCII(mData, aData, aLen) == 0;
}

bool
nsTSubstring_CharT::LowerCaseEqualsASCII(const char* aData) const
{
  return char_traits::compareLowerCaseToASCIINullTerminated(mData,
                                                            mLength,
                                                            aData) == 0;
}

nsTSubstring_CharT::size_type
nsTSubstring_CharT::CountChar(char_type aChar) const
{
  const char_type* start = mData;
  const char_type* end   = mData + mLength;

  return NS_COUNT(start, end, aChar);
}

int32_t
nsTSubstring_CharT::FindChar(char_type aChar, index_type aOffset) const
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

void
nsTSubstring_CharT::StripChar(char_type aChar, int32_t aOffset)
{
  if (mLength == 0 || aOffset >= int32_t(mLength)) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    NS_ABORT_OOM(mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = mData + aOffset;
  char_type* from = mData + aOffset;
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
nsTSubstring_CharT::StripChars(const char_type* aChars, uint32_t aOffset)
{
  if (aOffset >= uint32_t(mLength)) {
    return;
  }

  if (!EnsureMutable()) { // XXX do this lazily?
    NS_ABORT_OOM(mLength);
  }

  // XXX(darin): this code should defer writing until necessary.

  char_type* to   = mData + aOffset;
  char_type* from = mData + aOffset;
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

int
nsTSubstring_CharT::AppendFunc(void* aArg, const char* aStr, uint32_t aLen)
{
  self_type* self = static_cast<self_type*>(aArg);

  // NSPR sends us the final null terminator even though we don't want it
  if (aLen && aStr[aLen - 1] == '\0') {
    --aLen;
  }

  self->AppendASCII(aStr, aLen);

  return aLen;
}

void
nsTSubstring_CharT::AppendPrintf(const char* aFormat, ...)
{
  va_list ap;
  va_start(ap, aFormat);
  uint32_t r = PR_vsxprintf(AppendFunc, this, aFormat, ap);
  if (r == (uint32_t)-1) {
    NS_RUNTIMEABORT("Allocation or other failure in PR_vsxprintf");
  }
  va_end(ap);
}

void
nsTSubstring_CharT::AppendPrintf(const char* aFormat, va_list aAp)
{
  uint32_t r = PR_vsxprintf(AppendFunc, this, aFormat, aAp);
  if (r == (uint32_t)-1) {
    NS_RUNTIMEABORT("Allocation or other failure in PR_vsxprintf");
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
nsTSubstring_CharT::SizeOfExcludingThisMustBeUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  if (mFlags & F_SHARED) {
    return nsStringBuffer::FromData(mData)->
      SizeOfIncludingThisMustBeUnshared(aMallocSizeOf);
  }
  if (mFlags & F_OWNED) {
    return aMallocSizeOf(mData);
  }

  // If we reach here, exactly one of the following must be true:
  // - F_VOIDED is set, and mData points to sEmptyBuffer;
  // - F_FIXED is set, and mData points to a buffer within a string
  //   object (e.g. nsAutoString);
  // - None of F_SHARED, F_OWNED, F_FIXED is set, and mData points to a buffer
  //   owned by something else.
  //
  // In all three cases, we don't measure it.
  return 0;
}

size_t
nsTSubstring_CharT::SizeOfExcludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  // This is identical to SizeOfExcludingThisMustBeUnshared except for the
  // F_SHARED case.
  if (mFlags & F_SHARED) {
    return nsStringBuffer::FromData(mData)->
           SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }
  if (mFlags & F_OWNED) {
    return aMallocSizeOf(mData);
  }
  return 0;
}

size_t
nsTSubstring_CharT::SizeOfExcludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  // This is identical to SizeOfExcludingThisMustBeUnshared except for the
  // F_SHARED case.
  if (mFlags & F_SHARED) {
    return nsStringBuffer::FromData(mData)->
      SizeOfIncludingThisEvenIfShared(aMallocSizeOf);
  }
  if (mFlags & F_OWNED) {
    return aMallocSizeOf(mData);
  }
  return 0;
}

size_t
nsTSubstring_CharT::SizeOfIncludingThisMustBeUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) +
         SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
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

