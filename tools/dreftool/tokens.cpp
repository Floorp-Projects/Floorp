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

#include "tokens.h"
#include "CScanner.h"
#include "nsString.h"

static nsCAutoString gIdentChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
static nsCAutoString gDigits("0123456789");
static nsCAutoString gWhitespace(" \n\r\t\b");
static nsCAutoString gNewlines("\r\n");


int CIdentifierToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gIdentChars,false);
  return result;
};


int CNumberToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gDigits,false);
  return result;
};


int CWhitespaceToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gWhitespace,false);
  return result;
};


bool validOperator(nsAFlatCString& aString) {
  bool result=false;
  int  len=aString.Length();

  switch(len) {
    case 1: return true;
    case 3:
      result=(aString.Equals(NS_LITERAL_CSTRING("<<=")) ||
              aString.Equals(NS_LITERAL_CSTRING(">>=")));
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
          case '*':
          case '/':
          case '^':
          case '!':
          case '~':
          case '%':
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

int COperatorToken::consume(char aChar, CScanner& aScanner) {
  int result=0;
  mTextValue=aChar;  
  while((validOperator(mTextValue)) && (kNoError==result)){
    result=aScanner.getChar(aChar);
    mTextValue+=aChar;
  }
  if(kNoError==result) {
    mTextValue.Truncate(mTextValue.Length()-1);
    aScanner.push(aChar);
  }
  return result;
};


int CCommentToken::consume(char aChar, CScanner& aScanner) {
  int result=0;
  if('/'==mTextValue[1]) {
    result=aScanner.readUntil(mTextValue,gNewlines,false);    
  }
  else {
    bool done=false;
    nsCAutoString temp("/");
    while((0==result) && (false==done)){
      aScanner.readUntil(mTextValue,temp,true);
      char theChar=mTextValue[mTextValue.Length()-2];
      done=('*'==theChar);
    }
  }
  return result;
};


int CCompilerDirectiveToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readUntil(mTextValue,gNewlines,false);
  return result;
};


int CSemicolonToken::consume(char aChar, CScanner& aScanner) {
  int result=0;
  mTextValue=aChar;
  return result;
};


int CNewlineToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readWhile(mTextValue,gNewlines,false);
  return result;
};


int CQuotedStringToken::consume(char aChar, CScanner& aScanner) {
  mTextValue=aChar;
  int result=aScanner.readUntil(mTextValue,aChar,true);
  return result;
};
