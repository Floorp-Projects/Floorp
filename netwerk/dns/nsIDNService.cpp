/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "mozilla/Preferences.h"
#include "nsIDNService.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "harfbuzz/hb.h"
#include "punycode.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"
#include "mozilla/intl/FormatBuffer.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "mozilla/intl/UnicodeScriptCodes.h"

#include "ICUUtils.h"

using namespace mozilla;
using namespace mozilla::intl;
using namespace mozilla::unicode;
using namespace mozilla::net;
using mozilla::Preferences;

// Currently we use the non-transitional processing option -- see
// http://unicode.org/reports/tr46/
// To switch to transitional processing, change the value of this flag
// and kTransitionalProcessing in netwerk/test/unit/test_idna2008.js to true
// (revert bug 1218179).
const intl::IDNA::ProcessingType kIDNA2008_DefaultProcessingType =
    intl::IDNA::ProcessingType::NonTransitional;

//-----------------------------------------------------------------------------
// According to RFC 1034 - 3.1. Name space specifications and terminology
// the maximum label size would be 63. However, this is enforced at the DNS
// level and none of the other browsers seem to not enforce the VerifyDnsLength
// check in https://unicode.org/reports/tr46/#ToASCII
// Instead, we choose a rather arbitrary but larger size.
static const uint32_t kMaxULabelSize = 256;
// RFC 3490 - 5.   ACE prefix
static const char kACEPrefix[] = "xn--";

//-----------------------------------------------------------------------------

#define NS_NET_PREF_EXTRAALLOWED "network.IDN.extra_allowed_chars"
#define NS_NET_PREF_EXTRABLOCKED "network.IDN.extra_blocked_chars"
#define NS_NET_PREF_SHOWPUNYCODE "network.IDN_show_punycode"
#define NS_NET_PREF_IDNWHITELIST "network.IDN.whitelist."
#define NS_NET_PREF_IDNUSEWHITELIST "network.IDN.use_whitelist"
#define NS_NET_PREF_IDNRESTRICTION "network.IDN.restriction_profile"

static inline bool isOnlySafeChars(const nsString& in,
                                   const nsTArray<BlocklistRange>& aBlocklist) {
  if (aBlocklist.IsEmpty()) {
    return true;
  }
  const char16_t* cur = in.BeginReading();
  const char16_t* end = in.EndReading();

  for (; cur < end; ++cur) {
    if (CharInBlocklist(*cur, aBlocklist)) {
      return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

/* Implementation file */
NS_IMPL_ISUPPORTS(nsIDNService, nsIIDNService, nsISupportsWeakReference)

static const char* gCallbackPrefs[] = {
    NS_NET_PREF_EXTRAALLOWED,    NS_NET_PREF_EXTRABLOCKED,
    NS_NET_PREF_SHOWPUNYCODE,    NS_NET_PREF_IDNRESTRICTION,
    NS_NET_PREF_IDNUSEWHITELIST, nullptr,
};

nsresult nsIDNService::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexSingleWriterAutoLock lock(mLock);

  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    prefs->GetBranch(NS_NET_PREF_IDNWHITELIST,
                     getter_AddRefs(mIDNWhitelistPrefBranch));
  }

  Preferences::RegisterPrefixCallbacks(PrefChanged, gCallbackPrefs, this);
  prefsChanged(nullptr);
  InitializeBlocklist(mIDNBlocklist);

  return NS_OK;
}

void nsIDNService::prefsChanged(const char* pref) {
  MOZ_ASSERT(NS_IsMainThread());
  mLock.AssertCurrentThreadOwns();

  if (pref && nsLiteralCString(NS_NET_PREF_EXTRAALLOWED).Equals(pref)) {
    InitializeBlocklist(mIDNBlocklist);
  }
  if (pref && nsLiteralCString(NS_NET_PREF_EXTRABLOCKED).Equals(pref)) {
    InitializeBlocklist(mIDNBlocklist);
  }
  if (!pref || nsLiteralCString(NS_NET_PREF_SHOWPUNYCODE).Equals(pref)) {
    bool val;
    if (NS_SUCCEEDED(Preferences::GetBool(NS_NET_PREF_SHOWPUNYCODE, &val))) {
      mShowPunycode = val;
    }
  }
  if (!pref || nsLiteralCString(NS_NET_PREF_IDNUSEWHITELIST).Equals(pref)) {
    bool val;
    if (NS_SUCCEEDED(Preferences::GetBool(NS_NET_PREF_IDNUSEWHITELIST, &val))) {
      mIDNUseWhitelist = val;
    }
  }
  if (!pref || nsLiteralCString(NS_NET_PREF_IDNRESTRICTION).Equals(pref)) {
    nsAutoCString profile;
    if (NS_FAILED(
            Preferences::GetCString(NS_NET_PREF_IDNRESTRICTION, profile))) {
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

nsIDNService::nsIDNService() : mLock("IDNSService", this) {
  MOZ_ASSERT(NS_IsMainThread());
  mLock.AssertOnWritingThread();

  auto createResult =
      mozilla::intl::IDNA::TryCreate(kIDNA2008_DefaultProcessingType);
  MOZ_ASSERT(createResult.isOk());
  mIDNA = createResult.unwrap();
}

nsIDNService::~nsIDNService() {
  MOZ_ASSERT(NS_IsMainThread());
  mLock.AssertOnWritingThread();

  Preferences::UnregisterPrefixCallbacks(PrefChanged, gCallbackPrefs, this);
}

nsresult nsIDNService::IDNA2008ToUnicode(const nsACString& input,
                                         nsAString& output) {
  NS_ConvertUTF8toUTF16 inputStr(input);

  Span<const char16_t> inputSpan{inputStr};
  intl::nsTStringToBufferAdapter buffer(output);
  auto result = mIDNA->LabelToUnicode(inputSpan, buffer);

  nsresult rv = NS_OK;
  if (result.isErr()) {
    rv = ICUUtils::ICUErrorToNsResult(result.unwrapErr());
    if (rv == NS_ERROR_FAILURE) {
      rv = NS_ERROR_MALFORMED_URI;
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  intl::IDNA::Info info = result.unwrap();
  if (info.HasErrors()) {
    rv = NS_ERROR_MALFORMED_URI;
  }

  return rv;
}

nsresult nsIDNService::IDNA2008StringPrep(const nsAString& input,
                                          nsAString& output,
                                          stringPrepFlag flag) {
  Span<const char16_t> inputSpan{input};
  intl::nsTStringToBufferAdapter buffer(output);
  auto result = mIDNA->LabelToUnicode(inputSpan, buffer);

  nsresult rv = NS_OK;
  if (result.isErr()) {
    rv = ICUUtils::ICUErrorToNsResult(result.unwrapErr());
    if (rv == NS_ERROR_FAILURE) {
      rv = NS_ERROR_MALFORMED_URI;
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  intl::IDNA::Info info = result.unwrap();

  // Output the result of nameToUnicode even if there were errors.
  // But in the case of invalid punycode, the uidna_labelToUnicode result
  // appears to get an appended U+FFFD REPLACEMENT CHARACTER, which will
  // confuse our subsequent processing, so we drop that.
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=1399540#c9)
  if (info.HasInvalidPunycode() && !output.IsEmpty() &&
      output.Last() == 0xfffd) {
    output.Truncate(output.Length() - 1);
  }

  if (flag == eStringPrepIgnoreErrors) {
    return NS_OK;
  }

  bool hasError = flag == eStringPrepForDNS
                      ? info.HasErrors() && !info.HasInvalidHyphen()
                      : info.HasErrors();

  if (hasError) {
    if (flag == eStringPrepForDNS) {
      output.Truncate();
    }
    rv = NS_ERROR_MALFORMED_URI;
  }

  return rv;
}

NS_IMETHODIMP nsIDNService::ConvertUTF8toACE(const nsACString& input,
                                             nsACString& ace) {
  return UTF8toACE(input, ace, eStringPrepForDNS);
}

nsresult nsIDNService::UTF8toACE(const nsACString& input, nsACString& ace,
                                 stringPrepFlag flag) {
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

NS_IMETHODIMP nsIDNService::ConvertACEtoUTF8(const nsACString& input,
                                             nsACString& _retval) {
  return ACEtoUTF8(input, _retval, eStringPrepForDNS);
}

nsresult nsIDNService::ACEtoUTF8(const nsACString& input, nsACString& _retval,
                                 stringPrepFlag flag) {
  // RFC 3490 - 4.2 ToUnicode
  // ToUnicode never fails.  If any step fails, then the original input
  // sequence is returned immediately in that step.
  //
  // Note that this refers to the decoding of a single label.
  // ACEtoUTF8 may be called with a sequence of labels separated by dots;
  // this test applies individually to each label.

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
      nsDependentCSubstring origLabel(input, offset, len - 1);
      if (NS_FAILED(decodeACE(origLabel, decodedBuf, flag))) {
        // If decoding failed, use the original input sequence
        // for this label.
        _retval.Append(origLabel);
      } else {
        _retval.Append(decodedBuf);
      }

      _retval.Append('.');
      offset += len;
      len = 0;
    }
  }
  // decode the last node
  if (len) {
    nsDependentCSubstring origLabel(input, offset, len);
    if (NS_FAILED(decodeACE(origLabel, decodedBuf, flag))) {
      _retval.Append(origLabel);
    } else {
      _retval.Append(decodedBuf);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsIDNService::IsACE(const nsACString& input, bool* _retval) {
  // look for the ACE prefix in the input string.  it may occur
  // at the beginning of any segment in the domain name.  for
  // example: "www.xn--ENCODED.com"

  if (!IsAscii(input)) {
    *_retval = false;
    return NS_OK;
  }

  auto stringContains = [](const nsACString& haystack,
                           const nsACString& needle) {
    return std::search(haystack.BeginReading(), haystack.EndReading(),
                       needle.BeginReading(),
                       needle.EndReading()) != haystack.EndReading();
  };

  *_retval = StringBeginsWith(input, "xn--"_ns) ||
             (!input.IsEmpty() && input[0] != '.' &&
              stringContains(input, ".xn--"_ns));
  return NS_OK;
}

NS_IMETHODIMP nsIDNService::Normalize(const nsACString& input,
                                      nsACString& output) {
  // protect against bogus input
  NS_ENSURE_TRUE(IsUtf8(input), NS_ERROR_UNEXPECTED);

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

namespace {

template <typename T>
class MOZ_CAPABILITY MOZ_STACK_CLASS MutexSettableAutoUnlock final {
 private:
  T* mMutex = nullptr;

 public:
  MutexSettableAutoUnlock() = default;

  void Acquire(T& aMutex) MOZ_CAPABILITY_ACQUIRE(aMutex) {
    MOZ_ASSERT(!mMutex);
    mMutex = &aMutex;
    aMutex.Lock();
  }

  // Since this is commonly used in "if (!NS_IsMainThread())", if we make
  // this MOZ_CAPABILITY_RELEASE(mMutex) we'll generate warnings on each return.
  // We may still get a warning at the end of a function using this
  ~MutexSettableAutoUnlock() MOZ_CAPABILITY_ACQUIRE() {
    if (mMutex) {
      mMutex->Unlock();
    }
  }
};

}  // anonymous namespace

// conditional locks blow threadsafety's mind
NS_IMETHODIMP nsIDNService::ConvertToDisplayIDN(
    const nsACString& input, bool* _isASCII,
    nsACString& _retval) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  MutexSettableAutoUnlock<MutexSingleWriter> lock;
  if (!NS_IsMainThread()) {
    lock.Acquire(mLock);
  }
  mLock.AssertOnWritingThreadOrHeld();

  // If host is ACE, then convert to UTF-8 if the host is in the IDN whitelist.
  // Else, if host is already UTF-8, then make sure it is normalized per IDN.

  nsresult rv = NS_OK;

  // Even if the hostname is not ASCII, individual labels may still be ACE, so
  // test IsACE before testing IsASCII
  bool isACE;
  IsACE(input, &isACE);

  if (IsAscii(input)) {
    // first, canonicalize the host to lowercase, for whitelist lookup
    _retval = input;
    ToLowerCase(_retval);

    if (isACE && !mShowPunycode) {
      // ACEtoUTF8() can't fail, but might return the original ACE string
      nsAutoCString temp(_retval);
      // If the domain is in the whitelist, return the host in UTF-8.
      // Otherwise convert from ACE to UTF8 only those labels which are
      // considered safe for display
      ACEtoUTF8(
          temp, _retval,
          isInWhitelist(temp) ? eStringPrepIgnoreErrors : eStringPrepForUI);
      *_isASCII = IsAscii(_retval);
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
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mShowPunycode &&
        NS_SUCCEEDED(UTF8toACE(_retval, _retval, eStringPrepIgnoreErrors))) {
      *_isASCII = true;
      return NS_OK;
    }

    // normalization could result in an ASCII-only hostname. alternatively, if
    // the host is converted to ACE by the normalizer, then the host may contain
    // unsafe characters, so leave it ACE encoded. see bug 283016, bug 301694,
    // and bug 309311.
    *_isASCII = IsAscii(_retval);
    if (!*_isASCII && !isInWhitelist(_retval)) {
      // UTF8toACE with eStringPrepForUI may return a domain name where
      // some labels are in UTF-8 and some are in ACE, depending on
      // whether they are considered safe for display
      rv = UTF8toACE(_retval, _retval, eStringPrepForUI);
      *_isASCII = IsAscii(_retval);
      return rv;
    }
  }

  return NS_OK;
}  // Will generate a mutex still-held warning

//-----------------------------------------------------------------------------

static nsresult utf16ToUcs4(const nsAString& in, uint32_t* out,
                            uint32_t outBufLen, uint32_t* outLen) {
  uint32_t i = 0;
  nsAString::const_iterator start, end;
  in.BeginReading(start);
  in.EndReading(end);

  while (start != end) {
    char16_t curChar;

    curChar = *start++;

    if (start != end && NS_IS_SURROGATE_PAIR(curChar, *start)) {
      out[i] = SURROGATE_TO_UCS4(curChar, *start);
      ++start;
    } else {
      out[i] = curChar;
    }

    i++;
    if (i >= outBufLen) {
      return NS_ERROR_MALFORMED_URI;
    }
  }
  out[i] = (uint32_t)'\0';
  *outLen = i;
  return NS_OK;
}

static nsresult punycode(const nsAString& in, nsACString& out) {
  uint32_t ucs4Buf[kMaxULabelSize + 1];
  uint32_t ucs4Len = 0u;
  nsresult rv = utf16ToUcs4(in, ucs4Buf, kMaxULabelSize, &ucs4Len);
  NS_ENSURE_SUCCESS(rv, rv);

  // need maximum 20 bits to encode 16 bit Unicode character
  // (include null terminator)
  const uint32_t kEncodedBufSize = kMaxULabelSize * 20 / 8 + 1 + 1;
  char encodedBuf[kEncodedBufSize];
  punycode_uint encodedLength = kEncodedBufSize;

  enum punycode_status status =
      punycode_encode(ucs4Len, ucs4Buf, nullptr, &encodedLength, encodedBuf);

  if (punycode_success != status || encodedLength >= kEncodedBufSize) {
    return NS_ERROR_MALFORMED_URI;
  }

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
                                  stringPrepFlag flag) {
  return IDNA2008StringPrep(in, out, flag);
}

nsresult nsIDNService::stringPrepAndACE(const nsAString& in, nsACString& out,
                                        stringPrepFlag flag) {
  nsresult rv = NS_OK;

  out.Truncate();

  if (IsAscii(in)) {
    LossyCopyUTF16toASCII(in, out);
    // If label begins with xn-- we still want to check its validity
    if (!StringBeginsWith(in, u"xn--"_ns)) {
      return NS_OK;
    }
  }

  nsAutoString strPrep;
  rv = stringPrep(in, strPrep, flag);
  if (flag == eStringPrepForDNS) {
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsAscii(strPrep)) {
    LossyCopyUTF16toASCII(strPrep, out);
    return NS_OK;
  }

  if (flag == eStringPrepForUI && NS_SUCCEEDED(rv) && isLabelSafe(in)) {
    CopyUTF16toUTF8(strPrep, out);
    return NS_OK;
  }

  return punycode(strPrep, out);
}

// RFC 3490
// 1) Whenever dots are used as label separators, the following characters
//    MUST be recognized as dots: U+002E (full stop), U+3002 (ideographic full
//    stop), U+FF0E (fullwidth full stop), U+FF61 (halfwidth ideographic full
//    stop).

void nsIDNService::normalizeFullStops(nsAString& s) {
  nsAString::const_iterator start, end;
  s.BeginReading(start);
  s.EndReading(end);
  int32_t index = 0;

  while (start != end) {
    switch (*start) {
      case 0x3002:
      case 0xFF0E:
      case 0xFF61:
        s.ReplaceLiteral(index, 1, u".");
        break;
      default:
        break;
    }
    start++;
    index++;
  }
}

nsresult nsIDNService::decodeACE(const nsACString& in, nsACString& out,
                                 stringPrepFlag flag) {
  bool isAce;
  IsACE(in, &isAce);
  if (!isAce) {
    out.Assign(in);
    return NS_OK;
  }

  nsAutoString utf16;
  nsresult result = IDNA2008ToUnicode(in, utf16);
  NS_ENSURE_SUCCESS(result, result);

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
      !ace.Equals(in, nsCaseInsensitiveCStringComparator)) {
    return NS_ERROR_MALFORMED_URI;
  }

  return NS_OK;
}

bool nsIDNService::isInWhitelist(const nsACString& host) {
  if (!NS_IsMainThread()) {
    mLock.AssertCurrentThreadOwns();
  } else {
    mLock.AssertOnWritingThread();
  }

  if (mIDNUseWhitelist && mIDNWhitelistPrefBranch) {
    nsAutoCString tld(host);
    // make sure the host is ACE for lookup and check that there are no
    // unassigned codepoints
    if (!IsAscii(tld) && NS_FAILED(UTF8toACE(tld, tld, eStringPrepForDNS))) {
      return false;
    }

    // truncate trailing dots first
    tld.Trim(".");
    int32_t pos = tld.RFind(".");
    if (pos == kNotFound) {
      return false;
    }

    tld.Cut(0, pos + 1);

    bool safe;
    if (NS_SUCCEEDED(mIDNWhitelistPrefBranch->GetBoolPref(tld.get(), &safe))) {
      return safe;
    }
  }

  return false;
}

bool nsIDNService::isLabelSafe(const nsAString& label) {
  if (!NS_IsMainThread()) {
    mLock.AssertCurrentThreadOwns();
  } else {
    mLock.AssertOnWritingThread();
  }

  if (!isOnlySafeChars(PromiseFlatString(label), mIDNBlocklist)) {
    return false;
  }

  // We should never get here if the label is ASCII
  NS_ASSERTION(!IsAscii(label), "ASCII label in IDN checking");
  if (mRestrictionProfile == eASCIIOnlyProfile) {
    return false;
  }

  nsAString::const_iterator current, end;
  label.BeginReading(current);
  label.EndReading(end);

  Script lastScript = Script::INVALID;
  uint32_t previousChar = 0;
  uint32_t baseChar = 0;  // last non-diacritic seen (base char for marks)
  uint32_t savedNumberingSystem = 0;
// Simplified/Traditional Chinese check temporarily disabled -- bug 857481
#if 0
  HanVariantType savedHanVariant = HVT_NotHan;
#endif

  int32_t savedScript = -1;

  while (current != end) {
    uint32_t ch = *current++;

    if (current != end && NS_IS_SURROGATE_PAIR(ch, *current)) {
      ch = SURROGATE_TO_UCS4(ch, *current++);
    }

    IdentifierType idType = GetIdentifierType(ch);
    if (idType == IDTYPE_RESTRICTED) {
      return false;
    }
    MOZ_ASSERT(idType == IDTYPE_ALLOWED);

    // Check for mixed script
    Script script = UnicodeProperties::GetScriptCode(ch);
    if (script != Script::COMMON && script != Script::INHERITED &&
        script != lastScript) {
      if (illegalScriptCombo(script, savedScript)) {
        return false;
      }
    }

    // Check for mixed numbering systems
    auto genCat = GetGeneralCategory(ch);
    if (genCat == HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER) {
      uint32_t zeroCharacter =
          ch - mozilla::intl::UnicodeProperties::GetNumericValue(ch);
      if (savedNumberingSystem == 0) {
        // If we encounter a decimal number, save the zero character from that
        // numbering system.
        savedNumberingSystem = zeroCharacter;
      } else if (zeroCharacter != savedNumberingSystem) {
        return false;
      }
    }

    if (genCat == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) {
      // Check for consecutive non-spacing marks.
      if (previousChar != 0 && previousChar == ch) {
        return false;
      }
      // Check for marks whose expected script doesn't match the base script.
      if (lastScript != Script::INVALID) {
        UnicodeProperties::ScriptExtensionVector scripts;
        auto extResult = UnicodeProperties::GetExtensions(ch, scripts);
        MOZ_ASSERT(extResult.isOk());
        if (extResult.isErr()) {
          return false;
        }

        int nScripts = AssertedCast<int>(scripts.length());

        // nScripts will always be >= 1, because even for undefined characters
        // it will return Script::INVALID.
        // If the mark just has script=COMMON or INHERITED, we can't check any
        // more carefully, but if it has specific scriptExtension codes, then
        // assume those are the only valid scripts to use it with.
        if (nScripts > 1 || (Script(scripts[0]) != Script::COMMON &&
                             Script(scripts[0]) != Script::INHERITED)) {
          while (--nScripts >= 0) {
            if (Script(scripts[nScripts]) == lastScript) {
              break;
            }
          }
          if (nScripts == -1) {
            return false;
          }
        }
      }
      // Check for diacritics on dotless-i, which would be indistinguishable
      // from normal accented letter i.
      if (baseChar == 0x0131 &&
          ((ch >= 0x0300 && ch <= 0x0314) || ch == 0x031a)) {
        return false;
      }
    } else {
      baseChar = ch;
    }

    if (script != Script::COMMON && script != Script::INHERITED) {
      lastScript = script;
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
    Script::BOPOMOFO, Script::CYRILLIC, Script::GREEK,    Script::HANGUL,
    Script::HAN,      Script::HIRAGANA, Script::KATAKANA, Script::LATIN};

#define BOPO 0
#define CYRL 1
#define GREK 2
#define HANG 3
#define HANI 4
#define HIRA 5
#define KATA 6
#define LATN 7
#define OTHR 8
#define JPAN 9   // Latin + Han + Hiragana + Katakana
#define CHNA 10  // Latin + Han + Bopomofo
#define KORE 11  // Latin + Han + Hangul
#define HNLT 12  // Latin + Han (could be any of the above combinations)
#define FAIL 13

static inline int32_t findScriptIndex(Script aScript) {
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
    /* BOPO */ {BOPO, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL},
    /* CYRL */ {FAIL, CYRL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL},
    /* GREK */ {FAIL, FAIL, GREK, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL},
    /* HANG */ {FAIL, FAIL, FAIL, HANG, KORE, FAIL, FAIL, KORE, FAIL},
    /* HANI */ {CHNA, FAIL, FAIL, KORE, HANI, JPAN, JPAN, HNLT, FAIL},
    /* HIRA */ {FAIL, FAIL, FAIL, FAIL, JPAN, HIRA, JPAN, JPAN, FAIL},
    /* KATA */ {FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, KATA, JPAN, FAIL},
    /* LATN */ {CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, LATN, OTHR},
    /* OTHR */ {FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, OTHR, FAIL},
    /* JPAN */ {FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, JPAN, JPAN, FAIL},
    /* CHNA */ {CHNA, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL},
    /* KORE */ {FAIL, FAIL, FAIL, KORE, KORE, FAIL, FAIL, KORE, FAIL},
    /* HNLT */ {CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, HNLT, FAIL}};

bool nsIDNService::illegalScriptCombo(Script script, int32_t& savedScript) {
  if (!NS_IsMainThread()) {
    mLock.AssertCurrentThreadOwns();
  } else {
    mLock.AssertOnWritingThread();
  }

  if (savedScript == -1) {
    savedScript = findScriptIndex(script);
    return false;
  }

  savedScript = scriptComboTable[savedScript][findScriptIndex(script)];
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
