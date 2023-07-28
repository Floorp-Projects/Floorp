/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinMessages_h_
#define mozilla_widget_WinMessages_h_

/*****************************************************************************
 * MOZ_WM_* messages
 ****************************************************************************/

// A magic APP message that can be sent to quit, sort of like a
// QUERYENDSESSION/ENDSESSION, but without the query.
#define MOZ_WM_APP_QUIT (WM_APP + 0x0300)
// Used as a "tracer" event to probe event loop latency.
#define MOZ_WM_TRACE (WM_APP + 0x0301)
// accessibility priming
#define MOZ_WM_STARTA11Y (WM_APP + 0x0302)
// Our internal message for WM_MOUSEWHEEL, WM_MOUSEHWHEEL, WM_VSCROLL and
// WM_HSCROLL
#define MOZ_WM_MOUSEVWHEEL (WM_APP + 0x0310)
#define MOZ_WM_MOUSEHWHEEL (WM_APP + 0x0311)
#define MOZ_WM_VSCROLL (WM_APP + 0x0312)
#define MOZ_WM_HSCROLL (WM_APP + 0x0313)
#define MOZ_WM_MOUSEWHEEL_FIRST MOZ_WM_MOUSEVWHEEL
#define MOZ_WM_MOUSEWHEEL_LAST MOZ_WM_HSCROLL
// If a popup window is being activated, we try to reactivate the previous
// window with this message.
#define MOZ_WM_REACTIVATE (WM_APP + 0x0314)
// If TSFTextStore needs to notify TSF/TIP of layout change later, this
// message is posted.
#define MOZ_WM_NOTIY_TSF_OF_LAYOUT_CHANGE (WM_APP + 0x0315)
// Internal message used in correcting backwards clock skew
#define MOZ_WM_SKEWFIX (WM_APP + 0x0316)
// Internal message used for rolling up popups for dmanip events
#define MOZ_WM_DMANIP (WM_APP + 0x0317)

// Internal message used to work around race condition in explorer.exe's
// fullscreen window-state update handler in Windows 10+. (See bug 1835851.)
#define MOZ_WM_FULLSCREEN_STATE_UPDATE (WM_APP + 0x0318)

// XXX Should rename them to MOZ_WM_* and use safer values!
// Messages for fullscreen transition window
#define WM_FULLSCREEN_TRANSITION_BEFORE (WM_USER + 0)
#define WM_FULLSCREEN_TRANSITION_AFTER (WM_USER + 1)

#ifndef APPCOMMAND_BROWSER_BACKWARD
#  define APPCOMMAND_BROWSER_BACKWARD 1
#  define APPCOMMAND_BROWSER_FORWARD 2
#  define APPCOMMAND_BROWSER_REFRESH 3
#  define APPCOMMAND_BROWSER_STOP 4
#  define APPCOMMAND_BROWSER_SEARCH 5
#  define APPCOMMAND_BROWSER_FAVORITES 6
#  define APPCOMMAND_BROWSER_HOME 7

#  define APPCOMMAND_MEDIA_NEXTTRACK 11
#  define APPCOMMAND_MEDIA_PREVIOUSTRACK 12
#  define APPCOMMAND_MEDIA_STOP 13
#  define APPCOMMAND_MEDIA_PLAY_PAUSE 14

/*
 * Additional commands currently not in use.
 *
 *#define APPCOMMAND_VOLUME_MUTE            8
 *#define APPCOMMAND_VOLUME_DOWN            9
 *#define APPCOMMAND_VOLUME_UP              10
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

#  define GET_APPCOMMAND_LPARAM(lParam) \
    ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))

/*
 *#define GET_DEVICE_LPARAM(lParam)         ((WORD)(HIWORD(lParam) &
 *FAPPCOMMAND_MASK)) #define GET_MOUSEORKEY_LPARAM             GET_DEVICE_LPARAM
 *#define GET_FLAGS_LPARAM(lParam)          (LOWORD(lParam))
 *#define GET_KEYSTATE_LPARAM(lParam)       GET_FLAGS_LPARAM(lParam)
 */
#endif  // #ifndef APPCOMMAND_BROWSER_BACKWARD

#endif  // #ifndef mozilla_widget_WinMessages_h_
