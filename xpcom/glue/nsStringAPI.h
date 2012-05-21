/* vim:set ts=2 sw=2 et cindent: */
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

#include "nsXPCOMStrings.h"
#include "nsISupportsImpl.h"
#include "prlog.h"
#include "nsTArray.h"

/**
 * Comparison function for use with nsACString::Equals
 */
NS_HIDDEN_(PRInt32)
CaseInsensitiveCompare(const char *a, const char *b,
                       PRUint32 length);

class nsAString
{
public:
  typedef PRUnichar  char_type;
  typedef nsAString  self_type;
  typedef PRUint32   size_type;
  typedef PRUint32   index_type;

  /**
   * Returns the length, beginning, and end of a string in one operation.
   */
  NS_HIDDEN_(PRUint32) BeginReading(const char_type **begin,
                                    const char_type **end = nsnull) const;

  NS_HIDDEN_(const char_type*) BeginReading() const;
  NS_HIDDEN_(const char_type*) EndReading() const;

  NS_HIDDEN_(char_type) CharAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < Length(), "Out of bounds");
    return BeginReading()[aPos];
  }
  NS_HIDDEN_(char_type) operator [](PRUint32 aPos) const
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
    PRUint32 dataLen = NS_StringGetData(*this, &data);
    return data[dataLen - 1];
  }

  /**
   * Get the length, begin writing, and optionally set the length of a
   * string all in one operation.
   *
   * @param   newSize Size the string to this length. Pass PR_UINT32_MAX
   *                  to leave the length unchanged.
   * @return  The new length of the string, or 0 if resizing failed.
   */
  NS_HIDDEN_(PRUint32) BeginWriting(char_type **begin,
                                    char_type **end = nsnull,
                                    PRUint32 newSize = PR_UINT32_MAX);

  NS_HIDDEN_(char_type*) BeginWriting(PRUint32 = PR_UINT32_MAX);
  NS_HIDDEN_(char_type*) EndWriting();

  NS_HIDDEN_(bool) SetLength(PRUint32 aLen);

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_StringGetData(*this, &data);
  }

  NS_HIDDEN_(bool) IsEmpty() const
  {
    return Length() == 0;
  }

  NS_HIDDEN_(void) SetIsVoid(bool val)
  {
    NS_StringSetIsVoid(*this, val);
  }
  NS_HIDDEN_(bool) IsVoid() const
  {
    return NS_StringGetIsVoid(*this);
  }

  NS_HIDDEN_(void) Assign(const self_type& aString)
  {
    NS_StringCopy(*this, aString);
  }
  NS_HIDDEN_(void) Assign(const char_type* aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_StringSetData(*this, aData, aLength);
  }
  NS_HIDDEN_(void) Assign(char_type aChar)
  {
    NS_StringSetData(*this, &aChar, 1);
  }

  NS_HIDDEN_(void) AssignLiteral(const char *aStr);
  NS_HIDDEN_(void) AssignASCII(const char *aStr) { AssignLiteral(aStr); }

  NS_HIDDEN_(self_type&) operator=(const self_type& aString) { Assign(aString);   return *this; }
  NS_HIDDEN_(self_type&) operator=(const char_type* aPtr)    { Assign(aPtr);      return *this; }
  NS_HIDDEN_(self_type&) operator=(char_type aChar)          { Assign(aChar);     return *this; }

  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length = size_type(-1) )
  {
    NS_StringSetDataRange(*this, cutStart, cutLength, data, length);
  }
  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, char_type c )
  {
    Replace(cutStart, cutLength, &c, 1);
  }
  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, const self_type& readable )
  {
    const char_type* data;
    PRUint32 dataLen = NS_StringGetData(readable, &data);
    NS_StringSetDataRange(*this, cutStart, cutLength, data, dataLen);
  }
  NS_HIDDEN_(void) SetCharAt( char_type c, index_type pos )
                      { Replace(pos, 1, &c, 1); }

  NS_HIDDEN_(void) Append( char_type c )                                                              { Replace(size_type(-1), 0, c); }
  NS_HIDDEN_(void) Append( const char_type* data, size_type length = size_type(-1) )                  { Replace(size_type(-1), 0, data, length); }
  NS_HIDDEN_(void) Append( const self_type& readable )                                                { Replace(size_type(-1), 0, readable); }
  NS_HIDDEN_(void) AppendLiteral( const char *aASCIIStr );
  NS_HIDDEN_(void) AppendASCII( const char *aASCIIStr )                                               { AppendLiteral(aASCIIStr); }

  NS_HIDDEN_(self_type&) operator+=( char_type c )                                                    { Append(c);        return *this; }
  NS_HIDDEN_(self_type&) operator+=( const char_type* data )                                          { Append(data);     return *this; }
  NS_HIDDEN_(self_type&) operator+=( const self_type& readable )                                      { Append(readable); return *this; }

  NS_HIDDEN_(void) Insert( char_type c, index_type pos )                                              { Replace(pos, 0, c); }
  NS_HIDDEN_(void) Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )  { Replace(pos, 0, data, length); }
  NS_HIDDEN_(void) Insert( const self_type& readable, index_type pos )                                { Replace(pos, 0, readable); }

  NS_HIDDEN_(void) Cut( index_type cutStart, size_type cutLength )                                    { Replace(cutStart, cutLength, nsnull, 0); }

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  /**
   * Remove all occurences of characters in aSet from the string.
   */
  NS_HIDDEN_(void) StripChars(const char *aSet);

  /**
   * Strip whitespace characters from the string.
   */
  NS_HIDDEN_(void) StripWhitespace() { StripChars("\b\t\r\n "); }

  NS_HIDDEN_(void) Trim(const char *aSet, bool aLeading = true,
                        bool aTrailing = true);

  /**
   * Compare strings of characters. Return 0 if the characters are equal,
   */
  typedef PRInt32 (*ComparatorFunc)(const char_type *a,
                                    const char_type *b,
                                    PRUint32 length);

  static NS_HIDDEN_(PRInt32) DefaultComparator(const char_type *a,
                                               const char_type *b,
                                               PRUint32 length);

  NS_HIDDEN_(PRInt32) Compare( const char_type *other,
                               ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(PRInt32) Compare( const self_type &other,
                               ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) Equals( const char_type *other,
                             ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) Equals( const self_type &other,
                             ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) operator < (const self_type &other) const
  {
    return Compare(other) < 0;
  }
  NS_HIDDEN_(bool) operator < (const char_type *other) const
  {
    return Compare(other) < 0;
  }

  NS_HIDDEN_(bool) operator <= (const self_type &other) const
  {
    return Compare(other) <= 0;
  }
  NS_HIDDEN_(bool) operator <= (const char_type *other) const
  {
    return Compare(other) <= 0;
  }

  NS_HIDDEN_(bool) operator == (const self_type &other) const
  {
    return Equals(other);
  }
  NS_HIDDEN_(bool) operator == (const char_type *other) const
  {
    return Equals(other);
  }

  NS_HIDDEN_(bool) operator >= (const self_type &other) const
  {
    return Compare(other) >= 0;
  }
  NS_HIDDEN_(bool) operator >= (const char_type *other) const
  {
    return Compare(other) >= 0;
  }

  NS_HIDDEN_(bool) operator > (const self_type &other) const
  {
    return Compare(other) > 0;
  }
  NS_HIDDEN_(bool) operator > (const char_type *other) const
  {
    return Compare(other) > 0;
  }

  NS_HIDDEN_(bool) operator != (const self_type &other) const
  {
    return !Equals(other);
  }
  NS_HIDDEN_(bool) operator != (const char_type *other) const
  {
    return !Equals(other);
  }

  NS_HIDDEN_(bool) EqualsLiteral(const char *aASCIIString) const;
  NS_HIDDEN_(bool) EqualsASCII(const char *aASCIIString) const
  {
    return EqualsLiteral(aASCIIString);
  }

  /**
   * Case-insensitive match this string to a lowercase ASCII string.
   */
  NS_HIDDEN_(bool) LowerCaseEqualsLiteral(const char *aASCIIString) const;

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(PRInt32) Find(const self_type& aStr,
                           ComparatorFunc c = DefaultComparator) const
  { return Find(aStr, 0, c); }

  /**
   * Find the first occurrence of aStr in this string, beginning at aOffset.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(PRInt32) Find(const self_type& aStr, PRUint32 aOffset,
                           ComparatorFunc c = DefaultComparator) const;

  /**
   * Find an ASCII string within this string.
   *
   * @return the offset of aStr, or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) Find(const char *aStr, bool aIgnoreCase = false) const
  { return Find(aStr, 0, aIgnoreCase); }

  NS_HIDDEN_(PRInt32) Find(const char *aStr, PRUint32 aOffset, bool aIgnoreCase = false) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const self_type& aStr,
                            ComparatorFunc c = DefaultComparator) const
  { return RFind(aStr, -1, c); }

  /**
   * Find the last occurrence of aStr in this string, beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const self_type& aStr, PRInt32 aOffset,
                            ComparatorFunc c = DefaultComparator) const;

  /**
   * Find the last occurrence of an ASCII string within this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const char *aStr, bool aIgnoreCase = false) const
  { return RFind(aStr, -1, aIgnoreCase); }

  /**
   * Find the last occurrence of an ASCII string beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const char *aStr, PRInt32 aOffset, bool aIgnoreCase) const;

  /**
   * Search for the offset of the first occurrence of a character in a
   * string.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) FindChar(char_type aChar, PRUint32 aOffset = 0) const;

  /**
   * Search for the offset of the last occurrence of a character in a
   * string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFindChar(char_type aChar) const;

  /**
   * Append a string representation of a number.
   */
  NS_HIDDEN_(void) AppendInt(int aInt, PRInt32 aRadix = 10);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * Convert this string to an integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(PRInt32) ToInteger(nsresult* aErrorCode,
                                PRUint32 aRadix = 10) const;
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
  typedef PRUint32   size_type;
  typedef PRUint32   index_type;

  /**
   * Returns the length, beginning, and end of a string in one operation.
   */
  NS_HIDDEN_(PRUint32) BeginReading(const char_type **begin,
                                    const char_type **end = nsnull) const;

  NS_HIDDEN_(const char_type*) BeginReading() const;
  NS_HIDDEN_(const char_type*) EndReading() const;

  NS_HIDDEN_(char_type) CharAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < Length(), "Out of bounds");
    return BeginReading()[aPos];
  }
  NS_HIDDEN_(char_type) operator [](PRUint32 aPos) const
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
    PRUint32 dataLen = NS_CStringGetData(*this, &data);
    return data[dataLen - 1];
  }

  /**
   * Get the length, begin writing, and optionally set the length of a
   * string all in one operation.
   *
   * @param   newSize Size the string to this length. Pass PR_UINT32_MAX
   *                  to leave the length unchanged.
   * @return  The new length of the string, or 0 if resizing failed.
   */
  NS_HIDDEN_(PRUint32) BeginWriting(char_type **begin,
                                    char_type **end = nsnull,
                                    PRUint32 newSize = PR_UINT32_MAX);

  NS_HIDDEN_(char_type*) BeginWriting(PRUint32 aLen = PR_UINT32_MAX);
  NS_HIDDEN_(char_type*) EndWriting();

  NS_HIDDEN_(bool) SetLength(PRUint32 aLen);

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_CStringGetData(*this, &data);
  }

  NS_HIDDEN_(bool) IsEmpty() const
  {
    return Length() == 0;
  }

  NS_HIDDEN_(void) SetIsVoid(bool val)
  {
    NS_CStringSetIsVoid(*this, val);
  }
  NS_HIDDEN_(bool) IsVoid() const
  {
    return NS_CStringGetIsVoid(*this);
  }

  NS_HIDDEN_(void) Assign(const self_type& aString)
  {
    NS_CStringCopy(*this, aString);
  }
  NS_HIDDEN_(void) Assign(const char_type* aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_CStringSetData(*this, aData, aLength);
  }
  NS_HIDDEN_(void) Assign(char_type aChar)
  {
    NS_CStringSetData(*this, &aChar, 1);
  }
  NS_HIDDEN_(void) AssignLiteral(const char_type *aData)
  {
    Assign(aData);
  }
  NS_HIDDEN_(void) AssignASCII(const char_type *aData)
  {
    Assign(aData);
  }

  NS_HIDDEN_(self_type&) operator=(const self_type& aString) { Assign(aString);   return *this; }
  NS_HIDDEN_(self_type&) operator=(const char_type* aPtr)    { Assign(aPtr);      return *this; }
  NS_HIDDEN_(self_type&) operator=(char_type aChar)          { Assign(aChar);     return *this; }

  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length = size_type(-1) )
  {
    NS_CStringSetDataRange(*this, cutStart, cutLength, data, length);
  }
  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, char_type c )
  {
    Replace(cutStart, cutLength, &c, 1);
  }
  NS_HIDDEN_(void) Replace( index_type cutStart, size_type cutLength, const self_type& readable )
  {
    const char_type* data;
    PRUint32 dataLen = NS_CStringGetData(readable, &data);
    NS_CStringSetDataRange(*this, cutStart, cutLength, data, dataLen);
  }
  NS_HIDDEN_(void) SetCharAt( char_type c, index_type pos )
                      { Replace(pos, 1, &c, 1); }

  NS_HIDDEN_(void) Append( char_type c )                                                              { Replace(size_type(-1), 0, c); }
  NS_HIDDEN_(void) Append( const char_type* data, size_type length = size_type(-1) )                  { Replace(size_type(-1), 0, data, length); }
  NS_HIDDEN_(void) Append( const self_type& readable )                                                { Replace(size_type(-1), 0, readable); }
  NS_HIDDEN_(void) AppendLiteral( const char *aASCIIStr )                                             { Append(aASCIIStr); }
  NS_HIDDEN_(void) AppendASCII( const char *aASCIIStr )                                               { Append(aASCIIStr); }

  NS_HIDDEN_(self_type&) operator+=( char_type c )                                                    { Append(c);        return *this; }
  NS_HIDDEN_(self_type&) operator+=( const char_type* data )                                          { Append(data);     return *this; }
  NS_HIDDEN_(self_type&) operator+=( const self_type& readable )                                      { Append(readable); return *this; }

  NS_HIDDEN_(void) Insert( char_type c, index_type pos )                                              { Replace(pos, 0, c); }
  NS_HIDDEN_(void) Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )  { Replace(pos, 0, data, length); }
  NS_HIDDEN_(void) Insert( const self_type& readable, index_type pos )                                { Replace(pos, 0, readable); }

  NS_HIDDEN_(void) Cut( index_type cutStart, size_type cutLength )                                    { Replace(cutStart, cutLength, nsnull, 0); }

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  /**
   * Remove all occurences of characters in aSet from the string.
   */
  NS_HIDDEN_(void) StripChars(const char *aSet);

  /**
   * Strip whitespace characters from the string.
   */
  NS_HIDDEN_(void) StripWhitespace() { StripChars("\b\t\r\n "); }

  NS_HIDDEN_(void) Trim(const char *aSet, bool aLeading = true,
                        bool aTrailing = true);

  /**
   * Compare strings of characters. Return 0 if the characters are equal,
   */
  typedef PRInt32 (*ComparatorFunc)(const char_type *a,
                                    const char_type *b,
                                    PRUint32 length);

  static NS_HIDDEN_(PRInt32) DefaultComparator(const char_type *a,
                                               const char_type *b,
                                               PRUint32 length);

  NS_HIDDEN_(PRInt32) Compare( const char_type *other,
                               ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(PRInt32) Compare( const self_type &other,
                               ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) Equals( const char_type *other,
                             ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) Equals( const self_type &other,
                             ComparatorFunc c = DefaultComparator ) const;

  NS_HIDDEN_(bool) operator < (const self_type &other) const
  {
    return Compare(other) < 0;
  }
  NS_HIDDEN_(bool) operator < (const char_type *other) const
  {
    return Compare(other) < 0;
  }

  NS_HIDDEN_(bool) operator <= (const self_type &other) const
  {
    return Compare(other) <= 0;
  }
  NS_HIDDEN_(bool) operator <= (const char_type *other) const
  {
    return Compare(other) <= 0;
  }

  NS_HIDDEN_(bool) operator == (const self_type &other) const
  {
    return Equals(other);
  }
  NS_HIDDEN_(bool) operator == (const char_type *other) const
  {
    return Equals(other);
  }

  NS_HIDDEN_(bool) operator >= (const self_type &other) const
  {
    return Compare(other) >= 0;
  }
  NS_HIDDEN_(bool) operator >= (const char_type *other) const
  {
    return Compare(other) >= 0;
  }

  NS_HIDDEN_(bool) operator > (const self_type &other) const
  {
    return Compare(other) > 0;
  }
  NS_HIDDEN_(bool) operator > (const char_type *other) const
  {
    return Compare(other) > 0;
  }

  NS_HIDDEN_(bool) operator != (const self_type &other) const
  {
    return !Equals(other);
  }
  NS_HIDDEN_(bool) operator != (const char_type *other) const
  {
    return !Equals(other);
  }

  NS_HIDDEN_(bool) EqualsLiteral( const char_type *other ) const
  {
    return Equals(other);
  }
  NS_HIDDEN_(bool) EqualsASCII( const char_type *other ) const
  {
    return Equals(other);
  }

  /**
   * Case-insensitive match this string to a lowercase ASCII string.
   */
  NS_HIDDEN_(bool) LowerCaseEqualsLiteral(const char *aASCIIString) const
  {
    return Equals(aASCIIString, CaseInsensitiveCompare);
  }

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(PRInt32) Find(const self_type& aStr,
                           ComparatorFunc c = DefaultComparator) const
  { return Find(aStr, 0, c); }

  /**
   * Find the first occurrence of aStr in this string, beginning at aOffset.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(PRInt32) Find(const self_type& aStr, PRUint32 aOffset,
                           ComparatorFunc c = DefaultComparator) const;

  /**
   * Find the first occurrence of aStr in this string.
   *
   * @return the offset of aStr, or -1 if not found
   */
  NS_HIDDEN_(PRInt32) Find(const char_type *aStr,
                           ComparatorFunc c = DefaultComparator) const;

  NS_HIDDEN_(PRInt32) Find(const char_type *aStr, PRUint32 aLen,
                           ComparatorFunc c = DefaultComparator) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const self_type& aStr,
                            ComparatorFunc c = DefaultComparator) const
  { return RFind(aStr, -1, c); }

  /**
   * Find the last occurrence of aStr in this string, beginning at aOffset.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching. If aOffset < 0, search from end of this string.
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const self_type& aStr, PRInt32 aOffset,
                            ComparatorFunc c = DefaultComparator) const;

  /**
   * Find the last occurrence of aStr in this string.
   *
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const char_type *aStr,
                            ComparatorFunc c = DefaultComparator) const;

  /**
   * Find the last occurrence of an ASCII string in this string, 
   * beginning at aOffset.
   *
   * @param aLen is the length of aStr
   * @return The offset of aStr from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFind(const char_type *aStr, PRInt32 aLen,
                            ComparatorFunc c = DefaultComparator) const;

  /**
   * Search for the offset of the first occurrence of a character in a
   * string.
   *
   * @param aOffset the offset from the beginning of the string to begin
   *        searching
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) FindChar(char_type aChar, PRUint32 aOffset = 0) const;

  /**
   * Search for the offset of the last occurrence of a character in a
   * string.
   *
   * @return The offset of the character from the beginning of the string,
   *         or -1 if not found.
   */
  NS_HIDDEN_(PRInt32) RFindChar(char_type aChar) const;

  /**
   * Append a string representation of a number.
   */
  NS_HIDDEN_(void) AppendInt(int aInt, PRInt32 aRadix = 10);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * Convert this string to an integer.
   *
   * @param aErrorCode pointer to contain result code.
   * @param aRadix must be 10 or 16
   */
  NS_HIDDEN_(PRInt32) ToInteger(nsresult* aErrorCode,
                                PRUint32 aRadix = 10) const;
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

class nsStringContainer : public nsAString,
                          private nsStringContainer_base
{
};

class nsCStringContainer : public nsACString,
                           private nsStringContainer_base
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

  explicit
  nsString(const abstract_string_type& aReadable)
  {
    NS_StringContainerInit(*this);
    NS_StringCopy(*this, aReadable);
  }

  explicit
  nsString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_StringContainerInit2(*this, aData, aLength, 0);
  }
  
  ~nsString()
  {
    NS_StringContainerFinish(*this);
  }

  const char_type* get() const
  {
    return BeginReading();
  }

  self_type& operator=(const self_type& aString)              { Assign(aString);   return *this; }
  self_type& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
  self_type& operator=(const char_type* aPtr)                 { Assign(aPtr);      return *this; }
  self_type& operator=(char_type aChar)                       { Assign(aChar);     return *this; }

  void Adopt(const char_type *aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aData, aLength,
                            NS_STRING_CONTAINER_INIT_ADOPT);
  }

protected:
  
  nsString(const char_type* aData, size_type aLength, PRUint32 aFlags)
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

  explicit
  nsCString(const abstract_string_type& aReadable)
  {
    NS_CStringContainerInit(*this);
    NS_CStringCopy(*this, aReadable);
  }

  explicit
  nsCString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
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
  
  self_type& operator=(const self_type& aString)              { Assign(aString);   return *this; }
  self_type& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
  self_type& operator=(const char_type* aPtr)                 { Assign(aPtr);      return *this; }
  self_type& operator=(char_type aChar)                       { Assign(aChar);     return *this; }

  void Adopt(const char_type *aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aData, aLength,
                             NS_CSTRING_CONTAINER_INIT_ADOPT);
  }

protected:
  
  nsCString(const char_type* aData, size_type aLength, PRUint32 aFlags)
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

  explicit
  nsDependentString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
    : nsString(aData, aLength, NS_CSTRING_CONTAINER_INIT_DEPEND)
  {}

  void Rebind(const char_type* aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aData, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND);
  }
  
private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};

class nsDependentCString : public nsCString
{
public:
  typedef nsDependentCString        self_type;

  nsDependentCString() {}

  explicit
  nsDependentCString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
    : nsCString(aData, aLength, NS_CSTRING_CONTAINER_INIT_DEPEND)
  {}

  void Rebind(const char_type* aData, size_type aLength = PR_UINT32_MAX)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aData, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND);
  }
  
private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
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

NS_COM_GLUE char*
ToNewUTF8String(const nsAString& aSource);

class NS_ConvertASCIItoUTF16 : public nsString
{
public:
  typedef NS_ConvertASCIItoUTF16    self_type;

  explicit
  NS_ConvertASCIItoUTF16(const nsACString& aStr)
  {
    NS_CStringToUTF16(aStr, NS_CSTRING_ENCODING_ASCII, *this);
  }

  explicit
  NS_ConvertASCIItoUTF16(const char* aData, PRUint32 aLength = PR_UINT32_MAX)
  {
    NS_CStringToUTF16(nsDependentCString(aData, aLength),
                      NS_CSTRING_ENCODING_ASCII, *this);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};

class NS_ConvertUTF8toUTF16 : public nsString
{
public:
  typedef NS_ConvertUTF8toUTF16    self_type;

  explicit
  NS_ConvertUTF8toUTF16(const nsACString& aStr)
  {
    NS_CStringToUTF16(aStr, NS_CSTRING_ENCODING_UTF8, *this);
  }

  explicit
  NS_ConvertUTF8toUTF16(const char* aData, PRUint32 aLength = PR_UINT32_MAX)
  {
    NS_CStringToUTF16(nsDependentCString(aData, aLength),
                      NS_CSTRING_ENCODING_UTF8, *this);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};

class NS_ConvertUTF16toUTF8 : public nsCString
{
public:
  typedef NS_ConvertUTF16toUTF8    self_type;

  explicit
  NS_ConvertUTF16toUTF8(const nsAString& aStr)
  {
    NS_UTF16ToCString(aStr, NS_CSTRING_ENCODING_UTF8, *this);
  }

  explicit
  NS_ConvertUTF16toUTF8(const PRUnichar* aData, PRUint32 aLength = PR_UINT32_MAX)
  {
    NS_UTF16ToCString(nsDependentString(aData, aLength),
                      NS_CSTRING_ENCODING_UTF8, *this);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};

class NS_LossyConvertUTF16toASCII : public nsCString
{
public:
  typedef NS_LossyConvertUTF16toASCII    self_type;

  explicit
  NS_LossyConvertUTF16toASCII(const nsAString& aStr)
  {
    NS_UTF16ToCString(aStr, NS_CSTRING_ENCODING_ASCII, *this);
  }

  explicit
  NS_LossyConvertUTF16toASCII(const PRUnichar* aData, PRUint32 aLength = PR_UINT32_MAX)
  {
    NS_UTF16ToCString(nsDependentString(aData, aLength),
                      NS_CSTRING_ENCODING_ASCII, *this);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};


/**
 * literal strings
 *
 * NOTE: HAVE_CPP_2BYTE_WCHAR_T may be automatically defined for some platforms
 * in nscore.h.  On other platforms, it may be defined in xpcom-config.h.
 * Under GCC, this define should only be set if compiling with -fshort-wchar.
 */

#if defined(HAVE_CPP_CHAR16_T) || defined(HAVE_CPP_2BYTE_WCHAR_T)
#if defined(HAVE_CPP_CHAR16_T)
  MOZ_STATIC_ASSERT(sizeof(char16_t) == 2, "size of char16_t must be 2");
  #define NS_LL(s)                                u##s
#else
  MOZ_STATIC_ASSERT(sizeof(wchar_t) == 2, "size of wchar_t must be 2");
  #define NS_LL(s)                                L##s
#endif
  #define NS_MULTILINE_LITERAL_STRING(s)          nsDependentString(reinterpret_cast<const nsAString::char_type*>(s), PRUint32((sizeof(s)/2)-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(reinterpret_cast<const nsAString::char_type*>(s), PRUint32((sizeof(s)/2)-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const nsDependentString n(reinterpret_cast<const nsAString::char_type*>(s), PRUint32((sizeof(s)/2)-1))
  typedef nsDependentString nsLiteralString;
#else
  #define NS_LL(s)                                s
  #define NS_MULTILINE_LITERAL_STRING(s)          NS_ConvertASCIItoUTF16(s, PRUint32(sizeof(s)-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(s, PRUint32(sizeof(s)-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const NS_ConvertASCIItoUTF16 n(s, PRUint32(sizeof(s)-1))
  typedef NS_ConvertASCIItoUTF16 nsLiteralString;
#endif

/* Check that PRUnichar is unsigned */
MOZ_STATIC_ASSERT(PRUnichar(-1) > PRUnichar(0), "PRUnichar is by definition an unsigned type");

/*
 * Macro arguments used in concatenation or stringification won't be expanded.
 * Therefore, in order for |NS_L(FOO)| to work as expected (which is to expand
 * |FOO| before doing whatever |NS_L| needs to do to it) a helper macro needs
 * to be inserted in between to allow the macro argument to expand.
 * See "3.10.6 Separate Expansion of Macro Arguments" of the CPP manual for a
 * more accurate and precise explanation.
 */

#define NS_L(s)                                   NS_LL(s)

#define NS_LITERAL_STRING(s)                      static_cast<const nsString&>(NS_MULTILINE_LITERAL_STRING(NS_LL(s)))
#define NS_LITERAL_STRING_INIT(n,s)               NS_MULTILINE_LITERAL_STRING_INIT(n, NS_LL(s))
#define NS_NAMED_LITERAL_STRING(n,s)              NS_NAMED_MULTILINE_LITERAL_STRING(n, NS_LL(s))

#define NS_LITERAL_CSTRING(s)                     static_cast<const nsDependentCString&>(nsDependentCString(s, PRUint32(sizeof(s)-1)))
#define NS_LITERAL_CSTRING_INIT(n,s)              n(s, PRUint32(sizeof(s)-1))
#define NS_NAMED_LITERAL_CSTRING(n,s)             const nsDependentCString n(s, PRUint32(sizeof(s)-1))

typedef nsDependentCString nsLiteralCString;


/**
 * getter_Copies support
 *
 *    NS_IMETHOD GetBlah(PRUnichar**);
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
  typedef PRUnichar char_type;

  nsGetterCopies(nsString& aStr)
    : mString(aStr), mData(nsnull)
  {}

  ~nsGetterCopies()
  {
    mString.Adopt(mData);
  }

  operator char_type**()
  {
    return &mData;
  }

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

  nsCGetterCopies(nsCString& aStr)
    : mString(aStr), mData(nsnull)
  {}

  ~nsCGetterCopies()
  {
    mString.Adopt(mData);
  }

  operator char_type**()
  {
    return &mData;
  }

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

class NS_COM_GLUE nsDependentSubstring : public nsStringContainer
{
public:
  typedef nsDependentSubstring self_type;
  typedef nsAString            abstract_string_type;

  ~nsDependentSubstring()
  {
    NS_StringContainerFinish(*this);
  }

  nsDependentSubstring()
  {
    NS_StringContainerInit(*this);
  }

  nsDependentSubstring(const char_type *aStart, PRUint32 aLength)
  {
    NS_StringContainerInit2(*this, aStart, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentSubstring(const abstract_string_type& aStr,
                       PRUint32 aStartPos);
  nsDependentSubstring(const abstract_string_type& aStr,
                       PRUint32 aStartPos, PRUint32 aLength);

  void Rebind(const char_type *aStart, PRUint32 aLength)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aStart, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};

class NS_COM_GLUE nsDependentCSubstring : public nsCStringContainer
{
public:
  typedef nsDependentCSubstring self_type;
  typedef nsACString            abstract_string_type;

  ~nsDependentCSubstring()
  {
    NS_CStringContainerFinish(*this);
  }

  nsDependentCSubstring()
  {
    NS_CStringContainerInit(*this);
  }

  nsDependentCSubstring(const char_type *aStart, PRUint32 aLength)
  {
    NS_CStringContainerInit2(*this, aStart, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentCSubstring(const abstract_string_type& aStr,
                        PRUint32 aStartPos);
  nsDependentCSubstring(const abstract_string_type& aStr,
                        PRUint32 aStartPos, PRUint32 aLength);

  void Rebind(const char_type *aStart, PRUint32 aLength)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aStart, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString) MOZ_DELETE;
};


/**
 * Various nsDependentC?Substring constructor functions
 */

// PRUnichar
inline const nsDependentSubstring
Substring( const nsAString& str, PRUint32 startPos )
{
  return nsDependentSubstring(str, startPos);
}

inline const nsDependentSubstring
Substring( const nsAString& str, PRUint32 startPos, PRUint32 length )
{
  return nsDependentSubstring(str, startPos, length);
}

inline const nsDependentSubstring
Substring( const PRUnichar* start, const PRUnichar* end )
{
  NS_ABORT_IF_FALSE(PRUint32(end - start) == uintptr_t(end - start), "string too long");
  return nsDependentSubstring(start, PRUint32(end - start));
}

inline const nsDependentSubstring
Substring( const PRUnichar* start, PRUint32 length )
{
  return nsDependentSubstring(start, length);
}

inline const nsDependentSubstring
StringHead( const nsAString& str, PRUint32 count )
{
  return nsDependentSubstring(str, 0, count);
}

inline const nsDependentSubstring
StringTail( const nsAString& str, PRUint32 count )
{
  return nsDependentSubstring(str, str.Length() - count, count);
}

// char
inline const nsDependentCSubstring
Substring( const nsACString& str, PRUint32 startPos )
{
  return nsDependentCSubstring(str, startPos);
}

inline const nsDependentCSubstring
Substring( const nsACString& str, PRUint32 startPos, PRUint32 length )
{
  return nsDependentCSubstring(str, startPos, length);
}

inline
const nsDependentCSubstring
Substring( const char* start, const char* end )
{
  NS_ABORT_IF_FALSE(PRUint32(end - start) == uintptr_t(end - start), "string too long");
  return nsDependentCSubstring(start, PRUint32(end - start));
}

inline
const nsDependentCSubstring
Substring( const char* start, PRUint32 length )
{
  return nsDependentCSubstring(start, length);
}

inline const nsDependentCSubstring
StringHead( const nsACString& str, PRUint32 count )
{
  return nsDependentCSubstring(str, 0, count);
}

inline const nsDependentCSubstring
StringTail( const nsACString& str, PRUint32 count )
{
  return nsDependentCSubstring(str, str.Length() - count, count);
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
NS_HIDDEN_(void)
CompressWhitespace(nsAString& aString);

#define EmptyCString() nsCString()
#define EmptyString() nsString()

/**
 * Convert an ASCII string to all upper/lowercase (a-z,A-Z only). As a bonus,
 * returns the string length.
 */
NS_HIDDEN_(PRUint32)
ToLowerCase(nsACString& aStr);

NS_HIDDEN_(PRUint32)
ToUpperCase(nsACString& aStr);

NS_HIDDEN_(PRUint32)
ToLowerCase(const nsACString& aSrc, nsACString& aDest);

NS_HIDDEN_(PRUint32)
ToUpperCase(const nsACString& aSrc, nsACString& aDest);

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

inline PRUnichar*
ToNewUnicode(const nsAString& aStr)
{
  return NS_StringCloneData(aStr);
}

typedef nsString PromiseFlatString;
typedef nsCString PromiseFlatCString;

typedef nsCString nsCAutoString;
typedef nsString nsAutoString;

NS_HIDDEN_(bool) ParseString(const nsACString& aAstring, char aDelimiter, 
                               nsTArray<nsCString>& aArray);

#endif // nsStringAPI_h__
