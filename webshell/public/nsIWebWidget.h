/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsIWebWidget_h___
#define nsIWebWidget_h___

#include "nsweb.h"
#include "nsIDocumentWidget.h"
#include "nsIScrollableView.h"

// Forward declarations... 
class nsIDocument;
class nsIDOMDocument;
class nsILinkHandler;
class nsIPresContext;
class nsIStyleSet;

// IID for the nsWebWidget interface
#define NS_IWEBWIDGET_IID      \
 { 0x02606880, 0x94e1, 0x11d1, \
   {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

// XXX this doesn't belong here, but to put it elsewhere requires a new header
// CID for the web widget
#define NS_WEBWIDGET_CID    \
{ 0xc552f181, 0x8d, 0x11d2, \
   { 0x80, 0x32, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

// Interface to the web widget. The web widget is a container for web
// content.
class nsIWebWidget : public nsIDocumentWidget {
public:

  // Create a native window for this web widget; may be called once
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;

  // Create a native window for this web widget; may be called once.
  // Use the given presentation context and document for the widget
  // (this widget becomes a second view on the document using the
  // context for presentation).
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                        const nsRect& aBounds,
                        nsIDocument* aDocument,
                        nsIPresContext* aPresContext,
                        nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;

  // XXX instead of nsISupports for aContainer, something like nsIContainer would be better
  NS_IMETHOD SetContainer(nsISupports* aContainer, PRBool aRelationship = PR_TRUE) = 0;

  NS_IMETHOD GetContainer(nsISupports** aResult) = 0;

  virtual PRInt32 GetNumChildren() = 0;

  NS_IMETHOD AddChild(nsIWebWidget* aChild, PRBool aRelationship = PR_TRUE) = 0;

  NS_IMETHOD GetChildAt(PRInt32 aIndex, nsIWebWidget** aChild) = 0;

  virtual PRBool GetName(nsString& aName) = 0;
  virtual void SetName(const nsString& aName) = 0;
  virtual nsIWebWidget* GetWebWidgetWithName(const nsString& aName) = 0;
  virtual nsIWebWidget* GetTarget(const nsString& aName) = 0;

  virtual nsIWebWidget* GetRootWebWidget() = 0;

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler) = 0;

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult) = 0;

  virtual nsIDocument* GetDocument() = 0;

  virtual void DumpContent(FILE* out = nsnull) = 0;

  virtual void DumpFrames(FILE* out = nsnull) = 0;

  virtual void DumpStyleSheets(FILE* out = nsnull) = 0;

  virtual void DumpStyleContexts(FILE* out = nsnull) = 0;

  virtual void DumpViews(FILE* out = nsnull) = 0;

  virtual void ShowFrameBorders(PRBool aEnable) = 0;

  virtual PRBool GetShowFrameBorders() = 0;

  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDocument) = 0;
                                                
  virtual nsIPresContext* GetPresContext() = 0;

};

// Create a new web widget that uses the default (galley) presentation
// context.
extern NS_WEB nsresult NS_NewWebWidget(nsIWebWidget** aInstancePtrResult);

#endif /* nsWebWidget_h___ */
