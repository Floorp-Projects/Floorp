/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCRTGlue.h"
#include "prprf.h"
#include "nsStringAPI.h"
#include "nsXPCOMStrings.h"
#include "nsDebug.h"

#include <stdio.h>

// nsAString

uint32_t
nsAString::BeginReading(const char_type** aBegin, const char_type** aEnd) const
{
  uint32_t len = NS_StringGetData(*this, aBegin);
  if (aEnd) {
    *aEnd = *aBegin + len;
  }

  return len;
}

const nsAString::char_type*
nsAString::BeginReading() const
{
  const char_type* data;
  NS_StringGetData(*this, &data);
  return data;
}

const nsAString::char_type*
nsAString::EndReading() const
{
  const char_type* data;
  uint32_t len = NS_StringGetData(*this, &data);
  return data + len;
}

uint32_t
nsAString::BeginWriting(char_type** aBegin, char_type** aEnd, uint32_t aNewSize)
{
  uint32_t len = NS_StringGetMutableData(*this, aNewSize, aBegin);
  if (aEnd) {
    *aEnd = *aBegin + len;
  }

  return len;
}

nsAString::char_type*
nsAString::BeginWriting(uint32_t aLen)
{
  char_type* data;
  NS_StringGetMutableData(*this, aLen, &data);
  return data;
}

nsAString::char_type*
nsAString::EndWriting()
{
  char_type* data;
  uint32_t len = NS_StringGetMutableData(*this, UINT32_MAX, &data);
  return data + len;
}

bool
nsAString::SetLength(uint32_t aLen)
{
  char_type* data;
  NS_StringGetMutableData(*this, aLen, &data);
  return data != nullptr;
}

void
nsAString::AssignLiteral(const char* aStr)
{
  uint32_t len = strlen(aStr);
  char16_t* buf = BeginWriting(len);
  if (!buf) {
    return;
  }

  for (; *aStr; ++aStr, ++buf) {
    *buf = *aStr;
  }
}

void
nsAString::AppendLiteral(const char* aASCIIStr)
{
  uint32_t appendLen = strlen(aASCIIStr);

  uint32_t thisLen = Length();
  char16_t* begin;
  char16_t* end;
  BeginWriting(&begin, &end, appendLen + thisLen);
  if (!begin) {
    return;
  }

  for (begin += thisLen; begin < end; ++begin, ++aASCIIStr) {
    *begin = *aASCIIStr;
  }
}

void
nsAString::StripChars(const char* aSet)
{
  nsString copy(*this);

  const char_type* source;
  const char_type* sourceEnd;
  copy.BeginReading(&source, &sourceEnd);

  char_type* dest;
  BeginWriting(&dest);
  if (!dest) {
    return;
  }

  char_type* curDest = dest;

  for (; source < sourceEnd; ++source) {
    const char* test;
    for (test = aSet; *test; ++test) {
      if (*source == char_type(*test)) {
        break;
      }
    }

    if (!*test) {
      // not stripped, copy this char
      *curDest = *source;
      ++curDest;
    }
  }

  SetLength(curDest - dest);
}

void
nsAString::Trim(const char* aSet, bool aLeading, bool aTrailing)
{
  NS_ASSERTION(aLeading || aTrailing, "Ineffective Trim");

  const char16_t* start;
  const char16_t* end;
  uint32_t cutLen;

  if (aLeading) {
    BeginReading(&start, &end);
    for (cutLen = 0; start < end; ++start, ++cutLen) {
      const char* test;
      for (test = aSet; *test; ++test) {
        if (*test == *start) {
          break;
        }
      }
      if (!*test) {
        break;
      }
    }
    if (cutLen) {
      NS_StringCutData(*this, 0, cutLen);
    }
  }
  if (aTrailing) {
    uint32_t len = BeginReading(&start, &end);
    --end;
    for (cutLen = 0; end >= start; --end, ++cutLen) {
      const char* test;
      for (test = aSet; *test; ++test) {
        if (*test == *end) {
          break;
        }
      }
      if (!*test) {
        break;
      }
    }
    if (cutLen) {
      NS_StringCutData(*this, len - cutLen, cutLen);
    }
  }
}

int32_t
nsAString::DefaultComparator(const char_type* aStrA, const char_type* aStrB,
                             uint32_t aLen)
{
  for (const char_type* end = aStrA + aLen; aStrA < end; ++aStrA, ++aStrB) {
    if (*aStrA == *aStrB) {
      continue;
    }

    return *aStrA < *aStrB ? -1 : 1;
  }

  return 0;
}

int32_t
nsAString::Compare(const char_type* aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  uint32_t selflen = NS_StringGetData(*this, &cself);
  uint32_t otherlen = NS_strlen(aOther);
  uint32_t comparelen = selflen <= otherlen ? selflen : otherlen;

  int32_t result = aComparator(cself, aOther, comparelen);
  if (result == 0) {
    if (selflen < otherlen) {
      return -1;
    } else if (selflen > otherlen) {
      return 1;
    }
  }
  return result;
}

int32_t
nsAString::Compare(const self_type& aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  const char_type* cother;
  uint32_t selflen = NS_StringGetData(*this, &cself);
  uint32_t otherlen = NS_StringGetData(aOther, &cother);
  uint32_t comparelen = selflen <= otherlen ? selflen : otherlen;

  int32_t result = aComparator(cself, cother, comparelen);
  if (result == 0) {
    if (selflen < otherlen) {
      return -1;
    } else if (selflen > otherlen) {
      return 1;
    }
  }
  return result;
}

bool
nsAString::Equals(const char_type* aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  uint32_t selflen = NS_StringGetData(*this, &cself);
  uint32_t otherlen = NS_strlen(aOther);

  if (selflen != otherlen) {
    return false;
  }

  return aComparator(cself, aOther, selflen) == 0;
}

bool
nsAString::Equals(const self_type& aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  const char_type* cother;
  uint32_t selflen = NS_StringGetData(*this, &cself);
  uint32_t otherlen = NS_StringGetData(aOther, &cother);

  if (selflen != otherlen) {
    return false;
  }

  return aComparator(cself, cother, selflen) == 0;
}

bool
nsAString::EqualsLiteral(const char* aASCIIString) const
{
  const char16_t* begin;
  const char16_t* end;
  BeginReading(&begin, &end);

  for (; begin < end; ++begin, ++aASCIIString) {
    if (!*aASCIIString || !NS_IsAscii(*begin) ||
        (char)*begin != *aASCIIString) {
      return false;
    }
  }

  return *aASCIIString == '\0';
}

bool
nsAString::LowerCaseEqualsLiteral(const char* aASCIIString) const
{
  const char16_t* begin;
  const char16_t* end;
  BeginReading(&begin, &end);

  for (; begin < end; ++begin, ++aASCIIString) {
    if (!*aASCIIString || !NS_IsAscii(*begin) ||
        NS_ToLower((char)*begin) != *aASCIIString) {
      return false;
    }
  }

  return *aASCIIString == '\0';
}

int32_t
nsAString::Find(const self_type& aStr, uint32_t aOffset,
                ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  if (aOffset > selflen) {
    return -1;
  }

  const char_type* other;
  uint32_t otherlen = aStr.BeginReading(&other);

  if (otherlen > selflen - aOffset) {
    return -1;
  }

  // We want to stop searching otherlen characters before the end of the string
  end -= otherlen;

  for (const char_type* cur = begin + aOffset; cur <= end; ++cur) {
    if (!aComparator(cur, other, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

static bool
ns_strnmatch(const char16_t* aStr, const char* aSubstring, uint32_t aLen)
{
  for (; aLen; ++aStr, ++aSubstring, --aLen) {
    if (!NS_IsAscii(*aStr)) {
      return false;
    }

    if ((char)*aStr != *aSubstring) {
      return false;
    }
  }

  return true;
}

static bool
ns_strnimatch(const char16_t* aStr, const char* aSubstring, uint32_t aLen)
{
  for (; aLen; ++aStr, ++aSubstring, --aLen) {
    if (!NS_IsAscii(*aStr)) {
      return false;
    }

    if (NS_ToLower((char)*aStr) != NS_ToLower(*aSubstring)) {
      return false;
    }
  }

  return true;
}

int32_t
nsAString::Find(const char* aStr, uint32_t aOffset, bool aIgnoreCase) const
{
  bool (*match)(const char16_t*, const char*, uint32_t) =
    aIgnoreCase ? ns_strnimatch : ns_strnmatch;

  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  if (aOffset > selflen) {
    return -1;
  }

  uint32_t otherlen = strlen(aStr);

  if (otherlen > selflen - aOffset) {
    return -1;
  }

  // We want to stop searching otherlen characters before the end of the string
  end -= otherlen;

  for (const char_type* cur = begin + aOffset; cur <= end; ++cur) {
    if (match(cur, aStr, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsAString::RFind(const self_type& aStr, int32_t aOffset,
                 ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  const char_type* other;
  uint32_t otherlen = aStr.BeginReading(&other);

  if (selflen < otherlen) {
    return -1;
  }

  if (aOffset < 0 || uint32_t(aOffset) > (selflen - otherlen)) {
    end -= otherlen;
  } else {
    end = begin + aOffset;
  }

  for (const char_type* cur = end; cur >= begin; --cur) {
    if (!aComparator(cur, other, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsAString::RFind(const char* aStr, int32_t aOffset, bool aIgnoreCase) const
{
  bool (*match)(const char16_t*, const char*, uint32_t) =
    aIgnoreCase ? ns_strnimatch : ns_strnmatch;

  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);
  uint32_t otherlen = strlen(aStr);

  if (selflen < otherlen) {
    return -1;
  }

  if (aOffset < 0 || uint32_t(aOffset) > (selflen - otherlen)) {
    end -= otherlen;
  } else {
    end = begin + aOffset;
  }

  for (const char_type* cur = end; cur >= begin; --cur) {
    if (match(cur, aStr, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsAString::FindChar(char_type aChar, uint32_t aOffset) const
{
  const char_type* start;
  const char_type* end;
  uint32_t len = BeginReading(&start, &end);
  if (aOffset > len) {
    return -1;
  }

  const char_type* cur;

  for (cur = start + aOffset; cur < end; ++cur) {
    if (*cur == aChar) {
      return cur - start;
    }
  }

  return -1;
}

int32_t
nsAString::RFindChar(char_type aChar) const
{
  const char16_t* start;
  const char16_t* end;
  BeginReading(&start, &end);

  do {
    --end;

    if (*end == aChar) {
      return end - start;
    }

  } while (end >= start);

  return -1;
}

void
nsAString::AppendInt(int aInt, int32_t aRadix)
{
  const char* fmt;
  switch (aRadix) {
    case 8:
      fmt = "%o";
      break;

    case 10:
      fmt = "%d";
      break;

    case 16:
      fmt = "%x";
      break;

    default:
      NS_ERROR("Unrecognized radix");
      fmt = "";
  }

  char buf[20];
  int len = snprintf(buf, sizeof(buf), fmt, aInt);
  buf[sizeof(buf) - 1] = '\0';

  Append(NS_ConvertASCIItoUTF16(buf, len));
}

// Strings

#ifndef XPCOM_GLUE_AVOID_NSPR
int32_t
nsAString::ToInteger(nsresult* aErrorCode, uint32_t aRadix) const
{
  NS_ConvertUTF16toUTF8 narrow(*this);

  const char* fmt;
  switch (aRadix) {
    case 10:
      fmt = "%i";
      break;

    case 16:
      fmt = "%x";
      break;

    default:
      NS_ERROR("Unrecognized radix!");
      *aErrorCode = NS_ERROR_INVALID_ARG;
      return 0;
  }

  int32_t result = 0;
  if (PR_sscanf(narrow.get(), fmt, &result) == 1) {
    *aErrorCode = NS_OK;
  } else {
    *aErrorCode = NS_ERROR_FAILURE;
  }

  return result;
}

int64_t
nsAString::ToInteger64(nsresult* aErrorCode, uint32_t aRadix) const
{
  NS_ConvertUTF16toUTF8 narrow(*this);

  const char* fmt;
  switch (aRadix) {
    case 10:
      fmt = "%lli";
      break;

    case 16:
      fmt = "%llx";
      break;

    default:
      NS_ERROR("Unrecognized radix!");
      *aErrorCode = NS_ERROR_INVALID_ARG;
      return 0;
  }

  int64_t result = 0;
  if (PR_sscanf(narrow.get(), fmt, &result) == 1) {
    *aErrorCode = NS_OK;
  } else {
    *aErrorCode = NS_ERROR_FAILURE;
  }

  return result;
}
#endif // XPCOM_GLUE_AVOID_NSPR

// nsACString

uint32_t
nsACString::BeginReading(const char_type** aBegin, const char_type** aEnd) const
{
  uint32_t len = NS_CStringGetData(*this, aBegin);
  if (aEnd) {
    *aEnd = *aBegin + len;
  }

  return len;
}

const nsACString::char_type*
nsACString::BeginReading() const
{
  const char_type* data;
  NS_CStringGetData(*this, &data);
  return data;
}

const nsACString::char_type*
nsACString::EndReading() const
{
  const char_type* data;
  uint32_t len = NS_CStringGetData(*this, &data);
  return data + len;
}

uint32_t
nsACString::BeginWriting(char_type** aBegin, char_type** aEnd,
                         uint32_t aNewSize)
{
  uint32_t len = NS_CStringGetMutableData(*this, aNewSize, aBegin);
  if (aEnd) {
    *aEnd = *aBegin + len;
  }

  return len;
}

nsACString::char_type*
nsACString::BeginWriting(uint32_t aLen)
{
  char_type* data;
  NS_CStringGetMutableData(*this, aLen, &data);
  return data;
}

nsACString::char_type*
nsACString::EndWriting()
{
  char_type* data;
  uint32_t len = NS_CStringGetMutableData(*this, UINT32_MAX, &data);
  return data + len;
}

bool
nsACString::SetLength(uint32_t aLen)
{
  char_type* data;
  NS_CStringGetMutableData(*this, aLen, &data);
  return data != nullptr;
}

void
nsACString::StripChars(const char* aSet)
{
  nsCString copy(*this);

  const char_type* source;
  const char_type* sourceEnd;
  copy.BeginReading(&source, &sourceEnd);

  char_type* dest;
  BeginWriting(&dest);
  if (!dest) {
    return;
  }

  char_type* curDest = dest;

  for (; source < sourceEnd; ++source) {
    const char* test;
    for (test = aSet; *test; ++test) {
      if (*source == char_type(*test)) {
        break;
      }
    }

    if (!*test) {
      // not stripped, copy this char
      *curDest = *source;
      ++curDest;
    }
  }

  SetLength(curDest - dest);
}

void
nsACString::Trim(const char* aSet, bool aLeading, bool aTrailing)
{
  NS_ASSERTION(aLeading || aTrailing, "Ineffective Trim");

  const char* start;
  const char* end;
  uint32_t cutLen;

  if (aLeading) {
    BeginReading(&start, &end);
    for (cutLen = 0; start < end; ++start, ++cutLen) {
      const char* test;
      for (test = aSet; *test; ++test) {
        if (*test == *start) {
          break;
        }
      }
      if (!*test) {
        break;
      }
    }
    if (cutLen) {
      NS_CStringCutData(*this, 0, cutLen);
    }
  }
  if (aTrailing) {
    uint32_t len = BeginReading(&start, &end);
    --end;
    for (cutLen = 0; end >= start; --end, ++cutLen) {
      const char* test;
      for (test = aSet; *test; ++test) {
        if (*test == *end) {
          break;
        }
      }
      if (!*test) {
        break;
      }
    }
    if (cutLen) {
      NS_CStringCutData(*this, len - cutLen, cutLen);
    }
  }
}

int32_t
nsACString::DefaultComparator(const char_type* aStrA, const char_type* aStrB,
                              uint32_t aLen)
{
  return memcmp(aStrA, aStrB, aLen);
}

int32_t
nsACString::Compare(const char_type* aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  uint32_t selflen = NS_CStringGetData(*this, &cself);
  uint32_t otherlen = strlen(aOther);
  uint32_t comparelen = selflen <= otherlen ? selflen : otherlen;

  int32_t result = aComparator(cself, aOther, comparelen);
  if (result == 0) {
    if (selflen < otherlen) {
      return -1;
    } else if (selflen > otherlen) {
      return 1;
    }
  }
  return result;
}

int32_t
nsACString::Compare(const self_type& aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  const char_type* cother;
  uint32_t selflen = NS_CStringGetData(*this, &cself);
  uint32_t otherlen = NS_CStringGetData(aOther, &cother);
  uint32_t comparelen = selflen <= otherlen ? selflen : otherlen;

  int32_t result = aComparator(cself, cother, comparelen);
  if (result == 0) {
    if (selflen < otherlen) {
      return -1;
    } else if (selflen > otherlen) {
      return 1;
    }
  }
  return result;
}

bool
nsACString::Equals(const char_type* aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  uint32_t selflen = NS_CStringGetData(*this, &cself);
  uint32_t otherlen = strlen(aOther);

  if (selflen != otherlen) {
    return false;
  }

  return aComparator(cself, aOther, selflen) == 0;
}

bool
nsACString::Equals(const self_type& aOther, ComparatorFunc aComparator) const
{
  const char_type* cself;
  const char_type* cother;
  uint32_t selflen = NS_CStringGetData(*this, &cself);
  uint32_t otherlen = NS_CStringGetData(aOther, &cother);

  if (selflen != otherlen) {
    return false;
  }

  return aComparator(cself, cother, selflen) == 0;
}

int32_t
nsACString::Find(const self_type& aStr, uint32_t aOffset,
                 ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  if (aOffset > selflen) {
    return -1;
  }

  const char_type* other;
  uint32_t otherlen = aStr.BeginReading(&other);

  if (otherlen > selflen - aOffset) {
    return -1;
  }

  // We want to stop searching otherlen characters before the end of the string
  end -= otherlen;

  for (const char_type* cur = begin + aOffset; cur <= end; ++cur) {
    if (!aComparator(cur, other, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsACString::Find(const char_type* aStr, ComparatorFunc aComparator) const
{
  return Find(aStr, strlen(aStr), aComparator);
}

int32_t
nsACString::Find(const char_type* aStr, uint32_t aLen,
                 ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  if (aLen == 0) {
    NS_WARNING("Searching for zero-length string.");
    return -1;
  }

  if (aLen > selflen) {
    return -1;
  }

  // We want to stop searching otherlen characters before the end of the string
  end -= aLen;

  for (const char_type* cur = begin; cur <= end; ++cur) {
    if (!aComparator(cur, aStr, aLen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsACString::RFind(const self_type& aStr, int32_t aOffset,
                  ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  const char_type* other;
  uint32_t otherlen = aStr.BeginReading(&other);

  if (selflen < otherlen) {
    return -1;
  }

  if (aOffset < 0 || uint32_t(aOffset) > (selflen - otherlen)) {
    end -= otherlen;
  } else {
    end = begin + aOffset;
  }

  for (const char_type* cur = end; cur >= begin; --cur) {
    if (!aComparator(cur, other, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsACString::RFind(const char_type* aStr, ComparatorFunc aComparator) const
{
  return RFind(aStr, strlen(aStr), aComparator);
}

int32_t
nsACString::RFind(const char_type* aStr, int32_t aLen,
                  ComparatorFunc aComparator) const
{
  const char_type* begin;
  const char_type* end;
  uint32_t selflen = BeginReading(&begin, &end);

  if (aLen <= 0) {
    NS_WARNING("Searching for zero-length string.");
    return -1;
  }

  if (uint32_t(aLen) > selflen) {
    return -1;
  }

  // We want to start searching otherlen characters before the end of the string
  end -= aLen;

  for (const char_type* cur = end; cur >= begin; --cur) {
    if (!aComparator(cur, aStr, aLen)) {
      return cur - begin;
    }
  }
  return -1;
}

int32_t
nsACString::FindChar(char_type aChar, uint32_t aOffset) const
{
  const char_type* start;
  const char_type* end;
  uint32_t len = BeginReading(&start, &end);
  if (aOffset > len) {
    return -1;
  }

  const char_type* cur;

  for (cur = start + aOffset; cur < end; ++cur) {
    if (*cur == aChar) {
      return cur - start;
    }
  }

  return -1;
}

int32_t
nsACString::RFindChar(char_type aChar) const
{
  const char* start;
  const char* end;
  BeginReading(&start, &end);

  for (; end >= start; --end) {
    if (*end == aChar) {
      return end - start;
    }
  }

  return -1;
}

void
nsACString::AppendInt(int aInt, int32_t aRadix)
{
  const char* fmt;
  switch (aRadix) {
    case 8:
      fmt = "%o";
      break;

    case 10:
      fmt = "%d";
      break;

    case 16:
      fmt = "%x";
      break;

    default:
      NS_ERROR("Unrecognized radix");
      fmt = "";
  }

  char buf[20];
  int len = snprintf(buf, sizeof(buf), fmt, aInt);
  buf[sizeof(buf) - 1] = '\0';

  Append(buf, len);
}

#ifndef XPCOM_GLUE_AVOID_NSPR
int32_t
nsACString::ToInteger(nsresult* aErrorCode, uint32_t aRadix) const
{
  const char* fmt;
  switch (aRadix) {
    case 10:
      fmt = "%i";
      break;

    case 16:
      fmt = "%x";
      break;

    default:
      NS_ERROR("Unrecognized radix!");
      *aErrorCode = NS_ERROR_INVALID_ARG;
      return 0;
  }

  int32_t result = 0;
  if (PR_sscanf(nsCString(*this).get(), fmt, &result) == 1) {
    *aErrorCode = NS_OK;
  } else {
    *aErrorCode = NS_ERROR_FAILURE;
  }

  return result;
}

int64_t
nsACString::ToInteger64(nsresult* aErrorCode, uint32_t aRadix) const
{
  const char* fmt;
  switch (aRadix) {
    case 10:
      fmt = "%lli";
      break;

    case 16:
      fmt = "%llx";
      break;

    default:
      NS_ERROR("Unrecognized radix!");
      *aErrorCode = NS_ERROR_INVALID_ARG;
      return 0;
  }

  int64_t result = 0;
  if (PR_sscanf(nsCString(*this).get(), fmt, &result) == 1) {
    *aErrorCode = NS_OK;
  } else {
    *aErrorCode = NS_ERROR_FAILURE;
  }

  return result;
}
#endif // XPCOM_GLUE_AVOID_NSPR

// Substrings

nsDependentSubstring::nsDependentSubstring(const abstract_string_type& aStr,
                                           uint32_t aStartPos)
{
  const char16_t* data;
  uint32_t len = NS_StringGetData(aStr, &data);

  if (aStartPos > len) {
    aStartPos = len;
  }

  NS_StringContainerInit2(*this, data + aStartPos, len - aStartPos,
                          NS_STRING_CONTAINER_INIT_DEPEND |
                          NS_STRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentSubstring::nsDependentSubstring(const abstract_string_type& aStr,
                                           uint32_t aStartPos,
                                           uint32_t aLength)
{
  const char16_t* data;
  uint32_t len = NS_StringGetData(aStr, &data);

  if (aStartPos > len) {
    aStartPos = len;
  }

  if (aStartPos + aLength > len) {
    aLength = len - aStartPos;
  }

  NS_StringContainerInit2(*this, data + aStartPos, aLength,
                          NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentCSubstring::nsDependentCSubstring(const abstract_string_type& aStr,
                                             uint32_t aStartPos)
{
  const char* data;
  uint32_t len = NS_CStringGetData(aStr, &data);

  if (aStartPos > len) {
    aStartPos = len;
  }

  NS_CStringContainerInit2(*this, data + aStartPos, len - aStartPos,
                           NS_CSTRING_CONTAINER_INIT_DEPEND |
                           NS_CSTRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentCSubstring::nsDependentCSubstring(const abstract_string_type& aStr,
                                             uint32_t aStartPos,
                                             uint32_t aLength)
{
  const char* data;
  uint32_t len = NS_CStringGetData(aStr, &data);

  if (aStartPos > len) {
    aStartPos = len;
  }

  if (aStartPos + aLength > len) {
    aLength = len - aStartPos;
  }

  NS_CStringContainerInit2(*this, data + aStartPos, aLength,
                           NS_CSTRING_CONTAINER_INIT_DEPEND |
                           NS_CSTRING_CONTAINER_INIT_SUBSTRING);
}

// Utils

char*
ToNewUTF8String(const nsAString& aSource)
{
  nsCString temp;
  CopyUTF16toUTF8(aSource, temp);
  return NS_CStringCloneData(temp);
}

void
CompressWhitespace(nsAString& aString)
{
  char16_t* start;
  uint32_t len = NS_StringGetMutableData(aString, UINT32_MAX, &start);
  char16_t* end = start + len;
  char16_t* from = start;
  char16_t* to = start;

  // Skip any leading whitespace
  while (from < end && NS_IsAsciiWhitespace(*from)) {
    from++;
  }

  while (from < end) {
    char16_t theChar = *from++;

    if (NS_IsAsciiWhitespace(theChar)) {
      // We found a whitespace char, so skip over any more
      while (from < end && NS_IsAsciiWhitespace(*from)) {
        from++;
      }

      // Turn all whitespace into spaces
      theChar = ' ';
    }

    *to++ = theChar;
  }

  // Drop any trailing space
  if (to > start && to[-1] == ' ') {
    to--;
  }

  // Re-terminate the string
  *to = '\0';

  // Set the new length
  aString.SetLength(to - start);
}

uint32_t
ToLowerCase(nsACString& aStr)
{
  char* begin;
  char* end;
  uint32_t len = aStr.BeginWriting(&begin, &end);

  for (; begin < end; ++begin) {
    *begin = NS_ToLower(*begin);
  }

  return len;
}

uint32_t
ToUpperCase(nsACString& aStr)
{
  char* begin;
  char* end;
  uint32_t len = aStr.BeginWriting(&begin, &end);

  for (; begin < end; ++begin) {
    *begin = NS_ToUpper(*begin);
  }

  return len;
}

uint32_t
ToLowerCase(const nsACString& aSrc, nsACString& aDest)
{
  const char* begin;
  const char* end;
  uint32_t len = aSrc.BeginReading(&begin, &end);

  char* dest;
  NS_CStringGetMutableData(aDest, len, &dest);

  for (; begin < end; ++begin, ++dest) {
    *dest = NS_ToLower(*begin);
  }

  return len;
}

uint32_t
ToUpperCase(const nsACString& aSrc, nsACString& aDest)
{
  const char* begin;
  const char* end;
  uint32_t len = aSrc.BeginReading(&begin, &end);

  char* dest;
  NS_CStringGetMutableData(aDest, len, &dest);

  for (; begin < end; ++begin, ++dest) {
    *dest = NS_ToUpper(*begin);
  }

  return len;
}

int32_t
CaseInsensitiveCompare(const char* aStrA, const char* aStrB,
                       uint32_t aLen)
{
  for (const char* aend = aStrA + aLen; aStrA < aend; ++aStrA, ++aStrB) {
    char la = NS_ToLower(*aStrA);
    char lb = NS_ToLower(*aStrB);

    if (la == lb) {
      continue;
    }

    return la < lb ? -1 : 1;
  }

  return 0;
}

bool
ParseString(const nsACString& aSource, char aDelimiter,
            nsTArray<nsCString>& aArray)
{
  int32_t start = 0;
  int32_t end = aSource.Length();

  uint32_t oldLength = aArray.Length();

  for (;;) {
    int32_t delimiter = aSource.FindChar(aDelimiter, start);
    if (delimiter < 0) {
      delimiter = end;
    }

    if (delimiter != start) {
      if (!aArray.AppendElement(Substring(aSource, start, delimiter - start))) {
        aArray.RemoveElementsAt(oldLength, aArray.Length() - oldLength);
        return false;
      }
    }

    if (delimiter == end) {
      break;
    }
    start = ++delimiter;
    if (start == end) {
      break;
    }
  }

  return true;
}
