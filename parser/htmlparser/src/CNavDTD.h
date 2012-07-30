/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * MODULE NOTES:
 *
 * NavDTD is an implementation of the nsIDTD interface.
 * In particular, this class captures the behaviors of the original 
 * Navigator parser productions.
 *
 * This DTD, like any other in NGLayout, provides a few basic services:
 *  - First, the DTD collaborates with the Parser class to convert plain 
 *    text into a sequence of HTMLTokens. 
 *  - Second, the DTD describes containment rules for known elements. 
 *  - Third the DTD controls and coordinates the interaction between the
 *    parsing system and content sink. (The content sink is the interface
 *    that serves as a proxy for content model).
 *  - Fourth the DTD maintains an internal style-stack to handle residual (leaky)
 *    style tags.
 *
 * You're most likely working in this class file because
 * you want to add or change a behavior inherent in this DTD. The remainder
 * of this section will describe what you need to do to affect the kind of
 * change you want in this DTD.
 *
 * RESIDUAL-STYLE HANDLNG:
 *   There are a number of ways to represent style in an HTML document.
 *    1) explicit style tags (<B>, <I> etc)
 *    2) implicit styles (like those implicit in <Hn>)
 *    3) CSS based styles
 *
 *   Residual style handling results from explicit style tags that are
 *   not closed. Consider this example: <p>text <b>bold </p>
 *   When the <p> tag closes, the <b> tag is NOT automatically closed.
 *   Unclosed style tags are handled by the process we call residual-style 
 *   tag handling. 
 *
 *   There are two aspects to residual style tag handling. The first is the 
 *   construction and managing of a stack of residual style tags. The 
 *   second is the automatic emission of residual style tags onto leaf content
 *   in subsequent portions of the document.This step is necessary to propagate
 *   the expected style behavior to subsequent portions of the document.
 *
 *   Construction and managing the residual style stack is an inline process that
 *   occurs during the model building phase of the parse process. During the model-
 *   building phase of the parse process, a content stack is maintained which tracks
 *   the open container hierarchy. If a style tag(s) fails to be closed when a normal
 *   container is closed, that style tag is placed onto the residual style stack. If
 *   that style tag is subsequently closed (in most contexts), it is popped off the
 *   residual style stack -- and are of no further concern.
 *
 *   Residual style tag emission occurs when the style stack is not empty, and leaf
 *   content occurs. In our earlier example, the <b> tag "leaked" out of the <p> 
 *   container. Just before the next leaf is emitted (in this or another container) the 
 *   style tags that are on the stack are emitted in succession. These same residual
 *   style tags get closed automatically when the leaf's container closes, or if a
 *   child container is opened.
 * 
 *         
 */
#ifndef NS_NAVHTMLDTD__
#define NS_NAVHTMLDTD__

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsHTMLTags.h"
#include "nsDeque.h"
#include "nsParserCIID.h"
#include "nsDTDUtils.h"
#include "nsParser.h"
#include "nsCycleCollectionParticipant.h"

class nsIHTMLContentSink;
class nsIParserNode;
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
    /**
     *  Common constructor for navdtd. You probably want to call
     *  NS_NewNavHTMLDTD().
     */
    CNavDTD();
    virtual ~CNavDTD();

    /**
     * This method is offered publically for the sole use from
     * nsParser::ParseFragment. In general, you should prefer to use methods
     * that are directly on nsIDTD, since those will be guaranteed to do the
     * right thing.
     *
     * @param aNode The parser node that contains the token information for
     *              this tag.
     * @param aTag The actual tag that is being opened (should correspond to
     *             aNode.
     * @param aStyleStack The style stack that aNode might be a member of
     *                    (usually null).
     */
    nsresult OpenContainer(const nsCParserNode *aNode,
                           eHTMLTags aTag,
                           nsEntryStack* aStyleStack = nullptr);

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIDTD
    NS_DECL_CYCLE_COLLECTION_CLASS(CNavDTD)

private:
    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @param   aParent Tag of parent container
     *  @param   aChild Tag of child container
     *  @return  true if parent can contain child
     */
    bool CanPropagate(eHTMLTags aParent,
                        eHTMLTags aChild,
                        PRInt32 aParentContains);

    /**
     *  This method gets called to determine whether a given 
     *  child tag can be omitted by the given parent.
     *  
     *  @param   aParent Parent tag being asked about omitting given child
     *  @param   aChild Child tag being tested for omittability by parent
     *  @param   aParentContains Can be 0,1,-1 (false,true, unknown)
     *                           XXX should be PRInt32, not bool
     *  @return  true if given tag can be omitted
     */
    bool CanOmit(eHTMLTags aParent, 
                   eHTMLTags aChild,
                   PRInt32& aParentContains);

    /**
     * Looking at aParent, try to see if we can propagate from aChild to
     * aParent. If aParent is a TR tag, then see if we can start at TD instead
     * of at aChild.
     *
     * @param   aParent Tag type of parent
     * @param   aChild Tag type of child
     * @return  true if closure was achieved -- otherwise false
     */
    bool ForwardPropagate(nsString& aSequence,
                            eHTMLTags aParent,
                            eHTMLTags aChild);

    /**
     * Given aParent that does not contain aChild, starting with aChild's
     * first root tag, try to find aParent. If we can reach aParent simply by
     * going up each first root tag, then return true. Otherwise, we could not
     * propagate from aChild up to aParent, so return false.
     *
     * @param   aParent Tag type of parent
     * @param   aChild Tag type of child
     * @return  true if closure was achieved -- other false
     */
    bool BackwardPropagate(nsString& aSequence,
                             eHTMLTags aParent,
                             eHTMLTags aChild) const;

    /**
     * Attempt forward and/or backward propagation for the given child within
     * the current context vector stack. And actually open the required tags.
     *
     * @param   aParent The tag we're trying to open this element inside of.
     * @param   aChild Type of child to be propagated.
     */
    void CreateContextStackFor(eHTMLTags aParent, eHTMLTags aChild);

    /**
     * Ask if a given container is open anywhere on its stack
     *
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    bool HasOpenContainer(eHTMLTags aContainer) const;

    /**
     * This method allows the caller to determine if a any member
     * in a set of tags is currently open.
     *
     * @param   aTagSet A set of tags you care about.
     * @return  true if any of the members of aTagSet are currently open.
     */
    bool HasOpenContainer(const eHTMLTags aTagSet[], PRInt32 aCount) const;

    /**
     * Accessor that retrieves the tag type of the topmost item on the DTD's
     * tag stack.
     *
     * @return The tag type (may be unknown)
     */
    eHTMLTags GetTopNode() const;

    /**
     * Finds the topmost occurrence of given tag within context vector stack.
     *
     * @param   tag to be found
     * @return  index of topmost tag occurrence -- may be -1 (kNotFound).
     */
    PRInt32 LastOf(eHTMLTags aTagSet[], PRInt32 aCount) const;

    nsresult HandleToken(CToken* aToken);

    /**
     *  This method gets called when a start token has been
     *  encountered in the parse process. If the current container
     *  can contain this tag, then add it. Otherwise, you have
     *  two choices: 1) create an implicit container for this tag
     *                  to be stored in
     *               2) close the top container, and add this to
     *                  whatever container ends up on top.
     *
     *  @param   aToken -- next (start) token to be handled
     *  @return  Whether or not we should block the parser.
     */
    nsresult    HandleStartToken(CToken* aToken);

    /** 
     *  This method gets called when a start token has been 
     *  encountered in the parse process. If the current container
     *  can contain this tag, then add it. Otherwise, you have
     *  two choices: 1) create an implicit container for this tag
     *                  to be stored in
     *               2) close the top container, and add this to
     *                  whatever container ends up on top.
     *   
     *  @param   aToken Next (start) token to be handled.
     *  @param   aChildTag The tag corresponding to aToken.
     *  @param   aNode CParserNode representing this start token
     *  @return  A potential request to block the parser.
     */
    nsresult    HandleDefaultStartToken(CToken* aToken, eHTMLTags aChildTag,
                                        nsCParserNode *aNode);
    nsresult    HandleEndToken(CToken* aToken);
    nsresult    HandleEntityToken(CToken* aToken);
    nsresult    HandleCommentToken(CToken* aToken);
    nsresult    HandleAttributeToken(CToken* aToken);
    nsresult    HandleProcessingInstructionToken(CToken* aToken);
    nsresult    HandleDocTypeDeclToken(CToken* aToken);
    nsresult    BuildNeglectedTarget(eHTMLTags aTarget, eHTMLTokenTypes aType);

    nsresult OpenHTML(const nsCParserNode *aNode);
    nsresult OpenBody(const nsCParserNode *aNode);

    /**
     * The special purpose methods automatically close
     * one or more open containers.
     * @return  error code - 0 if all went well.
     */
    nsresult CloseContainer(const eHTMLTags aTag, bool aMalformed);
    nsresult CloseContainersTo(eHTMLTags aTag, bool aClosedByStartTag);
    nsresult CloseContainersTo(PRInt32 anIndex, eHTMLTags aTag,
                               bool aClosedByStartTag);
    nsresult CloseResidualStyleTags(const eHTMLTags aTag,
                                    bool aClosedByStartTag);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @param   aNode is leaf node to be added.
     * @return  error code - 0 if all went well.
     */
    nsresult AddLeaf(const nsIParserNode *aNode);
    nsresult AddHeadContent(nsIParserNode *aNode);

    /**
     * This set of methods is used to create and manage the set of
     * transient styles that occur as a result of poorly formed HTML
     * or bugs in the original navigator.
     *
     * @param   aTag -- represents the transient style tag to be handled.
     * @return  error code -- usually 0
     */
    nsresult  OpenTransientStyles(eHTMLTags aChildTag,
                                  bool aCloseInvalid = true);
    void      PopStyle(eHTMLTags aTag);

    nsresult  PushIntoMisplacedStack(CToken* aToken)
    {
      NS_ENSURE_ARG_POINTER(aToken);
      aToken->SetNewlineCount(0); // Note: We have already counted the newlines for these tokens

      mMisplacedContent.Push(aToken);
      return NS_OK;
    }

protected:

    nsresult        CollectAttributes(nsIParserNode* aNode,eHTMLTags aTag,PRInt32 aCount);

    /**
     * This gets called before we've handled a given start tag.
     * It's a generic hook to let us do pre processing.
     *
     * @param   aToken contains the tag in question
     * @param   aTag is the tag itself.
     * @param   aNode is the node (tag) with associated attributes.
     * @return  NS_OK if we should continue, a failure code otherwise.
     */
    nsresult        WillHandleStartTag(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);
    nsresult        DidHandleStartTag(nsIParserNode& aNode,eHTMLTags aChildTag);

    /**
     *  This method gets called when a start token has been encountered that
     *  the parent wants to omit. It stashes it in the current element if that
     *  element accepts such misplaced tokens.
     *
     *  @param   aToken Next (start) token to be handled
     *  @param   aChildTag id of the child in question
     *  @param   aParent id of the parent in question
     *  @param   aNode CParserNode representing this start token
     */
    void            HandleOmittedTag(CToken* aToken, eHTMLTags aChildTag,
                                     eHTMLTags aParent, nsIParserNode *aNode);
    nsresult        HandleSavedTokens(PRInt32 anIndex);
    nsresult        HandleKeyGen(nsIParserNode *aNode);
    bool            IsAlternateTag(eHTMLTags aTag);
    bool            IsBlockElement(PRInt32 aTagID, PRInt32 aParentID) const;
    bool            IsInlineElement(PRInt32 aTagID, PRInt32 aParentID) const;

    nsDeque             mMisplacedContent;
    
    nsCOMPtr<nsIHTMLContentSink> mSink;
    nsTokenAllocator*   mTokenAllocator;
    nsDTDContext*       mBodyContext;
    nsDTDContext*       mTempContext;
    bool                mCountLines;
    nsITokenizer*       mTokenizer; // weak
   
    nsString            mFilename; 
    nsString            mScratch;  //used for various purposes; non-persistent
    nsCString           mMimeType;

    nsNodeAllocator     mNodeAllocator;
    nsDTDMode           mDTDMode;
    eParserDocType      mDocType;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...

    PRInt32             mLineNumber;
    PRInt32             mOpenMapCount;
    PRInt32             mHeadContainerPosition;

    PRUint16            mFlags;
};

#endif 



