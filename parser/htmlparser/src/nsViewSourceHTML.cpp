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
 * @update  gess 4/8/98
 * 
 *         
 */
 

#ifdef RAPTOR_PERF_METRICS
#  define START_TIMER()                    \
    if(mParser) mParser->mParseTime.Start(PR_FALSE); \
    if(mParser) mParser->mDTDTime.Start(PR_FALSE); 

#  define STOP_TIMER()                     \
    if(mParser) mParser->mParseTime.Stop(); \
    if(mParser) mParser->mDTDTime.Stop(); 

#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif


#include "nsIDTDDebug.h"
#include "nsViewSourceHTML.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsTokenHandler.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "COtherDTD.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"

#include "prmem.h"


#ifdef RAPTOR_PERF_METRICS
#include "stopwatch.h"
Stopwatch vsTimer;
#endif


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_VIEWSOURCE_HTML_IID); 
static int gErrorThreshold = 10;

//#define rickgdebug
#ifdef rickgdebug
#include <fstream.h>
  fstream* gDumpFile=0;
#endif


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
nsresult CViewSourceHTML::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CViewSourceHTML*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult)
{
  CViewSourceHTML* it = new CViewSourceHTML();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CViewSourceHTML)
NS_IMPL_RELEASE(CViewSourceHTML)

/********************************************
 ********************************************/

class CIndirectTextToken : public CTextToken {
public:
  CIndirectTextToken() : CTextToken() {
    mIndirectString=0;
  }
  
  void SetIndirectString(const nsString& aString) {
    mIndirectString=&aString;
  }

  virtual nsString& GetStringValueXXX(void){
    return (nsString&)*mIndirectString;
  }

  const nsString* mIndirectString;
};


/*******************************************************************
  Now define the CSharedVSCOntext class...
 *******************************************************************/

class CSharedVSContext {
public:

  CSharedVSContext() : 
    mEndNode(), 
    mStartNode(),
    mTokenNode(),
    mErrorNode(),
    mITextToken(),
    mITextNode(&mITextToken),
    mTextToken(),
    mErrorToken("error"),
    mTextNode(&mTextToken){
  }
  
  ~CSharedVSContext() {
  }

  static CSharedVSContext& GetSharedContext() {
    static CSharedVSContext gSharedVSContext;
    return gSharedVSContext;
  }

  nsCParserNode       mEndNode;
  nsCParserNode       mStartNode;
  nsCParserNode       mTokenNode;
  nsCParserNode       mErrorNode;
  CIndirectTextToken  mITextToken;
  nsCParserNode       mITextNode;
  CTextToken          mTextToken;
  CToken              mErrorToken;
  nsCParserNode       mTextNode;
};

/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::CViewSourceHTML() : mTags(), mErrors() {
  NS_INIT_REFCNT();

  mStartTag.AssignWithConversion("start");
  mEndTag.AssignWithConversion("end");
  mCommentTag.AssignWithConversion("comment");
  mCDATATag.AssignWithConversion("cdata");
  mDocTypeTag.AssignWithConversion("doctype");
  mPITag.AssignWithConversion("pi");
  mEntityTag.AssignWithConversion("entity");
  mText.AssignWithConversion("txt");
  mKey.AssignWithConversion("key");
  mValue.AssignWithConversion("val");
  mSummaryTag.AssignWithConversion("summary");
  mPopupTag.AssignWithConversion("popup");

  mParser=0;
  mSink=0;
  mLineNumber=0;
  mTokenizer=0;
  mDocType=eHTML3Text;
  mValidator=0;
  mHasOpenRoot=PR_FALSE;

  //set this to 1 if you want to see errors in your HTML markup.
  char* theEnvString = PR_GetEnv("MOZ_VALIDATE_HTML"); 
  mShowErrors=PRBool(theEnvString);

#ifdef rickgdebug
  gDumpFile = new fstream("c:/temp/viewsource.xml",ios::trunc);
#endif

}



/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::~CViewSourceHTML(){
  mParser=0; //just to prove we destructed...

  NS_IF_RELEASE(mTokenizer);

}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& CViewSourceHTML::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult CViewSourceHTML::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewViewSourceHTML(aInstancePtrResult);
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
eAutoDetectResult CViewSourceHTML::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType) ||
     aParserContext.mMimeType.EqualsWithConversion(kTextCSSContentType)) {
    result=eValidDetect;
  }
  else if(eViewSource==aParserContext.mParserCommand) {
    if(aParserContext.mMimeType.EqualsWithConversion(kXMLTextContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kRDFTextContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kHTMLTextContentType) ||
       aParserContext.mMimeType.EqualsWithConversion(kXULTextContentType)) {
      result=ePrimaryDetect;
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
nsresult CViewSourceHTML::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){

  nsresult result=NS_OK;

#ifdef RAPTOR_PERF_METRICS
  vsTimer.Reset();
  NS_START_STOPWATCH(vsTimer);
#endif 

  STOP_TIMER();
  mSink=(nsIXMLContentSink*)aSink;

  if((!aParserContext.mPrevContext) && (mSink)) {

    mTags.Truncate();
    mErrors.AssignWithConversion(" HTML 4.0 Strict-DTD validation (enabled); [Should use Transitional?].\n");

    mValidator=aParserContext.mValidator;
    mDocType=aParserContext.mDocType;
    mMimeType=aParserContext.mMimeType;
    mDTDMode=aParserContext.mDTDMode;
    mParserCommand=aParserContext.mParserCommand;
    mErrorCount=0;
    mTagCount=0;

    static const char* theHeader="<?xml version=\"1.0\"?>";
    CToken ssToken(theHeader);
    nsCParserNode ssNode(&ssToken);
    result= mSink->AddCharacterData(ssNode);

  #ifdef rickgdebug
    (*gDumpFile) << theHeader << endl;
    (*gDumpFile) << "<viewsource xmlns=\"viewsource\">" << endl;
  #endif

  }


  if(eViewSource!=aParserContext.mParserCommand)
    mDocType=ePlainText;
  else mDocType=aParserContext.mDocType;

  mLineNumber=0;
  result = mSink->WillBuildModel(); 

  START_TIMER();
  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess5/18/98
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CViewSourceHTML::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer && aParser) {

    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;

    if(!mHasOpenRoot) {
      //now let's automatically open the root container...
      CToken theToken("viewsource");
      nsCParserNode theNode(&theToken,0);
     
      CAttributeToken *theAttr=nsnull;
      nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
      if(theAllocator) {
        theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_ConvertToString("http://www.mozilla.org/viewsource"));
        nsString& theKey=theAttr->GetKey();
        theKey=NS_ConvertToString("xmlns");
      }
      if(theAttr)
        theNode.AddAttribute(theAttr);
      result=mSink->OpenContainer(theNode);
      if(NS_SUCCEEDED(result)) mHasOpenRoot=PR_TRUE;
    }

    while(NS_SUCCEEDED(result)){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken,aParser);
        if(NS_SUCCEEDED(result)) {
          IF_FREE(theToken);
        }
        else if(NS_ERROR_HTMLPARSER_BLOCK!=result){
          mTokenizer->PushTokenFront(theToken);
        }
        // theRootDTD->Verify(kEmptyString,aParser);
      }
      else break;
    }//while
   
    mTokenizer=oldTokenizer;
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}


/**
 * Call this to display an error summary regarding the page.
 * 
 * @update	rickg 6June2000
 * @return  nsresult
 */
nsresult  CViewSourceHTML::GenerateSummary() {
  nsresult result=NS_OK;

  if(mErrorCount && mTagCount) {

    mErrors.AppendWithConversion("\n\n ");
    mErrors.AppendInt(mErrorCount);
    mErrors.AppendWithConversion(" error(s) detected -- see highlighted portions.\n");

    result=WriteTag(mSummaryTag,mErrors,0,PR_FALSE);

  }

  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){

    mParser=(nsParser*)aParser;  //debug XXX
    STOP_TIMER();

    mSink=(nsIXMLContentSink*)aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
        //now let's automatically auto-opened containers...

#ifdef rickgdebug
  if(gDumpFile){
    (*gDumpFile) << "</viewsource>" << endl;
    gDumpFile->close();
    delete gDumpFile;
  }
#endif

      if(ePlainText!=mDocType) {
        //now let's automatically close the root container...

        GenerateSummary();

        CToken theToken("viewsource");
        nsCParserNode theNode(&theToken,0);
        mSink->CloseContainer(theNode);
      }
      result = mSink->DidBuildModel(1);
    }

    START_TIMER();

  }

#ifdef RAPTOR_PERF_METRICS
  NS_STOP_STOPWATCH(vsTimer);
  printf("viewsource timer: ");
  vsTimer.Print();
  printf("\n");
#endif 

  return result;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsTokenAllocator* CViewSourceHTML::GetTokenAllocator(void){
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
 * the parser's terminate() method.
 *
 * @update	harishd 07/22/99
 * @param 
 * @return
 */
nsresult  CViewSourceHTML::Terminate(nsIParser* aParser) {
  return NS_ERROR_HTMLPARSER_STOPPARSING;
}


/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult CViewSourceHTML::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {

    result=NS_NewHTMLTokenizer(&mTokenizer,eDTDMode_quirks,mDocType,mParserCommand);
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
NS_IMETHODIMP CViewSourceHTML::WillResumeParse(void){
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
NS_IMETHODIMP CViewSourceHTML::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
PRBool CViewSourceHTML::Verify(nsString& aURLRef,nsIParser* aParser) {
  PRBool result=PR_TRUE;
  mParser=(nsParser*)aParser;
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void CViewSourceHTML::SetVerification(PRBool aEnabled){
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
PRBool CViewSourceHTML::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CViewSourceHTML::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CViewSourceHTML::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CViewSourceHTML::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool CViewSourceHTML::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}

PRBool CViewSourceHTML::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CViewSourceHTML::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}


/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult WriteText(const nsString& aTextString,nsIContentSink& aSink,PRBool aPreserveSpace,PRBool aPlainText,CSharedVSContext& aContext) {
  nsresult result=NS_OK;

  nsString&  temp=aContext.mTextToken.GetStringValueXXX();
  temp.Truncate();

  PRInt32   theEnd=aTextString.Length();
  PRInt32   theTextOffset=0;
  PRInt32   theOffset=-1; //aTextString.FindCharInSet("\t\n\r ",theStartOffset);
  PRInt32   theSpaces=0;
  PRUnichar theChar=0;
  PRUnichar theNextChar=0;

  while(++theOffset<theEnd){
    //theNextChar=aTextString[theOffset-1];
    theChar=aTextString[theOffset];
    switch(theChar){
      case kSpace:
        theNextChar=(theOffset+1 != theEnd) ? aTextString[theOffset+1] : '\0';
        if((!aPlainText) && aPreserveSpace) { //&& ((kSpace==theNextChar) || (0==theNextChar)))
          if(theTextOffset<theOffset) {
            if(kSpace==theNextChar) {
              aTextString.Mid(temp,theTextOffset,theOffset-theTextOffset);
              theTextOffset=theOffset--;
            }
          }
          else{
            while((theOffset<theEnd) && (kSpace==aTextString[theOffset])){
              theOffset++;
              theSpaces++;
            }
            theTextOffset=theOffset--;
          }
        }
        break;

      case kTab:
        if((!aPlainText) && aPreserveSpace){
          if(theTextOffset<theOffset) {
            aTextString.Mid(temp,theTextOffset,theOffset-theTextOffset);
          }
          theSpaces+=8;
          theOffset++;
          theTextOffset=theOffset;
        }
        break;

      case kCR:
      case kLF:
        if(theTextOffset<theOffset) {
          aTextString.Mid(temp,theTextOffset,theOffset-theTextOffset);
          theTextOffset=theOffset--;  //back up one on purpose...
        }
        else {
          theNextChar=(theOffset+1 != theEnd) ? aTextString.CharAt(theOffset+1) : '\0';
          if((kCR==theChar) && (kLF==theNextChar)) {
            theOffset++;
          }

          CStartToken theToken(eHTMLTag_br);
          aContext.mStartNode.Init(&theToken);
          result=aSink.AddLeaf(aContext.mStartNode); 
          theTextOffset=theOffset+1;
        }
        break;

      default:
        break;
    } //switch
    if(0<temp.Length()){
      result=aSink.AddLeaf(aContext.mTextNode);
      temp.Truncate();
    }
    if(0<theSpaces){

      CEntityToken theToken( NS_ConvertToString("nbsp") );
      aContext.mStartNode.Init(&theToken);
      int theIndex;
      for(theIndex=0;theIndex<theSpaces;theIndex++)
        result=aSink.AddLeaf(aContext.mStartNode); 
      theSpaces=0;
    }
  }

  if(theTextOffset<theEnd){
    temp.Truncate();
    aTextString.Mid(temp,theTextOffset,theEnd-theTextOffset);
    result=aSink.AddLeaf(aContext.mTextNode); //just dump the whole string...
  }

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteText(const nsString& aTextString,nsIContentSink& aSink,PRBool aPreserveSpace,PRBool aPlainText) {
  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();
  return ::WriteText(aTextString,aSink,aPreserveSpace,aPlainText,theContext);
}


/**
 *  This method gets called when a tag needs to write it's attributes
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteAttributes(PRInt32 attrCount) {
  nsresult result=NS_OK;
  
  if(attrCount){ //go collect the attributes...

    CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

    int attr=0;
    for(attr=0;attr<attrCount;attr++){
      CToken* theToken=mTokenizer->PeekToken();
      if(theToken)  {
        eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
        if(eToken_attribute==theType){
          mTokenizer->PopToken(); //pop it for real...
          theContext.mTokenNode.AddAttribute(theToken);  //and add it to the node.

          CAttributeToken* theAttrToken=(CAttributeToken*)theToken;
          nsString& theKey=theAttrToken->GetKey();
          theKey.StripChar(kCR);

          CToken theKeyToken(theKey);
          result=WriteTag(mKey,&theKeyToken,0,PR_FALSE);
          nsString& theValue=theToken->GetStringValueXXX();
          theValue.StripChar(kCR);

          if((0<theValue.Length()) || (theAttrToken->mHasEqualWithoutValue)){
            result=WriteTag(mValue,theToken,0,PR_FALSE);
          }
        } 
      }
      else return kEOF;
    }
  }

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteTag(nsString &theXMLTagName,nsString & aText,PRInt32 attrCount,PRBool aNewlineRequired) {
  static nsString       theString;

  nsresult result=NS_OK;

  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

  CToken theTagToken(theXMLTagName);
  theContext.mStartNode.Init(&theTagToken,mLineNumber);

  STOP_TIMER();

  mSink->OpenContainer(theContext.mStartNode);  //emit <starttag>...

#ifdef rickgdebug

      if(aNewlineRequired) {
        (*gDumpFile)<<endl;
      }

      nsCAutoString cstr(theXMLTagName);
      (*gDumpFile) << "<" << cstr << ">";
      cstr.Assign(aText);
      (*gDumpFile) << cstr;
#endif
  

  theContext.mITextToken.SetIndirectString(aText);  //now emit the tag name...

  mSink->AddLeaf(theContext.mITextNode);

  if(attrCount){
    result=WriteAttributes(attrCount);
  }

  theContext.mEndNode.Init(&theTagToken,mLineNumber);
  mSink->CloseContainer(theContext.mEndNode);  //emit </starttag>...

#ifdef rickgdebug
      cstr.Assign(theXMLTagName);
      (*gDumpFile) << "</" << cstr << ">";
#endif

  START_TIMER();

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 6June2000 -- factored to use WriteTag(txt,txt...);
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteTag(nsString &theXMLTagName,CToken* aToken,PRInt32 attrCount,PRBool aNewlineRequired) {

  nsString& theStr=aToken->GetStringValueXXX();
  theStr.StripChar(kCR);

  nsresult result=WriteTag(theXMLTagName,theStr,attrCount,aNewlineRequired);
  return result;
}

/**
 *  This method gets called when a tag needs to be sent out but is known to be misplaced (error)
 *  
 *  @update  gess 6June2000 -- 
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteTagWithError(nsString &theXMLTagName,CToken* aToken,PRInt32 attrCount,PRBool aNewlineRequired) {

  STOP_TIMER();

  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();
  nsresult result=NS_OK;
  
  if(ePlainText!=mDocType) {

    //first write the error tag itself...

    theContext.mErrorNode.Init(&theContext.mErrorToken,mLineNumber);
    result=mSink->OpenContainer(theContext.mErrorNode);  //emit <error>...
  }

    //now write the tag from the source file...
  result=WriteTag(theXMLTagName,aToken,attrCount,aNewlineRequired);

  if(ePlainText!=mDocType) {

   //now close the error tag...
    STOP_TIMER();
    theContext.mErrorNode.Init(&theContext.mErrorToken,mLineNumber);
    mSink->CloseContainer(theContext.mErrorNode);
    START_TIMER();
  }

  return result; 
}

void CViewSourceHTML::AddContainmentError(eHTMLTags aChildTag,eHTMLTags aParentTag,PRInt32 aLineNumber) {

  if (mShowErrors) {
    mErrorCount++;

    if(mErrorCount<=gErrorThreshold) {

      char theChildMsg[100];
      if(eHTMLTag_text==aChildTag) 
        strcpy(theChildMsg,"text");
      else sprintf(theChildMsg,"<%s>",nsHTMLTags::GetCStringValue(aChildTag));

      char theMsg[256];
      sprintf(theMsg,"\n -- Line (%i) error: %s is not a legal child of <%s>",
              aLineNumber,theChildMsg,nsHTMLTags::GetCStringValue(aParentTag));

      mErrors.AppendWithConversion(theMsg);
    }
    else if(gErrorThreshold+1==mErrorCount){
      mErrors.AppendWithConversion("\n -- Too many errors -- terminating output.");
    }
  }

}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP CViewSourceHTML::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
 
  mParser=(nsParser*)aParser;
  mSink=(nsIXMLContentSink*)aParser->GetContentSink();
 
  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();
  theContext.mTokenNode.Init(theToken,mLineNumber);

  eHTMLTags theParent=(mTags.Length()) ? (eHTMLTags)mTags.Last() : eHTMLTag_unknown;
  eHTMLTags theChild=(eHTMLTags)aToken->GetTypeID();

  switch(theType) {
    
    case eToken_start:
      {
        mTagCount++;

        if(mShowErrors) {
          PRBool theChildIsValid=PR_TRUE;
          if(mValidator) {
            theChildIsValid=mValidator->CanContain(theParent,theChild);
            if(theChildIsValid) {
              if(mValidator->IsContainer(theChild))
                mTags.Append(PRUnichar(theChild));
            }
          }

          if(theChildIsValid)
            result=WriteTag(mStartTag,aToken,aToken->GetAttributeCount(),PR_TRUE);
          else {
            AddContainmentError(theChild,theParent,mLineNumber);
            result=WriteTagWithError(mStartTag,aToken,aToken->GetAttributeCount(),PR_TRUE);
          }
        }
        else result=WriteTag(mStartTag,aToken,aToken->GetAttributeCount(),PR_TRUE);

        if((ePlainText!=mDocType) && mParser && (NS_OK==result)) {
          CObserverService* theService=mParser->GetObserverService();
          if(theService) {
            CParserContext*   pc=mParser->PeekContext(); 
            void*             theDocID=(pc)? pc->mKey:0; 
            eHTMLTags         theTag=(eHTMLTags)theToken->GetTypeID();  

            result=theService->Notify(theTag,theContext.mTokenNode,theDocID, NS_ConvertToString(kViewSourceCommand), mParser);
          }
        }
      }
      break;

    case eToken_end:
      if(theParent==theChild) {
        mTags.Truncate(mTags.Length()-1);
      }
      result=WriteTag(mEndTag,aToken,0,PR_TRUE);
      break;

    case eToken_cdatasection:
      {
        nsString& theStr=aToken->GetStringValueXXX();
        theStr.InsertWithConversion("<!",0);
        theStr.AppendWithConversion("]]>");
        result=WriteTag(mCDATATag,aToken,0,PR_TRUE);
      }
      break;

    case eToken_comment: 
      result=WriteTag(mCommentTag,aToken,0,PR_TRUE);
      break;

    case eToken_doctypeDecl:
      {
        result=WriteTag(mDocTypeTag,aToken,0,PR_TRUE);
      }
      break;

    case eToken_newline:
      mLineNumber++; //now fall through...
    case eToken_whitespace:
      result=WriteTag(mText,aToken,0,PR_FALSE);
      break;

    case eToken_text:

      if(mShowErrors) {
        if((0==mValidator) || mValidator->CanContain(theParent,eHTMLTag_text))
          result=WriteTag(mText,aToken,aToken->GetAttributeCount(),PR_TRUE);
        else {
          AddContainmentError(eHTMLTag_text,theParent,mLineNumber);
          result=WriteTagWithError(mText,aToken,aToken->GetAttributeCount(),PR_FALSE);
        }
      }
      else {
        // Fix bug 40809
        nsString& theStr=aToken->GetStringValueXXX();
        theStr.ReplaceSubstring(NS_ConvertASCIItoUCS2("\r\n"), NS_ConvertASCIItoUCS2("\n"));
        theStr.ReplaceChar(kCR,kLF);  
        result=WriteTag(mText,theStr,aToken->GetAttributeCount(),PR_TRUE);
      }

      break;

    case eToken_entity:
      {
        nsAutoString theStr;
        nsString& theEntity=aToken->GetStringValueXXX();
        if(!theEntity.EqualsWithConversion("XI",IGNORE_CASE)) {
          PRUnichar theChar=theEntity.CharAt(0);
          if((nsCRT::IsAsciiDigit(theChar)) || ('X'==theChar) || ('x'==theChar)){
            theStr.AppendWithConversion("#");
          }
          theStr.Append(theEntity);
        }
        result=WriteTag(mEntityTag,theStr,0,PR_FALSE);
      }
      break;

    case eToken_instruction:
      result=WriteTag(mPITag,aToken,0,PR_TRUE);

    default:
      result=NS_OK;
  }//switch

  theContext.mTokenNode.ReleaseAll(); 

  return result;
}

