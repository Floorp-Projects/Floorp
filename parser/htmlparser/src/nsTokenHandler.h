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

#ifndef CTokenHandler__
#define CTokenHandler__

#include "prtypes.h"
#include "nsString.h"
#include "nsHTMLTokens.h"
#include "nsITokenHandler.h"

class CToken;
class nsHTMLParser;

typedef PRInt32 (*dispatchFP)(eHTMLTokenTypes,CToken*,nsHTMLParser*);

class CTokenHandler : public CITokenHandler {
public:
                            CTokenHandler(dispatchFP aFP,eHTMLTokenTypes aType=eToken_unknown);
  virtual                   ~CTokenHandler();
                          
  virtual   eHTMLTokenTypes GetTokenType(void);
  virtual   PRInt32         operator()(CToken* aToken,nsHTMLParser* aParser);

protected:
            eHTMLTokenTypes mType;
            dispatchFP      mFP;
};



#endif


