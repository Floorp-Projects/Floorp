/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -
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
 * The Original Code is the Text to HTML converter code.
 * 
 * The Initial Developer of the Original Code is Ben Bucksch
 * <http://www.bucksch.org>. Portions created by Ben Bucksch are Copyright
 * (C) 1999 Ben Bucksch. All Rights Reserved.
 * 
 * Contributor(s):
 */

#include "mozTXTToHTMLConv.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"

nsAutoString
mozTXTToHTMLConv::EscapeChar(const PRUnichar ch)
{
    switch (ch)
    {
    case '<':
      return "&lt;";
    case '>':
      return "&gt;";
    case '&':
      return "&amp;";
    default:
      return ch;
    }
}

nsAutoString
mozTXTToHTMLConv::EscapeStr(const nsAutoString& aString)
{
  nsAutoString result;
  for (PRUint32 i = 0; PRInt32(i) < aString.Length(); i++)
    result += EscapeChar(aString[i]);
  return result;
}

nsAutoString
mozTXTToHTMLConv::UnescapeStr(const nsAutoString& aString)
{
  nsAutoString result;
  for (PRUint32 i = 0; PRInt32(i) < aString.Length();)
  {
    if (aString[i] == '&')
    {
      nsAutoString temp;
      if (aString.Mid(temp, i, 4), temp == "&lt;")
      {
        result += '<';
        i += 4;
      }
      else if (aString.Mid(temp, i, 4), temp == "&gt;")
      {
        result += '>';
        i += 4;
      }
      else if (aString.Mid(temp, i, 5), temp == "&amp;")
      {
        result += '&';
        i += 5;
      }
      else
      {
        result += aString[i];
        i++;
      }
    }
    else
    {
      result += aString[i];
      i++;
    }
  }
  return result;
}

nsAutoString
mozTXTToHTMLConv::CompleteAbbreviatedURL(const nsAutoString& text,
                                         const PRUint32 pos)
{
  nsAutoString result;
  if (text[pos] == '@')
  {
    result = "mailto:";
    result += text;
  }
  else if (text[pos] == '.')
  {
    if (ItMatchesDelimited(text, "www.", LT_IGNORE, LT_IGNORE))
    {
      result = "http://";
      result += text;
    }
    else if (ItMatchesDelimited(text, "ftp.", LT_IGNORE, LT_IGNORE))
    { 
      result = "ftp://";
      result += text;
    }
  }
  return result;
}

PRBool
mozTXTToHTMLConv::FindURLStart(const nsAutoString& text, const PRUint32 pos,
            	               const modetype check, PRUint32& start)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  {
    nsAutoString temp;
    text.Mid(temp, MaxInt(pos - 4, 0), 5);
    if (temp == "<URL:")
    {
      start = pos + 1;
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case RFC2396E:
  {
    PRInt32 i = pos - 1;
    for (; i >= 0
             && text[PRUint32(i)] != '<'
             && text[PRUint32(i)] != '"'
             && text[PRUint32(i)] != '>'
         ; i--)
      ;
    if (i >= 0 && (text[PRUint32(i)] == '<' || text[PRUint32(i)] == '"'))
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
         nsString::IsAlpha(text[PRUint32(i)]) ||
         nsString::IsDigit(text[PRUint32(i)]) ||
         text[PRUint32(i)] == '+' ||
         text[PRUint32(i)] == '-' ||
         text[PRUint32(i)] == '.'
         ); i--)
      ;
    if (nsString::IsAlpha(text[PRUint32(++i)]))
    {
      start = PRUint32(i);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case abbreviated:
  {
    PRInt32 i = pos + 1;
    for (; i >= 0
             && text[PRUint32(i)] != '>' && text[PRUint32(i)] != '<'
             && text[PRUint32(i)] != '"' && text[PRUint32(i)] != '\\'
             && text[PRUint32(i)] != '`' && text[PRUint32(i)] != '}'
             && text[PRUint32(i)] != ']' && text[PRUint32(i)] != ')'
             && text[PRUint32(i)] != '|'
             && !nsString::IsSpace(text[PRUint32(i)])
         ; i--)
      ;
    if (PRUint32(++i) != pos)
    {
      start = i;
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
mozTXTToHTMLConv::FindURLEnd(const nsAutoString& text, const PRUint32 pos,
           const modetype check, const PRUint32 start, PRUint32& end)
{
  switch(check)
  { // no breaks, because end of blocks is never reached
  case RFC1738:
  case RFC2396E:
  {
    PRUint32 i = pos + 1;
    for (; PRInt32(i) < text.Length()
             && text[i] != '>'
             && text[i] != '"'
             && text[i] != '<'
           ; i++)
      ;
    if (text[i] == (check == RFC1738 || text[start - 1] == '<' ? '>' : '"')
        && --i != pos)
    {
      end = i;
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  case freetext:
  case abbreviated:
  {
    PRUint32 i = pos + 1;
    for (; PRInt32(i) < text.Length()
             && text[i] != '>' && text[i] != '<'
             && text[i] != '"' && text[i] != '\''
             && text[i] != '`' && text[i] != '}'
             && text[i] != ']' && text[i] != ')'
             && text[i] != '|'
             && !nsString::IsSpace(text[i])
         ; i++)
      ;
    while (--i > pos && (
             text[i] == '.' || text[i] == ',' || text[i] == ';' ||
             text[i] == '!' || text[i] == '?' || text[i] == '-'
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
mozTXTToHTMLConv::CalculateURLBoundaries(const nsAutoString& text,
     const PRUint32 pos, const PRUint32 whathasbeendone,
     const modetype check, const PRUint32 start, const PRUint32 end,
     nsAutoString& txtURL, nsAutoString& desc,
     PRInt32& replaceBefore, PRInt32& replaceAfter)
{
  PRUint32 descstart;

  switch(check)
  {
  case RFC1738:
  {
    descstart = start - 5;
    text.Mid(desc, descstart, end - descstart + 2); // include "<URL:" and ">"
    replaceAfter = end - pos + 1;
  } break;
  case RFC2396E:
  {
    descstart = start - 1;
    text.Mid(desc, descstart, end - descstart + 2); // include brackets
    replaceAfter = end - pos + 1;
  } break;
  case freetext:
  case abbreviated:
  {
    descstart = start;
    text.Mid(desc, descstart, end - start + 1);   // don't include brackets
    replaceAfter = end - pos;
  } break;
  default: break;
  } //switch

  desc = EscapeStr(desc);

  text.Mid(txtURL, start, end - start + 1);
  txtURL.StripWhitespace();

  nsAutoString temp;
  text.Mid(temp, descstart, pos - descstart);
  replaceBefore = ScanTXT(temp, ~kURLs /*prevents loop*/
    & whathasbeendone).Length();

  return;
}

PRBool
mozTXTToHTMLConv::CheckURLAndCreateHTML(
     const nsAutoString& txtURL, const nsAutoString& desc,
     nsAutoString& outputHTML)
{
  // Create *uri from txtURL
  nsIURI* uri;
  nsresult rv;
  static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
  NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
  if (NS_FAILED(rv))
    return PR_FALSE;
  char* specStr = txtURL.ToNewCString(); //I18N this forces a single byte char
  if (specStr == nsnull)
    return PR_FALSE;
  rv = serv->NewURI(specStr, nsnull, &uri);
  Recycle(specStr);

  // Real work
  if (NS_SUCCEEDED(rv) && uri)
  {
    //PRUnichar* validURL;
    //uri->ToString(&validURL);

    outputHTML = "<a href=\"";
    //outputHTML += validURL;
    outputHTML += txtURL;
    outputHTML += "\">";
    outputHTML += desc;
    outputHTML += "</a>";
    //Recycle(validURL);
    NS_RELEASE(uri);
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

PRBool
mozTXTToHTMLConv::FindURL(const nsAutoString& text, const PRUint32 pos,
     const PRUint32 whathasbeendone,
     nsAutoString& outputHTML, PRInt32& replaceBefore, PRInt32& replaceAfter)
{
  enum statetype {unchecked, invalid, startok, endok, success};
  const modetype ranking[mozTXTToHTMLConv_numberOfModes] =
                      {RFC1738, RFC2396E, freetext, abbreviated};

  statetype state[mozTXTToHTMLConv_lastMode + 1]; // 0(=unknown)..lastMode
  /* I don't like this abuse of enums as index for the array,
     but I don't know a better method */

  // Define, which modes to check
  /* all modes but abbreviated are checked for text[pos] == ':',
     only abbreviated for '.', RFC2396E and abbreviated for '@' */
  for (modetype iState = unknown; iState <= mozTXTToHTMLConv_lastMode;
       iState = modetype(iState + 1))
    state[iState] = text[pos] == ':' ? unchecked : invalid;
  switch (text[pos])
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
      if (FindURLStart(text, pos, check, start))
        state[check] = startok;

    if (state[check] == startok)
      if (FindURLEnd(text, pos, check, start, end))
        state[check] = endok;

    if (state[check] == endok)
    {
      nsAutoString txtURL, desc;
      PRInt32 resultReplaceBefore, resultReplaceAfter;

      CalculateURLBoundaries(text, pos, whathasbeendone, check, start, end,
                             txtURL, desc,
                             resultReplaceBefore, resultReplaceAfter);

      nsAutoString temp;
      temp = CompleteAbbreviatedURL(txtURL, pos - start);
      if (!temp.IsEmpty())
        txtURL = temp;

      if (CheckURLAndCreateHTML(txtURL, desc, outputHTML))
      {
        replaceBefore = resultReplaceBefore;
        replaceAfter = resultReplaceAfter;
        state[check] = success;
      }
    } // if
  } // for
  return state[check] == success;
}

nsAutoString
mozTXTToHTMLConv::Right(const nsAutoString& text, PRUint32 start)
{
  MOZ_TIMER_START(mRightTimer);

  nsAutoString result;
  text.Right(result, text.Length() - start);

  MOZ_TIMER_STOP(mRightTimer);
  return result;
}

PRBool
mozTXTToHTMLConv::ItMatchesDelimited(const nsAutoString& text,
    const char* rep, LIMTYPE before, LIMTYPE after)
{
  PRInt32 repLen = rep ? nsCRT::strlen(rep) : 0;
  
  if
    (
      (before == LT_IGNORE && (after == LT_IGNORE || after == LT_DELIMITER))
        && text.Length() < repLen ||
      (before != LT_IGNORE || after != LT_IGNORE && after != LT_DELIMITER)
        && text.Length() < repLen + 1 ||
      before != LT_IGNORE && after != LT_IGNORE && after != LT_DELIMITER
        && text.Length() < repLen + 2
    )
    return PR_FALSE;

    PRUint32 afterPos = repLen + (before == LT_IGNORE ? 0 : 1);

  if
    (
      before == LT_ALPHA
        && !nsString::IsAlpha(text.First()) ||
      before == LT_DIGIT
        && !nsString::IsDigit(text.First()) ||
      before == LT_DELIMITER
        &&
        (
          nsString::IsAlpha(text.First()) ||
          nsString::IsDigit(text.First()) ||
          text.First() == *rep
        ) ||
      after == LT_ALPHA
        && !nsString::IsAlpha(text[afterPos]) ||
      after == LT_DIGIT
        && !nsString::IsDigit(text[afterPos]) ||
      after == LT_DELIMITER
        &&
        (
          nsString::IsAlpha(text[afterPos]) ||
          nsString::IsDigit(text[afterPos]) ||
          text[afterPos] == *rep
        ) ||
      !(before == LT_IGNORE ? text : Right(text, 1)).Equals(rep,
           PR_TRUE, repLen) //  XXX bug #21071 
/*      !Equals((before == LT_IGNORE ? text : Right(text, 1)), rep,
           PR_TRUE, rep.Length())*/
    )
    return PR_FALSE;

  return PR_TRUE;
}

PRUint32
mozTXTToHTMLConv::NumberOfMatches(const nsAutoString& text,
     const char* rep, LIMTYPE before, LIMTYPE after)
{
  PRInt32 result = 0;
  for (PRInt32 i = 0; i < text.Length(); i++)
    if (ItMatchesDelimited(Right(text, i), rep, before, after))
      result++;
  return result;
}

PRBool
mozTXTToHTMLConv::StructPhraseHit(const nsAutoString& text, PRBool col0,
     const char* tagTXT,
     const char* tagHTML, const char* attributeHTML,
     nsAutoString& outputHTML, PRUint32& openTags)
{
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existance of a pair could be verified
     We use the first opening/closing tag, if we can choose */

  // opening tag
  if
    (
      ItMatchesDelimited(text, tagTXT,
           (col0 ? LT_IGNORE : LT_DELIMITER), LT_ALPHA) // opening tag
        && NumberOfMatches((col0 ? text : Right(text, 1)), tagTXT,
             LT_ALPHA, LT_DELIMITER) /* remaining closing tags */ > openTags
    )
  {
    openTags++;
    outputHTML = "<";
    outputHTML += tagHTML;
    outputHTML += ' ';
    outputHTML += attributeHTML;
    outputHTML += '>';
    outputHTML += tagTXT;
    return PR_TRUE;
  }

  // closing tag
  else if (openTags > 0
       && ItMatchesDelimited(text, tagTXT, LT_ALPHA, LT_DELIMITER))
  {
    openTags--;
    outputHTML = tagTXT;
    outputHTML += "</";
    outputHTML += tagHTML;
    outputHTML += '>';
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
mozTXTToHTMLConv::SmilyHit(const nsAutoString& text, PRBool col0,
         const char* tagTXT, const char* tagHTML,
         nsAutoString& outputHTML, PRInt32& glyphTextLen)
{
  PRInt32  tagLen = nsCRT::strlen(tagTXT);
  PRInt32  txtLen = text.Length();

  PRUint32 delim = (col0 ? 0 : 1) + tagLen;
  if
    (
      (col0 || nsString::IsSpace(text.First()))
        &&
        (
          txtLen <= PRInt32(delim) ||
          nsString::IsSpace(text[delim]) ||
          txtLen > PRInt32(delim + 1)
            &&
            (
              text[delim] == '.' ||
              text[delim] == ',' ||
              text[delim] == ';' ||
              text[delim] == '!' ||
              text[delim] == '?'
            )
            && nsString::IsSpace(text[delim + 1])
        )
        && ItMatchesDelimited(text, tagTXT,
                              col0 ? LT_IGNORE : LT_DELIMITER, LT_IGNORE)
	        // Note: tests at different pos for LT_IGNORE and LT_DELIMITER
    )
  {
    if (col0)
    {
      outputHTML = tagHTML;
    }
    else
    {
      outputHTML.Truncate();
      outputHTML += ' ';
      outputHTML += tagHTML;
    }
    glyphTextLen = (col0 ? 0 : 1) + tagLen;
    return PR_TRUE;
  }
  else
  {
    return PR_FALSE;
  }
}

PRBool
mozTXTToHTMLConv::GlyphHit(const nsAutoString& text, PRBool col0,
         nsAutoString& outputHTML, PRInt32& glyphTextLen)
{
  MOZ_TIMER_START(mGlyphHitTimer);

  if
    (
      ((col0 ? text.First() : text[1]) == ':' ||  // Performance increase
       (col0 ? text.First() : text[1]) == ';' )
	&&
        (
          SmilyHit(text, col0, ":-)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, col0, ":)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, col0, ":-(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, col0, ":(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, col0, ";-)", "<img SRC=\"chrome://messenger/skin/wink.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, col0, ";-P", "<img SRC=\"chrome://messenger/skin/sick.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen)
        )
    )
  {
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if   // XXX Hotfix
        (
          !col0    // Performance increase
            &&
            (
	            text[1] == ':' ||
	            text[1] == ';'
            )
	    &&
    	(
          SmilyHit(text, PR_FALSE, ":-)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, PR_FALSE, ":)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, PR_FALSE, ":-(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, PR_FALSE, ":(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, PR_FALSE, ";-)", "<img SRC=\"chrome://messenger/skin/wink.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
          SmilyHit(text, PR_FALSE, ";-P", "<img SRC=\"chrome://messenger/skin/sick.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen)
	    )
    )
  {
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, "(c)", LT_IGNORE, LT_DELIMITER))
       // Note: ItMatchesDelimited compares case-insensitive
  {
    outputHTML = "&copy;";
    glyphTextLen = 3;
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, "(r)", LT_IGNORE, LT_DELIMITER))
       // see above
  {
    outputHTML = "&reg;";
    glyphTextLen = 3;
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, " +/-", LT_IGNORE, LT_IGNORE))
  {
    outputHTML = " &plusmn;";
    glyphTextLen = 4;
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if (col0 && ItMatchesDelimited(text, "+/-", LT_IGNORE, LT_IGNORE))
  {
    outputHTML = "&plusmn;";
    glyphTextLen = 3;
    MOZ_TIMER_STOP(mGlyphHitTimer);
    return PR_TRUE;
  }
  else if    // x^2 -> sup
    (
      text[1] == '^' // Performance increase
        &&
        (
          ItMatchesDelimited(text, "^", LT_DIGIT, LT_DIGIT) ||
          ItMatchesDelimited(text, "^", LT_ALPHA, LT_DIGIT) ||
          ItMatchesDelimited(Right(text, 1), "^", LT_IGNORE, LT_DIGIT)
            && text.First() == ')'
        )
    )
  {
    // Find first non-digit
    PRInt32 delimPos = 3;  // 3 = Position after first digit after "^"
    for (; delimPos < text.Length() &&
         nsString::IsDigit(text[PRUint32(delimPos)]); delimPos++)
      ;
    // Note: (delimPos == text.Length()) could be true

    if (nsString::IsAlpha(text[PRUint32(delimPos)]))
    {
      MOZ_TIMER_STOP(mGlyphHitTimer);
      return PR_FALSE;
    }

    outputHTML.Truncate();
    outputHTML += text.First();
    outputHTML += "<sup>";
    nsAutoString temp;
    if (text.Mid(temp, 2, delimPos - 2) != PRUint32(delimPos - 2))
    {
      MOZ_TIMER_STOP(mGlyphHitTimer);
      return PR_FALSE;
    }
    outputHTML += temp;
    outputHTML += "</sup>";
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
  MOZ_TIMER_RESET(mRightTimer);
  MOZ_TIMER_RESET(mTotalMimeTime);
  MOZ_TIMER_START(mTotalMimeTime);
}

mozTXTToHTMLConv::~mozTXTToHTMLConv() 
{
  MOZ_TIMER_START(mTotalMimeTime);
  MOZ_TIMER_DEBUGLOG(("MIME Total Processing Time: "));
  MOZ_TIMER_PRINT(mTotalMimeTime);
  
  MOZ_TIMER_DEBUGLOG(("mozTXTToHTMLConv::ScanTXT(): "));
  MOZ_TIMER_PRINT(mScanTXTTimer);

  MOZ_TIMER_DEBUGLOG(("mozTXTToHTMLConv::GlyphHit(): "));
  MOZ_TIMER_PRINT(mGlyphHitTimer);

  MOZ_TIMER_DEBUGLOG(("mozTXTToHTMLConv::Right(): "));
  MOZ_TIMER_PRINT(mRightTimer);
}

NS_IMPL_ISUPPORTS(mozTXTToHTMLConv, NS_GET_IID(mozTXTToHTMLConv));

PRInt32
mozTXTToHTMLConv::CiteLevelTXT(const nsAutoString& line,
				    PRUint32& logLineStart)
{
  PRInt32 result = 0;

  PRBool moreCites = PR_TRUE;
  while (moreCites)
  {
    /* E.g. the following counts as quote:

       >text
       > text
       ] text
           > text
       USER> text
       user] text

       logLineStart is the position of "t" in this example
    */
    PRUint32 i = logLineStart;
    for (; PRInt32(i) < line.Length() && nsString::IsSpace(line[i]); i++)
      ;
    for (; PRInt32(i) < line.Length() && nsString::IsAlpha(line[i]); i++)
      ;
    if (line[i] == '>' || line[i] == ']')
    {
      // Sendmail
      nsAutoString temp;
      line.Mid(temp, logLineStart, 6);
      if (temp == ">From ")      //XXX RFC2646
        moreCites = PR_FALSE;
      else
      {
        result++;
        logLineStart = i + 1;
      }
    }
    else
      moreCites = PR_FALSE;
  }

  return result;
}

nsAutoString
mozTXTToHTMLConv::ScanTXT(const nsAutoString& text, PRUint32 whattodo)
{
  PRBool doURLs = whattodo & kURLs;
  PRBool doGlyphSubstitution = whattodo & kGlyphSubstitution;
  PRBool doStructPhrase = whattodo & kStructPhrase;

#ifdef DEBUG_BenB
printf("ScanTXT orginal: ");
printf(text.ToNewCString());
#endif

  MOZ_TIMER_START(mScanTXTTimer);

  nsAutoString result;

  PRUint32 structPhrase_strong = 0;  // Number of currently open tags
  PRUint32 structPhrase_underline = 0;
  PRUint32 structPhrase_italic = 0;
  PRUint32 structPhrase_code = 0;

  for(PRUint32 i = 0; PRInt32(i) < text.Length();)
  {
    if (doGlyphSubstitution)
    {
      PRInt32 glyphTextLen;
      nsAutoString glyphHTML;
      if (GlyphHit(Right(text, i), i == 0, glyphHTML, glyphTextLen))
      {
        result += glyphHTML;
        i += glyphTextLen;
        continue;
      }
    }

    if (doStructPhrase)
    {
      nsAutoString HTMLnsStr;
      switch (text[i]) // Performance increase
      {
      case '*':
      case '_':
      case '/':
      case '|':
        if
	  (
            StructPhraseHit(i == 0 ? text : Right(text, i - 1), i == 0,
                 "*", "strong", "class=txt_star",
                 HTMLnsStr, structPhrase_strong) ||
            StructPhraseHit(i == 0 ? text : Right(text, i - 1), i == 0,
                 "_", "em" /* <u> is deprecated */, "class=txt_underscore",
                 HTMLnsStr, structPhrase_underline) ||
            StructPhraseHit(i == 0 ? text : Right(text, i - 1), i == 0,
                 "/", "em", "class=txt_slash",
                 HTMLnsStr, structPhrase_italic) ||
            StructPhraseHit(i == 0 ? text : Right(text, i - 1), i == 0,
                 "|", "code", "class=txt_verticalline",
                 HTMLnsStr, structPhrase_code)
	  )
        {
          result += HTMLnsStr;
          i++;
          continue;
        }
      }
    }

    if (doURLs)
    {
      switch (text[i])
      {
      case ':':
      case '@':
      case '.':
        if (text[i - 1] != ' ' && text[i + 1] != ' ') // Preformance increase
        {
          nsAutoString outputHTML;
          PRInt32 replaceBefore;
          PRInt32 replaceAfter;
          if (FindURL(text, i, whattodo,
                      outputHTML, replaceBefore, replaceAfter)
                  && !(text[i] == '@' && (   // workaround for bug #19445
                    structPhrase_strong + structPhrase_italic +   // dito
                    structPhrase_underline + structPhrase_code != 0  )))
          {
            nsAutoString temp;
            result.Left(temp, result.Length() - replaceBefore);
            result = temp;
            result += outputHTML;
            i += replaceAfter + 1;
            continue;
          }
        }
        break;
      } //switch
    }

    switch (text[i])
    {
    // Special symbols
    case '<':
    case '>':
    case '&':
      result += EscapeChar(text[i]);
      i++;
      break;
    // Normal characters
    default:
      result += text[i];
      i++;
    }
  }

#ifdef DEBUG_BenB
printf("ScanTXT result:  ");
printf(result.ToNewCString());
printf("\n");
#endif

  MOZ_TIMER_STOP(mScanTXTTimer);

  return result;
}

nsAutoString
mozTXTToHTMLConv::ScanHTML(const nsAutoString& text, PRUint32 whattodo)
{
  nsAutoString result;

#ifdef DEBUG_BenB
printf("ScanHTML orginal: ");
printf(text.ToNewCString());
printf("\n");
#endif

  // Look for simple entities not included in a tags and scan them.
  /* Skip all tags ("<[...]>") and content in an a tag ("<a[...]</a>").
     Unescape the rest (text between tags) and pass it to ScanTXT. */
  for (PRUint32 i = 0; PRInt32(i) < text.Length();)
  {
    if (text[i] == '<')  // html tag
    {
      PRUint32 start = i;
      nsAutoString temp;
      if (nsCRT::ToLower(text[i + 1]) == 'a')  // if a tag, skip until </a>
      {
        for (; PRInt32(i + 3) < text.Length()
        	 && (text.Mid(temp, i, 4), temp.ToLowerCase(), temp != "</a>")
             ; i++)
          ;
        i += 4;
      }
      else  // just skip tag (attributes etc.)
      {
        for (; PRInt32(i) < text.Length() && text[i] != '>'; i++)
          ;
        i++;
      }
      text.Mid(temp, start, i - start);  // i is one char after the tag
      result += temp;
    }
    else
    {
      PRUint32 start = i;
      for (; PRInt32(i) < text.Length() && text[i] != '<'; i++)
        ;
      nsAutoString temp;
      text.Mid(temp, start, i - start);  // i is first char of the tag
      result += ScanTXT(UnescapeStr(temp), whattodo);
    }
  }

#ifdef DEBUG_BenB
printf("ScanHTML result:  ");
printf(result.ToNewCString());
printf("\n");
#endif

  return result;
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
mozTXTToHTMLConv::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                                     nsIInputStream *inStr, PRUint32 sourceOffset,
                                     PRUint32 count)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozTXTToHTMLConv::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                nsresult status, const PRUnichar *errorMsg)
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
  if (!_retval || !text)
    return NS_ERROR_NULL_POINTER;
  *_retval = ScanTXT(text, whattodo).ToNewUnicode();
  return _retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
mozTXTToHTMLConv::ScanHTML(const PRUnichar *text, PRUint32 whattodo,
			    PRUnichar **_retval)
{
  if (!_retval || !text)
    return NS_ERROR_NULL_POINTER;
  *_retval = ScanHTML(text, whattodo).ToNewUnicode();
  return _retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


/**************************************************************************
  Global functions
***************************************************************************/
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
