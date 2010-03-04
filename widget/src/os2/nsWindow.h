/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rich Walsh <dragtext@e-vertise.com>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM are
 * Copyright (c) International Business Machines Corporation, 2000
 *
 */

//=============================================================================
/*
 * nsWindow derives from nsIWidget via nsBaseWindow, and is created when
 * NS_CHILD_CID is specified.  It is used for widgets that are either
 * children of other widgets or are popups (standalone windows such as
 * menus that float above other widgets).
 * 
 * Top-level widgets (windows surrounded by a frame with a titlebar, etc.)
 * are implemented by nsFrameWindow which is a subclass of nsWindow.  Many
 * nsWindow methods operate on both child & top-level windows.  Where the
 * two categories require differing implementations, these methods rely on
 * a flag or on virtual helper methods to produce the correct result.
 *
 */
//=============================================================================

#ifndef _nswindow_h
#define _nswindow_h

#include "nsBaseWidget.h"
#include "gfxASurface.h"

#define INCL_DOS
#define INCL_WIN
#define INCL_NLS
#define INCL_GPI
#include <os2.h>

//-----------------------------------------------------------------------------
// Items that may not be in the OS/2 Toolkit headers

// For WM_MOUSEENTER/LEAVE, mp2 is the other window.
#ifndef WM_MOUSEENTER
#define WM_MOUSEENTER   0x041E
#endif

#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE   0x041F
#endif

#ifndef WM_FOCUSCHANGED
#define WM_FOCUSCHANGED 0x000E
#endif

extern "C" {
  PVOID  APIENTRY WinQueryProperty(HWND hwnd, PCSZ pszNameOrAtom);
  PVOID  APIENTRY WinRemoveProperty(HWND hwnd, PCSZ pszNameOrAtom);
  BOOL   APIENTRY WinSetProperty(HWND hwnd, PCSZ pszNameOrAtom,
                                 PVOID pvData, ULONG ulFlags);
  APIRET APIENTRY DosQueryModFromEIP(HMODULE* phMod, ULONG* pObjNum,
                                     ULONG BuffLen,  PCHAR pBuff,
                                     ULONG* pOffset, ULONG Address);
}

//-----------------------------------------------------------------------------
// Macros

// nsWindow's PM window class name
#define kWindowClassName            "MozillaWindowClass"
#define QWL_NSWINDOWPTR             (QWL_USER+4)

// Miscellaneous global flags stored in gOS2Flags
#define kIsInitialized              0x0001
#define kIsDBCS                     0x0002
#define kIsTrackPoint               0x0004

// Possible states of the window
#define nsWindowState_ePrecreate    0x0001      // Create() not called yet
#define nsWindowState_eInCreate     0x0002      // processing Create() method
#define nsWindowState_eLive         0x0004      // active, existing window
#define nsWindowState_eClosing      0x0008      // processing Close() method
#define nsWindowState_eDoingDelete  0x0010      // object destructor running
#define nsWindowState_eDead         0x0100      // window destroyed

//-----------------------------------------------------------------------------
// Debug

//#define DEBUG_FOCUS

#ifdef DEBUG_FOCUS
  #define DEBUGFOCUS(what) fprintf(stderr, "[%8x]  %8lx  (%02d)  "#what"\n", \
                                   (int)this, mWnd, mWindowIdentifier)
#else
  #define DEBUGFOCUS(what)
#endif

//-----------------------------------------------------------------------------
// Forward declarations

class imgIContainer;
class gfxOS2Surface;

MRESULT EXPENTRY fnwpNSWindow(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

//=============================================================================
//  nsWindow
//=============================================================================

class nsWindow : public nsBaseWidget
{
public:
  nsWindow();
  virtual ~nsWindow();

  // from nsIWidget
  NS_IMETHOD            Create(nsIWidget* aParent,
                               nsNativeWidget aNativeParent,
                               const nsIntRect& aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext* aContext,
                               nsIAppShell* aAppShell = nsnull,
                               nsIToolkit* aToolkit = nsnull,
                               nsWidgetInitData* aInitData = nsnull);
  NS_IMETHOD            Destroy();
  virtual nsIWidget*    GetParent();
  NS_IMETHOD            Enable(PRBool aState);
  NS_IMETHOD            IsEnabled(PRBool* aState);
  NS_IMETHOD            Show(PRBool aState);
  NS_IMETHOD            IsVisible(PRBool& aState);
  NS_IMETHOD            SetFocus(PRBool aRaise);
  NS_IMETHOD            Invalidate(const nsIntRect& aRect,
                                   PRBool aIsSynchronous);
  NS_IMETHOD            Update();
  gfxASurface*          GetThebesSurface();
  virtual void*         GetNativeData(PRUint32 aDataType);
  virtual void          FreeNativeData(void* aDatum, PRUint32 aDataType);
  NS_IMETHOD            CaptureMouse(PRBool aCapture);
  virtual PRBool        HasPendingInputEvent();
  virtual void          Scroll(const nsIntPoint& aDelta,
                               const nsTArray<nsIntRect>& aDestRects,
                               const nsTArray<Configuration>& aReconfigureChildren);
  NS_IMETHOD            GetBounds(nsIntRect& aRect);
  NS_IMETHOD            GetClientBounds(nsIntRect& aRect);
  virtual nsIntPoint    WidgetToScreenOffset();
  NS_IMETHOD            Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD            Resize(PRInt32 aWidth, PRInt32 aHeight,
                               PRBool  aRepaint);
  NS_IMETHOD            Resize(PRInt32 aX, PRInt32 aY,
                               PRInt32 aWidth, PRInt32 aHeight,
                               PRBool  aRepaint);
  NS_IMETHOD            PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                    nsIWidget* aWidget, PRBool aActivate);
  NS_IMETHOD            SetZIndex(PRInt32 aZIndex);
  virtual nsresult      ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD            SetSizeMode(PRInt32 aMode);
  NS_IMETHOD            HideWindowChrome(PRBool aShouldHide);
  NS_IMETHOD            SetTitle(const nsAString& aTitle); 
  NS_IMETHOD            SetIcon(const nsAString& aIconSpec); 
  NS_IMETHOD            ConstrainPosition(PRBool aAllowSlop,
                                          PRInt32* aX, PRInt32* aY);
  NS_IMETHOD            SetCursor(nsCursor aCursor);
  NS_IMETHOD            SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD            CaptureRollupEvents(nsIRollupListener* aListener,
                                            nsIMenuRollup* aMenuRollup,
                                            PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD            GetToggledKeyState(PRUint32 aKeyCode,
                                           PRBool* aLEDState);
  NS_IMETHOD            DispatchEvent(nsGUIEvent* event,
                                      nsEventStatus& aStatus);

  // nsWindow
  static void           ReleaseGlobals();

protected:
  // from nsBaseWidget
  virtual void          OnDestroy();

  // nsWindow
  static void           InitGlobals();
  virtual nsresult      CreateWindow(nsWindow* aParent,
                                     HWND aParentWnd,
                                     const nsIntRect& aRect,
                                     PRUint32 aStyle);
  virtual HWND          GetMainWindow()     const {return mWnd;}
  static nsWindow*      GetNSWindowPtr(HWND aWnd);
  static PRBool         SetNSWindowPtr(HWND aWnd, nsWindow* aPtr);
  void                  NS2PM(POINTL& ptl);
  void                  NS2PM(RECTL& rcl);
  void                  NS2PM_PARENT(POINTL& ptl);
  void                  ActivatePlugin(HWND aWnd);
  void                  SetPluginClipRegion(const Configuration& aConfiguration);
  HWND                  GetPluginClipWindow(HWND aParentWnd);
  virtual void          ActivateTopLevelWidget();
  HBITMAP               DataToBitmap(PRUint8* aImageData, PRUint32 aWidth,
                                     PRUint32 aHeight, PRUint32 aDepth);
  HBITMAP               CreateBitmapRGB(PRUint8* aImageData,
                                        PRUint32 aWidth, PRUint32 aHeight);
  HBITMAP               CreateTransparencyMask(gfxASurface::gfxImageFormat format,
                                               PRUint8* aImageData,
                                               PRUint32 aWidth, PRUint32 aHeight);
  static PRBool         EventIsInsideWindow(nsWindow* aWindow); 
  static PRBool         RollupOnButtonDown(ULONG aMsg);
  static void           RollupOnFocusLost(HWND aFocus);
  MRESULT               ProcessMessage(ULONG msg, MPARAM mp1, MPARAM mp2);
  virtual PRBool        OnReposition(PSWP pNewSwp);
  PRBool                OnPaint();
  PRBool                OnMouseChord(MPARAM mp1, MPARAM mp2);
  PRBool                OnDragDropMsg(ULONG msg, MPARAM mp1, MPARAM mp2,
                                      MRESULT& mr);
  PRBool                CheckDragStatus(PRUint32 aAction, HPS* aHps);
  PRBool                ReleaseIfDragHPS(HPS aHps);
  PRBool                OnTranslateAccelerator(PQMSG pQmsg);
  PRBool                DispatchKeyEvent(MPARAM mp1, MPARAM mp2);
  void                  InitEvent(nsGUIEvent& event, nsIntPoint* pt = 0);
  PRBool                DispatchWindowEvent(nsGUIEvent* event);
  PRBool                DispatchWindowEvent(nsGUIEvent* event,
                                            nsEventStatus& aStatus);
  PRBool                DispatchCommandEvent(PRUint32 aEventCommand);
  PRBool                DispatchDragDropEvent(PRUint32 aMsg);
  PRBool                DispatchMoveEvent(PRInt32 aX, PRInt32 aY);
  PRBool                DispatchResizeEvent(PRInt32 aClientX, 
                                            PRInt32 aClientY);
  PRBool                DispatchMouseEvent(PRUint32 aEventType,
                                           MPARAM mp1, MPARAM mp2, 
                                           PRBool aIsContextMenuKey = PR_FALSE,
                                           PRInt16 aButton = nsMouseEvent::eLeftButton);
  PRBool                DispatchActivationEvent(PRUint32 aEventType);
  PRBool                DispatchScrollEvent(ULONG msg, MPARAM mp1, MPARAM mp2);
  virtual PRInt32       GetClientHeight()   {return mBounds.height;}

  friend MRESULT EXPENTRY fnwpNSWindow(HWND hwnd, ULONG msg,
                                       MPARAM mp1, MPARAM mp2);
  friend MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);

  HWND          mWnd;               // window handle
  nsWindow*     mParent;            // parent widget
  PRBool        mIsTopWidgetWindow; // is nsFrameWindow class?
  PRInt32       mWindowState;       // current nsWindowState_* value
  PRBool        mIsDestroying;      // in destructor
  PRBool        mInSetFocus;        // prevent recursive calls
  HPS           mDragHps;           // retrieved by DrgGetPS() during a drag
  PRUint32      mDragStatus;        // set when object is being dragged over
  HWND          mClipWnd;           // used to clip plugin windows
  HPOINTER      mCssCursorHPtr;     // created by SetCursor(imgIContainer*)
  nsCOMPtr<imgIContainer> mCssCursorImg;// saved by SetCursor(imgIContainer*)
  nsRefPtr<gfxOS2Surface> mThebesSurface;
  HWND          mFrameWnd;          // frame window handle
  HPOINTER      mFrameIcon;         // current frame icon
  PRBool        mChromeHidden;      // are frame controls hidden?
#ifdef DEBUG_FOCUS
  int           mWindowIdentifier;  // a serial number for each new window
#endif
};

#endif // _nswindow_h

//=============================================================================

