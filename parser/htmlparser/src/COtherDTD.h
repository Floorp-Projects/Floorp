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
#ifndef NS_OTHERDTD__
#define NS_OTHERDTD__

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsIParser.h"
#include "nsHTMLTokens.h"
#include "nsVoidArray.h"
#include "nsDeque.h"
#include "nsParserCIID.h"

#define NS_IOTHERHTML_DTD_IID      \
  {0x8a5e89c0, 0xd16d,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x8, 0x14, 0x98, 0x89}}

class nsIHTMLContentSink;
class nsIParserNode;
class nsParser;
class nsDTDContext;
class nsEntryStack;
class nsITokenizer;
class nsIParserNode;
class nsTokenAllocator;
class nsNodeAllocator;

/***************************************************************
  Now the main event: COtherDTD.

  This not so simple class performs all the duties of token 
  construction and model building. It works in conjunction with
  an nsParser.
 ***************************************************************/

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif

class COtherDTD : public nsIDTD
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
    COtherDTD();

    /**
     *  Virtual destructor -- you know what to do
     *  @update  gess 7/9/98
     */
    virtual ~COtherDTD();

    /**
     * The following set of methods are used to partially construct 
     * the content model (via the sink) according to the type of token.
     * @update	gess5/11/98
     * @param   aToken is the token (of a given type) to be handled
     * @return  error code representing construction state; usually 0.
     */
    nsresult    HandleStartToken(CToken* aToken);
    nsresult    HandleEndToken(CToken* aToken);
    nsresult    HandleEntityToken(CToken* aToken);

    //*************************************************
    //these cover methods mimic the sink, and are used
    //by the parser to manage its context-stack.
    //*************************************************
    
protected:

		nsresult        CollectAttributes(nsIParserNode& aNode,eHTMLTags aTag,PRInt32 aCount);
    nsresult        WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsIParserNode& aNode);
    nsresult        DidHandleStartTag(nsIParserNode& aNode,eHTMLTags aChildTag);
    nsIParserNode*  CreateNode(CToken* aToken=nsnull,PRInt32 aLineNumber=1,nsTokenAllocator* aTokenAllocator=0);

    nsIHTMLContentSink* mSink;

    nsDTDContext*       mBodyContext;
    PRInt32             mHasOpenHead;
    PRPackedBool        mHasOpenForm;
    PRPackedBool        mHasOpenMap;
    PRPackedBool        mHasOpenBody;
    PRPackedBool        mHadFrameset;
    PRPackedBool        mHadBody;
    PRPackedBool        mHasOpenScript;
    PRPackedBool        mEnableStrict;
    nsString            mFilename;
    PRInt32             mLineNumber;
    nsParser*           mParser;
    nsITokenizer*       mTokenizer; // weak
    nsTokenAllocator*   mTokenAllocator;
    nsNodeAllocator*    mNodeAllocator;
    eHTMLTags           mSkipTarget;
    nsresult            mDTDState;
    nsDTDMode           mDTDMode;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...

    PRUint32            mComputedCRC32;
    PRUint32            mExpectedCRC32;
    nsString            mScratch;  //used for various purposes; non-persistent
    eParserDocType      mDocType;

};

extern nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult);


class CTransitionalDTD : public COtherDTD
{
  public:
    CTransitionalDTD();
    virtual ~CTransitionalDTD();
};

#endif //NS_OTHERDTD__



