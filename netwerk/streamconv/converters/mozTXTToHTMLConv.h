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

/*
  Description: Currently only functions to enhance plain text with HTML tags. See mozITXTToHTMLConv.
*/

#ifndef _mozTXTToHTMLConv_h__
#define _mozTXTToHTMLConv_h__

#include "mozITXTToHTMLConv.h"
#include "nsString.h"

static NS_DEFINE_CID(kTXTToHTMLConvCID, MOZITXTTOHTMLCONV_CID);

class mozTXTToHTMLConv : public mozITXTToHTMLConv { 
public:
  mozTXTToHTMLConv();
  virtual ~mozTXTToHTMLConv();
  NS_DECL_ISUPPORTS
  static const nsIID& GetIID(void) { static nsIID iid = MOZITXTTOHTMLCONV_IID; return iid; }

  NS_DECL_MOZITXTTOHTMLCONV
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMCONVERTER

/*
  see ScanTXT
 */
  static nsAutoString ScanTXT(const nsAutoString& text, PRUint32 whattodo);

/*
  see ScanHTML
 */
  static nsAutoString ScanHTML(const nsAutoString& text, PRUint32 whattodo);

/*
  see CiteLevelTXT
 */
  static PRInt32 CiteLevelTXT(const nsAutoString& line,PRUint32& logLineStart);

protected:

/*
  Currently only changes "<", ">" and "&". All others stay as they are.<p>
  "Char" in function name to avoid side effects with nsString(char)
  constructors.
  @param ch (in)
  @return ch in its HTML representation
*/
  static nsAutoString EscapeChar(const PRUnichar ch);

/*
  See EscapeChar
*/
  static nsAutoString EscapeStr(const nsAutoString& aString);

/*
  Currently only reverts "<", ">" and "&". All others stay as they are.<p>
  @param aString (in) HTML string
  @return aString in its plain text representation
*/
  static nsAutoString UnescapeStr(const nsAutoString& aString);

/*
  Completes<ul>
  <li>1. "mozilla@bucksch.org" to "mailto:mozilla@bucksch.org"
  <li>2. "www.mozilla.org" to "http://www.mozilla.org"
  <li>3. "ftp.mozilla.org" to "ftp://www.mozilla.org"
  </ul>
  It does no check, if the resulting URL is valid.
  @param text (in): abbreviated URL
  @param pos (in): position of "@" (case 1) or first "." (case 2 and 3)
  @return Completed URL at success and empty string at failure
 */
  static nsAutoString CompleteAbbreviatedURL(const nsAutoString& text,
       const PRUint32 pos);

/*
  <p><em>Note:</em> replaceBefore + replaceAfter + 1 (for char at pos) chars
  in text should be replaced by outputHTML.</p>
  <p><em>Note:</em> This function should be able to process a URL on multiple
  lines, but currently, ScanForURLs is called for every line, so it can't.</p>
  @param text (in): includes possibly a URL
  @param pos (in): position in text, where either ":", "." or "@" are found
  @param whathasbeendone (in): What the calling ScanTXT did/has to do with the
              (not-linkified) text. (Needed to calculate replaceBefore.)
              NOT what will be done with the content of the link.
  @param outputHTML (out): URL with HTML-a tag
  @param replaceBefore (out): Number of chars of URL before pos
  @param replaceAfter (out): Number of chars of URL after pos
  @return URL found
*/
  static PRBool FindURL(const nsAutoString& text, const PRUint32 pos,
       const PRUint32 whathasbeendone, nsAutoString& outputHTML,
       PRInt32& replaceBefore, PRInt32& replaceAfter);

/*
  @param text (in): the source string
  @param start (in): offset of text specifying the start of the new object
  @return a new (local) object containing the substring
*/
  static nsAutoString Right(const nsAutoString& text, PRUint32 start);

  enum LIMTYPE
  {
    LT_IGNORE,     // limitation not checked
    LT_DELIMITER,  // not alphanumeric and not rep[0]. End of text is also ok.
    LT_ALPHA,      // alpha char
    LT_DIGIT
  };

/*
  @param text (in): the string to search through.<p>
         If before = IGNORE,<br>
           rep is compared starting at 1. char of text (text[0]),<br>
           else starting at 2. char of text (text[1]).
  @param rep (in): the string to look for
  @param before (in): limitation before rep
  @param after (in): limitation after rep
  @return true, if rep is found and limitation spec is met or rep is empty
*/
  static PRBool ItMatchesDelimited(const nsAutoString& text,
       const nsAutoString& rep, LIMTYPE before, LIMTYPE after);

/*
  @param see ItMatchesDelimited
  @return Number of ItMatchesDelimited in text
*/
  static PRUint32 NumberOfMatches(const nsAutoString& text,
       const nsAutoString& rep, LIMTYPE before, LIMTYPE after);

/*
  @param text (in): line of text possibly with tagTXT.<p>
              if col0 is true,
                starting with tagTXT<br>
              else
                starting one char before tagTXT
  @param col0 (in): tagTXT is on the beginning of the logical line.
              open must be 0 then.
  @param tagTXT (in): Tag in plaintext to search for, e.g. "*"
  @param tagHTML (in): HTML-Tag to replace tagTXT with,
              without "<" and ">", e.g. "strong"
  @param attributeHTML (in): HTML-attribute to add to opening tagHTML,
              e.g. "class=txt_star"
  @param outputHTML (out): string to insert in output stream
  @param open (in/out): Number of currently open tags of type tagHTML
  @return Conversion succeeded
*/
  static PRBool StructPhraseHit(const nsAutoString& text, PRBool col0,
       const nsAutoString tagTXT,
       const nsAutoString tagHTML, const nsAutoString attributeHTML,
       nsAutoString& outputHTML, PRUint32& openTags);

/*
  @param text (in), col0 (in): see GlyphHit
  @param tagTXT (in): Smily, see also StructPhraseHit
  @param tagHTML (in): see StructPhraseHit
  @param outputHTML (out), glyphTextLen (out): see GlyphHit
*/
  static PRBool SmilyHit(const nsAutoString& text, PRBool col0,
       const nsAutoString tagTXT, const nsAutoString tagHTML,
       nsAutoString& outputHTML, PRInt32& glyphTextLen);

/*
  Checks, if we can replace some chars at the start of line with prettier HTML
  code.<p>
  If success is reported, replace the first glyphTextLen chars with outputHTML

  @param text (in): line in msg, starting at char to investigate
  @param col0 (in): text starts at the beginning of the logical line
  @param outputHTML (out): see StructPhraseHit
  @param glyphTextLen (out): Length of original text to replace
  @return see StructPhraseHit
*/
  static PRBool GlyphHit(const nsAutoString& text, PRBool col0,
       nsAutoString& outputHTML, PRInt32& glyphTextLen);

private:
  enum modetype {
         unknown = 0,
         RFC1738,          /* Check, if RFC1738, APPENDIX compliant,
                              like "<URL:http://www.mozilla.org>". */
         RFC2396E,         /* RFC2396, APPENDIX E allows anglebrackets (like
                              "<http://www.mozilla.org>") (without "URL:") or
                              quotation marks (like""http://www.mozilla.org"").
                              Also allow email addresses without scheme,
                              e.g. "<mozilla@bucksch.org>" */
         freetext,         /* assume heading scheme
                              with "[a-zA-Z][a-zA-Z0-9+\-.]*:" like "news:"
                              (see RFC2396, Section 3.1).
                              Certain characters (see code) or any whitespace
                              (including linebreaks) end the URL.
                              Other certain (punctation) characters (see code)
                              at the end are stripped off. */
         abbreviated       /* "www.mozilla.org", "ftp.mozilla.org" and
                              "mozilla@bucksch.org". Similar to freetext. */
      /* RFC1738 and RFC2396E type URLs may use multiple lines,
         whitespace is stripped. Special characters like "," stay intact.*/
  };

/**
 * @param text (in), pos (in): see FindURL
 * @param check (in): Start must be conform with this modetype
 * @param start (out): Position in text, where URL (including brackets or
 *             similar) starts
 * @return |check|-conform start has been found
 */
  static PRBool FindURLStart(const nsAutoString& text, const PRUint32 pos,
       const modetype check, PRUint32& start);

/**
 * @param text (in), pos (in): see FindURL
 * @param check (in): End must be conform with this modetype
 * @param start (in): see FindURLStart
 * @param end (out): Similar to |start| param of FindURLStart
 * @return |check|-conform end has been found
 */
  static PRBool FindURLEnd(const nsAutoString& text, const PRUint32 pos,
       const modetype check, const PRUint32 start, PRUint32& end);

/**
 * @param text (in), pos (in), whathasbeendone (in): see FindURL
 * @param start (in), end (in): see FindURLEnd
 * @param txtURL (out): Guessed (raw) URL.
               Without whitespace, but not completed.
 * @param desc (out): Link as shown to the user, but already escaped.
 *             Should be placed between the <a> and </a> tags.
 * @param replaceBefore(out), replaceAfter (out): see FindURL
 */
  static void CalculateURLBoundaries(const nsAutoString& text,
       const PRUint32 pos, const PRUint32 whathasbeendone,
       const modetype check, const PRUint32 start, const PRUint32 end,
       nsAutoString& txtURL, nsAutoString& desc,
       PRInt32& replaceBefore, PRInt32& replaceAfter);

/**
 * @param txtURL (in), desc (in): see CalculateURLBoundaries
 * @param outputHTML (out): see FindURL
 * @return A valid URL could be found (and creation of HTML successful)
 */
  static PRBool CheckURLAndCreateHTML(
       const nsAutoString& txtURL, const nsAutoString& desc,
       nsAutoString& outputHTML);
};

// BenB: I had to move these to here to get stuff compiling 
//       correctly on Win32 & Mac
const PRInt32 lastMode = 4;  // Needed (only) by FindURL
const PRUint32 numberOfModes = 4;       // dito

#endif
