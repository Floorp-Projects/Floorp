/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file defines simple key mapping between native keycode value and
 * DOM key name index.
 * You must define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX macro before include
 * this.
 *
 * It must have two arguments, (aNativeKey, aKeyNameIndex).
 * aNativeKey is a native keycode value.
 * aKeyNameIndex is the widget::KeyNameIndex value.
 */

// Windows (both Desktop and Metro)
#define KEY_MAP_WIN(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_JPN(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_KOR(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_OTH(aCPPKeyName, aNativeKey)
// OS/2
#define KEY_MAP_OS2(aCPPKeyName, aNativeKey)
// Mac OS X
#define KEY_MAP_COCOA(aCPPKeyName, aNativeKey)
// GTK
#define KEY_MAP_GTK(aCPPKeyName, aNativeKey)
// Qt
#define KEY_MAP_QT(aCPPKeyName, aNativeKey)
// Android and Gonk
#define KEY_MAP_ANDROID(aCPPKeyName, aNativeKey)

#if defined(XP_WIN)
// KEY_MAP_WIN() defines the mapping not depending on keyboard layout.
#undef KEY_MAP_WIN
#define KEY_MAP_WIN(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
// KEY_MAP_WIN_JPN() defines the mapping which is valid only with Japanese
// keyboard layout.
#undef KEY_MAP_WIN_JPN
#define KEY_MAP_WIN_JPN(aCPPKeyName, aNativeKey) \
  NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                               KEY_NAME_INDEX_##aCPPKeyName)
// KEY_MAP_WIN_KOR() defines the mapping which is valid only with Korean
// keyboard layout.
#undef KEY_MAP_WIN_KOR
#define KEY_MAP_WIN_KOR(aCPPKeyName, aNativeKey) \
  NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                             KEY_NAME_INDEX_##aCPPKeyName)
// KEY_MAP_WIN_OTH() defines the mapping which is valid with neither
// Japanese keyboard layout nor Korean keyboard layout.
#undef KEY_MAP_WIN_OTH
#define KEY_MAP_WIN_OTH(aCPPKeyName, aNativeKey) \
  NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                            KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(XP_MACOSX)
#undef KEY_MAP_COCOA
#define KEY_MAP_COCOA(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(MOZ_WIDGET_GTK)
#undef KEY_MAP_GTK
#define KEY_MAP_GTK(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(MOZ_WIDGET_QT)
#undef KEY_MAP_QT
#define KEY_MAP_QT(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(ANDROID)
#undef KEY_MAP_ANDROID
#define KEY_MAP_ANDROID(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#endif

// Attn
KEY_MAP_WIN_OTH (Attn, VK_ATTN) // not valid with Japanese keyboard layout
KEY_MAP_GTK     (Attn, GDK_3270_Attn) // legacy IBM keyboard layout

// Apps
KEY_MAP_ANDROID (Apps, AKEYCODE_APP_SWITCH)

// Crsel
KEY_MAP_WIN     (Crsel, VK_CRSEL)
KEY_MAP_GTK     (Crsel, GDK_3270_CursorSelect) // legacy IBM keyboard layout

// Exsel
KEY_MAP_WIN     (Exsel, VK_EXSEL)
KEY_MAP_GTK     (Exsel, GDK_3270_ExSelect) // legacy IBM keyboard layout

// F1
KEY_MAP_WIN     (F1, VK_F1)
KEY_MAP_COCOA   (F1, kVK_F1)
KEY_MAP_GTK     (F1, GDK_F1)
KEY_MAP_GTK     (F1, GDK_KP_F1)
KEY_MAP_QT      (F1, Qt::Key_F1)
KEY_MAP_ANDROID (F1, AKEYCODE_F1)

// F2
KEY_MAP_WIN     (F2, VK_F2)
KEY_MAP_COCOA   (F2, kVK_F2)
KEY_MAP_GTK     (F2, GDK_F2)
KEY_MAP_GTK     (F2, GDK_KP_F2)
KEY_MAP_QT      (F2, Qt::Key_F2)
KEY_MAP_ANDROID (F2, AKEYCODE_F2)

// F3
KEY_MAP_WIN     (F3, VK_F3)
KEY_MAP_COCOA   (F3, kVK_F3)
KEY_MAP_GTK     (F3, GDK_F3)
KEY_MAP_GTK     (F3, GDK_KP_F3)
KEY_MAP_QT      (F3, Qt::Key_F3)
KEY_MAP_ANDROID (F3, AKEYCODE_F3)

// F4
KEY_MAP_WIN     (F4, VK_F4)
KEY_MAP_COCOA   (F4, kVK_F4)
KEY_MAP_GTK     (F4, GDK_F4)
KEY_MAP_GTK     (F4, GDK_KP_F4)
KEY_MAP_QT      (F4, Qt::Key_F4)
KEY_MAP_ANDROID (F4, AKEYCODE_F4)

// F5
KEY_MAP_WIN     (F5, VK_F5)
KEY_MAP_COCOA   (F5, kVK_F5)
KEY_MAP_GTK     (F5, GDK_F5)
KEY_MAP_QT      (F5, Qt::Key_F5)
KEY_MAP_ANDROID (F5, AKEYCODE_F5)

// F6
KEY_MAP_WIN     (F6, VK_F6)
KEY_MAP_COCOA   (F6, kVK_F6)
KEY_MAP_GTK     (F6, GDK_F6)
KEY_MAP_QT      (F6, Qt::Key_F6)
KEY_MAP_ANDROID (F6, AKEYCODE_F6)

// F7
KEY_MAP_WIN     (F7, VK_F7)
KEY_MAP_COCOA   (F7, kVK_F7)
KEY_MAP_GTK     (F7, GDK_F7)
KEY_MAP_QT      (F7, Qt::Key_F7)
KEY_MAP_ANDROID (F7, AKEYCODE_F7)

// F8
KEY_MAP_WIN     (F8, VK_F8)
KEY_MAP_COCOA   (F8, kVK_F8)
KEY_MAP_GTK     (F8, GDK_F8)
KEY_MAP_QT      (F8, Qt::Key_F8)
KEY_MAP_ANDROID (F8, AKEYCODE_F8)

// F9
KEY_MAP_WIN     (F9, VK_F9)
KEY_MAP_COCOA   (F9, kVK_F9)
KEY_MAP_GTK     (F9, GDK_F9)
KEY_MAP_QT      (F9, Qt::Key_F9)
KEY_MAP_ANDROID (F9, AKEYCODE_F9)

// F10
KEY_MAP_WIN     (F10, VK_F10)
KEY_MAP_COCOA   (F10, kVK_F10)
KEY_MAP_GTK     (F10, GDK_F10)
KEY_MAP_QT      (F10, Qt::Key_F10)
KEY_MAP_ANDROID (F10, AKEYCODE_F10)

// F11
KEY_MAP_WIN     (F11, VK_F11)
KEY_MAP_COCOA   (F11, kVK_F11)
KEY_MAP_GTK     (F11, GDK_F11 /* same as GDK_L1 */)
KEY_MAP_QT      (F11, Qt::Key_F11)
KEY_MAP_ANDROID (F11, AKEYCODE_F11)

// F12
KEY_MAP_WIN     (F12, VK_F12)
KEY_MAP_COCOA   (F12, kVK_F12)
KEY_MAP_GTK     (F12, GDK_F12 /* same as GDK_L2 */)
KEY_MAP_QT      (F12, Qt::Key_F12)
KEY_MAP_ANDROID (F12, AKEYCODE_F12)

// F13
KEY_MAP_WIN     (F13, VK_F13)
KEY_MAP_COCOA   (F13, kVK_F13)
KEY_MAP_GTK     (F13, GDK_F13 /* same as GDK_L3 */)
KEY_MAP_QT      (F13, Qt::Key_F13)

// F14
KEY_MAP_WIN     (F14, VK_F14)
KEY_MAP_COCOA   (F14, kVK_F14)
KEY_MAP_GTK     (F14, GDK_F14 /* same as GDK_L4 */)
KEY_MAP_QT      (F14, Qt::Key_F14)

// F15
KEY_MAP_WIN     (F15, VK_F15)
KEY_MAP_COCOA   (F15, kVK_F15)
KEY_MAP_GTK     (F15, GDK_F15 /* same as GDK_L5 */)
KEY_MAP_QT      (F15, Qt::Key_F15)

// F16
KEY_MAP_WIN     (F16, VK_F16)
KEY_MAP_COCOA   (F16, kVK_F16)
KEY_MAP_GTK     (F16, GDK_F16 /* same as GDK_L6 */)
KEY_MAP_QT      (F16, Qt::Key_F16)

// F17
KEY_MAP_WIN     (F17, VK_F17)
KEY_MAP_COCOA   (F17, kVK_F17)
KEY_MAP_GTK     (F17, GDK_F17 /* same as GDK_L7 */)
KEY_MAP_QT      (F17, Qt::Key_F17)

// F18
KEY_MAP_WIN     (F18, VK_F18)
KEY_MAP_COCOA   (F18, kVK_F18)
KEY_MAP_GTK     (F18, GDK_F18 /* same as GDK_L8 */)
KEY_MAP_QT      (F18, Qt::Key_F18)

// F19
KEY_MAP_WIN     (F19, VK_F19)
KEY_MAP_COCOA   (F19, kVK_F19)
KEY_MAP_GTK     (F19, GDK_F19 /* same as GDK_L9 */)
KEY_MAP_QT      (F19, Qt::Key_F19)

// F20
KEY_MAP_WIN     (F20, VK_F20)
KEY_MAP_GTK     (F20, GDK_F20 /* same as GDK_L10 */)
KEY_MAP_QT      (F20, Qt::Key_F20)

// F21
KEY_MAP_WIN     (F21, VK_F21)
KEY_MAP_GTK     (F21, GDK_F21 /* same as GDK_R1 */)
KEY_MAP_QT      (F21, Qt::Key_F21)

// F22
KEY_MAP_WIN     (F22, VK_F22)
KEY_MAP_GTK     (F22, GDK_F22 /* same as GDK_R2 */)
KEY_MAP_QT      (F22, Qt::Key_F22)

// F23
KEY_MAP_WIN     (F23, VK_F23)
KEY_MAP_GTK     (F23, GDK_F23 /* same as GDK_R3 */)
KEY_MAP_QT      (F23, Qt::Key_F23)

// F24
KEY_MAP_WIN     (F24, VK_F24)
KEY_MAP_GTK     (F24, GDK_F24 /* same as GDK_R4 */)
KEY_MAP_QT      (F24, Qt::Key_F24)

// F25
KEY_MAP_GTK     (F25, GDK_F25 /* same as GDK_R5 */)
KEY_MAP_QT      (F25, Qt::Key_F25)

// F26
KEY_MAP_GTK     (F26, GDK_F26 /* same as GDK_R6 */)
KEY_MAP_QT      (F26, Qt::Key_F26)

// F27
KEY_MAP_GTK     (F27, GDK_F27 /* same as GDK_R7 */)
KEY_MAP_QT      (F27, Qt::Key_F27)

// F28
KEY_MAP_GTK     (F28, GDK_F28 /* same as GDK_R8 */)
KEY_MAP_QT      (F28, Qt::Key_F28)

// F29
KEY_MAP_GTK     (F29, GDK_F29 /* same as GDK_R9 */)
KEY_MAP_QT      (F29, Qt::Key_F29)

// F30
KEY_MAP_GTK     (F30, GDK_F30 /* same as GDK_R10 */)
KEY_MAP_QT      (F30, Qt::Key_F30)

// F31
KEY_MAP_GTK     (F31, GDK_F31 /* same as GDK_R11 */)
KEY_MAP_QT      (F31, Qt::Key_F31)

// F32
KEY_MAP_GTK     (F32, GDK_F32 /* same as GDK_R12 */)
KEY_MAP_QT      (F32, Qt::Key_F32)

// F33
KEY_MAP_GTK     (F33, GDK_F33 /* same as GDK_R13 */)
KEY_MAP_QT      (F33, Qt::Key_F33)

// F34
KEY_MAP_GTK     (F34, GDK_F34 /* same as GDK_R14 */)
KEY_MAP_QT      (F34, Qt::Key_F34)

// F35
KEY_MAP_GTK     (F35, GDK_F35 /* same as GDK_R15 */)
KEY_MAP_QT      (F35, Qt::Key_F35)

// LaunchApplication1
KEY_MAP_WIN     (LaunchApplication1, VK_LAUNCH_APP1)
KEY_MAP_GTK     (LaunchApplication1, GDK_Launch0)
KEY_MAP_QT      (LaunchApplication1, Qt::Key_Launch0)
KEY_MAP_ANDROID (LaunchApplication1, AKEYCODE_EXPLORER)

// LaunchApplication2
KEY_MAP_WIN     (LaunchApplication2, VK_LAUNCH_APP2)
KEY_MAP_GTK     (LaunchApplication2, GDK_Launch1)
KEY_MAP_QT      (LaunchApplication2, Qt::Key_Launch1)

// LaunchApplication3
KEY_MAP_GTK     (LaunchApplication3, GDK_Launch2)
KEY_MAP_QT      (LaunchApplication3, Qt::Key_Launch2)

// LaunchApplication4
KEY_MAP_GTK     (LaunchApplication4, GDK_Launch3)
KEY_MAP_QT      (LaunchApplication4, Qt::Key_Launch3)

// LaunchApplication5
KEY_MAP_GTK     (LaunchApplication5, GDK_Launch4)
KEY_MAP_QT      (LaunchApplication5, Qt::Key_Launch4)

// LaunchApplication6
KEY_MAP_GTK     (LaunchApplication6, GDK_Launch5)
KEY_MAP_QT      (LaunchApplication6, Qt::Key_Launch5)

// LaunchApplication7
KEY_MAP_GTK     (LaunchApplication7, GDK_Launch6)
KEY_MAP_QT      (LaunchApplication7, Qt::Key_Launch6)

// LaunchApplication8
KEY_MAP_GTK     (LaunchApplication8, GDK_Launch7)
KEY_MAP_QT      (LaunchApplication8, Qt::Key_Launch7)

// LaunchApplication9
KEY_MAP_GTK     (LaunchApplication9, GDK_Launch8)
KEY_MAP_QT      (LaunchApplication9, Qt::Key_Launch8)

// LaunchApplication10
KEY_MAP_GTK     (LaunchApplication10, GDK_Launch9)
KEY_MAP_QT      (LaunchApplication10, Qt::Key_Launch9)

// LaunchApplication11
KEY_MAP_GTK     (LaunchApplication11, GDK_LaunchA)
KEY_MAP_QT      (LaunchApplication11, Qt::Key_LaunchA)

// LaunchApplication12
KEY_MAP_GTK     (LaunchApplication12, GDK_LaunchB)
KEY_MAP_QT      (LaunchApplication12, Qt::Key_LaunchB)

// LaunchApplication13
KEY_MAP_GTK     (LaunchApplication13, GDK_LaunchC)
KEY_MAP_QT      (LaunchApplication13, Qt::Key_LaunchC)

// LaunchApplication14
KEY_MAP_GTK     (LaunchApplication14, GDK_LaunchD)
KEY_MAP_QT      (LaunchApplication14, Qt::Key_LaunchD)

// LaunchApplication15
KEY_MAP_GTK     (LaunchApplication15, GDK_LaunchE)
KEY_MAP_QT      (LaunchApplication15, Qt::Key_LaunchE)

// LaunchApplication16
KEY_MAP_GTK     (LaunchApplication16, GDK_LaunchF)
KEY_MAP_QT      (LaunchApplication16, Qt::Key_LaunchF)

// LaunchApplication17
KEY_MAP_QT      (LaunchApplication17, Qt::Key_LaunchG)

// LaunchApplication18
KEY_MAP_QT      (LaunchApplication18, Qt::Key_LaunchH)

// LaunchMail
KEY_MAP_WIN     (LaunchMail, VK_LAUNCH_MAIL)
KEY_MAP_GTK     (LaunchMail, GDK_Mail)
KEY_MAP_QT      (LaunchMail, Qt::Key_LaunchMail)
KEY_MAP_ANDROID (LaunchMail, AKEYCODE_ENVELOPE)

// List

// Props

// Soft1

// Soft2

// Soft3

// Soft4

// Accept
KEY_MAP_WIN     (Accept, VK_ACCEPT)
KEY_MAP_ANDROID (Accept, AKEYCODE_DPAD_CENTER)

// Again

// Enter
KEY_MAP_WIN     (Enter, VK_RETURN)
KEY_MAP_COCOA   (Enter, kVK_Return)
KEY_MAP_COCOA   (Enter, kVK_ANSI_KeypadEnter)
KEY_MAP_COCOA   (Enter, kVK_Powerbook_KeypadEnter)
KEY_MAP_GTK     (Enter, GDK_Return)
KEY_MAP_GTK     (Enter, GDK_KP_Enter)
KEY_MAP_GTK     (Enter, GDK_ISO_Enter)
KEY_MAP_GTK     (Enter, GDK_3270_Enter)
KEY_MAP_QT      (Enter, Qt::Key_Return)
KEY_MAP_QT      (Enter, Qt::Key_Enter)
KEY_MAP_ANDROID (Enter, AKEYCODE_ENTER)
KEY_MAP_ANDROID (Enter, AKEYCODE_NUMPAD_ENTER)

// Find
KEY_MAP_GTK     (Find, GDK_Find)

// Help
KEY_MAP_WIN     (Help, VK_HELP)
KEY_MAP_COCOA   (Help, kVK_Help)
KEY_MAP_GTK     (Help, GDK_Help)
KEY_MAP_QT      (Help, Qt::Key_Help)
KEY_MAP_ANDROID (Help, AKEYCODE_ASSIST)

// Info
KEY_MAP_ANDROID (Info, AKEYCODE_INFO)

// Menu
KEY_MAP_WIN     (Menu, VK_APPS)
KEY_MAP_COCOA   (Menu, kVK_PC_ContextMenu)
KEY_MAP_GTK     (Menu, GDK_Menu)
KEY_MAP_QT      (Menu, Qt::Key_Menu)
KEY_MAP_ANDROID (Menu, AKEYCODE_MENU)

// Pause
KEY_MAP_WIN     (Pause, VK_PAUSE)
KEY_MAP_GTK     (Pause, GDK_Pause)
// Break is typically mapped to Alt+Pause or Ctrl+Pause on GTK.
KEY_MAP_GTK     (Pause, GDK_Break)
KEY_MAP_QT      (Pause, Qt::Key_Pause)
KEY_MAP_ANDROID (Pause, AKEYCODE_BREAK)

// Play
KEY_MAP_WIN     (Play, VK_PLAY)
KEY_MAP_GTK     (Play, GDK_3270_Play) // legacy IBM keyboard layout
KEY_MAP_QT      (Play, Qt::Key_Play)

// ScrollLock
KEY_MAP_WIN     (ScrollLock, VK_SCROLL)
KEY_MAP_GTK     (ScrollLock, GDK_Scroll_Lock)
KEY_MAP_QT      (ScrollLock, Qt::Key_ScrollLock)
KEY_MAP_ANDROID (ScrollLock, AKEYCODE_SCROLL_LOCK)

// Execute
KEY_MAP_WIN     (Execute, VK_EXECUTE)
KEY_MAP_GTK     (Execute, GDK_Execute)
KEY_MAP_QT      (Execute, Qt::Key_Execute)

// Cancel
KEY_MAP_WIN     (Cancel, VK_CANCEL)
KEY_MAP_GTK     (Cancel, GDK_Cancel)
KEY_MAP_QT      (Cancel, Qt::Key_Cancel)

// Esc
KEY_MAP_WIN     (Esc, VK_ESCAPE)
KEY_MAP_COCOA   (Esc, kVK_Escape)
KEY_MAP_GTK     (Esc, GDK_Escape)
KEY_MAP_QT      (Esc, Qt::Key_Escape)
KEY_MAP_ANDROID (Esc, AKEYCODE_ESCAPE)

// Exit
KEY_MAP_ANDROID (Exit, AKEYCODE_HOME)

// Zoom
KEY_MAP_WIN     (Zoom, VK_ZOOM)
KEY_MAP_QT      (Zoom, Qt::Key_Zoom)

// Separator
KEY_MAP_WIN     (Separator, VK_SEPARATOR)
KEY_MAP_WIN     (Separator, VK_ABNT_C2) // This is on Brazilian keyboard.
KEY_MAP_COCOA   (Separator, kVK_JIS_KeypadComma)
KEY_MAP_GTK     (Separator, GDK_KP_Separator)
KEY_MAP_ANDROID (Separator, AKEYCODE_NUMPAD_COMMA)

// Spacebar
KEY_MAP_WIN     (Spacebar, VK_SPACE)
KEY_MAP_COCOA   (Spacebar, kVK_Space)
KEY_MAP_GTK     (Spacebar, GDK_space)
KEY_MAP_GTK     (Spacebar, GDK_KP_Space)
KEY_MAP_QT      (Spacebar, Qt::Key_Space)
KEY_MAP_ANDROID (Spacebar, AKEYCODE_SPACE)

// Add
KEY_MAP_WIN     (Add, VK_ADD)
KEY_MAP_COCOA   (Add, kVK_ANSI_KeypadPlus)
KEY_MAP_GTK     (Add, GDK_KP_Add)
KEY_MAP_ANDROID (Add, AKEYCODE_NUMPAD_ADD)

// Subtract
KEY_MAP_WIN     (Subtract, VK_SUBTRACT)
KEY_MAP_COCOA   (Subtract, kVK_ANSI_KeypadMinus)
KEY_MAP_GTK     (Subtract, GDK_KP_Subtract)
KEY_MAP_ANDROID (Subtract, AKEYCODE_NUMPAD_SUBTRACT)

// Multiply
KEY_MAP_WIN     (Multiply, VK_MULTIPLY)
KEY_MAP_COCOA   (Multiply, kVK_ANSI_KeypadMultiply)
KEY_MAP_GTK     (Multiply, GDK_KP_Multiply)
KEY_MAP_ANDROID (Multiply, AKEYCODE_NUMPAD_MULTIPLY)

// Divide
KEY_MAP_WIN     (Divide, VK_DIVIDE)
KEY_MAP_COCOA   (Divide, kVK_ANSI_KeypadDivide)
KEY_MAP_GTK     (Divide, GDK_KP_Divide)
KEY_MAP_ANDROID (Divide, AKEYCODE_NUMPAD_DIVIDE)

// Equals
KEY_MAP_COCOA   (Equals, kVK_ANSI_KeypadEquals)
KEY_MAP_GTK     (Equals, GDK_KP_Equal)
KEY_MAP_ANDROID (Equals, AKEYCODE_NUMPAD_EQUALS)

// Decimal
KEY_MAP_WIN     (Decimal, VK_DECIMAL)
KEY_MAP_COCOA   (Decimal, kVK_ANSI_KeypadDecimal)
KEY_MAP_GTK     (Decimal, GDK_KP_Decimal)
KEY_MAP_ANDROID (Decimal, AKEYCODE_NUMPAD_DOT)

// BrightnessDown
KEY_MAP_GTK     (BrightnessDown, GDK_MonBrightnessDown)
KEY_MAP_QT      (BrightnessDown, Qt::Key_MonBrightnessDown)

// BrightnessUp
KEY_MAP_GTK     (BrightnessUp, GDK_MonBrightnessUp)
KEY_MAP_QT      (BrightnessUp, Qt::Key_MonBrightnessUp)

// Camera
KEY_MAP_QT      (Camera, Qt::Key_Camera)
KEY_MAP_ANDROID (Camera, AKEYCODE_CAMERA)

// Eject
KEY_MAP_GTK     (Eject, GDK_Eject)
KEY_MAP_QT      (Eject, Qt::Key_Eject)
KEY_MAP_ANDROID (Eject, AKEYCODE_MEDIA_EJECT)

// Power
KEY_MAP_GTK     (Power, GDK_PowerOff)
KEY_MAP_QT      (Power, Qt::Key_PowerOff)
KEY_MAP_ANDROID (Power, AKEYCODE_POWER)

// PrintScreen
KEY_MAP_WIN     (PrintScreen, VK_SNAPSHOT)
KEY_MAP_GTK     (PrintScreen, GDK_3270_PrintScreen)
KEY_MAP_GTK     (PrintScreen, GDK_Print)
KEY_MAP_GTK     (PrintScreen, GDK_Sys_Req)
KEY_MAP_QT      (PrintScreen, Qt::Key_Print)
KEY_MAP_QT      (PrintScreen, Qt::Key_SysReq)
KEY_MAP_ANDROID (PrintScreen, AKEYCODE_SYSRQ)

// BrowserFavorites
KEY_MAP_WIN     (BrowserFavorites, VK_BROWSER_FAVORITES)
KEY_MAP_QT      (BrowserFavorites, Qt::Key_Favorites)
KEY_MAP_ANDROID (BrowserFavorites, AKEYCODE_BOOKMARK)

// BrowserHome
KEY_MAP_WIN     (BrowserHome, VK_BROWSER_HOME)
KEY_MAP_GTK     (BrowserHome, GDK_HomePage)
KEY_MAP_QT      (BrowserHome, Qt::Key_HomePage)

// BrowserRefresh
KEY_MAP_WIN     (BrowserRefresh, VK_BROWSER_REFRESH)
KEY_MAP_GTK     (BrowserRefresh, GDK_Refresh)
KEY_MAP_GTK     (BrowserRefresh, GDK_Reload)
KEY_MAP_QT      (BrowserRefresh, Qt::Key_Refresh)
KEY_MAP_QT      (BrowserRefresh, Qt::Key_Reload)

// BrowserSearch
KEY_MAP_WIN     (BrowserSearch, VK_BROWSER_SEARCH)
KEY_MAP_GTK     (BrowserSearch, GDK_Search)
KEY_MAP_QT      (BrowserSearch, Qt::Key_Search)
KEY_MAP_ANDROID (BrowserSearch, AKEYCODE_SEARCH)

// BrowserStop
KEY_MAP_WIN     (BrowserStop, VK_BROWSER_STOP)
KEY_MAP_GTK     (BrowserStop, GDK_Stop)
KEY_MAP_QT      (BrowserStop, Qt::Key_Stop)

// BrowserBack
KEY_MAP_WIN     (BrowserBack, VK_BROWSER_BACK)
KEY_MAP_GTK     (BrowserBack, GDK_Back)
KEY_MAP_QT      (BrowserBack, Qt::Key_Back)
KEY_MAP_ANDROID (BrowserBack, AKEYCODE_BACK)

// BrowserForward
KEY_MAP_WIN     (BrowserForward, VK_BROWSER_FORWARD)
KEY_MAP_GTK     (BrowserForward, GDK_Forward)
KEY_MAP_QT      (BrowserForward, Qt::Key_Forward)
KEY_MAP_ANDROID (BrowserForward, AKEYCODE_FORWARD)

// Left
KEY_MAP_WIN     (Left, VK_LEFT)
KEY_MAP_COCOA   (Left, kVK_LeftArrow)
KEY_MAP_GTK     (Left, GDK_Left)
KEY_MAP_GTK     (Left, GDK_KP_Left)
KEY_MAP_QT      (Left, Qt::Key_Left)
KEY_MAP_ANDROID (Left, AKEYCODE_DPAD_LEFT)

// PageDown
KEY_MAP_WIN     (PageDown, VK_NEXT)
KEY_MAP_COCOA   (PageDown, kVK_PageDown)
KEY_MAP_GTK     (PageDown, GDK_Page_Down /* same as GDK_Next */)
KEY_MAP_GTK     (PageDown, GDK_KP_Page_Down /* same as GDK_KP_Next */)
KEY_MAP_QT      (PageDown, Qt::Key_PageDown)
KEY_MAP_ANDROID (PageDown, AKEYCODE_PAGE_DOWN)

// PageUp
KEY_MAP_WIN     (PageUp, VK_PRIOR)
KEY_MAP_COCOA   (PageUp, kVK_PageUp)
KEY_MAP_GTK     (PageUp, GDK_Page_Up /* same as GDK_Prior */)
KEY_MAP_GTK     (PageUp, GDK_KP_Page_Up /* same as GDK_KP_Prior */)
KEY_MAP_QT      (PageUp, Qt::Key_PageUp)
KEY_MAP_ANDROID (PageUp, AKEYCODE_PAGE_UP)

// Right
KEY_MAP_WIN     (Right, VK_RIGHT)
KEY_MAP_COCOA   (Right, kVK_RightArrow)
KEY_MAP_GTK     (Right, GDK_Right)
KEY_MAP_GTK     (Right, GDK_KP_Right)
KEY_MAP_QT      (Right, Qt::Key_Right)
KEY_MAP_ANDROID (Right, AKEYCODE_DPAD_RIGHT)

// Up
KEY_MAP_WIN     (Up, VK_UP)
KEY_MAP_COCOA   (Up, kVK_UpArrow)
KEY_MAP_GTK     (Up, GDK_Up)
KEY_MAP_GTK     (Up, GDK_KP_Up)
KEY_MAP_QT      (Up, Qt::Key_Up)
KEY_MAP_ANDROID (Up, AKEYCODE_DPAD_UP)

// UpLeft

// UpRight

// Down
KEY_MAP_WIN     (Down, VK_DOWN)
KEY_MAP_COCOA   (Down, kVK_DownArrow)
KEY_MAP_GTK     (Down, GDK_Down)
KEY_MAP_GTK     (Down, GDK_KP_Down)
KEY_MAP_QT      (Down, Qt::Key_Down)
KEY_MAP_ANDROID (Down, AKEYCODE_DPAD_DOWN)

// DownLeft

// DownRight

// Home
KEY_MAP_WIN     (Home, VK_HOME)
KEY_MAP_COCOA   (Home, kVK_Home)
KEY_MAP_GTK     (Home, GDK_Home)
KEY_MAP_GTK     (Home, GDK_KP_Home)
KEY_MAP_QT      (Home, Qt::Key_Home)
KEY_MAP_ANDROID (Home, AKEYCODE_MOVE_HOME)

// End
KEY_MAP_WIN     (End, VK_END)
KEY_MAP_COCOA   (End, kVK_End)
KEY_MAP_GTK     (End, GDK_End)
KEY_MAP_GTK     (End, GDK_KP_End)
KEY_MAP_QT      (End, Qt::Key_End)
KEY_MAP_ANDROID (End, AKEYCODE_MOVE_END)

// Select
KEY_MAP_WIN     (Select, VK_SELECT)
KEY_MAP_GTK     (Select, GDK_Select)

// Tab
KEY_MAP_WIN     (Tab, VK_TAB)
KEY_MAP_COCOA   (Tab, kVK_Tab)
KEY_MAP_GTK     (Tab, GDK_Tab)
KEY_MAP_GTK     (Tab, GDK_KP_Tab)
KEY_MAP_QT      (Tab, Qt::Key_Tab)
KEY_MAP_ANDROID (Tab, AKEYCODE_TAB)

// Backspace
KEY_MAP_WIN     (Backspace, VK_BACK)
KEY_MAP_COCOA   (Backspace, kVK_PC_Backspace)
KEY_MAP_GTK     (Backspace, GDK_BackSpace)
KEY_MAP_QT      (Backspace, Qt::Key_Backspace)
KEY_MAP_ANDROID (Backspace, AKEYCODE_DEL)

// Clear
KEY_MAP_WIN     (Clear, VK_CLEAR)
KEY_MAP_WIN     (Clear, VK_OEM_CLEAR)
KEY_MAP_COCOA   (Clear, kVK_ANSI_KeypadClear)
KEY_MAP_GTK     (Clear, GDK_Clear)
KEY_MAP_QT      (Clear, Qt::Key_Clear)
KEY_MAP_ANDROID (Clear, AKEYCODE_CLEAR)

// Copy
KEY_MAP_GTK     (Copy, GDK_Copy)
KEY_MAP_QT      (Copy, Qt::Key_Copy)

// Cut
KEY_MAP_GTK     (Cut, GDK_Cut)
KEY_MAP_QT      (Cut, Qt::Key_Cut)

// Del
KEY_MAP_WIN     (Del, VK_DELETE)
KEY_MAP_COCOA   (Del, kVK_PC_Delete)
KEY_MAP_GTK     (Del, GDK_Delete)
KEY_MAP_GTK     (Del, GDK_KP_Delete)
KEY_MAP_QT      (Del, Qt::Key_Delete)
KEY_MAP_ANDROID (Del, AKEYCODE_FORWARD_DEL)

// EraseEof
KEY_MAP_WIN     (EraseEof, VK_EREOF)
KEY_MAP_GTK     (EraseEof, GDK_3270_EraseEOF) // legacy IBM keyboard layout

// Insert
KEY_MAP_WIN     (Insert, VK_INSERT)
KEY_MAP_GTK     (Insert, GDK_Insert)
KEY_MAP_GTK     (Insert, GDK_KP_Insert)
KEY_MAP_QT      (Insert, Qt::Key_Insert)
KEY_MAP_ANDROID (Insert, AKEYCODE_INSERT)

// Paste
KEY_MAP_GTK     (Paste, GDK_Paste)
KEY_MAP_QT      (Paste, Qt::Key_Paste)

// Undo
KEY_MAP_GTK     (Undo, GDK_Undo)

// DeadGrave
KEY_MAP_GTK     (DeadGrave, GDK_dead_grave)
KEY_MAP_QT      (DeadGrave, Qt::Key_Dead_Grave)

// DeadAcute
KEY_MAP_GTK     (DeadAcute, GDK_dead_acute)
KEY_MAP_QT      (DeadAcute, Qt::Key_Dead_Acute)

// DeadCircumflex
KEY_MAP_GTK     (DeadCircumflex, GDK_dead_circumflex)
KEY_MAP_QT      (DeadCircumflex, Qt::Key_Dead_Circumflex)

// DeadTilde
KEY_MAP_GTK     (DeadTilde, GDK_dead_tilde)
KEY_MAP_QT      (DeadTilde, Qt::Key_Dead_Tilde)

// DeadMacron
KEY_MAP_GTK     (DeadMacron, GDK_dead_macron)
KEY_MAP_QT      (DeadMacron, Qt::Key_Dead_Macron)

// DeadBreve
KEY_MAP_GTK     (DeadBreve, GDK_dead_breve)
KEY_MAP_QT      (DeadBreve, Qt::Key_Dead_Breve)

// DeadAboveDot
KEY_MAP_GTK     (DeadAboveDot, GDK_dead_abovedot)
KEY_MAP_QT      (DeadAboveDot, Qt::Key_Dead_Abovedot)

// DeadUmlaut
KEY_MAP_GTK     (DeadUmlaut, GDK_dead_diaeresis)
KEY_MAP_QT      (DeadUmlaut, Qt::Key_Dead_Diaeresis)

// DeadAboveRing
KEY_MAP_GTK     (DeadAboveRing, GDK_dead_abovering)
KEY_MAP_QT      (DeadAboveRing, Qt::Key_Dead_Abovering)

// DeadDoubleacute
KEY_MAP_GTK     (DeadDoubleacute, GDK_dead_doubleacute)
KEY_MAP_QT      (DeadDoubleacute, Qt::Key_Dead_Doubleacute)

// DeadCaron
KEY_MAP_GTK     (DeadCaron, GDK_dead_caron)
KEY_MAP_QT      (DeadCaron, Qt::Key_Dead_Caron)

// DeadCedilla
KEY_MAP_GTK     (DeadCedilla, GDK_dead_cedilla)
KEY_MAP_QT      (DeadCedilla, Qt::Key_Dead_Cedilla)

// DeadOgonek
KEY_MAP_GTK     (DeadOgonek, GDK_dead_ogonek)
KEY_MAP_QT      (DeadOgonek, Qt::Key_Dead_Ogonek)

// DeadIota
KEY_MAP_GTK     (DeadIota, GDK_dead_iota)
KEY_MAP_QT      (DeadIota, Qt::Key_Dead_Iota)

// DeadVoicedSound
KEY_MAP_GTK     (DeadVoicedSound, GDK_dead_voiced_sound)
KEY_MAP_QT      (DeadVoicedSound, Qt::Key_Dead_Voiced_Sound)

// DeadSemivoicedSound
KEY_MAP_GTK     (DeadSemivoicedSound, GDK_dead_semivoiced_sound)
KEY_MAP_QT      (DeadSemivoicedSound, Qt::Key_Dead_Semivoiced_Sound)

// Alphanumeric
KEY_MAP_WIN_JPN (Alphanumeric, VK_OEM_ATTN)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_Shift)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_toggle)
KEY_MAP_QT      (Alphanumeric, Qt::Key_Eisu_Shift)
KEY_MAP_QT      (Alphanumeric, Qt::Key_Eisu_toggle)

// Alt
KEY_MAP_WIN     (Alt, VK_MENU)
KEY_MAP_WIN     (Alt, VK_LMENU)
KEY_MAP_WIN     (Alt, VK_RMENU)
KEY_MAP_COCOA   (Alt, kVK_Option)
KEY_MAP_COCOA   (Alt, kVK_RightOption)
KEY_MAP_GTK     (Alt, GDK_Alt_L)
KEY_MAP_GTK     (Alt, GDK_Alt_R)
KEY_MAP_QT      (Alt, Qt::Key_Alt)
KEY_MAP_ANDROID (Alt, AKEYCODE_ALT_LEFT)
KEY_MAP_ANDROID (Alt, AKEYCODE_ALT_RIGHT)

// AltGraph
KEY_MAP_GTK     (AltGraph, GDK_Mode_switch /* same as GDK_kana_switch,
                                              GDK_ISO_Group_Shift and
                                              GDK_script_switch */)
// Let's treat both Level 3 shift and Level 5 shift as AltGr.
// And also, let's treat Latch key and Lock key as AltGr key too like
// GDK_Shift_Lock.
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level3_Shift)
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level3_Latch)
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level3_Lock)
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level5_Shift)
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level5_Latch)
KEY_MAP_GTK     (AltGraph, GDK_ISO_Level5_Lock)
KEY_MAP_QT      (AltGraph, Qt::Key_AltGr)
KEY_MAP_QT      (AltGraph, Qt::Key_Mode_switch)

// CapsLock
KEY_MAP_WIN     (CapsLock, VK_CAPITAL)
KEY_MAP_COCOA   (CapsLock, kVK_CapsLock)
KEY_MAP_GTK     (CapsLock, GDK_Caps_Lock)
KEY_MAP_QT      (CapsLock, Qt::Key_CapsLock)
KEY_MAP_ANDROID (CapsLock, AKEYCODE_CAPS_LOCK)

// Control
KEY_MAP_WIN     (Control, VK_CONTROL)
KEY_MAP_WIN     (Control, VK_LCONTROL)
KEY_MAP_WIN     (Control, VK_RCONTROL)
KEY_MAP_COCOA   (Control, kVK_Control)
KEY_MAP_COCOA   (Control, kVK_RightControl)
KEY_MAP_GTK     (Control, GDK_Control_L)
KEY_MAP_GTK     (Control, GDK_Control_R)
KEY_MAP_QT      (Control, Qt::Key_Control)
KEY_MAP_ANDROID (Control, AKEYCODE_CTRL_LEFT)
KEY_MAP_ANDROID (Control, AKEYCODE_CTRL_RIGHT)

// Fn
KEY_MAP_COCOA   (Fn, kVK_Function)
KEY_MAP_ANDROID (Fn, AKEYCODE_FUNCTION)

// FnLock

// Meta
KEY_MAP_COCOA   (Meta, kVK_Command)
KEY_MAP_COCOA   (Meta, kVK_RightCommand)
KEY_MAP_GTK     (Meta, GDK_Meta_L)
KEY_MAP_GTK     (Meta, GDK_Meta_R)
KEY_MAP_QT      (Meta, Qt::Key_Meta)
KEY_MAP_ANDROID (Meta, AKEYCODE_META_LEFT)
KEY_MAP_ANDROID (Meta, AKEYCODE_META_RIGHT)

// Process

// NumLock
KEY_MAP_WIN     (NumLock, VK_NUMLOCK)
KEY_MAP_GTK     (NumLock, GDK_Num_Lock)
KEY_MAP_QT      (NumLock, Qt::Key_NumLock)
KEY_MAP_ANDROID (NumLock, AKEYCODE_NUM_LOCK)

// Shift
KEY_MAP_WIN     (Shift, VK_SHIFT)
KEY_MAP_WIN     (Shift, VK_LSHIFT)
KEY_MAP_WIN     (Shift, VK_RSHIFT)
KEY_MAP_COCOA   (Shift, kVK_Shift)
KEY_MAP_COCOA   (Shift, kVK_RightShift)
KEY_MAP_GTK     (Shift, GDK_Shift_L)
KEY_MAP_GTK     (Shift, GDK_Shift_R)
KEY_MAP_GTK     (Shift, GDK_Shift_Lock) // Let's treat as Shift key (bug 769159)
KEY_MAP_QT      (Shift, Qt::Key_Shift)
KEY_MAP_ANDROID (Shift, AKEYCODE_SHIFT_LEFT)
KEY_MAP_ANDROID (Shift, AKEYCODE_SHIFT_RIGHT)

// SymbolLock

// OS
KEY_MAP_WIN     (OS, VK_LWIN)
KEY_MAP_WIN     (OS, VK_RWIN)
KEY_MAP_GTK     (OS, GDK_Super_L)
KEY_MAP_GTK     (OS, GDK_Super_R)
KEY_MAP_GTK     (OS, GDK_Hyper_L)
KEY_MAP_GTK     (OS, GDK_Hyper_R)
KEY_MAP_QT      (OS, Qt::Key_Super_L)
KEY_MAP_QT      (OS, Qt::Key_Super_R)
KEY_MAP_QT      (OS, Qt::Key_Hyper_L)
KEY_MAP_QT      (OS, Qt::Key_Hyper_R)

// Compose
KEY_MAP_GTK     (Compose, GDK_Multi_key) // "Multi Key" is "Compose key" on X
KEY_MAP_QT      (Compose, Qt::Key_Multi_key)

// AllCandidates
KEY_MAP_GTK     (AllCandidates, GDK_MultipleCandidate) // OADG 109, Zen Koho
KEY_MAP_QT      (AllCandidates, Qt::Key_MultipleCandidate)

// NextCandidate

// PreviousCandidate
KEY_MAP_GTK     (PreviousCandidate, GDK_PreviousCandidate) // OADG 109, Mae Koho
KEY_MAP_QT      (PreviousCandidate, Qt::Key_PreviousCandidate)

// CodeInput
KEY_MAP_GTK     (CodeInput, GDK_Codeinput) // OADG 109, Kanji Bangou
KEY_MAP_QT      (CodeInput, Qt::Key_Codeinput)

// Convert
KEY_MAP_WIN     (Convert, VK_CONVERT)
KEY_MAP_GTK     (Convert, GDK_Henkan)
KEY_MAP_QT      (Convert, Qt::Key_Henkan)
KEY_MAP_ANDROID (Convert, AKEYCODE_HENKAN)

// Nonconvert
KEY_MAP_WIN     (Nonconvert, VK_NONCONVERT)
KEY_MAP_GTK     (Nonconvert, GDK_Muhenkan)
KEY_MAP_QT      (Nonconvert, Qt::Key_Muhenkan)
KEY_MAP_ANDROID (Nonconvert, AKEYCODE_MUHENKAN)

// FinalMode
KEY_MAP_WIN     (FinalMode, VK_FINAL)

// FullWidth
KEY_MAP_WIN_JPN (FullWidth, VK_OEM_ENLW)
KEY_MAP_GTK     (FullWidth, GDK_Zenkaku)
KEY_MAP_QT      (FullWidth, Qt::Key_Zenkaku)

// HalfWidth
KEY_MAP_WIN_JPN (HalfWidth, VK_OEM_AUTO)
KEY_MAP_GTK     (HalfWidth, GDK_Hankaku)
KEY_MAP_QT      (HalfWidth, Qt::Key_Hankaku)

// ModeChange
KEY_MAP_WIN     (ModeChange, VK_MODECHANGE)
KEY_MAP_ANDROID (ModeChange, AKEYCODE_SWITCH_CHARSET)

// RomanCharacters
KEY_MAP_WIN_JPN (RomanCharacters, VK_OEM_BACKTAB)
KEY_MAP_COCOA   (RomanCharacters, kVK_JIS_Eisu)
KEY_MAP_GTK     (RomanCharacters, GDK_Romaji)
KEY_MAP_QT      (RomanCharacters, Qt::Key_Romaji)
// Assuming that EISU key of Android is the Eisu key on Mac keyboard.
KEY_MAP_ANDROID (RomanCharacters, AKEYCODE_EISU)

// HangulMode
KEY_MAP_WIN_KOR (HangulMode, VK_HANGUL /* same as VK_KANA */)

// HanjaMode
KEY_MAP_WIN_KOR (HanjaMode, VK_HANJA /* same as VK_KANJI */)

// JunjaMode
KEY_MAP_WIN     (JunjaMode, VK_JUNJA)

// Hiragana
KEY_MAP_WIN_JPN (Hiragana, VK_OEM_COPY)
KEY_MAP_GTK     (Hiragana, GDK_Hiragana)
KEY_MAP_QT      (Hiragana, Qt::Key_Hiragana)

// KanaMode
// VK_KANA is never used with modern Japanese keyboard, however, IE maps it to
// KanaMode, therefore, we should use same map for it.
KEY_MAP_WIN_JPN (KanaMode, VK_KANA /* same as VK_HANGUL */)
KEY_MAP_WIN_JPN (KanaMode, VK_ATTN)
KEY_MAP_GTK     (KanaMode, GDK_Kana_Lock)
KEY_MAP_GTK     (KanaMode, GDK_Kana_Shift)
KEY_MAP_QT      (KanaMode, Qt::Key_Kana_Lock)
KEY_MAP_QT      (KanaMode, Qt::Key_Kana_Shift)

// KanjiMode
KEY_MAP_WIN_JPN (KanjiMode, VK_KANJI /* same as VK_HANJA */)
KEY_MAP_COCOA   (KanjiMode, kVK_JIS_Kana) // Kana key opens IME
KEY_MAP_GTK     (KanjiMode, GDK_Kanji) // Typically, Alt + Hankaku/Zenkaku key
KEY_MAP_QT      (KanjiMode, Qt::Key_Kanji)
// Assuming that KANA key of Android is the Kana key on Mac keyboard.
KEY_MAP_ANDROID (KanjiMode, AKEYCODE_KANA)

// Katakana
KEY_MAP_WIN_JPN (Katakana, VK_OEM_FINISH)
KEY_MAP_GTK     (Katakana, GDK_Katakana)
KEY_MAP_QT      (Katakana, Qt::Key_Katakana)

// AudioFaderFront

// AudioFaderRear

// AudioBalanceLeft

// AudioBalanceRight

// AudioBassBoostDown
KEY_MAP_QT      (AudioBassBoostDown, Qt::Key_BassDown)

// AudioBassBoostUp
KEY_MAP_QT      (AudioBassBoostUp, Qt::Key_BassUp)

// VolumeMute
KEY_MAP_WIN     (VolumeMute, VK_VOLUME_MUTE)
KEY_MAP_COCOA   (VolumeMute, kVK_Mute)
KEY_MAP_GTK     (VolumeMute, GDK_AudioMute)
KEY_MAP_QT      (VolumeMute, Qt::Key_VolumeMute)
KEY_MAP_ANDROID (VolumeMute, AKEYCODE_VOLUME_MUTE)

// VolumeDown
KEY_MAP_WIN     (VolumeDown, VK_VOLUME_DOWN)
KEY_MAP_COCOA   (VolumeDown, kVK_VolumeDown)
KEY_MAP_GTK     (VolumeDown, GDK_AudioLowerVolume)
KEY_MAP_QT      (VolumeDown, Qt::Key_VolumeDown)
KEY_MAP_ANDROID (VolumeDown, AKEYCODE_VOLUME_DOWN)

// VolumeUp
KEY_MAP_WIN     (VolumeUp, VK_VOLUME_UP)
KEY_MAP_COCOA   (VolumeUp, kVK_VolumeUp)
KEY_MAP_GTK     (VolumeUp, GDK_AudioRaiseVolume)
KEY_MAP_QT      (VolumeUp, Qt::Key_VolumeUp)
KEY_MAP_ANDROID (VolumeUp, AKEYCODE_VOLUME_UP)

// MediaPause
KEY_MAP_GTK     (MediaPause, GDK_AudioPause)
KEY_MAP_QT      (MediaPause, Qt::Key_MediaPause)
KEY_MAP_ANDROID (MediaPause, AKEYCODE_MEDIA_PAUSE)

// MediaPlay
KEY_MAP_GTK     (MediaPlay, GDK_AudioPlay)
KEY_MAP_QT      (MediaPlay, Qt::Key_MediaPlay)
KEY_MAP_ANDROID (MediaPlay, AKEYCODE_MEDIA_PLAY)

// MediaStop
KEY_MAP_WIN     (MediaStop, VK_MEDIA_STOP)
KEY_MAP_GTK     (MediaStop, GDK_AudioStop)
KEY_MAP_QT      (MediaStop, Qt::Key_MediaStop)
KEY_MAP_ANDROID (MediaStop, AKEYCODE_MEDIA_STOP)

// MediaNextTrack
KEY_MAP_WIN     (MediaNextTrack, VK_MEDIA_NEXT_TRACK)
KEY_MAP_GTK     (MediaNextTrack, GDK_AudioNext)
KEY_MAP_QT      (MediaNextTrack, Qt::Key_MediaNext)
KEY_MAP_ANDROID (MediaNextTrack, AKEYCODE_MEDIA_NEXT)

// MediaPreviousTrack
KEY_MAP_WIN     (MediaPreviousTrack, VK_MEDIA_PREV_TRACK)
KEY_MAP_GTK     (MediaPreviousTrack, GDK_AudioPrev)
KEY_MAP_QT      (MediaPreviousTrack, Qt::Key_MediaPrevious)
KEY_MAP_ANDROID (MediaPreviousTrack, AKEYCODE_MEDIA_PREVIOUS)

// MediaPlayPause
KEY_MAP_WIN     (MediaPlayPause, VK_MEDIA_PLAY_PAUSE)
KEY_MAP_QT      (MediaPlayPause, Qt::Key_MediaTogglePlayPause)
KEY_MAP_ANDROID (MediaPlayPause, AKEYCODE_MEDIA_PLAY_PAUSE)

// MediaTrackSkip

// MediaTrackStart

// MediaTrackEnd

// SelectMedia
KEY_MAP_WIN     (SelectMedia, VK_LAUNCH_MEDIA_SELECT)

// Blue
KEY_MAP_GTK     (Blue, GDK_Blue)
KEY_MAP_ANDROID (Blue, AKEYCODE_PROG_BLUE)

// Brown

// ChannelDown
KEY_MAP_ANDROID (ChannelDown, AKEYCODE_CHANNEL_DOWN)

// ChannelUp
KEY_MAP_ANDROID (ChannelUp, AKEYCODE_CHANNEL_UP)

// ClearFavorite0

// ClearFavorite1

// ClearFavorite2

// ClearFavorite3

// Dimmer
KEY_MAP_GTK     (Dimmer, GDK_BrightnessAdjust)
KEY_MAP_QT      (Dimmer, Qt::Key_BrightnessAdjust)

// DisplaySwap

// FastFwd
KEY_MAP_QT      (FastFwd, Qt::Key_AudioForward)
KEY_MAP_ANDROID (FastFwd, AKEYCODE_MEDIA_FAST_FORWARD)

// Green
KEY_MAP_GTK     (Green, GDK_Green)
KEY_MAP_ANDROID (Green, AKEYCODE_PROG_GREEN)

// Grey

// Guide
KEY_MAP_ANDROID (Guide, AKEYCODE_GUIDE)

// InstantReplay

// MediaLast
KEY_MAP_QT      (MediaLast, Qt::Key_MediaLast)

// Link

// Live
KEY_MAP_ANDROID (Live, AKEYCODE_TV)

// Lock

// NextDay

// NextFavoriteChannel

// OnDemand

// PinPDown

// PinPMove

// PinPToggle
KEY_MAP_ANDROID (PinPToggle, AKEYCODE_WINDOW)

// PinPUp

// PlaySpeedDown

// PlaySpeedReset

// PlaySpeedUp

// PrevDay

// RandomToggle
KEY_MAP_GTK     (RandomToggle, GDK_AudioRandomPlay)
KEY_MAP_QT      (RandomToggle, Qt::Key_AudioRandomPlay)

// RecallFavorite0

// RecallFavorite1

// RecallFavorite2

// RecallFavorite3

// MediaRecord
KEY_MAP_GTK     (MediaRecord, GDK_AudioRecord)
KEY_MAP_QT      (MediaRecord, Qt::Key_MediaRecord)
KEY_MAP_ANDROID (MediaRecord, AKEYCODE_MEDIA_RECORD)

// RecordSpeedNext

// Red
KEY_MAP_GTK     (Red, GDK_Red)
KEY_MAP_ANDROID (Red, AKEYCODE_PROG_RED)

// MediaRewind
KEY_MAP_GTK     (MediaRewind, GDK_AudioRewind)
KEY_MAP_QT      (MediaRewind, Qt::Key_AudioRewind)
KEY_MAP_ANDROID (MediaRewind, AKEYCODE_MEDIA_REWIND)

// RfBypass

// ScanChannelsToggle

// ScreenModeNext

// Settings
KEY_MAP_ANDROID (Settings, AKEYCODE_SETTINGS)

// SplitScreenToggle

// StoreFavorite0

// StoreFavorite1

// StoreFavorite2

// StoreFavorite3

// Subtitle
KEY_MAP_GTK     (Subtitle, GDK_Subtitle)
KEY_MAP_QT      (Subtitle, Qt::Key_Subtitle)
KEY_MAP_ANDROID (Subtitle, AKEYCODE_CAPTIONS)

// AudioSurroundModeNext

// Teletext

// VideoModeNext

// DisplayWide

// Wink

// Yellow
KEY_MAP_GTK     (Yellow, GDK_Yellow)
KEY_MAP_ANDROID (Yellow, AKEYCODE_PROG_YELLOW)

#undef KEY_MAP_WIN
#undef KEY_MAP_WIN_JPN
#undef KEY_MAP_WIN_KOR
#undef KEY_MAP_WIN_OTH
#undef KEY_MAP_OS2
#undef KEY_MAP_COCOA
#undef KEY_MAP_GTK
#undef KEY_MAP_QT
#undef KEY_MAP_ANDROID
