/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * The "License" shall be the Mozilla Public License Version 1.1, except
 * Sections 6.2 and 11, but with the addition of the below defined Section 14.
 * You may obtain a copy of the Mozilla Public License Version 1.1 at
 * <http://www.mozilla.org/MPL/>. The contents of this file are subject to the
 * License; you may not use this file except in compliance with the License.
 * 
 * Section 14: MISCELLANEOUS.
 * This License represents the complete agreement concerning subject matter
 * hereof. If any provision of this License is held to be unenforceable, such
 * provision shall be reformed only to the extent necessary to make it
 * enforceable. This License shall be governed by German law provisions. Any
 * litigation relating to this License shall be subject to German jurisdiction.
 * 
 * Once Covered Code has been published under a particular version of the
 * License, You may always continue to use it under the terms of that version.
 + The Initial Developer and no one else has the right to modify the terms
 * applicable to Covered Code created under this License.
 * (End of Section 14)
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is the Mozilla Text to HTML converter code.
 * 
 * The Initial Developer of the Original Code is Ben Bucksch
 * <http://www.bucksch.org>. Portions created by Ben Bucksch are Copyright
 * (C) 1999, 2000 Ben Bucksch. All Rights Reserved.
 * 
 * Contributor(s):
 */

#include "mozTXTToHTMLConv.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#ifdef DEBUG_BenB_Perf
#include "prtime.h"
#include "prinrval.h"
#endif

const PRFloat64 growthRate = 1.2;

// Escape Char will take ch, escape it and append the result to 
// aStringToAppendTo
void
mozTXTToHTMLConv::EscapeChar(const PRUnichar ch, nsString& aStringToAppendTo)
{
    switch (ch)
    {
    case '<':
      aStringToAppendTo.AppendWithConversion("&lt;");
      break;
    case '>':
      aStringToAppendTo.AppendWithConversion("&gt;");
      break;
    case '&':
      aStringToAppendTo.AppendWithConversion("&amp;");
      break;
    default:
      aStringToAppendTo += ch;
    }

    return;
}

// EscapeStr takes the passed in string and
// escapes it IN PLACE.
void 
mozTXTToHTMLConv::EscapeStr(nsString& aInString)
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
      aInString.InsertWithConversion("&lt;", i, 4);
      i += 4; // skip past the integers we just added
      break;
    case '>':
      aInString.Cut(i, 1);
      aInString.InsertWithConversion("&gt;", i, 4);
      i += 4; // skip past the integers we just added
      break;
    case '&':
      aInString.Cut(i, 1);
      aInString.InsertWithConversion("&amp;", i, 5);
      i += 5; // skip past the integers we just added
      break;
    default:
      i++;
    }
  }
}

void 
mozTXTToHTMLConv::UnescapeStr(const PRUnichar * aInString, PRInt32 aStartPos, PRInt32 aLength, nsString& aOutString)
{
  const PRUnichar * subString = nsnull;
  for (PRUint32 i = aStartPos; PRInt32(i) - aStartPos < aLength;)
  {
    PRInt32 remainingChars = i - aStartPos;
    if (aInString[i] == '&')
    {
      subString = &aInString[i];
      if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&lt;").get(), MinInt(4, aLength - remainingChars)))
      {
        aOutString.AppendWithConversion('<');
        i += 4;
      }
      else if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&gt;").get(), MinInt(4, aLength - remainingChars)))
      {
        aOutString.AppendWithConversion('>');
        i += 4;
      }
      else if (!nsCRT::strncmp(subString, NS_LITERAL_STRING("&amp;").get(), MinInt(5, aLength - remainingChars)))
      {
        aOutString.AppendWithConversion('&');
        i += 5;
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
  if (aInString[pos] == '@')
  {
    // only pre-pend a mailto url if the string contains a .domain in it..
    //i.e. we want to linkify johndoe@foo.com but not "let's meet @8pm"
    nsDependentString inString(aInString, aInLength);
    if (inString.FindChar('.', pos) != kNotFound) // if we have a '.' after the @ sign....
    {
      aOutString.AssignWithConversion("mailto:");
      aOutString += aInString;
    }
  }
  else if (aInString[pos] == '.')
  {
    if (ItMatchesDelimited(aInString, aInLength,
                           NS_LITERAL_STRING("www.").get(), 4, LT_IGNORE, LT_IGNORE))
    {
      aOutString.AssignWithConversion("http://");
      aOutString += aInString;
    }
    else if (ItMatchesDelimited(aInString,aInLength, NS_LITERAL_STRING("ftp.").get(), 4, LT_IGNORE, LT_IGNORE))
    { 
      aOutString.AssignWithConversion("ftp://");
      aOutString += aInString;
    }
  }
}

PRBool
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
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case RFC2396E:
  {
    nsString temp(aInString, aInLength);
    PRInt32 i = pos <= 0 ? kNotFound : temp.RFindCharInSet("<>\"", pos - 1);
    if (i != kNotFound && (temp[PRUint32(i)] == '<' ||
                           temp[PRUint32(i)] == '"'))
    {
      start = PRUint32(++i);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
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
    if (++i >= 0 && nsCRT::IsAsciiAlpha(aInString[PRUint32(i)]))
    {
      start = PRUint32(i);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case abbreviated:
  {
    PRInt32 i = pos - 1;
    for (; i >= 0
             && aInString[PRUint32(i)] != '>' && aInString[PRUint32(i)] != '<'
             && aInString[PRUint32(i)] != '"' && aInString[PRUint32(i)] != '\''
             && aInString[PRUint32(i)] != '`' && aInString[PRUint32(i)] != ','
             && aInString[PRUint32(i)] != '{' && aInString[PRUint32(i)] != '['
             && aInString[PRUint32(i)] != '(' && aInString[PRUint32(i)] != '|'
             && aInString[PRUint32(i)] != '\\'
             && !nsCRT::IsAsciiSpace(aInString[PRUint32(i)])
         ; i--)
      ;
    if
      (
        ++i >= 0
          &&
          (
            nsCRT::IsAsciiAlpha(aInString[PRUint32(i)]) ||
            nsCRT::IsAsciiDigit(aInString[PRUint32(i)])
          )
      )
    {
      start = PRUint32(i);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  default:
    return PR_FALSE;
  } //switch
}

PRBool
mozTXTToHTMLConv::FindURLEnd(const PRUnichar * aInString, PRInt32 aInStringLength, const PRUint32 pos,
           const modetype check, const PRUint32 start, PRUint32& end)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  case RFC2396E:
  {
    nsString temp(aInString, aInStringLength);

    PRInt32 i = temp.FindCharInSet("<>\"", pos + 1);
    if (i != kNotFound && temp[PRUint32(i--)] ==
        (check == RFC1738 || temp[start - 1] == '<' ? '>' : '"'))
    {
      end = PRUint32(i);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case freetext:
  case abbreviated:
  {
    PRUint32 i = pos + 1;
    for (; PRInt32(i) < aInStringLength
             && aInString[i] != '>' && aInString[i] != '<'
             && aInString[i] != '"' && aInString[i] != '\''
             && aInString[i] != '`'
             && aInString[i] != '}' && aInString[i] != ']'
             && aInString[i] != ')' && aInString[i] != '|'
             && !nsCRT::IsAsciiSpace(aInString[i])
         ; i++)
      ;
    while (--i > pos && (
             aInString[i] == '.' || aInString[i] == ',' || aInString[i] == ';' ||
             aInString[i] == '!' || aInString[i] == '?' || aInString[i] == '-'
             ))
        ;
    if (i > pos)
    {
      end = i;
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  default:
    return PR_FALSE;
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

  EscapeStr(desc);

  txtURL.Append(&aInString[start], end - start + 1);
  txtURL.StripWhitespace();

  // FIX ME
  nsAutoString temp2;
  ScanTXT(&aInString[descstart], pos - descstart, ~kURLs /*prevents loop*/ & whathasbeendone, temp2);
  replaceBefore = temp2.Length();
  return;
}

PRBool
mozTXTToHTMLConv::CheckURLAndCreateHTML(
     const nsString& txtURL, const nsString& desc, const modetype mode,
     nsString& outputHTML)
{
  // Create *uri from txtURL
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;
  if (!mIOService)
    mIOService = do_GetService(kIOServiceCID, &rv);
  
  if (NS_FAILED(rv) || !mIOService)
    return PR_FALSE;

  char* specStr = ToNewCString(txtURL);  //I18N this forces a single byte char
  if (specStr == nsnull)
    return PR_FALSE;

  // it would be faster if we could just check to see if there is a protocol
  // handler for the url and return instead of actually trying to create a url...
  rv = mIOService->NewURI(specStr, nsnull, getter_AddRefs(uri));
  Recycle(specStr);

  // Real work
  if (NS_SUCCEEDED(rv) && uri)
  {
    outputHTML.AssignWithConversion("<a class=\"moz-txt-link-");
    switch(mode)
    {
    case RFC1738:
      outputHTML.AppendWithConversion("rfc1738");
      break;
    case RFC2396E:
      outputHTML.AppendWithConversion("rfc2396E");
      break;
    case freetext:
      outputHTML.AppendWithConversion("freetext");
      break;
    case abbreviated:
      outputHTML.AppendWithConversion("abbreviated");
      break;
    default: break;
    }
    outputHTML.AppendWithConversion("\" href=\"");
    outputHTML += txtURL;
    outputHTML.AppendWithConversion("\">");
    outputHTML += desc;
    outputHTML.AppendWithConversion("</a>");
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

PRBool
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

PRBool
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
      (before == LT_IGNORE && (after == LT_IGNORE || after == LT_DELIMITER))
        && textLen < aRepLen ||
      (before != LT_IGNORE || after != LT_IGNORE && after != LT_DELIMITER)
        && textLen < aRepLen + 1 ||
      before != LT_IGNORE && after != LT_IGNORE && after != LT_DELIMITER
        && textLen < aRepLen + 2
    )
    return PR_FALSE;

  PRUnichar text0 = aInString[0];
  PRUnichar textAfterPos = aInString[aRepLen + (before == LT_IGNORE ? 0 : 1)];

  if
    (
      before == LT_ALPHA
        && !nsCRT::IsAsciiAlpha(text0) ||
      before == LT_DIGIT
        && !nsCRT::IsAsciiDigit(text0) ||
      before == LT_DELIMITER
        &&
        (
          nsCRT::IsAsciiAlpha(text0) ||
          nsCRT::IsAsciiDigit(text0) ||
          text0 == *rep
        ) ||
      after == LT_ALPHA
        && !nsCRT::IsAsciiAlpha(textAfterPos) ||
      after == LT_DIGIT
        && !nsCRT::IsAsciiDigit(textAfterPos) ||
      after == LT_DELIMITER
        &&
        (
          nsCRT::IsAsciiAlpha(textAfterPos) ||
          nsCRT::IsAsciiDigit(textAfterPos) ||
          textAfterPos == *rep
        ) ||
        !(before == LT_IGNORE ? !nsCRT::strncasecmp(aInString, rep, aRepLen) :
          !nsCRT::strncasecmp(aInString + 1, rep, aRepLen))
    )
    return PR_FALSE;

  return PR_TRUE;
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
PRBool
mozTXTToHTMLConv::StructPhraseHit(const PRUnichar * aInString, PRInt32 aInStringLength, PRBool col0,
     const PRUnichar* tagTXT, PRInt32 aTagTXTLen, 
     const char* tagHTML, const char* attributeHTML,
     nsString& aOutString, PRUint32& openTags)
{
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existance of a pair could be verified
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
    aOutString.Append(NS_LITERAL_STRING("<"));
    aOutString.AppendWithConversion(tagHTML);
    aOutString.Append(PRUnichar(' '));
    aOutString.AppendWithConversion(attributeHTML);
    aOutString.Append(NS_LITERAL_STRING("><span class=\"moz-txt-tag\">"));
    aOutString.Append(tagTXT);
    aOutString.Append(NS_LITERAL_STRING("</span>"));
    return PR_TRUE;
  }

  // closing tag
  else if (openTags > 0
       && ItMatchesDelimited(aInString, aInStringLength, tagTXT, aTagTXTLen, LT_ALPHA, LT_DELIMITER))
  {
    openTags--;
    aOutString.Append(NS_LITERAL_STRING("<span class=\"moz-txt-tag\">"));
    aOutString.Append(tagTXT);
    aOutString.Append(NS_LITERAL_STRING("</span></"));
    aOutString.AppendWithConversion(tagHTML);
    aOutString.Append(PRUnichar('>'));
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
mozTXTToHTMLConv::SmilyHit(const PRUnichar * aInString, PRInt32 aLength, PRBool col0,
         const PRUnichar* tagTXT, PRInt32 aTagTxtLength, const char* tagHTML,
         nsString& outputHTML, PRInt32& glyphTextLen)
{
  PRInt32  tagLen = nsCRT::strlen(tagTXT);

  PRUint32 delim = (col0 ? 0 : 1) + tagLen;
  if
    (
      (col0 || nsCRT::IsAsciiSpace(aInString[0]))
        &&
        (
          aLength <= PRInt32(delim) ||
          nsCRT::IsAsciiSpace(aInString[delim]) ||
          aLength > PRInt32(delim + 1)
            &&
            (
              aInString[delim] == '.' ||
              aInString[delim] == ',' ||
              aInString[delim] == ';' ||
              aInString[delim] == '!' ||
              aInString[delim] == '?'
            )
            && nsCRT::IsAsciiSpace(aInString[delim + 1])
        )
        && ItMatchesDelimited(aInString, aLength, tagTXT, aTagTxtLength, 
                              col0 ? LT_IGNORE : LT_DELIMITER, LT_IGNORE)
	        // Note: tests at different pos for LT_IGNORE and LT_DELIMITER
    )
  {
    if (col0)
    {
      outputHTML.AssignWithConversion(tagHTML);
    }
    else
    {
      outputHTML.Truncate();
      outputHTML.AppendWithConversion(' ');
      outputHTML.AppendWithConversion(tagHTML);
    }
    glyphTextLen = (col0 ? 0 : 1) + tagLen;
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

// the glyph is appended to aOutputString instead of the original string...
PRBool
mozTXTToHTMLConv::GlyphHit(const PRUnichar * aInString, PRInt32 aInLength, PRBool col0,
         nsString& aOutputString, PRInt32& glyphTextLen)
{
  MOZ_TIMER_START(mGlyphHitTimer);

  PRUnichar text0 = aInString[0]; 
  PRUnichar text1 = aInString[1];

  // temporary variable used to store the glyph html text
  nsAutoString outputHTML;

  if
    (
      (  // Performance increase
        (col0 ? text0 : text1) == ':' ||
        (col0 ? text0 : text1) == ';'
      )
      &&
        (
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-)").get(),  3,
                   "<img src=\"chrome://editor/content/images/smile_n.gif\" alt=\":-)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)       ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":)").get(),   2,
                   "<img src=\"chrome://editor/content/images/smile_n.gif\" alt=\":)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-D").get(),  3,
                   "<img src=\"chrome://editor/content/images/laughing_n.gif\" alt=\":-D\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)    ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-(").get(),  3,
                   "<img src=\"chrome://editor/content/images/frown_n.gif\" alt=\":-(\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)       ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":(").get(),   2,
                   "<img src=\"chrome://editor/content/images/frown_n.gif\" alt=\":(\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-[").get(),  3,
                   "<img src=\"chrome://editor/content/images/embarrassed_n.gif\" alt=\":-[\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen) ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(";-)").get(),  3,
                   "<img src=\"chrome://editor/content/images/wink_n.gif\" alt=\";-)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(";)").get(),   2,
                   "<img src=\"chrome://editor/content/images/wink_n.gif\" alt=\";)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)         ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-\\").get(), 3,
                   "<img src=\"chrome://editor/content/images/undecided_n.gif\" alt=\":-\\\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)  ||
          
          SmilyHit(aInString, aInLength, col0,
                   NS_LITERAL_STRING(":-P").get(),  3,
                   "<img src=\"chrome://editor/content/images/tongue_n.gif\" alt=\":-P\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)
          

        )
    )
  {
    aOutputString.Append(outputHTML);
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  if   // XXX Hotfix
    (
      (  // Performance increase
        col0
          &&
          (
            text1 == ':' ||
            text1 == ';'
          )
      )
      &&
        (
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-)").get(),  3,
                   "<img src=\"chrome://editor/content/images/smile_n.gif\" alt=\":-)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)       ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":)").get(),   2,
                   "<img src=\"chrome://editor/content/images/smile_n.gif\" alt=\":)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-D").get(),  3,
                   "<img src=\"chrome://editor/content/images/laughing_n.gif\" alt=\":-D\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)    ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-(").get(),  3,
                   "<img src=\"chrome://editor/content/images/frown_n.gif\" alt=\":-(\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)       ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":(").get(),   2,
                   "<img src=\"chrome://editor/content/images/frown_n.gif\" alt=\":(\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-[").get(),  3,
                   "<img src=\"chrome://editor/content/images/embarrassed_n.gif\" alt=\":-[\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen) ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(";-)").get(),  3,
                   "<img src=\"chrome://editor/content/images/wink_n.gif\" alt=\";-)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)        ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(";)").get(),   2,
                   "<img src=\"chrome://editor/content/images/wink_n.gif\" alt=\";)\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)         ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-\\").get(), 3,
                   "<img src=\"chrome://editor/content/images/undecided_n.gif\" alt=\":-\\\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)  ||
          
          SmilyHit(aInString, aInLength, PR_FALSE,
                   NS_LITERAL_STRING(":-P").get(),  3,
                   "<img src=\"chrome://editor/content/images/tongue_n.gif\" alt=\":-P\" class=\"moz-txt-smily\" height=19 width=19 align=ABSCENTER>",
                   outputHTML, glyphTextLen)
          

        )
    )
  {
    aOutputString.Append(outputHTML);
    MOZ_TIMER_STOP(mGlyphHitTimer);    
    return PR_TRUE;
  }
  if (text0 == '+' || text1 == '+')
  {
    if (ItMatchesDelimited(aInString, aInLength,
                           NS_LITERAL_STRING(" +/-").get(), 4,
                           LT_IGNORE, LT_IGNORE))
    {
      aOutputString.AppendWithConversion(" &plusmn;");
      glyphTextLen = 4;
      MOZ_TIMER_STOP(mGlyphHitTimer);
      return PR_TRUE;
    }
    if (col0 && ItMatchesDelimited(aInString, aInLength,
                                   NS_LITERAL_STRING("+/-").get(), 3,
                                   LT_IGNORE, LT_IGNORE))
    {
      aOutputString.AppendWithConversion("&plusmn;");
      glyphTextLen = 3;
      MOZ_TIMER_STOP(mGlyphHitTimer);
      return PR_TRUE;
    }
  }

  if    // x^2 -> sup
    (
      text1 == '^' // Performance increase
        &&
        (
          ItMatchesDelimited(aInString, aInLength,
                             NS_LITERAL_STRING("^").get(), 1,
                             LT_DIGIT, LT_DIGIT) ||
          ItMatchesDelimited(aInString, aInLength,
                             NS_LITERAL_STRING("^").get(), 1,
                             LT_ALPHA, LT_DIGIT) ||
          ItMatchesDelimited(&aInString[1], aInLength - 1,
                             NS_LITERAL_STRING("^").get(), 1,
                             LT_IGNORE, LT_DIGIT)
            && text0 == ')'
        )
    )
  {
    // Find first non-digit
    PRInt32 delimPos = 3;  // 3 = Position after first digit after "^"
    for (; delimPos < aInLength &&
         nsCRT::IsAsciiDigit(aInString[PRUint32(delimPos)]); delimPos++)
      ;
    // Note: (delimPos == text.Length()) could be true

    if (nsCRT::IsAsciiAlpha(aInString[PRUint32(delimPos)]))
    {
      MOZ_TIMER_STOP(mGlyphHitTimer);
      return PR_FALSE;
    }

    outputHTML.Truncate();
    outputHTML += text0;
    outputHTML.AppendWithConversion("<sup class=\"moz-txt-sup\">");

    aOutputString.Append(outputHTML);
    aOutputString.Append(&aInString[2], delimPos - 2);
    aOutputString.AppendWithConversion("</sup>");

    glyphTextLen = delimPos /* - 1 + 1 */ ;
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
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
  MOZ_TIMER_STOP(mGlyphHitTimer);
  return PR_FALSE;
}

/***************************************************************************
  Library-internal Interface
****************************************************************************/

mozTXTToHTMLConv::mozTXTToHTMLConv()
{
  NS_INIT_ISUPPORTS();
  MOZ_TIMER_RESET(mScanTXTTimer);
  MOZ_TIMER_RESET(mGlyphHitTimer);
  MOZ_TIMER_RESET(mTotalMimeTime);
  MOZ_TIMER_START(mTotalMimeTime);
}

mozTXTToHTMLConv::~mozTXTToHTMLConv() 
{
  MOZ_TIMER_STOP(mTotalMimeTime);
  MOZ_TIMER_DEBUGLOG(("MIME Total Processing Time: "));
  MOZ_TIMER_PRINT(mTotalMimeTime);
  
  MOZ_TIMER_DEBUGLOG(("mozTXTToHTMLConv::ScanTXT(): "));
  MOZ_TIMER_PRINT(mScanTXTTimer);

  MOZ_TIMER_DEBUGLOG(("mozTXTToHTMLConv::GlyphHit(): "));
  MOZ_TIMER_PRINT(mGlyphHitTimer);
}

NS_IMPL_ISUPPORTS1(mozTXTToHTMLConv, mozTXTToHTMLConv)

PRInt32
mozTXTToHTMLConv::CiteLevelTXT(const PRUnichar *line,
				    PRUint32& logLineStart)
{
  PRInt32 result = 0;
  PRInt32 lineLength = nsCRT::strlen(line);

  PRBool moreCites = PR_TRUE;
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
    for (; PRInt32(i) < lineLength && nsCRT::IsAsciiSpace(line[i]); i++)
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
      if (!nsCRT::strncasecmp(indexString, NS_LITERAL_STRING(">From ").get(),
                              MinInt(6, nsCRT::strlen(indexString))))
                              //XXX RFC2646
        moreCites = PR_FALSE;
      else
      {
        result++;
        logLineStart = i;
      }
    }
    else
      moreCites = PR_FALSE;
  }

  return result;
}

void
mozTXTToHTMLConv::ScanTXT(const PRUnichar * aInString, PRInt32 aInStringLength, PRUint32 whattodo, nsString& aOutString)
{
  PRBool doURLs = whattodo & kURLs;
  PRBool doGlyphSubstitution = whattodo & kGlyphSubstitution;
  PRBool doStructPhrase = whattodo & kStructPhrase;

  MOZ_TIMER_START(mScanTXTTimer);

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
      EscapeChar(aInString[i], aOutString);
      i++;
      break;
    // Normal characters
    default:
      aOutString += aInString[i];
      i++;
      break;
    }
  }

  MOZ_TIMER_STOP(mScanTXTTimer);
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
  /* Skip all tags ("<[...]>") and content in an a tag ("<a[...]</a>").
     Unescape the rest (text between tags) and pass it to ScanTXT. */
  for (PRInt32 i = 0; PRUint32(i) < lengthOfInString;)
  {
    if (aInString[i] == '<')  // html tag
    {
      PRUint32 start = PRUint32(i);
      if (nsCRT::ToLower((char)aInString[PRUint32(i) + 1]) == 'a')
           // if a tag, skip until </a>
      {
        i = aInString.Find("</a>", PR_TRUE, i);
        if (i == kNotFound)
          i = lengthOfInString;
        else
          i += 4;
      }
      else  // just skip tag (attributes etc.)
      {
        i = aInString.FindChar('>', PR_FALSE, i);
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
      i = aInString.FindChar('<', PR_FALSE, i);
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
                             const PRUnichar *aFromType,
                             const PRUnichar *aToType,
                             nsISupports *aCtxt, nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::AsyncConvertData(const PRUnichar *aFromType,
                                      const PRUnichar *aToType,
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
  PRInt32 inLength = nsCRT::strlen(text);
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
    NS_PRECONDITION(aConv != nsnull, "null ptr");
    if (!aConv)
      return NS_ERROR_NULL_POINTER;

    *aConv = new mozTXTToHTMLConv();
    if (!*aConv)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aConv);
    //    return (*aConv)->Init();
    return NS_OK;
}
