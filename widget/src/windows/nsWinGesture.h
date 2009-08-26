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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>.
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

#ifndef WinGesture_h__
#define WinGesture_h__

/*
 * nsWinGesture - Touch input handling for tablet displays.
 */

#include "nsdefs.h"
#include <winuser.h>
#include "nsPoint.h"
#include "nsGUIEvent.h"

#if !defined(NTDDI_WIN7) ||  NTDDI_VERSION < NTDDI_WIN7

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
#define WM_TABLET_QUERYSYSTEMGESTURESTATUS 0x02CC
#define WM_GESTURE                         0x0119
#define WM_GESTURENOTIFY                   0x011A

// WM_TABLET_QUERYSYSTEMGESTURESTATUS return values
#define TABLET_ROTATE_GESTURE_ENABLE    0x02000000

#endif /* NTDDI_VERSION < NTDDI_WIN7 */

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
  PRBool SetWinGestureSupport(HWND hWnd, nsGestureNotifyEvent::ePanDirection aDirection);
  PRBool ShutdownWinGestureSupport();
  PRBool IsAvailable();
  
  // Simple gesture process
  PRBool ProcessGestureMessage(HWND hWnd, WPARAM wParam, LPARAM lParam, nsSimpleGestureEvent& evt);

  // Pan processing
  PRBool IsPanEvent(LPARAM lParam);
  PRBool ProcessPanMessage(HWND hWnd, WPARAM wParam, LPARAM lParam);
  PRBool PanDeltaToPixelScrollX(nsMouseScrollEvent& evt);
  PRBool PanDeltaToPixelScrollY(nsMouseScrollEvent& evt);
  void UpdatePanFeedbackX(HWND hWnd, PRInt32 scrollOverflow, PRBool& endFeedback);
  void UpdatePanFeedbackY(HWND hWnd, PRInt32 scrollOverflow, PRBool& endFeedback);
  void PanFeedbackFinalize(HWND hWnd, PRBool endFeedback);
  
public:
  // Helpers
  PRBool GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
  PRBool CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
  PRBool GetGestureExtraArgs(HGESTUREINFO hGestureInfo, UINT cbExtraArgs, PBYTE pExtraArgs);
  PRBool SetGestureConfig(HWND hWnd, UINT cIDs, PGESTURECONFIG pGestureConfig);
  PRBool GetGestureConfig(HWND hWnd, DWORD dwFlags, PUINT pcIDs, PGESTURECONFIG pGestureConfig);
  PRBool BeginPanningFeedback(HWND hWnd);
  PRBool EndPanningFeedback(HWND hWnd);
  PRBool UpdatePanningFeedback(HWND hWnd, LONG offsetX, LONG offsetY, BOOL fInInertia);

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

  // Static function pointers
  static GetGestureInfoPtr getGestureInfo;
  static CloseGestureInfoHandlePtr closeGestureInfoHandle;
  static GetGestureExtraArgsPtr getGestureExtraArgs;
  static SetGestureConfigPtr setGestureConfig;
  static GetGestureConfigPtr getGestureConfig;
  static BeginPanningFeedbackPtr beginPanningFeedback;
  static EndPanningFeedbackPtr endPanningFeedback;
  static UpdatePanningFeedbackPtr updatePanningFeedback;

  // Delay load info 
  PRBool InitLibrary();

  static HMODULE sLibraryHandle;
  static const PRUnichar kGestureLibraryName[];
  static const PRUnichar kThemeLibraryName[];

  // Pan and feedback state
  nsPointWin mPanIntermediate;
  nsPointWin mPanRefPoint;
  nsPointWin mPixelScrollDelta;
  PRPackedBool mPanActive;
  PRPackedBool mFeedbackActive;
  PRPackedBool mXAxisFeedback;
  PRPackedBool mYAxisFeedback;
  PRPackedBool mPanInertiaActive;
  nsPointWin mPixelScrollOverflow;

  // Zoom state
  double mZoomIntermediate;

  // Rotate state
  double mRotateIntermediate;
};

#endif /* WinGesture_h__ */


