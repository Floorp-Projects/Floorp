/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include "nsScanner.h"
#include "nsToken.h"
#include "nsHTMLTokens.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"
#include "nsHTMLEntities.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsScanner.h"
#include "nsParserConstants.h"

static const PRUnichar sUserdefined[] = {'u', 's', 'e', 'r', 'd', 'e', 'f',
                                         'i', 'n', 'e', 'd', 0};

static const PRUnichar kAttributeTerminalChars[] = {
  PRUnichar('&'), PRUnichar('\t'), PRUnichar('\n'),
  PRUnichar('\r'), PRUnichar(' '), PRUnichar('>'),
  PRUnichar(0)
};

static void AppendNCR(nsSubstring& aString, PRInt32 aNCRValue);
/**
 * Consumes an entity from aScanner and expands it into aString.
 *
 * @param   aString The target string to append the entity to.
 * @param   aScanner Controller of underlying input source
 * @param   aIECompatible Controls whether we respect entities with values >
 *                        255 and no terminating semicolon.
 * @param   aFlag If NS_IPARSER_FLAG_VIEW_SOURCE do not reduce entities...
 * @return  error result
 */
static nsresult
ConsumeEntity(nsScannerSharedSubstring& aString,
              nsScanner& aScanner,
              bool aIECompatible,
              PRInt32 aFlag)
{
  nsresult result = NS_OK;

  PRUnichar ch;
  result = aScanner.Peek(ch, 1);

  if (NS_SUCCEEDED(result)) {
    PRUnichar amp = 0;
    PRInt32 theNCRValue = 0;
    nsAutoString entity;

    if (nsCRT::IsAsciiAlpha(ch) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result = CEntityToken::ConsumeEntity(ch, entity, aScanner);
      if (NS_SUCCEEDED(result)) {
        theNCRValue = nsHTMLEntities::EntityToUnicode(entity);
        PRUnichar theTermChar = entity.Last();
        // If an entity value is greater than 255 then:
        // Nav 4.x does not treat it as an entity,
        // IE treats it as an entity if terminated with a semicolon.
        // Resembling IE!!

        nsSubstring &writable = aString.writable();
        if (theNCRValue < 0 ||
            (aIECompatible && theNCRValue > 255 && theTermChar != ';')) {
          // Looks like we're not dealing with an entity
          writable.Append(kAmpersand);
          writable.Append(entity);
        } else {
          // A valid entity so reduce it.
          writable.Append(PRUnichar(theNCRValue));
        }
      }
    } else if (ch == kHashsign && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result = CEntityToken::ConsumeEntity(ch, entity, aScanner);
      if (NS_SUCCEEDED(result)) {
        nsSubstring &writable = aString.writable();
        if (result == NS_HTMLTOKENS_NOT_AN_ENTITY) {
          // Looked like an entity but it's not
          aScanner.GetChar(amp);
          writable.Append(amp);
          result = NS_OK;
        } else {
          PRInt32 err;
          theNCRValue = entity.ToInteger(&err, kAutoDetect);
          AppendNCR(writable, theNCRValue);
        }
      }
    } else {
      // What we thought as entity is not really an entity...
      aScanner.GetChar(amp);
      aString.writable().Append(amp);
    }
  }

  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume attributed text value.
 *  Note: It also reduces entities.
 *
 *  @param   aNewlineCount -- the newline count to increment when hitting newlines
 *  @param   aScanner -- controller of underlying input source
 *  @param   aTerminalChars -- characters that stop consuming attribute.
 *  @param   aAllowNewlines -- whether to allow newlines in the value.
 *                             XXX it would be nice to roll this info into
 *                             aTerminalChars somehow....
 *  @param   aIECompatEntities IE treats entities with values > 255 as
 *                             entities only if they're terminated with a
 *                             semicolon. This is true to follow that behavior
 *                             and false to treat all values as entities.
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
static nsresult
ConsumeUntil(nsScannerSharedSubstring& aString,
             PRInt32& aNewlineCount,
             nsScanner& aScanner,
             const nsReadEndCondition& aEndCondition,
             bool aAllowNewlines,
             bool aIECompatEntities,
             PRInt32 aFlag)
{
  nsresult result = NS_OK;
  bool     done = false;

  do {
    result = aScanner.ReadUntil(aString, aEndCondition, false);
    if (NS_SUCCEEDED(result)) {
      PRUnichar ch;
      aScanner.Peek(ch);
      if (ch == kAmpersand) {
        result = ConsumeEntity(aString, aScanner, aIECompatEntities, aFlag);
      } else if (ch == kCR && aAllowNewlines) {
        aScanner.GetChar(ch);
        result = aScanner.Peek(ch);
        if (NS_SUCCEEDED(result)) {
          nsSubstring &writable = aString.writable();
          if (ch == kNewLine) {
            writable.AppendLiteral("\r\n");
            aScanner.GetChar(ch);
          } else {
            writable.Append(PRUnichar('\r'));
          }
          ++aNewlineCount;
        }
      } else if (ch == kNewLine && aAllowNewlines) {
        aScanner.GetChar(ch);
        aString.writable().Append(PRUnichar('\n'));
        ++aNewlineCount;
      } else {
        done = true;
      }
    }
  } while (NS_SUCCEEDED(result) && !done);

  return result;
}

/**************************************************************
  And now for the token classes...
 **************************************************************/

/**
 * Constructor from tag id
 */
CHTMLToken::CHTMLToken(eHTMLTags aTag)
  : CToken(aTag)
{
}


CHTMLToken::~CHTMLToken()
{
}

/*
 * Constructor from tag id
 */
CStartToken::CStartToken(eHTMLTags aTag)
  : CHTMLToken(aTag)
{
  mEmpty = false;
  mContainerInfo = eFormUnknown;
#ifdef DEBUG
  mAttributed = false;
#endif
}

CStartToken::CStartToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_unknown)
{
  mEmpty = false;
  mContainerInfo = eFormUnknown;
  mTextValue.Assign(aName);
#ifdef DEBUG
  mAttributed = false;
#endif
}

CStartToken::CStartToken(const nsAString& aName, eHTMLTags aTag)
  : CHTMLToken(aTag)
{
  mEmpty = false;
  mContainerInfo = eFormUnknown;
  mTextValue.Assign(aName);
#ifdef DEBUG
  mAttributed = false;
#endif
}

/*
 * This method returns the typeid (the tag type) for this token.
 */
PRInt32
CStartToken::GetTypeID()
{
  if (eHTMLTag_unknown == mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }
  return mTypeID;
}

PRInt32
CStartToken::GetTokenType()
{
  return eToken_start;
}

void
CStartToken::SetEmpty(bool aValue)
{
  mEmpty = aValue;
}

bool
CStartToken::IsEmpty()
{
  return mEmpty;
}

/*
 * Consume the identifier portion of the start tag
 */
nsresult
CStartToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  // If you're here, we've already Consumed the < char, and are
  // ready to Consume the rest of the open tag identifier.
  // Stop consuming as soon as you see a space or a '>'.
  // NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  nsresult result = NS_OK;
  nsScannerSharedSubstring tagIdent;

  if (aFlag & NS_IPARSER_FLAG_HTML) {
    result = aScanner.ReadTagIdentifier(tagIdent);
    mTypeID = (PRInt32)nsHTMLTags::LookupTag(tagIdent.str());
    // Save the original tag string if this is user-defined or if we
    // are viewing source
    if (eHTMLTag_userdefined == mTypeID ||
        (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      mTextValue = tagIdent.str();
    }
  } else {
    result = aScanner.ReadTagIdentifier(tagIdent);
    mTextValue = tagIdent.str();
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  if (NS_SUCCEEDED(result) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
    result = aScanner.SkipWhitespace(mNewlineCount);
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // Take what we can get.
    result = NS_OK;
  }

  return result;
}

const nsSubstring&
CStartToken::GetStringValue()
{
  if (eHTMLTag_unknown < mTypeID && mTypeID < eHTMLTag_text) {
    if (!mTextValue.Length()) {
      mTextValue.Assign(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

void
CStartToken::GetSource(nsString& anOutputString)
{
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

void
CStartToken::AppendSourceTo(nsAString& anOutputString)
{
  anOutputString.Append(PRUnichar('<'));
  /*
   * Watch out for Bug 15204
   */
  if (!mTextValue.IsEmpty()) {
    anOutputString.Append(mTextValue);
  } else {
    anOutputString.Append(GetTagName(mTypeID));
  }

  anOutputString.Append(PRUnichar('>'));
}

CEndToken::CEndToken(eHTMLTags aTag)
  : CHTMLToken(aTag)
{
}

CEndToken::CEndToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_unknown)
{
  mTextValue.Assign(aName);
}

CEndToken::CEndToken(const nsAString& aName, eHTMLTags aTag)
  : CHTMLToken(aTag)
{
  mTextValue.Assign(aName);
}

nsresult
CEndToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  nsresult result = NS_OK;
  nsScannerSharedSubstring tagIdent;

  if (aFlag & NS_IPARSER_FLAG_HTML) {
    result = aScanner.ReadTagIdentifier(tagIdent);

    mTypeID = (PRInt32)nsHTMLTags::LookupTag(tagIdent.str());
    // Save the original tag string if this is user-defined or if we
    // are viewing source
    if (eHTMLTag_userdefined == mTypeID ||
        (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      mTextValue = tagIdent.str();
    }
  } else {
    result = aScanner.ReadTagIdentifier(tagIdent);
    mTextValue = tagIdent.str();
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  if (NS_SUCCEEDED(result) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
    result = aScanner.SkipWhitespace(mNewlineCount);
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // Take what we can get.
    result = NS_OK;
  }

  return result;
}


/*
 *  Asks the token to determine the <i>HTMLTag type</i> of
 *  the token. This turns around and looks up the tag name
 *  in the tag dictionary.
 */
PRInt32
CEndToken::GetTypeID()
{
  if (eHTMLTag_unknown == mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
    switch (mTypeID) {
      case eHTMLTag_dir:
      case eHTMLTag_menu:
        mTypeID = eHTMLTag_ul;
        break;

      default:
        break;
    }
  }

  return mTypeID;
}

PRInt32
CEndToken::GetTokenType()
{
  return eToken_end;
}

const nsSubstring&
CEndToken::GetStringValue()
{
  if (eHTMLTag_unknown < mTypeID && mTypeID < eHTMLTag_text) {
    if (!mTextValue.Length()) {
      mTextValue.Assign(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

void
CEndToken::GetSource(nsString& anOutputString)
{
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

void
CEndToken::AppendSourceTo(nsAString& anOutputString)
{
  anOutputString.AppendLiteral("</");
  if (!mTextValue.IsEmpty()) {
    anOutputString.Append(mTextValue);
  } else {
    anOutputString.Append(GetTagName(mTypeID));
  }

  anOutputString.Append(PRUnichar('>'));
}

CTextToken::CTextToken()
  : CHTMLToken(eHTMLTag_text)
{
}

CTextToken::CTextToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_text)
{
  mTextValue.Rebind(aName);
}

PRInt32
CTextToken::GetTokenType()
{
  return eToken_text;
}

PRInt32
CTextToken::GetTextLength()
{
  return mTextValue.Length();
}

nsresult
CTextToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  static const PRUnichar theTerminalsChars[] =
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('&'), PRUnichar('<'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result = NS_OK;
  bool      done = false;
  nsScannerIterator origin, start, end;

  // Start scanning after the first character, because we know it to
  // be part of this text token (we wouldn't have come here if it weren't)
  aScanner.CurrentPosition(origin);
  start = origin;
  aScanner.EndReading(end);

  NS_ASSERTION(start != end, "Calling CTextToken::Consume when already at the "
                             "end of a document is a bad idea.");

  aScanner.SetPosition(++start);

  while (NS_OK == result && !done) {
    result = aScanner.ReadUntil(start, end, theEndCondition, false);
    if (NS_OK == result) {
      result = aScanner.Peek(aChar);

      if (NS_OK == result && (kCR == aChar || kNewLine == aChar)) {
        switch (aChar) {
          case kCR:
          {
            // It's a carriage return. See if this is part of a CR-LF pair (in
            // which case we need to treat it as one newline). If we're at the
            // edge of a packet, then leave the CR on the scanner, since it
            // could still be part of a CR-LF pair. Otherwise, it isn't.
            PRUnichar theNextChar;
            result = aScanner.Peek(theNextChar, 1);

            if (result == kEOF && aScanner.IsIncremental()) {
              break;
            }

            if (NS_SUCCEEDED(result)) {
              // Actually get the carriage return.
              aScanner.GetChar(aChar);
            }

            if (kLF == theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and let
              // it be ignored by the layout system.
              end.advance(2);
              aScanner.GetChar(theNextChar);
            } else {
              // If it is standalone, replace the "\r" with a "\n" so that it
              // will be considered by the layout system.
              aScanner.ReplaceCharacter(end, kLF);
              ++end;
            }
            ++mNewlineCount;
            break;
          }
          case kLF:
            aScanner.GetChar(aChar);
            ++end;
            ++mNewlineCount;
            break;
        }
      } else {
        done = true;
      }
    }
  }

  // Note: This function is only called from nsHTMLTokenizer::ConsumeText. If
  // we return an error result from the final buffer, then it is responsible
  // for turning it into an NS_OK result.
  aScanner.BindSubstring(mTextValue, origin, end);

  return result;
}

/*
 *  Consume as much clear text from scanner as possible.
 *  The scanner is left on the < of the perceived end tag.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aConservativeConsume -- controls our handling of content with no
 *                                   terminating string.
 *  @param   aIgnoreComments -- whether or not we should take comments into
 *                              account in looking for the end tag.
 *  @param   aScanner -- controller of underlying input source
 *  @param   aEndTagname -- the terminal tag name.
 *  @param   aFlag -- dtd modes and such.
 *  @param   aFlushTokens -- true if we found the terminal tag.
 *  @return  error result
 */
nsresult
CTextToken::ConsumeCharacterData(bool aIgnoreComments,
                                 nsScanner& aScanner,
                                 const nsAString& aEndTagName,
                                 PRInt32 aFlag,
                                 bool& aFlushTokens)
{
  nsresult result = NS_OK;
  nsScannerIterator theStartOffset, theCurrOffset, theTermStrPos,
                    theStartCommentPos, theAltTermStrPos, endPos;
  bool          done = false;
  bool          theLastIteration = false;

  aScanner.CurrentPosition(theStartOffset);
  theCurrOffset = theStartOffset;
  aScanner.EndReading(endPos);
  theTermStrPos = theStartCommentPos = theAltTermStrPos = endPos;

  // ALGORITHM: *** The performance is based on correctness of the document ***
  // 1. Look for a '<' character.  This could be
  //      a) Start of a comment (<!--),
  //      b) Start of the terminal string, or
  //      c) a start of a tag.
  //    We are interested in a) and b). c) is ignored because in CDATA we
  //    don't care for tags.
  //    NOTE: Technically speaking in CDATA we should ignore the comments too!
  //    But for compatibility we don't.
  // 2. Having the offset, for '<', search for the terminal string from there
  //    on and record its offset.
  // 3. From the same '<' offset also search for start of a comment '<!--'.
  //    If found search for end comment '-->' between the terminal string and
  //    '<!--'.  If you did not find the end comment, then we have a malformed
  //    document, i.e., this section has a prematured terminal string Ex.
  //    <SCRIPT><!-- document.write('</SCRIPT>') //--> </SCRIPT>. But record
  //    terminal string's offset if this is the first premature terminal
  //    string, and update the current offset to the terminal string
  //    (prematured) offset and goto step 1.
  // 4. Amen...If you found a terminal string and '-->'. Otherwise goto step 1.
  // 5. If the end of the document is reached and if we still don't have the
  //    condition in step 4. then assume that the prematured terminal string
  //    is the actual terminal string and goto step 1. This will be our last
  //    iteration. If there is no premature terminal string and we're being
  //    conservative in our consumption (aConservativeConsume), then don't
  //    consume anything from the scanner. Otherwise, we consume all the way
  //    until the end.

  NS_NAMED_LITERAL_STRING(ltslash, "</");
  const nsString theTerminalString = ltslash + aEndTagName;

  PRUint32 termStrLen = theTerminalString.Length();
  while (result == NS_OK && !done) {
    bool found = false;
    nsScannerIterator gtOffset, ltOffset = theCurrOffset;
    while (FindCharInReadable(PRUnichar(kLessThan), ltOffset, endPos) &&
           ((PRUint32)ltOffset.size_forward() >= termStrLen ||
            Distance(ltOffset, endPos) >= termStrLen)) {
      // Make a copy of the (presumed) end tag and
      // do a case-insensitive comparison

      nsScannerIterator start(ltOffset), end(ltOffset);
      end.advance(termStrLen);

      if (CaseInsensitiveFindInReadable(theTerminalString, start, end) &&
          (end == endPos || (*end == '>'  || *end == ' '  ||
                             *end == '\t' || *end == '\n' ||
                             *end == '\r'))) {
        gtOffset = end;
        // Note that aIgnoreComments is only not set for <script>. We don't
        // want to execute scripts that aren't in the form of: <script\s.*>
        if ((end == endPos && aIgnoreComments) ||
            FindCharInReadable(PRUnichar(kGreaterThan), gtOffset, endPos)) {
          found = true;
          theTermStrPos = start;
        }
        break;
      }
      ltOffset.advance(1);
    }

    if (found && theTermStrPos != endPos) {
      if (!(aFlag & NS_IPARSER_FLAG_STRICT_MODE) &&
          !theLastIteration && !aIgnoreComments) {
        nsScannerIterator endComment(ltOffset);
        endComment.advance(5);

        if ((theStartCommentPos == endPos) &&
            FindInReadable(NS_LITERAL_STRING("<!--"), theCurrOffset,
                           endComment)) {
          theStartCommentPos = theCurrOffset;
        }

        if (theStartCommentPos != endPos) {
          // Search for --> between <!-- and </TERMINALSTRING>.
          theCurrOffset = theStartCommentPos;
          nsScannerIterator terminal(theTermStrPos);
          if (!RFindInReadable(NS_LITERAL_STRING("-->"),
                               theCurrOffset, terminal)) {
            // If you're here it means that we have a bogus terminal string.
            // Even though it is bogus, the position of the terminal string
            // could be helpful in case we hit the rock bottom.
            if (theAltTermStrPos == endPos) {
              // But we only want to remember the first bogus terminal string.
              theAltTermStrPos = theTermStrPos;
            }

            // We did not find '-->' so keep searching for terminal string.
            theCurrOffset = theTermStrPos;
            theCurrOffset.advance(termStrLen);
            continue;
          }
        }
      }

      aScanner.BindSubstring(mTextValue, theStartOffset, theTermStrPos);
      aScanner.SetPosition(ltOffset);

      // We found </SCRIPT> or </STYLE>...permit flushing -> Ref: Bug 22485
      aFlushTokens = true;
      done = true;
    } else {
      // We end up here if:
      // a) when the buffer runs out ot data.
      // b) when the terminal string is not found.
      if (!aScanner.IsIncremental()) {
        if (theAltTermStrPos != endPos) {
          // If you're here it means that we hit the rock bottom and therefore
          // switch to plan B, since we have an alternative terminating string.
          theCurrOffset = theAltTermStrPos;
          theLastIteration = true;
        } else {
          // Oops, We fell all the way down to the end of the document.
          done = true; // Do this to fix Bug. 35456
          result = kFakeEndTag;
          aScanner.BindSubstring(mTextValue, theStartOffset, endPos);
          aScanner.SetPosition(endPos);
        }
      } else {
        result = kEOF;
      }
    }
  }

  if (result == NS_OK) {
    mNewlineCount = mTextValue.CountChar(kNewLine);
  }

  return result;
}

/*
 *  Consume as much clear text from scanner as possible. Reducing entities.
 *  The scanner is left on the < of the perceived end tag.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aConservativeConsume -- controls our handling of content with no
 *                                   terminating string.
 *  @param   aScanner -- controller of underlying input source
 *  @param   aEndTagname -- the terminal tag name.
 *  @param   aFlag -- dtd modes and such.
 *  @param   aFlushTokens -- true if we found the terminal tag.
 *  @return  error result
 */
nsresult
CTextToken::ConsumeParsedCharacterData(bool aDiscardFirstNewline,
                                       bool aConservativeConsume,
                                       nsScanner& aScanner,
                                       const nsAString& aEndTagName,
                                       PRInt32 aFlag,
                                       bool& aFound)
{
  // This function is fairly straightforward except if there is no terminating
  // string. If there is, we simply loop through all of the entities, reducing
  // them as necessary and skipping over non-terminal strings starting with <.
  // If there is *no* terminal string, then we examine aConservativeConsume.
  // If we want to be conservative, we backtrack to the first place in the
  // document that looked like the end of PCDATA (i.e., the first tag). This
  // is for compatibility and so we don't regress bug 42945. If we are not
  // conservative, then we consume everything, all the way up to the end of
  // the document.

  static const PRUnichar terminalChars[] = {
    PRUnichar('\r'), PRUnichar('\n'), PRUnichar('&'), PRUnichar('<'),
    PRUnichar(0)
  };
  static const nsReadEndCondition theEndCondition(terminalChars);

  nsScannerIterator currPos, endPos, altEndPos;
  PRUint32 truncPos = 0;
  PRInt32 truncNewlineCount = 0;
  aScanner.CurrentPosition(currPos);
  aScanner.EndReading(endPos);

  altEndPos = endPos;

  nsScannerSharedSubstring theContent;
  PRUnichar ch = 0;

  NS_NAMED_LITERAL_STRING(commentStart, "<!--");
  NS_NAMED_LITERAL_STRING(ltslash, "</");
  const nsString theTerminalString = ltslash + aEndTagName;
  PRUint32 termStrLen = theTerminalString.Length();
  PRUint32 commentStartLen = commentStart.Length();

  nsresult result = NS_OK;

  // Note that if we're already at the end of the document, the ConsumeUntil
  // will fail, and we'll do the right thing.
  do {
    result = ConsumeUntil(theContent, mNewlineCount, aScanner,
                          theEndCondition, true, false, aFlag);

    if (aDiscardFirstNewline &&
        (NS_SUCCEEDED(result) || !aScanner.IsIncremental()) &&
        !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      // Check if the very first character is a newline, and if so discard it.
      // Note that we don't want to discard it in view source!
      // Also note that this has to happen here (as opposed to before the
      // ConsumeUntil) because we have to expand any entities.
      // XXX It would be nice to be able to do this without calling
      // writable()!
      const nsSubstring &firstChunk = theContent.str();
      if (!firstChunk.IsEmpty()) {
        PRUint32 where = 0;
        PRUnichar newline = firstChunk.First();

        if (newline == kCR || newline == kNewLine) {
          ++where;

          if (firstChunk.Length() > 1) {
            if (newline == kCR && firstChunk.CharAt(1) == kNewLine) {
              // Handle \r\n = 1 newline.
              ++where;
            }
            // Note: \n\r = 2 newlines.
          }
        }

        if (where != 0) {
          theContent.writable() = Substring(firstChunk, where);
        }
      }
    }
    aDiscardFirstNewline = false;

    if (NS_FAILED(result)) {
      if (kEOF == result && !aScanner.IsIncremental()) {
        aFound = true; // this is as good as it gets.
        result = kFakeEndTag;

        if (aConservativeConsume && altEndPos != endPos) {
          // We ran out of room looking for a </title>. Go back to the first
          // place that looked like a tag and use that as our stopping point.
          theContent.writable().Truncate(truncPos);
          mNewlineCount = truncNewlineCount;
          aScanner.SetPosition(altEndPos, false, true);
        }
        // else we take everything we consumed.
        mTextValue.Rebind(theContent.str());
      } else {
        aFound = false;
      }

      return result;
    }

    aScanner.CurrentPosition(currPos);
    aScanner.GetChar(ch); // this character must be '&' or '<'

    if (ch == kLessThan && altEndPos == endPos) {
      // Keep this position in case we need it for later.
      altEndPos = currPos;
      truncPos = theContent.str().Length();
      truncNewlineCount = mNewlineCount;
    }

    if (Distance(currPos, endPos) >= termStrLen) {
      nsScannerIterator start(currPos), end(currPos);
      end.advance(termStrLen);

      if (CaseInsensitiveFindInReadable(theTerminalString, start, end)) {
        if (end != endPos && (*end == '>'  || *end == ' '  ||
                              *end == '\t' || *end == '\n' ||
                              *end == '\r')) {
          aFound = true;
          mTextValue.Rebind(theContent.str());

          // Note: This SetPosition() is actually going backwards from the
          // scanner's mCurrentPosition (so we pass aReverse == true). This
          // is because we call GetChar() above after we get the current
          // position.
          aScanner.SetPosition(currPos, false, true);
          break;
        }
      }
    }
    // IE only consumes <!-- --> as comments in PCDATA.
    if (Distance(currPos, endPos) >= commentStartLen) {
      nsScannerIterator start(currPos), end(currPos);
      end.advance(commentStartLen);

      if (CaseInsensitiveFindInReadable(commentStart, start, end)) {
        CCommentToken consumer; // stack allocated.

        // CCommentToken expects us to be on the '-'
        aScanner.SetPosition(currPos.advance(2));

        // In quirks mode we consume too many things as comments, so pretend
        // that we're not by modifying aFlag.
        result = consumer.Consume(*currPos, aScanner,
                                  (aFlag & ~NS_IPARSER_FLAG_QUIRKS_MODE) |
                                   NS_IPARSER_FLAG_STRICT_MODE);
        if (kEOF == result) {
          // This can only happen if we're really out of space.
          return kEOF;
        } else if (kNotAComment == result) {
          // Fall through and consume this as text.
          aScanner.CurrentPosition(currPos);
          aScanner.SetPosition(currPos.advance(1));
        } else {
          consumer.AppendSourceTo(theContent.writable());
          mNewlineCount += consumer.GetNewlineCount();
          continue;
        }
      }
    }

    result = kEOF;
    // We did not find the terminal string yet so
    // include the character that stopped consumption.
    theContent.writable().Append(ch);
  } while (currPos != endPos);

  return result;
}

void
CTextToken::CopyTo(nsAString& aStr)
{
  nsScannerIterator start, end;
  mTextValue.BeginReading(start);
  mTextValue.EndReading(end);
  CopyUnicodeTo(start, end, aStr);
}

const nsSubstring& CTextToken::GetStringValue()
{
  return mTextValue.AsString();
}

void
CTextToken::Bind(nsScanner* aScanner, nsScannerIterator& aStart,
                 nsScannerIterator& aEnd)
{
  aScanner->BindSubstring(mTextValue, aStart, aEnd);
}

void
CTextToken::Bind(const nsAString& aStr)
{
  mTextValue.Rebind(aStr);
}

CCDATASectionToken::CCDATASectionToken(eHTMLTags aTag)
  : CHTMLToken(aTag)
{
}

CCDATASectionToken::CCDATASectionToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_unknown)
{
  mTextValue.Assign(aName);
}

PRInt32
CCDATASectionToken::GetTokenType()
{
  return eToken_cdatasection;
}

/*
 *  Consume as much marked test from scanner as possible.
 *  Note: This has to handle case: "<![ ! IE 5]>", in addition to "<![..[..]]>"
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CCDATASectionToken::Consume(PRUnichar aChar, nsScanner& aScanner,
                            PRInt32 aFlag)
{
  static const PRUnichar theTerminalsChars[] =
  { PRUnichar('\r'), PRUnichar('\n'), PRUnichar(']'), PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result = NS_OK;
  bool      done = false;

  while (NS_OK == result && !done) {
    result = aScanner.ReadUntil(mTextValue, theEndCondition, false);
    if (NS_OK == result) {
      result = aScanner.Peek(aChar);
      if (kCR == aChar && NS_OK == result) {
        result = aScanner.GetChar(aChar); // Strip off the \r
        result = aScanner.Peek(aChar);    // Then see what's next.
        if (NS_OK == result) {
          switch(aChar) {
            case kCR:
              result = aScanner.GetChar(aChar); // Strip off the \r
              mTextValue.AppendLiteral("\n\n");
              mNewlineCount += 2;
              break;

            case kNewLine:
              // Which means we saw \r\n, which becomes \n
              result = aScanner.GetChar(aChar); // Strip off the \n

              // Fall through...
            default:
              mTextValue.AppendLiteral("\n");
              mNewlineCount++;
              break;
          }
        }
      } else if (kNewLine == aChar) {
        result = aScanner.GetChar(aChar);
        mTextValue.Append(aChar);
        ++mNewlineCount;
      } else if (kRightSquareBracket == aChar) {
        bool canClose = false;
        result = aScanner.GetChar(aChar); // Strip off the ]
        mTextValue.Append(aChar);
        result = aScanner.Peek(aChar);    // Then see what's next.
        if (NS_OK == result && kRightSquareBracket == aChar) {
          result = aScanner.GetChar(aChar); // Strip off the second ]
          mTextValue.Append(aChar);
          canClose = true;
        }

        // The goal here is to not lose data from the page when encountering
        // markup like: <![endif]-->.  This means that in normal parsing, we
        // allow ']' to end the marked section and just drop everything between
        // it an the '>'.  In view-source mode, we cannot drop things on the
        // floor like that.  In fact, to make view-source of XML with script in
        // CDATA sections at all bearable, we need to somewhat enforce the ']]>'
        // terminator for marked sections.  So make the tokenization somewhat
        // different when in view-source _and_ dealing with a CDATA section.
        // XXX We should remember this StringBeginsWith test.
        bool inCDATA = (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) &&
          StringBeginsWith(mTextValue, NS_LITERAL_STRING("[CDATA["));
        if (inCDATA) {
          // Consume all right square brackets to catch cases such as:
          // <![CDATA[foo]]]>
          while (true) {
            result = aScanner.Peek(aChar);
            if (result != NS_OK || aChar != kRightSquareBracket) {
              break;
            }

            mTextValue.Append(aChar);
            aScanner.GetChar(aChar);
          }
        } else {
          nsAutoString dummy; // Skip any bad data
          result = aScanner.ReadUntil(dummy, kGreaterThan, false);
        }
        if (NS_OK == result &&
            (!inCDATA || (canClose && kGreaterThan == aChar))) {
          result = aScanner.GetChar(aChar); // Strip off the >
          done = true;
        }
      } else {
        done = true;
      }
    }
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // We ran out of space looking for the end of this CDATA section.
    // In order to not completely lose the entire section, treat everything
    // until the end of the document as part of the CDATA section and let
    // the DTD handle it.
    mInError = true;
    result = NS_OK;
  }

  return result;
}

const nsSubstring&
CCDATASectionToken::GetStringValue()
{
  return mTextValue;
}


CMarkupDeclToken::CMarkupDeclToken()
  : CHTMLToken(eHTMLTag_markupDecl)
{
}

CMarkupDeclToken::CMarkupDeclToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_markupDecl)
{
  mTextValue.Rebind(aName);
}

PRInt32
CMarkupDeclToken::GetTokenType()
{
  return eToken_markupDecl;
}

/*
 *  Consume as much declaration from scanner as possible.
 *  Declaration is a markup declaration of ELEMENT, ATTLIST, ENTITY or
 *  NOTATION, which can span multiple lines and ends in >.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CMarkupDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,
                          PRInt32 aFlag)
{
  static const PRUnichar theTerminalsChars[] =
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('\''), PRUnichar('"'),
      PRUnichar('>'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result = NS_OK;
  bool      done = false;
  PRUnichar quote = 0;

  nsScannerIterator origin, start, end;
  aScanner.CurrentPosition(origin);
  start = origin;

  while (NS_OK == result && !done) {
    aScanner.SetPosition(start);
    result = aScanner.ReadUntil(start, end, theEndCondition, false);
    if (NS_OK == result) {
      result = aScanner.Peek(aChar);

      if (NS_OK == result) {
        PRUnichar theNextChar = 0;
        if (kCR == aChar || kNewLine == aChar) {
          result = aScanner.GetChar(aChar); // Strip off the char
          result = aScanner.Peek(theNextChar); // Then see what's next.
        }
        switch(aChar) {
          case kCR:
            // result = aScanner.GetChar(aChar);
            if (kLF == theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and
              // let it be ignored by the layout system
              end.advance(2);
              result = aScanner.GetChar(theNextChar);
            } else {
              // If it standalone, replace the "\r" with a "\n" so that
              // it will be considered by the layout system
              aScanner.ReplaceCharacter(end, kLF);
              ++end;
            }
            ++mNewlineCount;
            break;
          case kLF:
            ++end;
            ++mNewlineCount;
            break;
          case '\'':
          case '"':
            ++end;
            if (quote) {
              if (quote == aChar) {
                quote = 0;
              }
            } else {
              quote = aChar;
            }
            break;
          case kGreaterThan:
            if (quote) {
              ++end;
            } else {
              start = end;
              // Note that start is wrong after this, we just avoid temp var
              ++start;
              aScanner.SetPosition(start); // Skip the >
              done = true;
            }
            break;
          default:
            NS_ABORT_IF_FALSE(0, "should not happen, switch is missing cases?");
            break;
        }
        start = end;
      } else {
        done = true;
      }
    }
  }
  aScanner.BindSubstring(mTextValue, origin, end);

  if (kEOF == result) {
    mInError = true;
    if (!aScanner.IsIncremental()) {
      // Hide this EOF.
      result = NS_OK;
    }
  }

  return result;
}

const nsSubstring&
CMarkupDeclToken::GetStringValue()
{
  return mTextValue.AsString();
}


CCommentToken::CCommentToken()
  : CHTMLToken(eHTMLTag_comment)
{
}

CCommentToken::CCommentToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_comment)
{
  mComment.Rebind(aName);
}

void
CCommentToken::AppendSourceTo(nsAString& anOutputString)
{
  AppendUnicodeTo(mCommentDecl, anOutputString);
}

static bool
IsCommentEnd(const nsScannerIterator& aCurrent, const nsScannerIterator& aEnd,
             nsScannerIterator& aGt)
{
  nsScannerIterator current = aCurrent;
  PRInt32 dashes = 0;

  while (current != aEnd && dashes != 2) {
    if (*current == kGreaterThan) {
      aGt = current;
      return true;
    }
    if (*current == PRUnichar('-')) {
      ++dashes;
    } else {
      dashes = 0;
    }
    ++current;
  }

  return false;
}

nsresult
CCommentToken::ConsumeStrictComment(nsScanner& aScanner)
{
  // <!--[... -- ... -- ...]*-->
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          when they're formatted per spec, but if they're not
          we don't handle them well.
   *********************************************************/
  nsScannerIterator end, current, gt, lt;
  aScanner.EndReading(end);
  aScanner.CurrentPosition(current);

  nsScannerIterator beginData = end;

  lt = current;
  lt.advance(-2); // <!

  current.advance(-1);

  // Regular comment must start with <!--
  if (*current == kExclamation &&
      ++current != end && *current == kMinus &&
      ++current != end && *current == kMinus &&
      ++current != end) {
    nsScannerIterator currentEnd = end;
    bool balancedComment = false;
    NS_NAMED_LITERAL_STRING(dashes, "--");
    beginData = current;

    while (FindInReadable(dashes, current, currentEnd)) {
      current.advance(2);

      balancedComment = !balancedComment; // We need to match '--' with '--'

      if (balancedComment && IsCommentEnd(current, end, gt)) {
        // done
        current.advance(-2);
        // Note: it's ok if beginData == current, (we'll copy an empty string)
        // and we need to bind mComment anyway.
        aScanner.BindSubstring(mComment, beginData, current);
        aScanner.BindSubstring(mCommentDecl, lt, ++gt);
        aScanner.SetPosition(gt);
        return NS_OK;
      }

      // Continue after the last '--'
      currentEnd = end;
    }
  }

  // If beginData == end, we did not find opening '--'
  if (beginData == end) {
    // This might have been empty comment: <!>
    // Or it could have been something completely bogus like: <!This is foobar>
    // Handle both cases below
    aScanner.CurrentPosition(current);
    beginData = current;
    if (FindCharInReadable('>', current, end)) {
      aScanner.BindSubstring(mComment, beginData, current);
      aScanner.BindSubstring(mCommentDecl, lt, ++current);
      aScanner.SetPosition(current);
      return NS_OK;
    }
  }

  if (aScanner.IsIncremental()) {
    // We got here because we saw the beginning of a comment,
    // but not yet the end, and we are still loading the page. In that
    // case the return value here will cause us to unwind,
    // wait for more content, and try again.
    // XXX For performance reasons we should cache where we were, and
    //     continue from there for next call
    return kEOF;
  }

  // There was no terminating string, parse this comment as text.
  aScanner.SetPosition(lt, false, true);
  return kNotAComment;
}

nsresult
CCommentToken::ConsumeQuirksComment(nsScanner& aScanner)
{
  // <![-[-]] ... [[-]-|--!]>
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          commonly used, but it doesn't really consume them
          per spec (But then, neither does IE or Nav).
   *********************************************************/
  nsScannerIterator end, current;
  aScanner.EndReading(end);
  aScanner.CurrentPosition(current);
  nsScannerIterator beginData = current,
                    beginLastMinus = end,
                    bestAltCommentEnd = end,
                    lt = current;
  lt.advance(-2); // <!

  // When we get here, we have always already consumed <!
  // Skip over possible leading minuses
  if (current != end && *current == kMinus) {
    beginLastMinus = current;
    ++current;
    ++beginData;
    if (current != end && *current == kMinus) { // <!--
      beginLastMinus = current;
      ++current;
      ++beginData;
      // Long form comment

      nsScannerIterator currentEnd = end, gt = end;

      // Find the end of the comment
      while (FindCharInReadable(kGreaterThan, current, currentEnd)) {
        gt = current;
        if (bestAltCommentEnd == end) {
          bestAltCommentEnd = gt;
        }
        --current;
        bool goodComment = false;
        if (current != beginLastMinus && *current == kMinus) { // ->
          --current;
          if (current != beginLastMinus && *current == kMinus) { // -->
            goodComment = true;
            --current;
          }
        } else if (current != beginLastMinus && *current == '!') {
          --current;
          if (current != beginLastMinus && *current == kMinus) {
            --current;
            if (current != beginLastMinus && *current == kMinus) { // --!>
              --current;
              goodComment = true;
            }
          }
        } else if (current == beginLastMinus) {
          goodComment = true;
        }

        if (goodComment) {
          // done
          aScanner.BindSubstring(mComment, beginData, ++current);
          aScanner.BindSubstring(mCommentDecl, lt, ++gt);
          aScanner.SetPosition(gt);
          return NS_OK;
        } else {
          // try again starting after the last '>'
          current = ++gt;
          currentEnd = end;
        }
      }

      if (aScanner.IsIncremental()) {
        // We got here because we saw the beginning of a comment,
        // but not yet the end, and we are still loading the page. In that
        // case the return value here will cause us to unwind,
        // wait for more content, and try again.
        // XXX For performance reasons we should cache where we were, and
        //     continue from there for next call
        return kEOF;
      }

      // If you're here, then we're in a special state.
      // The problem at hand is that we've hit the end of the document without
      // finding the normal endcomment delimiter "-->".  In this case, the
      // first thing we try is to see if we found an alternate endcomment
      // delimiter ">".  If so, rewind just pass that, and use everything up
      // to that point as your comment.  If not, the document has no end
      // comment and should be treated as one big comment.
      gt = bestAltCommentEnd;
      aScanner.BindSubstring(mComment, beginData, gt);
      if (gt != end) {
        ++gt;
      }
      aScanner.BindSubstring(mCommentDecl, lt, gt);
      aScanner.SetPosition(gt);
      return NS_OK;
    }
  }

  // This could be short form of comment
  // Find the end of the comment
  current = beginData;
  if (FindCharInReadable(kGreaterThan, current, end)) {
    nsScannerIterator gt = current;
    if (current != beginData) {
      --current;
      if (current != beginData && *current == kMinus) { // ->
        --current;
        if (current != beginData && *current == kMinus) { // -->
          --current;
        }
      } else if (current != beginData && *current == '!') { // !>
        --current;
        if (current != beginData && *current == kMinus) { // -!>
          --current;
          if (current != beginData && *current == kMinus) { // --!>
            --current;
          }
        }
      }
    }

    if (current != gt) {
      aScanner.BindSubstring(mComment, beginData, ++current);
    } else {
      // Bind mComment to an empty string (note that if current == gt,
      // then current == beginData). We reach this for <!>
      aScanner.BindSubstring(mComment, beginData, current);
    }
    aScanner.BindSubstring(mCommentDecl, lt, ++gt);
    aScanner.SetPosition(gt);
    return NS_OK;
  }

  if (!aScanner.IsIncremental()) {
    // This isn't a comment at all, go back to the < and consume as text.
    aScanner.SetPosition(lt, false, true);
    return kNotAComment;
  }

  // Wait for more data...
  return kEOF;
}

/*
 *  Consume the identifier portion of the comment.
 *  Note that we've already eaten the "<!" portion.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CCommentToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  nsresult result = true;

  if (aFlag & NS_IPARSER_FLAG_STRICT_MODE) {
    // Enabling strict comment parsing for Bug 53011 and 2749 contradicts!
    result = ConsumeStrictComment(aScanner);
  } else {
    result = ConsumeQuirksComment(aScanner);
  }

  if (NS_SUCCEEDED(result)) {
    mNewlineCount = mCommentDecl.CountChar(kNewLine);
  }

  return result;
}

const nsSubstring&
CCommentToken::GetStringValue()
{
  return mComment.AsString();
}

PRInt32
CCommentToken::GetTokenType()
{
  return eToken_comment;
}

CNewlineToken::CNewlineToken()
  : CHTMLToken(eHTMLTag_newline)
{
}

PRInt32
CNewlineToken::GetTokenType()
{
  return eToken_newline;
}

static nsScannerSubstring* gNewlineStr;
void
CNewlineToken::AllocNewline()
{
  gNewlineStr = new nsScannerSubstring(NS_LITERAL_STRING("\n"));
}

void
CNewlineToken::FreeNewline()
{
  if (gNewlineStr) {
    delete gNewlineStr;
    gNewlineStr = nsnull;
  }
}

/**
 *  This method retrieves the value of this internal string.
 *
 *  @return nsString reference to internal string value
 */
const nsSubstring&
CNewlineToken::GetStringValue()
{
  return gNewlineStr->AsString();
}

/*
 * Consume one newline (cr/lf pair).
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CNewlineToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  /*
   * Here's what the HTML spec says about newlines:
   *
   * "A line break is defined to be a carriage return (&#x000D;),
   * a line feed (&#x000A;), or a carriage return/line feed pair.
   * All line breaks constitute white space."
   */

  nsresult rv = NS_OK;
  if (aChar == kCR) {
    PRUnichar theChar;
    rv = aScanner.Peek(theChar);
    if (theChar == kNewLine) {
      rv = aScanner.GetChar(theChar);
    } else if (rv == kEOF && !aScanner.IsIncremental()) {
      // Make sure we don't lose information about this trailing newline.
      rv = NS_OK;
    }
  }

  mNewlineCount = 1;
  return rv;
}

CAttributeToken::CAttributeToken()
  : CHTMLToken(eHTMLTag_unknown)
{
  mHasEqualWithoutValue = false;
}

/*
 * String based constructor
 */
CAttributeToken::CAttributeToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_unknown)
{
  mTextValue.writable().Assign(aName);
  mHasEqualWithoutValue = false;
}

/*
 *  construct initializing data to key value pair
 */
CAttributeToken::CAttributeToken(const nsAString& aKey, const nsAString& aName)
  : CHTMLToken(eHTMLTag_unknown)
{
  mTextValue.writable().Assign(aName);
  mTextKey.Rebind(aKey);
  mHasEqualWithoutValue = false;
}

PRInt32
CAttributeToken::GetTokenType()
{
  return eToken_attribute;
}

const nsSubstring&
CAttributeToken::GetStringValue()
{
  return mTextValue.str();
}

void
CAttributeToken::GetSource(nsString& anOutputString)
{
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

void
CAttributeToken::AppendSourceTo(nsAString& anOutputString)
{
  AppendUnicodeTo(mTextKey, anOutputString);
  if (mTextValue.str().Length() || mHasEqualWithoutValue) {
    anOutputString.AppendLiteral("=");
  }
  anOutputString.Append(mTextValue.str());
  // anOutputString.AppendLiteral(";");
}

/*
 * This general purpose method is used when you want to
 * consume a known quoted string.
 */
static nsresult
ConsumeQuotedString(PRUnichar aChar,
                    nsScannerSharedSubstring& aString,
                    PRInt32& aNewlineCount,
                    nsScanner& aScanner,
                    PRInt32 aFlag)
{
  NS_ASSERTION(aChar == kQuote || aChar == kApostrophe,
               "char is neither quote nor apostrophe");
  // Hold onto this in case this is an unterminated string literal
  PRUint32 origLen = aString.str().Length();

  static const PRUnichar theTerminalCharsQuote[] = {
    PRUnichar(kQuote), PRUnichar('&'), PRUnichar(kCR),
    PRUnichar(kNewLine), PRUnichar(0) };
  static const PRUnichar theTerminalCharsApostrophe[] = {
    PRUnichar(kApostrophe), PRUnichar('&'), PRUnichar(kCR),
    PRUnichar(kNewLine), PRUnichar(0) };
  static const nsReadEndCondition
    theTerminateConditionQuote(theTerminalCharsQuote);
  static const nsReadEndCondition
    theTerminateConditionApostrophe(theTerminalCharsApostrophe);

  // Assume Quote to init to something
  const nsReadEndCondition *terminateCondition = &theTerminateConditionQuote;
  if (aChar == kApostrophe) {
    terminateCondition = &theTerminateConditionApostrophe;
  }

  nsresult result = NS_OK;
  nsScannerIterator theOffset;
  aScanner.CurrentPosition(theOffset);

  result = ConsumeUntil(aString, aNewlineCount, aScanner,
                      *terminateCondition, true, true, aFlag);

  if (NS_SUCCEEDED(result)) {
    result = aScanner.GetChar(aChar); // aChar should be " or '
  }

  // Ref: Bug 35806
  // A back up measure when disaster strikes...
  // Ex <table> <tr d="><td>hello</td></tr></table>
  if (!aString.str().IsEmpty() && aString.str().Last() != aChar &&
      !aScanner.IsIncremental() && result == kEOF) {
    static const nsReadEndCondition
      theAttributeTerminator(kAttributeTerminalChars);
    aString.writable().Truncate(origLen);
    aScanner.SetPosition(theOffset, false, true);
    result = ConsumeUntil(aString, aNewlineCount, aScanner,
                          theAttributeTerminator, false, true, aFlag);
    if (NS_SUCCEEDED(result) && (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      // Remember that this string literal was unterminated.
      result = NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL;
    }
  }
  return result;
}

/*
 * This method is meant to be used by view-source to consume invalid attributes.
 * For the purposes of this method, an invalid attribute is an attribute that
 * starts with either ', ", or /. We consume all ', ", or / and the following
 * whitespace.
 *
 * @param aScanner -- the scanner we're reading our data from.
 * @param aChar -- the character we're skipping
 * @param aCurrent -- the current position that we're looking at.
 * @param aNewlineCount -- a count of the newlines we've consumed.
 * @return error result.
 */
static nsresult
ConsumeInvalidAttribute(nsScanner& aScanner,
                        PRUnichar aChar,
                        nsScannerIterator& aCurrent,
                        PRInt32& aNewlineCount)
{
  NS_ASSERTION(aChar == kApostrophe || aChar == kQuote || aChar == kForwardSlash,
               "aChar must be a quote or apostrophe");
  nsScannerIterator end, wsbeg;
  aScanner.EndReading(end);

  while (aCurrent != end && *aCurrent == aChar) {
    ++aCurrent;
  }

  aScanner.SetPosition(aCurrent);
  return aScanner.ReadWhitespace(wsbeg, aCurrent, aNewlineCount);
}

/*
 * Consume the key and value portions of the attribute.
 */
nsresult
CAttributeToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  nsresult result;
  nsScannerIterator wsstart, wsend;

  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
    result = aScanner.ReadWhitespace(wsstart, wsend, mNewlineCount);
    if (kEOF == result && wsstart != wsend) {
      // Do this here so if this is the final token in the document, we don't
      // lose the whitespace.
      aScanner.BindSubstring(mTextKey, wsstart, wsend);
    }
  } else {
    result = aScanner.SkipWhitespace(mNewlineCount);
  }

  if (NS_OK == result) {
    static const PRUnichar theTerminalsChars[] =
    { PRUnichar(' '), PRUnichar('"'),
      PRUnichar('='), PRUnichar('\n'),
      PRUnichar('\r'), PRUnichar('\t'),
      PRUnichar('>'), PRUnichar('<'),
      PRUnichar('\''), PRUnichar('/'),
      PRUnichar(0) };
    static const nsReadEndCondition theEndCondition(theTerminalsChars);

    nsScannerIterator start, end;
    result = aScanner.ReadUntil(start, end, theEndCondition, false);

    if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      aScanner.BindSubstring(mTextKey, start, end);
    } else if (kEOF == result && wsstart != end) {
      // Capture all of the text (from the beginning of the whitespace to the
      // end of the document).
      aScanner.BindSubstring(mTextKey, wsstart, end);
    }

    // Now it's time to Consume the (optional) value...
    if (NS_OK == result) {
      if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
        result = aScanner.ReadWhitespace(start, wsend, mNewlineCount);
        aScanner.BindSubstring(mTextKey, wsstart, wsend);
      } else {
        result = aScanner.SkipWhitespace(mNewlineCount);
      }

      if (NS_OK == result) {
        // Skip ahead until you find an equal sign or a '>'...
        result = aScanner.Peek(aChar);
        if (NS_OK == result) {
          if (kEqual == aChar) {
            result = aScanner.GetChar(aChar);  // Skip the equal sign...
            if (NS_OK == result) {
              if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                bool haveCR;
                result = aScanner.ReadWhitespace(mTextValue, mNewlineCount,
                                                 haveCR);
              } else {
                result = aScanner.SkipWhitespace(mNewlineCount);
              }

              if (NS_OK == result) {
                result = aScanner.Peek(aChar);  // And grab the next char.
                if (NS_OK == result) {
                  if (kQuote == aChar || kApostrophe == aChar) {
                    aScanner.GetChar(aChar);
                    if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                      mTextValue.writable().Append(aChar);
                    }

                    result = ConsumeQuotedString(aChar, mTextValue,
                                                 mNewlineCount, aScanner,
                                                 aFlag);
                    if (NS_SUCCEEDED(result) &&
                        (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
                      mTextValue.writable().Append(aChar);
                    } else if (result ==
                                NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL) {
                      result = NS_OK;
                      mInError = true;
                    }
                    // According to spec. we ( who? ) should ignore linefeeds.
                    // But look, even the carriage return was getting stripped
                    // ( wonder why! ) - Ref. to bug 15204.  Okay, so the
                    // spec. told us to ignore linefeeds, bug then what about
                    // bug 47535 ? Should we preserve everything then?  Well,
                    // let's make it so!
                  } else if (kGreaterThan == aChar) {
                    mHasEqualWithoutValue = true;
                    mInError = true;
                  } else {
                    static const nsReadEndCondition
                      theAttributeTerminator(kAttributeTerminalChars);
                    result =
                      ConsumeUntil(mTextValue,
                                   mNewlineCount,
                                   aScanner,
                                   theAttributeTerminator,
                                   false,
                                   true,
                                   aFlag);
                  }
                }
                if (NS_OK == result) {
                  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                    bool haveCR;
                    result = aScanner.ReadWhitespace(mTextValue, mNewlineCount,
                                                     haveCR);
                  } else {
                    result = aScanner.SkipWhitespace(mNewlineCount);
                  }
                }
              } else {
                // We saw an equal sign but ran out of room looking for a value.
                mHasEqualWithoutValue = true;
                mInError = true;
              }
            }
          } else {
            // This is where we have to handle fairly busted content.
            // If you're here, it means we saw an attribute name, but couldn't
            // find the following equal sign.  <tag NAME....

            // Doing this right in all cases is <i>REALLY</i> ugly.
            // My best guess is to grab the next non-ws char. We know it's not
            // '=', so let's see what it is. If it's a '"', then assume we're
            // reading from the middle of the value. Try stripping the quote
            // and continuing...  Note that this code also strips forward
            // slashes to handle cases like <tag NAME/>
            if (kQuote == aChar || kApostrophe == aChar ||
                kForwardSlash == aChar) {
              // In XML, a trailing slash isn't an error.
              if (kForwardSlash != aChar || !(aFlag & NS_IPARSER_FLAG_XML)) {
                mInError = true;
              }

              if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
                result = aScanner.SkipOver(aChar); // Strip quote or slash.
                if (NS_SUCCEEDED(result)) {
                  result = aScanner.SkipWhitespace(mNewlineCount);
                }
              } else {
                // We want to collect whitespace here so that following
                // attributes can have the right line number (and for
                // parity with the non-view-source code above).
                result = ConsumeInvalidAttribute(aScanner, aChar,
                                                 wsend, mNewlineCount);

                aScanner.BindSubstring(mTextKey, wsstart, wsend);
                aScanner.SetPosition(wsend);
              }
            }
          }
        }
      }
    }

    if (NS_OK == result) {
      if (mTextValue.str().Length() == 0 && mTextKey.Length() == 0 &&
          mNewlineCount == 0 && !mHasEqualWithoutValue) {
        // This attribute contains no useful information for us, so there is no
        // use in keeping it around. Attributes that are otherwise empty, but
        // have newlines in them are passed on the the DTD so it can get line
        // numbering right.
        return NS_ERROR_HTMLPARSER_BADATTRIBUTE;
      }
    }
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // This is our run-of-the mill "don't lose content at the end of a
    // document" with a slight twist: we don't want to bother returning an
    // empty attribute key, even if this is the end of the document.
    if (mTextKey.Length() == 0) {
      result = NS_ERROR_HTMLPARSER_BADATTRIBUTE;
    } else {
      result = NS_OK;
    }
  }

  return result;
}

void
CAttributeToken::SetKey(const nsAString& aKey)
{
  mTextKey.Rebind(aKey);
}

void
CAttributeToken::BindKey(nsScanner* aScanner,
                         nsScannerIterator& aStart,
                         nsScannerIterator& aEnd)
{
  aScanner->BindSubstring(mTextKey, aStart, aEnd);
}

CWhitespaceToken::CWhitespaceToken()
  : CHTMLToken(eHTMLTag_whitespace)
{
}

CWhitespaceToken::CWhitespaceToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_whitespace)
{
  mTextValue.writable().Assign(aName);
}

PRInt32 CWhitespaceToken::GetTokenType()
{
  return eToken_whitespace;
}

/*
 * This general purpose method is used when you want to
 * consume an aribrary sequence of whitespace.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CWhitespaceToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  // If possible, we'd like to just be a dependent substring starting at
  // |aChar|.  The scanner has already been advanced, so we need to
  // back it up to facilitate this.

  nsScannerIterator start;
  aScanner.CurrentPosition(start);
  aScanner.SetPosition(--start, false, true);

  bool haveCR;

  nsresult result = aScanner.ReadWhitespace(mTextValue, mNewlineCount, haveCR);

  if (result == kEOF && !aScanner.IsIncremental()) {
    // Oops, we ran off the end, make sure we don't lose the trailing
    // whitespace!
    result = NS_OK;
  }

  if (NS_OK == result && haveCR) {
    mTextValue.writable().StripChar(kCR);
  }
  return result;
}

const nsSubstring&
CWhitespaceToken::GetStringValue()
{
  return mTextValue.str();
}

CEntityToken::CEntityToken()
  : CHTMLToken(eHTMLTag_entity)
{
}

CEntityToken::CEntityToken(const nsAString& aName)
  : CHTMLToken(eHTMLTag_entity)
{
  mTextValue.Assign(aName);
}


/*
 *  Consume the rest of the entity. We've already eaten the "&".
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CEntityToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  nsresult result = ConsumeEntity(aChar, mTextValue, aScanner);
  return result;
}

PRInt32
CEntityToken::GetTokenType()
{
  return eToken_entity;
}

/*
 * This general purpose method is used when you want to
 * consume an entity &xxxx;. Keep in mind that entities
 * are <i>not</i> reduced inline.
 *
 * @param   aChar -- last char consumed from stream
 * @param   aScanner -- controller of underlying input source
 * @return  error result
 */
nsresult
CEntityToken::ConsumeEntity(PRUnichar aChar,
                            nsString& aString,
                            nsScanner& aScanner)
{
  nsresult result = NS_OK;
  if (kLeftBrace == aChar) {
    // You're consuming a script entity...
    aScanner.GetChar(aChar); // Consume &

    PRInt32 rightBraceCount = 0;
    PRInt32 leftBraceCount  = 0;

    do {
      result = aScanner.GetChar(aChar);

      if (NS_FAILED(result)) {
        return result;
      }

      aString.Append(aChar);
      if (aChar == kRightBrace) {
        ++rightBraceCount;
      } else if (aChar == kLeftBrace) {
        ++leftBraceCount;
      }
    } while (leftBraceCount != rightBraceCount);
  } else {
    PRUnichar theChar = 0;
    if (kHashsign == aChar) {
      result = aScanner.Peek(theChar, 2);

      if (NS_FAILED(result)) {
        if (kEOF == result && !aScanner.IsIncremental()) {
          // If this is the last buffer then we are certainly
          // not dealing with an entity. That's, there are
          // no more characters after &#. Bug 188278.
          return NS_HTMLTOKENS_NOT_AN_ENTITY;
        }
        return result;
      }

      if (nsCRT::IsAsciiDigit(theChar)) {
        aScanner.GetChar(aChar); // Consume &
        aScanner.GetChar(aChar); // Consume #
        aString.Assign(aChar);
        result = aScanner.ReadNumber(aString, 10);
      } else if (theChar == 'x' || theChar == 'X') {
        aScanner.GetChar(aChar);   // Consume &
        aScanner.GetChar(aChar);   // Consume #
        aScanner.GetChar(theChar); // Consume x
        aString.Assign(aChar);
        aString.Append(theChar);
        result = aScanner.ReadNumber(aString, 16);
      } else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY;
      }
    } else {
      result = aScanner.Peek(theChar, 1);

      if (NS_FAILED(result)) {
        return result;
      }

      if (nsCRT::IsAsciiAlpha(theChar) ||
        theChar == '_' ||
        theChar == ':') {
        aScanner.GetChar(aChar); // Consume &
        result = aScanner.ReadEntityIdentifier(aString);
      } else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY;
      }
    }
  }

  if (NS_FAILED(result)) {
    return result;
  }

  result = aScanner.Peek(aChar);

  if (NS_FAILED(result)) {
    return result;
  }

  if (aChar == kSemicolon) {
    // Consume semicolon that stopped the scan
    aString.Append(aChar);
    result = aScanner.GetChar(aChar);
  }

  return result;
}

/**
 * Map some illegal but commonly used numeric entities into their
 * appropriate unicode value.
 */
#define NOT_USED 0xfffd

static const PRUint16 PA_HackTable[] = {
	0x20ac,  /* EURO SIGN */
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	0x017D,  /* LATIN CAPITAL LETTER Z WITH CARON */
	NOT_USED,
	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	0x017E,  /* LATIN SMALL LETTER Z WITH CARON */
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};

static void
AppendNCR(nsSubstring& aString, PRInt32 aNCRValue)
{
  /* For some illegal, but popular usage */
  if (aNCRValue >= 0x0080 && aNCRValue <= 0x009f) {
    aNCRValue = PA_HackTable[aNCRValue - 0x0080];
  }

  AppendUCS4ToUTF16(ENSURE_VALID_CHAR(aNCRValue), aString);
}

/*
 * This method converts this entity into its underlying
 * unicode equivalent.
 *
 *  @param   aString will hold the resulting string value
 *  @return  numeric (unichar) value
 */
PRInt32
CEntityToken::TranslateToUnicodeStr(nsString& aString)
{
  PRInt32 value = 0;

  if (mTextValue.Length() > 1) {
    PRUnichar theChar0 = mTextValue.CharAt(0);

    if (kHashsign == theChar0) {
      PRInt32 err = 0;

      value = mTextValue.ToInteger(&err, kAutoDetect);

      if (0 == err) {
        AppendNCR(aString, value);
      }
    } else {
      value = nsHTMLEntities::EntityToUnicode(mTextValue);
      if (-1 < value) {
        // We found a named entity...
        aString.Assign(PRUnichar(value));
      }
    }
  }

  return value;
}


const
nsSubstring& CEntityToken::GetStringValue()
{
  return mTextValue;
}

void
CEntityToken::GetSource(nsString& anOutputString)
{
  anOutputString.AppendLiteral("&");
  anOutputString += mTextValue;
  // Any possible ; is part of our text value.
}

void
CEntityToken::AppendSourceTo(nsAString& anOutputString)
{
  anOutputString.AppendLiteral("&");
  anOutputString += mTextValue;
  // Any possible ; is part of our text value.
}

const PRUnichar*
GetTagName(PRInt32 aTag)
{
  const PRUnichar *result = nsHTMLTags::GetStringValue((nsHTMLTag) aTag);

  if (result) {
    return result;
  }

  if (aTag >= eHTMLTag_userdefined) {
    return sUserdefined;
  }

  return 0;
}


CInstructionToken::CInstructionToken()
  : CHTMLToken(eHTMLTag_instruction)
{
}

CInstructionToken::CInstructionToken(const nsAString& aString)
  : CHTMLToken(eHTMLTag_unknown)
{
  mTextValue.Assign(aString);
}

nsresult
CInstructionToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  mTextValue.AssignLiteral("<?");
  nsresult result = NS_OK;
  bool done = false;

  while (NS_OK == result && !done) {
    // Note, this call does *not* consume the >.
    result = aScanner.ReadUntil(mTextValue, kGreaterThan, false);
    if (NS_SUCCEEDED(result)) {
      // In HTML, PIs end with a '>', in XML, they end with a '?>'. Cover both
      // cases here.
      if (!(aFlag & NS_IPARSER_FLAG_XML) ||
          kQuestionMark == mTextValue.Last()) {
        // This really is the end of the PI.
        done = true;
      }
      // Need to append this character no matter what.
      aScanner.GetChar(aChar);
      mTextValue.Append(aChar);
    }
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // Hide the EOF result because there is no more text coming.
    mInError = true;
    result = NS_OK;
  }

  return result;
}

PRInt32
CInstructionToken::GetTokenType()
{
  return eToken_instruction;
}

const nsSubstring&
CInstructionToken::GetStringValue()
{
  return mTextValue;
}

// Doctype decl token

CDoctypeDeclToken::CDoctypeDeclToken(eHTMLTags aTag)
  : CHTMLToken(aTag)
{
}

CDoctypeDeclToken::CDoctypeDeclToken(const nsAString& aString, eHTMLTags aTag)
  : CHTMLToken(aTag), mTextValue(aString)
{
}

/**
 *  This method consumes a doctype element.
 *  Note: I'm rewriting this method to seek to the first <, since quotes can
 *  really screw us up.
 *  XXX Maybe this should do better in XML or strict mode?
 */
nsresult
CDoctypeDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner, PRInt32 aFlag)
{
  static const PRUnichar terminalChars[] =
  { PRUnichar('>'), PRUnichar('<'),
    PRUnichar(0)
  };
  static const nsReadEndCondition theEndCondition(terminalChars);

  nsScannerIterator start, end;

  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);

  nsresult result = aScanner.ReadUntil(start, end, theEndCondition, false);

  if (NS_SUCCEEDED(result)) {
    PRUnichar ch;
    aScanner.Peek(ch);
    if (ch == kGreaterThan) {
      // Include '>' but not '<' since '<'
      // could belong to another tag.
      aScanner.GetChar(ch);
      end.advance(1);
    } else {
      NS_ASSERTION(kLessThan == ch,
                   "Make sure this doctype decl. is really in error.");
      mInError = true;
    }
  } else if (!aScanner.IsIncremental()) {
    // We have reached the document end but haven't
    // found either a '<' or a '>'. Therefore use
    // whatever we have.
    mInError = true;
    result = NS_OK;
  }

  if (NS_SUCCEEDED(result)) {
    start.advance(-2); // Make sure to consume <!
    CopyUnicodeTo(start, end, mTextValue);
  }

  return result;
}

PRInt32
CDoctypeDeclToken::GetTokenType()
{
  return eToken_doctypeDecl;
}

const nsSubstring&
CDoctypeDeclToken::GetStringValue()
{
  return mTextValue;
}

void
CDoctypeDeclToken::SetStringValue(const nsAString& aStr)
{
  mTextValue.Assign(aStr);
}
