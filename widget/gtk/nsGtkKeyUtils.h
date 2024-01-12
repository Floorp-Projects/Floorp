/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGdkKeyUtils_h__
#define __nsGdkKeyUtils_h__

#include "mozilla/EventForwards.h"
#include "nsIWidget.h"
#include "nsTArray.h"

#include <gdk/gdk.h>
#ifdef MOZ_X11
#  include <X11/XKBlib.h>
#endif
#ifdef MOZ_WAYLAND
#  include <gdk/gdkwayland.h>
#  include <xkbcommon/xkbcommon.h>
#endif
#include "X11UndefineNone.h"

class nsWindow;

namespace mozilla {
namespace widget {

/**
 *  KeymapWrapper is a wrapper class of GdkKeymap.  GdkKeymap doesn't support
 *  all our needs, therefore, we need to access lower level APIs.
 *  But such code is usually complex and might be slow.  Against such issues,
 *  we should cache some information.
 *
 *  This class provides only static methods.  The methods is using internal
 *  singleton instance which is initialized by default GdkKeymap.  When the
 *  GdkKeymap is destroyed, the singleton instance will be destroyed.
 */

class KeymapWrapper {
 public:
  /**
   * Compute an our DOM keycode from a GDK keyval.
   */
  static uint32_t ComputeDOMKeyCode(const GdkEventKey* aGdkKeyEvent);

  /**
   * Compute a DOM key name index from aGdkKeyEvent.
   */
  static KeyNameIndex ComputeDOMKeyNameIndex(const GdkEventKey* aGdkKeyEvent);

  /**
   * Compute a DOM code name index from aGdkKeyEvent.
   */
  static CodeNameIndex ComputeDOMCodeNameIndex(const GdkEventKey* aGdkKeyEvent);

  /**
   * We need to translate modifiers masks from Gdk to Gecko.
   * MappedModifier is a table of mapped modifiers, we ignore other
   * Gdk ones.
   */
  enum MappedModifier {
    NOT_MODIFIER = 0x0000,
    CAPS_LOCK = 0x0001,
    NUM_LOCK = 0x0002,
    SCROLL_LOCK = 0x0004,
    SHIFT = 0x0008,
    CTRL = 0x0010,
    ALT = 0x0020,
    META = 0x0040,
    SUPER = 0x0080,
    HYPER = 0x0100,
    LEVEL3 = 0x0200,
    LEVEL5 = 0x0400
  };

  /**
   * MappedModifiers is used for combination of MappedModifier.
   * E.g., |MappedModifiers modifiers = (SHIFT | CTRL);| means Shift and Ctrl.
   */
  typedef uint32_t MappedModifiers;

  /**
   * GetCurrentModifierState() returns current modifier key state.
   * The "current" means actual state of hardware keyboard when this is
   * called.  I.e., if some key events are not still dispatched by GDK,
   * the state may mismatch with GdkEventKey::state.
   *
   * @return                  Current modifier key state.
   */
  static guint GetCurrentModifierState();

  /**
   * Utility function to compute current keyboard modifiers for
   * WidgetInputEvent
   */
  static uint32_t ComputeCurrentKeyModifiers();

  /**
   * Utility function to covert platform modifier state to keyboard modifiers
   * of WidgetInputEvent
   */
  static uint32_t ComputeKeyModifiers(guint aGdkModifierState);

  /**
   * Convert native modifiers for `nsIWidget::SynthesizeNative*()` to
   * GDK's state.
   */
  static guint ConvertWidgetModifierToGdkState(
      nsIWidget::Modifiers aNativeModifiers);

  /**
   * InitInputEvent() initializes the aInputEvent with aModifierState.
   */
  static void InitInputEvent(WidgetInputEvent& aInputEvent,
                             guint aGdkModifierState);

  /**
   * InitKeyEvent() intializes aKeyEvent's modifier key related members
   * and keycode related values.
   *
   * @param aKeyEvent         It's an WidgetKeyboardEvent which needs to be
   *                          initialized.
   * @param aGdkKeyEvent      A native GDK key event.
   * @param aIsProcessedByIME true if aGdkKeyEvent is handled by IME.
   */
  static void InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                           GdkEventKey* aGdkKeyEvent, bool aIsProcessedByIME);

  /**
   * DispatchKeyDownOrKeyUpEvent() dispatches eKeyDown or eKeyUp event.
   *
   * @param aWindow           The window to dispatch a keyboard event.
   * @param aGdkKeyEvent      A native GDK_KEY_PRESS or GDK_KEY_RELEASE
   *                          event.
   * @param aIsProcessedByIME true if the event is handled by IME.
   * @param aIsCancelled      [Out] true if the default is prevented.
   * @return                  true if eKeyDown event is actually dispatched.
   *                          Otherwise, false.
   */
  static bool DispatchKeyDownOrKeyUpEvent(nsWindow* aWindow,
                                          GdkEventKey* aGdkKeyEvent,
                                          bool aIsProcessedByIME,
                                          bool* aIsCancelled);

  /**
   * DispatchKeyDownOrKeyUpEvent() dispatches eKeyDown or eKeyUp event.
   *
   * @param aWindow           The window to dispatch aKeyboardEvent.
   * @param aKeyboardEvent    An eKeyDown or eKeyUp event.  This will be
   *                          dispatched as is.
   * @param aIsCancelled      [Out] true if the default is prevented.
   * @return                  true if eKeyDown event is actually dispatched.
   *                          Otherwise, false.
   */
  static bool DispatchKeyDownOrKeyUpEvent(nsWindow* aWindow,
                                          WidgetKeyboardEvent& aKeyboardEvent,
                                          bool* aIsCancelled);

  /**
   * GDK_KEY_PRESS event handler.
   *
   * @param aWindow           The window to dispatch eKeyDown event (and maybe
   *                          eKeyPress events).
   * @param aGdkKeyEvent      Receivied GDK_KEY_PRESS event.
   */
  static void HandleKeyPressEvent(nsWindow* aWindow, GdkEventKey* aGdkKeyEvent);

  /**
   * GDK_KEY_RELEASE event handler.
   *
   * @param aWindow           The window to dispatch eKeyUp event.
   * @param aGdkKeyEvent      Receivied GDK_KEY_RELEASE event.
   * @return                  true if an event is dispatched.  Otherwise, false.
   */
  static bool HandleKeyReleaseEvent(nsWindow* aWindow,
                                    GdkEventKey* aGdkKeyEvent);

  /**
   * WillDispatchKeyboardEvent() is called via
   * TextEventDispatcherListener::WillDispatchKeyboardEvent().
   *
   * @param aKeyEvent         An instance of KeyboardEvent which will be
   *                          dispatched.  This method should set charCode
   *                          and alternative char codes if it's necessary.
   * @param aGdkKeyEvent      A GdkEventKey instance which caused the
   *                          aKeyEvent.
   */
  static void WillDispatchKeyboardEvent(WidgetKeyboardEvent& aKeyEvent,
                                        GdkEventKey* aGdkKeyEvent);

#ifdef MOZ_WAYLAND
  /**
   * Utility function to set all supported modifier masks
   * from xkb_keymap. We call that from Wayland backend routines.
   */
  static void SetModifierMasks(xkb_keymap* aKeymap);

  /**
   * Wayland global focus handlers
   */
  static void SetFocusIn(wl_surface* aFocusSurface, uint32_t aFocusSerial);
  static void SetFocusOut(wl_surface* aFocusSurface);
  static void GetFocusInfo(wl_surface** aFocusSurface, uint32_t* aFocusSerial);

  static void SetSeat(wl_seat* aSeat, int aId);
  static void ClearSeat(int aId);
  static wl_seat* GetSeat();

  static void SetKeyboard(wl_keyboard* aKeyboard);
  static wl_keyboard* GetKeyboard();
  static void ClearKeyboard();

  /**
   * EnsureInstance() is provided on Wayland to register Wayland callbacks
   * early.
   */
  static void EnsureInstance();
#endif

  /**
   * ResetKeyboard is called on keymap changes from OnKeysChanged and
   * keyboard_handle_keymap to prepare for keymap changes.
   */
  static void ResetKeyboard();

  /**
   * Destroys the singleton KeymapWrapper instance, if it exists.
   */
  static void Shutdown();

 private:
  /**
   * GetInstance() returns a KeymapWrapper instance.
   *
   * @return                  A singleton instance of KeymapWrapper.
   */
  static KeymapWrapper* GetInstance();

  KeymapWrapper();
  ~KeymapWrapper();

  bool mInitialized;

  /**
   * Initializing methods.
   */
  void Init();
#ifdef MOZ_X11
  void InitXKBExtension();
  void InitBySystemSettingsX11();
#endif
#ifdef MOZ_WAYLAND
  void InitBySystemSettingsWayland();
#endif

  /**
   * mModifierKeys stores each hardware key information.
   */
  struct ModifierKey {
    guint mHardwareKeycode;
    guint mMask;

    explicit ModifierKey(guint aHardwareKeycode)
        : mHardwareKeycode(aHardwareKeycode), mMask(0) {}
  };
  nsTArray<ModifierKey> mModifierKeys;

  /**
   * GetModifierKey() returns modifier key information of the hardware
   * keycode.  If the key isn't a modifier key, returns nullptr.
   */
  ModifierKey* GetModifierKey(guint aHardwareKeycode);

  /**
   * mModifierMasks is bit masks for each modifier.  The index should be one
   * of ModifierIndex values.
   */
  enum ModifierIndex {
    INDEX_NUM_LOCK,
    INDEX_SCROLL_LOCK,
    INDEX_ALT,
    INDEX_META,
    INDEX_HYPER,
    INDEX_LEVEL3,
    INDEX_LEVEL5,
    COUNT_OF_MODIFIER_INDEX
  };
  guint mModifierMasks[COUNT_OF_MODIFIER_INDEX];

  guint GetGdkModifierMask(MappedModifier aModifier) const;

  /**
   * @param aGdkKeyval        A GDK defined modifier key value such as
   *                          GDK_Shift_L.
   * @return                  Returns MappedModifier values for aGdkKeyval.
   *                          If the given key code isn't a modifier key,
   *                          returns NOT_MODIFIER.
   */
  static MappedModifier GetModifierForGDKKeyval(guint aGdkKeyval);

  static const char* GetModifierName(MappedModifier aModifier);

  /**
   * AreModifiersActive() just checks whether aGdkModifierState indicates
   * all modifiers in aModifiers are active or not.
   *
   * @param aModifiers        One or more of MappedModifier values except
   *                          NOT_MODIFIER.
   * @param aGdkModifierState GDK's modifier states.
   * @return                  TRUE if aGdkModifierType indicates all of
   *                          modifiers in aModifier are active.
   *                          Otherwise, FALSE.
   */
  static bool AreModifiersActive(MappedModifiers aModifiers,
                                 guint aGdkModifierState);

  /**
   * mGdkKeymap is a wrapped instance by this class.
   */
  GdkKeymap* mGdkKeymap;

  /**
   * The base event code of XKB extension.
   */
  int mXKBBaseEventCode;

#ifdef MOZ_X11
  /**
   * Only auto_repeats[] stores valid value.  If you need to use other
   * members, you need to listen notification events for them.
   * See a call of XkbSelectEventDetails() with XkbControlsNotify in
   * InitXKBExtension().
   */
  XKeyboardState mKeyboardState;
#endif

  /**
   * Pointer of the singleton instance.
   */
  static KeymapWrapper* sInstance;

  /**
   * Auto key repeat management.
   */
  static guint sLastRepeatableHardwareKeyCode;
#ifdef MOZ_X11
  static Time sLastRepeatableKeyTime;
#endif
  enum RepeatState { NOT_PRESSED, FIRST_PRESS, REPEATING };
  static RepeatState sRepeatState;

  /**
   * IsAutoRepeatableKey() returns true if the key supports auto repeat.
   * Otherwise, false.
   */
  bool IsAutoRepeatableKey(guint aHardwareKeyCode);

  /**
   * Signal handlers.
   */
  static void OnKeysChanged(GdkKeymap* aKeymap, KeymapWrapper* aKeymapWrapper);
  static void OnDirectionChanged(GdkKeymap* aGdkKeymap,
                                 KeymapWrapper* aKeymapWrapper);

  gulong mOnKeysChangedSignalHandle;
  gulong mOnDirectionChangedSignalHandle;

  /**
   * GetCharCodeFor() Computes what character is inputted by the key event
   * with aModifierState and aGroup.
   *
   * @param aGdkKeyEvent      Native key event, must not be nullptr.
   * @param aModifierState    Combination of GdkModifierType which you
   *                          want to test with aGdkKeyEvent.
   * @param aGroup            Set group in the mGdkKeymap.
   * @return                  charCode which is inputted by aGdkKeyEvent.
   *                          If failed, this returns 0.
   */
  static uint32_t GetCharCodeFor(const GdkEventKey* aGdkKeyEvent);
  uint32_t GetCharCodeFor(const GdkEventKey* aGdkKeyEvent,
                          guint aGdkModifierState, gint aGroup);

  /**
   * GetUnmodifiedCharCodeFor() computes what character is inputted by the
   * key event without Ctrl/Alt/Meta/Super/Hyper modifiers.
   * If Level3 or Level5 Shift causes no character input, this also ignores
   * them.
   *
   * @param aGdkKeyEvent      Native key event, must not be nullptr.
   * @return                  charCode which is computed without modifiers
   *                          which prevent text input.
   */
  uint32_t GetUnmodifiedCharCodeFor(const GdkEventKey* aGdkKeyEvent);

  /**
   * GetKeyLevel() returns level of the aGdkKeyEvent in mGdkKeymap.
   *
   * @param aGdkKeyEvent      Native key event, must not be nullptr.
   * @return                  Using level.  Typically, this is 0 or 1.
   *                          If failed, this returns -1.
   */
  gint GetKeyLevel(GdkEventKey* aGdkKeyEvent);

  /**
   * GetFirstLatinGroup() returns group of mGdkKeymap which can input an
   * ASCII character by GDK_A.
   *
   * @return                  group value of GdkEventKey.
   */
  gint GetFirstLatinGroup();

  /**
   * IsLatinGroup() checkes whether the keyboard layout of aGroup is
   * ASCII alphabet inputtable or not.
   *
   * @param aGroup            The group value of GdkEventKey.
   * @return                  TRUE if the keyboard layout can input
   *                          ASCII alphabet.  Otherwise, FALSE.
   */
  bool IsLatinGroup(guint8 aGroup);

  /**
   * IsBasicLatinLetterOrNumeral() Checks whether the aCharCode is an
   * alphabet or a numeric character in ASCII.
   *
   * @param aCharCode         Charcode which you want to test.
   * @return                  TRUE if aCharCode is an alphabet or a numeric
   *                          in ASCII range.  Otherwise, FALSE.
   */
  static bool IsBasicLatinLetterOrNumeral(uint32_t aCharCode);

  /**
   * IsPrintableASCIICharacter() checks whether the aCharCode is a printable
   * ASCII character.  I.e., returns false if aCharCode is a control
   * character even in an ASCII character.
   */
  static bool IsPrintableASCIICharacter(uint32_t aCharCode) {
    return aCharCode >= 0x20 && aCharCode <= 0x7E;
  }

  /**
   * GetGDKKeyvalWithoutModifier() returns the keyval for aGdkKeyEvent when
   * ignoring the modifier state except NumLock. (NumLock is a key to change
   * some key's meaning.)
   */
  static guint GetGDKKeyvalWithoutModifier(const GdkEventKey* aGdkKeyEvent);

  /**
   * GetDOMKeyCodeFromKeyPairs() returns DOM keycode for aGdkKeyval if
   * it's in KeyPair table.
   */
  static uint32_t GetDOMKeyCodeFromKeyPairs(guint aGdkKeyval);

#ifdef MOZ_X11
  /**
   * FilterEvents() listens all events on all our windows.
   * Be careful, this may make damage to performance if you add expensive
   * code in this method.
   */
  static GdkFilterReturn FilterEvents(GdkXEvent* aXEvent, GdkEvent* aGdkEvent,
                                      gpointer aData);
#endif

  /**
   * MaybeDispatchContextMenuEvent() may dispatch eContextMenu event if
   * the given key combination should cause opening context menu.
   *
   * @param aWindow           The window to dispatch a contextmenu event.
   * @param aEvent            The native key event.
   * @return                  true if this method dispatched eContextMenu
   *                          event.  Otherwise, false.
   *                          Be aware, when this returns true, the
   *                          widget may have been destroyed.
   */
  static bool MaybeDispatchContextMenuEvent(nsWindow* aWindow,
                                            const GdkEventKey* aEvent);

  /**
   * See the document of WillDispatchKeyboardEvent().
   */
  void WillDispatchKeyboardEventInternal(WidgetKeyboardEvent& aKeyEvent,
                                         GdkEventKey* aGdkKeyEvent);

  static guint GetModifierState(GdkEventKey* aGdkKeyEvent,
                                KeymapWrapper* aWrapper);

#ifdef MOZ_WAYLAND
  /**
   * Utility function to set Xkb modifier key mask.
   */
  void SetModifierMask(xkb_keymap* aKeymap, ModifierIndex aModifierIndex,
                       const char* aModifierName);
#endif

#ifdef MOZ_WAYLAND
  static wl_seat* sSeat;
  static int sSeatID;
  static wl_keyboard* sKeyboard;
  wl_surface* mFocusSurface = nullptr;
  uint32_t mFocusSerial = 0;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif /* __nsGdkKeyUtils_h__ */
