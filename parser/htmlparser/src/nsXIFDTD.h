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

   The nsXIFDTD acts as both a DTD and a content sink. The DTD converts a stream of 
   input tokens into a stream of html.

 * @update  gpk 06/16/98
 * 
 *         
 */

#ifndef NS_XIFDTD__
#define NS_XIFDTD__

#include "nshtmlpars.h"
#include "nsIDTD.h"
#include "nsIContentSink.h"
#include "nsHTMLTokens.h"
#include "nsVoidArray.h"
#include <fstream.h>


#define NS_XIF_DTD_IID      \
  {0xc2edf770, 0x06d5,  0x11d2,  \
  {0xbc, 0x4a, 0x00,    0xaa, 0x00, 0x53, 0x3d, 0x6d}}


class nsParser;
class nsCParserNode;
class CTokenHandler;
class nsIDTDDebug;
class nsIHTMLContentSink;
class nsITokenizer;

//*** This enum is used to define the known universe of XIF tags.
//*** The use of this table doesn't preclude of from using non-standard
//*** tags. It simply makes normal tag handling more efficient.
enum eXIFTags
{
  eXIFTag_unknown=0,   

  eXIFTag_attr,
  eXIFTag_comment,
  eXIFTag_container,
  eXIFTag_content,

  eXIFTag_css_declaration,
  eXIFTag_css_declaration_list,
  eXIFTag_css_rule,
  eXIFTag_css_selector,
  eXIFTag_css_stylelist,
  eXIFTag_css_stylerule,
  eXIFTag_css_stylesheet,

  eXIFTag_doctype,
  eXIFTag_encode,    
  eXIFTag_import,
  eXIFTag_leaf,
  eXIFTag_link,
  eXIFTag_section,
  eXIFTag_section_body,
  eXIFTag_section_head,
  eXIFTag_stylelist,
  eXIFTag_url,    
  eXIFTag_xml,    
  
  eXIFTag_newline = eHTMLTag_newline,
  eXIFTag_text = eHTMLTag_text,
  eXIFTag_whitespace = eHTMLTag_whitespace,

  eXIFTag_userdefined
};

//*** This enum is used to define the known universe of XIF attributes.
//*** The use of this table doesn't preclude of from using non-standard
//*** attributes. It simply makes normal tag handling more efficient.
enum eXIFAttributes {
  eXIFAttr_unknown=0,

  eXIFAttr_key,
  eXIFAttr_tag,
  eXIFAttr_value,

  eXIFAttr_userdefined  
};

class nsXIFDTD : public nsIDTD {
            
  public:

 
    NS_DECL_ISUPPORTS

    /**
     *  
     *  
     *  @update  gpk 06/18/98
     *  @param   
     *  @return  
     */
    nsXIFDTD();

    virtual const nsIID&  GetMostDerivedIID(void) const;

    /**
     *  
     *  
     *  @update  gpk 06/18/98
     *  @param   
     *  @return  
     */
    virtual ~nsXIFDTD();

    /**
     * Call this method if you want the DTD to construct a clone of itself.
     * @update	gess7/23/98
     * @param 
     * @return
     */
    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

    /**
     * This method is called to determine if the given DTD can parse
     * a document in a given source-type. 
     * NOTE: Parsing always assumes that the end result will involve
     *       storing the result in the main content model.
     * @update	gpk 7/9/98
     * @param   
     * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
     */
    virtual eAutoDetectResult CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion);

    /**
     * 
     * @update	gess 7/24/98
     * @param 
     * @return
     */
    NS_IMETHOD WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink=0);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver=0,nsIContentSink* aSink=0);

    /**
     *  
     * @update	gess 7/24/98
     * @param 
     * @return
     */
    NS_IMETHOD DidBuildModel(nsresult aQualityLevel,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink=0);

    /**
     *  
     *  @update  gpk 06/18/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    NS_IMETHOD HandleToken(CToken* aToken,nsIParser* aParser);

    /**
     *  This method causes all tokens to be dispatched to the given tag handler.
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object to receive subsequent tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD CaptureTokenPump(nsITagHandler* aHandler);

    /**
     *  This method releases the token-pump capture obtained in CaptureTokenPump()
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object that received tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD ReleaseTokenPump(nsITagHandler* aHandler);


    /**
     * 
     * @update	gpk 06/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillResumeParse(void);

    /**
     * 
     * @update	gpk 06/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillInterruptParse(void);

    /**
     * 
     * @update	jevering 6/18/98
     * @param  aURLRef if the current URL reference (for debugger)
     * @return
     */
    virtual void SetURLRef(char * aURLRef);

    virtual  void EmitMisplacedContent(nsITokenizer* aTokenizer);

    /**
     * 
     * @update	jevering 6/18/98
     * @param  aParent  parent tag
     * @param  aChild   child tag
     * @return PR_TRUE if valid container
     */
    virtual PRBool CanContain(PRInt32 aParent, PRInt32 aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsContainer(PRInt32 aTag) const;

    /**
     * Called by the parser to initiate dtd verification of the
     * internal context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual PRBool Verify(nsString& aURLRef,nsIParser* aParser);

    /**
     * Set this to TRUE if you want the DTD to verify its
     * context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual void SetVerification(PRBool aEnable);

    /**
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gpk 06/18/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool CanOmit(eXIFTags aParent,eXIFTags aChild)const;

    /**
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gpk 06/18/98
     *  @param   aParent -- tag type of parent
     *  @param   aChild -- tag type of child
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool CanOmitEndTag(eXIFTags aParent,eXIFTags aChild)const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gpk 06/18/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsXIFContainer(eXIFTags aTag) const;
    virtual PRBool IsHTMLContainer(eHTMLTags aTag) const;

    /**
     * This method does two things: 1st, help construct
     * our own internal model of the content-stack; and
     * 2nd, pass this message on to the sink.
     * @update  gpk 06/18/98
     * @param   aNode -- next node to be added to model
     * @return  TRUE if ok, FALSE if error
     */
    virtual eXIFTags GetDefaultParentTagFor(eXIFTags aTag) const;


    /**
     * This method gets called at various times by the parser
     * whenever we want to verify a valid context stack. This
     * method also gives us a hook to add debugging metrics.
     *
     * @update  gpk 06/18/98
     * @param   aStack[] array of ints (tokens)
     * @param   aCount number of elements in given array
     * @return  TRUE if stack is valid, else FALSE
     */
    virtual PRBool VerifyContextVector(void) const;

   
    /**
     * 
     * @update	gpk 06/18/98
     * @param 
     * @return
     */
    virtual nsresult DidOpenContainer(eXIFTags aTag,PRBool anExplicitOpen);    

    /**
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gpk 06/18/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(eXIFTags aContainer) const;


    /**
     * Retrieve the tag type of the topmost item on context vector stack
     * @update	gpk 06/18/98
     * @return  tag type (may be unknown)
     */
    virtual eXIFTags GetTopNode() const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gpk 06/18/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    virtual PRInt32 GetTopmostIndexOf(eXIFTags aTag) const;

    /**
     * 
     * @update	gpk 06/18/98
     * @param 
     * @return
     */
    virtual nsresult DidCloseContainer(eXIFTags aTag,PRBool anExplicitClosure);

    /**
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gpk 06/18/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleStartToken(CToken* aToken);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gpk 06/18/98
     * @param   aToken is the end token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gpk 06/18/98
     * @param   aToken is the entity token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gpk 06/18/98
     * @param   aToken is the comment token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleCommentToken(CToken* aToken);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gpk 06/18/98
     * @param   aToken is the attribute token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleAttributeToken(CToken* aToken);


    nsresult HandleWhiteSpaceToken(CToken* aToken);
    nsresult HandleTextToken(CToken* aToken);


private:

    /**
     * Causes token handlers to be registered for this parser.
     * DO NOT CALL THIS! IT'S DEPRECATED!
     * @update	gpk 06/18/98
     */
    void InitializeDefaultTokenHandlers();

   
    /**
     * DEPRECATED
     * @update	gpk 06/18/98
     */
    CTokenHandler* GetTokenHandler(eHTMLTokenTypes aType) const;

    /**
     * DEPRECATED
     * @update	gpk 06/18/98
     */
    CTokenHandler* AddTokenHandler(CTokenHandler* aHandler);

    /**
     * DEPRECATED
     * @update	gpk 06/18/98
     */
    void DeleteTokenHandlers(void);

    /**
     * This cover method opens the given node as a generic container in 
     * content sink.
     * @update	gpk 06/18/98
     * @param   generic container (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenContainer(const nsIParserNode& aNode);

    /**
     * This cover method causes a generic containre in the content-sink to be closed
     * @update	gpk 06/18/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseContainer(const nsIParserNode& aNode);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gpk 06/18/98
     * @param   aNode is leaf node to be added.
     * @return  TRUE if all went well -- FALSE otherwise.
     */
    nsresult AddLeaf(const nsIParserNode& aNode);

     /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gpk 06/18/98
     * @param   type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    nsresult CreateContextStackFor(eXIFTags aChildTag);

    /**
     * 
     * @update	gess12/28/98
     * @param 
     * @return
     */
    nsITokenizer* GetTokenizer(void);

    /**
     * Retrieve a ptr to the global token recycler...
     * @update	gess8/4/98
     * @return  ptr to recycler (or null)
     */
    virtual nsITokenRecycler* GetTokenRecycler(void);

private:

    void ProcessEncodeTag(const nsIParserNode& aNode);

    void BeginCSSStyleSheet(const nsIParserNode& aNode);
    void EndCSSStyleSheet(const nsIParserNode& aNode);

    void BeginCSSStyleRule(const nsIParserNode& aNode);
    void EndCSSStyleRule(const nsIParserNode& aNode);

    void AddCSSSelector(const nsIParserNode& aNode);
    void AddCSSDeclaration(const nsIParserNode& aNode);
    void BeginCSSDeclarationList(const nsIParserNode& aNode);
    void EndCSSDeclarationList(const nsIParserNode& aNode);

    
    void      AddAttribute(nsIParserNode& aNode);
    eHTMLTags GetHTMLTag(const nsString& aName);    
    void      PushHTMLTag(const eHTMLTags aTag, const nsString& aName);
    void      PopHTMLTag(eHTMLTags& aTag, nsString*& aName);

    PRBool    GetAttributePair(nsIParserNode& aNode, nsString& aKey, nsString& aValue);
    PRBool    GetAttribute(const nsIParserNode& aNode, const nsString& aKey, nsString& aValue);
    void      BeginStartTag(const nsIParserNode& aNode);
    eHTMLTags GetStartTag(const nsIParserNode& aNode, nsString& aName);
    void      AddEndTag(const nsIParserNode& aNode);

    PRBool          StartTopOfStack();
    nsIParserNode*  PeekNode();
    CToken*         PeekToken();
    void            PushNodeAndToken(nsString& aName);
    void            PopAndDelete();


protected:

    PRBool			CanContainFormElement(eXIFTags aParent,eXIFTags aChild) const;
		nsresult		CollectAttributes(nsCParserNode& aNode,PRInt32 aCount);
		nsresult		CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount);
    
    nsParser*             mParser;
    nsIHTMLContentSink*   mSink;

    CTokenHandler*        mTokenHandlers[eToken_last];

    PRBool                mLeafBits[100];
    eXIFTags              mContextStack[100];
    PRInt32               mContextStackPos;

    PRBool                mHasOpenForm;
    PRBool                mHasOpenMap;

    nsIDTDDebug*		      mDTDDebug;

    nsVoidArray           mNodeStack;
    nsVoidArray           mTokenStack;

    PRInt32               mHTMLStackPos;
    eHTMLTags             mHTMLTagStack[512];
    nsAutoString*         mHTMLNameStack[512];
    PRBool                mInContent;

    nsString              mBuffer;
    PRInt32               mMaxCSSSelectorWidth;
    PRInt32               mCSSDeclarationCount;
    PRInt32               mCSSSelectorCount;
    PRBool                mLowerCaseTags;
    PRBool                mLowerCaseAttributes;
    nsITokenizer*         mTokenizer;
};


extern NS_HTMLPARS nsresult NS_NewXIFDTD(nsIDTD** aInstancePtrResult);


#endif 



