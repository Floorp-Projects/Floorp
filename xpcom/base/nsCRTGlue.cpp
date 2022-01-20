/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRTGlue.h"
#include "nsXPCOM.h"
#include "nsDebug.h"
#include "prtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "mozilla/Sprintf.h"

#ifdef XP_WIN
#  include <io.h>
#  include <windows.h>
#  include "mozilla/LateWriteChecks.h"
#  include "mozilla/UniquePtr.h"
#endif

#ifdef ANDROID
#  include <android/log.h>
#endif

using namespace mozilla;

const char* NS_strspnp(const char* aDelims, const char* aStr) {
  const char* d;
  do {
    for (d = aDelims; *d != '\0'; ++d) {
      if (*aStr == *d) {
        ++aStr;
        break;
      }
    }
  } while (*d);

  return aStr;
}

char* NS_strtok(const char* aDelims, char** aStr) {
  if (!*aStr) {
    return nullptr;
  }

  char* ret = (char*)NS_strspnp(aDelims, *aStr);

  if (!*ret) {
    *aStr = ret;
    return nullptr;
  }

  char* i = ret;
  do {
    for (const char* d = aDelims; *d != '\0'; ++d) {
      if (*i == *d) {
        *i = '\0';
        *aStr = ++i;
        return ret;
      }
    }
    ++i;
  } while (*i);

  *aStr = nullptr;
  return ret;
}

uint32_t NS_strlen(const char16_t* aString) {
  MOZ_ASSERT(aString);
  const char16_t* end;

  for (end = aString; *end; ++end) {
    // empty loop
  }

  return end - aString;
}

int NS_strcmp(const char16_t* aStrA, const char16_t* aStrB) {
  while (*aStrB) {
    int r = *aStrA - *aStrB;
    if (r) {
      return r;
    }

    ++aStrA;
    ++aStrB;
  }

  return *aStrA != '\0';
}

int NS_strncmp(const char16_t* aStrA, const char16_t* aStrB, size_t aLen) {
  while (aLen && *aStrB) {
    int r = *aStrA - *aStrB;
    if (r) {
      return r;
    }

    ++aStrA;
    ++aStrB;
    --aLen;
  }

  return aLen ? *aStrA != '\0' : 0;
}

char16_t* NS_xstrdup(const char16_t* aString) {
  uint32_t len = NS_strlen(aString);
  return NS_xstrndup(aString, len);
}

template <typename CharT>
CharT* NS_xstrndup(const CharT* aString, uint32_t aLen) {
  auto newBuf = (CharT*)moz_xmalloc((aLen + 1) * sizeof(CharT));
  memcpy(newBuf, aString, aLen * sizeof(CharT));
  newBuf[aLen] = '\0';
  return newBuf;
}

template char16_t* NS_xstrndup<char16_t>(const char16_t* aString,
                                         uint32_t aLen);
template char* NS_xstrndup<char>(const char* aString, uint32_t aLen);

char* NS_xstrdup(const char* aString) {
  uint32_t len = strlen(aString);
  char* str = (char*)moz_xmalloc(len + 1);
  memcpy(str, aString, len);
  str[len] = '\0';
  return str;
}

// clang-format off

// This table maps uppercase characters to lower case characters;
// characters that are neither upper nor lower case are unaffected.
const unsigned char nsLowerUpperUtils::kUpper2Lower[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64,

    // upper band mapped to lower [A-Z] => [a-z]
       97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,

                                               91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

const unsigned char nsLowerUpperUtils::kLower2Upper[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96,

    // lower band mapped to upper [a-z] => [A-Z]
       65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,

                                              123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

// clang-format on

bool NS_IsUpper(char aChar) {
  return aChar != (char)nsLowerUpperUtils::kUpper2Lower[(unsigned char)aChar];
}

bool NS_IsLower(char aChar) {
  return aChar != (char)nsLowerUpperUtils::kLower2Upper[(unsigned char)aChar];
}

#ifndef XPCOM_GLUE_AVOID_NSPR

void NS_MakeRandomString(char* aBuf, int32_t aBufLen) {
#  define TABLE_SIZE 36
  static const char table[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                               'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                               's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0',
                               '1', '2', '3', '4', '5', '6', '7', '8', '9'};

  // turn PR_Now() into milliseconds since epoch
  // and salt rand with that.
  static unsigned int seed = 0;
  if (seed == 0) {
    double fpTime = double(PR_Now());
    seed =
        (unsigned int)(fpTime * 1e-6 + 0.5);  // use 1e-6, granularity of
                                              // PR_Now() on the mac is seconds
    srand(seed);
  }

  int32_t i;
  for (i = 0; i < aBufLen; ++i) {
    *aBuf++ = table[rand() % TABLE_SIZE];
  }
  *aBuf = 0;
}

#endif

#if defined(XP_WIN)
void vprintf_stderr(const char* aFmt, va_list aArgs) {
  if (IsDebuggerPresent()) {
    int lengthNeeded = _vscprintf(aFmt, aArgs);
    if (lengthNeeded) {
      lengthNeeded++;
      auto buf = MakeUnique<char[]>(lengthNeeded);
      if (buf) {
        va_list argsCpy;
        va_copy(argsCpy, aArgs);
        vsnprintf(buf.get(), lengthNeeded, aFmt, argsCpy);
        buf[lengthNeeded - 1] = '\0';
        va_end(argsCpy);
        OutputDebugStringA(buf.get());
      }
    }
  }

  // stderr is unbuffered by default so we open a new FILE (which is buffered)
  // so that calls to printf_stderr are not as likely to get mixed together.
  int fd = _fileno(stderr);
  if (fd == -2) {
    return;
  }

  FILE* fp = _fdopen(_dup(fd), "a");
  if (!fp) {
    return;
  }

  vfprintf(fp, aFmt, aArgs);

  AutoSuspendLateWriteChecks suspend;
  fclose(fp);
}

#elif defined(ANDROID)
void vprintf_stderr(const char* aFmt, va_list aArgs) {
  __android_log_vprint(ANDROID_LOG_INFO, "Gecko", aFmt, aArgs);
}
#else
void vprintf_stderr(const char* aFmt, va_list aArgs) {
  vfprintf(stderr, aFmt, aArgs);
}
#endif

void printf_stderr(const char* aFmt, ...) {
  va_list args;
  va_start(args, aFmt);
  vprintf_stderr(aFmt, args);
  va_end(args);
}

void fprintf_stderr(FILE* aFile, const char* aFmt, ...) {
  va_list args;
  va_start(args, aFmt);
  if (aFile == stderr) {
    vprintf_stderr(aFmt, args);
  } else {
    vfprintf(aFile, aFmt, args);
  }
  va_end(args);
}

void print_stderr(std::stringstream& aStr) {
#if defined(ANDROID)
  // On Android logcat output is truncated to 1024 chars per line, and
  // we usually use std::stringstream to build up giant multi-line gobs
  // of output. So to avoid the truncation we find the newlines and
  // print the lines individually.
  std::string line;
  while (std::getline(aStr, line)) {
    printf_stderr("%s\n", line.c_str());
  }
#else
  printf_stderr("%s", aStr.str().c_str());
#endif
}

void fprint_stderr(FILE* aFile, std::stringstream& aStr) {
  if (aFile == stderr) {
    print_stderr(aStr);
  } else {
    fprintf_stderr(aFile, "%s", aStr.str().c_str());
  }
}
