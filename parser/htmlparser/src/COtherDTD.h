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

#ifndef NS_OTHERHTMLDTD__
#define NS_OTHERHTMLDTD__

#include "CNavDTD.h"


#define NS_IOtherHTML_DTD_IID      \
  {0x8a5e89c0, 0xd16d,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x8, 0x14, 0x98, 0x89}}


//class nsParser;
//class nsIHTMLContentSink;

class COtherDTD : public CNavDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    COtherDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~COtherDTD();

    virtual const nsIID&  GetMostDerivedIID(void) const;

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
     * @update	gess6/24/98
     * @param   
     * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
     */
    virtual eAutoDetectResult CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion);
    

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillResumeParse(void);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillInterruptParse(void);

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool CanOmit(eHTMLTags aParent,eHTMLTags aChild,PRBool& aParentContains) ;

    /**
     * Give rest of world access to our tag enums, so that CanContain(), etc,
     * become useful.
     */
    NS_IMETHOD StringTagToIntTag(nsString &aTag, PRInt32* aIntTag)const;

    NS_IMETHOD IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const;

    NS_IMETHOD ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const;

    /**
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleStartToken(CToken* aToken);

    /**
     * This method gets called when a start token has been consumed, and
     * we want to use default start token handling behavior.
     * This method gets called automatically by handleStartToken.
     *
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @param   aChildTag is the tag-type of given token
     * @param   aNode is a node be updated with info from given token
     * @return  TRUE if the token was handled.
     */
    nsresult HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode *aNode);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the end token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the entity token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the comment token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleCommentToken(CToken* aToken);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the attribute token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleAttributeToken(CToken* aToken);

    /**
     * This method gets called when a script token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the script token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleScriptToken(const nsIParserNode *aNode);
    
    /**
     * This method gets called when a style token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the style token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleStyleToken(CToken* aToken);

private:


    //*************************************************
    //these cover methods mimic the sink, and are used
    //by the parser to manage its context-stack.
    //*************************************************

    /**
     * The next set of method open given HTML elements of
	   * various types.
     * 
     * @update	gess5/11/98
     * @param   node to be opened in content sink.
     * @return  error code representing error condition-- usually 0.
     */
    nsresult OpenHTML(const nsIParserNode *aNode);
    nsresult OpenHead(const nsIParserNode *aNode);
    nsresult OpenBody(const nsIParserNode *aNode);
    nsresult OpenForm(const nsIParserNode *aNode);
    nsresult OpenMap(const nsIParserNode *aNode);
    nsresult OpenFrameset(const nsIParserNode *aNode);
    nsresult OpenContainer(const nsIParserNode *aNode,eHTMLTags aTag,PRBool aUpdateStyleStack,nsEntryStack* aStyleStack=0);

    /**
     * The next set of methods close the given HTML element.
     * 
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  error code - 0 if all went well.
     */
    nsresult CloseHTML(const nsIParserNode *aNode);
    nsresult CloseHead(const nsIParserNode *aNode);
    nsresult CloseBody(const nsIParserNode *aNode);
    nsresult CloseForm(const nsIParserNode *aNode);
    nsresult CloseMap(const nsIParserNode *aNode);
    nsresult CloseFrameset(const nsIParserNode *aNode);

    nsresult CloseContainer(const nsIParserNode *aNode,eHTMLTags aTarget,PRBool aUpdateStyles);
    nsresult CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles);
    nsresult CloseContainersTo(PRInt32 anIndex,eHTMLTags aTarget,PRBool aUpdateStyles);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  TRUE if all went well -- FALSE otherwise.
     */
    nsresult AddLeaf(const nsIParserNode *aNode);


    /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gess5/11/98
     * @param   type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    nsresult CreateContextStackFor(eHTMLTags aChildTag);

    nsresult OpenTransientStyles(eHTMLTags aTag);
    nsresult CloseTransientStyles(eHTMLTags aTag);
    nsresult PopStyle(eHTMLTags aTag);

    nsresult DoFragment(PRBool aFlag);

protected:

    
};

extern NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult);

#endif 
