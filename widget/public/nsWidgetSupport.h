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

#ifndef nsWidgetSupport_h__
#define nsWidgetSupport_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIWidget.h"


struct nsRect;
class nsITextAreaWidget;
class nsIFileWidget;
class nsIAppShell;
class nsIButton;
class nsIComboBox;
class nsIEventListener;
class nsILabel;
class nsIListBox;
class nsIListWidget;
class nsILookAndFeel;
class nsIMouseListener;
class nsIToolkit;
class nsIWidget;
class nsICheckButton;
class nsIScrollbar;
class nsIRadioButton;
class nsITextWidget;
class nsIBrowserWindow;

// These are a series of support methods which help in the creation
// of widgets. They are not needed, but are provided as a convenience
// mechanism when creating widgets


extern NS_WIDGET nsresult 
NS_CreateButton( nsISupports* aParent, 
								nsIButton* aButton, 
								const nsRect& aRect, 
								EVENT_CALLBACK aHandleEventFunction,
								const nsFont* aFont = nsnull);

extern NS_WIDGET nsresult 
NS_CreateCheckButton( nsISupports* aParent, 
											nsICheckButton* aCheckButton, 
											const nsRect& aRect, 
											EVENT_CALLBACK aHandleEventFunction,
											const nsFont* aFont = nsnull);

extern NS_WIDGET nsresult 
NS_CreateRadioButton( nsISupports* aParent, 
											nsIRadioButton* aButton, 
											const nsRect& aRect, 
											EVENT_CALLBACK aHandleEventFunction,
											const nsFont* aFont = nsnull);

extern NS_WIDGET nsresult 
NS_CreateLabel( nsISupports* aParent,	
                nsILabel* aLabel, 
                const nsRect& aRect, 
                EVENT_CALLBACK aHandleEventFunction,
                const nsFont* aFont = nsnull);

extern NS_WIDGET nsresult 
NS_CreateTextWidget(nsISupports* aParent,	
                    nsITextWidget* aWidget, 
                    const nsRect& aRect, 
                    EVENT_CALLBACK aHandleEventFunction,
                    const nsFont* aFont = nsnull);

extern NS_WIDGET nsresult 
NS_CreateTextAreaWidget(nsISupports* aParent,	
                    nsITextAreaWidget* aWidget, 
                    const nsRect& aRect, 
                    EVENT_CALLBACK aHandleEventFunction,
                    const nsFont* aFont = nsnull);


extern NS_WIDGET nsresult 
NS_CreateListBox(nsISupports* aParent,	
                      nsIListBox* aWidget, 
                      const nsRect& aRect, 
                      EVENT_CALLBACK aHandleEventFunction,
                      const nsFont* aFont = nsnull);


extern NS_WIDGET nsresult 
NS_CreateComboBox(nsISupports* aParent,	
                      nsIComboBox* aWidget, 
                      const nsRect& aRect, 
                      EVENT_CALLBACK aHandleEventFunction,
                      const nsFont* aFont = nsnull);


extern NS_WIDGET nsresult 
NS_CreateScrollBar(nsISupports* aParent,	
                      nsIScrollbar* aWidget, 
                      const nsRect& aRect, 
                      EVENT_CALLBACK aHandleEventFunction);


extern NS_WIDGET nsresult 
NS_ShowWidget(nsISupports* aWidget, PRBool aShow);

extern NS_WIDGET nsresult 
NS_MoveWidget(nsISupports* aWidget, PRUint32 aX, PRUint32 aY);

extern NS_WIDGET nsresult 
NS_EnableWidget(nsISupports* aWidget, PRBool aEnable);

extern NS_WIDGET nsresult 
NS_SetFocusToWidget(nsISupports* aWidget);

extern NS_WIDGET nsresult 
NS_GetWidgetNativeData(nsISupports* aWidget, void** aNativeData);





#endif
