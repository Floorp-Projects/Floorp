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
 * @update  gess 4/1/98
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
 * OpenXXX method: OpenHTML,OpenHead,OpenBody,OpenFrameSet;
 * likewise, the CloseXXX version will be called when the
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

#ifndef  NS_IHTMLCONTENTSINK
#define  NS_ITMLCONTENTSINK

#include "nsIParserNode.h"
#include "nsIContentSink.h"

#define NS_IHTMLCONTENTSINK_IID      \
  {0xd690e200, 0xc286,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x08, 0x14, 0x98, 0x89}}


class nsIHTMLContentSink : public nsIContentSink {
  public:

   /**
    * This method gets called by the parser when it encounters
    * a title tag and wants to set the document title in the sink.
    *
    * @update 4/1/98 gess
    * @param  nsString reference to new title value
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool SetTitle(const nsString& aValue)=0;

   /**
    * This method is used to open the outer HTML container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenHTML(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the outer HTML container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseHTML(const nsIParserNode& aNode)=0;

   /**
    * This method is used to open the only HEAD container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenHead(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the only HEAD container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseHead(const nsIParserNode& aNode)=0;
  
   /**
    * This method is used to open the main BODY container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenBody(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the main BODY container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseBody(const nsIParserNode& aNode)=0;

   /**
    * This method is used to open a new FORM container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenForm(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the outer FORM container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseForm(const nsIParserNode& aNode)=0;
        
   /**
    * This method is used to open the FRAMESET container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenFrameset(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the FRAMESET container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseFrameset(const nsIParserNode& aNode)=0;
        
   /**
    * This method is used to a general container. 
    * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool OpenContainer(const nsIParserNode& aNode)=0;
    
   /**
    * This method is used to close a generic container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseContainer(const nsIParserNode& aNode)=0;

   /**
    * This method is used to close the topmost container, regardless
    * of the type.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool CloseTopmostContainer()=0;

   /**
    * This method is used to add a leaf to the currently 
    * open container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRBool AddLeaf(const nsIParserNode& aNode)=0;

   /**
    * This method gets called when the parser begins the process
    * of building the content model via the content sink.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillBuildModel(void)=0;

   /**
    * This method gets called when the parser concludes the process
    * of building the content model via the content sink.
    *
    * @update 5/7/98 gess
    */     
    virtual void DidBuildModel(void)=0;

   /**
    * This method gets called when the parser gets i/o blocked,
    * and wants to notify the sink that it may be a while before
    * more data is available.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillInterrupt(void)=0;

   /**
    * This method gets called when the parser i/o gets unblocked,
    * and we're about to start dumping content again to the sink.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillResume(void)=0;
};


#endif




