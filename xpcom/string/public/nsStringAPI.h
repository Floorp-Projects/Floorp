/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsStringAPI_h__
#define nsStringAPI_h__

/**
 * nsStringAPI.h
 *
 * This file describes a minimal API for working with XPCOM's abstract
 * string classes.  It divorces the consumer from having any run-time
 * dependency on the implementation details of the abstract string types.
 */

#include "nscore.h"

#define NS_STRINGAPI(x) extern "C" NS_COM x

/* The base string types */
class nsAString;
class nsACString;

/* ------------------------------------------------------------------------- */

/**
 * nsStringContainer
 *
 * This is an opaque data type that is large enough to hold the canonical
 * implementation of nsAString.  The binary structure of this class is an
 * implementation detail.
 *
 * The string data stored in a string container is always single fragment
 * and null-terminated.
 *
 * Typically, string containers are allocated on the stack for temporary
 * use.  However, they can also be malloc'd if necessary.  In either case,
 * a string container is not useful until it has been initialized with a
 * call to NS_StringContainerInit.  The following example shows how to use
 * a string container to call a function that takes a |nsAString &| out-param.
 *
 *   NS_METHOD GetBlah(nsAString &aBlah);
 *
 *   nsresult MyCode()
 *   {
 *     nsresult rv;
 *
 *     nsStringContainer sc;
 *     rv = NS_StringContainerInit(sc);
 *     if (NS_FAILED(rv))
 *       return rv;
 *
 *     rv = GetBlah(sc);
 *     if (NS_SUCCEEDED(rv))
 *     {
 *       const PRUnichar *data;
 *       NS_StringGetData(sc, &data);
 *       //
 *       // |data| now points to the result of the GetBlah function
 *       //
 *     }
 *
 *     NS_StringContainerFinish(sc);
 *     return rv;
 *   }
 *
 * The following example show how to use a string container to pass a string
 * parameter to a function taking a |const nsAString &| in-param.
 *
 *   NS_METHOD SetBlah(const nsAString &aBlah);
 *
 *   nsresult MyCode()
 *   {
 *     nsresult rv;
 *
 *     nsStringContainer sc;
 *     rv = NS_StringContainerInit(sc);
 *     if (NS_FAILED(rv))
 *       return rv;
 *
 *     const PRUnichar kData[] = {'x','y','z','\0'};
 *     rv = NS_StringSetData(sc, kData, sizeof(kData)/2 - 1);
 *     if (NS_SUCCEEDED(rv))
 *       rv = SetBlah(sc);
 *
 *     NS_StringContainerFinish(sc);
 *     return rv;
 *   }
 */
class nsStringContainer;

/**
 * NS_StringContainerInit
 *
 * @param aContainer    string container reference
 * @return              NS_OK if string container successfully initialized
 *
 * This function may allocate additional memory for aContainer.  When
 * aContainer is no longer needed, NS_StringContainerFinish should be called.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_StringContainerInit(nsStringContainer &aContainer);

/**
 * NS_StringContainerFinish
 *
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 *
 * @status FROZEN
 */
NS_STRINGAPI(void)
NS_StringContainerFinish(nsStringContainer &aContainer);

/* ------------------------------------------------------------------------- */

/**
 * NS_StringGetData
 *
 * This function returns a const character pointer to the string's internal
 * buffer, the length of the string, and a boolean value indicating whether
 * or not the buffer is null-terminated.
 *
 * @param aStr          abstract string reference
 * @param aData         out param that will hold the address of aStr's
 *                      internal buffer
 * @param aTerminated   if non-null, this out param will be set to indicate
 *                      whether or not aStr's internal buffer is null-
 *                      terminated
 * @return              length of aStr's internal buffer
 *
 * @status FROZEN
 */
NS_STRINGAPI(PRUint32)
NS_StringGetData
  (const nsAString &aStr, const PRUnichar **aData,
   PRBool *aTerminated = nsnull);

/**
 * NS_StringCloneData
 *
 * This function returns a null-terminated copy of the string's
 * internal buffer.
 *
 * @param aStr          abstract string reference
 * @return              null-terminated copy of the string's internal buffer
 *                      (it must be free'd using using nsMemory::Free)
 *
 * @status FROZEN
 */
NS_STRINGAPI(PRUnichar *)
NS_StringCloneData
  (const nsAString &aStr);

/**
 * NS_StringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_StringSetData
  (nsAString &aStr, const PRUnichar *aData,
   PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_StringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass PR_UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass PR_UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_StringSetDataRange
  (nsAString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength,
   const PRUnichar *aData, PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_StringCopy
 *
 * This function makes aDestStr have the same value as aSrcStr.  It is
 * provided as an optimization.
 *
 * @param aDestStr      abstract string reference to be modified
 * @param aSrcStr       abstract string reference containing source string
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aDestStr after copying
 * data from aSrcStr.  The behavior depends on the implementation of the
 * abstract string, aDestStr.  If aDestStr is a reference to a
 * nsStringContainer, then its data will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_StringCopy
  (nsAString &aDestStr, const nsAString &aSrcStr);

/**
 * NS_StringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_StringAppendData(nsAString &aStr, const PRUnichar *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  return NS_StringSetDataRange(aStr, PR_UINT32_MAX, 0, aData, aDataLength);
}

/**
 * NS_StringInsertData
 *
 * This function inserts data into the existing value of aStr at the specified
 * offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aOffset       specifies where in the string to insert aData
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_StringInsertData(nsAString &aStr, PRUint32 aOffset, const PRUnichar *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  return NS_StringSetDataRange(aStr, aOffset, 0, aData, aDataLength);
}

/**
 * NS_StringCutData
 *
 * This function shortens the existing value of aStr, by removing characters
 * at the specified offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aCutOffset    specifies where in the string to insert aData
 * @param aCutLength    number of characters to remove
 * @return              NS_OK if function succeeded
 */
inline NS_HIDDEN_(nsresult)
NS_StringCutData(nsAString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength)
{
  return NS_StringSetDataRange(aStr, aCutOffset, aCutLength, nsnull, 0);
}

/* ------------------------------------------------------------------------- */

/**
 * nsCStringContainer
 *
 * This is an opaque data type that is large enough to hold the canonical
 * implementation of nsACString.  The binary structure of this class is an
 * implementation detail.
 *
 * The string data stored in a string container is always single fragment
 * and null-terminated.
 *
 * @see nsStringContainer for use cases and further documentation.
 */
class nsCStringContainer;

/**
 * NS_CStringContainerInit
 *
 * @param aContainer    string container reference
 * @return              NS_OK if string container successfully initialized
 *
 * This function may allocate additional memory for aContainer.  When
 * aContainer is no longer needed, NS_CStringContainerFinish should be called.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringContainerInit(nsCStringContainer &aContainer);

/**
 * NS_CStringContainerFinish
 *
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 *
 * @status FROZEN
 */
NS_STRINGAPI(void)
NS_CStringContainerFinish(nsCStringContainer &aContainer);

/* ------------------------------------------------------------------------- */

/**
 * NS_CStringGetData
 *
 * This function returns a const character pointer to the string's internal
 * buffer, the length of the string, and a boolean value indicating whether
 * or not the buffer is null-terminated.
 *
 * @param aStr          abstract string reference
 * @param aData         out param that will hold the address of aStr's
 *                      internal buffer
 * @param aTerminated   if non-null, this out param will be set to indicate
 *                      whether or not aStr's internal buffer is null-
 *                      terminated
 * @return              length of aStr's internal buffer
 *
 * @status FROZEN
 */
NS_STRINGAPI(PRUint32)
NS_CStringGetData
  (const nsACString &aStr, const char **aData,
   PRBool *aTerminated = nsnull);

/**
 * NS_CStringCloneData
 *
 * This function returns a null-terminated copy of the string's
 * internal buffer.
 *
 * @param aStr          abstract string reference
 * @return              null-terminated copy of the string's internal buffer
 *                      (it must be free'd using using nsMemory::Free)
 *
 * @status FROZEN
 */
NS_STRINGAPI(char *)
NS_CStringCloneData
  (const nsACString &aStr);

/**
 * NS_CStringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringSetData
  (nsACString &aStr, const char *aData,
   PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_CStringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass PR_UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass PR_UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringSetDataRange
  (nsACString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength,
   const char *aData, PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_CStringCopy
 *
 * This function makes aDestStr have the same value as aSrcStr.  It is
 * provided as an optimization.
 *
 * @param aDestStr      abstract string reference to be modified
 * @param aSrcStr       abstract string reference containing source string
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aDestStr after copying
 * data from aSrcStr.  The behavior depends on the implementation of the
 * abstract string, aDestStr.  If aDestStr is a reference to a
 * nsStringContainer, then its data will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringCopy
  (nsACString &aDestStr, const nsACString &aSrcStr);

/**
 * NS_CStringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_CStringAppendData(nsACString &aStr, const char *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  return NS_CStringSetDataRange(aStr, PR_UINT32_MAX, 0, aData, aDataLength);
}

/**
 * NS_CStringInsertData
 *
 * This function inserts data into the existing value of aStr at the specified
 * offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aOffset       specifies where in the string to insert aData
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_CStringInsertData(nsACString &aStr, PRUint32 aOffset, const char *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  return NS_CStringSetDataRange(aStr, aOffset, 0, aData, aDataLength);
}

/**
 * NS_CStringCutData
 *
 * This function shortens the existing value of aStr, by removing characters
 * at the specified offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aCutOffset    specifies where in the string to insert aData
 * @param aCutLength    number of characters to remove
 * @return              NS_OK if function succeeded
 */
inline NS_HIDDEN_(nsresult)
NS_CStringCutData(nsACString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength)
{
  return NS_CStringSetDataRange(aStr, aCutOffset, aCutLength, nsnull, 0);
}

/* ------------------------------------------------------------------------- */

/**
 * Encodings that can be used with the following conversion routines.
 */
enum nsCStringEncoding {
  /* Conversion between ASCII and UTF-16 assumes that all bytes in the source
   * string are 7-bit ASCII and can be inflated to UTF-16 by inserting null
   * bytes.  Reverse conversion is done by truncating every other byte.  The
   * conversion may result in loss and/or corruption of information if the
   * strings do not strictly contain ASCII data. */
  NS_CSTRING_ENCODING_ASCII = 0,

  /* Conversion between UTF-8 and UTF-16 is non-lossy. */
  NS_CSTRING_ENCODING_UTF8 = 1,

  /* Conversion from UTF-16 to the native filesystem charset may result in a
   * loss of information.  No attempt is made to protect against data loss in
   * this case.  The native filesystem charset applies to strings passed to
   * the "Native" method variants on nsIFile and nsILocalFile. */
  NS_CSTRING_ENCODING_NATIVE_FILESYSTEM = 2
};

/**
 * NS_CStringToUTF16
 *
 * This function converts the characters in a nsACString to an array of UTF-16
 * characters, in the platform endianness.  The result is stored in a nsAString
 * object.
 *
 * @param aSource       abstract string reference containing source string
 * @param aSrcEncoding  character encoding of the source string
 * @param aDest         abstract string reference to hold the result
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringToUTF16(const nsACString &aSource, nsCStringEncoding aSrcEncoding,
                  nsAString &aDest);

/**
 * NS_UTF16ToCString
 *
 * This function converts the UTF-16 characters in a nsAString to a single-byte
 * encoding.  The result is stored in a nsACString object.  In some cases this
 * conversion may be lossy.  In such cases, the conversion may succeed with a
 * return code indicating loss of information.  The exact behavior is not
 * specified at this time.
 *
 * @param aSource       abstract string reference containing source string
 * @param aDestEncoding character encoding of the resulting string
 * @param aDest         abstract string reference to hold the result
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_UTF16ToCString(const nsAString &aSource, nsCStringEncoding aDestEncoding,
                  nsACString &aDest);

/* ------------------------------------------------------------------------- */

/**
 * Below we define nsAString and nsACString.  The "_external" suffix is an
 * implementation detail.  nsAString_external is the name of the external
 * representation of nsAString from the point of view of the Mozilla codebase.
 * To a user of this API, nsAString_external is exactly nsAString.
 *
 * These classes should be treated as abstract classes with unspecified
 * structure.  The inline methods are provided as helper functions around the
 * C-style API provided above.
 *
 * Do not try to mix these definitions of nsAString and nsACString with the
 * internal definition of these classes from nsAString.h in the Mozilla tree.
 */

#ifndef NS_STRINGAPI_IMPL
#define nsAString_external nsAString
#define nsACString_external nsACString
#endif

class nsAString_external
{
#ifndef NS_STRINGAPI_IMPL

public:
  typedef PRUnichar             char_type;
  typedef nsAString_external    self_type;
  typedef PRUint32              size_type;
  typedef PRUint32              index_type;

  NS_HIDDEN_(const char_type*) BeginReading() const
  {
    const char_type *data;
    NS_StringGetData(*this, &data);
    return data;
  }

  NS_HIDDEN_(const char_type*) EndReading() const
  {
    const char_type *data;
    PRUint32 len = NS_StringGetData(*this, &data);
    return data + len;
  }

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_StringGetData(*this, &data);
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

  NS_HIDDEN_(void) Append( char_type c )                                                              { Replace(size_type(-1), 0, c); }
  NS_HIDDEN_(void) Append( const char_type* data, size_type length = size_type(-1) )                  { Replace(size_type(-1), 0, data, length); }
  NS_HIDDEN_(void) Append( const self_type& readable )                                                { Replace(size_type(-1), 0, readable); }

  NS_HIDDEN_(self_type&) operator+=( char_type c )                                                    { Append(c);        return *this; }
  NS_HIDDEN_(self_type&) operator+=( const char_type* data )                                          { Append(data);     return *this; }
  NS_HIDDEN_(self_type&) operator+=( const self_type& readable )                                      { Append(readable); return *this; }

  NS_HIDDEN_(void) Insert( char_type c, index_type pos )                                              { Replace(pos, 0, c); }
  NS_HIDDEN_(void) Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )  { Replace(pos, 0, data, length); }
  NS_HIDDEN_(void) Insert( const self_type& readable, index_type pos )                                { Replace(pos, 0, readable); }

  NS_HIDDEN_(void) Cut( index_type cutStart, size_type cutLength )                                    { Replace(cutStart, cutLength, nsnull, 0); }

#endif // NS_STRINGAPI_IMPL

private:
  void *v;
};

class nsACString_external
{
#ifndef NS_STRINGAPI_IMPL

public:
  typedef char                  char_type;
  typedef nsACString_external   self_type;
  typedef PRUint32              size_type;
  typedef PRUint32              index_type;

  NS_HIDDEN_(const char_type*) BeginReading() const
  {
    const char_type *data;
    NS_CStringGetData(*this, &data);
    return data;
  }

  NS_HIDDEN_(const char_type*) EndReading() const
  {
    const char_type *data;
    PRUint32 len = NS_CStringGetData(*this, &data);
    return data + len;
  }

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_CStringGetData(*this, &data);
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

  NS_HIDDEN_(void) Append( char_type c )                                                              { Replace(size_type(-1), 0, c); }
  NS_HIDDEN_(void) Append( const char_type* data, size_type length = size_type(-1) )                  { Replace(size_type(-1), 0, data, length); }
  NS_HIDDEN_(void) Append( const self_type& readable )                                                { Replace(size_type(-1), 0, readable); }

  NS_HIDDEN_(self_type&) operator+=( char_type c )                                                    { Append(c);        return *this; }
  NS_HIDDEN_(self_type&) operator+=( const char_type* data )                                          { Append(data);     return *this; }
  NS_HIDDEN_(self_type&) operator+=( const self_type& readable )                                      { Append(readable); return *this; }

  NS_HIDDEN_(void) Insert( char_type c, index_type pos )                                              { Replace(pos, 0, c); }
  NS_HIDDEN_(void) Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )  { Replace(pos, 0, data, length); }
  NS_HIDDEN_(void) Insert( const self_type& readable, index_type pos )                                { Replace(pos, 0, readable); }

  NS_HIDDEN_(void) Cut( index_type cutStart, size_type cutLength )                                    { Replace(cutStart, cutLength, nsnull, 0); }

#endif // NS_STRINGAPI_IMPL

private:
  void *v;
};

/* ------------------------------------------------------------------------- */

/**
 * Below we define nsStringContainer and nsCStringContainer.  These classes
 * have unspecified structure.  In most cases, your code should use
 * nsEmbedString instead of these classes; however, if you prefer C-style
 * programming, then look no further...
 */

class nsStringContainer : public nsAString_external
{
private:
  void     *d1;
  PRUint32  d2;
  void     *d3;

public:
  nsStringContainer() {} // MSVC6 needs this
};

class nsCStringContainer : public nsACString_external
{
private:
  void    *d1;
  PRUint32 d2;
  void    *d3;

public:
  nsCStringContainer() {} // MSVC6 needs this
};

#endif // nsStringAPI_h__
