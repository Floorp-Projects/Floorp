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

#include "nsXMLTokenizer.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsDTDUtils.h"
#include "nsParser.h"

/************************************************************************
  And now for the main class -- nsXMLTokenizer...
 ************************************************************************/

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kITokenizerIID,    NS_ITOKENIZER_IID);
static NS_DEFINE_IID(kHTMLTokenizerIID, NS_HTMLTOKENIZER_IID);
static NS_DEFINE_IID(kClassIID,         NS_XMLTOKENIZER_IID); 

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsXMLTokenizer::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsXMLTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do ITOkenizer base class...
    *aInstancePtr = (nsITokenizer*)(this);                                        
  }
  else if(aIID.Equals(kHTMLTokenizerIID)) {  //do nsHTMLTokenizer base class...
    *aInstancePtr = (nsHTMLTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsXMLTokenizer*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewXMLTokenizer(nsIDTD** aInstancePtrResult) {
  nsXMLTokenizer* it = new nsXMLTokenizer();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsXMLTokenizer)
NS_IMPL_RELEASE(nsXMLTokenizer)


/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsXMLTokenizer::nsXMLTokenizer() : nsHTMLTokenizer() {
  NS_INIT_REFCNT();
  mDoXMLEmptyTags=PR_TRUE;
}

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsXMLTokenizer::~nsXMLTokenizer(){
}


/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsXMLTokenizer::ConsumeToken(nsScanner& aScanner) {
  return nsHTMLTokenizer::ConsumeToken(aScanner);
}


nsITokenRecycler* nsXMLTokenizer::GetTokenRecycler(void) {
  return nsHTMLTokenizer::GetTokenRecycler();
}

/*
 * Consume characters as long as they match the string passed in.
 * If they don't match, put them all back. 
 * XXX The scanner should be able to do this.
 *
 *  @update vidur 11/12/98
 */
static
nsresult ConsumeConditional(nsScanner& aScanner,const nsString& aMatchString,PRBool& aMatch) {
  nsresult result=NS_OK;
  PRUnichar matchChar;

  PRInt32 i, count = aMatchString.Length();
  for (i=0; i < count; i++) {
    result = aScanner.GetChar(matchChar);
    if ((NS_OK != result) || (aMatchString.CharAt(i) != matchChar)) {
      break;
    }
  }

  if (NS_OK == result) {
    if (i != count) {
      for (; i >= 0; i--) {
        aScanner.PutBack(aMatchString.CharAt(i));
      }
      aMatch = PR_FALSE;
    }
    else {
      aMatch = PR_TRUE;
    }
  }

  return result;
}

/**
 *  This method is called just after a "<!" has been consumed 
 *  and we know we're at the start of a comment.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsXMLTokenizer::ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner){
  nsresult result=NS_OK;
  nsAutoString CDATAString("[CDATA[");
  PRBool isCDATA = PR_FALSE;

  result = ConsumeConditional(aScanner, CDATAString, isCDATA);
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  
  if(theRecycler) {
    if (NS_OK == result) {
      nsAutoString  theEmpty;
      if (isCDATA) {
        aToken=theRecycler->CreateTokenOfType(eToken_cdatasection,eHTMLTag_unknown,theEmpty);
      }
      else {
        aToken=theRecycler->CreateTokenOfType(eToken_comment,eHTMLTag_comment,theEmpty);
      }
    }

    if(aToken) {
      result=aToken->Consume(aChar,aScanner);
      AddToken(aToken,result,mTokenDeque,theRecycler);
    }
  }

  return result;
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsresult nsXMLTokenizer::HandleSkippedContent(nsScanner& aScanner,CToken*& aToken) {
  nsresult result=NS_OK;
  return result;
}
