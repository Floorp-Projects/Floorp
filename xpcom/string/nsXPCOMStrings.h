/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOMStrings_h__
#define nsXPCOMStrings_h__

#include <string.h>
#include "nscore.h"
#include <limits>

/**
 * nsXPCOMStrings.h
 *
 * This file describes a minimal API for working with XPCOM's abstract
 * string classes.  It divorces the consumer from having any run-time
 * dependency on the implementation details of the abstract string types.
 */

#include "nscore.h"

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
 *   nsresult GetBlah(nsAString &aBlah);
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
 *       const char16_t *data;
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
 *   nsresult SetBlah(const nsAString &aBlah);
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
 *     const char16_t kData[] = {'x','y','z','\0'};
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
 * This struct is never used directly. It is designed to have the same
 * size as nsString. It can be stack and heap allocated and the internal
 * functions cast it to nsString.
 * While this practice is a strict aliasing violation, it doesn't seem to
 * cause problems since the the struct is only accessed via the casts to
 * nsString.
 * We use protected instead of private to avoid compiler warnings about
 * the members being unused.
 */
struct nsStringContainer_base
{
protected:
  void* d1;
  uint32_t d2;
  uint32_t d3;
};

/**
 * Flags that may be OR'd together to pass to NS_StringContainerInit2:
 */
enum
{
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
 */
XPCOM_API(nsresult) NS_StringContainerInit(nsStringContainer& aContainer);

/**
 * NS_StringContainerInit2
 *
 * @param aContainer    string container reference
 * @param aData         character buffer (may be null)
 * @param aDataLength   number of characters stored at aData (may pass
 *                      UINT32_MAX if aData is null-terminated)
 * @param aFlags        flags affecting how the string container is
 *                      initialized.  this parameter is ignored when aData
 *                      is null.  otherwise, if this parameter is 0, then
 *                      aData is copied into the string.
 *
 * This function resembles NS_StringContainerInit but provides further
 * options that permit more efficient memory usage.  When aContainer is
 * no longer needed, NS_StringContainerFinish should be called.
 *
 * NOTE: NS_StringContainerInit2(container, nullptr, 0, 0) is equivalent to
 * NS_StringContainerInit(container).
 */
XPCOM_API(nsresult) NS_StringContainerInit2(nsStringContainer& aContainer,
                                            const char16_t* aData = nullptr,
                                            uint32_t aDataLength = UINT32_MAX,
                                            uint32_t aFlags = 0);

/**
 * NS_StringContainerFinish
 *
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 */
XPCOM_API(void) NS_StringContainerFinish(nsStringContainer& aContainer);

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
 */
XPCOM_API(uint32_t) NS_StringGetData(const nsAString& aStr,
                                     const char16_t** aData,
                                     bool* aTerminated = nullptr);

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
 * string's internal buffer.  By passing some value other than UINT32_MAX,
 * the caller can request that the buffer be resized to the specified number of
 * characters before returning.  The caller is not responsible for writing a
 * null-terminator.
 *
 * @param aStr          abstract string reference
 * @param aDataLength   number of characters to resize the string's internal
 *                      buffer to or UINT32_MAX if no resizing is needed
 * @param aData         out param that upon return holds the address of aStr's
 *                      internal buffer or null if the function failed
 * @return              number of characters or zero if the function failed
 *
 * This function does not necessarily null-terminate aStr after resizing its
 * internal buffer.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(uint32_t) NS_StringGetMutableData(nsAString& aStr,
                                            uint32_t aDataLength,
                                            char16_t** aData);

/**
 * NS_StringCloneData
 *
 * This function returns a null-terminated copy of the string's
 * internal buffer.
 *
 * @param aStr          abstract string reference
 * @return              null-terminated copy of the string's internal buffer
 *                      (it must be free'd using using free)
 */
XPCOM_API(char16_t*) NS_StringCloneData(const nsAString& aStr);

/**
 * NS_StringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(nsresult) NS_StringSetData(nsAString& aStr, const char16_t* aData,
                                     uint32_t aDataLength = UINT32_MAX);

/**
 * NS_StringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(nsresult) NS_StringSetDataRange(nsAString& aStr,
                                          uint32_t aCutOffset, uint32_t aCutLength,
                                          const char16_t* aData,
                                          uint32_t aDataLength = UINT32_MAX);

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
 */
XPCOM_API(nsresult) NS_StringCopy(nsAString& aDestStr,
                                  const nsAString& aSrcStr);

/**
 * NS_StringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_StringAppendData(nsAString& aStr, const char16_t* aData,
                    uint32_t aDataLength = UINT32_MAX)
{
  return NS_StringSetDataRange(aStr, UINT32_MAX, 0, aData, aDataLength);
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
 * @param aDataLength   number of characters to append (pass UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_StringInsertData(nsAString& aStr, uint32_t aOffset, const char16_t* aData,
                    uint32_t aDataLength = UINT32_MAX)
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
NS_StringCutData(nsAString& aStr, uint32_t aCutOffset, uint32_t aCutLength)
{
  return NS_StringSetDataRange(aStr, aCutOffset, aCutLength, nullptr, 0);
}

/**
 * NS_StringSetIsVoid
 *
 * This function marks a string as being a "void string".  Any data in the
 * string will be lost.
 */
XPCOM_API(void) NS_StringSetIsVoid(nsAString& aStr, const bool aIsVoid);

/**
 * NS_StringGetIsVoid
 *
 * This function provides a way to test if a string is a "void string", as
 * marked by NS_StringSetIsVoid.
 */
XPCOM_API(bool) NS_StringGetIsVoid(const nsAString& aStr);

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
enum
{
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
 */
XPCOM_API(nsresult) NS_CStringContainerInit(nsCStringContainer& aContainer);

/**
 * NS_CStringContainerInit2
 *
 * @param aContainer    string container reference
 * @param aData         character buffer (may be null)
 * @param aDataLength   number of characters stored at aData (may pass
 *                      UINT32_MAX if aData is null-terminated)
 * @param aFlags        flags affecting how the string container is
 *                      initialized.  this parameter is ignored when aData
 *                      is null.  otherwise, if this parameter is 0, then
 *                      aData is copied into the string.
 *
 * This function resembles NS_CStringContainerInit but provides further
 * options that permit more efficient memory usage.  When aContainer is
 * no longer needed, NS_CStringContainerFinish should be called.
 *
 * NOTE: NS_CStringContainerInit2(container, nullptr, 0, 0) is equivalent to
 * NS_CStringContainerInit(container).
 */
XPCOM_API(nsresult) NS_CStringContainerInit2(nsCStringContainer& aContainer,
                                             const char* aData = nullptr,
                                             uint32_t aDataLength = UINT32_MAX,
                                             uint32_t aFlags = 0);

/**
 * NS_CStringContainerFinish
 *
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 */
XPCOM_API(void) NS_CStringContainerFinish(nsCStringContainer& aContainer);

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
 */
XPCOM_API(uint32_t) NS_CStringGetData(const nsACString& aStr,
                                      const char** aData,
                                      bool* aTerminated = nullptr);

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
 * string's internal buffer.  By passing some value other than UINT32_MAX,
 * the caller can request that the buffer be resized to the specified number of
 * characters before returning.  The caller is not responsible for writing a
 * null-terminator.
 *
 * @param aStr          abstract string reference
 * @param aDataLength   number of characters to resize the string's internal
 *                      buffer to or UINT32_MAX if no resizing is needed
 * @param aData         out param that upon return holds the address of aStr's
 *                      internal buffer or null if the function failed
 * @return              number of characters or zero if the function failed
 *
 * This function does not necessarily null-terminate aStr after resizing its
 * internal buffer.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(uint32_t) NS_CStringGetMutableData(nsACString& aStr,
                                             uint32_t aDataLength,
                                             char** aData);

/**
 * NS_CStringCloneData
 *
 * This function returns a null-terminated copy of the string's
 * internal buffer.
 *
 * @param aStr          abstract string reference
 * @return              null-terminated copy of the string's internal buffer
 *                      (it must be free'd using using free)
 */
XPCOM_API(char*) NS_CStringCloneData(const nsACString& aStr);

/**
 * NS_CStringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(nsresult) NS_CStringSetData(nsACString& aStr, const char* aData,
                                      uint32_t aDataLength = UINT32_MAX);

/**
 * NS_CStringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
XPCOM_API(nsresult) NS_CStringSetDataRange(nsACString& aStr,
                                           uint32_t aCutOffset,
                                           uint32_t aCutLength,
                                           const char* aData,
                                           uint32_t aDataLength = UINT32_MAX);

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
 */
XPCOM_API(nsresult) NS_CStringCopy(nsACString& aDestStr,
                                   const nsACString& aSrcStr);

/**
 * NS_CStringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_CStringAppendData(nsACString& aStr, const char* aData,
                     uint32_t aDataLength = UINT32_MAX)
{
  return NS_CStringSetDataRange(aStr, UINT32_MAX, 0, aData, aDataLength);
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
 * @param aDataLength   number of characters to append (pass UINT32_MAX to
 *                      append until a null-character is encountered)
 * @return              NS_OK if function succeeded
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline NS_HIDDEN_(nsresult)
NS_CStringInsertData(nsACString& aStr, uint32_t aOffset, const char* aData,
                     uint32_t aDataLength = UINT32_MAX)
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
NS_CStringCutData(nsACString& aStr, uint32_t aCutOffset, uint32_t aCutLength)
{
  return NS_CStringSetDataRange(aStr, aCutOffset, aCutLength, nullptr, 0);
}

/**
 * NS_CStringSetIsVoid
 *
 * This function marks a string as being a "void string".  Any data in the
 * string will be lost.
 */
XPCOM_API(void) NS_CStringSetIsVoid(nsACString& aStr, const bool aIsVoid);

/**
 * NS_CStringGetIsVoid
 *
 * This function provides a way to test if a string is a "void string", as
 * marked by NS_CStringSetIsVoid.
 */
XPCOM_API(bool) NS_CStringGetIsVoid(const nsACString& aStr);

/* ------------------------------------------------------------------------- */

/**
 * Encodings that can be used with the following conversion routines.
 */
enum nsCStringEncoding
{
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
   * the "Native" method variants on nsIFile. */
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
 */
XPCOM_API(nsresult) NS_CStringToUTF16(const nsACString& aSource,
                                      nsCStringEncoding aSrcEncoding,
                                      nsAString& aDest);

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
 */
XPCOM_API(nsresult) NS_UTF16ToCString(const nsAString& aSource,
                                      nsCStringEncoding aDestEncoding,
                                      nsACString& aDest);

#endif // nsXPCOMStrings_h__
