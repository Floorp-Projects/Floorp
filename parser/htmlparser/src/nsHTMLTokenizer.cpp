/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * @update  gess 4/1/98
 * 
 */

#include "nsHTMLTokenizer.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsElementTable.h"
#include "nsHTMLEntities.h"
#include "CParserContext.h"

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
 *  This method is defined in nsHTMLTokenizer.h. It is used to 
 *  cause the COM-like construction of an HTMLTokenizer.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */

NS_HTMLPARS nsresult NS_NewHTMLTokenizer(nsITokenizer** aInstancePtrResult,PRInt32 aMode,eParserDocType aDocType, eParserCommands aCommand) {
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLTokenizer* it = new nsHTMLTokenizer(aMode,aDocType,aCommand);
  if (nsnull == it) {
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
 nsHTMLTokenizer::nsHTMLTokenizer(PRInt32 aParseMode,
                                  eParserDocType aDocType,
                                  eParserCommands aCommand) :
  nsITokenizer(), mTokenDeque(0), mParseMode(aParseMode)
{
  NS_INIT_REFCNT();
  mDoXMLEmptyTags=((eDTDMode_strict==aParseMode) || (eDTDMode_transitional==aParseMode));
  mDocType=aDocType;
  mRecordTrailingContent=PR_FALSE;
  mParserCommand=aCommand;
  mTokenAllocator=nsnull;
  mTokenScanPos=0;
}


/**
 *  Destructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsHTMLTokenizer::~nsHTMLTokenizer(){
  if(mTokenDeque.GetSize()){
    CTokenDeallocator theDeallocator;
    mTokenDeque.ForEach(theDeallocator);
  }
}
 

/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

void nsHTMLTokenizer::AddToken(CToken*& aToken,nsresult aResult,nsDeque* aDeque,nsTokenAllocator* aTokenAllocator) {
  if(aToken && aDeque) {
    if(NS_SUCCEEDED(aResult)) {
      aDeque->Push(aToken);
    }
    else {
      IF_FREE(aToken);
    }
  }
}

/**
 * Retrieve a ptr to the global token recycler...
 * @update	gess8/4/98
 * @return  ptr to recycler (or null)
 */
nsTokenAllocator* nsHTMLTokenizer::GetTokenAllocator(void) {
  return mTokenAllocator;
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is not really removed from the list.
 * @update	gess8/2/98
 * @return  ptr to token
 */
CToken* nsHTMLTokenizer::PeekToken() {
  return (CToken*)mTokenDeque.PeekFront();
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is really removed from the list; if the list is empty we return 0.
 * @update	gess8/2/98
 * @return  ptr to token or NULL
 */
CToken* nsHTMLTokenizer::PopToken() {
  CToken* result=nsnull;
  result=(CToken*)mTokenDeque.PopFront();
  return result;
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
 * @update	gess 12/29/98
 * @update	harishd 08/04/00
 * @param 
 * @return
 */
nsresult nsHTMLTokenizer::WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator)
{
  mTokenAllocator=aTokenAllocator;
  mIsFinalChunk=aIsFinalChunk;
  mTokenScanPos=mTokenDeque.GetSize()+1; //cause scanDocStructure to search from here for new tokens...
  return NS_OK;
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

static nsDeque& GetTempStack() {
  static nsDeque theTempStack(0);
  return theTempStack;
}


/**
 * This is a utilty method for ScanDocStructure, which finds a given
 * tag in the stack.
 *
 * @update	gess 08/30/00
 * @param   aTag -- the ID of the tag we're seeking
 * @param   aTagStack -- the stack to be searched
 * @return  index pos of tag in stack if found, otherwise kNotFound
 */
static PRInt32 FindLastIndexOfTag(eHTMLTags aTag,nsDeque &aTagStack) {
  PRInt32 theCount=aTagStack.GetSize();
  
  while(0<theCount) {
    CHTMLToken *theToken=(CHTMLToken*)aTagStack.ObjectAt(--theCount);  
    if(theToken) {
      eHTMLTags  theTag=(eHTMLTags)theToken->GetTypeID();
      if(theTag==aTag) {
        return theCount;
      }
    }
  }

  return kNotFound;
}

/**
 * This method scans the sequence of tokens to determine the 
 * well formedness of each tag structure. This is used to 
 * disable residual-style handling in well formed cases.
 *
 * @update	gess 1Sep2000
 * @param 
 * @return
 */
nsresult nsHTMLTokenizer::ScanDocStructure(PRBool aFinalChunk) {
  nsresult result=NS_OK;


  CHTMLToken  *theRootToken=0;

    //*** start by finding the first start tag that hasn't been reviewed.

  while(mTokenScanPos>0) {
    theRootToken=(CHTMLToken*)mTokenDeque.ObjectAt(mTokenScanPos);
    if(theRootToken) {
      eHTMLTokenTypes theType=eHTMLTokenTypes(theRootToken->GetTokenType());  
      if(eToken_start==theType) {
        if(eFormUnknown==theRootToken->GetContainerInfo()) {
          break;
        }
      }      
    }
    mTokenScanPos--;
  }

  /*----------------------------------------------------------------------
   *  Now that we know where to start, let's walk through the
   *  tokens to see which are well-formed. Stop when you run out
   *  of fresh tokens.
   *---------------------------------------------------------------------*/

  theRootToken=(CHTMLToken*)mTokenDeque.ObjectAt(mTokenScanPos); //init to root

  nsDeque       &theStack=GetTempStack();
  eHTMLTags     theRootTag=eHTMLTag_unknown;
  CHTMLToken    *theToken=theRootToken; //init to root
  PRInt32       theStackDepth=0;    

  static  const PRInt32 theMaxStackDepth=200;   //dont bother if we get ridiculously deep.

  while(theToken && (theStackDepth<theMaxStackDepth)) {

    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();
    PRBool          theTagIsBlock=gHTMLElements[theTag].IsMemberOf(kBlockEntity);
    PRBool          theTagIsInline= (theTagIsBlock) ? PR_FALSE : gHTMLElements[theTag].IsMemberOf(kInlineEntity);

    if(theTagIsBlock || theTagIsInline || (eHTMLTag_table==theTag)) {

      switch(theType) {

        case eToken_start:
          if(0==theStack.GetSize()) {
              //track the tag on the top of the stack...
            theRootToken=theToken;
            theRootTag=theTag;
          }
          theStack.Push(theToken);
          theStackDepth++;
          break;

        case eToken_end: 
          {
            CHTMLToken *theLastToken= NS_STATIC_CAST(CHTMLToken*, theStack.Peek());
            if(theLastToken) {
              if(theTag==theLastToken->GetTypeID()) {
                theStack.Pop(); //yank it for real 
                theStackDepth--;
                theLastToken->SetContainerInfo(eWellFormed);

                //in addition, let's look above this container to see if we can find 
                //any tags that are already marked malformed. If so, pop them too!

                theLastToken= NS_STATIC_CAST(CHTMLToken*, theStack.Peek());
                while(theLastToken) {
                  if(eMalformed==theRootToken->GetContainerInfo()) {
                    theStack.Pop(); //yank the malformed token for real.
                    theLastToken= NS_STATIC_CAST(CHTMLToken*, theStack.Peek());
                    continue;
                  }
                  break;
                }
              }
              else {
                //the topmost token isn't what we expected, so that container must
                //be malformed. If the tag is a block, we don't really care (but we'll
                //mark it anyway). If it's an inline we DO care, especially if the 
                //inline tried to contain a block (that's when RS handling kicks in).
                if(theTagIsInline) {
                  PRInt32 theIndex=FindLastIndexOfTag(theTag,theStack);
                  if(kNotFound!=theIndex) {
                    theToken=(CHTMLToken*)theStack.ObjectAt(theIndex);                        
                    theToken->SetContainerInfo(eMalformed);
                  }
                  //otherwise we ignore an out-of-place end tag.
                }
                else {
                }
              }
            }
          } 
          break;

        default:
          break; 
      } //switch

    }

    theToken=(CHTMLToken*)mTokenDeque.ObjectAt(++mTokenScanPos);
  }

  theStack.Empty();
  return result;
}

nsresult nsHTMLTokenizer::DidTokenize(PRBool aFinalChunk) {
  return ScanDocStructure(aFinalChunk);
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
nsresult nsHTMLTokenizer::ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens) {

  PRUnichar theChar;
  CToken* theToken=0;

  nsresult result=aScanner.GetChar(theChar);

  switch(result) {
    case kEOF:
        //We convert from eof to complete here, because we never really tried to get data.
        //All we did was try to see if data was available, which it wasn't.
        //It's important to return process complete, so that controlling logic can know that
        //everything went well, but we're done with token processing.
      return result;

    case NS_OK:
    default:

      if(ePlainText!=mDocType) {
        if(kLessThan==theChar) {
          return ConsumeTag(theChar,theToken,aScanner,aFlushTokens);
        }
        else if(kAmpersand==theChar){
          return ConsumeEntity(theChar,theToken,aScanner);
        }
      }
      
      if((kCR==theChar) || (kLF==theChar)) {
        return ConsumeNewline(theChar,theToken,aScanner);
      }
      else {
        if(!nsCRT::IsAsciiSpace(theChar)) {
          nsAutoString temp(theChar);
          result=ConsumeText(temp,theToken,aScanner);
          break;
        }
        result=ConsumeWhitespace(theChar,theToken,aScanner);
      } 
      break; 
  } //switch

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
nsresult nsHTMLTokenizer::ConsumeTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,PRBool& aFlushTokens) {

  nsresult result=aScanner.GetChar(aChar);

  if(NS_OK==result) {

    switch(aChar) {
      case kForwardSlash:
        PRUnichar ch; 
        result=aScanner.Peek(ch);
        if(NS_OK==result) {
          if(nsCRT::IsAsciiAlpha(ch)||(kGreaterThan==ch)) {
            result=ConsumeEndTag(aChar,aToken,aScanner);
          }
          else result=ConsumeComment(aChar,aToken,aScanner);
        }//if
        break;

      case kExclamation:
        PRUnichar theNextChar; 
        result=aScanner.Peek(theNextChar);
        if(NS_OK==result) {
          if((kMinus==theNextChar) || (kGreaterThan==theNextChar)) {
            result=ConsumeComment(aChar,aToken,aScanner);
          }
          else
            result=ConsumeSpecialMarkup(aChar,aToken,aScanner); 
        }
        break;

      case kQuestionMark: //it must be an XML processing instruction...
        result=ConsumeProcessingInstruction(aChar,aToken,aScanner);
        break;

      default:
        if(nsCRT::IsAsciiAlpha(aChar))
          result=ConsumeStartTag(aChar,aToken,aScanner,aFlushTokens);
        else if(kEOF!=aChar) {
          // We are not dealing with a tag. So, put back the char
          // and leave the decision to ConsumeText().
          aScanner.PutBack(aChar);
          nsAutoString temp; temp.AssignWithConversion("<");
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
 *  @update  rickg  03.23.2000
 *  @param   aChar: last char read
 *  @param   aScanner: see nsScanner.h
 *  @param   aLeadingWS: contains ws chars that preceeded the first attribute
 *  @return  
 */
nsresult nsHTMLTokenizer::ConsumeAttributes(PRUnichar aChar,CStartToken* aToken,nsScanner& aScanner,nsString& aLeadingWS) {
  PRBool done=PR_FALSE;
  nsresult result=NS_OK;
  PRInt16 theAttrCount=0;

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();

  while((!done) && (result==NS_OK)) {
    CToken* theToken= (CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown);
    if(theToken){
      if(aLeadingWS.Length()) {
        nsString& theKey=((CAttributeToken*)theToken)->GetKey();
        theKey=aLeadingWS;
        aLeadingWS.Truncate(0);
      }
      result=theToken->Consume(aChar,aScanner,PRBool(eViewSource==mParserCommand));  //tell new token to finish consuming text...    
 
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
          IF_FREE(theToken);
        }
        else {
          theAttrCount++;
          AddToken(theToken,result,&mTokenDeque,theAllocator);
        }
      }
      else { //if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result){
        aToken->SetEmpty(PR_TRUE);
        IF_FREE(theToken);
        if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result)
          result=NS_OK;
      }
    }//if
    
    if(NS_SUCCEEDED(result)){
      result=aScanner.SkipWhitespace();
      if(NS_SUCCEEDED(result)) {
        result=aScanner.Peek(aChar);
        if(NS_SUCCEEDED(result)) {
          if(aChar==kGreaterThan) { //you just ate the '>'
            aScanner.GetChar(aChar); //skip the '>'
            done=PR_TRUE;
          }
          else if(aChar==kLessThan) {
            eHTMLTags theEndTag = (eHTMLTags)aToken->GetTypeID();
            if(result==NS_OK&&(gHTMLElements[theEndTag].mSkipTarget)){
              CToken* theEndToken=theAllocator->CreateTokenOfType(eToken_end,theEndTag);
              AddToken(theEndToken,NS_OK,&mTokenDeque,theAllocator); 
            }
            done=PR_TRUE;
          }
        }//if
      }
    }//if
  }//while

  aToken->SetAttributeCount(theAttrCount);
  return result;
}

/**
 * In the case that we just read the given tag, we should go and
 * consume all the input until we find a matching end tag.
 * @update	gess12/28/98
 * @param  
 * @return
 */
nsresult nsHTMLTokenizer::ConsumeScriptContent(nsScanner& aScanner,CToken*& aToken) {
  nsresult result=NS_OK;

  return result;
}

nsString& GetScratchString(void) {
  static nsString gScratchString;
  gScratchString.Truncate(0);
  return gScratchString;
}

/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsresult nsHTMLTokenizer::ConsumeStartTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,PRBool& aFlushTokens) {
  PRInt32 theDequeSize=mTokenDeque.GetSize(); //remember this for later in case you have to unwind...
  nsresult result=NS_OK;

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_start,eHTMLTag_unknown);
  
  if(aToken) {
    ((CStartToken*)aToken)->mOrigin=aScanner.GetOffset()-1; // Save the position after '<' for use in recording traling contents. Ref: Bug. 15204.
    PRBool isHTML=((eHTML3Text==mDocType) || (eHTML4Text==mDocType));
    result= aToken->Consume(aChar,aScanner,isHTML);     //tell new token to finish consuming text...    

    if(NS_SUCCEEDED(result)) {
     
      AddToken(aToken,result,&mTokenDeque,theAllocator);
      eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();

       //Good. Now, let's see if the next char is ">". 
       //If so, we have a complete tag, otherwise, we have attributes.
      nsString &theScratchStr=GetScratchString();
      PRBool theTagHasAttributes=PR_FALSE;
      if(NS_OK==result) { 
        result=(eViewSource==mParserCommand) ? aScanner.ReadWhitespace(theScratchStr) : aScanner.SkipWhitespace();
        aToken->mNewlineCount += aScanner.GetNewlinesSkipped();
        if(NS_OK==result) {
          result=aScanner.GetChar(aChar);
          if(NS_OK==result) {
            if(kGreaterThan!=aChar) { //look for '>' 
             //push that char back, since we apparently have attributes...
              result=aScanner.PutBack(aChar);
              theTagHasAttributes=PR_TRUE;
            } //if
          } //if
        }//if
      }
      
      if(theTagHasAttributes) {
        result=ConsumeAttributes(aChar,(CStartToken*)aToken,aScanner,theScratchStr);
      }

      /*  Now that that's over with, we have one more problem to solve.
          In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
          consume all the content itself.
       */
      if(NS_SUCCEEDED(result)) {
        
        //XXX - Find a better soution to record content
        //Added _plaintext to fix bug 46054.
        if((eHTMLTag_textarea==theTag || eHTMLTag_xmp==theTag || eHTMLTag_plaintext==theTag) && !mRecordTrailingContent) {
          mRecordTrailingContent=PR_TRUE;
        }
          
        if(mRecordTrailingContent) 
          RecordTrailingContent((CStartToken*)aToken,aScanner);
        
        //if((eHTMLTag_style==theTag) || (eHTMLTag_script==theTag)) {
        if(gHTMLElements[theTag].CanContainType(kCDATA)) {
          nsAutoString endTag; endTag.AssignWithConversion(nsHTMLTags::GetStringValue(theTag));
          endTag.InsertWithConversion("</",0,2);
          CToken* textToken=theAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text);
          result=((CTextToken*)textToken)->ConsumeUntil(0,PRBool(theTag!=eHTMLTag_script),aScanner,endTag,mParseMode,aFlushTokens);  //tell new token to finish consuming text...    
          //endTag.Append(">");
          CToken* endToken=theAllocator->CreateTokenOfType(eToken_end,theTag,endTag);
          AddToken(textToken,result,&mTokenDeque,theAllocator);
          AddToken(endToken,result,&mTokenDeque,theAllocator);
        }
      }
 
      //EEEEECCCCKKKK!!! 
      //This code is confusing, so pay attention.
      //If you're here, it's because we were in the midst of consuming a start
      //tag but ran out of data (not in the stream, but in this *part* of the stream.
      //For simplicity, we have to unwind our input. Therefore, we pop and discard
      //any new tokens we've cued this round. Later we can get smarter about this.
      if(!NS_SUCCEEDED(result)) {
        while(mTokenDeque.GetSize()>theDequeSize) {
          CToken* theToken=(CToken*)mTokenDeque.Pop();
          IF_FREE(theToken);
        }
      }
    } //if
    else IF_FREE(aToken);
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

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_end,eHTMLTag_unknown);
  nsresult result=NS_OK;
  
  if(aToken) {
    eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();
    if((theTag==eHTMLTag_textarea || theTag==eHTMLTag_xmp) && mRecordTrailingContent) {
      mRecordTrailingContent=PR_FALSE;
    }
    result= aToken->Consume(aChar,aScanner,mParseMode);  //tell new token to finish consuming text...    
    AddToken(aToken,result,&mTokenDeque,theAllocator);
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

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  if(NS_OK==result) {
    if(nsCRT::IsAsciiAlpha(theChar)) { //handle common enity references &xxx; or &#000.
       aToken = theAllocator->CreateTokenOfType(eToken_entity,eHTMLTag_entity);
       result = aToken->Consume(theChar,aScanner,mParseMode);  //tell new token to finish consuming text...    
    }
    else if(kHashsign==theChar) {
       aToken = theAllocator->CreateTokenOfType(eToken_entity,eHTMLTag_entity);
       result=aToken->Consume(theChar,aScanner,mParseMode);
    }
    else {
       //oops, we're actually looking at plain text...
       nsAutoString temp; temp.AssignWithConversion("&");
       aScanner.PutBack(theChar);
       return ConsumeText(temp,aToken,aScanner);
    }//if
    if(aToken){
#if 0
      nsString& theStr=aToken->GetStringValueXXX();

      if((kHashsign!=theChar) && (-1==nsHTMLEntities::EntityToUnicode(theStr))){
        //if you're here we have a bogus entity.
        //convert it into a text token.
        nsAutoString temp; temp.AssignWithConversion("&");
        temp.Append(theStr);
        CToken* theToken=theAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,temp);
        IF_FREE(aToken);
        aToken=theToken;
      }
#endif

      if(mIsFinalChunk && (kEOF==result)) {
        result=NS_OK; //use as much of the entity as you can get.
      }
      AddToken(aToken,result,&mTokenDeque,theAllocator);
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
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_whitespace,eHTMLTag_whitespace);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mParseMode);
    AddToken(aToken,result,&mTokenDeque,theAllocator);
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
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_comment,eHTMLTag_comment);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mParseMode);
    AddToken(aToken,result,&mTokenDeque,theAllocator);
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
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,aString);
  if(aToken) {
    PRUnichar ch=0;
    result=aToken->Consume(ch,aScanner,mParseMode);
    if(!NS_SUCCEEDED(result)) {
      nsString& temp=aToken->GetStringValueXXX();
      if(0==temp.Length()){
        IF_FREE(aToken);
        aToken = nsnull;
      }
      else result=NS_OK;
    }
    AddToken(aToken,result,&mTokenDeque,theAllocator);
  }
  return result;
}

/**
 *  This method is called just after a "<!" has been consumed.
 *  NOTE: Here we might consume DOCTYPE and "special" markups. 
 * 
 *  
 *  @update harishd 09/02/99
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsHTMLTokenizer::ConsumeSpecialMarkup(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner){
  nsresult result=NS_OK;
  nsAutoString theBufCopy;
  nsString& theBuffer=aScanner.GetBuffer();
  theBuffer.Mid(theBufCopy,aScanner.GetOffset(),20);
  theBufCopy.ToUpperCase();
  PRInt32 theIndex=theBufCopy.Find("DOCTYPE");
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  
  if(theIndex==kNotFound) {
    if('['==theBufCopy.CharAt(0)) 
      aToken = theAllocator->CreateTokenOfType(eToken_cdatasection,eHTMLTag_comment);  
    else aToken = theAllocator->CreateTokenOfType(eToken_comment,eHTMLTag_comment);
  }
  else
    aToken = theAllocator->CreateTokenOfType(eToken_doctypeDecl,eHTMLTag_markupDecl);
  
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mParseMode);
    AddToken(aToken,result,&mTokenDeque,theAllocator);
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
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_newline,eHTMLTag_newline);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mParseMode);
    AddToken(aToken,result,&mTokenDeque,theAllocator);
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
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_instruction,eHTMLTag_unknown);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mParseMode);
    AddToken(aToken,result,&mTokenDeque,theAllocator);
  }
  return result;
}

/**
 *  This method keeps a copy of contents within the start token.
 *  The stored content could later be used in displaying TEXTAREA, 
 *  and also in view source.
 *  
 *  @update harishd 11/09/99
 *  @param  aStartToken: The token whose trailing contents are to be recorded
 *  @param  aScanner: see nsScanner.h
 *  
 */

void nsHTMLTokenizer::RecordTrailingContent(CStartToken* aStartToken, nsScanner& aScanner) {
  if(aStartToken) {
    PRInt32   theOrigin        =aStartToken->mOrigin;
    PRInt32   theCurrOffset    =aScanner.GetOffset();
    PRInt32   theLength        =(theCurrOffset>theOrigin)? theCurrOffset-theOrigin:-1;
    if(theLength>0) {
      nsString& theRawXXX      =aStartToken->mTrailingContent;
      const PRUnichar* theBuff =(aScanner.GetBuffer()).GetUnicode();
      theRawXXX.Append(&theBuff[theOrigin],theLength);
    }
  }
}

