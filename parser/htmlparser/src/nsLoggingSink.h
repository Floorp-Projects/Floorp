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

#ifndef NS_LOGGING_SINK_H__
#define NS_LOGGING_SINK_H__

#include "nsILoggingSink.h"
#include "nsIParser.h"

class nsLoggingSink : public nsILoggingSink {
public:
  nsLoggingSink();
  virtual ~nsLoggingSink();

  void SetProxySink(nsIHTMLContentSink *aSink) {
    mSink=aSink;    
  }

  void ReleaseProxySink() {
    NS_IF_RELEASE(mSink);
    mSink=0;
  }


  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel();
  NS_IMETHOD DidBuildModel();
  NS_IMETHOD WillInterrupt();
  NS_IMETHOD WillResume();
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddHeadContent(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  virtual void FlushContent(PRBool aNotify) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }

  // nsIHTMLContentSink
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML();
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead();
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody();
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm();
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap();
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn) { return NS_OK; }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }

  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }

  // nsILoggingSink
  NS_IMETHOD SetOutputStream(PRFileDesc *aStream,PRBool autoDelete=PR_FALSE);

  nsresult OpenNode(const char* aKind, const nsIParserNode& aNode);
  nsresult CloseNode(const char* aKind);
  nsresult LeafNode(const nsIParserNode& aNode);
  nsresult WriteAttributes(const nsIParserNode& aNode);
  nsresult QuoteText(const nsAString& aValue, nsString& aResult);
  nsresult GetNewCString(const nsAString& aValue, char** aResult);
  PRBool WillWriteAttributes(const nsIParserNode& aNode);

protected:
  PRFileDesc          *mOutput;
	int				          mLevel;
  nsIHTMLContentSink  *mSink;
  PRBool              mAutoDeleteOutput;
  nsIParser*          mParser;
};

#endif

