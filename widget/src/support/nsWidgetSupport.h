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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsWidgetSupport_h__
#define nsWidgetSupport_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIWidget.h"


struct nsRect;
class nsIFileWidget;
class nsIAppShell;
class nsIButton;
class nsIEventListener;
class nsILabel;
class nsILookAndFeel;
class nsIMouseListener;
class nsIToolkit;
class nsIWidget;
class nsICheckButton;
class nsIScrollbar;
class nsITooltipWidget;
class nsITextWidget;

// These are a series of support methods which help in the creation
// of widgets. They are not needed, but are provided as a convenience
// mechanism when creating widgets

extern nsresult 
NS_CreateButton( nsISupports* aParent, 
								nsIButton* aButton, 
								const nsRect& aRect, 
								EVENT_CALLBACK aHandleEventFunction,
								const nsFont* aFont = nsnull);

extern nsresult 
NS_CreateCheckButton( nsISupports* aParent, 
											nsICheckButton* aCheckButton, 
											const nsRect& aRect, 
											EVENT_CALLBACK aHandleEventFunction,
											const nsFont* aFont = nsnull);

extern nsresult 
NS_CreateLabel( nsISupports* aParent,	
                nsILabel* aLabel, 
                const nsRect& aRect, 
                EVENT_CALLBACK aHandleEventFunction,
                const nsFont* aFont = nsnull);

extern nsresult 
NS_CreateTextWidget(nsISupports* aParent,	
                    nsITextWidget* aWidget, 
                    const nsRect& aRect, 
                    EVENT_CALLBACK aHandleEventFunction,
                    const nsFont* aFont = nsnull);

extern nsresult 
NS_CreateTooltipWidget(nsISupports* aParent,	
                      nsITooltipWidget* aWidget, 
                      const nsRect& aRect, 
                      EVENT_CALLBACK aHandleEventFunction,
                      const nsFont* aFont = nsnull);


extern nsresult 
NS_CreateScrollBar(nsISupports* aParent,	
                      nsIScrollbar* aWidget, 
                      const nsRect& aRect, 
                      EVENT_CALLBACK aHandleEventFunction);


extern nsresult 
NS_ShowWidget(nsISupports* aWidget, PRBool aShow);

extern nsresult 
NS_MoveWidget(nsISupports* aWidget, PRUint32 aX, PRUint32 aY);

extern nsresult 
NS_EnableWidget(nsISupports* aWidget, PRBool aEnable);

extern nsresult 
NS_SetFocusToWidget(nsISupports* aWidget);

extern nsresult 
NS_GetWidgetNativeData(nsISupports* aWidget, void** aNativeData);





#endif
