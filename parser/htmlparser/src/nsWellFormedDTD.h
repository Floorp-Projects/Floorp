/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef __NS_WELLFORMED_DTD
#define __NS_WELLFORMED_DTD

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nsIContentSink.h"

#define NS_WELLFORMED_DTD_IID      \
  {0xa39c6bfd, 0x15f0,  0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}


class nsIDTDDebug;
class nsIParserNode;
class nsParser;
class nsHTMLTokenizer;


class CWellFormedDTD : public nsIDTD
{
public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDTD
    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    CWellFormedDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~CWellFormedDTD();
   
    virtual void SetVerification(PRBool aEnable);

protected:
    nsresult    HandleStartToken(CToken* aToken);
    nsresult    HandleEndToken(CToken* aToken);
    nsresult    HandleCommentToken(CToken* aToken);
    nsresult    HandleErrorToken(CToken* aToken);
    nsresult    HandleDocTypeDeclToken(CToken* aToken);
    nsresult    HandleLeafToken(CToken* aToken);
    nsresult    HandleProcessingInstructionToken(CToken* aToken);

    nsParser*           mParser;
    nsIContentSink*     mSink;
    nsString            mFilename;
    PRInt32             mLineNumber;
    nsHTMLTokenizer*    mTokenizer;
    nsresult            mDTDState;
};

extern nsresult NS_NewWellFormed_DTD(nsIDTD** aInstancePtrResult);

#endif 



