/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDNService.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsUnicharUtils.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "punycode.h"

#include "mozilla/FunctionTimer.h"

//-----------------------------------------------------------------------------
// RFC 1034 - 3.1. Name space specifications and terminology
static const PRUint32 kMaxDNSNodeLen = 63;

//-----------------------------------------------------------------------------

#define NS_NET_PREF_IDNTESTBED      "network.IDN_testbed"
#define NS_NET_PREF_IDNPREFIX       "network.IDN_prefix"
#define NS_NET_PREF_IDNBLACKLIST    "network.IDN.blacklist_chars"
#define NS_NET_PREF_SHOWPUNYCODE    "network.IDN_show_punycode"
#define NS_NET_PREF_IDNWHITELIST    "network.IDN.whitelist."

inline bool isOnlySafeChars(const nsAFlatString& in,
                              const nsAFlatString& blacklist)
{
  return (blacklist.IsEmpty() ||
          in.FindCharInSet(blacklist) == kNotFound);
}

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS3(nsIDNService,
                              nsIIDNService,
                              nsIObserver,
                              nsISupportsWeakReference)

nsresult nsIDNService::Init()
{
  NS_TIME_FUNCTION;

  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs)
    prefs->GetBranch(NS_NET_PREF_IDNWHITELIST, getter_AddRefs(mIDNWhitelistPrefBranch));

  nsCOMPtr<nsIPrefBranch2> prefInternal(do_QueryInterface(prefs));
  if (prefInternal) {
    prefInternal->AddObserver(NS_NET_PREF_IDNTESTBED, this, PR_TRUE); 
    prefInternal->AddObserver(NS_NET_PREF_IDNPREFIX, this, PR_TRUE); 
    prefInternal->AddObserver(NS_NET_PREF_IDNBLACKLIST, this, PR_TRUE);
    prefInternal->AddObserver(NS_NET_PREF_SHOWPUNYCODE, this, PR_TRUE);
    prefsChanged(prefInternal, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const PRUnichar *aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch( do_QueryInterface(aSubject) );
    if (prefBranch)
      prefsChanged(prefBranch, aData);
  }
  return NS_OK;
}

void nsIDNService::prefsChanged(nsIPrefBranch *prefBranch, const PRUnichar *pref)
{
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_IDNTESTBED).Equals(pref)) {
    bool val;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref(NS_NET_PREF_IDNTESTBED, &val)))
      mMultilingualTestBed = val;
  }
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_IDNPREFIX).Equals(pref)) {
    nsXPIDLCString prefix;
    nsresult rv = prefBranch->GetCharPref(NS_NET_PREF_IDNPREFIX, getter_Copies(prefix));
    if (NS_SUCCEEDED(rv) && prefix.Length() <= kACEPrefixLen)
      PL_strncpyz(nsIDNService::mACEPrefix, prefix.get(), kACEPrefixLen + 1);
  }
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_IDNBLACKLIST).Equals(pref)) {
    nsCOMPtr<nsISupportsString> blacklist;
    nsresult rv = prefBranch->GetComplexValue(NS_NET_PREF_IDNBLACKLIST,
                                              NS_GET_IID(nsISupportsString),
                                              getter_AddRefs(blacklist));
    if (NS_SUCCEEDED(rv))
      blacklist->ToString(getter_Copies(mIDNBlacklist));
    else
      mIDNBlacklist.Truncate();
  }
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_SHOWPUNYCODE).Equals(pref)) {
    bool val;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref(NS_NET_PREF_SHOWPUNYCODE, &val)))
      mShowPunycode = val;
  }
}

nsIDNService::nsIDNService()
{
  // initialize to the official prefix (RFC 3490 "5. ACE prefix")
  const char kIDNSPrefix[] = "xn--";
  strcpy(mACEPrefix, kIDNSPrefix);

  mMultilingualTestBed = PR_FALSE;

  if (idn_success != idn_nameprep_create(NULL, &mNamePrepHandle))
    mNamePrepHandle = nsnull;

  mNormalizer = do_GetService(NS_UNICODE_NORMALIZER_CONTRACTID);
  /* member initializers and constructor code */
}

nsIDNService::~nsIDNService()
{
  idn_nameprep_destroy(mNamePrepHandle);
}

/* ACString ConvertUTF8toACE (in AUTF8String input); */
NS_IMETHODIMP nsIDNService::ConvertUTF8toACE(const nsACString & input, nsACString & ace)
{
  return UTF8toACE(input, ace, PR_TRUE);
}

nsresult nsIDNService::UTF8toACE(const nsACString & input, nsACString & ace, bool allowUnassigned)
{
  nsresult rv;
  NS_ConvertUTF8toUTF16 ustr(input);

  // map ideographic period to ASCII period etc.
  normalizeFullStops(ustr);


  PRUint32 len, offset;
  len = 0;
  offset = 0;
  nsCAutoString encodedBuf;

  nsAString::const_iterator start, end;
  ustr.BeginReading(start); 
  ustr.EndReading(end); 
  ace.Truncate();

  // encode nodes if non ASCII
  while (start != end) {
    len++;
    if (*start++ == (PRUnichar)'.') {
      rv = stringPrepAndACE(Substring(ustr, offset, len - 1), encodedBuf,
                            allowUnassigned);
      NS_ENSURE_SUCCESS(rv, rv);

      ace.Append(encodedBuf);
      ace.Append('.');
      offset += len;
      len = 0;
    }
  }

  // add extra node for multilingual test bed
  if (mMultilingualTestBed)
    ace.AppendLiteral("mltbd.");
  // encode the last node if non ASCII
  if (len) {
    rv = stringPrepAndACE(Substring(ustr, offset, len), encodedBuf,
                          allowUnassigned);
    NS_ENSURE_SUCCESS(rv, rv);

    ace.Append(encodedBuf);
  }

  return NS_OK;
}

/* AUTF8String convertACEtoUTF8(in ACString input); */
NS_IMETHODIMP nsIDNService::ConvertACEtoUTF8(const nsACString & input, nsACString & _retval)
{
  return ACEtoUTF8(input, _retval, PR_TRUE);
}

nsresult nsIDNService::ACEtoUTF8(const nsACString & input, nsACString & _retval,
                                 bool allowUnassigned)
{
  // RFC 3490 - 4.2 ToUnicode
  // ToUnicode never fails.  If any step fails, then the original input
  // sequence is returned immediately in that step.

  if (!IsASCII(input)) {
    _retval.Assign(input);
    return NS_OK;
  }
  
  PRUint32 len = 0, offset = 0;
  nsCAutoString decodedBuf;

  nsACString::const_iterator start, end;
  input.BeginReading(start); 
  input.EndReading(end); 
  _retval.Truncate();

  // loop and decode nodes
  while (start != end) {
    len++;
    if (*start++ == '.') {
      if (NS_FAILED(decodeACE(Substring(input, offset, len - 1), decodedBuf,
                              allowUnassigned))) {
        _retval.Assign(input);
        return NS_OK;
      }

      _retval.Append(decodedBuf);
      _retval.Append('.');
      offset += len;
      len = 0;
    }
  }
  // decode the last node
  if (len) {
    if (NS_FAILED(decodeACE(Substring(input, offset, len), decodedBuf,
                            allowUnassigned)))
      _retval.Assign(input);
    else
      _retval.Append(decodedBuf);
  }

  return NS_OK;
}

/* boolean isACE(in ACString input); */
NS_IMETHODIMP nsIDNService::IsACE(const nsACString & input, bool *_retval)
{
  nsACString::const_iterator begin;
  input.BeginReading(begin);

  const char *data = begin.get();
  PRUint32 dataLen = begin.size_forward();

  // look for the ACE prefix in the input string.  it may occur
  // at the beginning of any segment in the domain name.  for
  // example: "www.xn--ENCODED.com"

  const char *p = PL_strncasestr(data, mACEPrefix, dataLen);

  *_retval = p && (p == data || *(p - 1) == '.');
  return NS_OK;
}

/* AUTF8String normalize(in AUTF8String input); */
NS_IMETHODIMP nsIDNService::Normalize(const nsACString & input, nsACString & output)
{
  // protect against bogus input
  NS_ENSURE_TRUE(IsUTF8(input), NS_ERROR_UNEXPECTED);

  NS_ConvertUTF8toUTF16 inUTF16(input);
  normalizeFullStops(inUTF16);

  // pass the domain name to stringprep label by label
  nsAutoString outUTF16, outLabel;

  PRUint32 len = 0, offset = 0;
  nsresult rv;
  nsAString::const_iterator start, end;
  inUTF16.BeginReading(start);
  inUTF16.EndReading(end);

  while (start != end) {
    len++;
    if (*start++ == PRUnichar('.')) {
      rv = stringPrep(Substring(inUTF16, offset, len - 1), outLabel, PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
   
      outUTF16.Append(outLabel);
      outUTF16.Append(PRUnichar('.'));
      offset += len;
      len = 0;
    }
  }
  if (len) {
    rv = stringPrep(Substring(inUTF16, offset, len), outLabel, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    outUTF16.Append(outLabel);
  }

  CopyUTF16toUTF8(outUTF16, output);
  if (!isOnlySafeChars(outUTF16, mIDNBlacklist))
    return ConvertUTF8toACE(output, output);

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::ConvertToDisplayIDN(const nsACString & input, bool * _isASCII, nsACString & _retval)
{
  // If host is ACE, then convert to UTF-8 if the host is in the IDN whitelist.
  // Else, if host is already UTF-8, then make sure it is normalized per IDN.

  nsresult rv;

  if (IsASCII(input)) {
    // first, canonicalize the host to lowercase, for whitelist lookup
    _retval = input;
    ToLowerCase(_retval);

    bool isACE;
    IsACE(_retval, &isACE);

    if (isACE && !mShowPunycode && isInWhitelist(_retval)) {
      // ACEtoUTF8() can't fail, but might return the original ACE string
      nsCAutoString temp(_retval);
      ACEtoUTF8(temp, _retval, PR_FALSE);
      *_isASCII = IsASCII(_retval);
    } else {
      *_isASCII = PR_TRUE;
    }
  } else {
    // We have to normalize the hostname before testing against the domain
    // whitelist (see bug 315411), and to ensure the entire string gets
    // normalized.
    rv = Normalize(input, _retval);
    if (NS_FAILED(rv)) return rv;

    if (mShowPunycode && NS_SUCCEEDED(ConvertUTF8toACE(_retval, _retval))) {
      *_isASCII = PR_TRUE;
      return NS_OK;
    }

    // normalization could result in an ASCII-only hostname. alternatively, if
    // the host is converted to ACE by the normalizer, then the host may contain
    // unsafe characters, so leave it ACE encoded. see bug 283016, bug 301694, and bug 309311.
    *_isASCII = IsASCII(_retval);
    if (!*_isASCII && !isInWhitelist(_retval)) {
      *_isASCII = PR_TRUE;
      return ConvertUTF8toACE(_retval, _retval);
    }
  }

  return NS_OK;
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
        NS_IS_HIGH_SURROGATE(curChar) && 
        NS_IS_LOW_SURROGATE(*start)) {
      out[i] = SURROGATE_TO_UCS4(curChar, *start);
      ++start;
    }
    else
      out[i] = curChar;

    i++;
    if (i >= outBufLen) {
      NS_ERROR("input too big, the result truncated");
      out[outBufLen-1] = (PRUint32)'\0';
      *outLen = outBufLen-1;
      return;
    }
  }
  out[i] = (PRUint32)'\0';
  *outLen = i;
}

static void ucs4toUtf16(const PRUint32 *in, nsAString& out)
{
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
  punycode_uint encodedLength = kEncodedBufSize;

  enum punycode_status status = punycode_encode(ucs4Len,
                                                ucs4Buf,
                                                nsnull,
                                                &encodedLength,
                                                encodedBuf);

  if (punycode_success != status ||
      encodedLength >= kEncodedBufSize)
    return NS_ERROR_FAILURE;

  encodedBuf[encodedLength] = '\0';
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
  idn_result_t result = race_compress_encode((const unsigned short *) temp, 
                                             get_compress_mode((unsigned short *) temp + 1), 
                                             encodedBuf, kEncodedBufSize);
  if (idn_success != result)
    return NS_ERROR_FAILURE;

  out.Assign(prefix);
  out.Append(encodedBuf);

  return NS_OK;
}

// RFC 3454
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
// 5) Check unassigned code points -- If allowUnassigned is false, check for
// any unassigned Unicode points and if any are found return an error.
// This is described in section 7.
//
nsresult nsIDNService::stringPrep(const nsAString& in, nsAString& out,
                                  bool allowUnassigned)
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
  idn_err = idn_nameprep_map(mNamePrepHandle, (const PRUint32 *) ucs4Buf,
		                     (PRUint32 *) namePrepBuf, kMaxDNSNodeLen * 3);
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
  const PRUint32 *found = nsnull;
  idn_err = idn_nameprep_isprohibited(mNamePrepHandle, 
                                      (const PRUint32 *) ucs4Buf, &found);
  if (idn_err != idn_success || found)
    return NS_ERROR_FAILURE;

  // check bidi
  idn_err = idn_nameprep_isvalidbidi(mNamePrepHandle, 
                                     (const PRUint32 *) ucs4Buf, &found);
  if (idn_err != idn_success || found)
    return NS_ERROR_FAILURE;

  if (!allowUnassigned) {
    // check unassigned code points
    idn_err = idn_nameprep_isunassigned(mNamePrepHandle,
                                        (const PRUint32 *) ucs4Buf, &found);
    if (idn_err != idn_success || found)
      return NS_ERROR_FAILURE;
  }

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

nsresult nsIDNService::stringPrepAndACE(const nsAString& in, nsACString& out,
                                        bool allowUnassigned)
{
  nsresult rv = NS_OK;

  out.Truncate();

  if (in.Length() > kMaxDNSNodeLen) {
    NS_WARNING("IDN node too large");
    return NS_ERROR_FAILURE;
  }

  if (IsASCII(in))
    LossyCopyUTF16toASCII(in, out);
  else {
    nsAutoString strPrep;
    rv = stringPrep(in, strPrep, allowUnassigned);
    if (NS_SUCCEEDED(rv)) {
      if (IsASCII(strPrep))
        LossyCopyUTF16toASCII(strPrep, out);
      else
        rv = encodeToACE(strPrep, out);
    }
  }

  if (out.Length() > kMaxDNSNodeLen) {
    NS_WARNING("IDN node too large");
    return NS_ERROR_FAILURE;
  }

  return rv;
}

// RFC 3490
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

nsresult nsIDNService::decodeACE(const nsACString& in, nsACString& out,
                                 bool allowUnassigned)
{
  bool isAce;
  IsACE(in, &isAce);
  if (!isAce) {
    out.Assign(in);
    return NS_OK;
  }

  // RFC 3490 - 4.2 ToUnicode
  // The ToUnicode output never contains more code points than its input.
  punycode_uint output_length = in.Length() - kACEPrefixLen + 1;
  punycode_uint *output = new punycode_uint[output_length];
  NS_ENSURE_TRUE(output, NS_ERROR_OUT_OF_MEMORY);

  enum punycode_status status = punycode_decode(in.Length() - kACEPrefixLen,
                                                PromiseFlatCString(in).get() + kACEPrefixLen,
                                                &output_length,
                                                output,
                                                nsnull);
  if (status != punycode_success) {
    delete [] output;
    return NS_ERROR_FAILURE;
  }

  // UCS4 -> UTF8
  output[output_length] = 0;
  nsAutoString utf16;
  ucs4toUtf16(output, utf16);
  delete [] output;
  if (!isOnlySafeChars(utf16, mIDNBlacklist))
    return NS_ERROR_FAILURE;
  CopyUTF16toUTF8(utf16, out);

  // Validation: encode back to ACE and compare the strings
  nsCAutoString ace;
  nsresult rv = UTF8toACE(out, ace, allowUnassigned);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ace.Equals(in, nsCaseInsensitiveCStringComparator()))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

bool nsIDNService::isInWhitelist(const nsACString &host)
{
  if (mIDNWhitelistPrefBranch) {
    nsCAutoString tld(host);
    // make sure the host is ACE for lookup and check that there are no
    // unassigned codepoints
    if (!IsASCII(tld) && NS_FAILED(UTF8toACE(tld, tld, PR_FALSE))) {
      return PR_FALSE;
    }

    // truncate trailing dots first
    tld.Trim(".");
    PRInt32 pos = tld.RFind(".");
    if (pos == kNotFound)
      return PR_FALSE;

    tld.Cut(0, pos + 1);

    bool safe;
    if (NS_SUCCEEDED(mIDNWhitelistPrefBranch->GetBoolPref(tld.get(), &safe)))
      return safe;
  }

  return PR_FALSE;
}

