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


#include "nsTokenHandler.h"
#include "nsHTMLParser.h"
#include "nsHTMLTokens.h"
#include "nsDebug.h"

static const char* kNullParserGiven = "Error: Null parser given as argument";

/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler::CTokenHandler(dispatchFP aFP,eHTMLTokenTypes aType){
  mType=aType;
  mFP=aFP;
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler::~CTokenHandler(){
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
eHTMLTokenTypes CTokenHandler::GetTokenType(void){
  return mType;
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRInt32 CTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  PRInt32 result=kNoError;
  if((0!=aParser) && (0!=mFP)) {
     result=(*mFP)(mType,aToken,aParser);
  }
  return result;
}


