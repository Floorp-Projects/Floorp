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

#ifndef _CPPTOKENS
#define _CPPTOKENS

#include "CToken.h"

enum eCPPTokens
{
  eToken_unknown=2000,

  eToken_whitespace,
  eToken_comment,
  eToken_identifier,
  eToken_number,
  eToken_operator,
  eToken_semicolon,
  eToken_newline,
  eToken_compilerdirective,
  eToken_quotedstring,
  eToken_last
};

class CIdentifierToken : public CToken {
  public:
                       CIdentifierToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_identifier;}
};

class CNumberToken: public CToken {
  public:
                       CNumberToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_number;}
};

class CWhitespaceToken: public CToken {
  public:
                       CWhitespaceToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_whitespace;}
};

class COperatorToken: public CToken {
  public:
                       COperatorToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_operator;}
};

class CCommentToken: public CToken {
  public:
                       CCommentToken(char the2ndChar) : CToken() { 
                        char theBuf[3]={'/',0,0};
                        theBuf[1]=the2ndChar;
                        mTextValue.Assign(theBuf);
                      }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_comment;}
};

class CCompilerDirectiveToken: public CToken {
  public:
                       CCompilerDirectiveToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_compilerdirective;}
};

class CSemicolonToken: public CToken {
  public:
                       CSemicolonToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_semicolon;}
};

class CNewlineToken: public CToken {
  public:
                       CNewlineToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_newline;}
};

class CQuotedStringToken: public CToken {
  public:
                       CQuotedStringToken() : CToken() { }
  virtual int       consume(char aChar,CScanner& aScanner);
  virtual int       getTokenType(void) {return eToken_quotedstring;}
};

#endif
