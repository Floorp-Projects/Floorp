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
 * This class is defines the basic notion of a token
 * within our system. All other tokens are derived from
 * this one. It offers a few basic interfaces, but the
 * most important is consume(). The consume() method gets
 * called during the tokenization process when an instance
 * of that particular token type gets detected in the 
 * input stream.
 *
 */


#ifndef CTOKEN__
#define CTOKEN__

#include "prtypes.h"
#include "nsString.h"
#include <iostream.h>

class CScanner;

class CToken {
	public:
									        CToken(const nsString& aName);
            					    ~CToken();
		
    virtual	nsString&     GetStringValue(void) {return mTextValue;}
    virtual	nsString&     GetText(void) {return mTextValue;}
		virtual	void			    SetStringValue(const char* name);
    virtual	void			    SetOrdinal(PRInt32 value) {mOrdinalValue=value;}
  	virtual	PRInt32			  GetOrdinal(void) {return mOrdinalValue;}
  	virtual	PRInt32       Consume(PRUnichar aChar,CScanner* aScanner);
  	virtual	void			    DebugDumpToken(ostream& out);
  	virtual	void			    DebugDumpSource(ostream& out);
		virtual	PRInt32			  GetTokenType(void) {return -1;}
    virtual const char*	  GetClassName(void) {return "token";}
    static void           SelfTest();

protected:
          	PRInt32			  mOrdinalValue;
          	nsString      mTextValue;
};

#endif
