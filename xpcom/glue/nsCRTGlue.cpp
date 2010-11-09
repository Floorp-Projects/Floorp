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
 * The Original Code is Mozilla Application Update.
 * This file is based on code originally located at
 * mozilla/toolkit/mozapps/update/updater/updater.cpp
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsCRTGlue.h"
#include "nsXPCOM.h"
#include "nsDebug.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef XP_WIN
#include <io.h>
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
    return NULL;

  char *ret = (char*) NS_strspnp(delims, *str);

  if (!*ret) {
    *str = ret;
    return NULL;
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

  *str = NULL;
  return ret;
}

PRUint32
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
  PRUint32 len = NS_strlen(aString);
  return NS_strndup(aString, len);
}

PRUnichar*
NS_strndup(const PRUnichar *aString, PRUint32 aLen)
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
  PRUint32 len = strlen(aString);
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

PRBool NS_IsUpper(char aChar)
{
  return aChar != (char)nsLowerUpperUtils::kUpper2Lower[(unsigned char)aChar];
}

PRBool NS_IsLower(char aChar)
{
  return aChar != (char)nsLowerUpperUtils::kLower2Upper[(unsigned char)aChar];
}

PRBool NS_IsAscii(PRUnichar aChar)
{
  return (0x0080 > aChar);
}

PRBool NS_IsAscii(const PRUnichar *aString)
{
  while(*aString) {
    if( 0x0080 <= *aString)
      return PR_FALSE;
    aString++;
  }
  return PR_TRUE;
}

PRBool NS_IsAscii(const char *aString)
{
  while(*aString) {
    if( 0x80 & *aString)
      return PR_FALSE;
    aString++;
  }
  return PR_TRUE;
}

PRBool NS_IsAscii(const char* aString, PRUint32 aLength)
{
  const char* end = aString + aLength;
  while (aString < end) {
    if (0x80 & *aString)
      return PR_FALSE;
    ++aString;
  }
  return PR_TRUE;
}

PRBool NS_IsAsciiAlpha(PRUnichar aChar)
{
  return ((aChar >= 'A') && (aChar <= 'Z')) ||
         ((aChar >= 'a') && (aChar <= 'z'));
}

PRBool NS_IsAsciiWhitespace(PRUnichar aChar)
{
  return aChar == ' ' ||
         aChar == '\r' ||
         aChar == '\n' ||
         aChar == '\t';
}

PRBool NS_IsAsciiDigit(PRUnichar aChar)
{
  return aChar >= '0' && aChar <= '9';
}

#if defined(XP_WIN) && !defined(WINCE)
void
printf_stderr(const char *fmt, ...)
{
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
