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
#include "nsUrlClassifierUtils.h"

static char int_to_hex_digit(PRInt32 i)
{
  NS_ASSERTION((i >= 0) && (i <= 15), "int too big in int_to_hex_digit");
  return NS_STATIC_CAST(char, ((i < 10) ? (i + '0') : ((i - 10) + 'A')));
}


nsUrlClassifierUtils::nsUrlClassifierUtils()
{
}

NS_IMPL_ISUPPORTS1(nsUrlClassifierUtils, nsIUrlClassifierUtils)

/* ACString canonicalizeURL (in ACString url); */
NS_IMETHODIMP
nsUrlClassifierUtils::CanonicalizeURL(const nsACString & url, nsACString & _retval)
{
  nsCAutoString decodedUrl(url);
  nsCAutoString temp;
  while (NS_UnescapeURL(decodedUrl.get(), decodedUrl.Length(), 0, temp)) {
    decodedUrl.Assign(temp);
    temp.Truncate();
  }
  SpecialEncode(decodedUrl, _retval);
  return NS_OK;
}

// This function will encode all "special" characters in typical url
// encoding, that is %hh where h is a valid hex digit.  See the comment in
// the header file for details.
PRBool
nsUrlClassifierUtils::SpecialEncode(const nsACString & url, nsACString & _retval)
{
  PRBool changed = PR_FALSE;
  const char* curChar = url.BeginReading();
  const char* end = url.EndReading();

  while (curChar != end) {
    unsigned char c = NS_STATIC_CAST(unsigned char, *curChar);
    if (ShouldURLEscape(c)) {
      // We don't want to deal with 0, as it can break certain strings, just
      // encode as one.
      if (c == 0)
        c = 1;

      _retval.Append('%');
      _retval.Append(int_to_hex_digit(c / 16));
      _retval.Append(int_to_hex_digit(c % 16));

      changed = PR_TRUE;
    } else {
      _retval.Append(*curChar);
    }
    curChar++;
  }
  return changed;
}

PRBool
nsUrlClassifierUtils::ShouldURLEscape(const unsigned char c) const
{
  return c <= 32 || c == '%' || c >=127;
}
