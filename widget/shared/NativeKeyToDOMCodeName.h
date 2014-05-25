/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file defines simple code mapping between native scancode or
 * something and DOM code name index.
 * You must define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX macro before include
 * this.
 *
 * It must have two arguments, (aNativeKey, aCodeNameIndex).
 * aNativeKey is a scancode value or something (depends on the platform).
 * aCodeNameIndex is the widget::CodeNameIndex value.
 */

// Windows
#define CODE_MAP_WIN(aCPPCodeName, aNativeKey)
// Mac OS X
#define CODE_MAP_MAC(aCPPCodeName, aNativeKey)
// GTK and Qt on Linux
#define CODE_MAP_X11(aCPPCodeName, aNativeKey)
// Android and Gonk
#define CODE_MAP_ANDROID(aCPPCodeName, aNativeKey)

#if defined(XP_WIN)
#undef CODE_MAP_WIN
// aNativeKey is scan code
#define CODE_MAP_WIN(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#elif defined(XP_MACOSX)
#undef CODE_MAP_MAC
// aNativeKey is key code starting with kVK_.
#define CODE_MAP_MAC(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#elif defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)
#undef CODE_MAP_X11
#define CODE_MAP_X11(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#elif defined(ANDROID)
#undef CODE_MAP_ANDROID
#define CODE_MAP_ANDROID(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#endif

// Writing system keys
CODE_MAP_WIN(Backquote,                 0x0029)
CODE_MAP_MAC(Backquote,                 kVK_ANSI_Grave)

CODE_MAP_WIN(Backslash,                 0x002B)
CODE_MAP_MAC(Backslash,                 kVK_ANSI_Backslash)

CODE_MAP_WIN(Backspace,                 0x000E)
CODE_MAP_MAC(Backspace,                 kVK_Delete)

CODE_MAP_WIN(BracketLeft,               0x001A)
CODE_MAP_MAC(BracketLeft,               kVK_ANSI_LeftBracket)

CODE_MAP_WIN(BracketRight,              0x001B)
CODE_MAP_MAC(BracketRight,              kVK_ANSI_RightBracket)

CODE_MAP_WIN(Comma,                     0x0033)
CODE_MAP_MAC(Comma,                     kVK_ANSI_Comma)

CODE_MAP_WIN(Digit0,                    0x0002)
CODE_MAP_MAC(Digit0,                    kVK_ANSI_0)

CODE_MAP_WIN(Digit1,                    0x0003)
CODE_MAP_MAC(Digit1,                    kVK_ANSI_1)

CODE_MAP_WIN(Digit2,                    0x0004)
CODE_MAP_MAC(Digit2,                    kVK_ANSI_2)

CODE_MAP_WIN(Digit3,                    0x0005)
CODE_MAP_MAC(Digit3,                    kVK_ANSI_3)

CODE_MAP_WIN(Digit4,                    0x0006)
CODE_MAP_MAC(Digit4,                    kVK_ANSI_4)

CODE_MAP_WIN(Digit5,                    0x0007)
CODE_MAP_MAC(Digit5,                    kVK_ANSI_5)

CODE_MAP_WIN(Digit6,                    0x0008)
CODE_MAP_MAC(Digit6,                    kVK_ANSI_6)

CODE_MAP_WIN(Digit7,                    0x0009)
CODE_MAP_MAC(Digit7,                    kVK_ANSI_7)

CODE_MAP_WIN(Digit8,                    0x000A)
CODE_MAP_MAC(Digit8,                    kVK_ANSI_8)

CODE_MAP_WIN(Digit9,                    0x000B)
CODE_MAP_MAC(Digit9,                    kVK_ANSI_9)

CODE_MAP_WIN(Equal,                     0x000D)
CODE_MAP_MAC(Equal,                     kVK_ANSI_Equal)

CODE_MAP_WIN(IntlBackslash,             0x0056)
CODE_MAP_MAC(IntlBackslash,             kVK_ISO_Section)

// Win: IntlHash's scan code is shared with "Backslash" key.
// Mac: IntlHash's virtual key code is shared with "Backslash" key.

CODE_MAP_WIN(IntlRo,                    0x0073)
CODE_MAP_MAC(IntlRo,                    kVK_JIS_Underscore)

CODE_MAP_WIN(IntlYen,                   0x007D)
CODE_MAP_MAC(IntlYen,                   kVK_JIS_Yen)

CODE_MAP_WIN(KeyA,                      0x001E)
CODE_MAP_MAC(KeyA,                      kVK_ANSI_A)

CODE_MAP_WIN(KeyB,                      0x0030)
CODE_MAP_MAC(KeyB,                      kVK_ANSI_B)

CODE_MAP_WIN(KeyC,                      0x002E)
CODE_MAP_MAC(KeyC,                      kVK_ANSI_C)

CODE_MAP_WIN(KeyD,                      0x0020)
CODE_MAP_MAC(KeyD,                      kVK_ANSI_D)

CODE_MAP_WIN(KeyE,                      0x0012)
CODE_MAP_MAC(KeyE,                      kVK_ANSI_E)

CODE_MAP_WIN(KeyF,                      0x0021)
CODE_MAP_MAC(KeyF,                      kVK_ANSI_F)

CODE_MAP_WIN(KeyG,                      0x0022)
CODE_MAP_MAC(KeyG,                      kVK_ANSI_G)

CODE_MAP_WIN(KeyH,                      0x0023)
CODE_MAP_MAC(KeyH,                      kVK_ANSI_H)

CODE_MAP_WIN(KeyI,                      0x0017)
CODE_MAP_MAC(KeyI,                      kVK_ANSI_I)

CODE_MAP_WIN(KeyJ,                      0x0024)
CODE_MAP_MAC(KeyJ,                      kVK_ANSI_J)

CODE_MAP_WIN(KeyK,                      0x0025)
CODE_MAP_MAC(KeyK,                      kVK_ANSI_K)

CODE_MAP_WIN(KeyL,                      0x0026)
CODE_MAP_MAC(KeyL,                      kVK_ANSI_L)

CODE_MAP_WIN(KeyM,                      0x0032)
CODE_MAP_MAC(KeyM,                      kVK_ANSI_M)

CODE_MAP_WIN(KeyN,                      0x0031)
CODE_MAP_MAC(KeyN,                      kVK_ANSI_N)

CODE_MAP_WIN(KeyO,                      0x0018)
CODE_MAP_MAC(KeyO,                      kVK_ANSI_O)

CODE_MAP_WIN(KeyP,                      0x0019)
CODE_MAP_MAC(KeyP,                      kVK_ANSI_P)

CODE_MAP_WIN(KeyQ,                      0x0010)
CODE_MAP_MAC(KeyQ,                      kVK_ANSI_Q)

CODE_MAP_WIN(KeyR,                      0x0013)
CODE_MAP_MAC(KeyR,                      kVK_ANSI_R)

CODE_MAP_WIN(KeyS,                      0x001F)
CODE_MAP_MAC(KeyS,                      kVK_ANSI_S)

CODE_MAP_WIN(KeyT,                      0x0014)
CODE_MAP_MAC(KeyT,                      kVK_ANSI_T)

CODE_MAP_WIN(KeyU,                      0x0016)
CODE_MAP_MAC(KeyU,                      kVK_ANSI_U)

CODE_MAP_WIN(KeyV,                      0x002F)
CODE_MAP_MAC(KeyV,                      kVK_ANSI_V)

CODE_MAP_WIN(KeyW,                      0x0011)
CODE_MAP_MAC(KeyW,                      kVK_ANSI_W)

CODE_MAP_WIN(KeyX,                      0x002D)
CODE_MAP_MAC(KeyX,                      kVK_ANSI_X)

CODE_MAP_WIN(KeyY,                      0x0015)
CODE_MAP_MAC(KeyY,                      kVK_ANSI_Y)

CODE_MAP_WIN(KeyZ,                      0x002C)
CODE_MAP_MAC(KeyZ,                      kVK_ANSI_Z)

CODE_MAP_WIN(Minus,                     0x000C)
CODE_MAP_MAC(Minus,                     kVK_ANSI_Minus)

CODE_MAP_WIN(Period,                    0x0034)
CODE_MAP_MAC(Period,                    kVK_ANSI_Period)

CODE_MAP_WIN(Quote,                     0x0028)
CODE_MAP_MAC(Quote,                     kVK_ANSI_Quote)

CODE_MAP_WIN(Semicolon,                 0x0027)
CODE_MAP_MAC(Semicolon,                 kVK_ANSI_Semicolon)

CODE_MAP_WIN(Slash,                     0x0035)
CODE_MAP_MAC(Slash,                     kVK_ANSI_Slash)

// Functional keys
CODE_MAP_WIN(AltLeft,                   0x0038)
CODE_MAP_MAC(AltLeft,                   kVK_Option)

CODE_MAP_WIN(AltRight,                  0xE038)
CODE_MAP_MAC(AltRight,                  kVK_RightOption)

CODE_MAP_WIN(CapsLock,                  0x003A)
CODE_MAP_MAC(CapsLock,                  kVK_CapsLock)

CODE_MAP_WIN(ContextMenu,               0xE05D)
CODE_MAP_MAC(ContextMenu,               kVK_PC_ContextMenu)

CODE_MAP_WIN(ControlLeft,               0x001D)
CODE_MAP_MAC(ControlLeft,               kVK_Control)

CODE_MAP_WIN(ControlRight,              0xE01D)
CODE_MAP_MAC(ControlRight,              kVK_RightControl)

CODE_MAP_WIN(Enter,                     0x001C)
CODE_MAP_MAC(Enter,                     kVK_Return)

CODE_MAP_WIN(OSLeft,                    0xE05B)
CODE_MAP_MAC(OSLeft,                    kVK_Command)

CODE_MAP_WIN(OSRight,                   0xE05C)
CODE_MAP_MAC(OSRight,                   kVK_RightCommand)

CODE_MAP_WIN(ShiftLeft,                 0x002A)
CODE_MAP_MAC(ShiftLeft,                 kVK_Shift)

CODE_MAP_WIN(ShiftRight,                0x0036)
CODE_MAP_MAC(ShiftRight,                kVK_RightShift)

CODE_MAP_WIN(Space,                     0x0039)
CODE_MAP_MAC(Space,                     kVK_Space)

CODE_MAP_WIN(Tab,                       0x000F)
CODE_MAP_MAC(Tab,                       kVK_Tab)

// IME keys
CODE_MAP_WIN(Convert,                   0x0079)

CODE_MAP_WIN(Lang1,                     0x0072) // for non-Korean layout
CODE_MAP_WIN(Lang1,                     0xE0F2) // for Korean layout
CODE_MAP_MAC(Lang1,                     kVK_JIS_Kana)

CODE_MAP_WIN(Lang2,                     0x0071) // for non-Korean layout
CODE_MAP_WIN(Lang2,                     0xE0F1) // for Korean layout
CODE_MAP_MAC(Lang2,                     kVK_JIS_Eisu)

CODE_MAP_WIN(KanaMode,                  0x0070)

CODE_MAP_WIN(NonConvert,                0x007B)

// Control pad section
CODE_MAP_WIN(Delete,                    0xE053)
CODE_MAP_MAC(Delete,                    kVK_ForwardDelete)

CODE_MAP_WIN(End,                       0xE04F)
CODE_MAP_MAC(End,                       kVK_End)

CODE_MAP_MAC(Help,                      kVK_Help) // Insert key on PC keyboard

CODE_MAP_WIN(Home,                      0xE047)
CODE_MAP_MAC(Home,                      kVK_Home)

CODE_MAP_WIN(Insert,                    0xE052)

CODE_MAP_WIN(PageDown,                  0xE051)
CODE_MAP_MAC(PageDown,                  kVK_PageDown)

CODE_MAP_WIN(PageUp,                    0xE049)
CODE_MAP_MAC(PageUp,                    kVK_PageUp)

// Arrow pad section
CODE_MAP_WIN(ArrowDown,                 0xE050)
CODE_MAP_MAC(ArrowDown,                 kVK_DownArrow)

CODE_MAP_WIN(ArrowLeft,                 0xE04B)
CODE_MAP_MAC(ArrowLeft,                 kVK_LeftArrow)

CODE_MAP_WIN(ArrowRight,                0xE04D)
CODE_MAP_MAC(ArrowRight,                kVK_RightArrow)

CODE_MAP_WIN(ArrowUp,                   0xE048)
CODE_MAP_MAC(ArrowUp,                   kVK_UpArrow)

// Numpad section
CODE_MAP_WIN(NumLock,                   0xE045) // MSDN says 0x0045, though...
CODE_MAP_MAC(NumLock,                   kVK_ANSI_KeypadClear)

CODE_MAP_WIN(Numpad0,                   0x0052)
CODE_MAP_MAC(Numpad0,                   kVK_ANSI_Keypad0)

CODE_MAP_WIN(Numpad1,                   0x004F)
CODE_MAP_MAC(Numpad1,                   kVK_ANSI_Keypad1)

CODE_MAP_WIN(Numpad2,                   0x0050)
CODE_MAP_MAC(Numpad2,                   kVK_ANSI_Keypad2)

CODE_MAP_WIN(Numpad3,                   0x0051)
CODE_MAP_MAC(Numpad3,                   kVK_ANSI_Keypad3)

CODE_MAP_WIN(Numpad4,                   0x004B)
CODE_MAP_MAC(Numpad4,                   kVK_ANSI_Keypad4)

CODE_MAP_WIN(Numpad5,                   0x004C)
CODE_MAP_MAC(Numpad5,                   kVK_ANSI_Keypad5)

CODE_MAP_WIN(Numpad6,                   0x004D)
CODE_MAP_MAC(Numpad6,                   kVK_ANSI_Keypad6)

CODE_MAP_WIN(Numpad7,                   0x0047)
CODE_MAP_MAC(Numpad7,                   kVK_ANSI_Keypad7)

CODE_MAP_WIN(Numpad8,                   0x0048)
CODE_MAP_MAC(Numpad8,                   kVK_ANSI_Keypad8)

CODE_MAP_WIN(Numpad9,                   0x0049)
CODE_MAP_MAC(Numpad9,                   kVK_ANSI_Keypad9)

CODE_MAP_WIN(NumpadAdd,                 0x004E)
CODE_MAP_MAC(NumpadAdd,                 kVK_ANSI_KeypadPlus)

CODE_MAP_WIN(NumpadComma,               0x007E)
CODE_MAP_MAC(NumpadComma,               kVK_JIS_KeypadComma)

CODE_MAP_WIN(NumpadDecimal,             0x0053)
CODE_MAP_MAC(NumpadDecimal,             kVK_ANSI_KeypadDecimal)

CODE_MAP_WIN(NumpadDivide,              0xE035)
CODE_MAP_MAC(NumpadDivide,              kVK_ANSI_KeypadDivide)

CODE_MAP_WIN(NumpadEnter,               0xE01C)
CODE_MAP_MAC(NumpadEnter,               kVK_ANSI_KeypadEnter)
CODE_MAP_MAC(NumpadEnter,               kVK_Powerbook_KeypadEnter)

CODE_MAP_WIN(NumpadEqual,               0x0059)
CODE_MAP_MAC(NumpadEqual,               kVK_ANSI_KeypadEquals)

CODE_MAP_WIN(NumpadMultiply,            0x0037)
CODE_MAP_MAC(NumpadMultiply,            kVK_ANSI_KeypadMultiply)

CODE_MAP_WIN(NumpadSubtract,            0x004A)
CODE_MAP_MAC(NumpadSubtract,            kVK_ANSI_KeypadMinus)

// Function section
CODE_MAP_WIN(Escape,                    0x0001)
CODE_MAP_MAC(Escape,                    kVK_Escape)

CODE_MAP_WIN(F1,                        0x003B)
CODE_MAP_MAC(F1,                        kVK_F1)

CODE_MAP_WIN(F2,                        0x003C)
CODE_MAP_MAC(F2,                        kVK_F2)

CODE_MAP_WIN(F3,                        0x003D)
CODE_MAP_MAC(F3,                        kVK_F3)

CODE_MAP_WIN(F4,                        0x003E)
CODE_MAP_MAC(F4,                        kVK_F4)

CODE_MAP_WIN(F5,                        0x003F)
CODE_MAP_MAC(F5,                        kVK_F5)

CODE_MAP_WIN(F6,                        0x0040)
CODE_MAP_MAC(F6,                        kVK_F6)

CODE_MAP_WIN(F7,                        0x0041)
CODE_MAP_MAC(F7,                        kVK_F7)

CODE_MAP_WIN(F8,                        0x0042)
CODE_MAP_MAC(F8,                        kVK_F8)

CODE_MAP_WIN(F9,                        0x0043)
CODE_MAP_MAC(F9,                        kVK_F9)

CODE_MAP_WIN(F10,                       0x0044)
CODE_MAP_MAC(F10,                       kVK_F10)

CODE_MAP_WIN(F11,                       0x0057)
CODE_MAP_MAC(F11,                       kVK_F11)

CODE_MAP_WIN(F12,                       0x0058)
CODE_MAP_MAC(F12,                       kVK_F12)

CODE_MAP_WIN(F13,                       0x0064)
CODE_MAP_MAC(F13,                       kVK_F13) // PrintScreen on PC keyboard

CODE_MAP_WIN(F14,                       0x0065)
CODE_MAP_MAC(F14,                       kVK_F14) // ScrollLock on PC keyboard

CODE_MAP_WIN(F15,                       0x0066)
CODE_MAP_MAC(F15,                       kVK_F15) // Pause on PC keyboard

CODE_MAP_WIN(F16,                       0x0067)
CODE_MAP_MAC(F16,                       kVK_F16)

CODE_MAP_WIN(F17,                       0x0068)
CODE_MAP_MAC(F17,                       kVK_F17)

CODE_MAP_WIN(F18,                       0x0069)
CODE_MAP_MAC(F18,                       kVK_F18)

CODE_MAP_WIN(F19,                       0x006A)
CODE_MAP_MAC(F19,                       kVK_F19)

CODE_MAP_WIN(F20,                       0x006B)
CODE_MAP_MAC(F20,                       kVK_F20)

CODE_MAP_WIN(F21,                       0x006C)

CODE_MAP_WIN(F22,                       0x006D)

CODE_MAP_WIN(F23,                       0x006E)

CODE_MAP_WIN(F24,                       0x0076)

CODE_MAP_MAC(Fn,                        kVK_Function) // not available?

CODE_MAP_WIN(PrintScreen,               0xE037)
CODE_MAP_WIN(PrintScreen,               0x0054) // Alt + PrintScreen

CODE_MAP_WIN(ScrollLock,                0x0046)

CODE_MAP_WIN(Pause,                     0x0045)
CODE_MAP_WIN(Pause,                     0xE046) // Ctrl + Pause

// Media keys

// NOTE: Following media keys which cause scancode 0xE000 on Windows should be
//       mapped with virtual keycode.
//       See KeyboardLayout::ConvertScanCodeToCodeNameIndex() for the detail.

// CODE_MAP_WIN(BrowserBack,               0xE000) // VK_BROWSER_BACK

// CODE_MAP_WIN(BrowserFavorites,          0xE000) // VK_BROWSER_FAVORITES

// CODE_MAP_WIN(BrowserForward,            0xE000) // VK_BROWSER_FORWARD

// CODE_MAP_WIN(BrowserHome,               0xE000) // VK_BROWSER_HOME

// CODE_MAP_WIN(BrowserRefresh,            0xE000) // VK_BROWSER_REFRESH

// CODE_MAP_WIN(BrowserSearch,             0xE000) // VK_BROWSER_SEARCH

// CODE_MAP_WIN(BrowserStop,               0xE000) // VK_BROWSER_STOP

// CODE_MAP_WIN(Eject) // not available?
// CODE_MAP_MAC(Eject) // not available?

// CODE_MAP_WIN(LaunchApp1,                0xE000) // VK_LAUNCH_APP1

// CODE_MAP_WIN(LaunchApp2,                0xE000) // VK_LAUNCH_APP2

// CODE_MAP_WIN(LaunchMail,                0xE000) // VK_LAUNCH_MAIL

// CODE_MAP_WIN(MediaPlayPause,            0xE000) // VK_MEDIA_PLAY_PAUSE

// CODE_MAP_WIN(MediaSelect,               0xE000) // VK_LAUNCH_MEDIA_SELECT

// CODE_MAP_WIN(MediaStop,                 0xE000) // VK_MEDIA_STOP

// CODE_MAP_WIN(MediaTrackNext,            0xE000) // VK_MEDIA_NEXT_TRACK

// CODE_MAP_WIN(MediaTrackPrevious,        0xE000) // VK_MEDIA_PREV_TRACK

CODE_MAP_WIN(Power,                     0xE05E)
CODE_MAP_MAC(Power,                     0x007F) // On 10.7 and 10.8 only

// CODE_MAP_WIN(Sleep) // not available?

// CODE_MAP_WIN(VolumeDown,                0xE000) // VK_VOLUME_DOWN
CODE_MAP_MAC(VolumeDown,                kVK_VolumeDown) // not available?

// CODE_MAP_WIN(VolumeMute,                0xE000) // VK_VOLUME_MUTE
CODE_MAP_MAC(VolumeMute,                kVK_Mute) // not available?

// CODE_MAP_WIN(VolumeUp,                  0xE000) // VK_VOLUME_UP
CODE_MAP_MAC(VolumeUp,                  kVK_VolumeUp) // not available?

// CODE_MAP_WIN(WakeUp) // not available?

#undef CODE_MAP_WIN
#undef CODE_MAP_MAC
#undef CODE_MAP_X11
#undef CODE_MAP_ANDROID
