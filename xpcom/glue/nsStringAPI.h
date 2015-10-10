/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header provides wrapper classes around the frozen string API
 * which are roughly equivalent to the internal string classes.
 */

#ifdef MOZILLA_INTERNAL_API
#error nsStringAPI.h is only usable from non-MOZILLA_INTERNAL_API code!
#endif

#ifndef nsStringAPI_h__
#define nsStringAPI_h__

#include "mozilla/Attributes.h"
#include "mozilla/Char16.h"

#include "nsXPCOMStrings.h"
#include "nsISupportsImpl.h"
#include "mozilla/Logging.h"
#include "nsTArray.h"

/**
 * Comparison function for use with nsACString::Equals
 */
NS_HIDDEN_(int32_t) CaseInsensitiveCompare(const char* aStrA, const char* aStrB,
                                           uint32_t aLength);

class nsAString
{
public:
  typedef char16_t  char_type;
  typedef nsAString self_type;
  typedef uint32_t  size_type;
  typedef uint32_t  index_type;

  /**
   * Returns the length, beginning, and end of a string in one operation.
   */
  NS_HIDDEN_(uint32_t) BeginReading(const char_type** aBegin,
                                    const char_type** aEnd = nullptr) const;

  NS_HIDDEN_(const char_type*) BeginReading() const;
  NS_HIDDEN_(const char_type*) EndReading() const;

  NS_HIDDEN_(char_type) CharAt(uint32_t aPos) const
  {
    NS_ASSERTION(aPos < Length(), "Out of bounds");
    return BeginReading()[aPos];
  }
  NS_HIDDEN_(char_type) operator [](uint32_t aPos) const
  {
    return CharAt(aPos);
  }
  NS_HIDDEN_(char_type) First() const
  {
    return CharAt(0);
  }
  NS_HIDDEN_(char_type) Last() const
  {
    const char_type* data;
    uint32_t dataLen = NS_StringGetData(*this, &data);
    return data[dataLen - 1];
  }

  /**
   * Get the length, begin writing, and optionally set the length of a
   * string all in one operation.
   *
   * @param   newSize Size the string to this length. Pass UINT32_MAX
   *                  to leave the length unchanged.
   * @return  The new length of the string, or 0 if resizing failed.
   */
  NS_HIDDEN_(uint32_t) BeginWriting(char_type** aBegin,
                                    char_type** aEnd = nullptr,
                                    uint32_t aNewSize = UINT32_MAX);

  NS_HIDDEN_(char_type*) BeginWriting(uint32_t = UINT32_MAX);
  NS_HIDDEN_(char_type*) EndWriting();

  NS_HIDDEN_(bool) SetLength(uint32_t aLen);

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_StringGetData(*this, &data);
  }

  NS_HIDDEN_(bool) IsEmpty() const { return Length() == 0; }

  NS_HIDDEN_(void) SetIsVoid(bool aVal) { NS_StringSetIsVoid(*this, aVal); }
  NS_HIDDEN_(bool) IsVoid() const { return NS_StringGetIsVoid(*this); }

  NS_HIDDEN_(void) Assign(const self_type& aString)
  {
    NS_StringCopy(*this, aString);
  }
  NS_HIDDEN_(void) Assign(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_StringSetData(*this, aData, aLength);
  }
  NS_HIDDEN_(void) Assign(char_type aChar)
  {
    NS_StringSetData(*this, &aChar, 1);
  }
#ifdef MOZ_USE_CHAR16_WRAPPER
  NS_HIDDEN_(void) Assign(char16ptr_t aData, size_type aLength = UINT32_MAX)
  {
    NS_StringSetData(*this, aData, aLength);
  }
#endif

  NS_HIDDEN_(void) AssignLiteral(const char* aStr);
  NS_HIDDEN_(void) AssignASCII(const char* aStr)
  {
    AssignLiteral(aStr);
  }

  NS_HIDDEN_(self_type&) operator=(const self_type& aString)
  {
    Assign(aString);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator=(const char_type* aPtr)
  {
    Assign(aPtr);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator=(char_type aChar)
  {
    Assign(aChar);
    return *this;
  }
#ifdef MOZ_USE_CHAR16_WRAPPER
  NS_HIDDEN_(self_type&) operator=(char16ptr_t aPtr)
  {
    Assign(aPtr);
    return *this;
  }
#endif

  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           const char_type* aData,
                           size_type aLength = size_type(-1))
  {
    NS_StringSetDataRange(*this, aCutStart, aCutLength, aData, aLength);
  }
  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           char_type aChar)
  {
    Replace(aCutStart, aCutLength, &aChar, 1);
  }
  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           const self_type& aReadable)
  {
    const char_type* data;
    uint32_t dataLen = NS_StringGetData(aReadable, &data);
    NS_StringSetDataRange(*this, aCutStart, aCutLength, data, dataLen);
  }
  NS_HIDDEN_(void) SetCharAt(char_type aChar, index_type aPos)
  {
    Replace(aPos, 1, &aChar, 1);
  }

  NS_HIDDEN_(void) Append(char_type aChar)
  {
    Replace(size_type(-1), 0, aChar);
  }
  NS_HIDDEN_(void) Append(const char_type* aData,
                          size_type aLength = size_type(-1))
  {
    Replace(size_type(-1), 0, aData, aLength);
  }
#ifdef MOZ_USE_CHAR16_WRAPPER
  NS_HIDDEN_(void) Append(char16ptr_t aData, size_type aLength = size_type(-1))
  {
    Append(static_cast<const char16_t*>(aData), aLength);
  }
#endif
  NS_HIDDEN_(void) Append(const self_type& aReadable)
  {
    Replace(size_type(-1), 0, aReadable);
  }
  NS_HIDDEN_(void) AppendLiteral(const char* aASCIIStr);
  NS_HIDDEN_(void) AppendASCII(const char* aASCIIStr)
  {
    AppendLiteral(aASCIIStr);
  }

  NS_HIDDEN_(self_type&) operator+=(char_type aChar)
  {
    Append(aChar);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator+=(const char_type* aData)
  {
    Append(aData);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator+=(const self_type& aReadable)
  {
    Append(aReadable);
    return *this;
  }

  NS_HIDDEN_(void) Insert(char_type aChar, index_type aPos)
  {
    Replace(aPos, 0, aChar);
  }
  NS_HIDDEN_(void) Insert(const char_type* aData, index_type aPos,
                          size_type aLength = size_type(-1))
  {
    Replace(aPos, 0, aData, aLength);
  }
  NS_HIDDEN_(void) Insert(const self_type& aReadable, index_type aPos)
  {
    Replace(aPos, 0, aReadable);
  }

  NS_HIDDEN_(void) Cut(index_type aCutStart, size_type aCutLength)
  {
    Replace(aCutStart, aCutLength, nullptr, 0);
  }

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  /**
   * Remove all occurences of characters in aSet from the string.
   */
  NS_HIDDEN_(void) StripChars(const char* aSet);

  /**
   * Strip whitespace characters from the string.
   */
  NS_HIDDEN_(void) StripWhitespace() { StripChars("\b\t\r\n "); }

  NS_HIDDEN_(void) Trim(const char* aSet, bool aLeading = true,
                        bool aTrailing = true);

  /**
   * Compare strings of characters. Return 0 if the characters are equal,
   */
  typedef int32_t (*ComparatorFunc)(const char_type* aStrA,
                                    const char_type* aStrB,
                                    uint32_t aLength);

  static NS_HIDDEN_(int32_t) DefaultComparator(const char_type* aStrA,
                                               const char_type* aStrB,
                                               uint32_t aLength);

  NS_HIDDEN_(int32_t) Compare(const char_type* aOther,
                              ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(int32_t) Compare(const self_type& aOther,
                              ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) Equals(const char_type* aOther,
                          ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) Equals(const self_type& aOther,
                          ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) operator<(const self_type& aOther) const
  {
    return Compare(aOther) < 0;
  }
  NS_HIDDEN_(bool) operator<(const char_type* aOther) const
  {
    return Compare(aOther) < 0;
  }

  NS_HIDDEN_(bool) operator<=(const self_type& aOther) const
  {
    return Compare(aOther) <= 0;
  }
  NS_HIDDEN_(bool) operator<=(const char_type* aOther) const
  {
    return Compare(aOther) <= 0;
  }

  NS_HIDDEN_(bool) operator==(const self_type& aOther) const
  {
    return Equals(aOther);
  }
  NS_HIDDEN_(bool) operator==(const char_type* aOther) const
  {
    return Equals(aOther);
  }
#ifdef MOZ_USE_CHAR16_WRAPPER
  NS_HIDDEN_(bool) operator==(char16ptr_t aOther) const
  {
    return Equals(aOther);
  }
#endif

  NS_HIDDEN_(bool) operator>=(const self_type& aOther) const
  {
    return Compare(aOther) >= 0;
  }
  NS_HIDDEN_(bool) operator>=(const char_type* aOther) const
  {
    return Compare(aOther) >= 0;
  }

  NS_HIDDEN_(bool) operator>(const self_type& aOther) const
  {
    return Compare(aOther) > 0;
  }
  NS_HIDDEN_(bool) operator>(const char_type* aOther) const
  {
    return Compare(aOther) > 0;
  }

  NS_HIDDEN_(bool) operator!=(const self_type& aOther) const
  {
    return !Equals(aOther);
  }
  NS_HIDDEN_(bool) operator!=(const char_type* aOther) const
  {
    return !Equals(aOther);
  }

  NS_HIDDEN_(bool) EqualsLiteral(const char* aASCIIString) const;
  NS_HIDDEN_(bool) EqualsASCII(const char* aASCIIString) const
  {
    return EqualsLiteral(aASCIIString);
  }

  /**
   * Case-insensitive match this string to a lowercase ASCII string.
   */
  NS_HIDDEN_(bool) LowerCaseEqualsLiteral(const char* aASCIIString) const;

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(int32_t) Find(const self_type& aStr,
                           ComparatorFunc aComparator = DefaultComparator) const
  {
    return Find(aStr, 0, aComparator);
  }

  /**
   * Find the first occurrence of aStr in this string, beginning at aOffset.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(int32_t) Find(const self_type& aStr, uint32_t aOffset,
                           ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find an ASCII string within this string.
   *
   * @return the offset of aStr, or -1 if not found.
   */
  NS_HIDDEN_(int32_t) Find(const char* aStr, bool aIgnoreCase = false) const
  {
    return Find(aStr, 0, aIgnoreCase);
  }

  NS_HIDDEN_(int32_t) Find(const char* aStr, uint32_t aOffset,
                           bool aIgnoreCase = false) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const self_type& aStr,
                            ComparatorFunc aComparator = DefaultComparator) const
  {
    return RFind(aStr, -1, aComparator);
  }

  /**
   * Find the last occurrence of aStr in this string, beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const self_type& aStr, int32_t aOffset,
                            ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find the last occurrence of an ASCII string within this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const char* aStr, bool aIgnoreCase = false) const
  {
    return RFind(aStr, -1, aIgnoreCase);
  }

  /**
   * Find the last occurrence of an ASCII string beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const char* aStr, int32_t aOffset,
                            bool aIgnoreCase) const;

  /**
   * Search for the offset of the first occurrence of a character in a
   * string.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) FindChar(char_type aChar, uint32_t aOffset = 0) const;

  /**
   * Search for the offset of the last occurrence of a character in a
   * string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFindChar(char_type aChar) const;

  /**
   * Append a string representation of a number.
   */
  NS_HIDDEN_(void) AppendInt(int aInt, int32_t aRadix = 10);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * Convert this string to an integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(int32_t) ToInteger(nsresult* aErrorCode,
                                uint32_t aRadix = 10) const;
  /**
   * Convert this string to a 64-bit integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(int64_t) ToInteger64(nsresult* aErrorCode,
                                  uint32_t aRadix = 10) const;
#endif // XPCOM_GLUE_AVOID_NSPR

protected:
  // Prevent people from allocating a nsAString directly.
  ~nsAString() {}
};

class nsACString
{
public:
  typedef char       char_type;
  typedef nsACString self_type;
  typedef uint32_t   size_type;
  typedef uint32_t   index_type;

  /**
   * Returns the length, beginning, and end of a string in one operation.
   */
  NS_HIDDEN_(uint32_t) BeginReading(const char_type** aBegin,
                                    const char_type** aEnd = nullptr) const;

  NS_HIDDEN_(const char_type*) BeginReading() const;
  NS_HIDDEN_(const char_type*) EndReading() const;

  NS_HIDDEN_(char_type) CharAt(uint32_t aPos) const
  {
    NS_ASSERTION(aPos < Length(), "Out of bounds");
    return BeginReading()[aPos];
  }
  NS_HIDDEN_(char_type) operator [](uint32_t aPos) const
  {
    return CharAt(aPos);
  }
  NS_HIDDEN_(char_type) First() const
  {
    return CharAt(0);
  }
  NS_HIDDEN_(char_type) Last() const
  {
    const char_type* data;
    uint32_t dataLen = NS_CStringGetData(*this, &data);
    return data[dataLen - 1];
  }

  /**
   * Get the length, begin writing, and optionally set the length of a
   * string all in one operation.
   *
   * @param   newSize Size the string to this length. Pass UINT32_MAX
   *                  to leave the length unchanged.
   * @return  The new length of the string, or 0 if resizing failed.
   */
  NS_HIDDEN_(uint32_t) BeginWriting(char_type** aBegin,
                                    char_type** aEnd = nullptr,
                                    uint32_t aNewSize = UINT32_MAX);

  NS_HIDDEN_(char_type*) BeginWriting(uint32_t aLen = UINT32_MAX);
  NS_HIDDEN_(char_type*) EndWriting();

  NS_HIDDEN_(bool) SetLength(uint32_t aLen);

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_CStringGetData(*this, &data);
  }

  NS_HIDDEN_(bool) IsEmpty() const { return Length() == 0; }

  NS_HIDDEN_(void) SetIsVoid(bool aVal) { NS_CStringSetIsVoid(*this, aVal); }
  NS_HIDDEN_(bool) IsVoid() const { return NS_CStringGetIsVoid(*this); }

  NS_HIDDEN_(void) Assign(const self_type& aString)
  {
    NS_CStringCopy(*this, aString);
  }
  NS_HIDDEN_(void) Assign(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_CStringSetData(*this, aData, aLength);
  }
  NS_HIDDEN_(void) Assign(char_type aChar)
  {
    NS_CStringSetData(*this, &aChar, 1);
  }
  NS_HIDDEN_(void) AssignLiteral(const char_type* aData)
  {
    Assign(aData);
  }
  NS_HIDDEN_(void) AssignASCII(const char_type* aData)
  {
    Assign(aData);
  }

  NS_HIDDEN_(self_type&) operator=(const self_type& aString)
  {
    Assign(aString);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator=(const char_type* aPtr)
  {
    Assign(aPtr);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator=(char_type aChar)
  {
    Assign(aChar);
    return *this;
  }

  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           const char_type* aData,
                           size_type aLength = size_type(-1))
  {
    NS_CStringSetDataRange(*this, aCutStart, aCutLength, aData, aLength);
  }
  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           char_type aChar)
  {
    Replace(aCutStart, aCutLength, &aChar, 1);
  }
  NS_HIDDEN_(void) Replace(index_type aCutStart, size_type aCutLength,
                           const self_type& aReadable)
  {
    const char_type* data;
    uint32_t dataLen = NS_CStringGetData(aReadable, &data);
    NS_CStringSetDataRange(*this, aCutStart, aCutLength, data, dataLen);
  }
  NS_HIDDEN_(void) SetCharAt(char_type aChar, index_type aPos)
  {
    Replace(aPos, 1, &aChar, 1);
  }

  NS_HIDDEN_(void) Append(char_type aChar)
  {
    Replace(size_type(-1), 0, aChar);
  }
  NS_HIDDEN_(void) Append(const char_type* aData,
                          size_type aLength = size_type(-1))
  {
    Replace(size_type(-1), 0, aData, aLength);
  }
  NS_HIDDEN_(void) Append(const self_type& aReadable)
  {
    Replace(size_type(-1), 0, aReadable);
  }
  NS_HIDDEN_(void) AppendLiteral(const char* aASCIIStr)
  {
    Append(aASCIIStr);
  }
  NS_HIDDEN_(void) AppendASCII(const char* aASCIIStr)
  {
    Append(aASCIIStr);
  }

  NS_HIDDEN_(self_type&) operator+=(char_type aChar)
  {
    Append(aChar);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator+=(const char_type* aData)
  {
    Append(aData);
    return *this;
  }
  NS_HIDDEN_(self_type&) operator+=(const self_type& aReadable)
  {
    Append(aReadable);
    return *this;
  }

  NS_HIDDEN_(void) Insert(char_type aChar, index_type aPos)
  {
    Replace(aPos, 0, aChar);
  }
  NS_HIDDEN_(void) Insert(const char_type* aData, index_type aPos,
                          size_type aLength = size_type(-1))
  {
    Replace(aPos, 0, aData, aLength);
  }
  NS_HIDDEN_(void) Insert(const self_type& aReadable, index_type aPos)
  {
    Replace(aPos, 0, aReadable);
  }

  NS_HIDDEN_(void) Cut(index_type aCutStart, size_type aCutLength)
  {
    Replace(aCutStart, aCutLength, nullptr, 0);
  }

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  /**
   * Remove all occurences of characters in aSet from the string.
   */
  NS_HIDDEN_(void) StripChars(const char* aSet);

  /**
   * Strip whitespace characters from the string.
   */
  NS_HIDDEN_(void) StripWhitespace() { StripChars("\b\t\r\n "); }

  NS_HIDDEN_(void) Trim(const char* aSet, bool aLeading = true,
                        bool aTrailing = true);

  /**
   * Compare strings of characters. Return 0 if the characters are equal,
   */
  typedef int32_t (*ComparatorFunc)(const char_type* a,
                                    const char_type* b,
                                    uint32_t length);

  static NS_HIDDEN_(int32_t) DefaultComparator(const char_type* aStrA,
                                               const char_type* aStrB,
                                               uint32_t aLength);

  NS_HIDDEN_(int32_t) Compare(const char_type* aOther,
                              ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(int32_t) Compare(const self_type& aOther,
                              ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) Equals(const char_type* aOther,
                          ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) Equals(const self_type& aOther,
                          ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(bool) operator<(const self_type& aOther) const
  {
    return Compare(aOther) < 0;
  }
  NS_HIDDEN_(bool) operator<(const char_type* aOther) const
  {
    return Compare(aOther) < 0;
  }

  NS_HIDDEN_(bool) operator<=(const self_type& aOther) const
  {
    return Compare(aOther) <= 0;
  }
  NS_HIDDEN_(bool) operator<=(const char_type* aOther) const
  {
    return Compare(aOther) <= 0;
  }

  NS_HIDDEN_(bool) operator==(const self_type& aOther) const
  {
    return Equals(aOther);
  }
  NS_HIDDEN_(bool) operator==(const char_type* aOther) const
  {
    return Equals(aOther);
  }

  NS_HIDDEN_(bool) operator>=(const self_type& aOther) const
  {
    return Compare(aOther) >= 0;
  }
  NS_HIDDEN_(bool) operator>=(const char_type* aOther) const
  {
    return Compare(aOther) >= 0;
  }

  NS_HIDDEN_(bool) operator>(const self_type& aOther) const
  {
    return Compare(aOther) > 0;
  }
  NS_HIDDEN_(bool) operator>(const char_type* aOther) const
  {
    return Compare(aOther) > 0;
  }

  NS_HIDDEN_(bool) operator!=(const self_type& aOther) const
  {
    return !Equals(aOther);
  }
  NS_HIDDEN_(bool) operator!=(const char_type* aOther) const
  {
    return !Equals(aOther);
  }

  NS_HIDDEN_(bool) EqualsLiteral(const char_type* aOther) const
  {
    return Equals(aOther);
  }
  NS_HIDDEN_(bool) EqualsASCII(const char_type* aOther) const
  {
    return Equals(aOther);
  }

  /**
   * Case-insensitive match this string to a lowercase ASCII string.
   */
  NS_HIDDEN_(bool) LowerCaseEqualsLiteral(const char* aASCIIString) const
  {
    return Equals(aASCIIString, CaseInsensitiveCompare);
  }

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(int32_t) Find(const self_type& aStr,
                           ComparatorFunc aComparator = DefaultComparator) const
  {
    return Find(aStr, 0, aComparator);
  }

  /**
   * Find the first occurrence of aStr in this string, beginning at aOffset.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(int32_t) Find(const self_type& aStr, uint32_t aOffset,
                           ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(int32_t) Find(const char_type* aStr,
                           ComparatorFunc aComparator = DefaultComparator) const;

  NS_HIDDEN_(int32_t) Find(const char_type* aStr, uint32_t aLen,
                           ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const self_type& aStr,
                            ComparatorFunc aComparator = DefaultComparator) const
  {
    return RFind(aStr, -1, aComparator);
  }

  /**
   * Find the last occurrence of aStr in this string, beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const self_type& aStr, int32_t aOffset,
                            ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const char_type* aStr,
                            ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Find the last occurrence of an ASCII string in this string,
   * beginning at aOffset.
   *
   * @param aLen is the length of aStr
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFind(const char_type* aStr, int32_t aLen,
                            ComparatorFunc aComparator = DefaultComparator) const;

  /**
   * Search for the offset of the first occurrence of a character in a
   * string.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) FindChar(char_type aChar, uint32_t aOffset = 0) const;

  /**
   * Search for the offset of the last occurrence of a character in a
   * string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(int32_t) RFindChar(char_type aChar) const;

  /**
   * Append a string representation of a number.
   */
  NS_HIDDEN_(void) AppendInt(int aInt, int32_t aRadix = 10);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * Convert this string to an integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(int32_t) ToInteger(nsresult* aErrorCode,
                                uint32_t aRadix = 10) const;
  /**
   * Convert this string to a 64-bit integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(int64_t) ToInteger64(nsresult* aErrorCode,
                                  uint32_t aRadix = 10) const;
#endif // XPCOM_GLUE_AVOID_NSPR

protected:
  // Prevent people from allocating a nsAString directly.
  ~nsACString() {}
};

/* ------------------------------------------------------------------------- */

/**
 * Below we define nsStringContainer and nsCStringContainer.  These classes
 * have unspecified structure.  In most cases, your code should use
 * nsString/nsCString instead of these classes; if you prefer C-style
 * programming, then look no further.
 */

class nsStringContainer
  : public nsAString
  , private nsStringContainer_base
{
};

class nsCStringContainer
  : public nsACString
  , private nsStringContainer_base
{
};

/**
 * The following classes are C++ helper classes that make the frozen string
 * API easier to use.
 */

/**
 * Rename symbols to avoid conflicting with internal versions.
 */
#define nsString                       nsString_external
#define nsCString                      nsCString_external
#define nsDependentString              nsDependentString_external
#define nsDependentCString             nsDependentCString_external
#define NS_ConvertASCIItoUTF16         NS_ConvertASCIItoUTF16_external
#define NS_ConvertUTF8toUTF16          NS_ConvertUTF8toUTF16_external
#define NS_ConvertUTF16toUTF8          NS_ConvertUTF16toUTF8_external
#define NS_LossyConvertUTF16toASCII    NS_LossyConvertUTF16toASCII_external
#define nsGetterCopies                 nsGetterCopies_external
#define nsCGetterCopies                nsCGetterCopies_external
#define nsDependentSubstring           nsDependentSubstring_external
#define nsDependentCSubstring          nsDependentCSubstring_external

/**
 * basic strings
 */

class nsString : public nsStringContainer
{
public:
  typedef nsString         self_type;
  typedef nsAString        abstract_string_type;

  nsString()
  {
    NS_StringContainerInit(*this);
  }

  nsString(const self_type& aString)
  {
    NS_StringContainerInit(*this);
    NS_StringCopy(*this, aString);
  }

  explicit nsString(const abstract_string_type& aReadable)
  {
    NS_StringContainerInit(*this);
    NS_StringCopy(*this, aReadable);
  }

  explicit nsString(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_StringContainerInit2(*this, aData, aLength, 0);
  }

#ifdef MOZ_USE_CHAR16_WRAPPER
  explicit nsString(char16ptr_t aData, size_type aLength = UINT32_MAX)
    : nsString(static_cast<const char16_t*>(aData), aLength)
  {
  }
#endif

  ~nsString()
  {
    NS_StringContainerFinish(*this);
  }

  char16ptr_t get() const
  {
    return char16ptr_t(BeginReading());
  }

  self_type& operator=(const self_type& aString)
  {
    Assign(aString);
    return *this;
  }
  self_type& operator=(const abstract_string_type& aReadable)
  {
    Assign(aReadable);
    return *this;
  }
  self_type& operator=(const char_type* aPtr)
  {
    Assign(aPtr);
    return *this;
  }
  self_type& operator=(char_type aChar)
  {
    Assign(aChar);
    return *this;
  }

  void Adopt(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aData, aLength,
                            NS_STRING_CONTAINER_INIT_ADOPT);
  }

protected:
  nsString(const char_type* aData, size_type aLength, uint32_t aFlags)
  {
    NS_StringContainerInit2(*this, aData, aLength, aFlags);
  }
};

class nsCString : public nsCStringContainer
{
public:
  typedef nsCString        self_type;
  typedef nsACString       abstract_string_type;

  nsCString()
  {
    NS_CStringContainerInit(*this);
  }

  nsCString(const self_type& aString)
  {
    NS_CStringContainerInit(*this);
    NS_CStringCopy(*this, aString);
  }

  explicit nsCString(const abstract_string_type& aReadable)
  {
    NS_CStringContainerInit(*this);
    NS_CStringCopy(*this, aReadable);
  }

  explicit nsCString(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_CStringContainerInit(*this);
    NS_CStringSetData(*this, aData, aLength);
  }

  ~nsCString()
  {
    NS_CStringContainerFinish(*this);
  }

  const char_type* get() const
  {
    return BeginReading();
  }

  self_type& operator=(const self_type& aString)
  {
    Assign(aString);
    return *this;
  }
  self_type& operator=(const abstract_string_type& aReadable)
  {
    Assign(aReadable);
    return *this;
  }
  self_type& operator=(const char_type* aPtr)
  {
    Assign(aPtr);
    return *this;
  }
  self_type& operator=(char_type aChar)
  {
    Assign(aChar);
    return *this;
  }

  void Adopt(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aData, aLength,
                             NS_CSTRING_CONTAINER_INIT_ADOPT);
  }

protected:
  nsCString(const char_type* aData, size_type aLength, uint32_t aFlags)
  {
    NS_CStringContainerInit2(*this, aData, aLength, aFlags);
  }
};


/**
 * dependent strings
 */

class nsDependentString : public nsString
{
public:
  typedef nsDependentString         self_type;

  nsDependentString() {}

  explicit nsDependentString(const char_type* aData,
                             size_type aLength = UINT32_MAX)
    : nsString(aData, aLength, NS_CSTRING_CONTAINER_INIT_DEPEND)
  {
  }

#ifdef MOZ_USE_CHAR16_WRAPPER
  explicit nsDependentString(char16ptr_t aData, size_type aLength = UINT32_MAX)
    : nsDependentString(static_cast<const char16_t*>(aData), aLength)
  {
  }
#endif

  void Rebind(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aData, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};

class nsDependentCString : public nsCString
{
public:
  typedef nsDependentCString        self_type;

  nsDependentCString() {}

  explicit nsDependentCString(const char_type* aData,
                              size_type aLength = UINT32_MAX)
    : nsCString(aData, aLength, NS_CSTRING_CONTAINER_INIT_DEPEND)
  {
  }

  void Rebind(const char_type* aData, size_type aLength = UINT32_MAX)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aData, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};


/**
 * conversion classes
 */

inline void
CopyUTF16toUTF8(const nsAString& aSource, nsACString& aDest)
{
  NS_UTF16ToCString(aSource, NS_CSTRING_ENCODING_UTF8, aDest);
}

inline void
CopyUTF8toUTF16(const nsACString& aSource, nsAString& aDest)
{
  NS_CStringToUTF16(aSource, NS_CSTRING_ENCODING_UTF8, aDest);
}

inline void
LossyCopyUTF16toASCII(const nsAString& aSource, nsACString& aDest)
{
  NS_UTF16ToCString(aSource, NS_CSTRING_ENCODING_ASCII, aDest);
}

inline void
CopyASCIItoUTF16(const nsACString& aSource, nsAString& aDest)
{
  NS_CStringToUTF16(aSource, NS_CSTRING_ENCODING_ASCII, aDest);
}

char*
ToNewUTF8String(const nsAString& aSource);

class NS_ConvertASCIItoUTF16 : public nsString
{
public:
  typedef NS_ConvertASCIItoUTF16    self_type;

  explicit NS_ConvertASCIItoUTF16(const nsACString& aStr)
  {
    NS_CStringToUTF16(aStr, NS_CSTRING_ENCODING_ASCII, *this);
  }

  explicit NS_ConvertASCIItoUTF16(const char* aData,
                                  uint32_t aLength = UINT32_MAX)
  {
    NS_CStringToUTF16(nsDependentCString(aData, aLength),
                      NS_CSTRING_ENCODING_ASCII, *this);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};

class NS_ConvertUTF8toUTF16 : public nsString
{
public:
  typedef NS_ConvertUTF8toUTF16    self_type;

  explicit NS_ConvertUTF8toUTF16(const nsACString& aStr)
  {
    NS_CStringToUTF16(aStr, NS_CSTRING_ENCODING_UTF8, *this);
  }

  explicit NS_ConvertUTF8toUTF16(const char* aData,
                                 uint32_t aLength = UINT32_MAX)
  {
    NS_CStringToUTF16(nsDependentCString(aData, aLength),
                      NS_CSTRING_ENCODING_UTF8, *this);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};

class NS_ConvertUTF16toUTF8 : public nsCString
{
public:
  typedef NS_ConvertUTF16toUTF8    self_type;

  explicit NS_ConvertUTF16toUTF8(const nsAString& aStr)
  {
    NS_UTF16ToCString(aStr, NS_CSTRING_ENCODING_UTF8, *this);
  }

  explicit NS_ConvertUTF16toUTF8(const char16ptr_t aData,
                                 uint32_t aLength = UINT32_MAX)
  {
    NS_UTF16ToCString(nsDependentString(aData, aLength),
                      NS_CSTRING_ENCODING_UTF8, *this);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};

class NS_LossyConvertUTF16toASCII : public nsCString
{
public:
  typedef NS_LossyConvertUTF16toASCII    self_type;

  explicit NS_LossyConvertUTF16toASCII(const nsAString& aStr)
  {
    NS_UTF16ToCString(aStr, NS_CSTRING_ENCODING_ASCII, *this);
  }

  explicit NS_LossyConvertUTF16toASCII(const char16ptr_t aData,
                                       uint32_t aLength = UINT32_MAX)
  {
    NS_UTF16ToCString(nsDependentString(aData, aLength),
                      NS_CSTRING_ENCODING_ASCII, *this);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};


/**
 * literal strings
 */
static_assert(sizeof(char16_t) == 2, "size of char16_t must be 2");
static_assert(char16_t(-1) > char16_t(0), "char16_t must be unsigned");

#define NS_MULTILINE_LITERAL_STRING(s) \
  nsDependentString(reinterpret_cast<const nsAString::char_type*>(s), \
                    uint32_t((sizeof(s) / 2) - 1))
#define NS_MULTILINE_LITERAL_STRING_INIT(n, s) \
  n(reinterpret_cast<const nsAString::char_type*>(s), \
    uint32_t((sizeof(s) / 2) - 1))
#define NS_NAMED_MULTILINE_LITERAL_STRING(n,s) \
  const nsDependentString n(reinterpret_cast<const nsAString::char_type*>(s), \
                            uint32_t((sizeof(s) / 2) - 1))
typedef nsDependentString nsLiteralString;

/* Check that char16_t is unsigned */
static_assert(char16_t(-1) > char16_t(0),
              "char16_t is by definition an unsigned type");

#define NS_LITERAL_STRING(s) \
  static_cast<const nsString&>(NS_MULTILINE_LITERAL_STRING(MOZ_UTF16(s)))
#define NS_LITERAL_STRING_INIT(n, s) \
  NS_MULTILINE_LITERAL_STRING_INIT(n, MOZ_UTF16(s))
#define NS_NAMED_LITERAL_STRING(n, s) \
  NS_NAMED_MULTILINE_LITERAL_STRING(n, MOZ_UTF16(s))

#define NS_LITERAL_CSTRING(s) \
  static_cast<const nsDependentCString&>(nsDependentCString(s, uint32_t(sizeof(s) - 1)))
#define NS_LITERAL_CSTRING_INIT(n, s) \
  n(s, uint32_t(sizeof(s)-1))
#define NS_NAMED_LITERAL_CSTRING(n, s) \
  const nsDependentCString n(s, uint32_t(sizeof(s)-1))

typedef nsDependentCString nsLiteralCString;


/**
 * getter_Copies support
 *
 *    NS_IMETHOD GetBlah(char16_t**);
 *
 *    void some_function()
 *    {
 *      nsString blah;
 *      GetBlah(getter_Copies(blah));
 *      // ...
 *    }
 */

class nsGetterCopies
{
public:
  typedef char16_t char_type;

  explicit nsGetterCopies(nsString& aStr)
    : mString(aStr)
    , mData(nullptr)
  {
  }

  ~nsGetterCopies() { mString.Adopt(mData); }

  operator char_type**() { return &mData; }

private:
  nsString&  mString;
  char_type* mData;
};

inline nsGetterCopies
getter_Copies(nsString& aString)
{
  return nsGetterCopies(aString);
}

class nsCGetterCopies
{
public:
  typedef char char_type;

  explicit nsCGetterCopies(nsCString& aStr)
    : mString(aStr)
    , mData(nullptr)
  {
  }

  ~nsCGetterCopies() { mString.Adopt(mData); }

  operator char_type**() { return &mData; }

private:
  nsCString& mString;
  char_type* mData;
};

inline nsCGetterCopies
getter_Copies(nsCString& aString)
{
  return nsCGetterCopies(aString);
}


/**
* substrings
*/

class nsDependentSubstring : public nsStringContainer
{
public:
  typedef nsDependentSubstring self_type;
  typedef nsAString            abstract_string_type;

  ~nsDependentSubstring() { NS_StringContainerFinish(*this); }
  nsDependentSubstring() { NS_StringContainerInit(*this); }

  nsDependentSubstring(const char_type* aStart, uint32_t aLength)
  {
    NS_StringContainerInit2(*this, aStart, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentSubstring(const abstract_string_type& aStr,
                       uint32_t aStartPos);
  nsDependentSubstring(const abstract_string_type& aStr,
                       uint32_t aStartPos, uint32_t aLength);

  void Rebind(const char_type* aStart, uint32_t aLength)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aStart, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};

class nsDependentCSubstring : public nsCStringContainer
{
public:
  typedef nsDependentCSubstring self_type;
  typedef nsACString            abstract_string_type;

  ~nsDependentCSubstring() { NS_CStringContainerFinish(*this); }
  nsDependentCSubstring() { NS_CStringContainerInit(*this); }

  nsDependentCSubstring(const char_type* aStart, uint32_t aLength)
  {
    NS_CStringContainerInit2(*this, aStart, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentCSubstring(const abstract_string_type& aStr,
                        uint32_t aStartPos);
  nsDependentCSubstring(const abstract_string_type& aStr,
                        uint32_t aStartPos, uint32_t aLength);

  void Rebind(const char_type* aStart, uint32_t aLength)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aStart, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString) = delete;
};


/**
 * Various nsDependentC?Substring constructor functions
 */

// char16_t
inline const nsDependentSubstring
Substring(const nsAString& aStr, uint32_t aStartPos)
{
  return nsDependentSubstring(aStr, aStartPos);
}

inline const nsDependentSubstring
Substring(const nsAString& aStr, uint32_t aStartPos, uint32_t aLength)
{
  return nsDependentSubstring(aStr, aStartPos, aLength);
}

inline const nsDependentSubstring
Substring(const char16_t* aStart, const char16_t* aEnd)
{
  MOZ_ASSERT(uint32_t(aEnd - aStart) == uintptr_t(aEnd - aStart),
             "string too long");
  return nsDependentSubstring(aStart, uint32_t(aEnd - aStart));
}

inline const nsDependentSubstring
Substring(const char16_t* aStart, uint32_t aLength)
{
  return nsDependentSubstring(aStart, aLength);
}

inline const nsDependentSubstring
StringHead(const nsAString& aStr, uint32_t aCount)
{
  return nsDependentSubstring(aStr, 0, aCount);
}

inline const nsDependentSubstring
StringTail(const nsAString& aStr, uint32_t aCount)
{
  return nsDependentSubstring(aStr, aStr.Length() - aCount, aCount);
}

// char
inline const nsDependentCSubstring
Substring(const nsACString& aStr, uint32_t aStartPos)
{
  return nsDependentCSubstring(aStr, aStartPos);
}

inline const nsDependentCSubstring
Substring(const nsACString& aStr, uint32_t aStartPos, uint32_t aLength)
{
  return nsDependentCSubstring(aStr, aStartPos, aLength);
}

inline const nsDependentCSubstring
Substring(const char* aStart, const char* aEnd)
{
  MOZ_ASSERT(uint32_t(aEnd - aStart) == uintptr_t(aEnd - aStart),
             "string too long");
  return nsDependentCSubstring(aStart, uint32_t(aEnd - aStart));
}

inline const nsDependentCSubstring
Substring(const char* aStart, uint32_t aLength)
{
  return nsDependentCSubstring(aStart, aLength);
}

inline const nsDependentCSubstring
StringHead(const nsACString& aStr, uint32_t aCount)
{
  return nsDependentCSubstring(aStr, 0, aCount);
}

inline const nsDependentCSubstring
StringTail(const nsACString& aStr, uint32_t aCount)
{
  return nsDependentCSubstring(aStr, aStr.Length() - aCount, aCount);
}


inline bool
StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring,
                 nsAString::ComparatorFunc aComparator = nsAString::DefaultComparator)
{
  return aSubstring.Length() <= aSource.Length() &&
    StringHead(aSource, aSubstring.Length()).Equals(aSubstring, aComparator);
}

inline bool
StringEndsWith(const nsAString& aSource, const nsAString& aSubstring,
               nsAString::ComparatorFunc aComparator = nsAString::DefaultComparator)
{
  return aSubstring.Length() <= aSource.Length() &&
    StringTail(aSource, aSubstring.Length()).Equals(aSubstring, aComparator);
}

inline bool
StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring,
                 nsACString::ComparatorFunc aComparator = nsACString::DefaultComparator)
{
  return aSubstring.Length() <= aSource.Length() &&
    StringHead(aSource, aSubstring.Length()).Equals(aSubstring, aComparator);
}

inline bool
StringEndsWith(const nsACString& aSource, const nsACString& aSubstring,
               nsACString::ComparatorFunc aComparator = nsACString::DefaultComparator)
{
  return aSubstring.Length() <= aSource.Length() &&
    StringTail(aSource, aSubstring.Length()).Equals(aSubstring, aComparator);
}

/**
 * Trim whitespace from the beginning and end of a string; then compress
 * remaining runs of whitespace characters to a single space.
 */
NS_HIDDEN_(void) CompressWhitespace(nsAString& aString);

#define EmptyCString() nsCString()
#define EmptyString() nsString()

/**
 * Convert an ASCII string to all upper/lowercase (a-z,A-Z only). As a bonus,
 * returns the string length.
 */
NS_HIDDEN_(uint32_t) ToLowerCase(nsACString& aStr);

NS_HIDDEN_(uint32_t) ToUpperCase(nsACString& aStr);

NS_HIDDEN_(uint32_t) ToLowerCase(const nsACString& aSrc, nsACString& aDest);

NS_HIDDEN_(uint32_t) ToUpperCase(const nsACString& aSrc, nsACString& aDest);

/**
 * The following declarations are *deprecated*, and are included here only
 * to make porting from existing code that doesn't use the frozen string API
 * easier. They may disappear in the future.
 */

inline char*
ToNewCString(const nsACString& aStr)
{
  return NS_CStringCloneData(aStr);
}

inline char16_t*
ToNewUnicode(const nsAString& aStr)
{
  return NS_StringCloneData(aStr);
}

typedef nsString PromiseFlatString;
typedef nsCString PromiseFlatCString;

typedef nsCString nsAutoCString;
typedef nsString nsAutoString;

NS_HIDDEN_(bool) ParseString(const nsACString& aAstring, char aDelimiter,
                             nsTArray<nsCString>& aArray);

#endif // nsStringAPI_h__
