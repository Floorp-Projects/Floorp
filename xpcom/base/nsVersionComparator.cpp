/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVersionComparator.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined(XP_WIN) && !defined(UPDATER_NO_STRING_GLUE_STL)
#include <wchar.h>
#include "nsString.h"
#endif

struct VersionPart
{
  int32_t     numA;

  const char* strB;    // NOT null-terminated, can be a null pointer
  uint32_t    strBlen;

  int32_t     numC;

  char*       extraD;  // null-terminated
};

#ifdef XP_WIN
struct VersionPartW
{
  int32_t     numA;

  wchar_t*    strB;    // NOT null-terminated, can be a null pointer
  uint32_t    strBlen;

  int32_t     numC;

  wchar_t*    extraD;  // null-terminated

};
#endif

/**
 * Parse a version part into a number and "extra text".
 *
 * @returns A pointer to the next versionpart, or null if none.
 */
static char*
ParseVP(char* aPart, VersionPart& aResult)
{
  char* dot;

  aResult.numA = 0;
  aResult.strB = nullptr;
  aResult.strBlen = 0;
  aResult.numC = 0;
  aResult.extraD = nullptr;

  if (!aPart) {
    return aPart;
  }

  dot = strchr(aPart, '.');
  if (dot) {
    *dot = '\0';
  }

  if (aPart[0] == '*' && aPart[1] == '\0') {
    aResult.numA = INT32_MAX;
    aResult.strB = "";
  } else {
    aResult.numA = strtol(aPart, const_cast<char**>(&aResult.strB), 10);
  }

  if (!*aResult.strB) {
    aResult.strB = nullptr;
    aResult.strBlen = 0;
  } else {
    if (aResult.strB[0] == '+') {
      static const char kPre[] = "pre";

      ++aResult.numA;
      aResult.strB = kPre;
      aResult.strBlen = sizeof(kPre) - 1;
    } else {
      const char* numstart = strpbrk(aResult.strB, "0123456789+-");
      if (!numstart) {
        aResult.strBlen = strlen(aResult.strB);
      } else {
        aResult.strBlen = numstart - aResult.strB;

        aResult.numC = strtol(numstart, &aResult.extraD, 10);
        if (!*aResult.extraD) {
          aResult.extraD = nullptr;
        }
      }
    }
  }

  if (dot) {
    ++dot;

    if (!*dot) {
      dot = nullptr;
    }
  }

  return dot;
}


/**
 * Parse a version part into a number and "extra text".
 *
 * @returns A pointer to the next versionpart, or null if none.
 */
#ifdef XP_WIN
static wchar_t*
ParseVP(wchar_t* aPart, VersionPartW& aResult)
{

  wchar_t* dot;

  aResult.numA = 0;
  aResult.strB = nullptr;
  aResult.strBlen = 0;
  aResult.numC = 0;
  aResult.extraD = nullptr;

  if (!aPart) {
    return aPart;
  }

  dot = wcschr(aPart, '.');
  if (dot) {
    *dot = '\0';
  }

  if (aPart[0] == '*' && aPart[1] == '\0') {
    static wchar_t kEmpty[] = L"";

    aResult.numA = INT32_MAX;
    aResult.strB = kEmpty;
  } else {
    aResult.numA = wcstol(aPart, const_cast<wchar_t**>(&aResult.strB), 10);
  }

  if (!*aResult.strB) {
    aResult.strB = nullptr;
    aResult.strBlen = 0;
  } else {
    if (aResult.strB[0] == '+') {
      static wchar_t kPre[] = L"pre";

      ++aResult.numA;
      aResult.strB = kPre;
      aResult.strBlen = sizeof(kPre) - 1;
    } else {
      const wchar_t* numstart = wcspbrk(aResult.strB, L"0123456789+-");
      if (!numstart) {
        aResult.strBlen = wcslen(aResult.strB);
      } else {
        aResult.strBlen = numstart - aResult.strB;

        aResult.numC = wcstol(numstart, &aResult.extraD, 10);
        if (!*aResult.extraD) {
          aResult.extraD = nullptr;
        }
      }
    }
  }

  if (dot) {
    ++dot;

    if (!*dot) {
      dot = nullptr;
    }
  }

  return dot;
}
#endif

// compare two null-terminated strings, which may be null pointers
static int32_t
ns_strcmp(const char* aStr1, const char* aStr2)
{
  // any string is *before* no string
  if (!aStr1) {
    return aStr2 != 0;
  }

  if (!aStr2) {
    return -1;
  }

  return strcmp(aStr1, aStr2);
}

// compare two length-specified string, which may be null pointers
static int32_t
ns_strnncmp(const char* aStr1, uint32_t aLen1,
            const char* aStr2, uint32_t aLen2)
{
  // any string is *before* no string
  if (!aStr1) {
    return aStr2 != 0;
  }

  if (!aStr2) {
    return -1;
  }

  for (; aLen1 && aLen2; --aLen1, --aLen2, ++aStr1, ++aStr2) {
    if (*aStr1 < *aStr2) {
      return -1;
    }

    if (*aStr1 > *aStr2) {
      return 1;
    }
  }

  if (aLen1 == 0) {
    return aLen2 == 0 ? 0 : -1;
  }

  return 1;
}

// compare two int32_t
static int32_t
ns_cmp(int32_t aNum1, int32_t aNum2)
{
  if (aNum1 < aNum2) {
    return -1;
  }

  return aNum1 != aNum2;
}

/**
 * Compares two VersionParts
 */
static int32_t
CompareVP(VersionPart& aVer1, VersionPart& aVer2)
{
  int32_t r = ns_cmp(aVer1.numA, aVer2.numA);
  if (r) {
    return r;
  }

  r = ns_strnncmp(aVer1.strB, aVer1.strBlen, aVer2.strB, aVer2.strBlen);
  if (r) {
    return r;
  }

  r = ns_cmp(aVer1.numC, aVer2.numC);
  if (r) {
    return r;
  }

  return ns_strcmp(aVer1.extraD, aVer2.extraD);
}

/**
 * Compares two VersionParts
 */
#ifdef XP_WIN
static int32_t
CompareVP(VersionPartW& aVer1, VersionPartW& aVer2)
{
  int32_t r = ns_cmp(aVer1.numA, aVer2.numA);
  if (r) {
    return r;
  }

  r = wcsncmp(aVer1.strB, aVer2.strB, XPCOM_MIN(aVer1.strBlen, aVer2.strBlen));
  if (r) {
    return r;
  }

  r = ns_cmp(aVer1.numC, aVer2.numC);
  if (r) {
    return r;
  }

  if (!aVer1.extraD) {
    return aVer2.extraD != 0;
  }

  if (!aVer2.extraD) {
    return -1;
  }

  return wcscmp(aVer1.extraD, aVer2.extraD);
}
#endif

namespace mozilla {

#ifdef XP_WIN
int32_t
CompareVersions(const char16_t* aStrA, const char16_t* aStrB)
{
  wchar_t* A2 = wcsdup(char16ptr_t(aStrA));
  if (!A2) {
    return 1;
  }

  wchar_t* B2 = wcsdup(char16ptr_t(aStrB));
  if (!B2) {
    free(A2);
    return 1;
  }

  int32_t result;
  wchar_t* a = A2;
  wchar_t* b = B2;

  do {
    VersionPartW va, vb;

    a = ParseVP(a, va);
    b = ParseVP(b, vb);

    result = CompareVP(va, vb);
    if (result) {
      break;
    }

  } while (a || b);

  free(A2);
  free(B2);

  return result;
}
#endif

int32_t
CompareVersions(const char* aStrA, const char* aStrB)
{
  char* A2 = strdup(aStrA);
  if (!A2) {
    return 1;
  }

  char* B2 = strdup(aStrB);
  if (!B2) {
    free(A2);
    return 1;
  }

  int32_t result;
  char* a = A2;
  char* b = B2;

  do {
    VersionPart va, vb;

    a = ParseVP(a, va);
    b = ParseVP(b, vb);

    result = CompareVP(va, vb);
    if (result) {
      break;
    }

  } while (a || b);

  free(A2);
  free(B2);

  return result;
}

} // namespace mozilla

