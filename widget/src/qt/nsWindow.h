/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef Window_h__
#define Window_h__

#include "nsISupports.h"
#include "nsWidget.h"
#include "nsIAppShell.h"
#include "nsString.h"

#include <qlayout.h>

class nsFont;

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


//=============================================================================
//
// nsQWidget class
//
//=============================================================================
class nsQWidget : public QWidget, public nsQBaseWidget
{
    Q_OBJECT
public:
	nsQWidget(nsWidget * widget,
              QWidget * parent = 0, 
              const char * name = 0, 
              WFlags f = WResizeNoErase);
	~nsQWidget();

protected:
#if 0
    virtual void closeEvent(QCloseEvent * event);
	virtual void showEvent(QShowEvent * event);
	virtual void hideEvent(QHideEvent * event);
	virtual void resizeEvent(QResizeEvent * event);
	virtual void moveEvent(QMoveEvent * event);
    virtual void paintEvent(QPaintEvent * event);
    virtual void mousePressedEvent(QMouseEvent * event);
    virtual void mouseReleasedEvent(QMouseEvent * event);
    virtual void mouseDoubleClickedEvent(QMouseEvent * event);
    virtual void focusInEvent(QFocusEvent * event);
    virtual void focusOutEvent(QFocusEvent * event);
    virtual void enterEvent(QEvent * event);
    virtual void leaveEvent(QEvent * event);
#endif

};

/**
 * Native QT window wrapper.
 */

class nsWindow : public nsWidget
{

public:
    // nsIWidget interface

    nsWindow();
    virtual ~nsWindow();

    // nsIsupports
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

    NS_IMETHOD PreCreateWidget(nsWidgetInitData *aWidgetInitData);

    //virtual void* GetNativeData(PRUint32 aDataType);

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);

    NS_IMETHOD SetTitle(const nsString& aTitle);
    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD ShowMenuBar(PRBool aShow);

    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRInt32 aX, 
                      PRInt32 aY, 
                      PRInt32 aWidth,
                      PRInt32 aHeight, 
                      PRBool aRepaint);

    NS_IMETHOD SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);
    NS_IMETHOD UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD RemoveTooltips();

    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);
    NS_IMETHOD Destroy(void);


    virtual PRBool IsChild() const;
    virtual void SetIsDestroying( PRBool val) { mIsDestroying = val; };

    PRBool IsDestroying() const { return mIsDestroying; }

    // Utility methods
    virtual PRBool OnPaint(nsPaintEvent &event);
    PRBool OnKey(nsKeyEvent &aEvent);
    PRBool DispatchFocus(nsGUIEvent &aEvent);
    virtual PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
    // in nsWidget now
    //    virtual  PRBool OnResize(nsSizeEvent &aEvent);

    char gInstanceClassName[256];
  
protected:
    virtual void OnDestroy();

    virtual void InitCallbacks(char * aName = nsnull);
    NS_IMETHOD CreateNative(QWidget *parentWidget);

    nsIFontMetrics * mFontMetrics;
    PRBool           mVisible;
    PRBool           mDisplayed;
    PRBool           mIsDestroying;

    // XXX Temporary, should not be caching the font
    nsFont         * mFont;

    // Resize event management
    nsRect           mResizeRect;
    int              mResized;
    PRBool           mLowerLeft;

    QWidget        * mShell;  /* used for toplevel windows */
    QHBoxLayout    * mHBox;
    nsIMenuBar     * m_nsIMenuBar;
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
  NS_IMETHOD Destroy(void);
};

#endif // Window_h__
