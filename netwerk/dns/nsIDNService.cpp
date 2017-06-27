/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDNService.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "harfbuzz/hb.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "punycode.h"

#ifdef IDNA2008
// Currently we use the non-transitional processing option -- see
// http://unicode.org/reports/tr46/
// To switch to transitional processing, change the value of this flag
// and kTransitionalProcessing in netwerk/test/unit/test_idna2008.js to true
// (revert bug 1218179).
const bool kIDNA2008_TransitionalProcessing = false;

#include "ICUUtils.h"
#endif

using namespace mozilla::unicode;

//-----------------------------------------------------------------------------
// RFC 1034 - 3.1. Name space specifications and terminology
static const uint32_t kMaxDNSNodeLen = 63;
// RFC 3490 - 5.   ACE prefix
static const char kACEPrefix[] = "xn--";
#define kACEPrefixLen 4

//-----------------------------------------------------------------------------

#define NS_NET_PREF_IDNBLACKLIST    "network.IDN.blacklist_chars"
#define NS_NET_PREF_SHOWPUNYCODE    "network.IDN_show_punycode"
#define NS_NET_PREF_IDNWHITELIST    "network.IDN.whitelist."
#define NS_NET_PREF_IDNUSEWHITELIST "network.IDN.use_whitelist"
#define NS_NET_PREF_IDNRESTRICTION  "network.IDN.restriction_profile"

inline bool isOnlySafeChars(const nsString& in, const nsString& blacklist)
{
  return (blacklist.IsEmpty() ||
          in.FindCharInSet(blacklist) == kNotFound);
}

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

/* Implementation file */
NS_IMPL_ISUPPORTS(nsIDNService,
                  nsIIDNService,
                  nsIObserver,
                  nsISupportsWeakReference)

nsresult nsIDNService::Init()
{
  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs)
    prefs->GetBranch(NS_NET_PREF_IDNWHITELIST, getter_AddRefs(mIDNWhitelistPrefBranch));

  nsCOMPtr<nsIPrefBranch> prefInternal(do_QueryInterface(prefs));
  if (prefInternal) {
    prefInternal->AddObserver(NS_NET_PREF_IDNBLACKLIST, this, true);
    prefInternal->AddObserver(NS_NET_PREF_SHOWPUNYCODE, this, true);
    prefInternal->AddObserver(NS_NET_PREF_IDNRESTRICTION, this, true);
    prefInternal->AddObserver(NS_NET_PREF_IDNUSEWHITELIST, this, true);
    prefsChanged(prefInternal, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const char16_t *aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch( do_QueryInterface(aSubject) );
    if (prefBranch)
      prefsChanged(prefBranch, aData);
  }
  return NS_OK;
}

void nsIDNService::prefsChanged(nsIPrefBranch *prefBranch, const char16_t *pref)
{
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
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_IDNUSEWHITELIST).Equals(pref)) {
    bool val;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref(NS_NET_PREF_IDNUSEWHITELIST,
                                             &val)))
      mIDNUseWhitelist = val;
  }
  if (!pref || NS_LITERAL_STRING(NS_NET_PREF_IDNRESTRICTION).Equals(pref)) {
    nsXPIDLCString profile;
    if (NS_FAILED(prefBranch->GetCharPref(NS_NET_PREF_IDNRESTRICTION,
                                          getter_Copies(profile)))) {
      profile.Truncate();
    }
    if (profile.EqualsLiteral("moderate")) {
      mRestrictionProfile = eModeratelyRestrictiveProfile;
    } else if (profile.EqualsLiteral("high")) {
      mRestrictionProfile = eHighlyRestrictiveProfile;
    } else {
      mRestrictionProfile = eASCIIOnlyProfile;
    }
  }
}

nsIDNService::nsIDNService()
  : mShowPunycode(false)
  , mIDNUseWhitelist(false)
{
#ifdef IDNA2008
  uint32_t IDNAOptions = UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ;
  if (!kIDNA2008_TransitionalProcessing) {
    IDNAOptions |= UIDNA_NONTRANSITIONAL_TO_UNICODE;
  }
  UErrorCode errorCode = U_ZERO_ERROR;
  mIDNA = uidna_openUTS46(IDNAOptions, &errorCode);
#else
  if (idn_success != idn_nameprep_create(nullptr, &mNamePrepHandle))
    mNamePrepHandle = nullptr;

  mNormalizer = do_GetService(NS_UNICODE_NORMALIZER_CONTRACTID);
  /* member initializers and constructor code */
#endif
}

nsIDNService::~nsIDNService()
{
#ifdef IDNA2008
  uidna_close(mIDNA);
#else
  idn_nameprep_destroy(mNamePrepHandle);
#endif
}

#ifdef IDNA2008
nsresult
nsIDNService::IDNA2008ToUnicode(const nsACString& input, nsAString& output)
{
  NS_ConvertUTF8toUTF16 inputStr(input);
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  UErrorCode errorCode = U_ZERO_ERROR;
  int32_t inLen = inputStr.Length();
  int32_t outMaxLen = kMaxDNSNodeLen + 1;
  UChar outputBuffer[kMaxDNSNodeLen + 1];

  int32_t outLen = uidna_labelToUnicode(mIDNA, (const UChar*)inputStr.get(),
                                        inLen, outputBuffer, outMaxLen,
                                        &info, &errorCode);
  if (info.errors != 0) {
    return NS_ERROR_MALFORMED_URI;
  }

  if (U_SUCCESS(errorCode)) {
    ICUUtils::AssignUCharArrayToString(outputBuffer, outLen, output);
  }

  nsresult rv = ICUUtils::UErrorToNsResult(errorCode);
  if (rv == NS_ERROR_FAILURE) {
    rv = NS_ERROR_MALFORMED_URI;
  }
  return rv;
}

nsresult
nsIDNService::IDNA2008StringPrep(const nsAString& input,
                                 nsAString& output,
                                 stringPrepFlag flag)
{
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  UErrorCode errorCode = U_ZERO_ERROR;
  int32_t inLen = input.Length();
  int32_t outMaxLen = kMaxDNSNodeLen + 1;
  UChar outputBuffer[kMaxDNSNodeLen + 1];

  int32_t outLen =
    uidna_labelToUnicode(mIDNA, (const UChar*)PromiseFlatString(input).get(),
                         inLen, outputBuffer, outMaxLen, &info, &errorCode);
  nsresult rv = ICUUtils::UErrorToNsResult(errorCode);
  if (rv == NS_ERROR_FAILURE) {
    rv = NS_ERROR_MALFORMED_URI;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Output the result of nameToUnicode even if there were errors
  ICUUtils::AssignUCharArrayToString(outputBuffer, outLen, output);

  if (flag == eStringPrepIgnoreErrors) {
    return NS_OK;
  }

  if (info.errors != 0) {
    if (flag == eStringPrepForDNS) {
      output.Truncate();
    }
    rv = NS_ERROR_MALFORMED_URI;
  }

  return rv;
}
#endif

NS_IMETHODIMP nsIDNService::ConvertUTF8toACE(const nsACString & input, nsACString & ace)
{
  return UTF8toACE(input, ace, eStringPrepForDNS);
}

nsresult nsIDNService::UTF8toACE(const nsACString & input, nsACString & ace,
                                 stringPrepFlag flag)
{
  nsresult rv;
  NS_ConvertUTF8toUTF16 ustr(input);

  // map ideographic period to ASCII period etc.
  normalizeFullStops(ustr);

  uint32_t len, offset;
  len = 0;
  offset = 0;
  nsAutoCString encodedBuf;

  nsAString::const_iterator start, end;
  ustr.BeginReading(start);
  ustr.EndReading(end);
  ace.Truncate();

  // encode nodes if non ASCII
  while (start != end) {
    len++;
    if (*start++ == (char16_t)'.') {
      rv = stringPrepAndACE(Substring(ustr, offset, len - 1), encodedBuf, flag);
      NS_ENSURE_SUCCESS(rv, rv);

      ace.Append(encodedBuf);
      ace.Append('.');
      offset += len;
      len = 0;
    }
  }

  // encode the last node if non ASCII
  if (len) {
    rv = stringPrepAndACE(Substring(ustr, offset, len), encodedBuf, flag);
    NS_ENSURE_SUCCESS(rv, rv);

    ace.Append(encodedBuf);
  }

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::ConvertACEtoUTF8(const nsACString & input, nsACString & _retval)
{
  return ACEtoUTF8(input, _retval, eStringPrepForDNS);
}

nsresult nsIDNService::ACEtoUTF8(const nsACString & input, nsACString & _retval,
                                 stringPrepFlag flag)
{
  // RFC 3490 - 4.2 ToUnicode
  // ToUnicode never fails.  If any step fails, then the original input
  // sequence is returned immediately in that step.

  uint32_t len = 0, offset = 0;
  nsAutoCString decodedBuf;

  nsACString::const_iterator start, end;
  input.BeginReading(start); 
  input.EndReading(end); 
  _retval.Truncate();

  // loop and decode nodes
  while (start != end) {
    len++;
    if (*start++ == '.') {
      if (NS_FAILED(decodeACE(Substring(input, offset, len - 1), decodedBuf,
                              flag))) {
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
                            flag)))
      _retval.Assign(input);
    else
      _retval.Append(decodedBuf);
  }

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::IsACE(const nsACString & input, bool *_retval)
{
  const char *data = input.BeginReading();
  uint32_t dataLen = input.Length();

  // look for the ACE prefix in the input string.  it may occur
  // at the beginning of any segment in the domain name.  for
  // example: "www.xn--ENCODED.com"

  const char *p = PL_strncasestr(data, kACEPrefix, dataLen);

  *_retval = p && (p == data || *(p - 1) == '.');
  return NS_OK;
}

NS_IMETHODIMP nsIDNService::Normalize(const nsACString & input,
                                      nsACString & output)
{
  // protect against bogus input
  NS_ENSURE_TRUE(IsUTF8(input), NS_ERROR_UNEXPECTED);

  NS_ConvertUTF8toUTF16 inUTF16(input);
  normalizeFullStops(inUTF16);

  // pass the domain name to stringprep label by label
  nsAutoString outUTF16, outLabel;

  uint32_t len = 0, offset = 0;
  nsresult rv;
  nsAString::const_iterator start, end;
  inUTF16.BeginReading(start);
  inUTF16.EndReading(end);

  while (start != end) {
    len++;
    if (*start++ == char16_t('.')) {
      rv = stringPrep(Substring(inUTF16, offset, len - 1), outLabel,
                      eStringPrepIgnoreErrors);
      NS_ENSURE_SUCCESS(rv, rv);
   
      outUTF16.Append(outLabel);
      outUTF16.Append(char16_t('.'));
      offset += len;
      len = 0;
    }
  }
  if (len) {
    rv = stringPrep(Substring(inUTF16, offset, len), outLabel,
                    eStringPrepIgnoreErrors);
    NS_ENSURE_SUCCESS(rv, rv);

    outUTF16.Append(outLabel);
  }

  CopyUTF16toUTF8(outUTF16, output);
  return NS_OK;
}

NS_IMETHODIMP nsIDNService::ConvertToDisplayIDN(const nsACString & input, bool * _isASCII, nsACString & _retval)
{
  // If host is ACE, then convert to UTF-8 if the host is in the IDN whitelist.
  // Else, if host is already UTF-8, then make sure it is normalized per IDN.

  nsresult rv = NS_OK;

  // Even if the hostname is not ASCII, individual labels may still be ACE, so
  // test IsACE before testing IsASCII
  bool isACE;
  IsACE(input, &isACE);

  if (IsASCII(input)) {
    // first, canonicalize the host to lowercase, for whitelist lookup
    _retval = input;
    ToLowerCase(_retval);

    if (isACE && !mShowPunycode) {
      // ACEtoUTF8() can't fail, but might return the original ACE string
      nsAutoCString temp(_retval);
      // If the domain is in the whitelist, return the host in UTF-8.
      // Otherwise convert from ACE to UTF8 only those labels which are
      // considered safe for display
      ACEtoUTF8(temp, _retval, isInWhitelist(temp) ?
                                 eStringPrepIgnoreErrors : eStringPrepForUI);
      *_isASCII = IsASCII(_retval);
    } else {
      *_isASCII = true;
    }
  } else {
    // We have to normalize the hostname before testing against the domain
    // whitelist (see bug 315411), and to ensure the entire string gets
    // normalized.
    //
    // Normalization and the tests for safe display below, assume that the
    // input is Unicode, so first convert any ACE labels to UTF8
    if (isACE) {
      nsAutoCString temp;
      ACEtoUTF8(input, temp, eStringPrepIgnoreErrors);
      rv = Normalize(temp, _retval);
    } else {
      rv = Normalize(input, _retval);
    }
    if (NS_FAILED(rv)) return rv;

    if (mShowPunycode && NS_SUCCEEDED(UTF8toACE(_retval, _retval,
                                                eStringPrepIgnoreErrors))) {
      *_isASCII = true;
      return NS_OK;
    }

    // normalization could result in an ASCII-only hostname. alternatively, if
    // the host is converted to ACE by the normalizer, then the host may contain
    // unsafe characters, so leave it ACE encoded. see bug 283016, bug 301694, and bug 309311.
    *_isASCII = IsASCII(_retval);
    if (!*_isASCII && !isInWhitelist(_retval)) {
      // UTF8toACE with eStringPrepForUI may return a domain name where
      // some labels are in UTF-8 and some are in ACE, depending on
      // whether they are considered safe for display
      rv = UTF8toACE(_retval, _retval, eStringPrepForUI);
      *_isASCII = IsASCII(_retval);
      return rv;
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult utf16ToUcs4(const nsAString& in,
                            uint32_t *out,
                            uint32_t outBufLen,
                            uint32_t *outLen)
{
  uint32_t i = 0;
  nsAString::const_iterator start, end;
  in.BeginReading(start); 
  in.EndReading(end); 

  while (start != end) {
    char16_t curChar;

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
    if (i >= outBufLen)
      return NS_ERROR_MALFORMED_URI;
  }
  out[i] = (uint32_t)'\0';
  *outLen = i;
  return NS_OK;
}

#ifndef IDNA2008
static void ucs4toUtf16(const uint32_t *in, nsAString& out)
{
  while (*in) {
    if (!IS_IN_BMP(*in)) {
      out.Append((char16_t) H_SURROGATE(*in));
      out.Append((char16_t) L_SURROGATE(*in));
    }
    else
      out.Append((char16_t) *in);
    in++;
  }
}
#endif

static nsresult punycode(const nsAString& in, nsACString& out)
{
  uint32_t ucs4Buf[kMaxDNSNodeLen + 1];
  uint32_t ucs4Len = 0u;
  nsresult rv = utf16ToUcs4(in, ucs4Buf, kMaxDNSNodeLen, &ucs4Len);
  NS_ENSURE_SUCCESS(rv, rv);

  // need maximum 20 bits to encode 16 bit Unicode character
  // (include null terminator)
  const uint32_t kEncodedBufSize = kMaxDNSNodeLen * 20 / 8 + 1 + 1;  
  char encodedBuf[kEncodedBufSize];
  punycode_uint encodedLength = kEncodedBufSize;

  enum punycode_status status = punycode_encode(ucs4Len,
                                                ucs4Buf,
                                                nullptr,
                                                &encodedLength,
                                                encodedBuf);

  if (punycode_success != status ||
      encodedLength >= kEncodedBufSize)
    return NS_ERROR_MALFORMED_URI;

  encodedBuf[encodedLength] = '\0';
  out.Assign(nsDependentCString(kACEPrefix) + nsDependentCString(encodedBuf));

  return rv;
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
                                  stringPrepFlag flag)
{
#ifdef IDNA2008
  return IDNA2008StringPrep(in, out, flag);
#else
  if (!mNamePrepHandle || !mNormalizer)
    return NS_ERROR_FAILURE;

  uint32_t ucs4Buf[kMaxDNSNodeLen + 1];
  uint32_t ucs4Len;
  nsresult rv = utf16ToUcs4(in, ucs4Buf, kMaxDNSNodeLen, &ucs4Len);
  NS_ENSURE_SUCCESS(rv, rv);

  // map
  idn_result_t idn_err;

  uint32_t namePrepBuf[kMaxDNSNodeLen * 3];   // map up to three characters
  idn_err = idn_nameprep_map(mNamePrepHandle, (const uint32_t *) ucs4Buf,
		                     (uint32_t *) namePrepBuf, kMaxDNSNodeLen * 3);
  NS_ENSURE_TRUE(idn_err == idn_success, NS_ERROR_MALFORMED_URI);

  nsAutoString namePrepStr;
  ucs4toUtf16(namePrepBuf, namePrepStr);
  if (namePrepStr.Length() >= kMaxDNSNodeLen)
    return NS_ERROR_MALFORMED_URI;

  // normalize
  nsAutoString normlizedStr;
  rv = mNormalizer->NormalizeUnicodeNFKC(namePrepStr, normlizedStr);
  if (normlizedStr.Length() >= kMaxDNSNodeLen)
    return NS_ERROR_MALFORMED_URI;

  // set the result string
  out.Assign(normlizedStr);

  if (flag == eStringPrepIgnoreErrors) {
    return NS_OK;
  }

  // prohibit
  const uint32_t *found = nullptr;
  idn_err = idn_nameprep_isprohibited(mNamePrepHandle,
                                      (const uint32_t *) ucs4Buf, &found);
  if (idn_err != idn_success || found) {
    rv = NS_ERROR_MALFORMED_URI;
  } else {
    // check bidi
    idn_err = idn_nameprep_isvalidbidi(mNamePrepHandle,
                                       (const uint32_t *) ucs4Buf, &found);
    if (idn_err != idn_success || found) {
      rv = NS_ERROR_MALFORMED_URI;
    } else  if (flag == eStringPrepForUI) {
      // check unassigned code points
      idn_err = idn_nameprep_isunassigned(mNamePrepHandle,
                                          (const uint32_t *) ucs4Buf, &found);
      if (idn_err != idn_success || found) {
        rv = NS_ERROR_MALFORMED_URI;
      }
    }
  }

  if (flag == eStringPrepForDNS && NS_FAILED(rv)) {
    out.Truncate();
  }

  return rv;
#endif
}

nsresult nsIDNService::stringPrepAndACE(const nsAString& in, nsACString& out,
                                        stringPrepFlag flag)
{
  nsresult rv = NS_OK;

  out.Truncate();

  if (in.Length() > kMaxDNSNodeLen) {
    NS_WARNING("IDN node too large");
    return NS_ERROR_MALFORMED_URI;
  }

  if (IsASCII(in)) {
    LossyCopyUTF16toASCII(in, out);
    return NS_OK;
  }

  nsAutoString strPrep;
  rv = stringPrep(in, strPrep, flag);
  if (flag == eStringPrepForDNS) {
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsASCII(strPrep)) {
    LossyCopyUTF16toASCII(strPrep, out);
    return NS_OK;
  }

  if (flag == eStringPrepForUI && NS_SUCCEEDED(rv) && isLabelSafe(in)) {
    CopyUTF16toUTF8(strPrep, out);
    return NS_OK;
  }

  rv = punycode(strPrep, out);
  // Check that the encoded output isn't larger than the maximum length
  // of a DNS node per RFC 1034.
  // This test isn't necessary in the code paths above where the input
  // is ASCII (since the output will be the same length as the input) or
  // where we convert to UTF-8 (since the output is only used for
  // display in the UI and not passed to DNS and can legitimately be
  // longer than the limit).
  if (out.Length() > kMaxDNSNodeLen) {
    NS_WARNING("IDN node too large");
    return NS_ERROR_MALFORMED_URI;
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
  int32_t index = 0;

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
                                 stringPrepFlag flag)
{
  bool isAce;
  IsACE(in, &isAce);
  if (!isAce) {
    out.Assign(in);
    return NS_OK;
  }

  nsAutoString utf16;
#ifdef IDNA2008
  nsresult result = IDNA2008ToUnicode(in, utf16);
  NS_ENSURE_SUCCESS(result, result);
#else
  // RFC 3490 - 4.2 ToUnicode
  // The ToUnicode output never contains more code points than its input.
  punycode_uint output_length = in.Length() - kACEPrefixLen + 1;
  auto *output = new punycode_uint[output_length];
  NS_ENSURE_TRUE(output, NS_ERROR_OUT_OF_MEMORY);

  enum punycode_status status = punycode_decode(in.Length() - kACEPrefixLen,
                                                PromiseFlatCString(in).get() + kACEPrefixLen,
                                                &output_length,
                                                output,
                                                nullptr);
  if (status != punycode_success) {
    delete [] output;
    return NS_ERROR_MALFORMED_URI;
  }

  // UCS4 -> UTF8
  output[output_length] = 0;
  ucs4toUtf16(output, utf16);
  delete [] output;
#endif
  if (flag != eStringPrepForUI || isLabelSafe(utf16)) {
    CopyUTF16toUTF8(utf16, out);
  } else {
    out.Assign(in);
    return NS_OK;
  }

  // Validation: encode back to ACE and compare the strings
  nsAutoCString ace;
  nsresult rv = UTF8toACE(out, ace, flag);
  NS_ENSURE_SUCCESS(rv, rv);

  if (flag == eStringPrepForDNS &&
      !ace.Equals(in, nsCaseInsensitiveCStringComparator())) {
    return NS_ERROR_MALFORMED_URI;
  }

  return NS_OK;
}

bool nsIDNService::isInWhitelist(const nsACString &host)
{
  if (mIDNUseWhitelist && mIDNWhitelistPrefBranch) {
    nsAutoCString tld(host);
    // make sure the host is ACE for lookup and check that there are no
    // unassigned codepoints
    if (!IsASCII(tld) && NS_FAILED(UTF8toACE(tld, tld, eStringPrepForDNS))) {
      return false;
    }

    // truncate trailing dots first
    tld.Trim(".");
    int32_t pos = tld.RFind(".");
    if (pos == kNotFound)
      return false;

    tld.Cut(0, pos + 1);

    bool safe;
    if (NS_SUCCEEDED(mIDNWhitelistPrefBranch->GetBoolPref(tld.get(), &safe)))
      return safe;
  }

  return false;
}

bool nsIDNService::isLabelSafe(const nsAString &label)
{
  if (!isOnlySafeChars(PromiseFlatString(label), mIDNBlacklist)) {
    return false;
  }

  // We should never get here if the label is ASCII
  NS_ASSERTION(!IsASCII(label), "ASCII label in IDN checking");
  if (mRestrictionProfile == eASCIIOnlyProfile) {
    return false;
  }

  nsAString::const_iterator current, end;
  label.BeginReading(current);
  label.EndReading(end);

  Script lastScript = Script::INVALID;
  uint32_t previousChar = 0;
  uint32_t savedNumberingSystem = 0;
// Simplified/Traditional Chinese check temporarily disabled -- bug 857481
#if 0
  HanVariantType savedHanVariant = HVT_NotHan;
#endif

  int32_t savedScript = -1;

  while (current != end) {
    uint32_t ch = *current++;

    if (NS_IS_HIGH_SURROGATE(ch) && current != end &&
        NS_IS_LOW_SURROGATE(*current)) {
      ch = SURROGATE_TO_UCS4(ch, *current++);
    }

    // Check for restricted characters; aspirational scripts are NOT permitted,
    // in anticipation of the category being merged into Limited-Use scripts
    // in the upcoming (Unicode 10.0-based) revision of UAX #31.
    IdentifierType idType = GetIdentifierType(ch);
    if (idType == IDTYPE_RESTRICTED || idType == IDTYPE_ASPIRATIONAL) {
      return false;
    }
    MOZ_ASSERT(idType == IDTYPE_ALLOWED);

    // Check for mixed script
    Script script = GetScriptCode(ch);
    if (script != Script::COMMON &&
        script != Script::INHERITED &&
        script != lastScript) {
      if (illegalScriptCombo(script, savedScript)) {
        return false;
      }
      lastScript = script;
    }

    // Check for mixed numbering systems
    if (GetGeneralCategory(ch) ==
        HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER) {
      uint32_t zeroCharacter = ch - GetNumericValue(ch);
      if (savedNumberingSystem == 0) {
        // If we encounter a decimal number, save the zero character from that
        // numbering system.
        savedNumberingSystem = zeroCharacter;
      } else if (zeroCharacter != savedNumberingSystem) {
        return false;
      }
    }

    // Check for consecutive non-spacing marks
    if (previousChar != 0 &&
        previousChar == ch &&
        GetGeneralCategory(ch) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) {
      return false;
    }

    // Simplified/Traditional Chinese check temporarily disabled -- bug 857481
#if 0

    // Check for both simplified-only and traditional-only Chinese characters
    HanVariantType hanVariant = GetHanVariant(ch);
    if (hanVariant == HVT_SimplifiedOnly || hanVariant == HVT_TraditionalOnly) {
      if (savedHanVariant == HVT_NotHan) {
        savedHanVariant = hanVariant;
      } else if (hanVariant != savedHanVariant)  {
        return false;
      }
    }
#endif

    previousChar = ch;
  }
  return true;
}

// Scripts that we care about in illegalScriptCombo
static const Script scriptTable[] = {
  Script::BOPOMOFO, Script::CYRILLIC, Script::GREEK,
  Script::HANGUL,   Script::HAN,      Script::HIRAGANA,
  Script::KATAKANA, Script::LATIN };

#define BOPO 0
#define CYRL 1
#define GREK 2
#define HANG 3
#define HANI 4
#define HIRA 5
#define KATA 6
#define LATN 7
#define OTHR 8
#define JPAN 9    // Latin + Han + Hiragana + Katakana
#define CHNA 10   // Latin + Han + Bopomofo
#define KORE 11   // Latin + Han + Hangul
#define HNLT 12   // Latin + Han (could be any of the above combinations)
#define FAIL 13

static inline int32_t findScriptIndex(Script aScript)
{
  int32_t tableLength = mozilla::ArrayLength(scriptTable);
  for (int32_t index = 0; index < tableLength; ++index) {
    if (aScript == scriptTable[index]) {
      return index;
    }
  }
  return OTHR;
}

static const int32_t scriptComboTable[13][9] = {
/* thisScript: BOPO  CYRL  GREK  HANG  HANI  HIRA  KATA  LATN  OTHR
 * savedScript */
 /* BOPO */  { BOPO, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL },
 /* CYRL */  { FAIL, CYRL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL },
 /* GREK */  { FAIL, FAIL, GREK, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL },
 /* HANG */  { FAIL, FAIL, FAIL, HANG, KORE, FAIL, FAIL, KORE, FAIL },
 /* HANI */  { CHNA, FAIL, FAIL, KORE, HANI, JPAN, JPAN, HNLT, FAIL },
 /* HIRA */  { FAIL, FAIL, FAIL, FAIL, JPAN, HIRA, JPAN, JPAN, FAIL },
 /* KATA */  { FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, KATA, JPAN, FAIL },
 /* LATN */  { CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, LATN, OTHR },
 /* OTHR */  { FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, OTHR, FAIL },
 /* JPAN */  { FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, JPAN, JPAN, FAIL },
 /* CHNA */  { CHNA, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL },
 /* KORE */  { FAIL, FAIL, FAIL, KORE, KORE, FAIL, FAIL, KORE, FAIL },
 /* HNLT */  { CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, HNLT, FAIL }
};

bool nsIDNService::illegalScriptCombo(Script script, int32_t& savedScript)
{
  if (savedScript == -1) {
    savedScript = findScriptIndex(script);
    return false;
  }

  savedScript = scriptComboTable[savedScript] [findScriptIndex(script)];
  /*
   * Special case combinations that depend on which profile is in use
   * In the Highly Restrictive profile Latin is not allowed with any
   *  other script
   *
   * In the Moderately Restrictive profile Latin mixed with any other
   *  single script is allowed.
   */
  return ((savedScript == OTHR &&
           mRestrictionProfile == eHighlyRestrictiveProfile) ||
          savedScript == FAIL);
}

#undef BOPO
#undef CYRL
#undef GREK
#undef HANG
#undef HANI
#undef HIRA
#undef KATA
#undef LATN
#undef OTHR
#undef JPAN
#undef CHNA
#undef KORE
#undef HNLT
#undef FAIL
