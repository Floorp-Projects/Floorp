/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * @update  gess 7/15/98
 *
 * NavDTD is an implementation of the nsIDTD interface.
 * In particular, this class captures the behaviors of the original 
 * Navigator parser productions.
 *
 * This DTD, like any other in NGLayout, provides a few basic services:
 *	- First, the DTD collaborates with the Parser class to convert plain 
 *    text into a sequence of HTMLTokens. 
 *	- Second, the DTD describes containment rules for known elements. 
 *	- Third the DTD controls and coordinates the interaction between the
 *	  parsing system and content sink. (The content sink is the interface
 *    that serves as a proxy for content model).
 *	- Fourth the DTD maintains an internal style-stack to handle residual (leaky)
 *	  style tags.
 *
 * You're most likely working in this class file because
 * you want to add or change a behavior inherent in this DTD. The remainder
 * of this section will describe what you need to do to affect the kind of
 * change you want in this DTD.
 *
 * RESIDUAL-STYLE HANDLNG:
 *	 There are a number of ways to represent style in an HTML document.
 *		1) explicit style tags (<B>, <I> etc)
 *		2) implicit styles (like those implicit in <Hn>)
 *		3) CSS based styles
 *
 *	 Residual style handling results from explicit style tags that are
 *	 not closed. Consider this example: <p>text <b>bold </p>
 *	 When the <p> tag closes, the <b> tag is NOT automatically closed.
 *	 Unclosed style tags are handled by the process we call residual-style 
 *	 tag handling. 
 *
 *	 There are two aspects to residual style tag handling. The first is the 
 *	 construction and managing of a stack of residual style tags. The 
 *	 second is the automatic emission of residual style tags onto leaf content
 *	 in subsequent portions of the document.This step is necessary to propagate
 *	 the expected style behavior to subsequent portions of the document.
 *
 *	 Construction and managing the residual style stack is an inline process that
 *	 occurs during the model building phase of the parse process. During the model-
 *	 building phase of the parse process, a content stack is maintained which tracks
 *	 the open container hierarchy. If a style tag(s) fails to be closed when a normal
 *	 container is closed, that style tag is placed onto the residual style stack. If
 *	 that style tag is subsequently closed (in most contexts), it is popped off the
 *	 residual style stack -- and are of no further concern.
 *
 *	 Residual style tag emission occurs when the style stack is not empty, and leaf
 *	 content occurs. In our earlier example, the <b> tag "leaked" out of the <p> 
 *	 container. Just before the next leaf is emitted (in this or another container) the 
 *	 style tags that are on the stack are emitted in succession. These same residual
 *   style tags get closed automatically when the leaf's container closes, or if a
 *   child container is opened.
 * 
 *         
 */
#ifndef NS_NAVHTMLDTD__
#define NS_NAVHTMLDTD__

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsIParser.h"
#include "nsHTMLTags.h"
#include "nsVoidArray.h"
#include "nsDeque.h"
#include "nsParserCIID.h"
#include "nsTime.h"
#include "nsDTDUtils.h"

#define NS_INAVHTML_DTD_IID      \
  {0x5c5cce40, 0xcfd6,  0x11d1,  \
  {0xaa, 0xda, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class nsIHTMLContentSink;
class nsIParserNode;
class nsParser;
class nsDTDContext;
class nsEntryStack;
class nsITokenizer;
class nsCParserNode;
class nsTokenAllocator;

/***************************************************************
  Now the main event: CNavDTD.

  This not so simple class performs all the duties of token 
  construction and model building. It works in conjunction with
  an nsParser.
 ***************************************************************/

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif

class CNavDTD : public nsIDTD
{

#ifdef _MSC_VER
#pragma warning( default : 4275 )
#endif

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDTD
     
    /**
      *  Common constructor for navdtd. You probably want to call
      *  NS_NewNavHTMLDTD().
      *
      *  @update  gess 7/9/98
      */
    CNavDTD();
    virtual ~CNavDTD();
    

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanPropagate(eHTMLTags aParent,
                                eHTMLTags aChild,
                                PRBool aParentContains) ;

    /**
     *  This method gets called to determine whether a given 
     *  child tag can be omitted by the given parent.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- parent tag being asked about omitting given child
     *  @param   aChild -- child tag being tested for omittability by parent
     *  @param   aParentContains -- can be 0,1,-1 (false,true, unknown)
     *  @return  PR_TRUE if given tag can be omitted
     */
    virtual PRBool CanOmit(eHTMLTags aParent, 
                           eHTMLTags aChild,
                           PRBool& aParentContains);

    /**
     * This method tries to design a context map (without actually
     * changing our parser state) from the parent down to the
     * child. 
     *
     * @update  gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  True if closure was achieved -- other false
     */
    virtual PRBool ForwardPropagate(nsString& aSequence,
                                    eHTMLTags aParent,
                                    eHTMLTags aChild);

    /**
     * This method tries to design a context map (without actually
     * changing our parser state) from the child up to the parent.
     *
     * @update  gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  True if closure was achieved -- other false
     */
    virtual PRBool BackwardPropagate(nsString& aSequence,
                                     eHTMLTags aParent,
                                     eHTMLTags aChild) const;

    /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gess5/11/98
     * @param   aChild -- type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    nsresult CreateContextStackFor(eHTMLTags aChild);

    /**
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gess5/11/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(eHTMLTags aContainer) const;

    /**
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gess5/11/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(const eHTMLTags aTagSet[],PRInt32 aCount) const;

    /**
     * Accessor that retrieves the tag type of the topmost item on context 
	   * vector stack.
	   *
     * @update	gess5/11/98
     * @return  tag type (may be unknown)
     */
    virtual eHTMLTags GetTopNode() const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gess5/11/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    // virtual PRInt32 GetTopmostIndexOf(eHTMLTags aTag) const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gess5/11/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    virtual PRInt32 LastOf(eHTMLTags aTagSet[],PRInt32 aCount) const;

    /**
     * The following set of methods are used to partially construct 
     * the content model (via the sink) according to the type of token.
     * @update	gess5/11/98
     * @param   aToken is the token (of a given type) to be handled
     * @return  error code representing construction state; usually 0.
     */
    nsresult    HandleStartToken(CToken* aToken);
    nsresult    HandleDefaultStartToken(CToken* aToken, eHTMLTags aChildTag,
                                        nsCParserNode *aNode);
    nsresult    HandleEndToken(CToken* aToken);
    nsresult    HandleEntityToken(CToken* aToken);
    nsresult    HandleCommentToken(CToken* aToken);
    nsresult    HandleAttributeToken(CToken* aToken);
    nsresult    HandleScriptToken(const nsIParserNode *aNode);
    nsresult    HandleProcessingInstructionToken(CToken* aToken);
    nsresult    HandleDocTypeDeclToken(CToken* aToken);
    nsresult    BuildNeglectedTarget(eHTMLTags aTarget, eHTMLTokenTypes aType,
                                     nsIParser* aParser, nsIContentSink* aSink);

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
    nsresult OpenHTML(const nsCParserNode *aNode);
    nsresult OpenHead(const nsIParserNode *aNode);
    nsresult OpenBody(const nsCParserNode *aNode);
    nsresult OpenForm(const nsIParserNode *aNode);
    nsresult OpenMap(const nsCParserNode *aNode);
    nsresult OpenFrameset(const nsCParserNode *aNode);
    nsresult OpenContainer(const nsCParserNode *aNode,
                           eHTMLTags aTag,
                           PRBool aClosedByStartTag,
                           nsEntryStack* aStyleStack=0);

    /**
     * The next set of methods close the given HTML element.
     * 
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  error code - 0 if all went well.
     */
    nsresult CloseHTML();
    nsresult CloseHead();
    nsresult CloseBody();
    nsresult CloseForm();
    nsresult CloseMap();
    nsresult CloseFrameset();
    
    /**
     * The special purpose methods automatically close
     * one or more open containers.
     * @update	gess5/11/98
     * @return  error code - 0 if all went well.
     */
    nsresult CloseContainer(const eHTMLTags aTag,
                            eHTMLTags aTarget,
                            PRBool aClosedByStartTag);
    nsresult CloseContainersTo(eHTMLTags aTag,
                               PRBool aClosedByStartTag);
    nsresult CloseContainersTo(PRInt32 anIndex,
                               eHTMLTags aTag,
                               PRBool aClosedByStartTag);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  error code - 0 if all went well.
     */
    nsresult AddLeaf(const nsIParserNode *aNode);
    nsresult AddHeadLeaf(nsIParserNode *aNode);

    /**
     * This set of methods is used to create and manage the set of
	   * transient styles that occur as a result of poorly formed HTML
   	 * or bugs in the original navigator.
	   *
     * @update	gess5/11/98
     * @param   aTag -- represents the transient style tag to be handled.
     * @return  error code -- usually 0
     */
    nsresult  OpenTransientStyles(eHTMLTags aChildTag);
    nsresult  CloseTransientStyles(eHTMLTags aChildTag);
    nsresult  PopStyle(eHTMLTags aTag);

    nsresult  PushIntoMisplacedStack(CToken* aToken)
    {
      NS_ENSURE_ARG_POINTER(aToken);
      aToken->SetNewlineCount(0); // Note: We have already counted the newlines for these tokens

      mMisplacedContent.Push(aToken);
      return NS_OK;
    }

protected:

    nsresult        CollectAttributes(nsIParserNode* aNode,eHTMLTags aTag,PRInt32 aCount);
    nsresult        CollectSkippedContent(nsIParserNode& aNode,PRInt32& aCount);
    nsresult        WillHandleStartTag(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);
    nsresult        DidHandleStartTag(nsIParserNode& aNode,eHTMLTags aChildTag);
    nsresult        HandleOmittedTag(CToken* aToken,eHTMLTags aChildTag,eHTMLTags aParent,nsIParserNode *aNode);
    nsresult        HandleSavedTokens(PRInt32 anIndex);
    nsresult        HandleKeyGen(nsIParserNode *aNode);
    
    nsDeque             mMisplacedContent;
    nsDeque             mSkippedContent;
    
    nsIHTMLContentSink* mSink;
    nsTokenAllocator*   mTokenAllocator;
    nsDTDContext*       mBodyContext;
    nsDTDContext*       mTempContext;
    nsParser*           mParser;
    nsITokenizer*       mTokenizer; // weak
   
    nsString            mFilename; 
    nsString            mScratch;  //used for various purposes; non-persistent
    nsCString           mMimeType;

    nsNodeAllocator     mNodeAllocator;
    nsDTDMode           mDTDMode;
    eParserDocType      mDocType;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...

    eHTMLTags           mSkipTarget;
    PRInt32             mLineNumber;
    PRInt32             mOpenMapCount;

    PRUint16            mFlags;

#ifdef ENABLE_CRC
    PRUint32            mComputedCRC32;
    PRUint32            mExpectedCRC32;
#endif
};

inline nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult)
{
  NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
  return nsComponentManager::CreateInstance(kNavDTDCID,
                                            nsnull,
                                            NS_GET_IID(nsIDTD),
                                            (void**)aInstancePtrResult);
}

#endif 



