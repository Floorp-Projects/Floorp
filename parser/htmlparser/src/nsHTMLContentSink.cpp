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

#include "nsHTMLContentSink.h"
#include "nsHTMLTokens.h"
#include "prtypes.h" 
#include <iostream.h>  

#define VERBOSE_DEBUG

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENTSINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTMLCONTENTSINK_IID);
static NS_DEFINE_IID(kClassIID, NS_HTMLCONTENTSINK_IID); 



/**-------------------------------------------------------
 *  "Fakey" factory method used to create an instance of
 *  this class.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsresult NS_NewHTMLContentSink(nsIContentSink** aInstancePtrResult)
{
  nsHTMLContentSink *it = new nsHTMLContentSink();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


/**-------------------------------------------------------
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsHTMLContentSink::nsHTMLContentSink() : nsIHTMLContentSink(), mTitle("") {
  mNodeStackPos=0;
  memset(mNodeStack,0,sizeof(mNodeStack));
}


/**-------------------------------------------------------
 *  Default destructor. Probably not a good idea to call 
 *  this if you created your instance via the factor method.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsHTMLContentSink::~nsHTMLContentSink() {
}

#ifdef VERBOSE_DEBUG
static void DebugDump(const char* str1,const nsString& str2,PRInt32 tabs) {
  for(PRInt32 i=0;i<tabs;i++)
    cout << " "; //add some tabbing to debug output...
  char* cp = str2.ToNewCString();
  cout << str1 << cp << ">" << endl;
  delete cp;
}
#endif


/**-------------------------------------------------------
 *  This bit of magic creates the addref and release 
 *  methods for this class.
 *
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 *------------------------------------------------------*/
NS_IMPL_ADDREF(nsHTMLContentSink)
NS_IMPL_RELEASE(nsHTMLContentSink)



/**-------------------------------------------------------
 *  Standard XPCOM query interface implementation. I used
 *  my own version because this class is a subclass of both
 *  ISupports and IContentSink. Perhaps there's a macro for
 *  this, but I didn't see it.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsresult nsHTMLContentSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIContentSinkIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {  //do nsIHTMLContentSink base class...
    *aInstancePtr = (nsIHTMLContentSink*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLContentSink*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


/**-------------------------------------------------------
 *  This method gets called by the parser when a <HTML> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenHTML(const nsIParserNode& aNode) {

  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a </HTML> 
 *  tag has been consumed.
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseHTML(const nsIParserNode& aNode){

  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser <i>any time</i>
 *  head data gets consumed by the parser. Currently, that
 *  list includes <META>, <ISINDEX>, <LINK>, <SCRIPT>,
 *  <STYLE>, <TITLE>.
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenHead(const nsIParserNode& aNode) {
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a </HEAD>
 *  tag has been seen (either implicitly or explicitly).
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseHead(const nsIParserNode& aNode) {
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This gets called by the parser when a <TITLE> tag 
 *  gets consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::SetTitle(const nsString& aValue){
  PRBool result=PR_TRUE;
  mTitle=aValue;
  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a <BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenBody(const nsIParserNode& aNode) {
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

  #ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
  #endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a </BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");
  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a <FORM> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenForm(const nsIParserNode& aNode) {
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a </FORM> 
 *  tag has been consumed.
 *   
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a <FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenFrameset(const nsIParserNode& aNode) {
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

  #ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
  #endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a </FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when any general
 *  type of container has been consumed and needs to be 
 *  opened. This includes things like <OL>, <Hn>, etc...
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::OpenContainer(const nsIParserNode& aNode){  
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This method gets called by the parser when a close
 *  container tag has been consumed and needs to be closed.
 *  
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRBool result=PR_TRUE;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This causes the topmost container to be closed, 
 *  regardless of its type.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::CloseTopmostContainer(){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");
  PRBool result=PR_TRUE;
  mNodeStack[mNodeStackPos--]=eHTMLTag_unknown;
  return result;
}

/**-------------------------------------------------------
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsHTMLContentSink::AddLeaf(const nsIParserNode& aNode){
  PRBool result=PR_TRUE;

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}



