/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Naoki Hotta <nhotta@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDNService.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsUnicharUtils.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "punycode.h"

//-----------------------------------------------------------------------------
static const PRUint32 kMaxDNSNodeLen = 63;

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsIDNService, nsIIDNService)

nsIDNService::nsIDNService()
{
  NS_INIT_ISUPPORTS();

  nsresult rv;

  // initialize to the official prefix
  // http://www.ietf.org/internet-drafts/draft-ietf-idn-idna-14.txt
  // "IESG--" is a place holder until the offical prefix is assigned by IANA
  const char kIDNSPrefix[] = "IESG--";
  strcpy(mACEPrefix, kIDNSPrefix);

  mMultilingualTestBed = PR_FALSE;

  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefService) {
    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (prefBranch) {

      // to support test environment which is a temporary testing environment
      // until IDN is actually deployed
      PRBool value;
      rv = prefBranch->GetBoolPref("network.IDN_testbed", &value);
      if (NS_SUCCEEDED(rv))
        mMultilingualTestBed = value;

      // read prefix from pref
      nsXPIDLCString prefix;
      rv = prefBranch->GetCharPref("network.IDN_prefix", getter_Copies(prefix));
      if (NS_SUCCEEDED(rv) && 
        prefix.Length() <= kACEPrefixLen) {
        strncpy(mACEPrefix, prefix.get(), kACEPrefixLen);
        mACEPrefix[sizeof(mACEPrefix)-1] = '\0';
      }
    }
  }

  if (idn_success != idn_nameprep_create(NULL, &mNamePrepHandle))
    mNamePrepHandle = nsnull;

  mNormalizer = do_GetService(NS_UNICODE_NORMALIZER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    mNormalizer = nsnull;
  /* member initializers and constructor code */
}

nsIDNService::~nsIDNService()
{
  idn_nameprep_destroy(mNamePrepHandle);
}

/* [noscript] string ConvertUTF8toACE (in string input); */
NS_IMETHODIMP nsIDNService::ConvertUTF8toACE(const char *input, char **_retval)
{
  nsresult rv;
  NS_ConvertUTF8toUCS2 ustr(input);

  // map ideographic period to ASCII period etc.
  normalizeFullStops(ustr);


  nsCAutoString ace; 
  PRUint32 len, offset;
  len = 0;
  offset = 0;
  nsCAutoString encodedBuf;

  nsAString::const_iterator start, end;
  ustr.BeginReading(start); 
  ustr.EndReading(end); 

  // encode nodes if non ASCII
  while (start != end) {
    len++;
    if (*start++ == (PRUnichar)'.') {
      rv = stringPrepAndACE(Substring(ustr, offset, len - 1), encodedBuf);
      NS_ENSURE_SUCCESS(rv, rv);

      ace.Append(encodedBuf + NS_LITERAL_CSTRING("."));
      offset += len;
      len = 0;
    }
  }

  // add extra node for multilingual test bed
  if (mMultilingualTestBed)
    ace.Append("mltbd.");
  // encode the last node if non ASCII
  if (len) {
    rv = stringPrepAndACE(Substring(ustr, offset, len), encodedBuf);
    NS_ENSURE_SUCCESS(rv, rv);

    ace.Append(encodedBuf);
  }

  *_retval = ToNewCString(ace);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

/* [noscript] string ConvertACEtoUTF8 (in string input); */
NS_IMETHODIMP nsIDNService::ConvertACEtoUTF8(const char *input, char **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------

static void utf16ToUcs4(const nsAString& in, PRUint32 *out, PRUint32 outBufLen, PRUint32 *outLen)
{
  PRUint32 i = 0;
  nsAString::const_iterator start, end;
  in.BeginReading(start); 
  in.EndReading(end); 

  while (start != end) {
    PRUnichar curChar;

    curChar= *start++;

    if (start != end &&
        IS_HIGH_SURROGATE(curChar) && 
        IS_LOW_SURROGATE(*start)) {
      out[i] = SURROGATE_TO_UCS4(curChar, *start);
      ++start;
    }
    else
      out[i] = curChar;

    i++;
    if (i >= outBufLen) {
      NS_ERROR("input too big, the result truncated");
      out[outBufLen-1] = (PRUint32)'\0';
      *outLen = i;
      return;
    }
  }
  out[i] = (PRUint32)'\0';
  *outLen = i;
}

static void ucs4toUtf16(const PRUint32 *in, nsAString& out)
{
#define H_SURROGATE(s) ((PRUnichar)(((PRUint32)s - (PRUint32)0x10000) >> 10) + (PRUnichar)0xd800)
#define L_SURROGATE(s) ((PRUnichar)(((PRUint32)s - (PRUint32)0x10000) & 0x3ff) + (PRUnichar)0xdc00)
#define IS_IN_BMP(ucs) ((PRUint32)ucs < 0x10000)

  while (*in) {
    if (!IS_IN_BMP(*in)) {
      out.Append((PRUnichar) H_SURROGATE(*in));
      out.Append((PRUnichar) L_SURROGATE(*in));
    }
    else
      out.Append((PRUnichar) *in);
    in++;
  }
}

static nsresult punycode(const char* prefix, const nsAString& in, nsACString& out)
{
  PRUint32 ucs4Buf[kMaxDNSNodeLen + 1];
  PRUint32 ucs4Len;
  utf16ToUcs4(in, ucs4Buf, kMaxDNSNodeLen, &ucs4Len);

  // need maximum 20 bits to encode 16 bit Unicode character
  // (include null terminator)
  const PRUint32 kEncodedBufSize = kMaxDNSNodeLen * 20 / 8 + 1 + 1;  
  char encodedBuf[kEncodedBufSize];
  punycode_uint encodedLength;

  enum punycode_status status = punycode_encode(ucs4Len,
                                                ucs4Buf,
                                                nsnull,
                                                &encodedLength,
                                                encodedBuf);

  if (punycode_success != status)
    return NS_ERROR_FAILURE;

  out.Assign(nsDependentCString(prefix) + nsDependentCString(encodedBuf));

  return NS_OK;
}

static nsresult encodeToRACE(const char* prefix, const nsAString& in, nsACString& out)
{
  // need maximum 20 bits to encode 16 bit Unicode character
  // (include null terminator)
  const PRUint32 kEncodedBufSize = kMaxDNSNodeLen * 20 / 8 + 1 + 1;  

  // set up a work buffer for RACE encoder
  PRUnichar temp[kMaxDNSNodeLen + 2];
  temp[0] = 0xFFFF;   // set a place holder (to be filled by get_compress_mode)
  temp[in.Length() + 1] = (PRUnichar)'\0';

  nsAString::const_iterator start, end;
  in.BeginReading(start); 
  in.EndReading(end);
  
  for (PRUint32 i = 1; start != end; i++)
    temp[i] = *start++;

  // encode nodes if non ASCII

  char encodedBuf[kEncodedBufSize];
  idn_result_t result = race_compress_encode(temp, get_compress_mode(temp + 1), 
                                             encodedBuf, kEncodedBufSize);
  if (idn_success != result)
    return NS_ERROR_FAILURE;

  out.Assign(nsDependentCString(prefix) + nsDependentCString(encodedBuf));

  return NS_OK;
}

// http://www.ietf.org/internet-drafts/draft-hoffman-stringprep-06.txt
//
// 1) Map -- For each character in the input, check if it has a mapping
// and, if so, replace it with its mapping. This is described in section 3.
//
// 2) Normalize -- Possibly normalize the result of step 1 using Unicode
// normalization. This is described in section 4.
//
// 3) Prohibit -- Check for any characters that are not allowed in the
// output. If any are found, return an error. This is described in section
// 5.
//
// 4) Check bidi -- Possibly check for right-to-left characters, and if any
// are found, make sure that the whole string satisfies the requirements
// for bidirectional strings. If the string does not satisfy the requirements
// for bidirectional strings, return an error. This is described in section 6.
//
nsresult nsIDNService::stringPrep(const nsAString& in, nsAString& out)
{
  if (!mNamePrepHandle || !mNormalizer)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  PRUint32 ucs4Buf[kMaxDNSNodeLen + 1];
  PRUint32 ucs4Len;
  utf16ToUcs4(in, ucs4Buf, kMaxDNSNodeLen, &ucs4Len);

  // map
  idn_result_t idn_err;

  PRUint32 namePrepBuf[kMaxDNSNodeLen * 3];   // map up to three characters
  idn_err = idn_nameprep_map(mNamePrepHandle, (const unsigned long*) ucs4Buf,
		                     (unsigned long*) namePrepBuf, kMaxDNSNodeLen * 3);
  NS_ENSURE_TRUE(idn_err == idn_success, NS_ERROR_FAILURE);

  nsAutoString namePrepStr;
  ucs4toUtf16(namePrepBuf, namePrepStr);
  if (namePrepStr.Length() >= kMaxDNSNodeLen)
    return NS_ERROR_FAILURE;

  // normalize
  nsAutoString normlizedStr;
  rv = mNormalizer->NormalizeUnicodeNFKC(namePrepStr, normlizedStr);
  if (normlizedStr.Length() >= kMaxDNSNodeLen)
    return NS_ERROR_FAILURE;

  // prohibit
  const unsigned long *found = nsnull;
  idn_err = idn_nameprep_isprohibited(mNamePrepHandle, 
                                      (const unsigned long*) ucs4Buf, &found);
  if (idn_err != idn_success || found)
    return NS_ERROR_FAILURE;

  // check bidi
  idn_err = idn_nameprep_isvalidbidi(mNamePrepHandle, 
                                     (const unsigned long*) ucs4Buf, &found);
  if (idn_err != idn_success || found)
    return NS_ERROR_FAILURE;

  // set the result string
  out.Assign(normlizedStr);

  return rv;
}

nsresult nsIDNService::encodeToACE(const nsAString& in, nsACString& out)
{
  // RACE encode is supported for existing testing environment
  if (!strcmp("bq--", mACEPrefix))
    return encodeToRACE(mACEPrefix, in, out);
  
  // use punycoce
  return punycode(mACEPrefix, in, out);
}

nsresult nsIDNService::stringPrepAndACE(const nsAString& in, nsACString& out)
{
  nsresult rv = NS_OK;

  out.Truncate();

  if (in.Length() >= kMaxDNSNodeLen) {
    NS_ERROR("IDN node too large");
    return NS_ERROR_FAILURE;
  }

  if (IsASCII(in))
    CopyUCS2toASCII(in, out);
  else {
    nsAutoString strPrep;
    rv = stringPrep(in, strPrep);
    if (NS_SUCCEEDED(rv))
      rv = encodeToACE(strPrep, out);
  }

  return rv;
}

// http://www.ietf.org/internet-drafts/draft-ietf-idn-idna-12.txt
// 1) Whenever dots are used as label separators, the following characters
//    MUST be recognized as dots: U+002E (full stop), U+3002 (ideographic full
//    stop), U+FF0E (fullwidth full stop), U+FF61 (halfwidth ideographic full
//    stop).

void nsIDNService::normalizeFullStops(nsAString& s)
{
  nsAString::const_iterator start, end;
  s.BeginReading(start); 
  s.EndReading(end); 
  PRInt32 index = 0;

  while (start != end) {
    switch (*start) {
      case 0x3002:
      case 0xFF0E:
      case 0xFF61:
        s.Replace(index, 1, NS_LITERAL_STRING("."));
        break;
      default:
        break;
    }
    start++;
    index++;
  }
}

