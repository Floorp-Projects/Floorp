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

#include "nsHTMLTokenizer.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsElementTable.h"
#include "nsHTMLEntities.h"

/************************************************************************
  And now for the main class -- nsHTMLTokenizer...
 ************************************************************************/

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kITokenizerIID,  NS_ITOKENIZER_IID);
static NS_DEFINE_IID(kClassIID,       NS_HTMLTOKENIZER_IID); 

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
nsresult nsHTMLTokenizer::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLTokenizer*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewHTMLTokenizer(nsIDTD** aInstancePtrResult) {
  nsHTMLTokenizer* it = new nsHTMLTokenizer();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsHTMLTokenizer)
NS_IMPL_RELEASE(nsHTMLTokenizer)


/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsHTMLTokenizer::nsHTMLTokenizer() : nsITokenizer(), mTokenDeque(new CTokenDeallocator()){
  NS_INIT_REFCNT();
  mDoXMLEmptyTags=PR_FALSE;
}

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsHTMLTokenizer::~nsHTMLTokenizer(){
}
 

/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

void nsHTMLTokenizer::AddToken(CToken*& aToken,nsresult aResult,nsDeque& aDeque,CTokenRecycler* aRecycler) {
  if(aToken) {
    if(NS_SUCCEEDED(aResult)) {
      aDeque.Push(aToken);
    }
    else {
      if(aRecycler) {
        aRecycler->RecycleToken(aToken);
      }
      else delete aToken; 
      aToken=0;
    }
  }
}

/**
 * Retrieve a ptr to the global token recycler...
 * @update	gess8/4/98
 * @return  ptr to recycler (or null)
 */
nsITokenRecycler* nsHTMLTokenizer::GetTokenRecycler(void) {
  static CTokenRecycler gTokenRecycler;
  return (nsITokenRecycler*)&gTokenRecycler;
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is not really removed from the list.
 * @update	gess8/2/98
 * @return  ptr to token
 */
CToken* nsHTMLTokenizer::PeekToken() {
  return (CToken*)mTokenDeque.Peek();
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is really removed from the list; if the list is empty we return 0.
 * @update	gess8/2/98
 * @return  ptr to token or NULL
 */
CToken* nsHTMLTokenizer::PopToken() {
  return (CToken*)mTokenDeque.PopFront();
}


/**
 * 
 * @update	gess8/2/98
 * @param 
 * @return
 */
CToken* nsHTMLTokenizer::PushTokenFront(CToken* theToken) {
  mTokenDeque.PushFront(theToken);
	return theToken;
}

/**
 * 
 * @update	gess8/2/98
 * @param 
 * @return
 */
CToken* nsHTMLTokenizer::PushToken(CToken* theToken) {
  mTokenDeque.Push(theToken);
	return theToken;
}

/**
 * 
 * @update	gess12/29/98
 * @param 
 * @return
 */
PRInt32 nsHTMLTokenizer::GetCount(void) {
  return mTokenDeque.GetSize();
}

/**
 * 
 * @update	gess12/29/98
 * @param 
 * @return
 */
CToken* nsHTMLTokenizer::GetTokenAt(PRInt32 anIndex){
  return (CToken*)mTokenDeque.ObjectAt(anIndex);
}


/**
 * 
 * @update	gess12/29/98
 * @param 
 * @return
 */
void nsHTMLTokenizer::PrependTokens(nsDeque& aDeque){

  PRInt32 aCount=aDeque.GetSize();
  
  //last but not least, let's check the misplaced content list.
  //if we find it, then we have to push it all into the body before continuing...
  PRInt32 anIndex=0;
  for(anIndex=0;anIndex<aCount;anIndex++){
    CToken* theToken=(CToken*)aDeque.Pop();
    PushTokenFront(theToken);
  }

}


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
nsresult nsHTMLTokenizer::ConsumeToken(nsScanner& aScanner) {

  nsresult result=NS_OK;
  if(NS_OK==result){
    PRUnichar theChar;
    result=aScanner.GetChar(theChar);
    CToken* theToken=0;
    switch(result) {
      case kEOF:
          //We convert from eof to complete here, because we never really tried to get data.
          //All we did was try to see if data was available, which it wasn't.
          //It's important to return process complete, so that controlling logic can know that
          //everything went well, but we're done with token processing.
        break;

      case NS_OK:
      default:
        switch(theChar) {
          case kLessThan:
            result=ConsumeTag(theChar,theToken,aScanner);
            break;

          case kAmpersand:
            result=ConsumeEntity(theChar,theToken,aScanner);
            break;
          
          case kCR: case kLF:
            result=ConsumeNewline(theChar,theToken,aScanner);
            break;
          
          case kNotFound:
            break;
        
          case 0: //preceeds a EOF...
            break;
            
          default:
            if(!nsString::IsSpace(theChar)) {
              nsAutoString temp(theChar);
              result=ConsumeText(temp,theToken,aScanner);
              break;
            }
            result=ConsumeWhitespace(theChar,theToken,aScanner);
            break;
        } //switch
        break; 
    } //switch
  } //if
  return result;
}


/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of some kind of tagged 
 *  element. We don't know yet if it's a tag or a comment.
 *  
 *  @update  gess 5/12/98
 *  @param   aChar is the last char read
 *  @param   aScanner is represents our input source
 *  @param   aToken is the out arg holding our new token
 *  @return  error code.
 */
nsresult nsHTMLTokenizer::ConsumeTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner) {

  nsresult result=aScanner.GetChar(aChar);

  if(NS_OK==result) {

    switch(aChar) {
      case kForwardSlash:
        PRUnichar ch; 
        result=aScanner.Peek(ch);
        if(NS_OK==result) {
          if(nsString::IsAlpha(ch)) {
            result=ConsumeEndTag(aChar,aToken,aScanner);
          }
          else result=ConsumeComment(aChar,aToken,aScanner);
        }//if
        break;

      case kExclamation:
        result=ConsumeComment(aChar,aToken,aScanner);
        break;

      case kQuestionMark: //it must be an XML processing instruction...
        result=ConsumeProcessingInstruction(aChar,aToken,aScanner);
        break;

      default:
        if(nsString::IsAlpha(aChar))
          result=ConsumeStartTag(aChar,aToken,aScanner);
        else if(kEOF!=aChar) {
          nsAutoString temp("<");
          result=ConsumeText(temp,aToken,aScanner);
        }
    } //switch

  } //if
  return result;
}

/**
 *  This method is called just after we've consumed a start
 *  tag, and we now have to consume its attributes.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar: last char read
 *  @param   aScanner: see nsScanner.h
 *  @return  
 */
nsresult nsHTMLTokenizer::ConsumeAttributes(PRUnichar aChar,CStartToken* aToken,nsScanner& aScanner) {
  PRBool done=PR_FALSE;
  nsresult result=NS_OK;
  PRInt16 theAttrCount=0;

  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();

  while((!done) && (result==NS_OK)) {
    CToken* theToken= (CAttributeToken*)theRecycler->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown);
    if(theToken){
      result=theToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
 
      //Much as I hate to do this, here's some special case code.
      //This handles the case of empty-tags in XML. Our last
      //attribute token will come through with a text value of ""
      //and a textkey of "/". We should destroy it, and tell the 
      //start token it was empty.
      if(NS_SUCCEEDED(result)) {
        nsString& key=((CAttributeToken*)theToken)->GetKey();
        nsString& text=theToken->GetStringValueXXX();
        if((mDoXMLEmptyTags) && (kForwardSlash==key.CharAt(0)) && (0==text.Length())){
          //tada! our special case! Treat it like an empty start tag...
          aToken->SetEmpty(PR_TRUE);
          theRecycler->RecycleToken(theToken);
        }
        else {
          theAttrCount++;
          AddToken(theToken,result,mTokenDeque,theRecycler);
        }
      }
      else { //if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result){
        aToken->SetEmpty(PR_TRUE);
        theRecycler->RecycleToken(theToken);
        if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result)
          result=NS_OK;
      }
    }//if
    
    if(NS_SUCCEEDED(result)){
      result=aScanner.SkipWhitespace();
      if(NS_SUCCEEDED(result)) {
        result=aScanner.Peek(aChar);
        if(NS_SUCCEEDED(result) && (aChar==kGreaterThan)) { //you just ate the '>'
          aScanner.GetChar(aChar); //skip the '>'
          done=PR_TRUE;
        }//if
      }
    }//if
  }//while

  aToken->SetAttributeCount(theAttrCount);
  return result;
}

/**
 *  This is a special case method. It's job is to consume 
 *  all of the given tag up to an including the end tag.
 *
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null
 */
nsresult nsHTMLTokenizer::ConsumeContentToEndTag(PRUnichar aChar,
                                eHTMLTags aChildTag,
                                nsScanner& aScanner,
                                CToken*& aToken){
  
  //In the case that we just read the given tag, we should go and
  //consume all the input until we find a matching end tag.

  nsAutoString endTag("</");
  endTag.Append(NS_EnumToTag(aChildTag));
  endTag.Append(">");

  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_skippedcontent,aChildTag,endTag);
  return aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
}

/**
 * 
 * @update	gess12/28/98
 * @param  
 * @return
 */
nsresult nsHTMLTokenizer::HandleSkippedContent(nsScanner& aScanner,CToken*& aToken) {
  nsresult result=NS_OK;

  eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();
  if(eHTMLTag_unknown!=gHTMLElements[theTag].mSkipTarget) {

      //Do special case handling for <script>, <style>, <title> or <textarea>...
    CToken*   skippedToken=0;
    PRUnichar theChar=0;
    result=ConsumeContentToEndTag(theChar,gHTMLElements[theTag].mSkipTarget,aScanner,skippedToken);

    CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
    AddToken(skippedToken,result,mTokenDeque,theRecycler);

    if(NS_SUCCEEDED(result) && skippedToken){
      //In the case that we just read a given tag, we should go and
      //consume all the tag content itself (and throw it all away).

      nsString& theTagStr=skippedToken->GetStringValueXXX();
      CToken* endtoken=theRecycler->CreateTokenOfType(eToken_end,theTag,theTagStr);
      if(endtoken){
        nsAutoString temp;
        theTagStr.Mid(temp,2,theTagStr.Length()-3);
        //now strip the leading and trailing delimiters...
        endtoken->Reinitialize(theTag,temp);
        AddToken(endtoken,result,mTokenDeque,theRecycler);
      }
    } //if
  } //if
  return result;
}

/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsresult nsHTMLTokenizer::ConsumeStartTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner) {
  PRInt32 theDequeSize=mTokenDeque.GetSize(); //remember this for later in case you have to unwind...
  nsresult result=NS_OK;

  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_start,eHTMLTag_unknown);
  
  if(aToken) {
    result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(NS_SUCCEEDED(result)) {
     
      AddToken(aToken,result,mTokenDeque,theRecycler);
      eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();
      
      if(((CStartToken*)aToken)->IsAttributed()) {
        result=ConsumeAttributes(aChar,(CStartToken*)aToken,aScanner);
      }

      //now that that's over with, we have one more problem to solve.
      //In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
      //consume all the content itself.
      if(NS_SUCCEEDED(result)) {
        result=HandleSkippedContent(aScanner,aToken);
      }
 
      //EEEEECCCCKKKK!!! 
      //This code is confusing, so pay attention.
      //If you're here, it's because we were in the midst of consuming a start
      //tag but ran out of data (not in the stream, but in this *part* of the stream.
      //For simplicity, we have to unwind our input. Therefore, we pop and discard
      //any new tokens we've cued this round. Later we can get smarter about this.
      if(!NS_SUCCEEDED(result)) {
        while(mTokenDeque.GetSize()>theDequeSize) {
          theRecycler->RecycleToken((CToken*)mTokenDeque.Pop());
        }
      }
    } //if
    else theRecycler->RecycleToken(aToken);
  } //if
  return result;
}

/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsresult nsHTMLTokenizer::ConsumeEndTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner) {

  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_end,eHTMLTag_unknown);
  nsresult result=NS_OK;
  
  if(aToken) {
    result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    AddToken(aToken,result,mTokenDeque,theRecycler);
  } //if
  return result;
}

/**
 *  This method is called just after a "&" has been consumed 
 *  and we know we're at the start of an entity.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsHTMLTokenizer::ConsumeEntity(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner) {
   PRUnichar  theChar;
   nsresult result=aScanner.GetChar(theChar);

  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  if(NS_OK==result) {
    if(nsString::IsAlpha(theChar)) { //handle common enity references &xxx; or &#000.
       aToken = theRecycler->CreateTokenOfType(eToken_entity,eHTMLTag_entity);
       result = aToken->Consume(theChar,aScanner);  //tell new token to finish consuming text...    
    }
    else if(kHashsign==theChar) {
       aToken = theRecycler->CreateTokenOfType(eToken_entity,eHTMLTag_entity);
       result=aToken->Consume(theChar,aScanner);
    }
    else {
       //oops, we're actually looking at plain text...
       nsAutoString temp("&");
       aScanner.PutBack(theChar);
       return ConsumeText(temp,aToken,aScanner);
    }//if
    if(aToken){
      char cbuf[30];
      nsString& theStr=aToken->GetStringValueXXX();
      theStr.ToCString(cbuf, sizeof(cbuf)-1);
      if((kHashsign!=theChar) && (-1==NS_EntityToUnicode(cbuf))){
        //if you're here we have a bogus entity.
        //convert it into a text token.
        nsAutoString temp("&");
        temp.Append(theStr);
        CToken* theToken=theRecycler->CreateTokenOfType(eToken_text,eHTMLTag_text,temp);
        theRecycler->RecycleToken(aToken);
        aToken=theToken;
      }
      AddToken(aToken,result,mTokenDeque,theRecycler);
    }
  }//if
  return result;
}


/**
 *  This method is called just after whitespace has been 
 *  consumed and we know we're at the start a whitespace run.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsHTMLTokenizer::ConsumeWhitespace(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner) {
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken = theRecycler->CreateTokenOfType(eToken_whitespace,eHTMLTag_whitespace);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
    AddToken(aToken,result,mTokenDeque,theRecycler);
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
nsresult nsHTMLTokenizer::ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner){
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken = theRecycler->CreateTokenOfType(eToken_comment,eHTMLTag_comment);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
    AddToken(aToken,result,mTokenDeque,theRecycler);
  }
  return result;
}

/**
 *  This method is called just after a known text char has
 *  been consumed and we should read a text run.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */ 
nsresult nsHTMLTokenizer::ConsumeText(const nsString& aString,CToken*& aToken,nsScanner& aScanner){
  nsresult result=NS_OK;
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_text,eHTMLTag_text,aString);
  if(aToken) {
    PRUnichar ch=0;
    result=aToken->Consume(ch,aScanner);
    if(!NS_SUCCEEDED(result)) {
      nsString& temp=aToken->GetStringValueXXX();
      if(0==temp.Length()){
        theRecycler->RecycleToken(aToken);
        aToken = nsnull;
      }
      else result=NS_OK;
    }
    AddToken(aToken,result,mTokenDeque,theRecycler);
  }
  return result;
}

/**
 *  This method is called just after a newline has been consumed. 
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  aToken is the newly created newline token that is parsing
 *  @return error code
 */
nsresult nsHTMLTokenizer::ConsumeNewline(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner){
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_newline,eHTMLTag_newline);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
    AddToken(aToken,result,mTokenDeque,theRecycler);
  }
  return result;
}


/**
 *  This method is called just after a ? has been consumed. 
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  aToken is the newly created newline token that is parsing
 *  @return error code
 */
nsresult nsHTMLTokenizer::ConsumeProcessingInstruction(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner){
  CTokenRecycler* theRecycler=(CTokenRecycler*)GetTokenRecycler();
  aToken=theRecycler->CreateTokenOfType(eToken_instruction,eHTMLTag_unknown);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
    AddToken(aToken,result,mTokenDeque,theRecycler);
  }
  return result;
}

