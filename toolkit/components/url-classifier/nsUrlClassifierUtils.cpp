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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsEscape.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsUrlClassifierUtils.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "plbase64.h"
#include "prmem.h"
#include "prprf.h"

static char int_to_hex_digit(PRInt32 i)
{
  NS_ASSERTION((i >= 0) && (i <= 15), "int too big in int_to_hex_digit");
  return static_cast<char>(((i < 10) ? (i + '0') : ((i - 10) + 'A')));
}

static bool
IsDecimal(const nsACString & num)
{
  for (PRUint32 i = 0; i < num.Length(); i++) {
    if (!isdigit(num[i])) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

static bool
IsHex(const nsACString & num)
{
  if (num.Length() < 3) {
    return PR_FALSE;
  }

  if (num[0] != '0' || !(num[1] == 'x' || num[1] == 'X')) {
    return PR_FALSE;
  }

  for (PRUint32 i = 2; i < num.Length(); i++) {
    if (!isxdigit(num[i])) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

static bool
IsOctal(const nsACString & num)
{
  if (num.Length() < 2) {
    return PR_FALSE;
  }

  if (num[0] != '0') {
    return PR_FALSE;
  }

  for (PRUint32 i = 1; i < num.Length(); i++) {
    if (!isdigit(num[i]) || num[i] == '8' || num[i] == '9') {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

nsUrlClassifierUtils::nsUrlClassifierUtils() : mEscapeCharmap(nsnull)
{
}

nsresult
nsUrlClassifierUtils::Init()
{
  // Everything but alpha numerics, - and .
  mEscapeCharmap = new Charmap(0xffffffff, 0xfc009fff, 0xf8000001, 0xf8000001,
                               0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
  if (!mEscapeCharmap)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsUrlClassifierUtils, nsIUrlClassifierUtils)

/////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUtils

NS_IMETHODIMP
nsUrlClassifierUtils::GetKeyForURI(nsIURI * uri, nsACString & _retval)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
  if (!innerURI)
    innerURI = uri;

  nsCAutoString host;
  innerURI->GetAsciiHost(host);

  if (host.IsEmpty()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsresult rv = CanonicalizeHostname(host, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString path;
  rv = innerURI->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // strip out anchors
  PRInt32 ref = path.FindChar('#');
  if (ref != kNotFound)
    path.SetLength(ref);

  nsCAutoString temp;
  rv = CanonicalizePath(path, temp);
  NS_ENSURE_SUCCESS(rv, rv);

  _retval.Append(temp);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// non-interface methods

nsresult
nsUrlClassifierUtils::CanonicalizeHostname(const nsACString & hostname,
                                           nsACString & _retval)
{
  nsCAutoString unescaped;
  if (!NS_UnescapeURL(PromiseFlatCString(hostname).get(),
                      PromiseFlatCString(hostname).Length(),
                      0, unescaped)) {
    unescaped.Assign(hostname);
  }

  nsCAutoString cleaned;
  CleanupHostname(unescaped, cleaned);

  nsCAutoString temp;
  ParseIPAddress(cleaned, temp);
  if (!temp.IsEmpty()) {
    cleaned.Assign(temp);
  }

  ToLowerCase(cleaned);
  SpecialEncode(cleaned, PR_FALSE, _retval);

  return NS_OK;
}


nsresult
nsUrlClassifierUtils::CanonicalizePath(const nsACString & path,
                                       nsACString & _retval)
{
  _retval.Truncate();

  nsCAutoString decodedPath(path);
  nsCAutoString temp;
  while (NS_UnescapeURL(decodedPath.get(), decodedPath.Length(), 0, temp)) {
    decodedPath.Assign(temp);
    temp.Truncate();
  }

  SpecialEncode(decodedPath, PR_TRUE, _retval);
  // XXX: lowercase the path?

  return NS_OK;
}

void
nsUrlClassifierUtils::CleanupHostname(const nsACString & hostname,
                                      nsACString & _retval)
{
  _retval.Truncate();

  const char* curChar = hostname.BeginReading();
  const char* end = hostname.EndReading();
  char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (c == '.' && (lastChar == '\0' || lastChar == '.')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    ++curChar;
  }

  // cut off trailing dots
  while (_retval.Length() > 0 && _retval[_retval.Length() - 1] == '.') {
    _retval.SetLength(_retval.Length() - 1);
  }
}

void
nsUrlClassifierUtils::ParseIPAddress(const nsACString & host,
                                     nsACString & _retval)
{
  _retval.Truncate();
  nsACString::const_iterator iter, end;
  host.BeginReading(iter);
  host.EndReading(end);

  if (host.Length() <= 15) {
    // The Windows resolver allows a 4-part dotted decimal IP address to
    // have a space followed by any old rubbish, so long as the total length
    // of the string doesn't get above 15 characters. So, "10.192.95.89 xy"
    // is resolved to 10.192.95.89.
    // If the string length is greater than 15 characters, e.g.
    // "10.192.95.89 xy.wildcard.example.com", it will be resolved through
    // DNS.

    if (FindCharInReadable(' ', iter, end)) {
      end = iter;
    }
  }

  for (host.BeginReading(iter); iter != end; iter++) {
    if (!(isxdigit(*iter) || *iter == 'x' || *iter == 'X' || *iter == '.')) {
      // not an IP
      return;
    }
  }

  host.BeginReading(iter);
  nsTArray<nsCString> parts;
  ParseString(PromiseFlatCString(Substring(iter, end)), '.', parts);
  if (parts.Length() > 4) {
    return;
  }

  // If any potentially-octal numbers (start with 0 but not hex) have
  // non-octal digits, no part of the ip can be in octal
  // XXX: this came from the old javascript implementation, is it really
  // supposed to be like this?
  bool allowOctal = true;
  PRUint32 i;

  for (i = 0; i < parts.Length(); i++) {
    const nsCString& part = parts[i];
    if (part[0] == '0') {
      for (PRUint32 j = 1; j < part.Length(); j++) {
        if (part[j] == 'x') {
          break;
        }
        if (part[j] == '8' || part[j] == '9') {
          allowOctal = PR_FALSE;
          break;
        }
      }
    }
  }

  for (i = 0; i < parts.Length(); i++) {
    nsCAutoString canonical;

    if (i == parts.Length() - 1) {
      CanonicalNum(parts[i], 5 - parts.Length(), allowOctal, canonical);
    } else {
      CanonicalNum(parts[i], 1, allowOctal, canonical);
    }

    if (canonical.IsEmpty()) {
      _retval.Truncate();
      return;
    }

    if (_retval.IsEmpty()) {
      _retval.Assign(canonical);
    } else {
      _retval.Append('.');
      _retval.Append(canonical);
    }
  }
  return;
}

void
nsUrlClassifierUtils::CanonicalNum(const nsACString& num,
                                   PRUint32 bytes,
                                   bool allowOctal,
                                   nsACString& _retval)
{
  _retval.Truncate();

  if (num.Length() < 1) {
    return;
  }

  PRUint32 val;
  if (allowOctal && IsOctal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%o", &val) != 1) {
      return;
    }
  } else if (IsDecimal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%u", &val) != 1) {
      return;
    }
  } else if (IsHex(num)) {
  if (PR_sscanf(PromiseFlatCString(num).get(), num[1] == 'X' ? "0X%x" : "0x%x",
                &val) != 1) {
      return;
    }
  } else {
    return;
  }

  while (bytes--) {
    char buf[20];
    PR_snprintf(buf, sizeof(buf), "%u", val & 0xff);
    if (_retval.IsEmpty()) {
      _retval.Assign(buf);
    } else {
      _retval = nsDependentCString(buf) + NS_LITERAL_CSTRING(".") + _retval;
    }
    val >>= 8;
  }
}

// This function will encode all "special" characters in typical url
// encoding, that is %hh where h is a valid hex digit.  It will also fold
// any duplicated slashes.
bool
nsUrlClassifierUtils::SpecialEncode(const nsACString & url,
                                    bool foldSlashes,
                                    nsACString & _retval)
{
  bool changed = false;
  const char* curChar = url.BeginReading();
  const char* end = url.EndReading();

  unsigned char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (ShouldURLEscape(c)) {
      _retval.Append('%');
      _retval.Append(int_to_hex_digit(c / 16));
      _retval.Append(int_to_hex_digit(c % 16));

      changed = PR_TRUE;
    } else if (foldSlashes && (c == '/' && lastChar == '/')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    curChar++;
  }
  return changed;
}

bool
nsUrlClassifierUtils::ShouldURLEscape(const unsigned char c) const
{
  return c <= 32 || c == '%' || c >=127;
}

/* static */
void
nsUrlClassifierUtils::UnUrlsafeBase64(nsACString &str)
{
  nsACString::iterator iter, end;
  str.BeginWriting(iter);
  str.EndWriting(end);
  while (iter != end) {
    if (*iter == '-') {
      *iter = '+';
    } else if (*iter == '_') {
      *iter = '/';
    }
    iter++;
  }
}

/* static */
nsresult
nsUrlClassifierUtils::DecodeClientKey(const nsACString &key,
                                      nsACString &_retval)
{
  // Client key is sent in urlsafe base64, we need to decode it first.
  nsCAutoString base64(key);
  UnUrlsafeBase64(base64);

  // PL_Base64Decode doesn't null-terminate unless we let it allocate,
  // so we need to calculate the length ourselves.
  PRUint32 destLength;
  destLength = base64.Length();
  if (destLength > 0 && base64[destLength - 1] == '=') {
    if (destLength > 1 && base64[destLength - 2] == '=') {
      destLength -= 2;
    } else {
      destLength -= 1;
    }
  }

  destLength = ((destLength * 3) / 4);
  _retval.SetLength(destLength);
  if (destLength != _retval.Length())
    return NS_ERROR_OUT_OF_MEMORY;

  if (!PL_Base64Decode(base64.BeginReading(), base64.Length(),
                       _retval.BeginWriting())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
