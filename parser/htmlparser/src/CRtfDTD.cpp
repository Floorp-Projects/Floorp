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
 * @update  gess 4/8/98
 * 
 *         
 */

/**
 * TRANSIENT STYLE-HANDLING NOTES:
 * @update  gess 6/15/98
 * 
 * ...add comments here about transient style stack.
 *         
   */

#include "nsIDTDDebug.h"
#include "CRtfDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsTokenHandler.h"
#include "nsITokenizer.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_RTF_DTD_IID); 


static const char* kRTFTextContentType = "application/rtf";
static const char* kRTFDocHeader= "{\\rtf0";


struct RTFEntry {
  char      mName[10];
  eRTFTags  mTagID;
};


static RTFEntry gRTFTable[] = {

  {"$",eRTFCtrl_unknown},
  {"'",eRTFCtrl_quote},
  {"*",eRTFCtrl_star},   
  {"0x0a",eRTFCtrl_linefeed},
  {"0x0d",eRTFCtrl_return},
  {"\\",eRTFCtrl_begincontrol},
  {"b",eRTFCtrl_bold}, 
  {"bin",eRTFCtrl_bin},
  {"blue",eRTFCtrl_blue},
  {"cols",eRTFCtrl_cols},
  {"comment",eRTFCtrl_comment},
  {"f",eRTFCtrl_font},
  {"fonttbl",eRTFCtrl_fonttable},
  {"green",eRTFCtrl_green},
  {"i",eRTFCtrl_italic},
  {"margb",eRTFCtrl_bottommargin},
  {"margl",eRTFCtrl_leftmargin},
  {"margr",eRTFCtrl_rightmargin},
  {"margt",eRTFCtrl_topmargin},
  {"par",eRTFCtrl_par},
  {"pard",eRTFCtrl_pard},
  {"plain",eRTFCtrl_plain},
  {"qc",eRTFCtrl_justified},
  {"qj",eRTFCtrl_fulljustified},
  {"ql",eRTFCtrl_leftjustified},
  {"qr",eRTFCtrl_rightjustified},
  {"rdblquote",eRTFCtrl_rdoublequote},
  {"red",eRTFCtrl_red},
  {"rtf",eRTFCtrl_rtf},
  {"tab",eRTFCtrl_tab},
  {"title",eRTFCtrl_title},
  {"u",eRTFCtrl_underline},
  {"{",eRTFCtrl_startgroup},
  {"}",eRTFCtrl_endgroup},
  {"~",eRTFCtrl_last} //make sure this stays the last token...
};

/**
 * 
 * @update	gess4/25/98
 * @param 
 * @return
 */
static
const char* GetTagName(eRTFTags aTag) {
  PRInt32  cnt=sizeof(gRTFTable)/sizeof(RTFEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  while(low<=high) {
    middle=(PRInt32)(low+high)/2;
    if(aTag==gRTFTable[middle].mTagID)   
      return gRTFTable[middle].mName;
    if(aTag<gRTFTable[middle].mTagID)
      high=middle-1; 
    else low=middle+1; 
  }
  return "";
}

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
nsresult CRtfDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CRtfDTD*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewRTF_DTD(nsIDTD** aInstancePtrResult)
{
  CRtfDTD* it = new CRtfDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CRtfDTD)
NS_IMPL_RELEASE(CRtfDTD)


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CRtfDTD::CRtfDTD() : nsIDTD() {
  NS_INIT_REFCNT();
  mParser=0;
  mFilename=0;
  mTokenizer=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CRtfDTD::~CRtfDTD(){
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& CRtfDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct fresh instance
 * of itself.
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult CRtfDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewRTF_DTD(aInstancePtrResult);
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
eAutoDetectResult CRtfDTD::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;
  if(aContentType.Equals(kRTFTextContentType)) {
    result=ePrimaryDetect;
  }
  else if(kNotFound!=aBuffer.Find(kRTFDocHeader)) {
    result=ePrimaryDetect;
  }
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CRtfDTD::WillBuildModel(nsString& aFileName,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink){
  nsresult result=NS_OK;
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
NS_IMETHODIMP CRtfDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CRtfDTD::DidBuildModel(nsresult anErrorCode,PRInt32 aLevel,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  return result;
}

/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsITokenizer* CRtfDTD::GetTokenizer(void){
  return 0;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
nsresult CRtfDTD::WillResumeParse(void){
  return NS_OK;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
nsresult CRtfDTD::WillInterruptParse(void){
  return NS_OK;
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
PRBool CRtfDTD::Verify(nsString& aURLRef,nsIParser* aParser){
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
void CRtfDTD::SetVerification(PRBool aEnabled){
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CRtfDTD::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_FALSE;
  return result;
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
PRBool CRtfDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleGroup(CToken* aToken){
  nsresult result=NS_OK;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleControlWord(CToken* aToken){
  nsresult result=NS_OK;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleContent(CToken* aToken){
  nsresult result=NS_OK;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult result=NS_OK;

  mParser=(nsParser*)aParser;
  if(aToken) {
    eRTFTokenTypes theType=eRTFTokenTypes(aToken->GetTokenType());
    
    switch(theType) {
      case eRTFToken_group:
        result=HandleGroup(aToken); break;

      case eRTFToken_controlword:
        result=HandleControlWord(aToken); break;

      case eRTFToken_content:
        result=HandleContent(aToken); break;

      default:
        break;
      //everything else is just text or attributes of controls...
    }

  }//if

  return result;
}


/**
 *  This method causes all tokens to be dispatched to the given tag handler.
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object to receive subsequent tokens...
 *  @return	 error code (usually 0)
 */
nsresult CRtfDTD::CaptureTokenPump(nsITagHandler* aHandler) {
  nsresult result=NS_OK;
  return result;
}

/**
 *  This method releases the token-pump capture obtained in CaptureTokenPump()
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object that received tokens...
 *  @return	 error code (usually 0)
 */
nsresult CRtfDTD::ReleaseTokenPump(nsITagHandler* aHandler){
  nsresult result=NS_OK;
  return result;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsITokenRecycler* CRtfDTD::GetTokenRecycler(void){
  return 0;
}

/***************************************************************
  Heres's the RTFControlWord subclass...
 ***************************************************************/

CRTFControlWord::CRTFControlWord(char* aKey) : CToken(aKey), mArgument("") {
}


PRInt32 CRTFControlWord::GetTokenType() {
  return eRTFToken_controlword;
}


PRInt32 CRTFControlWord::Consume(nsScanner& aScanner){
  static nsString     gAlphaChars("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  static nsAutoString gDigits("-0123456789");

  PRInt32 result=aScanner.ReadWhile(mTextValue,gAlphaChars,PR_TRUE,PR_FALSE);
  if(NS_OK==result) {
    //ok, now look for an option parameter...
    PRUnichar ch;
    result=aScanner.Peek(ch);

    switch(ch) {
      case '0': case '1': case '2': case '3': case '4': 
      case '5': case '6': case '7': case '8': case '9': 
      case kMinus:
        result=aScanner.ReadWhile(mArgument,gDigits,PR_TRUE,PR_FALSE);
        break;

      case kSpace:
      default:
        break;
    }
  }
  if(NS_OK==result)
    result=aScanner.SkipWhitespace();
  return result;
}


/***************************************************************
  Heres's the RTFGroup subclass...
 ***************************************************************/

CRTFGroup::CRTFGroup(char* aKey,PRBool aStartGroup) : CToken(aKey) {
  mStart=aStartGroup;
}
 

PRInt32 CRTFGroup::GetTokenType() {
  return eRTFToken_group;
}

void CRTFGroup::SetGroupStart(PRBool aFlag){
  mStart=aFlag;
}

PRBool CRTFGroup::IsGroupStart(){
  return mStart;
}

PRInt32 CRTFGroup::Consume(nsScanner& aScanner){
  PRInt32 result=NS_OK;
  if(PR_FALSE==mStart)
    result=aScanner.SkipWhitespace();
  return result;
}


/***************************************************************
  Heres's the RTFContent subclass...
 ***************************************************************/

CRTFContent::CRTFContent(PRUnichar* aKey) : CToken(aKey) {
}


PRInt32 CRTFContent::GetTokenType() {
  return eRTFToken_content;
}



/**
 * We're supposed to read text until we encounter one
 * of the RTF control characters: \.{,}.
 * @update	gess7/9/98
 * @param 
 * @return
 */
static nsString textTerminators("\\{}");
PRInt32 CRTFContent::Consume(nsScanner& aScanner){
  PRInt32 result=aScanner.ReadUntil(mTextValue,textTerminators,PR_FALSE,PR_FALSE);
  return result;
}



