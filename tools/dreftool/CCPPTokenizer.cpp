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
 * The Original Code is Dreftool.
 *
 * The Initial Developer of the Original Code is
 * Rick Gessner <rick@gessner.com>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kenneth Herron <kjh-5727@comcast.net>
 *   Bernd Mielke <bmlk@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "CCPPTokenizer.h"
#include "CScanner.h"
#include "CToken.h"
#include "tokens.h"
#include <ctype.h>


CCPPTokenizer::CCPPTokenizer() : mTokens(0) {
}

/********************************************************
* Process:  Destroy the tokenizer
* Usage:
* Calls:
* Notes:
*********************************************************/
CCPPTokenizer::~CCPPTokenizer() {
  CToken* theToken=0;
  while((theToken=(CToken*)mTokens.Pop())){
    delete theToken;
  }
}


int consumeToken(CScanner& aScanner,CToken** aToken){
  static nsCAutoString gOperatorChars("/?.<>[]{}~^+=-!%&*(),|:");

  char theChar=0;
  int result=aScanner.getChar(theChar);
  if(kNoError==result){
    if((isspace(theChar)) || (theChar>=kSpace)) {
      if('#'==theChar)
        *aToken=(CToken*)new CCompilerDirectiveToken();
      else if((theChar=='_') || (isalpha(theChar)))
        *aToken=(CToken*)new CIdentifierToken();
      else if(isdigit(theChar))
        *aToken=(CToken*)new CNumberToken();
      else if((kLF==theChar) || (kCR==theChar))
        *aToken=(CToken*)new CNewlineToken();
      else if(isspace(theChar))
        *aToken=(CToken*)new CWhitespaceToken();
      else if(-1<gOperatorChars.FindChar(theChar)) {
        PRUnichar nextChar;
        aScanner.peek(nextChar);
        if((kForwardSlash==theChar) && ((kForwardSlash==nextChar) || (kAsterisk==nextChar))){
          *aToken=(CToken*)new CCommentToken(nextChar);
        }
        else *aToken=(CToken*)new COperatorToken();
      }
      else if((kQuote==theChar) || (kApostrophe==theChar))
        *aToken=(CToken*)new CQuotedStringToken();
      else if(kSemicolon==theChar) {
        *aToken=(CToken*)new CSemicolonToken();
      }
      if(*aToken){
        result=(*aToken)->consume(theChar,aScanner);
      }
    }
  }
  return result;
}

int32 CCPPTokenizer::tokenize(CScanner& aScanner){
  int result=0;
  while(kNoError==result){
    CToken* theToken=0;
    result=consumeToken(aScanner,&theToken);
    if(theToken) {
      //theToken->debugDumpSource(cout);
      mTokens.Push(theToken);
    }
  }
  return result;
}


CToken* CCPPTokenizer::getTokenAt(int anIndex){
  if((anIndex>=0) && (anIndex<mTokens.GetSize())){
    return (CToken*)mTokens.ObjectAt(anIndex);
  }
  return 0;
}


