/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsIHTMLContentSink_h___
#define nsIHTMLContentSink_h___

/**
 * This file declares the concrete HTMLContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 *
 * After the tokenizer completes, the parser iterates over
 * the known token list. As the parser identifies valid 
 * elements, it calls the contentsink interface to notify
 * the content model that a new node or child node is being
 * created and added to the content model.
 *
 * The HTMLContentSink interface assumes 4 underlying
 * containers: HTML, HEAD, BODY and FRAMESET. Before 
 * accessing any these, the parser will call the appropriate
 * OpennsIHTMLContentSink method: OpenHTML,OpenHead,OpenBody,OpenFrameSet;
 * likewise, the ClosensIHTMLContentSink version will be called when the
 * parser is done with a given section.
 *
 * IMPORTANT: The parser may Open each container more than
 * once! This is due to the irregular nature of HTML files.
 * For example, it is possible to encounter plain text at
 * the start of an HTML document (that preceeds the HTML tag).
 * Such text is treated as if it were part of the body.
 * In such cases, the parser will Open the body, pass the text-
 * node in and then Close the body. The body will likely be
 * re-Opened later when the actual <BODY> tag has been seen.
 *
 * Containers within the body are Opened and Closed
 * using the OpenContainer(...) and CloseContainer(...) calls.
 * It is assumed that the document or contentSink is 
 * maintaining its state to manage where new content should 
 * be added to the underlying document.
 *
 * NOTE: OpenHTML() and OpenBody() may get called multiple times
 *       in the same document. That's fine, and it doesn't mean
 *       that we have multiple bodies or HTML's.
 *
 * NOTE: I haven't figured out how sub-documents (non-frames)
 *       are going to be handled. Stay tuned.
 */
#include "nsIParserNode.h"
#include "nsIContentSink.h"
#include "nsHTMLTags.h"

// d19e6730-5e2f-4131-89db-8a918515097d
#define NS_IHTML_CONTENT_SINK_IID \
{ 0xd19e6730, 0x5e2f, 0x4131, \
  { 0x89, 0xdb, 0x8a, 0x91, 0x85, 0x15, 0x09, 0x7d } }

#define MAX_REFLOW_DEPTH  200

class nsIHTMLContentSink : public nsIContentSink 
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTML_CONTENT_SINK_IID)

  /**
   * This method is used to open the HEAD container. It is useful if a tag
   * is forcing us to open the head (probably again), like if we find a <meta>
   * tag in the body.
   */
  NS_IMETHOD OpenHead() = 0;

  /**
   * This gets called when handling illegal contents, especially
   * in dealing with tables. This method creates a new context.
   * 
   * @update 04/04/99 harishd
   * @param aPosition - The position from where the new context begins.
   */
  NS_IMETHOD BeginContext(PRInt32 aPosition) = 0;
  
  /**
   * This method terminates any new context that got created by
   * BeginContext and switches back to the main context.  
   *
   * @update 04/04/99 harishd
   * @param aPosition - Validates the end of a context.
   */
  NS_IMETHOD EndContext(PRInt32 aPosition) = 0;
  
  /**
   * @update 01/09/2003 harishd
   * @param aTag - Check if this tag is enabled or not.
   */
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn) = 0;

  /**
   * This method is called when parser has
   * completed processing a chunk of tokens. The processing of the
   * tokens may be interrupted by returning NS_ERROR_HTMLPARSER_INTERRUPTED from
   * DidProcessAToken.
   */
  NS_IMETHOD DidProcessTokens() = 0;

  /**
   * This method is called when parser is about to
   * process a single token
   */
  NS_IMETHOD WillProcessAToken(void) = 0;

  /**
   * This method is called when parser has completed
   * the processing for a single token.
   * @return NS_OK if processing should not be interrupted
   *         NS_ERROR_HTMLPARSER_INTERRUPTED if the parsing should be interrupted
   */
  NS_IMETHOD DidProcessAToken(void) = 0;

    /**
   * This method is used to open a generic container in the sink.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode) = 0;

  /**
   *  This method gets called by the parser when a close
   *  container tag has been consumed and needs to be closed.
   *
   * @param  aTag - The tag to be closed.
   */     
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag) = 0;

  /**
   * This method is used when we're closing a tag that was malformed
   * in some way. This way, the content sink can do special processing
   * (e.g., not execute a malformed script tag).
   *
   * @param aTag The tag to be closed.
   */
  NS_IMETHOD CloseMalformedContainer(const nsHTMLTag aTag)
  {
    return CloseContainer(aTag);
  }

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddComment(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode) = 0;

  /**
   * This method is called by the parser when it encounters
   * a document type declaration.
   *
   * XXX Should the parser also parse the internal subset?
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser to notify observers of
   * the tag
   *
   * @param aErrorResult the error code
   */
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) = 0;

  /**
   * Call this method to determnine if a FORM is on the sink's stack
   *
   * @return PR_TRUE if found else PR_FALSE
   */
  NS_IMETHOD_(PRBool) IsFormOnStack() = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLContentSink, NS_IHTML_CONTENT_SINK_IID)

#endif /* nsIHTMLContentSink_h___ */

