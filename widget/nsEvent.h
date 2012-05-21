/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEvent_h__
#define nsEvent_h__

/*
 * This is in a separate header file because it needs to be included
 * in many places where including nsGUIEvent.h would bring in many
 * header files that are totally unnecessary.
 */

enum UIStateChangeType {
  UIStateChangeType_NoChange,
  UIStateChangeType_Set,
  UIStateChangeType_Clear
};

/**
 * Return status for event processors.
 */

enum nsEventStatus {  
    /// The event is ignored, do default processing
  nsEventStatus_eIgnore,            
    /// The event is consumed, don't do default processing
  nsEventStatus_eConsumeNoDefault, 
    /// The event is consumed, but do default processing
  nsEventStatus_eConsumeDoDefault  
};

/**
 * sizemode is an adjunct to widget size
 */
enum nsSizeMode {
  nsSizeMode_Normal = 0,
  nsSizeMode_Minimized,
  nsSizeMode_Maximized,
  nsSizeMode_Fullscreen
};

struct nsAlternativeCharCode;
struct nsTextRangeStyle;
struct nsTextRange;

class nsEvent;
class nsGUIEvent;
class nsScriptErrorEvent;
class nsSizeEvent;
class nsSizeModeEvent;
class nsZLevelEvent;
class nsPaintEvent;
class nsScrollbarEvent;
class nsScrollPortEvent;
class nsScrollAreaEvent;
class nsInputEvent;
class nsMouseEvent_base;
class nsMouseEvent;
class nsDragEvent;
#ifdef ACCESSIBILITY
class nsAccessibleEvent;
#endif
class nsKeyEvent;
class nsTextEvent;
class nsCompositionEvent;
class nsMouseScrollEvent;
class nsGestureNotifyEvent;
class nsQueryContentEvent;
class nsFocusEvent;
class nsSelectionEvent;
class nsContentCommandEvent;
class nsMozTouchEvent;
class nsTouchEvent;
class nsFormEvent;
class nsCommandEvent;
class nsUIEvent;
class nsSimpleGestureEvent;
class nsTransitionEvent;
class nsAnimationEvent;
class nsUIStateChangeEvent;
class nsPluginEvent;

namespace mozilla {
namespace widget {

// All modifier keys should be defined here.  This is used for managing
// modifier states for DOM Level 3 or later.
enum Modifier {
  MODIFIER_ALT        = 0x0001,
  MODIFIER_ALTGRAPH   = 0x0002,
  MODIFIER_CAPSLOCK   = 0x0004,
  MODIFIER_CONTROL    = 0x0008,
  MODIFIER_FN         = 0x0010,
  MODIFIER_META       = 0x0020,
  MODIFIER_NUMLOCK    = 0x0040,
  MODIFIER_SCROLL     = 0x0080,
  MODIFIER_SHIFT      = 0x0100,
  MODIFIER_SYMBOLLOCK = 0x0200,
  MODIFIER_WIN        = 0x0400
};

typedef PRUint16 Modifiers;

} // namespace widget
} // namespace mozilla

#define NS_DOM_KEYNAME_ALT        "Alt"
#define NS_DOM_KEYNAME_ALTGRAPH   "AltGraph"
#define NS_DOM_KEYNAME_CAPSLOCK   "CapsLock"
#define NS_DOM_KEYNAME_CONTROL    "Control"
#define NS_DOM_KEYNAME_FN         "Fn"
#define NS_DOM_KEYNAME_META       "Meta"
#define NS_DOM_KEYNAME_NUMLOCK    "NumLock"
#define NS_DOM_KEYNAME_SCROLL     "Scroll"
#define NS_DOM_KEYNAME_SHIFT      "Shift"
#define NS_DOM_KEYNAME_SYMBOLLOCK "SymbolLock"
#define NS_DOM_KEYNAME_WIN        "Win"

#endif // nsEvent_h__
