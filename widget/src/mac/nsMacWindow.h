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
#ifndef MacWindow_h__
#define MacWindow_h__

#include <memory>	// for auto_ptr

#include "nsWindow.h"
#include "nsMacEventHandler.h"

class nsMacEventHandler;

//-------------------------------------------------------------------------
//
// nsMacWindow
//
//-------------------------------------------------------------------------
//	MacOS native window

class nsMacWindow : public nsWindow
{

public:
    nsMacWindow();
    virtual ~nsMacWindow();

/*
    // nsIWidget interface
    NS_IMETHOD            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
*/
    NS_IMETHOD              Create(nsNativeWidget aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);

     // Utility method for implementing both Create(nsIWidget ...) and
     // Create(nsNativeWidget...)

    virtual nsresult        StandardCreate(nsIWidget *aParent,
				                            const nsRect &aRect,
				                            EVENT_CALLBACK aHandleEventFunction,
				                            nsIDeviceContext *aContext,
				                            nsIAppShell *aAppShell,
				                            nsIToolkit *aToolkit,
				                            nsWidgetInitData *aInitData,
				                            nsNativeWidget aNativeParent = nsnull);

    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD            	Move(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD            	Resize(PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    virtual PRBool          OnPaint(nsPaintEvent &event);

		NS_IMETHOD              SetTitle(const nsString& aTitle);

		virtual PRBool					HandleOSEvent(
																		EventRecord&		aOSEvent);

		virtual PRBool					HandleMenuCommand(
																		EventRecord&		aOSEvent,
																		long						aMenuResult);

protected:
	PRBool							mWindowMadeHere;	// true if we created the window
	PRBool							mIsDialog;				// true if the window is a dialog
	auto_ptr<nsMacEventHandler>		mMacEventHandler;
};

#endif // MacWindow_h__
