/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the spoofed keycodes of en-US for fingerprinting resistance.
 * When privacy.resistFingerprinting is active, we spoof the user's keyboard
 * layout according to the language of the document.
 *
 * Use CONTROL to define the control key.
 *   CONTROL(keyNameIndex, codeNameIndex, keyCode)
 *   @param keyNameIndex  The keyNameIndex of this control key.
 *                        See KeyNameList.h for details.
 *   @param codeNameIndex The codeNameIndex of this contorl key.
 *                        See PhysicalKeyCodeNameList.h for details.
 *   @param keyCode       The keyCode of this control key.
 *                        See nsIDOMKeyEvent.idl for details.
 *
 * Use KEY to define the key with its modifier states. The key will be spoofed
 * with given modifier states.
 *   KEY(keyString, codeNameIndex, keyCode, modifiers)
 *   @param keyString     The key string of this key.
 *   @param codeNameIndex The codeNameIndex of this key.
 *                        See PhysicalKeyCodeNameList.h for details.
 *   @param keyCode       The keyCode of this key.
 *                        See nsIDOMKeyEvent.idl for details.
 *   @param modifiers     The spoofing modifier states for this key.
 *                        See BasicEvents.h for details.
 */

/**
 * Spoofed keycodes for English content (US English keyboard layout).
 */

CONTROL(Alt,         AltLeft,     nsIDOMKeyEvent::DOM_VK_ALT)
CONTROL(ArrowDown,   ArrowDown,   nsIDOMKeyEvent::DOM_VK_DOWN)
CONTROL(ArrowLeft,   ArrowLeft,   nsIDOMKeyEvent::DOM_VK_LEFT)
CONTROL(ArrowRight,  ArrowRight,  nsIDOMKeyEvent::DOM_VK_RIGHT)
CONTROL(ArrowUp,     ArrowUp,     nsIDOMKeyEvent::DOM_VK_UP)
CONTROL(Backspace,   Backspace,   nsIDOMKeyEvent::DOM_VK_BACK_SPACE)
CONTROL(CapsLock,    CapsLock,    nsIDOMKeyEvent::DOM_VK_CAPS_LOCK)
// Leaving "ContextMenu" key unimplemented; not every english keyboard has this.
// For example, MACOS doesn't have this.
CONTROL(Control,     ControlLeft, nsIDOMKeyEvent::DOM_VK_CONTROL)
CONTROL(Delete,      Delete,      nsIDOMKeyEvent::DOM_VK_DELETE)
CONTROL(End,         End,         nsIDOMKeyEvent::DOM_VK_END)
CONTROL(Enter,       Enter,       nsIDOMKeyEvent::DOM_VK_RETURN)
CONTROL(Escape,      Escape,      nsIDOMKeyEvent::DOM_VK_ESCAPE)
// Leaving "Help" key unimplemented; it only appears in some keyboard in Linux.
CONTROL(Home,        Home,        nsIDOMKeyEvent::DOM_VK_HOME)
CONTROL(Insert,      Insert,      nsIDOMKeyEvent::DOM_VK_INSERT)
CONTROL(Meta,        OSLeft,      nsIDOMKeyEvent::DOM_VK_WIN)
CONTROL(OS,          OSLeft,      nsIDOMKeyEvent::DOM_VK_WIN)
CONTROL(PageDown,    PageDown,    nsIDOMKeyEvent::DOM_VK_PAGE_DOWN)
CONTROL(PageUp,      PageUp,      nsIDOMKeyEvent::DOM_VK_PAGE_UP)
// Leaving "Pause", "PrintScreen" and "ScrollLock" keys unimplemented; they are
// non-MACOS only.
CONTROL(Shift,       ShiftLeft,   nsIDOMKeyEvent::DOM_VK_SHIFT)
CONTROL(Tab,         Tab,         nsIDOMKeyEvent::DOM_VK_TAB)
CONTROL(F1,          F1,          nsIDOMKeyEvent::DOM_VK_F1)
CONTROL(F2,          F2,          nsIDOMKeyEvent::DOM_VK_F2)
CONTROL(F3,          F3,          nsIDOMKeyEvent::DOM_VK_F3)
CONTROL(F4,          F4,          nsIDOMKeyEvent::DOM_VK_F4)
CONTROL(F5,          F5,          nsIDOMKeyEvent::DOM_VK_F5)
CONTROL(F6,          F6,          nsIDOMKeyEvent::DOM_VK_F6)
CONTROL(F7,          F7,          nsIDOMKeyEvent::DOM_VK_F7)
CONTROL(F8,          F8,          nsIDOMKeyEvent::DOM_VK_F8)
CONTROL(F9,          F9,          nsIDOMKeyEvent::DOM_VK_F9)
CONTROL(F10,         F10,         nsIDOMKeyEvent::DOM_VK_F10)
CONTROL(F11,         F11,         nsIDOMKeyEvent::DOM_VK_F11)
CONTROL(F12,         F12,         nsIDOMKeyEvent::DOM_VK_F12)
// Leaving "F13" to "F35" key unimplemented; they are some how platform dependent.
// "F13" to "F19" are on MAC's full keyboard but may not exist on usual keyboard.
// "F20" to "F24" are only available on Windows and Linux.
// "F25" to "F35" are Linux only.
// Leaving "Clear" key unimplemented; it's inconsistent between platforms.
KEY(" ",  Space,        nsIDOMKeyEvent::DOM_VK_SPACE,  MODIFIER_NONE)
KEY(",",  Comma,        nsIDOMKeyEvent::DOM_VK_COMMA, MODIFIER_NONE)
KEY("<",  Comma,        nsIDOMKeyEvent::DOM_VK_COMMA, MODIFIER_SHIFT)
KEY(".",  Period,       nsIDOMKeyEvent::DOM_VK_PERIOD, MODIFIER_NONE)
KEY(">",  Period,       nsIDOMKeyEvent::DOM_VK_PERIOD, MODIFIER_SHIFT)
KEY("/",  Slash,        nsIDOMKeyEvent::DOM_VK_SLASH, MODIFIER_NONE)
KEY("?",  Slash,        nsIDOMKeyEvent::DOM_VK_SLASH, MODIFIER_SHIFT)
KEY(";",  Semicolon,    nsIDOMKeyEvent::DOM_VK_SEMICOLON,  MODIFIER_NONE)
KEY(":",  Semicolon,    nsIDOMKeyEvent::DOM_VK_SEMICOLON,  MODIFIER_SHIFT)
KEY("'",  Quote,        nsIDOMKeyEvent::DOM_VK_QUOTE, MODIFIER_NONE)
KEY("\"", Quote,        nsIDOMKeyEvent::DOM_VK_QUOTE, MODIFIER_SHIFT)
KEY("[",  BracketLeft,  nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET, MODIFIER_NONE)
KEY("{",  BracketLeft,  nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET, MODIFIER_SHIFT)
KEY("]",  BracketRight, nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET, MODIFIER_NONE)
KEY("}",  BracketRight, nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET, MODIFIER_SHIFT)
KEY("`",  Backquote,    nsIDOMKeyEvent::DOM_VK_BACK_QUOTE, MODIFIER_NONE)
KEY("~",  Backquote,    nsIDOMKeyEvent::DOM_VK_BACK_QUOTE, MODIFIER_SHIFT)
KEY("\\", Backslash,    nsIDOMKeyEvent::DOM_VK_BACK_SLASH, MODIFIER_NONE)
KEY("|",  Backslash,    nsIDOMKeyEvent::DOM_VK_BACK_SLASH, MODIFIER_SHIFT)
KEY("-",  Minus,        nsIDOMKeyEvent::DOM_VK_HYPHEN_MINUS, MODIFIER_NONE)
KEY("_",  Minus,        nsIDOMKeyEvent::DOM_VK_HYPHEN_MINUS, MODIFIER_SHIFT)
KEY("=",  Equal,        nsIDOMKeyEvent::DOM_VK_EQUALS, MODIFIER_NONE)
KEY("+",  Equal,        nsIDOMKeyEvent::DOM_VK_EQUALS, MODIFIER_SHIFT)
KEY("A",  KeyA,         nsIDOMKeyEvent::DOM_VK_A, MODIFIER_SHIFT)
KEY("B",  KeyB,         nsIDOMKeyEvent::DOM_VK_B, MODIFIER_SHIFT)
KEY("C",  KeyC,         nsIDOMKeyEvent::DOM_VK_C, MODIFIER_SHIFT)
KEY("D",  KeyD,         nsIDOMKeyEvent::DOM_VK_D, MODIFIER_SHIFT)
KEY("E",  KeyE,         nsIDOMKeyEvent::DOM_VK_E, MODIFIER_SHIFT)
KEY("F",  KeyF,         nsIDOMKeyEvent::DOM_VK_F, MODIFIER_SHIFT)
KEY("G",  KeyG,         nsIDOMKeyEvent::DOM_VK_G, MODIFIER_SHIFT)
KEY("H",  KeyH,         nsIDOMKeyEvent::DOM_VK_H, MODIFIER_SHIFT)
KEY("I",  KeyI,         nsIDOMKeyEvent::DOM_VK_I, MODIFIER_SHIFT)
KEY("J",  KeyJ,         nsIDOMKeyEvent::DOM_VK_J, MODIFIER_SHIFT)
KEY("K",  KeyK,         nsIDOMKeyEvent::DOM_VK_K, MODIFIER_SHIFT)
KEY("L",  KeyL,         nsIDOMKeyEvent::DOM_VK_L, MODIFIER_SHIFT)
KEY("M",  KeyM,         nsIDOMKeyEvent::DOM_VK_M, MODIFIER_SHIFT)
KEY("N",  KeyN,         nsIDOMKeyEvent::DOM_VK_N, MODIFIER_SHIFT)
KEY("O",  KeyO,         nsIDOMKeyEvent::DOM_VK_O, MODIFIER_SHIFT)
KEY("P",  KeyP,         nsIDOMKeyEvent::DOM_VK_P, MODIFIER_SHIFT)
KEY("Q",  KeyQ,         nsIDOMKeyEvent::DOM_VK_Q, MODIFIER_SHIFT)
KEY("R",  KeyR,         nsIDOMKeyEvent::DOM_VK_R, MODIFIER_SHIFT)
KEY("S",  KeyS,         nsIDOMKeyEvent::DOM_VK_S, MODIFIER_SHIFT)
KEY("T",  KeyT,         nsIDOMKeyEvent::DOM_VK_T, MODIFIER_SHIFT)
KEY("U",  KeyU,         nsIDOMKeyEvent::DOM_VK_U, MODIFIER_SHIFT)
KEY("V",  KeyV,         nsIDOMKeyEvent::DOM_VK_V, MODIFIER_SHIFT)
KEY("W",  KeyW,         nsIDOMKeyEvent::DOM_VK_W, MODIFIER_SHIFT)
KEY("X",  KeyX,         nsIDOMKeyEvent::DOM_VK_X, MODIFIER_SHIFT)
KEY("Y",  KeyY,         nsIDOMKeyEvent::DOM_VK_Y, MODIFIER_SHIFT)
KEY("Z",  KeyZ,         nsIDOMKeyEvent::DOM_VK_Z, MODIFIER_SHIFT)
KEY("a",  KeyA,         nsIDOMKeyEvent::DOM_VK_A, MODIFIER_NONE)
KEY("b",  KeyB,         nsIDOMKeyEvent::DOM_VK_B, MODIFIER_NONE)
KEY("c",  KeyC,         nsIDOMKeyEvent::DOM_VK_C, MODIFIER_NONE)
KEY("d",  KeyD,         nsIDOMKeyEvent::DOM_VK_D, MODIFIER_NONE)
KEY("e",  KeyE,         nsIDOMKeyEvent::DOM_VK_E, MODIFIER_NONE)
KEY("f",  KeyF,         nsIDOMKeyEvent::DOM_VK_F, MODIFIER_NONE)
KEY("g",  KeyG,         nsIDOMKeyEvent::DOM_VK_G, MODIFIER_NONE)
KEY("h",  KeyH,         nsIDOMKeyEvent::DOM_VK_H, MODIFIER_NONE)
KEY("i",  KeyI,         nsIDOMKeyEvent::DOM_VK_I, MODIFIER_NONE)
KEY("j",  KeyJ,         nsIDOMKeyEvent::DOM_VK_J, MODIFIER_NONE)
KEY("k",  KeyK,         nsIDOMKeyEvent::DOM_VK_K, MODIFIER_NONE)
KEY("l",  KeyL,         nsIDOMKeyEvent::DOM_VK_L, MODIFIER_NONE)
KEY("m",  KeyM,         nsIDOMKeyEvent::DOM_VK_M, MODIFIER_NONE)
KEY("n",  KeyN,         nsIDOMKeyEvent::DOM_VK_N, MODIFIER_NONE)
KEY("o",  KeyO,         nsIDOMKeyEvent::DOM_VK_O, MODIFIER_NONE)
KEY("p",  KeyP,         nsIDOMKeyEvent::DOM_VK_P, MODIFIER_NONE)
KEY("q",  KeyQ,         nsIDOMKeyEvent::DOM_VK_Q, MODIFIER_NONE)
KEY("r",  KeyR,         nsIDOMKeyEvent::DOM_VK_R, MODIFIER_NONE)
KEY("s",  KeyS,         nsIDOMKeyEvent::DOM_VK_S, MODIFIER_NONE)
KEY("t",  KeyT,         nsIDOMKeyEvent::DOM_VK_T, MODIFIER_NONE)
KEY("u",  KeyU,         nsIDOMKeyEvent::DOM_VK_U, MODIFIER_NONE)
KEY("v",  KeyV,         nsIDOMKeyEvent::DOM_VK_V, MODIFIER_NONE)
KEY("w",  KeyW,         nsIDOMKeyEvent::DOM_VK_W, MODIFIER_NONE)
KEY("x",  KeyX,         nsIDOMKeyEvent::DOM_VK_X, MODIFIER_NONE)
KEY("y",  KeyY,         nsIDOMKeyEvent::DOM_VK_Y, MODIFIER_NONE)
KEY("z",  KeyZ,         nsIDOMKeyEvent::DOM_VK_Z, MODIFIER_NONE)
KEY("0",  Digit0,       nsIDOMKeyEvent::DOM_VK_0, MODIFIER_NONE)
KEY("1",  Digit1,       nsIDOMKeyEvent::DOM_VK_1, MODIFIER_NONE)
KEY("2",  Digit2,       nsIDOMKeyEvent::DOM_VK_2, MODIFIER_NONE)
KEY("3",  Digit3,       nsIDOMKeyEvent::DOM_VK_3, MODIFIER_NONE)
KEY("4",  Digit4,       nsIDOMKeyEvent::DOM_VK_4, MODIFIER_NONE)
KEY("5",  Digit5,       nsIDOMKeyEvent::DOM_VK_5, MODIFIER_NONE)
KEY("6",  Digit6,       nsIDOMKeyEvent::DOM_VK_6, MODIFIER_NONE)
KEY("7",  Digit7,       nsIDOMKeyEvent::DOM_VK_7, MODIFIER_NONE)
KEY("8",  Digit8,       nsIDOMKeyEvent::DOM_VK_8, MODIFIER_NONE)
KEY("9",  Digit9,       nsIDOMKeyEvent::DOM_VK_9, MODIFIER_NONE)
KEY(")",  Digit0,       nsIDOMKeyEvent::DOM_VK_0, MODIFIER_SHIFT)
KEY("!",  Digit1,       nsIDOMKeyEvent::DOM_VK_1, MODIFIER_SHIFT)
KEY("@",  Digit2,       nsIDOMKeyEvent::DOM_VK_2, MODIFIER_SHIFT)
KEY("#",  Digit3,       nsIDOMKeyEvent::DOM_VK_3, MODIFIER_SHIFT)
KEY("$",  Digit4,       nsIDOMKeyEvent::DOM_VK_4, MODIFIER_SHIFT)
KEY("%",  Digit5,       nsIDOMKeyEvent::DOM_VK_5, MODIFIER_SHIFT)
KEY("^",  Digit6,       nsIDOMKeyEvent::DOM_VK_6, MODIFIER_SHIFT)
KEY("&",  Digit7,       nsIDOMKeyEvent::DOM_VK_7, MODIFIER_SHIFT)
KEY("*",  Digit8,       nsIDOMKeyEvent::DOM_VK_8, MODIFIER_SHIFT)
KEY("(",  Digit9,       nsIDOMKeyEvent::DOM_VK_9, MODIFIER_SHIFT)
