/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGdkKeyUtils_h__
#define __nsGdkKeyUtils_h__

#include "nsTArray.h"
#include "mozilla/EventForwards.h"

#include <gdk/gdk.h>
#include <X11/XKBlib.h>

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

class KeymapWrapper
{
public:
    /**
     * Compute an our DOM keycode from a GDK keyval.
     */
    static uint32_t ComputeDOMKeyCode(const GdkEventKey* aGdkKeyEvent);

    /**
     * Compute a DOM key name index from aGdkKeyEvent.
     */
    KeyNameIndex ComputeDOMKeyNameIndex(const GdkEventKey* aGdkKeyEvent);

    /**
     * Modifier is list of modifiers which we support in widget level.
     */
    enum Modifier {
        NOT_MODIFIER       = 0x0000,
        CAPS_LOCK          = 0x0001,
        NUM_LOCK           = 0x0002,
        SCROLL_LOCK        = 0x0004,
        SHIFT              = 0x0008,
        CTRL               = 0x0010,
        ALT                = 0x0020,
        META               = 0x0040,
        SUPER              = 0x0080,
        HYPER              = 0x0100,
        LEVEL3             = 0x0200,
        LEVEL5             = 0x0400
    };

    /**
     * Modifiers is used for combination of Modifier.
     * E.g., |Modifiers modifiers = (SHIFT | CTRL);| means Shift and Ctrl.
     */
    typedef uint32_t Modifiers;

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
     * AreModifiersCurrentlyActive() checks the "current" modifier state
     * on aGdkWindow with the keymap of the singleton instance.
     *
     * @param aModifiers        One or more of Modifier values except
     *                          NOT_MODIFIER.
     * @return                  TRUE if all of modifieres in aModifiers are
     *                          active.  Otherwise, FALSE.
     */
    static bool AreModifiersCurrentlyActive(Modifiers aModifiers);

    /**
     * AreModifiersActive() just checks whether aModifierState indicates
     * all modifiers in aModifiers are active or not.
     *
     * @param aModifiers        One or more of Modifier values except
     *                          NOT_MODIFIER.
     * @param aModifierState    GDK's modifier states.
     * @return                  TRUE if aGdkModifierType indecates all of
     *                          modifiers in aModifier are active.
     *                          Otherwise, FALSE.
     */
    static bool AreModifiersActive(Modifiers aModifiers,
                                   guint aModifierState);

    /**
     * InitInputEvent() initializes the aInputEvent with aModifierState.
     */
    static void InitInputEvent(WidgetInputEvent& aInputEvent,
                               guint aModifierState);

    /**
     * InitKeyEvent() intializes aKeyEvent's modifier key related members
     * and keycode related values.
     *
     * @param aKeyEvent         It's an WidgetKeyboardEvent which needs to be
     *                          initialized.
     * @param aGdkKeyEvent      A native GDK key event.
     */
    static void InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                             GdkEventKey* aGdkKeyEvent);

    /**
     * IsKeyPressEventNecessary() returns TRUE when aGdkKeyEvent should cause
     * a DOM keypress event.  Otherwise, FALSE.
     */
    static bool IsKeyPressEventNecessary(GdkEventKey* aGdkKeyEvent);

protected:

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
    void InitXKBExtension();
    void InitBySystemSettings();

    /**
     * mModifierKeys stores each hardware key information.
     */
    struct ModifierKey {
        guint mHardwareKeycode;
        guint mMask;

        ModifierKey(guint aHardwareKeycode) :
          mHardwareKeycode(aHardwareKeycode), mMask(0)
        {
        }
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
        INDEX_SUPER,
        INDEX_HYPER,
        INDEX_LEVEL3,
        INDEX_LEVEL5,
        COUNT_OF_MODIFIER_INDEX
    };
    guint mModifierMasks[COUNT_OF_MODIFIER_INDEX];

    guint GetModifierMask(Modifier aModifier) const;

    /**
     * @param aGdkKeyval        A GDK defined modifier key value such as
     *                          GDK_Shift_L.
     * @return                  Returns Modifier values for aGdkKeyval.
     *                          If the given key code isn't a modifier key,
     *                          returns NOT_MODIFIER.
     */
    static Modifier GetModifierForGDKKeyval(guint aGdkKeyval);

#ifdef PR_LOGGING
    static const char* GetModifierName(Modifier aModifier);
#endif // PR_LOGGING

    /**
     * mGdkKeymap is a wrapped instance by this class.
     */
    GdkKeymap* mGdkKeymap;

    /**
     * The base event code of XKB extension.
     */
    int mXKBBaseEventCode;

    /**
     * Only auto_repeats[] stores valid value.  If you need to use other
     * members, you need to listen notification events for them.
     * See a call of XkbSelectEventDetails() with XkbControlsNotify in
     * InitXKBExtension().
     */
    XKeyboardState mKeyboardState;

    /**
     * Pointer of the singleton instance.
     */
    static KeymapWrapper* sInstance;

    /**
     * Auto key repeat management.
     */
    static guint sLastRepeatableHardwareKeyCode;
    enum RepeatState
    {
        NOT_PRESSED,
        FIRST_PRESS,
        REPEATING
    };
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
    static void OnDestroyKeymap(KeymapWrapper* aKeymapWrapper,
                                GdkKeymap *aGdkKeymap);

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
    static uint32_t GetCharCodeFor(const GdkEventKey *aGdkKeyEvent);
    uint32_t GetCharCodeFor(const GdkEventKey *aGdkKeyEvent,
                            guint aModifierState,
                            gint aGroup);

    /**
     * GetKeyLevel() returns level of the aGdkKeyEvent in mGdkKeymap.
     *
     * @param aGdkKeyEvent      Native key event, must not be nullptr.
     * @return                  Using level.  Typically, this is 0 or 1.
     *                          If failed, this returns -1.
     */
    gint GetKeyLevel(GdkEventKey *aGdkKeyEvent);

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
     * GetGDKKeyvalWithoutModifier() returns the keyval for aGdkKeyEvent when
     * ignoring the modifier state except NumLock. (NumLock is a key to change
     * some key's meaning.)
     */
    static guint GetGDKKeyvalWithoutModifier(const GdkEventKey *aGdkKeyEvent);

    /**
     * GetDOMKeyCodeFromKeyPairs() returns DOM keycode for aGdkKeyval if
     * it's in KeyPair table.
     */
    static uint32_t GetDOMKeyCodeFromKeyPairs(guint aGdkKeyval);

    /**
     * InitKeypressEvent() intializes keyCode, charCode and
     * alternativeCharCodes of keypress event.
     *
     * @param aKeyEvent         An NS_KEY_PRESS event, must not be nullptr.
     *                          The modifier related members and keyCode must
     *                          be initialized already.
     * @param aGdkKeyEvent      A native key event which causes dispatching
     *                          aKeyEvent.
     */
    void InitKeypressEvent(WidgetKeyboardEvent& aKeyEvent,
                           GdkEventKey* aGdkKeyEvent);

    /**
     * FilterEvents() listens all events on all our windows.
     * Be careful, this may make damage to performance if you add expensive
     * code in this method.
     */
    static GdkFilterReturn FilterEvents(GdkXEvent* aXEvent,
                                        GdkEvent* aGdkEvent,
                                        gpointer aData);
};

} // namespace widget
} // namespace mozilla

#endif /* __nsGdkKeyUtils_h__ */
