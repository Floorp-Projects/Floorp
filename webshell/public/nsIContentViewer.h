/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIContentViewer_h___
#define nsIContentViewer_h___

#include "nsweb.h"
#include "nsIWidget.h"
#include "nsIScrollableView.h"

// Forward declarations... 
class nsIDeviceContext;
class nsString;
struct nsRect;
class nsIContentViewerContainer;

// IID for the nsIContentViewer interface
#define NS_ICONTENT_VIEWER_IID \
 { 0xa6cf9056, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * Content viewer interface. When a stream of data is to be presented
 * visually to the user, a content viewer is found for the underlying
 * mime-type associated with the data in the stream. This interface
 * defines the capabilities of the viewer.
 */
class nsIContentViewer : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENT_VIEWER_IID)

  /**
   * Initialize the content viewer. Make it a child of the
   * the given native parent widget.
   */
  NS_IMETHOD Init(nsIWidget* aParentWidget,
                  nsIDeviceContext* aDeviceContext,
                  const nsRect& aBounds)=0;

  NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand) = 0;

  NS_IMETHOD SetContainer(nsISupports* aContainer) = 0;

  NS_IMETHOD GetContainer(nsISupports** aContainerResult) = 0;

  NS_IMETHOD Stop(void) = 0;

  NS_IMETHOD GetBounds(nsRect& aBounds) = 0;

  NS_IMETHOD SetBounds(const nsRect& aBounds) = 0;

  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY) = 0;

  NS_IMETHOD Show(void) = 0;

  NS_IMETHOD Hide(void) = 0;

  NS_IMETHOD SetEnableRendering(PRBool aOn) = 0;

  NS_IMETHOD GetEnableRendering(PRBool* aResult) = 0;
};

#endif /* nsIContentViewer_h___ */
