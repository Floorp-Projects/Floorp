/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Christopher Blizzard <blizzard@mozilla.org>.  
 * Portions created by the Initial Developer are Copyright (C) 2001 
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"

#include "nsGtkKeyUtils.h"

#include <gdk/gdkkeysyms.h>
#ifndef GDK_Sleep
#define GDK_Sleep 0x1008ff2f
#endif

#include <gdk/gdk.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#include "nsGUIEvent.h"
#include "keysym2ucs.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gKeymapWrapperLog = nsnull;
#endif // PR_LOGGING

#include "mozilla/Util.h"

namespace mozilla {
namespace widget {

struct KeyPair {
    PRUint32 DOMKeyCode;
    guint GDKKeyval;
};

#define IS_ASCII_ALPHABETICAL(key) \
    ((('a' <= key) && (key <= 'z')) || (('A' <= key) && (key <= 'Z')))

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// GTK keycodes are defined in <gdk/gdkkeysyms.h>
//
static const KeyPair kKeyPairs[] = {
    { NS_VK_CANCEL,     GDK_Cancel },
    { NS_VK_BACK,       GDK_BackSpace },
    { NS_VK_TAB,        GDK_Tab },
    { NS_VK_TAB,        GDK_ISO_Left_Tab },
    { NS_VK_CLEAR,      GDK_Clear },
    { NS_VK_RETURN,     GDK_Return },
    { NS_VK_SHIFT,      GDK_Shift_L },
    { NS_VK_SHIFT,      GDK_Shift_R },
    { NS_VK_CONTROL,    GDK_Control_L },
    { NS_VK_CONTROL,    GDK_Control_R },
    { NS_VK_ALT,        GDK_Alt_L },
    { NS_VK_ALT,        GDK_Alt_R },
    { NS_VK_META,       GDK_Meta_L },
    { NS_VK_META,       GDK_Meta_R },
    { NS_VK_PAUSE,      GDK_Pause },
    { NS_VK_CAPS_LOCK,  GDK_Caps_Lock },
    { NS_VK_KANA,       GDK_Kana_Lock },
    { NS_VK_KANA,       GDK_Kana_Shift },
    { NS_VK_HANGUL,     GDK_Hangul },
    // { NS_VK_JUNJA,      GDK_XXX },
    // { NS_VK_FINAL,      GDK_XXX },
    { NS_VK_HANJA,      GDK_Hangul_Hanja },
    { NS_VK_KANJI,      GDK_Kanji },
    { NS_VK_ESCAPE,     GDK_Escape },
    { NS_VK_CONVERT,    GDK_Henkan },
    { NS_VK_NONCONVERT, GDK_Muhenkan },
    // { NS_VK_ACCEPT,     GDK_XXX },
    { NS_VK_MODECHANGE, GDK_Mode_switch },
    { NS_VK_SPACE,      GDK_space },
    { NS_VK_PAGE_UP,    GDK_Page_Up },
    { NS_VK_PAGE_DOWN,  GDK_Page_Down },
    { NS_VK_END,        GDK_End },
    { NS_VK_HOME,       GDK_Home },
    { NS_VK_LEFT,       GDK_Left },
    { NS_VK_UP,         GDK_Up },
    { NS_VK_RIGHT,      GDK_Right },
    { NS_VK_DOWN,       GDK_Down },
    { NS_VK_SELECT,     GDK_Select },
    { NS_VK_PRINT,      GDK_Print },
    { NS_VK_EXECUTE,    GDK_Execute },
    { NS_VK_PRINTSCREEN, GDK_Print },
    { NS_VK_INSERT,     GDK_Insert },
    { NS_VK_DELETE,     GDK_Delete },
    { NS_VK_HELP,       GDK_Help },

    // keypad keys
    { NS_VK_LEFT,       GDK_KP_Left },
    { NS_VK_RIGHT,      GDK_KP_Right },
    { NS_VK_UP,         GDK_KP_Up },
    { NS_VK_DOWN,       GDK_KP_Down },
    { NS_VK_PAGE_UP,    GDK_KP_Page_Up },
    // Not sure what these are
    //{ NS_VK_,       GDK_KP_Prior },
    //{ NS_VK_,        GDK_KP_Next },
    // GDK_KP_Begin is the 5 on the non-numlock keypad
    //{ NS_VK_,        GDK_KP_Begin },
    { NS_VK_PAGE_DOWN,  GDK_KP_Page_Down },
    { NS_VK_HOME,       GDK_KP_Home },
    { NS_VK_END,        GDK_KP_End },
    { NS_VK_INSERT,     GDK_KP_Insert },
    { NS_VK_DELETE,     GDK_KP_Delete },

    { NS_VK_MULTIPLY,   GDK_KP_Multiply },
    { NS_VK_ADD,        GDK_KP_Add },
    { NS_VK_SEPARATOR,  GDK_KP_Separator },
    { NS_VK_SUBTRACT,   GDK_KP_Subtract },
    { NS_VK_DECIMAL,    GDK_KP_Decimal },
    { NS_VK_DIVIDE,     GDK_KP_Divide },
    { NS_VK_RETURN,     GDK_KP_Enter },
    { NS_VK_NUM_LOCK,   GDK_Num_Lock },
    { NS_VK_SCROLL_LOCK,GDK_Scroll_Lock },

    { NS_VK_COMMA,      GDK_comma },
    { NS_VK_PERIOD,     GDK_period },
    { NS_VK_SLASH,      GDK_slash },
    { NS_VK_BACK_SLASH, GDK_backslash },
    { NS_VK_BACK_QUOTE, GDK_grave },
    { NS_VK_OPEN_BRACKET, GDK_bracketleft },
    { NS_VK_CLOSE_BRACKET, GDK_bracketright },
    { NS_VK_SEMICOLON, GDK_colon },
    { NS_VK_QUOTE, GDK_apostrophe },

    // context menu key, keysym 0xff67, typically keycode 117 on 105-key (Microsoft) 
    // x86 keyboards, located between right 'Windows' key and right Ctrl key
    { NS_VK_CONTEXT_MENU, GDK_Menu },
    { NS_VK_SLEEP,      GDK_Sleep },

    // NS doesn't have dash or equals distinct from the numeric keypad ones,
    // so we'll use those for now.  See bug 17008:
    { NS_VK_SUBTRACT, GDK_minus },
    { NS_VK_EQUALS, GDK_equal },

    // Some shifted keys, see bug 15463 as well as 17008.
    // These should be subject to different keyboard mappings.
    { NS_VK_QUOTE, GDK_quotedbl },
    { NS_VK_OPEN_BRACKET, GDK_braceleft },
    { NS_VK_CLOSE_BRACKET, GDK_braceright },
    { NS_VK_BACK_SLASH, GDK_bar },
    { NS_VK_SEMICOLON, GDK_semicolon },
    { NS_VK_BACK_QUOTE, GDK_asciitilde },
    { NS_VK_COMMA, GDK_less },
    { NS_VK_PERIOD, GDK_greater },
    { NS_VK_SLASH,      GDK_question },
    { NS_VK_1, GDK_exclam },
    { NS_VK_2, GDK_at },
    { NS_VK_3, GDK_numbersign },
    { NS_VK_4, GDK_dollar },
    { NS_VK_5, GDK_percent },
    { NS_VK_6, GDK_asciicircum },
    { NS_VK_7, GDK_ampersand },
    { NS_VK_8, GDK_asterisk },
    { NS_VK_9, GDK_parenleft },
    { NS_VK_0, GDK_parenright },
    { NS_VK_SUBTRACT, GDK_underscore },
    { NS_VK_EQUALS, GDK_plus }
};

// map Sun Keyboard special keysyms on to NS_VK keys
static const KeyPair kSunKeyPairs[] = {
    {NS_VK_F11, 0x1005ff10 }, //Sun F11 key generates SunF36(0x1005ff10) keysym
    {NS_VK_F12, 0x1005ff11 }  //Sun F12 key generates SunF37(0x1005ff11) keysym
};

#define MOZ_MODIFIER_KEYS "MozKeymapWrapper"

KeymapWrapper* KeymapWrapper::sInstance = nsnull;

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
        case ALTGR:        return "AltGr";
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
        case GDK_Mode_switch:      return ALTGR;
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
        case ALTGR:
            return mModifierMasks[INDEX_ALTGR];
        default:
            return 0;
    }
}

KeymapWrapper::ModifierKey*
KeymapWrapper::GetModifierKey(guint aHardwareKeycode)
{
    for (PRUint32 i = 0; i < mModifierKeys.Length(); i++) {
        ModifierKey& key = mModifierKeys[i];
        if (key.mHardwareKeycode == aHardwareKeycode) {
            return &key;
        }
    }
    return nsnull;
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
    mInitialized(false), mGdkKeymap(gdk_keymap_get_default())
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
         "ScrollLock=0x%X, AltGr=0x%X, Shift=0x%X, Ctrl=0x%X, Alt=0x%X, "
         "Meta=0x%X, Super=0x%X, Hyper=0x%X",
         this,
         GetModifierMask(CAPS_LOCK), GetModifierMask(NUM_LOCK),
         GetModifierMask(SCROLL_LOCK), GetModifierMask(ALTGR),
         GetModifierMask(SHIFT), GetModifierMask(CTRL),
         GetModifierMask(ALT), GetModifierMask(META),
         GetModifierMask(SUPER), GetModifierMask(HYPER)));
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
    //  And also Super and Hyper share the Mod4.

    const PRUint32 map_size = 8 * xmodmap->max_keypermod;
    for (PRUint32 i = 0; i < map_size; i++) {
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
        const PRUint32 bit = i / xmodmap->max_keypermod;
        modifierKey->mMask |= 1 << bit;
        for (PRInt32 j = 0; j < keysyms_per_keycode; j++) {
            Modifier modifier = GetModifierForGDKKeyval(syms[j]);
            PR_LOG(gKeymapWrapperLog, PR_LOG_ALWAYS,
                ("KeymapWrapper(%p): InitBySystemSettings, "
                 "    bit=%d, j=%d, syms[j]=%s(0x%X), modifier=%s",
                 this, bit, j, gdk_keyval_name(syms[j]), syms[j],
                 GetModifierName(modifier)));

            ModifierIndex index;
            switch (modifier) {
                case NUM_LOCK:
                    index = INDEX_NUM_LOCK;
                    break;
                case SCROLL_LOCK:
                    index = INDEX_SCROLL_LOCK;
                    break;
                case ALT:
                    index = INDEX_ALT;
                    break;
                case SUPER:
                    index = INDEX_SUPER;
                    break;
                case HYPER:
                    index = INDEX_HYPER;
                    break;
                case META:
                    index = INDEX_META;
                    break;
                case ALTGR:
                    index = INDEX_ALTGR;
                    break;
                default:
                    // NOTE: We always use GDK_SHIFT_MASK, GDK_CONTROL_MASK and
                    //       GDK_LOCK_MASK for SHIFT, CTRL and CAPS_LOCK.
                    //       This is standard manners of GTK application.
                    continue;
            }
            mModifierMasks[index] |= 1 << bit;
        }
    }

    XFreeModifiermap(xmodmap);
    XFree(xkeymap);
}

KeymapWrapper::~KeymapWrapper()
{
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
    sInstance = nsnull;
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
}

/* static */ guint
KeymapWrapper::GetCurrentModifierState()
{
    GdkModifierType modifiers;
    gdk_display_get_pointer(gdk_display_get_default(),
                            NULL, NULL, NULL, &modifiers);
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
    for (PRUint32 i = 0; i < sizeof(Modifier) * 8 && aModifiers; i++) {
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
KeymapWrapper::InitInputEvent(nsInputEvent& aInputEvent,
                              guint aModifierState)
{
    KeymapWrapper* keymapWrapper = GetInstance();

    aInputEvent.isShift =
        keymapWrapper->AreModifiersActive(SHIFT, aModifierState);
    aInputEvent.isControl =
        keymapWrapper->AreModifiersActive(CTRL, aModifierState);
    aInputEvent.isAlt =
        keymapWrapper->AreModifiersActive(ALT, aModifierState);
    // XXX DOM Meta key should be TRUE only on Mac.  We need to discuss this
    //     issue later.
    aInputEvent.isMeta = false;

    PR_LOG(gKeymapWrapperLog, PR_LOG_DEBUG,
        ("KeymapWrapper(%p): InitInputEvent, aModifierState=0x%08X "
         "aKeyEvent={ isShift=%s, isControl=%s, isAlt=%s, isMeta=%s }",
         keymapWrapper, aModifierState,
         GetBoolName(aInputEvent.isShift), GetBoolName(aInputEvent.isControl),
         GetBoolName(aInputEvent.isAlt), GetBoolName(aInputEvent.isMeta)));
}

/* static */ PRUint32
KeymapWrapper::ComputeDOMKeyCode(const GdkEventKey* aGdkKeyEvent)
{
    guint keyval = aGdkKeyEvent->keyval;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    // since X has different key symbols for upper and lowercase letters and
    // mozilla does not, convert gdk's to mozilla's
    if (keyval >= GDK_a && keyval <= GDK_z) {
        return keyval - GDK_a + NS_VK_A;
    }
    if (keyval >= GDK_A && keyval <= GDK_Z) {
        return keyval - GDK_A + NS_VK_A;
    }

    // numbers
    if (keyval >= GDK_0 && keyval <= GDK_9) {
        return keyval - GDK_0 + NS_VK_0;
    }

    // keypad numbers
    if (keyval >= GDK_KP_0 && keyval <= GDK_KP_9) {
        return keyval - GDK_KP_0 + NS_VK_NUMPAD0;
    }

    // If the keyval indicates it's a modifier key, we should use unshifted
    // key's modifier keyval.
    if (GetModifierForGDKKeyval(keyval)) {
        KeymapWrapper* keymapWrapper = GetInstance();
        GdkKeymapKey key;
        key.keycode = aGdkKeyEvent->hardware_keycode;
        key.group = aGdkKeyEvent->group;
        key.level = 0;
        guint unshiftedKeyval =
            gdk_keymap_lookup_key(keymapWrapper->mGdkKeymap, &key);
        // But if the unshifted keyval isn't a modifier key, we shouldn't use
        // it.  E.g., Japanese keyboard layout's Shift + Eisu-Toggle key is
        // CapsLock.  This is an actual rare case, Windows uses different
        // keycode for a physical key for different shift key state.
        if (GetModifierForGDKKeyval(unshiftedKeyval)) {
            keyval = unshiftedKeyval;
        }
    }

    // map Sun Keyboard special keysyms
    for (PRUint32 i = 0; i < ArrayLength(kSunKeyPairs); i++) {
        if (kSunKeyPairs[i].GDKKeyval == keyval) {
            return kSunKeyPairs[i].DOMKeyCode;
        }
    }

    // misc other things
    for (PRUint32 i = 0; i < ArrayLength(kKeyPairs); i++) {
        if (kKeyPairs[i].GDKKeyval == keyval) {
            return kKeyPairs[i].DOMKeyCode;
        }
    }

    // function keys
    if (keyval >= GDK_F1 && keyval <= GDK_F24) {
        return keyval - GDK_F1 + NS_VK_F1;
    }

    return 0;
}

/* static */ guint
KeymapWrapper::GuessGDKKeyval(PRUint32 aDOMKeyCode)
{
    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    if (aDOMKeyCode >= NS_VK_A && aDOMKeyCode <= NS_VK_Z) {
        // gdk and DOM both use the ASCII codes for these keys.
        return aDOMKeyCode;
    }

    // numbers
    if (aDOMKeyCode >= NS_VK_0 && aDOMKeyCode <= NS_VK_9) {
        // gdk and DOM both use the ASCII codes for these keys.
        return aDOMKeyCode - NS_VK_0 + GDK_0;
    }

    // keypad numbers
    if (aDOMKeyCode >= NS_VK_NUMPAD0 && aDOMKeyCode <= NS_VK_NUMPAD9) {
        return aDOMKeyCode - NS_VK_NUMPAD0 + GDK_KP_0;
    }

    // misc other things
    for (PRUint32 i = 0; i < ArrayLength(kKeyPairs); ++i) {
        if (kKeyPairs[i].DOMKeyCode == aDOMKeyCode) {
            return kKeyPairs[i].GDKKeyval;
        }
    }

    // function keys
    if (aDOMKeyCode >= NS_VK_F1 && aDOMKeyCode <= NS_VK_F9) {
        return aDOMKeyCode - NS_VK_F1 + GDK_F1;
    }

    return 0;
}

/* static */ void
KeymapWrapper::InitKeyEvent(nsKeyEvent& aKeyEvent,
                            GdkEventKey* aGdkKeyEvent)
{
    KeymapWrapper* keymapWrapper = GetInstance();

    aKeyEvent.keyCode = ComputeDOMKeyCode(aGdkKeyEvent);

    // NOTE: The state of given key event indicates adjacent state of
    // modifier keys.  E.g., even if the event is Shift key press event,
    // the bit for Shift is still false.  By the same token, even if the
    // event is Shift key release event, the bit for Shift is still true.
    // Unfortunately, gdk_keyboard_get_modifiers() returns current modifier
    // state.  It means if there're some pending modifier key press or
    // key release events, the result isn't what we want.
    // Temporarily, we should compute the state only when the key event
    // is GDK_KEY_PRESS.
    // XXX If we could know the modifier keys state at the key release event,
    //     we should cut out changingMask from modifierState.
    guint modifierState = aGdkKeyEvent->state;
    if (aGdkKeyEvent->is_modifier && aGdkKeyEvent->type == GDK_KEY_PRESS) {
        ModifierKey* modifierKey =
            keymapWrapper->GetModifierKey(aGdkKeyEvent->hardware_keycode);
        if (modifierKey) {
            // If new modifier key is pressed, add the pressed mod mask.
            modifierState |= modifierKey->mMask;
        }
    }
    InitInputEvent(aKeyEvent, modifierState);

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
         GetBoolName(aKeyEvent.isShift), GetBoolName(aKeyEvent.isControl),
         GetBoolName(aKeyEvent.isAlt), GetBoolName(aKeyEvent.isMeta)));

    if (aKeyEvent.message == NS_KEY_PRESS) {
        keymapWrapper->InitKeypressEvent(aKeyEvent, aGdkKeyEvent);
    }

    // The transformations above and in gdk for the keyval are not invertible
    // so link to the GdkEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the GdkEvent is good enough.)
    aKeyEvent.pluginEvent = (void *)aGdkKeyEvent;
    aKeyEvent.time = aGdkKeyEvent->time;
}

/* static */ PRUint32
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

PRUint32
KeymapWrapper::GetCharCodeFor(const GdkEventKey *aGdkKeyEvent,
                              guint aModifierState,
                              gint aGroup)
{
    guint keyval;
    if (!gdk_keymap_translate_keyboard_state(mGdkKeymap,
             aGdkKeyEvent->hardware_keycode,
             GdkModifierType(aModifierState),
             aGroup, &keyval, NULL, NULL, NULL)) {
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
             aGdkKeyEvent->group, NULL, NULL, &level, NULL)) {
        return -1;
    }
    return level;
}

/* static */ PRBool
KeymapWrapper::IsBasicLatinLetterOrNumeral(PRUint32 aCharCode)
{
    return (aCharCode >= 'a' && aCharCode <= 'z') ||
           (aCharCode >= 'A' && aCharCode <= 'Z') ||
           (aCharCode >= '0' && aCharCode <= '9');
}

void
KeymapWrapper::InitKeypressEvent(nsKeyEvent& aKeyEvent,
                                 GdkEventKey* aGdkKeyEvent)
{
    NS_ENSURE_TRUE(aKeyEvent.message == NS_KEY_PRESS, );

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

    // If Ctrl or Alt or Meta is pressed, we need to append the key details
    // for handling shortcut key.  Otherwise, we have no additional work.
    if (!aKeyEvent.isControl && !aKeyEvent.isAlt && !aKeyEvent.isMeta) {
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
    nsAlternativeCharCode altCharCodes(0, 0);
    // unshifted charcode of current keyboard layout.
    altCharCodes.mUnshiftedCharCode =
        GetCharCodeFor(aGdkKeyEvent, baseState, aGdkKeyEvent->group);
    PRBool isLatin = (altCharCodes.mUnshiftedCharCode <= 0xFF);
    // shifted charcode of current keyboard layout.
    altCharCodes.mShiftedCharCode =
        GetCharCodeFor(aGdkKeyEvent,
                       baseState | GetModifierMask(SHIFT),
                       aGdkKeyEvent->group);
    isLatin = isLatin && (altCharCodes.mShiftedCharCode <= 0xFF);
    if (altCharCodes.mUnshiftedCharCode || altCharCodes.mShiftedCharCode) {
        aKeyEvent.alternativeCharCodes.AppendElement(altCharCodes);
    }

    PRBool needLatinKeyCodes = !isLatin;
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

    nsAlternativeCharCode altLatinCharCodes(0, 0);
    PRUint32 unmodifiedCh =
        aKeyEvent.isShift ? altCharCodes.mShiftedCharCode :
                            altCharCodes.mUnshiftedCharCode;

    // unshifted charcode of found keyboard layout.
    PRUint32 ch = GetCharCodeFor(aGdkKeyEvent, baseState, minGroup);
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
    ch = aKeyEvent.isShift ? altLatinCharCodes.mShiftedCharCode :
                             altLatinCharCodes.mUnshiftedCharCode;
    if (ch && !(aKeyEvent.isAlt || aKeyEvent.isMeta) &&
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
        case NS_VK_META:
        case NS_VK_ALT:
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
