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
 * See notes about transient style handling
 * in the nsNavDTD.h file.
 *         
 */

#include "COtherDTD.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsHTMLContentSink.h" 
#include "nsScanner.h"
#include "nsParserTypes.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"
#include "prio.h"
#include "plstr.h"
#include <fstream.h>


#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include <time.h>
#include "prmem.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_IOtherHTML_DTD_IID); 
static NS_DEFINE_IID(kBaseClassIID, NS_INAVHTML_DTD_IID); 

static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kHTMLTextContentType = "text/html";

static nsAutoString gEmpty;


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
nsresult COtherDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kBaseClassIID)) {  //do nav dtd base class...
    *aInstancePtr = (CNavDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (COtherDTD*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult)
{
  COtherDTD* it = new COtherDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(COtherDTD)
NS_IMPL_RELEASE(COtherDTD)

/**
 *  Default constructor; parent does all the real work.
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::COtherDTD() : CNavDTD() {
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::~COtherDTD(){
  //parent does all the real work of destruction.
}

/**
 * This method gets called by the parser to determine if this DTD can handle 
 * the requested process for the requested content type.
 *
 * @update	gess6/24/98
 * @param 
 * @return  TRUE if the DTD can handle the process for this type; FALSE otherwise.
 */
PRBool COtherDTD::IsCapableOf(eProcessType aProcessType, nsString& aContentType,PRInt32 aVersion){
  return CNavDTD::IsCapableOf(aProcessType,aContentType,aVersion);
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 COtherDTD::WillBuildModel(const char* aFilename) {
  return CNavDTD::WillBuildModel(aFilename);
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 COtherDTD::DidBuildModel(PRInt32 anErrorCode){
  return CNavDTD::DidBuildModel(anErrorCode);
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
PRInt32 COtherDTD::HandleToken(CToken* aToken){
  return CNavDTD::HandleToken(aToken);
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
PRInt32 COtherDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode) {
  return CNavDTD::HandleDefaultStartToken(aToken,aChildTag,aNode);
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
PRInt32 COtherDTD::HandleStartToken(CToken* aToken) {
  return CNavDTD::HandleStartToken(aToken);
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
PRInt32 COtherDTD::HandleEndToken(CToken* aToken) {
  return CNavDTD::HandleEndToken(aToken);
}

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 COtherDTD::HandleEntityToken(CToken* aToken) {
  return CNavDTD::HandleEntityToken(aToken);
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
PRInt32 COtherDTD::HandleCommentToken(CToken* aToken) {
  return CNavDTD::HandleCommentToken(aToken);
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
PRInt32 COtherDTD::HandleSkippedContentToken(CToken* aToken) {
  return CNavDTD::HandleSkippedContentToken(aToken);
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
PRInt32 COtherDTD::HandleAttributeToken(CToken* aToken) {
  return CNavDTD::HandleAttributeToken(aToken);
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 COtherDTD::HandleScriptToken(CToken* aToken) {
  return CNavDTD::HandleScriptToken(aToken);
}

/**
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 COtherDTD::HandleStyleToken(CToken* aToken){
  return CNavDTD::HandleStyleToken(aToken);
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
PRBool COtherDTD::CanContainFormElement(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanContainFormElement(aParent,aChild);
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
PRBool COtherDTD::CanContain(PRInt32 aParent,PRInt32 aChild) {
  return CNavDTD::CanContain(aParent,aChild);
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
PRBool COtherDTD::CanContainIndirect(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanContainIndirect(aParent,aChild);
}

/**
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanOmit(aParent,aChild);
}


/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::CanOmitEndTag(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanOmitEndTag(aParent,aChild);
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::IsContainer(eHTMLTags aTag) const {
  return CNavDTD::IsContainer(aTag);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
eHTMLTags COtherDTD::GetDefaultParentTagFor(eHTMLTags aTag) const{
  return CNavDTD::GetDefaultParentTagFor(aTag);
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
PRBool COtherDTD::ForwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag) {
  return CNavDTD::ForwardPropagate(aVector,aParentTag,aChildTag);
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
PRBool COtherDTD::BackwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag) const {
  return CNavDTD::BackwardPropagate(aVector,aParentTag,aChildTag);
}

/**
 * 
 * @update	gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
PRInt32 COtherDTD::DidOpenContainer(eHTMLTags aTag,PRBool anExplicitOpen){
  return CNavDTD::DidOpenContainer(aTag,anExplicitOpen);
}

/**
 * 
 * @update	gess6/4/98
 * @param 
 * @return
 */
PRInt32 COtherDTD::DidCloseContainer(eHTMLTags aTag,PRBool anExplicitClosure){
  return CNavDTD::DidOpenContainer(aTag,anExplicitClosure);
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
PRInt32 COtherDTD::OpenTransientStyles(eHTMLTags aTag){
  return CNavDTD::OpenTransientStyles(aTag);
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
PRInt32 COtherDTD::CloseTransientStyles(eHTMLTags aTag){
  return CNavDTD::CloseTransientStyles(aTag);
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
PRInt32 COtherDTD::OpenHTML(const nsIParserNode& aNode){
  return CNavDTD::OpenHTML(aNode);
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
PRInt32 COtherDTD::CloseHTML(const nsIParserNode& aNode){
  return CNavDTD::CloseHTML(aNode);
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenHead(const nsIParserNode& aNode){
  return CNavDTD::OpenHead(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseHead(const nsIParserNode& aNode){
  return CNavDTD::CloseHead(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenBody(const nsIParserNode& aNode){
  return CNavDTD::OpenBody(aNode);
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseBody(const nsIParserNode& aNode){
  return CNavDTD::CloseBody(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenForm(const nsIParserNode& aNode){
  return CNavDTD::OpenForm(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseForm(const nsIParserNode& aNode){
  return CNavDTD::CloseForm(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenMap(const nsIParserNode& aNode){
  return CNavDTD::OpenMap(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseMap(const nsIParserNode& aNode){
  return CNavDTD::CloseMap(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenFrameset(const nsIParserNode& aNode){
  return CNavDTD::OpenFrameset(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseFrameset(const nsIParserNode& aNode){
  return CNavDTD::CloseFrameset(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack){
  return CNavDTD::OpenContainer(aNode,aUpdateStyleStack);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseContainer(const nsIParserNode& aNode,eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainer(aNode,aTag,aUpdateStyles);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainersTo(anIndex,aTag,aUpdateStyles);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainersTo(aTag,aUpdateStyles);
}

/**
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update  gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::CloseTopmostContainer(){
  return CNavDTD::CloseTopmostContainer();
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 COtherDTD::AddLeaf(const nsIParserNode& aNode){
  return CNavDTD::AddLeaf(aNode);
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
PRInt32 COtherDTD::CreateContextStackFor(eHTMLTags aChildTag){
  return CNavDTD::CreateContextStackFor(aChildTag);
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
PRInt32 COtherDTD::ReduceContextStackFor(eHTMLTags aChildTag){
  return CNavDTD::ReduceContextStackFor(aChildTag);
}


/**
 * This method causes all explicit style-tag containers that
 * are opened to be reflected on our internal style-stack.
 *
 * @update	gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
PRInt32 COtherDTD::UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags anActualTag){
  return CNavDTD::UpdateStyleStackForOpenTag(aTag,anActualTag);
} //update...

/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update	gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
PRInt32 COtherDTD::UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags anActualTag){
  return CNavDTD::UpdateStyleStackForCloseTag(aTag,anActualTag);
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
PRInt32 COtherDTD::ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  return CNavDTD::ConsumeTag(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken) {
  return CNavDTD::ConsumeAttributes(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  return CNavDTD::ConsumeContentToEndTag(aString,aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  return CNavDTD::ConsumeStartTag(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  return CNavDTD::ConsumeEntity(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  return CNavDTD::ConsumeWhitespace(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  return CNavDTD::ConsumeComment(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken){
  return CNavDTD::ConsumeText(aString,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  return CNavDTD::ConsumeNewline(aChar,aScanner,aToken);
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
PRInt32 COtherDTD::ConsumeToken(CToken*& aToken){
  return CNavDTD::ConsumeToken(aToken);
}

/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
CToken* COtherDTD::CreateTokenOfType(eHTMLTokenTypes aType) {
  return CNavDTD::CreateTokenOfType(aType);
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void COtherDTD::WillResumeParse(void){
  CNavDTD::WillResumeParse();
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void COtherDTD::WillInterruptParse(void){
  CNavDTD::WillInterruptParse();
}

