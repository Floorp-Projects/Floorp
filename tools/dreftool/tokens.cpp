
#include "tokens.h"
#include "CScanner.h"
#include "nsString.h"

static nsCAutoString gIdentChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
static nsCAutoString gDigits("0123456789");
static nsCAutoString gWhitespace(" \n\r\t\b");
static nsCAutoString gNewlines("\r\n");


int CIdentifierToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gIdentChars,false);
  return result;
};


int CNumberToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gDigits,false);
  return result;
};


int CWhitespaceToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gWhitespace,false);
  return result;
};


bool validOperator(nsCString& aString) {
	bool 	result=false;
	int   len=aString.Length();

  switch(len) {
    case 1: return true;
    case 3:
      result=((0==aString.Compare("<<=")) ||(0==aString.Compare(">>=")));
      break;
    default:
       switch(aString[0]) {
          case '+':  case '=':
          case '&':  case '|':
          case '<':  case '>':
             if((aString[1]==aString[0]) || (aString[1]=='='))
                result=true;
             break;
          case '-':
             if((aString[1]==aString[0]) || (aString[1]=='>'))
                result=true;
             break;
          case ':':
             if((len==2) && (aString[1]==aString[0]))
               result=true;
             break;
          case '*':	case '/':
          case '^': 	case '!':
          case '~':	case '%':
             if((aString[1]=='='))
                result=true;
             break;
          case '(':
            result=aString[1]==')'; break;
          case '[':
            result=aString[1]==']'; break;
          default:
             break;
       }
       break;
  }
  return result;
}

int COperatorToken::consume(PRUnichar aChar, CScanner& aScanner) {
	int result=0;
  mTextValue=aChar;  
  while((validOperator(mTextValue)) && (kNoError==result)){
    result=aScanner.getChar(aChar);
    mTextValue+=aChar;
  }
  if(kNoError==result) {
    mTextValue.Truncate(mTextValue.mLength-1);
    aScanner.push(aChar);
  }
  return result;
};


int CCommentToken::consume(PRUnichar aChar, CScanner& aScanner) {
  int result=0;
  if('/'==mTextValue[1]) {
    result=aScanner.readUntil(mTextValue,gNewlines,false);    
  }
  else {
    bool done=false;
    nsCAutoString temp("/");
    while((0==result) && (false==done)){
      aScanner.readUntil(mTextValue,temp,true);
      PRUnichar theChar=mTextValue[mTextValue.Length()-2];
      done=('*'==theChar);
    }
  }
  return result;
};


int CCompilerDirectiveToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readUntil(mTextValue,gNewlines,false);
  return result;
};


int CSemicolonToken::consume(PRUnichar aChar, CScanner& aScanner) {
	int result=0;
  mTextValue=aChar;
  return result;
};


int CNewlineToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gNewlines,false);
  return result;
};


int CQuotedStringToken::consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readUntil(mTextValue,aChar,true);
  return result;
};
