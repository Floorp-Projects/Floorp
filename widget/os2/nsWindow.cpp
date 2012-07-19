/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//=============================================================================
/*
 *  This file is divided into the following major sections:
 *
 *  - Macros
 *  - Variables & Forward declarations
 *  - nsWindow Create / Destroy
 *  - Standard Window Operations
 *  - Window Positioning
 *  - Plugin Operations
 *  - Top-level (frame window) Operations
 *  - Mouse Pointers
 *  - Rollup Event Handlers
 *  - nsWindow's Window Procedure
 *  - Window Message Handlers
 *  - Drag & Drop - Target methods
 *  - Keyboard Handlers
 *  - IME
 *  - Event Dispatch
 *
 */
//=============================================================================

#include "nsWindow.h"
#include "os2FrameWindow.h"
#include "gfxContext.h"
#include "gfxOS2Surface.h"
#include "imgIContainer.h"
#include "npapi.h"
#include "nsDragService.h"
#include "nsGfxCIID.h"
#include "nsHashKeys.h"
#include "nsIRollupListener.h"
#include "nsIScreenManager.h"
#include "nsOS2Uni.h"
#include "nsTHashtable.h"
#include "nsGkAtoms.h"
#include "wdgtos2rc.h"
#include "mozilla/Preferences.h"
#include <os2im.h>

using namespace mozilla;
//=============================================================================
//  Macros
//=============================================================================

// Drag and Drop

// d&d flags - actions that might cause problems during d&d
#define ACTION_PAINT    1
#define ACTION_DRAW     2
#define ACTION_SCROLL   3
#define ACTION_SHOW     4
#define ACTION_PTRPOS   5

// d&d status - shorten these references a bit
#define DND_None                (nsIDragSessionOS2::DND_NONE)
#define DND_NativeDrag          (nsIDragSessionOS2::DND_NATIVEDRAG)
#define DND_MozDrag             (nsIDragSessionOS2::DND_MOZDRAG)
#define DND_InDrop              (nsIDragSessionOS2::DND_INDROP)
#define DND_DragStatus          (nsIDragSessionOS2::DND_DRAGSTATUS)
#define DND_DispatchEnterEvent  (nsIDragSessionOS2::DND_DISPATCHENTEREVENT)
#define DND_DispatchEvent       (nsIDragSessionOS2::DND_DISPATCHEVENT)
#define DND_GetDragoverResult   (nsIDragSessionOS2::DND_GETDRAGOVERRESULT)
#define DND_ExitSession         (nsIDragSessionOS2::DND_EXITSESSION)

//-----------------------------------------------------------------------------
// App Command messages for IntelliMouse and Natural Keyboard Pro

#define WM_APPCOMMAND                   0x0319

#define APPCOMMAND_BROWSER_BACKWARD     1
#define APPCOMMAND_BROWSER_FORWARD      2
#define APPCOMMAND_BROWSER_REFRESH      3
#define APPCOMMAND_BROWSER_STOP         4

//-----------------------------------------------------------------------------
// Keyboard-related macros

// Used for character-to-keycode translation
#define PMSCAN_PADMULT      0x37
#define PMSCAN_PAD7         0x47
#define PMSCAN_PAD8         0x48
#define PMSCAN_PAD9         0x49
#define PMSCAN_PADMINUS     0x4A
#define PMSCAN_PAD4         0x4B
#define PMSCAN_PAD5         0x4C
#define PMSCAN_PAD6         0x4D
#define PMSCAN_PADPLUS      0x4E
#define PMSCAN_PAD1         0x4F
#define PMSCAN_PAD2         0x50
#define PMSCAN_PAD3         0x51
#define PMSCAN_PAD0         0x52
#define PMSCAN_PADPERIOD    0x53
#define PMSCAN_PADDIV       0x5c

#define isNumPadScanCode(scanCode) !((scanCode < PMSCAN_PAD7) ||      \
                                     (scanCode > PMSCAN_PADPERIOD) || \
                                     (scanCode == PMSCAN_PADMULT) ||  \
                                     (scanCode == PMSCAN_PADDIV) ||   \
                                     (scanCode == PMSCAN_PADMINUS) || \
                                     (scanCode == PMSCAN_PADPLUS))

#define isNumlockOn     (WinGetKeyState(HWND_DESKTOP, VK_NUMLOCK) & 0x0001)
#define isKeyDown(vk)   ((WinGetKeyState(HWND_DESKTOP,vk) & 0x8000) == 0x8000)

//-----------------------------------------------------------------------------
// Miscellanea

// extract X & Y from a mouse msg mparam
#define XFROMMP(m)      (SHORT(LOUSHORT(m)))
#define YFROMMP(m)      (SHORT(HIUSHORT(m)))

// make these methods seem more appropriate in context
#define PM2NS_PARENT NS2PM_PARENT
#define PM2NS NS2PM

// used to identify plugin widgets (copied from nsPluginNativeWindowOS2.cpp)
#define NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION \
                        "MozillaPluginWindowPropertyAssociation"

// name of the window class used to clip plugins
#define kClipWndClass   "nsClipWnd"

//-----------------------------------------------------------------------------
// Debug

#ifdef DEBUG_FOCUS
  #define DEBUGFOCUS(what) fprintf(stderr, "[%8x]  %8lx  (%02d)  "#what"\n", \
                                   (int)this, mWnd, mWindowIdentifier)
#else
  #define DEBUGFOCUS(what)
#endif

//=============================================================================
//  Variables & Forward declarations
//=============================================================================

// Rollup Listener - used by nsWindow & os2FrameWindow
nsIRollupListener*  gRollupListener           = 0;
nsIWidget*          gRollupWidget             = 0;
bool                gRollupConsumeRollupEvent = false;

// Miscellaneous global flags
PRUint32            gOS2Flags = 0;

// Mouse pointers
static HPOINTER     sPtrArray[IDC_COUNT];

// location of last MB1 down - used for mouse-based copy/paste
static POINTS       sLastButton1Down = {0,0};

// set when any nsWindow is being dragged over
static PRUint32     sDragStatus = 0;

#ifdef DEBUG_FOCUS
  int currentWindowIdentifier = 0;
#endif
// IME stuffs
static HMODULE sIm32Mod = NULLHANDLE;
static APIRET (APIENTRY *spfnImGetInstance)(HWND, PHIMI);
static APIRET (APIENTRY *spfnImReleaseInstance)(HWND, HIMI);
static APIRET (APIENTRY *spfnImGetConversionString)(HIMI, ULONG, PVOID,
                                                    PULONG);
static APIRET (APIENTRY *spfnImGetResultString)(HIMI, ULONG, PVOID, PULONG);
static APIRET (APIENTRY *spfnImRequestIME)(HIMI, ULONG, ULONG, ULONG);

//-----------------------------------------------------------------------------
static PRUint32     WMChar2KeyCode(MPARAM mp1, MPARAM mp2);

//=============================================================================
//  nsWindow Create / Destroy
//=============================================================================

nsWindow::nsWindow() : nsBaseWidget()
{
  mWnd                = 0;
  mParent             = 0;
  mFrame              = 0;
  mWindowType         = eWindowType_toplevel;
  mBorderStyle        = eBorderStyle_default;
  mWindowState        = nsWindowState_ePrecreate;
  mOnDestroyCalled    = false;
  mIsDestroying       = false;
  mInSetFocus         = false;
  mNoPaint            = false;
  mDragHps            = 0;
  mDragStatus         = 0;
  mClipWnd            = 0;
  mCssCursorHPtr      = 0;
  mThebesSurface      = 0;
  mIsComposing        = false;
  if (!gOS2Flags) {
    InitGlobals();
  }
}

//-----------------------------------------------------------------------------

nsWindow::~nsWindow()
{
  // How destruction works: A call of Destroy() destroys the PM window.  This
  // triggers an OnDestroy(), which frees resources.  If not Destroy'd at
  // delete time, Destroy() gets called anyway.

  // NOTE: Calling virtual functions from destructors is bad; they always
  //       bind in the current object (ie. as if they weren't virtual).  It
  //       may even be illegal to call them from here.

  mIsDestroying = true;

  if (mCssCursorHPtr) {
    WinDestroyPointer(mCssCursorHPtr);
    mCssCursorHPtr = 0;
  }

  // If the widget was released without calling Destroy() then
  // the native window still exists, and we need to destroy it
  if (!(mWindowState & nsWindowState_eDead)) {
    mWindowState |= nsWindowState_eDoingDelete;
    mWindowState &= ~(nsWindowState_eLive | nsWindowState_ePrecreate |
                      nsWindowState_eInCreate);
    Destroy();
  }

  // Once a plugin window has been destroyed,
  // its parent, the clipping window, can be destroyed.
  if (mClipWnd) {
    WinDestroyWindow(mClipWnd);
    mClipWnd = 0;
  }
 
  // If it exists, destroy our os2FrameWindow helper object.
  if (mFrame) {
    delete mFrame;
    mFrame = 0;
  }
}

//-----------------------------------------------------------------------------
// Init Module-level variables.

// static
void nsWindow::InitGlobals()
{
  gOS2Flags = kIsInitialized;

  // Register the MozillaWindowClass with PM.
  WinRegisterClass(0, kWindowClassName, fnwpNSWindow, 0, 8);

  // Register the dummy window class used to clip plugins.
  WinRegisterClass(0, kClipWndClass, WinDefWindowProc, 0, 4);

  // Load the mouse pointers from the dll containing 'gOS2Flags'.
  HMODULE hModResources = 0;
  DosQueryModFromEIP(&hModResources, 0, 0, 0, 0, (ULONG)&gOS2Flags);
  for (int i = 0; i < IDC_COUNT; i++) {
    sPtrArray[i] = WinLoadPointer(HWND_DESKTOP, hModResources, IDC_BASE+i);
  }

  // Work out if the system is DBCS.
  char buffer[16];
  COUNTRYCODE cc = { 0 };
  DosQueryDBCSEnv(sizeof(buffer), &cc, buffer);
  if (buffer[0] || buffer[1]) {
    gOS2Flags |= kIsDBCS;
  }

  // This is ugly. The Thinkpad TrackPoint driver checks to see whether
  // or not a window actually has a scroll bar as a child before sending
  // it scroll messages. Needless to say, no Mozilla window has real scroll
  // bars. So if you have the "os2.trackpoint" preference set, we put an
  // invisible scroll bar on every child window so we can scroll.
  if (Preferences::GetBool("os2.trackpoint", false)) {
    gOS2Flags |= kIsTrackPoint;
  }

  InitIME();
}

//-----------------------------------------------------------------------------
// Determine whether to use IME
static
void InitIME()
{
  if (!getenv("MOZ_IME_OVERTHESPOT")) {
    CHAR szName[CCHMAXPATH];
    ULONG rc;

    rc = DosLoadModule(szName, sizeof(szName), "os2im", &sIm32Mod);

    if (!rc)
      rc = DosQueryProcAddr(sIm32Mod, 104, NULL,
                            (PFN *)&spfnImGetInstance);

    if (!rc)
      rc = DosQueryProcAddr(sIm32Mod, 106, NULL,
                            (PFN *)&spfnImReleaseInstance);

    if (!rc)
      rc = DosQueryProcAddr(sIm32Mod, 118, NULL,
                            (PFN *)&spfnImGetConversionString);

    if (!rc)
      rc = DosQueryProcAddr(sIm32Mod, 122, NULL,
                            (PFN *)&spfnImGetResultString);

    if (!rc)
      rc = DosQueryProcAddr(sIm32Mod, 131, NULL,
                            (PFN *)&spfnImRequestIME);

    if (rc) {
      DosFreeModule(sIm32Mod);

      sIm32Mod = NULLHANDLE;
    }
  }
}
//-----------------------------------------------------------------------------
// Release Module-level variables.

// static
void nsWindow::ReleaseGlobals()
{
  for (int i = 0; i < IDC_COUNT; i++) {
    WinDestroyPointer(sPtrArray[i]);
  }
}

//-----------------------------------------------------------------------------
// Init an nsWindow & create the appropriate native window.

NS_METHOD nsWindow::Create(nsIWidget* aParent,
                           nsNativeWidget aNativeParent,
                           const nsIntRect& aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsDeviceContext* aContext,
                           nsWidgetInitData* aInitData)
{
  mWindowState = nsWindowState_eInCreate;

  // Identify the parent's nsWindow & native window.  Only one of these
  // should be supplied.  Note:  only nsWindow saves pParent as mParent;
  // os2FrameWindow discards it since toplevel widgets have no parent.
  HWND      hParent;
  nsWindow* pParent;
  if (aParent) {
    hParent = (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW);
    pParent = (nsWindow*)aParent;
  } else {
    if (aNativeParent && (HWND)aNativeParent != HWND_DESKTOP) {
      hParent = (HWND)aNativeParent;
      pParent = GetNSWindowPtr(hParent);
    } else {
      hParent = HWND_DESKTOP;
      pParent = 0;
    }
  }

  BaseCreate(aParent, aRect, aHandleEventFunction, aContext, aInitData);

#ifdef DEBUG_FOCUS
  mWindowIdentifier = currentWindowIdentifier;
  currentWindowIdentifier++;
#endif

  // Some basic initialization.
  if (aInitData) {
    // Suppress creation of a Thebes surface for windows that will never
    // be painted because they're always covered by another window.
    if (mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_invisible) {
      mNoPaint = true;
    }
    // Popup windows should not have an nsWindow parent.
    else if (mWindowType == eWindowType_popup) {
      pParent = 0;
    }
  }

  // For toplevel windows, create an instance of our helper class,
  // then have it create a frame & client window;  otherwise,
  // call our own CreateWindow() method to create a child window.
  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog   ||
      mWindowType == eWindowType_invisible) {
    mFrame = new os2FrameWindow(this);
    NS_ENSURE_TRUE(mFrame, NS_ERROR_FAILURE);
    mWnd = mFrame->CreateFrameWindow(pParent, hParent, aRect,
                                     mWindowType, mBorderStyle);
    NS_ENSURE_TRUE(mWnd, NS_ERROR_FAILURE);
  } else {
    nsresult rv = CreateWindow(pParent, hParent, aRect, aInitData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Store a pointer to this object in the window's extra bytes.
  SetNSWindowPtr(mWnd, this);

  // Finalize the widget creation process.
  nsGUIEvent event(true, NS_CREATE, this);
  InitEvent(event);
  DispatchWindowEvent(&event);

  mWindowState = nsWindowState_eLive;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Create a native window for an nsWindow object.

nsresult nsWindow::CreateWindow(nsWindow* aParent,
                                HWND aParentWnd,
                                const nsIntRect& aRect,
                                nsWidgetInitData* aInitData)
{
  // For pop-ups, the Desktop is the parent and aParentWnd is the owner.
  HWND hOwner = 0;
  if (mWindowType == eWindowType_popup && aParentWnd != HWND_DESKTOP) {
    hOwner = aParentWnd;
    aParentWnd = HWND_DESKTOP;
  }

  // While we comply with the clipSiblings flag, we always set
  // clipChildren regardless of the flag for performance reasons.
  PRUint32 style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  if (aInitData && !aInitData->clipSiblings) {
    style &= ~WS_CLIPSIBLINGS;
  }

  // Create the window hidden;  it will be resized below.
  mWnd = WinCreateWindow(aParentWnd,
                         kWindowClassName,
                         0,
                         style,
                         0, 0, 0, 0,
                         hOwner,
                         HWND_TOP,
                         0,
                         0, 0);
  NS_ENSURE_TRUE(mWnd, NS_ERROR_FAILURE);

  // If a TrackPoint is in use, create dummy scrollbars.
  // XXX  Popups may need this also to scroll comboboxes.
  if ((gOS2Flags & kIsTrackPoint) && mWindowType == eWindowType_child) {
    WinCreateWindow(mWnd, WC_SCROLLBAR, 0, SBS_VERT,
                    0, 0, 0, 0, mWnd, HWND_TOP,
                    FID_VERTSCROLL, 0, 0);
  }

  // Store the window's dimensions, then resize accordingly.
  mBounds = aRect;
  nsIntRect parRect;
  if (aParent) {
    aParent->GetBounds(parRect);
  } else {
    parRect.height = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
  }
  WinSetWindowPos(mWnd, 0,
                  aRect.x, parRect.height - aRect.y - aRect.height,
                  aRect.width, aRect.height, SWP_SIZE | SWP_MOVE);

  // Store the widget's parent and add it to the parent's list of children.
  // Don't ADDREF mParent because AddChild() ADDREFs us.
  mParent = aParent;
  if (mParent) {
    mParent->AddChild(this);
  }

  DEBUGFOCUS(Create nsWindow);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Close this nsWindow.

NS_METHOD nsWindow::Destroy()
{
  // avoid calling into other objects if we're being deleted, 'cos
  // they must have no references to us.
  if ((mWindowState & nsWindowState_eLive) && mParent) {
    nsBaseWidget::Destroy();
  }

  // just to be safe. If we're going away and for some reason we're still
  // the rollup widget, rollup and turn off capture.
  if (this == gRollupWidget) {
    if (gRollupListener) {
      gRollupListener->Rollup(PR_UINT32_MAX);
    }
    CaptureRollupEvents(nsnull, false, true);
  }

  HWND hMain = GetMainWindow();
  if (hMain) {
    DEBUGFOCUS(Destroy);
    if (hMain == WinQueryFocus(HWND_DESKTOP)) {
      WinSetFocus(HWND_DESKTOP, WinQueryWindow(hMain, QW_PARENT));
    }
    WinDestroyWindow(hMain);
  }
  return NS_OK;
}

//=============================================================================
//  Standard Window Operations
//=============================================================================

// This can't be inlined in nsWindow.h because it doesn't know about
// GetFrameWnd().

inline HWND nsWindow::GetMainWindow()
{
  return mFrame ? mFrame->GetFrameWnd() : mWnd;
}

//-----------------------------------------------------------------------------
// Inline this here for consistency (and a cleaner looking .h).

// static
inline nsWindow* nsWindow::GetNSWindowPtr(HWND aWnd)
{
  return (nsWindow*)WinQueryWindowPtr(aWnd, QWL_NSWINDOWPTR);
}

//-----------------------------------------------------------------------------

// static
inline bool nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow* aPtr)
{
  return WinSetWindowPtr(aWnd, QWL_NSWINDOWPTR, aPtr);
}

//-----------------------------------------------------------------------------

nsIWidget* nsWindow::GetParent()
{
  // if this window isn't supposed to have a parent or it doesn't have
  // a parent, or if it or its parent is being destroyed, return null
  if (mFrame || mIsDestroying || mOnDestroyCalled ||
      !mParent || mParent->mIsDestroying) {
    return 0;
  }

  return mParent;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Enable(bool aState)
{
  HWND hMain = GetMainWindow();
  if (hMain) {
    WinEnableWindow(hMain, aState);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::IsEnabled(bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  HWND hMain = GetMainWindow();
  *aState = !hMain || WinIsWindowEnabled(hMain);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Show(bool aState)
{
  if (mFrame) {
    return mFrame->Show(aState);
  }
  if (mWnd) {
    if (aState) {
      // don't try to show new windows (e.g. the Bookmark menu)
      // during a native dragover because they'll remain invisible;
      if (CheckDragStatus(ACTION_SHOW, 0)) {
        if (!IsVisible()) {
          PlaceBehind(eZPlacementTop, 0, false);
        }
        WinShowWindow(mWnd, true);
      }
    } else {
      WinShowWindow(mWnd, false);
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------

bool nsWindow::IsVisible() const
{
  return WinIsWindowVisible(GetMainWindow());
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::SetFocus(bool aRaise)
{
  // for toplevel windows, this is directed to the client (i.e. mWnd)
  if (mWnd) {
    if (!mInSetFocus) {
      DEBUGFOCUS(SetFocus);
      mInSetFocus = true;
      WinSetFocus(HWND_DESKTOP, mWnd);
      mInSetFocus = false;
    }
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Invalidate(const nsIntRect& aRect)
{
  if (mWnd) {
    RECTL rcl = {aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height};
    NS2PM(rcl);
    WinInvalidateRect(mWnd, &rcl, false);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Create a Thebes surface using the current window handle.

gfxASurface* nsWindow::GetThebesSurface()
{
  if (mWnd && !mThebesSurface) {
    mThebesSurface = new gfxOS2Surface(mWnd);
  }
  return mThebesSurface;
}

//-----------------------------------------------------------------------------
// Internal-only method that suppresses creation of a Thebes surface
// for windows that aren't supposed to be visible.  If one was created
// by an external call to GetThebesSurface(), it will be returned.

gfxASurface* nsWindow::ConfirmThebesSurface()
{
  if (!mThebesSurface && !mNoPaint && mWnd) {
    mThebesSurface = new gfxOS2Surface(mWnd);
  }
  return mThebesSurface;
}

//-----------------------------------------------------------------------------

float nsWindow::GetDPI()
{
  static PRInt32 sDPI = 0;

  // Create DC compatible with the screen, then query the DPI setting.
  // If this fails, fall back to something sensible.
  if (!sDPI) {
    HDC dc = DevOpenDC(0, OD_MEMORY,"*",0L, 0, 0);
    if (dc > 0) {
      LONG lDPI;
      if (DevQueryCaps(dc, CAPS_VERTICAL_FONT_RES, 1, &lDPI))
        sDPI = lDPI;
      DevCloseDC(dc);
    }
    if (sDPI <= 0) {
      sDPI = 96;
    }
  }
  return sDPI;  
}

//-----------------------------------------------------------------------------
// Return some native data according to aDataType.

void* nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType) {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
      return (void*)mWnd;

    // during a native drag over the current window or any drag
    // originating in Moz, return a drag HPS to avoid screen corruption;
    case NS_NATIVE_GRAPHIC: {
      HPS hps = 0;
      CheckDragStatus(ACTION_DRAW, &hps);
      if (!hps) {
        hps = WinGetPS(mWnd);
      }
      return (void*)hps;
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

void nsWindow::FreeNativeData(void* data, PRUint32 aDataType)
{
  // an HPS is the only native data that needs to be freed
  if (aDataType == NS_NATIVE_GRAPHIC &&
      data &&
      !ReleaseIfDragHPS((HPS)data)) {
    WinReleasePS((HPS)data);
  }
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::CaptureMouse(bool aCapture)
{
  if (aCapture) {
    WinSetCapture(HWND_DESKTOP, mWnd);
  } else {
    WinSetCapture(HWND_DESKTOP, 0);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

bool nsWindow::HasPendingInputEvent()
{
  return (WinQueryQueueStatus(HWND_DESKTOP) & (QS_KEY | QS_MOUSE)) != 0;
}

//=============================================================================
//  Window Positioning
//=============================================================================

// For toplevel windows, mBounds contains the dimensions of the client
// window.  os2FrameWindow's "override" returns the size of the frame.

NS_METHOD nsWindow::GetBounds(nsIntRect& aRect)
{
  if (mFrame) {
    return mFrame->GetBounds(aRect);
  }
  aRect = mBounds;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Since mBounds contains the dimensions of the client, os2FrameWindow
// doesn't have to provide any special handling for this method.

NS_METHOD nsWindow::GetClientBounds(nsIntRect& aRect)
{
  aRect = mBounds;
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsIntPoint nsWindow::WidgetToScreenOffset()
{
  POINTL point = { 0, 0 };
  NS2PM(point);

  WinMapWindowPoints(mWnd, HWND_DESKTOP, &point, 1);
  return nsIntPoint(point.x,
                    WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - point.y - 1);
}

//-----------------------------------------------------------------------------
// Transform Y values between PM & XP coordinate systems.

// ptl is in this window's space
void nsWindow::NS2PM(POINTL& ptl)
{
  ptl.y = mBounds.height - ptl.y - 1;
}

// rcl is in this window's space
void nsWindow::NS2PM(RECTL& rcl)
{
  LONG height = rcl.yTop - rcl.yBottom;
  rcl.yTop = mBounds.height - rcl.yBottom;
  rcl.yBottom = rcl.yTop - height;
}

// ptl is in parent's space
void nsWindow::NS2PM_PARENT(POINTL& ptl)
{
  if (mParent) {
    mParent->NS2PM(ptl);
  } else {
    HWND hParent = WinQueryWindow(mWnd, QW_PARENT);
    SWP  swp;
    WinQueryWindowPos(hParent, &swp);
    ptl.y = swp.cy - ptl.y - 1;
  }
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  if (mFrame) {
    nsresult rv = mFrame->Move(aX, aY);
    NotifyRollupGeometryChange(gRollupListener);
    return rv;
  }
  Resize(aX, aY, mBounds.width, mBounds.height, false);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, bool aRepaint)
{
  if (mFrame) {
    nsresult rv = mFrame->Resize(aWidth, aHeight, aRepaint);
    NotifyRollupGeometryChange(gRollupListener);
    return rv;
  }
  Resize(mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight, bool aRepaint)
{
  if (mFrame) {
    nsresult rv = mFrame->Resize(aX, aY, aWidth, aHeight, aRepaint);
    NotifyRollupGeometryChange(gRollupListener);
    return rv;
  }

  // For mWnd & eWindowType_child set the cached values upfront, see bug 286555.
  // For other mWnd types we defer transfer of values to mBounds to
  // WinSetWindowPos(), see bug 391421.

  if (!mWnd ||
      mWindowType == eWindowType_child ||
      mWindowType == eWindowType_plugin) {
    mBounds.x      = aX;
    mBounds.y      = aY;
    mBounds.width  = aWidth;
    mBounds.height = aHeight;
  }

  // To keep top-left corner in the same place, use the new height
  // to calculate the coordinates for the top & bottom left corners.
  if (mWnd) {
    POINTL ptl = { aX, aY };
    NS2PM_PARENT(ptl);
    ptl.y -= aHeight - 1;

    // For popups, aX already gives the correct position.
    if (mWindowType == eWindowType_popup) {
      ptl.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - aHeight - 1 - aY;
    }
    else if (mParent) {
      WinMapWindowPoints(mParent->mWnd, WinQueryWindow(mWnd, QW_PARENT),
                         &ptl, 1);
    }

    if (!WinSetWindowPos(mWnd, 0, ptl.x, ptl.y, aWidth, aHeight,
                         SWP_MOVE | SWP_SIZE) && aRepaint) {
      WinInvalidateRect(mWnd, 0, FALSE);
    }
  }

  NotifyRollupGeometryChange(gRollupListener);
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                nsIWidget* aWidget, bool aActivate)
{
  HWND hBehind = HWND_TOP;

  if (aPlacement == eZPlacementBottom) {
    hBehind = HWND_BOTTOM;
  } else
  if (aPlacement == eZPlacementBelow && aWidget) {
    hBehind = (static_cast<nsWindow*>(aWidget))->GetMainWindow();
  }

  PRUint32 flags = SWP_ZORDER;
  if (aActivate) {
    flags |= SWP_ACTIVATE;
  }

  WinSetWindowPos(GetMainWindow(), hBehind, 0, 0, 0, 0, flags);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Set widget's position within its parent child list.

NS_METHOD nsWindow::SetZIndex(PRInt32 aZIndex)
{
  // nsBaseWidget::SetZIndex() never has done anything sensible but
  // has randomly placed widgets behind others (see bug 117730#c25).
  // To get bug #353011 solved simply override it here to do nothing.
  return NS_OK;
}

//=============================================================================
//  Plugin Operations
//=============================================================================

// Fire an NS_PLUGIN_ACTIVATE event whenever a window associated
// with a plugin widget get the focus.

void nsWindow::ActivatePlugin(HWND aWnd)
{
  // avoid acting on recursive WM_FOCUSCHANGED msgs
  static bool inPluginActivate = FALSE;
  if (inPluginActivate) {
    return;
  }

  // This property is used by the plugin window to store a pointer
  // to its plugin object.  We just use it as a convenient marker.
  if (!WinQueryProperty(mWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION)) {
    return;
  }

  // Fire a plugin activation event on the plugin widget.
  inPluginActivate = TRUE;
  DEBUGFOCUS(NS_PLUGIN_ACTIVATE);
  DispatchActivationEvent(NS_PLUGIN_ACTIVATE);

  // Activating the plugin moves the focus off the child that had it,
  // so try to restore it.  If the WM_FOCUSCHANGED msg was synthesized
  // by the plugin, then mp1 contains the child window that lost focus.
  // Otherwise, just move it to the plugin's first child unless this
  // is the mplayer plugin - doing so will put us into an endless loop.
  // Since its children belong to another process, use the PID as a test.
  HWND hFocus = 0;
  if (WinIsChild(aWnd, mWnd)) {
    hFocus = aWnd;
  } else {
    hFocus = WinQueryWindow(mWnd, QW_TOP);
    if (hFocus) {
      PID pidFocus, pidThis;
      TID tid;
      WinQueryWindowProcess(hFocus, &pidFocus, &tid);
      WinQueryWindowProcess(mWnd, &pidThis, &tid);
      if (pidFocus != pidThis) {
        hFocus = 0;
      }
    }
  }
  if (hFocus) {
    WinSetFocus(HWND_DESKTOP, hFocus);
  }

  inPluginActivate = FALSE;
  return;
}

//-----------------------------------------------------------------------------
// This is invoked on a window that has plugin widget children
// to resize and clip those child windows.

nsresult nsWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
    w->SetPluginClipRegion(configuration);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// This is invoked on a plugin window to resize it and set a persistent
// clipping region for it.  Since the latter isn't possible on OS/2, it
// inserts a dummy window between the plugin widget and its parent to
// act as a clipping rectangle.  The dummy window's dimensions and the
// plugin widget's position within the window are adjusted to correspond
// to the bounding box of the supplied array of clipping rectangles.
// Note: this uses PM calls rather than existing methods like Resize()
// and Update() because none of them support the options needed here.

void nsWindow::SetPluginClipRegion(const Configuration& aConfiguration)
{
  NS_ASSERTION((mParent && mParent->mWnd), "Child window has no parent");

  // If nothing has changed, exit.
  if (!StoreWindowClipRegion(aConfiguration.mClipRegion) &&
      mBounds.IsEqualInterior(aConfiguration.mBounds)) {
    return;
  }

  // Set the widget's x/y to its nominal unclipped value.  It doesn't
  // affect our calculations but other code relies on it being correct.
  mBounds.MoveTo(aConfiguration.mBounds.TopLeft());

  // Get or create the PM window we use as a clipping rectangle.
  HWND hClip = GetPluginClipWindow(mParent->mWnd);
  NS_ASSERTION(hClip, "No clipping window for plugin");
  if (!hClip) {
    return;
  }

  // Create the bounding box for the clip region.
  const nsTArray<nsIntRect>& rects = aConfiguration.mClipRegion;
  nsIntRect r;
  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    r.UnionRect(r, rects[i]);
  }

  // Size and position hClip to match the bounding box.
  SWP    swp;
  POINTL ptl;
  WinQueryWindowPos(hClip, &swp);
  ptl.x = aConfiguration.mBounds.x + r.x;
  ptl.y = mParent->mBounds.height
          - (aConfiguration.mBounds.y + r.y + r.height);

  ULONG  clipFlags = 0;
  if (swp.x != ptl.x || swp.y != ptl.y) {
    clipFlags |= SWP_MOVE;
  }
  if (swp.cx != r.width || swp.cy != r.height) {
    clipFlags |= SWP_SIZE;
  }
  if (clipFlags) {
    WinSetWindowPos(hClip, 0, ptl.x, ptl.y, r.width, r.height, clipFlags);
  }

  // Reducing the size of hClip clips the right & top sides of the
  // plugin widget.  To clip the left & bottom sides, we have to move
  // the widget so its origin's x and/or y is negative wrt hClip.
  WinQueryWindowPos(mWnd, &swp);
  ptl.x = -r.x;
  ptl.y = r.height + r.y - aConfiguration.mBounds.height;

  ULONG  wndFlags = 0;
  if (swp.x != ptl.x || swp.y != ptl.y) {
    wndFlags |= SWP_MOVE;
  }
  if (mBounds.Size() != aConfiguration.mBounds.Size()) {
    wndFlags |= SWP_SIZE;
  }
  if (wndFlags) {
    WinSetWindowPos(mWnd, 0, ptl.x, ptl.y,
                    aConfiguration.mBounds.width,
                    aConfiguration.mBounds.height, wndFlags);
  }

  // Some plugins don't resize themselves when the plugin widget changes
  // size, so help them out by resizing the first child (usually a frame).
  if (wndFlags & SWP_SIZE) {
    HWND hChild = WinQueryWindow(mWnd, QW_TOP);
    if (hChild) {
      WinSetWindowPos(hChild, 0, 0, 0,
                      aConfiguration.mBounds.width,
                      aConfiguration.mBounds.height,
                      SWP_MOVE | SWP_SIZE);
    }
  }

  // When hClip is resized, mWnd and its children may not get updated
  // automatically, so invalidate & repaint them
  if (clipFlags & SWP_SIZE) {
    WinInvalidateRect(mWnd, 0, TRUE);
    WinUpdateWindow(mWnd);
  }
}

//-----------------------------------------------------------------------------
// This gets or creates a window that's inserted between the main window
// and its plugin children.  This window does nothing except act as a
// clipping rectangle for the plugin widget.

HWND nsWindow::GetPluginClipWindow(HWND aParentWnd)
{
  if (mClipWnd) {
    return mClipWnd;
  }

  // Insert a new clip window in the hierarchy between mWnd & aParentWnd.
  mClipWnd = WinCreateWindow(aParentWnd, kClipWndClass, "",
                             WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                             0, 0, 0, 0, 0, mWnd, 0, 0, 0);
  if (mClipWnd) {
    if (!WinSetParent(mWnd, mClipWnd, FALSE)) {
      WinDestroyWindow(mClipWnd);
      mClipWnd = 0;
    }
  }

  return mClipWnd;
}

//=============================================================================
//  Top-level (frame window) Operations
//=============================================================================

// When a window gets the focus, call os2FrameWindow's version of this
// method.  It will fire an NS_ACTIVATE event on the top-level widget
// if appropriate.

void nsWindow::ActivateTopLevelWidget()
{
  if (mFrame) {
    mFrame->ActivateTopLevelWidget();
  } else {
    nsWindow* top = static_cast<nsWindow*>(GetTopLevelWidget());
    if (top && top->mFrame) {
      top->mFrame->ActivateTopLevelWidget();
    }
  }
  return;
}

//-----------------------------------------------------------------------------
// All of these methods are inherently toplevel-only, and are in fact
// only invoked on toplevel widgets.  If they're invoked on a child
// window, there's an error upstream.

NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode)
{
  NS_ENSURE_TRUE(mFrame, NS_ERROR_UNEXPECTED);
  return mFrame->SetSizeMode(aMode);
}

NS_IMETHODIMP nsWindow::HideWindowChrome(bool aShouldHide)
{
  NS_ENSURE_TRUE(mFrame, NS_ERROR_UNEXPECTED);
  return mFrame->HideWindowChrome(aShouldHide);
}

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
  NS_ENSURE_TRUE(mFrame, NS_ERROR_UNEXPECTED);
  return mFrame->SetTitle(aTitle);
}

NS_METHOD nsWindow::SetIcon(const nsAString& aIconSpec)
{
  NS_ENSURE_TRUE(mFrame, NS_ERROR_UNEXPECTED);
  return mFrame->SetIcon(aIconSpec);
}

NS_METHOD nsWindow::ConstrainPosition(bool aAllowSlop,
                                      PRInt32* aX, PRInt32* aY)
{
  NS_ENSURE_TRUE(mFrame, NS_ERROR_UNEXPECTED);
  return mFrame->ConstrainPosition(aAllowSlop, aX, aY);
}

//=============================================================================
//  Mouse Pointers
//=============================================================================

// Set one of the standard mouse pointers.

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  HPOINTER newPointer = 0;

  switch (aCursor) {
    case eCursor_select:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_TEXT, FALSE);
      break;

    case eCursor_wait:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE);
      break;

    case eCursor_hyperlink:
      newPointer = sPtrArray[IDC_SELECTANCHOR-IDC_BASE];
      break;

    case eCursor_standard:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE);
      break;

    case eCursor_n_resize:
    case eCursor_s_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE);
      break;

    case eCursor_w_resize:
    case eCursor_e_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE);
      break;

    case eCursor_nw_resize:
    case eCursor_se_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENWSE, FALSE);
      break;

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENESW, FALSE);
      break;

    case eCursor_crosshair:
      newPointer = sPtrArray[IDC_CROSS-IDC_BASE];
      break;

    case eCursor_move:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE);
      break;

    case eCursor_help:
      newPointer = sPtrArray[IDC_HELP-IDC_BASE];
      break;

    case eCursor_copy: // CSS3
      newPointer = sPtrArray[IDC_COPY-IDC_BASE];
      break;

    case eCursor_alias:
      newPointer = sPtrArray[IDC_ALIAS-IDC_BASE];
      break;

    case eCursor_cell:
      newPointer = sPtrArray[IDC_CELL-IDC_BASE];
      break;

    case eCursor_grab:
      newPointer = sPtrArray[IDC_GRAB-IDC_BASE];
      break;

    case eCursor_grabbing:
      newPointer = sPtrArray[IDC_GRABBING-IDC_BASE];
      break;

    case eCursor_spinning:
      newPointer = sPtrArray[IDC_ARROWWAIT-IDC_BASE];
      break;

    case eCursor_context_menu:
      // XXX this CSS3 cursor needs to be implemented
      break;

    case eCursor_zoom_in:
      newPointer = sPtrArray[IDC_ZOOMIN-IDC_BASE];
      break;

    case eCursor_zoom_out:
      newPointer = sPtrArray[IDC_ZOOMOUT-IDC_BASE];
      break;

    case eCursor_not_allowed:
    case eCursor_no_drop:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_ILLEGAL, FALSE);
      break;

    case eCursor_col_resize:
      newPointer = sPtrArray[IDC_COLRESIZE-IDC_BASE];
      break;

    case eCursor_row_resize:
      newPointer = sPtrArray[IDC_ROWRESIZE-IDC_BASE];
      break;

    case eCursor_vertical_text:
      newPointer = sPtrArray[IDC_VERTICALTEXT-IDC_BASE];
      break;

    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE);
      break;

    case eCursor_nesw_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENESW, FALSE);
      break;

    case eCursor_nwse_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENWSE, FALSE);
      break;

    case eCursor_ns_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE);
      break;

    case eCursor_ew_resize:
      newPointer = WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE);
      break;

    case eCursor_none:
      newPointer = sPtrArray[IDC_NONE-IDC_BASE];
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }

  if (newPointer) {
    WinSetPointer(HWND_DESKTOP, newPointer);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// Create a mouse pointer on the fly to support the CSS 'cursor' style.
// This code is based on the Win version by C. Biesinger but has been
// substantially modified to accommodate platform differences and to
// improve efficiency.

NS_IMETHODIMP nsWindow::SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY)
{

  // if this is the same image as last time, reuse the saved hptr;
  // it will be destroyed when we create a new one or when the
  // current window is destroyed
  if (mCssCursorImg == aCursor && mCssCursorHPtr) {
    WinSetPointer(HWND_DESKTOP, mCssCursorHPtr);
    return NS_OK;
  }

  nsRefPtr<gfxImageSurface> frame;
  aCursor->CopyFrame(imgIContainer::FRAME_CURRENT,
                     imgIContainer::FLAG_SYNC_DECODE,
                     getter_AddRefs(frame));
  NS_ENSURE_TRUE(frame, NS_ERROR_NOT_AVAILABLE);

  // if the image is ridiculously large, exit because
  // it will be unrecognizable when shrunk to 32x32
  PRInt32 width = frame->Width();
  PRInt32 height = frame->Height();
  NS_ENSURE_TRUE(width <= 128 && height <= 128, NS_ERROR_FAILURE);

  PRUint8* data = frame->Data();

  // create the color bitmap
  HBITMAP hBmp = CreateBitmapRGB(data, width, height);
  NS_ENSURE_TRUE(hBmp, NS_ERROR_FAILURE);

  // create a transparency mask from the alpha bytes
  HBITMAP hAlpha = CreateTransparencyMask(frame->Format(), data, width, height);
  if (!hAlpha) {
    GpiDeleteBitmap(hBmp);
    return NS_ERROR_FAILURE;
  }

  POINTERINFO info = {0};
  info.fPointer = TRUE;
  info.xHotspot = aHotspotX;
  info.yHotspot = height - aHotspotY - 1;
  info.hbmPointer = hAlpha;
  info.hbmColor = hBmp;

  // create the pointer
  HPOINTER cursor = WinCreatePointerIndirect(HWND_DESKTOP, &info);
  GpiDeleteBitmap(hBmp);
  GpiDeleteBitmap(hAlpha);
  NS_ENSURE_TRUE(cursor, NS_ERROR_FAILURE);

  // use it
  WinSetPointer(HWND_DESKTOP, cursor);

  // destroy the previous hptr;  this has to be done after the
  // new pointer is set or else WinDestroyPointer() will fail
  if (mCssCursorHPtr) {
    WinDestroyPointer(mCssCursorHPtr);
  }

  // save the hptr and a reference to the image for next time
  mCssCursorHPtr = cursor;
  mCssCursorImg = aCursor;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// Render image or modified alpha data as a native bitmap.

// aligned bytes per row, rounded up to next dword bounday
#define ALIGNEDBPR(cx,bits) ( ( ( ((cx)*(bits)) + 31) / 32) * 4)

HBITMAP nsWindow::DataToBitmap(PRUint8* aImageData, PRUint32 aWidth,
                               PRUint32 aHeight, PRUint32 aDepth)
{
  // get a presentation space for this window
  HPS hps = (HPS)GetNativeData(NS_NATIVE_GRAPHIC);
  if (!hps) {
    return 0;
  }

  // a handy structure that does double duty
  // as both BITMAPINFOHEADER2 & BITMAPINFO2
  struct {
    BITMAPINFOHEADER2 head;
    RGB2 black;
    RGB2 white;
  } bi;

  memset(&bi, 0, sizeof(bi));
  bi.white.bBlue = (BYTE)255;
  bi.white.bGreen = (BYTE)255;
  bi.white.bRed = (BYTE)255;

  // fill in the particulars
  bi.head.cbFix = sizeof(bi.head);
  bi.head.cx = aWidth;
  bi.head.cy = aHeight;
  bi.head.cPlanes = 1;
  bi.head.cBitCount = aDepth;
  bi.head.ulCompression = BCA_UNCOMP;
  bi.head.cbImage = ALIGNEDBPR(aWidth, aDepth) * aHeight;
  bi.head.cclrUsed = (aDepth == 1 ? 2 : 0);

  // create a bitmap from the image data
  HBITMAP hBmp = GpiCreateBitmap(hps, &bi.head, CBM_INIT,
                 reinterpret_cast<BYTE*>(aImageData),
                 (BITMAPINFO2*)&bi);

  // free the hps, then return the bitmap
  FreeNativeData((void*)hps, NS_NATIVE_GRAPHIC);
  return hBmp;
}

//-----------------------------------------------------------------------------
// Create an RGB24 bitmap from Cairo image data.

HBITMAP nsWindow::CreateBitmapRGB(PRUint8* aImageData,
                                  PRUint32 aWidth,
                                  PRUint32 aHeight)
{
  // calc width in bytes, rounding up to a dword boundary
  const PRUint32 bpr = ALIGNEDBPR(aWidth, 24);
  PRUint8* bmp = (PRUint8*)malloc(bpr * aHeight);
  if (!bmp) {
    return 0;
  }

  PRUint32* pSrc = (PRUint32*)aImageData;
  for (PRUint32 row = aHeight; row > 0; --row) {
    PRUint8* pDst = bmp + bpr * (row - 1);

    for (PRUint32 col = aWidth; col > 0; --col) {
      // In Cairo a color is encoded as ARGB in a DWORD
      // stored in machine endianess.
      PRUint32 color = *pSrc++;
      *pDst++ = color;       // Blue
      *pDst++ = color >> 8;  // Green
      *pDst++ = color >> 16; // Red
    }
  }

  // create the bitmap
  HBITMAP hAlpha = DataToBitmap(bmp, aWidth, aHeight, 24);

  // free the buffer, then return the bitmap
  free(bmp);
  return hAlpha;
}

//-----------------------------------------------------------------------------
// Create a monochrome AND/XOR bitmap from 0, 1, or 8-bit alpha data.

HBITMAP nsWindow::CreateTransparencyMask(gfxASurface::gfxImageFormat format,
                                         PRUint8* aImageData,
                                         PRUint32 aWidth,
                                         PRUint32 aHeight)
{
  // calc width in bytes, rounding up to a dword boundary
  PRUint32 abpr = ALIGNEDBPR(aWidth, 1);
  PRUint32 cbData = abpr * aHeight;

  // alloc and clear space to hold both the AND & XOR bitmaps
  PRUint8* mono = (PRUint8*)calloc(cbData, 2);
  if (!mono) {
    return 0;
  }

  // Non-alpha formats are already taken care of
  // by initializing the XOR and AND masks to zero
  if (format == gfxASurface::ImageFormatARGB32) {

    // make the AND mask the inverse of the 8-bit alpha data
    PRInt32* pSrc = (PRInt32*)aImageData;
    for (PRUint32 row = aHeight; row > 0; --row) {
      // Point to the right row in the AND mask
      PRUint8* pDst = mono + cbData + abpr * (row - 1);
      PRUint8 mask = 0x80;
      for (PRUint32 col = aWidth; col > 0; --col) {
        // Use the sign bit to test for transparency, as the alpha byte
        // is highest byte.  Positive means, alpha < 128, so consider it
        // as transparent and set the AND mask.
        if (*pSrc++ >= 0) {
          *pDst |= mask;
        }

        mask >>= 1;
        if (!mask) {
          pDst++;
          mask = 0x80;
        }
      }
    }
  }

  // create the bitmap
  HBITMAP hAlpha = DataToBitmap(mono, aWidth, aHeight * 2, 1);

  // free the buffer, then return the bitmap
  free(mono);
  return hAlpha;
}

//=============================================================================
//  Rollup Event Handlers
//=============================================================================

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener* aListener,
                                            bool aDoCapture,
                                            bool aConsumeRollupEvent)
{
  // We haven't bothered carrying a weak reference to gRollupWidget
  // because we believe lifespan is properly scoped.  The first
  // assertion helps assure that remains true.
  if (aDoCapture) {
    NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
    gRollupConsumeRollupEvent = aConsumeRollupEvent;
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    gRollupWidget = this;
    NS_ADDREF(this);
 } else {
    gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------

// static
bool nsWindow::EventIsInsideWindow(nsWindow* aWindow)
{
  RECTL  rcl;
  POINTL ptl;

  if (WinQueryMsgPos(0, &ptl)) {
    WinMapWindowPoints(HWND_DESKTOP, aWindow->mWnd, &ptl, 1);
    WinQueryWindowRect(aWindow->mWnd, &rcl);

    // now make sure that it wasn't one of our children
    if (ptl.x < rcl.xLeft || ptl.x > rcl.xRight ||
        ptl.y > rcl.yTop  || ptl.y < rcl.yBottom) {
      return false;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
// Handle events that would cause a popup (combobox, menu, etc) to rollup.

// static
bool nsWindow::RollupOnButtonDown(ULONG aMsg)
{
  // Exit if the event is inside the most recent popup.
  if (EventIsInsideWindow((nsWindow*)gRollupWidget)) {
    return false;
  }

  // See if we're dealing with a menu.  If so, exit if the
  // event was inside a parent of the current submenu.
  PRUint32 popupsToRollup = PR_UINT32_MAX;

  if (gRollupListener) {
    nsAutoTArray<nsIWidget*, 5> widgetChain;
    PRUint32 sameTypeCount = gRollupListener->GetSubmenuWidgetChain(&widgetChain);
    for (PRUint32 i = 0; i < widgetChain.Length(); ++i) {
      nsIWidget* widget = widgetChain[i];
      if (EventIsInsideWindow((nsWindow*)widget)) {
        if (i < sameTypeCount) {
          return false;
        }
        popupsToRollup = sameTypeCount;
        break;
      }
    } // for each parent menu widget
  } // if rollup listener knows about menus

  // We only need to deal with the last rollup for left mouse down events.
  NS_ASSERTION(!mLastRollup, "mLastRollup is null");
  mLastRollup = gRollupListener->Rollup(popupsToRollup, aMsg == WM_BUTTON1DOWN);
  NS_IF_ADDREF(mLastRollup);

  // If true, the buttondown event won't be passed on to the wndproc.
  return gRollupConsumeRollupEvent;
}

//-----------------------------------------------------------------------------

// static
void nsWindow::RollupOnFocusLost(HWND aFocus)
{
  HWND hRollup = ((nsWindow*)gRollupWidget)->mWnd;

  // Exit if focus was lost to the most recent popup.
  if (hRollup == aFocus) {
    return;
  }

  // Exit if focus was lost to a parent of the current submenu.
  if (gRollupListener) {
    nsAutoTArray<nsIWidget*, 5> widgetChain;
    gRollupListener->GetSubmenuWidgetChain(&widgetChain);
    for (PRUint32 i = 0; i < widgetChain.Length(); ++i) {
      if (((nsWindow*)widgetChain[i])->mWnd == aFocus) {
        return;
      }
    }
  }

  // Rollup all popups.
  gRollupListener->Rollup(PR_UINT32_MAX);
  return;
}

//=============================================================================
//  nsWindow's Window Procedure
//=============================================================================

// This is the actual wndproc;  it does some preprocessing then passes
// the msgs to the ProcessMessage() method which does most of the work.

MRESULT EXPENTRY fnwpNSWindow(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  nsAutoRollup autoRollup;

  // If this window doesn't have an object ptr,
  // send the msg to the default wndproc.
  nsWindow* wnd = nsWindow::GetNSWindowPtr(hwnd);
  if (!wnd) {
    return WinDefWindowProc(hwnd, msg, mp1, mp2);
  }

  // If we're not in the destructor, hold on to the object for the
  // life of this method, in case it gets deleted during processing.
  // Yes, it's a double hack since someWindow is not really an interface.
  nsCOMPtr<nsISupports> kungFuDeathGrip;
  if (!wnd->mIsDestroying) {
    kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)wnd);
  }

  // Pre-process msgs that may cause a rollup.
  if (gRollupListener && gRollupWidget) {
    switch (msg) {
      case WM_BUTTON1DOWN:
      case WM_BUTTON2DOWN:
      case WM_BUTTON3DOWN:
        if (nsWindow::RollupOnButtonDown(msg)) {
          return (MRESULT)true;
        }
        break;

      case WM_SETFOCUS:
        if (!mp2) {
          nsWindow::RollupOnFocusLost((HWND)mp1);
        }
        break;
    }
  }

  return wnd->ProcessMessage(msg, mp1, mp2);
}

//-----------------------------------------------------------------------------
// In effect, nsWindow's real wndproc.

MRESULT nsWindow::ProcessMessage(ULONG msg, MPARAM mp1, MPARAM mp2)
{
  bool    isDone = false;
  MRESULT mresult = 0;

  switch (msg) {

    // Interpret WM_QUIT as a close request so that
    // windows can be closed from the Window List
    case WM_CLOSE:
    case WM_QUIT: {
      mWindowState |= nsWindowState_eClosing;
      nsGUIEvent event(true, NS_XUL_CLOSE, this);
      InitEvent(event);
      DispatchWindowEvent(&event);
      // abort window closure
      isDone = true;
      break;
    }

    case WM_DESTROY:
      OnDestroy();
      isDone = true;
      break;

    case WM_PAINT:
      isDone = OnPaint();
      break;

    case WM_TRANSLATEACCEL:
      isDone = OnTranslateAccelerator((PQMSG)mp1);
      break;

    case WM_CHAR:
      isDone = DispatchKeyEvent(mp1, mp2);
      break;

    // Mouseclicks: we don't dispatch CLICK events because they just cause
    // trouble: gecko seems to expect EITHER buttondown/up OR click events
    // and so that's what we give it.

    case WM_BUTTON1DOWN:
      WinSetCapture(HWND_DESKTOP, mWnd);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, mp1, mp2);
      // If this msg is forwarded to a popup's owner, Moz will cause the
      // popup to be rolled-up in error when the owner processes the msg.
      if (mWindowType == eWindowType_popup) {
        isDone = true;
      }
      // there's no need to clear this on button-up
      sLastButton1Down.x = XFROMMP(mp1);
      sLastButton1Down.y = YFROMMP(mp1);
      break;

    case WM_BUTTON1UP:
      WinSetCapture(HWND_DESKTOP, 0);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, mp1, mp2);
      break;

    case WM_BUTTON1DBLCLK:
      isDone = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, mp1, mp2);
      break;

    case WM_BUTTON2DOWN:
      WinSetCapture(HWND_DESKTOP, mWnd);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, mp1, mp2, false,
                                  nsMouseEvent::eRightButton);
      break;

    case WM_BUTTON2UP:
      WinSetCapture(HWND_DESKTOP, 0);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, mp1, mp2, false,
                                  nsMouseEvent::eRightButton);
      break;

    case WM_BUTTON2DBLCLK:
      isDone = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, mp1, mp2,
                                  false, nsMouseEvent::eRightButton);
      break;

    case WM_BUTTON3DOWN:
      WinSetCapture(HWND_DESKTOP, mWnd);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, mp1, mp2, false,
                                  nsMouseEvent::eMiddleButton);
      break;

    case WM_BUTTON3UP:
      WinSetCapture(HWND_DESKTOP, 0);
      isDone = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, mp1, mp2, false,
                                  nsMouseEvent::eMiddleButton);
      break;

    case WM_BUTTON3DBLCLK:
      isDone = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, mp1, mp2, false,
                                  nsMouseEvent::eMiddleButton);
      break;

    case WM_CONTEXTMENU:
      if (SHORT2FROMMP(mp2)) {
        HWND hFocus = WinQueryFocus(HWND_DESKTOP);
        if (hFocus != mWnd) {
          WinSendMsg(hFocus, msg, mp1, mp2);
        } else {
          isDone = DispatchMouseEvent(NS_CONTEXTMENU, mp1, mp2, true,
                                      nsMouseEvent::eLeftButton);
        }
      } else {
        isDone = DispatchMouseEvent(NS_CONTEXTMENU, mp1, mp2, false,
                                    nsMouseEvent::eRightButton);
      }
      break;

    // If MB1 & MB2 are both pressed, perform a copy or paste.
    case WM_CHORD:
      isDone = OnMouseChord(mp1, mp2);
      break;

    case WM_MOUSEMOVE: {
      static POINTL ptlLastPos = { -1, -1 };

      // If mouse has actually moved, remember the new position,
      // then dispatch the event.
      if (ptlLastPos.x != (SHORT)SHORT1FROMMP(mp1) ||
          ptlLastPos.y != (SHORT)SHORT2FROMMP(mp1)) {
        ptlLastPos.x = (SHORT)SHORT1FROMMP(mp1);
        ptlLastPos.y = (SHORT)SHORT2FROMMP(mp1);
        DispatchMouseEvent(NS_MOUSE_MOVE, mp1, mp2);
      }

      // don't propagate mouse move or the OS will change the pointer
      isDone = true;
      break;
    }

    case WM_MOUSEENTER:
      isDone = DispatchMouseEvent(NS_MOUSE_ENTER, mp1, mp2);
      break;

    case WM_MOUSELEAVE:
      isDone = DispatchMouseEvent(NS_MOUSE_EXIT, mp1, mp2);
      break;

    case WM_APPCOMMAND: {
      PRUint32 appCommand = SHORT2FROMMP(mp2) & 0xfff;

      switch (appCommand) {
        case APPCOMMAND_BROWSER_BACKWARD:
        case APPCOMMAND_BROWSER_FORWARD:
        case APPCOMMAND_BROWSER_REFRESH:
        case APPCOMMAND_BROWSER_STOP:
          DispatchCommandEvent(appCommand);
          // tell the driver that we handled the event
          mresult = (MRESULT)1;
          isDone = true;
          break;
      }
      break;
    }

    case WM_HSCROLL:
    case WM_VSCROLL:
      isDone = DispatchScrollEvent(msg, mp1, mp2);
      break;

    // Do not act on WM_ACTIVATE - it is handled by os2FrameWindow.
    // case WM_ACTIVATE:
    //   break;

    // This msg is used to activate top-level and plugin widgets
    // after PM is done changing the focus.  We're only interested
    // in windows gaining focus, not in those losing it.
    case WM_FOCUSCHANGED:
      DEBUGFOCUS(WM_FOCUSCHANGED);
      if (SHORT1FROMMP(mp2)) {
        ActivateTopLevelWidget();
        ActivatePlugin(HWNDFROMMP(mp1));
      }
      break;

    case WM_WINDOWPOSCHANGED:
      isDone = OnReposition((PSWP) mp1);
      break;

      // all msgs that occur when this window is the target of a drag
    case DM_DRAGOVER:
    case DM_DRAGLEAVE:
    case DM_DROP:
    case DM_RENDERCOMPLETE:
    case DM_DROPHELP:
      OnDragDropMsg(msg, mp1, mp2, mresult);
      isDone = true;
      break;

    case WM_QUERYCONVERTPOS:
      isDone = OnQueryConvertPos(mp1, mresult);
      break;

    case WM_IMEREQUEST:
      isDone = OnImeRequest(mp1, mp2);
      break;
  }
  // If an event handler signalled that we should consume the event,
  // return.  Otherwise, pass it on to the default wndproc.
  if (!isDone) {
    mresult = WinDefWindowProc(mWnd, msg, mp1, mp2);
  }

  return mresult;
}

//=============================================================================
//  Window Message Handlers
//=============================================================================

// WM_DESTROY has been called.

void nsWindow::OnDestroy()
{
  mOnDestroyCalled = true;

  SetNSWindowPtr(mWnd, 0);
  mWnd = 0;

  // release references to context and children
  nsBaseWidget::OnDestroy();

  // dispatching of the event may cause the reference count to drop to 0
  // and result in this object being deleted. To avoid that, add a
  // reference and then release it after dispatching the event.
  //
  // It's important *not* to do this if we're being called from the
  // destructor -- this would result in our destructor being called *again*
  // from the Release() below.  This is very bad...
  if (!(nsWindowState_eDoingDelete & mWindowState)) {
    AddRef();
    nsGUIEvent event(true, NS_DESTROY, this);
    InitEvent(event);
    DispatchWindowEvent(&event);
    Release();
  }

  // dead widget
  mWindowState |= nsWindowState_eDead;
  mWindowState &= ~(nsWindowState_eLive|nsWindowState_ePrecreate|
                    nsWindowState_eInCreate);
}

//-----------------------------------------------------------------------------

bool nsWindow::OnReposition(PSWP pSwp)
{
  bool result = false;

  if (pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE)) {
    HWND hParent = mParent ? mParent->mWnd : WinQueryWindow(mWnd, QW_PARENT);

    // need screen coords.
    POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy - 1 };
    // XXX - this is peculiar...
    WinMapWindowPoints(WinQueryWindow(mWnd, QW_PARENT), hParent, &ptl, 1);
    PM2NS_PARENT(ptl);
    mBounds.x = ptl.x;
    mBounds.y = ptl.y;
    WinMapWindowPoints(hParent, HWND_DESKTOP, &ptl, 1);

    result = DispatchMoveEvent(ptl.x, ptl.y);
  }

  if (pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE)) {
    mBounds.width  = pSwp->cx;
    mBounds.height = pSwp->cy;

    // If the window is supposed to have a thebes surface, resize it.
    if (ConfirmThebesSurface()) {
        mThebesSurface->Resize(gfxIntSize(mBounds.width, mBounds.height));
    }

    result = DispatchResizeEvent(mBounds.width, mBounds.height);
  }

  return result;
}

//-----------------------------------------------------------------------------

bool nsWindow::OnPaint()
{
  HPS    hPS;
  HPS    hpsDrag;
  HRGN   hrgn;
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

#ifdef DEBUG_PAINT
  HRGN debugPaintFlashRegion = 0;
  HPS  debugPaintFlashPS = 0;

  if (debug_WantPaintFlashing()) {
    debugPaintFlashPS = WinGetPS(mWnd);
    debugPaintFlashRegion = GpiCreateRegion(debugPaintFlashPS, 0, 0);
    WinQueryUpdateRegion(mWnd, debugPaintFlashRegion);
  }
#endif

// Use a dummy do..while(0) loop to facilitate error handling & early-outs.
do {

  // Get the current drag status.  If we're in a Moz-originated drag,
  // it will return a special drag HPS to pass to WinBeginPaint().
  // Oherwise, get a cached micro PS.
  CheckDragStatus(ACTION_PAINT, &hpsDrag);
  hPS = hpsDrag ? hpsDrag : WinGetPS(mWnd);

  // If we can't get an HPS, validate the window so we don't
  // keep getting the same WM_PAINT msg over & over again.
  RECTL  rcl = { 0 };
  if (!hPS) {
    WinQueryWindowRect(mWnd, &rcl);
    WinValidateRect(mWnd, &rcl, FALSE);
    break;
  }

  // Get the update region before WinBeginPaint() resets it.
  hrgn = GpiCreateRegion(hPS, 0, 0);
  WinQueryUpdateRegion(mWnd, hrgn);
  WinBeginPaint(mWnd, hPS, &rcl);

  // Exit if the update rect is empty.
  if (WinIsRectEmpty(0, &rcl)) {
    break;
  }

  // Exit if a thebes surface can not/should not be created,
  // but first fill the area with the default background color
  // to erase any visual artifacts.
  if (!ConfirmThebesSurface()) {
    WinDrawBorder(hPS, &rcl, 0, 0, 0, 0, DB_INTERIOR | DB_AREAATTRS);
    break;
  }

  // Even if there is no callback to update the content (unlikely)
  // we still want to update the screen with whatever's available.
  if (!mEventCallback) {
    mThebesSurface->Refresh(&rcl, hPS);
    break;
  }

  // Create an event & a Thebes context.
  nsPaintEvent event(true, NS_PAINT, this);
  InitEvent(event);
  nsRefPtr<gfxContext> thebesContext = new gfxContext(mThebesSurface);

  // Intersect the update region with the paint rectangle to clip areas
  // that aren't visible (e.g. offscreen or covered by another window).
  HRGN hrgnPaint;
  hrgnPaint = GpiCreateRegion(hPS, 1, &rcl);
  if (hrgnPaint) {
    GpiCombineRegion(hPS, hrgn, hrgn, hrgnPaint, CRGN_AND);
    GpiDestroyRegion(hPS, hrgnPaint);
  }

  // See how many rects comprise the update region.  If there are 8
  // or fewer, update them individually.  If there are more or the call
  // failed, update the bounding rectangle returned by WinBeginPaint().
  #define MAX_CLIPRECTS 8
  RGNRECT rgnrect = { 1, MAX_CLIPRECTS, 0, RECTDIR_LFRT_TOPBOT };
  RECTL   arect[MAX_CLIPRECTS];
  RECTL*  pr = arect;

  if (!GpiQueryRegionRects(hPS, hrgn, 0, &rgnrect, 0) ||
      rgnrect.crcReturned > MAX_CLIPRECTS) {
    rgnrect.crcReturned = 1;
    arect[0] = rcl;
  } else {
    GpiQueryRegionRects(hPS, hrgn, 0, &rgnrect, arect);
  }

  // Create clipping regions for the event & the Thebes context.
  thebesContext->NewPath();
  for (PRUint32 i = 0; i < rgnrect.crcReturned; i++, pr++) {
    event.region.Or(event.region, 
                    nsIntRect(pr->xLeft,
                              mBounds.height - pr->yTop,
                              pr->xRight - pr->xLeft,
                              pr->yTop - pr->yBottom));

    thebesContext->Rectangle(gfxRect(pr->xLeft,
                                     mBounds.height - pr->yTop,
                                     pr->xRight - pr->xLeft,
                                     pr->yTop - pr->yBottom));
  }
  thebesContext->Clip();

#ifdef DEBUG_PAINT
  debug_DumpPaintEvent(stdout, this, &event, nsCAutoString("noname"),
                       (PRInt32)mWnd);
#endif

  // Init the Layers manager then dispatch the event.
  // If it returns false there's nothing to paint, so exit.
  AutoLayerManagerSetup
      setupLayerManager(this, thebesContext, BasicLayerManager::BUFFER_NONE);
  if (!DispatchWindowEvent(&event, eventStatus)) {
    break;
  }

  // Paint the surface, then use Refresh() to blit each rect to the screen.
  thebesContext->PopGroupToSource();
  thebesContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  thebesContext->Paint();
  pr = arect;
  for (PRUint32 i = 0; i < rgnrect.crcReturned; i++, pr++) {
    mThebesSurface->Refresh(pr, hPS);
  }

} while (0);

  // Cleanup.
  if (hPS) {
    WinEndPaint(hPS);
    if (hrgn) {
      GpiDestroyRegion(hPS, hrgn);
    }
    if (!hpsDrag || !ReleaseIfDragHPS(hpsDrag)) {
      WinReleasePS(hPS);
    }
  }

#ifdef DEBUG_PAINT
  if (debug_WantPaintFlashing()) {
    // Only flash paint events which have not ignored the paint message.
    // Those that ignore the paint message aren't painting anything so there
    // is only the overhead of the dispatching the paint event.
    if (eventStatus != nsEventStatus_eIgnore) {
      LONG CurMix = GpiQueryMix(debugPaintFlashPS);
      GpiSetMix(debugPaintFlashPS, FM_INVERT);

      GpiPaintRegion(debugPaintFlashPS, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
      GpiPaintRegion(debugPaintFlashPS, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));

      GpiSetMix(debugPaintFlashPS, CurMix);
    }
    GpiDestroyRegion(debugPaintFlashPS, debugPaintFlashRegion);
    WinReleasePS(debugPaintFlashPS);
  }
#endif

  return true;
}

//-----------------------------------------------------------------------------
// If MB1 & MB2 are both pressed, perform a copy or paste.

bool nsWindow::OnMouseChord(MPARAM mp1, MPARAM mp2)
{
  if (!isKeyDown(VK_BUTTON1) || !isKeyDown(VK_BUTTON2)) {
    return false;
  }

  // See how far the mouse has moved since MB1-down to determine
  // the operation (this really ought to look for selected content).
  bool isCopy = false;
  if (abs(XFROMMP(mp1) - sLastButton1Down.x) >
        (WinQuerySysValue(HWND_DESKTOP, SV_CXMOTIONSTART) / 2) ||
      abs(YFROMMP(mp1) - sLastButton1Down.y) >
        (WinQuerySysValue(HWND_DESKTOP, SV_CYMOTIONSTART) / 2)) {
    isCopy = true;
  }

  nsKeyEvent event(true, NS_KEY_PRESS, this);
  nsIntPoint point(0,0);
  InitEvent(event, &point);

  event.keyCode     = NS_VK_INSERT;
  if (isCopy) {
    event.modifiers = widget::MODIFIER_CONTROL;
  } else {
    event.modifiers = widget::MODIFIER_SHIFT;
  }
  event.eventStructType = NS_KEY_EVENT;
  event.charCode    = 0;

  // OS/2 does not set the Shift, Ctrl, or Alt on keyup
  if (SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_KEYUP | KC_LONEKEY)) {
    USHORT usVKey = SHORT2FROMMP(mp2);
    if (usVKey == VK_SHIFT) {
      event.modifiers |= widget::MODIFIER_SHIFT;
    }
    if (usVKey == VK_CTRL) {
      event.modifiers |= widget::MODIFIER_CONTROL;
    }
    if (usVKey == VK_ALTGRAF || usVKey == VK_ALT) {
      event.modifiers |= widget::MODIFIER_ALT;
    }
  }

  return DispatchWindowEvent(&event);
}

//=============================================================================
//  Drag & Drop - Target methods
//=============================================================================
//
// nsWindow knows almost nothing about d&d except that it can cause
// video corruption if the screen is updated during a drag. It relies
// on nsIDragSessionOS2 to handle native d&d messages and to return
// the status flags it uses to control screen updates.
//
// OnDragDropMsg() handles all of the DM_* messages messages nsWindow
// should ever receive.  CheckDragStatus() determines if a screen update
// is safe and may return a drag HPS if doing so will avoid corruption.
// As far as its author (R.Walsh) can tell, every use is required.
//
// For Moz drags, all while-you-drag features should be fully enabled &
// corruption free;  for native drags, popups & scrolling are suppressed
// but some niceties, e.g. moving the cursor in text fields, are enabled.
//
//-----------------------------------------------------------------------------

// This method was designed to be totally ignorant of drag and drop.
// It gives nsIDragSessionOS2 (near) complete control over handling.

bool nsWindow::OnDragDropMsg(ULONG msg, MPARAM mp1, MPARAM mp2, MRESULT& mr)
{
  nsresult rv;
  PRUint32 eventType = 0;
  PRUint32 dragFlags = 0;

  mr = 0;
  nsCOMPtr<nsIDragService> dragService =
                    do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  if (dragService) {
    nsCOMPtr<nsIDragSessionOS2> dragSession(
                        do_QueryInterface(dragService, &rv));
    if (dragSession) {

      // handle all possible input without regard to outcome
      switch (msg) {

        case DM_DRAGOVER:
          dragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
          rv = dragSession->DragOverMsg((PDRAGINFO)mp1, mr, &dragFlags);
          eventType = NS_DRAGDROP_OVER;
          break;

        case DM_DRAGLEAVE:
          rv = dragSession->DragLeaveMsg((PDRAGINFO)mp1, &dragFlags);
          eventType = NS_DRAGDROP_EXIT;
          break;

        case DM_DROP:
          rv = dragSession->DropMsg((PDRAGINFO)mp1, mWnd, &dragFlags);
          eventType = NS_DRAGDROP_DROP;
          break;

        case DM_DROPHELP:
          rv = dragSession->DropHelpMsg((PDRAGINFO)mp1, &dragFlags);
          eventType = NS_DRAGDROP_EXIT;
          break;

        case DM_RENDERCOMPLETE:
          rv = dragSession->RenderCompleteMsg((PDRAGTRANSFER)mp1,
                                              SHORT1FROMMP(mp2), &dragFlags);
          eventType = NS_DRAGDROP_DROP;
          break;

        default:
          rv = NS_ERROR_FAILURE;
      }

      // handle all possible outcomes without regard to their source
      if (NS_SUCCEEDED(rv)) {
        mDragStatus = sDragStatus = (dragFlags & DND_DragStatus);

        if (dragFlags & DND_DispatchEnterEvent) {
          DispatchDragDropEvent(NS_DRAGDROP_ENTER);
        }
        if (dragFlags & DND_DispatchEvent) {
          DispatchDragDropEvent(eventType);
        }
        if (dragFlags & DND_GetDragoverResult) {
          dragSession->GetDragoverResult(mr);
        }
        if (dragFlags & DND_ExitSession) {
          dragSession->ExitSession(&dragFlags);
        }
      }
    }
  }
  // save final drag status
  sDragStatus = mDragStatus = (dragFlags & DND_DragStatus);

  return true;
}

//-----------------------------------------------------------------------------
// CheckDragStatus() concentrates all the hacks needed to avoid video
// corruption during d&d into one place.  The caller specifies an action
// that might be a problem;  the method tells it whether to proceed and
// provides a Drg HPS if the situation calls for one.

bool nsWindow::CheckDragStatus(PRUint32 aAction, HPS* aHps)
{
  bool rtn    = true;
  bool getHps = false;

  switch (aAction) {

    // OnPaint() & Scroll..() - only Moz drags get a Drg hps
    case ACTION_PAINT:
    case ACTION_SCROLL:
      if (sDragStatus & DND_MozDrag) {
        getHps = true;
      }
      break;

    // GetNativeData() - Moz drags + native drags over this nsWindow
    case ACTION_DRAW:
      if ((sDragStatus & DND_MozDrag) ||
          (mDragStatus & DND_NativeDrag)) {
        getHps = true;
      }
      break;

    // Show() - don't show popups during a native dragover
    case ACTION_SHOW:
      if ((sDragStatus & (DND_NativeDrag | DND_InDrop)) == DND_NativeDrag) {
        rtn = false;
      }
      break;

    // InitEvent() - use PtrPos while in drag, MsgPos otherwise
    case ACTION_PTRPOS:
      if (!sDragStatus) {
        rtn = false;
      }
      break;

    default:
      rtn = false;
  }

  // If the caller wants an HPS, and the current drag status
  // calls for one, *and* a drag hps hasn't already been requested
  // for this window, get the hps;  otherwise, return zero;
  // (if we provide a 2nd hps for a window, the cursor in text
  // fields won't be erased when it's moved to another position)
  if (aHps) {
    if (getHps && !mDragHps) {
      mDragHps = DrgGetPS(mWnd);
      *aHps = mDragHps;
    } else {
      *aHps = 0;
    }
  }

  return rtn;
}

//-----------------------------------------------------------------------------
// If there's an outstanding drag hps & it matches the one passed in,
// release it.

bool nsWindow::ReleaseIfDragHPS(HPS aHps)
{
  if (mDragHps && aHps == mDragHps) {
    DrgReleasePS(mDragHps);
    mDragHps = 0;
    return true;
  }

  return false;
}

//=============================================================================
//  Keyboard Handlers
//=============================================================================

// Figure out which keyboard LEDs are on.

NS_IMETHODIMP nsWindow::GetToggledKeyState(PRUint32 aKeyCode, bool* aLEDState)
{
  PRUint32  vkey;

  NS_ENSURE_ARG_POINTER(aLEDState);

  switch (aKeyCode) {
    case NS_VK_CAPS_LOCK:
      vkey = VK_CAPSLOCK;
      break;
    case NS_VK_NUM_LOCK:
      vkey = VK_NUMLOCK;
      break;
    case NS_VK_SCROLL_LOCK:
      vkey = VK_SCRLLOCK;
      break;
    default:
      *aLEDState = false;
      return NS_OK;
  }

  *aLEDState = (WinGetKeyState(HWND_DESKTOP, vkey) & 1) != 0;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Prevent PM from translating some keys & key-combos into accelerators.

bool nsWindow::OnTranslateAccelerator(PQMSG pQmsg)
{
  if (pQmsg->msg != WM_CHAR) {
    return false;
  }

  LONG mp1 = (LONG)pQmsg->mp1;
  LONG mp2 = (LONG)pQmsg->mp2;
  LONG sca = SHORT1FROMMP(mp1) & (KC_SHIFT | KC_CTRL | KC_ALT);

  if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY) {

    // standalone F1 & F10
    if (SHORT2FROMMP(mp2) == VK_F1 || SHORT2FROMMP(mp2) == VK_F10) {
      return (!sca ? true : false);
    }

    // Shift+Enter
    if (SHORT2FROMMP(mp2) == VK_ENTER) {
      return (sca == KC_SHIFT ? true : false);
    }

    // Alt+Enter
    if (SHORT2FROMMP(mp2) == VK_NEWLINE) {
      return (sca == KC_ALT ? true : false);
    }

    // standalone Alt & AltGraf
    if ((SHORT2FROMMP(mp2) == VK_ALT || SHORT2FROMMP(mp2) == VK_ALTGRAF) &&
        (SHORT1FROMMP(mp1) & (KC_KEYUP | KC_LONEKEY))
                          == (KC_KEYUP | KC_LONEKEY)) {
      return true;
    }
  }

  return false;
}
bool nsWindow::OnQueryConvertPos(MPARAM mp1, MRESULT& mresult)
{
  PRECTL pCursorPos = (PRECTL)mp1;

  nsIntPoint point(0, 0);

  nsQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, this);
  InitEvent(selection, &point);
  DispatchWindowEvent(&selection);
  if (!selection.mSucceeded)
    return false;

  nsQueryContentEvent caret(true, NS_QUERY_CARET_RECT, this);
  caret.InitForQueryCaretRect(selection.mReply.mOffset);
  InitEvent(caret, &point);
  DispatchWindowEvent(&caret);
  if (!caret.mSucceeded)
    return false;

  pCursorPos->xLeft = caret.mReply.mRect.x;
  pCursorPos->yBottom = caret.mReply.mRect.y;
  pCursorPos->xRight = pCursorPos->xLeft + caret.mReply.mRect.width;
  pCursorPos->yTop = pCursorPos->yBottom + caret.mReply.mRect.height;
  NS2PM(*pCursorPos);

  mresult = (MRESULT)QCP_CONVERT;

  return true;
}
bool nsWindow::ImeResultString(HIMI himi)
{
  PCHAR pBuf;
  ULONG ulBufLen;

  // Get a buffer size
  ulBufLen = 0;
  if (spfnImGetResultString(himi, IMR_RESULT_RESULTSTRING, NULL, &ulBufLen))
    return false;

  pBuf = new CHAR[ulBufLen];
  if (!pBuf)
    return false;

  if (spfnImGetResultString(himi, IMR_RESULT_RESULTSTRING, pBuf,
                            &ulBufLen)) {
    delete pBuf;

    return false;
  }

  if (!mIsComposing) {
    mLastDispatchedCompositionString.Truncate();

    nsCompositionEvent start(true, NS_COMPOSITION_START, this);
    InitEvent(start);
    DispatchWindowEvent(&start);

    mIsComposing = true;
  }

  nsAutoChar16Buffer outBuf;
  PRInt32 outBufLen;
  MultiByteToWideChar(0, pBuf, ulBufLen, outBuf, outBufLen);

  delete pBuf;

  nsAutoString compositionString(outBuf.Elements());

  if (mLastDispatchedCompositionString != compositionString) {
    nsCompositionEvent update(true, NS_COMPOSITION_UPDATE, this);
    InitEvent(update);
    update.data = compositionString;
    mLastDispatchedCompositionString = compositionString;
    DispatchWindowEvent(&update);
  }

  nsTextEvent text(true, NS_TEXT_TEXT, this);
  InitEvent(text);
  text.theText = compositionString;
  DispatchWindowEvent(&text);

  nsCompositionEvent end(true, NS_COMPOSITION_END, this);
  InitEvent(end);
  end.data = compositionString;
  DispatchWindowEvent(&end);
  mIsComposing = false;
  mLastDispatchedCompositionString.Truncate();

  return true;
}

bool nsWindow::ImeConversionString(HIMI himi)
{
  PCHAR pBuf;
  ULONG ulBufLen;

  // Get a buffer size
  ulBufLen = 0;
  if (spfnImGetConversionString(himi, IMR_CONV_CONVERSIONSTRING, NULL,
                                &ulBufLen))
    return false;

  pBuf = new CHAR[ulBufLen];
  if (!pBuf)
    return false;

  if (spfnImGetConversionString(himi, IMR_CONV_CONVERSIONSTRING, pBuf,
                                &ulBufLen)) {
    delete pBuf;

    return false;
  }

  if (!mIsComposing) {
    mLastDispatchedCompositionString.Truncate();

    nsCompositionEvent start(true, NS_COMPOSITION_START, this);
    InitEvent(start);
    DispatchWindowEvent(&start);

    mIsComposing = true;
  }

  nsAutoChar16Buffer outBuf;
  PRInt32 outBufLen;
  MultiByteToWideChar(0, pBuf, ulBufLen, outBuf, outBufLen);

  delete pBuf;

  nsAutoString compositionString(outBuf.Elements());

  // Is a conversion string changed ?
  if (mLastDispatchedCompositionString != compositionString) {
    nsCompositionEvent update(true, NS_COMPOSITION_UPDATE, this);
    InitEvent(update);
    update.data = compositionString;
    mLastDispatchedCompositionString = compositionString;
    DispatchWindowEvent(&update);
  }

  nsAutoTArray<nsTextRange, 4> textRanges;

  if (!compositionString.IsEmpty()) {
    nsTextRange newRange;
    newRange.mStartOffset = 0;
    newRange.mEndOffset = compositionString.Length();
    newRange.mRangeType = NS_TEXTRANGE_SELECTEDRAWTEXT;
    textRanges.AppendElement(newRange);

    newRange.mStartOffset = compositionString.Length();
    newRange.mEndOffset = newRange.mStartOffset;
    newRange.mRangeType = NS_TEXTRANGE_CARETPOSITION;
    textRanges.AppendElement(newRange);
  }

  nsTextEvent text(true, NS_TEXT_TEXT, this);
  InitEvent(text);
  text.theText = compositionString;
  text.rangeArray = textRanges.Elements();
  text.rangeCount = textRanges.Length();
  DispatchWindowEvent(&text);

  if (compositionString.IsEmpty()) { // IME conversion was canceled ?
    nsCompositionEvent end(true, NS_COMPOSITION_END, this);
    InitEvent(end);
    end.data = compositionString;
    DispatchWindowEvent(&end);

    mIsComposing = false;
    mLastDispatchedCompositionString.Truncate();
  }

  return true;
}

bool nsWindow::OnImeRequest(MPARAM mp1, MPARAM mp2)
{
  HIMI himi;
  bool rc;

  if (!sIm32Mod)
    return false;

  if (SHORT1FROMMP(mp1) != IMR_CONVRESULT)
    return false;

  if (spfnImGetInstance(mWnd, &himi))
    return false;

  if (LONGFROMMP(mp2) & IMR_RESULT_RESULTSTRING)
    rc = ImeResultString(himi);
  else if (LONGFROMMP(mp2) & IMR_CONV_CONVERSIONSTRING)
    rc = ImeConversionString(himi);
  else
    rc = true;

  spfnImReleaseInstance(mWnd, himi);

  return rc;
}

//-----------------------------------------------------------------------------
// Key handler.  Specs for the various text messages are really confused;
// see other platforms for best results of how things are supposed to work.
//
// Perhaps more importantly, the main man listening to these events
// (besides random bits of javascript) is ender -- see
// mozilla/editor/base/nsEditorEventListeners.cpp.

bool nsWindow::DispatchKeyEvent(MPARAM mp1, MPARAM mp2)
{
  nsKeyEvent pressEvent(true, 0, nsnull);
  USHORT fsFlags = SHORT1FROMMP(mp1);
  USHORT usVKey = SHORT2FROMMP(mp2);
  USHORT usChar = SHORT1FROMMP(mp2);
  UCHAR uchScan = CHAR4FROMMP(mp1);

  // It appears we're not supposed to transmit shift,
  // control, & alt events to gecko.
  if (fsFlags & KC_VIRTUALKEY && !(fsFlags & KC_KEYUP) &&
      (usVKey == VK_SHIFT || usVKey == VK_CTRL || usVKey == VK_ALTGRAF)) {
    return false;
  }

  // Workaround bug where using Alt+Esc let an Alt key creep through
  // Only handle alt by itself if the LONEKEY bit is set
  if ((fsFlags & KC_VIRTUALKEY) && (usVKey == VK_ALT) && !usChar &&
      (!(fsFlags & KC_LONEKEY)) && (fsFlags & KC_KEYUP)) {
    return false;
  }

   // Now check if it's a dead-key
  if (fsFlags & KC_DEADKEY) {
    return true;
  }

  // Now dispatch a keyup/keydown event.  This one is *not* meant to
  // have the unicode charcode in.
  nsIntPoint point(0,0);
  nsKeyEvent event(true, (fsFlags & KC_KEYUP) ? NS_KEY_UP : NS_KEY_DOWN,
                   this);
  InitEvent(event, &point);
  event.keyCode   = WMChar2KeyCode(mp1, mp2);
  event.InitBasicModifiers(fsFlags & KC_CTRL, fsFlags & KC_ALT,
                           fsFlags & KC_SHIFT, false);
  event.charCode  = 0;

  // Check for a scroll mouse event vs. a keyboard event.  The way we know
  // this is that the repeat count is 0 and the key is not physically down.
  // Unfortunately, there is an exception here - if alt or ctrl are held
  // down, repeat count is set so we have to add special checks for them.
  if (((event.keyCode == NS_VK_UP) || (event.keyCode == NS_VK_DOWN)) &&
      !(fsFlags & KC_KEYUP) &&
      (!CHAR3FROMMP(mp1) || fsFlags & KC_CTRL || fsFlags & KC_ALT)) {
    if (!(WinGetPhysKeyState(HWND_DESKTOP, uchScan) & 0x8000)) {
      MPARAM mp2;
      if (event.keyCode == NS_VK_UP) {
        mp2 = MPFROM2SHORT(0, SB_LINEUP);
      } else {
        mp2 = MPFROM2SHORT(0, SB_LINEDOWN);
      }
      WinSendMsg(mWnd, WM_VSCROLL, 0, mp2);
      return FALSE;
    }
  }

  pressEvent = event;
  bool rc = DispatchWindowEvent(&event);

  // Break off now if this was a key-up.
  if (fsFlags & KC_KEYUP) {
    return rc;
  }

  // Break off if we've got an "invalid composition" -- that is,
  // the user typed a deadkey last time, but has now typed something
  // that doesn't make sense in that context.
  if (fsFlags & KC_INVALIDCOMP) {
    // actually, not sure whether we're supposed to abort the keypress
    // or process it as though the dead key has been pressed.
    return rc;
  }

  // Now we need to dispatch a keypress event which has the unicode char.
  // If keydown default was prevented, do same for keypress
  pressEvent.message = NS_KEY_PRESS;
  if (rc) {
    pressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }

  if (usChar) {
    USHORT inbuf[2];
    inbuf[0] = usChar;
    inbuf[1] = '\0';

    nsAutoChar16Buffer outbuf;
    PRInt32 bufLength;
    MultiByteToWideChar(0, (const char*)inbuf, 2, outbuf, bufLength);

    pressEvent.charCode = outbuf[0];

    if (pressEvent.IsControl() && !(fsFlags & (KC_VIRTUALKEY | KC_DEADKEY))) {
      if (!pressEvent.IsShift() && (pressEvent.charCode >= 'A' && pressEvent.charCode <= 'Z')) {
        pressEvent.charCode = tolower(pressEvent.charCode);
      }
      if (pressEvent.IsShift() && (pressEvent.charCode >= 'a' && pressEvent.charCode <= 'z')) {
        pressEvent.charCode = toupper(pressEvent.charCode);
      }
      pressEvent.keyCode = 0;
    } else if (!pressEvent.IsControl() && !pressEvent.IsAlt() && pressEvent.charCode != 0) {
      if (!(fsFlags & KC_VIRTUALKEY) || // not virtual key
          ((fsFlags & KC_CHAR) && !pressEvent.keyCode)) {
        pressEvent.keyCode = 0;
      } else if (usVKey == VK_SPACE) {
        // space key, do nothing here
      } else if ((fsFlags & KC_VIRTUALKEY) &&
                 isNumPadScanCode(uchScan) && pressEvent.keyCode != 0 && isNumlockOn) {
        // this is NumLock+Numpad (no Alt), handle this like a normal number
        pressEvent.keyCode = 0;
      } else { // Real virtual key
        pressEvent.charCode = 0;
      }
    }
    rc = DispatchWindowEvent(&pressEvent);
  }

  return rc;
}

//-----------------------------------------------------------------------------
// Helper function to translate from a WM_CHAR to an NS_VK_ constant.

static
PRUint32 WMChar2KeyCode(MPARAM mp1, MPARAM mp2)
{
  PRUint32 rc = SHORT1FROMMP(mp2);  // character code
  PRUint32 rcmask = rc & 0x00FF;    // masked character code for key up events
  USHORT sc = CHAR4FROMMP(mp1);     // scan code
  USHORT flags = SHORT1FROMMP(mp1); // flag word

  // First check for characters.
  // This is complicated by keystrokes such as Ctrl+K not having the KC_CHAR
  // bit set, but thankfully they do have the character actually there.

  // Assume that `if not vkey or deadkey or valid number then char'
  if (!(flags & (KC_VIRTUALKEY | KC_DEADKEY)) ||
      (rcmask >= '0' && rcmask <= '9' &&             // handle keys on Numpad, too,
       (isNumPadScanCode(sc) ? isNumlockOn : 1))) { // if NumLock is on
    if (flags & KC_KEYUP) { // On OS/2 the scancode is in the upper byte of
                            // usChar when KC_KEYUP is set so mask it off
      rc = rcmask;
    } else { // not KC_KEYUP
      if (!(flags & KC_CHAR)) {
        if ((flags & KC_ALT) || (flags & KC_CTRL)) {
          rc = rcmask;
        } else {
          rc = 0;
        }
      }
    }

    if (rc < 0xFF) {
      if (rc >= 'a' && rc <= 'z') { // The DOM_VK are for upper case only so
                                    // if rc is lower case upper case it.
        rc = rc - 'a' + NS_VK_A;
      } else if (rc >= 'A' && rc <= 'Z') { // Upper case
        rc = rc - 'A' + NS_VK_A;
      } else if (rc >= '0' && rc <= '9') {
        // Number keys, including Numpad if NumLock is not set
        rc = rc - '0' + NS_VK_0;
      } else {
        // For some characters, map the scan code to the NS_VK value
        // This only happens in the char case NOT the VK case!
        switch (sc) {
          case 0x02: rc = NS_VK_1;             break;
          case 0x03: rc = NS_VK_2;             break;
          case 0x04: rc = NS_VK_3;             break;
          case 0x05: rc = NS_VK_4;             break;
          case 0x06: rc = NS_VK_5;             break;
          case 0x07: rc = NS_VK_6;             break;
          case 0x08: rc = NS_VK_7;             break;
          case 0x09: rc = NS_VK_8;             break;
          case 0x0A: rc = NS_VK_9;             break;
          case 0x0B: rc = NS_VK_0;             break;
          case 0x0D: rc = NS_VK_EQUALS;        break;
          case 0x1A: rc = NS_VK_OPEN_BRACKET;  break;
          case 0x1B: rc = NS_VK_CLOSE_BRACKET; break;
          case 0x27: rc = NS_VK_SEMICOLON;     break;
          case 0x28: rc = NS_VK_QUOTE;         break;
          case 0x29: rc = NS_VK_BACK_QUOTE;    break;
          case 0x2B: rc = NS_VK_BACK_SLASH;    break;
          case 0x33: rc = NS_VK_COMMA;         break;
          case 0x34: rc = NS_VK_PERIOD;        break;
          case 0x35: rc = NS_VK_SLASH;         break;
          case 0x37: rc = NS_VK_MULTIPLY;      break;
          case 0x4A: rc = NS_VK_SUBTRACT;      break;
          case 0x4C: rc = NS_VK_CLEAR;         break; // numeric case is handled above
          case 0x4E: rc = NS_VK_ADD;           break;
          case 0x5C: rc = NS_VK_DIVIDE;        break;
          default: break;
        } // switch
      } // else
    } // if (rc < 0xFF)
  } else if (flags & KC_VIRTUALKEY) {
    USHORT vk = SHORT2FROMMP(mp2);
    if (flags & KC_KEYUP) { // On OS/2 there are extraneous bits in the upper byte of
                            // usChar when KC_KEYUP is set so mask them off
      rc = rcmask;
    }
    if (isNumPadScanCode(sc) &&
        (((flags & KC_ALT) && (sc != PMSCAN_PADPERIOD)) ||
          ((flags & (KC_CHAR | KC_SHIFT)) == KC_CHAR)  ||
          ((flags & KC_KEYUP) && rc != 0))) {
      CHAR numpadMap[] = {NS_VK_NUMPAD7, NS_VK_NUMPAD8, NS_VK_NUMPAD9, 0,
                          NS_VK_NUMPAD4, NS_VK_NUMPAD5, NS_VK_NUMPAD6, 0,
                          NS_VK_NUMPAD1, NS_VK_NUMPAD2, NS_VK_NUMPAD3,
                          NS_VK_NUMPAD0, NS_VK_DECIMAL};
      // If this is the Numpad must not return VK for ALT+Numpad or ALT+NumLock+Numpad
      // NumLock+Numpad is OK
      if (numpadMap[sc - PMSCAN_PAD7] != 0) { // not plus or minus on Numpad
        if (flags & KC_ALT) { // do not react on Alt plus ASCII-code sequences
          rc = 0;
        } else {
          rc = numpadMap[sc - PMSCAN_PAD7];
        }
      } else {                                // plus or minus of Numpad
        rc = 0; // No virtual key for Alt+Numpad or NumLock+Numpad
      }
    } else if (!(flags & KC_CHAR) || isNumPadScanCode(sc) ||
               (vk == VK_BACKSPACE) || (vk == VK_TAB) || (vk == VK_BACKTAB) ||
               (vk == VK_ENTER) || (vk == VK_NEWLINE) || (vk == VK_SPACE)) {
      if (vk >= VK_F1 && vk <= VK_F24) {
        rc = NS_VK_F1 + (vk - VK_F1);
      }
      else switch (vk) {
        case VK_NUMLOCK:   rc = NS_VK_NUM_LOCK; break;
        case VK_SCRLLOCK:  rc = NS_VK_SCROLL_LOCK; break;
        case VK_ESC:       rc = NS_VK_ESCAPE; break; // NS_VK_CANCEL
        case VK_BACKSPACE: rc = NS_VK_BACK; break;
        case VK_TAB:       rc = NS_VK_TAB; break;
        case VK_BACKTAB:   rc = NS_VK_TAB; break; // layout tests for isShift
        case VK_CLEAR:     rc = NS_VK_CLEAR; break;
        case VK_NEWLINE:   rc = NS_VK_RETURN; break;
        case VK_ENTER:     rc = NS_VK_RETURN; break;
        case VK_SHIFT:     rc = NS_VK_SHIFT; break;
        case VK_CTRL:      rc = NS_VK_CONTROL; break;
        case VK_ALT:       rc = NS_VK_ALT; break;
        case VK_PAUSE:     rc = NS_VK_PAUSE; break;
        case VK_CAPSLOCK:  rc = NS_VK_CAPS_LOCK; break;
        case VK_SPACE:     rc = NS_VK_SPACE; break;
        case VK_PAGEUP:    rc = NS_VK_PAGE_UP; break;
        case VK_PAGEDOWN:  rc = NS_VK_PAGE_DOWN; break;
        case VK_END:       rc = NS_VK_END; break;
        case VK_HOME:      rc = NS_VK_HOME; break;
        case VK_LEFT:      rc = NS_VK_LEFT; break;
        case VK_UP:        rc = NS_VK_UP; break;
        case VK_RIGHT:     rc = NS_VK_RIGHT; break;
        case VK_DOWN:      rc = NS_VK_DOWN; break;
        case VK_PRINTSCRN: rc = NS_VK_PRINTSCREEN; break;
        case VK_INSERT:    rc = NS_VK_INSERT; break;
        case VK_DELETE:    rc = NS_VK_DELETE; break;
      } // switch
    }
  } // KC_VIRTUALKEY

  return rc;
}

//=============================================================================
//  Event Dispatch
//=============================================================================

// Initialize an event to dispatch.

void nsWindow::InitEvent(nsGUIEvent& event, nsIntPoint* aPoint)
{
  // if no point was supplied, calculate it
  if (!aPoint) {
    // for most events, get the message position;  for drag events,
    // msg position may be incorrect, so get the current position instead
    POINTL ptl;
    if (CheckDragStatus(ACTION_PTRPOS, 0)) {
      WinQueryPointerPos(HWND_DESKTOP, &ptl);
    } else {
      WinQueryMsgPos(0, &ptl);
    }

    WinMapWindowPoints(HWND_DESKTOP, mWnd, &ptl, 1);
    PM2NS(ptl);
    event.refPoint.x = ptl.x;
    event.refPoint.y = ptl.y;
  } else {
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

  event.time = WinQueryMsgTime(0);
  return;
}

//-----------------------------------------------------------------------------
// Invoke the Event Listener object's callback.

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  if (!mEventCallback) {
    return NS_OK;
  }

  // if state is eInCreate, only send out NS_CREATE
  // if state is eDoingDelete, don't send out anything
  if ((mWindowState & nsWindowState_eLive) ||
      (mWindowState == nsWindowState_eInCreate && event->message == NS_CREATE)) {
    aStatus = (*mEventCallback)(event);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
  NS_PRECONDITION(aNewParent, "");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return (status == nsEventStatus_eConsumeNoDefault);
}

bool nsWindow::DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return (aStatus == nsEventStatus_eConsumeNoDefault);
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchCommandEvent(PRUint32 aEventCommand)
{
  nsCOMPtr<nsIAtom> command;

  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsGkAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsGkAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsGkAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsGkAtoms::Stop;
      break;
    default:
      return false;
  }

  nsCommandEvent event(true, nsGkAtoms::onAppCommand, command, this);
  InitEvent(event);
  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchDragDropEvent(PRUint32 aMsg)
{
  nsDragEvent event(true, aMsg, this);
  InitEvent(event);

  event.InitBasicModifiers(isKeyDown(VK_CTRL),
                           isKeyDown(VK_ALT) || isKeyDown(VK_ALTGRAF),
                           isKeyDown(VK_SHIFT), false);

  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchMoveEvent(PRInt32 aX, PRInt32 aY)
{
  // Params here are in XP-space for the desktop
  nsGUIEvent event(true, NS_MOVE, this);
  nsIntPoint point(aX, aY);
  InitEvent(event, &point);
  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchResizeEvent(PRInt32 aX, PRInt32 aY)
{
  nsSizeEvent event(true, NS_SIZE, this);
  nsIntRect   rect(0, 0, aX, aY);

  InitEvent(event);
  event.windowSize = &rect;             // this is the *client* rectangle
  event.mWinWidth = mBounds.width;
  event.mWinHeight = mBounds.height;

  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------
// Deal with all sorts of mouse events.

bool nsWindow::DispatchMouseEvent(PRUint32 aEventType, MPARAM mp1, MPARAM mp2,
                                    bool aIsContextMenuKey, PRInt16 aButton)
{
  NS_ENSURE_TRUE(aEventType, false);

  nsMouseEvent event(true, aEventType, this, nsMouseEvent::eReal,
                     aIsContextMenuKey
                     ? nsMouseEvent::eContextMenuKey
                     : nsMouseEvent::eNormal);
  event.button = aButton;
  if (aEventType == NS_MOUSE_BUTTON_DOWN && mIsComposing) {
    // If IME is composing, let it complete.
    HIMI himi;

    spfnImGetInstance(mWnd, &himi);
    spfnImRequestIME(himi, REQ_CONVERSIONSTRING, CNV_COMPLETE, 0);
    spfnImReleaseInstance(mWnd, himi);
  }

  if (aEventType == NS_MOUSE_ENTER || aEventType == NS_MOUSE_EXIT) {
    // Ignore enter/leave msgs forwarded from the frame to FID_CLIENT
    // because we're only interested msgs involving the content area.
    if (HWNDFROMMP(mp1) != mWnd) {
      return FALSE;
    }

    // If the mouse has exited the content area and entered either an
    // unrelated window or what Windows would call the nonclient area
    // (i.e. frame, titlebar, etc.), mark this as a toplevel exit.
    // Note: exits to and from menus will also be marked toplevel.
    if (aEventType == NS_MOUSE_EXIT) {
      HWND  hTop = 0;
      HWND  hCur = mWnd;
      HWND  hDesk = WinQueryDesktopWindow(0, 0);
      while (hCur && hCur != hDesk) {
        hTop = hCur;
        hCur = WinQueryWindow(hCur, QW_PARENT);
      }

      // event.exit was init'ed to eChild, so we don't need an 'else'
      hTop = WinWindowFromID(hTop, FID_CLIENT);
      if (!hTop || !WinIsChild(HWNDFROMMP(mp2), hTop)) {
        event.exit = nsMouseEvent::eTopLevel;
      }
    }

    InitEvent(event, nsnull);
    event.InitBasicModifiers(isKeyDown(VK_CTRL),
                             isKeyDown(VK_ALT) || isKeyDown(VK_ALTGRAF),
                             isKeyDown(VK_SHIFT), false);
  } else {
    POINTL ptl;
    if (aEventType == NS_CONTEXTMENU && aIsContextMenuKey) {
      WinQueryPointerPos(HWND_DESKTOP, &ptl);
      WinMapWindowPoints(HWND_DESKTOP, mWnd, &ptl, 1);
    } else {
      ptl.x = (SHORT)SHORT1FROMMP(mp1);
      ptl.y = (SHORT)SHORT2FROMMP(mp1);
    }
    PM2NS(ptl);
    nsIntPoint pt(ptl.x, ptl.y);
    InitEvent(event, &pt);

    USHORT usFlags  = SHORT2FROMMP(mp2);
    event.InitBasicModifiers(usFlags & KC_CTRL, usFlags & KC_ALT,
                             usFlags & KC_SHIFT, false);
  }

  // Dblclicks are used to set the click count, then changed to mousedowns
  if (aEventType == NS_MOUSE_DOUBLECLICK &&
      (aButton == nsMouseEvent::eLeftButton ||
       aButton == nsMouseEvent::eRightButton)) {
    event.message = NS_MOUSE_BUTTON_DOWN;
    event.button = (aButton == nsMouseEvent::eLeftButton) ?
                   nsMouseEvent::eLeftButton : nsMouseEvent::eRightButton;
    event.clickCount = 2;
  } else {
    event.clickCount = 1;
  }

  NPEvent pluginEvent;
  switch (aEventType) {

    case NS_MOUSE_BUTTON_DOWN:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1DOWN;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3DOWN;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2DOWN;
          break;
        default:
          break;
      }
      break;

    case NS_MOUSE_BUTTON_UP:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1UP;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3UP;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2UP;
          break;
        default:
          break;
      }
      break;

    case NS_MOUSE_DOUBLECLICK:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1DBLCLK;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3DBLCLK;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2DBLCLK;
          break;
        default:
          break;
      }
      break;

    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
  }

  pluginEvent.wParam = 0;
  pluginEvent.lParam = MAKELONG(event.refPoint.x, event.refPoint.y);

  event.pluginEvent = (void*)&pluginEvent;

  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------
// Signal plugin & top-level window activation.

bool nsWindow::DispatchActivationEvent(PRUint32 aEventType)
{
  nsGUIEvent event(true, aEventType, this);

  // These events should go to their base widget location,
  // not current mouse position.
  nsIntPoint point(0, 0);
  InitEvent(event, &point);

  NPEvent pluginEvent;
  switch (aEventType) {
    case NS_ACTIVATE:
      pluginEvent.event = WM_SETFOCUS;
      break;
    case NS_DEACTIVATE:
      pluginEvent.event = WM_FOCUSCHANGED;
      break;
    case NS_PLUGIN_ACTIVATE:
      pluginEvent.event = WM_FOCUSCHANGED;
      break;
  }
  event.pluginEvent = (void*)&pluginEvent;

  return DispatchWindowEvent(&event);
}

//-----------------------------------------------------------------------------

bool nsWindow::DispatchScrollEvent(ULONG msg, MPARAM mp1, MPARAM mp2)
{
  nsMouseScrollEvent scrollEvent(true, NS_MOUSE_SCROLL, this);
  InitEvent(scrollEvent);

  scrollEvent.InitBasicModifiers(isKeyDown(VK_CTRL),
                                 isKeyDown(VK_ALT) || isKeyDown(VK_ALTGRAF),
                                 isKeyDown(VK_SHIFT), false);
  scrollEvent.scrollFlags = (msg == WM_HSCROLL) ?
                            nsMouseScrollEvent::kIsHorizontal :
                            nsMouseScrollEvent::kIsVertical;

  // The SB_* constants for analogous vertical & horizontal ops have the
  // the same values, so only use the verticals to avoid compiler errors.
  switch (SHORT2FROMMP(mp2)) {
    case SB_LINEUP:
    //   SB_LINELEFT:
      scrollEvent.delta = -1;
      break;

    case SB_LINEDOWN:
    //   SB_LINERIGHT:
      scrollEvent.delta = 1;
      break;

    case SB_PAGEUP:
    //   SB_PAGELEFT:
      scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
      scrollEvent.delta = -1;
      break;

    case SB_PAGEDOWN:
    //   SB_PAGERIGHT:
      scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
      scrollEvent.delta = 1;
      break;

    default:
      scrollEvent.delta = 0;
      break;
  }
  DispatchWindowEvent(&scrollEvent);

  return false;
}

//=============================================================================

