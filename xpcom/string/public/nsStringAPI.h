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

#include <string.h>

/**
 * nsStringAPI.h
 *
 * This file describes a minimal API for working with XPCOM's abstract
 * string classes.  It divorces the consumer from having any run-time
 * dependency on the implementation details of the abstract string types.
 */

// Map frozen functions to private symbol names if not using strict API.
#ifdef MOZILLA_INTERNAL_API
# define NS_StringContainerInit           NS_StringContainerInit_P
# define NS_StringContainerInit2          NS_StringContainerInit2_P
# define NS_StringContainerFinish         NS_StringContainerFinish_P
# define NS_StringGetData                 NS_StringGetData_P
# define NS_StringGetMutableData          NS_StringGetMutableData_P
# define NS_StringCloneData               NS_StringCloneData_P
# define NS_StringSetData                 NS_StringSetData_P
# define NS_StringSetDataRange            NS_StringSetDataRange_P
# define NS_StringCopy                    NS_StringCopy_P
# define NS_CStringContainerInit          NS_CStringContainerInit_P
# define NS_CStringContainerInit2         NS_CStringContainerInit2_P
# define NS_CStringContainerFinish        NS_CStringContainerFinish_P
# define NS_CStringGetData                NS_CStringGetData_P
# define NS_CStringGetMutableData         NS_CStringGetMutableData_P
# define NS_CStringCloneData              NS_CStringCloneData_P
# define NS_CStringSetData                NS_CStringSetData_P
# define NS_CStringSetDataRange           NS_CStringSetDataRange_P
# define NS_CStringCopy                   NS_CStringCopy_P
# define NS_CStringToUTF16                NS_CStringToUTF16_P
# define NS_UTF16ToCString                NS_UTF16ToCString_P
#endif

#include "nscore.h"

#if defined( XPCOM_GLUE )
#define NS_STRINGAPI(type) extern "C" NS_HIDDEN_(type)
#elif defined( _IMPL_NS_STRINGAPI )
#define NS_STRINGAPI(type) extern "C" NS_EXPORT type
#else
#define NS_STRINGAPI(type) extern "C" NS_IMPORT type
#endif

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
 * and may be null-terminated depending on how it is initialized.
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
 * Flags that may be OR'd together to pass to NS_StringContainerInit2:
 */
enum {
  /* Data passed into NS_StringContainerInit2 is not copied; instead, the
   * string references the passed in data pointer directly.  The caller must
   * ensure that the data is valid for the lifetime of the string container.
   * This flag should not be combined with NS_STRING_CONTAINER_INIT_ADOPT. */
  NS_STRING_CONTAINER_INIT_DEPEND    = (1 << 1),

  /* Data passed into NS_StringContainerInit2 is not copied; instead, the
   * string takes ownership over the data pointer.  The caller must have
   * allocated the data array using the XPCOM memory allocator (nsMemory).
   * This flag should not be combined with NS_STRING_CONTAINER_INIT_DEPEND. */
  NS_STRING_CONTAINER_INIT_ADOPT     = (1 << 2),

  /* Data passed into NS_StringContainerInit2 is a substring that is not
   * null-terminated. */
  NS_STRING_CONTAINER_INIT_SUBSTRING = (1 << 3)
};

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
 * NS_StringContainerInit2
 *
 * @param aContainer    string container reference
 * @param aData         character buffer (may be null)
 * @param aDataLength   number of characters stored at aData (may pass
 *                      PR_UINT32_MAX if aData is null-terminated)
 * @param aFlags        flags affecting how the string container is
 *                      initialized.  this parameter is ignored when aData
 *                      is null.  otherwise, if this parameter is 0, then
 *                      aData is copied into the string.
 *
 * This function resembles NS_StringContainerInit but provides further
 * options that permit more efficient memory usage.  When aContainer is
 * no longer needed, NS_StringContainerFinish should be called.
 *
 * NOTE: NS_StringContainerInit2(container, nsnull, 0, 0) is equivalent to
 * NS_StringContainerInit(container).
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_StringContainerInit2
  (nsStringContainer &aContainer, const PRUnichar *aData = nsnull,
   PRUint32 aDataLength = PR_UINT32_MAX, PRUint32 aFlags = 0);

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
 * NS_StringGetMutableData
 *
 * This function provides mutable access to a string's internal buffer.  It
 * returns a pointer to an array of characters that may be modified.  The
 * returned pointer remains valid until the string object is passed to some
 * other string function.
 *
 * Optionally, this function may be used to resize the string's internal
 * buffer.  The aDataLength parameter specifies the requested length of the
 * string's internal buffer.  By passing some value other than PR_UINT32_MAX,
 * the caller can request that the buffer be resized to the specified number of
 * characters before returning.  The caller is not responsible for writing a
 * null-terminator.
 *
 * @param aStr          abstract string reference
 * @param aDataLength   number of characters to resize the string's internal
 *                      buffer to or PR_UINT32_MAX if no resizing is needed
 * @param aData         out param that upon return holds the address of aStr's
 *                      internal buffer or null if the function failed
 * @return              number of characters or zero if the function failed
 *
 * This function does not necessarily null-terminate aStr after resizing its
 * internal buffer.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(PRUint32)
NS_StringGetMutableData
  (nsAString &aStr, PRUint32 aDataLength, PRUnichar **aData);

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
 * and may be null-terminated depending on how it is initialized.
 *
 * @see nsStringContainer for use cases and further documentation.
 */
class nsCStringContainer;

/**
 * Flags that may be OR'd together to pass to NS_StringContainerInit2:
 */
enum {
  /* Data passed into NS_CStringContainerInit2 is not copied; instead, the
   * string references the passed in data pointer directly.  The caller must
   * ensure that the data is valid for the lifetime of the string container.
   * This flag should not be combined with NS_CSTRING_CONTAINER_INIT_ADOPT. */
  NS_CSTRING_CONTAINER_INIT_DEPEND    = (1 << 1),

  /* Data passed into NS_CStringContainerInit2 is not copied; instead, the
   * string takes ownership over the data pointer.  The caller must have
   * allocated the data array using the XPCOM memory allocator (nsMemory).
   * This flag should not be combined with NS_CSTRING_CONTAINER_INIT_DEPEND. */
  NS_CSTRING_CONTAINER_INIT_ADOPT     = (1 << 2),

  /* Data passed into NS_CStringContainerInit2 is a substring that is not
   * null-terminated. */
  NS_CSTRING_CONTAINER_INIT_SUBSTRING = (1 << 3)
};

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
 * NS_CStringContainerInit2
 *
 * @param aContainer    string container reference
 * @param aData         character buffer (may be null)
 * @param aDataLength   number of characters stored at aData (may pass
 *                      PR_UINT32_MAX if aData is null-terminated)
 * @param aFlags        flags affecting how the string container is
 *                      initialized.  this parameter is ignored when aData
 *                      is null.  otherwise, if this parameter is 0, then
 *                      aData is copied into the string.
 *
 * This function resembles NS_CStringContainerInit but provides further
 * options that permit more efficient memory usage.  When aContainer is
 * no longer needed, NS_CStringContainerFinish should be called.
 *
 * NOTE: NS_CStringContainerInit2(container, nsnull, 0, 0) is equivalent to
 * NS_CStringContainerInit(container).
 *
 * @status FROZEN
 */
NS_STRINGAPI(nsresult)
NS_CStringContainerInit2
  (nsCStringContainer &aContainer, const char *aData = nsnull,
   PRUint32 aDataLength = PR_UINT32_MAX, PRUint32 aFlags = 0);

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
 * NS_CStringGetMutableData
 *
 * This function provides mutable access to a string's internal buffer.  It
 * returns a pointer to an array of characters that may be modified.  The
 * returned pointer remains valid until the string object is passed to some
 * other string function.
 *
 * Optionally, this function may be used to resize the string's internal
 * buffer.  The aDataLength parameter specifies the requested length of the
 * string's internal buffer.  By passing some value other than PR_UINT32_MAX,
 * the caller can request that the buffer be resized to the specified number of
 * characters before returning.  The caller is not responsible for writing a
 * null-terminator.
 *
 * @param aStr          abstract string reference
 * @param aDataLength   number of characters to resize the string's internal
 *                      buffer to or PR_UINT32_MAX if no resizing is needed
 * @param aData         out param that upon return holds the address of aStr's
 *                      internal buffer or null if the function failed
 * @return              number of characters or zero if the function failed
 *
 * This function does not necessarily null-terminate aStr after resizing its
 * internal buffer.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 *
 * @status FROZEN
 */
NS_STRINGAPI(PRUint32)
NS_CStringGetMutableData
  (nsACString &aStr, PRUint32 aDataLength, char **aData);

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

#ifndef MOZILLA_INTERNAL_API
#define nsAString_external nsAString
#define nsACString_external nsACString
#endif

class nsAString_external
{
#ifndef MOZILLA_INTERNAL_API

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

  NS_HIDDEN_(char_type*) BeginWriting()
  {
    char_type *data;
    NS_StringGetMutableData(*this, PR_UINT32_MAX, &data);
    return data;
  }

  NS_HIDDEN_(PRBool) SetLength(PRUint32 aLen)
  {
    char_type *data;
    NS_StringGetMutableData(*this, aLen, &data);
    return data != nsnull;
  }

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_StringGetData(*this, &data);
  }

  NS_HIDDEN_(PRBool) IsEmpty() const
  {
    return Length() == 0;
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

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  NS_HIDDEN_(PRBool) Equals( const self_type &other ) const {
    const char_type *cself;
    const char_type *cother;
    PRUint32 selflen = NS_StringGetData(*this, &cself);
    PRUint32 otherlen = NS_StringGetData(other, &cother);

    if (selflen != otherlen)
      return PR_FALSE;

    return memcmp(cself, cother, selflen * sizeof(char_type)) == 0;
  }

#endif // MOZILLA_INTERNAL_API

protected:
  // Prevent people from allocating a nsAString directly.
  ~nsAString_external() {}

private:
  void *v;
};

class nsACString_external
{
#ifndef MOZILLA_INTERNAL_API

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

  NS_HIDDEN_(char_type*) BeginWriting()
  {
    char_type *data;
    NS_CStringGetMutableData(*this, PR_UINT32_MAX, &data);
    return data;
  }

  NS_HIDDEN_(PRBool) SetLength(PRUint32 aLen)
  {
    char_type *data;
    NS_CStringGetMutableData(*this, aLen, &data);
    return data != nsnull;
  }

  NS_HIDDEN_(size_type) Length() const
  {
    const char_type* data;
    return NS_CStringGetData(*this, &data);
  }

  NS_HIDDEN_(PRBool) IsEmpty() const
  {
    return Length() == 0;
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

  NS_HIDDEN_(void) Truncate() { SetLength(0); }

  NS_HIDDEN_(PRBool) Equals( const self_type &other ) const {
    const char_type *cself;
    const char_type *cother;
    PRUint32 selflen = NS_CStringGetData(*this, &cself);
    PRUint32 otherlen = NS_CStringGetData(other, &cother);

    if (selflen != otherlen)
      return PR_FALSE;
 
    return memcmp(cself, cother, selflen * sizeof(char_type)) == 0;
  }

#endif // MOZILLA_INTERNAL_API

protected:
  // Prevent people from allocating a nsACString directly.
  ~nsACString_external() {}

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

/* ------------------------------------------------------------------------- */

/**
 * Below we define a number of inlined helper classes that make the frozen
 * string API easier to use.
 */

#ifndef MOZILLA_INTERNAL_API
#include "nsDebug.h"

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
    const char_type* data;
    NS_StringGetData(*this, &data);
    return data;
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
    const char_type* data;
    NS_CStringGetData(*this, &data);
    return data;
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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
};


/**
 * conversion classes
 */

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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
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
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
};


/**
 * literal strings
 *
 * NOTE: HAVE_CPP_2BYTE_WCHAR_T may be automatically defined for some platforms
 * in nscore.h.  On other platforms, it may be defined in xpcom-config.h.
 * Under GCC, this define should only be set if compiling with -fshort-wchar.
 */

#ifdef HAVE_CPP_2BYTE_WCHAR_T
  #define NS_LL(s)                                L##s
  #define NS_MULTILINE_LITERAL_STRING(s)          nsDependentString(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const nsDependentString n(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  typedef nsDependentString nsLiteralString;
#else
  #define NS_LL(s)                                s
  #define NS_MULTILINE_LITERAL_STRING(s)          NS_ConvertASCIItoUTF16(s, PRUint32(sizeof(s)-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(s, PRUint32(sizeof(s)-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const NS_ConvertASCIItoUTF16 n(s, PRUint32(sizeof(s)-1))
  typedef NS_ConvertASCIItoUTF16 nsLiteralString;
#endif

/*
 * Macro arguments used in concatenation or stringification won't be expanded.
 * Therefore, in order for |NS_L(FOO)| to work as expected (which is to expand
 * |FOO| before doing whatever |NS_L| needs to do to it) a helper macro needs
 * to be inserted in between to allow the macro argument to expand.
 * See "3.10.6 Separate Expansion of Macro Arguments" of the CPP manual for a
 * more accurate and precise explanation.
 */

#define NS_L(s)                                   NS_LL(s)

#define NS_LITERAL_STRING(s)                      NS_STATIC_CAST(const nsString&, NS_MULTILINE_LITERAL_STRING(NS_LL(s)))
#define NS_LITERAL_STRING_INIT(n,s)               NS_MULTILINE_LITERAL_STRING_INIT(n, NS_LL(s))
#define NS_NAMED_LITERAL_STRING(n,s)              NS_NAMED_MULTILINE_LITERAL_STRING(n, NS_LL(s))

#define NS_LITERAL_CSTRING(s)                     NS_STATIC_CAST(const nsDependentCString&, nsDependentCString(s, PRUint32(sizeof(s)-1)))
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

class nsDependentSubstring : public nsStringContainer
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
                       PRUint32 aStartPos)
  {
    const PRUnichar* data;
    PRUint32 len = NS_StringGetData(aStr, &data);
    NS_StringContainerInit2(*this, data + aStartPos, len - aStartPos,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentSubstring(const abstract_string_type& aStr,
                       PRUint32 aStartPos, PRUint32 aLength)
  {
    const PRUnichar* data;
#ifdef DEBUG
    PRUint32 len =
#endif
    NS_StringGetData(aStr, &data);
    NS_ASSERTION(aStartPos + aLength <= len, "bad length");
    NS_StringContainerInit2(*this, data + aStartPos, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

  void Rebind(const char_type *aStart, PRUint32 aLength)
  {
    NS_StringContainerFinish(*this);
    NS_StringContainerInit2(*this, aStart, aLength,
                            NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
};

class nsDependentCSubstring : public nsCStringContainer
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
                        PRUint32 aStartPos)
  {
    const char* data;
    PRUint32 len = NS_CStringGetData(aStr, &data);
    NS_CStringContainerInit2(*this, data + aStartPos, len - aStartPos,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

  nsDependentCSubstring(const abstract_string_type& aStr,
                        PRUint32 aStartPos, PRUint32 aLength)
  {
    const char* data;
#ifdef DEBUG
    PRUint32 len =
#endif
    NS_CStringGetData(aStr, &data);
    NS_ASSERTION(aStartPos + aLength <= len, "bad length");
    NS_CStringContainerInit2(*this, data + aStartPos, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

  void Rebind(const char_type *aStart, PRUint32 aLength)
  {
    NS_CStringContainerFinish(*this);
    NS_CStringContainerInit2(*this, aStart, aLength,
                             NS_CSTRING_CONTAINER_INIT_DEPEND |
                             NS_CSTRING_CONTAINER_INIT_SUBSTRING);
  }

private:
  self_type& operator=(const self_type& aString); // NOT IMPLEMENTED
};


/**
 * Various nsDependentC?Substring constructor functions
 */

// PRUnichar
inline
const nsDependentSubstring
Substring( const nsAString& str, PRUint32 startPos )
{
  return nsDependentSubstring(str, startPos);
}

inline
const nsDependentSubstring
Substring( const nsAString& str, PRUint32 startPos, PRUint32 length )
{
  return nsDependentSubstring(str, startPos, length);
}

inline
const nsDependentSubstring
Substring( const PRUnichar* start, const PRUnichar* end )
{
  return nsDependentSubstring(start, end - start);
}

inline
const nsDependentSubstring
Substring( const PRUnichar* start, PRUint32 length )
{
  return nsDependentSubstring(start, length);
}

inline
const nsDependentSubstring
StringHead( const nsAString& str, PRUint32 count )
{
  return nsDependentSubstring(str, 0, count);
}

inline
const nsDependentSubstring
StringTail( const nsAString& str, PRUint32 count )
{
  return nsDependentSubstring(str, str.Length() - count, count);
}

// char
inline
const nsDependentCSubstring
Substring( const nsACString& str, PRUint32 startPos )
{
  return nsDependentCSubstring(str, startPos);
}

inline
const nsDependentCSubstring
Substring( const nsACString& str, PRUint32 startPos, PRUint32 length )
{
  return nsDependentCSubstring(str, startPos, length);
}

inline
const nsDependentCSubstring
Substring( const char* start, const char* end )
{
  return nsDependentCSubstring(start, end - start);
}

inline
const nsDependentCSubstring
Substring( const char* start, PRUint32 length )
{
  return nsDependentCSubstring(start, length);
}

inline
const nsDependentCSubstring
StringHead( const nsACString& str, PRUint32 count )
{
  return nsDependentCSubstring(str, 0, count);
}

inline
const nsDependentCSubstring
StringTail( const nsACString& str, PRUint32 count )
{
  return nsDependentCSubstring(str, str.Length() - count, count);
}


/*
 * Canonical empty strings
 */

#define EmptyCString() nsCString()
#define EmptyString() nsString()

#endif // MOZILLA_INTERNAL_API

#endif // nsStringAPI_h__
