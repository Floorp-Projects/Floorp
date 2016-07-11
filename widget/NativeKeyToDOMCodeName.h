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
#elif defined(MOZ_WIDGET_GTK)
#undef CODE_MAP_X11
// aNativeKey is hardware_keycode of GDKEvent or nativeScanCode of QKeyEvent.
#define CODE_MAP_X11(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#elif defined(ANDROID)
#undef CODE_MAP_ANDROID
// aNativeKey is scan code
#define CODE_MAP_ANDROID(aCPPCodeName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, \
                                       CODE_NAME_INDEX_##aCPPCodeName)
#endif

// Writing system keys
CODE_MAP_WIN(Backquote,                 0x0029)
CODE_MAP_MAC(Backquote,                 kVK_ANSI_Grave)
CODE_MAP_X11(Backquote,                 0x0031)
CODE_MAP_ANDROID(Backquote,             0x0029)

CODE_MAP_WIN(Backslash,                 0x002B)
CODE_MAP_MAC(Backslash,                 kVK_ANSI_Backslash)
CODE_MAP_X11(Backslash,                 0x0033)
CODE_MAP_ANDROID(Backslash,             0x002B)

CODE_MAP_WIN(Backspace,                 0x000E)
CODE_MAP_MAC(Backspace,                 kVK_Delete)
CODE_MAP_X11(Backspace,                 0x0016)
CODE_MAP_ANDROID(Backspace,             0x000E)

CODE_MAP_WIN(BracketLeft,               0x001A)
CODE_MAP_MAC(BracketLeft,               kVK_ANSI_LeftBracket)
CODE_MAP_X11(BracketLeft,               0x0022)
CODE_MAP_ANDROID(BracketLeft,           0x001A)

CODE_MAP_WIN(BracketRight,              0x001B)
CODE_MAP_MAC(BracketRight,              kVK_ANSI_RightBracket)
CODE_MAP_X11(BracketRight,              0x0023)
CODE_MAP_ANDROID(BracketRight,          0x001B)

CODE_MAP_WIN(Comma,                     0x0033)
CODE_MAP_MAC(Comma,                     kVK_ANSI_Comma)
CODE_MAP_X11(Comma,                     0x003B)
CODE_MAP_ANDROID(Comma,                 0x00033)

CODE_MAP_WIN(Digit0,                    0x000B)
CODE_MAP_MAC(Digit0,                    kVK_ANSI_0)
CODE_MAP_X11(Digit0,                    0x0013)
CODE_MAP_ANDROID(Digit0,                0x000B)

CODE_MAP_WIN(Digit1,                    0x0002)
CODE_MAP_MAC(Digit1,                    kVK_ANSI_1)
CODE_MAP_X11(Digit1,                    0x000A)
CODE_MAP_ANDROID(Digit1,                0x0002)

CODE_MAP_WIN(Digit2,                    0x0003)
CODE_MAP_MAC(Digit2,                    kVK_ANSI_2)
CODE_MAP_X11(Digit2,                    0x000B)
CODE_MAP_ANDROID(Digit2,                0x0003)

CODE_MAP_WIN(Digit3,                    0x0004)
CODE_MAP_MAC(Digit3,                    kVK_ANSI_3)
CODE_MAP_X11(Digit3,                    0x000C)
CODE_MAP_ANDROID(Digit3,                0x0004)

CODE_MAP_WIN(Digit4,                    0x0005)
CODE_MAP_MAC(Digit4,                    kVK_ANSI_4)
CODE_MAP_X11(Digit4,                    0x000D)
CODE_MAP_ANDROID(Digit4,                0x0005)

CODE_MAP_WIN(Digit5,                    0x0006)
CODE_MAP_MAC(Digit5,                    kVK_ANSI_5)
CODE_MAP_X11(Digit5,                    0x000E)
CODE_MAP_ANDROID(Digit5,                0x0006)

CODE_MAP_WIN(Digit6,                    0x0007)
CODE_MAP_MAC(Digit6,                    kVK_ANSI_6)
CODE_MAP_X11(Digit6,                    0x000F)
CODE_MAP_ANDROID(Digit6,                0x0007)

CODE_MAP_WIN(Digit7,                    0x0008)
CODE_MAP_MAC(Digit7,                    kVK_ANSI_7)
CODE_MAP_X11(Digit7,                    0x0010)
CODE_MAP_ANDROID(Digit7,                0x0008)

CODE_MAP_WIN(Digit8,                    0x0009)
CODE_MAP_MAC(Digit8,                    kVK_ANSI_8)
CODE_MAP_X11(Digit8,                    0x0011)
CODE_MAP_ANDROID(Digit8,                0x0009)

CODE_MAP_WIN(Digit9,                    0x000A)
CODE_MAP_MAC(Digit9,                    kVK_ANSI_9)
CODE_MAP_X11(Digit9,                    0x0012)
CODE_MAP_ANDROID(Digit9,                0x000A)

CODE_MAP_WIN(Equal,                     0x000D)
CODE_MAP_MAC(Equal,                     kVK_ANSI_Equal)
CODE_MAP_X11(Equal,                     0x0015)
CODE_MAP_ANDROID(Equal,                 0x000D)

CODE_MAP_WIN(IntlBackslash,             0x0056)
CODE_MAP_MAC(IntlBackslash,             kVK_ISO_Section)
CODE_MAP_X11(IntlBackslash,             0x005E)
CODE_MAP_ANDROID(IntlBackslash,         0x0056)

// Win: IntlHash's scan code is shared with "Backslash" key.
// Mac: IntlHash's virtual key code is shared with "Backslash" key.
// X11: IntlHash's scan code is shared with "Backslash" key.
// Android: IntlHash's scan code is shared with "Backslash" key.

CODE_MAP_WIN(IntlRo,                    0x0073)
CODE_MAP_MAC(IntlRo,                    kVK_JIS_Underscore)
CODE_MAP_X11(IntlRo,                    0x0061)
CODE_MAP_ANDROID(IntlRo,                0x0059)

CODE_MAP_WIN(IntlYen,                   0x007D)
CODE_MAP_MAC(IntlYen,                   kVK_JIS_Yen)
CODE_MAP_X11(IntlYen,                   0x0084)
CODE_MAP_ANDROID(IntlYen,               0x007C)

CODE_MAP_WIN(KeyA,                      0x001E)
CODE_MAP_MAC(KeyA,                      kVK_ANSI_A)
CODE_MAP_X11(KeyA,                      0x0026)
CODE_MAP_ANDROID(KeyA,                  0x001E)

CODE_MAP_WIN(KeyB,                      0x0030)
CODE_MAP_MAC(KeyB,                      kVK_ANSI_B)
CODE_MAP_X11(KeyB,                      0x0038)
CODE_MAP_ANDROID(KeyB,                  0x0030)

CODE_MAP_WIN(KeyC,                      0x002E)
CODE_MAP_MAC(KeyC,                      kVK_ANSI_C)
CODE_MAP_X11(KeyC,                      0x0036)
CODE_MAP_ANDROID(KeyC,                  0x002E)

CODE_MAP_WIN(KeyD,                      0x0020)
CODE_MAP_MAC(KeyD,                      kVK_ANSI_D)
CODE_MAP_X11(KeyD,                      0x0028)
CODE_MAP_ANDROID(KeyD,                  0x0020)

CODE_MAP_WIN(KeyE,                      0x0012)
CODE_MAP_MAC(KeyE,                      kVK_ANSI_E)
CODE_MAP_X11(KeyE,                      0x001A)
CODE_MAP_ANDROID(KeyE,                  0x0012)

CODE_MAP_WIN(KeyF,                      0x0021)
CODE_MAP_MAC(KeyF,                      kVK_ANSI_F)
CODE_MAP_X11(KeyF,                      0x0029)
CODE_MAP_ANDROID(KeyF,                  0x0021)

CODE_MAP_WIN(KeyG,                      0x0022)
CODE_MAP_MAC(KeyG,                      kVK_ANSI_G)
CODE_MAP_X11(KeyG,                      0x002A)
CODE_MAP_ANDROID(KeyG,                  0x0022)

CODE_MAP_WIN(KeyH,                      0x0023)
CODE_MAP_MAC(KeyH,                      kVK_ANSI_H)
CODE_MAP_X11(KeyH,                      0x002B)
CODE_MAP_ANDROID(KeyH,                  0x0023)

CODE_MAP_WIN(KeyI,                      0x0017)
CODE_MAP_MAC(KeyI,                      kVK_ANSI_I)
CODE_MAP_X11(KeyI,                      0x001F)
CODE_MAP_ANDROID(KeyI,                  0x0017)

CODE_MAP_WIN(KeyJ,                      0x0024)
CODE_MAP_MAC(KeyJ,                      kVK_ANSI_J)
CODE_MAP_X11(KeyJ,                      0x002C)
CODE_MAP_ANDROID(KeyJ,                  0x0024)

CODE_MAP_WIN(KeyK,                      0x0025)
CODE_MAP_MAC(KeyK,                      kVK_ANSI_K)
CODE_MAP_X11(KeyK,                      0x002D)
CODE_MAP_ANDROID(KeyK,                  0x0025)

CODE_MAP_WIN(KeyL,                      0x0026)
CODE_MAP_MAC(KeyL,                      kVK_ANSI_L)
CODE_MAP_X11(KeyL,                      0x002E)
CODE_MAP_ANDROID(KeyL,                  0x0026)

CODE_MAP_WIN(KeyM,                      0x0032)
CODE_MAP_MAC(KeyM,                      kVK_ANSI_M)
CODE_MAP_X11(KeyM,                      0x003A)
CODE_MAP_ANDROID(KeyM,                  0x0032)

CODE_MAP_WIN(KeyN,                      0x0031)
CODE_MAP_MAC(KeyN,                      kVK_ANSI_N)
CODE_MAP_X11(KeyN,                      0x0039)
CODE_MAP_ANDROID(KeyN,                  0x0031)

CODE_MAP_WIN(KeyO,                      0x0018)
CODE_MAP_MAC(KeyO,                      kVK_ANSI_O)
CODE_MAP_X11(KeyO,                      0x0020)
CODE_MAP_ANDROID(KeyO,                  0x0018)

CODE_MAP_WIN(KeyP,                      0x0019)
CODE_MAP_MAC(KeyP,                      kVK_ANSI_P)
CODE_MAP_X11(KeyP,                      0x0021)
CODE_MAP_ANDROID(KeyP,                  0x0019)

CODE_MAP_WIN(KeyQ,                      0x0010)
CODE_MAP_MAC(KeyQ,                      kVK_ANSI_Q)
CODE_MAP_X11(KeyQ,                      0x0018)
CODE_MAP_ANDROID(KeyQ,                  0x0010)

CODE_MAP_WIN(KeyR,                      0x0013)
CODE_MAP_MAC(KeyR,                      kVK_ANSI_R)
CODE_MAP_X11(KeyR,                      0x001B)
CODE_MAP_ANDROID(KeyR,                  0x0013)

CODE_MAP_WIN(KeyS,                      0x001F)
CODE_MAP_MAC(KeyS,                      kVK_ANSI_S)
CODE_MAP_X11(KeyS,                      0x0027)
CODE_MAP_ANDROID(KeyS,                  0x001F)

CODE_MAP_WIN(KeyT,                      0x0014)
CODE_MAP_MAC(KeyT,                      kVK_ANSI_T)
CODE_MAP_X11(KeyT,                      0x001C)
CODE_MAP_ANDROID(KeyT,                  0x0014)

CODE_MAP_WIN(KeyU,                      0x0016)
CODE_MAP_MAC(KeyU,                      kVK_ANSI_U)
CODE_MAP_X11(KeyU,                      0x001E)
CODE_MAP_ANDROID(KeyU,                  0x0016)

CODE_MAP_WIN(KeyV,                      0x002F)
CODE_MAP_MAC(KeyV,                      kVK_ANSI_V)
CODE_MAP_X11(KeyV,                      0x0037)
CODE_MAP_ANDROID(KeyV,                  0x002F)

CODE_MAP_WIN(KeyW,                      0x0011)
CODE_MAP_MAC(KeyW,                      kVK_ANSI_W)
CODE_MAP_X11(KeyW,                      0x0019)
CODE_MAP_ANDROID(KeyW,                  0x0011)

CODE_MAP_WIN(KeyX,                      0x002D)
CODE_MAP_MAC(KeyX,                      kVK_ANSI_X)
CODE_MAP_X11(KeyX,                      0x0035)
CODE_MAP_ANDROID(KeyX,                  0x002D)

CODE_MAP_WIN(KeyY,                      0x0015)
CODE_MAP_MAC(KeyY,                      kVK_ANSI_Y)
CODE_MAP_X11(KeyY,                      0x001D)
CODE_MAP_ANDROID(KeyY,                  0x0015)

CODE_MAP_WIN(KeyZ,                      0x002C)
CODE_MAP_MAC(KeyZ,                      kVK_ANSI_Z)
CODE_MAP_X11(KeyZ,                      0x0034)
CODE_MAP_ANDROID(KeyZ,                  0x002C)

CODE_MAP_WIN(Minus,                     0x000C)
CODE_MAP_MAC(Minus,                     kVK_ANSI_Minus)
CODE_MAP_X11(Minus,                     0x0014)
CODE_MAP_ANDROID(Minus,                 0x000C)

CODE_MAP_WIN(Period,                    0x0034)
CODE_MAP_MAC(Period,                    kVK_ANSI_Period)
CODE_MAP_X11(Period,                    0x003C)
CODE_MAP_ANDROID(Period,                0x0034)

CODE_MAP_WIN(Quote,                     0x0028)
CODE_MAP_MAC(Quote,                     kVK_ANSI_Quote)
CODE_MAP_X11(Quote,                     0x0030)
CODE_MAP_ANDROID(Quote,                 0x0028)

CODE_MAP_WIN(Semicolon,                 0x0027)
CODE_MAP_MAC(Semicolon,                 kVK_ANSI_Semicolon)
CODE_MAP_X11(Semicolon,                 0x002F)
CODE_MAP_ANDROID(Semicolon,             0x0027)

CODE_MAP_WIN(Slash,                     0x0035)
CODE_MAP_MAC(Slash,                     kVK_ANSI_Slash)
CODE_MAP_X11(Slash,                     0x003D)
CODE_MAP_ANDROID(Slash,                 0x0035)

// Functional keys
CODE_MAP_WIN(AltLeft,                   0x0038)
CODE_MAP_MAC(AltLeft,                   kVK_Option)
CODE_MAP_X11(AltLeft,                   0x0040)
CODE_MAP_ANDROID(AltLeft,               0x0038)

CODE_MAP_WIN(AltRight,                  0xE038)
CODE_MAP_MAC(AltRight,                  kVK_RightOption)
CODE_MAP_X11(AltRight,                  0x006C)
CODE_MAP_ANDROID(AltRight,              0x0064)

CODE_MAP_WIN(CapsLock,                  0x003A)
CODE_MAP_MAC(CapsLock,                  kVK_CapsLock)
CODE_MAP_X11(CapsLock,                  0x0042)
CODE_MAP_ANDROID(CapsLock,              0x003A)

CODE_MAP_WIN(ContextMenu,               0xE05D)
CODE_MAP_MAC(ContextMenu,               kVK_PC_ContextMenu)
CODE_MAP_X11(ContextMenu,               0x0087)
CODE_MAP_ANDROID(ContextMenu,           0x007F)

CODE_MAP_WIN(ControlLeft,               0x001D)
CODE_MAP_MAC(ControlLeft,               kVK_Control)
CODE_MAP_X11(ControlLeft,               0x0025)
CODE_MAP_ANDROID(ControlLeft,           0x001D)

CODE_MAP_WIN(ControlRight,              0xE01D)
CODE_MAP_MAC(ControlRight,              kVK_RightControl)
CODE_MAP_X11(ControlRight,              0x0069)
CODE_MAP_ANDROID(ControlRight,          0x0061)

CODE_MAP_WIN(Enter,                     0x001C)
CODE_MAP_MAC(Enter,                     kVK_Return)
CODE_MAP_X11(Enter,                     0x0024)
CODE_MAP_ANDROID(Enter,                 0x001C)

CODE_MAP_WIN(OSLeft,                    0xE05B)
CODE_MAP_MAC(OSLeft,                    kVK_Command)
CODE_MAP_X11(OSLeft,                    0x0085)
CODE_MAP_ANDROID(OSLeft,                0x007D)

CODE_MAP_WIN(OSRight,                   0xE05C)
CODE_MAP_MAC(OSRight,                   kVK_RightCommand)
CODE_MAP_X11(OSRight,                   0x0086)
CODE_MAP_ANDROID(OSRight,               0x007E)

CODE_MAP_WIN(ShiftLeft,                 0x002A)
CODE_MAP_MAC(ShiftLeft,                 kVK_Shift)
CODE_MAP_X11(ShiftLeft,                 0x0032)
CODE_MAP_ANDROID(ShiftLeft,             0x002A)

CODE_MAP_WIN(ShiftRight,                0x0036)
CODE_MAP_MAC(ShiftRight,                kVK_RightShift)
CODE_MAP_X11(ShiftRight,                0x003E)
CODE_MAP_ANDROID(ShiftRight,            0x0036)

CODE_MAP_WIN(Space,                     0x0039)
CODE_MAP_MAC(Space,                     kVK_Space)
CODE_MAP_X11(Space,                     0x0041)
CODE_MAP_ANDROID(Space,                 0x0039)

CODE_MAP_WIN(Tab,                       0x000F)
CODE_MAP_MAC(Tab,                       kVK_Tab)
CODE_MAP_X11(Tab,                       0x0017)
CODE_MAP_ANDROID(Tab,                   0x000F)

// IME keys
CODE_MAP_WIN(Convert,                   0x0079)
CODE_MAP_X11(Convert,                   0x0064)
CODE_MAP_ANDROID(Convert,               0x005C)

CODE_MAP_WIN(Lang1,                     0x0072) // for non-Korean layout
CODE_MAP_WIN(Lang1,                     0xE0F2) // for Korean layout
CODE_MAP_MAC(Lang1,                     kVK_JIS_Kana)
CODE_MAP_X11(Lang1,                     0x0082)
CODE_MAP_ANDROID(Lang1,                 0x007A)

CODE_MAP_WIN(Lang2,                     0x0071) // for non-Korean layout
CODE_MAP_WIN(Lang2,                     0xE0F1) // for Korean layout
CODE_MAP_MAC(Lang2,                     kVK_JIS_Eisu)
CODE_MAP_X11(Lang2,                     0x0083)
CODE_MAP_ANDROID(Lang2,                 0x007B)

CODE_MAP_WIN(KanaMode,                  0x0070)
CODE_MAP_X11(KanaMode,                  0x0065)
CODE_MAP_ANDROID(KanaMode,              0x005D)

CODE_MAP_WIN(NonConvert,                0x007B)
CODE_MAP_X11(NonConvert,                0x0066)
CODE_MAP_ANDROID(NonConvert,            0x005E)

// Control pad section
CODE_MAP_WIN(Delete,                    0xE053)
CODE_MAP_MAC(Delete,                    kVK_ForwardDelete)
CODE_MAP_X11(Delete,                    0x0077)
CODE_MAP_ANDROID(Delete,                0x006F)

CODE_MAP_WIN(End,                       0xE04F)
CODE_MAP_MAC(End,                       kVK_End)
CODE_MAP_X11(End,                       0x0073)
CODE_MAP_ANDROID(End,                   0x006B)

CODE_MAP_MAC(Help,                      kVK_Help) // Insert key on PC keyboard
CODE_MAP_X11(Help,                      0x0092) // Help key on Sun keyboard
CODE_MAP_ANDROID(Help,                  0x008A) // Help key on Sun keyboard

CODE_MAP_WIN(Home,                      0xE047)
CODE_MAP_MAC(Home,                      kVK_Home)
CODE_MAP_X11(Home,                      0x006E)
CODE_MAP_ANDROID(Home,                  0x0066)

CODE_MAP_WIN(Insert,                    0xE052)
CODE_MAP_X11(Insert,                    0x0076)
CODE_MAP_ANDROID(Insert,                0x006E)

CODE_MAP_WIN(PageDown,                  0xE051)
CODE_MAP_MAC(PageDown,                  kVK_PageDown)
CODE_MAP_X11(PageDown,                  0x0075)
CODE_MAP_ANDROID(PageDown,              0x006D)

CODE_MAP_WIN(PageUp,                    0xE049)
CODE_MAP_MAC(PageUp,                    kVK_PageUp)
CODE_MAP_X11(PageUp,                    0x0070)
CODE_MAP_ANDROID(PageUp,                0x0068)

// Arrow pad section
CODE_MAP_WIN(ArrowDown,                 0xE050)
CODE_MAP_MAC(ArrowDown,                 kVK_DownArrow)
CODE_MAP_X11(ArrowDown,                 0x0074)
CODE_MAP_ANDROID(ArrowDown,             0x006C)

CODE_MAP_WIN(ArrowLeft,                 0xE04B)
CODE_MAP_MAC(ArrowLeft,                 kVK_LeftArrow)
CODE_MAP_X11(ArrowLeft,                 0x0071)
CODE_MAP_ANDROID(ArrowLeft,             0x0069)

CODE_MAP_WIN(ArrowRight,                0xE04D)
CODE_MAP_MAC(ArrowRight,                kVK_RightArrow)
CODE_MAP_X11(ArrowRight,                0x0072)
CODE_MAP_ANDROID(ArrowRight,            0x006A)

CODE_MAP_WIN(ArrowUp,                   0xE048)
CODE_MAP_MAC(ArrowUp,                   kVK_UpArrow)
CODE_MAP_X11(ArrowUp,                   0x006F)
CODE_MAP_ANDROID(ArrowUp,               0x0067)

// Numpad section
CODE_MAP_WIN(NumLock,                   0xE045) // MSDN says 0x0045, though...
CODE_MAP_MAC(NumLock,                   kVK_ANSI_KeypadClear)
CODE_MAP_X11(NumLock,                   0x004D)
CODE_MAP_ANDROID(NumLock,               0x0045)

CODE_MAP_WIN(Numpad0,                   0x0052)
CODE_MAP_MAC(Numpad0,                   kVK_ANSI_Keypad0)
CODE_MAP_X11(Numpad0,                   0x005A)
CODE_MAP_ANDROID(Numpad0,               0x0052)

CODE_MAP_WIN(Numpad1,                   0x004F)
CODE_MAP_MAC(Numpad1,                   kVK_ANSI_Keypad1)
CODE_MAP_X11(Numpad1,                   0x0057)
CODE_MAP_ANDROID(Numpad1,               0x004F)

CODE_MAP_WIN(Numpad2,                   0x0050)
CODE_MAP_MAC(Numpad2,                   kVK_ANSI_Keypad2)
CODE_MAP_X11(Numpad2,                   0x0058)
CODE_MAP_ANDROID(Numpad2,               0x0050)

CODE_MAP_WIN(Numpad3,                   0x0051)
CODE_MAP_MAC(Numpad3,                   kVK_ANSI_Keypad3)
CODE_MAP_X11(Numpad3,                   0x0059)
CODE_MAP_ANDROID(Numpad3,               0x0051)

CODE_MAP_WIN(Numpad4,                   0x004B)
CODE_MAP_MAC(Numpad4,                   kVK_ANSI_Keypad4)
CODE_MAP_X11(Numpad4,                   0x0053)
CODE_MAP_ANDROID(Numpad4,               0x004B)

CODE_MAP_WIN(Numpad5,                   0x004C)
CODE_MAP_MAC(Numpad5,                   kVK_ANSI_Keypad5)
CODE_MAP_X11(Numpad5,                   0x0054)
CODE_MAP_ANDROID(Numpad5,               0x004C)

CODE_MAP_WIN(Numpad6,                   0x004D)
CODE_MAP_MAC(Numpad6,                   kVK_ANSI_Keypad6)
CODE_MAP_X11(Numpad6,                   0x0055)
CODE_MAP_ANDROID(Numpad6,               0x004D)

CODE_MAP_WIN(Numpad7,                   0x0047)
CODE_MAP_MAC(Numpad7,                   kVK_ANSI_Keypad7)
CODE_MAP_X11(Numpad7,                   0x004F)
CODE_MAP_ANDROID(Numpad7,               0x0047)

CODE_MAP_WIN(Numpad8,                   0x0048)
CODE_MAP_MAC(Numpad8,                   kVK_ANSI_Keypad8)
CODE_MAP_X11(Numpad8,                   0x0050)
CODE_MAP_ANDROID(Numpad8,               0x0048)

CODE_MAP_WIN(Numpad9,                   0x0049)
CODE_MAP_MAC(Numpad9,                   kVK_ANSI_Keypad9)
CODE_MAP_X11(Numpad9,                   0x0051)
CODE_MAP_ANDROID(Numpad9,               0x0049)

CODE_MAP_WIN(NumpadAdd,                 0x004E)
CODE_MAP_MAC(NumpadAdd,                 kVK_ANSI_KeypadPlus)
CODE_MAP_X11(NumpadAdd,                 0x0056)
CODE_MAP_ANDROID(NumpadAdd,             0x004E)

CODE_MAP_WIN(NumpadComma,               0x007E)
CODE_MAP_MAC(NumpadComma,               kVK_JIS_KeypadComma)
CODE_MAP_X11(NumpadComma,               0x0081)
CODE_MAP_ANDROID(NumpadComma,           0x0079)

CODE_MAP_WIN(NumpadDecimal,             0x0053)
CODE_MAP_MAC(NumpadDecimal,             kVK_ANSI_KeypadDecimal)
CODE_MAP_X11(NumpadDecimal,             0x005B)
CODE_MAP_ANDROID(NumpadDecimal,         0x0053)

CODE_MAP_WIN(NumpadDivide,              0xE035)
CODE_MAP_MAC(NumpadDivide,              kVK_ANSI_KeypadDivide)
CODE_MAP_X11(NumpadDivide,              0x006A)
CODE_MAP_ANDROID(NumpadDivide,          0x0062)

CODE_MAP_WIN(NumpadEnter,               0xE01C)
CODE_MAP_MAC(NumpadEnter,               kVK_ANSI_KeypadEnter)
CODE_MAP_MAC(NumpadEnter,               kVK_Powerbook_KeypadEnter)
CODE_MAP_X11(NumpadEnter,               0x0068)
CODE_MAP_ANDROID(NumpadEnter,           0x0060)

CODE_MAP_WIN(NumpadEqual,               0x0059)
CODE_MAP_MAC(NumpadEqual,               kVK_ANSI_KeypadEquals)
CODE_MAP_X11(NumpadEqual,               0x007D)
CODE_MAP_ANDROID(NumpadEqual,           0x0075)

CODE_MAP_WIN(NumpadMultiply,            0x0037)
CODE_MAP_MAC(NumpadMultiply,            kVK_ANSI_KeypadMultiply)
CODE_MAP_X11(NumpadMultiply,            0x003F)
CODE_MAP_ANDROID(NumpadMultiply,        0x0037)

CODE_MAP_WIN(NumpadSubtract,            0x004A)
CODE_MAP_MAC(NumpadSubtract,            kVK_ANSI_KeypadMinus)
CODE_MAP_X11(NumpadSubtract,            0x0052)
CODE_MAP_ANDROID(NumpadSubtract,        0x004A)

// Function section
CODE_MAP_WIN(Escape,                    0x0001)
CODE_MAP_MAC(Escape,                    kVK_Escape)
CODE_MAP_X11(Escape,                    0x0009)
CODE_MAP_ANDROID(Escape,                0x0001)

CODE_MAP_WIN(F1,                        0x003B)
CODE_MAP_MAC(F1,                        kVK_F1)
CODE_MAP_X11(F1,                        0x0043)
CODE_MAP_ANDROID(F1,                    0x003B)

CODE_MAP_WIN(F2,                        0x003C)
CODE_MAP_MAC(F2,                        kVK_F2)
CODE_MAP_X11(F2,                        0x0044)
CODE_MAP_ANDROID(F2,                    0x003C)

CODE_MAP_WIN(F3,                        0x003D)
CODE_MAP_MAC(F3,                        kVK_F3)
CODE_MAP_X11(F3,                        0x0045)
CODE_MAP_ANDROID(F3,                    0x003D)

CODE_MAP_WIN(F4,                        0x003E)
CODE_MAP_MAC(F4,                        kVK_F4)
CODE_MAP_X11(F4,                        0x0046)
CODE_MAP_ANDROID(F4,                    0x003E)

CODE_MAP_WIN(F5,                        0x003F)
CODE_MAP_MAC(F5,                        kVK_F5)
CODE_MAP_X11(F5,                        0x0047)
CODE_MAP_ANDROID(F5,                    0x003F)

CODE_MAP_WIN(F6,                        0x0040)
CODE_MAP_MAC(F6,                        kVK_F6)
CODE_MAP_X11(F6,                        0x0048)
CODE_MAP_ANDROID(F6,                    0x0040)

CODE_MAP_WIN(F7,                        0x0041)
CODE_MAP_MAC(F7,                        kVK_F7)
CODE_MAP_X11(F7,                        0x0049)
CODE_MAP_ANDROID(F7,                    0x0041)

CODE_MAP_WIN(F8,                        0x0042)
CODE_MAP_MAC(F8,                        kVK_F8)
CODE_MAP_X11(F8,                        0x004A)
CODE_MAP_ANDROID(F8,                    0x0042)

CODE_MAP_WIN(F9,                        0x0043)
CODE_MAP_MAC(F9,                        kVK_F9)
CODE_MAP_X11(F9,                        0x004B)
CODE_MAP_ANDROID(F9,                    0x0043)

CODE_MAP_WIN(F10,                       0x0044)
CODE_MAP_MAC(F10,                       kVK_F10)
CODE_MAP_X11(F10,                       0x004C)
CODE_MAP_ANDROID(F10,                   0x0044)

CODE_MAP_WIN(F11,                       0x0057)
CODE_MAP_MAC(F11,                       kVK_F11)
CODE_MAP_X11(F11,                       0x005F)
CODE_MAP_ANDROID(F11,                   0x0057)

CODE_MAP_WIN(F12,                       0x0058)
CODE_MAP_MAC(F12,                       kVK_F12)
CODE_MAP_X11(F12,                       0x0060)
CODE_MAP_ANDROID(F12,                   0x0058)

CODE_MAP_WIN(F13,                       0x0064)
CODE_MAP_MAC(F13,                       kVK_F13) // PrintScreen on PC keyboard
CODE_MAP_X11(F13,                       0x00BF)
CODE_MAP_ANDROID(F13,                   0x00B7)

CODE_MAP_WIN(F14,                       0x0065)
CODE_MAP_MAC(F14,                       kVK_F14) // ScrollLock on PC keyboard
CODE_MAP_X11(F14,                       0x00C0)
CODE_MAP_ANDROID(F14,                   0x00B8)

CODE_MAP_WIN(F15,                       0x0066)
CODE_MAP_MAC(F15,                       kVK_F15) // Pause on PC keyboard
CODE_MAP_X11(F15,                       0x00C1)
CODE_MAP_ANDROID(F15,                   0x00B9)

CODE_MAP_WIN(F16,                       0x0067)
CODE_MAP_MAC(F16,                       kVK_F16)
CODE_MAP_X11(F16,                       0x00C2)
CODE_MAP_ANDROID(F16,                   0x00BA)

CODE_MAP_WIN(F17,                       0x0068)
CODE_MAP_MAC(F17,                       kVK_F17)
CODE_MAP_X11(F17,                       0x00C3)
CODE_MAP_ANDROID(F17,                   0x00BB)

CODE_MAP_WIN(F18,                       0x0069)
CODE_MAP_MAC(F18,                       kVK_F18)
CODE_MAP_X11(F18,                       0x00C4)
CODE_MAP_ANDROID(F18,                   0x00BC)

CODE_MAP_WIN(F19,                       0x006A)
CODE_MAP_MAC(F19,                       kVK_F19)
CODE_MAP_X11(F19,                       0x00C5)
CODE_MAP_ANDROID(F19,                   0x00BD)

CODE_MAP_WIN(F20,                       0x006B)
CODE_MAP_MAC(F20,                       kVK_F20)
CODE_MAP_X11(F20,                       0x00C6)
CODE_MAP_ANDROID(F20,                   0x00BE)

CODE_MAP_WIN(F21,                       0x006C)
CODE_MAP_X11(F21,                       0x00C7)
CODE_MAP_ANDROID(F21,                   0x00BF)

CODE_MAP_WIN(F22,                       0x006D)
CODE_MAP_X11(F22,                       0x00C8)
CODE_MAP_ANDROID(F22,                   0x00C0)

CODE_MAP_WIN(F23,                       0x006E)
CODE_MAP_X11(F23,                       0x00C9)
CODE_MAP_ANDROID(F23,                   0x00C1)

CODE_MAP_WIN(F24,                       0x0076)
CODE_MAP_X11(F24,                       0x00CA)
CODE_MAP_ANDROID(F24,                   0x00C2)

CODE_MAP_MAC(Fn,                        kVK_Function) // not available?
CODE_MAP_ANDROID(Fn,                    0x01D0)

CODE_MAP_WIN(PrintScreen,               0xE037)
CODE_MAP_WIN(PrintScreen,               0x0054) // Alt + PrintScreen
CODE_MAP_X11(PrintScreen,               0x006B)
CODE_MAP_ANDROID(PrintScreen,           0x0063)

CODE_MAP_WIN(ScrollLock,                0x0046)
CODE_MAP_X11(ScrollLock,                0x004E)
CODE_MAP_ANDROID(ScrollLock,            0x0046)

CODE_MAP_WIN(Pause,                     0x0045)
CODE_MAP_WIN(Pause,                     0xE046) // Ctrl + Pause
CODE_MAP_X11(Pause,                     0x007F)
CODE_MAP_ANDROID(Pause,                 0x0077)

// Media keys
CODE_MAP_WIN(BrowserBack,               0xE06A)
CODE_MAP_X11(BrowserBack,               0x00A6)
CODE_MAP_ANDROID(BrowserBack,           0x009E)

CODE_MAP_WIN(BrowserFavorites,          0xE066)
CODE_MAP_X11(BrowserFavorites,          0x00A4)
CODE_MAP_ANDROID(BrowserFavorites,      0x009C)

CODE_MAP_WIN(BrowserForward,            0xE069)
CODE_MAP_X11(BrowserForward,            0x00A7)
CODE_MAP_ANDROID(BrowserForward,        0x009F)

CODE_MAP_WIN(BrowserHome,               0xE032)
CODE_MAP_X11(BrowserHome,               0x00B4)
// CODE_MAP_ANDROID(BrowserHome) // not available? works as Home key.

CODE_MAP_WIN(BrowserRefresh,            0xE067)
CODE_MAP_X11(BrowserRefresh,            0x00B5)
CODE_MAP_ANDROID(BrowserRefresh,        0x00AD)

CODE_MAP_WIN(BrowserSearch,             0xE065)
CODE_MAP_X11(BrowserSearch,             0x00E1)
CODE_MAP_ANDROID(BrowserSearch,         0x00D9)

CODE_MAP_WIN(BrowserStop,               0xE068)
CODE_MAP_X11(BrowserStop,               0x0088)
CODE_MAP_ANDROID(BrowserStop,           0x0080)

// CODE_MAP_WIN(Eject) // not available?
// CODE_MAP_MAC(Eject) // not available?
CODE_MAP_X11(Eject,                     0x00A9)
CODE_MAP_ANDROID(Eject,                 0x00A1)

CODE_MAP_WIN(LaunchApp1,                0xE06B)
CODE_MAP_X11(LaunchApp1,                0x0098)
CODE_MAP_ANDROID(LaunchApp1,            0x0090)

CODE_MAP_WIN(LaunchApp2,                0xE021)
CODE_MAP_X11(LaunchApp2,                0x0094)
// CODE_MAP_ANDROID(LaunchApp2) // not available?

CODE_MAP_WIN(LaunchMail,                0xE06C)
CODE_MAP_X11(LaunchMail,                0x00A3)
// CODE_MAP_ANDROID(LaunchMail) // not available?

CODE_MAP_WIN(MediaPlayPause,            0xE022)
CODE_MAP_X11(MediaPlayPause,            0x00AC)
CODE_MAP_ANDROID(MediaPlayPause,        0x00A4)

CODE_MAP_WIN(MediaSelect,               0xE06D)
CODE_MAP_X11(MediaSelect,               0x00B3)
// CODE_MAP_ANDROID(MediaSelect) // not available?

CODE_MAP_WIN(MediaStop,                 0xE024)
CODE_MAP_X11(MediaStop,                 0x00AE)
CODE_MAP_ANDROID(MediaStop,             0x00A6)

CODE_MAP_WIN(MediaTrackNext,            0xE019)
CODE_MAP_X11(MediaTrackNext,            0x00AB)
CODE_MAP_ANDROID(MediaTrackNext,        0x00A3)

CODE_MAP_WIN(MediaTrackPrevious,        0xE010)
CODE_MAP_X11(MediaTrackPrevious,        0x00AD)
CODE_MAP_ANDROID(MediaTrackPrevious,    0x00A5)

CODE_MAP_WIN(Power,                     0xE05E)
CODE_MAP_MAC(Power,                     0x007F) // On 10.7 and 10.8 only
// CODE_MAP_X11(Power) // not available?
CODE_MAP_ANDROID(Power,                 0x0074)

// CODE_MAP_WIN(Sleep) // not available?
// CODE_MAP_X11(Sleep) // not available?
CODE_MAP_ANDROID(Sleep,                 0x008E)

CODE_MAP_WIN(VolumeDown,                0xE02E)
CODE_MAP_MAC(VolumeDown,                kVK_VolumeDown) // not available?
CODE_MAP_X11(VolumeDown,                0x007A)
CODE_MAP_ANDROID(VolumeDown,            0x0072)

CODE_MAP_WIN(VolumeMute,                0xE020)
CODE_MAP_MAC(VolumeMute,                kVK_Mute) // not available?
CODE_MAP_X11(VolumeMute,                0x0079)
CODE_MAP_ANDROID(VolumeMute,            0x0071)

CODE_MAP_WIN(VolumeUp,                  0xE030)
CODE_MAP_MAC(VolumeUp,                  kVK_VolumeUp) // not available?
CODE_MAP_X11(VolumeUp,                  0x007B)
CODE_MAP_ANDROID(VolumeUp,              0x0073) // side of body, not on keyboard

// CODE_MAP_WIN(WakeUp) // not available?
CODE_MAP_X11(WakeUp,                    0x0097)
CODE_MAP_ANDROID(WakeUp,                0x008F)

// Legacy editing keys
CODE_MAP_X11(Again,                     0x0089) // Again key on Sun keyboard
CODE_MAP_ANDROID(Again,                 0x0081) // Again key on Sun keyboard

CODE_MAP_X11(Copy,                      0x008D) // Copy key on Sun keyboard
CODE_MAP_ANDROID(Copy,                  0x0085) // Copy key on Sun keyboard

CODE_MAP_X11(Cut,                       0x0091) // Cut key on Sun keyboard
CODE_MAP_ANDROID(Cut,                   0x0089) // Cut key on Sun keyboard

CODE_MAP_X11(Find,                      0x0090) // Find key on Sun keyboard
CODE_MAP_ANDROID(Find,                  0x0088) // Find key on Sun keyboard

CODE_MAP_X11(Open,                      0x008E) // Open key on Sun keyboard
CODE_MAP_ANDROID(Open,                  0x0086) // Open key on Sun keyboard

CODE_MAP_X11(Paste,                     0x008F) // Paste key on Sun keyboard
CODE_MAP_ANDROID(Paste,                 0x0087) // Paste key on Sun keyboard

CODE_MAP_X11(Props,                     0x008A) // Props key on Sun keyboard
CODE_MAP_ANDROID(Props,                 0x0082) // Props key on Sun keyboard

CODE_MAP_X11(Select,                    0x008C) // Front key on Sun keyboard
CODE_MAP_ANDROID(Select,                0x0084) // Front key on Sun keyboard

CODE_MAP_X11(Undo,                      0x008B) // Undo key on Sun keyboard
CODE_MAP_ANDROID(Undo,                  0x0083) // Undo key on Sun keyboard

#undef CODE_MAP_WIN
#undef CODE_MAP_MAC
#undef CODE_MAP_X11
#undef CODE_MAP_ANDROID
