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
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef nsStringSupport_h__
#define nsStringSupport_h__

#ifndef MOZILLA_INTERNAL_API

#include "nsStringAPI.h"
#include "nsMemory.h"
#include "prprf.h"
#include "plstr.h"

#ifndef kNotFound
#define kNotFound -1
#endif

inline void
AppendIntToString(nsCString &str, PRInt32 value)
{
  char buf[32];
  PR_snprintf(buf, sizeof(buf), "%d", value);
  str.Append(buf);
}

inline PRInt32
FindCharInString(nsCString &str, char c, PRUint32 offset = 0)
{
  NS_ASSERTION(offset <= str.Length(), "invalid offset");
  const char *data = str.get();
  for (const char *p = data + offset; *p; ++p)
    if (*p == c)
      return p - data;
  return kNotFound;
}

inline PRInt32
FindCharInString(nsString &str, PRUnichar c, PRUint32 offset = 0)
{
  NS_ASSERTION(offset <= str.Length(), "invalid offset");
  const PRUnichar *data = str.get();
  for (const PRUnichar *p = data + offset; *p; ++p)
    if (*p == c)
      return p - data;
  return kNotFound;
}

inline PRInt32
FindInString(nsCString &str, const char *needle, PRBool ignoreCase = PR_FALSE)
{
  const char *data = str.get(), *p;
  if (ignoreCase)
    p = PL_strcasestr(data, needle);
  else
    p = PL_strstr(data, needle);
  return p ? p - data : kNotFound;
}

inline void
NS_CopyUnicodeToNative(const nsAString &input, nsACString &output)
{
  NS_UTF16ToCString(input, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM, output);
}

inline void
NS_CopyNativeToUnicode(const nsACString &input, nsAString &output)
{
  NS_CStringToUTF16(input, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM, output);
}

inline void
CopyUTF16toUTF8(const nsAString &input, nsACString &output)
{
  NS_UTF16ToCString(input, NS_CSTRING_ENCODING_UTF8, output);
}

typedef nsCString nsCAutoString;
typedef nsString nsAutoString;
typedef nsCString nsXPIDLCString;
typedef nsString nsXPIDLString;

#else // MOZILLA_INTERNAL_API

#include "nsString.h"
#include "nsNativeCharsetUtils.h"
#include "nsReadableUtils.h"

inline void
AppendIntToString(nsCString &str, PRInt32 value)
{
  str.AppendInt(value);
}

inline PRInt32
FindCharInString(nsCString &str, char c, PRUint32 offset = 0)
{
  return str.FindChar(c, offset);
}

inline PRInt32
FindCharInString(nsString &str, PRUnichar c, PRUint32 offset = 0)
{
  return str.FindChar(c, offset);
}

inline PRInt32
FindInString(nsCString &str, const char *needle, PRBool ignoreCase = PR_FALSE)
{
  return str.Find(needle, ignoreCase);
}

#endif // MOZILLA_INTERNAL_API

#endif // !nsStringSupport_h__
