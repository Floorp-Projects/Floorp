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
#ifndef Window_h__
#define Window_h__

#include "nsISupports.h"

#include "nsWidget.h"
#include "nsIAppShell.h"

#include "nsString.h"

class nsFont;

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


/**
 * Native GTK++ window wrapper.
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

    NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

    virtual void*        GetNativeData(PRUint32 aDataType);

    NS_IMETHOD           SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);

    NS_IMETHOD           SetTitle(const nsString& aTitle);
    nsresult             SetIcon(GdkPixmap *window_pixmap, 
                                 GdkBitmap *window_mask);
    NS_IMETHOD           SetMenuBar(nsIMenuBar * aMenuBar);

    NS_IMETHOD           SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);
    NS_IMETHOD           UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD           RemoveTooltips();

    NS_IMETHOD            BeginResizingChildren(void);
    NS_IMETHOD            EndResizingChildren(void);
    NS_IMETHOD            Destroy(void);


    virtual PRBool IsChild() const;

    void SetIsDestroying(PRBool val) {
      mIsDestroying = val;
    }

    PRBool IsDestroying() const {
      return mIsDestroying;
    }

     // Utility methods
    virtual  PRBool OnPaint(nsPaintEvent &event);
    PRBool   OnKey(nsKeyEvent &aEvent);
    PRBool   DispatchFocus(nsGUIEvent &aEvent);
    virtual  PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  // in nsWidget now
  //    virtual  PRBool OnResize(nsSizeEvent &aEvent);

protected:

  virtual void InitCallbacks(char * aName = nsnull);
  NS_IMETHOD CreateNative(GtkWidget *parentWidget);
  nsresult     SetIcon();


  nsIFontMetrics *mFontMetrics;
  PRBool      mVisible;
  PRBool      mDisplayed;

  GtkWindowType mBorderStyle;

  // XXX Temporary, should not be caching the font
  nsFont *    mFont;

  // Resize event management
  nsRect mResizeRect;
  int    mResized;
  PRBool mLowerLeft;

  GtkWidget *mShell;  /* used for toplevel windows */
  GtkWidget *mVBox;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
public:
    ChildWindow();
    virtual PRBool IsChild() const;
};

#endif // Window_h__
