/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

#include "nsWellFormedDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "nsExpatTokenizer.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prprf.h"

#include "prmem.h"
#include "nsSpecialSystemDirectory.h"

#include <ctype.h>  // toupper()
#include "nsString.h"
#include "nsReadableUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_WELLFORMED_DTD_IID); 


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
nsresult CWellFormedDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (CWellFormedDTD*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewWellFormed_DTD(nsIDTD** aInstancePtrResult)
{
  CWellFormedDTD* it = new CWellFormedDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CWellFormedDTD)
NS_IMPL_RELEASE(CWellFormedDTD)


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CWellFormedDTD::CWellFormedDTD() : nsIDTD() {
  NS_INIT_REFCNT();
  mParser=0;
  mSink=0;
  mLineNumber=1;
  mTokenizer=0;
  mDTDState=NS_OK;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CWellFormedDTD::~CWellFormedDTD(){
  mParser=0; //just to prove we destructed...
  NS_IF_RELEASE(mTokenizer);
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& CWellFormedDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult CWellFormedDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewWellFormed_DTD(aInstancePtrResult);
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult CWellFormedDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(eViewSource!=aParserContext.mParserCommand) {
    if(aParserContext.mMimeType.EqualsWithConversion(kXMLTextContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kXMLApplicationContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kXHTMLApplicationContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kRDFTextContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kXULTextContentType)) {
      result=ePrimaryDetect;
    }
    else {
      if (0 == aParserContext.mMimeType.Length() &&
          kNotFound != aBuffer.Find("<?xml ")) {
        aParserContext.SetMimeType(NS_ConvertASCIItoUCS2(kXMLTextContentType));
        result=eValidDetect;
      }
    }
  }
  return result;
}


/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult CWellFormedDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){

  nsresult result=NS_OK;
  mFilename=aParserContext.mScanner->GetFilename();

  mSink=aSink;
  if((!aParserContext.mPrevContext) && (mSink)) {
    mLineNumber=1;
    result = mSink->WillBuildModel();

#if 0
    /* COMMENT OUT THIS BLOCK IF: you aren't using an nsHTMLContentSink...*/
    {

        //now let's automatically open the html...
      CStartToken theHTMLToken(eHTMLTag_html);
      nsCParserNode theHTMLNode(&theHTMLToken,0);
      mSink->OpenHTML(theHTMLNode);

        //now let's automatically open the body...
      CStartToken theBodyToken(eHTMLTag_body);
      nsCParserNode theBodyNode(&theBodyToken,0);
      mSink->OpenBody(theBodyNode);
    }
    /* COMMENT OUT THIS BLOCK IF: you aren't using an nsHTMLContentSink...*/
#endif
  }

  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess  1/4/99
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CWellFormedDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsHTMLTokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=(nsHTMLTokenizer*)aTokenizer;

    while(NS_SUCCEEDED(result)){
      if(mDTDState!=NS_ERROR_HTMLPARSER_STOPPARSING) {
        CToken* theToken=mTokenizer->PopToken();
        if(theToken) {
          result=HandleToken(theToken,aParser);
          if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
            IF_FREE(theToken, mTokenizer->GetTokenAllocator());
          }
          else {
            // if(NS_ERROR_HTMLPARSER_BLOCK!=result){
            mTokenizer->PushTokenFront(theToken);
          }
        }
        else break;
      }
      else result=mDTDState;
    }//while
    mTokenizer=oldTokenizer;
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CWellFormedDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){
    mSink=aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
        result = mSink->DidBuildModel(1);

  #if 0
      /* COMMENT OUT THIS BLOCK IF: you aren't using an nsHTMLContentSink...*/
      {
        nsIHTMLContentSink* mSink=(nsIHTMLContentSink*)mSink;

          //now let's automatically open the body...
        CEndToken theBodyToken(eHTMLTag_body);
        nsCParserNode theBodyNode(&theBodyToken,0);
        mSink->CloseBody(theBodyNode);

          //now let's automatically open the html...
        CEndToken theHTMLToken(eHTMLTag_html);
        nsCParserNode theHTMLNode(&theBodyToken,0);
        mSink->CloseHTML(theBodyNode);

      }
      /* COMMENT OUT THIS BLOCK IF: you aren't using an nsHTMLContentSink...*/
  #endif
    }
  }
  return result;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsTokenAllocator* CWellFormedDTD::GetTokenAllocator(void){
  nsITokenizer* theTokenizer=0;
  
  nsresult result=GetTokenizer(theTokenizer);

  if (NS_SUCCEEDED(result)) {
    return theTokenizer->GetTokenAllocator();
  }
  return 0;
}

/**
 * Use this id you want to stop the building content model
 * --------------[ Sets DTD to STOP mode ]----------------
 * It's recommended to use this method in accordance with
 * the parser's terminate() method.  Here we process only
 * the "error" token.
 *
 * @update	harishd 07/22/99
 * @param aParser - Parser that exists.
 * @return  NS_ERROR_HTMLPARSER_STOPPARSING or the appropriate error.
 */
nsresult  CWellFormedDTD::Terminate(nsIParser* aParser)
{
  nsresult result=NS_OK;
   
  if(mTokenizer) {
    nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
    if(theAllocator) {
      eHTMLTokenTypes   theType=eToken_unknown;
  
      mDTDState=NS_ERROR_HTMLPARSER_STOPPARSING;
 
      while(result==NS_OK){
        CToken* theToken=mTokenizer->PopToken();
        if(theToken) {
          theType=(eHTMLTokenTypes)theToken->GetTokenType();
          if(theType==eToken_error) {
            result=HandleToken(theToken,aParser);
          }
          IF_FREE(theToken, mTokenizer->GetTokenAllocator());
        }
        else break;
      }//while
    }//if
  }//if
  result=(NS_SUCCEEDED(result))? mDTDState:result;
  return result;
}


/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult CWellFormedDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    mTokenizer=(nsHTMLTokenizer*)new nsExpatTokenizer(&mFilename);
    NS_IF_ADDREF(mTokenizer);
  }
  aTokenizer=mTokenizer;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CWellFormedDTD::WillResumeParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CWellFormedDTD::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void CWellFormedDTD::SetVerification(PRBool aEnabled){
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent -- int tag of parent container
 *  @param   aChild -- int tag of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CWellFormedDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CWellFormedDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWellFormedDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWellFormedDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CWellFormedDTD::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}


/**
 *  
 *  @update  vidur 11/12/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP CWellFormedDTD::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();

  mParser=(nsParser*)aParser;
  mSink=aParser->GetContentSink();

  switch(theType) {

    case eToken_newline:
      mLineNumber++; //now fall through
    case eToken_entity:
    case eToken_whitespace:
    case eToken_text:
    case eToken_cdatasection:
    case eToken_markupDecl:
      result=HandleLeafToken(aToken);
      break;
    case eToken_comment:
      result=HandleCommentToken(aToken);
      break;   
    case eToken_instruction:
      result=HandleProcessingInstructionToken(aToken);
      break;
    case eToken_start:
      result=HandleStartToken(aToken);
      break;
    case eToken_end:
      result=HandleEndToken(aToken);
      break;
    case eToken_error:
      result=HandleErrorToken(aToken);
      break;
    case eToken_doctypeDecl:
      result=HandleDocTypeDeclToken(aToken);
      break;
    case eToken_style:
    case eToken_skippedcontent:
    default:
      result=NS_OK;
  }//switch
  return result;
}

/**
 *  This method gets called when a leaf token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleLeafToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  result= (mSink)? mSink->AddLeaf(theNode):NS_OK;
  return result;
}

/**
 * This method gets called when a comment token has been 
 * encountered in the parse process. Here we also make sure
 * to count the newlines in the comment.
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  CCommentToken* theToken = (CCommentToken*)aToken;
  mLineNumber += CountCharInReadable(theToken->GetStringValue(), 
                                     PRUnichar(kNewLine));
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  result=(mSink)? mSink->AddComment(theNode):NS_OK; 

  return result;
}

/**
 *  This method gets called when a prcessing instruction
 *  has been  encountered in the parse process. 
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleProcessingInstructionToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  result=(mSink)? mSink->AddProcessingInstruction(theNode):NS_OK; 
  return result;
}

/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next (start) token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  PRInt16 attrCount=aToken->GetAttributeCount();
      
  if(0<attrCount){ //go collect the attributes...
    int attr=0;
    for(attr=0;attr<attrCount;attr++){
      CToken* theInnerToken= (mTokenizer)? mTokenizer->PeekToken():nsnull;
      if(theInnerToken)  {
        eHTMLTokenTypes theInnerType=eHTMLTokenTypes(theInnerToken->GetTokenType());
        if(eToken_attribute==theInnerType){
          mTokenizer->PopToken(); //pop it for real...
          theNode.AddAttribute(theInnerToken);
        } 
      }
      else return kEOF;
    }
  }

  // Pass the ID Attribute atom from the start token to the parser node
  CStartToken* startToken = NS_STATIC_CAST(CStartToken *, aToken);
  nsCOMPtr<nsIAtom> IDAttr=nsnull;
  result = startToken->GetIDAttributeAtom(getter_AddRefs(IDAttr));
  if (IDAttr && NS_SUCCEEDED(result))
    result = theNode.SetIDAttributeAtom(IDAttr);

  if(NS_OK==result){
    if(mSink) {
      result=mSink->OpenContainer(theNode); 
      if(((CStartToken*)aToken)->IsEmpty()){
        result=mSink->CloseContainer(theNode); 
      }
    }
  }
  return result;
}

/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process.
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next (end) token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  result=(mSink)? mSink->CloseContainer(theNode):NS_OK;    
  return result;
}

/**
 *  This method gets called when an error token has been 
 *  encountered in the parse process. 
 *  FYI: when the document is malformed this is the only
 *  token that will get reported to the content sink.
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleErrorToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");
  nsresult result=NS_OK;
  
  if(mTokenizer) {
    // Cycle through the remaining tokens in the token stream and handle them
    // These tokens were added so that content objects for the error message
    // are generated by the content sink
    while (PR_TRUE) {
      CToken* token = mTokenizer->PopToken();    
      if(token) {
        eHTMLTokenTypes type = (eHTMLTokenTypes) token->GetTokenType();
        switch(type) {
          case eToken_newline:
            mLineNumber++; //now fall through        
          case eToken_whitespace:
          case eToken_text:        
            HandleLeafToken(token);
            break;
          case eToken_start:
            HandleStartToken(token);
            break;
          case eToken_end:
            HandleEndToken(token);
            break;
          default:
            // Do nothing
            break;
        }
        IF_FREE(token, mTokenizer->GetTokenAllocator());
      }
      else 
        break;
    }
  }

  // Propagate the error onto the content sink.
  CErrorToken *errTok = (CErrorToken *)aToken;
  const nsParserError* error = errTok->GetError();
  result=(mSink)? mSink->NotifyError(error):NS_OK;

  // Output the error to the console  
  if (error) {
    nsCAutoString temp;

    temp.AssignWithConversion(mFilename);

    PR_fprintf(PR_STDOUT, "XML Error in file '%s', ", temp.get());

    PR_fprintf(PR_STDOUT, "Line Number: %d, ", error->lineNumber);
    PR_fprintf(PR_STDOUT, "Col Number: %d, ", error->colNumber);

    temp.AssignWithConversion(error->description);

    PR_fprintf(PR_STDOUT, "Description: %s\n", temp.get());

    temp.AssignWithConversion(error->sourceLine);

    PR_fprintf(PR_STDOUT, "Source Line: %s\n", temp.get());
  }

  return result;
}

/**
 *  This method gets called when a doc. type token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 08/18/99
 *
 *  @param   aToken -- next token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult CWellFormedDTD::HandleDocTypeDeclToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"null token");

  nsresult result=NS_OK;
  
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber,mTokenizer->GetTokenAllocator());
  result = (mSink)? mSink->AddDocTypeDecl(theNode, 0):NS_OK;
  return result;
}

