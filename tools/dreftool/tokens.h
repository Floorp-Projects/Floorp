
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
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_identifier;}
};

class CNumberToken: public CToken {
	public:
   						        CNumberToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_number;}
};

class CWhitespaceToken: public CToken {
	public:
   						        CWhitespaceToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_whitespace;}
};

class COperatorToken: public CToken {
	public:
   						        COperatorToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_operator;}
};

class CCommentToken: public CToken {
	public:
   						        CCommentToken(PRUnichar the2ndChar) : CToken() { 
                        PRUnichar theBuf[3]={'/',0,0};
                        theBuf[1]=the2ndChar;
                        mTextValue.Assign(theBuf);
                      }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_comment;}
};

class CCompilerDirectiveToken: public CToken {
	public:
   						        CCompilerDirectiveToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_compilerdirective;}
};

class CSemicolonToken: public CToken {
	public:
   						        CSemicolonToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_semicolon;}
};

class CNewlineToken: public CToken {
	public:
   						        CNewlineToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_newline;}
};

class CQuotedStringToken: public CToken {
	public:
   						        CQuotedStringToken() : CToken() { }
  virtual int       consume(PRUnichar aChar,CScanner& aScanner);
	virtual int       getTokenType(void) {return eToken_quotedstring;}
};

#endif