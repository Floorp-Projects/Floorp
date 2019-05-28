/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "nsGtkKeyUtils.h"

#include <gdk/gdkkeysyms.h>
#include <algorithm>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#ifdef MOZ_WIDGET_GTK
#  include <gdk/gdkkeysyms-compat.h>
#endif
#include <X11/XKBlib.h>
#include "IMContextWrapper.h"
#include "WidgetUtils.h"
#include "keysym2ucs.h"
#include "nsContentUtils.h"
#include "nsGtkUtils.h"
#include "nsIBidiKeyboard.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWindow.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"

#ifdef MOZ_WAYLAND
#  include <sys/mman.h>
#endif

namespace mozilla {
namespace widget {

LazyLogModule gKeymapWrapperLog("KeymapWrapperWidgets");

#define IS_ASCII_ALPHABETICAL(key) \
  ((('a' <= key) && (key <= 'z')) || (('A' <= key) && (key <= 'Z')))

#define MOZ_MODIFIER_KEYS "MozKeymapWrapper"

KeymapWrapper* KeymapWrapper::sInstance = nullptr;
guint KeymapWrapper::sLastRepeatableHardwareKeyCode = 0;
Time KeymapWrapper::sLastRepeatableKeyTime = 0;
KeymapWrapper::RepeatState KeymapWrapper::sRepeatState =
    KeymapWrapper::NOT_PRESSED;

static const char* GetBoolName(bool aBool) { return aBool ? "TRUE" : "FALSE"; }

static const char* GetStatusName(nsEventStatus aStatus) {
  switch (aStatus) {
    case nsEventStatus_eConsumeDoDefault:
      return "nsEventStatus_eConsumeDoDefault";
    case nsEventStatus_eConsumeNoDefault:
      return "nsEventStatus_eConsumeNoDefault";
    case nsEventStatus_eIgnore:
      return "nsEventStatus_eIgnore";
    case nsEventStatus_eSentinel:
      return "nsEventStatus_eSentinel";
    default:
      return "Illegal value";
  }
}

static const nsCString GetKeyLocationName(uint32_t aLocation) {
  switch (aLocation) {
    case eKeyLocationLeft:
      return NS_LITERAL_CSTRING("KEY_LOCATION_LEFT");
    case eKeyLocationRight:
      return NS_LITERAL_CSTRING("KEY_LOCATION_RIGHT");
    case eKeyLocationStandard:
      return NS_LITERAL_CSTRING("KEY_LOCATION_STANDARD");
    case eKeyLocationNumpad:
      return NS_LITERAL_CSTRING("KEY_LOCATION_NUMPAD");
    default:
      return nsPrintfCString("Unknown (0x%04X)", aLocation);
  }
}

static const nsCString GetCharacterCodeName(char16_t aChar) {
  switch (aChar) {
    case 0x0000:
      return NS_LITERAL_CSTRING("NULL (0x0000)");
    case 0x0008:
      return NS_LITERAL_CSTRING("BACKSPACE (0x0008)");
    case 0x0009:
      return NS_LITERAL_CSTRING("CHARACTER TABULATION (0x0009)");
    case 0x000A:
      return NS_LITERAL_CSTRING("LINE FEED (0x000A)");
    case 0x000B:
      return NS_LITERAL_CSTRING("LINE TABULATION (0x000B)");
    case 0x000C:
      return NS_LITERAL_CSTRING("FORM FEED (0x000C)");
    case 0x000D:
      return NS_LITERAL_CSTRING("CARRIAGE RETURN (0x000D)");
    case 0x0018:
      return NS_LITERAL_CSTRING("CANCEL (0x0018)");
    case 0x001B:
      return NS_LITERAL_CSTRING("ESCAPE (0x001B)");
    case 0x0020:
      return NS_LITERAL_CSTRING("SPACE (0x0020)");
    case 0x007F:
      return NS_LITERAL_CSTRING("DELETE (0x007F)");
    case 0x00A0:
      return NS_LITERAL_CSTRING("NO-BREAK SPACE (0x00A0)");
    case 0x00AD:
      return NS_LITERAL_CSTRING("SOFT HYPHEN (0x00AD)");
    case 0x2000:
      return NS_LITERAL_CSTRING("EN QUAD (0x2000)");
    case 0x2001:
      return NS_LITERAL_CSTRING("EM QUAD (0x2001)");
    case 0x2002:
      return NS_LITERAL_CSTRING("EN SPACE (0x2002)");
    case 0x2003:
      return NS_LITERAL_CSTRING("EM SPACE (0x2003)");
    case 0x2004:
      return NS_LITERAL_CSTRING("THREE-PER-EM SPACE (0x2004)");
    case 0x2005:
      return NS_LITERAL_CSTRING("FOUR-PER-EM SPACE (0x2005)");
    case 0x2006:
      return NS_LITERAL_CSTRING("SIX-PER-EM SPACE (0x2006)");
    case 0x2007:
      return NS_LITERAL_CSTRING("FIGURE SPACE (0x2007)");
    case 0x2008:
      return NS_LITERAL_CSTRING("PUNCTUATION SPACE (0x2008)");
    case 0x2009:
      return NS_LITERAL_CSTRING("THIN SPACE (0x2009)");
    case 0x200A:
      return NS_LITERAL_CSTRING("HAIR SPACE (0x200A)");
    case 0x200B:
      return NS_LITERAL_CSTRING("ZERO WIDTH SPACE (0x200B)");
    case 0x200C:
      return NS_LITERAL_CSTRING("ZERO WIDTH NON-JOINER (0x200C)");
    case 0x200D:
      return NS_LITERAL_CSTRING("ZERO WIDTH JOINER (0x200D)");
    case 0x200E:
      return NS_LITERAL_CSTRING("LEFT-TO-RIGHT MARK (0x200E)");
    case 0x200F:
      return NS_LITERAL_CSTRING("RIGHT-TO-LEFT MARK (0x200F)");
    case 0x2029:
      return NS_LITERAL_CSTRING("PARAGRAPH SEPARATOR (0x2029)");
    case 0x202A:
      return NS_LITERAL_CSTRING("LEFT-TO-RIGHT EMBEDDING (0x202A)");
    case 0x202B:
      return NS_LITERAL_CSTRING("RIGHT-TO-LEFT EMBEDDING (0x202B)");
    case 0x202D:
      return NS_LITERAL_CSTRING("LEFT-TO-RIGHT OVERRIDE (0x202D)");
    case 0x202E:
      return NS_LITERAL_CSTRING("RIGHT-TO-LEFT OVERRIDE (0x202E)");
    case 0x202F:
      return NS_LITERAL_CSTRING("NARROW NO-BREAK SPACE (0x202F)");
    case 0x205F:
      return NS_LITERAL_CSTRING("MEDIUM MATHEMATICAL SPACE (0x205F)");
    case 0x2060:
      return NS_LITERAL_CSTRING("WORD JOINER (0x2060)");
    case 0x2066:
      return NS_LITERAL_CSTRING("LEFT-TO-RIGHT ISOLATE (0x2066)");
    case 0x2067:
      return NS_LITERAL_CSTRING("RIGHT-TO-LEFT ISOLATE (0x2067)");
    case 0x3000:
      return NS_LITERAL_CSTRING("IDEOGRAPHIC SPACE (0x3000)");
    case 0xFEFF:
      return NS_LITERAL_CSTRING("ZERO WIDTH NO-BREAK SPACE (0xFEFF)");
    default: {
      if (aChar < ' ' || (aChar >= 0x80 && aChar < 0xA0)) {
        return nsPrintfCString("control (0x%04X)", aChar);
      }
      if (NS_IS_HIGH_SURROGATE(aChar)) {
        return nsPrintfCString("high surrogate (0x%04X)", aChar);
      }
      if (NS_IS_LOW_SURROGATE(aChar)) {
        return nsPrintfCString("low surrogate (0x%04X)", aChar);
      }
      return nsPrintfCString("'%s' (0x%04X)",
                             NS_ConvertUTF16toUTF8(nsAutoString(aChar)).get(),
                             aChar);
    }
  }
}

static const nsCString GetCharacterCodeNames(const char16_t* aChars,
                                             uint32_t aLength) {
  if (!aLength) {
    return NS_LITERAL_CSTRING("\"\"");
  }
  nsCString result;
  for (uint32_t i = 0; i < aLength; ++i) {
    if (!result.IsEmpty()) {
      result.AppendLiteral(", ");
    } else {
      result.AssignLiteral("\"");
    }
    result.Append(GetCharacterCodeName(aChars[i]));
  }
  result.AppendLiteral("\"");
  return result;
}

static const nsCString GetCharacterCodeNames(const nsAString& aString) {
  return GetCharacterCodeNames(aString.BeginReading(), aString.Length());
}

/* static */ const char* KeymapWrapper::GetModifierName(Modifier aModifier) {
  switch (aModifier) {
    case CAPS_LOCK:
      return "CapsLock";
    case NUM_LOCK:
      return "NumLock";
    case SCROLL_LOCK:
      return "ScrollLock";
    case SHIFT:
      return "Shift";
    case CTRL:
      return "Ctrl";
    case ALT:
      return "Alt";
    case SUPER:
      return "Super";
    case HYPER:
      return "Hyper";
    case META:
      return "Meta";
    case LEVEL3:
      return "Level3";
    case LEVEL5:
      return "Level5";
    case NOT_MODIFIER:
      return "NotModifier";
    default:
      return "InvalidValue";
  }
}

/* static */ KeymapWrapper::Modifier KeymapWrapper::GetModifierForGDKKeyval(
    guint aGdkKeyval) {
  switch (aGdkKeyval) {
    case GDK_Caps_Lock:
      return CAPS_LOCK;
    case GDK_Num_Lock:
      return NUM_LOCK;
    case GDK_Scroll_Lock:
      return SCROLL_LOCK;
    case GDK_Shift_Lock:
    case GDK_Shift_L:
    case GDK_Shift_R:
      return SHIFT;
    case GDK_Control_L:
    case GDK_Control_R:
      return CTRL;
    case GDK_Alt_L:
    case GDK_Alt_R:
      return ALT;
    case GDK_Super_L:
    case GDK_Super_R:
      return SUPER;
    case GDK_Hyper_L:
    case GDK_Hyper_R:
      return HYPER;
    case GDK_Meta_L:
    case GDK_Meta_R:
      return META;
    case GDK_ISO_Level3_Shift:
    case GDK_Mode_switch:
      return LEVEL3;
    case GDK_ISO_Level5_Shift:
      return LEVEL5;
    default:
      return NOT_MODIFIER;
  }
}

guint KeymapWrapper::GetModifierMask(Modifier aModifier) const {
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

KeymapWrapper::ModifierKey* KeymapWrapper::GetModifierKey(
    guint aHardwareKeycode) {
  for (uint32_t i = 0; i < mModifierKeys.Length(); i++) {
    ModifierKey& key = mModifierKeys[i];
    if (key.mHardwareKeycode == aHardwareKeycode) {
      return &key;
    }
  }
  return nullptr;
}

/* static */
KeymapWrapper* KeymapWrapper::GetInstance() {
  if (sInstance) {
    sInstance->Init();
    return sInstance;
  }

  sInstance = new KeymapWrapper();
  return sInstance;
}

/* static */
void KeymapWrapper::Shutdown() {
  if (sInstance) {
    delete sInstance;
    sInstance = nullptr;
  }
}

KeymapWrapper::KeymapWrapper()
    : mInitialized(false),
      mGdkKeymap(gdk_keymap_get_default()),
      mXKBBaseEventCode(0) {
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p Constructor, mGdkKeymap=%p", this, mGdkKeymap));

  g_object_ref(mGdkKeymap);

  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) InitXKBExtension();

  Init();
}

void KeymapWrapper::Init() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p Init, mGdkKeymap=%p", this, mGdkKeymap));

  mModifierKeys.Clear();
  memset(mModifierMasks, 0, sizeof(mModifierMasks));

  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) InitBySystemSettingsX11();
#ifdef MOZ_WAYLAND
  else
    InitBySystemSettingsWayland();
#endif

  gdk_window_add_filter(nullptr, FilterEvents, this);

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p Init, CapsLock=0x%X, NumLock=0x%X, "
           "ScrollLock=0x%X, Level3=0x%X, Level5=0x%X, "
           "Shift=0x%X, Ctrl=0x%X, Alt=0x%X, Meta=0x%X, Super=0x%X, Hyper=0x%X",
           this, GetModifierMask(CAPS_LOCK), GetModifierMask(NUM_LOCK),
           GetModifierMask(SCROLL_LOCK), GetModifierMask(LEVEL3),
           GetModifierMask(LEVEL5), GetModifierMask(SHIFT),
           GetModifierMask(CTRL), GetModifierMask(ALT), GetModifierMask(META),
           GetModifierMask(SUPER), GetModifierMask(HYPER)));
}

void KeymapWrapper::InitXKBExtension() {
  PodZero(&mKeyboardState);

  int xkbMajorVer = XkbMajorVersion;
  int xkbMinorVer = XkbMinorVersion;
  if (!XkbLibraryVersion(&xkbMajorVer, &xkbMinorVer)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitXKBExtension failed due to failure of "
             "XkbLibraryVersion()",
             this));
    return;
  }

  Display* display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

  // XkbLibraryVersion() set xkbMajorVer and xkbMinorVer to that of the
  // library, which may be newer than what is required of the server in
  // XkbQueryExtension(), so these variables should be reset to
  // XkbMajorVersion and XkbMinorVersion before the XkbQueryExtension call.
  xkbMajorVer = XkbMajorVersion;
  xkbMinorVer = XkbMinorVersion;
  int opcode, baseErrorCode;
  if (!XkbQueryExtension(display, &opcode, &mXKBBaseEventCode, &baseErrorCode,
                         &xkbMajorVer, &xkbMinorVer)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitXKBExtension failed due to failure of "
             "XkbQueryExtension(), display=0x%p",
             this, display));
    return;
  }

  if (!XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify,
                             XkbModifierStateMask, XkbModifierStateMask)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitXKBExtension failed due to failure of "
             "XkbSelectEventDetails() for XModifierStateMask, display=0x%p",
             this, display));
    return;
  }

  if (!XkbSelectEventDetails(display, XkbUseCoreKbd, XkbControlsNotify,
                             XkbPerKeyRepeatMask, XkbPerKeyRepeatMask)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitXKBExtension failed due to failure of "
             "XkbSelectEventDetails() for XkbControlsNotify, display=0x%p",
             this, display));
    return;
  }

  if (!XGetKeyboardControl(display, &mKeyboardState)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitXKBExtension failed due to failure of "
             "XGetKeyboardControl(), display=0x%p",
             this, display));
    return;
  }

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p InitXKBExtension, Succeeded", this));
}

void KeymapWrapper::InitBySystemSettingsX11() {
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p InitBySystemSettingsX11, mGdkKeymap=%p", this, mGdkKeymap));

  g_signal_connect(mGdkKeymap, "keys-changed", (GCallback)OnKeysChanged, this);
  g_signal_connect(mGdkKeymap, "direction-changed",
                   (GCallback)OnDirectionChanged, this);

  Display* display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

  int min_keycode = 0;
  int max_keycode = 0;
  XDisplayKeycodes(display, &min_keycode, &max_keycode);

  int keysyms_per_keycode = 0;
  KeySym* xkeymap =
      XGetKeyboardMapping(display, min_keycode, max_keycode - min_keycode + 1,
                          &keysyms_per_keycode);
  if (!xkeymap) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitBySystemSettings, "
             "Failed due to null xkeymap",
             this));
    return;
  }

  XModifierKeymap* xmodmap = XGetModifierMapping(display);
  if (!xmodmap) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitBySystemSettings, "
             "Failed due to null xmodmap",
             this));
    XFree(xkeymap);
    return;
  }
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p InitBySystemSettings, min_keycode=%d, "
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
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p InitBySystemSettings, "
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
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("%p InitBySystemSettings, "
               "    Mod%d, j=%d, syms[j]=%s(0x%lX), modifier=%s",
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

#ifdef MOZ_WAYLAND
void KeymapWrapper::SetModifierMask(xkb_keymap* aKeymap,
                                    ModifierIndex aModifierIndex,
                                    const char* aModifierName) {
  static auto sXkbKeymapModGetIndex =
      (xkb_mod_index_t(*)(struct xkb_keymap*, const char*))dlsym(
          RTLD_DEFAULT, "xkb_keymap_mod_get_index");

  xkb_mod_index_t index = sXkbKeymapModGetIndex(aKeymap, aModifierName);
  if (index != XKB_MOD_INVALID) {
    mModifierMasks[aModifierIndex] = (1 << index);
  }
}

void KeymapWrapper::SetModifierMasks(xkb_keymap* aKeymap) {
  KeymapWrapper* keymapWrapper = GetInstance();

  // This mapping is derived from get_xkb_modifiers() at gdkkeys-wayland.c
  keymapWrapper->SetModifierMask(aKeymap, INDEX_NUM_LOCK, XKB_MOD_NAME_NUM);
  keymapWrapper->SetModifierMask(aKeymap, INDEX_ALT, XKB_MOD_NAME_ALT);
  keymapWrapper->SetModifierMask(aKeymap, INDEX_META, "Meta");
  keymapWrapper->SetModifierMask(aKeymap, INDEX_SUPER, "Super");
  keymapWrapper->SetModifierMask(aKeymap, INDEX_HYPER, "Hyper");

  keymapWrapper->SetModifierMask(aKeymap, INDEX_SCROLL_LOCK, "ScrollLock");
  keymapWrapper->SetModifierMask(aKeymap, INDEX_LEVEL3, "Level3");
  keymapWrapper->SetModifierMask(aKeymap, INDEX_LEVEL5, "Level5");

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p KeymapWrapper::SetModifierMasks, CapsLock=0x%X, NumLock=0x%X, "
           "ScrollLock=0x%X, Level3=0x%X, Level5=0x%X, "
           "Shift=0x%X, Ctrl=0x%X, Alt=0x%X, Meta=0x%X, Super=0x%X, Hyper=0x%X",
           keymapWrapper, keymapWrapper->GetModifierMask(CAPS_LOCK),
           keymapWrapper->GetModifierMask(NUM_LOCK),
           keymapWrapper->GetModifierMask(SCROLL_LOCK),
           keymapWrapper->GetModifierMask(LEVEL3),
           keymapWrapper->GetModifierMask(LEVEL5),
           keymapWrapper->GetModifierMask(SHIFT),
           keymapWrapper->GetModifierMask(CTRL),
           keymapWrapper->GetModifierMask(ALT),
           keymapWrapper->GetModifierMask(META),
           keymapWrapper->GetModifierMask(SUPER),
           keymapWrapper->GetModifierMask(HYPER)));
}

/* This keymap routine is derived from weston-2.0.0/clients/simple-im.c
 */
static void keyboard_handle_keymap(void* data, struct wl_keyboard* wl_keyboard,
                                   uint32_t format, int fd, uint32_t size) {
  KeymapWrapper::ResetKeyboard();

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }

  char* mapString = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if (mapString == MAP_FAILED) {
    close(fd);
    return;
  }

  static auto sXkbContextNew =
      (struct xkb_context * (*)(enum xkb_context_flags))
          dlsym(RTLD_DEFAULT, "xkb_context_new");
  static auto sXkbKeymapNewFromString =
      (struct xkb_keymap * (*)(struct xkb_context*, const char*,
                               enum xkb_keymap_format,
                               enum xkb_keymap_compile_flags))
          dlsym(RTLD_DEFAULT, "xkb_keymap_new_from_string");

  struct xkb_context* xkb_context = sXkbContextNew(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap* keymap =
      sXkbKeymapNewFromString(xkb_context, mapString, XKB_KEYMAP_FORMAT_TEXT_V1,
                              XKB_KEYMAP_COMPILE_NO_FLAGS);

  munmap(mapString, size);
  close(fd);

  if (!keymap) {
    NS_WARNING("keyboard_handle_keymap(): Failed to compile keymap!\n");
    return;
  }

  KeymapWrapper::SetModifierMasks(keymap);

  static auto sXkbKeymapUnRef =
      (void (*)(struct xkb_keymap*))dlsym(RTLD_DEFAULT, "xkb_keymap_unref");
  sXkbKeymapUnRef(keymap);

  static auto sXkbContextUnref =
      (void (*)(struct xkb_context*))dlsym(RTLD_DEFAULT, "xkb_context_unref");
  sXkbContextUnref(xkb_context);
}

static void keyboard_handle_enter(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface,
                                  struct wl_array* keys) {}
static void keyboard_handle_leave(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface) {
}
static void keyboard_handle_key(void* data, struct wl_keyboard* keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {}
static void keyboard_handle_modifiers(void* data, struct wl_keyboard* keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group) {}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap, keyboard_handle_enter,     keyboard_handle_leave,
    keyboard_handle_key,    keyboard_handle_modifiers,
};

static void seat_handle_capabilities(void* data, struct wl_seat* seat,
                                     unsigned int caps) {
  static wl_keyboard* keyboard = nullptr;

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
    keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(keyboard, &keyboard_listener, nullptr);
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
    wl_keyboard_destroy(keyboard);
    keyboard = nullptr;
  }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

static void gdk_registry_handle_global(void* data, struct wl_registry* registry,
                                       uint32_t id, const char* interface,
                                       uint32_t version) {
  if (strcmp(interface, "wl_seat") == 0) {
    wl_seat* seat =
        (wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 1);
    wl_seat_add_listener(seat, &seat_listener, data);
  }
}

static void gdk_registry_handle_global_remove(void* data,
                                              struct wl_registry* registry,
                                              uint32_t id) {}

static const struct wl_registry_listener keyboard_registry_listener = {
    gdk_registry_handle_global, gdk_registry_handle_global_remove};

void KeymapWrapper::InitBySystemSettingsWayland() {
  // Available as of GTK 3.8+
  static auto sGdkWaylandDisplayGetWlDisplay = (wl_display * (*)(GdkDisplay*))
      dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");
  wl_display* display =
      sGdkWaylandDisplayGetWlDisplay(gdk_display_get_default());
  wl_registry_add_listener(wl_display_get_registry(display),
                           &keyboard_registry_listener, this);
}
#endif

KeymapWrapper::~KeymapWrapper() {
  gdk_window_remove_filter(nullptr, FilterEvents, this);
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    g_signal_handlers_disconnect_by_func(mGdkKeymap,
                                         FuncToGpointer(OnKeysChanged), this);
    g_signal_handlers_disconnect_by_func(
        mGdkKeymap, FuncToGpointer(OnDirectionChanged), this);
  }
  g_object_unref(mGdkKeymap);
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info, ("%p Destructor", this));
}

/* static */
GdkFilterReturn KeymapWrapper::FilterEvents(GdkXEvent* aXEvent,
                                            GdkEvent* aGdkEvent,
                                            gpointer aData) {
  XEvent* xEvent = static_cast<XEvent*>(aXEvent);
  switch (xEvent->type) {
    case KeyPress: {
      // If the key doesn't support auto repeat, ignore the event because
      // even if such key (e.g., Shift) is pressed during auto repeat of
      // anoter key, it doesn't stop the auto repeat.
      KeymapWrapper* self = static_cast<KeymapWrapper*>(aData);
      if (!self->IsAutoRepeatableKey(xEvent->xkey.keycode)) {
        break;
      }
      if (sRepeatState == NOT_PRESSED) {
        sRepeatState = FIRST_PRESS;
        MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
                ("FilterEvents(aXEvent={ type=KeyPress, "
                 "xkey={ keycode=0x%08X, state=0x%08X, time=%lu } }, "
                 "aGdkEvent={ state=0x%08X }), "
                 "detected first keypress",
                 xEvent->xkey.keycode, xEvent->xkey.state, xEvent->xkey.time,
                 reinterpret_cast<GdkEventKey*>(aGdkEvent)->state));
      } else if (sLastRepeatableHardwareKeyCode == xEvent->xkey.keycode) {
        if (sLastRepeatableKeyTime == xEvent->xkey.time &&
            sLastRepeatableHardwareKeyCode ==
                IMContextWrapper::
                    GetWaitingSynthesizedKeyPressHardwareKeyCode()) {
          // On some environment, IM may generate duplicated KeyPress event
          // without any special state flags.  In such case, we shouldn't
          // treat the event as "repeated".
          MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
                  ("FilterEvents(aXEvent={ type=KeyPress, "
                   "xkey={ keycode=0x%08X, state=0x%08X, time=%lu } }, "
                   "aGdkEvent={ state=0x%08X }), "
                   "igored keypress since it must be synthesized by IME",
                   xEvent->xkey.keycode, xEvent->xkey.state, xEvent->xkey.time,
                   reinterpret_cast<GdkEventKey*>(aGdkEvent)->state));
          break;
        }
        sRepeatState = REPEATING;
        MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
                ("FilterEvents(aXEvent={ type=KeyPress, "
                 "xkey={ keycode=0x%08X, state=0x%08X, time=%lu } }, "
                 "aGdkEvent={ state=0x%08X }), "
                 "detected repeating keypress",
                 xEvent->xkey.keycode, xEvent->xkey.state, xEvent->xkey.time,
                 reinterpret_cast<GdkEventKey*>(aGdkEvent)->state));
      } else {
        // If a different key is pressed while another key is pressed,
        // auto repeat system repeats only the last pressed key.
        // So, setting new keycode and setting repeat state as first key
        // press should work fine.
        sRepeatState = FIRST_PRESS;
        MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
                ("FilterEvents(aXEvent={ type=KeyPress, "
                 "xkey={ keycode=0x%08X, state=0x%08X, time=%lu } }, "
                 "aGdkEvent={ state=0x%08X }), "
                 "detected different keypress",
                 xEvent->xkey.keycode, xEvent->xkey.state, xEvent->xkey.time,
                 reinterpret_cast<GdkEventKey*>(aGdkEvent)->state));
      }
      sLastRepeatableHardwareKeyCode = xEvent->xkey.keycode;
      sLastRepeatableKeyTime = xEvent->xkey.time;
      break;
    }
    case KeyRelease: {
      if (sLastRepeatableHardwareKeyCode != xEvent->xkey.keycode) {
        // This case means the key release event is caused by
        // a non-repeatable key such as Shift or a repeatable key that
        // was pressed before sLastRepeatableHardwareKeyCode was
        // pressed.
        break;
      }
      sRepeatState = NOT_PRESSED;
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("FilterEvents(aXEvent={ type=KeyRelease, "
               "xkey={ keycode=0x%08X, state=0x%08X, time=%lu } }, "
               "aGdkEvent={ state=0x%08X }), "
               "detected key release",
               xEvent->xkey.keycode, xEvent->xkey.state, xEvent->xkey.time,
               reinterpret_cast<GdkEventKey*>(aGdkEvent)->state));
      break;
    }
    case FocusOut: {
      // At moving focus, we should reset keyboard repeat state.
      // Strictly, this causes incorrect behavior.  However, this
      // correctness must be enough for web applications.
      sRepeatState = NOT_PRESSED;
      break;
    }
    default: {
      KeymapWrapper* self = static_cast<KeymapWrapper*>(aData);
      if (xEvent->type != self->mXKBBaseEventCode) {
        break;
      }
      XkbEvent* xkbEvent = (XkbEvent*)xEvent;
      if (xkbEvent->any.xkb_type != XkbControlsNotify ||
          !(xkbEvent->ctrls.changed_ctrls & XkbPerKeyRepeatMask)) {
        break;
      }
      if (!XGetKeyboardControl(xkbEvent->any.display, &self->mKeyboardState)) {
        MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
                ("%p FilterEvents failed due to failure "
                 "of XGetKeyboardControl(), display=0x%p",
                 self, xkbEvent->any.display));
      }
      break;
    }
  }

  return GDK_FILTER_CONTINUE;
}

static void ResetBidiKeyboard() {
  // Reset the bidi keyboard settings for the new GdkKeymap
  nsCOMPtr<nsIBidiKeyboard> bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (bidiKeyboard) {
    bidiKeyboard->Reset();
  }
  WidgetUtils::SendBidiKeyboardInfoToContent();
}

/* static */
void KeymapWrapper::ResetKeyboard() {
  sInstance->mInitialized = false;
  ResetBidiKeyboard();
}

/* static */
void KeymapWrapper::OnKeysChanged(GdkKeymap* aGdkKeymap,
                                  KeymapWrapper* aKeymapWrapper) {
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("OnKeysChanged, aGdkKeymap=%p, aKeymapWrapper=%p", aGdkKeymap,
           aKeymapWrapper));

  MOZ_ASSERT(sInstance == aKeymapWrapper,
             "This instance must be the singleton instance");

  // We cannot reintialize here becasue we don't have GdkWindow which is using
  // the GdkKeymap.  We'll reinitialize it when next GetInstance() is called.
  ResetKeyboard();
}

// static
void KeymapWrapper::OnDirectionChanged(GdkKeymap* aGdkKeymap,
                                       KeymapWrapper* aKeymapWrapper) {
  // XXX
  // A lot of diretion-changed signal might be fired on switching bidi
  // keyboard when using both ibus (with arabic layout) and fcitx (with IME).
  // See https://github.com/fcitx/fcitx/issues/257
  //
  // Also, when using ibus, switching to IM might not cause this signal.
  // See https://github.com/ibus/ibus/issues/1848

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("OnDirectionChanged, aGdkKeymap=%p, aKeymapWrapper=%p", aGdkKeymap,
           aKeymapWrapper));

  ResetBidiKeyboard();
}

/* static */
guint KeymapWrapper::GetCurrentModifierState() {
  GdkModifierType modifiers;
  gdk_display_get_pointer(gdk_display_get_default(), nullptr, nullptr, nullptr,
                          &modifiers);
  return static_cast<guint>(modifiers);
}

/* static */
bool KeymapWrapper::AreModifiersCurrentlyActive(Modifiers aModifiers) {
  guint modifierState = GetCurrentModifierState();
  return AreModifiersActive(aModifiers, modifierState);
}

/* static */
bool KeymapWrapper::AreModifiersActive(Modifiers aModifiers,
                                       guint aModifierState) {
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

/* static */
uint32_t KeymapWrapper::ComputeCurrentKeyModifiers() {
  return ComputeKeyModifiers(GetCurrentModifierState());
}

/* static */
uint32_t KeymapWrapper::ComputeKeyModifiers(guint aModifierState) {
  KeymapWrapper* keymapWrapper = GetInstance();

  uint32_t keyModifiers = 0;
  // DOM Meta key should be TRUE only on Mac.  We need to discuss this
  // issue later.
  if (keymapWrapper->AreModifiersActive(SHIFT, aModifierState)) {
    keyModifiers |= MODIFIER_SHIFT;
  }
  if (keymapWrapper->AreModifiersActive(CTRL, aModifierState)) {
    keyModifiers |= MODIFIER_CONTROL;
  }
  if (keymapWrapper->AreModifiersActive(ALT, aModifierState)) {
    keyModifiers |= MODIFIER_ALT;
  }
  if (keymapWrapper->AreModifiersActive(META, aModifierState)) {
    keyModifiers |= MODIFIER_META;
  }
  if (keymapWrapper->AreModifiersActive(SUPER, aModifierState) ||
      keymapWrapper->AreModifiersActive(HYPER, aModifierState)) {
    keyModifiers |= MODIFIER_OS;
  }
  if (keymapWrapper->AreModifiersActive(LEVEL3, aModifierState) ||
      keymapWrapper->AreModifiersActive(LEVEL5, aModifierState)) {
    keyModifiers |= MODIFIER_ALTGRAPH;
  }
  if (keymapWrapper->AreModifiersActive(CAPS_LOCK, aModifierState)) {
    keyModifiers |= MODIFIER_CAPSLOCK;
  }
  if (keymapWrapper->AreModifiersActive(NUM_LOCK, aModifierState)) {
    keyModifiers |= MODIFIER_NUMLOCK;
  }
  if (keymapWrapper->AreModifiersActive(SCROLL_LOCK, aModifierState)) {
    keyModifiers |= MODIFIER_SCROLLLOCK;
  }
  return keyModifiers;
}

/* static */
void KeymapWrapper::InitInputEvent(WidgetInputEvent& aInputEvent,
                                   guint aModifierState) {
  KeymapWrapper* keymapWrapper = GetInstance();

  aInputEvent.mModifiers = ComputeKeyModifiers(aModifierState);

  // Don't log this method for non-important events because e.g., eMouseMove is
  // just noisy and there is no reason to log it.
  bool doLog = aInputEvent.mMessage != eMouseMove;
  if (doLog) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Debug,
            ("%p InitInputEvent, aModifierState=0x%08X, "
             "aInputEvent={ mMessage=%s, mModifiers=0x%04X (Shift: %s, "
             "Control: %s, Alt: %s, "
             "Meta: %s, OS: %s, AltGr: %s, "
             "CapsLock: %s, NumLock: %s, ScrollLock: %s })",
             keymapWrapper, aModifierState, ToChar(aInputEvent.mMessage),
             aInputEvent.mModifiers,
             GetBoolName(aInputEvent.mModifiers & MODIFIER_SHIFT),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_CONTROL),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_ALT),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_META),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_OS),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_ALTGRAPH),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_CAPSLOCK),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_NUMLOCK),
             GetBoolName(aInputEvent.mModifiers & MODIFIER_SCROLLLOCK)));
  }

  switch (aInputEvent.mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case eSimpleGestureEventClass:
      break;
    default:
      return;
  }

  WidgetMouseEventBase& mouseEvent = *aInputEvent.AsMouseEventBase();
  mouseEvent.mButtons = 0;
  if (aModifierState & GDK_BUTTON1_MASK) {
    mouseEvent.mButtons |= MouseButtonsFlag::eLeftFlag;
  }
  if (aModifierState & GDK_BUTTON3_MASK) {
    mouseEvent.mButtons |= MouseButtonsFlag::eRightFlag;
  }
  if (aModifierState & GDK_BUTTON2_MASK) {
    mouseEvent.mButtons |= MouseButtonsFlag::eMiddleFlag;
  }

  if (doLog) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Debug,
            ("%p InitInputEvent, aInputEvent has mButtons, "
             "aInputEvent.mButtons=0x%04X (Left: %s, Right: %s, Middle: %s, "
             "4th (BACK): %s, 5th (FORWARD): %s)",
             keymapWrapper, mouseEvent.mButtons,
             GetBoolName(mouseEvent.mButtons & MouseButtonsFlag::eLeftFlag),
             GetBoolName(mouseEvent.mButtons & MouseButtonsFlag::eRightFlag),
             GetBoolName(mouseEvent.mButtons & MouseButtonsFlag::eMiddleFlag),
             GetBoolName(mouseEvent.mButtons & MouseButtonsFlag::e4thFlag),
             GetBoolName(mouseEvent.mButtons & MouseButtonsFlag::e5thFlag)));
  }
}

/* static */
uint32_t KeymapWrapper::ComputeDOMKeyCode(const GdkEventKey* aGdkKeyEvent) {
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
    // Note that any key may be a function key because of some unusual keyboard
    // layouts.  I.e., even if the pressed key is a printable key of en-US
    // keyboard layout, we should expose the function key's keyCode value to
    // web apps because web apps should handle the keydown/keyup events as
    // inputted by usual keyboard layout.  For example, Hatchak keyboard
    // maps Tab key to "Digit3" key and Level3 Shift makes it "Backspace".
    // In this case, we should expose DOM_VK_BACK_SPACE (8).
    uint32_t DOMKeyCode = GetDOMKeyCodeFromKeyPairs(keyval);
    if (DOMKeyCode) {
      // XXX If DOMKeyCode is a function key's keyCode value, it might be
      //     better to consume necessary modifiers.  For example, if there is
      //     no Control Pad section on keyboard like notebook, Delete key is
      //     available only with Level3 Shift+"Backspace" key if using Hatchak.
      //     If web apps accept Delete key operation only when no modifiers are
      //     active, such users cannot use Delete key to do it.  However,
      //     Chromium doesn't consume such necessary modifiers.  So, our default
      //     behavior should keep not touching modifiers for compatibility, but
      //     it might be better to add a pref to consume necessary modifiers.
      return DOMKeyCode;
    }
    // If aGdkKeyEvent cannot be mapped to a DOM keyCode value, we should
    // refer keyCode value without modifiers because web apps should be
    // able to identify the key as far as possible.
    guint keyvalWithoutModifier = GetGDKKeyvalWithoutModifier(aGdkKeyEvent);
    return GetDOMKeyCodeFromKeyPairs(keyvalWithoutModifier);
  }

  // printable numpad keys should be resolved here.
  switch (keyval) {
    case GDK_KP_Multiply:
      return NS_VK_MULTIPLY;
    case GDK_KP_Add:
      return NS_VK_ADD;
    case GDK_KP_Separator:
      return NS_VK_SEPARATOR;
    case GDK_KP_Subtract:
      return NS_VK_SUBTRACT;
    case GDK_KP_Decimal:
      return NS_VK_DECIMAL;
    case GDK_KP_Divide:
      return NS_VK_DIVIDE;
    case GDK_KP_0:
      return NS_VK_NUMPAD0;
    case GDK_KP_1:
      return NS_VK_NUMPAD1;
    case GDK_KP_2:
      return NS_VK_NUMPAD2;
    case GDK_KP_3:
      return NS_VK_NUMPAD3;
    case GDK_KP_4:
      return NS_VK_NUMPAD4;
    case GDK_KP_5:
      return NS_VK_NUMPAD5;
    case GDK_KP_6:
      return NS_VK_NUMPAD6;
    case GDK_KP_7:
      return NS_VK_NUMPAD7;
    case GDK_KP_8:
      return NS_VK_NUMPAD8;
    case GDK_KP_9:
      return NS_VK_NUMPAD9;
  }

  KeymapWrapper* keymapWrapper = GetInstance();

  // Ignore all modifier state except NumLock.
  guint baseState =
      (aGdkKeyEvent->state & keymapWrapper->GetModifierMask(NUM_LOCK));

  // Basically, we should use unmodified character for deciding our keyCode.
  uint32_t unmodifiedChar = keymapWrapper->GetCharCodeFor(
      aGdkKeyEvent, baseState, aGdkKeyEvent->group);
  if (IsBasicLatinLetterOrNumeral(unmodifiedChar)) {
    // If the unmodified character is an ASCII alphabet or an ASCII
    // numeric, it's the best hint for deciding our keyCode.
    return WidgetUtils::ComputeKeyCodeFromChar(unmodifiedChar);
  }

  // If the unmodified character is not an ASCII character, that means we
  // couldn't find the hint. We should reset it.
  if (!IsPrintableASCIICharacter(unmodifiedChar)) {
    unmodifiedChar = 0;
  }

  // Retry with shifted keycode.
  guint shiftState = (baseState | keymapWrapper->GetModifierMask(SHIFT));
  uint32_t shiftedChar = keymapWrapper->GetCharCodeFor(aGdkKeyEvent, shiftState,
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
  if (!IsPrintableASCIICharacter(shiftedChar)) {
    shiftedChar = 0;
  }

  // If current keyboard layout isn't ASCII alphabet inputtable layout,
  // look for ASCII alphabet inputtable keyboard layout.  If the key
  // inputs an ASCII alphabet or an ASCII numeric, we should use it
  // for deciding our keyCode.
  uint32_t unmodCharLatin = 0;
  uint32_t shiftedCharLatin = 0;
  if (!keymapWrapper->IsLatinGroup(aGdkKeyEvent->group)) {
    gint minGroup = keymapWrapper->GetFirstLatinGroup();
    if (minGroup >= 0) {
      unmodCharLatin =
          keymapWrapper->GetCharCodeFor(aGdkKeyEvent, baseState, minGroup);
      if (IsBasicLatinLetterOrNumeral(unmodCharLatin)) {
        // If the unmodified character is an ASCII alphabet or
        // an ASCII numeric, we should use it for the keyCode.
        return WidgetUtils::ComputeKeyCodeFromChar(unmodCharLatin);
      }
      // If the unmodified character in the alternative ASCII capable
      // keyboard layout isn't an ASCII character, that means we couldn't
      // find the hint. We should reset it.
      if (!IsPrintableASCIICharacter(unmodCharLatin)) {
        unmodCharLatin = 0;
      }
      shiftedCharLatin =
          keymapWrapper->GetCharCodeFor(aGdkKeyEvent, shiftState, minGroup);
      if (IsBasicLatinLetterOrNumeral(shiftedCharLatin)) {
        // If the shifted character is an ASCII alphabet or an ASCII
        // numeric, we should use it for the keyCode.
        return WidgetUtils::ComputeKeyCodeFromChar(shiftedCharLatin);
      }
      // If the shifted unmodified character in the alternative ASCII
      // capable keyboard layout isn't an ASCII character, we should
      // discard it too.
      if (!IsPrintableASCIICharacter(shiftedCharLatin)) {
        shiftedCharLatin = 0;
      }
    }
  }

  // If the key itself or with Shift state on active keyboard layout produces
  // an ASCII punctuation character, we should decide keyCode value with it.
  if (unmodifiedChar || shiftedChar) {
    return WidgetUtils::ComputeKeyCodeFromChar(unmodifiedChar ? unmodifiedChar
                                                              : shiftedChar);
  }

  // If the key itself or with Shift state on alternative ASCII capable
  // keyboard layout produces an ASCII punctuation character, we should
  // decide keyCode value with it.  Note that We've returned 0 for long
  // time if keyCode isn't for an alphabet keys or a numeric key even in
  // alternative ASCII capable keyboard layout because we decided that we
  // should avoid setting same keyCode value to 2 or more keys since active
  // keyboard layout may have a key to input the punctuation with different
  // key.  However, setting keyCode to 0 makes some web applications which
  // are aware of neither KeyboardEvent.key nor KeyboardEvent.code not work
  // with Firefox when user selects non-ASCII capable keyboard layout such
  // as Russian and Thai.  So, if alternative ASCII capable keyboard layout
  // has keyCode value for the key, we should use it.  In other words, this
  // behavior means that non-ASCII capable keyboard layout overrides some
  // keys' keyCode value only if the key produces ASCII character by itself
  // or with Shift key.
  if (unmodCharLatin || shiftedCharLatin) {
    return WidgetUtils::ComputeKeyCodeFromChar(
        unmodCharLatin ? unmodCharLatin : shiftedCharLatin);
  }

  // Otherwise, let's decide keyCode value from the hardware_keycode
  // value on major keyboard layout.
  CodeNameIndex code = ComputeDOMCodeNameIndex(aGdkKeyEvent);
  return WidgetKeyboardEvent::GetFallbackKeyCodeOfPunctuationKey(code);
}

KeyNameIndex KeymapWrapper::ComputeDOMKeyNameIndex(
    const GdkEventKey* aGdkKeyEvent) {
  switch (aGdkKeyEvent->keyval) {
#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
  case aNativeKey:                                                     \
    return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    default:
      break;
  }

  return KEY_NAME_INDEX_Unidentified;
}

/* static */
CodeNameIndex KeymapWrapper::ComputeDOMCodeNameIndex(
    const GdkEventKey* aGdkKeyEvent) {
  switch (aGdkKeyEvent->hardware_keycode) {
#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
  case aNativeKey:                                                       \
    return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

    default:
      break;
  }

  return CODE_NAME_INDEX_UNKNOWN;
}

/* static */
bool KeymapWrapper::DispatchKeyDownOrKeyUpEvent(nsWindow* aWindow,
                                                GdkEventKey* aGdkKeyEvent,
                                                bool aIsProcessedByIME,
                                                bool* aIsCancelled) {
  MOZ_ASSERT(aIsCancelled, "aIsCancelled must not be nullptr");

  *aIsCancelled = false;

  if (aGdkKeyEvent->type == GDK_KEY_PRESS && aGdkKeyEvent->keyval == GDK_Tab &&
      AreModifiersActive(CTRL | ALT, aGdkKeyEvent->state)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("  DispatchKeyDownOrKeyUpEvent(), didn't dispatch keyboard events "
             "because it's Ctrl + Alt + Tab"));
    return false;
  }

  EventMessage message =
      aGdkKeyEvent->type == GDK_KEY_PRESS ? eKeyDown : eKeyUp;
  WidgetKeyboardEvent keyEvent(true, message, aWindow);
  KeymapWrapper::InitKeyEvent(keyEvent, aGdkKeyEvent, aIsProcessedByIME);
  return DispatchKeyDownOrKeyUpEvent(aWindow, keyEvent, aIsCancelled);
}

/* static */
bool KeymapWrapper::DispatchKeyDownOrKeyUpEvent(
    nsWindow* aWindow, WidgetKeyboardEvent& aKeyboardEvent,
    bool* aIsCancelled) {
  MOZ_ASSERT(aIsCancelled, "aIsCancelled must not be nullptr");

  *aIsCancelled = false;

  RefPtr<TextEventDispatcher> dispatcher = aWindow->GetTextEventDispatcher();
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Error,
            ("  DispatchKeyDownOrKeyUpEvent(), stopped dispatching %s event "
             "because of failed to initialize TextEventDispatcher",
             ToChar(aKeyboardEvent.mMessage)));
    return FALSE;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  bool dispatched = dispatcher->DispatchKeyboardEvent(
      aKeyboardEvent.mMessage, aKeyboardEvent, status, nullptr);
  *aIsCancelled = (status == nsEventStatus_eConsumeNoDefault);
  return dispatched;
}

/* static */
bool KeymapWrapper::MaybeDispatchContextMenuEvent(nsWindow* aWindow,
                                                  const GdkEventKey* aEvent) {
  KeyNameIndex keyNameIndex = ComputeDOMKeyNameIndex(aEvent);

  // Shift+F10 and ContextMenu should cause eContextMenu event.
  if (keyNameIndex != KEY_NAME_INDEX_F10 &&
      keyNameIndex != KEY_NAME_INDEX_ContextMenu) {
    return false;
  }

  WidgetMouseEvent contextMenuEvent(true, eContextMenu, aWindow,
                                    WidgetMouseEvent::eReal,
                                    WidgetMouseEvent::eContextMenuKey);

  contextMenuEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
  contextMenuEvent.AssignEventTime(aWindow->GetWidgetEventTime(aEvent->time));
  contextMenuEvent.mClickCount = 1;
  KeymapWrapper::InitInputEvent(contextMenuEvent, aEvent->state);

  if (contextMenuEvent.IsControl() || contextMenuEvent.IsMeta() ||
      contextMenuEvent.IsAlt()) {
    return false;
  }

  // If the key is ContextMenu, then an eContextMenu mouse event is
  // dispatched regardless of the state of the Shift modifier.  When it is
  // pressed without the Shift modifier, a web page can prevent the default
  // context menu action.  When pressed with the Shift modifier, the web page
  // cannot prevent the default context menu action.
  // (PresShell::HandleEventInternal() sets mOnlyChromeDispatch to true.)

  // If the key is F10, it needs Shift state because Shift+F10 is well-known
  // shortcut key on Linux.  However, eContextMenu with Shift state is
  // special.  It won't fire "contextmenu" event in the web content for
  // blocking web page to prevent its default.  Therefore, this combination
  // should work same as ContextMenu key.
  // XXX Should we allow to block web page to prevent its default with
  //     Ctrl+Shift+F10 or Alt+Shift+F10 instead?
  if (keyNameIndex == KEY_NAME_INDEX_F10) {
    if (!contextMenuEvent.IsShift()) {
      return false;
    }
    contextMenuEvent.mModifiers &= ~MODIFIER_SHIFT;
  }

  aWindow->DispatchInputEvent(&contextMenuEvent);
  return true;
}

/* static*/
void KeymapWrapper::HandleKeyPressEvent(nsWindow* aWindow,
                                        GdkEventKey* aGdkKeyEvent) {
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("HandleKeyPressEvent(aWindow=%p, aGdkKeyEvent={ type=%s, "
           "keyval=%s(0x%X), state=0x%08X, hardware_keycode=0x%08X, "
           "time=%u, is_modifier=%s })",
           aWindow,
           ((aGdkKeyEvent->type == GDK_KEY_PRESS) ? "GDK_KEY_PRESS"
                                                  : "GDK_KEY_RELEASE"),
           gdk_keyval_name(aGdkKeyEvent->keyval), aGdkKeyEvent->keyval,
           aGdkKeyEvent->state, aGdkKeyEvent->hardware_keycode,
           aGdkKeyEvent->time, GetBoolName(aGdkKeyEvent->is_modifier)));

  // if we are in the middle of composing text, XIM gets to see it
  // before mozilla does.
  // FYI: Don't dispatch keydown event before notifying IME of the event
  //      because IME may send a key event synchronously and consume the
  //      original event.
  bool IMEWasEnabled = false;
  KeyHandlingState handlingState = KeyHandlingState::eNotHandled;
  RefPtr<IMContextWrapper> imContext = aWindow->GetIMContext();
  if (imContext) {
    IMEWasEnabled = imContext->IsEnabled();
    handlingState = imContext->OnKeyEvent(aWindow, aGdkKeyEvent);
    if (handlingState == KeyHandlingState::eHandled) {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), the event was handled by "
               "IMContextWrapper"));
      return;
    }
  }

  // work around for annoying things.
  if (aGdkKeyEvent->keyval == GDK_Tab &&
      AreModifiersActive(CTRL | ALT, aGdkKeyEvent->state)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("  HandleKeyPressEvent(), didn't dispatch keyboard events "
             "because it's Ctrl + Alt + Tab"));
    return;
  }

  // Dispatch keydown event always.  At auto repeating, we should send
  // KEYDOWN -> KEYPRESS -> KEYDOWN -> KEYPRESS ... -> KEYUP
  // However, old distributions (e.g., Ubuntu 9.10) sent native key
  // release event, so, on such platform, the DOM events will be:
  // KEYDOWN -> KEYPRESS -> KEYUP -> KEYDOWN -> KEYPRESS -> KEYUP...

  bool isKeyDownCancelled = false;
  if (handlingState == KeyHandlingState::eNotHandled) {
    if (DispatchKeyDownOrKeyUpEvent(aWindow, aGdkKeyEvent, false,
                                    &isKeyDownCancelled) &&
        (MOZ_UNLIKELY(aWindow->IsDestroyed()) || isKeyDownCancelled)) {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched eKeyDown event and "
               "stopped handling the event because %s",
               aWindow->IsDestroyed() ? "the window has been destroyed"
                                      : "the event was consumed"));
      return;
    }
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("  HandleKeyPressEvent(), dispatched eKeyDown event and "
             "it wasn't consumed"));
    handlingState = KeyHandlingState::eNotHandledButEventDispatched;
  }

  // If a keydown event handler causes to enable IME, i.e., it moves
  // focus from IME unusable content to IME usable editor, we should
  // send the native key event to IME for the first input on the editor.
  imContext = aWindow->GetIMContext();
  if (!IMEWasEnabled && imContext && imContext->IsEnabled()) {
    // Notice our keydown event was already dispatched.  This prevents
    // unnecessary DOM keydown event in the editor.
    handlingState = imContext->OnKeyEvent(aWindow, aGdkKeyEvent, true);
    if (handlingState == KeyHandlingState::eHandled) {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), the event was handled by "
               "IMContextWrapper which was enabled by the preceding eKeyDown "
               "event"));
      return;
    }
  }

  // Look for specialized app-command keys
  switch (aGdkKeyEvent->keyval) {
    case GDK_Back:
      aWindow->DispatchCommandEvent(nsGkAtoms::Back);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Back\" command event"));
      return;
    case GDK_Forward:
      aWindow->DispatchCommandEvent(nsGkAtoms::Forward);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Forward\" command "
               "event"));
      return;
    case GDK_Refresh:
      aWindow->DispatchCommandEvent(nsGkAtoms::Reload);
      return;
    case GDK_Stop:
      aWindow->DispatchCommandEvent(nsGkAtoms::Stop);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Stop\" command event"));
      return;
    case GDK_Search:
      aWindow->DispatchCommandEvent(nsGkAtoms::Search);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Search\" command event"));
      return;
    case GDK_Favorites:
      aWindow->DispatchCommandEvent(nsGkAtoms::Bookmarks);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Bookmarks\" command "
               "event"));
      return;
    case GDK_HomePage:
      aWindow->DispatchCommandEvent(nsGkAtoms::Home);
      return;
    case GDK_Copy:
    case GDK_F16:  // F16, F20, F18, F14 are old keysyms for Copy Cut Paste Undo
      aWindow->DispatchContentCommandEvent(eContentCommandCopy);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Copy\" content command "
               "event"));
      return;
    case GDK_Cut:
    case GDK_F20:
      aWindow->DispatchContentCommandEvent(eContentCommandCut);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Cut\" content command "
               "event"));
      return;
    case GDK_Paste:
    case GDK_F18:
      aWindow->DispatchContentCommandEvent(eContentCommandPaste);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Paste\" content command "
               "event"));
      return;
    case GDK_Redo:
      aWindow->DispatchContentCommandEvent(eContentCommandRedo);
      return;
    case GDK_Undo:
    case GDK_F14:
      aWindow->DispatchContentCommandEvent(eContentCommandUndo);
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched \"Undo\" content command "
               "event"));
      return;
    default:
      break;
  }

  // before we dispatch a key, check if it's the context menu key.
  // If so, send a context menu key event instead.
  if (MaybeDispatchContextMenuEvent(aWindow, aGdkKeyEvent)) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("  HandleKeyPressEvent(), stopped dispatching eKeyPress event "
             "because eContextMenu event was dispatched"));
    return;
  }

  RefPtr<TextEventDispatcher> textEventDispatcher =
      aWindow->GetTextEventDispatcher();
  nsresult rv = textEventDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Error,
            ("  HandleKeyPressEvent(), stopped dispatching eKeyPress event "
             "because of failed to initialize TextEventDispatcher"));
    return;
  }

  // If the character code is in the BMP, send the key press event.
  // Otherwise, send a compositionchange event with the equivalent UTF-16
  // string.
  // TODO: Investigate other browser's behavior in this case because
  //       this hack is odd for UI Events.
  WidgetKeyboardEvent keypressEvent(true, eKeyPress, aWindow);
  KeymapWrapper::InitKeyEvent(keypressEvent, aGdkKeyEvent, false);
  nsEventStatus status = nsEventStatus_eIgnore;
  if (keypressEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING ||
      keypressEvent.mKeyValue.Length() == 1) {
    if (textEventDispatcher->MaybeDispatchKeypressEvents(keypressEvent, status,
                                                         aGdkKeyEvent)) {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), dispatched eKeyPress event "
               "(status=%s)",
               GetStatusName(status)));
    } else {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyPressEvent(), didn't dispatch eKeyPress event "
               "(status=%s)",
               GetStatusName(status)));
    }
  } else {
    WidgetEventTime eventTime = aWindow->GetWidgetEventTime(aGdkKeyEvent->time);
    textEventDispatcher->CommitComposition(status, &keypressEvent.mKeyValue,
                                           &eventTime);
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("  HandleKeyPressEvent(), dispatched a set of composition "
             "events"));
  }
}

/* static */
bool KeymapWrapper::HandleKeyReleaseEvent(nsWindow* aWindow,
                                          GdkEventKey* aGdkKeyEvent) {
  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("HandleKeyReleaseEvent(aWindow=%p, aGdkKeyEvent={ type=%s, "
           "keyval=%s(0x%X), state=0x%08X, hardware_keycode=0x%08X, "
           "time=%u, is_modifier=%s })",
           aWindow,
           ((aGdkKeyEvent->type == GDK_KEY_PRESS) ? "GDK_KEY_PRESS"
                                                  : "GDK_KEY_RELEASE"),
           gdk_keyval_name(aGdkKeyEvent->keyval), aGdkKeyEvent->keyval,
           aGdkKeyEvent->state, aGdkKeyEvent->hardware_keycode,
           aGdkKeyEvent->time, GetBoolName(aGdkKeyEvent->is_modifier)));

  RefPtr<IMContextWrapper> imContext = aWindow->GetIMContext();
  if (imContext) {
    KeyHandlingState handlingState =
        imContext->OnKeyEvent(aWindow, aGdkKeyEvent);
    if (handlingState != KeyHandlingState::eNotHandled) {
      MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
              ("  HandleKeyReleaseEvent(), the event was handled by "
               "IMContextWrapper"));
      return true;
    }
  }

  bool isCancelled = false;
  if (NS_WARN_IF(!DispatchKeyDownOrKeyUpEvent(aWindow, aGdkKeyEvent, false,
                                              &isCancelled))) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Error,
            ("  HandleKeyReleaseEvent(), didn't dispatch eKeyUp event"));
    return false;
  }

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("  HandleKeyReleaseEvent(), dispatched eKeyUp event "
           "(isCancelled=%s)",
           GetBoolName(isCancelled)));
  return true;
}

/* static */
void KeymapWrapper::InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                                 GdkEventKey* aGdkKeyEvent,
                                 bool aIsProcessedByIME) {
  MOZ_ASSERT(
      !aIsProcessedByIME || aKeyEvent.mMessage != eKeyPress,
      "If the key event is handled by IME, keypress event shouldn't be fired");

  KeymapWrapper* keymapWrapper = GetInstance();

  aKeyEvent.mCodeNameIndex = ComputeDOMCodeNameIndex(aGdkKeyEvent);
  MOZ_ASSERT(aKeyEvent.mCodeNameIndex != CODE_NAME_INDEX_USE_STRING);
  aKeyEvent.mKeyNameIndex =
      aIsProcessedByIME ? KEY_NAME_INDEX_Process
                        : keymapWrapper->ComputeDOMKeyNameIndex(aGdkKeyEvent);
  if (aKeyEvent.mKeyNameIndex == KEY_NAME_INDEX_Unidentified) {
    uint32_t charCode = GetCharCodeFor(aGdkKeyEvent);
    if (!charCode) {
      charCode = keymapWrapper->GetUnmodifiedCharCodeFor(aGdkKeyEvent);
    }
    if (charCode) {
      aKeyEvent.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
      MOZ_ASSERT(aKeyEvent.mKeyValue.IsEmpty(),
                 "Uninitialized mKeyValue must be empty");
      AppendUCS4ToUTF16(charCode, aKeyEvent.mKeyValue);
    }
  }

  if (aIsProcessedByIME) {
    aKeyEvent.mKeyCode = NS_VK_PROCESSKEY;
  } else if (aKeyEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING ||
             aKeyEvent.mMessage != eKeyPress) {
    aKeyEvent.mKeyCode = ComputeDOMKeyCode(aGdkKeyEvent);
  } else {
    aKeyEvent.mKeyCode = 0;
  }

  // NOTE: The state of given key event indicates adjacent state of
  // modifier keys.  E.g., even if the event is Shift key press event,
  // the bit for Shift is still false.  By the same token, even if the
  // event is Shift key release event, the bit for Shift is still true.
  // Unfortunately, gdk_keyboard_get_modifiers() returns current modifier
  // state.  It means if there're some pending modifier key press or
  // key release events, the result isn't what we want.
  guint modifierState = aGdkKeyEvent->state;
  GdkDisplay* gdkDisplay = gdk_display_get_default();
  if (aGdkKeyEvent->is_modifier && GDK_IS_X11_DISPLAY(gdkDisplay)) {
    Display* display = gdk_x11_display_get_xdisplay(gdkDisplay);
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
      aKeyEvent.mLocation = eKeyLocationLeft;
      break;

    case GDK_Shift_R:
    case GDK_Control_R:
    case GDK_Alt_R:
    case GDK_Super_R:
    case GDK_Hyper_R:
    case GDK_Meta_R:
      aKeyEvent.mLocation = eKeyLocationRight;
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
    case GDK_KP_Prior:  // same as GDK_KP_Page_Up
    case GDK_KP_Next:   // same as GDK_KP_Page_Down
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
      aKeyEvent.mLocation = eKeyLocationNumpad;
      break;

    default:
      aKeyEvent.mLocation = eKeyLocationStandard;
      break;
  }

  // The transformations above and in gdk for the keyval are not invertible
  // so link to the GdkEvent (which will vanish soon after return from the
  // event callback) to give plugins access to hardware_keycode and state.
  // (An XEvent would be nice but the GdkEvent is good enough.)
  aKeyEvent.mPluginEvent.Copy(*aGdkKeyEvent);
  aKeyEvent.mTime = aGdkKeyEvent->time;
  aKeyEvent.mNativeKeyEvent = static_cast<void*>(aGdkKeyEvent);
  aKeyEvent.mIsRepeat =
      sRepeatState == REPEATING &&
      aGdkKeyEvent->hardware_keycode == sLastRepeatableHardwareKeyCode;

  MOZ_LOG(
      gKeymapWrapperLog, LogLevel::Info,
      ("%p InitKeyEvent, modifierState=0x%08X "
       "aKeyEvent={ mMessage=%s, isShift=%s, isControl=%s, "
       "isAlt=%s, isMeta=%s , mKeyCode=0x%02X, mCharCode=%s, "
       "mKeyNameIndex=%s, mKeyValue=%s, mCodeNameIndex=%s, mCodeValue=%s, "
       "mLocation=%s, mIsRepeat=%s }",
       keymapWrapper, modifierState, ToChar(aKeyEvent.mMessage),
       GetBoolName(aKeyEvent.IsShift()), GetBoolName(aKeyEvent.IsControl()),
       GetBoolName(aKeyEvent.IsAlt()), GetBoolName(aKeyEvent.IsMeta()),
       aKeyEvent.mKeyCode,
       GetCharacterCodeName(static_cast<char16_t>(aKeyEvent.mCharCode)).get(),
       ToString(aKeyEvent.mKeyNameIndex).get(),
       GetCharacterCodeNames(aKeyEvent.mKeyValue).get(),
       ToString(aKeyEvent.mCodeNameIndex).get(),
       GetCharacterCodeNames(aKeyEvent.mCodeValue).get(),
       GetKeyLocationName(aKeyEvent.mLocation).get(),
       GetBoolName(aKeyEvent.mIsRepeat)));
}

/* static */
uint32_t KeymapWrapper::GetCharCodeFor(const GdkEventKey* aGdkKeyEvent) {
  // Anything above 0xf000 is considered a non-printable
  // Exception: directly encoded UCS characters
  if (aGdkKeyEvent->keyval > 0xf000 &&
      (aGdkKeyEvent->keyval & 0xff000000) != 0x01000000) {
    // Keypad keys are an exception: they return a value different
    // from their non-keypad equivalents, but mozilla doesn't distinguish.
    switch (aGdkKeyEvent->keyval) {
      case GDK_KP_Space:
        return ' ';
      case GDK_KP_Equal:
        return '=';
      case GDK_KP_Multiply:
        return '*';
      case GDK_KP_Add:
        return '+';
      case GDK_KP_Separator:
        return ',';
      case GDK_KP_Subtract:
        return '-';
      case GDK_KP_Decimal:
        return '.';
      case GDK_KP_Divide:
        return '/';
      case GDK_KP_0:
        return '0';
      case GDK_KP_1:
        return '1';
      case GDK_KP_2:
        return '2';
      case GDK_KP_3:
        return '3';
      case GDK_KP_4:
        return '4';
      case GDK_KP_5:
        return '5';
      case GDK_KP_6:
        return '6';
      case GDK_KP_7:
        return '7';
      case GDK_KP_8:
        return '8';
      case GDK_KP_9:
        return '9';
      default:
        return 0;  // non-printables
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

uint32_t KeymapWrapper::GetCharCodeFor(const GdkEventKey* aGdkKeyEvent,
                                       guint aModifierState, gint aGroup) {
  guint keyval;
  if (!gdk_keymap_translate_keyboard_state(
          mGdkKeymap, aGdkKeyEvent->hardware_keycode,
          GdkModifierType(aModifierState), aGroup, &keyval, nullptr, nullptr,
          nullptr)) {
    return 0;
  }
  GdkEventKey tmpEvent = *aGdkKeyEvent;
  tmpEvent.state = aModifierState;
  tmpEvent.keyval = keyval;
  tmpEvent.group = aGroup;
  return GetCharCodeFor(&tmpEvent);
}

uint32_t KeymapWrapper::GetUnmodifiedCharCodeFor(
    const GdkEventKey* aGdkKeyEvent) {
  guint state = aGdkKeyEvent->state &
                (GetModifierMask(SHIFT) | GetModifierMask(CAPS_LOCK) |
                 GetModifierMask(NUM_LOCK) | GetModifierMask(SCROLL_LOCK) |
                 GetModifierMask(LEVEL3) | GetModifierMask(LEVEL5));
  uint32_t charCode =
      GetCharCodeFor(aGdkKeyEvent, GdkModifierType(state), aGdkKeyEvent->group);
  if (charCode) {
    return charCode;
  }
  // If no character is mapped to the key when Level3 Shift or Level5 Shift
  // is active, let's return a character which is inputted by the key without
  // Level3 nor Level5 Shift.
  guint stateWithoutAltGraph =
      state & ~(GetModifierMask(LEVEL3) | GetModifierMask(LEVEL5));
  if (state == stateWithoutAltGraph) {
    return 0;
  }
  return GetCharCodeFor(aGdkKeyEvent, GdkModifierType(stateWithoutAltGraph),
                        aGdkKeyEvent->group);
}

gint KeymapWrapper::GetKeyLevel(GdkEventKey* aGdkKeyEvent) {
  gint level;
  if (!gdk_keymap_translate_keyboard_state(
          mGdkKeymap, aGdkKeyEvent->hardware_keycode,
          GdkModifierType(aGdkKeyEvent->state), aGdkKeyEvent->group, nullptr,
          nullptr, &level, nullptr)) {
    return -1;
  }
  return level;
}

gint KeymapWrapper::GetFirstLatinGroup() {
  GdkKeymapKey* keys;
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

bool KeymapWrapper::IsLatinGroup(guint8 aGroup) {
  GdkKeymapKey* keys;
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

bool KeymapWrapper::IsAutoRepeatableKey(guint aHardwareKeyCode) {
  uint8_t indexOfArray = aHardwareKeyCode / 8;
  MOZ_ASSERT(indexOfArray < ArrayLength(mKeyboardState.auto_repeats),
             "invalid index");
  char bitMask = 1 << (aHardwareKeyCode % 8);
  return (mKeyboardState.auto_repeats[indexOfArray] & bitMask) != 0;
}

/* static */
bool KeymapWrapper::IsBasicLatinLetterOrNumeral(uint32_t aCharCode) {
  return (aCharCode >= 'a' && aCharCode <= 'z') ||
         (aCharCode >= 'A' && aCharCode <= 'Z') ||
         (aCharCode >= '0' && aCharCode <= '9');
}

/* static */
guint KeymapWrapper::GetGDKKeyvalWithoutModifier(
    const GdkEventKey* aGdkKeyEvent) {
  KeymapWrapper* keymapWrapper = GetInstance();
  guint state =
      (aGdkKeyEvent->state & keymapWrapper->GetModifierMask(NUM_LOCK));
  guint keyval;
  if (!gdk_keymap_translate_keyboard_state(
          keymapWrapper->mGdkKeymap, aGdkKeyEvent->hardware_keycode,
          GdkModifierType(state), aGdkKeyEvent->group, &keyval, nullptr,
          nullptr, nullptr)) {
    return 0;
  }
  return keyval;
}

/* static */
uint32_t KeymapWrapper::GetDOMKeyCodeFromKeyPairs(guint aGdkKeyval) {
  switch (aGdkKeyval) {
    case GDK_Cancel:
      return NS_VK_CANCEL;
    case GDK_BackSpace:
      return NS_VK_BACK;
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      return NS_VK_TAB;
    case GDK_Clear:
      return NS_VK_CLEAR;
    case GDK_Return:
      return NS_VK_RETURN;
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Shift_Lock:
      return NS_VK_SHIFT;
    case GDK_Control_L:
    case GDK_Control_R:
      return NS_VK_CONTROL;
    case GDK_Alt_L:
    case GDK_Alt_R:
      return NS_VK_ALT;
    case GDK_Meta_L:
    case GDK_Meta_R:
      return NS_VK_META;

    // Assume that Super or Hyper is always mapped to physical Win key.
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
      return NS_VK_WIN;

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
    case GDK_Mode_switch:
      return NS_VK_ALTGR;

    case GDK_Pause:
      return NS_VK_PAUSE;
    case GDK_Caps_Lock:
      return NS_VK_CAPS_LOCK;
    case GDK_Kana_Lock:
    case GDK_Kana_Shift:
      return NS_VK_KANA;
    case GDK_Hangul:
      return NS_VK_HANGUL;
    // case GDK_XXX:                   return NS_VK_JUNJA;
    // case GDK_XXX:                   return NS_VK_FINAL;
    case GDK_Hangul_Hanja:
      return NS_VK_HANJA;
    case GDK_Kanji:
      return NS_VK_KANJI;
    case GDK_Escape:
      return NS_VK_ESCAPE;
    case GDK_Henkan:
      return NS_VK_CONVERT;
    case GDK_Muhenkan:
      return NS_VK_NONCONVERT;
    // case GDK_XXX:                   return NS_VK_ACCEPT;
    // case GDK_XXX:                   return NS_VK_MODECHANGE;
    case GDK_Page_Up:
      return NS_VK_PAGE_UP;
    case GDK_Page_Down:
      return NS_VK_PAGE_DOWN;
    case GDK_End:
      return NS_VK_END;
    case GDK_Home:
      return NS_VK_HOME;
    case GDK_Left:
      return NS_VK_LEFT;
    case GDK_Up:
      return NS_VK_UP;
    case GDK_Right:
      return NS_VK_RIGHT;
    case GDK_Down:
      return NS_VK_DOWN;
    case GDK_Select:
      return NS_VK_SELECT;
    case GDK_Print:
      return NS_VK_PRINT;
    case GDK_Execute:
      return NS_VK_EXECUTE;
    case GDK_Insert:
      return NS_VK_INSERT;
    case GDK_Delete:
      return NS_VK_DELETE;
    case GDK_Help:
      return NS_VK_HELP;

    // keypad keys
    case GDK_KP_Left:
      return NS_VK_LEFT;
    case GDK_KP_Right:
      return NS_VK_RIGHT;
    case GDK_KP_Up:
      return NS_VK_UP;
    case GDK_KP_Down:
      return NS_VK_DOWN;
    case GDK_KP_Page_Up:
      return NS_VK_PAGE_UP;
    // Not sure what these are
    // case GDK_KP_Prior:              return NS_VK_;
    // case GDK_KP_Next:               return NS_VK_;
    case GDK_KP_Begin:
      return NS_VK_CLEAR;  // Num-unlocked 5
    case GDK_KP_Page_Down:
      return NS_VK_PAGE_DOWN;
    case GDK_KP_Home:
      return NS_VK_HOME;
    case GDK_KP_End:
      return NS_VK_END;
    case GDK_KP_Insert:
      return NS_VK_INSERT;
    case GDK_KP_Delete:
      return NS_VK_DELETE;
    case GDK_KP_Enter:
      return NS_VK_RETURN;

    case GDK_Num_Lock:
      return NS_VK_NUM_LOCK;
    case GDK_Scroll_Lock:
      return NS_VK_SCROLL_LOCK;

    // Function keys
    case GDK_F1:
      return NS_VK_F1;
    case GDK_F2:
      return NS_VK_F2;
    case GDK_F3:
      return NS_VK_F3;
    case GDK_F4:
      return NS_VK_F4;
    case GDK_F5:
      return NS_VK_F5;
    case GDK_F6:
      return NS_VK_F6;
    case GDK_F7:
      return NS_VK_F7;
    case GDK_F8:
      return NS_VK_F8;
    case GDK_F9:
      return NS_VK_F9;
    case GDK_F10:
      return NS_VK_F10;
    case GDK_F11:
      return NS_VK_F11;
    case GDK_F12:
      return NS_VK_F12;
    case GDK_F13:
      return NS_VK_F13;
    case GDK_F14:
      return NS_VK_F14;
    case GDK_F15:
      return NS_VK_F15;
    case GDK_F16:
      return NS_VK_F16;
    case GDK_F17:
      return NS_VK_F17;
    case GDK_F18:
      return NS_VK_F18;
    case GDK_F19:
      return NS_VK_F19;
    case GDK_F20:
      return NS_VK_F20;
    case GDK_F21:
      return NS_VK_F21;
    case GDK_F22:
      return NS_VK_F22;
    case GDK_F23:
      return NS_VK_F23;
    case GDK_F24:
      return NS_VK_F24;

    // context menu key, keysym 0xff67, typically keycode 117 on 105-key
    // (Microsoft) x86 keyboards, located between right 'Windows' key and
    // right Ctrl key
    case GDK_Menu:
      return NS_VK_CONTEXT_MENU;
    case GDK_Sleep:
      return NS_VK_SLEEP;

    case GDK_3270_Attn:
      return NS_VK_ATTN;
    case GDK_3270_CursorSelect:
      return NS_VK_CRSEL;
    case GDK_3270_ExSelect:
      return NS_VK_EXSEL;
    case GDK_3270_EraseEOF:
      return NS_VK_EREOF;
    case GDK_3270_Play:
      return NS_VK_PLAY;
    // case GDK_XXX:                   return NS_VK_ZOOM;
    case GDK_3270_PA1:
      return NS_VK_PA1;

    // map Sun Keyboard special keysyms on to NS_VK keys

    // Sun F11 key generates SunF36(0x1005ff10) keysym
    case 0x1005ff10:
      return NS_VK_F11;
    // Sun F12 key generates SunF37(0x1005ff11) keysym
    case 0x1005ff11:
      return NS_VK_F12;
    default:
      return 0;
  }
}

void KeymapWrapper::WillDispatchKeyboardEvent(WidgetKeyboardEvent& aKeyEvent,
                                              GdkEventKey* aGdkKeyEvent) {
  GetInstance()->WillDispatchKeyboardEventInternal(aKeyEvent, aGdkKeyEvent);
}

void KeymapWrapper::WillDispatchKeyboardEventInternal(
    WidgetKeyboardEvent& aKeyEvent, GdkEventKey* aGdkKeyEvent) {
  if (!aGdkKeyEvent) {
    // If aGdkKeyEvent is nullptr, we're trying to dispatch a fake keyboard
    // event in such case, we don't need to set alternative char codes.
    // So, we don't need to do nothing here.  This case is typically we're
    // dispatching eKeyDown or eKeyUp event during composition.
    return;
  }

  uint32_t charCode = GetCharCodeFor(aGdkKeyEvent);
  if (!charCode) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p WillDispatchKeyboardEventInternal, "
             "mKeyCode=0x%02X, charCode=0x%08X",
             this, aKeyEvent.mKeyCode, aKeyEvent.mCharCode));
    return;
  }

  // The mCharCode was set from mKeyValue. However, for example, when Ctrl key
  // is pressed, its value should indicate an ASCII character for backward
  // compatibility rather than inputting character without the modifiers.
  // Therefore, we need to modify mCharCode value here.
  aKeyEvent.SetCharCode(charCode);

  gint level = GetKeyLevel(aGdkKeyEvent);
  if (level != 0 && level != 1) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p WillDispatchKeyboardEventInternal, "
             "mKeyCode=0x%02X, mCharCode=0x%08X, level=%d",
             this, aKeyEvent.mKeyCode, aKeyEvent.mCharCode, level));
    return;
  }

  guint baseState =
      aGdkKeyEvent->state & ~(GetModifierMask(SHIFT) | GetModifierMask(CTRL) |
                              GetModifierMask(ALT) | GetModifierMask(META) |
                              GetModifierMask(SUPER) | GetModifierMask(HYPER));

  // We shold send both shifted char and unshifted char, all keyboard layout
  // users can use all keys.  Don't change event.mCharCode. On some keyboard
  // layouts, Ctrl/Alt/Meta keys are used for inputting some characters.
  AlternativeCharCode altCharCodes(0, 0);
  // unshifted charcode of current keyboard layout.
  altCharCodes.mUnshiftedCharCode =
      GetCharCodeFor(aGdkKeyEvent, baseState, aGdkKeyEvent->group);
  bool isLatin = (altCharCodes.mUnshiftedCharCode <= 0xFF);
  // shifted charcode of current keyboard layout.
  altCharCodes.mShiftedCharCode = GetCharCodeFor(
      aGdkKeyEvent, baseState | GetModifierMask(SHIFT), aGdkKeyEvent->group);
  isLatin = isLatin && (altCharCodes.mShiftedCharCode <= 0xFF);
  if (altCharCodes.mUnshiftedCharCode || altCharCodes.mShiftedCharCode) {
    aKeyEvent.mAlternativeCharCodes.AppendElement(altCharCodes);
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
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p WillDispatchKeyboardEventInternal, "
             "mKeyCode=0x%02X, mCharCode=0x%08X, level=%d, altCharCodes={ "
             "mUnshiftedCharCode=0x%08X, mShiftedCharCode=0x%08X }",
             this, aKeyEvent.mKeyCode, aKeyEvent.mCharCode, level,
             altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode));
    return;
  }

  // Next, find Latin inputtable keyboard layout.
  gint minGroup = GetFirstLatinGroup();
  if (minGroup < 0) {
    MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
            ("%p WillDispatchKeyboardEventInternal, "
             "Latin keyboard layout isn't found: "
             "mKeyCode=0x%02X, mCharCode=0x%08X, level=%d, "
             "altCharCodes={ mUnshiftedCharCode=0x%08X, "
             "mShiftedCharCode=0x%08X }",
             this, aKeyEvent.mKeyCode, aKeyEvent.mCharCode, level,
             altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode));
    return;
  }

  AlternativeCharCode altLatinCharCodes(0, 0);
  uint32_t unmodifiedCh = aKeyEvent.IsShift() ? altCharCodes.mShiftedCharCode
                                              : altCharCodes.mUnshiftedCharCode;

  // unshifted charcode of found keyboard layout.
  uint32_t ch = GetCharCodeFor(aGdkKeyEvent, baseState, minGroup);
  altLatinCharCodes.mUnshiftedCharCode =
      IsBasicLatinLetterOrNumeral(ch) ? ch : 0;
  // shifted charcode of found keyboard layout.
  ch = GetCharCodeFor(aGdkKeyEvent, baseState | GetModifierMask(SHIFT),
                      minGroup);
  altLatinCharCodes.mShiftedCharCode = IsBasicLatinLetterOrNumeral(ch) ? ch : 0;
  if (altLatinCharCodes.mUnshiftedCharCode ||
      altLatinCharCodes.mShiftedCharCode) {
    aKeyEvent.mAlternativeCharCodes.AppendElement(altLatinCharCodes);
  }
  // If the mCharCode is not Latin, and the level is 0 or 1, we should
  // replace the mCharCode to Latin char if Alt and Meta keys are not
  // pressed. (Alt should be sent the localized char for accesskey
  // like handling of Web Applications.)
  ch = aKeyEvent.IsShift() ? altLatinCharCodes.mShiftedCharCode
                           : altLatinCharCodes.mUnshiftedCharCode;
  if (ch && !(aKeyEvent.IsAlt() || aKeyEvent.IsMeta()) &&
      charCode == unmodifiedCh) {
    aKeyEvent.SetCharCode(ch);
  }

  MOZ_LOG(gKeymapWrapperLog, LogLevel::Info,
          ("%p WillDispatchKeyboardEventInternal, "
           "mKeyCode=0x%02X, mCharCode=0x%08X, level=%d, minGroup=%d, "
           "altCharCodes={ mUnshiftedCharCode=0x%08X, "
           "mShiftedCharCode=0x%08X } "
           "altLatinCharCodes={ mUnshiftedCharCode=0x%08X, "
           "mShiftedCharCode=0x%08X }",
           this, aKeyEvent.mKeyCode, aKeyEvent.mCharCode, level, minGroup,
           altCharCodes.mUnshiftedCharCode, altCharCodes.mShiftedCharCode,
           altLatinCharCodes.mUnshiftedCharCode,
           altLatinCharCodes.mShiftedCharCode));
}

}  // namespace widget
}  // namespace mozilla
