/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
    virtual PRBool OnText(nsTextEvent &aEvent);
    virtual PRBool OnComposition(nsCompositionEvent &aEvent);
    virtual PRBool DispatchFocusInEvent();
    virtual PRBool DispatchFocusOutEvent();
    virtual PRBool DispatchActivateEvent();
    virtual PRBool DispatchFocus(nsGUIEvent &aEvent);
    virtual PRBool OnScroll(nsScrollbarEvent &aEvent,PRUint32 cPos);

protected:
    virtual void InitCallbacks(char *aName = nsnull);
    NS_IMETHOD CreateNative(QWidget *parentWidget);

    PRBool mIsDialog;
    PRBool mIsPopup;
    PRBool mBlockFocusEvents;

    // are we doing a grab?
    static PRBool    mIsGrabbing;
    static nsWindow  *mGrabWindow;
    static nsQBaseWidget *mFocusWidget;
    static PRBool    mGlobalsInitialized;
    static PRBool    mRaiseWindows;
    static PRBool    mGotActivate;
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
