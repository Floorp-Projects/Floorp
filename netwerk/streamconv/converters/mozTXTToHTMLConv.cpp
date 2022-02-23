/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextUtils.h"
#include "mozTXTToHTMLConv.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/Maybe.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "nsCRT.h"
#include "nsIExternalProtocolHandler.h"
#include "nsIURI.h"

#include <algorithm>

#ifdef DEBUG_BenB_Perf
#  include "prtime.h"
#  include "prinrval.h"
#endif

using mozilla::IsAscii;
using mozilla::IsAsciiAlpha;
using mozilla::IsAsciiDigit;
using mozilla::Maybe;
using mozilla::Some;
using mozilla::Span;
using mozilla::intl::GraphemeClusterBreakIteratorUtf16;
using mozilla::intl::GraphemeClusterBreakReverseIteratorUtf16;

const double growthRate = 1.2;

// Bug 183111, editor now replaces multiple spaces with leading
// 0xA0's and a single ending space, so need to treat 0xA0's as spaces.
// 0xA0 is the Latin1/Unicode character for "non-breaking space (nbsp)"
// Also recognize the Japanese ideographic space 0x3000 as a space.
static inline bool IsSpace(const char16_t aChar) {
  return (nsCRT::IsAsciiSpace(aChar) || aChar == 0xA0 || aChar == 0x3000);
}

// Escape Char will take ch, escape it and append the result to
// aStringToAppendTo
void mozTXTToHTMLConv::EscapeChar(const char16_t ch,
                                  nsAString& aStringToAppendTo,
                                  bool inAttribute) {
  switch (ch) {
    case '<':
      aStringToAppendTo.AppendLiteral("&lt;");
      break;
    case '>':
      aStringToAppendTo.AppendLiteral("&gt;");
      break;
    case '&':
      aStringToAppendTo.AppendLiteral("&amp;");
      break;
    case '"':
      if (inAttribute) {
        aStringToAppendTo.AppendLiteral("&quot;");
        break;
      }
      // else fall through
      [[fallthrough]];
    default:
      aStringToAppendTo += ch;
  }
}

// EscapeStr takes the passed in string and
// escapes it IN PLACE.
void mozTXTToHTMLConv::EscapeStr(nsString& aInString, bool inAttribute) {
  // the replace substring routines
  // don't seem to work if you have a character
  // in the in string that is also in the replacement
  // string! =(
  // aInString.ReplaceSubstring("&", "&amp;");
  // aInString.ReplaceSubstring("<", "&lt;");
  // aInString.ReplaceSubstring(">", "&gt;");
  for (uint32_t i = 0; i < aInString.Length();) {
    switch (aInString[i]) {
      case '<':
        aInString.Cut(i, 1);
        aInString.InsertLiteral(u"&lt;", i);
        i += 4;  // skip past the integers we just added
        break;
      case '>':
        aInString.Cut(i, 1);
        aInString.InsertLiteral(u"&gt;", i);
        i += 4;  // skip past the integers we just added
        break;
      case '&':
        aInString.Cut(i, 1);
        aInString.InsertLiteral(u"&amp;", i);
        i += 5;  // skip past the integers we just added
        break;
      case '"':
        if (inAttribute) {
          aInString.Cut(i, 1);
          aInString.InsertLiteral(u"&quot;", i);
          i += 6;
          break;
        }
        // else fall through
        [[fallthrough]];
      default:
        i++;
    }
  }
}

void mozTXTToHTMLConv::UnescapeStr(const char16_t* aInString, int32_t aStartPos,
                                   int32_t aLength, nsString& aOutString) {
  const char16_t* subString = nullptr;
  for (uint32_t i = aStartPos; int32_t(i) - aStartPos < aLength;) {
    int32_t remainingChars = i - aStartPos;
    if (aInString[i] == '&') {
      subString = &aInString[i];
      if (!NS_strncmp(subString, u"&lt;",
                      std::min(4, aLength - remainingChars))) {
        aOutString.Append(char16_t('<'));
        i += 4;
      } else if (!NS_strncmp(subString, u"&gt;",
                             std::min(4, aLength - remainingChars))) {
        aOutString.Append(char16_t('>'));
        i += 4;
      } else if (!NS_strncmp(subString, u"&amp;",
                             std::min(5, aLength - remainingChars))) {
        aOutString.Append(char16_t('&'));
        i += 5;
      } else if (!NS_strncmp(subString, u"&quot;",
                             std::min(6, aLength - remainingChars))) {
        aOutString.Append(char16_t('"'));
        i += 6;
      } else {
        aOutString += aInString[i];
        i++;
      }
    } else {
      aOutString += aInString[i];
      i++;
    }
  }
}

void mozTXTToHTMLConv::CompleteAbbreviatedURL(const char16_t* aInString,
                                              int32_t aInLength,
                                              const uint32_t pos,
                                              nsString& aOutString) {
  NS_ASSERTION(int32_t(pos) < aInLength,
               "bad args to CompleteAbbreviatedURL, see bug #190851");
  if (int32_t(pos) >= aInLength) return;

  if (aInString[pos] == '@') {
    // only pre-pend a mailto url if the string contains a .domain in it..
    // i.e. we want to linkify johndoe@foo.com but not "let's meet @8pm"
    nsDependentString inString(aInString, aInLength);
    if (inString.FindChar('.', pos) !=
        kNotFound)  // if we have a '.' after the @ sign....
    {
      aOutString.AssignLiteral("mailto:");
      aOutString += aInString;
    }
  } else if (aInString[pos] == '.') {
    if (ItMatchesDelimited(aInString, aInLength, u"www.", 4, LT_IGNORE,
                           LT_IGNORE)) {
      aOutString.AssignLiteral("http://");
      aOutString += aInString;
    } else if (ItMatchesDelimited(aInString, aInLength, u"ftp.", 4, LT_IGNORE,
                                  LT_IGNORE)) {
      aOutString.AssignLiteral("ftp://");
      aOutString += aInString;
    }
  }
}

bool mozTXTToHTMLConv::FindURLStart(const char16_t* aInString,
                                    int32_t aInLength, const uint32_t pos,
                                    const modetype check, uint32_t& start) {
  switch (check) {  // no breaks, because end of blocks is never reached
    case RFC1738: {
      if (!NS_strncmp(&aInString[std::max(int32_t(pos - 4), 0)], u"<URL:", 5)) {
        start = pos + 1;
        return true;
      }
      return false;
    }
    case RFC2396E: {
      nsString temp(aInString, aInLength);
      int32_t i = pos <= 0 ? kNotFound : temp.RFindCharInSet(u"<>\"", pos - 1);
      if (i != kNotFound &&
          (temp[uint32_t(i)] == '<' || temp[uint32_t(i)] == '"')) {
        start = uint32_t(++i);
        return start < pos;
      }
      return false;
    }
    case freetext: {
      int32_t i = pos - 1;
      for (; i >= 0 &&
             (IsAsciiAlpha(aInString[uint32_t(i)]) ||
              IsAsciiDigit(aInString[uint32_t(i)]) ||
              aInString[uint32_t(i)] == '+' || aInString[uint32_t(i)] == '-' ||
              aInString[uint32_t(i)] == '.');
           i--) {
        ;
      }
      if (++i >= 0 && uint32_t(i) < pos &&
          IsAsciiAlpha(aInString[uint32_t(i)])) {
        start = uint32_t(i);
        return true;
      }
      return false;
    }
    case abbreviated: {
      int32_t i = pos - 1;
      // This disallows non-ascii-characters for email.
      // Currently correct, but revisit later after standards changed.
      bool isEmail = aInString[pos] == (char16_t)'@';
      // These chars mark the start of the URL
      for (; i >= 0 && aInString[uint32_t(i)] != '>' &&
             aInString[uint32_t(i)] != '<' && aInString[uint32_t(i)] != '"' &&
             aInString[uint32_t(i)] != '\'' && aInString[uint32_t(i)] != '`' &&
             aInString[uint32_t(i)] != ',' && aInString[uint32_t(i)] != '{' &&
             aInString[uint32_t(i)] != '[' && aInString[uint32_t(i)] != '(' &&
             aInString[uint32_t(i)] != '|' && aInString[uint32_t(i)] != '\\' &&
             !IsSpace(aInString[uint32_t(i)]) &&
             (!isEmail || IsAscii(aInString[uint32_t(i)])) &&
             (!isEmail || aInString[uint32_t(i)] != ')');
           i--) {
        ;
      }
      if (++i >= 0 && uint32_t(i) < pos &&
          (IsAsciiAlpha(aInString[uint32_t(i)]) ||
           IsAsciiDigit(aInString[uint32_t(i)]))) {
        start = uint32_t(i);
        return true;
      }
      return false;
    }
    default:
      return false;
  }  // switch
}

bool mozTXTToHTMLConv::FindURLEnd(const char16_t* aInString,
                                  int32_t aInStringLength, const uint32_t pos,
                                  const modetype check, const uint32_t start,
                                  uint32_t& end) {
  switch (check) {  // no breaks, because end of blocks is never reached
    case RFC1738:
    case RFC2396E: {
      nsString temp(aInString, aInStringLength);

      int32_t i = temp.FindCharInSet(u"<>\"", pos + 1);
      if (i != kNotFound &&
          temp[uint32_t(i--)] ==
              (check == RFC1738 || temp[start - 1] == '<' ? '>' : '"')) {
        end = uint32_t(i);
        return end > pos;
      }
      return false;
    }
    case freetext:
    case abbreviated: {
      uint32_t i = pos + 1;
      bool isEmail = aInString[pos] == (char16_t)'@';
      bool seenOpeningParenthesis = false;  // there is a '(' earlier in the URL
      bool seenOpeningSquareBracket =
          false;  // there is a '[' earlier in the URL
      for (; int32_t(i) < aInStringLength; i++) {
        // These chars mark the end of the URL
        if (aInString[i] == '>' || aInString[i] == '<' || aInString[i] == '"' ||
            aInString[i] == '`' || aInString[i] == '}' || aInString[i] == '{' ||
            (aInString[i] == ')' && !seenOpeningParenthesis) ||
            (aInString[i] == ']' && !seenOpeningSquareBracket) ||
            // Allow IPv6 adresses like http://[1080::8:800:200C:417A]/foo.
            (aInString[i] == '[' && i > 2 &&
             (aInString[i - 1] != '/' || aInString[i - 2] != '/')) ||
            IsSpace(aInString[i])) {
          break;
        }
        // Disallow non-ascii-characters for email.
        // Currently correct, but revisit later after standards changed.
        if (isEmail && (aInString[i] == '(' || aInString[i] == '\'' ||
                        !IsAscii(aInString[i]))) {
          break;
        }
        if (aInString[i] == '(') seenOpeningParenthesis = true;
        if (aInString[i] == '[') seenOpeningSquareBracket = true;
      }
      // These chars are allowed in the middle of the URL, but not at end.
      // Technically they are, but are used in normal text after the URL.
      while (--i > pos && (aInString[i] == '.' || aInString[i] == ',' ||
                           aInString[i] == ';' || aInString[i] == '!' ||
                           aInString[i] == '?' || aInString[i] == '-' ||
                           aInString[i] == ':' || aInString[i] == '\'')) {
        ;
      }
      if (i > pos) {
        end = i;
        return true;
      }
      return false;
    }
    default:
      return false;
  }  // switch
}

void mozTXTToHTMLConv::CalculateURLBoundaries(
    const char16_t* aInString, int32_t aInStringLength, const uint32_t pos,
    const uint32_t whathasbeendone, const modetype check, const uint32_t start,
    const uint32_t end, nsString& txtURL, nsString& desc,
    int32_t& replaceBefore, int32_t& replaceAfter) {
  uint32_t descstart = start;
  switch (check) {
    case RFC1738: {
      descstart = start - 5;
      desc.Append(&aInString[descstart],
                  end - descstart + 2);  // include "<URL:" and ">"
      replaceAfter = end - pos + 1;
    } break;
    case RFC2396E: {
      descstart = start - 1;
      desc.Append(&aInString[descstart],
                  end - descstart + 2);  // include brackets
      replaceAfter = end - pos + 1;
    } break;
    case freetext:
    case abbreviated: {
      descstart = start;
      desc.Append(&aInString[descstart],
                  end - start + 1);  // don't include brackets
      replaceAfter = end - pos;
    } break;
    default:
      break;
  }  // switch

  EscapeStr(desc, false);

  txtURL.Append(&aInString[start], end - start + 1);
  txtURL.StripWhitespace();

  // FIX ME
  nsAutoString temp2;
  ScanTXT(nsDependentSubstring(&aInString[descstart], pos - descstart),
          ~kURLs /*prevents loop*/ & whathasbeendone, temp2);
  replaceBefore = temp2.Length();
}

bool mozTXTToHTMLConv::ShouldLinkify(const nsCString& aURL) {
  if (!mIOService) return false;

  nsAutoCString scheme;
  nsresult rv = mIOService->ExtractScheme(aURL, scheme);
  if (NS_FAILED(rv)) return false;

  if (scheme == "http" || scheme == "https" || scheme == "mailto") {
    return true;
  }

  // Get the handler for this scheme.
  nsCOMPtr<nsIProtocolHandler> handler;
  rv = mIOService->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if (NS_FAILED(rv)) return false;

  // Is it an external protocol handler? If not, linkify it.
  nsCOMPtr<nsIExternalProtocolHandler> externalHandler =
      do_QueryInterface(handler);
  if (!externalHandler) return true;  // handler is built-in, linkify it!

  // If external app exists for the scheme then linkify it.
  bool exists;
  rv = externalHandler->ExternalAppExistsForScheme(scheme, &exists);
  return (NS_SUCCEEDED(rv) && exists);
}

bool mozTXTToHTMLConv::CheckURLAndCreateHTML(const nsString& txtURL,
                                             const nsString& desc,
                                             const modetype mode,
                                             nsString& outputHTML) {
  // Create *uri from txtURL
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  // Lazily initialize mIOService
  if (!mIOService) {
    mIOService = do_GetIOService();

    if (!mIOService) return false;
  }

  // See if the url should be linkified.
  NS_ConvertUTF16toUTF8 utf8URL(txtURL);
  if (!ShouldLinkify(utf8URL)) return false;

  // it would be faster if we could just check to see if there is a protocol
  // handler for the url and return instead of actually trying to create a
  // url...
  rv = mIOService->NewURI(utf8URL, nullptr, nullptr, getter_AddRefs(uri));

  // Real work
  if (NS_SUCCEEDED(rv) && uri) {
    outputHTML.AssignLiteral("<a class=\"moz-txt-link-");
    switch (mode) {
      case RFC1738:
        outputHTML.AppendLiteral("rfc1738");
        break;
      case RFC2396E:
        outputHTML.AppendLiteral("rfc2396E");
        break;
      case freetext:
        outputHTML.AppendLiteral("freetext");
        break;
      case abbreviated:
        outputHTML.AppendLiteral("abbreviated");
        break;
      default:
        break;
    }
    nsAutoString escapedURL(txtURL);
    EscapeStr(escapedURL, true);

    outputHTML.AppendLiteral("\" href=\"");
    outputHTML += escapedURL;
    outputHTML.AppendLiteral("\">");
    outputHTML += desc;
    outputHTML.AppendLiteral("</a>");
    return true;
  }
  return false;
}

NS_IMETHODIMP mozTXTToHTMLConv::FindURLInPlaintext(const char16_t* aInString,
                                                   int32_t aInLength,
                                                   int32_t aPos,
                                                   int32_t* aStartPos,
                                                   int32_t* aEndPos) {
  // call FindURL on the passed in string
  nsAutoString outputHTML;  // we'll ignore the generated output HTML

  *aStartPos = -1;
  *aEndPos = -1;

  FindURL(aInString, aInLength, aPos, kURLs, outputHTML, *aStartPos, *aEndPos);

  return NS_OK;
}

bool mozTXTToHTMLConv::FindURL(const char16_t* aInString, int32_t aInLength,
                               const uint32_t pos,
                               const uint32_t whathasbeendone,
                               nsString& outputHTML, int32_t& replaceBefore,
                               int32_t& replaceAfter) {
  enum statetype { unchecked, invalid, startok, endok, success };
  static const modetype ranking[] = {RFC1738, RFC2396E, freetext, abbreviated};

  statetype state[mozTXTToHTMLConv_lastMode + 1];  // 0(=unknown)..lastMode
  /* I don't like this abuse of enums as index for the array,
     but I don't know a better method */

  // Define, which modes to check
  /* all modes but abbreviated are checked for text[pos] == ':',
     only abbreviated for '.', RFC2396E and abbreviated for '@' */
  for (modetype iState = unknown; iState <= mozTXTToHTMLConv_lastMode;
       iState = modetype(iState + 1)) {
    state[iState] = aInString[pos] == ':' ? unchecked : invalid;
  }
  switch (aInString[pos]) {
    case '@':
      state[RFC2396E] = unchecked;
      [[fallthrough]];
    case '.':
      state[abbreviated] = unchecked;
      break;
    case ':':
      state[abbreviated] = invalid;
      break;
    default:
      break;
  }

  // Test, first successful mode wins, sequence defined by |ranking|
  int32_t iCheck = 0;  // the currently tested modetype
  modetype check = ranking[iCheck];
  for (; iCheck < mozTXTToHTMLConv_numberOfModes && state[check] != success;
       iCheck++)
  /* check state from last run.
     If this is the first, check this one, which isn't = success yet */
  {
    check = ranking[iCheck];

    uint32_t start, end;

    if (state[check] == unchecked) {
      if (FindURLStart(aInString, aInLength, pos, check, start)) {
        state[check] = startok;
      }
    }

    if (state[check] == startok) {
      if (FindURLEnd(aInString, aInLength, pos, check, start, end)) {
        state[check] = endok;
      }
    }

    if (state[check] == endok) {
      nsAutoString txtURL, desc;
      int32_t resultReplaceBefore, resultReplaceAfter;

      CalculateURLBoundaries(aInString, aInLength, pos, whathasbeendone, check,
                             start, end, txtURL, desc, resultReplaceBefore,
                             resultReplaceAfter);

      if (aInString[pos] != ':') {
        nsAutoString temp = txtURL;
        txtURL.SetLength(0);
        CompleteAbbreviatedURL(temp.get(), temp.Length(), pos - start, txtURL);
      }

      if (!txtURL.IsEmpty() &&
          CheckURLAndCreateHTML(txtURL, desc, check, outputHTML)) {
        replaceBefore = resultReplaceBefore;
        replaceAfter = resultReplaceAfter;
        state[check] = success;
      }
    }  // if
  }    // for
  return state[check] == success;
}

static inline bool IsAlpha(const uint32_t aChar) {
  return mozilla::unicode::GetGenCategory(aChar) == nsUGenCategory::kLetter;
}

static inline bool IsDigit(const uint32_t aChar) {
  return mozilla::unicode::GetGenCategory(aChar) == nsUGenCategory::kNumber;
}

bool mozTXTToHTMLConv::ItMatchesDelimited(const char16_t* aInString,
                                          int32_t aInLength,
                                          const char16_t* rep, int32_t aRepLen,
                                          LIMTYPE before, LIMTYPE after) {
  // this little method gets called a LOT. I found we were spending a
  // lot of time just calculating the length of the variable "rep"
  // over and over again every time we called it. So we're now passing
  // an integer in here.
  int32_t textLen = aInLength;

  if (((before == LT_IGNORE && (after == LT_IGNORE || after == LT_DELIMITER)) &&
       textLen < aRepLen) ||
      ((before != LT_IGNORE || (after != LT_IGNORE && after != LT_DELIMITER)) &&
       textLen < aRepLen + 1) ||
      (before != LT_IGNORE && after != LT_IGNORE && after != LT_DELIMITER &&
       textLen < aRepLen + 2)) {
    return false;
  }

  uint32_t text0 = aInString[0];
  if (aInLength > 1 && NS_IS_SURROGATE_PAIR(text0, aInString[1])) {
    text0 = SURROGATE_TO_UCS4(text0, aInString[1]);
  }
  // find length of the char/cluster to be ignored
  int32_t ignoreLen = before == LT_IGNORE ? 0 : 1;
  if (ignoreLen) {
    GraphemeClusterBreakIteratorUtf16 ci(
        Span<const char16_t>(aInString, aInLength));
    ignoreLen = *ci.Next();
  }

  int32_t afterIndex = aRepLen + ignoreLen;
  uint32_t textAfterPos = aInString[afterIndex];
  if (aInLength > afterIndex + 1 &&
      NS_IS_SURROGATE_PAIR(textAfterPos, aInString[afterIndex + 1])) {
    textAfterPos = SURROGATE_TO_UCS4(textAfterPos, aInString[afterIndex + 1]);
  }

  return !((before == LT_ALPHA && !IsAlpha(text0)) ||
           (before == LT_DIGIT && !IsDigit(text0)) ||
           (before == LT_DELIMITER &&
            (IsAlpha(text0) || IsDigit(text0) || text0 == *rep)) ||
           (after == LT_ALPHA && !IsAlpha(textAfterPos)) ||
           (after == LT_DIGIT && !IsDigit(textAfterPos)) ||
           (after == LT_DELIMITER &&
            (IsAlpha(textAfterPos) || IsDigit(textAfterPos) ||
             textAfterPos == *rep)) ||
           !Substring(Substring(aInString, aInString + aInLength), ignoreLen,
                      aRepLen)
                .Equals(Substring(rep, rep + aRepLen),
                        nsCaseInsensitiveStringComparator));
}

uint32_t mozTXTToHTMLConv::NumberOfMatches(const char16_t* aInString,
                                           int32_t aInStringLength,
                                           const char16_t* rep, int32_t aRepLen,
                                           LIMTYPE before, LIMTYPE after) {
  uint32_t result = 0;

  const uint32_t len = mozilla::AssertedCast<uint32_t>(aInStringLength);
  GraphemeClusterBreakIteratorUtf16 ci(Span<const char16_t>(aInString, len));
  for (uint32_t pos = 0; pos < len; pos = *ci.Next()) {
    if (ItMatchesDelimited(aInString + pos, aInStringLength - pos, rep, aRepLen,
                           before, after)) {
      result++;
    }
  }
  return result;
}

// NOTE: the converted html for the phrase is appended to aOutString
// tagHTML and attributeHTML are plain ASCII (literal strings, in fact)
bool mozTXTToHTMLConv::StructPhraseHit(
    const char16_t* aInString, int32_t aInStringLength, bool col0,
    const char16_t* tagTXT, int32_t aTagTXTLen, const char* tagHTML,
    const char* attributeHTML, nsAString& aOutString, uint32_t& openTags) {
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existence of a pair could be verified
     We use the first opening/closing tag, if we can choose */

  const char16_t* newOffset = aInString;
  int32_t newLength = aInStringLength;
  if (!col0)  // skip the first element?
  {
    newOffset = &aInString[1];
    newLength = aInStringLength - 1;
  }

  // opening tag
  if (ItMatchesDelimited(aInString, aInStringLength, tagTXT, aTagTXTLen,
                         (col0 ? LT_IGNORE : LT_DELIMITER),
                         LT_ALPHA)  // is opening tag
      && NumberOfMatches(newOffset, newLength, tagTXT, aTagTXTLen, LT_ALPHA,
                         LT_DELIMITER)  // remaining closing tags
             > openTags) {
    openTags++;
    aOutString.Append('<');
    aOutString.AppendASCII(tagHTML);
    aOutString.Append(char16_t(' '));
    aOutString.AppendASCII(attributeHTML);
    aOutString.AppendLiteral("><span class=\"moz-txt-tag\">");
    aOutString.Append(tagTXT);
    aOutString.AppendLiteral("</span>");
    return true;
  }

  // closing tag
  if (openTags > 0 && ItMatchesDelimited(aInString, aInStringLength, tagTXT,
                                         aTagTXTLen, LT_ALPHA, LT_DELIMITER)) {
    openTags--;
    aOutString.AppendLiteral("<span class=\"moz-txt-tag\">");
    aOutString.Append(tagTXT);
    aOutString.AppendLiteral("</span></");
    aOutString.AppendASCII(tagHTML);
    aOutString.Append(char16_t('>'));
    return true;
  }

  return false;
}

bool mozTXTToHTMLConv::SmilyHit(const char16_t* aInString, int32_t aLength,
                                bool col0, const char* tagTXT,
                                const nsString& imageName, nsString& outputHTML,
                                int32_t& glyphTextLen) {
  if (!aInString || !tagTXT || imageName.IsEmpty()) return false;

  int32_t tagLen = strlen(tagTXT);

  uint32_t delim = (col0 ? 0 : 1) + tagLen;

  if ((col0 || IsSpace(aInString[0])) &&
      (aLength <= int32_t(delim) || IsSpace(aInString[delim]) ||
       (aLength > int32_t(delim + 1) &&
        (aInString[delim] == '.' || aInString[delim] == ',' ||
         aInString[delim] == ';' || aInString[delim] == '8' ||
         aInString[delim] == '>' || aInString[delim] == '!' ||
         aInString[delim] == '?') &&
        IsSpace(aInString[delim + 1]))) &&
      ItMatchesDelimited(aInString, aLength,
                         NS_ConvertASCIItoUTF16(tagTXT).get(), tagLen,
                         col0 ? LT_IGNORE : LT_DELIMITER, LT_IGNORE)
      // Note: tests at different pos for LT_IGNORE and LT_DELIMITER
  ) {
    if (!col0) {
      outputHTML.Truncate();
      outputHTML.Append(char16_t(' '));
    }

    outputHTML.Append(imageName);  // emoji unicode
    glyphTextLen = (col0 ? 0 : 1) + tagLen;
    return true;
  }

  return false;
}

// the glyph is appended to aOutputString instead of the original string...
bool mozTXTToHTMLConv::GlyphHit(const char16_t* aInString, int32_t aInLength,
                                bool col0, nsAString& aOutputString,
                                int32_t& glyphTextLen) {
  char16_t text0 = aInString[0];
  char16_t text1 = aInString[1];
  char16_t firstChar = (col0 ? text0 : text1);

  // temporary variable used to store the glyph html text
  nsAutoString outputHTML;
  bool bTestSmilie;
  bool bArg = false;
  int i;

  // refactor some of this mess to avoid code duplication and speed execution a
  // bit there are two cases that need to be tried one after another. To avoid a
  // lot of duplicate code, rolling into a loop

  i = 0;
  while (i < 2) {
    bTestSmilie = false;
    if (!i && (firstChar == ':' || firstChar == ';' || firstChar == '=' ||
               firstChar == '>' || firstChar == '8' || firstChar == 'O')) {
      // first test passed

      bTestSmilie = true;
      bArg = col0;
    }
    if (i && col0 &&
        (text1 == ':' || text1 == ';' || text1 == '=' || text1 == '>' ||
         text1 == '8' || text1 == 'O')) {
      // second test passed

      bTestSmilie = true;
      bArg = false;
    }
    if (bTestSmilie && (SmilyHit(aInString, aInLength, bArg, ":-)",
                                 u"😄"_ns,  // smile, U+1F604
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":)",
                                 u"😄"_ns,  // smile, U+1F604
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-D",
                                 u"😂"_ns,  // laughing, U+1F602
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-(",
                                 u"🙁"_ns,  // frown, U+1F641
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":(",
                                 u"🙁"_ns,  // frown, U+1F641
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-[",
                                 u"😅"_ns,  // embarassed, U+1F605
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ";-)",
                                 u"😉"_ns,  // wink, U+1F609
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, col0, ";)",
                                 u"😉"_ns,  // wink, U+1F609
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-\\",
                                 u"😕"_ns,  // undecided, U+1F615
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-P",
                                 u"😛"_ns,  // tongue, U+1F61B
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ";-P",
                                 u"😜"_ns,  // winking face with tongue, U+1F61C
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, "=-O",
                                 u"😮"_ns,  // surprise, U+1F62E
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-*",
                                 u"😘"_ns,  // kiss, U+1F618
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ">:o",
                                 u"😄"_ns,  // yell, U+1F620
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ">:-o",
                                 u"😠"_ns,  // yell, U+1F620
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, "8-)",
                                 u"😎"_ns,  // cool, U+1F60E
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-$",
                                 u"🤑"_ns,  // money, U+1F911
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-!",
                                 u"😬"_ns,  // foot, U+1F62C
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, "O:-)",
                                 u"😇"_ns,  // innocent, U+1F607
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":'(",
                                 u"😭"_ns,  // cry, U+1F62D
                                 outputHTML, glyphTextLen) ||

                        SmilyHit(aInString, aInLength, bArg, ":-X",
                                 u"😷"_ns,  // sealed, U+1F637
                                 outputHTML, glyphTextLen))) {
      aOutputString.Append(outputHTML);
      return true;
    }
    i++;
  }
  if (text0 == '\f') {
    aOutputString.AppendLiteral("<span class='moz-txt-formfeed'></span>");
    glyphTextLen = 1;
    return true;
  }
  if (text0 == '+' || text1 == '+') {
    if (ItMatchesDelimited(aInString, aInLength, u" +/-", 4, LT_IGNORE,
                           LT_IGNORE)) {
      aOutputString.AppendLiteral(" &plusmn;");
      glyphTextLen = 4;
      return true;
    }
    if (col0 && ItMatchesDelimited(aInString, aInLength, u"+/-", 3, LT_IGNORE,
                                   LT_IGNORE)) {
      aOutputString.AppendLiteral("&plusmn;");
      glyphTextLen = 3;
      return true;
    }
  }

  // x^2  =>  x<sup>2</sup>,   also handle powers x^-2,  x^0.5
  // implement regular expression /[\dA-Za-z\)\]}]\^-?\d+(\.\d+)*[^\dA-Za-z]/
  if (text1 == '^' &&
      (IsAsciiDigit(text0) || IsAsciiAlpha(text0) || text0 == ')' ||
       text0 == ']' || text0 == '}') &&
      ((2 < aInLength && IsAsciiDigit(aInString[2])) ||
       (3 < aInLength && aInString[2] == '-' && IsAsciiDigit(aInString[3])))) {
    // Find first non-digit
    int32_t delimPos = 3;  // skip "^" and first digit (or '-')
    for (; delimPos < aInLength &&
           (IsAsciiDigit(aInString[delimPos]) ||
            (aInString[delimPos] == '.' && delimPos + 1 < aInLength &&
             IsAsciiDigit(aInString[delimPos + 1])));
         delimPos++) {
      ;
    }

    if (delimPos < aInLength && IsAsciiAlpha(aInString[delimPos])) {
      return false;
    }

    outputHTML.Truncate();
    outputHTML += text0;
    outputHTML.AppendLiteral(
        "<sup class=\"moz-txt-sup\">"
        "<span style=\"display:inline-block;width:0;height:0;overflow:hidden\">"
        "^</span>");

    aOutputString.Append(outputHTML);
    aOutputString.Append(&aInString[2], delimPos - 2);
    aOutputString.AppendLiteral("</sup>");

    glyphTextLen = delimPos /* - 1 + 1 */;
    return true;
  }
  /*
   The following strings are not substituted:
   |TXT   |HTML     |Reason
   +------+---------+----------
    ->     &larr;    Bug #454
    =>     &lArr;    dito
    <-     &rarr;    dito
    <=     &rArr;    dito
    (tm)   &trade;   dito
    1/4    &frac14;  is triggered by 1/4 Part 1, 2/4 Part 2, ...
    3/4    &frac34;  dito
    1/2    &frac12;  similar
  */
  return false;
}

/***************************************************************************
  Library-internal Interface
****************************************************************************/

NS_IMPL_ISUPPORTS(mozTXTToHTMLConv, mozITXTToHTMLConv, nsIStreamConverter,
                  nsIStreamListener, nsIRequestObserver)

int32_t mozTXTToHTMLConv::CiteLevelTXT(const char16_t* line,
                                       uint32_t& logLineStart) {
  int32_t result = 0;
  int32_t lineLength = NS_strlen(line);

  bool moreCites = true;
  while (moreCites) {
    /* E.g. the following lines count as quote:

       > text
       //#ifdef QUOTE_RECOGNITION_AGGRESSIVE
       >text
       //#ifdef QUOTE_RECOGNITION_AGGRESSIVE
           > text
       ] text
       USER> text
       USER] text
       //#endif

       logLineStart is the position of "t" in this example
    */
    uint32_t i = logLineStart;

#ifdef QUOTE_RECOGNITION_AGGRESSIVE
    for (; int32_t(i) < lineLength && IsSpace(line[i]); i++)
      ;
    for (; int32_t(i) < lineLength && IsAsciiAlpha(line[i]) &&
           nsCRT::IsUpper(line[i]);
         i++)
      ;
    if (int32_t(i) < lineLength && (line[i] == '>' || line[i] == ']'))
#else
    if (int32_t(i) < lineLength && line[i] == '>')
#endif
    {
      i++;
      if (int32_t(i) < lineLength && line[i] == ' ') i++;
      // sendmail/mbox
      // Placed here for performance increase
      const char16_t* indexString = &line[logLineStart];
      // here, |logLineStart < lineLength| is always true
      uint32_t minlength = std::min(uint32_t(6), NS_strlen(indexString));
      if (Substring(indexString, indexString + minlength)
              .Equals(Substring(u">From "_ns, 0, minlength),
                      nsCaseInsensitiveStringComparator)) {
        // XXX RFC2646
        moreCites = false;
      } else {
        result++;
        logLineStart = i;
      }
    } else {
      moreCites = false;
    }
  }

  return result;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanTXT(const nsAString& aInString, uint32_t whattodo,
                          nsAString& aOutString) {
  if (aInString.Length() == 0) {
    aOutString.Truncate();
    return NS_OK;
  }

  if (!aOutString.SetCapacity(uint32_t(aInString.Length() * growthRate),
                              mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  bool doURLs = 0 != (whattodo & kURLs);
  bool doGlyphSubstitution = 0 != (whattodo & kGlyphSubstitution);
  bool doStructPhrase = 0 != (whattodo & kStructPhrase);

  uint32_t structPhrase_strong = 0;  // Number of currently open tags
  uint32_t structPhrase_underline = 0;
  uint32_t structPhrase_italic = 0;
  uint32_t structPhrase_code = 0;

  uint32_t endOfLastURLOutput = 0;

  nsAutoString outputHTML;  // moved here for performance increase

  const char16_t* rawInputString = aInString.BeginReading();
  uint32_t inLength = aInString.Length();

  const Span<const char16_t> inString(aInString);
  GraphemeClusterBreakIteratorUtf16 ci(inString);
  uint32_t i = 0;
  while (i < inLength) {
    if (doGlyphSubstitution) {
      int32_t glyphTextLen;
      if (GlyphHit(&rawInputString[i], inLength - i, i == 0, aOutString,
                   glyphTextLen)) {
        i = *ci.Seek(i + glyphTextLen - 1);
        continue;
      }
    }

    if (doStructPhrase) {
      const char16_t* newOffset = rawInputString;
      int32_t newLength = aInString.Length();
      if (i > 0)  // skip the first element?
      {
        GraphemeClusterBreakReverseIteratorUtf16 ri(
            Span<const char16_t>(rawInputString, i));
        Maybe<uint32_t> nextPos = ri.Next();
        newOffset += *nextPos;
        newLength -= *nextPos;
      }

      switch (aInString[i])  // Performance increase
      {
        case '*':
          if (StructPhraseHit(newOffset, newLength, i == 0, u"*", 1, "b",
                              "class=\"moz-txt-star\"", aOutString,
                              structPhrase_strong)) {
            i = *ci.Next();
            continue;
          }
          break;
        case '/':
          if (StructPhraseHit(newOffset, newLength, i == 0, u"/", 1, "i",
                              "class=\"moz-txt-slash\"", aOutString,
                              structPhrase_italic)) {
            i = *ci.Next();
            continue;
          }
          break;
        case '_':
          if (StructPhraseHit(newOffset, newLength, i == 0, u"_", 1,
                              "span" /* <u> is deprecated */,
                              "class=\"moz-txt-underscore\"", aOutString,
                              structPhrase_underline)) {
            i = *ci.Next();
            continue;
          }
          break;
        case '|':
          if (StructPhraseHit(newOffset, newLength, i == 0, u"|", 1, "code",
                              "class=\"moz-txt-verticalline\"", aOutString,
                              structPhrase_code)) {
            i = *ci.Next();
            continue;
          }
          break;
      }
    }

    if (doURLs) {
      switch (aInString[i]) {
        case ':':
        case '@':
        case '.':
          if ((i == 0 || ((i > 0) && aInString[i - 1] != ' ')) &&
              ((i == aInString.Length() - 1) ||
               (aInString[i + 1] != ' ')))  // Performance increase
          {
            int32_t replaceBefore;
            int32_t replaceAfter;
            if (FindURL(rawInputString, aInString.Length(), i, whattodo,
                        outputHTML, replaceBefore, replaceAfter) &&
                structPhrase_strong + structPhrase_italic +
                        structPhrase_underline + structPhrase_code ==
                    0
                /* workaround for bug #19445 */) {
              // Don't cut into previously inserted HTML (bug 1509493)
              if (aOutString.Length() - replaceBefore < endOfLastURLOutput) {
                break;
              }
              aOutString.Cut(aOutString.Length() - replaceBefore,
                             replaceBefore);
              aOutString += outputHTML;
              endOfLastURLOutput = aOutString.Length();
              i = *ci.Seek(i + replaceAfter);
              continue;
            }
          }
          break;
      }  // switch
    }

    switch (aInString[i]) {
      // Special symbols
      case '<':
      case '>':
      case '&':
        EscapeChar(aInString[i], aOutString, false);
        i = *ci.Next();
        break;
      // Normal characters
      default: {
        const uint32_t oldIdx = i;
        i = *ci.Next();
        aOutString.Append(inString.FromTo(oldIdx, i));
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanHTML(const nsAString& input, uint32_t whattodo,
                           nsAString& aOutString) {
  const nsPromiseFlatString& aInString = PromiseFlatString(input);
  if (!aOutString.SetCapacity(uint32_t(aInString.Length() * growthRate),
                              mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // some common variables we were recalculating
  // every time inside the for loop...
  int32_t lengthOfInString = aInString.Length();
  const char16_t* uniBuffer = aInString.get();

#ifdef DEBUG_BenB_Perf
  PRTime parsing_start = PR_IntervalNow();
#endif

  // Look for simple entities not included in a tags and scan them.
  // Skip all tags ("<[...]>") and content in an a link tag ("<a [...]</a>"),
  // comment tag ("<!--[...]-->"), style tag, script tag or head tag.
  // Unescape the rest (text between tags) and pass it to ScanTXT.
  nsAutoCString canFollow(" \f\n\r\t>");
  for (int32_t i = 0; i < lengthOfInString;) {
    if (aInString[i] == '<')  // html tag
    {
      int32_t start = i;
      if (i + 2 < lengthOfInString && nsCRT::ToLower(aInString[i + 1]) == 'a' &&
          canFollow.FindChar(aInString[i + 2]) != kNotFound)
      // if a tag, skip until </a>.
      // Make sure there's a white-space character after, not to match "abbr".
      {
        i = aInString.Find("</a>", true, i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i += 4;
        }
      } else if (Substring(aInString, i + 1, 3).LowerCaseEqualsASCII("!--"))
      // if out-commended code, skip until -->
      {
        i = aInString.Find("-->", false, i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i += 3;
        }
      } else if (i + 6 < lengthOfInString &&
                 Substring(aInString, i + 1, 5).LowerCaseEqualsASCII("style") &&
                 canFollow.FindChar(aInString[i + 6]) != kNotFound)
      // if style tag, skip until </style>
      {
        i = aInString.Find("</style>", true, i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i += 8;
        }
      } else if (i + 7 < lengthOfInString &&
                 Substring(aInString, i + 1, 6)
                     .LowerCaseEqualsASCII("script") &&
                 canFollow.FindChar(aInString[i + 7]) != kNotFound)
      // if script tag, skip until </script>
      {
        i = aInString.Find("</script>", true, i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i += 9;
        }
      } else if (i + 5 < lengthOfInString &&
                 Substring(aInString, i + 1, 4).LowerCaseEqualsASCII("head") &&
                 canFollow.FindChar(aInString[i + 5]) != kNotFound)
      // if head tag, skip until </head>
      // Make sure not to match <header>.
      {
        i = aInString.Find("</head>", true, i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i += 7;
        }
      } else  // just skip tag (attributes etc.)
      {
        i = aInString.FindChar('>', i);
        if (i == kNotFound) {
          i = lengthOfInString;
        } else {
          i++;
        }
      }
      aOutString.Append(&uniBuffer[start], i - start);
    } else {
      uint32_t start = uint32_t(i);
      i = aInString.FindChar('<', i);
      if (i == kNotFound) i = lengthOfInString;

      nsString tempString;
      tempString.SetCapacity(uint32_t((uint32_t(i) - start) * growthRate));
      UnescapeStr(uniBuffer, start, uint32_t(i) - start, tempString);
      ScanTXT(tempString, whattodo, aOutString);
    }
  }

#ifdef DEBUG_BenB_Perf
  printf("ScanHTML time:    %d ms\n",
         PR_IntervalToMilliseconds(PR_IntervalNow() - parsing_start));
#endif
  return NS_OK;
}

/****************************************************************************
  XPCOM Interface
*****************************************************************************/

NS_IMETHODIMP
mozTXTToHTMLConv::Convert(nsIInputStream* aFromStream, const char* aFromType,
                          const char* aToType, nsISupports* aCtxt,
                          nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::AsyncConvertData(const char* aFromType, const char* aToType,
                                   nsIStreamListener* aListener,
                                   nsISupports* aCtxt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::GetConvertedType(const nsACString& aFromType,
                                   nsIChannel* aChannel, nsACString& aToType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnDataAvailable(nsIRequest* request, nsIInputStream* inStr,
                                  uint64_t sourceOffset, uint32_t count) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStartRequest(nsIRequest* request) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStopRequest(nsIRequest* request, nsresult aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::CiteLevelTXT(const char16_t* line, uint32_t* logLineStart,
                               uint32_t* _retval) {
  if (!logLineStart || !_retval || !line) return NS_ERROR_NULL_POINTER;
  *_retval = CiteLevelTXT(line, *logLineStart);
  return NS_OK;
}

nsresult MOZ_NewTXTToHTMLConv(mozTXTToHTMLConv** aConv) {
  MOZ_ASSERT(aConv != nullptr, "null ptr");
  if (!aConv) return NS_ERROR_NULL_POINTER;

  RefPtr<mozTXTToHTMLConv> conv = new mozTXTToHTMLConv();
  conv.forget(aConv);
  //    return (*aConv)->Init();
  return NS_OK;
}
