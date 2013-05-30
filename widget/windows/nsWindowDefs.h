/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#define MOZ_WM_MOUSEWHEEL_FIRST           MOZ_WM_MOUSEVWHEEL
#define MOZ_WM_MOUSEWHEEL_LAST            MOZ_WM_HSCROLL

// Internal message for ensuring the file picker is visible on multi monitor
// systems, and when the screen resolution changes.
#define MOZ_WM_ENSUREVISIBLE              (WM_APP + 14159)

#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER                 92
#endif

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

#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL                0x01000
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
#define MAPVK_VSC_TO_VK_EX                3
#define MAPVK_VK_TO_VSC_EX                4
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
#define MOUSE_INPUT_SOURCE() WinUtils::GetMouseInputSource()

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
const uint32_t kMaxClassNameLength   = 40;
const char kClassNameHidden[]        = "MozillaHiddenWindowClass";
const char kClassNameGeneral[]       = "MozillaWindowClass";
const char kClassNameDialog[]        = "MozillaDialogClass";
const char kClassNameDropShadow[]    = "MozillaDropShadowWindowClass";
const char kClassNameTemp[]          = "MozillaTempWindowClass";

/**************************************************************
 *
 * SECTION: structs
 * 
 **************************************************************/

// Used for synthesizing events
struct KeyPair {
  uint8_t mGeneral;
  uint8_t mSpecific;
  KeyPair(uint32_t aGeneral, uint32_t aSpecific)
    : mGeneral(uint8_t(aGeneral)), mSpecific(uint8_t(aSpecific)) {}
};

#if (WINVER < 0x0600)
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
