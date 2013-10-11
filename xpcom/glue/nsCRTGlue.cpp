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

#ifdef XP_WIN
#include <io.h>
#include <windows.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

const char*
NS_strspnp(const char *delims, const char *str)
{
  const char *d;
  do {
    for (d = delims; *d != '\0'; ++d) {
      if (*str == *d) {
        ++str;
        break;
      }
    }
  } while (*d);

  return str;
}

char*
NS_strtok(const char *delims, char **str)
{
  if (!*str)
    return nullptr;

  char *ret = (char*) NS_strspnp(delims, *str);

  if (!*ret) {
    *str = ret;
    return nullptr;
  }

  char *i = ret;
  do {
    for (const char *d = delims; *d != '\0'; ++d) {
      if (*i == *d) {
        *i = '\0';
        *str = ++i;
        return ret;
      }
    }
    ++i;
  } while (*i);

  *str = nullptr;
  return ret;
}

uint32_t
NS_strlen(const PRUnichar *aString)
{
  const PRUnichar *end;

  for (end = aString; *end; ++end) {
    // empty loop
  }

  return end - aString;
}

int
NS_strcmp(const PRUnichar *a, const PRUnichar *b)
{
  while (*b) {
    int r = *a - *b;
    if (r)
      return r;

    ++a;
    ++b;
  }

  return *a != '\0';
}

PRUnichar*
NS_strdup(const PRUnichar *aString)
{
  uint32_t len = NS_strlen(aString);
  return NS_strndup(aString, len);
}

PRUnichar*
NS_strndup(const PRUnichar *aString, uint32_t aLen)
{
  PRUnichar *newBuf = (PRUnichar*) NS_Alloc((aLen + 1) * sizeof(PRUnichar));
  if (newBuf) {
    memcpy(newBuf, aString, aLen * sizeof(PRUnichar));
    newBuf[aLen] = '\0';
  }
  return newBuf;
}

char*
NS_strdup(const char *aString)
{
  uint32_t len = strlen(aString);
  char *str = (char*) NS_Alloc(len + 1);
  if (str) {
    memcpy(str, aString, len);
    str[len] = '\0';
  }
  return str;
}

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

bool NS_IsUpper(char aChar)
{
  return aChar != (char)nsLowerUpperUtils::kUpper2Lower[(unsigned char)aChar];
}

bool NS_IsLower(char aChar)
{
  return aChar != (char)nsLowerUpperUtils::kLower2Upper[(unsigned char)aChar];
}

bool NS_IsAscii(PRUnichar aChar)
{
  return (0x0080 > aChar);
}

bool NS_IsAscii(const PRUnichar *aString)
{
  while(*aString) {
    if( 0x0080 <= *aString)
      return false;
    aString++;
  }
  return true;
}

bool NS_IsAscii(const char *aString)
{
  while(*aString) {
    if( 0x80 & *aString)
      return false;
    aString++;
  }
  return true;
}

bool NS_IsAscii(const char* aString, uint32_t aLength)
{
  const char* end = aString + aLength;
  while (aString < end) {
    if (0x80 & *aString)
      return false;
    ++aString;
  }
  return true;
}

bool NS_IsAsciiAlpha(PRUnichar aChar)
{
  return ((aChar >= 'A') && (aChar <= 'Z')) ||
         ((aChar >= 'a') && (aChar <= 'z'));
}

bool NS_IsAsciiWhitespace(PRUnichar aChar)
{
  return aChar == ' ' ||
         aChar == '\r' ||
         aChar == '\n' ||
         aChar == '\t';
}

bool NS_IsAsciiDigit(PRUnichar aChar)
{
  return aChar >= '0' && aChar <= '9';
}


#ifndef XPCOM_GLUE_AVOID_NSPR
#define TABLE_SIZE 36
static const char table[] = {
  'a','b','c','d','e','f','g','h','i','j',
  'k','l','m','n','o','p','q','r','s','t',
  'u','v','w','x','y','z','0','1','2','3',
  '4','5','6','7','8','9'
};

void NS_MakeRandomString(char *aBuf, int32_t aBufLen)
{
  // turn PR_Now() into milliseconds since epoch
  // and salt rand with that.
  static unsigned int seed = 0;
  if (seed == 0) {
    double fpTime = double(PR_Now());
    seed = (unsigned int)(fpTime * 1e-6 + 0.5); // use 1e-6, granularity of PR_Now() on the mac is seconds
    srand(seed);
  }

  int32_t i;
  for (i=0;i<aBufLen;i++) {
    *aBuf++ = table[rand()%TABLE_SIZE];
  }
  *aBuf = 0;
}

#endif
#if defined(XP_WIN)
void
printf_stderr(const char *fmt, ...)
{
  if (IsDebuggerPresent()) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    buf[sizeof(buf) - 1] = '\0';
    va_end(args);
    OutputDebugStringA(buf);
  }

  FILE *fp = _fdopen(_dup(2), "a");
  if (!fp)
      return;

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fclose(fp);
}
#elif defined(ANDROID)
void
printf_stderr(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "Gecko", fmt, args);
  va_end(args);
}
#else
void
printf_stderr(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}
#endif
