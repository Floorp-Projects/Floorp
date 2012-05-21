/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIHTMLContentSink_h___
#define nsIHTMLContentSink_h___

/**
 * This interface is OBSOLETE and in the process of being REMOVED.
 * Do NOT implement!
 *
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
 * the start of an HTML document (that precedes the HTML tag).
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

#define NS_IHTML_CONTENT_SINK_IID \
{ 0xb6d6ae00, 0x0884, 0x4a30, \
  { 0xa8, 0xb4, 0xce, 0xca, 0x57, 0x27, 0x1a, 0x3e } }

/**
 * This interface is OBSOLETE and in the process of being REMOVED.
 * Do NOT implement!
 */
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
  NS_IMETHOD IsEnabled(PRInt32 aTag, bool* aReturn) = 0;

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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLContentSink, NS_IHTML_CONTENT_SINK_IID)

#endif /* nsIHTMLContentSink_h___ */

