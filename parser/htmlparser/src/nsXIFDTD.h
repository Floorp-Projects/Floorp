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
#include "nsVoidArray.h"
#include "nsParserCIID.h"
#include "nsHTMLTags.h"


class nsParser;
class nsCParserNode;
class CTokenHandler;
class nsIDTDDebug;
class nsIHTMLContentSink;
class nsITokenizer;
class nsDTDContext;
class nsEntryStack;
class nsCParserNode;
class nsTokenAllocator;
class CNodeRecycler;
class CToken;

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
  eXIFTag_document_info,
  eXIFTag_encode, 
  eXIFTag_entity,
  eXIFTag_import,
  eXIFTag_leaf,
  eXIFTag_link,
  eXIFTag_section,
  eXIFTag_section_body,
  eXIFTag_section_head,
  eXIFTag_stylelist,
  eXIFTag_url,    
  eXIFTag_xml,    
  
  eXIFTag_markupDecl = eHTMLTag_markupDecl,
  eXIFTag_newline    = eHTMLTag_newline,
  eXIFTag_text       = eHTMLTag_text,
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
    virtual eAutoDetectResult CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	rickg 03.20.2000
      * @param	aParserContext
      * @param	aSink
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink);

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
     * Use this id you want to stop the building content model
     * --------------[ Sets DTD to STOP mode ]----------------
     * It's recommended to use this method in accordance with
     * the parser's terminate() method.
     */
    virtual nsresult  Terminate(nsIParser* aParser=nsnull);

    /**
     * Give rest of world access to our tag enums, so that CanContain(), etc,
     * become useful.
     */
    NS_IMETHOD StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const;

    NS_IMETHOD IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const;

    NS_IMETHOD ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const;

    virtual PRBool  IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const;
    virtual PRBool  IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const;

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
     *  tag is itself a container
     *  
     */
    virtual PRBool IsHTMLContainer(eHTMLTags aTag) const;

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
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     */
    nsresult HandleStartToken(CToken* aToken);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     */
    nsresult HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     */
    nsresult HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     */
    nsresult HandleCommentToken(CToken* aToken,nsIParserNode& aNode);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     */

    nsresult HandleAttributeToken(CToken* aToken,nsIParserNode& aNode);

    nsresult HandleContainer(nsIParserNode& aNode);
    nsresult HandleDefaultToken(CToken* aToken,nsIParserNode& aNode);
    PRBool   CanHandleDefaultTag(eXIFTags aTag, PRInt32& aIsContainer);

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
     * 
     * @update	gess 12/20/99
     * @param   ptr-ref to (out) tokenizer
     * @return  nsresult
     */
    NS_IMETHOD  GetTokenizer(nsITokenizer*& aTokenizer);

    /**
     * Retrieve a ptr to the global token recycler...
     * @update	gess8/4/98
     * @return  ptr to recycler (or null)
     */
    virtual nsTokenAllocator* GetTokenAllocator(void);

private:

    nsresult ProcessEncodeTag(const nsIParserNode& aNode);
    nsresult ProcessEntityTag(const nsIParserNode& aNode);
    nsresult ProcessDocumentInfoTag(const nsIParserNode& aNode);

    nsresult BeginCSSStyleSheet(const nsIParserNode& aNode);
    nsresult EndCSSStyleSheet(const nsIParserNode& aNode);

    nsresult BeginCSSStyleRule(const nsIParserNode& aNode);
    nsresult EndCSSStyleRule(const nsIParserNode& aNode);

    nsresult AddCSSSelector(const nsIParserNode& aNode);
    nsresult AddCSSDeclaration(const nsIParserNode& aNode);
    nsresult BeginCSSDeclarationList(const nsIParserNode& aNode);
    nsresult EndCSSDeclarationList(const nsIParserNode& aNode);

    PRBool    GetAttributePair(nsIParserNode& aNode, nsString& aKey, nsString& aValue);
    PRBool    GetAttribute(const nsIParserNode& aNode, const nsString& aKey, nsString& aValue);

protected:

    nsresult    WillHandleToken(CToken* aToken,PRInt32& aType);
    nsresult    DidHandleToken(CToken* aToken, nsresult aResult=NS_OK);
    nsresult    WillHandleStartToken(CToken* aToken,eXIFTags aTag, nsIParserNode& aNode);
    nsresult    DidHandleStartToken(CToken* aToken,eXIFTags aTag, nsIParserNode& aNode);
    nsresult    PreprocessStack();
    PRBool			CanContainFormElement(eXIFTags aParent,eXIFTags aChild) const;
		nsresult		CollectAttributes(nsCParserNode& aNode,PRInt32 aCount);
		nsresult		CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount);

    nsParser*             mParser;
    nsIHTMLContentSink*   mSink;

    nsIDTDDebug*		      mDTDDebug;
    PRBool                mInContent;

    nsString              mBuffer;
    PRInt32               mMaxCSSSelectorWidth;
    PRInt32               mCSSDeclarationCount;
    PRInt32               mCSSSelectorCount;
    PRBool                mLowerCaseAttributes;
    nsITokenizer*         mTokenizer;
    nsString              mCharset;

    nsDTDContext*         mXIFContext;
    nsTokenAllocator*      mTokenAllocator;
    CNodeRecycler*        mNodeRecycler;
    nsresult              mDTDState;
    
    nsString              mContainerKey;
    nsString              mEncodeKey;
    nsString              mCSSStyleSheetKey;
    nsString              mCSSSelectorKey;
    nsString              mCSSDeclarationKey;
    nsString              mGenericKey;

    PRInt32               mLineNumber;
};


inline nsresult NS_NewXIFDTD(nsIDTD** aInstancePtrResult)
{
  NS_DEFINE_CID(kXIFDTDCID, NS_XIF_DTD_CID);
  return nsComponentManager::CreateInstance(kXIFDTDCID,
                                            nsnull,
                                            NS_GET_IID(nsIDTD),
                                            (void**)aInstancePtrResult);
}

#endif 



