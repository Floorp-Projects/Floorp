/*================================================================*
	Copyright © 1998 Rick Gessner. All Rights Reserved.
 *================================================================*/

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
  while(theToken=(CToken*)mTokens.Pop()){
    delete theToken;
  }
}


int consumeToken(CScanner& aScanner,CToken** aToken){
  static nsCAutoString gOperatorChars("/?.<>[]{}~^+=-!%&*(),|:");

  PRUnichar theChar=0;
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
      else if(-1<gOperatorChars.Find(theChar)) {
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

int CCPPTokenizer::tokenize(CScanner& aScanner){
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


