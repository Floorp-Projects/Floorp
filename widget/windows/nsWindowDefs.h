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

#ifndef WindowDefs_h__
#define WindowDefs_h__

/*
 * nsWindowDefs - nsWindow related definitions, consts, and macros.
 */

#include "nsBaseWidget.h"
#include "nsdefs.h"
#include "resource.h"

/**************************************************************
 *
 * SECTION: defines
 * 
 **************************************************************/

// A magic APP message that can be sent to quit, sort of like a QUERYENDSESSION/ENDSESSION,
// but without the query.
#define MOZ_WM_APP_QUIT                   (WM_APP+0x0300)
// Used as a "tracer" event to probe event loop latency.
#define MOZ_WM_TRACE                      (WM_APP+0x0301)
// Our internal message for WM_MOUSEWHEEL, WM_MOUSEHWHEEL, WM_VSCROLL and
// WM_HSCROLL
#define MOZ_WM_MOUSEVWHEEL                (WM_APP+0x0310)
#define MOZ_WM_MOUSEHWHEEL                (WM_APP+0x0311)
#define MOZ_WM_VSCROLL                    (WM_APP+0x0312)
#define MOZ_WM_HSCROLL                    (WM_APP+0x0313)
// Internal message for ensuring the file picker is visible on multi monitor
// systems, and when the screen resolution changes.
#define MOZ_WM_ENSUREVISIBLE              (WM_APP + 14159)

// GetWindowsVersion constants
#define WIN2K_VERSION                     0x500
#define WINXP_VERSION                     0x501
#define WIN2K3_VERSION                    0x502
#define VISTA_VERSION                     0x600
#define WIN7_VERSION                      0x601

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED                   0x031A
#endif

#ifndef WM_GETOBJECT
#define WM_GETOBJECT                      0x03d
#endif

#ifndef PBT_APMRESUMEAUTOMATIC
#define PBT_APMRESUMEAUTOMATIC            0x0012
#endif

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL                    0x020E
#endif

#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE                     0x02A3
#endif

#ifndef SPI_GETWHEELSCROLLCHARS
#define SPI_GETWHEELSCROLLCHARS           0x006C
#endif

#ifndef SPI_SETWHEELSCROLLCHARS
#define SPI_SETWHEELSCROLLCHARS           0x006D
#endif

#ifndef MAPVK_VSC_TO_VK
#define MAPVK_VK_TO_VSC                   0
#define MAPVK_VSC_TO_VK                   1
#define MAPVK_VK_TO_CHAR                  2
#endif

// ConstrainPosition window positioning slop value
#define kWindowPositionSlop               20

// Origin of the system context menu when displayed in full screen mode
#define MOZ_SYSCONTEXT_X_POS              20
#define MOZ_SYSCONTEXT_Y_POS              20

// Drop shadow window style
#define CS_XP_DROPSHADOW                  0x00020000

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION               100

// App Command messages for IntelliMouse and Natural Keyboard Pro
// These messages are not included in Visual C++ 6.0, but are in 7.0+
#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND                     0x0319
#endif

#define FAPPCOMMAND_MASK                  0xF000

#ifndef WM_GETTITLEBARINFOEX
#define WM_GETTITLEBARINFOEX              0x033F
#endif

#ifndef CCHILDREN_TITLEBAR
#define CCHILDREN_TITLEBAR                5
#endif

#ifndef APPCOMMAND_BROWSER_BACKWARD
  #define APPCOMMAND_BROWSER_BACKWARD       1
  #define APPCOMMAND_BROWSER_FORWARD        2
  #define APPCOMMAND_BROWSER_REFRESH        3
  #define APPCOMMAND_BROWSER_STOP           4
  #define APPCOMMAND_BROWSER_SEARCH         5
  #define APPCOMMAND_BROWSER_FAVORITES      6
  #define APPCOMMAND_BROWSER_HOME           7

  /* 
   * Additional commands currently not in use.
   *
   *#define APPCOMMAND_VOLUME_MUTE            8
   *#define APPCOMMAND_VOLUME_DOWN            9
   *#define APPCOMMAND_VOLUME_UP              10
   *#define APPCOMMAND_MEDIA_NEXTTRACK        11
   *#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
   *#define APPCOMMAND_MEDIA_STOP             13
   *#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
   *#define APPCOMMAND_LAUNCH_MAIL            15
   *#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
   *#define APPCOMMAND_LAUNCH_APP1            17
   *#define APPCOMMAND_LAUNCH_APP2            18
   *#define APPCOMMAND_BASS_DOWN              19
   *#define APPCOMMAND_BASS_BOOST             20
   *#define APPCOMMAND_BASS_UP                21
   *#define APPCOMMAND_TREBLE_DOWN            22
   *#define APPCOMMAND_TREBLE_UP              23
   *#define FAPPCOMMAND_MOUSE                 0x8000
   *#define FAPPCOMMAND_KEY                   0
   *#define FAPPCOMMAND_OEM                   0x1000
   */

  #define GET_APPCOMMAND_LPARAM(lParam)     ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))

  /*
   *#define GET_DEVICE_LPARAM(lParam)         ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
   *#define GET_MOUSEORKEY_LPARAM             GET_DEVICE_LPARAM
   *#define GET_FLAGS_LPARAM(lParam)          (LOWORD(lParam))
   *#define GET_KEYSTATE_LPARAM(lParam)       GET_FLAGS_LPARAM(lParam)
   */
#endif // #ifndef APPCOMMAND_BROWSER_BACKWARD

//Tablet PC Mouse Input Source
#define TABLET_INK_SIGNATURE 0xFFFFFF00
#define TABLET_INK_CHECK     0xFF515700
#define TABLET_INK_TOUCH     0x00000080
#define MOUSE_INPUT_SOURCE() GetMouseInputSource()

/**************************************************************
 *
 * SECTION: enums
 * 
 **************************************************************/

// nsWindow::sCanQuit
typedef enum
{
    TRI_UNKNOWN = -1,
    TRI_FALSE = 0,
    TRI_TRUE = 1
} TriStateBool;

/**************************************************************
 *
 * SECTION: constants
 * 
 **************************************************************/

/*
 * Native windows class names
 *
 * ::: IMPORTANT :::
 *
 * External apps and drivers depend on window class names.
 * For example, changing the window classes could break
 * touchpad scrolling or screen readers.
 */
const PRUint32 kMaxClassNameLength   = 40;
const char kClassNameHidden[]        = "MozillaHiddenWindowClass";
const char kClassNameGeneral[]       = "MozillaWindowClass";
const char kClassNameDialog[]        = "MozillaDialogClass";
const char kClassNameDropShadow[]    = "MozillaDropShadowWindowClass";
const char kClassNameTemp[]          = "MozillaTempWindowClass";

static const PRUint32 sModifierKeyMap[][3] = {
  { nsIWidget::CAPS_LOCK, VK_CAPITAL, 0 },
  { nsIWidget::NUM_LOCK,  VK_NUMLOCK, 0 },
  { nsIWidget::SHIFT_L,   VK_SHIFT,   VK_LSHIFT },
  { nsIWidget::SHIFT_R,   VK_SHIFT,   VK_RSHIFT },
  { nsIWidget::CTRL_L,    VK_CONTROL, VK_LCONTROL },
  { nsIWidget::CTRL_R,    VK_CONTROL, VK_RCONTROL },
  { nsIWidget::ALT_L,     VK_MENU,    VK_LMENU },
  { nsIWidget::ALT_R,     VK_MENU,    VK_RMENU }
};

/**************************************************************
 *
 * SECTION: structs
 * 
 **************************************************************/

// Used in OnKeyDown
struct nsAlternativeCharCode; // defined in nsGUIEvent.h
struct nsFakeCharMessage {
  UINT mCharCode;
  UINT mScanCode;

  MSG GetCharMessage(HWND aWnd)
  {
    MSG msg;
    msg.hwnd = aWnd;
    msg.message = WM_CHAR;
    msg.wParam = static_cast<WPARAM>(mCharCode);
    msg.lParam = static_cast<LPARAM>(mScanCode);
    msg.time = 0;
    msg.pt.x = msg.pt.y = 0;
    return msg;
  }
};

// Used in char processing
struct nsModifierKeyState {
  bool mIsShiftDown;
  bool mIsControlDown;
  bool mIsAltDown;

  nsModifierKeyState();
  nsModifierKeyState(bool aIsShiftDown, bool aIsControlDown,
                     bool aIsAltDown) :
    mIsShiftDown(aIsShiftDown), mIsControlDown(aIsControlDown),
    mIsAltDown(aIsAltDown)
  {
  }
};

// Used for synthesizing events
struct KeyPair {
  PRUint8 mGeneral;
  PRUint8 mSpecific;
  KeyPair(PRUint32 aGeneral, PRUint32 aSpecific)
    : mGeneral(PRUint8(aGeneral)), mSpecific(PRUint8(aSpecific)) {}
};

#ifndef TITLEBARINFOEX
struct TITLEBARINFOEX
{
    DWORD cbSize;
    RECT rcTitleBar;
    DWORD rgstate[CCHILDREN_TITLEBAR + 1];
    RECT rgrect[CCHILDREN_TITLEBAR + 1];
};
#endif

/**************************************************************
 *
 * SECTION: macros
 * 
 **************************************************************/

#define NSRGB_2_COLOREF(color) \
      RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))
#define COLOREF_2_NSRGB(color) \
      NS_RGB(GetRValue(color), GetGValue(color), GetBValue(color))

#define VERIFY_WINDOW_STYLE(s) \
      NS_ASSERTION(((s) & (WS_CHILD | WS_POPUP)) != (WS_CHILD | WS_POPUP), \
      "WS_POPUP and WS_CHILD are mutually exclusive")

#endif /* WindowDefs_h__ */
