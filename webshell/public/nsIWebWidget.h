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
#include "nsIWidget.h"
#include "nsRect.h"
class nsIDocument;
class nsILinkHandler;
class nsIPresContext;
class nsIStyleSet;
class nsString;
class nsIScriptContext;

// IID for the nsWebWidget interface
#define NS_IWEBWIDGET_IID      \
 { 0x02606880, 0x94e1, 0x11d1, \
   {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

// Interface to the web widget. The web widget is a container for web
// content.
class nsIWebWidget : public nsISupports {
public:
  // Create a native window for this web widget; may be called once
  virtual nsresult Init(nsNativeWindow aNativeParent,
                        const nsRect& aBounds) = 0;

  // Create a native window for this web widget; may be called once.
  // Use the given presentation context and document for the widget
  // (this widget becomes a second view on the document using the
  // context for presentation).
  virtual nsresult Init(nsNativeWindow aNativeParent,
                        const nsRect& aBounds,
                        nsIDocument* aDocument,
                        nsIPresContext* aPresContext) = 0;

  virtual nsRect GetBounds() = 0;

  virtual void SetBounds(const nsRect& aBounds) = 0;

  virtual void Show() = 0;

  virtual void Hide() = 0;

  NS_IMETHOD SetContainer(nsISupports* aContainer) = 0;

  NS_IMETHOD GetContainer(nsISupports** aResult) = 0;

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler) = 0;

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult) = 0;

  NS_IMETHOD LoadURL(const nsString& aURLSpec) = 0;

  virtual nsIDocument* GetDocument() = 0;

  virtual void HackAppendContent() = 0;

  virtual void DumpContent(FILE* out = nsnull) = 0;

  virtual void DumpFrames(FILE* out = nsnull) = 0;

  virtual void DumpStyle(FILE* out = nsnull) = 0;

  virtual void DumpViews(FILE* out = nsnull) = 0;

  virtual void ShowFrameBorders(PRBool aEnable) = 0;

  virtual PRBool GetShowFrameBorders() = 0;

  virtual nsIWidget* GetWWWindow() = 0;

  virtual nsresult GetScriptContext(nsIScriptContext **aContext) = 0;
  virtual nsresult ReleaseScriptContext() = 0;
};

// Create a new web widget that uses the default (galley) presentation
// context.
extern NS_WEB nsresult
NS_NewWebWidget(nsIWebWidget** aInstancePtrResult);

#endif /* nsWebWidget_h___ */
