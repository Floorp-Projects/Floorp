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

// Windows
#define KEY_MAP_WIN(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_JPN(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_KOR(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_OTH(aCPPKeyName, aNativeKey)
#define KEY_MAP_WIN_CMD(aCPPKeyName, aAppCommand)
// Mac OS X
#define KEY_MAP_COCOA(aCPPKeyName, aNativeKey)
// GTK
#define KEY_MAP_GTK(aCPPKeyName, aNativeKey)
// Qt
#define KEY_MAP_QT(aCPPKeyName, aNativeKey)
// Android and B2G
#define KEY_MAP_ANDROID(aCPPKeyName, aNativeKey)
// Only for Android
#define KEY_MAP_ANDROID_EXCEPT_B2G(aCPPKeyName, aNativeKey)
// Only for B2G
#define KEY_MAP_B2G(aCPPKeyName, aNativeKey)

#if defined(XP_WIN)
#if defined(NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX)
// KEY_MAP_WIN() defines the mapping not depending on keyboard layout.
#undef KEY_MAP_WIN
#define KEY_MAP_WIN(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX)
// KEY_MAP_WIN_JPN() defines the mapping which is valid only with Japanese
// keyboard layout.
#undef KEY_MAP_WIN_JPN
#define KEY_MAP_WIN_JPN(aCPPKeyName, aNativeKey) \
  NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                               KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX)
// KEY_MAP_WIN_KOR() defines the mapping which is valid only with Korean
// keyboard layout.
#undef KEY_MAP_WIN_KOR
#define KEY_MAP_WIN_KOR(aCPPKeyName, aNativeKey) \
  NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                             KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX)
// KEY_MAP_WIN_OTH() defines the mapping which is valid with neither
// Japanese keyboard layout nor Korean keyboard layout.
#undef KEY_MAP_WIN_OTH
#define KEY_MAP_WIN_OTH(aCPPKeyName, aNativeKey) \
  NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, \
                                            KEY_NAME_INDEX_##aCPPKeyName)
#elif defined(NS_APPCOMMAND_TO_DOM_KEY_NAME_INDEX)
// KEY_MAP_WIN_CMD() defines the mapping from APPCOMMAND_* of WM_APPCOMMAND.
#undef KEY_MAP_WIN_CMD
#define KEY_MAP_WIN_CMD(aCPPKeyName, aAppCommand) \
  NS_APPCOMMAND_TO_DOM_KEY_NAME_INDEX(aAppCommand, \
                                      KEY_NAME_INDEX_##aCPPKeyName)
#else
#error Any NS_*_TO_DOM_KEY_NAME_INDEX() is not defined.
#endif // #if defined(NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX) ...
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
#ifndef MOZ_B2G
#undef KEY_MAP_ANDROID_EXCEPT_B2G
#define KEY_MAP_ANDROID_EXCEPT_B2G(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#else // #ifndef MOZ_B2G
#undef KEY_MAP_B2G
#define KEY_MAP_B2G(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#endif  // #ifndef MOZ_B2G #else
#endif

/******************************************************************************
 * Modifier Keys
 ******************************************************************************/
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

// Meta
KEY_MAP_COCOA   (Meta, kVK_Command)
KEY_MAP_COCOA   (Meta, kVK_RightCommand)
KEY_MAP_GTK     (Meta, GDK_Meta_L)
KEY_MAP_GTK     (Meta, GDK_Meta_R)
KEY_MAP_QT      (Meta, Qt::Key_Meta)
KEY_MAP_ANDROID (Meta, AKEYCODE_META_LEFT)
KEY_MAP_ANDROID (Meta, AKEYCODE_META_RIGHT)

// NumLock
KEY_MAP_WIN     (NumLock, VK_NUMLOCK)
KEY_MAP_GTK     (NumLock, GDK_Num_Lock)
KEY_MAP_QT      (NumLock, Qt::Key_NumLock)
KEY_MAP_ANDROID (NumLock, AKEYCODE_NUM_LOCK)

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

// ScrollLock
KEY_MAP_WIN     (ScrollLock, VK_SCROLL)
KEY_MAP_GTK     (ScrollLock, GDK_Scroll_Lock)
KEY_MAP_QT      (ScrollLock, Qt::Key_ScrollLock)
KEY_MAP_ANDROID (ScrollLock, AKEYCODE_SCROLL_LOCK)

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

// Symbol
KEY_MAP_ANDROID (Symbol, AKEYCODE_SYM)

/******************************************************************************
 * Whitespace Keys
 ******************************************************************************/
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

// Tab
KEY_MAP_WIN     (Tab, VK_TAB)
KEY_MAP_COCOA   (Tab, kVK_Tab)
KEY_MAP_GTK     (Tab, GDK_Tab)
KEY_MAP_GTK     (Tab, GDK_KP_Tab)
KEY_MAP_QT      (Tab, Qt::Key_Tab)
KEY_MAP_ANDROID (Tab, AKEYCODE_TAB)

/******************************************************************************
 * Navigation Keys
 ******************************************************************************/
// ArrowDown
KEY_MAP_WIN     (ArrowDown, VK_DOWN)
KEY_MAP_COCOA   (ArrowDown, kVK_DownArrow)
KEY_MAP_GTK     (ArrowDown, GDK_Down)
KEY_MAP_GTK     (ArrowDown, GDK_KP_Down)
KEY_MAP_QT      (ArrowDown, Qt::Key_Down)
KEY_MAP_ANDROID (ArrowDown, AKEYCODE_DPAD_DOWN)

// ArrowLeft
KEY_MAP_WIN     (ArrowLeft, VK_LEFT)
KEY_MAP_COCOA   (ArrowLeft, kVK_LeftArrow)
KEY_MAP_GTK     (ArrowLeft, GDK_Left)
KEY_MAP_GTK     (ArrowLeft, GDK_KP_Left)
KEY_MAP_QT      (ArrowLeft, Qt::Key_Left)
KEY_MAP_ANDROID (ArrowLeft, AKEYCODE_DPAD_LEFT)

// ArrowRight
KEY_MAP_WIN     (ArrowRight, VK_RIGHT)
KEY_MAP_COCOA   (ArrowRight, kVK_RightArrow)
KEY_MAP_GTK     (ArrowRight, GDK_Right)
KEY_MAP_GTK     (ArrowRight, GDK_KP_Right)
KEY_MAP_QT      (ArrowRight, Qt::Key_Right)
KEY_MAP_ANDROID (ArrowRight, AKEYCODE_DPAD_RIGHT)

// ArrowUp
KEY_MAP_WIN     (ArrowUp, VK_UP)
KEY_MAP_COCOA   (ArrowUp, kVK_UpArrow)
KEY_MAP_GTK     (ArrowUp, GDK_Up)
KEY_MAP_GTK     (ArrowUp, GDK_KP_Up)
KEY_MAP_QT      (ArrowUp, Qt::Key_Up)
KEY_MAP_ANDROID (ArrowUp, AKEYCODE_DPAD_UP)

// End
KEY_MAP_WIN     (End, VK_END)
KEY_MAP_COCOA   (End, kVK_End)
KEY_MAP_GTK     (End, GDK_End)
KEY_MAP_GTK     (End, GDK_KP_End)
KEY_MAP_QT      (End, Qt::Key_End)
KEY_MAP_ANDROID (End, AKEYCODE_MOVE_END)

// Home
KEY_MAP_WIN     (Home, VK_HOME)
KEY_MAP_COCOA   (Home, kVK_Home)
KEY_MAP_GTK     (Home, GDK_Home)
KEY_MAP_GTK     (Home, GDK_KP_Home)
KEY_MAP_QT      (Home, Qt::Key_Home)
KEY_MAP_ANDROID (Home, AKEYCODE_MOVE_HOME)

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

/******************************************************************************
 * Editing Keys
 ******************************************************************************/
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
KEY_MAP_WIN_CMD (Copy, APPCOMMAND_COPY)
KEY_MAP_GTK     (Copy, GDK_Copy)
KEY_MAP_QT      (Copy, Qt::Key_Copy)

// CrSel
KEY_MAP_WIN     (CrSel, VK_CRSEL)
KEY_MAP_GTK     (CrSel, GDK_3270_CursorSelect) // legacy IBM keyboard layout

// Cut
KEY_MAP_WIN_CMD (Cut, APPCOMMAND_CUT)
KEY_MAP_GTK     (Cut, GDK_Cut)
KEY_MAP_QT      (Cut, Qt::Key_Cut)

// Delete
KEY_MAP_WIN     (Delete, VK_DELETE)
KEY_MAP_COCOA   (Delete, kVK_PC_Delete)
KEY_MAP_GTK     (Delete, GDK_Delete)
KEY_MAP_GTK     (Delete, GDK_KP_Delete)
KEY_MAP_QT      (Delete, Qt::Key_Delete)
KEY_MAP_ANDROID (Delete, AKEYCODE_FORWARD_DEL)

// EraseEof
KEY_MAP_WIN     (EraseEof, VK_EREOF)
KEY_MAP_GTK     (EraseEof, GDK_3270_EraseEOF) // legacy IBM keyboard layout

// ExSel
KEY_MAP_WIN     (ExSel, VK_EXSEL)
KEY_MAP_GTK     (ExSel, GDK_3270_ExSelect) // legacy IBM keyboard layout

// Insert
KEY_MAP_WIN     (Insert, VK_INSERT)
KEY_MAP_GTK     (Insert, GDK_Insert)
KEY_MAP_GTK     (Insert, GDK_KP_Insert)
KEY_MAP_QT      (Insert, Qt::Key_Insert)
KEY_MAP_ANDROID (Insert, AKEYCODE_INSERT)

// Paste
KEY_MAP_WIN_CMD (Paste, APPCOMMAND_PASTE)
KEY_MAP_GTK     (Paste, GDK_Paste)
KEY_MAP_QT      (Paste, Qt::Key_Paste)

// Redo
KEY_MAP_WIN_CMD (Redo, APPCOMMAND_REDO)
KEY_MAP_GTK     (Redo, GDK_Redo)

// Undo
KEY_MAP_WIN_CMD (Undo, APPCOMMAND_UNDO)
KEY_MAP_GTK     (Undo, GDK_Undo)

/******************************************************************************
 * UI Keys
 ******************************************************************************/
// Accept
KEY_MAP_WIN     (Accept, VK_ACCEPT)
KEY_MAP_ANDROID (Accept, AKEYCODE_DPAD_CENTER)

// Attn
KEY_MAP_WIN_OTH (Attn, VK_ATTN) // not valid with Japanese keyboard layout
KEY_MAP_GTK     (Attn, GDK_3270_Attn) // legacy IBM keyboard layout

// Cancel
KEY_MAP_WIN     (Cancel, VK_CANCEL)
KEY_MAP_GTK     (Cancel, GDK_Cancel)
KEY_MAP_QT      (Cancel, Qt::Key_Cancel)

// ContextMenu
KEY_MAP_WIN     (ContextMenu, VK_APPS)
KEY_MAP_COCOA   (ContextMenu, kVK_PC_ContextMenu)
KEY_MAP_GTK     (ContextMenu, GDK_Menu)
KEY_MAP_QT      (ContextMenu, Qt::Key_Menu)
KEY_MAP_ANDROID (ContextMenu, AKEYCODE_MENU)

// Escape
KEY_MAP_WIN     (Escape, VK_ESCAPE)
KEY_MAP_COCOA   (Escape, kVK_Escape)
KEY_MAP_GTK     (Escape, GDK_Escape)
KEY_MAP_QT      (Escape, Qt::Key_Escape)
KEY_MAP_ANDROID (Escape, AKEYCODE_ESCAPE)

// Execute
KEY_MAP_WIN     (Execute, VK_EXECUTE)
KEY_MAP_GTK     (Execute, GDK_Execute)
KEY_MAP_QT      (Execute, Qt::Key_Execute)

// Find
KEY_MAP_WIN_CMD (Find, APPCOMMAND_FIND)
KEY_MAP_GTK     (Find, GDK_Find)

// Help
KEY_MAP_WIN     (Help, VK_HELP)
KEY_MAP_WIN_CMD (Help, APPCOMMAND_HELP)
KEY_MAP_COCOA   (Help, kVK_Help)
KEY_MAP_GTK     (Help, GDK_Help)
KEY_MAP_QT      (Help, Qt::Key_Help)
KEY_MAP_ANDROID (Help, AKEYCODE_ASSIST)

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

// Select
KEY_MAP_WIN     (Select, VK_SELECT)
KEY_MAP_GTK     (Select, GDK_Select)

// ZoomIn
KEY_MAP_GTK     (ZoomIn, GDK_ZoomIn)
KEY_MAP_QT      (ZoomIn, Qt::Key_ZoomIn)
KEY_MAP_ANDROID (ZoomIn, AKEYCODE_ZOOM_IN)

// ZoomOut
KEY_MAP_GTK     (ZoomOut, GDK_ZoomOut)
KEY_MAP_QT      (ZoomOut, Qt::Key_ZoomOut)
KEY_MAP_ANDROID (ZoomOut, AKEYCODE_ZOOM_OUT)

/******************************************************************************
 * Device Keys
 ******************************************************************************/
// BrightnessDown
KEY_MAP_GTK     (BrightnessDown, GDK_MonBrightnessDown)
KEY_MAP_QT      (BrightnessDown, Qt::Key_MonBrightnessDown)

// BrightnessUp
KEY_MAP_GTK     (BrightnessUp, GDK_MonBrightnessUp)
KEY_MAP_QT      (BrightnessUp, Qt::Key_MonBrightnessUp)

// Eject
KEY_MAP_GTK     (Eject, GDK_Eject)
KEY_MAP_QT      (Eject, Qt::Key_Eject)
KEY_MAP_ANDROID (Eject, AKEYCODE_MEDIA_EJECT)

// LogOff
KEY_MAP_GTK     (LogOff, GDK_LogOff)
KEY_MAP_QT      (LogOff, Qt::Key_LogOff)

// Power
KEY_MAP_ANDROID (Power, AKEYCODE_POWER)

// PowerOff
KEY_MAP_GTK     (PowerOff, GDK_PowerDown)
KEY_MAP_GTK     (PowerOff, GDK_PowerOff)
KEY_MAP_QT      (PowerOff, Qt::Key_PowerDown)
KEY_MAP_QT      (PowerOff, Qt::Key_PowerOff)

// PrintScreen
KEY_MAP_WIN     (PrintScreen, VK_SNAPSHOT)
KEY_MAP_GTK     (PrintScreen, GDK_3270_PrintScreen)
KEY_MAP_GTK     (PrintScreen, GDK_Print)
KEY_MAP_GTK     (PrintScreen, GDK_Sys_Req)
KEY_MAP_QT      (PrintScreen, Qt::Key_Print)
KEY_MAP_QT      (PrintScreen, Qt::Key_SysReq)
KEY_MAP_ANDROID (PrintScreen, AKEYCODE_SYSRQ)

// Hibernate
KEY_MAP_GTK     (Hibernate, GDK_Hibernate)
KEY_MAP_QT      (Hibernate, Qt::Key_Hibernate)

// Standby
KEY_MAP_WIN     (Standby, VK_SLEEP)
KEY_MAP_GTK     (Standby, GDK_Standby)
KEY_MAP_GTK     (Standby, GDK_Suspend)
KEY_MAP_GTK     (Standby, GDK_Sleep)
KEY_MAP_QT      (Standby, Qt::Key_Standby)
KEY_MAP_QT      (Standby, Qt::Key_Suspend)
KEY_MAP_QT      (Standby, Qt::Key_Sleep)

// WakeUp
KEY_MAP_GTK     (WakeUp, GDK_WakeUp)
KEY_MAP_QT      (WakeUp, Qt::Key_WakeUp)

/******************************************************************************
 * IME and Composition Keys
 ******************************************************************************/
// AllCandidates
KEY_MAP_GTK     (AllCandidates, GDK_MultipleCandidate) // OADG 109, Zen Koho
KEY_MAP_QT      (AllCandidates, Qt::Key_MultipleCandidate)

// Alphanumeric
KEY_MAP_WIN_JPN (Alphanumeric, VK_OEM_ATTN)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_Shift)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_toggle)
KEY_MAP_QT      (Alphanumeric, Qt::Key_Eisu_Shift)
KEY_MAP_QT      (Alphanumeric, Qt::Key_Eisu_toggle)

// CodeInput
KEY_MAP_GTK     (CodeInput, GDK_Codeinput) // OADG 109, Kanji Bangou
KEY_MAP_QT      (CodeInput, Qt::Key_Codeinput)

// Compose
KEY_MAP_GTK     (Compose, GDK_Multi_key) // "Multi Key" is "Compose key" on X
KEY_MAP_QT      (Compose, Qt::Key_Multi_key)

// Convert
KEY_MAP_WIN     (Convert, VK_CONVERT)
KEY_MAP_GTK     (Convert, GDK_Henkan)
KEY_MAP_QT      (Convert, Qt::Key_Henkan)
KEY_MAP_ANDROID (Convert, AKEYCODE_HENKAN)

// Dead
KEY_MAP_GTK     (Dead, GDK_dead_grave)
KEY_MAP_GTK     (Dead, GDK_dead_acute)
KEY_MAP_GTK     (Dead, GDK_dead_circumflex)
KEY_MAP_GTK     (Dead, GDK_dead_tilde) // Same as GDK_dead_perispomeni
KEY_MAP_GTK     (Dead, GDK_dead_macron)
KEY_MAP_GTK     (Dead, GDK_dead_breve)
KEY_MAP_GTK     (Dead, GDK_dead_abovedot)
KEY_MAP_GTK     (Dead, GDK_dead_diaeresis)
KEY_MAP_GTK     (Dead, GDK_dead_abovering)
KEY_MAP_GTK     (Dead, GDK_dead_doubleacute)
KEY_MAP_GTK     (Dead, GDK_dead_caron)
KEY_MAP_GTK     (Dead, GDK_dead_cedilla)
KEY_MAP_GTK     (Dead, GDK_dead_ogonek)
KEY_MAP_GTK     (Dead, GDK_dead_iota)
KEY_MAP_GTK     (Dead, GDK_dead_voiced_sound)
KEY_MAP_GTK     (Dead, GDK_dead_semivoiced_sound)
KEY_MAP_GTK     (Dead, GDK_dead_belowdot)
KEY_MAP_GTK     (Dead, GDK_dead_hook)
KEY_MAP_GTK     (Dead, GDK_dead_horn)
KEY_MAP_GTK     (Dead, GDK_dead_stroke)
KEY_MAP_GTK     (Dead, GDK_dead_abovecomma) // Same as GDK_dead_psili
KEY_MAP_GTK     (Dead, GDK_dead_abovereversedcomma) // Same as GDK_dead_dasia
KEY_MAP_GTK     (Dead, GDK_dead_doublegrave)
KEY_MAP_GTK     (Dead, GDK_dead_belowring)
KEY_MAP_GTK     (Dead, GDK_dead_belowmacron)
KEY_MAP_GTK     (Dead, GDK_dead_belowcircumflex)
KEY_MAP_GTK     (Dead, GDK_dead_belowtilde)
KEY_MAP_GTK     (Dead, GDK_dead_belowbreve)
KEY_MAP_GTK     (Dead, GDK_dead_belowdiaeresis)
KEY_MAP_GTK     (Dead, GDK_dead_invertedbreve)
KEY_MAP_GTK     (Dead, GDK_dead_belowcomma)
KEY_MAP_GTK     (Dead, GDK_dead_currency)
KEY_MAP_GTK     (Dead, GDK_dead_a)
KEY_MAP_GTK     (Dead, GDK_dead_A)
KEY_MAP_GTK     (Dead, GDK_dead_e)
KEY_MAP_GTK     (Dead, GDK_dead_E)
KEY_MAP_GTK     (Dead, GDK_dead_i)
KEY_MAP_GTK     (Dead, GDK_dead_I)
KEY_MAP_GTK     (Dead, GDK_dead_o)
KEY_MAP_GTK     (Dead, GDK_dead_O)
KEY_MAP_GTK     (Dead, GDK_dead_u)
KEY_MAP_GTK     (Dead, GDK_dead_U)
KEY_MAP_GTK     (Dead, GDK_dead_small_schwa)
KEY_MAP_GTK     (Dead, GDK_dead_capital_schwa)
KEY_MAP_GTK     (Dead, GDK_dead_greek)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Grave)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Acute)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Circumflex)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Tilde)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Macron)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Breve)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Abovedot)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Diaeresis)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Abovering)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Doubleacute)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Caron)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Cedilla)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Ogonek)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Iota)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Voiced_Sound)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Semivoiced_Sound)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Belowdot)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Hook)
KEY_MAP_QT      (Dead, Qt::Key_Dead_Horn)

// FinalMode
KEY_MAP_WIN     (FinalMode, VK_FINAL)

// GroupFirst
KEY_MAP_GTK     (GroupFirst, GDK_ISO_First_Group)

// GroupLast
KEY_MAP_GTK     (GroupLast, GDK_ISO_Last_Group)

// GroupNext
KEY_MAP_GTK     (GroupNext, GDK_ISO_Next_Group)
KEY_MAP_ANDROID (GroupNext, AKEYCODE_LANGUAGE_SWITCH)

// GroupPrevious
KEY_MAP_GTK     (GroupPrevious, GDK_ISO_Prev_Group)

// ModeChange
KEY_MAP_WIN     (ModeChange, VK_MODECHANGE)
KEY_MAP_ANDROID (ModeChange, AKEYCODE_SWITCH_CHARSET)

// NonConvert
KEY_MAP_WIN     (NonConvert, VK_NONCONVERT)
KEY_MAP_GTK     (NonConvert, GDK_Muhenkan)
KEY_MAP_QT      (NonConvert, Qt::Key_Muhenkan)
KEY_MAP_ANDROID (NonConvert, AKEYCODE_MUHENKAN)

// PreviousCandidate
KEY_MAP_GTK     (PreviousCandidate, GDK_PreviousCandidate) // OADG 109, Mae Koho
KEY_MAP_QT      (PreviousCandidate, Qt::Key_PreviousCandidate)

// SingleCandidate
KEY_MAP_GTK     (SingleCandidate, GDK_SingleCandidate)
KEY_MAP_QT      (SingleCandidate, Qt::Key_SingleCandidate)

/******************************************************************************
 * Keys specific to Korean keyboards
 ******************************************************************************/
// HangulMode
KEY_MAP_WIN_KOR (HangulMode, VK_HANGUL /* same as VK_KANA */)

// HanjaMode
KEY_MAP_WIN_KOR (HanjaMode, VK_HANJA /* same as VK_KANJI */)

// JunjaMode
KEY_MAP_WIN     (JunjaMode, VK_JUNJA)

/******************************************************************************
 * Keys specific to Japanese keyboards
 ******************************************************************************/
// Eisu
KEY_MAP_COCOA   (Eisu, kVK_JIS_Eisu)
KEY_MAP_ANDROID (Eisu, AKEYCODE_EISU)

// Hankaku
KEY_MAP_WIN_JPN (Hankaku, VK_OEM_AUTO)
KEY_MAP_GTK     (Hankaku, GDK_Hankaku)
KEY_MAP_QT      (Hankaku, Qt::Key_Hankaku)

// Hiragana
KEY_MAP_WIN_JPN (Hiragana, VK_OEM_COPY)
KEY_MAP_GTK     (Hiragana, GDK_Hiragana)
KEY_MAP_QT      (Hiragana, Qt::Key_Hiragana)

// HiraganaKatakana
KEY_MAP_GTK     (HiraganaKatakana, GDK_Hiragana_Katakana)
KEY_MAP_QT      (HiraganaKatakana, Qt::Key_Hiragana_Katakana)
KEY_MAP_ANDROID (HiraganaKatakana, AKEYCODE_KATAKANA_HIRAGANA)

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

// Romaji
KEY_MAP_WIN_JPN (Romaji, VK_OEM_BACKTAB)
KEY_MAP_GTK     (Romaji, GDK_Romaji)
KEY_MAP_QT      (Romaji, Qt::Key_Romaji)

// Zenkaku
KEY_MAP_WIN_JPN (Zenkaku, VK_OEM_ENLW)
KEY_MAP_GTK     (Zenkaku, GDK_Zenkaku)
KEY_MAP_QT      (Zenkaku, Qt::Key_Zenkaku)

// ZenkakuHankaku
KEY_MAP_GTK     (ZenkakuHankaku, GDK_Zenkaku_Hankaku)
KEY_MAP_QT      (ZenkakuHankaku, Qt::Key_Zenkaku_Hankaku)
KEY_MAP_ANDROID (ZenkakuHankaku, AKEYCODE_ZENKAKU_HANKAKU)

/******************************************************************************
 * General-Purpose Function Keys
 ******************************************************************************/
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

/******************************************************************************
 * Multimedia Keys
 ******************************************************************************/
// ChannelDown
KEY_MAP_WIN_CMD (ChannelDown, APPCOMMAND_MEDIA_CHANNEL_DOWN)
KEY_MAP_ANDROID (ChannelDown, AKEYCODE_CHANNEL_DOWN)

// ChannelUp
KEY_MAP_WIN_CMD (ChannelUp, APPCOMMAND_MEDIA_CHANNEL_UP)
KEY_MAP_ANDROID (ChannelUp, AKEYCODE_CHANNEL_UP)

// Close
// NOTE: This is not a key to close disk tray, this is a key to close document
//       or window.
KEY_MAP_WIN_CMD (Close, APPCOMMAND_CLOSE)
KEY_MAP_GTK     (Close, GDK_Close)
KEY_MAP_QT      (Close, Qt::Key_Close)

// MailForward
KEY_MAP_WIN_CMD (MailForward, APPCOMMAND_FORWARD_MAIL)
KEY_MAP_GTK     (MailForward, GDK_MailForward)
KEY_MAP_QT      (MailForward, Qt::Key_MailForward)

// MailReply
KEY_MAP_WIN_CMD (MailReply, APPCOMMAND_REPLY_TO_MAIL)
KEY_MAP_GTK     (MailReply, GDK_Reply)
KEY_MAP_QT      (MailReply, Qt::Key_Reply)

// MailSend
KEY_MAP_WIN_CMD (MailSend, APPCOMMAND_SEND_MAIL)
KEY_MAP_GTK     (MailSend, GDK_Send)
KEY_MAP_QT      (MailSend, Qt::Key_Send)

// MediaPause
KEY_MAP_WIN_CMD (MediaPause, APPCOMMAND_MEDIA_PAUSE)
KEY_MAP_GTK     (MediaPause, GDK_AudioPause)
KEY_MAP_QT      (MediaPause, Qt::Key_MediaPause)
KEY_MAP_ANDROID (MediaPause, AKEYCODE_MEDIA_PAUSE)

// MediaPlay
KEY_MAP_WIN_CMD (MediaPlay, APPCOMMAND_MEDIA_PLAY)
KEY_MAP_GTK     (MediaPlay, GDK_AudioPlay)
KEY_MAP_QT      (MediaPlay, Qt::Key_MediaPlay)
KEY_MAP_ANDROID (MediaPlay, AKEYCODE_MEDIA_PLAY)

// MediaPlayPause
KEY_MAP_WIN     (MediaPlayPause, VK_MEDIA_PLAY_PAUSE)
KEY_MAP_WIN_CMD (MediaPlayPause, APPCOMMAND_MEDIA_PLAY_PAUSE)
KEY_MAP_QT      (MediaPlayPause, Qt::Key_MediaTogglePlayPause)
KEY_MAP_ANDROID (MediaPlayPause, AKEYCODE_MEDIA_PLAY_PAUSE)

// MediaRecord
KEY_MAP_WIN_CMD (MediaRecord, APPCOMMAND_MEDIA_RECORD)
KEY_MAP_GTK     (MediaRecord, GDK_AudioRecord)
KEY_MAP_QT      (MediaRecord, Qt::Key_MediaRecord)
KEY_MAP_ANDROID (MediaRecord, AKEYCODE_MEDIA_RECORD)

// MediaRewind
KEY_MAP_WIN_CMD (MediaRewind, APPCOMMAND_MEDIA_REWIND)
KEY_MAP_GTK     (MediaRewind, GDK_AudioRewind)
KEY_MAP_QT      (MediaRewind, Qt::Key_AudioRewind)
KEY_MAP_ANDROID (MediaRewind, AKEYCODE_MEDIA_REWIND)

// MediaStop
KEY_MAP_WIN     (MediaStop, VK_MEDIA_STOP)
KEY_MAP_WIN_CMD (MediaStop, APPCOMMAND_MEDIA_STOP)
KEY_MAP_GTK     (MediaStop, GDK_AudioStop)
KEY_MAP_QT      (MediaStop, Qt::Key_MediaStop)
KEY_MAP_ANDROID (MediaStop, AKEYCODE_MEDIA_STOP)

// MediaTrackNext
KEY_MAP_WIN     (MediaTrackNext, VK_MEDIA_NEXT_TRACK)
KEY_MAP_WIN_CMD (MediaTrackNext, APPCOMMAND_MEDIA_NEXTTRACK)
KEY_MAP_GTK     (MediaTrackNext, GDK_AudioNext)
KEY_MAP_QT      (MediaTrackNext, Qt::Key_MediaNext)
KEY_MAP_ANDROID (MediaTrackNext, AKEYCODE_MEDIA_NEXT)

// MediaTrackPrevious
KEY_MAP_WIN     (MediaTrackPrevious, VK_MEDIA_PREV_TRACK)
KEY_MAP_WIN_CMD (MediaTrackPrevious, APPCOMMAND_MEDIA_PREVIOUSTRACK)
KEY_MAP_GTK     (MediaTrackPrevious, GDK_AudioPrev)
KEY_MAP_QT      (MediaTrackPrevious, Qt::Key_MediaPrevious)
KEY_MAP_ANDROID (MediaTrackPrevious, AKEYCODE_MEDIA_PREVIOUS)

// New
KEY_MAP_WIN_CMD (New, APPCOMMAND_NEW)
KEY_MAP_GTK     (New, GDK_New)

// Open
KEY_MAP_WIN_CMD (Open, APPCOMMAND_OPEN)
KEY_MAP_GTK     (Open, GDK_Open)

// Print
KEY_MAP_WIN_CMD (Print, APPCOMMAND_PRINT)
KEY_MAP_QT      (Print, Qt::Key_Printer)

// Save
KEY_MAP_WIN_CMD (Save, APPCOMMAND_SAVE)
KEY_MAP_GTK     (Save, GDK_Save)
KEY_MAP_QT      (Save, Qt::Key_Save)

// SpellCheck
KEY_MAP_WIN_CMD (SpellCheck, APPCOMMAND_SPELL_CHECK)
KEY_MAP_GTK     (SpellCheck, GDK_Spell)
KEY_MAP_QT      (SpellCheck, Qt::Key_Spell)

/******************************************************************************
 * Audio Keys
 *****************************************************************************/
// AudioBassBoostDown
KEY_MAP_WIN_CMD (AudioBassBoostDown, APPCOMMAND_BASS_DOWN)
KEY_MAP_QT      (AudioBassBoostDown, Qt::Key_BassDown)

// AudioBassBoostUp
KEY_MAP_WIN_CMD (AudioBassBoostUp, APPCOMMAND_BASS_UP)
KEY_MAP_QT      (AudioBassBoostUp, Qt::Key_BassUp)

// AudioVolumeDown
KEY_MAP_WIN               (AudioVolumeDown, VK_VOLUME_DOWN)
KEY_MAP_WIN_CMD           (AudioVolumeDown, APPCOMMAND_VOLUME_DOWN)
KEY_MAP_COCOA             (AudioVolumeDown, kVK_VolumeDown)
KEY_MAP_GTK               (AudioVolumeDown, GDK_AudioLowerVolume)
KEY_MAP_QT                (AudioVolumeDown, Qt::Key_VolumeDown)
KEY_MAP_ANDROID_EXCEPT_B2G(AudioVolumeDown, AKEYCODE_VOLUME_DOWN)
KEY_MAP_B2G               (VolumeDown,      AKEYCODE_VOLUME_DOWN)

// AudioVolumeUp
KEY_MAP_WIN               (AudioVolumeUp, VK_VOLUME_UP)
KEY_MAP_WIN_CMD           (AudioVolumeUp, APPCOMMAND_VOLUME_UP)
KEY_MAP_COCOA             (AudioVolumeUp, kVK_VolumeUp)
KEY_MAP_GTK               (AudioVolumeUp, GDK_AudioRaiseVolume)
KEY_MAP_QT                (AudioVolumeUp, Qt::Key_VolumeUp)
KEY_MAP_ANDROID_EXCEPT_B2G(AudioVolumeUp, AKEYCODE_VOLUME_UP)
KEY_MAP_B2G               (VolumeUp,      AKEYCODE_VOLUME_UP)

// AudioVolumeMute
KEY_MAP_WIN               (AudioVolumeMute, VK_VOLUME_MUTE)
KEY_MAP_WIN_CMD           (AudioVolumeMute, APPCOMMAND_VOLUME_MUTE)
KEY_MAP_COCOA             (AudioVolumeMute, kVK_Mute)
KEY_MAP_GTK               (AudioVolumeMute, GDK_AudioMute)
KEY_MAP_QT                (AudioVolumeMute, Qt::Key_VolumeMute)
KEY_MAP_ANDROID_EXCEPT_B2G(AudioVolumeMute, AKEYCODE_VOLUME_MUTE)
KEY_MAP_B2G               (VolumeMute,      AKEYCODE_VOLUME_MUTE)

/******************************************************************************
 * Application Keys
 ******************************************************************************/
// LaunchCalculator
KEY_MAP_GTK     (LaunchCalculator, GDK_Calculator)
KEY_MAP_QT      (LaunchCalculator, Qt::Key_Calculator)
KEY_MAP_ANDROID (LaunchCalculator, AKEYCODE_CALCULATOR)

// LaunchCalendar
KEY_MAP_GTK     (LaunchCalendar, GDK_Calendar)
KEY_MAP_QT      (LaunchCalendar, Qt::Key_Calendar)
KEY_MAP_ANDROID (LaunchCalendar, AKEYCODE_CALENDAR)

// LaunchMail
KEY_MAP_WIN     (LaunchMail, VK_LAUNCH_MAIL)
KEY_MAP_WIN_CMD (LaunchMail, APPCOMMAND_LAUNCH_MAIL)
KEY_MAP_GTK     (LaunchMail, GDK_Mail)
KEY_MAP_QT      (LaunchMail, Qt::Key_LaunchMail)
KEY_MAP_ANDROID (LaunchMail, AKEYCODE_ENVELOPE)

// LaunchMediaPlayer
KEY_MAP_WIN     (LaunchMediaPlayer, VK_LAUNCH_MEDIA_SELECT)
KEY_MAP_WIN_CMD (LaunchMediaPlayer, APPCOMMAND_LAUNCH_MEDIA_SELECT)
// GDK_CD is defined as "Launch CD/DVD player" in XF86keysym.h.
// Therefore, let's map it to media player rather than music player.
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_CD)
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_Video)
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_AudioMedia)
KEY_MAP_QT      (LaunchMediaPlayer, Qt::Key_LaunchMedia)
KEY_MAP_QT      (LaunchMediaPlayer, Qt::Key_CD)
KEY_MAP_QT      (LaunchMediaPlayer, Qt::Key_Video)

// LaunchMusicPlayer
KEY_MAP_GTK     (LaunchMusicPlayer, GDK_Music)
KEY_MAP_QT      (LaunchMusicPlayer, Qt::Key_Music)
KEY_MAP_ANDROID (LaunchMusicPlayer, AKEYCODE_MUSIC)

// LaunchMyComputer
KEY_MAP_GTK     (LaunchMyComputer, GDK_MyComputer)
KEY_MAP_GTK     (LaunchMyComputer, GDK_Explorer)
KEY_MAP_QT      (LaunchMyComputer, Qt::Key_Explorer)

// LaunchScreenSaver
KEY_MAP_GTK     (LaunchScreenSaver, GDK_ScreenSaver)
KEY_MAP_QT      (LaunchScreenSaver, Qt::Key_ScreenSaver)

// LaunchSpreadsheet
KEY_MAP_GTK     (LaunchSpreadsheet, GDK_Excel)
KEY_MAP_QT      (LaunchSpreadsheet, Qt::Key_Excel)

// LaunchWebBrowser
KEY_MAP_GTK     (LaunchWebBrowser, GDK_WWW)
KEY_MAP_QT      (LaunchWebBrowser, Qt::Key_WWW)
KEY_MAP_ANDROID (LaunchWebBrowser, AKEYCODE_EXPLORER)

// LaunchWebCam
KEY_MAP_GTK     (LaunchWebCam, GDK_WebCam)
KEY_MAP_QT      (LaunchWebCam, Qt::Key_WebCam)

// LaunchWordProcessor
KEY_MAP_GTK     (LaunchWordProcessor, GDK_Word)
KEY_MAP_QT      (LaunchWordProcessor, Qt::Key_Word)

// LaunchApplication1
KEY_MAP_WIN     (LaunchApplication1, VK_LAUNCH_APP1)
KEY_MAP_WIN_CMD (LaunchApplication1, APPCOMMAND_LAUNCH_APP1)
KEY_MAP_GTK     (LaunchApplication1, GDK_Launch0)
KEY_MAP_QT      (LaunchApplication1, Qt::Key_Launch0)

// LaunchApplication2
KEY_MAP_WIN     (LaunchApplication2, VK_LAUNCH_APP2)
KEY_MAP_WIN_CMD (LaunchApplication2, APPCOMMAND_LAUNCH_APP2)
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

/******************************************************************************
 * Browser Keys
 ******************************************************************************/
// BrowserBack
KEY_MAP_WIN     (BrowserBack, VK_BROWSER_BACK)
KEY_MAP_WIN_CMD (BrowserBack, APPCOMMAND_BROWSER_BACKWARD)
KEY_MAP_GTK     (BrowserBack, GDK_Back)
KEY_MAP_QT      (BrowserBack, Qt::Key_Back)
KEY_MAP_ANDROID (BrowserBack, AKEYCODE_BACK)

// BrowserFavorites
KEY_MAP_WIN     (BrowserFavorites, VK_BROWSER_FAVORITES)
KEY_MAP_WIN_CMD (BrowserFavorites, APPCOMMAND_BROWSER_FAVORITES)
KEY_MAP_QT      (BrowserFavorites, Qt::Key_Favorites)
KEY_MAP_ANDROID (BrowserFavorites, AKEYCODE_BOOKMARK)

// BrowserForward
KEY_MAP_WIN     (BrowserForward, VK_BROWSER_FORWARD)
KEY_MAP_WIN_CMD (BrowserForward, APPCOMMAND_BROWSER_FORWARD)
KEY_MAP_GTK     (BrowserForward, GDK_Forward)
KEY_MAP_QT      (BrowserForward, Qt::Key_Forward)
KEY_MAP_ANDROID (BrowserForward, AKEYCODE_FORWARD)

// BrowserHome
KEY_MAP_WIN     (BrowserHome, VK_BROWSER_HOME)
KEY_MAP_WIN_CMD (BrowserHome, APPCOMMAND_BROWSER_HOME)
KEY_MAP_GTK     (BrowserHome, GDK_HomePage)
KEY_MAP_QT      (BrowserHome, Qt::Key_HomePage)

// BrowserRefresh
KEY_MAP_WIN     (BrowserRefresh, VK_BROWSER_REFRESH)
KEY_MAP_WIN_CMD (BrowserRefresh, APPCOMMAND_BROWSER_REFRESH)
KEY_MAP_GTK     (BrowserRefresh, GDK_Refresh)
KEY_MAP_GTK     (BrowserRefresh, GDK_Reload)
KEY_MAP_QT      (BrowserRefresh, Qt::Key_Refresh)
KEY_MAP_QT      (BrowserRefresh, Qt::Key_Reload)

// BrowserSearch
KEY_MAP_WIN     (BrowserSearch, VK_BROWSER_SEARCH)
KEY_MAP_WIN_CMD (BrowserSearch, APPCOMMAND_BROWSER_SEARCH)
KEY_MAP_GTK     (BrowserSearch, GDK_Search)
KEY_MAP_QT      (BrowserSearch, Qt::Key_Search)
KEY_MAP_ANDROID (BrowserSearch, AKEYCODE_SEARCH)

// BrowserStop
KEY_MAP_WIN     (BrowserStop, VK_BROWSER_STOP)
KEY_MAP_WIN_CMD (BrowserStop, APPCOMMAND_BROWSER_STOP)
KEY_MAP_GTK     (BrowserStop, GDK_Stop)
KEY_MAP_QT      (BrowserStop, Qt::Key_Stop)

/******************************************************************************
 * Mobile Phone Keys
 ******************************************************************************/
// Call
KEY_MAP_ANDROID (Call, AKEYCODE_CALL)

// Camera
KEY_MAP_QT      (Camera, Qt::Key_Camera)
KEY_MAP_ANDROID (Camera, AKEYCODE_CAMERA)

// CameraFocus
KEY_MAP_QT                (CameraFocus,       Qt::Key_CameraFocus)
KEY_MAP_ANDROID_EXCEPT_B2G(CameraFocus,       AKEYCODE_FOCUS)
KEY_MAP_B2G               (CameraFocusAdjust, AKEYCODE_FOCUS)

// GoHome
KEY_MAP_ANDROID_EXCEPT_B2G(GoHome,     AKEYCODE_HOME)
KEY_MAP_B2G               (HomeScreen, AKEYCODE_HOME)

/******************************************************************************
 * TV Keys
 ******************************************************************************/
// TV
KEY_MAP_ANDROID (TV, AKEYCODE_TV)

// TVInput
KEY_MAP_ANDROID (TVInput, AKEYCODE_TV_INPUT)

// TVPower
KEY_MAP_ANDROID (TVPower, AKEYCODE_TV_POWER)

/******************************************************************************
 * Media Controller Keys
 ******************************************************************************/
// AVRInput
KEY_MAP_ANDROID (AVRInput, AKEYCODE_AVR_INPUT)

// AVRPower
KEY_MAP_ANDROID (AVRPower, AKEYCODE_AVR_POWER)

// ColorF0Red
KEY_MAP_GTK     (ColorF0Red, GDK_Red)
KEY_MAP_QT      (ColorF0Red, Qt::Key_Red)
KEY_MAP_ANDROID (ColorF0Red, AKEYCODE_PROG_RED)

// ColorF1Green
KEY_MAP_GTK     (ColorF1Green, GDK_Green)
KEY_MAP_QT      (ColorF1Green, Qt::Key_Green)
KEY_MAP_ANDROID (ColorF1Green, AKEYCODE_PROG_GREEN)

// ColorF2Yellow
KEY_MAP_GTK     (ColorF2Yellow, GDK_Yellow)
KEY_MAP_QT      (ColorF2Yellow, Qt::Key_Yellow)
KEY_MAP_ANDROID (ColorF2Yellow, AKEYCODE_PROG_YELLOW)

// ColorF3Blue
KEY_MAP_GTK     (ColorF3Blue, GDK_Blue)
KEY_MAP_QT      (ColorF3Blue, Qt::Key_Blue)
KEY_MAP_ANDROID (ColorF3Blue, AKEYCODE_PROG_BLUE)

// Dimmer
KEY_MAP_GTK     (Dimmer, GDK_BrightnessAdjust)
KEY_MAP_QT      (Dimmer, Qt::Key_BrightnessAdjust)

// Guide
KEY_MAP_ANDROID (Guide, AKEYCODE_GUIDE)

// Info
KEY_MAP_ANDROID (Info, AKEYCODE_INFO)

// MediaFastForward
KEY_MAP_WIN_CMD (MediaFastForward, APPCOMMAND_MEDIA_FAST_FORWARD)
KEY_MAP_GTK     (MediaFastForward, GDK_AudioForward)
KEY_MAP_QT      (MediaFastForward, Qt::Key_AudioForward)
KEY_MAP_ANDROID (MediaFastForward, AKEYCODE_MEDIA_FAST_FORWARD)

// MediaLast
KEY_MAP_QT      (MediaLast, Qt::Key_MediaLast)

// PinPToggle
KEY_MAP_ANDROID (PinPToggle, AKEYCODE_WINDOW)

// RandomToggle
KEY_MAP_GTK     (RandomToggle, GDK_AudioRandomPlay)
KEY_MAP_QT      (RandomToggle, Qt::Key_AudioRandomPlay)

// Settings
KEY_MAP_ANDROID (Settings, AKEYCODE_SETTINGS)

// STBInput
KEY_MAP_ANDROID (STBInput, AKEYCODE_STB_INPUT)

// STBPower
KEY_MAP_ANDROID (STBPower, AKEYCODE_STB_POWER)

// Subtitle
KEY_MAP_GTK     (Subtitle, GDK_Subtitle)
KEY_MAP_QT      (Subtitle, Qt::Key_Subtitle)
KEY_MAP_ANDROID (Subtitle, AKEYCODE_CAPTIONS)

// VideoModeNext
KEY_MAP_GTK     (VideoModeNext, GDK_Next_VMode)

// ZoomToggle
KEY_MAP_WIN     (ZoomToggle, VK_ZOOM)
KEY_MAP_QT      (ZoomToggle, Qt::Key_Zoom)

/******************************************************************************
 * Keys not defined by any standards
 ******************************************************************************/
// SoftLeft
KEY_MAP_ANDROID (SoftLeft, AKEYCODE_SOFT_LEFT)

// SoftRight
KEY_MAP_ANDROID (SoftRight, AKEYCODE_SOFT_RIGHT)

#undef KEY_MAP_WIN
#undef KEY_MAP_WIN_JPN
#undef KEY_MAP_WIN_KOR
#undef KEY_MAP_WIN_OTH
#undef KEY_MAP_WIN_CMD
#undef KEY_MAP_COCOA
#undef KEY_MAP_GTK
#undef KEY_MAP_QT
#undef KEY_MAP_ANDROID
#undef KEY_MAP_ANDROID_EXCEPT_B2G
#undef KEY_MAP_B2G
