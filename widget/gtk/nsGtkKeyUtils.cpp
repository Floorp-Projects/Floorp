/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"

#include "nsGtkKeyUtils.h"

#include <gdk/gdkkeysyms.h>
#include <algorithm>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#if (MOZ_WIDGET_GTK == 3)
#include <gdk/gdkkeysyms-compat.h>
#endif
#include <X11/XKBlib.h>
#include "WidgetUtils.h"
#include "keysym2ucs.h"
#include "nsIBidiKeyboard.h"
#include "nsServiceManagerUtils.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gKeymapWrapperLog = nullptr;
#endif // PR_LOGGING

#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Util.h"

namespace mozilla {
namespace widget {

#define IS_ASCII_ALPHABETICAL(key) \
    ((('a' <= key) && (key <= 'z')) || (('A' <= key) && (key <= 'Z')))

#define MOZ_MODIFIER_KEYS "MozKeymapWrapper"

KeymapWrapper* KeymapWrapper::sInstance = nullptr;
nsIBidiKeyboard* sBidiKeyboard = nullptr;

#ifdef PR_LOGGING

static const char* GetBoolName(bool aBool)
{
    return aBool ? "TRUE" : "FALSE";
}

/* static */ const char*
KeymapWrapper::GetModifierName(Modifier aModifier)
{
    switch (aModifier) {
        case CAPS_LOCK:    return "CapsLock";
        case NUM_LOCK:     return "NumLock";
        case SCROLL_LOCK:  return "ScrollLock";
        case SHIFT:        return "Shift";
        case CTRL:         return "Ctrl";
        case ALT:          return "Alt";
        case SUPER:        return "Super";
        case HYPER:        return "Hyper";
        case META:         return "Meta";
        case LEVEL3:       return "Level3";
        case LEVEL5:       return "Level5";
        case NOT_MODIFIER: return "NotModifier";
        default:           return "InvalidValue";
    }
}

#endif // PR_LOGGING

/* static */ KeymapWrapper::Modifier
KeymapWrapper::GetModifierForGDKKeyval(guint aGdkKeyval)
{
    switch (aGdkKeyval) {
        case GDK_Caps_Lock:        return CAPS_LOCK;
        case GDK_Num_Lock:         return NUM_LOCK;
        case GDK_Scroll_Lock:      return SCROLL_LOCK;
        case GDK_Shift_Lock:
        case GDK_Shift_L:
        case GDK_Shift_R:          return SHIFT;
        case GDK_Control_L:
        case GDK_Control_R:        return CTRL;
        case GDK_Alt_L:
        case GDK_Alt_R:            return ALT;
        case GDK_Super_L:
        case GDK_Super_R:          return SUPER;
        case GDK_Hyper_L:
        case GDK_Hyper_R:          return HYPER;
        case GDK_Meta_L:
        case GDK_Meta_R:           return META;
        case GDK_ISO_Level3_Shift:
        case GDK_Mode_switch:      return LEVEL3;
        case GDK_ISO_Level5_Shift: return LEVEL5;
        default:                   return NOT_MODIFIER;
    }
}

guint
KeymapWrapper::GetModifierMask(Modifier aModifier) const
{
    switch (aModifier) {
        case CAPS_LOCK:
            return GDK_LOCK_MASK;
        case NUM_LOCK:
            return mModifierMasks[INDEX_NUM_LOCK];
        case SCROLL_LOCK:
            return mModifierMasks[INDEX_SCROLL_LOCK];
        case SHIFT:
            return GDK_SHIFT_MASK;
        case CTRL:
            return GDK_CONTROL_MASK;
        case ALT:
            return mModifierMasks[INDEX_ALT];
        case SUPER:
            return mModifierMasks[INDEX_SUPER];
        case HYPER:
            return mModifierMasks[INDEX_HYPER];
        case META:
            return mModifierMasks[INDEX_META];
        case LEVEL3:
            return mModifierMasks[INDEX_LEVEL3];
        case LEVEL5:
            return mModifierMasks[INDEX_LEVEL5];
        default:
            return 0;
    }
}

KeymapWrapper::ModifierKey*
KeymapWrapper::GetModifierKey(guint aHardwareKeycode)
{
    for (uint32_t i = 0; i < mModifierKeys.Length(); i++) {
        ModifierKey& key = mModifierKeys[i];
        if (key.mHardwareKeycode == aHardwareKeycode) {
            return &key;
        }
    }
    return nullptr;
}

/* static */ KeymapWrapper*
KeymapWrapper::GetInstance()
{
    if (sInstance) {
        sInstance->Init();
        return sInstance;
    }

    sInstance = new KeymapWrapper();
    return sInstance;
}

KeymapWrapper::KeymapWrapper() :
    mInitialized(false), mGdkKeymap(gdk_keymap_get_default()),
    mXKBBaseEventCode(0)
{
#ifdef PR_LOGGING
    if (!gKeymapWrapperLog) {
        gKeymapWrapperLog = PR_NewLogModule("KeymapWrapperWidgets");
    }
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): Constructor, mGdkKeymap=%p",
         this, mGdkKeymap));
#endif // PR_LOGGING

    g_signal_connect(mGdkKeymap, "keys-changed",
                     (GCallback)OnKeysChanged, this);

    // This is necessary for catching the destroying timing.
    g_object_weak_ref(G_OBJECT(mGdkKeymap),
                      (GWeakNotify)OnDestroyKeymap, this);

    InitXKBExtension();

    Init();
}

void
KeymapWrapper::Init()
{
    if (mInitialized) {
        return;
    }
    mInitialized = true;

    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): Init, mGdkKeymap=%p",
         this, mGdkKeymap));

    mModifierKeys.Clear();
    memset(mModifierMasks, 0, sizeof(mModifierMasks));

    InitBySystemSettings();

    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): Init, CapsLock=0x%X, NumLock=0x%X, "
         "ScrollLock=0x%X, Level3=0x%X, Level5=0x%X, "
         "Shift=0x%X, Ctrl=0x%X, Alt=0x%X, Meta=0x%X, Super=0x%X, Hyper=0x%X",
         this,
         GetModifierMask(CAPS_LOCK), GetModifierMask(NUM_LOCK),
         GetModifierMask(SCROLL_LOCK), GetModifierMask(LEVEL3),
         GetModifierMask(LEVEL5),
         GetModifierMask(SHIFT), GetModifierMask(CTRL),
         GetModifierMask(ALT), GetModifierMask(META),
         GetModifierMask(SUPER), GetModifierMask(HYPER)));
}

void
KeymapWrapper::InitXKBExtension()
{
    int xkbMajorVer = XkbMajorVersion;
    int xkbMinorVer = XkbMinorVersion;
    if (!XkbLibraryVersion(&xkbMajorVer, &xkbMinorVer)) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
               ("KeymapWrapper(%p): InitXKBExtension failed due to failure of "
                "XkbLibraryVersion()", this));
        return;
    }

    Display* display =
        gdk_x11_display_get_xdisplay(gdk_display_get_default());

    // XkbLibraryVersion() set xkbMajorVer and xkbMinorVer to that of the
    // library, which may be newer than what is required of the server in
    // XkbQueryExtension(), so these variables should be reset to
    // XkbMajorVersion and XkbMinorVersion before the XkbQueryExtension call.
    xkbMajorVer = XkbMajorVersion;
    xkbMinorVer = XkbMinorVersion;
    int opcode, baseErrorCode;
    if (!XkbQueryExtension(display, &opcode, &mXKBBaseEventCode, &baseErrorCode,
                           &xkbMajorVer, &xkbMinorVer)) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
               ("KeymapWrapper(%p): InitXKBExtension failed due to failure of "
                "XkbQueryExtension(), display=0x%p", this, display));
        return;
    }

    if (!XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify,
                               XkbModifierStateMask, XkbModifierStateMask)) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
               ("KeymapWrapper(%p): InitXKBExtension failed due to failure of "
                "XkbSelectEventDetails(), display=0x%p", this, display));
        return;
    }

    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
           ("KeymapWrapper(%p): InitXKBExtension, Succeeded", this));
}

void
KeymapWrapper::InitBySystemSettings()
{
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
      ("KeymapWrapper(%p): InitBySystemSettings, mGdkKeymap=%p",
       this, mGdkKeymap));

    Display* display =
        gdk_x11_display_get_xdisplay(gdk_display_get_default());

    int min_keycode = 0;
    int max_keycode = 0;
    XDisplayKeycodes(display, &min_keycode, &max_keycode);

    int keysyms_per_keycode = 0;
    KeySym* xkeymap = XGetKeyboardMapping(display, min_keycode,
                                          max_keycode - min_keycode + 1,
                                          &keysyms_per_keycode);
    if (!xkeymap) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitBySystemSettings, "
             "Failed due to null xkeymap", this));
        return;
    }

    XModifierKeymap* xmodmap = XGetModifierMapping(display);
    if (!xmodmap) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitBySystemSettings, "
             "Failed due to null xmodmap", this));
        XFree(xkeymap);
        return;
    }
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): InitBySystemSettings, min_keycode=%d, "
         "max_keycode=%d, keysyms_per_keycode=%d, max_keypermod=%d",
         this, min_keycode, max_keycode, keysyms_per_keycode,
         xmodmap->max_keypermod));

    // The modifiermap member of the XModifierKeymap structure contains 8 sets
    // of max_keypermod KeyCodes, one for each modifier in the order Shift,
    // Lock, Control, Mod1, Mod2, Mod3, Mod4, and Mod5.
    // Only nonzero KeyCodes have meaning in each set, and zero KeyCodes are
    // ignored.

    // Note that two or more modifiers may use one modifier flag.  E.g.,
    // on Ubuntu 10.10, Alt and Meta share the Mod1 in default settings.
    // And also Super and Hyper share the Mod4. In such cases, we need to
    // decide which modifier flag means one of DOM modifiers.

    // mod[0] is Modifier introduced by Mod1.
    Modifier mod[5];
    int32_t foundLevel[5];
    for (uint32_t i = 0; i < ArrayLength(mod); i++) {
        mod[i] = NOT_MODIFIER;
        foundLevel[i] = INT32_MAX;
    }
    const uint32_t map_size = 8 * xmodmap->max_keypermod;
    for (uint32_t i = 0; i < map_size; i++) {
        KeyCode keycode = xmodmap->modifiermap[i];
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitBySystemSettings, "
             "  i=%d, keycode=0x%08X",
             this, i, keycode));
        if (!keycode || keycode < min_keycode || keycode > max_keycode) {
            continue;
        }

        ModifierKey* modifierKey = GetModifierKey(keycode);
        if (!modifierKey) {
            modifierKey = mModifierKeys.AppendElement(ModifierKey(keycode));
        }

        const KeySym* syms =
            xkeymap + (keycode - min_keycode) * keysyms_per_keycode;
        const uint32_t bit = i / xmodmap->max_keypermod;
        modifierKey->mMask |= 1 << bit;

        // We need to know the meaning of Mod1, Mod2, Mod3, Mod4 and Mod5.
        // Let's skip if current map is for others.
        if (bit < 3) {
            continue;
        }

        const int32_t modIndex = bit - 3;
        for (int32_t j = 0; j < keysyms_per_keycode; j++) {
            Modifier modifier = GetModifierForGDKKeyval(syms[j]);
            PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
                ("KeymapWrapper(%p): InitBySystemSettings, "
                 "    Mod%d, j=%d, syms[j]=%s(0x%X), modifier=%s",
                 this, modIndex + 1, j, gdk_keyval_name(syms[j]), syms[j],
                 GetModifierName(modifier)));

            switch (modifier) {
                case NOT_MODIFIER:
                    // Don't overwrite the stored information with
                    // NOT_MODIFIER.
                    break;
                case CAPS_LOCK:
                case SHIFT:
                case CTRL:
                    // Ignore the modifiers defined in GDK spec. They shouldn't
                    // be mapped to Mod1-5 because they must not work on native
                    // GTK applications.
                    break;
                default:
                    // If new modifier is found in higher level than stored
                    // value, we don't need to overwrite it.
                    if (j > foundLevel[modIndex]) {
                        break;
                    }
                    // If new modifier is more important than stored value,
                    // we should overwrite it with new modifier.
                    if (j == foundLevel[modIndex]) {
                        mod[modIndex] = std::min(modifier, mod[modIndex]);
                        break;
                    }
                    foundLevel[modIndex] = j;
                    mod[modIndex] = modifier;
                    break;
            }
        }
    }

    for (uint32_t i = 0; i < COUNT_OF_MODIFIER_INDEX; i++) {
        Modifier modifier;
        switch (i) {
            case INDEX_NUM_LOCK:
                modifier = NUM_LOCK;
                break;
            case INDEX_SCROLL_LOCK:
                modifier = SCROLL_LOCK;
                break;
            case INDEX_ALT:
                modifier = ALT;
                break;
            case INDEX_META:
                modifier = META;
                break;
            case INDEX_SUPER:
                modifier = SUPER;
                break;
            case INDEX_HYPER:
                modifier = HYPER;
                break;
            case INDEX_LEVEL3:
                modifier = LEVEL3;
                break;
            case INDEX_LEVEL5:
                modifier = LEVEL5;
                break;
            default:
                MOZ_CRASH("All indexes must be handled here");
        }
        for (uint32_t j = 0; j < ArrayLength(mod); j++) {
            if (modifier == mod[j]) {
                mModifierMasks[i] |= 1 << (j + 3);
            }
        }
    }

    XFreeModifiermap(xmodmap);
    XFree(xkeymap);
}

KeymapWrapper::~KeymapWrapper()
{
    NS_IF_RELEASE(sBidiKeyboard);
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): Destructor", this));
}

/* static */ void
KeymapWrapper::OnDestroyKeymap(KeymapWrapper* aKeymapWrapper,
                               GdkKeymap *aGdkKeymap)
{
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper: OnDestroyKeymap, aGdkKeymap=%p, aKeymapWrapper=%p",
         aGdkKeymap, aKeymapWrapper));
    MOZ_ASSERT(aKeymapWrapper == sInstance,
               "Desroying unexpected instance");
    delete sInstance;
    sInstance = nullptr;
}

/* static */ void
KeymapWrapper::OnKeysChanged(GdkKeymap *aGdkKeymap,
                             KeymapWrapper* aKeymapWrapper)
{
    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper: OnKeysChanged, aGdkKeymap=%p, aKeymapWrapper=%p",
         aGdkKeymap, aKeymapWrapper));

    MOZ_ASSERT(sInstance == aKeymapWrapper,
               "This instance must be the singleton instance");

    // We cannot reintialize here becasue we don't have GdkWindow which is using
    // the GdkKeymap.  We'll reinitialize it when next GetInstance() is called.
    sInstance->mInitialized = false;

    // Reset the bidi keyboard settings for the new GdkKeymap
    if (!sBidiKeyboard) {
        CallGetService("@mozilla.org/widget/bidikeyboard;1", &sBidiKeyboard);
    }
    if (sBidiKeyboard) {
        sBidiKeyboard->Reset();
    }
}

/* static */ guint
KeymapWrapper::GetCurrentModifierState()
{
    GdkModifierType modifiers;
    gdk_display_get_pointer(gdk_display_get_default(),
                            nullptr, nullptr, nullptr, &modifiers);
    return static_cast<guint>(modifiers);
}

/* static */ bool
KeymapWrapper::AreModifiersCurrentlyActive(Modifiers aModifiers)
{
    guint modifierState = GetCurrentModifierState();
    return AreModifiersActive(aModifiers, modifierState);
}

/* static */ bool
KeymapWrapper::AreModifiersActive(Modifiers aModifiers,
                                  guint aModifierState)
{
    NS_ENSURE_TRUE(aModifiers, false);

    KeymapWrapper* keymapWrapper = GetInstance();
    for (uint32_t i = 0; i < sizeof(Modifier) * 8 && aModifiers; i++) {
        Modifier modifier = static_cast<Modifier>(1 << i);
        if (!(aModifiers & modifier)) {
            continue;
        }
        if (!(aModifierState & keymapWrapper->GetModifierMask(modifier))) {
            return false;
        }
        aModifiers &= ~modifier;
    }
    return true;
}

/* static */ void
KeymapWrapper::InitInputEvent(WidgetInputEvent& aInputEvent,
                              guint aModifierState)
{
    KeymapWrapper* keymapWrapper = GetInstance();

    aInputEvent.modifiers = 0;
    // DOM Meta key should be TRUE only on Mac.  We need to discuss this
    // issue later.
    if (keymapWrapper->AreModifiersActive(SHIFT, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_SHIFT;
    }
    if (keymapWrapper->AreModifiersActive(CTRL, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_CONTROL;
    }
    if (keymapWrapper->AreModifiersActive(ALT, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_ALT;
    }
    if (keymapWrapper->AreModifiersActive(META, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_META;
    }
    if (keymapWrapper->AreModifiersActive(SUPER, aModifierState) ||
        keymapWrapper->AreModifiersActive(HYPER, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_OS;
    }
    if (keymapWrapper->AreModifiersActive(LEVEL3, aModifierState) ||
        keymapWrapper->AreModifiersActive(LEVEL5, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_ALTGRAPH;
    }
    if (keymapWrapper->AreModifiersActive(CAPS_LOCK, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_CAPSLOCK;
    }
    if (keymapWrapper->AreModifiersActive(NUM_LOCK, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_NUMLOCK;
    }
    if (keymapWrapper->AreModifiersActive(SCROLL_LOCK, aModifierState)) {
        aInputEvent.modifiers |= MODIFIER_SCROLLLOCK;
    }

    PR_LOG(gKeymapWrapperLog, PR_LOG_DEBUG,
        ("KeymapWrapper(%p): InitInputEvent, aModifierState=0x%08X, "
         "aInputEvent.modifiers=0x%04X (Shift: %s, Control: %s, Alt: %s, "
         "Meta: %s, OS: %s, AltGr: %s, "
         "CapsLock: %s, NumLock: %s, ScrollLock: %s)",
         keymapWrapper, aModifierState, aInputEvent.modifiers,
         GetBoolName(aInputEvent.modifiers & MODIFIER_SHIFT),
         GetBoolName(aInputEvent.modifiers & MODIFIER_CONTROL),
         GetBoolName(aInputEvent.modifiers & MODIFIER_ALT),
         GetBoolName(aInputEvent.modifiers & MODIFIER_META),
         GetBoolName(aInputEvent.modifiers & MODIFIER_OS),
         GetBoolName(aInputEvent.modifiers & MODIFIER_ALTGRAPH),
         GetBoolName(aInputEvent.modifiers & MODIFIER_CAPSLOCK),
         GetBoolName(aInputEvent.modifiers & MODIFIER_NUMLOCK),
         GetBoolName(aInputEvent.modifiers & MODIFIER_SCROLLLOCK)));

    switch(aInputEvent.eventStructType) {
        case NS_MOUSE_EVENT:
        case NS_MOUSE_SCROLL_EVENT:
        case NS_WHEEL_EVENT:
        case NS_DRAG_EVENT:
        case NS_SIMPLE_GESTURE_EVENT:
            break;
        default:
            return;
    }

    WidgetMouseEventBase& mouseEvent =
      static_cast<WidgetMouseEventBase&>(aInputEvent);
    mouseEvent.buttons = 0;
    if (aModifierState & GDK_BUTTON1_MASK) {
        mouseEvent.buttons |= WidgetMouseEvent::eLeftButtonFlag;
    }
    if (aModifierState & GDK_BUTTON3_MASK) {
        mouseEvent.buttons |= WidgetMouseEvent::eRightButtonFlag;
    }
    if (aModifierState & GDK_BUTTON2_MASK) {
        mouseEvent.buttons |= WidgetMouseEvent::eMiddleButtonFlag;
    }

    PR_LOG(gKeymapWrapperLog, PR_LOG_DEBUG,
        ("KeymapWrapper(%p): InitInputEvent, aInputEvent has buttons, "
         "aInputEvent.buttons=0x%04X (Left: %s, Right: %s, Middle: %s, "
         "4th (BACK): %s, 5th (FORWARD): %s)",
         keymapWrapper, mouseEvent.buttons,
         GetBoolName(mouseEvent.buttons & WidgetMouseEvent::eLeftButtonFlag),
         GetBoolName(mouseEvent.buttons & WidgetMouseEvent::eRightButtonFlag),
         GetBoolName(mouseEvent.buttons & WidgetMouseEvent::eMiddleButtonFlag),
         GetBoolName(mouseEvent.buttons & WidgetMouseEvent::e4thButtonFlag),
         GetBoolName(mouseEvent.buttons & WidgetMouseEvent::e5thButtonFlag)));
}

/* static */ uint32_t
KeymapWrapper::ComputeDOMKeyCode(const GdkEventKey* aGdkKeyEvent)
{
    // If the keyval indicates it's a modifier key, we should use unshifted
    // key's modifier keyval.
    guint keyval = aGdkKeyEvent->keyval;
    if (GetModifierForGDKKeyval(keyval)) {
        // But if the keyval without modifiers isn't a modifier key, we
        // shouldn't use it.  E.g., Japanese keyboard layout's
        // Shift + Eisu-Toggle key is CapsLock.  This is an actual rare case,
        // Windows uses different keycode for a physical key for different
        // shift key state.
        guint keyvalWithoutModifier = GetGDKKeyvalWithoutModifier(aGdkKeyEvent);
        if (GetModifierForGDKKeyval(keyvalWithoutModifier)) {
            keyval = keyvalWithoutModifier;
        }
        // Note that the modifier keycode and activating or deactivating
        // modifier flag may be mismatched, but it's okay.  If a DOM key
        // event handler is testing a keydown event, it's more likely being
        // used to test which key is being pressed than to test which
        // modifier will become active.  So, if we computed DOM keycode
        // from modifier flag which were changing by the physical key, then
        // there would be no other way for the user to generate the original
        // keycode.
        uint32_t DOMKeyCode = GetDOMKeyCodeFromKeyPairs(keyval);
        NS_ASSERTION(DOMKeyCode, "All modifier keys must have a DOM keycode");
        return DOMKeyCode;
    }

    // If the key isn't printable, let's look at the key pairs.
    uint32_t charCode = GetCharCodeFor(aGdkKeyEvent);
    if (!charCode) {
        // Always use unshifted keycode for the non-printable key.
        // XXX It might be better to decide DOM keycode from all keyvals of
        //     the hardware keycode.  However, I think that it's too excessive.
        guint keyvalWithoutModifier = GetGDKKeyvalWithoutModifier(aGdkKeyEvent);
        uint32_t DOMKeyCode = GetDOMKeyCodeFromKeyPairs(keyvalWithoutModifier);
        if (!DOMKeyCode) {
            // If the unshifted keyval couldn't be mapped to a DOM keycode,
            // we should fallback to legacy logic, so, we should recompute with
            // the keyval with aGdkKeyEvent.
            DOMKeyCode = GetDOMKeyCodeFromKeyPairs(keyval);
        }
        return DOMKeyCode;
    }

    // printable numpad keys should be resolved here.
    switch (keyval) {
        case GDK_KP_Multiply:  return NS_VK_MULTIPLY;
        case GDK_KP_Add:       return NS_VK_ADD;
        case GDK_KP_Separator: return NS_VK_SEPARATOR;
        case GDK_KP_Subtract:  return NS_VK_SUBTRACT;
        case GDK_KP_Decimal:   return NS_VK_DECIMAL;
        case GDK_KP_Divide:    return NS_VK_DIVIDE;
        case GDK_KP_0:         return NS_VK_NUMPAD0;
        case GDK_KP_1:         return NS_VK_NUMPAD1;
        case GDK_KP_2:         return NS_VK_NUMPAD2;
        case GDK_KP_3:         return NS_VK_NUMPAD3;
        case GDK_KP_4:         return NS_VK_NUMPAD4;
        case GDK_KP_5:         return NS_VK_NUMPAD5;
        case GDK_KP_6:         return NS_VK_NUMPAD6;
        case GDK_KP_7:         return NS_VK_NUMPAD7;
        case GDK_KP_8:         return NS_VK_NUMPAD8;
        case GDK_KP_9:         return NS_VK_NUMPAD9;
    }

    KeymapWrapper* keymapWrapper = GetInstance();

    // Ignore all modifier state except NumLock.
    guint baseState =
        (aGdkKeyEvent->state & keymapWrapper->GetModifierMask(NUM_LOCK));

    // Basically, we should use unmodified character for deciding our keyCode.
    uint32_t unmodifiedChar =
        keymapWrapper->GetCharCodeFor(aGdkKeyEvent, baseState,
                                      aGdkKeyEvent->group);
    if (IsBasicLatinLetterOrNumeral(unmodifiedChar)) {
        // If the unmodified character is an ASCII alphabet or an ASCII
        // numeric, it's the best hint for deciding our keyCode.
        return WidgetUtils::ComputeKeyCodeFromChar(unmodifiedChar);
    }

    // If the unmodified character is not an ASCII character, that means we
    // couldn't find the hint. We should reset it.
    if (unmodifiedChar > 0x7F) {
        unmodifiedChar = 0;
    }

    // Retry with shifted keycode.
    guint shiftState = (baseState | keymapWrapper->GetModifierMask(SHIFT));
    uint32_t shiftedChar =
        keymapWrapper->GetCharCodeFor(aGdkKeyEvent, shiftState,
                                      aGdkKeyEvent->group);
    if (IsBasicLatinLetterOrNumeral(shiftedChar)) {
        // A shifted character can be an ASCII alphabet on Hebrew keyboard
        // layout. And also shifted character can be an ASCII numeric on
        // AZERTY keyboad layout.  Then, it's a good hint for deciding our
        // keyCode.
        return WidgetUtils::ComputeKeyCodeFromChar(shiftedChar);
    }

    // If the shifted unmodified character isn't an ASCII character, we should
    // discard it too.
    if (shiftedChar > 0x7F) {
        shiftedChar = 0;
    }

    // If current keyboard layout isn't ASCII alphabet inputtable layout,
    // look for ASCII alphabet inputtable keyboard layout.  If the key
    // inputs an ASCII alphabet or an ASCII numeric, we should use it
    // for deciding our keyCode.
    // Note that it's important not to use alternative keyboard layout for ASCII
    // alphabet inputabble keyboard layout because the keycode for the key with
    // alternative keyboard layout may conflict with another key on current
    // keyboard layout.
    if (!keymapWrapper->IsLatinGroup(aGdkKeyEvent->group)) {
        gint minGroup = keymapWrapper->GetFirstLatinGroup();
        if (minGroup >= 0) {
            uint32_t unmodCharLatin =
                keymapWrapper->GetCharCodeFor(aGdkKeyEvent, baseState,
                                              minGroup);
            if (IsBasicLatinLetterOrNumeral(unmodCharLatin)) {
                // If the unmodified character is an ASCII alphabet or
                // an ASCII numeric, we should use it for the keyCode.
                return WidgetUtils::ComputeKeyCodeFromChar(unmodCharLatin);
            }
            uint32_t shiftedCharLatin =
                keymapWrapper->GetCharCodeFor(aGdkKeyEvent, shiftState,
                                              minGroup);
            if (IsBasicLatinLetterOrNumeral(shiftedCharLatin)) {
                // If the shifted character is an ASCII alphabet or an ASCII
                // numeric, we should use it for the keyCode.
                return WidgetUtils::ComputeKeyCodeFromChar(shiftedCharLatin);
            }
        }
    }

    // If unmodified character is in ASCII range, use it.  Otherwise, use
    // shifted character.
    if (!unmodifiedChar && !shiftedChar) {
        return 0;
    }
    return WidgetUtils::ComputeKeyCodeFromChar(
                unmodifiedChar ? unmodifiedChar : shiftedChar);
}

KeyNameIndex
KeymapWrapper::ComputeDOMKeyNameIndex(const GdkEventKey* aGdkKeyEvent)
{
    switch (aGdkKeyEvent->keyval) {

#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
        case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

        default:
            break;
    }

    uint32_t ch = GetCharCodeFor(aGdkKeyEvent);
    return ch ? KEY_NAME_INDEX_PrintableKey : KEY_NAME_INDEX_Unidentified;
}

/* static */ void
KeymapWrapper::InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                            GdkEventKey* aGdkKeyEvent)
{
    KeymapWrapper* keymapWrapper = GetInstance();

    aKeyEvent.mKeyNameIndex =
        keymapWrapper->ComputeDOMKeyNameIndex(aGdkKeyEvent);
    aKeyEvent.keyCode = ComputeDOMKeyCode(aGdkKeyEvent);

    // NOTE: The state of given key event indicates adjacent state of
    // modifier keys.  E.g., even if the event is Shift key press event,
    // the bit for Shift is still false.  By the same token, even if the
    // event is Shift key release event, the bit for Shift is still true.
    // Unfortunately, gdk_keyboard_get_modifiers() returns current modifier
    // state.  It means if there're some pending modifier key press or
    // key release events, the result isn't what we want.
    guint modifierState = aGdkKeyEvent->state;
    if (aGdkKeyEvent->is_modifier) {
        Display* display =
            gdk_x11_display_get_xdisplay(gdk_display_get_default());
        if (XEventsQueued(display, QueuedAfterReading)) {
            XEvent nextEvent;
            XPeekEvent(display, &nextEvent);
            if (nextEvent.type == keymapWrapper->mXKBBaseEventCode) {
                XkbEvent* XKBEvent = (XkbEvent*)&nextEvent;
                if (XKBEvent->any.xkb_type == XkbStateNotify) {
                    XkbStateNotifyEvent* stateNotifyEvent =
                        (XkbStateNotifyEvent*)XKBEvent;
                    modifierState &= ~0xFF;
                    modifierState |= stateNotifyEvent->lookup_mods;
                }
            }
        }
    }
    InitInputEvent(aKeyEvent, modifierState);

    switch (aGdkKeyEvent->keyval) {
        case GDK_Shift_L:
        case GDK_Control_L:
        case GDK_Alt_L:
        case GDK_Super_L:
        case GDK_Hyper_L:
        case GDK_Meta_L:
            aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;
            break;

        case GDK_Shift_R:
        case GDK_Control_R:
        case GDK_Alt_R:
        case GDK_Super_R:
        case GDK_Hyper_R:
        case GDK_Meta_R:
            aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;
            break;

        case GDK_KP_0:
        case GDK_KP_1:
        case GDK_KP_2:
        case GDK_KP_3:
        case GDK_KP_4:
        case GDK_KP_5:
        case GDK_KP_6:
        case GDK_KP_7:
        case GDK_KP_8:
        case GDK_KP_9:
        case GDK_KP_Space:
        case GDK_KP_Tab:
        case GDK_KP_Enter:
        case GDK_KP_F1:
        case GDK_KP_F2:
        case GDK_KP_F3:
        case GDK_KP_F4:
        case GDK_KP_Home:
        case GDK_KP_Left:
        case GDK_KP_Up:
        case GDK_KP_Right:
        case GDK_KP_Down:
        case GDK_KP_Prior: // same as GDK_KP_Page_Up
        case GDK_KP_Next:  // same as GDK_KP_Page_Down
        case GDK_KP_End:
        case GDK_KP_Begin:
        case GDK_KP_Insert:
        case GDK_KP_Delete:
        case GDK_KP_Equal:
        case GDK_KP_Multiply:
        case GDK_KP_Add:
        case GDK_KP_Separator:
        case GDK_KP_Subtract:
        case GDK_KP_Decimal:
        case GDK_KP_Divide:
            aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;
            break;

        default:
            aKeyEvent.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
            break;
    }

    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): InitKeyEvent, modifierState=0x%08X "
         "aGdkKeyEvent={ type=%s, keyval=%s(0x%X), state=0x%08X, "
         "hardware_keycode=0x%08X, is_modifier=%s } "
         "aKeyEvent={ message=%s, isShift=%s, isControl=%s, "
         "isAlt=%s, isMeta=%s }",
         keymapWrapper, modifierState,
         ((aGdkKeyEvent->type == GDK_KEY_PRESS) ?
               "GDK_KEY_PRESS" : "GDK_KEY_RELEASE"),
         gdk_keyval_name(aGdkKeyEvent->keyval),
         aGdkKeyEvent->keyval, aGdkKeyEvent->state,
         aGdkKeyEvent->hardware_keycode,
         GetBoolName(aGdkKeyEvent->is_modifier),
         ((aKeyEvent.message == NS_KEY_DOWN) ? "NS_KEY_DOWN" :
               (aKeyEvent.message == NS_KEY_PRESS) ? "NS_KEY_PRESS" :
                                                      "NS_KEY_UP"),
         GetBoolName(aKeyEvent.IsShift()), GetBoolName(aKeyEvent.IsControl()),
         GetBoolName(aKeyEvent.IsAlt()), GetBoolName(aKeyEvent.IsMeta())));

    if (aKeyEvent.message == NS_KEY_PRESS) {
        keymapWrapper->InitKeypressEvent(aKeyEvent, aGdkKeyEvent);
    }

    // The transformations above and in gdk for the keyval are not invertible
    // so link to the GdkEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the GdkEvent is good enough.)
    aKeyEvent.pluginEvent = (void *)aGdkKeyEvent;
    aKeyEvent.time = aGdkKeyEvent->time;
    aKeyEvent.mNativeKeyEvent = static_cast<void*>(aGdkKeyEvent);
}

/* static */ uint32_t
KeymapWrapper::GetCharCodeFor(const GdkEventKey *aGdkKeyEvent)
{
    // Anything above 0xf000 is considered a non-printable
    // Exception: directly encoded UCS characters
    if (aGdkKeyEvent->keyval > 0xf000 &&
        (aGdkKeyEvent->keyval & 0xff000000) != 0x01000000) {
        // Keypad keys are an exception: they return a value different
        // from their non-keypad equivalents, but mozilla doesn't distinguish.
        switch (aGdkKeyEvent->keyval) {
            case GDK_KP_Space:              return ' ';
            case GDK_KP_Equal:              return '=';
            case GDK_KP_Multiply:           return '*';
            case GDK_KP_Add:                return '+';
            case GDK_KP_Separator:          return ',';
            case GDK_KP_Subtract:           return '-';
            case GDK_KP_Decimal:            return '.';
            case GDK_KP_Divide:             return '/';
            case GDK_KP_0:                  return '0';
            case GDK_KP_1:                  return '1';
            case GDK_KP_2:                  return '2';
            case GDK_KP_3:                  return '3';
            case GDK_KP_4:                  return '4';
            case GDK_KP_5:                  return '5';
            case GDK_KP_6:                  return '6';
            case GDK_KP_7:                  return '7';
            case GDK_KP_8:                  return '8';
            case GDK_KP_9:                  return '9';
            default:                        return 0; // non-printables
        }
    }

    static const long MAX_UNICODE = 0x10FFFF;

    // we're supposedly printable, let's try to convert
    long ucs = keysym2ucs(aGdkKeyEvent->keyval);
    if ((ucs != -1) && (ucs < MAX_UNICODE)) {
         return ucs;
    }

    // I guess we couldn't convert
    return 0;
}

uint32_t
KeymapWrapper::GetCharCodeFor(const GdkEventKey *aGdkKeyEvent,
                              guint aModifierState,
                              gint aGroup)
{
    guint keyval;
    if (!gdk_keymap_translate_keyboard_state(mGdkKeymap,
             aGdkKeyEvent->hardware_keycode,
             GdkModifierType(aModifierState),
             aGroup, &keyval, nullptr, nullptr, nullptr)) {
        return 0;
    }
    GdkEventKey tmpEvent = *aGdkKeyEvent;
    tmpEvent.state = aModifierState;
    tmpEvent.keyval = keyval;
    tmpEvent.group = aGroup;
    return GetCharCodeFor(&tmpEvent);
}


gint
KeymapWrapper::GetKeyLevel(GdkEventKey *aGdkKeyEvent)
{
    gint level;
    if (!gdk_keymap_translate_keyboard_state(mGdkKeymap,
             aGdkKeyEvent->hardware_keycode,
             GdkModifierType(aGdkKeyEvent->state),
             aGdkKeyEvent->group, nullptr, nullptr, &level, nullptr)) {
        return -1;
    }
    return level;
}

gint
KeymapWrapper::GetFirstLatinGroup()
{
    GdkKeymapKey *keys;
    gint count;
    gint minGroup = -1;
    if (gdk_keymap_get_entries_for_keyval(mGdkKeymap, GDK_a, &keys, &count)) {
        // find the minimum number group for latin inputtable layout
        for (gint i = 0; i < count && minGroup != 0; ++i) {
            if (keys[i].level != 0 && keys[i].level != 1) {
                continue;
            }
            if (minGroup >= 0 && keys[i].group > minGroup) {
                continue;
            }
            minGroup = keys[i].group;
        }
        g_free(keys);
    }
    return minGroup;
}

bool
KeymapWrapper::IsLatinGroup(guint8 aGroup)
{
    GdkKeymapKey *keys;
    gint count;
    bool result = false;
    if (gdk_keymap_get_entries_for_keyval(mGdkKeymap, GDK_a, &keys, &count)) {
        for (gint i = 0; i < count; ++i) {
            if (keys[i].level != 0 && keys[i].level != 1) {
                continue;
            }
            if (keys[i].group == aGroup) {
                result = true;
                break;
            }
        }
        g_free(keys);
    }
    return result;
}

/* static */ bool
KeymapWrapper::IsBasicLatinLetterOrNumeral(uint32_t aCharCode)
{
    return (aCharCode >= 'a' && aCharCode <= 'z') ||
           (aCharCode >= 'A' && aCharCode <= 'Z') ||
           (aCharCode >= '0' && aCharCode <= '9');
}

/* static */ guint
KeymapWrapper::GetGDKKeyvalWithoutModifier(const GdkEventKey *aGdkKeyEvent)
{
    KeymapWrapper* keymapWrapper = GetInstance();
    guint state =
        (aGdkKeyEvent->state & keymapWrapper->GetModifierMask(NUM_LOCK));
    guint keyval;
    if (!gdk_keymap_translate_keyboard_state(keymapWrapper->mGdkKeymap,
             aGdkKeyEvent->hardware_keycode, GdkModifierType(state),
             aGdkKeyEvent->group, &keyval, nullptr, nullptr, nullptr)) {
        return 0;
    }
    return keyval;
}

/* static */ uint32_t
KeymapWrapper::GetDOMKeyCodeFromKeyPairs(guint aGdkKeyval)
{
    switch (aGdkKeyval) {
        case GDK_Cancel:                return NS_VK_CANCEL;
        case GDK_BackSpace:             return NS_VK_BACK;
        case GDK_Tab:
        case GDK_ISO_Left_Tab:          return NS_VK_TAB;
        case GDK_Clear:                 return NS_VK_CLEAR;
        case GDK_Return:                return NS_VK_RETURN;
        case GDK_Shift_L:
        case GDK_Shift_R:
        case GDK_Shift_Lock:            return NS_VK_SHIFT;
        case GDK_Control_L:
        case GDK_Control_R:             return NS_VK_CONTROL;
        case GDK_Alt_L:
        case GDK_Alt_R:                 return NS_VK_ALT;
        case GDK_Meta_L:
        case GDK_Meta_R:                return NS_VK_META;

        // Assume that Super or Hyper is always mapped to physical Win key.
        case GDK_Super_L:
        case GDK_Super_R:
        case GDK_Hyper_L:
        case GDK_Hyper_R:               return NS_VK_WIN;

        // GTK's AltGraph key is similar to Mac's Option (Alt) key.  However,
        // unfortunately, browsers on Mac are using NS_VK_ALT for it even though
        // it's really different from Alt key on Windows.
        // On the other hand, GTK's AltGrapsh keys are really different from
        // Alt key.  However, there is no AltGrapsh key on Windows.  On Windows,
        // both Ctrl and Alt keys are pressed internally when AltGr key is
        // pressed.  For some languages' users, AltGraph key is important, so,
        // web applications on such locale may want to know AltGraph key press.
        // Therefore, we should map AltGr keycode for them only on GTK.
        case GDK_ISO_Level3_Shift:
        case GDK_ISO_Level5_Shift:
        // We assume that Mode_switch is always used for level3 shift.
        case GDK_Mode_switch:           return NS_VK_ALTGR;

        case GDK_Pause:                 return NS_VK_PAUSE;
        case GDK_Caps_Lock:             return NS_VK_CAPS_LOCK;
        case GDK_Kana_Lock:
        case GDK_Kana_Shift:            return NS_VK_KANA;
        case GDK_Hangul:                return NS_VK_HANGUL;
        // case GDK_XXX:                   return NS_VK_JUNJA;
        // case GDK_XXX:                   return NS_VK_FINAL;
        case GDK_Hangul_Hanja:          return NS_VK_HANJA;
        case GDK_Kanji:                 return NS_VK_KANJI;
        case GDK_Escape:                return NS_VK_ESCAPE;
        case GDK_Henkan:                return NS_VK_CONVERT;
        case GDK_Muhenkan:              return NS_VK_NONCONVERT;
        // case GDK_XXX:                   return NS_VK_ACCEPT;
        // case GDK_XXX:                   return NS_VK_MODECHANGE;
        case GDK_space:                 return NS_VK_SPACE;
        case GDK_Page_Up:               return NS_VK_PAGE_UP;
        case GDK_Page_Down:             return NS_VK_PAGE_DOWN;
        case GDK_End:                   return NS_VK_END;
        case GDK_Home:                  return NS_VK_HOME;
        case GDK_Left:                  return NS_VK_LEFT;
        case GDK_Up:                    return NS_VK_UP;
        case GDK_Right:                 return NS_VK_RIGHT;
        case GDK_Down:                  return NS_VK_DOWN;
        case GDK_Select:                return NS_VK_SELECT;
        case GDK_Print:                 return NS_VK_PRINT;
        case GDK_Execute:               return NS_VK_EXECUTE;
        case GDK_Insert:                return NS_VK_INSERT;
        case GDK_Delete:                return NS_VK_DELETE;
        case GDK_Help:                  return NS_VK_HELP;

        // keypad keys
        case GDK_KP_Left:               return NS_VK_LEFT;
        case GDK_KP_Right:              return NS_VK_RIGHT;
        case GDK_KP_Up:                 return NS_VK_UP;
        case GDK_KP_Down:               return NS_VK_DOWN;
        case GDK_KP_Page_Up:            return NS_VK_PAGE_UP;
        // Not sure what these are
        // case GDK_KP_Prior:              return NS_VK_;
        // case GDK_KP_Next:               return NS_VK_;
        case GDK_KP_Begin:              return NS_VK_CLEAR; // Num-unlocked 5
        case GDK_KP_Page_Down:          return NS_VK_PAGE_DOWN;
        case GDK_KP_Home:               return NS_VK_HOME;
        case GDK_KP_End:                return NS_VK_END;
        case GDK_KP_Insert:             return NS_VK_INSERT;
        case GDK_KP_Delete:             return NS_VK_DELETE;
        case GDK_KP_Enter:              return NS_VK_RETURN;

        case GDK_Num_Lock:              return NS_VK_NUM_LOCK;
        case GDK_Scroll_Lock:           return NS_VK_SCROLL_LOCK;

        // Function keys
        case GDK_F1:                    return NS_VK_F1;
        case GDK_F2:                    return NS_VK_F2;
        case GDK_F3:                    return NS_VK_F3;
        case GDK_F4:                    return NS_VK_F4;
        case GDK_F5:                    return NS_VK_F5;
        case GDK_F6:                    return NS_VK_F6;
        case GDK_F7:                    return NS_VK_F7;
        case GDK_F8:                    return NS_VK_F8;
        case GDK_F9:                    return NS_VK_F9;
        case GDK_F10:                   return NS_VK_F10;
        case GDK_F11:                   return NS_VK_F11;
        case GDK_F12:                   return NS_VK_F12;
        case GDK_F13:                   return NS_VK_F13;
        case GDK_F14:                   return NS_VK_F14;
        case GDK_F15:                   return NS_VK_F15;
        case GDK_F16:                   return NS_VK_F16;
        case GDK_F17:                   return NS_VK_F17;
        case GDK_F18:                   return NS_VK_F18;
        case GDK_F19:                   return NS_VK_F19;
        case GDK_F20:                   return NS_VK_F20;
        case GDK_F21:                   return NS_VK_F21;
        case GDK_F22:                   return NS_VK_F22;
        case GDK_F23:                   return NS_VK_F23;
        case GDK_F24:                   return NS_VK_F24;

        // context menu key, keysym 0xff67, typically keycode 117 on 105-key
        // (Microsoft) x86 keyboards, located between right 'Windows' key and
        // right Ctrl key
        case GDK_Menu:                  return NS_VK_CONTEXT_MENU;
        case GDK_Sleep:                 return NS_VK_SLEEP;

        case GDK_3270_Attn:             return NS_VK_ATTN;
        case GDK_3270_CursorSelect:     return NS_VK_CRSEL;
        case GDK_3270_ExSelect:         return NS_VK_EXSEL;
        case GDK_3270_EraseEOF:         return NS_VK_EREOF;
        case GDK_3270_Play:             return NS_VK_PLAY;
        // case GDK_XXX:                   return NS_VK_ZOOM;
        case GDK_3270_PA1:              return NS_VK_PA1;

        // map Sun Keyboard special keysyms on to NS_VK keys

        // Sun F11 key generates SunF36(0x1005ff10) keysym
        case 0x1005ff10:                return NS_VK_F11;
        // Sun F12 key generates SunF37(0x1005ff11) keysym
        case 0x1005ff11:                return NS_VK_F12;
        default:                        return 0;
    }
}

void
KeymapWrapper::InitKeypressEvent(WidgetKeyboardEvent& aKeyEvent,
                                 GdkEventKey* aGdkKeyEvent)
{
    NS_ENSURE_TRUE_VOID(aKeyEvent.message == NS_KEY_PRESS);

    aKeyEvent.charCode = GetCharCodeFor(aGdkKeyEvent);
    if (!aKeyEvent.charCode) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitKeypressEvent, "
             "keyCode=0x%02X, charCode=0x%08X",
             this, aKeyEvent.keyCode, aKeyEvent.charCode));
        return;
    }

    // If the event causes inputting a character, keyCode must be zero.
    aKeyEvent.keyCode = 0;

    // If Ctrl or Alt or Meta or OS is pressed, we need to append the key
    // details for handling shortcut key.  Otherwise, we have no additional
    // work.
    if (!aKeyEvent.IsControl() && !aKeyEvent.IsAlt() &&
        !aKeyEvent.IsMeta() && !aKeyEvent.IsOS()) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitKeypressEvent, "
             "keyCode=0x%02X, charCode=0x%08X",
             this, aKeyEvent.keyCode, aKeyEvent.charCode));
        return;
    }

    gint level = GetKeyLevel(aGdkKeyEvent);
    if (level != 0 && level != 1) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitKeypressEvent, "
             "keyCode=0x%02X, charCode=0x%08X, level=%d",
             this, aKeyEvent.keyCode, aKeyEvent.charCode, level));
        return;
    }

    guint baseState = aGdkKeyEvent->state &
        ~(GetModifierMask(SHIFT) | GetModifierMask(CTRL) |
          GetModifierMask(ALT) | GetModifierMask(META) |
          GetModifierMask(SUPER) | GetModifierMask(HYPER));

    // We shold send both shifted char and unshifted char, all keyboard layout
    // users can use all keys.  Don't change event.charCode. On some keyboard
    // layouts, Ctrl/Alt/Meta keys are used for inputting some characters.
    AlternativeCharCode altCharCodes(0, 0);
    // unshifted charcode of current keyboard layout.
    altCharCodes.mUnshiftedCharCode =
        GetCharCodeFor(aGdkKeyEvent, baseState, aGdkKeyEvent->group);
    bool isLatin = (altCharCodes.mUnshiftedCharCode <= 0xFF);
    // shifted charcode of current keyboard layout.
    altCharCodes.mShiftedCharCode =
        GetCharCodeFor(aGdkKeyEvent,
                       baseState | GetModifierMask(SHIFT),
                       aGdkKeyEvent->group);
    isLatin = isLatin && (altCharCodes.mShiftedCharCode <= 0xFF);
    if (altCharCodes.mUnshiftedCharCode || altCharCodes.mShiftedCharCode) {
        aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
    }

    bool needLatinKeyCodes = !isLatin;
    if (!needLatinKeyCodes) {
        needLatinKeyCodes = 
            (IS_ASCII_ALPHABETICAL(altCharCodes.mUnshiftedCharCode) !=
             IS_ASCII_ALPHABETICAL(altCharCodes.mShiftedCharCode));
    }

    // If current keyboard layout can input Latin characters, we don't need
    // more information.
    if (!needLatinKeyCodes) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitKeypressEvent, keyCode=0x%02X, "
             "charCode=0x%08X, level=%d, altCharCodes={ "
             "mUnshiftedCharCode=0x%08X, mShiftedCharCode=0x%08X }",
             this, aKeyEvent.keyCode, aKeyEvent.charCode, level,
             altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode));
        return;
    }

    // Next, find Latin inputtable keyboard layout.
    gint minGroup = GetFirstLatinGroup();
    if (minGroup < 0) {
        PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
            ("KeymapWrapper(%p): InitKeypressEvent, "
             "Latin keyboard layout isn't found: "
             "keyCode=0x%02X, charCode=0x%08X, level=%d, "
             "altCharCodes={ mUnshiftedCharCode=0x%08X, "
             "mShiftedCharCode=0x%08X }",
             this, aKeyEvent.keyCode, aKeyEvent.charCode, level,
             altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode));
        return;
    }

    AlternativeCharCode altLatinCharCodes(0, 0);
    uint32_t unmodifiedCh =
        aKeyEvent.IsShift() ? altCharCodes.mShiftedCharCode :
                              altCharCodes.mUnshiftedCharCode;

    // unshifted charcode of found keyboard layout.
    uint32_t ch = GetCharCodeFor(aGdkKeyEvent, baseState, minGroup);
    altLatinCharCodes.mUnshiftedCharCode =
        IsBasicLatinLetterOrNumeral(ch) ? ch : 0;
    // shifted charcode of found keyboard layout.
    ch = GetCharCodeFor(aGdkKeyEvent,
                        baseState | GetModifierMask(SHIFT),
                        minGroup);
    altLatinCharCodes.mShiftedCharCode =
        IsBasicLatinLetterOrNumeral(ch) ? ch : 0;
    if (altLatinCharCodes.mUnshiftedCharCode ||
        altLatinCharCodes.mShiftedCharCode) {
        aKeyEvent.alternativeCharCodes.AppendElement(altLatinCharCodes);
    }
    // If the charCode is not Latin, and the level is 0 or 1, we should
    // replace the charCode to Latin char if Alt and Meta keys are not
    // pressed. (Alt should be sent the localized char for accesskey
    // like handling of Web Applications.)
    ch = aKeyEvent.IsShift() ? altLatinCharCodes.mShiftedCharCode :
                               altLatinCharCodes.mUnshiftedCharCode;
    if (ch && !(aKeyEvent.IsAlt() || aKeyEvent.IsMeta()) &&
        aKeyEvent.charCode == unmodifiedCh) {
        aKeyEvent.charCode = ch;
    }

    PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
        ("KeymapWrapper(%p): InitKeypressEvent, "
         "keyCode=0x%02X, charCode=0x%08X, level=%d, minGroup=%d, "
         "altCharCodes={ mUnshiftedCharCode=0x%08X, "
         "mShiftedCharCode=0x%08X } "
         "altLatinCharCodes={ mUnshiftedCharCode=0x%08X, "
         "mShiftedCharCode=0x%08X }",
         this, aKeyEvent.keyCode, aKeyEvent.charCode, level, minGroup,
         altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode,
         altLatinCharCodes.mUnshiftedCharCode,
         altLatinCharCodes.mShiftedCharCode));
}

/* static */ bool
KeymapWrapper::IsKeyPressEventNecessary(GdkEventKey* aGdkKeyEvent)
{
    // If this is a modifier key event, we shouldn't send keypress event.
    switch (ComputeDOMKeyCode(aGdkKeyEvent)) {
        case NS_VK_SHIFT:
        case NS_VK_CONTROL:
        case NS_VK_ALT:
        case NS_VK_ALTGR:
        case NS_VK_WIN:
        case NS_VK_CAPS_LOCK:
        case NS_VK_NUM_LOCK:
        case NS_VK_SCROLL_LOCK:
            return false;
        default:
            return true;
    }
}

} // namespace widget
} // namespace mozilla
