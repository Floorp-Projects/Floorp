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

#include "nsToken.h"
#include "nsScanner.h"

/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   default constructor
 * PARMS:
 * RETURNS:
 *------------------------------------------------------*/
CToken::CToken(const nsString& aName) : mTextValue(aName) {
  mOrdinalValue=0;
}
 
/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   destructor
 * PARMS:
 * RETURNS:
 *------------------------------------------------------*/
CToken::~CToken() {
}

 
/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   virtual method used to tell this token to
              Consume valid characters.
 * PARMS:     aChar: last char read
              aScanner: see scanner.h
 * RETURNS:   error code
 *------------------------------------------------------*/
PRInt32 CToken::Consume(PRUnichar aChar,CScanner* aScanner) {
	PRInt32 result=kNoError;
  return result;
}

/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   Allows caller to set value of internal  
              string buffer
 * PARMS:     aValue: new value for internal string
 * RETURNS:   
 *------------------------------------------------------*/
void CToken::SetStringValue(const char* aValue) {
	mTextValue=aValue;
}

/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   used for debugging purposes, this method
              causes the token dump its contents to the
              given stream.
 * PARMS:     aOutputStream: stream to write data to
 * RETURNS:   nada
 *------------------------------------------------------*/
void CToken::DebugDumpToken(ostream& anOutputStream) {
	anOutputStream << "[" << GetClassName() << "] ";
  for(int i=0;i<mTextValue.Length();i++){
    anOutputStream << char(mTextValue[i]);
  }
  anOutputStream << ": " << mOrdinalValue << endl;
}

/*-------------------------------------------------------
 * LAST MODS:	gess
 * PURPOSE:   used for debugging purposes, this method
              causes the token dump its contents to the
              given stream.
 * PARMS:     aOutputStream: stream to write data to
 * RETURNS:   nada
 *------------------------------------------------------*/
void CToken::DebugDumpSource(ostream& anOutputStream) {
	anOutputStream << mTextValue;
}

/*-------------------------------------------------------
 * LAST MODS:	gess 28Feb98
 * PURPOSE:   self test
 * PARMS:
 * RETURNS:
 *------------------------------------------------------*/
void CToken::SelfTest(void) {
#ifdef _DEBUG
#endif
}
