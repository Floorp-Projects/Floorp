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
#ifndef nsIDocumentWidget_h___
#define nsIDocumentWidget_h___

#include "nsIWidget.h"
#include "nsRect.h"
class nsIDocument;
class nsString;
class nsIScriptContext;

// Interface to the web widget. The web widget is a container for web
// content.
class nsIDocumentWidget : public nsISupports {
public:
  // Create a native window for this web widget; may be called once
  virtual nsresult Init(nsNativeWindow aNativeParent,
                        const nsRect& aBounds) = 0;

  virtual nsRect GetBounds() = 0;

  virtual void SetBounds(const nsRect& aBounds) = 0;

  virtual void Show() = 0;

  virtual void Hide() = 0;

  NS_IMETHOD LoadURL(const nsString& aURLSpec) = 0;

  virtual nsIWidget* GetWWWindow() = 0;

};

#endif /* nsIDocumentWidget_h___ */
