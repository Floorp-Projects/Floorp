/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *		John C. Griggs <johng@corel.com>
 *
 */
#ifndef Window_h__
#define Window_h__

#include "nsWidget.h"

/* Native QT window wrapper */
class nsWindow : public nsWidget
{
public:
    nsWindow();
    virtual ~nsWindow();

    // nsIsupports
    NS_DECL_ISUPPORTS_INHERITED

    // nsIWidget interface
    virtual void ConvertToDeviceCoordinates(nscoord &aX,nscoord &aY);

    NS_IMETHOD PreCreateWidget(nsWidgetInitData *aWidgetInitData);

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD Scroll(PRInt32 aDx,PRInt32 aDy,nsRect *aClipRect);

    NS_IMETHOD ConstrainPosition(PRInt32 *aX,PRInt32 *aY);
    NS_IMETHOD Move(PRInt32 aX,PRInt32 aY);

    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   PRBool aDoCapture,
                                   PRBool aConsumeRollupEvent);
    NS_IMETHOD SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);
    NS_IMETHOD UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD RemoveTooltips();

    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);
    NS_IMETHOD SetFocus(PRBool aRaise);

    NS_IMETHOD GetBounds(nsRect &aRect);
    NS_IMETHOD GetBoundsAppUnits(nsRect &aRect,float aAppUnits);
    NS_IMETHOD GetClientBounds(nsRect &aRect);
    NS_IMETHOD GetScreenBounds(nsRect &aRect);

    virtual PRBool IsChild() const;
    virtual PRBool IsPopup() const { return mIsPopup; };
    virtual PRBool IsDialog() const { return mIsDialog; };

    // Utility methods
    virtual PRBool OnPaint(nsPaintEvent &event);
    virtual PRBool OnKey(nsKeyEvent &aEvent);
    virtual PRBool DispatchFocus(nsGUIEvent &aEvent);
    virtual PRBool OnScroll(nsScrollbarEvent &aEvent,PRUint32 cPos);

protected:
    virtual void InitCallbacks(char *aName = nsnull);
    NS_IMETHOD CreateNative(QWidget *parentWidget);

    PRBool mBlockFocusEvents;
    PRBool mIsDialog;
    PRBool mIsPopup;

    // are we doing a grab?
    static PRBool    mIsGrabbing;
    static nsWindow  *mGrabWindow;
    PRInt32	     mWindowID;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow 
{
public:
  ChildWindow();
  ~ChildWindow();
  virtual PRBool IsChild() const;

  PRInt32 mChildID;
};

#endif // Window_h__
