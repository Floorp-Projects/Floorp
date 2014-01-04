/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsCharTraits.h"

#include "nsXPCOMStrings.h"
#include "nsNativeCharsetUtils.h"

/* ------------------------------------------------------------------------- */

XPCOM_API(nsresult)
NS_StringContainerInit(nsStringContainer &aContainer)
{
  NS_ASSERTION(sizeof(nsStringContainer_base) >= sizeof(nsString),
      "nsStringContainer is not large enough");

  // use placement new to avoid heap allocating nsString object
  new (&aContainer) nsString();

  return NS_OK;
}

XPCOM_API(nsresult)
NS_StringContainerInit2(nsStringContainer &aContainer,
                        const char16_t   *aData,
                        uint32_t           aDataLength,
                        uint32_t           aFlags)
{
  NS_ASSERTION(sizeof(nsStringContainer_base) >= sizeof(nsString),
      "nsStringContainer is not large enough");

  if (!aData)
  {
    new (&aContainer) nsString();
  }
  else
  {
    if (aDataLength == UINT32_MAX)
    {
      if (NS_WARN_IF(aFlags & NS_STRING_CONTAINER_INIT_SUBSTRING))
	return NS_ERROR_INVALID_ARG;
      aDataLength = nsCharTraits<char16_t>::length(aData);
    }

    if (aFlags & (NS_STRING_CONTAINER_INIT_DEPEND |
                  NS_STRING_CONTAINER_INIT_ADOPT))
    {
      uint32_t flags;
      if (aFlags & NS_STRING_CONTAINER_INIT_SUBSTRING)
        flags = nsSubstring::F_NONE;
      else
        flags = nsSubstring::F_TERMINATED;

      if (aFlags & NS_STRING_CONTAINER_INIT_ADOPT)
        flags |= nsSubstring::F_OWNED;

      new (&aContainer) nsSubstring(const_cast<char16_t *>(aData),
                                    aDataLength, flags);
    }
    else
    {
      new (&aContainer) nsString(aData, aDataLength);
    }
  }

  return NS_OK;
}

XPCOM_API(void)
NS_StringContainerFinish(nsStringContainer &aContainer)
{
  // call the nsString dtor
  reinterpret_cast<nsString *>(&aContainer)->~nsString();
}

/* ------------------------------------------------------------------------- */

XPCOM_API(uint32_t)
NS_StringGetData(const nsAString &aStr, const char16_t **aData,
                 bool *aTerminated)
{
  if (aTerminated)
    *aTerminated = aStr.IsTerminated();

  nsAString::const_iterator begin;
  aStr.BeginReading(begin);
  *aData = begin.get();
  return begin.size_forward();
}

XPCOM_API(uint32_t)
NS_StringGetMutableData(nsAString &aStr, uint32_t aDataLength,
                        char16_t **aData)
{
  if (aDataLength != UINT32_MAX) {
    aStr.SetLength(aDataLength);
    if (aStr.Length() != aDataLength) {
      *aData = nullptr;
      return 0;
    }
  }

  nsAString::iterator begin;
  aStr.BeginWriting(begin);
  *aData = begin.get();
  return begin.size_forward();
}

XPCOM_API(char16_t *)
NS_StringCloneData(const nsAString &aStr)
{
  return ToNewUnicode(aStr);
}

XPCOM_API(nsresult)
NS_StringSetData(nsAString &aStr, const char16_t *aData, uint32_t aDataLength)
{
  aStr.Assign(aData, aDataLength);
  return NS_OK; // XXX report errors
}

XPCOM_API(nsresult)
NS_StringSetDataRange(nsAString &aStr,
                      uint32_t aCutOffset, uint32_t aCutLength,
                      const char16_t *aData, uint32_t aDataLength)
{
  if (aCutOffset == UINT32_MAX)
  {
    // append case
    if (aData)
      aStr.Append(aData, aDataLength);
    return NS_OK; // XXX report errors
  }

  if (aCutLength == UINT32_MAX)
    aCutLength = aStr.Length() - aCutOffset;

  if (aData)
  {
    if (aDataLength == UINT32_MAX)
      aStr.Replace(aCutOffset, aCutLength, nsDependentString(aData));
    else
      aStr.Replace(aCutOffset, aCutLength, Substring(aData, aDataLength));
  }
  else
    aStr.Cut(aCutOffset, aCutLength);

  return NS_OK; // XXX report errors
}

XPCOM_API(nsresult)
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
  aDest.Assign(aSrc);
  return NS_OK; // XXX report errors
}

XPCOM_API(void)
NS_StringSetIsVoid(nsAString &aStr, const bool aIsVoid)
{
  aStr.SetIsVoid(aIsVoid);
}

XPCOM_API(bool)
NS_StringGetIsVoid(const nsAString &aStr)
{
  return aStr.IsVoid();
}

/* ------------------------------------------------------------------------- */

XPCOM_API(nsresult)
NS_CStringContainerInit(nsCStringContainer &aContainer)
{
  NS_ASSERTION(sizeof(nsStringContainer_base) >= sizeof(nsCString),
      "nsCStringContainer is not large enough");

  // use placement new to avoid heap allocating nsCString object
  new (&aContainer) nsCString();

  return NS_OK;
}

XPCOM_API(nsresult)
NS_CStringContainerInit2(nsCStringContainer &aContainer,
                         const char         *aData,
                         uint32_t            aDataLength,
                         uint32_t            aFlags)
{
  NS_ASSERTION(sizeof(nsStringContainer_base) >= sizeof(nsCString),
      "nsStringContainer is not large enough");

  if (!aData)
  {
    new (&aContainer) nsCString();
  }
  else
  {
    if (aDataLength == UINT32_MAX)
    {
      if (NS_WARN_IF(aFlags & NS_CSTRING_CONTAINER_INIT_SUBSTRING))
	return NS_ERROR_INVALID_ARG;
      aDataLength = nsCharTraits<char>::length(aData);
    }

    if (aFlags & (NS_CSTRING_CONTAINER_INIT_DEPEND |
                  NS_CSTRING_CONTAINER_INIT_ADOPT))
    {
      uint32_t flags;
      if (aFlags & NS_CSTRING_CONTAINER_INIT_SUBSTRING)
        flags = nsCSubstring::F_NONE;
      else
        flags = nsCSubstring::F_TERMINATED;

      if (aFlags & NS_CSTRING_CONTAINER_INIT_ADOPT)
        flags |= nsCSubstring::F_OWNED;

      new (&aContainer) nsCSubstring(const_cast<char *>(aData),
                                     aDataLength, flags);
    }
    else
    {
      new (&aContainer) nsCString(aData, aDataLength);
    }
  }

  return NS_OK;
}

XPCOM_API(void)
NS_CStringContainerFinish(nsCStringContainer &aContainer)
{
  // call the nsCString dtor
  reinterpret_cast<nsCString *>(&aContainer)->~nsCString();
}

/* ------------------------------------------------------------------------- */

XPCOM_API(uint32_t)
NS_CStringGetData(const nsACString &aStr, const char **aData,
                  bool *aTerminated)
{
  if (aTerminated)
    *aTerminated = aStr.IsTerminated();

  nsACString::const_iterator begin;
  aStr.BeginReading(begin);
  *aData = begin.get();
  return begin.size_forward();
}

XPCOM_API(uint32_t)
NS_CStringGetMutableData(nsACString &aStr, uint32_t aDataLength, char **aData)
{
  if (aDataLength != UINT32_MAX) {
    aStr.SetLength(aDataLength);
    if (aStr.Length() != aDataLength) {
      *aData = nullptr;
      return 0;
    }
  }

  nsACString::iterator begin;
  aStr.BeginWriting(begin);
  *aData = begin.get();
  return begin.size_forward();
}

XPCOM_API(char *)
NS_CStringCloneData(const nsACString &aStr)
{
  return ToNewCString(aStr);
}

XPCOM_API(nsresult)
NS_CStringSetData(nsACString &aStr, const char *aData, uint32_t aDataLength)
{
  aStr.Assign(aData, aDataLength);
  return NS_OK; // XXX report errors
}

XPCOM_API(nsresult)
NS_CStringSetDataRange(nsACString &aStr,
                       uint32_t aCutOffset, uint32_t aCutLength,
                       const char *aData, uint32_t aDataLength)
{
  if (aCutOffset == UINT32_MAX)
  {
    // append case
    if (aData)
      aStr.Append(aData, aDataLength);
    return NS_OK; // XXX report errors
  }

  if (aCutLength == UINT32_MAX)
    aCutLength = aStr.Length() - aCutOffset;

  if (aData)
  {
    if (aDataLength == UINT32_MAX)
      aStr.Replace(aCutOffset, aCutLength, nsDependentCString(aData));
    else
      aStr.Replace(aCutOffset, aCutLength, Substring(aData, aDataLength));
  }
  else
    aStr.Cut(aCutOffset, aCutLength);

  return NS_OK; // XXX report errors
}

XPCOM_API(nsresult)
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
  aDest.Assign(aSrc);
  return NS_OK; // XXX report errors
}

XPCOM_API(void)
NS_CStringSetIsVoid(nsACString &aStr, const bool aIsVoid)
{
  aStr.SetIsVoid(aIsVoid);
}

XPCOM_API(bool)
NS_CStringGetIsVoid(const nsACString &aStr)
{
  return aStr.IsVoid();
}

/* ------------------------------------------------------------------------- */

XPCOM_API(nsresult)
NS_CStringToUTF16(const nsACString &aSrc,
                  nsCStringEncoding aSrcEncoding,
                  nsAString &aDest)
{
  switch (aSrcEncoding)
  {
    case NS_CSTRING_ENCODING_ASCII:
      CopyASCIItoUTF16(aSrc, aDest);
      break;
    case NS_CSTRING_ENCODING_UTF8:
      CopyUTF8toUTF16(aSrc, aDest);
      break;
    case NS_CSTRING_ENCODING_NATIVE_FILESYSTEM:
      NS_CopyNativeToUnicode(aSrc, aDest);
      break;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK; // XXX report errors
}

XPCOM_API(nsresult)
NS_UTF16ToCString(const nsAString &aSrc,
                  nsCStringEncoding aDestEncoding,
                  nsACString &aDest)
{
  switch (aDestEncoding)
  {
    case NS_CSTRING_ENCODING_ASCII:
      LossyCopyUTF16toASCII(aSrc, aDest);
      break;
    case NS_CSTRING_ENCODING_UTF8:
      CopyUTF16toUTF8(aSrc, aDest);
      break;
    case NS_CSTRING_ENCODING_NATIVE_FILESYSTEM:
      NS_CopyUnicodeToNative(aSrc, aDest);
      break;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK; // XXX report errors
}
