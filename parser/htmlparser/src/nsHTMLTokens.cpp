/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <ctype.h> 
#include <time.h>
#include <stdio.h>  
#include "nsScanner.h"
#include "nsToken.h"
#include "nsIAtom.h"
#include "nsHTMLTokens.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"
#include "nsHTMLEntities.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsScanner.h"


static const PRUnichar sUserdefined[] = {'u', 's', 'e', 'r', 'd', 'e', 'f',
                                         'i', 'n', 'e', 'd', 0};

static const PRUnichar kAttributeTerminalChars[] = {
  PRUnichar('&'), PRUnichar('\b'), PRUnichar('\t'), 
  PRUnichar('\n'), PRUnichar('\r'), PRUnichar(' '),  
  PRUnichar('>'),  
  PRUnichar(0) 
};
                   

/**************************************************************
  And now for the token classes...
 **************************************************************/

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(eHTMLTags aTag) : CToken(aTag) {
}


CHTMLToken::~CHTMLToken() {

}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CStartToken::CStartToken(eHTMLTags aTag) : CHTMLToken(aTag) {
  mEmpty=PR_FALSE;
  mContainerInfo=eFormUnknown;
#ifdef DEBUG
  mAttributed = PR_FALSE;
#endif
}

CStartToken::CStartToken(const nsAString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mEmpty=PR_FALSE;
  mContainerInfo=eFormUnknown;
  mTextValue.Assign(aName);
#ifdef DEBUG
  mAttributed = PR_FALSE;
#endif
}

CStartToken::CStartToken(const nsAString& aName,eHTMLTags aTag) : CHTMLToken(aTag) {
  mEmpty=PR_FALSE;
  mContainerInfo=eFormUnknown;
  mTextValue.Assign(aName);
#ifdef DEBUG
  mAttributed = PR_FALSE;
#endif
}

/*
 *  This method returns the typeid (the tag type) for this token.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTokenType(void) {
  return eToken_start;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CStartToken::SetEmpty(PRBool aValue) {
  mEmpty=aValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRBool CStartToken::IsEmpty(void) {
  return mEmpty;
}


/*
 *  Consume the identifier portion of the start tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CStartToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

  //if you're here, we've already Consumed the < char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  nsresult result=NS_OK;
  if (aFlag & NS_IPARSER_FLAG_HTML) {
    nsAutoString theSubstr;
    result=aScanner.ReadTagIdentifier(theSubstr);
    mTypeID = (PRInt32)nsHTMLTags::LookupTag(theSubstr);
    // Save the original tag string if this is user-defined or if we
    // are viewing source
    if(eHTMLTag_userdefined==mTypeID || (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      mTextValue=theSubstr;
    }
  }
  else {
    //added PR_TRUE to readId() call below to fix bug 46083. The problem was that the tag given
    //was written <title_> but since we didn't respect the '_', we only saw <title>. Then 
    //we searched for end title, which never comes (they give </title_>). 

    result=aScanner.ReadTagIdentifier(mTextValue);  
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  if (NS_SUCCEEDED(result) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
    result = aScanner.SkipWhitespace(mNewlineCount);
  }

  return result;
}


const nsAString& CStartToken::GetStringValue()
{
  if((eHTMLTag_unknown<mTypeID) && (mTypeID<eHTMLTag_text)) {
    if(!mTextValue.Length()) {
      mTextValue.Assign(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CStartToken::GetSource(nsString& anOutputString){
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CStartToken::AppendSourceTo(nsAString& anOutputString){
  anOutputString.Append(PRUnichar('<'));
  /*
   * Watch out for Bug 15204 
   */
  if(!mTrailingContent.IsEmpty())
    anOutputString.Append(mTrailingContent);
  else {
    if(!mTextValue.IsEmpty())
      anOutputString.Append(mTextValue);
    else
     anOutputString.Append(GetTagName(mTypeID));
    anOutputString.Append(PRUnichar('>'));
  }
}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CEndToken::CEndToken(eHTMLTags aTag) : CHTMLToken(aTag) {
}

CEndToken::CEndToken(const nsAString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
}

CEndToken::CEndToken(const nsAString& aName,eHTMLTags aTag) : CHTMLToken(aTag) {
  mTextValue.Assign(aName);
}

/*
 *  Consume the identifier portion of the end tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CEndToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) 
{
  nsresult result = NS_OK;
  if (aFlag & NS_IPARSER_FLAG_HTML) {
    nsAutoString theSubstr;
    result=aScanner.ReadTagIdentifier(theSubstr);
    NS_ENSURE_SUCCESS(result, result);
    
    mTypeID = (PRInt32)nsHTMLTags::LookupTag(theSubstr);
    // Save the original tag string if this is user-defined or if we
    // are viewing source
    if(eHTMLTag_userdefined==mTypeID ||
       (aFlag & (NS_IPARSER_FLAG_VIEW_SOURCE | NS_IPARSER_FLAG_PRESERVE_CONTENT))) {
      mTextValue=theSubstr;
    }
  }
  else {
    result = aScanner.ReadTagIdentifier(mTextValue);
    NS_ENSURE_SUCCESS(result, result);

    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
    result = aScanner.SkipWhitespace(mNewlineCount);
    NS_ENSURE_SUCCESS(result, result);
  }

  return result;
}


/*
 *  Asks the token to determine the <i>HTMLTag type</i> of
 *  the token. This turns around and looks up the tag name
 *  in the tag dictionary.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  eHTMLTag id of this endtag
 */
PRInt32 CEndToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
    switch(mTypeID) {
      case eHTMLTag_dir:
      case eHTMLTag_menu:
        mTypeID=eHTMLTag_ul;
        break;
      default:
        break;
    }
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEndToken::GetTokenType(void) {
  return eToken_end;
}

const nsAString& CEndToken::GetStringValue()
{
  if((eHTMLTag_unknown<mTypeID) && (mTypeID<eHTMLTag_text)) {
    if(!mTextValue.Length()) {
      mTextValue.Assign(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CEndToken::GetSource(nsString& anOutputString){
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CEndToken::AppendSourceTo(nsAString& anOutputString){
  anOutputString.AppendLiteral("</");
  if(!mTextValue.IsEmpty())
    anOutputString.Append(mTextValue);
  else
    anOutputString.Append(GetTagName(mTypeID));
  anOutputString.Append(PRUnichar('>'));
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken() : CHTMLToken(eHTMLTag_text) {
}


/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken(const nsAString& aName) : CHTMLToken(eHTMLTag_text) {
  mTextValue.Rebind(aName);
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CTextToken::GetTokenType(void) {
  return eToken_text;
}

PRInt32 CTextToken::GetTextLength(void) {
  return mTextValue.Length();
}

/*
 *  Consume as much clear text from scanner as possible.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('&'), PRUnichar('<'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;
  nsScannerIterator origin, start, end;
  
  // Start scanning after the first character, because we know it to
  // be part of this text token (we wouldn't have come here if it weren't)
  aScanner.CurrentPosition(origin);
  start = origin;
  ++start;
  aScanner.SetPosition(start);
  aScanner.EndReading(end);

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);

      if(((kCR==aChar) || (kNewLine==aChar)) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the char
        PRUnichar theNextChar;
        result=aScanner.Peek(theNextChar);    //then see what's next.
        switch(aChar) {
          case kCR:
            // result=aScanner.GetChar(aChar);       
            if(kLF==theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and 
              // let it be ignored by the layout system
              end.advance(2);
              result=aScanner.GetChar(theNextChar);
            }
            else {
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
        } //switch
      }
      else done=PR_TRUE;
    }
  }
  
  aScanner.BindSubstring(mTextValue, origin, end);

  return result;
}

/*
 *  Consume as much clear text from scanner as possible.
 *  The scanner is left on the < of the perceived end tag.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::ConsumeUntil(PRUnichar aChar,PRBool aIgnoreComments,nsScanner& aScanner,
                                  const nsAString& aEndTagName,PRInt32 aFlag,
                                  PRBool& aFlushTokens){
  nsresult      result=NS_OK;
  nsScannerIterator theStartOffset, theCurrOffset, theTermStrPos, theStartCommentPos, theAltTermStrPos, endPos;
  PRBool        done=PR_FALSE;
  PRBool        theLastIteration=PR_FALSE;

  aScanner.CurrentPosition(theStartOffset);
  theCurrOffset = theStartOffset;
  aScanner.EndReading(endPos);
  theTermStrPos = theStartCommentPos = theAltTermStrPos = endPos;

  // ALGORITHM: *** The performance is based on correctness of the document ***
  // 1. Look for a '<' character.  This could be
  //    a) Start of a comment (<!--), b) Start of the terminal string, or c) a start of a tag.
  //    We are interested in a) and b). c) is ignored because in CDATA we don't care for tags.
  //    NOTE: Technically speaking in CDATA we should ignore the comments too!! But for compatibility
  //          we don't.
  // 2. Having the offset, for '<', search for the terminal string from there on and record its offset.
  // 3. From the same '<' offset also search for start of a comment '<!--'. If found search for
  //    end comment '-->' between the terminal string and '<!--'.  If you did not find the end
  //    comment, then we have a malformed document, i.e., this section has a prematured terminal string
  //    Ex. <SCRIPT><!-- document.write('</SCRIPT>') //--> </SCRIPT>. But record terminal string's
  //    offset if this is the first premature terminal string, and update the current offset to the terminal 
  //    string (prematured) offset and goto step 1.
  // 4. Amen...If you found a terminal string and '-->'. Otherwise goto step 1.
  // 5. If the end of the document is reached and if we still don't have the condition in step 4. then
  //    assume that the prematured terminal string is the actual terminal string and goto step 1. This
  //    will be our last iteration.

  NS_NAMED_LITERAL_STRING(ltslash, "</");
  const nsString theTerminalString = ltslash + aEndTagName;

  PRUint32 termStrLen=theTerminalString.Length();
  while((result == NS_OK) && !done) {
    PRBool found = PR_FALSE;
    nsScannerIterator gtOffset,ltOffset = theCurrOffset;
    while (FindCharInReadable(PRUnichar(kLessThan), ltOffset, endPos) &&
           ((PRUint32)ltOffset.size_forward() >= termStrLen ||
            Distance(ltOffset, endPos) >= termStrLen)) {
      // Make a copy of the (presumed) end tag and
      // do a case-insensitive comparison

      nsScannerIterator start(ltOffset), end(ltOffset);
      end.advance(termStrLen);

      if (CaseInsensitiveFindInReadable(theTerminalString,start,end) && 
          end != endPos && (*end == '>'  || *end == ' '  || 
                            *end == '\t' || *end == '\n' || 
                            *end == '\r' || *end == '\b')) {
        gtOffset = end;
        if (FindCharInReadable(PRUnichar(kGreaterThan), gtOffset, endPos)) {
          found = PR_TRUE;
          theTermStrPos = start;
        }
        break;
      }
      ltOffset.advance(1);
    }
     
    if (found && theTermStrPos != endPos) {
      if(!(aFlag & NS_IPARSER_FLAG_STRICT_MODE) &&
         !theLastIteration && !aIgnoreComments) {
        nsScannerIterator endComment(ltOffset);
        endComment.advance(5);
         
        if ((theStartCommentPos == endPos) &&
            FindInReadable(NS_LITERAL_STRING("<!--"), theCurrOffset, endComment)) {
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
      aFlushTokens=PR_TRUE;
      done = PR_TRUE;
    }
    else {
      // We end up here if:
      // a) when the buffer runs out ot data.
      // b) when the terminal string is not found.
      if(!aScanner.IsIncremental()) {
        if(theAltTermStrPos != endPos) {
          // If you're here it means..we hit the rock bottom and therefore switch to plan B.
          theCurrOffset = theAltTermStrPos;
          theLastIteration = PR_TRUE;
        }
        else {
          done = PR_TRUE; // Do this to fix Bug. 35456
        }
      }
      else {
       result=kEOF;
      }
    }
  }
  return result;
}

void CTextToken::CopyTo(nsAString& aStr)
{
  nsScannerIterator start, end;
  mTextValue.BeginReading(start);
  mTextValue.EndReading(end);
  CopyUnicodeTo(start, end, aStr);
}

const nsAString& CTextToken::GetStringValue(void)
{
  return mTextValue.AsString();
}

void CTextToken::Bind(nsScanner* aScanner, nsScannerIterator& aStart, nsScannerIterator& aEnd)
{
  aScanner->BindSubstring(mTextValue, aStart, aEnd);
}

void CTextToken::Bind(const nsAString& aStr)
{
  mTextValue.Rebind(aStr);
}

/*
 *  default constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken(eHTMLTags aTag) : CHTMLToken(aTag) {
}


/*
 *  string based constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken(const nsAString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
}

/*
 *  
 *  @update  vidur 11/12/98
 *  @param   
 *  @return  
 */
PRInt32 CCDATASectionToken::GetTokenType(void) {
  return eToken_cdatasection;
}

/*
 *  Consume as much marked test from scanner as possible.
 *
 *  @update  rgess 12/15/99: had to handle case: "<![ ! IE 5]>", in addition to "<![..[..]]>".
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCDATASectionToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
  { PRUnichar('\r'), PRUnichar('\n'), PRUnichar(']'), PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,theEndCondition,PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      if((kCR==aChar) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the \r
        result=aScanner.Peek(aChar);    //then see what's next.
        if(NS_OK==result) {
          switch(aChar) {
            case kCR:
              result=aScanner.GetChar(aChar); //strip off the \r
              mTextValue.AppendLiteral("\n\n");
              mNewlineCount += 2;
              break;
            case kNewLine:
               //which means we saw \r\n, which becomes \n
              result=aScanner.GetChar(aChar); //strip off the \n
                  //now fall through on purpose...
            default:
              mTextValue.AppendLiteral("\n");
              mNewlineCount++;
              break;
          } //switch
        } //if
      }
      else if (kNewLine == aChar) {
        result=aScanner.GetChar(aChar);
        mTextValue.Append(aChar);
        ++mNewlineCount;
      }
      else if (kRightSquareBracket == aChar) {        
        result=aScanner.GetChar(aChar); //strip off the ]
        mTextValue.Append(aChar);
        result=aScanner.Peek(aChar);    //then see what's next.
        if((NS_OK==result) && (kRightSquareBracket==aChar)) {
          result=aScanner.GetChar(aChar); //strip off the second ]
          mTextValue.Append(aChar);
        }
        // The goal here is to not lose data from the page when encountering
        // markup like: <![endif]-->.  This means that in normal parsing, we
        // allow ']' to end the marked section and just drop everything between
        // it an the '>'.  In view-source mode, we cannot drop things on the
        // floor like that.  In fact, to make view-source of XML with script in
        // CDATA sections at all bearable, we need to somewhat enforce the ']>'
        // terminator for marked sections.  So make the tokenization somewhat
        // different when in view-source _and_ dealing with a CDATA section.
        PRBool inCDATA = (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) &&
          StringBeginsWith(mTextValue, NS_LITERAL_STRING("[CDATA["));
        if (inCDATA) {
          result = aScanner.Peek(aChar);
        } else {
          nsAutoString dummy; // skip any bad data
          result=aScanner.ReadUntil(dummy,kGreaterThan,PR_FALSE);
        }
        if (NS_OK==result &&
            (!inCDATA || kGreaterThan == aChar)) {
          result=aScanner.GetChar(aChar); //strip off the >

          // XXX take me out when view source isn't stupid anymore
          if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
            mTextValue.Append(aChar);
          }
          done=PR_TRUE;
        }
      }
      else done=PR_TRUE;
    }
  }

  if (kEOF == result && !aScanner.IsIncremental()) {
    // We ran out of space looking for the end of this CDATA section.
    // In order to not completely lose the entire section, treat everything
    // until the end of the document as part of the CDATA section and let
    // the DTD handle it.
    // XXX when view source actually displays errors, we'll need to propagate
    // the EOF down to it (i.e., not do this if we're viewing source).
    result = NS_OK;
  }

  return result;
}

const nsAString& CCDATASectionToken::GetStringValue(void)
{
  return mTextValue;
}


/*
 *  default constructor
 *  
 *  @param   aName -- string to init token name with
 *  @return  
 */
CMarkupDeclToken::CMarkupDeclToken() : CHTMLToken(eHTMLTag_markupDecl) {
}


/*
 *  string based constructor
 *  
 *  @param   aName -- string to init token name with
 *  @return  
 */
CMarkupDeclToken::CMarkupDeclToken(const nsAString& aName) : CHTMLToken(eHTMLTag_markupDecl) {
  mTextValue.Rebind(aName);
}


/*
 *  
 *  @param   
 *  @return  
 */
PRInt32 CMarkupDeclToken::GetTokenType(void) {
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
nsresult CMarkupDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('\''), PRUnichar('"'),
      PRUnichar('>'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;
  PRUnichar quote=0;

  nsScannerIterator origin, start, end;
  aScanner.CurrentPosition(origin);
  start = origin;

  while((NS_OK==result) && (!done)) {
    aScanner.SetPosition(start);
    result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);

      if(NS_OK==result) {
        PRUnichar theNextChar=0;
        if ((kCR==aChar) || (kNewLine==aChar)) {
          result=aScanner.GetChar(aChar); //strip off the char
          result=aScanner.Peek(theNextChar);    //then see what's next.
        }
        switch(aChar) {
          case kCR:
            // result=aScanner.GetChar(aChar);       
            if(kLF==theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and 
              // let it be ignored by the layout system
              end.advance(2);
              result=aScanner.GetChar(theNextChar);
            }
            else {
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
              ++start;  // Note that start is wrong after this, we just avoid temp var
              aScanner.SetPosition(start); // Skip the >
              done=PR_TRUE;
            }
            break;
          default:
            NS_ABORT_IF_FALSE(0,"should not happen, switch is missing cases?");
            break;
        } //switch
        start = end;
      }
      else done=PR_TRUE;
    } // if read until !ok
  } // while
  
  aScanner.BindSubstring(mTextValue, origin, end);

  return result;
}

const nsAString& CMarkupDeclToken::GetStringValue(void)
{
  return mTextValue.AsString();
}


/*
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCommentToken::CCommentToken() : CHTMLToken(eHTMLTag_comment) {
}


/*
 *  Copy constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CCommentToken::CCommentToken(const nsAString& aName) : CHTMLToken(eHTMLTag_comment) {
  mComment.Rebind(aName);
}

void CCommentToken::AppendSourceTo(nsAString& anOutputString){
  AppendUnicodeTo(mCommentDecl, anOutputString);
}

static PRBool IsCommentEnd(
  const nsScannerIterator& aCurrent, 
  const nsScannerIterator& aEnd, 
  nsScannerIterator& aGt)
{
  nsScannerIterator current = aCurrent;
  PRInt32 dashes = 0;

  while ((current != aEnd) && (dashes != 2)) {
    if (*current == kGreaterThan) {
      aGt = current;
      return PR_TRUE;
    }
    if (*current == PRUnichar('-')) {
      ++dashes;
    } else {
      dashes = 0;
    }
    ++current;
  }

  return PR_FALSE;
}

nsresult CCommentToken::ConsumeStrictComment(nsScanner& aScanner) 
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

  // Regular comment must start with <!--
  if (current != end && *current == kMinus &&
      ++current != end && *current == kMinus &&
      ++current != end) {
    nsScannerIterator currentEnd = end;
    PRBool balancedComment = PR_FALSE;
    static NS_NAMED_LITERAL_STRING(dashes,"--");
    beginData = current;

    while (FindInReadable(dashes, current, currentEnd)) {
      current.advance(2);

      balancedComment = !balancedComment; // We need to match '--' with '--'
    
      if (balancedComment && IsCommentEnd(current, end, gt)) {
        // done
        current.advance(-2);
        if (beginData != current) { // protects from <!---->
          aScanner.BindSubstring(mComment, beginData, current);
        }
        aScanner.BindSubstring(mCommentDecl, lt, ++gt);
        aScanner.SetPosition(gt);
        return NS_OK;
      } else {
        // Continue after the last '--'
        currentEnd = end;
      }
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
    return kEOF; // not really an nsresult, but...
  }

  // XXX We should return kNotAComment, parse comment open as text, and parse
  //     the rest of the document normally. Now we ALMOST do that: <! is
  //     missing from the content model.
  return NS_OK;
}

nsresult CCommentToken::ConsumeQuirksComment(nsScanner& aScanner) 
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
        PRBool goodComment = PR_FALSE;
        if (current != beginLastMinus && *current == kMinus) { // ->
          --current;
          if (current != beginLastMinus && *current == kMinus) { // -->
            goodComment = PR_TRUE;
            --current;
          }
        } else if (current != beginLastMinus && *current == '!') {
          --current;
          if (current != beginLastMinus && *current == kMinus) {
            --current;
            if (current != beginLastMinus && *current == kMinus) { // --!>
              --current;
              goodComment = PR_TRUE;
            }
          }
        } else if (current == beginLastMinus) {
          goodComment = PR_TRUE;
        }
    
        if (goodComment) {
          // done
          if (beginLastMinus != current) { // protects from <!---->
            aScanner.BindSubstring(mComment, beginData, ++current);
          }
          aScanner.BindSubstring(mCommentDecl, lt, ++gt);
          aScanner.SetPosition(gt);
          return NS_OK;
        } else {
          // try again starting after the last '>'
          current = ++gt;
          currentEnd = end;
        }
      } //while
  
      if (aScanner.IsIncremental()) {
        // We got here because we saw the beginning of a comment,
        // but not yet the end, and we are still loading the page. In that
        // case the return value here will cause us to unwind,
        // wait for more content, and try again.
        // XXX For performance reasons we should cache where we were, and
        //     continue from there for next call
        return kEOF;  // not really an nsresult, but...
      }

      // If you're here, then we're in a special state. 
      // The problem at hand is that we've hit the end of the document without finding the normal endcomment delimiter "-->".
      // In this case, the first thing we try is to see if we found an alternate endcomment delimiter ">".
      // If so, rewind just pass that, and use everything up to that point as your comment.
      // If not, the document has no end comment and should be treated as one big comment.
      gt = bestAltCommentEnd;
      if (beginData != gt) { // protects from <!-->
        aScanner.BindSubstring(mComment, beginData, gt);
      }
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
    }
    aScanner.BindSubstring(mCommentDecl, lt, ++gt);
    aScanner.SetPosition(gt);
    return NS_OK;
  }

  return kEOF; // not really an nsresult, but...
}

/*
 *  Consume the identifier portion of the comment. 
 *  Note that we've already eaten the "<!" portion.
 *  
 *  @update  gess 16June2000
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCommentToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  nsresult result=PR_TRUE;
  
  if (aFlag & NS_IPARSER_FLAG_STRICT_MODE) {
    //Enabling strict comment parsing for Bug 53011 and  2749 contradicts!!!!
    result = ConsumeStrictComment(aScanner);
  }
  else {
    result = ConsumeQuirksComment(aScanner);
  }

  if (NS_SUCCEEDED(result)) {
    mNewlineCount = !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) ? mCommentDecl.CountChar(kNewLine) : -1;
  }

  return result;
}

const nsAString& CCommentToken::GetStringValue(void)
{
  return mComment.AsString();
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CCommentToken::GetTokenType(void) {
  return eToken_comment;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CNewlineToken::CNewlineToken() : CHTMLToken(eHTMLTag_newline) {
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CNewlineToken::GetTokenType(void) {
  return eToken_newline;
}


static nsScannerSubstring* gNewlineStr;
void CNewlineToken::AllocNewline()
{
  gNewlineStr = new nsScannerSubstring(NS_LITERAL_STRING("\n"));
}

void CNewlineToken::FreeNewline()
{
  if (gNewlineStr) {
    delete gNewlineStr;
    gNewlineStr = nsnull;
  }
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
const nsAString& CNewlineToken::GetStringValue(void) {
  return gNewlineStr->AsString();
}

/*
 *  Consume as many cr/lf pairs as you can find.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CNewlineToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

/*******************************************************************

  Here's what the HTML spec says about newlines:

  "A line break is defined to be a carriage return (&#x000D;), 
   a line feed (&#x000A;), or a carriage return/line feed pair. 
   All line breaks constitute white space."

 *******************************************************************/

  PRUnichar theChar;
  nsresult result=aScanner.Peek(theChar);

  if(NS_OK==result) {
    switch(aChar) {
      case kNewLine:
        if(kCR==theChar) {
          result=aScanner.GetChar(theChar);
        }
        break;
      case kCR: 
          //convert CRLF into just CR
        if(kNewLine==theChar) {
          result=aScanner.GetChar(theChar);
        }
        break;
      default:
        break;
    }
  }

  mNewlineCount = 1;
  return result;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken() : CHTMLToken(eHTMLTag_unknown) {
  mHasEqualWithoutValue=PR_FALSE;
#ifdef DEBUG
  mLastAttribute = PR_FALSE;
#endif
}

/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsAString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
  mHasEqualWithoutValue=PR_FALSE;
#ifdef DEBUG
  mLastAttribute = PR_FALSE;
#endif
}

/*
 *  construct initializing data to 
 *  key value pair
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsAString& aKey, const nsAString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
  mTextKey.Rebind(aKey);
  mHasEqualWithoutValue=PR_FALSE;
#ifdef DEBUG
  mLastAttribute = PR_FALSE;
#endif
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CAttributeToken::GetTokenType(void) {
  return eToken_attribute;
}

/*
 *  Removes non-alpha-non-digit characters from the end of a KEY
 *  
 *  @update harishd 07/15/99
 *  @param  
 *  @return  
 */
void CAttributeToken::SanitizeKey() {
  PRInt32   length=mTextKey.Length();
  if(length > 0) {
    nsScannerIterator iter, begin, end;
    mTextKey.BeginReading(begin);
    mTextKey.EndReading(end);
    iter = end;

    // Look for the first legal character starting from
    // the end of the string
    do {
      --iter;
    } while (!nsCRT::IsAsciiAlpha(*iter) && 
             !nsCRT::IsAsciiDigit(*iter) && 
             (iter != begin));
    
    // If there were any illegal characters, just copy out the
    // legal part
    if (iter != --end) {
      nsAutoString buf;
      CopyUnicodeTo(begin, ++iter, buf);
      mTextKey.Rebind(buf);
    }
  }

  return;
}

const nsAString& CAttributeToken::GetKey(void)
{
  return mTextKey.AsString();
}

const nsAString& CAttributeToken::GetStringValue(void)
{
  return mTextValue;
}
 
/*
 *  
 *  
 *  @update  rickg  6June2000
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CAttributeToken::GetSource(nsString& anOutputString){
  anOutputString.Truncate();
  AppendSourceTo(anOutputString);
}

/*
 *  
 *  
 *  @update  rickg  6June2000
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CAttributeToken::AppendSourceTo(nsAString& anOutputString){
  AppendUnicodeTo(mTextKey, anOutputString);
  if(mTextValue.Length() || mHasEqualWithoutValue) 
    anOutputString.AppendLiteral("=");
  anOutputString.Append(mTextValue);
  // anOutputString.AppendLiteral(";");
}

static void AppendNCR(nsString& aString, PRInt32 aNCRValue);
/*
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag -- If NS_IPARSER_FLAG_VIEW_SOURCE do not reduce entities...
 *  @return  error result
 *
 */
static
nsresult ConsumeAttributeEntity(nsString& aString,
                                nsScanner& aScanner,
                                PRInt32 aFlag) 
{
 
  nsresult result=NS_OK;

  PRUnichar ch;
  result=aScanner.Peek(ch, 1);

  if (NS_SUCCEEDED(result)) {
    PRUnichar amp=0;
    PRInt32 theNCRValue=0;
    nsAutoString entity;

    if (nsCRT::IsAsciiAlpha(ch) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result=CEntityToken::ConsumeEntity(ch,entity,aScanner);
      if (NS_SUCCEEDED(result)) {
        theNCRValue = nsHTMLEntities::EntityToUnicode(entity);
        PRUnichar theTermChar=entity.Last();
        // If an entity value is greater than 255 then:
        // Nav 4.x does not treat it as an entity,
        // IE treats it as an entity if terminated with a semicolon.
        // Resembling IE!!
        if(theNCRValue < 0 || (theNCRValue > 255 && theTermChar != ';')) {
          // Looks like we're not dealing with an entity
          aString.Append(kAmpersand);
          aString.Append(entity);
        }
        else {
          // A valid entity so reduce it.
          aString.Append(PRUnichar(theNCRValue));
        }
      }
    }
    else if (ch==kHashsign && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result=CEntityToken::ConsumeEntity(ch,entity,aScanner);
      if (NS_SUCCEEDED(result)) {
        if (result == NS_HTMLTOKENS_NOT_AN_ENTITY) {
          // Looked like an entity but it's not
          aScanner.GetChar(amp);
          aString.Append(amp);
          result = NS_OK; // just being safe..
        }
        else {
          PRInt32 err;
          theNCRValue=entity.ToInteger(&err,kAutoDetect);
          AppendNCR(aString, theNCRValue);
        }
      }
    }
    else {
      // What we thought as entity is not really an entity...
      aScanner.GetChar(amp);
      aString.Append(amp);
    }//if
  }

  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume attributed text value. 
 *  Note: It also reduces entities within attributes.
 *
 *  @param   aNewlineCount -- the newline count to increment when hitting newlines
 *  @param   aScanner -- controller of underlying input source
 *  @param   aTerminalChars -- characters that stop consuming attribute.
 *  @param   aAllowNewlines -- whether to allow newlines in the value.
 *                             XXX it would be nice to roll this info into
 *                             aTerminalChars somehow....
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
static
nsresult ConsumeAttributeValueText(nsString& aString,
                                   PRInt32& aNewlineCount,
                                   nsScanner& aScanner,
                                   const nsReadEndCondition& aEndCondition,
                                   PRBool aAllowNewlines,
                                   PRInt32 aFlag)
{
  nsresult result = NS_OK;
  PRBool   done = PR_FALSE;
  
  do {
    result = aScanner.ReadUntil(aString,aEndCondition,PR_FALSE);
    if(NS_SUCCEEDED(result)) {
      PRUnichar ch;
      aScanner.Peek(ch);
      if(ch == kAmpersand) {
        result = ConsumeAttributeEntity(aString,aScanner,aFlag);
      }
      else if(ch == kCR && aAllowNewlines) {
        aScanner.GetChar(ch);
        result = aScanner.Peek(ch);
        if (NS_SUCCEEDED(result)) {
          if(ch == kNewLine) {
            aString.AppendLiteral("\r\n");
            aScanner.GetChar(ch);
          }
          else {
            aString.Append(PRUnichar('\r'));
          }
          ++aNewlineCount;
        }
      }
      else if(ch == kNewLine && aAllowNewlines) {
        aScanner.GetChar(ch);
        aString.Append(PRUnichar('\n'));
        ++aNewlineCount;
      }
      else {
        done = PR_TRUE;
      }
    }
  } while (NS_SUCCEEDED(result) && !done);

  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume a known quoted string. 
 *  
 *  @param   aScanner -- controller of underlying input source
 *  @param   aTerminalChars -- characters that stop consuming attribute.
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
static
nsresult ConsumeQuotedString(PRUnichar aChar,
                             nsString& aString,
                             PRInt32& aNewlineCount,
                             nsScanner& aScanner,
                             PRInt32 aFlag)
{
  NS_ASSERTION(aChar==kQuote || aChar==kApostrophe,"char is neither quote nor apostrophe");
  // hold onto this in case this is an unterminated string literal
  PRUint32 origLen = aString.Length();

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
  if (aChar==kApostrophe)
    terminateCondition = &theTerminateConditionApostrophe;
  
  nsresult result=NS_OK;
  nsScannerIterator theOffset;
  aScanner.CurrentPosition(theOffset);

  result=ConsumeAttributeValueText(aString,aNewlineCount,aScanner,
                                   *terminateCondition,PR_TRUE,aFlag);

  if(NS_SUCCEEDED(result)) {
    result = aScanner.GetChar(aChar); // aChar should be " or '
  }

  // Ref: Bug 35806
  // A back up measure when disaster strikes...
  // Ex <table> <tr d="><td>hello</td></tr></table>
  if(!aString.IsEmpty() && aString.Last()!=aChar &&
     !aScanner.IsIncremental() && result==kEOF) {
    static const nsReadEndCondition
      theAttributeTerminator(kAttributeTerminalChars);
    aString.Truncate(origLen);
    aScanner.SetPosition(theOffset, PR_FALSE, PR_TRUE);
    result=ConsumeAttributeValueText(aString,aNewlineCount,aScanner,
                                     theAttributeTerminator,PR_FALSE,aFlag);
    if (NS_SUCCEEDED(result) && (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      // Remember that this string literal was unterminated.
      result = NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL;
    }
  }
  return result;
}

/*
 *  Consume the key and value portions of the attribute.
 *  
 *  @update  rickg 03.23.2000
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CAttributeToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

  nsresult result;
 
  nsScannerIterator wsstart, wsend;
  
  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
    result = aScanner.ReadWhitespace(wsstart, wsend, mNewlineCount);
  }
  else {
    result = aScanner.SkipWhitespace(mNewlineCount);
  }

  if (NS_OK==result) {
    static const PRUnichar theTerminalsChars[] = 
    { PRUnichar(' '), PRUnichar('"'), 
      PRUnichar('='), PRUnichar('\n'), 
      PRUnichar('\r'), PRUnichar('\t'), 
      PRUnichar('>'), PRUnichar('<'),
      PRUnichar('\b'), PRUnichar(0) };
    static const nsReadEndCondition theEndCondition(theTerminalsChars);

    nsScannerIterator start, end;
    result=aScanner.ReadUntil(start,end,theEndCondition,PR_FALSE);

    if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      aScanner.BindSubstring(mTextKey, start, end);
    }

    //now it's time to Consume the (optional) value...
    if (NS_OK==result) {
      if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
        result = aScanner.ReadWhitespace(start, wsend, mNewlineCount);
        aScanner.BindSubstring(mTextKey, wsstart, wsend);
      }
      else {
        result = aScanner.SkipWhitespace(mNewlineCount);
      }

      if (NS_OK==result) { 
        result=aScanner.Peek(aChar);       //Skip ahead until you find an equal sign or a '>'...
        if (NS_OK==result) {  
          if (kEqual==aChar){
            result=aScanner.GetChar(aChar);  //skip the equal sign...
            if (NS_OK==result) {
              if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                result = aScanner.ReadWhitespace(mTextValue, mNewlineCount);
              }
              else {
                result = aScanner.SkipWhitespace(mNewlineCount);
              }

              if (NS_OK==result) {
                result=aScanner.Peek(aChar);  //and grab the next char.    
                if (NS_OK==result) {
                  if ((kQuote==aChar) || (kApostrophe==aChar)) {
                    aScanner.GetChar(aChar);
                    if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                      mTextValue.Append(aChar);
                    }
                    
                    result=ConsumeQuotedString(aChar,mTextValue,mNewlineCount,
                                               aScanner,aFlag);
                    if (NS_SUCCEEDED(result) && (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
                      mTextValue.Append(aChar);
                    } else if (result == NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL) {
                      result = NS_OK;
                    }
                    // According to spec. we ( who? ) should ignore linefeeds. But look,
                    // even the carriage return was getting stripped ( wonder why! ) -
                    // Ref. to bug 15204.  Okay, so the spec. told us to ignore linefeeds,
                    // bug then what about bug 47535 ? Should we preserve everything then?
                    // Well, let's make it so! Commenting out the next two lines..
                    /*if(!aRetain)
                      mTextValue.StripChars("\r\n"); //per the HTML spec, ignore linefeeds...
                    */
                  }
                  else if (kGreaterThan==aChar){      
                    mHasEqualWithoutValue=PR_TRUE;
                  }
                  else {
                    static const nsReadEndCondition
                      theAttributeTerminator(kAttributeTerminalChars);
                    result=ConsumeAttributeValueText(mTextValue,
                                                     mNewlineCount,
                                                     aScanner,
                                                     theAttributeTerminator,
                                                     PR_FALSE,
                                                     aFlag);
                  } 
                }//if
                if (NS_OK==result) {
                  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                    result = aScanner.ReadWhitespace(mTextValue, mNewlineCount);
                  }
                  else {
                    result = aScanner.SkipWhitespace(mNewlineCount);
                  }
                }
              }//if
            }//if
          }//if
          else {
            //This is where we have to handle fairly busted content.
            //If you're here, it means we saw an attribute name, but couldn't find 
            //the following equal sign.  <tag NAME=....
        
            //Doing this right in all cases is <i>REALLY</i> ugly. 
            //My best guess is to grab the next non-ws char. We know it's not '=',
            //so let's see what it is. If it's a '"', then assume we're reading
            //from the middle of the value. Try stripping the quote and continuing...
            if (kQuote==aChar){
              if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
                result=aScanner.SkipOver(aChar); //strip quote.
              } else {
                aScanner.BindSubstring(mTextKey, wsstart, ++wsend);
                // I've just incremented wsend.
                aScanner.SetPosition(wsend);
              } 
            }
          }
        }//if
      } //if
    }//if (consume optional value)

    if (NS_OK==result) {
      result=aScanner.Peek(aChar);

      if (mTextValue.Length() == 0 && mTextKey.Length() == 0 && 
          aChar == kLessThan) {
        // This attribute is completely bogus, tell the tokenizer.
        // This happens when we have stuff like:
        // <script>foo()</script  <p>....
        return NS_ERROR_HTMLPARSER_BADATTRIBUTE;
      }

#ifdef DEBUG
      mLastAttribute = (kGreaterThan == aChar || kEOF == result);
#endif
    }
  }//if
  return result;
}

void CAttributeToken::SetKey(const nsAString& aKey)
{
  mTextKey.Rebind(aKey);
}

void CAttributeToken::BindKey(nsScanner* aScanner, 
                              nsScannerIterator& aStart, 
                              nsScannerIterator& aEnd)
{
  aScanner->BindSubstring(mTextKey, aStart, aEnd);
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken() : CHTMLToken(eHTMLTag_whitespace) {
}


/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken(const nsAString& aName) : CHTMLToken(eHTMLTag_whitespace) {
  mTextValue.Assign(aName);
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CWhitespaceToken::GetTokenType(void) {
  return eToken_whitespace;
}

/*
 *  This general purpose method is used when you want to
 *  consume an aribrary sequence of whitespace. 
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CWhitespaceToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  mTextValue.Assign(aChar);
  nsresult result=aScanner.ReadWhitespace(mTextValue, mNewlineCount);
  if(NS_OK==result) {
    mTextValue.StripChar(kCR);
  }
  return result;
}

const nsAString& CWhitespaceToken::GetStringValue(void)
{
  return mTextValue;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CEntityToken::CEntityToken() : CHTMLToken(eHTMLTag_entity) {
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CEntityToken::CEntityToken(const nsAString& aName) : CHTMLToken(eHTMLTag_entity) {
  mTextValue.Assign(aName);
#ifdef VERBOSE_DEBUG
  if(!VerifyEntityTable())  {
    cout<<"Entity table is invalid!" << endl;
  }
#endif
}


/*
 *  Consume the rest of the entity. We've already eaten the "&".
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CEntityToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  nsresult result=ConsumeEntity(aChar,mTextValue,aScanner);
  return result;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEntityToken::GetTokenType(void) {
  return eToken_entity;
}

/*
 *  This general purpose method is used when you want to
 *  consume an entity &xxxx;. Keep in mind that entities
 *  are <i>not</i> reduced inline.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CEntityToken::ConsumeEntity(PRUnichar aChar,
                            nsString& aString,
                            nsScanner& aScanner) {
  nsresult result=NS_OK;
  if(kLeftBrace==aChar) {
    //you're consuming a script entity...
    aScanner.GetChar(aChar); // Consume &

    PRInt32 rightBraceCount = 0;
    PRInt32 leftBraceCount  = 0;

    do {
      result=aScanner.GetChar(aChar);
      
      if (NS_FAILED(result)) {
        return result;
      }

      aString.Append(aChar);
      if(aChar==kRightBrace)
        ++rightBraceCount;
      else if(aChar==kLeftBrace)
        ++leftBraceCount;
    } while(leftBraceCount!=rightBraceCount);
  } //if
  else {
    PRUnichar theChar=0;
    if (kHashsign==aChar) {
      result = aScanner.Peek(theChar,2);
       
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
        result=aScanner.ReadNumber(aString,10);
      }
      else if (theChar == 'x' || theChar == 'X') {
        aScanner.GetChar(aChar);   // Consume &
        aScanner.GetChar(aChar);   // Consume #
        aScanner.GetChar(theChar); // Consume x
        aString.Assign(aChar);
        aString.Append(theChar); 
        result=aScanner.ReadNumber(aString,16);
      }
      else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY; 
      }
    }
    else {
      result = aScanner.Peek(theChar,1);
       
      if (NS_FAILED(result)) {
        return result;
      }

      if(nsCRT::IsAsciiAlpha(theChar) || 
        theChar == '_' ||
        theChar == ':') {
        aScanner.GetChar(aChar); // Consume &
        result=aScanner.ReadEntityIdentifier(aString);
      }
      else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY;
      }
    }
  }
    
  if (NS_FAILED(result)) {
    return result;
  }
    
  result=aScanner.Peek(aChar);
  
  if (NS_FAILED(result)) {
    return result;
  }

  if (aChar == kSemicolon) {
    // consume semicolon that stopped the scan
    aString.Append(aChar);
    result=aScanner.GetChar(aChar);
  }
  
  return result;
}

#define PA_REMAP_128_TO_160_ILLEGAL_NCR 1

#ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
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
#endif /* PA_REMAP_128_TO_160_ILLEGAL_NCR */

static void AppendNCR(nsString& aString, PRInt32 aNCRValue)
{
#ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
  /* for some illegal, but popular usage */
  if ((aNCRValue >= 0x0080) && (aNCRValue <= 0x009f)) {
    aNCRValue = PA_HackTable[aNCRValue - 0x0080];
  }
#endif

  if (IS_IN_BMP(aNCRValue))
    aString.Append(PRUnichar(aNCRValue));
  else {
    aString.Append(PRUnichar(H_SURROGATE(aNCRValue)));
    aString.Append(PRUnichar(L_SURROGATE(aNCRValue)));
  }
}

/*
 *  This method converts this entity into its underlying
 *  unicode equivalent.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will hold the resulting string value
 *  @return  numeric (unichar) value
 */
PRInt32 CEntityToken::TranslateToUnicodeStr(nsString& aString) {
  PRInt32 value=0;

  if(mTextValue.Length()>1) {
    PRUnichar theChar0=mTextValue.CharAt(0);

    if(kHashsign==theChar0) {
      PRInt32 err=0;
      
      value=mTextValue.ToInteger(&err,kAutoDetect);

      if(0==err) {
        AppendNCR(aString, value);
      }
    }
    else{
      value = nsHTMLEntities::EntityToUnicode(mTextValue);
      if(-1<value) {
        //we found a named entity...
        aString.Assign(PRUnichar(value));
      }
    }//else
  }//if

  return value;
}


const nsAString& CEntityToken::GetStringValue(void)
{
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CEntityToken::GetSource(nsString& anOutputString){
  anOutputString.AppendLiteral("&");
  anOutputString+=mTextValue;
  //anOutputString+=";";
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CEntityToken::AppendSourceTo(nsAString& anOutputString){
  anOutputString.AppendLiteral("&");
  anOutputString+=mTextValue;
  //anOutputString+=";";
}

/**
 * 
 * @update	gess4/25/98
 * @param 
 * @return
 */
const PRUnichar* GetTagName(PRInt32 aTag)
{
  const PRUnichar *result = nsHTMLTags::GetStringValue((nsHTMLTag) aTag);

  if (result) {
    return result;
  }

  if(aTag >= eHTMLTag_userdefined)
    return sUserdefined;

  return 0;
}


/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken() : CHTMLToken(eHTMLTag_instruction) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken(const nsAString& aString) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aString);
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CInstructionToken::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aFlag){
  mTextValue.AssignLiteral("<?");
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_TRUE);
  return result;
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
PRInt32 CInstructionToken::GetTokenType(void){
  return eToken_instruction;
}

const nsAString& CInstructionToken::GetStringValue(void)
{
  return mTextValue;
}

// Doctype decl token

CDoctypeDeclToken::CDoctypeDeclToken(eHTMLTags aTag)
  : CHTMLToken(aTag) {
}

CDoctypeDeclToken::CDoctypeDeclToken(const nsAString& aString,eHTMLTags aTag)
  : CHTMLToken(aTag), mTextValue(aString) {
}

/**
 *  This method consumes a doctype element.
 *  Note: I'm rewriting this method to seek to the first <, since quotes can really screw us up.
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CDoctypeDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
    
  static const PRUnichar terminalChars[] = 
  { PRUnichar('>'), PRUnichar('<'),
    PRUnichar(0) 
  };
  static const nsReadEndCondition theEndCondition(terminalChars);

  nsScannerIterator start, end;
  
  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);

  nsresult result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);

  if (NS_SUCCEEDED(result)) {
    PRUnichar ch;
    aScanner.Peek(ch);
    if (ch == kGreaterThan) {
      // Include '>' but not '<' since '<' 
      // could belong to another tag.
      aScanner.GetChar(ch);
      end.advance(1); 
    }
  }
  else if (!aScanner.IsIncremental()) {
    // We have reached the document end but haven't
    // found either a '<' or a '>'. Therefore use
    // whatever we have.
    result = NS_OK; 
  }
  
  if (NS_SUCCEEDED(result)) {
    start.advance(-2); // Make sure to consume <!
    CopyUnicodeTo(start,end,mTextValue);
  }
  
  return result;
}

PRInt32 CDoctypeDeclToken::GetTokenType(void) {
  return eToken_doctypeDecl;
}

const nsAString& CDoctypeDeclToken::GetStringValue(void)
{
  return mTextValue;
}

void CDoctypeDeclToken::SetStringValue(const nsAString& aStr)
{
  mTextValue.Assign(aStr);
}
