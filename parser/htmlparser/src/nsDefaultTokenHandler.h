/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef CDEFAULTTOKENHANDLER__
#define CDEFAULTTOKENHANDLER__

#include "prtypes.h"
#include "nsString.h"
#include "nsHTMLTokens.h"
#include "nsITokenHandler.h"

class CToken;
class nsHTMLParser;


class CDefaultTokenHandler : public CITokenHandler {
public:
                          CDefaultTokenHandler(eHTMLTokenTypes aType=eToken_unknown);
  virtual                 ~CDefaultTokenHandler();
                          
  virtual   eHTMLTokenTypes GetTokenType(void);
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);

protected:
            eHTMLTokenTypes mType;
};


class CStartTokenHandler : public CDefaultTokenHandler  {
public:
                          CStartTokenHandler();
  virtual                 ~CStartTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CEndTokenHandler : public CDefaultTokenHandler  {
public:
                          CEndTokenHandler();
  virtual                 ~CEndTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CCommentTokenHandler : public CDefaultTokenHandler  {
public:
                          CCommentTokenHandler();
  virtual                 ~CCommentTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CEntityTokenHandler : public CDefaultTokenHandler  {
public:
                          CEntityTokenHandler();
  virtual                 ~CEntityTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CWhitespaceTokenHandler : public CDefaultTokenHandler  {
public:
                          CWhitespaceTokenHandler();
  virtual                 ~CWhitespaceTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CNewlineTokenHandler : public CDefaultTokenHandler  {
public:
                          CNewlineTokenHandler();
  virtual                 ~CNewlineTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CTextTokenHandler : public CDefaultTokenHandler  {
public:
                          CTextTokenHandler();
  virtual                 ~CTextTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CAttributeTokenHandler : public CDefaultTokenHandler  {
public:
                          CAttributeTokenHandler();
  virtual                 ~CAttributeTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CScriptTokenHandler :  public CDefaultTokenHandler  {
public:
                          CScriptTokenHandler();
  virtual                 ~CScriptTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CStyleTokenHandler : public CDefaultTokenHandler  {
public:
                          CStyleTokenHandler();
  virtual                 ~CStyleTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CSkippedContentTokenHandler : public CDefaultTokenHandler  {
public:
                          CSkippedContentTokenHandler();
  virtual                 ~CSkippedContentTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


#endif


