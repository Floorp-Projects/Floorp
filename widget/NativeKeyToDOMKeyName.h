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
// Only for Android
#define KEY_MAP_ANDROID(aCPPKeyName, aNativeKey)

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
#elif defined(ANDROID)
#undef KEY_MAP_ANDROID
#define KEY_MAP_ANDROID(aCPPKeyName, aNativeKey) \
  NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, KEY_NAME_INDEX_##aCPPKeyName)
#endif

/******************************************************************************
 * Modifier Keys
 ******************************************************************************/
// Alt
KEY_MAP_WIN     (Alt, VK_MENU)
KEY_MAP_WIN     (Alt, VK_LMENU)
KEY_MAP_WIN     (Alt, VK_RMENU) // This is ignored if active keyboard layout
                                // has AltGr.  In such case, AltGraph is mapped.
KEY_MAP_COCOA   (Alt, kVK_Option)
KEY_MAP_COCOA   (Alt, kVK_RightOption)
KEY_MAP_GTK     (Alt, GDK_Alt_L)
KEY_MAP_GTK     (Alt, GDK_Alt_R)
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

// CapsLock
KEY_MAP_WIN     (CapsLock, VK_CAPITAL)
KEY_MAP_COCOA   (CapsLock, kVK_CapsLock)
KEY_MAP_GTK     (CapsLock, GDK_Caps_Lock)
KEY_MAP_ANDROID (CapsLock, AKEYCODE_CAPS_LOCK)

// Control
KEY_MAP_WIN     (Control, VK_CONTROL)
KEY_MAP_WIN     (Control, VK_LCONTROL)
KEY_MAP_WIN     (Control, VK_RCONTROL)
KEY_MAP_COCOA   (Control, kVK_Control)
KEY_MAP_COCOA   (Control, kVK_RightControl)
KEY_MAP_GTK     (Control, GDK_Control_L)
KEY_MAP_GTK     (Control, GDK_Control_R)
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
KEY_MAP_ANDROID (Meta, AKEYCODE_META_LEFT)
KEY_MAP_ANDROID (Meta, AKEYCODE_META_RIGHT)

// NumLock
KEY_MAP_WIN     (NumLock, VK_NUMLOCK)
KEY_MAP_GTK     (NumLock, GDK_Num_Lock)
KEY_MAP_ANDROID (NumLock, AKEYCODE_NUM_LOCK)

// OS
KEY_MAP_WIN     (OS, VK_LWIN)
KEY_MAP_WIN     (OS, VK_RWIN)
KEY_MAP_GTK     (OS, GDK_Super_L)
KEY_MAP_GTK     (OS, GDK_Super_R)
KEY_MAP_GTK     (OS, GDK_Hyper_L)
KEY_MAP_GTK     (OS, GDK_Hyper_R)

// ScrollLock
KEY_MAP_WIN     (ScrollLock, VK_SCROLL)
KEY_MAP_GTK     (ScrollLock, GDK_Scroll_Lock)
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
KEY_MAP_ANDROID (Enter, AKEYCODE_DPAD_CENTER)
KEY_MAP_ANDROID (Enter, AKEYCODE_ENTER)
KEY_MAP_ANDROID (Enter, AKEYCODE_NUMPAD_ENTER)

// Tab
KEY_MAP_WIN     (Tab, VK_TAB)
KEY_MAP_COCOA   (Tab, kVK_Tab)
KEY_MAP_GTK     (Tab, GDK_Tab)
KEY_MAP_GTK     (Tab, GDK_ISO_Left_Tab) // Shift+Tab
KEY_MAP_GTK     (Tab, GDK_KP_Tab)
KEY_MAP_ANDROID (Tab, AKEYCODE_TAB)

/******************************************************************************
 * Navigation Keys
 ******************************************************************************/
// ArrowDown
KEY_MAP_WIN     (ArrowDown, VK_DOWN)
KEY_MAP_COCOA   (ArrowDown, kVK_DownArrow)
KEY_MAP_GTK     (ArrowDown, GDK_Down)
KEY_MAP_GTK     (ArrowDown, GDK_KP_Down)
KEY_MAP_ANDROID (ArrowDown, AKEYCODE_DPAD_DOWN)

// ArrowLeft
KEY_MAP_WIN     (ArrowLeft, VK_LEFT)
KEY_MAP_COCOA   (ArrowLeft, kVK_LeftArrow)
KEY_MAP_GTK     (ArrowLeft, GDK_Left)
KEY_MAP_GTK     (ArrowLeft, GDK_KP_Left)
KEY_MAP_ANDROID (ArrowLeft, AKEYCODE_DPAD_LEFT)

// ArrowRight
KEY_MAP_WIN     (ArrowRight, VK_RIGHT)
KEY_MAP_COCOA   (ArrowRight, kVK_RightArrow)
KEY_MAP_GTK     (ArrowRight, GDK_Right)
KEY_MAP_GTK     (ArrowRight, GDK_KP_Right)
KEY_MAP_ANDROID (ArrowRight, AKEYCODE_DPAD_RIGHT)

// ArrowUp
KEY_MAP_WIN     (ArrowUp, VK_UP)
KEY_MAP_COCOA   (ArrowUp, kVK_UpArrow)
KEY_MAP_GTK     (ArrowUp, GDK_Up)
KEY_MAP_GTK     (ArrowUp, GDK_KP_Up)
KEY_MAP_ANDROID (ArrowUp, AKEYCODE_DPAD_UP)

// End
KEY_MAP_WIN     (End, VK_END)
KEY_MAP_COCOA   (End, kVK_End)
KEY_MAP_GTK     (End, GDK_End)
KEY_MAP_GTK     (End, GDK_KP_End)
KEY_MAP_ANDROID (End, AKEYCODE_MOVE_END)

// Home
KEY_MAP_WIN     (Home, VK_HOME)
KEY_MAP_COCOA   (Home, kVK_Home)
KEY_MAP_GTK     (Home, GDK_Home)
KEY_MAP_GTK     (Home, GDK_KP_Home)
KEY_MAP_ANDROID (Home, AKEYCODE_MOVE_HOME)

// PageDown
KEY_MAP_WIN     (PageDown, VK_NEXT)
KEY_MAP_COCOA   (PageDown, kVK_PageDown)
KEY_MAP_GTK     (PageDown, GDK_Page_Down /* same as GDK_Next */)
KEY_MAP_GTK     (PageDown, GDK_KP_Page_Down /* same as GDK_KP_Next */)
KEY_MAP_ANDROID (PageDown, AKEYCODE_PAGE_DOWN)

// PageUp
KEY_MAP_WIN     (PageUp, VK_PRIOR)
KEY_MAP_COCOA   (PageUp, kVK_PageUp)
KEY_MAP_GTK     (PageUp, GDK_Page_Up /* same as GDK_Prior */)
KEY_MAP_GTK     (PageUp, GDK_KP_Page_Up /* same as GDK_KP_Prior */)
KEY_MAP_ANDROID (PageUp, AKEYCODE_PAGE_UP)

/******************************************************************************
 * Editing Keys
 ******************************************************************************/
// Backspace
KEY_MAP_WIN     (Backspace, VK_BACK)
KEY_MAP_COCOA   (Backspace, kVK_PC_Backspace)
KEY_MAP_GTK     (Backspace, GDK_BackSpace)
KEY_MAP_ANDROID (Backspace, AKEYCODE_DEL)

// Clear
KEY_MAP_WIN     (Clear, VK_CLEAR)
KEY_MAP_WIN     (Clear, VK_OEM_CLEAR)
KEY_MAP_COCOA   (Clear, kVK_ANSI_KeypadClear)
KEY_MAP_GTK     (Clear, GDK_Clear)
KEY_MAP_ANDROID (Clear, AKEYCODE_CLEAR)

// Copy
KEY_MAP_WIN_CMD (Copy, APPCOMMAND_COPY)
KEY_MAP_GTK     (Copy, GDK_Copy)
KEY_MAP_ANDROID (Copy, AKEYCODE_COPY)

// CrSel
KEY_MAP_WIN     (CrSel, VK_CRSEL)
KEY_MAP_GTK     (CrSel, GDK_3270_CursorSelect) // legacy IBM keyboard layout

// Cut
KEY_MAP_WIN_CMD (Cut, APPCOMMAND_CUT)
KEY_MAP_GTK     (Cut, GDK_Cut)
KEY_MAP_ANDROID (Cut, AKEYCODE_CUT)

// Delete
KEY_MAP_WIN     (Delete, VK_DELETE)
KEY_MAP_COCOA   (Delete, kVK_PC_Delete)
KEY_MAP_GTK     (Delete, GDK_Delete)
KEY_MAP_GTK     (Delete, GDK_KP_Delete)
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
KEY_MAP_ANDROID (Insert, AKEYCODE_INSERT)

// Paste
KEY_MAP_WIN_CMD (Paste, APPCOMMAND_PASTE)
KEY_MAP_GTK     (Paste, GDK_Paste)
KEY_MAP_ANDROID (Paste, AKEYCODE_PASTE)

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

// Attn
KEY_MAP_WIN_OTH (Attn, VK_ATTN) // not valid with Japanese keyboard layout
KEY_MAP_GTK     (Attn, GDK_3270_Attn) // legacy IBM keyboard layout

// Cancel
KEY_MAP_WIN     (Cancel, VK_CANCEL)
KEY_MAP_GTK     (Cancel, GDK_Cancel)

// ContextMenu
KEY_MAP_WIN     (ContextMenu, VK_APPS)
KEY_MAP_COCOA   (ContextMenu, kVK_PC_ContextMenu)
KEY_MAP_GTK     (ContextMenu, GDK_Menu)
KEY_MAP_ANDROID (ContextMenu, AKEYCODE_MENU)

// Escape
KEY_MAP_WIN     (Escape, VK_ESCAPE)
KEY_MAP_COCOA   (Escape, kVK_Escape)
KEY_MAP_GTK     (Escape, GDK_Escape)
KEY_MAP_ANDROID (Escape, AKEYCODE_ESCAPE)

// Execute
KEY_MAP_WIN     (Execute, VK_EXECUTE)
KEY_MAP_GTK     (Execute, GDK_Execute)

// Find
KEY_MAP_WIN_CMD (Find, APPCOMMAND_FIND)
KEY_MAP_GTK     (Find, GDK_Find)

// Help
KEY_MAP_WIN     (Help, VK_HELP)
KEY_MAP_WIN_CMD (Help, APPCOMMAND_HELP)
KEY_MAP_COCOA   (Help, kVK_Help)
KEY_MAP_GTK     (Help, GDK_Help)
KEY_MAP_ANDROID (Help, AKEYCODE_HELP)

// Pause
KEY_MAP_WIN     (Pause, VK_PAUSE)
KEY_MAP_GTK     (Pause, GDK_Pause)
// Break is typically mapped to Alt+Pause or Ctrl+Pause on GTK.
KEY_MAP_GTK     (Pause, GDK_Break)
KEY_MAP_ANDROID (Pause, AKEYCODE_BREAK)

// Play
KEY_MAP_WIN     (Play, VK_PLAY)
KEY_MAP_GTK     (Play, GDK_3270_Play) // legacy IBM keyboard layout

// Select
KEY_MAP_WIN     (Select, VK_SELECT)
KEY_MAP_GTK     (Select, GDK_Select)

// ZoomIn
KEY_MAP_GTK     (ZoomIn, GDK_ZoomIn)
KEY_MAP_ANDROID (ZoomIn, AKEYCODE_ZOOM_IN)

// ZoomOut
KEY_MAP_GTK     (ZoomOut, GDK_ZoomOut)
KEY_MAP_ANDROID (ZoomOut, AKEYCODE_ZOOM_OUT)

/******************************************************************************
 * Device Keys
 ******************************************************************************/
// BrightnessDown
KEY_MAP_GTK     (BrightnessDown, GDK_MonBrightnessDown)
KEY_MAP_ANDROID (BrightnessDown, AKEYCODE_BRIGHTNESS_DOWN)

// BrightnessUp
KEY_MAP_GTK     (BrightnessUp, GDK_MonBrightnessUp)
KEY_MAP_ANDROID (BrightnessUp, AKEYCODE_BRIGHTNESS_UP)

// Eject
KEY_MAP_GTK     (Eject, GDK_Eject)
KEY_MAP_ANDROID (Eject, AKEYCODE_MEDIA_EJECT)

// LogOff
KEY_MAP_GTK     (LogOff, GDK_LogOff)

// Power
KEY_MAP_ANDROID (Power, AKEYCODE_POWER)

// PowerOff
KEY_MAP_GTK     (PowerOff, GDK_PowerDown)
KEY_MAP_GTK     (PowerOff, GDK_PowerOff)

// PrintScreen
KEY_MAP_WIN     (PrintScreen, VK_SNAPSHOT)
KEY_MAP_GTK     (PrintScreen, GDK_3270_PrintScreen)
KEY_MAP_GTK     (PrintScreen, GDK_Print)
KEY_MAP_GTK     (PrintScreen, GDK_Sys_Req)
KEY_MAP_ANDROID (PrintScreen, AKEYCODE_SYSRQ)

// Hibernate
KEY_MAP_GTK     (Hibernate, GDK_Hibernate)

// Standby
KEY_MAP_WIN     (Standby, VK_SLEEP)
KEY_MAP_GTK     (Standby, GDK_Standby)
KEY_MAP_GTK     (Standby, GDK_Suspend)
KEY_MAP_GTK     (Standby, GDK_Sleep)
KEY_MAP_ANDROID (Standby, AKEYCODE_SLEEP)

// WakeUp
KEY_MAP_GTK     (WakeUp, GDK_WakeUp)
KEY_MAP_ANDROID (WakeUp, AKEYCODE_WAKEUP)

/******************************************************************************
 * IME and Composition Keys
 ******************************************************************************/
// AllCandidates
KEY_MAP_GTK     (AllCandidates, GDK_MultipleCandidate) // OADG 109, Zen Koho

// Alphanumeric
KEY_MAP_WIN_JPN (Alphanumeric, VK_OEM_ATTN)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_Shift)
KEY_MAP_GTK     (Alphanumeric, GDK_Eisu_toggle)

// CodeInput
KEY_MAP_GTK     (CodeInput, GDK_Codeinput) // OADG 109, Kanji Bangou

// Compose
KEY_MAP_GTK     (Compose, GDK_Multi_key) // "Multi Key" is "Compose key" on X

// Convert
KEY_MAP_WIN     (Convert, VK_CONVERT)
KEY_MAP_GTK     (Convert, GDK_Henkan)
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
KEY_MAP_ANDROID (NonConvert, AKEYCODE_MUHENKAN)

// PreviousCandidate
KEY_MAP_GTK     (PreviousCandidate, GDK_PreviousCandidate) // OADG 109, Mae Koho

// Process
KEY_MAP_WIN     (Process, VK_PROCESSKEY)

// SingleCandidate
KEY_MAP_GTK     (SingleCandidate, GDK_SingleCandidate)

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

// Hiragana
KEY_MAP_WIN_JPN (Hiragana, VK_OEM_COPY)
KEY_MAP_GTK     (Hiragana, GDK_Hiragana)

// HiraganaKatakana
KEY_MAP_GTK     (HiraganaKatakana, GDK_Hiragana_Katakana)
KEY_MAP_ANDROID (HiraganaKatakana, AKEYCODE_KATAKANA_HIRAGANA)

// KanaMode
// VK_KANA is never used with modern Japanese keyboard, however, IE maps it to
// KanaMode, therefore, we should use same map for it.
KEY_MAP_WIN_JPN (KanaMode, VK_KANA /* same as VK_HANGUL */)
KEY_MAP_WIN_JPN (KanaMode, VK_ATTN)
KEY_MAP_GTK     (KanaMode, GDK_Kana_Lock)
KEY_MAP_GTK     (KanaMode, GDK_Kana_Shift)

// KanjiMode
KEY_MAP_WIN_JPN (KanjiMode, VK_KANJI /* same as VK_HANJA */)
KEY_MAP_COCOA   (KanjiMode, kVK_JIS_Kana) // Kana key opens IME
KEY_MAP_GTK     (KanjiMode, GDK_Kanji) // Typically, Alt + Hankaku/Zenkaku key
// Assuming that KANA key of Android is the Kana key on Mac keyboard.
KEY_MAP_ANDROID (KanjiMode, AKEYCODE_KANA)

// Katakana
KEY_MAP_WIN_JPN (Katakana, VK_OEM_FINISH)
KEY_MAP_GTK     (Katakana, GDK_Katakana)

// Romaji
KEY_MAP_WIN_JPN (Romaji, VK_OEM_BACKTAB)
KEY_MAP_GTK     (Romaji, GDK_Romaji)

// Zenkaku
KEY_MAP_WIN_JPN (Zenkaku, VK_OEM_ENLW)
KEY_MAP_GTK     (Zenkaku, GDK_Zenkaku)

// ZenkakuHankaku
KEY_MAP_GTK     (ZenkakuHankaku, GDK_Zenkaku_Hankaku)
KEY_MAP_ANDROID (ZenkakuHankaku, AKEYCODE_ZENKAKU_HANKAKU)

/******************************************************************************
 * General-Purpose Function Keys
 ******************************************************************************/
// F1
KEY_MAP_WIN     (F1, VK_F1)
KEY_MAP_COCOA   (F1, kVK_F1)
KEY_MAP_GTK     (F1, GDK_F1)
KEY_MAP_GTK     (F1, GDK_KP_F1)
KEY_MAP_ANDROID (F1, AKEYCODE_F1)

// F2
KEY_MAP_WIN     (F2, VK_F2)
KEY_MAP_COCOA   (F2, kVK_F2)
KEY_MAP_GTK     (F2, GDK_F2)
KEY_MAP_GTK     (F2, GDK_KP_F2)
KEY_MAP_ANDROID (F2, AKEYCODE_F2)

// F3
KEY_MAP_WIN     (F3, VK_F3)
KEY_MAP_COCOA   (F3, kVK_F3)
KEY_MAP_GTK     (F3, GDK_F3)
KEY_MAP_GTK     (F3, GDK_KP_F3)
KEY_MAP_ANDROID (F3, AKEYCODE_F3)

// F4
KEY_MAP_WIN     (F4, VK_F4)
KEY_MAP_COCOA   (F4, kVK_F4)
KEY_MAP_GTK     (F4, GDK_F4)
KEY_MAP_GTK     (F4, GDK_KP_F4)
KEY_MAP_ANDROID (F4, AKEYCODE_F4)

// F5
KEY_MAP_WIN     (F5, VK_F5)
KEY_MAP_COCOA   (F5, kVK_F5)
KEY_MAP_GTK     (F5, GDK_F5)
KEY_MAP_ANDROID (F5, AKEYCODE_F5)

// F6
KEY_MAP_WIN     (F6, VK_F6)
KEY_MAP_COCOA   (F6, kVK_F6)
KEY_MAP_GTK     (F6, GDK_F6)
KEY_MAP_ANDROID (F6, AKEYCODE_F6)

// F7
KEY_MAP_WIN     (F7, VK_F7)
KEY_MAP_COCOA   (F7, kVK_F7)
KEY_MAP_GTK     (F7, GDK_F7)
KEY_MAP_ANDROID (F7, AKEYCODE_F7)

// F8
KEY_MAP_WIN     (F8, VK_F8)
KEY_MAP_COCOA   (F8, kVK_F8)
KEY_MAP_GTK     (F8, GDK_F8)
KEY_MAP_ANDROID (F8, AKEYCODE_F8)

// F9
KEY_MAP_WIN     (F9, VK_F9)
KEY_MAP_COCOA   (F9, kVK_F9)
KEY_MAP_GTK     (F9, GDK_F9)
KEY_MAP_ANDROID (F9, AKEYCODE_F9)

// F10
KEY_MAP_WIN     (F10, VK_F10)
KEY_MAP_COCOA   (F10, kVK_F10)
KEY_MAP_GTK     (F10, GDK_F10)
KEY_MAP_ANDROID (F10, AKEYCODE_F10)

// F11
KEY_MAP_WIN     (F11, VK_F11)
KEY_MAP_COCOA   (F11, kVK_F11)
KEY_MAP_GTK     (F11, GDK_F11 /* same as GDK_L1 */)
KEY_MAP_ANDROID (F11, AKEYCODE_F11)

// F12
KEY_MAP_WIN     (F12, VK_F12)
KEY_MAP_COCOA   (F12, kVK_F12)
KEY_MAP_GTK     (F12, GDK_F12 /* same as GDK_L2 */)
KEY_MAP_ANDROID (F12, AKEYCODE_F12)

// F13
KEY_MAP_WIN     (F13, VK_F13)
KEY_MAP_COCOA   (F13, kVK_F13)
KEY_MAP_GTK     (F13, GDK_F13 /* same as GDK_L3 */)

// F14
KEY_MAP_WIN     (F14, VK_F14)
KEY_MAP_COCOA   (F14, kVK_F14)
KEY_MAP_GTK     (F14, GDK_F14 /* same as GDK_L4 */)

// F15
KEY_MAP_WIN     (F15, VK_F15)
KEY_MAP_COCOA   (F15, kVK_F15)
KEY_MAP_GTK     (F15, GDK_F15 /* same as GDK_L5 */)

// F16
KEY_MAP_WIN     (F16, VK_F16)
KEY_MAP_COCOA   (F16, kVK_F16)
KEY_MAP_GTK     (F16, GDK_F16 /* same as GDK_L6 */)

// F17
KEY_MAP_WIN     (F17, VK_F17)
KEY_MAP_COCOA   (F17, kVK_F17)
KEY_MAP_GTK     (F17, GDK_F17 /* same as GDK_L7 */)

// F18
KEY_MAP_WIN     (F18, VK_F18)
KEY_MAP_COCOA   (F18, kVK_F18)
KEY_MAP_GTK     (F18, GDK_F18 /* same as GDK_L8 */)

// F19
KEY_MAP_WIN     (F19, VK_F19)
KEY_MAP_COCOA   (F19, kVK_F19)
KEY_MAP_GTK     (F19, GDK_F19 /* same as GDK_L9 */)

// F20
KEY_MAP_WIN     (F20, VK_F20)
KEY_MAP_GTK     (F20, GDK_F20 /* same as GDK_L10 */)

// F21
KEY_MAP_WIN     (F21, VK_F21)
KEY_MAP_GTK     (F21, GDK_F21 /* same as GDK_R1 */)

// F22
KEY_MAP_WIN     (F22, VK_F22)
KEY_MAP_GTK     (F22, GDK_F22 /* same as GDK_R2 */)

// F23
KEY_MAP_WIN     (F23, VK_F23)
KEY_MAP_GTK     (F23, GDK_F23 /* same as GDK_R3 */)

// F24
KEY_MAP_WIN     (F24, VK_F24)
KEY_MAP_GTK     (F24, GDK_F24 /* same as GDK_R4 */)

// F25
KEY_MAP_GTK     (F25, GDK_F25 /* same as GDK_R5 */)

// F26
KEY_MAP_GTK     (F26, GDK_F26 /* same as GDK_R6 */)

// F27
KEY_MAP_GTK     (F27, GDK_F27 /* same as GDK_R7 */)

// F28
KEY_MAP_GTK     (F28, GDK_F28 /* same as GDK_R8 */)

// F29
KEY_MAP_GTK     (F29, GDK_F29 /* same as GDK_R9 */)

// F30
KEY_MAP_GTK     (F30, GDK_F30 /* same as GDK_R10 */)

// F31
KEY_MAP_GTK     (F31, GDK_F31 /* same as GDK_R11 */)

// F32
KEY_MAP_GTK     (F32, GDK_F32 /* same as GDK_R12 */)

// F33
KEY_MAP_GTK     (F33, GDK_F33 /* same as GDK_R13 */)

// F34
KEY_MAP_GTK     (F34, GDK_F34 /* same as GDK_R14 */)

// F35
KEY_MAP_GTK     (F35, GDK_F35 /* same as GDK_R15 */)

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

// MailForward
KEY_MAP_WIN_CMD (MailForward, APPCOMMAND_FORWARD_MAIL)
KEY_MAP_GTK     (MailForward, GDK_MailForward)

// MailReply
KEY_MAP_WIN_CMD (MailReply, APPCOMMAND_REPLY_TO_MAIL)
KEY_MAP_GTK     (MailReply, GDK_Reply)

// MailSend
KEY_MAP_WIN_CMD (MailSend, APPCOMMAND_SEND_MAIL)
KEY_MAP_GTK     (MailSend, GDK_Send)

// MediaFastForward
KEY_MAP_WIN_CMD (MediaFastForward, APPCOMMAND_MEDIA_FAST_FORWARD)
KEY_MAP_GTK     (MediaFastForward, GDK_AudioForward)
KEY_MAP_ANDROID (MediaFastForward, AKEYCODE_MEDIA_FAST_FORWARD)

// MediaPause
KEY_MAP_WIN_CMD (MediaPause, APPCOMMAND_MEDIA_PAUSE)
KEY_MAP_GTK     (MediaPause, GDK_AudioPause)
KEY_MAP_ANDROID (MediaPause, AKEYCODE_MEDIA_PAUSE)

// MediaPlay
KEY_MAP_WIN_CMD (MediaPlay, APPCOMMAND_MEDIA_PLAY)
KEY_MAP_GTK     (MediaPlay, GDK_AudioPlay)
KEY_MAP_ANDROID (MediaPlay, AKEYCODE_MEDIA_PLAY)

// MediaPlayPause
KEY_MAP_WIN     (MediaPlayPause, VK_MEDIA_PLAY_PAUSE)
KEY_MAP_WIN_CMD (MediaPlayPause, APPCOMMAND_MEDIA_PLAY_PAUSE)
KEY_MAP_ANDROID (MediaPlayPause, AKEYCODE_MEDIA_PLAY_PAUSE)

// MediaRecord
KEY_MAP_WIN_CMD (MediaRecord, APPCOMMAND_MEDIA_RECORD)
KEY_MAP_GTK     (MediaRecord, GDK_AudioRecord)
KEY_MAP_ANDROID (MediaRecord, AKEYCODE_MEDIA_RECORD)

// MediaRewind
KEY_MAP_WIN_CMD (MediaRewind, APPCOMMAND_MEDIA_REWIND)
KEY_MAP_GTK     (MediaRewind, GDK_AudioRewind)
KEY_MAP_ANDROID (MediaRewind, AKEYCODE_MEDIA_REWIND)

// MediaStop
KEY_MAP_WIN     (MediaStop, VK_MEDIA_STOP)
KEY_MAP_WIN_CMD (MediaStop, APPCOMMAND_MEDIA_STOP)
KEY_MAP_GTK     (MediaStop, GDK_AudioStop)
KEY_MAP_ANDROID (MediaStop, AKEYCODE_MEDIA_STOP)

// MediaTrackNext
KEY_MAP_WIN     (MediaTrackNext, VK_MEDIA_NEXT_TRACK)
KEY_MAP_WIN_CMD (MediaTrackNext, APPCOMMAND_MEDIA_NEXTTRACK)
KEY_MAP_GTK     (MediaTrackNext, GDK_AudioNext)
KEY_MAP_ANDROID (MediaTrackNext, AKEYCODE_MEDIA_NEXT)

// MediaTrackPrevious
KEY_MAP_WIN     (MediaTrackPrevious, VK_MEDIA_PREV_TRACK)
KEY_MAP_WIN_CMD (MediaTrackPrevious, APPCOMMAND_MEDIA_PREVIOUSTRACK)
KEY_MAP_GTK     (MediaTrackPrevious, GDK_AudioPrev)
KEY_MAP_ANDROID (MediaTrackPrevious, AKEYCODE_MEDIA_PREVIOUS)

// New
KEY_MAP_WIN_CMD (New, APPCOMMAND_NEW)
KEY_MAP_GTK     (New, GDK_New)

// Open
KEY_MAP_WIN_CMD (Open, APPCOMMAND_OPEN)
KEY_MAP_GTK     (Open, GDK_Open)

// Print
KEY_MAP_WIN_CMD (Print, APPCOMMAND_PRINT)

// Save
KEY_MAP_WIN_CMD (Save, APPCOMMAND_SAVE)
KEY_MAP_GTK     (Save, GDK_Save)

// SpellCheck
KEY_MAP_WIN_CMD (SpellCheck, APPCOMMAND_SPELL_CHECK)
KEY_MAP_GTK     (SpellCheck, GDK_Spell)

/******************************************************************************
 * Audio Keys
 *****************************************************************************/
// AudioBassBoostDown
KEY_MAP_WIN_CMD (AudioBassBoostDown, APPCOMMAND_BASS_DOWN)

// AudioBassBoostUp
KEY_MAP_WIN_CMD (AudioBassBoostUp, APPCOMMAND_BASS_UP)

// AudioVolumeDown
KEY_MAP_WIN     (AudioVolumeDown, VK_VOLUME_DOWN)
KEY_MAP_WIN_CMD (AudioVolumeDown, APPCOMMAND_VOLUME_DOWN)
KEY_MAP_COCOA   (AudioVolumeDown, kVK_VolumeDown)
KEY_MAP_GTK     (AudioVolumeDown, GDK_AudioLowerVolume)
KEY_MAP_ANDROID (AudioVolumeDown, AKEYCODE_VOLUME_DOWN)

// AudioVolumeUp
KEY_MAP_WIN     (AudioVolumeUp, VK_VOLUME_UP)
KEY_MAP_WIN_CMD (AudioVolumeUp, APPCOMMAND_VOLUME_UP)
KEY_MAP_COCOA   (AudioVolumeUp, kVK_VolumeUp)
KEY_MAP_GTK     (AudioVolumeUp, GDK_AudioRaiseVolume)
KEY_MAP_ANDROID (AudioVolumeUp, AKEYCODE_VOLUME_UP)

// AudioVolumeMute
KEY_MAP_WIN     (AudioVolumeMute, VK_VOLUME_MUTE)
KEY_MAP_WIN_CMD (AudioVolumeMute, APPCOMMAND_VOLUME_MUTE)
KEY_MAP_COCOA   (AudioVolumeMute, kVK_Mute)
KEY_MAP_GTK     (AudioVolumeMute, GDK_AudioMute)
KEY_MAP_ANDROID (AudioVolumeMute, AKEYCODE_VOLUME_MUTE)

// MicrophoneVolumeMute
KEY_MAP_ANDROID (MicrophoneVolumeMute, AKEYCODE_MUTE)

/******************************************************************************
 * Application Keys
 ******************************************************************************/
// LaunchCalculator
KEY_MAP_GTK     (LaunchCalculator, GDK_Calculator)
KEY_MAP_ANDROID (LaunchCalculator, AKEYCODE_CALCULATOR)

// LaunchCalendar
KEY_MAP_GTK     (LaunchCalendar, GDK_Calendar)
KEY_MAP_ANDROID (LaunchCalendar, AKEYCODE_CALENDAR)

// LaunchContacts
KEY_MAP_ANDROID (LaunchContacts, AKEYCODE_CONTACTS)

// LaunchMail
KEY_MAP_WIN     (LaunchMail, VK_LAUNCH_MAIL)
KEY_MAP_WIN_CMD (LaunchMail, APPCOMMAND_LAUNCH_MAIL)
KEY_MAP_GTK     (LaunchMail, GDK_Mail)
KEY_MAP_ANDROID (LaunchMail, AKEYCODE_ENVELOPE)

// LaunchMediaPlayer
KEY_MAP_WIN     (LaunchMediaPlayer, VK_LAUNCH_MEDIA_SELECT)
KEY_MAP_WIN_CMD (LaunchMediaPlayer, APPCOMMAND_LAUNCH_MEDIA_SELECT)
// GDK_CD is defined as "Launch CD/DVD player" in XF86keysym.h.
// Therefore, let's map it to media player rather than music player.
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_CD)
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_Video)
KEY_MAP_GTK     (LaunchMediaPlayer, GDK_AudioMedia)

// LaunchMusicPlayer
KEY_MAP_GTK     (LaunchMusicPlayer, GDK_Music)
KEY_MAP_ANDROID (LaunchMusicPlayer, AKEYCODE_MUSIC)

// LaunchMyComputer
KEY_MAP_GTK     (LaunchMyComputer, GDK_MyComputer)
KEY_MAP_GTK     (LaunchMyComputer, GDK_Explorer)

// LaunchScreenSaver
KEY_MAP_GTK     (LaunchScreenSaver, GDK_ScreenSaver)

// LaunchSpreadsheet
KEY_MAP_GTK     (LaunchSpreadsheet, GDK_Excel)

// LaunchWebBrowser
KEY_MAP_GTK     (LaunchWebBrowser, GDK_WWW)
KEY_MAP_ANDROID (LaunchWebBrowser, AKEYCODE_EXPLORER)

// LaunchWebCam
KEY_MAP_GTK     (LaunchWebCam, GDK_WebCam)

// LaunchWordProcessor
KEY_MAP_GTK     (LaunchWordProcessor, GDK_Word)

// LaunchApplication1
KEY_MAP_WIN     (LaunchApplication1, VK_LAUNCH_APP1)
KEY_MAP_WIN_CMD (LaunchApplication1, APPCOMMAND_LAUNCH_APP1)
KEY_MAP_GTK     (LaunchApplication1, GDK_Launch0)

// LaunchApplication2
KEY_MAP_WIN     (LaunchApplication2, VK_LAUNCH_APP2)
KEY_MAP_WIN_CMD (LaunchApplication2, APPCOMMAND_LAUNCH_APP2)
KEY_MAP_GTK     (LaunchApplication2, GDK_Launch1)

// LaunchApplication3
KEY_MAP_GTK     (LaunchApplication3, GDK_Launch2)

// LaunchApplication4
KEY_MAP_GTK     (LaunchApplication4, GDK_Launch3)

// LaunchApplication5
KEY_MAP_GTK     (LaunchApplication5, GDK_Launch4)

// LaunchApplication6
KEY_MAP_GTK     (LaunchApplication6, GDK_Launch5)

// LaunchApplication7
KEY_MAP_GTK     (LaunchApplication7, GDK_Launch6)

// LaunchApplication8
KEY_MAP_GTK     (LaunchApplication8, GDK_Launch7)

// LaunchApplication9
KEY_MAP_GTK     (LaunchApplication9, GDK_Launch8)

// LaunchApplication10
KEY_MAP_GTK     (LaunchApplication10, GDK_Launch9)

// LaunchApplication11
KEY_MAP_GTK     (LaunchApplication11, GDK_LaunchA)

// LaunchApplication12
KEY_MAP_GTK     (LaunchApplication12, GDK_LaunchB)

// LaunchApplication13
KEY_MAP_GTK     (LaunchApplication13, GDK_LaunchC)

// LaunchApplication14
KEY_MAP_GTK     (LaunchApplication14, GDK_LaunchD)

// LaunchApplication15
KEY_MAP_GTK     (LaunchApplication15, GDK_LaunchE)

// LaunchApplication16
KEY_MAP_GTK     (LaunchApplication16, GDK_LaunchF)

// LaunchApplication17

// LaunchApplication18

/******************************************************************************
 * Browser Keys
 ******************************************************************************/
// BrowserBack
KEY_MAP_WIN     (BrowserBack, VK_BROWSER_BACK)
KEY_MAP_WIN_CMD (BrowserBack, APPCOMMAND_BROWSER_BACKWARD)
KEY_MAP_GTK     (BrowserBack, GDK_Back)

// BrowserFavorites
KEY_MAP_WIN     (BrowserFavorites, VK_BROWSER_FAVORITES)
KEY_MAP_WIN_CMD (BrowserFavorites, APPCOMMAND_BROWSER_FAVORITES)
KEY_MAP_ANDROID (BrowserFavorites, AKEYCODE_BOOKMARK)

// BrowserForward
KEY_MAP_WIN     (BrowserForward, VK_BROWSER_FORWARD)
KEY_MAP_WIN_CMD (BrowserForward, APPCOMMAND_BROWSER_FORWARD)
KEY_MAP_GTK     (BrowserForward, GDK_Forward)
KEY_MAP_ANDROID (BrowserForward, AKEYCODE_FORWARD)

// BrowserHome
KEY_MAP_WIN     (BrowserHome, VK_BROWSER_HOME)
KEY_MAP_WIN_CMD (BrowserHome, APPCOMMAND_BROWSER_HOME)
KEY_MAP_GTK     (BrowserHome, GDK_HomePage)

// BrowserRefresh
KEY_MAP_WIN     (BrowserRefresh, VK_BROWSER_REFRESH)
KEY_MAP_WIN_CMD (BrowserRefresh, APPCOMMAND_BROWSER_REFRESH)
KEY_MAP_GTK     (BrowserRefresh, GDK_Refresh)
KEY_MAP_GTK     (BrowserRefresh, GDK_Reload)

// BrowserSearch
KEY_MAP_WIN     (BrowserSearch, VK_BROWSER_SEARCH)
KEY_MAP_WIN_CMD (BrowserSearch, APPCOMMAND_BROWSER_SEARCH)
KEY_MAP_GTK     (BrowserSearch, GDK_Search)
KEY_MAP_ANDROID (BrowserSearch, AKEYCODE_SEARCH)

// BrowserStop
KEY_MAP_WIN     (BrowserStop, VK_BROWSER_STOP)
KEY_MAP_WIN_CMD (BrowserStop, APPCOMMAND_BROWSER_STOP)
KEY_MAP_GTK     (BrowserStop, GDK_Stop)

/******************************************************************************
 * Mobile Phone Keys
 ******************************************************************************/
// AppSwitch
KEY_MAP_ANDROID (AppSwitch, AKEYCODE_APP_SWITCH)

// Call
KEY_MAP_ANDROID (Call, AKEYCODE_CALL)

// Camera
KEY_MAP_ANDROID (Camera, AKEYCODE_CAMERA)

// CameraFocus
KEY_MAP_ANDROID(CameraFocus, AKEYCODE_FOCUS)

// EndCall
KEY_MAP_ANDROID (EndCall, AKEYCODE_ENDCALL)

// GoBack
KEY_MAP_ANDROID (GoBack, AKEYCODE_BACK)

// GoHome
KEY_MAP_ANDROID(GoHome,     AKEYCODE_HOME)

// HeadsetHook
KEY_MAP_ANDROID (HeadsetHook, AKEYCODE_HEADSETHOOK)

// Notification
KEY_MAP_ANDROID (Notification, AKEYCODE_NOTIFICATION)

// MannerMode
KEY_MAP_ANDROID (MannerMode, AKEYCODE_MANNER_MODE)

/******************************************************************************
 * TV Keys
 ******************************************************************************/
// TV
KEY_MAP_ANDROID (TV, AKEYCODE_TV)

// TV3DMode
KEY_MAP_ANDROID (TV3DMode, AKEYCODE_3D_MODE)

// TVAntennaCable
KEY_MAP_ANDROID (TVAntennaCable, AKEYCODE_TV_ANTENNA_CABLE)

// TVAudioDescription
KEY_MAP_ANDROID (TVAudioDescription, AKEYCODE_TV_AUDIO_DESCRIPTION)

// TVAudioDescriptionMixDown
KEY_MAP_ANDROID (TVAudioDescriptionMixDown, AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN)

// TVAudioDescriptionMixUp
KEY_MAP_ANDROID (TVAudioDescriptionMixUp, AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP)

// TVContentsMenu
KEY_MAP_ANDROID (TVContentsMenu, AKEYCODE_TV_CONTENTS_MENU)

// TVDataService
KEY_MAP_ANDROID (TVDataService, AKEYCODE_TV_DATA_SERVICE)

// TVInput
KEY_MAP_ANDROID (TVInput, AKEYCODE_TV_INPUT)

// TVInputComponent1
KEY_MAP_ANDROID (TVInputComponent1, AKEYCODE_TV_INPUT_COMPONENT_1)

// TVInputComponent2
KEY_MAP_ANDROID (TVInputComponent2, AKEYCODE_TV_INPUT_COMPONENT_2)

// TVInputComposite1
KEY_MAP_ANDROID (TVInputComposite1, AKEYCODE_TV_INPUT_COMPOSITE_1)

// TVInputComposite2
KEY_MAP_ANDROID (TVInputComposite2, AKEYCODE_TV_INPUT_COMPOSITE_2)

// TVInputHDMI1
KEY_MAP_ANDROID (TVInputHDMI1, AKEYCODE_TV_INPUT_HDMI_1)

// TVInputHDMI2
KEY_MAP_ANDROID (TVInputHDMI2, AKEYCODE_TV_INPUT_HDMI_2)

// TVInputHDMI3
KEY_MAP_ANDROID (TVInputHDMI3, AKEYCODE_TV_INPUT_HDMI_3)

// TVInputHDMI4
KEY_MAP_ANDROID (TVInputHDMI4, AKEYCODE_TV_INPUT_HDMI_4)

// TVInputVGA1
KEY_MAP_ANDROID (TVInputVGA1, AKEYCODE_TV_INPUT_VGA_1)

// TVNetwork
KEY_MAP_ANDROID (TVNetwork, AKEYCODE_TV_NETWORK)

// TVNumberEntry
KEY_MAP_ANDROID (TVNumberEntry, AKEYCODE_TV_NUMBER_ENTRY)

// TVPower
KEY_MAP_ANDROID (TVPower, AKEYCODE_TV_POWER)

// TVRadioService
KEY_MAP_ANDROID (TVRadioService, AKEYCODE_TV_RADIO_SERVICE)

// TVSatellite
KEY_MAP_ANDROID (TVSatellite, AKEYCODE_TV_SATELLITE)

// TVSatelliteBS
KEY_MAP_ANDROID (TVSatelliteBS, AKEYCODE_TV_SATELLITE_BS)

// TVSatelliteCS
KEY_MAP_ANDROID (TVSatelliteCS, AKEYCODE_TV_SATELLITE_CS)

// TVSatelliteToggle
KEY_MAP_ANDROID (TVSatelliteToggle, AKEYCODE_TV_SATELLITE_SERVICE)

// TVTerrestrialAnalog
KEY_MAP_ANDROID (TVTerrestrialAnalog, AKEYCODE_TV_TERRESTRIAL_ANALOG)

// TVTerrestrialDigital
KEY_MAP_ANDROID (TVTerrestrialDigital, AKEYCODE_TV_TERRESTRIAL_DIGITAL)

// TVTimer
KEY_MAP_ANDROID (TVTimer, AKEYCODE_TV_TIMER_PROGRAMMING)

/******************************************************************************
 * Media Controller Keys
 ******************************************************************************/
// AVRInput
KEY_MAP_ANDROID (AVRInput, AKEYCODE_AVR_INPUT)

// AVRPower
KEY_MAP_ANDROID (AVRPower, AKEYCODE_AVR_POWER)

// ColorF0Red
KEY_MAP_GTK     (ColorF0Red, GDK_Red)
KEY_MAP_ANDROID (ColorF0Red, AKEYCODE_PROG_RED)

// ColorF1Green
KEY_MAP_GTK     (ColorF1Green, GDK_Green)
KEY_MAP_ANDROID (ColorF1Green, AKEYCODE_PROG_GREEN)

// ColorF2Yellow
KEY_MAP_GTK     (ColorF2Yellow, GDK_Yellow)
KEY_MAP_ANDROID (ColorF2Yellow, AKEYCODE_PROG_YELLOW)

// ColorF3Blue
KEY_MAP_GTK     (ColorF3Blue, GDK_Blue)
KEY_MAP_ANDROID (ColorF3Blue, AKEYCODE_PROG_BLUE)

// ClosedCaptionToggle
KEY_MAP_ANDROID (ClosedCaptionToggle, AKEYCODE_CAPTIONS)

// Dimmer
KEY_MAP_GTK     (Dimmer, GDK_BrightnessAdjust)

// DVR
KEY_MAP_ANDROID (DVR, AKEYCODE_DVR)

// Guide
KEY_MAP_ANDROID (Guide, AKEYCODE_GUIDE)

// Info
KEY_MAP_ANDROID (Info, AKEYCODE_INFO)

// MediaAudioTrack
KEY_MAP_ANDROID (MediaAudioTrack, AKEYCODE_MEDIA_AUDIO_TRACK)

// MediaLast
KEY_MAP_ANDROID (MediaLast, AKEYCODE_LAST_CHANNEL)

// MediaTopMenu
KEY_MAP_ANDROID (MediaTopMenu, AKEYCODE_MEDIA_TOP_MENU)

// MediaSkipBackward
KEY_MAP_ANDROID (MediaSkipBackward, AKEYCODE_MEDIA_SKIP_BACKWARD)

// MediaSkipForward
KEY_MAP_ANDROID (MediaSkipForward, AKEYCODE_MEDIA_SKIP_FORWARD)

// MediaStepBackward
KEY_MAP_ANDROID (MediaStepBackward, AKEYCODE_MEDIA_STEP_BACKWARD)

// MediaStepForward
KEY_MAP_ANDROID (MediaStepForward, AKEYCODE_MEDIA_STEP_FORWARD)

// NavigateIn
KEY_MAP_ANDROID (NavigateIn, AKEYCODE_NAVIGATE_IN)

// NavigateNext
KEY_MAP_ANDROID (NavigateNext, AKEYCODE_NAVIGATE_NEXT)

// NavigateOut
KEY_MAP_ANDROID (NavigateOut, AKEYCODE_NAVIGATE_OUT)

// NavigatePrevious
KEY_MAP_ANDROID (NavigatePrevious, AKEYCODE_NAVIGATE_PREVIOUS)

// Pairing
KEY_MAP_ANDROID (Pairing, AKEYCODE_PAIRING)

// PinPToggle
KEY_MAP_ANDROID (PinPToggle, AKEYCODE_WINDOW)

// RandomToggle
KEY_MAP_GTK     (RandomToggle, GDK_AudioRandomPlay)

// Settings
KEY_MAP_ANDROID (Settings, AKEYCODE_SETTINGS)

// STBInput
KEY_MAP_ANDROID (STBInput, AKEYCODE_STB_INPUT)

// STBPower
KEY_MAP_ANDROID (STBPower, AKEYCODE_STB_POWER)

// Subtitle
KEY_MAP_GTK     (Subtitle, GDK_Subtitle)

// Teletext
KEY_MAP_ANDROID (Teletext, AKEYCODE_TV_TELETEXT)

// VideoModeNext
KEY_MAP_GTK     (VideoModeNext, GDK_Next_VMode)

// ZoomToggle
KEY_MAP_WIN     (ZoomToggle, VK_ZOOM)
KEY_MAP_ANDROID (ZoomToggle, AKEYCODE_TV_ZOOM_MODE)

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
#undef KEY_MAP_ANDROID
