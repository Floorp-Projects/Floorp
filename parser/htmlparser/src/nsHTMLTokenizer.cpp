/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#include "nsIAtom.h"
#include "nsHTMLTokenizer.h"
#include "nsScanner.h"
#include "nsElementTable.h"
#include "CParserContext.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

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
    *aInstancePtr = (nsISupports*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do IParser base class...
    *aInstancePtr = (nsITokenizer*)(this);                                        
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

nsresult NS_NewHTMLTokenizer(nsITokenizer** aInstancePtrResult,
                                         PRInt32 aFlag,
                                         eParserDocType aDocType, 
                                         eParserCommands aCommand) 
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLTokenizer* it = new nsHTMLTokenizer(aFlag,aDocType,aCommand);
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
  nsITokenizer(), mTokenDeque(0)
{
  if (aParseMode==eDTDMode_full_standards ||
      aParseMode==eDTDMode_almost_standards) {
    mFlags = NS_IPARSER_FLAG_STRICT_MODE;
  }
  else if (aParseMode==eDTDMode_quirks)  {
    mFlags = NS_IPARSER_FLAG_QUIRKS_MODE;
  }
  else if (aParseMode==eDTDMode_autodetect) {
    mFlags = NS_IPARSER_FLAG_AUTO_DETECT_MODE;
  }
  else {
    mFlags = NS_IPARSER_FLAG_UNKNOWN_MODE;
  }

  if (aDocType==ePlainText) {
    mFlags |= NS_IPARSER_FLAG_PLAIN_TEXT;
  }
  else if (aDocType==eXML) {
    mFlags |= NS_IPARSER_FLAG_XML;
  }
  else if (aDocType==eHTML_Quirks ||
           aDocType==eHTML3_Quirks ||
           aDocType==eHTML_Strict) {
    mFlags |= NS_IPARSER_FLAG_HTML;
  }
  
  mFlags |= (aCommand==eViewSource)? NS_IPARSER_FLAG_VIEW_SOURCE:NS_IPARSER_FLAG_VIEW_NORMAL;

  mTokenAllocator = nsnull;
  mTokenScanPos = 0;
  mPreserveTarget = eHTMLTag_unknown;
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
    CTokenDeallocator theDeallocator(mTokenAllocator->GetArenaPool());
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
      IF_FREE(aToken, aTokenAllocator);
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
  mTokenScanPos=mTokenDeque.GetSize(); //cause scanDocStructure to search from here for new tokens...
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
  for(anIndex=0;anIndex<aCount;++anIndex){
    CToken* theToken=(CToken*)aDeque.Pop();
    PushTokenFront(theToken);
  }

}

NS_IMETHODIMP
nsHTMLTokenizer::CopyState(nsITokenizer* aTokenizer)
{
  if (aTokenizer) {
    mFlags &= ~NS_IPARSER_FLAG_PRESERVE_CONTENT;
    mPreserveTarget =
      NS_STATIC_CAST(nsHTMLTokenizer*, aTokenizer)->mPreserveTarget;
    if (mPreserveTarget != eHTMLTag_unknown)
      mFlags |= NS_IPARSER_FLAG_PRESERVE_CONTENT;
  }
  return NS_OK;
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
  if (!mTokenDeque.GetSize())
    return result;

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

  nsDeque       theStack(0);
  eHTMLTags     theRootTag=eHTMLTag_unknown;
  CHTMLToken    *theToken=theRootToken; //init to root
  PRInt32       theStackDepth=0;    

  static  const PRInt32 theMaxStackDepth=200;   //dont bother if we get ridiculously deep.

  while(theToken && (theStackDepth<theMaxStackDepth)) {

    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();

    PRBool          theTagIsContainer=nsHTMLElement::IsContainer(theTag);  //bug54117...

    if(theTagIsContainer) {
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
            ++theStackDepth;
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
    }

    theToken=(CHTMLToken*)mTokenDeque.ObjectAt(++mTokenScanPos);
  }

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

  nsresult result=aScanner.Peek(theChar);

  switch(result) {
    case kEOF:
        //We convert from eof to complete here, because we never really tried to get data.
        //All we did was try to see if data was available, which it wasn't.
        //It's important to return process complete, so that controlling logic can know that
        //everything went well, but we're done with token processing.
      return result;

    case NS_OK:
    default:

      if(!(mFlags & NS_IPARSER_FLAG_PLAIN_TEXT)) {
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
          if(theChar!=nsnull) { 
            result=ConsumeText(theToken,aScanner); 
          } 
          else { 
            aScanner.GetChar(theChar); // skip the embedded null char. Fix bug 64098. 
          } 
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

  PRUnichar theNextChar, oldChar;
  nsresult result=aScanner.Peek(aChar,1);

  if(NS_OK==result) {

    switch(aChar) {
      case kForwardSlash:
        // Get the original "<" (we've already seen it with a Peek)
        aScanner.GetChar(oldChar);

        result=aScanner.Peek(theNextChar, 1);
        if(NS_OK==result) {
          // xml allow non ASCII tag name, consume as end tag. need to make xml view source work
          PRBool isXML=(mFlags & NS_IPARSER_FLAG_XML);
          if(nsCRT::IsAsciiAlpha(theNextChar)||(kGreaterThan==theNextChar)|| 
             (isXML && (! nsCRT::IsAscii(theNextChar)))) { 
            result=ConsumeEndTag(aChar,aToken,aScanner);
          }
          else result=ConsumeComment(aChar,aToken,aScanner);
        }//if
        break;

      case kExclamation:
        // Get the original "<" (we've already seen it with a Peek)
        aScanner.GetChar(oldChar);

        result=aScanner.Peek(theNextChar, 1);
        if(NS_OK==result) {
          if((kMinus==theNextChar) || (kGreaterThan==theNextChar)) {
            result=ConsumeComment(aChar,aToken,aScanner);
          }
          else
            result=ConsumeSpecialMarkup(aChar,aToken,aScanner); 
        }
        break;

      case kQuestionMark: //it must be an XML processing instruction...
        // Get the original "<" (we've already seen it with a Peek)
        aScanner.GetChar(oldChar);
        result=ConsumeProcessingInstruction(aChar,aToken,aScanner);
        break;

      default:
        // xml allows non ASCII tag names, consume as a start tag.
        PRBool isXML=(mFlags & NS_IPARSER_FLAG_XML);
        if(nsCRT::IsAsciiAlpha(aChar) ||
            (isXML && (! nsCRT::IsAscii(aChar)))) { 
          // Get the original "<" (we've already seen it with a Peek)
          aScanner.GetChar(oldChar);
          result=ConsumeStartTag(aChar,aToken,aScanner,aFlushTokens);
        }
        else {
          // We are not dealing with a tag. So, don't consume the original
          // char and leave the decision to ConsumeText().
          result=ConsumeText(aToken,aScanner);
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
nsresult nsHTMLTokenizer::ConsumeAttributes(PRUnichar aChar,
                                            CToken* aToken,
                                            nsScanner& aScanner) {
  PRBool done=PR_FALSE;
  nsresult result=NS_OK;
  PRInt16 theAttrCount=0;

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();

  while((!done) && (result==NS_OK)) {
    CAttributeToken* theToken= NS_STATIC_CAST(CAttributeToken*, theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown));
    if(theToken){
      result=theToken->Consume(aChar,aScanner,mFlags);  //tell new token to finish consuming text...    
 
      //Much as I hate to do this, here's some special case code.
      //This handles the case of empty-tags in XML. Our last
      //attribute token will come through with a text value of ""
      //and a textkey of "/". We should destroy it, and tell the 
      //start token it was empty.
      if(NS_SUCCEEDED(result)) {
        PRBool isUsableAttr = PR_TRUE;
        const nsAString& key=theToken->GetKey();
        const nsAString& text=theToken->GetValue();

         // support XML like syntax to fix bugs like 44186
        if(!key.IsEmpty() && kForwardSlash==key.First() && text.IsEmpty()) {
          isUsableAttr = PRBool(mFlags & NS_IPARSER_FLAG_VIEW_SOURCE); // Fix bug 103095
          aToken->SetEmpty(isUsableAttr);
        }
        if(isUsableAttr) {
          ++theAttrCount;
          AddToken((CToken*&)theToken,result,&mTokenDeque,theAllocator);
        }
        else {
          IF_FREE(theToken, mTokenAllocator);
        }
      }
      else { //if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result){
        IF_FREE(theToken, mTokenAllocator);
        //Bad attributes are not a reason to set empty.
        if(NS_ERROR_HTMLPARSER_BADATTRIBUTE==result) {
          result=NS_OK;
        } else {
          aToken->SetEmpty(PR_TRUE);
        }
      }
    }//if
    
#ifdef DEBUG
    if(NS_SUCCEEDED(result)){
      PRInt32 newline = 0;
      result = aScanner.SkipWhitespace(newline);
      NS_ASSERTION(newline == 0, "CAttribute::Consume() failed to collect all the newlines!");
    }
#endif
    if (NS_SUCCEEDED(result)) {
      result = aScanner.Peek(aChar);
      if (NS_SUCCEEDED(result)) {
        if (aChar == kGreaterThan) { //you just ate the '>'
          aScanner.GetChar(aChar); //skip the '>'
          done = PR_TRUE;
        }
        else if(aChar == kLessThan) {
          done = PR_TRUE;
        }
      }//if
    }//if
  }//while

  aToken->SetAttributeCount(theAttrCount);
  return result;
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
    // Save the position after '<' for use in recording traling contents. Ref: Bug. 15204.
    nsScannerIterator origin;
    aScanner.CurrentPosition(origin);

    result= aToken->Consume(aChar,aScanner,mFlags);     //tell new token to finish consuming text...    

    if(NS_SUCCEEDED(result)) {
     
      AddToken(aToken,result,&mTokenDeque,theAllocator);
      NS_ENSURE_SUCCESS(result, result);

      eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();

      //Good. Now, let's see if the next char is ">". 
      //If so, we have a complete tag, otherwise, we have attributes.
      result = aScanner.Peek(aChar);
      NS_ENSURE_SUCCESS(result, result);

      if(kGreaterThan != aChar) { //look for '>' 
        result = ConsumeAttributes(aChar, aToken, aScanner);
      } //if
      else {
        aScanner.GetChar(aChar);
      }        

      /*  Now that that's over with, we have one more problem to solve.
          In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
          consume all the content itself.
       */
      if(NS_SUCCEEDED(result)) {
        CStartToken* theStartToken = NS_STATIC_CAST(CStartToken*,aToken);
        //XXX - Find a better soution to record content
        if(!(mFlags & NS_IPARSER_FLAG_PRESERVE_CONTENT) &&
           (theTag == eHTMLTag_textarea  ||
            theTag == eHTMLTag_xmp       || 
            theTag == eHTMLTag_noscript  ||
            theTag == eHTMLTag_noframes)) {
          NS_ASSERTION(mPreserveTarget == eHTMLTag_unknown,
                       "mPreserveTarget set but not preserving content?");
          mPreserveTarget = theTag;
          mFlags |= NS_IPARSER_FLAG_PRESERVE_CONTENT;
        }
          
        if (mFlags & NS_IPARSER_FLAG_PRESERVE_CONTENT) 
          PreserveToken(theStartToken, aScanner, origin);
        
        //if((eHTMLTag_style==theTag) || (eHTMLTag_script==theTag)) {
        if(gHTMLElements[theTag].CanContainType(kCDATA)) {
          nsDependentString endTagName(nsHTMLTags::GetStringValue(theTag)); 

          CToken*     text=theAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text);
          CTextToken* textToken=NS_STATIC_CAST(CTextToken*,text);

          //tell new token to finish consuming text...    
          result=textToken->ConsumeUntil(0,theTag!=eHTMLTag_script,
                                         aScanner,
                                         endTagName,
                                         mFlags,
                                         aFlushTokens);
          
          // Fix bug 44186
          // Support XML like syntax, i.e., <script src="external.js"/> == <script src="external.js"></script>
          // Note: if aFlushTokens is TRUE then we have seen an </script>
          // We do NOT want to output the end token if we didn't see a
          // </script> and have a preserve target.  If that happens, then we'd
          // be messing up the text inside the <textarea> or <xmp> or whatever
          // it is.
          if((!(mFlags & NS_IPARSER_FLAG_PRESERVE_CONTENT) &&
              !theStartToken->IsEmpty()) || aFlushTokens) {
            // Setting this would make cases like <script/>d.w("text");</script> work.
            theStartToken->SetEmpty(PR_FALSE);
            // do this up here so we can just add the end token later on
            AddToken(text,result,&mTokenDeque,theAllocator);

            CToken* endToken=nsnull;
            
            if (NS_SUCCEEDED(result) && aFlushTokens) {
              PRUnichar theChar;
              // Get the <
              result = aScanner.GetChar(theChar);
              NS_ASSERTION(NS_SUCCEEDED(result) && theChar == kLessThan,
                           "CTextToken::ConsumeUntil is broken!");
#ifdef DEBUG
              // Ensure we have a /
              PRUnichar tempChar;  // Don't change non-debug vars in debug-only code
              result = aScanner.Peek(tempChar);
              NS_ASSERTION(NS_SUCCEEDED(result) && tempChar == kForwardSlash,
                           "CTextToken::ConsumeUntil is broken!");
#endif
              result = ConsumeEndTag(PRUnichar('/'),endToken,aScanner);
            } else if (!(mFlags & NS_IPARSER_FLAG_VIEW_SOURCE)) {
              endToken=theAllocator->CreateTokenOfType(eToken_end,theTag,endTagName);
              AddToken(endToken,result,&mTokenDeque,theAllocator);
            }
          }
          else {
            IF_FREE(text, mTokenAllocator);
          }
        }
      }
 
      //EEEEECCCCKKKK!!! 
      //This code is confusing, so pay attention.
      //If you're here, it's because we were in the midst of consuming a start
      //tag but ran out of data (not in the stream, but in this *part* of the stream.
      //For simplicity, we have to unwind our input. Therefore, we pop and discard
      //any new tokens we've cued this round. Later we can get smarter about this.
      if(NS_FAILED(result)) {
        while(mTokenDeque.GetSize()>theDequeSize) {
          CToken* theToken=(CToken*)mTokenDeque.Pop();
          IF_FREE(theToken, mTokenAllocator);
        }
      }
    } //if
    else IF_FREE(aToken, mTokenAllocator);
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
 
  // Get the "/" (we've already seen it with a Peek)
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_end,eHTMLTag_unknown);
  nsresult result=NS_OK;
  
  if(aToken) {
    result= aToken->Consume(aChar,aScanner,mFlags);  //tell new token to finish consuming text...    
    AddToken(aToken,result,&mTokenDeque,theAllocator);
    NS_ENSURE_SUCCESS(result, result);
      
    result = aScanner.Peek(aChar);
    NS_ENSURE_SUCCESS(result, result);

    if(kGreaterThan != aChar) {
      result = ConsumeAttributes(aChar, aToken, aScanner);
      NS_ENSURE_SUCCESS(result, result);
    }
    else {
      aScanner.GetChar(aChar);
    }        

    if (NS_SUCCEEDED(result)) {
      eHTMLTags theTag = (eHTMLTags)aToken->GetTypeID();
      if (mPreserveTarget == theTag) {
        // Target reached. Stop preserving content.
        mPreserveTarget = eHTMLTag_unknown;
        mFlags &= ~NS_IPARSER_FLAG_PRESERVE_CONTENT;
      }
    }
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
   nsresult result=aScanner.Peek(theChar, 1);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  if (NS_SUCCEEDED(result)) {
    if (nsCRT::IsAsciiAlpha(theChar) || theChar==kHashsign) {
      aToken = theAllocator->CreateTokenOfType(eToken_entity,eHTMLTag_entity);
      result=aToken->Consume(theChar,aScanner,mFlags);

      if (result == NS_HTMLTOKENS_NOT_AN_ENTITY) {
        IF_FREE(aToken, mTokenAllocator);
      }
      else {
        if (mIsFinalChunk && result == kEOF) {
          result=NS_OK; //use as much of the entity as you can get.
        }
        AddToken(aToken,result,&mTokenDeque,theAllocator);
        return result;
      }
    }
    // oops, we're actually looking at plain text...
    result = ConsumeText(aToken,aScanner);
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
  // Get the whitespace character
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_whitespace,eHTMLTag_whitespace);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mFlags);
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
  // Get the "!"
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_comment,eHTMLTag_comment);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mFlags);
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
nsresult nsHTMLTokenizer::ConsumeText(CToken*& aToken,nsScanner& aScanner){
  nsresult result=NS_OK;
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  CTextToken* theToken = (CTextToken*)theAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text);
  if(theToken) {
    PRUnichar ch=0;
    result=theToken->Consume(ch,aScanner,mFlags);
    if(NS_FAILED(result)) {
      if(0==theToken->GetTextLength()){
        IF_FREE(aToken, mTokenAllocator);
        aToken = nsnull;
      }
      else result=NS_OK;
    }
    aToken = theToken;
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

  // Get the "!"
  aScanner.GetChar(aChar);

  nsresult result=NS_OK;
  nsAutoString theBufCopy;
  aScanner.Peek(theBufCopy, 20);
  ToUpperCase(theBufCopy);
  PRInt32 theIndex=theBufCopy.Find("DOCTYPE");
  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  
  if(theIndex==kNotFound) {
    if('['==theBufCopy.CharAt(0)) {
      aToken = theAllocator->CreateTokenOfType(eToken_cdatasection,eHTMLTag_comment);  
    } else if (StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ELEMENT")) ||
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ATTLIST")) || 
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ENTITY")) || 
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("NOTATION"))) {
      aToken = theAllocator->CreateTokenOfType(eToken_markupDecl,eHTMLTag_markupDecl);
    } else {
      aToken = theAllocator->CreateTokenOfType(eToken_comment,eHTMLTag_comment);
    }
  }
  else
    aToken = theAllocator->CreateTokenOfType(eToken_doctypeDecl,eHTMLTag_doctypeDecl);
  
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mFlags);
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
  // Get the newline character
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_newline,eHTMLTag_newline);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mFlags);
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
  
  // Get the "?"
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator=this->GetTokenAllocator();
  aToken=theAllocator->CreateTokenOfType(eToken_instruction,eHTMLTag_unknown);
  nsresult result=NS_OK;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner,mFlags);
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

void nsHTMLTokenizer::PreserveToken(CStartToken* aStartToken, 
                                    nsScanner& aScanner, 
                                    nsScannerIterator aOrigin) {
  if(aStartToken) {
    nsScannerIterator theCurrentPosition;
    aScanner.CurrentPosition(theCurrentPosition);

    nsString& trailingContent = aStartToken->mTrailingContent;
    PRUint32 oldLength = trailingContent.Length();
    trailingContent.SetLength(oldLength + Distance(aOrigin, theCurrentPosition));

    nsWritingIterator<PRUnichar> beginWriting;
    trailingContent.BeginWriting(beginWriting);
    beginWriting.advance(oldLength);

    copy_string( aOrigin, theCurrentPosition, beginWriting );
  }
}
