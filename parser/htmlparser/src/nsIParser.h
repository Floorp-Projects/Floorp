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
 * This file declarse the parser interface.
 *
 */

#ifndef NS_IPARSER___
#define NS_IPARSER___


#include "nshtmlpars.h"
#include "nsISupports.h"
#include "nsIURL.h"


#define NS_IPARSER_IID      \
  {0x355cbba0, 0xbf7d,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class nsIContentSink;
class nsString;

/**
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *  
 *  @update  gess 3/25/98
 */
class nsIParser : public nsISupports {
            
  public:

    virtual nsIContentSink* SetContentSink(nsIContentSink* aContentSink)=0;

    virtual PRInt32 Parse(nsIURL* aURL,PRBool aIncremental=PR_TRUE)=0;
    virtual PRInt32 Parse(const char* aFilename,PRBool aIncremental)=0;
    virtual PRInt32 Parse(nsString& anHTMLString,PRBool appendTokens)=0;

    virtual PRInt32 ResumeParse(void)=0;
    virtual PRInt32 GetStack(PRInt32* aStackPtr)=0;
    virtual PRBool  HasOpenContainer(PRInt32 aContainer) const=0;
};

extern NS_HTMLPARS nsresult NS_NewHTMLParser(nsIParser** aInstancePtrResult);

#endif 



