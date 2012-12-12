/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include <os2im.h>

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
                               nsWidgetInitData* aInitData = nullptr);
  NS_IMETHOD            Destroy();
  virtual nsIWidget*    GetParent();
  virtual float         GetDPI();
  NS_IMETHOD            Enable(bool aState);
  virtual bool          IsEnabled() const;
  NS_IMETHOD            Show(bool aState);
  virtual bool          IsVisible() const;
  NS_IMETHOD            SetFocus(bool aRaise);
  NS_IMETHOD            Invalidate(const nsIntRect& aRect);
  gfxASurface*          GetThebesSurface();
  virtual void*         GetNativeData(uint32_t aDataType);
  virtual void          FreeNativeData(void* aDatum, uint32_t aDataType);
  NS_IMETHOD            CaptureMouse(bool aCapture);
  virtual bool          HasPendingInputEvent();
  NS_IMETHOD            GetBounds(nsIntRect& aRect);
  NS_IMETHOD            GetClientBounds(nsIntRect& aRect);
  virtual nsIntPoint    WidgetToScreenOffset();
  NS_IMETHOD            Move(double aX, double aY);
  NS_IMETHOD            Resize(double aWidth, double aHeight,
                               bool    aRepaint);
  NS_IMETHOD            Resize(double aX, double aY,
                               double aWidth, double aHeight,
                               bool    aRepaint);
  NS_IMETHOD            PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                    nsIWidget* aWidget, bool aActivate);
  NS_IMETHOD            SetZIndex(int32_t aZIndex);
  virtual nsresult      ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD            SetSizeMode(int32_t aMode);
  NS_IMETHOD            HideWindowChrome(bool aShouldHide);
  NS_IMETHOD            SetTitle(const nsAString& aTitle); 
  NS_IMETHOD            SetIcon(const nsAString& aIconSpec); 
  NS_IMETHOD            ConstrainPosition(bool aAllowSlop,
                                          int32_t* aX, int32_t* aY);
  NS_IMETHOD            SetCursor(nsCursor aCursor);
  NS_IMETHOD            SetCursor(imgIContainer* aCursor,
                                  uint32_t aHotspotX, uint32_t aHotspotY);
  NS_IMETHOD            CaptureRollupEvents(nsIRollupListener* aListener,
                                            bool aDoCapture, bool aConsumeRollupEvent);
  NS_IMETHOD            GetToggledKeyState(uint32_t aKeyCode,
                                           bool* aLEDState);
  NS_IMETHOD            DispatchEvent(nsGUIEvent* event,
                                      nsEventStatus& aStatus);
  NS_IMETHOD            ReparentNativeWidget(nsIWidget* aNewParent);

  NS_IMETHOD_(void)     SetInputContext(const InputContext& aInputContext,
                                        const InputContextAction& aAction)
  {
    mInputContext = aInputContext;
  }
  NS_IMETHOD_(InputContext) GetInputContext();

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
  HBITMAP               DataToBitmap(uint8_t* aImageData, uint32_t aWidth,
                                     uint32_t aHeight, uint32_t aDepth);
  HBITMAP               CreateBitmapRGB(uint8_t* aImageData,
                                        uint32_t aWidth, uint32_t aHeight);
  HBITMAP               CreateTransparencyMask(gfxASurface::gfxImageFormat format,
                                               uint8_t* aImageData,
                                               uint32_t aWidth, uint32_t aHeight);
  static bool           EventIsInsideWindow(nsWindow* aWindow); 
  static bool           RollupOnButtonDown(ULONG aMsg);
  static void           RollupOnFocusLost(HWND aFocus);
  MRESULT               ProcessMessage(ULONG msg, MPARAM mp1, MPARAM mp2);
  bool                  OnReposition(PSWP pNewSwp);
  bool                  OnPaint();
  bool                  OnMouseChord(MPARAM mp1, MPARAM mp2);
  bool                  OnDragDropMsg(ULONG msg, MPARAM mp1, MPARAM mp2,
                                      MRESULT& mr);
  bool                  CheckDragStatus(uint32_t aAction, HPS* aHps);
  bool                  ReleaseIfDragHPS(HPS aHps);
  bool                  OnTranslateAccelerator(PQMSG pQmsg);
  bool                  OnQueryConvertPos(MPARAM mp1, MRESULT& mresult);
  bool                  ImeResultString(HIMI himi);
  bool                  ImeConversionString(HIMI himi);
  bool                  OnImeRequest(MPARAM mp1, MPARAM mp2);
  bool                  DispatchKeyEvent(MPARAM mp1, MPARAM mp2);
  void                  InitEvent(nsGUIEvent& event, nsIntPoint* pt = 0);
  bool                  DispatchWindowEvent(nsGUIEvent* event);
  bool                  DispatchWindowEvent(nsGUIEvent* event,
                                            nsEventStatus& aStatus);
  bool                  DispatchCommandEvent(uint32_t aEventCommand);
  bool                  DispatchDragDropEvent(uint32_t aMsg);
  bool                  DispatchMoveEvent(int32_t aX, int32_t aY);
  bool                  DispatchResizeEvent(int32_t aClientX, 
                                            int32_t aClientY);
  bool                  DispatchMouseEvent(uint32_t aEventType,
                                           MPARAM mp1, MPARAM mp2, 
                                           bool aIsContextMenuKey = false,
                                           int16_t aButton = nsMouseEvent::eLeftButton);
  bool                  DispatchActivationEvent(uint32_t aEventType);
  bool                  DispatchScrollEvent(ULONG msg, MPARAM mp1, MPARAM mp2);

  friend MRESULT EXPENTRY fnwpNSWindow(HWND hwnd, ULONG msg,
                                       MPARAM mp1, MPARAM mp2);
  friend MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);
  friend class os2FrameWindow;

  HWND          mWnd;               // window handle
  nsWindow*     mParent;            // parent widget
  os2FrameWindow* mFrame;           // ptr to os2FrameWindow helper object
  int32_t       mWindowState;       // current nsWindowState_* value
  bool          mIsDestroying;      // in destructor
  bool          mInSetFocus;        // prevent recursive calls
  bool          mNoPaint;           // true if window is never visible
  HPS           mDragHps;           // retrieved by DrgGetPS() during a drag
  uint32_t      mDragStatus;        // set when object is being dragged over
  HWND          mClipWnd;           // used to clip plugin windows
  HPOINTER      mCssCursorHPtr;     // created by SetCursor(imgIContainer*)
  nsCOMPtr<imgIContainer> mCssCursorImg;// saved by SetCursor(imgIContainer*)
  nsRefPtr<gfxOS2Surface> mThebesSurface;
  bool          mIsComposing;
  nsString      mLastDispatchedCompositionString;
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

