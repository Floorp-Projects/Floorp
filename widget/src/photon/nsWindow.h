/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWindow_h__
#define nsWindow_h__



#include "nsISupports.h"

#include "nsWidget.h"

#include "nsString.h"

#include <Pt.h>

class nsFont;
class nsIAppShell;

#define NSRGB_2_COLOREF(color)				RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

/**
 * Native Photon window wrapper. 
 */

class nsWindow : public nsWidget
{

public:
  // nsIWidget interface

  nsWindow();
  virtual ~nsWindow();

  NS_IMETHOD           WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD           ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  inline NS_IMETHOD    ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy)
		{
		NS_WARNING("nsWindow::ScrollRect Not Implemented\n");
		return NS_OK;
		}

  NS_IMETHOD           SetTitle(const nsAString& aTitle);
 
  NS_IMETHOD           Move(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD           Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD           Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
		{
		Move(aX,aY);
		Resize(aWidth,aHeight,aRepaint);
		return NS_OK;
		}

  NS_IMETHOD           CaptureRollupEvents(nsIRollupListener * aListener,
                                           PRBool aDoCapture,
                                           PRBool aConsumeRollupEvent);

	NS_IMETHOD SetFocus(PRBool aRaise);

  inline NS_IMETHOD    GetAttention(PRInt32 aCycleCount)
		{
		if( mWidget ) PtWindowToFront( mWidget );
		return NS_OK;
		}

  virtual PRBool       IsChild() const;


  // Utility methods
  inline PRBool         OnKey(nsKeyEvent &aEvent)
		{
		if( mEventCallback ) return DispatchWindowEvent(&aEvent);
		return PR_FALSE;
		}

  inline NS_IMETHOD			GetClientBounds( nsRect &aRect )
		{
		aRect.x = 0;
		aRect.y = 0;
		aRect.width = mBounds.width;
		aRect.height = mBounds.height;
		return NS_OK;
		}

  NS_IMETHOD            SetModal(PRBool aModal);
	NS_IMETHOD            MakeFullScreen(PRBool aFullScreen);

 // Native draw function... like doPaint()
 static void            RawDrawFunc( PtWidget_t *pWidget, PhTile_t *damage );

 inline nsIRegion              *GetRegion();

private:
  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void          DestroyNative(void);
  void                  DestroyNativeChildren(void);

  static int            MenuRegionCallback(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo);  

  NS_IMETHOD            CreateNative(PtWidget_t *parentWidget);

  static int            ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
	static int            EvInfo( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int            WindowWMHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int            MenuRegionDestroyed( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );

  inline NS_IMETHOD     ModalEventFilter(PRBool aRealEvent, void *aEvent, PRBool *aForWindow)
		{
		*aForWindow = PR_TRUE;
		return NS_OK;
		}

private:
  PtWidget_t *mClientWidget, *mLastMenu;
  PRBool mIsTooSmall;
  PRBool mIsDestroying;
	static nsIRollupListener *gRollupListener;
	static nsIWidget *gRollupWidget;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
  public:
    ChildWindow()
			{
			mBorderStyle     = eBorderStyle_none;
			mWindowType      = eWindowType_child;
			}
    ~ChildWindow() { }
	inline PRBool IsChild() const { return PR_TRUE; }
};

#endif // Window_h__
