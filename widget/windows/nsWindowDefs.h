/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowDefs_h__
#define WindowDefs_h__

/*
 * nsWindowDefs - nsWindow related definitions, consts, and macros.
 */

#include "mozilla/widget/WinMessages.h"
#include "nsBaseWidget.h"
#include "nsdefs.h"
#include "resource.h"

/**************************************************************
 *
 * SECTION: defines
 * 
 **************************************************************/

// ConstrainPosition window positioning slop value
#define kWindowPositionSlop               20

// Origin of the system context menu when displayed in full screen mode
#define MOZ_SYSCONTEXT_X_POS              20
#define MOZ_SYSCONTEXT_Y_POS              20

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION               100

//Tablet PC Mouse Input Source
#define TABLET_INK_SIGNATURE 0xFFFFFF00
#define TABLET_INK_CHECK     0xFF515700
#define TABLET_INK_TOUCH     0x00000080
#define TABLET_INK_ID_MASK   0x0000007F
#define MOUSE_INPUT_SOURCE() WinUtils::GetMouseInputSource()
#define MOUSE_POINTERID()    WinUtils::GetMousePointerID()

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
const wchar_t kClassNameHidden[]     = L"MozillaHiddenWindowClass";
const wchar_t kClassNameGeneral[]    = L"MozillaWindowClass";
const wchar_t kClassNameDialog[]     = L"MozillaDialogClass";
const wchar_t kClassNameDropShadow[] = L"MozillaDropShadowWindowClass";
const wchar_t kClassNameTemp[]       = L"MozillaTempWindowClass";
const wchar_t kClassNameTransition[] = L"MozillaTransitionWindowClass";

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

namespace mozilla {
namespace widget {

struct MSGResult
{
  // Result for the message.
  LRESULT& mResult;
  // If mConsumed is true, the caller shouldn't call next wndproc.
  bool mConsumed;

  MSGResult(LRESULT* aResult = nullptr) :
    mResult(aResult ? *aResult : mDefaultResult), mConsumed(false)
  {
  }

private:
  LRESULT mDefaultResult;
};

} // namespace widget
} // namespace mozilla

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
