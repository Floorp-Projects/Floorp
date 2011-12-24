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
 * nsWindow derives from nsIWidget via nsBaseWindow.  With the aid
 * of a helper class (os2FrameWindow) and a subclass (nsChildWindow),
 * it implements the functionality of both toplevel and child widgets.
 *
 * Top-level widgets (windows surrounded by a frame with a titlebar, etc.)
 * are created when NS_WINDOW_CID is used to identify the type of object
 * needed.  Child widgets (windows that either are children of other windows
 * or are popups such as menus) are created when NS_CHILD_CID is specified.
 * Since Mozilla expects these to be different classes, NS_CHILD_CID is
 * mapped to a subclass (nsChildWindow) which acts solely as a wrapper for
 * nsWindow and adds no functionality.
 *
 * While most of the methods inherited from nsIWidget are generic, some
 * apply only to toplevel widgets (e.g. setting a title or icon).  The
 * nature of toplevel windows on OS/2 with their separate frame & client
 * windows introduces the need for additional toplevel-specific methods,
 * as well as for special handling in otherwise generic methods.
 *
 * Rather than incorporating these toplevel functions into the body of
 * the class, nsWindow delegates them to a helper class, os2FrameWindow.
 * An instance of this class is created when nsWindow is told to create
 * a toplevel native window and is destroyed in nsWindow's destructor.
 * The class's methods operate exclusively on the frame window and never
 * deal with the frame's client other than to create it.  Similarly,
 * nsWindow never operates on frame windows except for a few trivial
 * methods (e.g. Enable()).  Neither class accesses the other's data
 * though, of necessity, both call the other's methods.
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

//-----------------------------------------------------------------------------
// Forward declarations

class imgIContainer;
class gfxOS2Surface;
class os2FrameWindow;

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
                               nsDeviceContext* aContext,
                               nsWidgetInitData* aInitData = nsnull);
  NS_IMETHOD            Destroy();
  virtual nsIWidget*    GetParent();
  virtual float         GetDPI();
  NS_IMETHOD            Enable(bool aState);
  NS_IMETHOD            IsEnabled(bool* aState);
  NS_IMETHOD            Show(bool aState);
  NS_IMETHOD            IsVisible(bool& aState);
  NS_IMETHOD            SetFocus(bool aRaise);
  NS_IMETHOD            Invalidate(const nsIntRect& aRect,
                                   bool aIsSynchronous);
  NS_IMETHOD            Update();
  gfxASurface*          GetThebesSurface();
  virtual void*         GetNativeData(PRUint32 aDataType);
  virtual void          FreeNativeData(void* aDatum, PRUint32 aDataType);
  NS_IMETHOD            CaptureMouse(bool aCapture);
  virtual bool          HasPendingInputEvent();
  NS_IMETHOD            GetBounds(nsIntRect& aRect);
  NS_IMETHOD            GetClientBounds(nsIntRect& aRect);
  virtual nsIntPoint    WidgetToScreenOffset();
  NS_IMETHOD            Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD            Resize(PRInt32 aWidth, PRInt32 aHeight,
                               bool    aRepaint);
  NS_IMETHOD            Resize(PRInt32 aX, PRInt32 aY,
                               PRInt32 aWidth, PRInt32 aHeight,
                               bool    aRepaint);
  NS_IMETHOD            PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                    nsIWidget* aWidget, bool aActivate);
  NS_IMETHOD            SetZIndex(PRInt32 aZIndex);
  virtual nsresult      ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD            SetSizeMode(PRInt32 aMode);
  NS_IMETHOD            HideWindowChrome(bool aShouldHide);
  NS_IMETHOD            SetTitle(const nsAString& aTitle); 
  NS_IMETHOD            SetIcon(const nsAString& aIconSpec); 
  NS_IMETHOD            ConstrainPosition(bool aAllowSlop,
                                          PRInt32* aX, PRInt32* aY);
  NS_IMETHOD            SetCursor(nsCursor aCursor);
  NS_IMETHOD            SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD            CaptureRollupEvents(nsIRollupListener* aListener,
                                            bool aDoCapture, bool aConsumeRollupEvent);
  NS_IMETHOD            GetToggledKeyState(PRUint32 aKeyCode,
                                           bool* aLEDState);
  NS_IMETHOD            DispatchEvent(nsGUIEvent* event,
                                      nsEventStatus& aStatus);
  NS_IMETHOD            ReparentNativeWidget(nsIWidget* aNewParent);

  NS_IMETHOD_(void)     SetInputContext(const InputContext& aInputContext,
                                        const InputContextAction& aAction)
  {
    mInputContext = aInputContext;
  }
  NS_IMETHOD_(InputContext) GetInputContext()
  {
    return mInputContext;
  }

  // nsWindow
  static void           ReleaseGlobals();
protected:
  // from nsBaseWidget
  virtual void          OnDestroy();

  // nsWindow
  static void           InitGlobals();
  nsresult              CreateWindow(nsWindow* aParent,
                                     HWND aParentWnd,
                                     const nsIntRect& aRect,
                                     nsWidgetInitData* aInitData);
  gfxASurface*          ConfirmThebesSurface();
  HWND                  GetMainWindow();
  static nsWindow*      GetNSWindowPtr(HWND aWnd);
  static bool           SetNSWindowPtr(HWND aWnd, nsWindow* aPtr);
  void                  NS2PM(POINTL& ptl);
  void                  NS2PM(RECTL& rcl);
  void                  NS2PM_PARENT(POINTL& ptl);
  void                  ActivatePlugin(HWND aWnd);
  void                  SetPluginClipRegion(const Configuration& aConfiguration);
  HWND                  GetPluginClipWindow(HWND aParentWnd);
  void                  ActivateTopLevelWidget();
  HBITMAP               DataToBitmap(PRUint8* aImageData, PRUint32 aWidth,
                                     PRUint32 aHeight, PRUint32 aDepth);
  HBITMAP               CreateBitmapRGB(PRUint8* aImageData,
                                        PRUint32 aWidth, PRUint32 aHeight);
  HBITMAP               CreateTransparencyMask(gfxASurface::gfxImageFormat format,
                                               PRUint8* aImageData,
                                               PRUint32 aWidth, PRUint32 aHeight);
  static bool           EventIsInsideWindow(nsWindow* aWindow); 
  static bool           RollupOnButtonDown(ULONG aMsg);
  static void           RollupOnFocusLost(HWND aFocus);
  MRESULT               ProcessMessage(ULONG msg, MPARAM mp1, MPARAM mp2);
  bool                  OnReposition(PSWP pNewSwp);
  bool                  OnPaint();
  bool                  OnMouseChord(MPARAM mp1, MPARAM mp2);
  bool                  OnDragDropMsg(ULONG msg, MPARAM mp1, MPARAM mp2,
                                      MRESULT& mr);
  bool                  CheckDragStatus(PRUint32 aAction, HPS* aHps);
  bool                  ReleaseIfDragHPS(HPS aHps);
  bool                  OnTranslateAccelerator(PQMSG pQmsg);
  bool                  DispatchKeyEvent(MPARAM mp1, MPARAM mp2);
  void                  InitEvent(nsGUIEvent& event, nsIntPoint* pt = 0);
  bool                  DispatchWindowEvent(nsGUIEvent* event);
  bool                  DispatchWindowEvent(nsGUIEvent* event,
                                            nsEventStatus& aStatus);
  bool                  DispatchCommandEvent(PRUint32 aEventCommand);
  bool                  DispatchDragDropEvent(PRUint32 aMsg);
  bool                  DispatchMoveEvent(PRInt32 aX, PRInt32 aY);
  bool                  DispatchResizeEvent(PRInt32 aClientX, 
                                            PRInt32 aClientY);
  bool                  DispatchMouseEvent(PRUint32 aEventType,
                                           MPARAM mp1, MPARAM mp2, 
                                           bool aIsContextMenuKey = false,
                                           PRInt16 aButton = nsMouseEvent::eLeftButton);
  bool                  DispatchActivationEvent(PRUint32 aEventType);
  bool                  DispatchScrollEvent(ULONG msg, MPARAM mp1, MPARAM mp2);

  friend MRESULT EXPENTRY fnwpNSWindow(HWND hwnd, ULONG msg,
                                       MPARAM mp1, MPARAM mp2);
  friend MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);
  friend class os2FrameWindow;

  HWND          mWnd;               // window handle
  nsWindow*     mParent;            // parent widget
  os2FrameWindow* mFrame;           // ptr to os2FrameWindow helper object
  PRInt32       mWindowState;       // current nsWindowState_* value
  bool          mIsDestroying;      // in destructor
  bool          mInSetFocus;        // prevent recursive calls
  bool          mNoPaint;           // true if window is never visible
  HPS           mDragHps;           // retrieved by DrgGetPS() during a drag
  PRUint32      mDragStatus;        // set when object is being dragged over
  HWND          mClipWnd;           // used to clip plugin windows
  HPOINTER      mCssCursorHPtr;     // created by SetCursor(imgIContainer*)
  nsCOMPtr<imgIContainer> mCssCursorImg;// saved by SetCursor(imgIContainer*)
  nsRefPtr<gfxOS2Surface> mThebesSurface;
#ifdef DEBUG_FOCUS
  int           mWindowIdentifier;  // a serial number for each new window
#endif
  InputContext  mInputContext;
};

//=============================================================================
//  nsChildWindow
//=============================================================================

// This only purpose of this subclass is to map NS_CHILD_CID to
// some class other than nsWindow which is mapped to NS_WINDOW_CID.

class nsChildWindow : public nsWindow {
public:
  nsChildWindow()       {}
  ~nsChildWindow()      {}
};

#endif // _nswindow_h

//=============================================================================

