/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinGesture_h__
#define WinGesture_h__

/*
 * nsWinGesture - Touch input handling for tablet displays.
 */

#include "nsdefs.h"
#include <winuser.h>
#include <tpcshrd.h>
#include "nsPoint.h"
#include "nsGUIEvent.h"

// Desktop builds target apis for 502. Win8 Metro builds target 602.
#if WINVER < 0x0602

DECLARE_HANDLE(HGESTUREINFO);

/*
 * Gesture flags - GESTUREINFO.dwFlags
 */
#define GF_BEGIN                        0x00000001
#define GF_INERTIA                      0x00000002
#define GF_END                          0x00000004

/*
 * Gesture configuration structure
 *   - Used in SetGestureConfig and GetGestureConfig
 *   - Note that any setting not included in either GESTURECONFIG.dwWant or
 *     GESTURECONFIG.dwBlock will use the parent window's preferences or
 *     system defaults.
 */
typedef struct tagGESTURECONFIG {
    DWORD dwID;                     // gesture ID
    DWORD dwWant;                   // settings related to gesture ID that are to be turned on
    DWORD dwBlock;                  // settings related to gesture ID that are to be turned off
} GESTURECONFIG, *PGESTURECONFIG;

/*
 * Gesture information structure
 *   - Pass the HGESTUREINFO received in the WM_GESTURE message lParam into the
 *     GetGestureInfo function to retrieve this information.
 *   - If cbExtraArgs is non-zero, pass the HGESTUREINFO received in the WM_GESTURE
 *     message lParam into the GetGestureExtraArgs function to retrieve extended
 *     argument information.
 */
typedef struct tagGESTUREINFO {
    UINT cbSize;                    // size, in bytes, of this structure (including variable length Args field)
    DWORD dwFlags;                  // see GF_* flags
    DWORD dwID;                     // gesture ID, see GID_* defines
    HWND hwndTarget;                // handle to window targeted by this gesture
    POINTS ptsLocation;             // current location of this gesture
    DWORD dwInstanceID;             // internally used
    DWORD dwSequenceID;             // internally used
    ULONGLONG ullArguments;         // arguments for gestures whose arguments fit in 8 BYTES
    UINT cbExtraArgs;               // size, in bytes, of extra arguments, if any, that accompany this gesture
} GESTUREINFO, *PGESTUREINFO;
typedef GESTUREINFO const * PCGESTUREINFO;

/*
 * Gesture notification structure
 *   - The WM_GESTURENOTIFY message lParam contains a pointer to this structure.
 *   - The WM_GESTURENOTIFY message notifies a window that gesture recognition is
 *     in progress and a gesture will be generated if one is recognized under the
 *     current gesture settings.
 */
typedef struct tagGESTURENOTIFYSTRUCT {
    UINT cbSize;                    // size, in bytes, of this structure
    DWORD dwFlags;                  // unused
    HWND hwndTarget;                // handle to window targeted by the gesture
    POINTS ptsLocation;             // starting location
    DWORD dwInstanceID;             // internally used
} GESTURENOTIFYSTRUCT, *PGESTURENOTIFYSTRUCT;


/*
 * Gesture argument helpers
 *   - Angle should be a double in the range of -2pi to +2pi
 *   - Argument should be an unsigned 16-bit value
 */
#define GID_ROTATE_ANGLE_TO_ARGUMENT(_arg_)     ((USHORT)((((_arg_) + 2.0 * 3.14159265) / (4.0 * 3.14159265)) * 65535.0))
#define GID_ROTATE_ANGLE_FROM_ARGUMENT(_arg_)   ((((double)(_arg_) / 65535.0) * 4.0 * 3.14159265) - 2.0 * 3.14159265)

/*
 * Gesture configuration flags
 */
#define GC_ALLGESTURES                              0x00000001

#define GC_ZOOM                                     0x00000001

#define GC_PAN                                      0x00000001
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY        0x00000002
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY      0x00000004
#define GC_PAN_WITH_GUTTER                          0x00000008
#define GC_PAN_WITH_INERTIA                         0x00000010

#define GC_ROTATE                                   0x00000001

#define GC_TWOFINGERTAP                             0x00000001

#define GC_PRESSANDTAP                              0x00000001

/*
 * Gesture IDs
 */
#define GID_BEGIN                       1
#define GID_END                         2
#define GID_ZOOM                        3
#define GID_PAN                         4
#define GID_ROTATE                      5
#define GID_TWOFINGERTAP                6
#define GID_PRESSANDTAP                 7

// Maximum number of gestures that can be included
// in a single call to SetGestureConfig / GetGestureConfig
#define GESTURECONFIGMAXCOUNT           256

// If specified, GetGestureConfig returns consolidated configuration
// for the specified window and it's parent window chain
#define GCF_INCLUDE_ANCESTORS           0x00000001

// Window events we need to respond to or receive
#define WM_GESTURE                         0x0119
#define WM_GESTURENOTIFY                   0x011A

typedef struct _TOUCHINPUT {
  LONG      x;
  LONG      y;
  HANDLE    hSource;
  DWORD     dwID;
  DWORD     dwFlags;
  DWORD     dwMask;
  DWORD     dwTime;
  ULONG_PTR dwExtraInfo;
  DWORD     cxContact;
  DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;

typedef HANDLE HTOUCHINPUT;

#define WM_TOUCH 0x0240

#define TOUCHEVENTF_MOVE       0x0001
#define TOUCHEVENTF_DOWN       0x0002
#define TOUCHEVENTF_UP         0x0004
#define TOUCHEVENTF_INRANGE    0x0008
#define TOUCHEVENTF_PRIMARY    0x0010
#define TOUCHEVENTF_NOCOALESCE 0x0020
#define TOUCHEVENTF_PEN        0x0040
#define TOUCHEVENTF_PALM       0x0080

#define TOUCHINPUTMASKF_TIMEFROMSYSTEM 0x0001
#define TOUCHINPUTMASKF_EXTRAINFO      0x0002
#define TOUCHINPUTMASKF_CONTACTAREA    0x0004

#define TOUCH_COORD_TO_PIXEL(C) (C/100)

#define TWF_FINETOUCH          0x0001
#define TWF_WANTPALM           0x0002

#endif // WINVER < 0x0602

// WM_TABLET_QUERYSYSTEMGESTURESTATUS return values
#define TABLET_ROTATE_GESTURE_ENABLE    0x02000000

class nsPointWin : public nsIntPoint
{
public:
   nsPointWin& operator=(const POINTS& aPoint) {
     x = aPoint.x; y = aPoint.y;
     return *this;
   }
   nsPointWin& operator=(const POINT& aPoint) {
     x = aPoint.x; y = aPoint.y;
     return *this;
   }
   nsPointWin& operator=(int val) {
     x = y = val;
     return *this;
   }
   void ScreenToClient(HWND hWnd) {
     POINT tmp;
     tmp.x = x; tmp.y = y;
     ::ScreenToClient(hWnd, &tmp);
     *this = tmp;
   }
};

class nsWinGesture
{
public:
  nsWinGesture();

public:
  bool SetWinGestureSupport(HWND hWnd, nsGestureNotifyEvent::ePanDirection aDirection);
  bool ShutdownWinGestureSupport();
  bool RegisterTouchWindow(HWND hWnd);
  bool UnregisterTouchWindow(HWND hWnd);
  bool GetTouchInputInfo(HTOUCHINPUT hTouchInput, uint32_t cInputs, PTOUCHINPUT pInputs);
  bool CloseTouchInputHandle(HTOUCHINPUT hTouchInput);
  bool IsAvailable();
  
  // Simple gesture process
  bool ProcessGestureMessage(HWND hWnd, WPARAM wParam, LPARAM lParam, nsSimpleGestureEvent& evt);

  // Pan processing
  bool IsPanEvent(LPARAM lParam);
  bool ProcessPanMessage(HWND hWnd, WPARAM wParam, LPARAM lParam);
  bool PanDeltaToPixelScroll(mozilla::widget::WheelEvent& aWheelEvent);
  void UpdatePanFeedbackX(HWND hWnd, int32_t scrollOverflow, bool& endFeedback);
  void UpdatePanFeedbackY(HWND hWnd, int32_t scrollOverflow, bool& endFeedback);
  void PanFeedbackFinalize(HWND hWnd, bool endFeedback);
  
public:
  // Helpers
  bool GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
  bool CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
  bool GetGestureExtraArgs(HGESTUREINFO hGestureInfo, UINT cbExtraArgs, PBYTE pExtraArgs);
  bool SetGestureConfig(HWND hWnd, UINT cIDs, PGESTURECONFIG pGestureConfig);
  bool GetGestureConfig(HWND hWnd, DWORD dwFlags, PUINT pcIDs, PGESTURECONFIG pGestureConfig);
  bool BeginPanningFeedback(HWND hWnd);
  bool EndPanningFeedback(HWND hWnd);
  bool UpdatePanningFeedback(HWND hWnd, LONG offsetX, LONG offsetY, BOOL fInInertia);

protected:

private:
  // Function prototypes
  typedef BOOL (WINAPI * GetGestureInfoPtr)(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
  typedef BOOL (WINAPI * CloseGestureInfoHandlePtr)(HGESTUREINFO hGestureInfo);
  typedef BOOL (WINAPI * GetGestureExtraArgsPtr)(HGESTUREINFO hGestureInfo, UINT cbExtraArgs, PBYTE pExtraArgs);
  typedef BOOL (WINAPI * SetGestureConfigPtr)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
  typedef BOOL (WINAPI * GetGestureConfigPtr)(HWND hwnd, DWORD dwReserved, DWORD dwFlags, PUINT pcIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
  typedef BOOL (WINAPI * BeginPanningFeedbackPtr)(HWND hWnd);
  typedef BOOL (WINAPI * EndPanningFeedbackPtr)(HWND hWnd, BOOL fAnimateBack);
  typedef BOOL (WINAPI * UpdatePanningFeedbackPtr)(HWND hWnd, LONG offsetX, LONG offsetY, BOOL fInInertia);
  typedef BOOL (WINAPI * RegisterTouchWindowPtr)(HWND hWnd, ULONG flags);
  typedef BOOL (WINAPI * UnregisterTouchWindowPtr)(HWND hWnd);
  typedef BOOL (WINAPI * GetTouchInputInfoPtr)(HTOUCHINPUT hTouchInput, uint32_t cInputs, PTOUCHINPUT pInputs, int32_t cbSize);
  typedef BOOL (WINAPI * CloseTouchInputHandlePtr)(HTOUCHINPUT hTouchInput);

  // Static function pointers
  static GetGestureInfoPtr getGestureInfo;
  static CloseGestureInfoHandlePtr closeGestureInfoHandle;
  static GetGestureExtraArgsPtr getGestureExtraArgs;
  static SetGestureConfigPtr setGestureConfig;
  static GetGestureConfigPtr getGestureConfig;
  static BeginPanningFeedbackPtr beginPanningFeedback;
  static EndPanningFeedbackPtr endPanningFeedback;
  static UpdatePanningFeedbackPtr updatePanningFeedback;
  static RegisterTouchWindowPtr registerTouchWindow;
  static UnregisterTouchWindowPtr unregisterTouchWindow;
  static GetTouchInputInfoPtr getTouchInputInfo;
  static CloseTouchInputHandlePtr closeTouchInputHandle;

  // Delay load info 
  bool InitLibrary();

  static HMODULE sLibraryHandle;
  static const PRUnichar kGestureLibraryName[];

  // Pan and feedback state
  nsPointWin mPanIntermediate;
  nsPointWin mPanRefPoint;
  nsPointWin mPixelScrollDelta;
  bool mPanActive;
  bool mFeedbackActive;
  bool mXAxisFeedback;
  bool mYAxisFeedback;
  bool mPanInertiaActive;
  nsPointWin mPixelScrollOverflow;

  // Zoom state
  double mZoomIntermediate;

  // Rotate state
  double mRotateIntermediate;
};

#endif /* WinGesture_h__ */


