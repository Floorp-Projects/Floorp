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

mozTXTToHTMLConv::mozTXTToHTMLConv()
{
  NS_INIT_ISUPPORTS();
}

mozTXTToHTMLConv::~mozTXTToHTMLConv() {}
NS_IMPL_ISUPPORTS(mozTXTToHTMLConv, NS_GET_IID(mozTXTToHTMLConv));

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
    if (text.Equals("www.", PR_FALSE, 4))
    {
      result = "http://";
      result += text;
    }
    else if (text.Equals("ftp.", PR_FALSE, 4))
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
  if (NS_FAILED(rv)) return PR_FALSE;
  char* specStr = txtURL.ToNewCString(); //XXX this forces a single byte char
  if (NS_FAILED(rv) || specStr == nsnull)
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
  const modetype ranking[numberOfModes] =
                      {RFC1738, RFC2396E, freetext, abbreviated};

  statetype state[lastMode + 1]; // 0(=unknown)..lastMode
  /* I don't like this abuse of enums as index for the array,
     but I don't know a better method */

  // Define, which modes to check
  /* all modes but abbreviated are checked for text[pos] == ':',
     only abbreviated for '.', RFC2396E and abbreviated for '@' */
  for (modetype iState = unknown; iState <= lastMode;
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
  PRUint32 iCheck = 0;  // the currently tested modetype
  modetype check = ranking[iCheck];
  for (; iCheck < numberOfModes && state[check] != success; iCheck++)
      /* check state from last run.
         If this is the first, check this one, which is never success */
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
      { // Localize temp
        nsAutoString temp;
        temp = CompleteAbbreviatedURL(txtURL, pos - start);
        if (!temp.IsEmpty())
          txtURL = temp;
      }

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
  nsAutoString result;
  text.Right(result, text.Length() - start);
  return result;
}

PRBool
mozTXTToHTMLConv::ItMatchesDelimited(const nsAutoString& text,
    const nsAutoString& rep, LIMTYPE before, LIMTYPE after)
{
  if
    (
      text.IsEmpty() ||
      rep.IsEmpty() ||
      before != LT_IGNORE && after != LT_IGNORE && after != LT_DELIMITER
        && text.Length() < 3 ||
      (before != LT_IGNORE || (after != LT_IGNORE && after != LT_DELIMITER))
        && text.Length() < 2
    )
    return PR_FALSE;

  PRUint32 afterPos = rep.Length() + (before == LT_IGNORE ? 0 : 1);
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
          text.First() == rep.First()
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
          text[afterPos] == rep.First()
        ) ||
      (before == LT_IGNORE ? text : Right(text, 1)).Compare(rep,
           PR_TRUE, rep.Length()) // returns true for mismatch
    )
    return PR_FALSE;

  return PR_TRUE;
}

PRUint32
mozTXTToHTMLConv::NumberOfMatches(const nsAutoString& text,
     const nsAutoString& rep, LIMTYPE before, LIMTYPE after)
{
  PRInt32 result = 0;
  for (PRInt32 i = 0; i < text.Length(); i++)
    if (ItMatchesDelimited(Right(text, i), rep, before, after))
      result++;
  return result;
}

PRBool
mozTXTToHTMLConv::StructPhraseHit(const nsAutoString& text, PRBool col0,
     const nsAutoString tagTXT,
     const nsAutoString tagHTML, const nsAutoString attributeHTML,
     nsAutoString& outputHTML, PRUint32& openTags)
{
  /* We're searching for the following pattern:
     LT_DELIMITER - "*" - ALPHA -
     [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
     <strong> is only inserted, if existance of a pair could be verified */

  // opening tag
  if (
      ItMatchesDelimited(text, tagTXT,
           (col0 ? LT_IGNORE : LT_DELIMITER), LT_ALPHA) // opening tag
        && NumberOfMatches((col0 ? text : Right(text, 1)), tagTXT,
             LT_ALPHA, LT_DELIMITER) /* remaining closing tags */ > openTags
             /* I don't special case tagTXT before end of text,
                because text hopefully includes EOL,
                which counts as LT_DELIMITER */
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
         const nsAutoString tagTXT, const nsAutoString tagHTML,
         nsAutoString& outputHTML, PRInt32& glyphTextLen)
{
  PRUint32 delim = (col0 ? 0 : 1) + tagTXT.Length();
  if  //CHANGE nsAutoString(NS_LINEBREAK).First()
    (
      (col0 || nsString::IsSpace(text.First()))
        &&
        (
          text.Length() > PRInt32(delim)
            && nsString::IsSpace(text[delim]) ||
          text.Length() > PRInt32(delim + 1)
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
        && ItMatchesDelimited((col0 ? text : Right(text, 1)), tagTXT,
                              LT_IGNORE, LT_IGNORE)
    )
  {
    if (col0)
      outputHTML = tagHTML;
    else
    {
      outputHTML.Truncate();
      outputHTML += ' ';
      outputHTML += tagHTML;
    }
    glyphTextLen = (col0 ? 0 : 1) + tagTXT.Length();
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

PRBool
mozTXTToHTMLConv::GlyphHit(const nsAutoString& text, PRBool col0,
         nsAutoString& outputHTML, PRInt32& glyphTextLen)
{
  if (col0 ? text.First() : ((text[1] == ':') || (text[1] == ';')) )  // Performance increase
  {
    if
      (
        SmilyHit(text, col0, ":-)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
        SmilyHit(text, col0, ":)", "<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
        SmilyHit(text, col0, ":-(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
        SmilyHit(text, col0, ":(", "<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
        SmilyHit(text, col0, ";-)", "<img SRC=\"chrome://messenger/skin/wink.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen) ||
        SmilyHit(text, col0, ";-P", "<img SRC=\"chrome://messenger/skin/sick.gif\" height=17 width=17 align=ABSCENTER>", outputHTML, glyphTextLen)
      )
      return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, "(c)", LT_IGNORE, LT_DELIMITER))
       // Note: ItMatchesDelimited compares case-insensitive
  {
    outputHTML = "&copy;";
    glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, "(r)", LT_IGNORE, LT_DELIMITER))
  {
    outputHTML = "&reg;";
    glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatchesDelimited(text, " +/-", LT_IGNORE, LT_IGNORE))
  {
    outputHTML = " &plusmn;";
    glyphTextLen = 4;
    return PR_TRUE;
  }
  else if (col0 && ItMatchesDelimited(text, "+/-", LT_IGNORE, LT_IGNORE))
  {
    outputHTML = "&plusmn;";
    glyphTextLen = 3;
    return PR_TRUE;
  }
  else if
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
    // Attention: (delimPos == text.Length()) could be true

    if (nsString::IsAlpha(text[PRUint32(delimPos)]))
      return PR_FALSE;

    outputHTML.Truncate();
    outputHTML += text.First();
    outputHTML += "<sup>";
    nsAutoString temp;
    if (text.Mid(temp, 2, delimPos - 2) != PRUint32(delimPos - 2))
      return PR_FALSE;
    outputHTML += temp;
    outputHTML += "</sup>";
    glyphTextLen = delimPos /* - 1 + 1 */ ;
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
  return PR_FALSE;
}

/****************************************************************************
  C++ Interface
  Use these classes from C++
****************************************************************************/

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
      switch (text[i])
      {
      // *bold* -> <strong>
      case '*':
        if (StructPhraseHit(Right(text, i == 0 ? i : i - 1), i == 0,
                 '*', "strong", "class=txt_star",
                 HTMLnsStr, structPhrase_strong))
        {
          result += HTMLnsStr;
          i++;
          continue;
        }
        break;
      // _underline_ -> <em> (<u> is deprecated)
      case '_':
        if (StructPhraseHit(Right(text, i == 0 ? i : i - 1), i == 0,
                 '_', "em", "class=txt_underscore",
                 HTMLnsStr, structPhrase_underline))
        {
          result += HTMLnsStr;
          i++;
          continue;
        }
        break;
      // /italic/ -> <em>
      case '/':
        if (StructPhraseHit(Right(text, i == 0 ? i : i - 1), i == 0,
                 '/', "em", "class=txt_slash",
                 HTMLnsStr, structPhrase_italic))
        {
          result += HTMLnsStr;
          i++;
          continue;
        }
        break;
      // |code| -> <code>
      case '|':
        if (StructPhraseHit(Right(text, i == 0 ? i : i - 1), i == 0,
                 '|', "code", "class=txt_verticalline",
                 HTMLnsStr, structPhrase_code))
        {
          result += HTMLnsStr;
          i++;
          continue;
        }
        break;
      } //switch
    } //if

    if (doURLs)
    {
      switch (text[i])
      {
      case ':':
      case '@':
      case '.':
        if (text[i + 1] != ' ') // Performance increase
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
      }
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// RICHIE: ROLL THE UGLYNESS...
// UGLY but effective - will soon dissappear when BenB is done debugging
// new code
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsIPref.h"
#include "prprf.h"
#include "msgCore.h"
#include "net.h" /* for xxx_TYPE_URL */

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

// Forwards...
nsresult  URLType(const char *URL, PRInt32  *retType);

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
mime_SACopy (char **destination, const char *source)
{
  if(*destination)
  {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source)
  {
    *destination = NULL;
  }
  else 
  {
    *destination = (char *) PR_Malloc (nsCRT::strlen(source) + 1);
    if (*destination == NULL) 
      return(NULL);
    
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
extern "C"  char *
mime_SACat (char **destination, const char *source)
{
  if (source && *source)
  {
    if (*destination)
    {
      int length = nsCRT::strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + nsCRT::strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination + length, source);
    }
    else
    {
      *destination = (char *) PR_Malloc (nsCRT::strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination, source);
    }
  }
  return *destination;
}

char *
FindAmbitiousMailToTag(const char *line, PRInt32 line_size)
{
  char  *atLoc;
  char  *workLine;

  // Sanity...
  if ( (!line) || (!*line) )
    return NULL;

  // Should I bother at all...
  if ( !(atLoc = PL_strnchr(line, '@', line_size)) )
    return NULL;

  // create a working copy...
  workLine = PL_strndup(line, line_size);
  if (!workLine)
    return NULL;

  char *ptr = PL_strchr(workLine, '@');
  if (!ptr)
    return NULL;

  *(ptr+1) = '\0';
  --ptr;
  while (ptr >= workLine)
  {
    if (nsString::IsSpace(*ptr) ||
				  *ptr == '<' || *ptr == '>' ||
          *ptr == '`' || *ptr == ')' ||
          *ptr == '\'' || *ptr == '"' ||
          *ptr == ']' || *ptr == '}'
          )
          break;
    --ptr;
  }

  ++ptr;

  if (*ptr == '@') 
  {
    PR_FREEIF(workLine);
    return NULL;
  }

  PRInt32           retType;

  URLType(ptr, &retType);
  if (retType != 0)
  {
    PR_FREEIF(workLine);
    return NULL;
  }

  char *retVal = nsCRT::strdup(ptr);
  PR_FREEIF(workLine);
  return retVal;
}

nsresult
AmbitiousURLType(const char *URL, PRInt32  *retType, const char *newURLTag)
{
  *retType = 0;

  if (!nsCRT::strncasecmp(URL, newURLTag, nsCRT::strlen(newURLTag))) 
  {
    *retType = MAILTO_TYPE_URL;
  }

  return NS_OK;
}

#define NSCP_URL  13
const char NSCP_CHK[5] = {0x41, 0x49, 0x4d, 0x3a, 0x00};

/* from libnet/mkutils.c */
nsresult 
URLType(const char *URL, PRInt32  *retType)
{
  if (!URL || (URL && *URL == '\0'))
    return(NS_ERROR_NULL_POINTER);
  
  switch(*URL) {
  case 'a':
  case 'A':
    if(!nsCRT::strncasecmp(URL,"about:security", 14))
    {
		    *retType = SECURITY_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"about:",6))
    {
		    *retType = ABOUT_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,NSCP_CHK,4))
    {
        *retType = NSCP_URL;
        return NS_OK;
    }
    break;
  case 'f':
  case 'F':
    if ( (!nsCRT::strncasecmp(URL,"ftp:",4)) ||
         (!nsCRT::strncasecmp(URL,"ftp.",4)) )
    {
		    *retType = FTP_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"file:",5))
    {
		    *retType = FILE_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'g':
  case 'G':
    if(!nsCRT::strncasecmp(URL,"gopher:",7)) 
    {
      *retType = GOPHER_TYPE_URL;
      return NS_OK;
    }
    break;
  case 'h':
  case 'H':
    if(!nsCRT::strncasecmp(URL,"http:",5))
    {
		    *retType = HTTP_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"https:",6))
    {
		    *retType = SECURE_HTTP_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'i':
  case 'I':
    if(!nsCRT::strncasecmp(URL,"internal-gopher-",16))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-news-",14))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-edit-",14))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-attachment-",20))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-dialog-handler",23))
    {
      *retType = HTML_DIALOG_HANDLER_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-panel-handler",22))
    {
      *retType = HTML_PANEL_HANDLER_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"internal-security-",18))
    {
      *retType = INTERNAL_SECLIB_TYPE_URL;
      return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"IMAP:",5))
    {
    	*retType = IMAP_TYPE_URL;
      return NS_OK;
    }
    break;
  case 'j':
  case 'J':
    if(!nsCRT::strncasecmp(URL, "javascript:",11))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'l':
  case 'L':
    if(!nsCRT::strncasecmp(URL, "livescript:",11))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'm':
  case 'M':
    if(!nsCRT::strncasecmp(URL,"mailto:",7)) 
    {
		    *retType = MAILTO_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"mailbox:",8))
    {
		    *retType = MAILBOX_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL, "mocha:",6))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'n':
  case 'N':
    if(!nsCRT::strncasecmp(URL,"news:",5))
    {
		    *retType = NEWS_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'p':
  case 'P':
    if(!nsCRT::strncasecmp(URL,"pop3:",5))
    {
		    *retType = POP3_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'r':
  case 'R':
    if(!nsCRT::strncasecmp(URL,"rlogin:",7))
    {
        *retType = RLOGIN_TYPE_URL;
        return NS_OK;
    }
    break;
  case 's':
  case 'S':
    if(!nsCRT::strncasecmp(URL,"snews:",6))
    {
		    *retType = NEWS_TYPE_URL;
        return NS_OK;
    }
  case 't':
  case 'T':
    if(!nsCRT::strncasecmp(URL,"telnet:",7))
    {
		    *retType = TELNET_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"tn3270:",7))
    {
		    *retType = TN3270_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'v':
  case 'V':
    if(!nsCRT::strncasecmp(URL, VIEW_SOURCE_URL_PREFIX, 
      sizeof(VIEW_SOURCE_URL_PREFIX)-1))
    {
		    *retType = VIEW_SOURCE_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'w':
  case 'W':
    if(!nsCRT::strncasecmp(URL,"wais:",5))
    {
		    *retType = WAIS_TYPE_URL;
        return NS_OK;
    }
    else if(!nsCRT::strncasecmp(URL,"www.",4))
    {
		    *retType = HTTP_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'u':
  case 'U':
    if(!nsCRT::strncasecmp(URL,"URN:",4))
    {
		    *retType = URN_TYPE_URL;
        return NS_OK;
    }
    break;
    
    }
    
    /* no type match :( */
    *retType = 0;
    return NS_OK;
}

PRBool
ItMatches(const char *line, PRInt32 lineLen, const char *rep)
{
  if ( (!rep) || (!*rep) || (!line) || (!*line) )
    return PR_FALSE;

  PRInt32 compLen = nsCRT::strlen(rep);

  if (lineLen < compLen)
    return PR_FALSE;

  if (!nsCRT::strncasecmp(line, rep, compLen))
    return PR_TRUE;

  return PR_FALSE;
}

enum LIMTYPE
{
  LT_IGNORE,     // limitation not checked
  LT_DELIMITER,  // not alphanumeric and not rep[0]
  LT_ALPHA       // alpha char
};

/*
  This performs poorly. But I've made a test and didn't notice a performance
  decrease between this code and no struct phrase transformation at all.
  @param text (in): the source string
  @param start (in): offset of text specifying the start of the new object
  @return a new (local) object containing the substring
 */
nsCAutoString
Right(const nsCAutoString& text, PRInt32 start)
{
  nsCAutoString result;
  text.Right(result, text.Length() - start);
  return result;
}

/*
  @param text (in): the string to search through.
         If before = IGNORE,
           rep is compared starting at 1. char of text (text[0]),
           else starting at 2. char of text (text[1]).
  @param rep (in): the string to look for
  @param before (in): limitation before rep
  @param after (in): limitation after rep
  @return true, if rep is matched and limitation spec is met or rep is empty
*/
PRBool
ItMatchesDelimited(const nsCAutoString& text, const nsAutoString& rep,
                   LIMTYPE before, LIMTYPE after)
{
  if (
      text.IsEmpty() || rep.IsEmpty() ||
      before != LT_IGNORE && after != LT_IGNORE && text.Length() < 3 ||
      (before != LT_IGNORE || after != LT_IGNORE) && text.Length() < 2
     )
    return PR_FALSE;

  PRUint32 afterPos = rep.Length() + (before == LT_IGNORE ? 0 : 1);
  if (
      before == LT_ALPHA
        &&
        (
          !nsString::IsAlpha(text.First()) ||
          Right(text, 1).Compare(rep, PR_TRUE, rep.Length())
               // returns true for mismatch
        ) ||
      before == LT_DELIMITER
        &&
        (
          nsString::IsAlpha(text.First()) ||
          nsString::IsDigit(text.First()) ||
          text.First() == rep.First() ||
          Right(text, 1).Compare(rep, PR_TRUE, rep.Length())
        ) ||
      before == LT_IGNORE
        && text.Compare(rep, PR_TRUE, rep.Length()) ||
      after == LT_ALPHA
        && !nsString::IsAlpha(text[afterPos]) ||
      after == LT_DELIMITER
        &&
        (
          nsString::IsAlpha(text[afterPos]) ||
          nsString::IsDigit(text[afterPos]) ||
          text[afterPos] == rep.First()
        )
     )
    return PR_FALSE;

  return PR_TRUE;
}

/*
  @param see ItMatchesDelimited
  @return Number of ItMatchesDelimited in text
*/
PRInt32
NumberOfMatches(const nsCAutoString& text, const nsAutoString& rep,
                LIMTYPE before, LIMTYPE after)
{
  PRInt32 result = 0;
  for (PRInt32 i = 0; i < text.Length(); i++)
    if (ItMatchesDelimited(Right(text, i), rep, before, after))
      result++;
  return result;
}

/*
  @param text (in): line of text possibly with tagTXT, starting one character
              before tagTXT or, if col0 is true, with tagTXT
  @param col0 (in): tagTXT is on the beginning of the text.
              open must be 0 then.
  @param tagTXT (in): Tag to search for, e.g. "*"
  @param tagHTML (in): HTML-Tag to replace tagTXT with,
              without "<" and ">", e.g. "strong"
  @param outputHTML (out): string to insert in output stream
  @param open (in/out): Number of currently open tags of type tagHTML
  @return Conversion succeeded
*/
PRBool
StructPhraseHit(const nsCAutoString text, PRBool col0,
     const nsAutoString& tagTXT, const nsAutoString& tagHTML,
     nsAutoString& outputHTML, PRInt32& openTags)
{
  // opening tag
  if (
      ItMatchesDelimited(text, tagTXT,
           (col0 ? LT_IGNORE : LT_DELIMITER), LT_ALPHA) // opening tag
        && NumberOfMatches(Right(text, (col0 ? 0 : 1)), tagTXT,
             LT_ALPHA, LT_DELIMITER) /* remaining closing tags */ > openTags
             /* I don't special case tagTXT before EOL,
                because text hopefully includes EOL,
                which counts as LT_DELIMITER */
     )
  {
    openTags++;
    outputHTML = '<';  // unfortunately, there's no operator+ (char, nsString&)
    outputHTML += tagHTML; outputHTML += '>';
    return PR_TRUE;
  }

  // closing tag
  if (openTags > 0 && ItMatchesDelimited(text, tagTXT, LT_ALPHA, LT_DELIMITER))
  {
    openTags--;
    outputHTML = "</"; outputHTML += tagHTML; outputHTML += '>';
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
GlyphHit(const char *line, PRInt32 line_size, char **outputHTML, PRInt32 *glyphTextLen)
{

  if ( ItMatches(line, line_size, ":-)") || ItMatches(line, line_size, ":)") ) 
  {
    *outputHTML = nsCRT::strdup("<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }  
  else if ( ItMatches(line, line_size, ":-(") || ItMatches(line, line_size, ":(") ) 
  {
    *outputHTML = nsCRT::strdup("<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatches(line, line_size, ";-)"))
  {
    *outputHTML = nsCRT::strdup("<img SRC=\"chrome://messenger/skin/wink.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatches(line, line_size, ";-P"))
  {
    *outputHTML = nsCRT::strdup("<img SRC=\"chrome://messenger/skin/sick.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
IsThisAnAmbitiousLinkType(char *link, char *mailToTag, char **linkPrefix)
{
  if (!nsCRT::strncasecmp(link, "www.", 4))
  {
    *linkPrefix = nsCRT::strdup("http://");
    return PR_TRUE;
  }  
  else if (!nsCRT::strncasecmp(link, "ftp.", 4))
  {
    *linkPrefix = nsCRT::strdup("ftp://");
    return PR_TRUE;
  }
  else if (mailToTag && *mailToTag)
  {
    if (!nsCRT::strncasecmp(link, mailToTag, nsCRT::strlen(mailToTag)))
    {
      *linkPrefix = nsCRT::strdup("mailto:");
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
ScanForURLs(const char *input, PRInt32 input_size,
                            char *output, PRInt32 output_size, PRBool urls_only)
{
  int col = 0;
  const char *end = input + input_size;
  char *output_ptr = output;
  char *end_of_buffer = output + output_size - 40; /* add safty zone :( */
  PRBool line_is_citation = PR_FALSE;
  const char *cite_open1, *cite_close1;
  const char *cite_open2, *cite_close2;
  const char* color = NULL;

  char *mailToTag = NULL;

  if (urls_only)
	{
	  cite_open1 = cite_close1 = "";
	  cite_open2 = cite_close2 = "";
	}
  else
	{
    cite_open1 = "", cite_close1 = "";
    cite_open2 = cite_close2 = "";
	}

  if (!urls_only)
	{
	  /* Decide whether this line is a quotation, and should be italicized.
		 This implements the following case-sensitive regular expression:

			^[ \t]*[A-Z]*[]>]

	     Which matches these lines:

		    > blah blah blah
		         > blah blah blah
		    LOSER> blah blah blah
		    LOSER] blah blah blah
	   */
	  const char *s = input;
	  while (s < end && nsString::IsSpace (*s)) s++;
	  while (s < end && *s >= 'A' && *s <= 'Z') s++;

	  if (s >= end)
		;
	  else if (input_size >= 6 && *s == '>' &&
			   !nsCRT::strncmp (input, ">From ", 6))	/* #$%^ing sendmail... */
		;
	  else if (*s == '>' || *s == ']')
		{
		  line_is_citation = PR_TRUE;
		  PL_strcpy(output_ptr, cite_open1);
		  output_ptr += nsCRT::strlen(cite_open1);
		  PL_strcpy(output_ptr, cite_open2);
		  output_ptr += nsCRT::strlen(cite_open2);
		  if (color &&
			  output_ptr + nsCRT::strlen(color) + 20 < end_of_buffer) {
			PL_strcpy(output_ptr, "<FONT COLOR=");
			output_ptr += nsCRT::strlen(output_ptr);
			PL_strcpy(output_ptr, color);
			output_ptr += nsCRT::strlen(output_ptr);
			PL_strcpy(output_ptr, ">");
			output_ptr += nsCRT::strlen(output_ptr);
		  }
		}
	}

  mailToTag = FindAmbitiousMailToTag(input, (PRInt32)input_size);


  /* See if we should get cute with text to glyph replacement and 
     structured phrases */

  PRBool      do_glyph_substitution = PR_TRUE;
  PRBool      do_struct_phrase = PR_TRUE;
  nsresult    rv;
  
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs)
    {
    prefs->GetBoolPref("mail.do_glyph_substitution", &do_glyph_substitution);
    prefs->GetBoolPref("mail.do_struct_phrase", &do_struct_phrase);
    }

  // We shouldn't do the fun smiley stuff etc. if we are only looking for URL's
  if (urls_only)
    {
    do_glyph_substitution = PR_FALSE;
    do_struct_phrase = PR_FALSE;
    }

  PRInt32 structPhrase_strong = 0;  // Number of currently open tags
  PRInt32 structPhrase_underline = 0;
  PRInt32 structPhrase_italic = 0;
  //  PRInt32 structPhrase_code = 0;

  /* Normal lines are scanned for buried references to URL's
     Unfortunately, it may screw up once in a while (nobody's perfect)
   */
  for(const char* cp = input; cp < end && output_ptr < end_of_buffer; cp++)
	{
	  /* if URLType returns true then it is most likely a URL
		 But only match protocol names if at the very beginning of
		 the string, or if the preceeding character was not alphanumeric;
		 this lets us match inside "---HTTP://XXX" but not inside of
		 things like "NotHTTP://xxx"
	   */
	  int     type = 0;
    PRBool  ambitiousHit;
    char    *glyphHTML;
    PRInt32 glyphTextLen;

    // Structured Phrases
    // E.g. *Bold* -> <strong>
    /* We're searching for the following pattern:
       LT_DELIMITER - "*" - ALPHA -
       [ some text (maybe more "*"-pairs) - ALPHA ] "*" - LT_DELIMITER.
       <strong> is only inserted, if existance of a pair could be verified */
    if (do_struct_phrase)
    {
      nsAutoString HTMLnsStr;
      switch (*cp)
      {
      // *bold* -> <strong>
      case '*':
        if (StructPhraseHit(cp == input ? cp : cp - 1, cp == input,
             '*', "strong", HTMLnsStr, structPhrase_strong))
        {
          char* HTMLCStr = HTMLnsStr.ToNewCString();
          PR_snprintf(output_ptr, output_size - (output_ptr-output),
               HTMLCStr);
          Recycle(HTMLCStr);
          output_ptr += HTMLnsStr.Length();
          cp++;
        }
        break;
      // _underline_ / _italic_ -> <em>
      case '_':
        if (StructPhraseHit(cp == input ? cp : cp - 1, cp == input,
             '_', "em", HTMLnsStr, structPhrase_underline))
             // <u> is deprecated
        {
          char* HTMLCStr = HTMLnsStr.ToNewCString();
          PR_snprintf(output_ptr, output_size - (output_ptr-output),
               HTMLCStr);
          Recycle(HTMLCStr);
          output_ptr += HTMLnsStr.Length();
          cp++;
        }
        break;
      // /italic/ -> <em>
      case '/':
        if (StructPhraseHit(cp == input ? cp : cp - 1, cp == input,
             '/', "em", HTMLnsStr, structPhrase_italic))
        {
          char* HTMLCStr = HTMLnsStr.ToNewCString();
          PR_snprintf(output_ptr, output_size - (output_ptr-output),
               HTMLCStr);
          Recycle(HTMLCStr);
          output_ptr += HTMLnsStr.Length();
          cp++;
        }
        break;
      // |code| -> <code>
      /* <code> formatting is not rendered visible at the moment :-(
      case '|':
        if (StructPhraseHit(cp == input ? cp : cp - 1, cp == input,
             '|', "code", HTMLnsStr, structPhrase_code))
        {
          char* HTMLCStr = HTMLnsStr.ToNewCString();
          PR_snprintf(output_ptr, output_size - (output_ptr-output),
               HTMLCStr);
          Recycle(HTMLCStr);
          output_ptr += HTMLnsStr.Length();
          cp++;
        }
        break;
      */
      } //switch
    } //if

    // Glyphs (Smilies etc.)
    if ((do_glyph_substitution) && GlyphHit(cp, end - cp, &glyphHTML, &glyphTextLen))
    {
      PRInt32   size_available = output_size - (output_ptr-output);
      PR_snprintf(output_ptr, size_available, glyphHTML);
      output_ptr += nsCRT::strlen(glyphHTML);
      cp += glyphTextLen-1;
      PR_FREEIF(glyphHTML);
      continue;
    }

    // URLs
    ambitiousHit = PR_FALSE;
    if (mailToTag)
      AmbitiousURLType(cp, &type, mailToTag);
    if (!type)
      URLType(cp, &type);
    else
      ambitiousHit = PR_TRUE;

	  if(!nsString::IsSpace(*cp) &&
		 (cp == input || (!IS_ALPHA(cp[-1]) && !IS_DIGIT(cp[-1]))) &&
		 (type) != 0)
		{
		  const char *cp2;
		  for(cp2=cp; cp2 < end; cp2++)
			{
			  /* These characters always mark the end of the URL. */
			  if (nsString::IsSpace(*cp2) ||
				  *cp2 == '<' || *cp2 == '>' ||
				  *cp2 == '`' || *cp2 == ')' ||
				  *cp2 == '\'' || *cp2 == '"' ||
				  /**cp2 == ']' || */ *cp2 == '}'
				  )
				break;
			}

		  /* Check for certain punctuation characters on the end, and strip
			 them off. */
		  while (cp2 > cp && 
				 (cp2[-1] == '.' || cp2[-1] == ',' || cp2[-1] == '!' ||
				  cp2[-1] == ';' || cp2[-1] == '-' || cp2[-1] == '?' ||
				  cp2[-1] == '#'))
  			cp2--;

		  col += (cp2 - cp);

		  /* if the url is less than 7 characters then we screwed up
		   * and got a "news:" url or something which is worthless
		   * to us.  Exclude the A tag in this case.
		   *
		   * Also exclude any URL that ends in a colon; those tend
		   * to be internal and magic and uninteresting.
		   *
		   * And also exclude the builtin icons, whose URLs look
		   * like "internal-gopher-binary".
		   */
      PRBool invalidHit = PR_FALSE;

      if ( (cp2 > cp && cp2[-1] == ':') ||
			      !nsCRT::strncmp(cp, "internal-", 9) )
        invalidHit = PR_TRUE;

      if (!invalidHit)
      {
        if ((ambitiousHit) && ((cp2-cp) < (PRInt32)nsCRT::strlen(mailToTag)))
          invalidHit = PR_TRUE;
        if ( (!ambitiousHit) && (cp2-cp < 7) )
          invalidHit = PR_TRUE;
      }

      if (invalidHit)
			{
        nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr += (cp2-cp);
			  *output_ptr = 0;
			}
		  else
			{
			  char      *quoted_url;
        PRBool    rawLink = PR_FALSE;
			  PRInt32   size_left = output_size - (output_ptr-output);
        char      *linkPrefix = NULL;

			  if(cp2-cp > size_left)
				return NS_ERROR_OUT_OF_MEMORY;

			  nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr[cp2-cp] = 0;

        rawLink = IsThisAnAmbitiousLinkType(output_ptr, mailToTag, &linkPrefix);
			  quoted_url = nsEscapeHTML(output_ptr);
			  if (!quoted_url) return NS_ERROR_OUT_OF_MEMORY;

        if (rawLink)
			    PR_snprintf(output_ptr, size_left,
						    "<A HREF=\"%s%s\">%s</A>",
                linkPrefix,
						    quoted_url,
						    quoted_url);
        else
			    PR_snprintf(output_ptr, size_left,
						    "<A HREF=\"%s\">%s</A>",
						    quoted_url,
						    quoted_url);

			  output_ptr += nsCRT::strlen(output_ptr);
			  nsCRT::free(quoted_url);
        PR_FREEIF(linkPrefix);
			  output_ptr += nsCRT::strlen(output_ptr);
			}

		  cp = cp2-1;  /* go to next word */
		}
	  else
        // Special symbols
		{
		  /* Make sure that special symbols don't screw up the HTML parser
		   */
		  if(*cp == '<')
			{
			  PL_strcpy(output_ptr, "&lt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '>')
			{
			  PL_strcpy(output_ptr, "&gt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '&')
			{
			  PL_strcpy(output_ptr, "&amp;");
			  output_ptr += 5;
			  col++;
			}
		  else
            // Normal characters
			{
			  *output_ptr++ = *cp;
			  col++;
			}

      PR_FREEIF(mailToTag);
      mailToTag = FindAmbitiousMailToTag(cp, (PRInt32)end - (PRInt32)cp);
		}
	}

  *output_ptr = 0;

  if (line_is_citation)	/* Close off the highlighting */
	{
	  if (color) {
		PL_strcpy(output_ptr, "</FONT>");
		output_ptr += nsCRT::strlen(output_ptr);
	  }

	  PL_strcpy(output_ptr, cite_close2);
	  output_ptr += nsCRT::strlen (cite_close2);
	  PL_strcpy(output_ptr, cite_close1);
	  output_ptr += nsCRT::strlen (cite_close1);
	}

  PR_FREEIF(mailToTag);
  return NS_OK;
}

/* modifies a url of the form   /foo/../foo1  ->  /foo1
 *
 * it only operates on "file" "ftp" and "http" url's all others are returned
 * unmodified
 *
 * returns the modified passed in URL string
 */
nsresult
ReduceURL (char *url, char **retURL)
{
  PRInt32     url_type;

  URLType(url, &url_type);
  char * fwd_ptr;
  char * url_ptr;
  char * path_ptr;
  
  if(!url)
  {
    *retURL = url;
    return NS_OK;
  }
  
  if(url_type == HTTP_TYPE_URL || url_type == FILE_TYPE_URL ||
	   url_type == FTP_TYPE_URL ||
     url_type == SECURE_HTTP_TYPE_URL)
  {
    
		/* find the path so we only change that and not the host
    */
    path_ptr = PL_strchr(url, '/');
    
    if(!path_ptr)
    {
      *retURL = url;
      return NS_OK;
    }
    
    if(*(path_ptr+1) == '/')
		    path_ptr = PL_strchr(path_ptr+2, '/');
    
    if(!path_ptr)
    {
      *retURL = url;
      return NS_OK;
    }
    
    fwd_ptr = path_ptr;
    url_ptr = path_ptr;
    
    for(; *fwd_ptr != '\0'; fwd_ptr++)
		  {
      
      if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '/')
      {
      /* remove ./ 
        */	
        fwd_ptr += 1;
      }
      else if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '.' && 
        (*(fwd_ptr+3) == '/' || *(fwd_ptr+3) == '\0'))
      {
      /* remove foo/.. 
        */	
        /* reverse the url_ptr to the previous slash
        */
        if(url_ptr != path_ptr) 
          url_ptr--; /* we must be going back at least one */
        for(;*url_ptr != '/' && url_ptr != path_ptr; url_ptr--)
          ;  /* null body */
        
             /* forward the fwd_prt past the ../
        */
        fwd_ptr += 2;
      }
      else
      {
      /* copy the url incrementaly 
        */
        *url_ptr++ = *fwd_ptr;
      }
		  }
    *url_ptr = '\0';  /* terminate the url */
  }
  
  *retURL = url;
  return NS_OK;
}

static void
Append(char** output, PRInt32* output_max, char** curoutput, const char* buf, PRInt32 length)
{
  if (length + (*curoutput) - (*output) >= *output_max) {
    int offset = (*curoutput) - (*output);
    do {
      (*output_max) += 1024;
    } while (length + (*curoutput) - (*output) >= *output_max);
    *output = (char *)PR_Realloc(*output, *output_max);
    if (!*output) return;
    *curoutput = *output + offset;
  }
  nsCRT::memcpy(*curoutput, buf, length);
  *curoutput += length;
}

nsresult
ScanHTMLForURLs(const char* input, char **retBuf)
{
    char* output = NULL;
    char* curoutput;
    PRInt32 output_max;
    char* tmpbuf = NULL;
    PRInt32 tmpbuf_max;
    PRInt32 inputlength;
    const char* inputend;
    const char* linestart;
    const char* lineend;

    PR_ASSERT(input);
    if (!input) 
    {
      *retBuf = NULL;
      return NS_OK;
    }
    inputlength = nsCRT::strlen(input);

    output_max = inputlength + 1024; /* 1024 bytes ought to be enough to quote
                                        several URLs, which ought to be as many
                                        as most docs use. */
    output = (char *)PR_Malloc(output_max);
    if (!output) 
      goto FAIL;

    tmpbuf_max = 1024;
    tmpbuf = (char *)PR_Malloc(tmpbuf_max);
    if (!tmpbuf) 
      goto FAIL;

    inputend = input + inputlength;

    linestart = input;
    curoutput = output;

    /* Here's the strategy.  We find a chunk of plainish looking text -- no
       embedded CR or LF, no "<" or "&".  We feed that off to ScanForURLs,
       and append the result.  Then we skip to the next bit of plain text.  If
       we stopped at an "&", go to the terminating ";".  If we stopped at a
       "<", well, if it was a "<A>" tag, then skip to the closing "</A>".
       Otherwise, skip to the end of the tag.
       */
    lineend = linestart;
    while (linestart < inputend && lineend <= inputend) {
        switch (*lineend) {
        case '<':
        case '>':
        case '&':
        case CR:
        case LF:
        case '\0':
            if (lineend > linestart) {
                int length = lineend - linestart;
                if (length * 3 > tmpbuf_max) {
                    tmpbuf_max = length * 3 + 512;
                    PR_Free(tmpbuf);
                    tmpbuf = (char *)PR_Malloc(tmpbuf_max);
                    if (!tmpbuf) 
                      goto FAIL;
                }
                if (ScanForURLs(linestart, length,
                                tmpbuf, tmpbuf_max, TRUE) != NS_OK) {
                    goto FAIL;
                }
                length = nsCRT::strlen(tmpbuf);
                Append(&output, &output_max, &curoutput, tmpbuf, length);
                if (!output) 
                  goto FAIL;
            }
            linestart = lineend;
            lineend = NULL;
            if (inputend - linestart < 5) {
                /* Too little to worry about; shove the rest out. */
                lineend = inputend;
            } else {
                switch (*linestart) {
                case '<':
                    if ((linestart[1] == 'a' || linestart[1] == 'A') &&
                        linestart[2] == ' ') {
                        lineend = PL_strcasestr(linestart, "</a");
                        if (lineend) {
                            lineend = PL_strchr(lineend, '>');
                            if (lineend) lineend++;
                        }
                    } else {
                        lineend = PL_strchr(linestart, '>');
                        if (lineend) lineend++;
                    }
                    break;
                case '&':
                    lineend = PL_strchr(linestart, ';');
                    if (lineend) lineend++;
                    break;
                default:
                    lineend = linestart + 1;
                    break;
                }
            }
            if (!lineend) lineend = inputend;
            Append(&output, &output_max, &curoutput, linestart,
                   lineend - linestart);
            if (!output) 
              goto FAIL;
            linestart = lineend;
            break;
        default:
            lineend++;
        }
    }
    PR_Free(tmpbuf);
    tmpbuf = NULL;
    *curoutput = '\0';
    *retBuf = output;
    return NS_OK;

FAIL:
    if (tmpbuf) PR_Free(tmpbuf);
    if (output) PR_Free(output);
    *retBuf = NULL;
    return NS_ERROR_FAILURE;
}

//
// Also skip '>' as part of host name 
//
nsresult
ParseURL(const char *url, int parts_requested, char **returnVal)
{
  char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
  char *atSign, *host, *passwordColon, *gtThan;
  
  assert(url); 
  if(!url)
  {
    *returnVal = mime_SACat(&rv, "");
    return NS_ERROR_FAILURE;
  }
  colon = PL_strchr(url, ':'); /* returns a const char */
  
  /* Get the protocol part, not including anything beyond the colon */
  if (parts_requested & GET_PROTOCOL_PART) {
    if(colon) {
      char val = *(colon+1);
      *(colon+1) = '\0';
      mime_SACopy(&rv, url);
      *(colon+1) = val;
      
      /* If the user wants more url info, tack on extra slashes. */
      if( (parts_requested & GET_HOST_PART)
        || (parts_requested & GET_USERNAME_PART)
        || (parts_requested & GET_PASSWORD_PART)
        )
      {
        if( *(colon+1) == '/' && *(colon+2) == '/')
          mime_SACat(&rv, "//");
        /* If there's a third slash consider it file:/// and tack on the last slash. */
        if( *(colon+3) == '/' )
          mime_SACat(&rv, "/");
      }
		  }
  }
  
  
  /* Get the username if one exists */
  if (parts_requested & GET_USERNAME_PART) {
    if (colon 
      && (*(colon+1) == '/')
      && (*(colon+2) == '/')
      && (*(colon+3) != '\0')) {
      
      if ( (slash = PL_strchr(colon+3, '/')) != NULL)
        *slash = '\0';
      if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
        *atSign = '\0';
        if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL)
          *passwordColon = '\0';
        mime_SACat(&rv, colon+3);
        
        /* Get the password if one exists */
        if (parts_requested & GET_PASSWORD_PART) {
          if (passwordColon) {
            mime_SACat(&rv, ":");
            mime_SACat(&rv, passwordColon+1);
          }
        }
        if (parts_requested & GET_HOST_PART)
          mime_SACat(&rv, "@");
        if (passwordColon)
          *passwordColon = ':';
        *atSign = '@';
      }
      if (slash)
        *slash = '/';
    }
  }
  
  /* Get the host part */
  if (parts_requested & GET_HOST_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/')
      {
        slash = PL_strchr(colon+3, '/');
        
        if(slash)
          *slash = '\0';
        
        if( (atSign = PL_strchr(colon+3, '@')) != NULL)
          host = atSign+1;
        else
          host = colon+3;
        
        ques_mark = PL_strchr(host, '?');
        
        if(ques_mark)
          *ques_mark = '\0';
        
        gtThan = PL_strchr(host, '>');
        
        if (gtThan)
          *gtThan = '\0';
        
          /*
          * Protect systems whose header files forgot to let the client know when
          * gethostbyname would trash their stack.
        */
#ifndef MAXHOSTNAMELEN
#ifdef XP_OS2
#error Managed to get into NET_ParseURL without defining MAXHOSTNAMELEN !!!
#endif
#define MAXHOSTNAMELEN  256
#endif
        /* limit hostnames to within MAXHOSTNAMELEN characters to keep
        * from crashing
        */
        if(nsCRT::strlen(host) > MAXHOSTNAMELEN)
        {
          char * cp;
          char old_char;
          
          cp = host+MAXHOSTNAMELEN;
          old_char = *cp;
          
          *cp = '\0';
          
          mime_SACat(&rv, host);
          
          *cp = old_char;
        }
        else
        {
          mime_SACat(&rv, host);
        }
        
        if(slash)
          *slash = '/';
        
        if(ques_mark)
          *ques_mark = '?';
        
        if (gtThan)
          *gtThan = '>';
      }
    }
  }
  
  /* Get the path part */
  if (parts_requested & GET_PATH_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/')
      {
        /* skip host part */
        slash = PL_strchr(colon+3, '/');
      }
      else 
      {
      /* path is right after the colon
        */
        slash = colon+1;
      }
      
      if(slash)
      {
        ques_mark = PL_strchr(slash, '?');
        hash_mark = PL_strchr(slash, '#');
        
        if(ques_mark)
          *ques_mark = '\0';
        
        if(hash_mark)
          *hash_mark = '\0';
        
        mime_SACat(&rv, slash);
        
        if(ques_mark)
          *ques_mark = '?';
        
        if(hash_mark)
          *hash_mark = '#';
      }
		  }
  }
		
  if(parts_requested & GET_HASH_PART) {
    hash_mark = PL_strchr(url, '#'); /* returns a const char * */
    
    if(hash_mark)
		  {
      ques_mark = PL_strchr(hash_mark, '?');
      
      if(ques_mark)
        *ques_mark = '\0';
      
      mime_SACat(&rv, hash_mark);
      
      if(ques_mark)
        *ques_mark = '?';
		  }
  }
  
  if(parts_requested & GET_SEARCH_PART) {
    ques_mark = PL_strchr(url, '?'); /* returns a const char * */
    
    if(ques_mark)
    {
      hash_mark = PL_strchr(ques_mark, '#');
      
      if(hash_mark)
        *hash_mark = '\0';
      
      mime_SACat(&rv, ques_mark);
      
      if(hash_mark)
        *hash_mark = '#';
    }
  }
  
  /* copy in a null string if nothing was copied in
	 */
  if(!rv)
	   mime_SACopy(&rv, "");

  *returnVal = rv;

  return NS_OK;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// RICHIE: ...WHERE DO I SIGN!!!
// UGLY but effective - will soon dissappear when BenB is done debugging
// new code
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/****************************************************************************
  XPCOM Interface
  Use this from anything != C++
*****************************************************************************/

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
mozTXTToHTMLConv::ScanTXT(const PRUnichar *text, PRUint32 whattodo, PRUnichar **_retval)
{
  if (!_retval || !text)
    return NS_ERROR_NULL_POINTER;

  nsString  tString(text);
  char      *workString;
  PRInt32   length;

  workString = tString.ToNewCString();
  if (!workString)
    return NS_ERROR_NULL_POINTER;

  // Ok, there is always the issue of guessing how much space we will need for emoticons.
  // So what we will do is count the total number of "special" chars and multiply by 82 
  // (max len for a smiley line) and add one for good measure
  length = nsCRT::strlen(workString);
  PRInt32   specialCharCount = 0;
  for (PRInt32 z=0; z<length; z++)
  {
    if ( (workString[z] == ')') || (workString[z] == '(') || (workString[z] == ':') || (workString[z] == ';') )
      ++specialCharCount;
  }

  PRInt32 buffersizeneeded = length + (82 * (specialCharCount + 1));   
  char *workBuf = (char *)PR_MALLOC(buffersizeneeded);
  if (!workBuf)
    return NS_ERROR_NULL_POINTER;

  nsCRT::memset(workBuf, 0, buffersizeneeded);
  nsresult rv = ScanForURLs(workString, length, workBuf, buffersizeneeded, PR_FALSE);

  if (NS_SUCCEEDED(rv))
  {
    *_retval = nsString(workBuf).ToNewUnicode();
    if (!*_retval)
      rv = NS_ERROR_NULL_POINTER;
  }

  PR_FREEIF(workBuf);
  PR_FREEIF(workString);
  return rv;
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

