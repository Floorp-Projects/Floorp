/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozTXTToHTMLConv.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsIExternalProtocolHandler.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#ifdef DEBUG_BenB_Perf
#include "prtime.h"
#include "prinrval.h"
#endif

const PRFloat64 growthRate = 1.2;

// Bug 183111, editor now replaces multiple spaces with leading
// 0xA0's and a single ending space, so need to treat 0xA0's as spaces.
// 0xA0 is the Latin1/Unicode character for "non-breaking space (nbsp)"
// Also recognize the Japanese ideographic space 0x3000 as a space.
static inline bool IsSpace(const PRUnichar aChar)
{
  return (nsCRT::IsAsciiSpace(aChar) || aChar == 0xA0 || aChar == 0x3000);
}

// Escape Char will take ch, escape it and append the result to 
// aStringToAppendTo
void
mozTXTToHTMLConv::EscapeChar(const PRUnichar ch, nsString& aStringToAppendTo,
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
    default:
      aStringToAppendTo += ch;
    }

    return;
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
  for (PRUint32 i = 0; i < aInString.Length();)
  {
    switch (aInString[i])
    {
    case '<':
      aInString.Cut(i, 1);
      aInString.Insert(NS_LITERAL_STRING("&lt;"), i);
      i += 4; // skip past the integers we just added
      break;
    case '>':
      aInString.Cut(i, 1);
      aInString.Insert(NS_LITERAL_STRING("&gt;"), i);
      i += 4; // skip past the integers we just added
      break;
    case '&':
      aInString.Cut(i, 1);
      aInString.Insert(NS_LITERAL_STRING("&amp;"), i);
      i += 5; // skip past the integers we just added
      break;
    case '"':
      if (inAttribute)
      {
        aInString.Cut(i, 1);
        aInString.Insert(NS_LITERAL_STRING("&quot;"), i);
        i += 6;
        break;
      }
      // else fall through
    default:
      i++;
    }
  }
}

void 
mozTXTToHTMLConv::UnescapeStr(const PRUnichar * aInString, PRInt32 aStartPos, PRInt32 aLength, nsString& aOutString)
{
  const PRUnichar * subString = nullptr;
  for (PRUint32 i = aStartPos; PRInt32(i) - aStartPos < aLength;)
  {
    PRInt32 remainingChars = i - aStartPos;
    if (aInString[i] == '&')
    {
      subString = &aInString[i];
      if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&lt;").get(), MinInt(4, aLength - remainingChars)))
      {
        aOutString.Append(PRUnichar('<'));
        i += 4;
      }
      else if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&gt;").get(), MinInt(4, aLength - remainingChars)))
      {
        aOutString.Append(PRUnichar('>'));
        i += 4;
      }
      else if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&amp;").get(), MinInt(5, aLength - remainingChars)))
      {
        aOutString.Append(PRUnichar('&'));
        i += 5;
      }
      else if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&quot;").get(), MinInt(6, aLength - remainingChars)))
      {
        aOutString.Append(PRUnichar('"'));
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
mozTXTToHTMLConv::CompleteAbbreviatedURL(const PRUnichar * aInString, PRInt32 aInLength, 
                                         const PRUint32 pos, nsString& aOutString)
{
  NS_ASSERTION(PRInt32(pos) < aInLength, "bad args to CompleteAbbreviatedURL, see bug #190851");
  if (PRInt32(pos) >= aInLength)
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
                           NS_LITERAL_STRING("www.").get(), 4, LT_IGNORE, LT_IGNORE))
    {
      aOutString.AssignLiteral("http://");
      aOutString += aInString;
    }
    else if (ItMatchesDelimited(aInString,aInLength, NS_LITERAL_STRING("ftp.").get(), 4, LT_IGNORE, LT_IGNORE))
    { 
      aOutString.AssignLiteral("ftp://");
      aOutString += aInString;
    }
  }
}

bool
mozTXTToHTMLConv::FindURLStart(const PRUnichar * aInString, PRInt32 aInLength,
                               const PRUint32 pos, const modetype check,
                               PRUint32& start)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  {
    if (!nsCRT::strncmp(&aInString[MaxInt(pos - 4, 0)], NS_LITERAL_STRING("<URL:").get(), 5))
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
    PRInt32 i = pos <= 0 ? kNotFound : temp.RFindCharInSet(NS_LITERAL_STRING("<>\"").get(), pos - 1);
    if (i != kNotFound && (temp[PRUint32(i)] == '<' ||
                           temp[PRUint32(i)] == '"'))
    {
      start = PRUint32(++i);
      return start < pos;
    }
    else
      return false;
  }
  case freetext:
  {
    PRInt32 i = pos - 1;
    for (; i >= 0 && (
         nsCRT::IsAsciiAlpha(aInString[PRUint32(i)]) ||
         nsCRT::IsAsciiDigit(aInString[PRUint32(i)]) ||
         aInString[PRUint32(i)] == '+' ||
         aInString[PRUint32(i)] == '-' ||
         aInString[PRUint32(i)] == '.'
         ); i--)
      ;
    if (++i >= 0 && PRUint32(i) < pos && nsCRT::IsAsciiAlpha(aInString[PRUint32(i)]))
    {
      start = PRUint32(i);
      return true;
    }
    else
      return false;
  }
  case abbreviated:
  {
    PRInt32 i = pos - 1;
    // This disallows non-ascii-characters for email.
    // Currently correct, but revisit later after standards changed.
    bool isEmail = aInString[pos] == (PRUnichar)'@';
    // These chars mark the start of the URL
    for (; i >= 0
             && aInString[PRUint32(i)] != '>' && aInString[PRUint32(i)] != '<'
             && aInString[PRUint32(i)] != '"' && aInString[PRUint32(i)] != '\''
             && aInString[PRUint32(i)] != '`' && aInString[PRUint32(i)] != ','
             && aInString[PRUint32(i)] != '{' && aInString[PRUint32(i)] != '['
             && aInString[PRUint32(i)] != '(' && aInString[PRUint32(i)] != '|'
             && aInString[PRUint32(i)] != '\\'
             && !IsSpace(aInString[PRUint32(i)])
             && (!isEmail || nsCRT::IsAscii(aInString[PRUint32(i)]))
         ; i--)
      ;
    if
      (
        ++i >= 0 && PRUint32(i) < pos
          &&
          (
            nsCRT::IsAsciiAlpha(aInString[PRUint32(i)]) ||
            nsCRT::IsAsciiDigit(aInString[PRUint32(i)])
          )
      )
    {
      start = PRUint32(i);
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
mozTXTToHTMLConv::FindURLEnd(const PRUnichar * aInString, PRInt32 aInStringLength, const PRUint32 pos,
           const modetype check, const PRUint32 start, PRUint32& end)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  case RFC2396E:
  {
    nsString temp(aInString, aInStringLength);

    PRInt32 i = temp.FindCharInSet(NS_LITERAL_STRING("<>\"").get(), pos + 1);
    if (i != kNotFound && temp[PRUint32(i--)] ==
        (check == RFC1738 || temp[start - 1] == '<' ? '>' : '"'))
    {
      end = PRUint32(i);
      return end > pos;
    }
    return false;
  }
  case freetext:
  case abbreviated:
  {
    PRUint32 i = pos + 1;
    bool isEmail = aInString[pos] == (PRUnichar)'@';
    bool seenOpeningParenthesis = false; // there is a '(' earlier in the URL
    bool seenOpeningSquareBracket = false; // there is a '[' earlier in the URL
    for (; PRInt32(i) < aInStringLength; i++)
    {
      // These chars mark the end of the URL
      if (aInString[i] == '>' || aInString[i] == '<' ||
          aInString[i] == '"' || aInString[i] == '`' ||
          aInString[i] == '}' || aInString[i] == '{' ||
          aInString[i] == '|' ||
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
mozTXTToHTMLConv::CalculateURLBoundaries(const PRUnichar * aInString, PRInt32 aInStringLength, 
     const PRUint32 pos, const PRUint32 whathasbeendone,
     const modetype check, const PRUint32 start, const PRUint32 end,
     nsString& txtURL, nsString& desc,
     PRInt32& replaceBefore, PRInt32& replaceAfter)
{
  PRUint32 descstart = start;
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
  return;
}

bool mozTXTToHTMLConv::ShouldLinkify(const nsCString& aURL)
{
  if (!mIOService)
    return false;

  nsCAutoString scheme;
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

NS_IMETHODIMP mozTXTToHTMLConv::FindURLInPlaintext(const PRUnichar * aInString, PRInt32 aInLength, PRInt32 aPos, PRInt32 * aStartPos, PRInt32 * aEndPos)
{
  // call FindURL on the passed in string
  nsAutoString outputHTML; // we'll ignore the generated output HTML

  *aStartPos = -1;
  *aEndPos = -1;

  FindURL(aInString, aInLength, aPos, kURLs, outputHTML, *aStartPos, *aEndPos);

  return NS_OK;
}

bool
mozTXTToHTMLConv::FindURL(const PRUnichar * aInString, PRInt32 aInLength, const PRUint32 pos,
     const PRUint32 whathasbeendone,
     nsString& outputHTML, PRInt32& replaceBefore, PRInt32& replaceAfter)
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
    // no break here
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
  PRInt32 iCheck = 0;  // the currently tested modetype
  modetype check = ranking[iCheck];
  for (; iCheck < mozTXTToHTMLConv_numberOfModes && state[check] != success;
       iCheck++)
    /* check state from last run.
       If this is the first, check this one, which isn't = success yet */
  {
    check = ranking[iCheck];

    PRUint32 start, end;

    if (state[check] == unchecked)
      if (FindURLStart(aInString, aInLength, pos, check, start))
        state[check] = startok;

    if (state[check] == startok)
      if (FindURLEnd(aInString, aInLength, pos, check, start, end))
        state[check] = endok;

    if (state[check] == endok)
    {
      nsAutoString txtURL, desc;
      PRInt32 resultReplaceBefore, resultReplaceAfter;

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
mozTXTToHTMLConv::ItMatchesDelimited(const PRUnichar * aInString,
    PRInt32 aInLength, const PRUnichar* rep, PRInt32 aRepLen,
    LIMTYPE before, LIMTYPE after)
{

  // this little method gets called a LOT. I found we were spending a
  // lot of time just calculating the length of the variable "rep"
  // over and over again every time we called it. So we're now passing
  // an integer in here.
  PRInt32 textLen = aInLength;

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

  PRUnichar text0 = aInString[0];
  PRUnichar textAfterPos = aInString[aRepLen + (before == LT_IGNORE ? 0 : 1)];

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

PRUint32
mozTXTToHTMLConv::NumberOfMatches(const PRUnichar * aInString, PRInt32 aInStringLength, 
     const PRUnichar* rep, PRInt32 aRepLen, LIMTYPE before, LIMTYPE after)
{
  PRUint32 result = 0;

  for (PRInt32 i = 0; i < aInStringLength; i++)
  {
    const PRUnichar * indexIntoString = &aInString[i];
    if (ItMatchesDelimited(indexIntoString, aInStringLength - i, rep, aRepLen, before, after))
      result++;
  }
  return result;
}


// NOTE: the converted html for the phrase is appended to aOutString
// tagHTML and attributeHTML are plain ASCII (literal strings, in fact)
bool
mozTXTToHTMLConv::StructPhraseHit(const PRUnichar * aInString, PRInt32 aInStringLength, bool col0,
     const PRUnichar* tagTXT, PRInt32 aTagTXTLen, 
     const char* tagHTML, const char* attributeHTML,
     nsString& aOutString, PRUint32& openTags)
{
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existence of a pair could be verified
     We use the first opening/closing tag, if we can choose */

  const PRUnichar * newOffset = aInString;
  PRInt32 newLength = aInStringLength;
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
    aOutString.AppendLiteral("<");
    aOutString.AppendASCII(tagHTML);
    aOutString.Append(PRUnichar(' '));
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
    aOutString.Append(PRUnichar('>'));
    return true;
  }

  return false;
}


bool
mozTXTToHTMLConv::SmilyHit(const PRUnichar * aInString, PRInt32 aLength, bool col0,
         const char* tagTXT, const char* imageName,
         nsString& outputHTML, PRInt32& glyphTextLen)
{
  if ( !aInString || !tagTXT || !imageName )
      return false;

  PRInt32 tagLen = strlen(tagTXT);
 
  PRUint32 delim = (col0 ? 0 : 1) + tagLen;

  if
    (
      (col0 || IsSpace(aInString[0]))
        &&
        (
          aLength <= PRInt32(delim) ||
          IsSpace(aInString[delim]) ||
          (aLength > PRInt32(delim + 1)
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
      outputHTML.Append(PRUnichar(' '));
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
mozTXTToHTMLConv::GlyphHit(const PRUnichar * aInString, PRInt32 aInLength, bool col0,
         nsString& aOutputString, PRInt32& glyphTextLen)
{
  PRUnichar text0 = aInString[0]; 
  PRUnichar text1 = aInString[1];
  PRUnichar firstChar = (col0 ? text0 : text1);

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
                           NS_LITERAL_STRING(" +/-").get(), 4,
                           LT_IGNORE, LT_IGNORE))
    {
      aOutputString.AppendLiteral(" &plusmn;");
      glyphTextLen = 4;
      return true;
    }
    if (col0 && ItMatchesDelimited(aInString, aInLength,
                                   NS_LITERAL_STRING("+/-").get(), 3,
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
    PRInt32 delimPos = 3;  // skip "^" and first digit (or '-')
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

NS_IMPL_ISUPPORTS4(mozTXTToHTMLConv,
                   mozITXTToHTMLConv,
                   nsIStreamConverter,
                   nsIStreamListener,
                   nsIRequestObserver)

PRInt32
mozTXTToHTMLConv::CiteLevelTXT(const PRUnichar *line,
				    PRUint32& logLineStart)
{
  PRInt32 result = 0;
  PRInt32 lineLength = NS_strlen(line);

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
    PRUint32 i = logLineStart;

#ifdef QUOTE_RECOGNITION_AGGRESSIVE
    for (; PRInt32(i) < lineLength && IsSpace(line[i]); i++)
      ;
    for (; PRInt32(i) < lineLength && nsCRT::IsAsciiAlpha(line[i])
                                   && nsCRT::IsUpper(line[i])   ; i++)
      ;
    if (PRInt32(i) < lineLength && (line[i] == '>' || line[i] == ']'))
#else
    if (PRInt32(i) < lineLength && line[i] == '>')
#endif
    {
      i++;
      if (PRInt32(i) < lineLength && line[i] == ' ')
        i++;
      // sendmail/mbox
      // Placed here for performance increase
      const PRUnichar * indexString = &line[logLineStart];
           // here, |logLineStart < lineLength| is always true
      PRUint32 minlength = MinInt(6, NS_strlen(indexString));
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
mozTXTToHTMLConv::ScanTXT(const PRUnichar * aInString, PRInt32 aInStringLength, PRUint32 whattodo, nsString& aOutString)
{
  bool doURLs = 0 != (whattodo & kURLs);
  bool doGlyphSubstitution = 0 != (whattodo & kGlyphSubstitution);
  bool doStructPhrase = 0 != (whattodo & kStructPhrase);

  PRUint32 structPhrase_strong = 0;  // Number of currently open tags
  PRUint32 structPhrase_underline = 0;
  PRUint32 structPhrase_italic = 0;
  PRUint32 structPhrase_code = 0;

  nsAutoString outputHTML;  // moved here for performance increase

  for(PRUint32 i = 0; PRInt32(i) < aInStringLength;)
  {
    if (doGlyphSubstitution)
    {
      PRInt32 glyphTextLen;
      if (GlyphHit(&aInString[i], aInStringLength - i, i == 0, aOutString, glyphTextLen))
      {
        i += glyphTextLen;
        continue;
      }
    }

    if (doStructPhrase)
    {
      const PRUnichar * newOffset = aInString;
      PRInt32 newLength = aInStringLength;
      if (i > 0 ) // skip the first element?
      {
        newOffset = &aInString[i-1];
        newLength = aInStringLength - i + 1;
      }

      switch (aInString[i]) // Performance increase
      {
      case '*':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            NS_LITERAL_STRING("*").get(), 1,
                            "b", "class=\"moz-txt-star\"",
                            aOutString, structPhrase_strong))
        {
          i++;
          continue;
        }
        break;
      case '/':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            NS_LITERAL_STRING("/").get(), 1,
                            "i", "class=\"moz-txt-slash\"",
                            aOutString, structPhrase_italic))
        {
          i++;
          continue;
        }
        break;
      case '_':
        if (StructPhraseHit(newOffset, newLength, i == 0,
                            NS_LITERAL_STRING("_").get(), 1,
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
                            NS_LITERAL_STRING("|").get(), 1,
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
          PRInt32 replaceBefore;
          PRInt32 replaceAfter;
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
mozTXTToHTMLConv::ScanHTML(nsString& aInString, PRUint32 whattodo, nsString &aOutString)
{ 
  // some common variables we were recalculating
  // every time inside the for loop...
  PRInt32 lengthOfInString = aInString.Length();
  const PRUnichar * uniBuffer = aInString.get();

#ifdef DEBUG_BenB_Perf
  PRTime parsing_start = PR_IntervalNow();
#endif

  // Look for simple entities not included in a tags and scan them.
  /* Skip all tags ("<[...]>") and content in an a tag ("<a[...]</a>")
     or in a tag ("<!--[...]-->").
     Unescape the rest (text between tags) and pass it to ScanTXT. */
  for (PRInt32 i = 0; i < lengthOfInString;)
  {
    if (aInString[i] == '<')  // html tag
    {
      PRUint32 start = PRUint32(i);
      if (nsCRT::ToLower((char)aInString[PRUint32(i) + 1]) == 'a')
           // if a tag, skip until </a>
      {
        i = aInString.Find("</a>", true, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 4;
      }
      else if (aInString[PRUint32(i) + 1] == '!' && aInString[PRUint32(i) + 2] == '-' &&
        aInString[PRUint32(i) + 3] == '-')
          //if out-commended code, skip until -->
      {
        i = aInString.Find("-->", false, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 3;

      }
      else  // just skip tag (attributes etc.)
      {
        i = aInString.FindChar('>', i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i++;
      }
      aOutString.Append(&uniBuffer[start], PRUint32(i) - start);
    }
    else
    {
      PRUint32 start = PRUint32(i);
      i = aInString.FindChar('<', i);
      if (i == kNotFound)
        i = lengthOfInString;
  
      nsString tempString;     
      tempString.SetCapacity(PRUint32((PRUint32(i) - start) * growthRate));
      UnescapeStr(uniBuffer, start, PRUint32(i) - start, tempString);
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
                                 nsIInputStream *inStr, PRUint32 sourceOffset,
                                 PRUint32 count)
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
mozTXTToHTMLConv::CiteLevelTXT(const PRUnichar *line, PRUint32 *logLineStart,
				PRUint32 *_retval)
{
   if (!logLineStart || !_retval || !line)
     return NS_ERROR_NULL_POINTER;
   *_retval = CiteLevelTXT(line, *logLineStart);
   return NS_OK;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanTXT(const PRUnichar *text, PRUint32 whattodo,
			   PRUnichar **_retval)
{
  NS_ENSURE_ARG(text);

  // FIX ME!!!
  nsString outString;
  PRInt32 inLength = NS_strlen(text);
  // by setting a large capacity up front, we save time
  // when appending characters to the output string because we don't
  // need to reallocate and re-copy the characters already in the out String.
  NS_ASSERTION(inLength, "ScanTXT passed 0 length string");
  if (inLength == 0) {
    *_retval = nsCRT::strdup(text);
    return NS_OK;
  }

  outString.SetCapacity(PRUint32(inLength * growthRate));
  ScanTXT(text, inLength, whattodo, outString);

  *_retval = ToNewUnicode(outString);
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanHTML(const PRUnichar *text, PRUint32 whattodo,
			    PRUnichar **_retval)
{
  NS_ENSURE_ARG(text);

  // FIX ME!!!
  nsString outString;
  nsString inString (text); // look at this nasty extra copy of the entire input buffer!
  outString.SetCapacity(PRUint32(inString.Length() * growthRate));

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
