/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozTXTToHTMLConv.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsIExternalProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIURI.h"

#include <algorithm>

#ifdef DEBUG_BenB_Perf
#include "prtime.h"
#include "prinrval.h"
#endif

const double growthRate = 1.2;

// Bug 183111, editor now replaces multiple spaces with leading
// 0xA0's and a single ending space, so need to treat 0xA0's as spaces.
// 0xA0 is the Latin1/Unicode character for "non-breaking space (nbsp)"
// Also recognize the Japanese ideographic space 0x3000 as a space.
static inline bool IsSpace(const char16_t aChar)
{
  return (nsCRT::IsAsciiSpace(aChar) || aChar == 0xA0 || aChar == 0x3000);
}

// Escape Char will take ch, escape it and append the result to
// aStringToAppendTo
void
mozTXTToHTMLConv::EscapeChar(const char16_t ch, nsString& aStringToAppendTo,
                             bool inAttribute)
{
    switch (ch)
    {
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
      if (inAttribute)
      {
        aStringToAppendTo.AppendLiteral("&quot;");
        break;
      }
      // else fall through
      MOZ_FALLTHROUGH;
    default:
      aStringToAppendTo += ch;
    }
}

// EscapeStr takes the passed in string and
// escapes it IN PLACE.
void
mozTXTToHTMLConv::EscapeStr(nsString& aInString, bool inAttribute)
{
  // the replace substring routines
  // don't seem to work if you have a character
  // in the in string that is also in the replacement
  // string! =(
  //aInString.ReplaceSubstring("&", "&amp;");
  //aInString.ReplaceSubstring("<", "&lt;");
  //aInString.ReplaceSubstring(">", "&gt;");
  for (uint32_t i = 0; i < aInString.Length();)
  {
    switch (aInString[i])
    {
    case '<':
      aInString.Cut(i, 1);
      aInString.InsertLiteral(u"&lt;", i);
      i += 4; // skip past the integers we just added
      break;
    case '>':
      aInString.Cut(i, 1);
      aInString.InsertLiteral(u"&gt;", i);
      i += 4; // skip past the integers we just added
      break;
    case '&':
      aInString.Cut(i, 1);
      aInString.InsertLiteral(u"&amp;", i);
      i += 5; // skip past the integers we just added
      break;
    case '"':
      if (inAttribute)
      {
        aInString.Cut(i, 1);
        aInString.InsertLiteral(u"&quot;", i);
        i += 6;
        break;
      }
      // else fall through
      MOZ_FALLTHROUGH;
    default:
      i++;
    }
  }
}

void
mozTXTToHTMLConv::UnescapeStr(const char16_t * aInString, int32_t aStartPos, int32_t aLength, nsString& aOutString)
{
  const char16_t * subString = nullptr;
  for (uint32_t i = aStartPos; int32_t(i) - aStartPos < aLength;)
  {
    int32_t remainingChars = i - aStartPos;
    if (aInString[i] == '&')
    {
      subString = &aInString[i];
      if (!NS_strncmp(subString, u"&lt;", std::min(4, aLength - remainingChars)))
      {
        aOutString.Append(char16_t('<'));
        i += 4;
      }
      else if (!NS_strncmp(subString, u"&gt;", std::min(4, aLength - remainingChars)))
      {
        aOutString.Append(char16_t('>'));
        i += 4;
      }
      else if (!NS_strncmp(subString, u"&amp;", std::min(5, aLength - remainingChars)))
      {
        aOutString.Append(char16_t('&'));
        i += 5;
      }
      else if (!NS_strncmp(subString, u"&quot;", std::min(6, aLength - remainingChars)))
      {
        aOutString.Append(char16_t('"'));
        i += 6;
      }
      else
      {
        aOutString += aInString[i];
        i++;
      }
    }
    else
    {
      aOutString += aInString[i];
      i++;
    }
  }
}

void
mozTXTToHTMLConv::CompleteAbbreviatedURL(const char16_t * aInString, int32_t aInLength,
                                         const uint32_t pos, nsString& aOutString)
{
  NS_ASSERTION(int32_t(pos) < aInLength, "bad args to CompleteAbbreviatedURL, see bug #190851");
  if (int32_t(pos) >= aInLength)
    return;

  if (aInString[pos] == '@')
  {
    // only pre-pend a mailto url if the string contains a .domain in it..
    //i.e. we want to linkify johndoe@foo.com but not "let's meet @8pm"
    nsDependentString inString(aInString, aInLength);
    if (inString.FindChar('.', pos) != kNotFound) // if we have a '.' after the @ sign....
    {
      aOutString.AssignLiteral("mailto:");
      aOutString += aInString;
    }
  }
  else if (aInString[pos] == '.')
  {
    if (ItMatchesDelimited(aInString, aInLength,
                           u"www.", 4, LT_IGNORE, LT_IGNORE))
    {
      aOutString.AssignLiteral("http://");
      aOutString += aInString;
    }
    else if (ItMatchesDelimited(aInString,aInLength, u"ftp.", 4, LT_IGNORE, LT_IGNORE))
    {
      aOutString.AssignLiteral("ftp://");
      aOutString += aInString;
    }
  }
}

bool
mozTXTToHTMLConv::FindURLStart(const char16_t * aInString, int32_t aInLength,
                               const uint32_t pos, const modetype check,
                               uint32_t& start)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  {
    if (!NS_strncmp(&aInString[std::max(int32_t(pos - 4), 0)], u"<URL:", 5))
    {
      start = pos + 1;
      return true;
    }
    else
      return false;
  }
  case RFC2396E:
  {
    nsString temp(aInString, aInLength);
    int32_t i = pos <= 0 ? kNotFound : temp.RFindCharInSet(u"<>\"", pos - 1);
    if (i != kNotFound && (temp[uint32_t(i)] == '<' ||
                           temp[uint32_t(i)] == '"'))
    {
      start = uint32_t(++i);
      return start < pos;
    }
    else
      return false;
  }
  case freetext:
  {
    int32_t i = pos - 1;
    for (; i >= 0 && (
         nsCRT::IsAsciiAlpha(aInString[uint32_t(i)]) ||
         nsCRT::IsAsciiDigit(aInString[uint32_t(i)]) ||
         aInString[uint32_t(i)] == '+' ||
         aInString[uint32_t(i)] == '-' ||
         aInString[uint32_t(i)] == '.'
         ); i--)
      ;
    if (++i >= 0 && uint32_t(i) < pos && nsCRT::IsAsciiAlpha(aInString[uint32_t(i)]))
    {
      start = uint32_t(i);
      return true;
    }
    else
      return false;
  }
  case abbreviated:
  {
    int32_t i = pos - 1;
    // This disallows non-ascii-characters for email.
    // Currently correct, but revisit later after standards changed.
    bool isEmail = aInString[pos] == (char16_t)'@';
    // These chars mark the start of the URL
    for (; i >= 0
             && aInString[uint32_t(i)] != '>' && aInString[uint32_t(i)] != '<'
             && aInString[uint32_t(i)] != '"' && aInString[uint32_t(i)] != '\''
             && aInString[uint32_t(i)] != '`' && aInString[uint32_t(i)] != ','
             && aInString[uint32_t(i)] != '{' && aInString[uint32_t(i)] != '['
             && aInString[uint32_t(i)] != '(' && aInString[uint32_t(i)] != '|'
             && aInString[uint32_t(i)] != '\\'
             && !IsSpace(aInString[uint32_t(i)])
             && (!isEmail || nsCRT::IsAscii(aInString[uint32_t(i)]))
         ; i--)
      ;
    if
      (
        ++i >= 0 && uint32_t(i) < pos
          &&
          (
            nsCRT::IsAsciiAlpha(aInString[uint32_t(i)]) ||
            nsCRT::IsAsciiDigit(aInString[uint32_t(i)])
          )
      )
    {
      start = uint32_t(i);
      return true;
    }
    else
      return false;
  }
  default:
    return false;
  } //switch
}

bool
mozTXTToHTMLConv::FindURLEnd(const char16_t * aInString, int32_t aInStringLength, const uint32_t pos,
           const modetype check, const uint32_t start, uint32_t& end)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  case RFC2396E:
  {
    nsString temp(aInString, aInStringLength);

    int32_t i = temp.FindCharInSet(u"<>\"", pos + 1);
    if (i != kNotFound && temp[uint32_t(i--)] ==
        (check == RFC1738 || temp[start - 1] == '<' ? '>' : '"'))
    {
      end = uint32_t(i);
      return end > pos;
    }
    return false;
  }
  case freetext:
  case abbreviated:
  {
    uint32_t i = pos + 1;
    bool isEmail = aInString[pos] == (char16_t)'@';
    bool seenOpeningParenthesis = false; // there is a '(' earlier in the URL
    bool seenOpeningSquareBracket = false; // there is a '[' earlier in the URL
    for (; int32_t(i) < aInStringLength; i++)
    {
      // These chars mark the end of the URL
      if (aInString[i] == '>' || aInString[i] == '<' ||
          aInString[i] == '"' || aInString[i] == '`' ||
          aInString[i] == '}' || aInString[i] == '{' ||
          (aInString[i] == ')' && !seenOpeningParenthesis) ||
          (aInString[i] == ']' && !seenOpeningSquareBracket) ||
          // Allow IPv6 adresses like http://[1080::8:800:200C:417A]/foo.
          (aInString[i] == '[' && i > 2 &&
           (aInString[i - 1] != '/' || aInString[i - 2] != '/')) ||
          IsSpace(aInString[i]))
          break;
      // Disallow non-ascii-characters for email.
      // Currently correct, but revisit later after standards changed.
      if (isEmail && (
            aInString[i] == '(' || aInString[i] == '\'' ||
            !nsCRT::IsAscii(aInString[i])))
          break;
      if (aInString[i] == '(')
        seenOpeningParenthesis = true;
      if (aInString[i] == '[')
        seenOpeningSquareBracket = true;
    }
    // These chars are allowed in the middle of the URL, but not at end.
    // Technically they are, but are used in normal text after the URL.
    while (--i > pos && (
             aInString[i] == '.' || aInString[i] == ',' || aInString[i] == ';' ||
             aInString[i] == '!' || aInString[i] == '?' || aInString[i] == '-' ||
             aInString[i] == ':' || aInString[i] == '\''
             ))
        ;
    if (i > pos)
    {
      end = i;
      return true;
    }
    return false;
  }
  default:
    return false;
  } //switch
}

void
mozTXTToHTMLConv::CalculateURLBoundaries(const char16_t * aInString, int32_t aInStringLength,
     const uint32_t pos, const uint32_t whathasbeendone,
     const modetype check, const uint32_t start, const uint32_t end,
     nsString& txtURL, nsString& desc,
     int32_t& replaceBefore, int32_t& replaceAfter)
{
  uint32_t descstart = start;
  switch(check)
  {
  case RFC1738:
  {
    descstart = start - 5;
    desc.Append(&aInString[descstart], end - descstart + 2);  // include "<URL:" and ">"
    replaceAfter = end - pos + 1;
  } break;
  case RFC2396E:
  {
    descstart = start - 1;
    desc.Append(&aInString[descstart], end - descstart + 2); // include brackets
    replaceAfter = end - pos + 1;
  } break;
  case freetext:
  case abbreviated:
  {
    descstart = start;
    desc.Append(&aInString[descstart], end - start + 1); // don't include brackets
    replaceAfter = end - pos;
  } break;
  default: break;
  } //switch

  EscapeStr(desc, false);

  txtURL.Append(&aInString[start], end - start + 1);
  txtURL.StripWhitespace();

  // FIX ME
  nsAutoString temp2;
  ScanTXT(&aInString[descstart], pos - descstart, ~kURLs /*prevents loop*/ & whathasbeendone, temp2);
  replaceBefore = temp2.Length();
}

bool mozTXTToHTMLConv::ShouldLinkify(const nsCString& aURL)
{
  if (!mIOService)
    return false;

  nsAutoCString scheme;
  nsresult rv = mIOService->ExtractScheme(aURL, scheme);
  if(NS_FAILED(rv))
    return false;

  // Get the handler for this scheme.
  nsCOMPtr<nsIProtocolHandler> handler;
  rv = mIOService->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if(NS_FAILED(rv))
    return false;

  // Is it an external protocol handler? If not, linkify it.
  nsCOMPtr<nsIExternalProtocolHandler> externalHandler = do_QueryInterface(handler);
  if (!externalHandler)
   return true; // handler is built-in, linkify it!

  // If external app exists for the scheme then linkify it.
  bool exists;
  rv = externalHandler->ExternalAppExistsForScheme(scheme, &exists);
  return(NS_SUCCEEDED(rv) && exists);
}

bool
mozTXTToHTMLConv::CheckURLAndCreateHTML(
     const nsString& txtURL, const nsString& desc, const modetype mode,
     nsString& outputHTML)
{
  // Create *uri from txtURL
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  // Lazily initialize mIOService
  if (!mIOService)
  {
    mIOService = do_GetIOService();

    if (!mIOService)
      return false;
  }

  // See if the url should be linkified.
  NS_ConvertUTF16toUTF8 utf8URL(txtURL);
  if (!ShouldLinkify(utf8URL))
    return false;

  // it would be faster if we could just check to see if there is a protocol
  // handler for the url and return instead of actually trying to create a url...
  rv = mIOService->NewURI(utf8URL, nullptr, nullptr, getter_AddRefs(uri));

  // Real work
  if (NS_SUCCEEDED(rv) && uri)
  {
    outputHTML.AssignLiteral("<a class=\"moz-txt-link-");
    switch(mode)
    {
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
    default: break;
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
  else
    return false;
}

NS_IMETHODIMP mozTXTToHTMLConv::FindURLInPlaintext(const char16_t * aInString, int32_t aInLength, int32_t aPos, int32_t * aStartPos, int32_t * aEndPos)
{
  // call FindURL on the passed in string
  nsAutoString outputHTML; // we'll ignore the generated output HTML

  *aStartPos = -1;
  *aEndPos = -1;

  FindURL(aInString, aInLength, aPos, kURLs, outputHTML, *aStartPos, *aEndPos);

  return NS_OK;
}

bool
mozTXTToHTMLConv::FindURL(const char16_t * aInString, int32_t aInLength, const uint32_t pos,
     const uint32_t whathasbeendone,
     nsString& outputHTML, int32_t& replaceBefore, int32_t& replaceAfter)
{
  enum statetype {unchecked, invalid, startok, endok, success};
  static const modetype ranking[] = {RFC1738, RFC2396E, freetext, abbreviated};

  statetype state[mozTXTToHTMLConv_lastMode + 1]; // 0(=unknown)..lastMode
  /* I don't like this abuse of enums as index for the array,
     but I don't know a better method */

  // Define, which modes to check
  /* all modes but abbreviated are checked for text[pos] == ':',
     only abbreviated for '.', RFC2396E and abbreviated for '@' */
  for (modetype iState = unknown; iState <= mozTXTToHTMLConv_lastMode;
       iState = modetype(iState + 1))
    state[iState] = aInString[pos] == ':' ? unchecked : invalid;
  switch (aInString[pos])
  {
  case '@':
    state[RFC2396E] = unchecked;
    MOZ_FALLTHROUGH;
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

    if (state[check] == unchecked)
      if (FindURLStart(aInString, aInLength, pos, check, start))
        state[check] = startok;

    if (state[check] == startok)
      if (FindURLEnd(aInString, aInLength, pos, check, start, end))
        state[check] = endok;

    if (state[check] == endok)
    {
      nsAutoString txtURL, desc;
      int32_t resultReplaceBefore, resultReplaceAfter;

      CalculateURLBoundaries(aInString, aInLength, pos, whathasbeendone, check, start, end,
                             txtURL, desc,
                             resultReplaceBefore, resultReplaceAfter);

      if (aInString[pos] != ':')
      {
        nsAutoString temp = txtURL;
        txtURL.SetLength(0);
        CompleteAbbreviatedURL(temp.get(),temp.Length(), pos - start, txtURL);
      }

      if (!txtURL.IsEmpty() && CheckURLAndCreateHTML(txtURL, desc, check,
                                                     outputHTML))
      {
        replaceBefore = resultReplaceBefore;
        replaceAfter = resultReplaceAfter;
        state[check] = success;
      }
    } // if
  } // for
  return state[check] == success;
}

bool
mozTXTToHTMLConv::ItMatchesDelimited(const char16_t * aInString,
    int32_t aInLength, const char16_t* rep, int32_t aRepLen,
    LIMTYPE before, LIMTYPE after)
{

  // this little method gets called a LOT. I found we were spending a
  // lot of time just calculating the length of the variable "rep"
  // over and over again every time we called it. So we're now passing
  // an integer in here.
  int32_t textLen = aInLength;

  if
    (
      ((before == LT_IGNORE && (after == LT_IGNORE || after == LT_DELIMITER))
        && textLen < aRepLen) ||
      ((before != LT_IGNORE || (after != LT_IGNORE && after != LT_DELIMITER))
        && textLen < aRepLen + 1) ||
      (before != LT_IGNORE && after != LT_IGNORE && after != LT_DELIMITER
        && textLen < aRepLen + 2)
    )
    return false;

  char16_t text0 = aInString[0];
  char16_t textAfterPos = aInString[aRepLen + (before == LT_IGNORE ? 0 : 1)];

  if
    (
      (before == LT_ALPHA
        && !nsCRT::IsAsciiAlpha(text0)) ||
      (before == LT_DIGIT
        && !nsCRT::IsAsciiDigit(text0)) ||
      (before == LT_DELIMITER
        &&
        (
          nsCRT::IsAsciiAlpha(text0) ||
          nsCRT::IsAsciiDigit(text0) ||
          text0 == *rep
        )) ||
      (after == LT_ALPHA
        && !nsCRT::IsAsciiAlpha(textAfterPos)) ||
      (after == LT_DIGIT
        && !nsCRT::IsAsciiDigit(textAfterPos)) ||
      (after == LT_DELIMITER
        &&
        (
          nsCRT::IsAsciiAlpha(textAfterPos) ||
          nsCRT::IsAsciiDigit(textAfterPos) ||
          textAfterPos == *rep
        )) ||
        !Substring(Substring(aInString, aInString+aInLength),
                   (before == LT_IGNORE ? 0 : 1),
                   aRepLen).Equals(Substring(rep, rep+aRepLen),
                                   nsCaseInsensitiveStringComparator())
    )
    return false;

  return true;
}

uint32_t
mozTXTToHTMLConv::NumberOfMatches(const char16_t * aInString, int32_t aInStringLength,
     const char16_t* rep, int32_t aRepLen, LIMTYPE before, LIMTYPE after)
{
  uint32_t result = 0;

  for (int32_t i = 0; i < aInStringLength; i++)
  {
    const char16_t * indexIntoString = &aInString[i];
    if (ItMatchesDelimited(indexIntoString, aInStringLength - i, rep, aRepLen, before, after))
      result++;
  }
  return result;
}


// NOTE: the converted html for the phrase is appended to aOutString
// tagHTML and attributeHTML are plain ASCII (literal strings, in fact)
bool
mozTXTToHTMLConv::StructPhraseHit(const char16_t * aInString, int32_t aInStringLength, bool col0,
     const char16_t* tagTXT, int32_t aTagTXTLen,
     const char* tagHTML, const char* attributeHTML,
     nsString& aOutString, uint32_t& openTags)
{
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existence of a pair could be verified
     We use the first opening/closing tag, if we can choose */

  const char16_t * newOffset = aInString;
  int32_t newLength = aInStringLength;
  if (!col0) // skip the first element?
  {
    newOffset = &aInString[1];
    newLength = aInStringLength - 1;
  }

  // opening tag
  if
    (
      ItMatchesDelimited(aInString, aInStringLength, tagTXT, aTagTXTLen,
           (col0 ? LT_IGNORE : LT_DELIMITER), LT_ALPHA) // is opening tag
        && NumberOfMatches(newOffset, newLength, tagTXT, aTagTXTLen,
              LT_ALPHA, LT_DELIMITER)  // remaining closing tags
              > openTags
    )
  {
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
  else if (openTags > 0
       && ItMatchesDelimited(aInString, aInStringLength, tagTXT, aTagTXTLen, LT_ALPHA, LT_DELIMITER))
  {
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


bool
mozTXTToHTMLConv::SmilyHit(const char16_t * aInString, int32_t aLength, bool col0,
         const char* tagTXT, const char* imageName,
         nsString& outputHTML, int32_t& glyphTextLen)
{
  if ( !aInString || !tagTXT || !imageName )
      return false;

  int32_t tagLen = strlen(tagTXT);

  uint32_t delim = (col0 ? 0 : 1) + tagLen;

  if
    (
      (col0 || IsSpace(aInString[0]))
        &&
        (
          aLength <= int32_t(delim) ||
          IsSpace(aInString[delim]) ||
          (aLength > int32_t(delim + 1)
            &&
            (
              aInString[delim] == '.' ||
              aInString[delim] == ',' ||
              aInString[delim] == ';' ||
              aInString[delim] == '8' ||
              aInString[delim] == '>' ||
              aInString[delim] == '!' ||
              aInString[delim] == '?'
            )
            && IsSpace(aInString[delim + 1]))
        )
        && ItMatchesDelimited(aInString, aLength, NS_ConvertASCIItoUTF16(tagTXT).get(), tagLen,
                              col0 ? LT_IGNORE : LT_DELIMITER, LT_IGNORE)
	        // Note: tests at different pos for LT_IGNORE and LT_DELIMITER
    )
  {
    if (!col0)
    {
      outputHTML.Truncate();
      outputHTML.Append(char16_t(' '));
    }

    outputHTML.AppendLiteral("<span class=\""); // <span class="
    AppendASCIItoUTF16(imageName, outputHTML);  // e.g. smiley-frown
    outputHTML.AppendLiteral("\" title=\"");    // " title="
    AppendASCIItoUTF16(tagTXT, outputHTML);     // smiley tooltip
    outputHTML.AppendLiteral("\"><span>");      // "><span>
    AppendASCIItoUTF16(tagTXT, outputHTML);     // original text
    outputHTML.AppendLiteral("</span></span>"); // </span></span>
    glyphTextLen = (col0 ? 0 : 1) + tagLen;
    return true;
  }

  return false;
}

// the glyph is appended to aOutputString instead of the original string...
bool
mozTXTToHTMLConv::GlyphHit(const char16_t * aInString, int32_t aInLength, bool col0,
         nsString& aOutputString, int32_t& glyphTextLen)
{
  char16_t text0 = aInString[0];
  char16_t text1 = aInString[1];
  char16_t firstChar = (col0 ? text0 : text1);

  // temporary variable used to store the glyph html text
  nsAutoString outputHTML;
  bool bTestSmilie;
  bool bArg = false;
  int i;

  // refactor some of this mess to avoid code duplication and speed execution a bit
  // there are two cases that need to be tried one after another. To avoid a lot of
  // duplicate code, rolling into a loop

  i = 0;
  while ( i < 2 )
  {
    bTestSmilie = false;
    if ( !i && (firstChar == ':' || firstChar == ';' || firstChar == '=' || firstChar == '>' || firstChar == '8' || firstChar == 'O'))
    {
        // first test passed

        bTestSmilie = true;
        bArg = col0;
    }
    if ( i && col0 && ( text1 == ':' || text1 == ';' || text1 == '=' || text1 == '>' || text1 == '8' || text1 == 'O' ) )
    {
        // second test passed

        bTestSmilie = true;
        bArg = false;
    }
    if ( bTestSmilie && (
          SmilyHit(aInString, aInLength, bArg,
                   ":-)",
                   "moz-smiley-s1", // smile
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":)",
                   "moz-smiley-s1", // smile
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-D",
                   "moz-smiley-s5", // laughing
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-(",
                   "moz-smiley-s2", // frown
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":(",
                   "moz-smiley-s2", // frown
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-[",
                   "moz-smiley-s6", // embarassed
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ";-)",
                   "moz-smiley-s3", // wink
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, col0,
                   ";)",
                   "moz-smiley-s3", // wink
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-\\",
                   "moz-smiley-s7", // undecided
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-P",
                   "moz-smiley-s4", // tongue
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ";-P",
                   "moz-smiley-s4", // tongue
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   "=-O",
                   "moz-smiley-s8", // surprise
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-*",
                   "moz-smiley-s9", // kiss
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ">:o",
                   "moz-smiley-s10", // yell
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ">:-o",
                   "moz-smiley-s10", // yell
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   "8-)",
                   "moz-smiley-s11", // cool
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-$",
                   "moz-smiley-s12", // money
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-!",
                   "moz-smiley-s13", // foot
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   "O:-)",
                   "moz-smiley-s14", // innocent
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":'(",
                   "moz-smiley-s15", // cry
                   outputHTML, glyphTextLen) ||

          SmilyHit(aInString, aInLength, bArg,
                   ":-X",
                   "moz-smiley-s16", // sealed
                   outputHTML, glyphTextLen)
        )
    )
    {
        aOutputString.Append(outputHTML);
        return true;
    }
    i++;
  }
  if (text0 == '\f')
  {
      aOutputString.AppendLiteral("<span class='moz-txt-formfeed'></span>");
      glyphTextLen = 1;
      return true;
  }
  if (text0 == '+' || text1 == '+')
  {
    if (ItMatchesDelimited(aInString, aInLength,
                           u" +/-", 4,
                           LT_IGNORE, LT_IGNORE))
    {
      aOutputString.AppendLiteral(" &plusmn;");
      glyphTextLen = 4;
      return true;
    }
    if (col0 && ItMatchesDelimited(aInString, aInLength,
                                   u"+/-", 3,
                                   LT_IGNORE, LT_IGNORE))
    {
      aOutputString.AppendLiteral("&plusmn;");
      glyphTextLen = 3;
      return true;
    }
  }

  // x^2  =>  x<sup>2</sup>,   also handle powers x^-2,  x^0.5
  // implement regular expression /[\dA-Za-z\)\]}]\^-?\d+(\.\d+)*[^\dA-Za-z]/
  if
    (
      text1 == '^'
      &&
      (
        nsCRT::IsAsciiDigit(text0) || nsCRT::IsAsciiAlpha(text0) ||
        text0 == ')' || text0 == ']' || text0 == '}'
      )
      &&
      (
        (2 < aInLength && nsCRT::IsAsciiDigit(aInString[2])) ||
        (3 < aInLength && aInString[2] == '-' && nsCRT::IsAsciiDigit(aInString[3]))
      )
    )
  {
    // Find first non-digit
    int32_t delimPos = 3;  // skip "^" and first digit (or '-')
    for (; delimPos < aInLength
           &&
           (
             nsCRT::IsAsciiDigit(aInString[delimPos]) ||
             (aInString[delimPos] == '.' && delimPos + 1 < aInLength &&
               nsCRT::IsAsciiDigit(aInString[delimPos + 1]))
           );
         delimPos++)
      ;

    if (delimPos < aInLength && nsCRT::IsAsciiAlpha(aInString[delimPos]))
    {
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

    glyphTextLen = delimPos /* - 1 + 1 */ ;
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

mozTXTToHTMLConv::mozTXTToHTMLConv()
{
}

mozTXTToHTMLConv::~mozTXTToHTMLConv()
{
}

NS_IMPL_ISUPPORTS(mozTXTToHTMLConv,
                  mozITXTToHTMLConv,
                  nsIStreamConverter,
                  nsIStreamListener,
                  nsIRequestObserver)

int32_t
mozTXTToHTMLConv::CiteLevelTXT(const char16_t *line,
				    uint32_t& logLineStart)
{
  int32_t result = 0;
  int32_t lineLength = NS_strlen(line);

  bool moreCites = true;
  while (moreCites)
  {
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
    for (; int32_t(i) < lineLength && nsCRT::IsAsciiAlpha(line[i])
                                   && nsCRT::IsUpper(line[i])   ; i++)
      ;
    if (int32_t(i) < lineLength && (line[i] == '>' || line[i] == ']'))
#else
    if (int32_t(i) < lineLength && line[i] == '>')
#endif
    {
      i++;
      if (int32_t(i) < lineLength && line[i] == ' ')
        i++;
      // sendmail/mbox
      // Placed here for performance increase
      const char16_t * indexString = &line[logLineStart];
           // here, |logLineStart < lineLength| is always true
      uint32_t minlength = std::min(uint32_t(6), NS_strlen(indexString));
      if (Substring(indexString,
                    indexString+minlength).Equals(Substring(NS_LITERAL_STRING(">From "), 0, minlength),
                                                  nsCaseInsensitiveStringComparator()))
        //XXX RFC2646
        moreCites = false;
      else
      {
        result++;
        logLineStart = i;
      }
    }
    else
      moreCites = false;
  }

  return result;
}

void
mozTXTToHTMLConv::ScanTXT(const char16_t * aInString, int32_t aInStringLength, uint32_t whattodo, nsString& aOutString)
{
  bool doURLs = 0 != (whattodo & kURLs);
  bool doGlyphSubstitution = 0 != (whattodo & kGlyphSubstitution);
  bool doStructPhrase = 0 != (whattodo & kStructPhrase);

  uint32_t structPhrase_strong = 0;  // Number of currently open tags
  uint32_t structPhrase_underline = 0;
  uint32_t structPhrase_italic = 0;
  uint32_t structPhrase_code = 0;

  nsAutoString outputHTML;  // moved here for performance increase

  for(uint32_t i = 0; int32_t(i) < aInStringLength;)
  {
    if (doGlyphSubstitution)
    {
      int32_t glyphTextLen;
      if (GlyphHit(&aInString[i], aInStringLength - i, i == 0, aOutString, glyphTextLen))
      {
        i += glyphTextLen;
        continue;
      }
    }

    if (doStructPhrase)
    {
      const char16_t * newOffset = aInString;
      int32_t newLength = aInStringLength;
      if (i > 0 ) // skip the first element?
      {
        newOffset = &aInString[i-1];
        newLength = aInStringLength - i + 1;
      }

      switch (aInString[i]) // Performance increase
      {
      case '*':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            u"*", 1,
                            "b", "class=\"moz-txt-star\"",
                            aOutString, structPhrase_strong))
        {
          i++;
          continue;
        }
        break;
      case '/':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            u"/", 1,
                            "i", "class=\"moz-txt-slash\"",
                            aOutString, structPhrase_italic))
        {
          i++;
          continue;
        }
        break;
      case '_':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            u"_", 1,
                            "span" /* <u> is deprecated */,
                            "class=\"moz-txt-underscore\"",
                            aOutString, structPhrase_underline))
        {
          i++;
          continue;
        }
        break;
      case '|':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            u"|", 1,
                            "code", "class=\"moz-txt-verticalline\"",
                            aOutString, structPhrase_code))
        {
          i++;
          continue;
        }
        break;
      }
    }

    if (doURLs)
    {
      switch (aInString[i])
      {
      case ':':
      case '@':
      case '.':
        if ( (i == 0 || ((i > 0) && aInString[i - 1] != ' ')) && aInString[i +1] != ' ') // Performance increase
        {
          int32_t replaceBefore;
          int32_t replaceAfter;
          if (FindURL(aInString, aInStringLength, i, whattodo,
                      outputHTML, replaceBefore, replaceAfter)
                  && structPhrase_strong + structPhrase_italic +
                       structPhrase_underline + structPhrase_code == 0
                       /* workaround for bug #19445 */ )
          {
            aOutString.Cut(aOutString.Length() - replaceBefore, replaceBefore);
            aOutString += outputHTML;
            i += replaceAfter + 1;
            continue;
          }
        }
        break;
      } //switch
    }

    switch (aInString[i])
    {
    // Special symbols
    case '<':
    case '>':
    case '&':
      EscapeChar(aInString[i], aOutString, false);
      i++;
      break;
    // Normal characters
    default:
      aOutString += aInString[i];
      i++;
      break;
    }
  }
}

void
mozTXTToHTMLConv::ScanHTML(nsString& aInString, uint32_t whattodo, nsString &aOutString)
{
  // some common variables we were recalculating
  // every time inside the for loop...
  int32_t lengthOfInString = aInString.Length();
  const char16_t * uniBuffer = aInString.get();

#ifdef DEBUG_BenB_Perf
  PRTime parsing_start = PR_IntervalNow();
#endif

  // Look for simple entities not included in a tags and scan them.
  // Skip all tags ("<[...]>") and content in an a link tag ("<a [...]</a>"),
  // comment tag ("<!--[...]-->"), style tag, script tag or head tag.
  // Unescape the rest (text between tags) and pass it to ScanTXT.
  nsAutoCString canFollow(" \f\n\r\t>");
  for (int32_t i = 0; i < lengthOfInString;)
  {
    if (aInString[i] == '<')  // html tag
    {
      int32_t start = i;
      if (i + 2 < lengthOfInString &&
          nsCRT::ToLower(aInString[i + 1]) == 'a' &&
          canFollow.FindChar(aInString[i + 2]) != kNotFound)
           // if a tag, skip until </a>.
           // Make sure there's a white-space character after, not to match "abbr".
      {
        i = aInString.Find("</a>", true, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 4;
      }
      else if (Substring(aInString, i + 1, 3).LowerCaseEqualsASCII("!--"))
          // if out-commended code, skip until -->
      {
        i = aInString.Find("-->", false, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 3;
      }
      else if (i + 6 < lengthOfInString &&
      Substring(aInString, i + 1, 5).LowerCaseEqualsASCII("style") &&
               canFollow.FindChar(aInString[i + 6]) != kNotFound)
           // if style tag, skip until </style>
      {
        i = aInString.Find("</style>", true, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 8;
      }
      else if (i + 7 < lengthOfInString &&
               Substring(aInString, i + 1, 6).LowerCaseEqualsASCII("script") &&
               canFollow.FindChar(aInString[i + 7]) != kNotFound)
           // if script tag, skip until </script>
      {
        i = aInString.Find("</script>", true, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 9;
      }
      else if (i + 5 < lengthOfInString &&
               Substring(aInString, i + 1, 4).LowerCaseEqualsASCII("head") &&
               canFollow.FindChar(aInString[i + 5]) != kNotFound)
           // if head tag, skip until </head>
           // Make sure not to match <header>.
      {
        i = aInString.Find("</head>", true, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 7;
      }
      else  // just skip tag (attributes etc.)
      {
        i = aInString.FindChar('>', i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i++;
      }
      aOutString.Append(&uniBuffer[start], i - start);
    }
    else
    {
      uint32_t start = uint32_t(i);
      i = aInString.FindChar('<', i);
      if (i == kNotFound)
        i = lengthOfInString;

      nsString tempString;
      tempString.SetCapacity(uint32_t((uint32_t(i) - start) * growthRate));
      UnescapeStr(uniBuffer, start, uint32_t(i) - start, tempString);
      ScanTXT(tempString.get(), tempString.Length(), whattodo, aOutString);
    }
  }

#ifdef DEBUG_BenB_Perf
  printf("ScanHTML time:    %d ms\n", PR_IntervalToMilliseconds(PR_IntervalNow() - parsing_start));
#endif
}

/****************************************************************************
  XPCOM Interface
*****************************************************************************/

NS_IMETHODIMP
mozTXTToHTMLConv::Convert(nsIInputStream *aFromStream,
                          const char *aFromType,
                          const char *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::AsyncConvertData(const char *aFromType,
                                   const char *aToType,
                                   nsIStreamListener *aListener, nsISupports *aCtxt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                 nsIInputStream *inStr, uint64_t sourceOffset,
                                 uint32_t count)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                                nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::CiteLevelTXT(const char16_t *line, uint32_t *logLineStart,
				uint32_t *_retval)
{
   if (!logLineStart || !_retval || !line)
     return NS_ERROR_NULL_POINTER;
   *_retval = CiteLevelTXT(line, *logLineStart);
   return NS_OK;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanTXT(const char16_t *text, uint32_t whattodo,
			   char16_t **_retval)
{
  NS_ENSURE_ARG(text);

  // FIX ME!!!
  nsString outString;
  int32_t inLength = NS_strlen(text);
  // by setting a large capacity up front, we save time
  // when appending characters to the output string because we don't
  // need to reallocate and re-copy the characters already in the out String.
  NS_ASSERTION(inLength, "ScanTXT passed 0 length string");
  if (inLength == 0) {
    *_retval = NS_strdup(text);
    return NS_OK;
  }

  outString.SetCapacity(uint32_t(inLength * growthRate));
  ScanTXT(text, inLength, whattodo, outString);

  *_retval = ToNewUnicode(outString);
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanHTML(const char16_t *text, uint32_t whattodo,
			    char16_t **_retval)
{
  NS_ENSURE_ARG(text);

  // FIX ME!!!
  nsString outString;
  nsString inString (text); // look at this nasty extra copy of the entire input buffer!
  outString.SetCapacity(uint32_t(inString.Length() * growthRate));

  ScanHTML(inString, whattodo, outString);
  *_retval = ToNewUnicode(outString);
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
MOZ_NewTXTToHTMLConv(mozTXTToHTMLConv** aConv)
{
    NS_PRECONDITION(aConv != nullptr, "null ptr");
    if (!aConv)
      return NS_ERROR_NULL_POINTER;

    *aConv = new mozTXTToHTMLConv();
    if (!*aConv)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aConv);
    //    return (*aConv)->Init();
    return NS_OK;
}
