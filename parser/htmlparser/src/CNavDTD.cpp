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

#include "nsIParserDebug.h"
#include "CNavDTD.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsHTMLContentSink.h" 
#include "nsScanner.h"
#include "nsParserTypes.h"

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
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 

static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";

static nsAutoString gEmpty;

static char formElementTags[]= {  
    eHTMLTag_button,  eHTMLTag_fieldset,  eHTMLTag_input,
    eHTMLTag_isindex, eHTMLTag_label,     eHTMLTag_legend,
    eHTMLTag_option,  eHTMLTag_select,    eHTMLTag_textarea,0};

static char gHeadingTags[]={
  eHTMLTag_h1,  eHTMLTag_h2,  eHTMLTag_h3,  
  eHTMLTag_h4,  eHTMLTag_h5,  eHTMLTag_h6, 
  0};

static char  gStyleTags[]={
  eHTMLTag_a,       eHTMLTag_bold,    eHTMLTag_big,
  eHTMLTag_blink,   eHTMLTag_cite,    eHTMLTag_em, 
  eHTMLTag_font,    eHTMLTag_italic,  eHTMLTag_kbd,     
  eHTMLTag_small,   eHTMLTag_spell,   eHTMLTag_strike,  
  eHTMLTag_strong,  eHTMLTag_sub,     eHTMLTag_sup,     
  eHTMLTag_tt,      eHTMLTag_u,       eHTMLTag_var,     
  0};
  
static char  gWhitespaceTags[]={
  eHTMLTag_newline, eHTMLTag_whitespace,
  0};


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
nsresult CNavDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CNavDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
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
NS_HTMLPARS nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult)
{
  CNavDTD* it = new CNavDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CNavDTD)
NS_IMPL_RELEASE(CNavDTD)



/**
 *  
 *  
 *  @update  gess 6/9/98
 *  @param   
 *  @return  
 */
PRInt32 NavDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  PRInt32         result=0;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
  CNavDTD*        theDTD=(CNavDTD*)aDTD;
  
  if(aDTD) {
    switch(theType) {
      case eToken_start:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eToken_comment:
        result=theDTD->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=theDTD->HandleEntityToken(aToken); break;
      case eToken_whitespace:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_newline:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_text:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      case eToken_style:
        result=theDTD->HandleStyleToken(aToken); break;
      case eToken_skippedcontent:
        result=theDTD->HandleSkippedContentToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CNavDTD::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_start));

  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_end));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_comment));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_entity));

  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_whitespace));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_newline));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_text));
  
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_attribute));
//  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_script));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_style));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_skippedcontent));
}


class CNavTokenDeallocator: public nsDequeFunctor{
public:
  virtual void operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
  }
};

static CNavTokenDeallocator gTokenKiller;

/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD(), mTokenDeque(gTokenKiller)  {
  NS_INIT_REFCNT();
  mParser=0;
  mFilename=0;
  mParserDebug=0;
  nsCRT::zero(mLeafBits,sizeof(mLeafBits));
  nsCRT::zero(mContextStack,sizeof(mContextStack));
  nsCRT::zero(mStyleStack,sizeof(mStyleStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  mContextStackPos=0;
  mStyleStackPos=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  InitializeDefaultTokenHandlers();
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::~CNavDTD(){
  DeleteTokenHandlers();
  if (mFilename)
    PL_strfree(mFilename);

  if (mParserDebug)
    NS_RELEASE(mParserDebug);
//  NS_RELEASE(mSink);
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CNavDTD::WillBuildModel(const char* aFilename, nsIParserDebug* aParserDebug){
  PRInt32 result=0;

  if (mFilename) {
    PL_strfree(mFilename);
    mFilename=0;
  }
  if(aFilename) {
    mFilename = PL_strdup(aFilename);
  }

  mParserDebug = aParserDebug;
  NS_IF_ADDREF(mParserDebug);

  if(mSink)
    mSink->WillBuildModel();

  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CNavDTD::DidBuildModel(PRInt32 anErrorCode){
  PRInt32 result=0;

  if((kNoError==anErrorCode) && (mContextStackPos>0)) {
    CloseContainersTo(0,eHTMLTag_unknown,PR_FALSE);
  }
  if(mSink) {
    mSink->DidBuildModel(1);
  }

  return result;
}


/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 5/21/98
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
PRInt32 CNavDTD::HandleToken(CToken* aToken){
  PRInt32 result=0;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    CTokenHandler*  aHandler=GetTokenHandler(theType);

    if(aHandler) {
      result=(*aHandler)(theToken,this);
      if (mParserDebug)
         mParserDebug->Verify(this, mParser, mContextStackPos, mContextStack, mFilename);
    }

  }//if
  return result;
}



/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsCParserNode& aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  eHTMLTags parentTag=(eHTMLTags)GetTopNode();
  PRInt32   result=kNoError;
  PRBool    contains=CanContain(parentTag,aChildTag);

  if(PR_FALSE==contains){
    result=CreateContextStackFor(aChildTag);
    if(kNoError!=result) {
      //if you're here, then the new topmost container can't contain aToken.
      //You must determine what container hierarchy you need to hold aToken,
      //and create that on the parsestack.
      result=ReduceContextStackFor(aChildTag);
      if(PR_FALSE==CanContain(GetTopNode(),aChildTag)) {
        //we unwound too far; now we have to recreate a valid context stack.
        result=CreateContextStackFor(aChildTag);
      }
    }
  }

  if(IsContainer(aChildTag)){
    if(PR_TRUE==mLeafBits[mContextStackPos-1]) {
      CloseTransientStyles(aChildTag);
    }
    result=OpenContainer(aNode,PR_TRUE);
  }
  else {
    if(PR_FALSE==mLeafBits[mContextStackPos-1]) {
      OpenTransientStyles(aChildTag);
    }
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st= (CStartToken*)(aToken);
  eHTMLTags     tokenTagType=(eHTMLTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode attrNode((CHTMLToken*)aToken);
  PRInt16       attrCount=aToken->GetAttributeCount();
  PRInt32       theCount;
  PRInt32       result=(0==attrCount) ? kNoError : mParser->CollectAttributes(attrNode,attrCount);

  if(kNoError==result) {
      //now check to see if this token should be omitted...
    if(PR_FALSE==CanOmit(GetTopNode(),tokenTagType)) {
  
      switch(tokenTagType) {

        case eHTMLTag_html:
          result=OpenHTML(attrNode); break;

        case eHTMLTag_title:
          {
            nsCParserNode theNode(st);
            result=OpenHead(theNode); //open the head...
            if(kNoError==result) {
              result=mParser->CollectSkippedContent(attrNode,theCount);
              mSink->SetTitle(attrNode.GetSkippedContent());
              result=CloseHead(theNode); //close the head...
            }
          }
          break;

        case eHTMLTag_textarea:
          {
            mParser->CollectSkippedContent(attrNode,theCount);
            result=AddLeaf(attrNode);
          }
          break;

        case eHTMLTag_form:
          result = OpenForm(attrNode);
          break;

        case eHTMLTag_meta:
        case eHTMLTag_link:
          {
            nsCParserNode theNode((CHTMLToken*)aToken);
            result=OpenHead(theNode);
            if(kNoError==result)
              result=AddLeaf(theNode);
            if(kNoError==result)
              result=CloseHead(theNode);
          }
          break;

        case eHTMLTag_style:
          {
            nsCParserNode theNode((CHTMLToken*)aToken);
            result=OpenHead(theNode);
            if(kNoError==result) {
              mParser->CollectSkippedContent(attrNode,theCount);
              if(kNoError==result) {
                result=AddLeaf(attrNode);
                if(kNoError==result)
                  result=CloseHead(theNode);
              }
            }
          }
          break;

        case eHTMLTag_script:
          result=HandleScriptToken(st); break;
      

        case eHTMLTag_head:
          break; //ignore head tags...

        case eHTMLTag_base:
          result=OpenHead(attrNode);
          if(kNoError==result) {
            result=AddLeaf(attrNode);
            if(kNoError==result)
              result=CloseHead(attrNode);
          }
          break;

        case eHTMLTag_nobr:
          result=PR_TRUE;

        case eHTMLTag_map:
          result=PR_TRUE;

        default:
          result=HandleDefaultStartToken(aToken,tokenTagType,attrNode);
          break;
      } //switch
    } //if
  } //if
  return result;
}


/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRInt32     result=kNoError;
  CEndToken*  et = (CEndToken*)(aToken);
  eHTMLTags   tokenTagType=(eHTMLTags)et->GetTypeID();


  // Here's the hacky part:
  // Because we're trying to be backward compatible with Nav4/5, 
  // we have to handle explicit styles the way it does. That means
  // that we keep an internal style stack.When an EndToken occurs, 
  // we should see if it is an explicit style tag. If so, we can 
  // close AND explicit style tag (goofy, huh?)

/*
  if(0!=strchr(gStyleTags,tokenTagType)){
    eHTMLTags topTag=(eHTMLTags)GetTopNode();
    if(0!=strchr(gStyleTags,topTag)){
      tokenTagType=topTag;
    }
  }
*/

    //now check to see if this token should be omitted...
  if(PR_TRUE==CanOmitEndTag(GetTopNode(),tokenTagType)) {
    UpdateStyleStackForCloseTag(tokenTagType,tokenTagType);
    return result;
  }

  nsCParserNode theNode((CHTMLToken*)aToken);
  switch(tokenTagType) {

    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
    case eHTMLTag_head:
    case eHTMLTag_script:
      break;

    case eHTMLTag_map:
      result=CloseContainer(theNode,tokenTagType,PR_TRUE);
      break;

    case eHTMLTag_form:
      {
        nsCParserNode aNode((CHTMLToken*)aToken);
        result=CloseForm(aNode);
      }
      break;

    default:
      if(IsContainer(tokenTagType)){
        result=CloseContainersTo(tokenTagType,PR_TRUE); 
      }
      //
      break;
  }
  return result;
}

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CEntityToken* et = (CEntityToken*)(aToken);
  PRInt32       result=kNoError;
  eHTMLTags     tokenTagType=(eHTMLTags)et->GetTypeID();

  if(PR_FALSE==CanOmit(GetTopNode(),tokenTagType)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  return kNoError;
}

/**
 *  This method gets called when a skippedcontent token has 
 *  been encountered in the parse process. After verifying 
 *  that the topmost container can contain text, we call 
 *  AddLeaf to store this token in the top container.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleSkippedContentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRInt32 result=kNoError;

  if(HasOpenContainer(eHTMLTag_body)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  CAttributeToken*  at = (CAttributeToken*)(aToken);
  PRInt32 result=kNoError;
  return result;
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleScriptToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsCParserNode theNode((CHTMLToken*)aToken);
  PRInt32       theCount=0;
  PRInt32       result=mParser->CollectSkippedContent(theNode,theCount);

  return result;
}

/**
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 CNavDTD::HandleStyleToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStyleToken*  st = (CStyleToken*)(aToken);
  PRInt32 result=kNoError;
  return result;
}


/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
void CNavDTD::DeleteTokenHandlers(void) {
  int i=0;
  for(i=eToken_unknown;i<eToken_last;i++){
    delete mTokenHandlers[i];
    mTokenHandlers[i]=0;
  }
  return;
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gess 4/2/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* CNavDTD::GetTokenHandler(eHTMLTokenTypes aType) const {
  CTokenHandler* result=0;
  if((aType>0) && (aType<eToken_last)) {
    result=mTokenHandlers[aType];
  } 
  else {
  }
  return result;
}


/**
 *  Register a handler.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler* CNavDTD::AddTokenHandler(CTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler");
  
  if(aHandler)  {
    eHTMLTokenTypes type=(eHTMLTokenTypes)aHandler->GetTokenType();
    if(type<eToken_last) {
      CTokenHandler* old=mTokenHandlers[type];
      mTokenHandlers[type]=aHandler;
    }
    else {
      //add code here to handle dynamic tokens...
    }
  }
  return 0;
}

/**
 * 
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return 
 */
void CNavDTD::SetParser(nsIParser* aParser) {
  mParser=(nsParser*)aParser;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* CNavDTD::SetContentSink(nsIContentSink* aSink) {
  nsIContentSink* old=mSink;
  mSink=(nsIHTMLContentSink*)aSink;
  return old;
}


/**
 *  This method is called to determine whether or not a tag
 *  can contain an explict style tag (font, italic, bold, etc.)
 *  Most can -- but some, like option, cannot. Therefore we
 *  don't bother to open transient styles within these elements.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContainStyles(eHTMLTags aParent) const {
  PRBool result=PR_TRUE;
  switch(aParent) {
    case eHTMLTag_option:
      result=PR_FALSE; break;
    default:
      break;
  }
  return result;
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContainFormElement(eHTMLTags aParent,eHTMLTags aChild) const {
  PRBool result=(mParser) ? HasOpenContainer(eHTMLTag_form) : PR_FALSE;
  return result;
}


/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContain(PRInt32 aParent,PRInt32 aChild) {

  PRBool result=PR_FALSE;

    //tagset1 has 65 members...
  static char  gTagSet1[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_address,   eHTMLTag_applet,
    eHTMLTag_bold,      eHTMLTag_basefont,  eHTMLTag_bdo,       eHTMLTag_big,
    eHTMLTag_blockquote,eHTMLTag_br,        eHTMLTag_button,    eHTMLTag_center,
    eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,       eHTMLTag_dir,
    eHTMLTag_div,       eHTMLTag_dl,        eHTMLTag_em,        eHTMLTag_fieldset,
    eHTMLTag_font,      eHTMLTag_form,      eHTMLTag_h1,        eHTMLTag_h2,
    eHTMLTag_h3,        eHTMLTag_h4,        eHTMLTag_h5,        eHTMLTag_h6,
    eHTMLTag_hr,        eHTMLTag_italic,    eHTMLTag_iframe,    eHTMLTag_img,
    eHTMLTag_input,     eHTMLTag_isindex,   
    
    eHTMLTag_kbd,       eHTMLTag_label,     eHTMLTag_listitem,
    eHTMLTag_map,       eHTMLTag_menu,      eHTMLTag_newline,   eHTMLTag_nobr,
    eHTMLTag_noframes,  eHTMLTag_noscript,
    eHTMLTag_object,    eHTMLTag_ol,        eHTMLTag_paragraph, eHTMLTag_pre,
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_script,
    eHTMLTag_select,    eHTMLTag_small,     eHTMLTag_span,      eHTMLTag_strong,
    eHTMLTag_sub,       eHTMLTag_sup,       eHTMLTag_table,     eHTMLTag_text,
    
    eHTMLTag_textarea,  eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_ul,        
    eHTMLTag_userdefined,   eHTMLTag_var,   
    eHTMLTag_whitespace,  //JUST ADDED!
    0};

    //tagset2 has 44 members...
  static char  gTagSet2[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
    eHTMLTag_basefont,  eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,
    eHTMLTag_button,    eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,
    eHTMLTag_div,       eHTMLTag_em,        eHTMLTag_font,      eHTMLTag_hr,        
    eHTMLTag_italic,    eHTMLTag_iframe,    eHTMLTag_img,       eHTMLTag_input,     
    eHTMLTag_kbd,       

    eHTMLTag_label,     eHTMLTag_map,       eHTMLTag_newline,   eHTMLTag_nobr,
    eHTMLTag_object,    eHTMLTag_paragraph, 
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_script,    
    eHTMLTag_select,    eHTMLTag_small,     eHTMLTag_span,      eHTMLTag_strong,    
    eHTMLTag_sub,       eHTMLTag_sup,       eHTMLTag_text,      eHTMLTag_textarea,  
    eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_userdefined, eHTMLTag_var,       
    eHTMLTag_whitespace,//JUST ADDED!
    0};

    //tagset3 has 57 members...
  static char  gTagSet3[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
    eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,        eHTMLTag_blockquote,
    eHTMLTag_body,      eHTMLTag_caption,   eHTMLTag_center,    eHTMLTag_cite,
    eHTMLTag_code,      eHTMLTag_dd,        eHTMLTag_del,       eHTMLTag_dfn,        
    eHTMLTag_div,       eHTMLTag_dt,        eHTMLTag_em,        eHTMLTag_fieldset,    
    eHTMLTag_font,      eHTMLTag_form,      eHTMLTag_h1,        eHTMLTag_h2,
    eHTMLTag_h3,        eHTMLTag_h4,        eHTMLTag_h5,        eHTMLTag_h6,
    eHTMLTag_italic,    eHTMLTag_iframe,    eHTMLTag_ins,       eHTMLTag_kbd,       

    eHTMLTag_label,     eHTMLTag_legend,    
    eHTMLTag_listitem,  eHTMLTag_newline,   //JUST ADDED!
        
    eHTMLTag_noframes,
    eHTMLTag_noscript,  eHTMLTag_object,    eHTMLTag_paragraph, eHTMLTag_pre,
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_small,
    eHTMLTag_span,      eHTMLTag_strong,    eHTMLTag_sub,       eHTMLTag_sup,   
    eHTMLTag_td,        eHTMLTag_text,
    
    eHTMLTag_th,        eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_userdefined,
    eHTMLTag_var,       eHTMLTag_whitespace,  //JUST ADDED!
    0};

    //This hack code is here because we don't yet know what to do
    //with userdefined tags...  XXX Hack
  if(eHTMLTag_userdefined==aChild)  // XXX Hack: For now...
    result=PR_TRUE;

    //handle form elements (this is very much a WIP!!!)
  if(0!=strchr(formElementTags,aChild)){
    return CanContainFormElement((eHTMLTags)aParent,(eHTMLTags)aChild);
  }

  
  switch((eHTMLTags)aParent) {
    case eHTMLTag_a:
    case eHTMLTag_acronym:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_address:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_applet:
      {
        static char okTags[]={  
          eHTMLTag_a,       eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
          eHTMLTag_basefont,eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,
          eHTMLTag_button,  eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,
          eHTMLTag_em,      eHTMLTag_font,      eHTMLTag_italic,    eHTMLTag_iframe,
          eHTMLTag_img,     eHTMLTag_input,     eHTMLTag_kbd,       eHTMLTag_label,
          eHTMLTag_map,     eHTMLTag_object,    eHTMLTag_param,     eHTMLTag_quotation,
          eHTMLTag_samp,    eHTMLTag_script,    eHTMLTag_select,    eHTMLTag_small,
          eHTMLTag_span,    eHTMLTag_strike,    eHTMLTag_strong,    eHTMLTag_sub,
          eHTMLTag_sup,     eHTMLTag_textarea,  eHTMLTag_tt,        eHTMLTag_u,
          eHTMLTag_var,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }


    case eHTMLTag_area:
    case eHTMLTag_base:
    case eHTMLTag_basefont:
    case eHTMLTag_br:
      break;  //singletons can't contain other tags

    case eHTMLTag_bdo:
    case eHTMLTag_big:
    case eHTMLTag_blink:
    case eHTMLTag_bold:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_blockquote:
    case eHTMLTag_body:
      if(eHTMLTag_userdefined==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_button:
      result=PRBool(0!=strchr(gTagSet3,aChild)); break;

    case eHTMLTag_caption:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_center:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_cite:   case eHTMLTag_code:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_col:
    case eHTMLTag_colgroup:
      break;    //singletons can't contain anything...

    case eHTMLTag_dd:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_del:  case eHTMLTag_dfn:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_div:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_dl:
      {
        char okTags[]={eHTMLTag_dd,eHTMLTag_dt,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_dt:
      break;    //singletons can't contain anything...

    case eHTMLTag_em:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_fieldset:
      if(eHTMLTag_legend==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_font:
    case eHTMLTag_form:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_frame:
      break;  //singletons can't contain other tags

    case eHTMLTag_frameset:
      {
        static char okTags[]={eHTMLTag_frame,eHTMLTag_frameset,eHTMLTag_noframes,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }

    case eHTMLTag_h1: case eHTMLTag_h2:
    case eHTMLTag_h3: case eHTMLTag_h4:
    case eHTMLTag_h5: case eHTMLTag_h6:
      {
        if(0!=strchr(gHeadingTags,aChild))
          result=PR_FALSE;
        else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      }
      break;

    case eHTMLTag_head:
      {
        static char  okTags[]={
          eHTMLTag_base,  eHTMLTag_isindex, eHTMLTag_link,  eHTMLTag_meta,
          eHTMLTag_script,eHTMLTag_style,   eHTMLTag_title, 0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_hr:      
      break;    //singletons can't contain anything...

    case eHTMLTag_html:
      {
        static char  okTags[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_head,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_iframe:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_img:
    case eHTMLTag_input:
    case eHTMLTag_isindex:
    case eHTMLTag_spacer:
    case eHTMLTag_wbr:
      break;    //singletons can't contain anything...

    case eHTMLTag_italic:
    case eHTMLTag_ins:
    case eHTMLTag_kbd:
    case eHTMLTag_label:
    case eHTMLTag_legend:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_layer:
      break;

    case eHTMLTag_link:
      break;    //singletons can't contain anything...

    case eHTMLTag_listitem:
      if (eHTMLTag_listitem == aChild) {
        return PR_FALSE;
      }
      result=PRBool(!strchr(gHeadingTags,aChild)); break;

    case eHTMLTag_listing:
      result = PR_TRUE; break;

    case eHTMLTag_map:
      result=PRBool(eHTMLTag_area==aChild); break;

    case eHTMLTag_marquee:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_math:
      break; //nothing but plain text...

    case eHTMLTag_meta:
      break;  //singletons can't contain other tags

    case eHTMLTag_menu:
    case eHTMLTag_dir:
    case eHTMLTag_ol:
    case eHTMLTag_ul:
      // XXX kipp was here
      result=PRBool(!strchr(gHeadingTags,aChild)); break;

    case eHTMLTag_nobr:
      result=PR_TRUE; break;

    case eHTMLTag_noframes:
      if(eHTMLTag_body==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_noscript:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_note:

    case eHTMLTag_object:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_option:
      //for now, allow an option to contain anything but another option...
      result=PRBool(eHTMLTag_option!=aChild); break;

    case eHTMLTag_paragraph:
      if(eHTMLTag_paragraph==aChild)
        result=PR_FALSE;
      else result=PRBool(0!=strchr(gTagSet2,aChild)); 
      break;

    case eHTMLTag_param:
      break;  //singletons can't contain other tags

    case eHTMLTag_plaintext:
      break;

    case eHTMLTag_pre:
    case eHTMLTag_quotation:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_script:
      break; //unadorned script text...

    case eHTMLTag_select:
      result=PR_TRUE; break; //for now, allow select to contain anything...

    case eHTMLTag_small:
    case eHTMLTag_span:
    case eHTMLTag_spell:
    case eHTMLTag_strike:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_style:
      break;  //singletons can't contain other tags

    case eHTMLTag_table:
      {
        static char  okTags[]={ 
          eHTMLTag_caption, eHTMLTag_col, eHTMLTag_colgroup,eHTMLTag_tbody,   
          eHTMLTag_tfoot,  /* eHTMLTag_tr,*/  eHTMLTag_thead,   0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_thead:
      result=PRBool(eHTMLTag_tr==aChild); break;

    case eHTMLTag_th:
      result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_td:
      {
        static char  extraTags[]={eHTMLTag_newline,0};
        result=PRBool(0!=strchr(extraTags,aChild));
        if(PR_FALSE==result)
          result=PRBool(0!=strchr(gTagSet1,aChild)); 
      }
      break;

    case eHTMLTag_textarea:
    case eHTMLTag_title:
      break; //nothing but plain text...

    case eHTMLTag_tr:
      {
        static char  okTags[]={eHTMLTag_td,eHTMLTag_th,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_tt:
    case eHTMLTag_u:
    case eHTMLTag_var:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_userdefined:
      result=PR_TRUE; break; //XXX for now...

    case eHTMLTag_xmp: 
    default:
      break;
  } //switch
  return result;
}


/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContainIndirect(eHTMLTags aParent,eHTMLTags aChild) const {
  PRBool result=PR_FALSE;

  switch(aParent) {

    case eHTMLTag_html:
      {
        static char  okTags[]={
          eHTMLTag_head,    eHTMLTag_body,
          eHTMLTag_header,  eHTMLTag_footer,0
        };
        result=PRBool(0!=strchr(okTags,aChild));
      }

    case eHTMLTag_body:
      result=PR_TRUE; break;

    case eHTMLTag_table:
      {
        static char  okTags[]={
          eHTMLTag_caption, eHTMLTag_colgroup,
          eHTMLTag_tbody,   eHTMLTag_tfoot,
          eHTMLTag_thead,   eHTMLTag_tr,
          eHTMLTag_td,      eHTMLTag_th,
          eHTMLTag_col,     0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

      result=PR_TRUE; break;

    default:
      break;
  }
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild) const {
  PRBool result=PR_FALSE;

  //begin with some simple (and obvious) cases...
  switch(aChild) {

    case eHTMLTag_userdefined:
    case eHTMLTag_comment:
      result=PR_TRUE; 
      break;

    case eHTMLTag_html:
    case eHTMLTag_body:
      result=HasOpenContainer(aChild); //don't bother if they're already open...
      break;

    case eHTMLTag_button:       case eHTMLTag_fieldset:
    case eHTMLTag_input:        case eHTMLTag_isindex:
    case eHTMLTag_label:        case eHTMLTag_legend:
    case eHTMLTag_select:       case eHTMLTag_textarea:
      if(PR_FALSE==HasOpenContainer(eHTMLTag_form))
        result=PR_TRUE; 
      break;

    case eHTMLTag_newline:    
    case eHTMLTag_whitespace:

      switch(aParent) {
        case eHTMLTag_html:     case eHTMLTag_head:   
        case eHTMLTag_title:    case eHTMLTag_map:    
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    case eHTMLTag_col:    
        case eHTMLTag_colgroup: case eHTMLTag_unknown:
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    case eHTMLTag_entity:
      switch((eHTMLTags)aParent) {
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    default:
      if(eHTMLTag_unknown==aParent)
        result=PR_FALSE;
      break;
  } //switch
  return result;
}


/**
 * 
 * @update	gess6/16/98
 * @param 
 * @return
 */
PRBool IsCompatibleStyleTag(eHTMLTags aTag1,eHTMLTags aTag2) {
  PRBool result=PR_FALSE;

  if(0!=strchr(gStyleTags,aTag1)) {
    result=PRBool(0!=strchr(gStyleTags,aTag2));
  }
  return result;
}

/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmitEndTag(eHTMLTags aParent,eHTMLTags aChild) const {
  PRBool result=PR_FALSE;

  //begin with some simple (and obvious) cases...
  switch((eHTMLTags)aChild) {

    case eHTMLTag_userdefined:
    case eHTMLTag_comment:
      result=PR_TRUE; 
      break;

    case eHTMLTag_newline:    
    case eHTMLTag_whitespace:

      switch((eHTMLTags)aParent) {
        case eHTMLTag_html:     case eHTMLTag_head:   
        case eHTMLTag_title:    case eHTMLTag_map:    
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    case eHTMLTag_col:    
        case eHTMLTag_colgroup: case eHTMLTag_unknown:
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    default:
      if(IsCompatibleStyleTag(aChild,(eHTMLTags)GetTopNode()))
        result=PR_FALSE;
      else result=(!HasOpenContainer(aChild));
      break;
  } //switch
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::IsContainer(eHTMLTags aTag) const {
  PRBool result=PR_FALSE;

  switch(aTag){
    case eHTMLTag_area:       case eHTMLTag_base:
    case eHTMLTag_basefont:   case eHTMLTag_br:
    case eHTMLTag_col:        case eHTMLTag_colgroup:
    case eHTMLTag_frame:
    case eHTMLTag_hr:         case eHTMLTag_img:
    case eHTMLTag_input:      case eHTMLTag_isindex:
    case eHTMLTag_link:
    case eHTMLTag_math:       case eHTMLTag_meta:
//    case eHTMLTag_option:     
    case eHTMLTag_param:
    case eHTMLTag_style:      case eHTMLTag_spacer:
    case eHTMLTag_wbr:
    case eHTMLTag_form:
    case eHTMLTag_newline:
    case eHTMLTag_whitespace:
    case eHTMLTag_text: 
      result=PR_FALSE;
      break;

    default:
      result=PR_TRUE;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
eHTMLTags CNavDTD::GetDefaultParentTagFor(eHTMLTags aTag) const{
  eHTMLTags result=eHTMLTag_unknown;
  switch(aTag) {

    case eHTMLTag_text:
      result=eHTMLTag_paragraph; break;

    case eHTMLTag_html:
      result=eHTMLTag_unknown; break;

    case eHTMLTag_body:
    case eHTMLTag_head:
    case eHTMLTag_header:
    case eHTMLTag_footer:
    case eHTMLTag_frameset:
      result=eHTMLTag_html; break;

      //These tags are head specific...
    case eHTMLTag_style:
    case eHTMLTag_meta:
    case eHTMLTag_title:
    case eHTMLTag_base:
    case eHTMLTag_link:
      result=eHTMLTag_head; break;

      //These tags are table specific...
    case eHTMLTag_caption:
    case eHTMLTag_colgroup:
    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_thead:
      result=eHTMLTag_table; break;

    case eHTMLTag_tr:
      result=eHTMLTag_tbody; break;

    case eHTMLTag_td:
    case eHTMLTag_th:
      result=eHTMLTag_tr; break;

    case eHTMLTag_col:
      result=eHTMLTag_colgroup; break;    

    case eHTMLTag_dd:
    case eHTMLTag_dt:
      result=eHTMLTag_dl; break;    

    case eHTMLTag_option:
      result=eHTMLTag_select; break;    

      //These have to do with image maps...
    case eHTMLTag_area:
      result=eHTMLTag_map; break;    

      //These have to do with applets...
    case eHTMLTag_param:
      result=eHTMLTag_applet; break;    

      //These have to do with frames...
    case eHTMLTag_frame:
      result=eHTMLTag_frameset; break;    

    default:
      result=eHTMLTag_body; //XXX Hack! Just for now.
      break;
  }
  return result;
}


/**
 * This method tries to design a context vector (without actually
 * changing our parser state) from the parent down to the
 * child. 
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::ForwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag)  {
  PRBool result=PR_FALSE;

  switch(aParentTag) {
    case eHTMLTag_table:
      {
        static char  tableTags[]={eHTMLTag_tr,eHTMLTag_td,0};
        if(strchr(tableTags,aChildTag)) {
          //if you're here, we know we can correctly backward propagate.
          return BackwardPropagate(aVector,aParentTag,aChildTag);
        }
      }
      //otherwise, intentionally fall through...

    case eHTMLTag_tr:
      if(PR_TRUE==CanContain((PRInt32)eHTMLTag_td,(PRInt32)aChildTag)) {
        aVector.Append((PRUnichar)eHTMLTag_td);
        result=BackwardPropagate(aVector,aParentTag,eHTMLTag_td);
//        result=PR_TRUE;
      }

      break;

    case eHTMLTag_th:
      break;

    default:
      break;
  }//switch
  return result;
}


/**
 * This method tries to design a context map (without actually
 * changing our parser state) from the child up to the parent.
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::BackwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag) const {

  PRBool    result=PR_FALSE;
  eHTMLTags theParentTag=(eHTMLTags)aChildTag;

//  aVector.Truncate();

    //create the necessary stack of parent tags...
    //continue your search until you run out of known parents,
    //or you find the specific parent you were given (aParentTag).
//  aVector.Append((PRUnichar)aChildTag);
  do {
    theParentTag=(eHTMLTags)GetDefaultParentTagFor(theParentTag);
    if(theParentTag!=eHTMLTag_unknown) {
      aVector.Append((PRUnichar)theParentTag);
    }
  } while((theParentTag!=eHTMLTag_unknown) && (theParentTag!=aParentTag));
  
  return PRBool(aParentTag==theParentTag);
}

/**
 * 
 * @update	gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
PRInt32 CNavDTD::DidOpenContainer(eHTMLTags aTag,PRBool /*anExplicitOpen*/){
  PRInt32   result=0;
  
  switch (aTag) {

    case eHTMLTag_a:
    case eHTMLTag_bold:
    case eHTMLTag_big:
    case eHTMLTag_blink:
    case eHTMLTag_cite:
    case eHTMLTag_em:
    case eHTMLTag_font:
    case eHTMLTag_italic:
    case eHTMLTag_kbd:
    case eHTMLTag_small:
    case eHTMLTag_spell:
    case eHTMLTag_strike:
    case eHTMLTag_strong:
    case eHTMLTag_sub:
    case eHTMLTag_sup:
    case eHTMLTag_tt:
    case eHTMLTag_u:
    case eHTMLTag_var:
      break;

    default:
      break;
  }

  return result;
}

/**
 * 
 * @update	gess6/4/98
 * @param 
 * @return
 */
PRInt32 CNavDTD::DidCloseContainer(eHTMLTags aTag,PRBool/*anExplicitClosure*/){
  PRInt32 result=0;
  return result;
}


/**
 *  This method allows the caller to determine if a form
 *  element is currently open.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRBool CNavDTD::HasOpenContainer(eHTMLTags aContainer) const {
  PRBool result=PR_FALSE;

  switch(aContainer) {
    case eHTMLTag_form:
      result=mHasOpenForm; break;

    default:
      result=(kNotFound!=GetTopmostIndexOf(aContainer)); break;
  }
  return result;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @return  tag id of topmost node in contextstack
 */
eHTMLTags CNavDTD::GetTopNode() const {
  if(mContextStackPos) 
    return mContextStack[mContextStackPos-1];
  return eHTMLTag_unknown;
}


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 CNavDTD::GetTopmostIndexOf(eHTMLTags aTag) const {
  int i=0;
  for(i=mContextStackPos-1;i>=0;i--){
    if(mContextStack[i]==aTag)
      return i;
  }
  return kNotFound;
}

/*********************************************
  Here comes code that handles the interface
  to our content sink.
 *********************************************/


/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called whenever a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and open any style containers that
 * are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 *
 * @update	gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
PRInt32 CNavDTD::OpenTransientStyles(eHTMLTags aTag){
  PRInt32 result=0;

  if(0==strchr(gWhitespaceTags,aTag)){
    PRInt32 pos=0;

    eHTMLTags parentTag=(eHTMLTags)GetTopNode();
    if(CanContainStyles(parentTag)) {
      for(pos=0;pos<mStyleStackPos;pos++) {
        eHTMLTags theTag=mStyleStack[pos]; 
        if(PR_FALSE==HasOpenContainer(theTag)) {

          CStartToken   token(GetTagName(theTag));
          nsCParserNode theNode(&token);

          switch(theTag) {
            case eHTMLTag_secret_h1style: case eHTMLTag_secret_h2style: 
            case eHTMLTag_secret_h3style: case eHTMLTag_secret_h4style:
            case eHTMLTag_secret_h5style: case eHTMLTag_secret_h6style:
              break;
            default:
              token.SetTypeID(theTag);  //open the html container...
              result=OpenContainer(theNode,PR_FALSE);
              mLeafBits[mContextStackPos-1]=PR_TRUE;
          } //switch
        }
        if(kNoError!=result)
          break;
      }//for
    }
  }//if
  return result;
}

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called just prior when a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and <i>close</i> any style containers 
 * that are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 * SEE THE TOP OF THIS FILE for more information about how the transient style stack works.
 *
 * @update	gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
PRInt32 CNavDTD::CloseTransientStyles(eHTMLTags aTag){
  PRInt32 result=0;

  if((mStyleStackPos>0) && (mLeafBits[mContextStackPos-1])) {
    if(0==strchr(gWhitespaceTags,aTag)){

      result=CloseContainersTo(mStyleStack[0],PR_FALSE);
      mLeafBits[mContextStackPos-1]=PR_FALSE;

    }//if
  }//if
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRInt32 result=mSink->OpenHTML(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseHTML(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenHead(const nsIParserNode& aNode){
  mContextStack[mContextStackPos++]=eHTMLTag_head;
  PRInt32 result=mSink->OpenHead(aNode); 
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseHead(const nsIParserNode& aNode){
  PRInt32 result=mSink->CloseHead(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRInt32 result=kNoError;
  eHTMLTags topTag=(eHTMLTags)CNavDTD::GetTopNode();

  if(eHTMLTag_html!=topTag) {
    
    //ok, there are two cases:
    //  1. Nobody opened the html container
    //  2. Someone left the head (or other) open
    PRInt32 pos=GetTopmostIndexOf(eHTMLTag_html);
    if(kNotFound!=pos) {
      //if you're here, it means html is open, 
      //but some other tag(s) are in the way.
      //So close other tag(s).
      result=CloseContainersTo(pos+1,eHTMLTag_body,PR_TRUE);
    } else {
      //if you're here, it means that there is
      //no HTML tag in document. Let's open it.

      result=CloseContainersTo(0,eHTMLTag_html,PR_TRUE);  //close current stack containers.

      CHTMLToken    token(gEmpty);
      nsCParserNode htmlNode(&token);

      token.SetTypeID(eHTMLTag_html);  //open the html container...
      result=OpenHTML(htmlNode);
    }
  }

  if(kNoError==result) {
    result=mSink->OpenBody(aNode); 
    mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  }
  return result;
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseBody(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenForm(const nsIParserNode& aNode){
  if(mHasOpenForm)
    CloseForm(aNode);
  PRInt32 result=mSink->OpenForm(aNode);
  if(kNoError==result)
    mHasOpenForm=PR_TRUE;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseForm(const nsIParserNode& aNode){
  PRInt32 result=kNoError;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;
    result=mSink->CloseForm(aNode); 
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenMap(const nsIParserNode& aNode){
  if(mHasOpenMap)
    CloseMap(aNode);

  //NOTE: We need to change to method so that it opens a MAP, 
  //      instead of a FORM. This was copy/paste coding at its best.

  PRInt32 result=mSink->OpenForm(aNode);
  if(kNoError==result)
    mHasOpenMap=PR_TRUE;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseMap(const nsIParserNode& aNode){
  PRInt32 result=kNoError;
  if(mHasOpenMap) {
    mHasOpenMap=PR_FALSE;

  //NOTE: We need to change to method so that it closes a MAP, 
  //      instead of a FORM. This was copy/paste coding at its best.
    
    result=mSink->CloseForm(aNode); 
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRInt32 result=mSink->OpenFrameset(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseFrameset(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  
  PRInt32   result=kNoError; 
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();

//  CloseTransientStyles(nodeType);

  switch(nodeType) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_body:
      result=OpenBody(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
    case eHTMLTag_head:
    case eHTMLTag_title:
      break;

    case eHTMLTag_form:
      result=OpenForm(aNode); break;

    default:
      result=mSink->OpenContainer(aNode); 
      mContextStack[mContextStackPos++]=nodeType;
      break;
  }

  if((kNoError==result) && (PR_TRUE==aUpdateStyleStack)){
    UpdateStyleStackForOpenTag(nodeType,nodeType);
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseContainer(const nsIParserNode& aNode,eHTMLTags aTag,PRBool aUpdateStyles){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32   result=kNoError; //was false
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(nodeType) {

    case eHTMLTag_html:
      result=CloseHTML(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
      break;

    case eHTMLTag_head:
      //result=CloseHead(aNode); 
      break;

    case eHTMLTag_body:
      result=CloseBody(aNode); break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_title:
    default:
      result=mSink->CloseContainer(aNode); 
      mContextStack[--mContextStackPos]=eHTMLTag_unknown;
      break;
  }

  mLeafBits[mContextStackPos]=PR_FALSE;
  if((kNoError==result) && (PR_TRUE==aUpdateStyles)){
    UpdateStyleStackForCloseTag(nodeType,aTag);
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=kNoError;

  CEndToken aToken(gEmpty);
  nsCParserNode theNode(&aToken);

  if((anIndex<mContextStackPos) && (anIndex>=0)) {
    while(mContextStackPos>anIndex) {
      eHTMLTags theTag=mContextStack[mContextStackPos-1];
      aToken.SetTypeID(theTag);
      result=CloseContainer(theNode,aTag,aUpdateStyles);
    }
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  PRInt32 pos=GetTopmostIndexOf(aTag);

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    return CloseContainersTo(pos,aTag,aUpdateStyles);
  }

  eHTMLTags theTopTag=(eHTMLTags)GetTopNode();
  if(IsCompatibleStyleTag(aTag,theTopTag)) {
    //if you're here, it's because we're trying to close one style tag,
    //but a different one is actually open. Because this is NAV4x
    //compatibililty mode, we must close the one that's really open.
    aTag=theTopTag;    
    pos=GetTopmostIndexOf(aTag);
    if(kNotFound!=pos) {
      //the tag is indeed open, so close it.
      return CloseContainersTo(pos,aTag,aUpdateStyles);
    }
  }
  
  PRInt32 result=kNoError;
  eHTMLTags theParentTag=GetDefaultParentTagFor(aTag);
  pos=GetTopmostIndexOf(theParentTag);
  if(kNotFound!=pos) {
    //the parent container is open, so close it instead
    result=CloseContainersTo(pos+1,aTag,aUpdateStyles);
  }
  return result;
}

/**
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update  gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::CloseTopmostContainer(){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  CEndToken aToken(gEmpty);
  eHTMLTags theTag=(eHTMLTags)mContextStack[mContextStackPos-1];
  aToken.SetTypeID(theTag);
  nsCParserNode theNode(&aToken);
  return CloseContainer(theNode,theTag,PR_TRUE);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 CNavDTD::AddLeaf(const nsIParserNode& aNode){
  PRInt32 result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gess 4/8/98
 *  @param   aChildTag is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
PRInt32 CNavDTD::CreateContextStackFor(eHTMLTags aChildTag){
  nsAutoString  theVector;
  
  PRInt32   result=kNoError;
  PRInt32   pos=0;
  PRInt32   cnt=0;
  eHTMLTags theTop=GetTopNode();
  
  if(PR_TRUE==ForwardPropagate(theVector,theTop,aChildTag)){
    //add code here to build up context stack based on forward propagated context vector...
    pos=0;
    cnt=theVector.Length()-1;
    if(mContextStack[mContextStackPos-1]==theVector[cnt])
      result=kNoError;
    else result=kContextMismatch;
  }
  else {
    PRBool tempResult;
    if(eHTMLTag_unknown!=theTop) {
      tempResult=BackwardPropagate(theVector,theTop,aChildTag);
      if(eHTMLTag_html!=theTop)
        BackwardPropagate(theVector,eHTMLTag_html,theTop);
    }
    else tempResult=BackwardPropagate(theVector,eHTMLTag_html,aChildTag);

    if(PR_TRUE==tempResult) {

      //propagation worked, so pop unwanted containers, push new ones, then exit...
      pos=0;
      cnt=theVector.Length();
      result=kNoError;
      while(pos<mContextStackPos) {
        if(mContextStack[pos]==theVector[cnt-1-pos]) {
          pos++;
        }
        else {
          //if you're here, you have something on the stack
          //that doesn't match your needed tags order.
          result=CloseContainersTo(pos,eHTMLTag_unknown,PR_TRUE);
          break;
        }
      } //while
    } //elseif
    else result=kCantPropagate;
  } //elseif

    //now, build up the stack according to the tags 
    //you have that aren't in the stack...
  if(kNoError==result){
    int i=0;
    for(i=pos;i<cnt;i++) {
      CStartToken* st=new CStartToken((eHTMLTags)theVector[cnt-1-i]);
      HandleStartToken(st);
    }
  }
  return result;
}


/**
 *  This method gets called to ensure that the context
 *  stack is properly set up for the given child. 
 *  We pop containers off the stack (all the way down 
 *  html) until we get a container that can contain
 *  the given child.
 *  
 *  @update  gess 4/8/98
 *  @param   
 *  @return  
 */
PRInt32 CNavDTD::ReduceContextStackFor(eHTMLTags aChildTag){
  PRInt32   result=kNoError;
  eHTMLTags topTag=(eHTMLTags)GetTopNode();

  while( (topTag!=kNotFound) && 
         (PR_FALSE==CanContain(topTag,aChildTag)) &&
         (PR_FALSE==CanContainIndirect(topTag,aChildTag))) {
    CloseTopmostContainer();
    topTag=(eHTMLTags)GetTopNode();
  }
  return result;
}


/**
 * This method causes all explicit style-tag containers that
 * are opened to be reflected on our internal style-stack.
 *
 * @update	gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
PRInt32 CNavDTD::UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags anActualTag){
  PRInt32   result=0;
  
  switch (aTag) {

    case eHTMLTag_a:
    case eHTMLTag_bold:
    case eHTMLTag_big:
    case eHTMLTag_blink:
    case eHTMLTag_cite:
    case eHTMLTag_em:
    case eHTMLTag_font:
    case eHTMLTag_italic:
    case eHTMLTag_kbd:
    case eHTMLTag_small:
    case eHTMLTag_spell:
    case eHTMLTag_strike:
    case eHTMLTag_strong:
    case eHTMLTag_sub:
    case eHTMLTag_sup:
    case eHTMLTag_tt:
    case eHTMLTag_u:
    case eHTMLTag_var:
      mStyleStack[mStyleStackPos++]=aTag;
      break;

    case eHTMLTag_h1: case eHTMLTag_h2:
    case eHTMLTag_h3: case eHTMLTag_h4:
    case eHTMLTag_h5: case eHTMLTag_h6:
      break;

    default:
      break;
  }

  return result;
} //update...

/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update	gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
PRInt32 CNavDTD::UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags anActualTag){
  PRInt32 result=0;
  
  if(mStyleStackPos>0) {
    switch (aTag) {

      case eHTMLTag_a:
      case eHTMLTag_bold:
      case eHTMLTag_big:
      case eHTMLTag_blink:
      case eHTMLTag_cite:
      case eHTMLTag_em:
      case eHTMLTag_font:
      case eHTMLTag_italic:
      case eHTMLTag_kbd:
      case eHTMLTag_small:
      case eHTMLTag_spell:
      case eHTMLTag_strike:
      case eHTMLTag_strong:
      case eHTMLTag_sub:
      case eHTMLTag_sup:
      case eHTMLTag_tt:
      case eHTMLTag_u:
      case eHTMLTag_var:
        if(aTag==anActualTag)
          mStyleStack[--mStyleStackPos]=eHTMLTag_unknown;
        break;

      case eHTMLTag_h1: case eHTMLTag_h2:
      case eHTMLTag_h3: case eHTMLTag_h4:
      case eHTMLTag_h5: case eHTMLTag_h6:
        break;

      default:
        break;
    }//switch
  }//if
  return result;
} //update...


/*******************************************************************
  These methods used to be hidden in the tokenizer-delegate. 
  That file merged with the DTD, since the separation wasn't really
  buying us anything.
 *******************************************************************/

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of some kind of tagged 
 *  element. We don't know yet if it's a tag or a comment.
 *  
 *  @update  gess 5/12/98
 *  @param   aChar is the last char read
 *  @param   aScanner is represents our input source
 *  @param   aToken is the out arg holding our new token
 *  @return  error code (may return kInterrupted).
 */
PRInt32 CNavDTD::ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {

  nsAutoString empty("");
  PRInt32 result=aScanner.GetChar(aChar);

  if(kNoError==result) {

    switch(aChar) {
      case kForwardSlash:
        PRUnichar ch; 
        result=aScanner.Peek(ch);
        if(kNoError==result) {
          if(nsString::IsAlpha(ch))
            aToken=new CEndToken(empty);
          else aToken=new CCommentToken(empty); //Special case: </ ...> is treated as a comment
        }//if
        break;
      case kExclamation:
        aToken=new CCommentToken(empty);
        break;
      default:
        if(nsString::IsAlpha(aChar))
          return ConsumeStartTag(aChar,aScanner,aToken);
        else if(kEOF!=aChar) {
          nsAutoString temp("<");
          return ConsumeText(temp,aScanner,aToken);
        }
    } //switch

    if((0!=aToken) && (kNoError==result)) {
      result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
      if(result) {
        delete aToken;
        aToken=0;
      }
    } //if
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
PRInt32 CNavDTD::ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken) {
  PRBool done=PR_FALSE;
  PRInt32 result=kNoError;
  nsAutoString as("");
  PRInt16 theAttrCount=0;

  while((!done) && (result==kNoError)) {
    CAttributeToken* theToken= new CAttributeToken(as);
    if(theToken){
      result=theToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    

      //Much as I hate to do this, here's some special case code.
      //This handles the case of empty-tags in XML. Our last
      //attribute token will come through with a text value of ""
      //and a textkey of "/". We should destroy it, and tell the 
      //start token it was empty.
      nsString& key=theToken->GetKey();
      nsString& text=theToken->GetText();
      if((key[0]==kForwardSlash) && (0==text.Length())){
        //tada! our special case! Treat it like an empty start tag...
        aToken->SetEmpty(PR_TRUE);
        delete theToken;
      }
      else if(kNoError==result){
        theAttrCount++;
        mTokenDeque.Push(theToken);
      }//if
      else delete theToken; //we can't keep it...
    }//if
    
    if(kNoError==result){
      result=aScanner.Peek(aChar);
      if(aChar==kGreaterThan) { //you just ate the '>'
        aScanner.GetChar(aChar); //skip the '>'
        done=PR_TRUE;
      }//if
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
PRInt32 CNavDTD::ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  
  //In the case that we just read the given tag, we should go and
  //consume all the input until we find a matching end tag.

  nsAutoString endTag("</");
  endTag.Append(aString);
  endTag.Append(">");
  aToken=new CSkippedContentToken(endTag);
  return aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
}

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of a tag.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 CNavDTD::ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  PRInt32 theDequeSize=mTokenDeque.GetSize();
  PRInt32 result=kNoError;

  aToken=new CStartToken(nsAutoString(""));
  
  if(aToken) {
    result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(kNoError==result) {
      if(((CStartToken*)aToken)->IsAttributed()) {
        result=ConsumeAttributes(aChar,aScanner,(CStartToken*)aToken);
      }
      //now that that's over with, we have one more problem to solve.
      //In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
      //consume all the content itself.
      if(kNoError==result) {
        nsString& str=aToken->GetText();
        CToken*   skippedToken=0;
        if(str.EqualsIgnoreCase("SCRIPT") ||
           str.EqualsIgnoreCase("STYLE") ||
           str.EqualsIgnoreCase("TITLE") ||
           str.EqualsIgnoreCase("TEXTAREA")) {
          result=ConsumeContentToEndTag(str,aChar,aScanner,skippedToken);
    
          if((kNoError==result) && skippedToken){
              //now we strip the ending sequence from our new SkippedContent token...
            PRInt32 slen=str.Length()+3;
            nsString& skippedText=skippedToken->GetText();
    
            skippedText.Cut(skippedText.Length()-slen,slen);
            mTokenDeque.Push(skippedToken);
  
            //In the case that we just read a given tag, we should go and
            //consume all the tag content itself (and throw it all away).

            CEndToken* endtoken=new CEndToken(str);
            mTokenDeque.Push(endtoken);
          } //if
        } //if
      } //if

      //EEEEECCCCKKKK!!! 
      //This code is confusing, so pay attention.
      //If you're here, it's because we were in the midst of consuming a start
      //tag but ran out of data (not in the stream, but in this *part* of the stream.
      //For simplicity, we have to unwind our input. Therefore, we pop and discard
      //any new tokens we've cued this round. Later we can get smarter about this.
      if(kNoError!=result) {
        while(mTokenDeque.GetSize()>theDequeSize) {
          delete mTokenDeque.PopBack();
        }
      }


    } //if
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
PRInt32 CNavDTD::ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
   PRUnichar  ch;
   PRInt32 result=aScanner.GetChar(ch);

   if(kNoError==result) {
     if(nsString::IsAlpha(ch)) { //handle common enity references &xxx; or &#000.
       aToken = new CEntityToken(nsAutoString(""));
       result = aToken->Consume(ch,aScanner);  //tell new token to finish consuming text...    
     }
     else if(kHashsign==ch) {
       aToken = new CEntityToken(nsAutoString(""));
       result=aToken->Consume(0,aScanner);
     }
     else {
       //oops, we're actually looking at plain text...
       nsAutoString temp("&");
       result=ConsumeText(temp,aScanner,aToken);
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
PRInt32 CNavDTD::ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  aToken = new CWhitespaceToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
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
PRInt32 CNavDTD::ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken = new CCommentToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
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
PRInt32 CNavDTD::ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken){

  PRInt32 result=kNoError;
  if(aToken=new CTextToken(aString)) {
    PRUnichar ch=0;
    result=aToken->Consume(ch,aScanner);
  }
  return result;
}

/**
 *  This method is called just after a newline has been consumed. 
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 CNavDTD::ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken=new CNewlineToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
  }
  return result;
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
PRInt32 CNavDTD::ConsumeToken(CToken*& aToken){
  
  aToken=0;
  if(mTokenDeque.GetSize()>0) {
    aToken=(CToken*)mTokenDeque.Pop();
    return kNoError;
  }

  PRInt32   result=kNoError;
  CScanner* theScanner=mParser->GetScanner();
  if(kNoError==result){
    
    PRUnichar aChar;
    result=theScanner->GetChar(aChar);
    switch(result) {
      case kEOF:
        break;

      case kInterrupted:
        theScanner->RewindToMark();
        break; 

      case kNoError:
      default:
        switch(aChar) {
          case kLessThan:
            return ConsumeTag(aChar,*theScanner,aToken);

          case kAmpersand:
            return ConsumeEntity(aChar,*theScanner,aToken);
          
          case kCR: case kLF:
            return ConsumeNewline(aChar,*theScanner,aToken);
          
          case kNotFound:
            break;
          
          default:
            if(!nsString::IsSpace(aChar)) {
              nsAutoString temp(aChar);
              return ConsumeText(temp,*theScanner,aToken);
            }
            else return ConsumeWhitespace(aChar,*theScanner,aToken);
            break;
        } //switch
        break; 
    } //switch
    if(kNoError==result)
      result=theScanner->Eof();
  } //while
  return result;
}

/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
CToken* CNavDTD::CreateTokenOfType(eHTMLTokenTypes aType) {
  return 0;
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CNavDTD::WillResumeParse(void){
  if(mSink) {
    mSink->WillResume();
  }
  return;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CNavDTD::WillInterruptParse(void){
  if(mSink) {
    mSink->WillInterrupt();
  }
  return;
}


