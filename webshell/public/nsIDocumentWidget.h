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

#include "nsweb.h"
#include "nsRect.h"
#include "nsIWidget.h"
#include "nsIScrollableView.h"

// Forward declarations... 
class nsIDeviceContext;
class nsString;

class nsIDocument;
class nsIPresContext;
class nsIStyleSheet;
class nsIViewerContainer;

// IID for the nsIDocumentWidget interface
// a7d1b8b0-0b1c-11d2-beba-00805f8a66dc
#define NS_IDOCUMENTWIDGET_IID      \
 { 0xa7d1b8b0, 0x0b1c, 0x11d2, \
   {0xbe, 0xba, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

// Interface to the web widget. The web widget is a container for web
// content.
class nsIContentViewer : public nsISupports {
public:
  // Create a native window for this web widget; may be called once
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  nsIDeviceContext* aDeviceContext,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;

  NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand) = 0;
  NS_IMETHOD SetContainer(nsIViewerContainer* aContainer) = 0;

  virtual nsRect GetBounds() = 0;
  virtual void SetBounds(const nsRect& aBounds) = 0;

  virtual void Move(PRInt32 aX, PRInt32 aY) = 0;

  virtual void Show() = 0;

  virtual void Hide() = 0;
};




/* 30d26b00-176d-11d2-bec0-00805f8a66dc */
#define NS_IWEBWIDGETVIEWER_IID      \
 { 0x30d26b00, 0x176d, 0x11d2, \
   {0xbe, 0xc0, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }


class nsIWebWidgetViewer : public nsIContentViewer {
public:
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  nsIDeviceContext* aDeviceContext,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;

  NS_IMETHOD Init(nsNativeWidget aParent,
                  const nsRect& aBounds,
                  nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;

  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet) = 0;
  
  virtual nsIPresContext* GetPresContext() = 0;
};

extern "C" NS_WEB nsresult NS_NewContentViewer(nsIWebWidgetViewer** aViewer);


#endif /* nsIDocumentWidget_h___ */
