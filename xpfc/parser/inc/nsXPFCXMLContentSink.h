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

#ifndef  NS_XPFCXMLCONTENTSINK
#define  NS_XPFCXMLCONTENTSINK

#include "nsIParserNode.h"
#include "nsIContentSink.h"
#include "nsString.h"
#include "nsIHTMLContentSink.h"
#include "nsIWebViewerContainer.h"
#include "nsxpfcCIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIStack.h"
#include "nsHTMLTokens.h"
#include "nsXPFCXMLDTD.h"
#include "nsIXMLParserObject.h"
#include "nsIXPFCXMLContentSink.h"
#include "nsIXPFCContentSink.h"

class nsXPFCXMLContentSink : public nsIHTMLContentSink,
                             public nsIXPFCXMLContentSink,
                             public nsIXPFCContentSink
{

public:

  NS_DECL_ISUPPORTS

  nsXPFCXMLContentSink();
  virtual ~nsXPFCXMLContentSink();

  NS_IMETHOD Init();
  NS_IMETHOD SetViewerContainer(nsIWebViewerContainer * aViewerContainer);
  NS_IMETHOD SetRootCanvas(nsIXPFCCanvas * aCanvas);
  NS_IMETHOD SetContentSinkContainer(nsISupports * aContentSinkContainer);

  // nsIContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);

  // nsIHTMLContentSink
  NS_IMETHOD PushMark();
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);

private:
  NS_IMETHOD CIDFromTag(eXPFCXMLTags tag, nsCID &aClass);
  NS_IMETHOD ConsumeAttributes(const nsIParserNode& aNode, nsIXMLParserObject& aObject);
  NS_IMETHOD AddToHierarchy(nsIXMLParserObject& aObject, PRBool aPush);

public:
  NS_IMETHOD_(PRBool) IsContainer(const nsIParserNode& aNode);

private:
  NS_IMETHOD PushComponent(nsISupports* aComponent);


private:
    nsIWebViewerContainer * mViewerContainer ;
    nsIStack *  mXPFCStack;
    nsIArray * mOrphanMenuList;
    nsIArray * mContainerList ;
    PRUint32    mState;

};


#endif


