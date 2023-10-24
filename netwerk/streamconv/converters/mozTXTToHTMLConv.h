/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  Description: Currently only functions to enhance plain text with HTML tags.
  See mozITXTToHTMLConv. Stream conversion is defunct.
*/

#ifndef _mozTXTToHTMLConv_h__
#define _mozTXTToHTMLConv_h__

#include "mozITXTToHTMLConv.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIIOService;

class mozTXTToHTMLConv : public mozITXTToHTMLConv {
  virtual ~mozTXTToHTMLConv() = default;

  //////////////////////////////////////////////////////////
 public:
  //////////////////////////////////////////////////////////

  mozTXTToHTMLConv() = default;
  NS_DECL_ISUPPORTS

  NS_DECL_MOZITXTTOHTMLCONV
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSISTREAMCONVERTER

  /**
    see mozITXTToHTMLConv::CiteLevelTXT
   */
  int32_t CiteLevelTXT(const char16_t* line, uint32_t& logLineStart);

  //////////////////////////////////////////////////////////
 protected:
  //////////////////////////////////////////////////////////
  nsCOMPtr<nsIIOService>
      mIOService;  // for performance reasons, cache the netwerk service...
                   /**
                     Completes<ul>
                     <li>Case 1: mailto: "mozilla@bucksch.org" -> "mailto:mozilla@bucksch.org"
                     <li>Case 2: http:   "www.mozilla.org"     -> "http://www.mozilla.org"
                     <li>Case 3: ftp:    "ftp.mozilla.org"     -> "ftp://www.mozilla.org"
                     </ul>
                     It does no check, if the resulting URL is valid.
                     @param text (in): abbreviated URL
                     @param pos (in): position of "@" (case 1) or first "." (case 2 and 3)
                     @return Completed URL at success and empty string at failure
                    */
  void CompleteAbbreviatedURL(const char16_t* aInString, int32_t aInLength,
                              const uint32_t pos, nsString& aOutString);

  //////////////////////////////////////////////////////////
 private:
  //////////////////////////////////////////////////////////

  enum LIMTYPE {
    LT_IGNORE,     // limitation not checked
    LT_DELIMITER,  // not alphanumeric and not rep[0]. End of text is also ok.
    LT_ALPHA,      // alpha char
    LT_DIGIT
  };

  /**
    @param text (in): the string to search through.<p>
           If before = IGNORE,<br>
             rep is compared starting at 1. char of text (text[0]),<br>
             else starting at 2. char of text (text[1]).
           Chars after "after"-delimiter are ignored.
    @param rep (in): the string to look for
    @param aRepLen (in): the number of bytes in the string to look for
    @param before (in): limitation before rep
    @param after (in): limitation after rep
    @return true, if rep is found and limitation spec is met or rep is empty
  */
  bool ItMatchesDelimited(const char16_t* aInString, int32_t aInLength,
                          const char16_t* rep, int32_t aRepLen, LIMTYPE before,
                          LIMTYPE after);

  /**
    @param see ItMatchesDelimited
    @return Number of ItMatchesDelimited in text
  */
  uint32_t NumberOfMatches(const char16_t* aInString, int32_t aInStringLength,
                           const char16_t* rep, int32_t aRepLen, LIMTYPE before,
                           LIMTYPE after);

  /**
    Currently only changes "<", ">" and "&". All others stay as they are.<p>
    "Char" in function name to avoid side effects with nsString(ch)
    constructors.
    @param ch (in)
    @param aStringToAppendto (out) - the string to append the escaped
                                     string to.
    @param inAttribute (in) - will escape quotes, too (which is
                              only needed for attribute values)
  */
  void EscapeChar(const char16_t ch, nsAString& aStringToAppendto,
                  bool inAttribute);

  /**
    See EscapeChar. Escapes the string in place.
  */
  void EscapeStr(nsString& aInString, bool inAttribute);

  /**
    Currently only reverts "<", ">" and "&". All others stay as they are.<p>
    @param aInString (in) HTML string
    @param aStartPos (in) start index into the buffer
    @param aLength (in) length of the buffer
    @param aOutString (out) unescaped buffer
  */
  void UnescapeStr(const char16_t* aInString, int32_t aStartPos,
                   int32_t aLength, nsString& aOutString);

  /**
    <em>Note</em>: I use different strategies to pass context between the
    functions (full text and pos vs. cutted text and col0, glphyTextLen vs.
    replaceBefore/-After). It makes some sense, but is hard to understand
    (maintain) :-(.
  */

  /**
    <p><em>Note:</em> replaceBefore + replaceAfter + 1 (for char at pos) chars
    in text should be replaced by outputHTML.</p>
    <p><em>Note:</em> This function should be able to process a URL on multiple
    lines, but currently, ScanForURLs is called for every line, so it can't.</p>
    @param text (in): includes possibly a URL
    @param pos (in): position in text, where either ":", "." or "@" are found
    @param whathasbeendone (in): What the calling ScanTXT did/has to do with the
                (not-linkified) text, i.e. usually the "whattodo" parameter.
                (Needed to calculate replaceBefore.) NOT what will be done with
                the content of the link.
    @param outputHTML (out): URL with HTML-a tag
    @param replaceBefore (out): Number of chars of URL before pos
    @param replaceAfter (out): Number of chars of URL after pos
    @return URL found
  */
  bool FindURL(const char16_t* aInString, int32_t aInLength, const uint32_t pos,
               const uint32_t whathasbeendone, nsString& outputHTML,
               int32_t& replaceBefore, int32_t& replaceAfter);

  enum modetype {
    unknown,
    RFC1738,    /* Check, if RFC1738, APPENDIX compliant,
                   like "<URL:http://www.mozilla.org>". */
    RFC2396E,   /* RFC2396, APPENDIX E allows anglebrackets (like
                   "<http://www.mozilla.org>") (without "URL:") or
                   quotation marks(like ""http://www.mozilla.org"").
                   Also allow email addresses without scheme,
                   e.g. "<mozilla@bucksch.org>" */
    freetext,   /* assume heading scheme
                   with "[a-zA-Z][a-zA-Z0-9+\-\.]*:" like "news:"
                   (see RFC2396, Section 3.1).
                   Certain characters (see code) or any whitespace
                   (including linebreaks) end the URL.
                   Other certain (punctation) characters (see code)
                   at the end are stripped off. */
    abbreviated /* Similar to freetext, but without scheme, e.g.
                   "www.mozilla.org", "ftp.mozilla.org" and
                   "mozilla@bucksch.org". */
                /* RFC1738 and RFC2396E type URLs may use multiple lines,
                   whitespace is stripped. Special characters like ")" stay intact.*/
  };

  /**
   * @param text (in), pos (in): see FindURL
   * @param check (in): Start must be conform with this mode
   * @param start (out): Position in text, where URL (including brackets or
   *             similar) starts
   * @return |check|-conform start has been found
   */
  bool FindURLStart(const char16_t* aInString, int32_t aInLength,
                    const uint32_t pos, const modetype check, uint32_t& start);

  /**
   * @param text (in), pos (in): see FindURL
   * @param check (in): End must be conform with this mode
   * @param start (in): see FindURLStart
   * @param end (out): Similar to |start| param of FindURLStart
   * @return |check|-conform end has been found
   */
  bool FindURLEnd(const char16_t* aInString, int32_t aInStringLength,
                  const uint32_t pos, const modetype check,
                  const uint32_t start, uint32_t& end);

  /**
   * @param text (in), pos (in), whathasbeendone (in): see FindURL
   * @param check (in): Current mode
   * @param start (in), end (in): see FindURLEnd
   * @param txtURL (out): Guessed (raw) URL.
   *             Without whitespace, but not completed.
   * @param desc (out): Link as shown to the user, but already escaped.
   *             Should be placed between the <a> and </a> tags.
   * @param replaceBefore(out), replaceAfter (out): see FindURL
   */
  void CalculateURLBoundaries(const char16_t* aInString,
                              int32_t aInStringLength, const uint32_t pos,
                              const uint32_t whathasbeendone,
                              const modetype check, const uint32_t start,
                              const uint32_t end, nsString& txtURL,
                              nsString& desc, int32_t& replaceBefore,
                              int32_t& replaceAfter);

  /**
   * @param txtURL (in), desc (in): see CalculateURLBoundaries
   * @param outputHTML (out): see FindURL
   * @return A valid URL could be found (and creation of HTML successful)
   */
  bool CheckURLAndCreateHTML(const nsString& txtURL, const nsString& desc,
                             const modetype mode, nsString& outputHTML);

  /**
    @param text (in): line of text possibly with tagTXT.<p>
                if col0 is true,
                  starting with tagTXT<br>
                else
                  starting one char before tagTXT
    @param col0 (in): tagTXT is on the beginning of the line (or paragraph).
                open must be 0 then.
    @param tagTXT (in): Tag in plaintext to search for, e.g. "*"
    @param aTagTxtLen (in): length of tagTXT.
    @param tagHTML (in): HTML-Tag to replace tagTXT with,
                without "<" and ">", e.g. "strong"
    @param attributeHTML (in): HTML-attribute to add to opening tagHTML,
                e.g. "class=txt_star"
    @param aOutString: string to APPEND the converted html into
    @param open (in/out): Number of currently open tags of type tagHTML
    @return Conversion succeeded
  */
  bool StructPhraseHit(const char16_t* aInString, int32_t aInStringLength,
                       bool col0, const char16_t* tagTXT, int32_t aTagTxtLen,
                       const char* tagHTML, const char* attributeHTML,
                       nsAString& aOutString, uint32_t& openTags);

  /**
    @param text (in), col0 (in): see GlyphHit
    @param tagTXT (in): Smily, see also StructPhraseHit
    @param imageName (in): the basename of the file that contains the image for
                           this smilie
    @param outputHTML (out): new string containing the html for the smily
    @param glyphTextLen (out): see GlyphHit
  */
  bool SmilyHit(const char16_t* aInString, int32_t aLength, bool col0,
                const char* tagTXT, const nsString& imageName,
                nsString& outputHTML, int32_t& glyphTextLen);

  /**
    Checks, if we can replace some chars at the start of line with prettier HTML
    code.<p>
    If success is reported, replace the first glyphTextLen chars with outputHTML

    @param text (in): line of text possibly with Glyph.<p>
                If col0 is true,
                  starting with Glyph <br><!-- (br not part of text) -->
                else
                  starting one char before Glyph
    @param col0 (in): text starts at the beginning of the line (or paragraph)
    @param aOutString (out): APPENDS html for the glyph to this string
    @param glyphTextLen (out): Length of original text to replace
    @return see StructPhraseHit
  */
  bool GlyphHit(const char16_t* aInString, int32_t aInLength, bool col0,
                nsAString& aOutputString, int32_t& glyphTextLen);

  /**
    Check if a given url should be linkified.
    @param aURL (in): url to be checked on.
  */
  bool ShouldLinkify(const nsCString& aURL);
};

// It's said, that Win32 and Mac don't like static const members
const int32_t mozTXTToHTMLConv_lastMode = 4;
// Needed (only) by mozTXTToHTMLConv::FindURL
const int32_t mozTXTToHTMLConv_numberOfModes = 4;  // dito; unknown not counted

#endif
