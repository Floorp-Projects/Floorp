/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WindowsVersion.h"

#include "nsAlgorithm.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsGkAtoms.h"
#include "nsIDOMKeyEvent.h"
#include "nsIIdleServiceInternal.h"
#include "nsMemory.h"
#include "nsPrintfCString.h"
#include "nsQuickSort.h"
#include "nsServiceManagerUtils.h"
#include "nsToolkit.h"
#include "nsUnicharUtils.h"
#include "nsWindowDbg.h"

#include "KeyboardLayout.h"
#include "WidgetUtils.h"
#include "WinUtils.h"

#include "npapi.h"

#include <windows.h>
#include <winuser.h>
#include <algorithm>

#ifndef WINABLEAPI
#include <winable.h>
#endif

// In WinUser.h, MAPVK_VK_TO_VSC_EX is defined only when WINVER >= 0x0600
#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX (4)
#endif

namespace mozilla {
namespace widget {

static const char* kVirtualKeyName[] = {
  "NULL", "VK_LBUTTON", "VK_RBUTTON", "VK_CANCEL",
  "VK_MBUTTON", "VK_XBUTTON1", "VK_XBUTTON2", "0x07",
  "VK_BACK", "VK_TAB", "0x0A", "0x0B",
  "VK_CLEAR", "VK_RETURN", "0x0E", "0x0F",

  "VK_SHIFT", "VK_CONTROL", "VK_MENU", "VK_PAUSE",
  "VK_CAPITAL", "VK_KANA, VK_HANGUL", "0x16", "VK_JUNJA",
  "VK_FINAL", "VK_HANJA, VK_KANJI", "0x1A", "VK_ESCAPE",
  "VK_CONVERT", "VK_NONCONVERT", "VK_ACCEPT", "VK_MODECHANGE",

  "VK_SPACE", "VK_PRIOR", "VK_NEXT", "VK_END",
  "VK_HOME", "VK_LEFT", "VK_UP", "VK_RIGHT",
  "VK_DOWN", "VK_SELECT", "VK_PRINT", "VK_EXECUTE",
  "VK_SNAPSHOT", "VK_INSERT", "VK_DELETE", "VK_HELP",

  "VK_0", "VK_1", "VK_2", "VK_3",
  "VK_4", "VK_5", "VK_6", "VK_7",
  "VK_8", "VK_9", "0x3A", "0x3B",
  "0x3C", "0x3D", "0x3E", "0x3F",

  "0x40", "VK_A", "VK_B", "VK_C",
  "VK_D", "VK_E", "VK_F", "VK_G",
  "VK_H", "VK_I", "VK_J", "VK_K",
  "VK_L", "VK_M", "VK_N", "VK_O",

  "VK_P", "VK_Q", "VK_R", "VK_S",
  "VK_T", "VK_U", "VK_V", "VK_W",
  "VK_X", "VK_Y", "VK_Z", "VK_LWIN",
  "VK_RWIN", "VK_APPS", "0x5E", "VK_SLEEP",

  "VK_NUMPAD0", "VK_NUMPAD1", "VK_NUMPAD2", "VK_NUMPAD3",
  "VK_NUMPAD4", "VK_NUMPAD5", "VK_NUMPAD6", "VK_NUMPAD7",
  "VK_NUMPAD8", "VK_NUMPAD9", "VK_MULTIPLY", "VK_ADD",
  "VK_SEPARATOR", "VK_SUBTRACT", "VK_DECIMAL", "VK_DIVIDE",

  "VK_F1", "VK_F2", "VK_F3", "VK_F4",
  "VK_F5", "VK_F6", "VK_F7", "VK_F8",
  "VK_F9", "VK_F10", "VK_F11", "VK_F12",
  "VK_F13", "VK_F14", "VK_F15", "VK_F16",

  "VK_F17", "VK_F18", "VK_F19", "VK_F20",
  "VK_F21", "VK_F22", "VK_F23", "VK_F24",
  "0x88", "0x89", "0x8A", "0x8B",
  "0x8C", "0x8D", "0x8E", "0x8F",

  "VK_NUMLOCK", "VK_SCROLL", "VK_OEM_NEC_EQUAL, VK_OEM_FJ_JISHO",
    "VK_OEM_FJ_MASSHOU",
  "VK_OEM_FJ_TOUROKU", "VK_OEM_FJ_LOYA", "VK_OEM_FJ_ROYA", "0x97",
  "0x98", "0x99", "0x9A", "0x9B",
  "0x9C", "0x9D", "0x9E", "0x9F",

  "VK_LSHIFT", "VK_RSHIFT", "VK_LCONTROL", "VK_RCONTROL",
  "VK_LMENU", "VK_RMENU", "VK_BROWSER_BACK", "VK_BROWSER_FORWARD",
  "VK_BROWSER_REFRESH", "VK_BROWSER_STOP", "VK_BROWSER_SEARCH",
    "VK_BROWSER_FAVORITES",
  "VK_BROWSER_HOME", "VK_VOLUME_MUTE", "VK_VOLUME_DOWN", "VK_VOLUME_UP",

  "VK_MEDIA_NEXT_TRACK", "VK_MEDIA_PREV_TRACK", "VK_MEDIA_STOP",
    "VK_MEDIA_PLAY_PAUSE",
  "VK_LAUNCH_MAIL", "VK_LAUNCH_MEDIA_SELECT", "VK_LAUNCH_APP1",
    "VK_LAUNCH_APP2",
  "0xB8", "0xB9", "VK_OEM_1", "VK_OEM_PLUS",
  "VK_OEM_COMMA", "VK_OEM_MINUS", "VK_OEM_PERIOD", "VK_OEM_2",

  "VK_OEM_3", "VK_ABNT_C1", "VK_ABNT_C2", "0xC3",
  "0xC4", "0xC5", "0xC6", "0xC7",
  "0xC8", "0xC9", "0xCA", "0xCB",
  "0xCC", "0xCD", "0xCE", "0xCF",

  "0xD0", "0xD1", "0xD2", "0xD3",
  "0xD4", "0xD5", "0xD6", "0xD7",
  "0xD8", "0xD9", "0xDA", "VK_OEM_4",
  "VK_OEM_5", "VK_OEM_6", "VK_OEM_7", "VK_OEM_8",

  "0xE0", "VK_OEM_AX", "VK_OEM_102", "VK_ICO_HELP",
  "VK_ICO_00", "VK_PROCESSKEY", "VK_ICO_CLEAR", "VK_PACKET",
  "0xE8", "VK_OEM_RESET", "VK_OEM_JUMP", "VK_OEM_PA1",
  "VK_OEM_PA2", "VK_OEM_PA3", "VK_OEM_WSCTRL", "VK_OEM_CUSEL",

  "VK_OEM_ATTN", "VK_OEM_FINISH", "VK_OEM_COPY", "VK_OEM_AUTO",
  "VK_OEM_ENLW", "VK_OEM_BACKTAB", "VK_ATTN", "VK_CRSEL",
  "VK_EXSEL", "VK_EREOF", "VK_PLAY", "VK_ZOOM",
  "VK_NONAME", "VK_PA1", "VK_OEM_CLEAR", "0xFF"
};

static_assert(sizeof(kVirtualKeyName) / sizeof(const char*) == 0x100,
  "The virtual key name must be defined just 256 keys");

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static const nsCString
GetCharacterCodeName(WPARAM aCharCode)
{
  switch (aCharCode) {
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
      if (aCharCode < ' ' ||
          (aCharCode >= 0x80 && aCharCode < 0xA0)) {
        return nsPrintfCString("control (0x%04X)", aCharCode);
      }
      if (NS_IS_HIGH_SURROGATE(aCharCode)) {
        return nsPrintfCString("high surrogate (0x%04X)", aCharCode);
      }
      if (NS_IS_LOW_SURROGATE(aCharCode)) {
        return nsPrintfCString("low surrogate (0x%04X)", aCharCode);
      }
      return IS_IN_BMP(aCharCode) ?
        nsPrintfCString("'%s' (0x%04X)",
          NS_ConvertUTF16toUTF8(nsAutoString(aCharCode)).get(), aCharCode) :
        nsPrintfCString("'%s' (0x%08X)",
          NS_ConvertUTF16toUTF8(nsAutoString(aCharCode)).get(), aCharCode);
    }
  }
}

static const nsCString
GetKeyLocationName(uint32_t aLocation)
{
  switch (aLocation) {
    case nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT:
      return NS_LITERAL_CSTRING("KEY_LOCATION_LEFT");
    case nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT:
      return NS_LITERAL_CSTRING("KEY_LOCATION_RIGHT");
    case nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD:
      return NS_LITERAL_CSTRING("KEY_LOCATION_STANDARD");
    case nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD:
      return NS_LITERAL_CSTRING("KEY_LOCATION_NUMPAD");
    default:
      return nsPrintfCString("Unknown (0x%04X)", aLocation);
  }
}

static const nsCString
GetCharacterCodeName(char16_t* aChars, uint32_t aLength)
{
  if (!aLength) {
    return NS_LITERAL_CSTRING("");
  }
  nsAutoCString result;
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

class MOZ_STACK_CLASS GetShiftStateName final : public nsAutoCString
{
public:
  explicit GetShiftStateName(VirtualKey::ShiftState aShiftState)
  {
    if (!aShiftState) {
      AssignLiteral("none");
      return;
    }
    if (aShiftState & VirtualKey::STATE_SHIFT) {
      AssignLiteral("Shift");
      aShiftState &= ~VirtualKey::STATE_SHIFT;
    }
    if (aShiftState & VirtualKey::STATE_CONTROL) {
      MaybeAppendSeparator();
      AssignLiteral("Ctrl");
      aShiftState &= ~VirtualKey::STATE_CONTROL;
    }
    if (aShiftState & VirtualKey::STATE_ALT) {
      MaybeAppendSeparator();
      AssignLiteral("Alt");
      aShiftState &= ~VirtualKey::STATE_ALT;
    }
    if (aShiftState & VirtualKey::STATE_CAPSLOCK) {
      MaybeAppendSeparator();
      AssignLiteral("CapsLock");
      aShiftState &= ~VirtualKey::STATE_CAPSLOCK;
    }
    MOZ_ASSERT(!aShiftState);
  }

private:
  void MaybeAppendSeparator()
  {
    if (!IsEmpty()) {
      AppendLiteral(" | ");
    }
  }
};

static const nsCString
GetMessageName(UINT aMessage)
{
  switch (aMessage) {
    case WM_NULL:
      return NS_LITERAL_CSTRING("WM_NULL");
    case WM_KEYDOWN:
      return NS_LITERAL_CSTRING("WM_KEYDOWN");
    case WM_KEYUP:
      return NS_LITERAL_CSTRING("WM_KEYUP");
    case WM_SYSKEYDOWN:
      return NS_LITERAL_CSTRING("WM_SYSKEYDOWN");
    case WM_SYSKEYUP:
      return NS_LITERAL_CSTRING("WM_SYSKEYUP");
    case WM_CHAR:
      return NS_LITERAL_CSTRING("WM_CHAR");
    case WM_UNICHAR:
      return NS_LITERAL_CSTRING("WM_UNICHAR");
    case WM_SYSCHAR:
      return NS_LITERAL_CSTRING("WM_SYSCHAR");
    case WM_DEADCHAR:
      return NS_LITERAL_CSTRING("WM_DEADCHAR");
    case WM_SYSDEADCHAR:
      return NS_LITERAL_CSTRING("WM_SYSDEADCHAR");
    case MOZ_WM_KEYDOWN:
      return NS_LITERAL_CSTRING("MOZ_WM_KEYDOWN");
    case MOZ_WM_KEYUP:
      return NS_LITERAL_CSTRING("MOZ_WM_KEYUP");
    case WM_APPCOMMAND:
      return NS_LITERAL_CSTRING("WM_APPCOMMAND");
    case WM_QUIT:
      return NS_LITERAL_CSTRING("WM_QUIT");
    default:
      return nsPrintfCString("Unknown Message (0x%04X)", aMessage);
  }
}

static const nsCString
GetVirtualKeyCodeName(WPARAM aVK)
{
  if (aVK >= ArrayLength(kVirtualKeyName)) {
    return nsPrintfCString("Invalid (0x%08X)", aVK);
  }
  return nsCString(kVirtualKeyName[aVK]);
}

static const nsCString
GetAppCommandName(WPARAM aCommand)
{
  switch (aCommand) {
    case APPCOMMAND_BASS_BOOST:
      return NS_LITERAL_CSTRING("APPCOMMAND_BASS_BOOST");
    case APPCOMMAND_BASS_DOWN:
      return NS_LITERAL_CSTRING("APPCOMMAND_BASS_DOWN");
    case APPCOMMAND_BASS_UP:
      return NS_LITERAL_CSTRING("APPCOMMAND_BASS_UP");
    case APPCOMMAND_BROWSER_BACKWARD:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_BACKWARD");
    case APPCOMMAND_BROWSER_FAVORITES:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_FAVORITES");
    case APPCOMMAND_BROWSER_FORWARD:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_FORWARD");
    case APPCOMMAND_BROWSER_HOME:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_HOME");
    case APPCOMMAND_BROWSER_REFRESH:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_REFRESH");
    case APPCOMMAND_BROWSER_SEARCH:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_SEARCH");
    case APPCOMMAND_BROWSER_STOP:
      return NS_LITERAL_CSTRING("APPCOMMAND_BROWSER_STOP");
    case APPCOMMAND_CLOSE:
      return NS_LITERAL_CSTRING("APPCOMMAND_CLOSE");
    case APPCOMMAND_COPY:
      return NS_LITERAL_CSTRING("APPCOMMAND_COPY");
    case APPCOMMAND_CORRECTION_LIST:
      return NS_LITERAL_CSTRING("APPCOMMAND_CORRECTION_LIST");
    case APPCOMMAND_CUT:
      return NS_LITERAL_CSTRING("APPCOMMAND_CUT");
    case APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE:
      return NS_LITERAL_CSTRING("APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE");
    case APPCOMMAND_FIND:
      return NS_LITERAL_CSTRING("APPCOMMAND_FIND");
    case APPCOMMAND_FORWARD_MAIL:
      return NS_LITERAL_CSTRING("APPCOMMAND_FORWARD_MAIL");
    case APPCOMMAND_HELP:
      return NS_LITERAL_CSTRING("APPCOMMAND_HELP");
    case APPCOMMAND_LAUNCH_APP1:
      return NS_LITERAL_CSTRING("APPCOMMAND_LAUNCH_APP1");
    case APPCOMMAND_LAUNCH_APP2:
      return NS_LITERAL_CSTRING("APPCOMMAND_LAUNCH_APP2");
    case APPCOMMAND_LAUNCH_MAIL:
      return NS_LITERAL_CSTRING("APPCOMMAND_LAUNCH_MAIL");
    case APPCOMMAND_LAUNCH_MEDIA_SELECT:
      return NS_LITERAL_CSTRING("APPCOMMAND_LAUNCH_MEDIA_SELECT");
    case APPCOMMAND_MEDIA_CHANNEL_DOWN:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_CHANNEL_DOWN");
    case APPCOMMAND_MEDIA_CHANNEL_UP:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_CHANNEL_UP");
    case APPCOMMAND_MEDIA_FAST_FORWARD:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_FAST_FORWARD");
    case APPCOMMAND_MEDIA_NEXTTRACK:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_NEXTTRACK");
    case APPCOMMAND_MEDIA_PAUSE:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_PAUSE");
    case APPCOMMAND_MEDIA_PLAY:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_PLAY");
    case APPCOMMAND_MEDIA_PLAY_PAUSE:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_PLAY_PAUSE");
    case APPCOMMAND_MEDIA_PREVIOUSTRACK:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_PREVIOUSTRACK");
    case APPCOMMAND_MEDIA_RECORD:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_RECORD");
    case APPCOMMAND_MEDIA_REWIND:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_REWIND");
    case APPCOMMAND_MEDIA_STOP:
      return NS_LITERAL_CSTRING("APPCOMMAND_MEDIA_STOP");
    case APPCOMMAND_MIC_ON_OFF_TOGGLE:
      return NS_LITERAL_CSTRING("APPCOMMAND_MIC_ON_OFF_TOGGLE");
    case APPCOMMAND_MICROPHONE_VOLUME_DOWN:
      return NS_LITERAL_CSTRING("APPCOMMAND_MICROPHONE_VOLUME_DOWN");
    case APPCOMMAND_MICROPHONE_VOLUME_MUTE:
      return NS_LITERAL_CSTRING("APPCOMMAND_MICROPHONE_VOLUME_MUTE");
    case APPCOMMAND_MICROPHONE_VOLUME_UP:
      return NS_LITERAL_CSTRING("APPCOMMAND_MICROPHONE_VOLUME_UP");
    case APPCOMMAND_NEW:
      return NS_LITERAL_CSTRING("APPCOMMAND_NEW");
    case APPCOMMAND_OPEN:
      return NS_LITERAL_CSTRING("APPCOMMAND_OPEN");
    case APPCOMMAND_PASTE:
      return NS_LITERAL_CSTRING("APPCOMMAND_PASTE");
    case APPCOMMAND_PRINT:
      return NS_LITERAL_CSTRING("APPCOMMAND_PRINT");
    case APPCOMMAND_REDO:
      return NS_LITERAL_CSTRING("APPCOMMAND_REDO");
    case APPCOMMAND_REPLY_TO_MAIL:
      return NS_LITERAL_CSTRING("APPCOMMAND_REPLY_TO_MAIL");
    case APPCOMMAND_SAVE:
      return NS_LITERAL_CSTRING("APPCOMMAND_SAVE");
    case APPCOMMAND_SEND_MAIL:
      return NS_LITERAL_CSTRING("APPCOMMAND_SEND_MAIL");
    case APPCOMMAND_SPELL_CHECK:
      return NS_LITERAL_CSTRING("APPCOMMAND_SPELL_CHECK");
    case APPCOMMAND_TREBLE_DOWN:
      return NS_LITERAL_CSTRING("APPCOMMAND_TREBLE_DOWN");
    case APPCOMMAND_TREBLE_UP:
      return NS_LITERAL_CSTRING("APPCOMMAND_TREBLE_UP");
    case APPCOMMAND_UNDO:
      return NS_LITERAL_CSTRING("APPCOMMAND_UNDO");
    case APPCOMMAND_VOLUME_DOWN:
      return NS_LITERAL_CSTRING("APPCOMMAND_VOLUME_DOWN");
    case APPCOMMAND_VOLUME_MUTE:
      return NS_LITERAL_CSTRING("APPCOMMAND_VOLUME_MUTE");
    case APPCOMMAND_VOLUME_UP:
      return NS_LITERAL_CSTRING("APPCOMMAND_VOLUME_UP");
    default:
      return nsPrintfCString("Unknown app command (0x%08X)", aCommand);
  }
}

static const nsCString
GetAppCommandDeviceName(LPARAM aDevice)
{
  switch (aDevice) {
    case FAPPCOMMAND_KEY:
      return NS_LITERAL_CSTRING("FAPPCOMMAND_KEY");
    case FAPPCOMMAND_MOUSE:
      return NS_LITERAL_CSTRING("FAPPCOMMAND_MOUSE");
    case FAPPCOMMAND_OEM:
      return NS_LITERAL_CSTRING("FAPPCOMMAND_OEM");
    default:
      return nsPrintfCString("Unknown app command device (0x%04X)", aDevice);
  }
};

class MOZ_STACK_CLASS GetAppCommandKeysName final : public nsAutoCString
{
public:
  GetAppCommandKeysName(WPARAM aKeys)
  {
    if (aKeys & MK_CONTROL) {
      AppendLiteral("MK_CONTROL");
      aKeys &= ~MK_CONTROL;
    }
    if (aKeys & MK_LBUTTON) {
      MaybeAppendSeparator();
      AppendLiteral("MK_LBUTTON");
      aKeys &= ~MK_LBUTTON;
    }
    if (aKeys & MK_MBUTTON) {
      MaybeAppendSeparator();
      AppendLiteral("MK_MBUTTON");
      aKeys &= ~MK_MBUTTON;
    }
    if (aKeys & MK_RBUTTON) {
      MaybeAppendSeparator();
      AppendLiteral("MK_RBUTTON");
      aKeys &= ~MK_RBUTTON;
    }
    if (aKeys & MK_SHIFT) {
      MaybeAppendSeparator();
      AppendLiteral("MK_SHIFT");
      aKeys &= ~MK_SHIFT;
    }
    if (aKeys & MK_XBUTTON1) {
      MaybeAppendSeparator();
      AppendLiteral("MK_XBUTTON1");
      aKeys &= ~MK_XBUTTON1;
    }
    if (aKeys & MK_XBUTTON2) {
      MaybeAppendSeparator();
      AppendLiteral("MK_XBUTTON2");
      aKeys &= ~MK_XBUTTON2;
    }
    if (aKeys) {
      MaybeAppendSeparator();
      AppendPrintf("Unknown Flags (0x%04X)", aKeys);
    }
    if (IsEmpty()) {
      AssignLiteral("none (0x0000)");
    }
  }

private:
  void MaybeAppendSeparator()
  {
    if (!IsEmpty()) {
      AppendLiteral(" | ");
    }
  }
};

static const nsCString
ToString(const MSG& aMSG)
{
  nsAutoCString result;
  result.AssignLiteral("{ message=");
  result.Append(GetMessageName(aMSG.message).get());
  result.AppendLiteral(", ");
  switch (aMSG.message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case MOZ_WM_KEYDOWN:
    case MOZ_WM_KEYUP:
      result.AppendPrintf(
               "virtual keycode=%s, repeat count=%d, "
               "scancode=0x%02X, extended key=%s, "
               "context code=%s, previous key state=%s, "
               "transition state=%s",
               GetVirtualKeyCodeName(aMSG.wParam).get(),
               aMSG.lParam & 0xFFFF,
               WinUtils::GetScanCode(aMSG.lParam),
               GetBoolName(WinUtils::IsExtendedScanCode(aMSG.lParam)),
               GetBoolName((aMSG.lParam & (1 << 29)) != 0),
               GetBoolName((aMSG.lParam & (1 << 30)) != 0),
               GetBoolName((aMSG.lParam & (1 << 31)) != 0));
      break;
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
      result.AppendPrintf(
               "character code=%s, repeat count=%d, "
               "scancode=0x%02X, extended key=%s, "
               "context code=%s, previous key state=%s, "
               "transition state=%s",
               GetCharacterCodeName(aMSG.wParam).get(),
               aMSG.lParam & 0xFFFF,
               WinUtils::GetScanCode(aMSG.lParam),
               GetBoolName(WinUtils::IsExtendedScanCode(aMSG.lParam)),
               GetBoolName((aMSG.lParam & (1 << 29)) != 0),
               GetBoolName((aMSG.lParam & (1 << 30)) != 0),
               GetBoolName((aMSG.lParam & (1 << 31)) != 0));
      break;
    case WM_APPCOMMAND:
      result.AppendPrintf(
               "window handle=0x%p, app command=%s, device=%s, dwKeys=%s",
               aMSG.wParam,
               GetAppCommandName(GET_APPCOMMAND_LPARAM(aMSG.lParam)).get(),
               GetAppCommandDeviceName(GET_DEVICE_LPARAM(aMSG.lParam)).get(),
               GetAppCommandKeysName(GET_KEYSTATE_LPARAM(aMSG.lParam)).get());
      break;
    default:
      result.AppendPrintf("wParam=%u, lParam=%u", aMSG.wParam, aMSG.lParam);
      break;
  }
  result.AppendPrintf(", hwnd=0x%p", aMSG.hwnd);
  return result;
}

static const nsCString
ToString(const UniCharsAndModifiers& aUniCharsAndModifiers)
{
  if (aUniCharsAndModifiers.IsEmpty()) {
    return NS_LITERAL_CSTRING("{}");
  }
  nsAutoCString result;
  result.AssignLiteral("{ ");
  result.Append(GetCharacterCodeName(aUniCharsAndModifiers.mChars[0]));
  for (uint32_t i = 1; i < aUniCharsAndModifiers.mLength; ++i) {
    if (aUniCharsAndModifiers.mModifiers[i - 1] !=
          aUniCharsAndModifiers.mModifiers[i]) {
      result.AppendLiteral(" [");
      result.Append(GetModifiersName(aUniCharsAndModifiers.mModifiers[0]));
      result.AppendLiteral("]");
    }
    result.AppendLiteral(", ");
    result.Append(GetCharacterCodeName(aUniCharsAndModifiers.mChars[i]));
  }
  result.AppendLiteral(" [");
  uint32_t lastIndex = aUniCharsAndModifiers.mLength - 1;
  result.Append(GetModifiersName(aUniCharsAndModifiers.mModifiers[lastIndex]));
  result.AppendLiteral("] }");
  return result;
}

const nsCString
ToString(const ModifierKeyState& aModifierKeyState)
{
  nsAutoCString result;
  result.AssignLiteral("{ ");
  result.Append(GetModifiersName(aModifierKeyState.GetModifiers()).get());
  result.AppendLiteral(" }");
  return result;
}

// Unique id counter associated with a keydown / keypress events. Used in
// identifing keypress events for removal from async event dispatch queue
// in metrofx after preventDefault is called on keydown events.
static uint32_t sUniqueKeyEventId = 0;

struct DeadKeyEntry
{
  char16_t BaseChar;
  char16_t CompositeChar;
};


class DeadKeyTable
{
  friend class KeyboardLayout;

  uint16_t mEntries;
  // KeyboardLayout::AddDeadKeyTable() will allocate as many entries as
  // required.  It is the only way to create new DeadKeyTable instances.
  DeadKeyEntry mTable[1];

  void Init(const DeadKeyEntry* aDeadKeyArray, uint32_t aEntries)
  {
    mEntries = aEntries;
    memcpy(mTable, aDeadKeyArray, aEntries * sizeof(DeadKeyEntry));
  }

  static uint32_t SizeInBytes(uint32_t aEntries)
  {
    return offsetof(DeadKeyTable, mTable) + aEntries * sizeof(DeadKeyEntry);
  }

public:
  uint32_t Entries() const
  {
    return mEntries;
  }

  bool IsEqual(const DeadKeyEntry* aDeadKeyArray, uint32_t aEntries) const
  {
    return (mEntries == aEntries &&
            !memcmp(mTable, aDeadKeyArray,
                    aEntries * sizeof(DeadKeyEntry)));
  }

  char16_t GetCompositeChar(char16_t aBaseChar) const;
};


/*****************************************************************************
 * mozilla::widget::ModifierKeyState
 *****************************************************************************/

ModifierKeyState::ModifierKeyState()
{
  Update();
}

ModifierKeyState::ModifierKeyState(bool aIsShiftDown,
                                   bool aIsControlDown,
                                   bool aIsAltDown)
{
  Update();
  Unset(MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_ALTGRAPH);
  Modifiers modifiers = 0;
  if (aIsShiftDown) {
    modifiers |= MODIFIER_SHIFT;
  }
  if (aIsControlDown) {
    modifiers |= MODIFIER_CONTROL;
  }
  if (aIsAltDown) {
    modifiers |= MODIFIER_ALT;
  }
  if (modifiers) {
    Set(modifiers);
  }
}

ModifierKeyState::ModifierKeyState(Modifiers aModifiers) :
  mModifiers(aModifiers)
{
  EnsureAltGr();
}

void
ModifierKeyState::Update()
{
  mModifiers = 0;
  if (IS_VK_DOWN(VK_SHIFT)) {
    mModifiers |= MODIFIER_SHIFT;
  }
  if (IS_VK_DOWN(VK_CONTROL)) {
    mModifiers |= MODIFIER_CONTROL;
  }
  if (IS_VK_DOWN(VK_MENU)) {
    mModifiers |= MODIFIER_ALT;
  }
  if (IS_VK_DOWN(VK_LWIN) || IS_VK_DOWN(VK_RWIN)) {
    mModifiers |= MODIFIER_OS;
  }
  if (::GetKeyState(VK_CAPITAL) & 1) {
    mModifiers |= MODIFIER_CAPSLOCK;
  }
  if (::GetKeyState(VK_NUMLOCK) & 1) {
    mModifiers |= MODIFIER_NUMLOCK;
  }
  if (::GetKeyState(VK_SCROLL) & 1) {
    mModifiers |= MODIFIER_SCROLLLOCK;
  }

  EnsureAltGr();
}

void
ModifierKeyState::Unset(Modifiers aRemovingModifiers)
{
  mModifiers &= ~aRemovingModifiers;
  // Note that we don't need to unset AltGr flag here automatically.
  // For EditorBase, we need to remove Alt and Control flags but AltGr isn't
  // checked in EditorBase, so, it can be kept.
}

void
ModifierKeyState::Set(Modifiers aAddingModifiers)
{
  mModifiers |= aAddingModifiers;
  EnsureAltGr();
}

void
ModifierKeyState::InitInputEvent(WidgetInputEvent& aInputEvent) const
{
  aInputEvent.mModifiers = mModifiers;

  switch(aInputEvent.mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case eSimpleGestureEventClass:
      InitMouseEvent(aInputEvent);
      break;
    default:
      break;
  }
}

void
ModifierKeyState::InitMouseEvent(WidgetInputEvent& aMouseEvent) const
{
  NS_ASSERTION(aMouseEvent.mClass == eMouseEventClass ||
               aMouseEvent.mClass == eWheelEventClass ||
               aMouseEvent.mClass == eDragEventClass ||
               aMouseEvent.mClass == eSimpleGestureEventClass,
               "called with non-mouse event");

  WidgetMouseEventBase& mouseEvent = *aMouseEvent.AsMouseEventBase();
  mouseEvent.buttons = 0;
  if (::GetKeyState(VK_LBUTTON) < 0) {
    mouseEvent.buttons |= WidgetMouseEvent::eLeftButtonFlag;
  }
  if (::GetKeyState(VK_RBUTTON) < 0) {
    mouseEvent.buttons |= WidgetMouseEvent::eRightButtonFlag;
  }
  if (::GetKeyState(VK_MBUTTON) < 0) {
    mouseEvent.buttons |= WidgetMouseEvent::eMiddleButtonFlag;
  }
  if (::GetKeyState(VK_XBUTTON1) < 0) {
    mouseEvent.buttons |= WidgetMouseEvent::e4thButtonFlag;
  }
  if (::GetKeyState(VK_XBUTTON2) < 0) {
    mouseEvent.buttons |= WidgetMouseEvent::e5thButtonFlag;
  }
}

bool
ModifierKeyState::IsShift() const
{
  return (mModifiers & MODIFIER_SHIFT) != 0;
}

bool
ModifierKeyState::IsControl() const
{
  return (mModifiers & MODIFIER_CONTROL) != 0;
}

bool
ModifierKeyState::IsAlt() const
{
  return (mModifiers & MODIFIER_ALT) != 0;
}

bool
ModifierKeyState::IsAltGr() const
{
  return IsControl() && IsAlt();
}

bool
ModifierKeyState::IsWin() const
{
  return (mModifiers & MODIFIER_OS) != 0;
}

bool
ModifierKeyState::MaybeMatchShortcutKey() const
{
  // If Windows key is pressed, even if both Ctrl key and Alt key are pressed,
  // it's possible to match a shortcut key.
  if (IsWin()) {
    return true;
  }
  // Otherwise, when both Ctrl key and Alt key are pressed, it shouldn't be
  // a shortcut key for Windows since it means pressing AltGr key on
  // some keyboard layouts.
  if (IsControl() ^ IsAlt()) {
    return true;
  }
  // If no modifier key is active except a lockable modifier nor Shift key,
  // the key shouldn't match any shortcut keys (there are Space and
  // Shift+Space, though, let's ignore these special case...).
  return false;
}

bool
ModifierKeyState::IsCapsLocked() const
{
  return (mModifiers & MODIFIER_CAPSLOCK) != 0;
}

bool
ModifierKeyState::IsNumLocked() const
{
  return (mModifiers & MODIFIER_NUMLOCK) != 0;
}

bool
ModifierKeyState::IsScrollLocked() const
{
  return (mModifiers & MODIFIER_SCROLLLOCK) != 0;
}

void
ModifierKeyState::EnsureAltGr()
{
  // If both Control key and Alt key are pressed, it means AltGr is pressed.
  // Ideally, we should check whether the current keyboard layout has AltGr
  // or not.  However, setting AltGr flags for keyboard which doesn't have
  // AltGr must not be serious bug.  So, it should be OK for now.
  if (IsAltGr()) {
    mModifiers |= MODIFIER_ALTGRAPH;
  }
}

/*****************************************************************************
 * mozilla::widget::UniCharsAndModifiers
 *****************************************************************************/

void
UniCharsAndModifiers::Append(char16_t aUniChar, Modifiers aModifiers)
{
  MOZ_ASSERT(mLength < 5);
  mChars[mLength] = aUniChar;
  mModifiers[mLength] = aModifiers;
  mLength++;
}

void
UniCharsAndModifiers::FillModifiers(Modifiers aModifiers)
{
  for (uint32_t i = 0; i < mLength; i++) {
    mModifiers[i] = aModifiers;
  }
}

bool
UniCharsAndModifiers::UniCharsEqual(const UniCharsAndModifiers& aOther) const
{
  if (mLength != aOther.mLength) {
    return false;
  }
  return !memcmp(mChars, aOther.mChars, mLength * sizeof(char16_t));
}

bool
UniCharsAndModifiers::UniCharsCaseInsensitiveEqual(
                        const UniCharsAndModifiers& aOther) const
{
  if (mLength != aOther.mLength) {
    return false;
  }

  nsCaseInsensitiveStringComparator comp;
  return !comp(mChars, aOther.mChars, mLength, aOther.mLength);
}

UniCharsAndModifiers&
UniCharsAndModifiers::operator+=(const UniCharsAndModifiers& aOther)
{
  uint32_t copyCount = std::min(aOther.mLength, 5 - mLength);
  NS_ENSURE_TRUE(copyCount > 0, *this);
  memcpy(&mChars[mLength], aOther.mChars, copyCount * sizeof(char16_t));
  memcpy(&mModifiers[mLength], aOther.mModifiers,
         copyCount * sizeof(Modifiers));
  mLength += copyCount;
  return *this;
}

UniCharsAndModifiers
UniCharsAndModifiers::operator+(const UniCharsAndModifiers& aOther) const
{
  UniCharsAndModifiers result(*this);
  result += aOther;
  return result;
}

/*****************************************************************************
 * mozilla::widget::VirtualKey
 *****************************************************************************/

// static
VirtualKey::ShiftState
VirtualKey::ModifiersToShiftState(Modifiers aModifiers)
{
  ShiftState state = 0;
  if (aModifiers & MODIFIER_SHIFT) {
    state |= STATE_SHIFT;
  }
  if (aModifiers & MODIFIER_CONTROL) {
    state |= STATE_CONTROL;
  }
  if (aModifiers & MODIFIER_ALT) {
    state |= STATE_ALT;
  }
  if (aModifiers & MODIFIER_CAPSLOCK) {
    state |= STATE_CAPSLOCK;
  }
  return state;
}

// static
Modifiers
VirtualKey::ShiftStateToModifiers(ShiftState aShiftState)
{
  Modifiers modifiers = 0;
  if (aShiftState & STATE_SHIFT) {
    modifiers |= MODIFIER_SHIFT;
  }
  if (aShiftState & STATE_CONTROL) {
    modifiers |= MODIFIER_CONTROL;
  }
  if (aShiftState & STATE_ALT) {
    modifiers |= MODIFIER_ALT;
  }
  if (aShiftState & STATE_CAPSLOCK) {
    modifiers |= MODIFIER_CAPSLOCK;
  }
  if ((modifiers & (MODIFIER_ALT | MODIFIER_CONTROL)) ==
         (MODIFIER_ALT | MODIFIER_CONTROL)) {
    modifiers |= MODIFIER_ALTGRAPH;
  }
  return modifiers;
}

inline char16_t
VirtualKey::GetCompositeChar(ShiftState aShiftState, char16_t aBaseChar) const
{
  return mShiftStates[aShiftState].DeadKey.Table->GetCompositeChar(aBaseChar);
}

const DeadKeyTable*
VirtualKey::MatchingDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                 uint32_t aEntries) const
{
  if (!mIsDeadKey) {
    return nullptr;
  }

  for (ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    if (!IsDeadKey(shiftState)) {
      continue;
    }
    const DeadKeyTable* dkt = mShiftStates[shiftState].DeadKey.Table;
    if (dkt && dkt->IsEqual(aDeadKeyArray, aEntries)) {
      return dkt;
    }
  }

  return nullptr;
}

void
VirtualKey::SetNormalChars(ShiftState aShiftState,
                           const char16_t* aChars,
                           uint32_t aNumOfChars)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, false);

  for (uint32_t index = 0; index < aNumOfChars; index++) {
    // Ignore legacy non-printable control characters
    mShiftStates[aShiftState].Normal.Chars[index] =
      (aChars[index] >= 0x20) ? aChars[index] : 0;
  }

  uint32_t len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (uint32_t index = aNumOfChars; index < len; index++) {
    mShiftStates[aShiftState].Normal.Chars[index] = 0;
  }
}

void
VirtualKey::SetDeadChar(ShiftState aShiftState, char16_t aDeadChar)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, true);

  mShiftStates[aShiftState].DeadKey.DeadChar = aDeadChar;
  mShiftStates[aShiftState].DeadKey.Table = nullptr;
}

UniCharsAndModifiers
VirtualKey::GetUniChars(ShiftState aShiftState) const
{
  UniCharsAndModifiers result = GetNativeUniChars(aShiftState);

  const ShiftState STATE_ALT_CONTROL = (STATE_ALT | STATE_CONTROL);
  if (!(aShiftState & STATE_ALT_CONTROL)) {
    return result;
  }

  if (!result.mLength) {
    result = GetNativeUniChars(aShiftState & ~STATE_ALT_CONTROL);
    result.FillModifiers(ShiftStateToModifiers(aShiftState));
    return result;
  }

  if ((aShiftState & STATE_ALT_CONTROL) == STATE_ALT_CONTROL) {
    // Even if the shifted chars and the unshifted chars are same, we
    // should consume the Alt key state and the Ctrl key state when
    // AltGr key is pressed. Because if we don't consume them, the input
    // events are ignored on EditorBase. (I.e., Users cannot input the
    // characters with this key combination.)
    Modifiers finalModifiers = ShiftStateToModifiers(aShiftState);
    finalModifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL);
    result.FillModifiers(finalModifiers);
    return result;
  }

  UniCharsAndModifiers unmodifiedReslt =
    GetNativeUniChars(aShiftState & ~STATE_ALT_CONTROL);
  if (!result.UniCharsEqual(unmodifiedReslt)) {
    // Otherwise, we should consume the Alt key state and the Ctrl key state
    // only when the shifted chars and unshifted chars are different.
    Modifiers finalModifiers = ShiftStateToModifiers(aShiftState);
    finalModifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL);
    result.FillModifiers(finalModifiers);
  }
  return result;
}


UniCharsAndModifiers
VirtualKey::GetNativeUniChars(ShiftState aShiftState) const
{
#ifdef DEBUG
  if (aShiftState >= ArrayLength(mShiftStates)) {
    nsPrintfCString warning("Shift state is out of range: "
                            "aShiftState=%d, ArrayLength(mShiftState)=%d",
                            aShiftState, ArrayLength(mShiftStates));
    NS_WARNING(warning.get());
  }
#endif

  UniCharsAndModifiers result;
  Modifiers modifiers = ShiftStateToModifiers(aShiftState);
  if (IsDeadKey(aShiftState)) {
    result.Append(mShiftStates[aShiftState].DeadKey.DeadChar, modifiers);
    return result;
  }

  uint32_t index;
  uint32_t len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (index = 0;
       index < len && mShiftStates[aShiftState].Normal.Chars[index]; index++) {
    result.Append(mShiftStates[aShiftState].Normal.Chars[index], modifiers);
  }
  return result;
}

// static
void
VirtualKey::FillKbdState(PBYTE aKbdState,
                         const ShiftState aShiftState)
{
  NS_ASSERTION(aShiftState < 16, "aShiftState out of range");

  if (aShiftState & STATE_SHIFT) {
    aKbdState[VK_SHIFT] |= 0x80;
  } else {
    aKbdState[VK_SHIFT]  &= ~0x80;
    aKbdState[VK_LSHIFT] &= ~0x80;
    aKbdState[VK_RSHIFT] &= ~0x80;
  }

  if (aShiftState & STATE_CONTROL) {
    aKbdState[VK_CONTROL] |= 0x80;
  } else {
    aKbdState[VK_CONTROL]  &= ~0x80;
    aKbdState[VK_LCONTROL] &= ~0x80;
    aKbdState[VK_RCONTROL] &= ~0x80;
  }

  if (aShiftState & STATE_ALT) {
    aKbdState[VK_MENU] |= 0x80;
  } else {
    aKbdState[VK_MENU]  &= ~0x80;
    aKbdState[VK_LMENU] &= ~0x80;
    aKbdState[VK_RMENU] &= ~0x80;
  }

  if (aShiftState & STATE_CAPSLOCK) {
    aKbdState[VK_CAPITAL] |= 0x01;
  } else {
    aKbdState[VK_CAPITAL] &= ~0x01;
  }
}

/*****************************************************************************
 * mozilla::widget::NativeKey
 *****************************************************************************/

uint8_t NativeKey::sDispatchedKeyOfAppCommand = 0;

LazyLogModule sNativeKeyLogger("NativeKeyWidgets");

NativeKey::NativeKey(nsWindowBase* aWidget,
                     const MSG& aMessage,
                     const ModifierKeyState& aModKeyState,
                     HKL aOverrideKeyboardLayout,
                     nsTArray<FakeCharMsg>* aFakeCharMsgs)
  : mWidget(aWidget)
  , mDispatcher(aWidget->GetTextEventDispatcher())
  , mMsg(aMessage)
  , mFocusedWndBeforeDispatch(::GetFocus())
  , mDOMKeyCode(0)
  , mKeyNameIndex(KEY_NAME_INDEX_Unidentified)
  , mCodeNameIndex(CODE_NAME_INDEX_UNKNOWN)
  , mModKeyState(aModKeyState)
  , mVirtualKeyCode(0)
  , mOriginalVirtualKeyCode(0)
  , mShiftedLatinChar(0)
  , mUnshiftedLatinChar(0)
  , mScanCode(0)
  , mIsExtended(false)
  , mIsDeadKey(false)
  , mFakeCharMsgs(aFakeCharMsgs && aFakeCharMsgs->Length() ?
                    aFakeCharMsgs : nullptr)
{
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p NativeKey::NativeKey(aWidget=0x%p { GetWindowHandle()=0x%p }, "
     "aMessage=%s, aModKeyState=%s)",
     this, aWidget, aWidget->GetWindowHandle(), ToString(aMessage).get(),
     ToString(aModKeyState).get()));

  MOZ_ASSERT(aWidget);
  MOZ_ASSERT(mDispatcher);
  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  mKeyboardLayout = keyboardLayout->GetLayout();
  if (aOverrideKeyboardLayout && mKeyboardLayout != aOverrideKeyboardLayout) {
    keyboardLayout->OverrideLayout(aOverrideKeyboardLayout);
    mKeyboardLayout = keyboardLayout->GetLayout();
    MOZ_ASSERT(mKeyboardLayout == aOverrideKeyboardLayout);
    mIsOverridingKeyboardLayout = true;
  } else {
    mIsOverridingKeyboardLayout = false;
  }

  if (mMsg.message == WM_APPCOMMAND) {
    InitWithAppCommand();
  } else {
    InitWithKeyChar();
  }

  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::NativeKey(), mKeyboardLayout=0x%08X, "
     "mFocusedWndBeforeDispatch=0x%p, mDOMKeyCode=0x%04X, "
     "mKeyNameIndex=%s, mCodeNameIndex=%s, mModKeyState=%s, "
     "mVirtualKeyCode=%s, mOriginalVirtualKeyCode=%s, "
     "mCommittedCharsAndModifiers=%s, mInputtingStringAndModifiers=%s, "
     "mShiftedString=%s, mUnshiftedString=%s, mShiftedLatinChar=%s, "
     "mUnshiftedLatinChar=%s, mScanCode=0x%04X, mIsExtended=%s, "
     "mIsDeadKey=%s, mIsPrintableKey=%s, mIsOverridingKeyboardLayout=%s",
     this, mKeyboardLayout, mFocusedWndBeforeDispatch,
     GetDOMKeyCodeName(mDOMKeyCode).get(), ToString(mKeyNameIndex).get(),
     ToString(mCodeNameIndex).get(),
     ToString(mModKeyState).get(),
     GetVirtualKeyCodeName(mVirtualKeyCode).get(),
     GetVirtualKeyCodeName(mOriginalVirtualKeyCode).get(),
     ToString(mCommittedCharsAndModifiers).get(),
     ToString(mInputtingStringAndModifiers).get(),
     ToString(mShiftedString).get(), ToString(mUnshiftedString).get(),
     GetCharacterCodeName(mShiftedLatinChar).get(),
     GetCharacterCodeName(mUnshiftedLatinChar).get(),
     mScanCode, GetBoolName(mIsExtended), GetBoolName(mIsDeadKey),
     GetBoolName(mIsPrintableKey), GetBoolName(mIsOverridingKeyboardLayout)));
}

void
NativeKey::InitWithKeyChar()
{
  mScanCode = WinUtils::GetScanCode(mMsg.lParam);
  mIsExtended = WinUtils::IsExtendedScanCode(mMsg.lParam);
  switch (mMsg.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      // If next message is WM_*CHAR message, let's consume it now.
      MSG charMsg;
      while (GetFollowingCharMessage(charMsg)) {
        // Although, got message shouldn't be WM_NULL in desktop apps,
        // we should keep checking this.  FYI: This was added for Metrofox.
        if (charMsg.message == WM_NULL) {
          continue;
        }
        MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
          ("%p   NativeKey::InitWithKeyChar(), removed char message, %s",
           this, ToString(charMsg).get()));
        NS_WARN_IF(charMsg.hwnd != mMsg.hwnd);
        mFollowingCharMsgs.AppendElement(charMsg);
      }
      MOZ_FALLTHROUGH;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case MOZ_WM_KEYDOWN:
    case MOZ_WM_KEYUP: {
      // First, resolve the IME converted virtual keycode to its original
      // keycode.
      if (mMsg.wParam == VK_PROCESSKEY) {
        mOriginalVirtualKeyCode =
          static_cast<uint8_t>(::ImmGetVirtualKey(mMsg.hwnd));
      } else {
        mOriginalVirtualKeyCode = static_cast<uint8_t>(mMsg.wParam);
      }

      // If the key message is sent from other application like a11y tools, the
      // scancode value might not be set proper value.  Then, probably the value
      // is 0.
      // NOTE: If the virtual keycode can be caused by both non-extended key
      //       and extended key, the API returns the non-extended key's
      //       scancode.  E.g., VK_LEFT causes "4" key on numpad.
      if (!mScanCode && mOriginalVirtualKeyCode != VK_PACKET) {
        uint16_t scanCodeEx = ComputeScanCodeExFromVirtualKeyCode(mMsg.wParam);
        if (scanCodeEx) {
          mScanCode = static_cast<uint8_t>(scanCodeEx & 0xFF);
          uint8_t extended = static_cast<uint8_t>((scanCodeEx & 0xFF00) >> 8);
          mIsExtended = (extended == 0xE0) || (extended == 0xE1);
        }
      }

      // Most keys are not distinguished as left or right keys.
      bool isLeftRightDistinguishedKey = false;

      // mOriginalVirtualKeyCode must not distinguish left or right of
      // Shift, Control or Alt.
      switch (mOriginalVirtualKeyCode) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LSHIFT:
        case VK_RSHIFT:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_SHIFT;
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LCONTROL:
        case VK_RCONTROL:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_CONTROL;
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LMENU:
        case VK_RMENU:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_MENU;
          isLeftRightDistinguishedKey = true;
          break;
      }

      // If virtual keycode (left-right distinguished keycode) is already
      // computed, we don't need to do anymore.
      if (mVirtualKeyCode) {
        break;
      }

      // If the keycode doesn't have LR distinguished keycode, we just set
      // mOriginalVirtualKeyCode to mVirtualKeyCode.  Note that don't compute
      // it from MapVirtualKeyEx() because the scan code might be wrong if
      // the message is sent/posted by other application.  Then, we will compute
      // unexpected keycode from the scan code.
      if (!isLeftRightDistinguishedKey) {
        break;
      }

      if (!CanComputeVirtualKeyCodeFromScanCode()) {
        // The right control key and the right alt key are extended keys.
        // Therefore, we never get VK_RCONTRL and VK_RMENU for the result of
        // MapVirtualKeyEx() on WinXP or WinServer2003.
        //
        // If VK_SHIFT, VK_CONTROL or VK_MENU key message is caused by well
        // known scan code, we should decide it as Right key.  Otherwise,
        // decide it as Left key.
        switch (mOriginalVirtualKeyCode) {
          case VK_CONTROL:
            mVirtualKeyCode =
              mIsExtended && mScanCode == 0x1D ? VK_RCONTROL : VK_LCONTROL;
            break;
          case VK_MENU:
            mVirtualKeyCode =
              mIsExtended && mScanCode == 0x38 ? VK_RMENU : VK_LMENU;
            break;
          case VK_SHIFT:
            // Neither left shift nor right shift is an extended key,
            // let's use VK_LSHIFT for unknown mapping.
            mVirtualKeyCode = VK_LSHIFT;
            break;
          default:
            MOZ_CRASH("Unsupported mOriginalVirtualKeyCode");
        }
        break;
      }

      NS_ASSERTION(!mVirtualKeyCode,
                   "mVirtualKeyCode has been computed already");

      // Otherwise, compute the virtual keycode with MapVirtualKeyEx().
      mVirtualKeyCode = ComputeVirtualKeyCodeFromScanCodeEx();

      // Following code shouldn't be used now because we compute scancode value
      // if we detect that the sender doesn't set proper scancode.
      // However, the detection might fail. Therefore, let's keep using this.
      switch (mOriginalVirtualKeyCode) {
        case VK_CONTROL:
          if (mVirtualKeyCode != VK_LCONTROL &&
              mVirtualKeyCode != VK_RCONTROL) {
            mVirtualKeyCode = mIsExtended ? VK_RCONTROL : VK_LCONTROL;
          }
          break;
        case VK_MENU:
          if (mVirtualKeyCode != VK_LMENU && mVirtualKeyCode != VK_RMENU) {
            mVirtualKeyCode = mIsExtended ? VK_RMENU : VK_LMENU;
          }
          break;
        case VK_SHIFT:
          if (mVirtualKeyCode != VK_LSHIFT && mVirtualKeyCode != VK_RSHIFT) {
            // Neither left shift nor right shift is an extended key,
            // let's use VK_LSHIFT for unknown mapping.
            mVirtualKeyCode = VK_LSHIFT;
          }
          break;
        default:
          MOZ_CRASH("Unsupported mOriginalVirtualKeyCode");
      }
      break;
    }
    case WM_CHAR:
    case WM_UNICHAR:
    case WM_SYSCHAR:
      // NOTE: If other applications like a11y tools sends WM_*CHAR without
      //       scancode, we cannot compute virtual keycode.  I.e., with such
      //       applications, we cannot generate proper KeyboardEvent.code value.

      // We cannot compute the virtual key code from WM_CHAR message on WinXP
      // if it's caused by an extended key.
      if (!CanComputeVirtualKeyCodeFromScanCode()) {
        break;
      }
      mVirtualKeyCode = mOriginalVirtualKeyCode =
        ComputeVirtualKeyCodeFromScanCodeEx();
      NS_ASSERTION(mVirtualKeyCode, "Failed to compute virtual keycode");
      break;
    default:
      MOZ_CRASH("Unsupported message");
  }

  if (!mVirtualKeyCode) {
    mVirtualKeyCode = mOriginalVirtualKeyCode;
  }

  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  mDOMKeyCode =
    keyboardLayout->ConvertNativeKeyCodeToDOMKeyCode(mOriginalVirtualKeyCode);
  // Be aware, keyboard utilities can change non-printable keys to printable
  // keys.  In such case, we should make the key value as a printable key.
  // FYI: IsFollowedByNonControlCharMessage() returns true only when it's
  //      handling a keydown message.
  mKeyNameIndex = IsFollowedByNonControlCharMessage() ?
    KEY_NAME_INDEX_USE_STRING :
    keyboardLayout->ConvertNativeKeyCodeToKeyNameIndex(mOriginalVirtualKeyCode);
  mCodeNameIndex =
    KeyboardLayout::ConvertScanCodeToCodeNameIndex(
      GetScanCodeWithExtendedFlag());

  keyboardLayout->InitNativeKey(*this, mModKeyState);

  mIsDeadKey =
    (IsFollowedByDeadCharMessage() ||
     keyboardLayout->IsDeadKey(mOriginalVirtualKeyCode, mModKeyState));
  mIsPrintableKey =
    mKeyNameIndex == KEY_NAME_INDEX_USE_STRING ||
    KeyboardLayout::IsPrintableCharKey(mOriginalVirtualKeyCode);

  if (IsKeyDownMessage()) {
    // Compute some strings which may be inputted by the key with various
    // modifier state if this key event won't cause text input actually.
    // They will be used for setting mAlternativeCharCodes in the callback
    // method which will be called by TextEventDispatcher.
    if (NeedsToHandleWithoutFollowingCharMessages()) {
      ComputeInputtingStringWithKeyboardLayout();
    } else {
      // This message might be sent by SendInput() API to input a Unicode
      // character, in such case, we can only know what chars will be inputted
      // with following WM_CHAR messages.
      // TODO: We cannot initialize mCommittedCharsAndModifiers for VK_PACKET
      //       if the message is WM_KEYUP because we don't have preceding
      //       WM_CHAR message.  Therefore, we should dispatch eKeyUp event at
      //       handling WM_KEYDOWN.
      // TODO: Like Edge, we shouldn't dispatch two sets of keyboard events
      //       for a Unicode character in non-BMP because its key value looks
      //       broken and not good thing for our editor if only one keydown or
      //       keypress event's default is prevented.  I guess, we should store
      //       key message information globally and we should wait following
      //       WM_KEYDOWN if following WM_CHAR is a part of a Unicode character.
      mCommittedCharsAndModifiers.Clear();
      for (size_t i = 0; i < mFollowingCharMsgs.Length(); ++i) {
        char16_t ch = static_cast<char16_t>(mFollowingCharMsgs[i].wParam);
        // Skip control characters.
        if (IsControlChar(ch)) {
          continue;
        }
        mCommittedCharsAndModifiers.Append(ch, mModKeyState.GetModifiers());
      }
    }
    // Remove odd char messages if there are.
    RemoveFollowingOddCharMessages();
  }
}

NativeKey::~NativeKey()
{
  MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
    ("%p   NativeKey::~NativeKey(), destroyed", this));
  if (mIsOverridingKeyboardLayout) {
    KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
    keyboardLayout->RestoreLayout();
  }
}

void
NativeKey::InitWithAppCommand()
{
  if (GET_DEVICE_LPARAM(mMsg.lParam) != FAPPCOMMAND_KEY) {
    return;
  }

  uint32_t appCommand = GET_APPCOMMAND_LPARAM(mMsg.lParam);
  switch (GET_APPCOMMAND_LPARAM(mMsg.lParam)) {

#undef NS_APPCOMMAND_TO_DOM_KEY_NAME_INDEX
#define NS_APPCOMMAND_TO_DOM_KEY_NAME_INDEX(aAppCommand, aKeyNameIndex) \
    case aAppCommand: \
      mKeyNameIndex = aKeyNameIndex; \
      break;

#include "NativeKeyToDOMKeyName.h"

#undef NS_APPCOMMAND_TO_DOM_KEY_NAME_INDEX

    default:
      mKeyNameIndex = KEY_NAME_INDEX_Unidentified;
  }

  // Guess the virtual keycode which caused this message.
  switch (appCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_BACK;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_FORWARD;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_REFRESH;
      break;
    case APPCOMMAND_BROWSER_STOP:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_STOP;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_SEARCH;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_FAVORITES;
      break;
    case APPCOMMAND_BROWSER_HOME:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_BROWSER_HOME;
      break;
    case APPCOMMAND_VOLUME_MUTE:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_VOLUME_MUTE;
      break;
    case APPCOMMAND_VOLUME_DOWN:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_VOLUME_DOWN;
      break;
    case APPCOMMAND_VOLUME_UP:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_VOLUME_UP;
      break;
    case APPCOMMAND_MEDIA_NEXTTRACK:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_MEDIA_NEXT_TRACK;
      break;
    case APPCOMMAND_MEDIA_PREVIOUSTRACK:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_MEDIA_PREV_TRACK;
      break;
    case APPCOMMAND_MEDIA_STOP:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_MEDIA_STOP;
      break;
    case APPCOMMAND_MEDIA_PLAY_PAUSE:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_MEDIA_PLAY_PAUSE;
      break;
    case APPCOMMAND_LAUNCH_MAIL:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_LAUNCH_MAIL;
      break;
    case APPCOMMAND_LAUNCH_MEDIA_SELECT:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_LAUNCH_MEDIA_SELECT;
      break;
    case APPCOMMAND_LAUNCH_APP1:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_LAUNCH_APP1;
      break;
    case APPCOMMAND_LAUNCH_APP2:
      mVirtualKeyCode = mOriginalVirtualKeyCode = VK_LAUNCH_APP2;
      break;
    default:
      return;
  }

  uint16_t scanCodeEx = ComputeScanCodeExFromVirtualKeyCode(mVirtualKeyCode);
  mScanCode = static_cast<uint8_t>(scanCodeEx & 0xFF);
  uint8_t extended = static_cast<uint8_t>((scanCodeEx & 0xFF00) >> 8);
  mIsExtended = (extended == 0xE0) || (extended == 0xE1);
  mDOMKeyCode =
    KeyboardLayout::GetInstance()->
      ConvertNativeKeyCodeToDOMKeyCode(mOriginalVirtualKeyCode);
  mCodeNameIndex =
    KeyboardLayout::ConvertScanCodeToCodeNameIndex(
      GetScanCodeWithExtendedFlag());
}

// static
bool
NativeKey::IsControlChar(char16_t aChar)
{
  static const char16_t U_SPACE = 0x20;
  static const char16_t U_DELETE = 0x7F;
  return aChar < U_SPACE || aChar == U_DELETE;
}

bool
NativeKey::IsFollowedByDeadCharMessage() const
{
  if (mFollowingCharMsgs.IsEmpty()) {
    return false;
  }
  return IsDeadCharMessage(mFollowingCharMsgs[0]);
}

bool
NativeKey::IsFollowedByNonControlCharMessage() const
{
  if (mFollowingCharMsgs.IsEmpty()) {
    return false;
  }
  return mFollowingCharMsgs[0].message == WM_CHAR &&
         !IsControlChar(static_cast<char16_t>(mFollowingCharMsgs[0].wParam));
}

bool
NativeKey::IsIMEDoingKakuteiUndo() const
{
  // Following message pattern is caused by "Kakutei-Undo" of ATOK or WXG:
  // ---------------------------------------------------------------------------
  // WM_KEYDOWN              * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC0000001) # ATOK
  // WM_IME_STARTCOMPOSITION * 1 (wParam = 0x0, lParam = 0x0)
  // WM_IME_COMPOSITION      * 1 (wParam = 0x0, lParam = 0x1BF)
  // WM_CHAR                 * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC00E0001)
  // ---------------------------------------------------------------------------
  // This doesn't match usual key message pattern such as:
  //   WM_KEYDOWN -> WM_CHAR -> WM_KEYDOWN -> WM_CHAR -> ... -> WM_KEYUP
  // See following bugs for the detail.
  // https://bugzilla.mozilla.gr.jp/show_bug.cgi?id=2885 (written in Japanese)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=194559 (written in English)
  MSG startCompositionMsg, compositionMsg, charMsg;
  return WinUtils::PeekMessage(&startCompositionMsg, mMsg.hwnd,
                               WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION,
                               PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&compositionMsg, mMsg.hwnd, WM_IME_COMPOSITION,
                               WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&charMsg, mMsg.hwnd, WM_CHAR, WM_CHAR,
                               PM_NOREMOVE | PM_NOYIELD) &&
         startCompositionMsg.wParam == 0x0 &&
         startCompositionMsg.lParam == 0x0 &&
         compositionMsg.wParam == 0x0 &&
         compositionMsg.lParam == 0x1BF &&
         charMsg.wParam == VK_BACK && charMsg.lParam == 0x1 &&
         startCompositionMsg.time <= compositionMsg.time &&
         compositionMsg.time <= charMsg.time;
}

void
NativeKey::RemoveFollowingOddCharMessages()
{
  MOZ_ASSERT(IsKeyDownMessage());

  // If the keydown message is synthesized for automated tests, there is
  // nothing to do here.
  if (mFakeCharMsgs) {
    return;
  }

  // If there are some following char messages before another key message,
  // there is nothing to do here.
  if (!mFollowingCharMsgs.IsEmpty()) {
    return;
  }

  // If the handling key isn't Backspace, there is nothing to do here.
  if (mOriginalVirtualKeyCode != VK_BACK) {
    return;
  }

  // If we don't see the odd message pattern, there is nothing to do here.
  if (!IsIMEDoingKakuteiUndo()) {
    return;
  }

  // Otherwise, we need to remove odd WM_CHAR messages for ATOK or WXG (both
  // of them are Japanese IME).
  MSG msg;
  while (WinUtils::PeekMessage(&msg, mMsg.hwnd, WM_CHAR, WM_CHAR,
                               PM_REMOVE | PM_NOYIELD)) {
    if (msg.message != WM_CHAR) {
      MOZ_RELEASE_ASSERT(msg.message == WM_NULL,
                         "Unexpected message was removed");
      continue;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::RemoveFollowingOddCharMessages(), removed odd char "
       "message, %s",
       this, ToString(msg).get()));
    mRemovedOddCharMsgs.AppendElement(msg);
  }
}

UINT
NativeKey::GetScanCodeWithExtendedFlag() const
{
  if (!mIsExtended) {
    return mScanCode;
  }
  return (0xE000 | mScanCode);
}

uint32_t
NativeKey::GetKeyLocation() const
{
  switch (mVirtualKeyCode) {
    case VK_LSHIFT:
    case VK_LCONTROL:
    case VK_LMENU:
    case VK_LWIN:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;

    case VK_RSHIFT:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_RWIN:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;

    case VK_RETURN:
      // XXX This code assumes that all keyboard drivers use same mapping.
      return !mIsExtended ? nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD :
                            nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    case VK_INSERT:
    case VK_DELETE:
    case VK_END:
    case VK_DOWN:
    case VK_NEXT:
    case VK_LEFT:
    case VK_CLEAR:
    case VK_RIGHT:
    case VK_HOME:
    case VK_UP:
    case VK_PRIOR:
      // XXX This code assumes that all keyboard drivers use same mapping.
      return mIsExtended ? nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD :
                           nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    // NumLock key isn't included due to IE9's behavior.
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_DECIMAL:
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    // Separator key of Brazilian keyboard or JIS keyboard for Mac
    case VK_ABNT_C2:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
      NS_WARNING("Failed to decide the key location?");

    default:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
  }
}

bool
NativeKey::CanComputeVirtualKeyCodeFromScanCode() const
{
  // Vista or later supports ScanCodeEx.
  if (IsVistaOrLater()) {
    return true;
  }
  // Otherwise, MapVirtualKeyEx() can compute virtual keycode only with
  // non-extended key.
  return !mIsExtended;
}

uint8_t
NativeKey::ComputeVirtualKeyCodeFromScanCode() const
{
  return static_cast<uint8_t>(
           ::MapVirtualKeyEx(mScanCode, MAPVK_VSC_TO_VK, mKeyboardLayout));
}

uint8_t
NativeKey::ComputeVirtualKeyCodeFromScanCodeEx() const
{
  // MapVirtualKeyEx() has been improved for supporting extended keys since
  // Vista.  When we call it for mapping a scancode of an extended key and
  // a virtual keycode, we need to add 0xE000 to the scancode.
  // On the other hand, neither WinXP nor WinServer2003 doesn't support 0xE000.
  // Therefore, we have no way to get virtual keycode from scan code of
  // extended keys.
  if (NS_WARN_IF(!CanComputeVirtualKeyCodeFromScanCode())) {
    return 0;
  }
  return static_cast<uint8_t>(
           ::MapVirtualKeyEx(GetScanCodeWithExtendedFlag(), MAPVK_VSC_TO_VK_EX,
                             mKeyboardLayout));
}

uint16_t
NativeKey::ComputeScanCodeExFromVirtualKeyCode(UINT aVirtualKeyCode) const
{
  return static_cast<uint16_t>(
           ::MapVirtualKeyEx(aVirtualKeyCode,
                             IsVistaOrLater() ? MAPVK_VK_TO_VSC_EX :
                                                MAPVK_VK_TO_VSC,
                             mKeyboardLayout));
}

char16_t
NativeKey::ComputeUnicharFromScanCode() const
{
  return static_cast<char16_t>(
           ::MapVirtualKeyEx(ComputeVirtualKeyCodeFromScanCode(),
                             MAPVK_VK_TO_CHAR, mKeyboardLayout));
}

nsEventStatus
NativeKey::InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                        const MSG* aMsgSentToPlugin) const
{
  return InitKeyEvent(aKeyEvent, mModKeyState, aMsgSentToPlugin);
}

nsEventStatus
NativeKey::InitKeyEvent(WidgetKeyboardEvent& aKeyEvent,
                        const ModifierKeyState& aModKeyState,
                        const MSG* aMsgSentToPlugin) const
{
  if (mWidget->Destroyed()) {
    MOZ_CRASH("NativeKey tries to dispatch a key event on destroyed widget");
  }

  LayoutDeviceIntPoint point(0, 0);
  mWidget->InitEvent(aKeyEvent, &point);

  switch (aKeyEvent.mMessage) {
    case eKeyDown:
    case eKeyDownOnPlugin:
      aKeyEvent.mKeyCode = mDOMKeyCode;
      // Unique id for this keydown event and its associated keypress.
      sUniqueKeyEventId++;
      aKeyEvent.mUniqueId = sUniqueKeyEventId;
      break;
    case eKeyUp:
    case eKeyUpOnPlugin:
      aKeyEvent.mKeyCode = mDOMKeyCode;
      // Set defaultPrevented of the key event if the VK_MENU is not a system
      // key release, so that the menu bar does not trigger.  This helps avoid
      // triggering the menu bar for ALT key accelerators used in assistive
      // technologies such as Window-Eyes and ZoomText or for switching open
      // state of IME.
      if (mOriginalVirtualKeyCode == VK_MENU && mMsg.message != WM_SYSKEYUP) {
        aKeyEvent.PreventDefaultBeforeDispatch();
      }
      break;
    case eKeyPress:
      aKeyEvent.mUniqueId = sUniqueKeyEventId;
      break;
    default:
      MOZ_CRASH("Invalid event message");
  }

  aKeyEvent.mIsRepeat = IsRepeat();
  aKeyEvent.mKeyNameIndex = mKeyNameIndex;
  if (mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    aKeyEvent.mKeyValue = mCommittedCharsAndModifiers.ToString();
  }
  aKeyEvent.mCodeNameIndex = mCodeNameIndex;
  MOZ_ASSERT(mCodeNameIndex != CODE_NAME_INDEX_USE_STRING);
  aKeyEvent.mLocation = GetKeyLocation();
  aModKeyState.InitInputEvent(aKeyEvent);

  NPEvent pluginEvent;
  if (aMsgSentToPlugin &&
      mWidget->GetInputContext().mIMEState.mEnabled == IMEState::PLUGIN) {
    pluginEvent.event = aMsgSentToPlugin->message;
    pluginEvent.wParam = aMsgSentToPlugin->wParam;
    pluginEvent.lParam = aMsgSentToPlugin->lParam;
    aKeyEvent.mPluginEvent.Copy(pluginEvent);
  }

  KeyboardLayout::NotifyIdleServiceOfUserActivity();

  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::InitKeyEvent(), initialized, aKeyEvent={ "
     "mMessage=%s, mKeyNameIndex=%s, mKeyValue=\"%s\", mCodeNameIndex=%s, "
     "mKeyCode=%s, mLocation=%s, mModifiers=%s, DefaultPrevented()=%s }",
     this, ToChar(aKeyEvent.mMessage),
     ToString(aKeyEvent.mKeyNameIndex).get(),
     NS_ConvertUTF16toUTF8(aKeyEvent.mKeyValue).get(),
     ToString(aKeyEvent.mCodeNameIndex).get(),
     GetDOMKeyCodeName(aKeyEvent.mKeyCode).get(),
     GetKeyLocationName(aKeyEvent.mLocation).get(),
     GetModifiersName(aKeyEvent.mModifiers).get(),
     GetBoolName(aKeyEvent.DefaultPrevented())));

  return aKeyEvent.DefaultPrevented() ? nsEventStatus_eConsumeNoDefault :
                                        nsEventStatus_eIgnore;
}

bool
NativeKey::DispatchCommandEvent(uint32_t aEventCommand) const
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsGkAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsGkAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsGkAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsGkAtoms::Stop;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      command = nsGkAtoms::Search;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      command = nsGkAtoms::Bookmarks;
      break;
    case APPCOMMAND_BROWSER_HOME:
      command = nsGkAtoms::Home;
      break;
    case APPCOMMAND_CLOSE:
      command = nsGkAtoms::Close;
      break;
    case APPCOMMAND_FIND:
      command = nsGkAtoms::Find;
      break;
    case APPCOMMAND_HELP:
      command = nsGkAtoms::Help;
      break;
    case APPCOMMAND_NEW:
      command = nsGkAtoms::New;
      break;
    case APPCOMMAND_OPEN:
      command = nsGkAtoms::Open;
      break;
    case APPCOMMAND_PRINT:
      command = nsGkAtoms::Print;
      break;
    case APPCOMMAND_SAVE:
      command = nsGkAtoms::Save;
      break;
    case APPCOMMAND_FORWARD_MAIL:
      command = nsGkAtoms::ForwardMail;
      break;
    case APPCOMMAND_REPLY_TO_MAIL:
      command = nsGkAtoms::ReplyToMail;
      break;
    case APPCOMMAND_SEND_MAIL:
      command = nsGkAtoms::SendMail;
      break;
    case APPCOMMAND_MEDIA_NEXTTRACK:
      command = nsGkAtoms::NextTrack;
      break;
    case APPCOMMAND_MEDIA_PREVIOUSTRACK:
      command = nsGkAtoms::PreviousTrack;
      break;
    case APPCOMMAND_MEDIA_STOP:
      command = nsGkAtoms::MediaStop;
      break;
    case APPCOMMAND_MEDIA_PLAY_PAUSE:
      command = nsGkAtoms::PlayPause;
      break;
    default:
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::DispatchCommandEvent(), doesn't dispatch command "
         "event", this));
      return false;
  }
  WidgetCommandEvent commandEvent(true, nsGkAtoms::onAppCommand,
                                  command, mWidget);

  mWidget->InitEvent(commandEvent);
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::DispatchCommandEvent(), dispatching %s command event...",
     this, nsAtomCString(command).get()));
  bool ok = mWidget->DispatchWindowEvent(&commandEvent) || mWidget->Destroyed();
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::DispatchCommandEvent(), dispatched command event, "
     "result=%s, mWidget->Destroyed()=%s",
     this, GetBoolName(ok), GetBoolName(mWidget->Destroyed())));
  return ok;
}

bool
NativeKey::HandleAppCommandMessage() const
{
  // NOTE: Typical behavior of WM_APPCOMMAND caused by key is, WM_APPCOMMAND
  //       message is _sent_ first.  Then, the DefaultWndProc will _post_
  //       WM_KEYDOWN message and WM_KEYUP message if the keycode for the
  //       command is available (i.e., mVirtualKeyCode is not 0).

  // NOTE: IntelliType (Microsoft's keyboard utility software) always consumes
  //       WM_KEYDOWN and WM_KEYUP.

  // Let's dispatch keydown message before our chrome handles the command
  // when the message is caused by a keypress.  This behavior makes handling
  // WM_APPCOMMAND be a default action of the keydown event.  This means that
  // web applications can handle multimedia keys and prevent our default action.
  // This allow web applications to provide better UX for multimedia keyboard
  // users.
  bool dispatchKeyEvent = (GET_DEVICE_LPARAM(mMsg.lParam) == FAPPCOMMAND_KEY);
  if (dispatchKeyEvent) {
    // If a plug-in window has focus but it didn't consume the message, our
    // window receive WM_APPCOMMAND message.  In this case, we shouldn't
    // dispatch KeyboardEvents because an event handler may access the
    // plug-in process synchronously.
    dispatchKeyEvent =
      WinUtils::IsOurProcessWindow(reinterpret_cast<HWND>(mMsg.wParam));
  }

  bool consumed = false;

  if (dispatchKeyEvent) {
    nsresult rv = mDispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::HandleAppCommandMessage(), FAILED due to "
         "BeginNativeInputTransaction() failure", this));
      return true;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), initializing keydown "
       "event...", this));
    WidgetKeyboardEvent keydownEvent(true, eKeyDown, mWidget);
    nsEventStatus status = InitKeyEvent(keydownEvent, mModKeyState, &mMsg);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), tries to dispatch "
       "keydown event...", this));
    // NOTE: If the keydown event is consumed by web contents, we shouldn't
    //       continue to handle the command.
    if (!mDispatcher->DispatchKeyboardEvent(eKeyDown, keydownEvent, status,
                                            const_cast<NativeKey*>(this))) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), keydown event isn't "
         "dispatched", this));
      // If keyboard event wasn't fired, there must be composition.
      // So, we don't need to dispatch a command event.
      return true;
    }
    consumed = status == nsEventStatus_eConsumeNoDefault;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), keydown event was "
       "dispatched, consumed=%s",
       this, GetBoolName(consumed)));
    sDispatchedKeyOfAppCommand = mVirtualKeyCode;
    if (mWidget->Destroyed()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), keydown event caused "
         "destroying the widget", this));
      return true;
    }
  }

  // Dispatch a command event or a content command event if the command is
  // supported.
  if (!consumed) {
    uint32_t appCommand = GET_APPCOMMAND_LPARAM(mMsg.lParam);
    EventMessage contentCommandMessage = eVoidEvent;
    switch (appCommand) {
      case APPCOMMAND_BROWSER_BACKWARD:
      case APPCOMMAND_BROWSER_FORWARD:
      case APPCOMMAND_BROWSER_REFRESH:
      case APPCOMMAND_BROWSER_STOP:
      case APPCOMMAND_BROWSER_SEARCH:
      case APPCOMMAND_BROWSER_FAVORITES:
      case APPCOMMAND_BROWSER_HOME:
      case APPCOMMAND_CLOSE:
      case APPCOMMAND_FIND:
      case APPCOMMAND_HELP:
      case APPCOMMAND_NEW:
      case APPCOMMAND_OPEN:
      case APPCOMMAND_PRINT:
      case APPCOMMAND_SAVE:
      case APPCOMMAND_FORWARD_MAIL:
      case APPCOMMAND_REPLY_TO_MAIL:
      case APPCOMMAND_SEND_MAIL:
      case APPCOMMAND_MEDIA_NEXTTRACK:
      case APPCOMMAND_MEDIA_PREVIOUSTRACK:
      case APPCOMMAND_MEDIA_STOP:
      case APPCOMMAND_MEDIA_PLAY_PAUSE:
        // We shouldn't consume the message always because if we don't handle
        // the message, the sender (typically, utility of keyboard or mouse)
        // may send other key messages which indicate well known shortcut key.
        consumed = DispatchCommandEvent(appCommand);
        break;

      // Use content command for following commands:
      case APPCOMMAND_COPY:
        contentCommandMessage = eContentCommandCopy;
        break;
      case APPCOMMAND_CUT:
        contentCommandMessage = eContentCommandCut;
        break;
      case APPCOMMAND_PASTE:
        contentCommandMessage = eContentCommandPaste;
        break;
      case APPCOMMAND_REDO:
        contentCommandMessage = eContentCommandRedo;
        break;
      case APPCOMMAND_UNDO:
        contentCommandMessage = eContentCommandUndo;
        break;
    }

    if (contentCommandMessage) {
      WidgetContentCommandEvent contentCommandEvent(true, contentCommandMessage,
                                                    mWidget);
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), dispatching %s event...",
         this, ToChar(contentCommandMessage)));
      mWidget->DispatchWindowEvent(&contentCommandEvent);
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), dispatched %s event",
         this, ToChar(contentCommandMessage)));
      consumed = true;

      if (mWidget->Destroyed()) {
        MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
          ("%p   NativeKey::HandleAppCommandMessage(), %s event caused "
           "destroying the widget",
           this, ToChar(contentCommandMessage)));
        return true;
      }
    } else {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), doesn't dispatch content "
         "command event", this));
    }
  }

  // Dispatch a keyup event if the command is caused by pressing a key and
  // the key isn't mapped to a virtual keycode.
  if (dispatchKeyEvent && !mVirtualKeyCode) {
    nsresult rv = mDispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::HandleAppCommandMessage(), FAILED due to "
         "BeginNativeInputTransaction() failure", this));
      return true;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), initializing keyup "
       "event...", this));
    WidgetKeyboardEvent keyupEvent(true, eKeyUp, mWidget);
    nsEventStatus status = InitKeyEvent(keyupEvent, mModKeyState, &mMsg);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), dispatching keyup event...",
       this));
    // NOTE: Ignore if the keyup event is consumed because keyup event
    //       represents just a physical key event state change.
    mDispatcher->DispatchKeyboardEvent(eKeyUp, keyupEvent, status,
                                       const_cast<NativeKey*>(this));
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleAppCommandMessage(), dispatched keyup event",
       this));
    if (mWidget->Destroyed()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleAppCommandMessage(), %s event caused "
         "destroying the widget", this));
      return true;
    }
  }

  return consumed;
}

bool
NativeKey::HandleKeyDownMessage(bool* aEventDispatched) const
{
  MOZ_ASSERT(IsKeyDownMessage());

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  if (sDispatchedKeyOfAppCommand &&
      sDispatchedKeyOfAppCommand == mOriginalVirtualKeyCode) {
    // The multimedia key event has already been dispatch from
    // HandleAppCommandMessage().
    sDispatchedKeyOfAppCommand = 0;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), doesn't dispatch keydown "
       "event due to already dispatched from HandleAppCommandMessage(), ",
       this));
    return true;
  }

  bool defaultPrevented = false;
  if (mFakeCharMsgs || IsKeyMessageOnPlugin() ||
      !RedirectedKeyDownMessageManager::IsRedirectedMessage(mMsg)) {
    // Ignore [shift+]alt+space so the OS can handle it.
    if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
        mVirtualKeyCode == VK_SPACE) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), doesn't dispatch keydown "
         "event due to Alt+Space", this));
      return false;
    }

    nsresult rv = mDispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::HandleKeyDownMessage(), FAILED due to "
         "BeginNativeInputTransaction() failure", this));
      return true;
    }

    bool isIMEEnabled = WinUtils::IsIMEEnabled(mWidget->GetInputContext());

    MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
      ("%p   NativeKey::HandleKeyDownMessage(), initializing keydown "
       "event...", this));

    EventMessage keyDownMessage =
      IsKeyMessageOnPlugin() ? eKeyDownOnPlugin : eKeyDown;
    WidgetKeyboardEvent keydownEvent(true, keyDownMessage, mWidget);
    nsEventStatus status = InitKeyEvent(keydownEvent, mModKeyState, &mMsg);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), dispatching keydown event...",
       this));
    bool dispatched =
      mDispatcher->DispatchKeyboardEvent(keyDownMessage, keydownEvent, status,
                                         const_cast<NativeKey*>(this));
    if (aEventDispatched) {
      *aEventDispatched = dispatched;
    }
    if (!dispatched) {
      // If the keydown event wasn't fired, there must be composition.
      // we don't need to do anything anymore.
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), doesn't dispatch keypress "
         "event(s) because keydown event isn't dispatched actually", this));
      return false;
    }
    defaultPrevented = status == nsEventStatus_eConsumeNoDefault;

    // We don't need to handle key messages on plugin for eKeyPress since
    // eKeyDownOnPlugin is handled as both eKeyDown and eKeyPress.
    if (IsKeyMessageOnPlugin()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), doesn't dispatch keypress "
         "event(s) because it's a keydown message on windowed plugin, "
         "defaultPrevented=%s",
         this, GetBoolName(defaultPrevented)));
      return defaultPrevented;
    }

    if (mWidget->Destroyed() || IsFocusedWindowChanged()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), keydown event caused "
         "destroying the widget", this));
      return true;
    }

    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), dispatched keydown event, "
       "dispatched=%s, defaultPrevented=%s",
       this, GetBoolName(dispatched), GetBoolName(defaultPrevented)));

    // If IMC wasn't associated to the window but is associated it now (i.e.,
    // focus is moved from a non-editable editor to an editor by keydown
    // event handler), WM_CHAR and WM_SYSCHAR shouldn't cause first character
    // inputting if IME is opened.  But then, we should redirect the native
    // keydown message to IME.
    // However, note that if focus has been already moved to another
    // application, we shouldn't redirect the message to it because the keydown
    // message is processed by us, so, nobody shouldn't process it.
    HWND focusedWnd = ::GetFocus();
    if (!defaultPrevented && !mFakeCharMsgs && !IsKeyMessageOnPlugin() &&
        focusedWnd && !mWidget->PluginHasFocus() && !isIMEEnabled &&
        WinUtils::IsIMEEnabled(mWidget->GetInputContext())) {
      RedirectedKeyDownMessageManager::RemoveNextCharMessage(focusedWnd);

      INPUT keyinput;
      keyinput.type = INPUT_KEYBOARD;
      keyinput.ki.wVk = mOriginalVirtualKeyCode;
      keyinput.ki.wScan = mScanCode;
      keyinput.ki.dwFlags = KEYEVENTF_SCANCODE;
      if (mIsExtended) {
        keyinput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      }
      keyinput.ki.time = 0;
      keyinput.ki.dwExtraInfo = 0;

      RedirectedKeyDownMessageManager::WillRedirect(mMsg, defaultPrevented);

      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), redirecting %s...",
         this, ToString(mMsg).get()));

      ::SendInput(1, &keyinput, sizeof(keyinput));

      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleKeyDownMessage(), redirected %s",
         this, ToString(mMsg).get()));

      // Return here.  We shouldn't dispatch keypress event for this WM_KEYDOWN.
      // If it's needed, it will be dispatched after next (redirected)
      // WM_KEYDOWN.
      return true;
    }
  } else {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), received a redirected %s",
       this, ToString(mMsg).get()));

    defaultPrevented = RedirectedKeyDownMessageManager::DefaultPrevented();
    // If this is redirected keydown message, we have dispatched the keydown
    // event already.
    if (aEventDispatched) {
      *aEventDispatched = true;
    }
  }

  RedirectedKeyDownMessageManager::Forget();

  // If the key was processed by IME, we shouldn't dispatch keypress event.
  if (mOriginalVirtualKeyCode == VK_PROCESSKEY) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), not dispatching keypress "
       "event because the key was already handled by IME, defaultPrevented=%s",
       this, GetBoolName(defaultPrevented)));
    return defaultPrevented;
  }

  if (defaultPrevented) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), not dispatching keypress "
       "event because preceding keydown event was consumed",
       this));
    MaybeDispatchPluginEventsForRemovedCharMessages();
    return true;
  }

  // If we won't be getting a WM_CHAR, WM_SYSCHAR or WM_DEADCHAR, synthesize a
  // keypress for almost all keys
  if (NeedsToHandleWithoutFollowingCharMessages()) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), tries to be dispatching "
       "keypress events...", this));
    return (MaybeDispatchPluginEventsForRemovedCharMessages() ||
            DispatchKeyPressEventsWithoutCharMessage());
  }

  if (!mFollowingCharMsgs.IsEmpty()) {
    bool consumed = false;
    for (size_t i = 0; i < mFollowingCharMsgs.Length(); ++i) {
        MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
          ("%p   NativeKey::HandleKeyDownMessage(), stopped dispatching "
           "keypress events for remaining char messages, consumed=%s, "
           "mFollowingCharMsgs[%u]=%s, mMsg=%s, "
           "mFocusedWndBeforeDispatch=0x%p, ::GetFocus()=0x%p",
           this, GetBoolName(consumed), i,
           ToString(mFollowingCharMsgs[i]).get(),
           ToString(mMsg).get(), mFocusedWndBeforeDispatch, ::GetFocus()));
      consumed =
        DispatchKeyPressEventForFollowingCharMessage(mFollowingCharMsgs[i]) ||
        consumed;
      if (mWidget->Destroyed() || IsFocusedWindowChanged()) {
        MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
          ("%p   NativeKey::HandleKeyDownMessage(), %s event caused "
           "destroying the widget", this));
        return true;
      }
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), handled all following char "
       "messages, consumed=%s",
       this, GetBoolName(consumed)));
    return consumed;
  }

  // If WM_KEYDOWN of VK_PACKET isn't followed by WM_CHAR, we don't need to
  // dispatch keypress events.
  if (mVirtualKeyCode == VK_PACKET) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), not dispatching keypress event "
       "because the key is VK_PACKET and there are no char messages",
       this));
    return false;
  }

  if (!mModKeyState.IsControl() && !mModKeyState.IsAlt() &&
      !mModKeyState.IsWin() && mIsPrintableKey) {
    // If this is simple KeyDown event but next message is not WM_CHAR,
    // this event may not input text, so we should ignore this event.
    // See bug 314130.
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), not dispatching keypress event "
       "because the key event is simple printable key's event but not followed "
       "by char messages", this));
    return false;
  }

  if (mIsDeadKey) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyDownMessage(), not dispatching keypress event "
       "because the key is a dead key and not followed by char messages",
       this));
    return false;
  }

  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::HandleKeyDownMessage(), tries to be dispatching "
     "keypress events due to no following char messages...", this));
  return DispatchKeyPressEventsWithoutCharMessage();
}

bool
NativeKey::HandleCharMessage(const MSG& aCharMsg,
                             bool* aEventDispatched) const
{
  MOZ_ASSERT(IsKeyDownMessage() || IsPrintableCharMessage(mMsg));
  MOZ_ASSERT(IsPrintableCharMessage(aCharMsg.message));

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  // Alt+Space key is handled by OS, we shouldn't touch it.
  if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_SPACE) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), doesn't dispatch keypress "
       "event due to Alt+Space", this));
    return false;
  }

  // Bug 818235: Ignore Ctrl+Enter.
  if (!mModKeyState.IsAlt() && mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_RETURN) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), doesn't dispatch keypress "
       "event due to Ctrl+Enter", this));
    return false;
  }

  // XXXmnakao I think that if aNativeKeyDown is null, such lonely WM_CHAR
  //           should cause composition events because they are not caused
  //           by actual keyboard operation.

  static const char16_t U_EQUAL = 0x3D;

  // First, handle normal text input or non-printable key case here.
  if ((!mModKeyState.IsAlt() && !mModKeyState.IsControl()) ||
      mModKeyState.IsAltGr() ||
      (mOriginalVirtualKeyCode &&
       !KeyboardLayout::IsPrintableCharKey(mOriginalVirtualKeyCode))) {
    WidgetKeyboardEvent keypressEvent(true, eKeyPress, mWidget);
    if (!IsControlChar(static_cast<char16_t>(aCharMsg.wParam))) {
      keypressEvent.mCharCode = static_cast<uint32_t>(aCharMsg.wParam);
    } else {
      keypressEvent.mKeyCode = mDOMKeyCode;
    }
    nsresult rv = mDispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::HandleCharMessage(), FAILED due to "
         "BeginNativeInputTransaction() failure", this));
      return true;
    }

    MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
      ("%p   NativeKey::HandleCharMessage(), initializing keypress "
       "event...", this));

    // When AltGr (Alt+Ctrl) is pressed, that causes normal text input.
    // At this time, if either alt or ctrl flag is set, EditorBase ignores the
    // keypress event.  For avoiding this issue, we should remove ctrl and alt
    // flags.
    ModifierKeyState modKeyState(mModKeyState);
    modKeyState.Unset(MODIFIER_ALT | MODIFIER_CONTROL);
    nsEventStatus status = InitKeyEvent(keypressEvent, modKeyState, &aCharMsg);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), dispatching keypress event...",
       this));
    bool dispatched =
      mDispatcher->MaybeDispatchKeypressEvents(keypressEvent, status,
                                               const_cast<NativeKey*>(this));
    if (aEventDispatched) {
      *aEventDispatched = dispatched;
    }
    if (mWidget->Destroyed()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleCharMessage(), keypress event caused "
         "destroying the widget", this));
      return true;
    }
    bool consumed = status == nsEventStatus_eConsumeNoDefault;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), dispatched keypress event, "
       "dispatched=%s, consumed=%s",
       this, GetBoolName(dispatched), GetBoolName(consumed)));
    return consumed;
  }

  // XXX It seems that following code was implemented for shortcut key
  //     handling.  However, it's now handled in WM_KEYDOWN message handler.
  //     So, this actually runs only when WM_CHAR is sent/posted without
  //     WM_KEYDOWN.  I think that we don't need to keypress event in such
  //     case especially for shortcut keys.

  char16_t uniChar;
  // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details
  if (mModKeyState.IsControl() &&
      IsControlChar(static_cast<char16_t>(aCharMsg.wParam))) {
    // Bug 16486: Need to account for shift here.
    uniChar = aCharMsg.wParam - 1 + (mModKeyState.IsShift() ? 'A' : 'a');
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), computing charCode for ASCII "
       "control characters which are inputted with Ctrl key, uniChar=%s",
       this, GetCharacterCodeName(uniChar).get()));
  } else if (mModKeyState.IsControl() && aCharMsg.wParam <= 0x1F) {
    // XXX Looks like that this block won't run since the condition is
    //     included in the first |if|'s condition.
    // Bug 50255: <ctrl><[> and <ctrl><]> are not being processed.
    // also fixes ctrl+\ (x1c), ctrl+^ (x1e) and ctrl+_ (x1f)
    // for some reason the keypress handler need to have the uniChar code set
    // with the addition of a upper case A not the lower case.
    uniChar = aCharMsg.wParam - 1 + 'A';
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), computing charCode for ASCII "
       "control characters which are inputted with Ctrl key, uniChar=%s",
       this, GetCharacterCodeName(uniChar).get()));
  } else if (IsControlChar(static_cast<char16_t>(aCharMsg.wParam)) ||
             (aCharMsg.wParam == U_EQUAL && mModKeyState.IsControl())) {
    uniChar = 0;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), setting charCode to 0 because "
       "the character is a control character without Ctrl key or Ctrl+=",
       this));
  } else {
    uniChar = aCharMsg.wParam;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), deciding to use given charCode, "
       "uniChar=%s",
       this, GetCharacterCodeName(uniChar).get()));
  }

  // Bug 50255 and Bug 351310: Keep the characters unshifted for shortcuts and
  // accesskeys and make sure that numbers are always passed as such.
  if (uniChar && (mModKeyState.IsControl() || mModKeyState.IsAlt())) {
    char16_t unshiftedCharCode =
      (mVirtualKeyCode >= '0' && mVirtualKeyCode <= '9') ?
        mVirtualKeyCode :  mModKeyState.IsShift() ?
                             ComputeUnicharFromScanCode() : 0;
    // Ignore diacritics (top bit set) and key mapping errors (char code 0)
    if (uniChar != unshiftedCharCode &&
        static_cast<int32_t>(unshiftedCharCode) > 0) {
      uniChar = unshiftedCharCode;
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::HandleCharMessage(), adjusting computed charCode "
         "because unshifted charCode is better, uniChar=%s, mModKeyState=%s",
         this, GetCharacterCodeName(uniChar).get(),
         ToString(mModKeyState).get()));
    }
  }

  // Bug 285161 and Bug 295095: They were caused by the initial fix for
  // bug 178110.  When pressing (alt|ctrl)+char, the char must be lowercase
  // unless shift is pressed too.
  if (!mModKeyState.IsShift() &&
      (mModKeyState.IsAlt() || mModKeyState.IsControl()) &&
      uniChar != towlower(uniChar)) {
    uniChar = towlower(uniChar);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), making computed charCode "
       "lower case character because Shift isn't pressed but Ctrl or Alt is "
       "pressed, uniChar=%s, mModKeyState=%s",
       this, GetCharacterCodeName(uniChar).get(),
       ToString(mModKeyState).get()));
  }

  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
      ("%p   NativeKey::HandleCharMessage(), FAILED due to "
       "BeginNativeInputTransaction() failure", this));
    return true;
  }

  MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
    ("%p   NativeKey::HandleCharMessage(), initializing keypress "
     "event after some hacks...", this));
  WidgetKeyboardEvent keypressEvent(true, eKeyPress, mWidget);
  keypressEvent.mCharCode = uniChar;
  if (!keypressEvent.mCharCode) {
    keypressEvent.mKeyCode = mDOMKeyCode;
  }
  nsEventStatus status = InitKeyEvent(keypressEvent, mModKeyState, &aCharMsg);
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::HandleCharMessage(), dispatching keypress event with "
     "some hacks...", this));
  bool dispatched =
    mDispatcher->MaybeDispatchKeypressEvents(keypressEvent, status,
                                             const_cast<NativeKey*>(this));
  if (aEventDispatched) {
    *aEventDispatched = dispatched;
  }
  if (mWidget->Destroyed()) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleCharMessage(), keypress event caused "
       "destroying the widget", this));
    return true;
  }
  bool consumed = status == nsEventStatus_eConsumeNoDefault;
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::HandleCharMessage(), dispatched keypress event with "
     "some hacks, dispatched=%s, consumed=%s",
     this, GetBoolName(dispatched), GetBoolName(consumed)));
  return consumed;
}

bool
NativeKey::HandleKeyUpMessage(bool* aEventDispatched) const
{
  MOZ_ASSERT(IsKeyUpMessage());

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  // Ignore [shift+]alt+space so the OS can handle it.
  if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_SPACE) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyUpMessage(), doesn't dispatch keyup "
       "event due to Alt+Space", this));
    return false;
  }

  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
      ("%p   NativeKey::HandleKeyUpMessage(), FAILED due to "
       "BeginNativeInputTransaction() failure", this));
    return true;
  }

  MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
    ("%p   NativeKey::HandleKeyUpMessage(), initializing keyup event...",
     this));
  EventMessage keyUpMessage = IsKeyMessageOnPlugin() ? eKeyUpOnPlugin : eKeyUp;
  WidgetKeyboardEvent keyupEvent(true, keyUpMessage, mWidget);
  nsEventStatus status = InitKeyEvent(keyupEvent, mModKeyState, &mMsg);
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::HandleKeyUpMessage(), dispatching keyup event...",
     this));
  bool dispatched =
    mDispatcher->DispatchKeyboardEvent(keyUpMessage, keyupEvent, status,
                                       const_cast<NativeKey*>(this));
  if (aEventDispatched) {
    *aEventDispatched = dispatched;
  }
  if (mWidget->Destroyed()) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::HandleKeyUpMessage(), keyup event caused "
       "destroying the widget", this));
    return true;
  }
  bool consumed = status == nsEventStatus_eConsumeNoDefault;
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::HandleKeyUpMessage(), dispatched keyup event, "
     "dispatched=%s, consumed=%s",
     this, GetBoolName(dispatched), GetBoolName(consumed)));
  return consumed;
}

bool
NativeKey::NeedsToHandleWithoutFollowingCharMessages() const
{
  MOZ_ASSERT(IsKeyDownMessage());

  // We cannot know following char messages of key messages in a plugin
  // process.  So, let's compute the character to be inputted with every
  // printable key should be computed with the keyboard layout.
  if (IsKeyMessageOnPlugin()) {
    return true;
  }

  // If the keydown message is generated for inputting some Unicode characters
  // via SendInput() API, we need to handle it only with WM_*CHAR messages.
  if (mVirtualKeyCode == VK_PACKET) {
    return false;
  }

  // Enter and backspace are always handled here to avoid for example the
  // confusion between ctrl-enter and ctrl-J.
  if (mDOMKeyCode == NS_VK_RETURN || mDOMKeyCode == NS_VK_BACK) {
    return true;
  }

  // If inputting two or more characters, should be dispatched after removing
  // whole following char messages.
  if (mCommittedCharsAndModifiers.mLength > 1) {
    return true;
  }

  // If keydown message is followed by WM_CHAR whose wParam isn't a control
  // character, we should dispatch keypress event with the char message
  // even with any modifier state.
  if (IsFollowedByNonControlCharMessage()) {
    return false;
  }

  // If any modifier keys which may cause printable keys becoming non-printable
  // are not pressed, we don't need special handling for the key.
  if (!mModKeyState.IsControl() && !mModKeyState.IsAlt() &&
      !mModKeyState.IsWin()) {
    return false;
  }

  // If the key event causes dead key event, we don't need to dispatch keypress
  // event.
  if (mIsDeadKey && mCommittedCharsAndModifiers.IsEmpty()) {
    return false;
  }

  // Even if the key is a printable key, it might cause non-printable character
  // input with modifier key(s).
  return mIsPrintableKey;
}

#ifdef MOZ_CRASHREPORTER

static nsCString
GetResultOfInSendMessageEx()
{
  DWORD ret = ::InSendMessageEx(nullptr);
  if (!ret) {
    return NS_LITERAL_CSTRING("ISMEX_NOSEND");
  }
  nsAutoCString result;
  if (ret & ISMEX_CALLBACK) {
    result = "ISMEX_CALLBACK";
  }
  if (ret & ISMEX_NOTIFY) {
    if (!result.IsEmpty()) {
      result += " | ";
    }
    result += "ISMEX_NOTIFY";
  }
  if (ret & ISMEX_REPLIED) {
    if (!result.IsEmpty()) {
      result += " | ";
    }
    result += "ISMEX_REPLIED";
  }
  if (ret & ISMEX_SEND) {
    if (!result.IsEmpty()) {
      result += " | ";
    }
    result += "ISMEX_SEND";
  }
  return result;
}

#endif // #ifdef MOZ_CRASHREPORTER

bool
NativeKey::MayBeSameCharMessage(const MSG& aCharMsg1,
                                const MSG& aCharMsg2) const
{
  // NOTE: Although, we don't know when this case occurs, the scan code value
  //       in lParam may be changed from 0 to something.  The changed value
  //       is different from the scan code of handling keydown message.
  static const LPARAM kScanCodeMask = 0x00FF0000;
  return
    aCharMsg1.message == aCharMsg2.message &&
    aCharMsg1.wParam == aCharMsg2.wParam &&
    (aCharMsg1.lParam & ~kScanCodeMask) == (aCharMsg2.lParam & ~kScanCodeMask);
}

bool
NativeKey::GetFollowingCharMessage(MSG& aCharMsg) const
{
  MOZ_ASSERT(IsKeyDownMessage());
  MOZ_ASSERT(!IsKeyMessageOnPlugin());

  aCharMsg.message = WM_NULL;

  if (mFakeCharMsgs) {
    for (size_t i = 0; i < mFakeCharMsgs->Length(); i++) {
      FakeCharMsg& fakeCharMsg = mFakeCharMsgs->ElementAt(i);
      if (fakeCharMsg.mConsumed) {
        continue;
      }
      MSG charMsg = fakeCharMsg.GetCharMsg(mMsg.hwnd);
      fakeCharMsg.mConsumed = true;
      if (!IsCharMessage(charMsg)) {
        return false;
      }
      aCharMsg = charMsg;
      return true;
    }
    return false;
  }

  // If next key message is not char message, we should give up to find a
  // related char message for the handling keydown event for now.
  // Note that it's possible other applications may send other key message
  // after we call TranslateMessage(). That may cause PeekMessage() failing
  // to get char message for the handling keydown message.
  MSG nextKeyMsg;
  if (!WinUtils::PeekMessage(&nextKeyMsg, mMsg.hwnd, WM_KEYFIRST, WM_KEYLAST,
                             PM_NOREMOVE | PM_NOYIELD) ||
      !IsCharMessage(nextKeyMsg)) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Verbose,
      ("%p   NativeKey::GetFollowingCharMessage(), there are no char messages",
       this));
    return false;
  }

  // On Metrofox, PeekMessage() sometimes returns WM_NULL even if we specify
  // the message range.  So, if it returns WM_NULL, we should retry to get
  // the following char message it was found above.
  for (uint32_t i = 0; i < 5; i++) {
    MSG removedMsg, nextKeyMsgInAllWindows;
    bool doCrash = false;
    if (!WinUtils::PeekMessage(&removedMsg, mMsg.hwnd,
                               nextKeyMsg.message, nextKeyMsg.message,
                               PM_REMOVE | PM_NOYIELD)) {
      // We meets unexpected case.  We should collect the message queue state
      // and crash for reporting the bug.
      doCrash = true;
      // The char message is redirected to different thread's window by focus
      // move or something or just cancelled by external application.
      if (!WinUtils::PeekMessage(&nextKeyMsgInAllWindows, 0,
                                 WM_KEYFIRST, WM_KEYLAST,
                                 PM_NOREMOVE | PM_NOYIELD)) {
        MOZ_LOG(sNativeKeyLogger, LogLevel::Warning,
          ("%p   NativeKey::GetFollowingCharMessage(), WARNING, failed to "
           "remove a char message, but it's already gone from all message "
           "queues, nextKeyMsg=%s",
           this, ToString(nextKeyMsg).get()));
        return true;
      }
      if (MayBeSameCharMessage(nextKeyMsgInAllWindows, nextKeyMsg)) {
        // The char message is redirected to different window created by our
        // thread.
        if (nextKeyMsgInAllWindows.hwnd != mMsg.hwnd) {
          aCharMsg = nextKeyMsgInAllWindows;
          MOZ_LOG(sNativeKeyLogger, LogLevel::Warning,
            ("%p   NativeKey::GetFollowingCharMessage(), WARNING, failed to "
             "remove a char message, but found in another message queue, "
             "nextKeyMsgInAllWindows=%s",
             this, ToString(nextKeyMsgInAllWindows).get()));
          return true;
        }
        // The found char message still in the queue, but PeekMessage() failed
        // to remove it only with PM_REMOVE.  Although, we don't know why this
        // occurs.  However, this occurs acctually.
        // Try to remove the char message with GetMessage() again.
        if (WinUtils::GetMessage(&removedMsg, mMsg.hwnd,
                                 nextKeyMsg.message, nextKeyMsg.message)) {
          MOZ_LOG(sNativeKeyLogger, LogLevel::Warning,
            ("%p   NativeKey::GetFollowingCharMessage(), WARNING, failed to "
             "remove a char message, but succeeded with GetMessage(), "
             "removedMsg=%s",
             this, ToString(removedMsg).get()));
          // Cancel to crash, but we need to check the removed message value.
          doCrash = false;
        }
      }
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::GetFollowingCharMessage(), FAILED, lost target "
         "message to remove, nextKeyMsg=%s",
         this, ToString(nextKeyMsg).get()));
    }

    if (doCrash) {
#ifdef MOZ_CRASHREPORTER
      nsPrintfCString info("\nPeekMessage() failed to remove char message! "
                           "\nHandling message: %s, InSendMessageEx()=%s, "
                           "\nFound message: %s, "
                           "\nWM_NULL has been removed: %d, "
                           "\nNext key message in all windows: %s, "
                           "time=%d, ",
                           ToString(mMsg).get(),
                           GetResultOfInSendMessageEx().get(),
                           ToString(nextKeyMsg).get(), i,
                           ToString(nextKeyMsgInAllWindows).get(),
                           nextKeyMsgInAllWindows.time);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MSG nextMsg;
      if (WinUtils::PeekMessage(&nextMsg, 0, 0, 0,
                                PM_NOREMOVE | PM_NOYIELD)) {
        nsPrintfCString info("\nNext message in all windows: %s, time=%d",
                             ToString(nextMsg).get(), nextMsg.time);
        CrashReporter::AppendAppNotesToCrashReport(info);
      } else {
        CrashReporter::AppendAppNotesToCrashReport(
          NS_LITERAL_CSTRING("\nThere is no message in any window"));
      }
#endif // #ifdef MOZ_CRASHREPORTER
      MOZ_CRASH("We lost the following char message");
    }

    // Retry for the strange case.
    if (removedMsg.message == WM_NULL) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Warning,
        ("%p   NativeKey::GetFollowingCharMessage(), WARNING, failed to "
         "remove a char message, instead, removed WM_NULL message, ",
         "removedMsg=%s",
         this, ToString(removedMsg).get()));
      continue;
    }

    // Typically, this case occurs with WM_DEADCHAR.  If the removed message's
    // wParam becomes 0, that means that the key event shouldn't cause text
    // input.  So, let's ignore the strange char message.
    if (removedMsg.message == nextKeyMsg.message && !removedMsg.wParam) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Warning,
        ("%p   NativeKey::GetFollowingCharMessage(), WARNING, succeeded to "
         "remove a char message, but the removed message's wParam is 0, "
         "removedMsg=%s",
         this, ToString(removedMsg).get()));
      return false;
    }

    // NOTE: Although, we don't know when this case occurs, the scan code value
    //       in lParam may be changed from 0 to something.  The changed value
    //       is different from the scan code of handling keydown message.
    if (!MayBeSameCharMessage(removedMsg, nextKeyMsg)) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
        ("%p   NativeKey::GetFollowingCharMessage(), FAILED, removed message "
         "is really different from what we have already found, removedMsg=%s, "
         "nextKeyMsg=%s",
         this, ToString(removedMsg).get(), ToString(nextKeyMsg).get()));
#ifdef MOZ_CRASHREPORTER
      nsPrintfCString info("\nPeekMessage() removed unexpcted char message! "
                           "\nHandling message: %s, InSendMessageEx()=%s, "
                           "\nFound message: %s, "
                           "\nRemoved message: %s, ",
                           ToString(mMsg).get(),
                           GetResultOfInSendMessageEx().get(),
                           ToString(nextKeyMsg).get(),
                           ToString(removedMsg).get());
      CrashReporter::AppendAppNotesToCrashReport(info);
      // What's the next key message?
      MSG nextKeyMsgAfter;
      if (WinUtils::PeekMessage(&nextKeyMsgAfter, mMsg.hwnd,
                                WM_KEYFIRST, WM_KEYLAST,
                                PM_NOREMOVE | PM_NOYIELD)) {
        nsPrintfCString info("\nNext key message after unexpected char message "
                             "removed: %s, ",
                             ToString(nextKeyMsgAfter).get());
        CrashReporter::AppendAppNotesToCrashReport(info);
      } else {
        CrashReporter::AppendAppNotesToCrashReport(
          NS_LITERAL_CSTRING("\nThere is no key message after unexpected char "
                             "message removed, "));
      }
      // Another window has a key message?
      MSG nextKeyMsgInAllWindows;
      if (WinUtils::PeekMessage(&nextKeyMsgInAllWindows, 0,
                                WM_KEYFIRST, WM_KEYLAST,
                                PM_NOREMOVE | PM_NOYIELD)) {
        nsPrintfCString info("\nNext key message in all windows: %s.",
                             ToString(nextKeyMsgInAllWindows).get());
        CrashReporter::AppendAppNotesToCrashReport(info);
      } else {
        CrashReporter::AppendAppNotesToCrashReport(
          NS_LITERAL_CSTRING("\nThere is no key message in any windows."));
      }
#endif // #ifdef MOZ_CRASHREPORTER
      MOZ_CRASH("PeekMessage() removed unexpected message");
    }

    aCharMsg = removedMsg;
    MOZ_LOG(sNativeKeyLogger, LogLevel::Verbose,
      ("%p   NativeKey::GetFollowingCharMessage(), succeeded to retrieve next "
       "char message, aCharMsg=%s",
       this, ToString(aCharMsg).get()));
    return true;
  }
  MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
    ("%p   NativeKey::GetFollowingCharMessage(), FAILED, removed messages "
     "are all WM_NULL, nextKeyMsg=%s",
     this, ToString(nextKeyMsg).get()));
#ifdef MOZ_CRASHREPORTER
  nsPrintfCString info("\nWe lost following char message! "
                       "\nHandling message: %s, InSendMessageEx()=%s, \n"
                       "Found message: %s, removed a lot of WM_NULL",
                       ToString(mMsg).get(),
                       GetResultOfInSendMessageEx().get(),
                       ToString(nextKeyMsg).get());
  CrashReporter::AppendAppNotesToCrashReport(info);
#endif // #ifdef MOZ_CRASHREPORTER
  MOZ_CRASH("We lost the following char message");
  return false;
}

bool
NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages() const
{
  MOZ_ASSERT(IsKeyDownMessage());
  MOZ_ASSERT(!IsKeyMessageOnPlugin());

  for (size_t i = 0;
       i < mFollowingCharMsgs.Length() && mWidget->ShouldDispatchPluginEvent();
       ++i) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
       "dispatching %uth plugin event for %s...",
       this, i + 1, ToString(mFollowingCharMsgs[i]).get()));
    MOZ_RELEASE_ASSERT(!mWidget->Destroyed(),
      "NativeKey tries to dispatch a plugin event on destroyed widget");
    mWidget->DispatchPluginEvent(mFollowingCharMsgs[i]);
    if (mWidget->Destroyed() || IsFocusedWindowChanged()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
         "%uth plugin event caused %s",
         this, i + 1, mWidget->Destroyed() ? "destroying the widget" :
                                             "focus change"));
      return true;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
       "dispatched %uth plugin event",
       this, i + 1));
  }

  // Dispatch odd char messages which are caused by ATOK or WXG (both of them
  // are Japanese IME) and removed by RemoveFollowingOddCharMessages().
  for (size_t i = 0;
       i < mRemovedOddCharMsgs.Length() && mWidget->ShouldDispatchPluginEvent();
       ++i) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
       "dispatching %uth plugin event for odd char message, %s...",
       this, i + 1, ToString(mFollowingCharMsgs[i]).get()));
    MOZ_RELEASE_ASSERT(!mWidget->Destroyed(),
      "NativeKey tries to dispatch a plugin event on destroyed widget");
    mWidget->DispatchPluginEvent(mRemovedOddCharMsgs[i]);
    if (mWidget->Destroyed() || IsFocusedWindowChanged()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
         "%uth plugin event for odd char message caused %s",
         this, i + 1, mWidget->Destroyed() ? "destroying the widget" :
                                             "focus change"));
      return true;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::MaybeDispatchPluginEventsForRemovedCharMessages(), "
       "dispatched %uth plugin event for odd char message",
       this, i + 1));
  }

  return false;
}

void
NativeKey::ComputeInputtingStringWithKeyboardLayout()
{
  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();

  if (KeyboardLayout::IsPrintableCharKey(mVirtualKeyCode)) {
    mInputtingStringAndModifiers = mCommittedCharsAndModifiers;
  } else {
    mInputtingStringAndModifiers.Clear();
  }
  mShiftedString.Clear();
  mUnshiftedString.Clear();
  mShiftedLatinChar = mUnshiftedLatinChar = 0;

  // XXX How about when Win key is pressed?
  if (mModKeyState.IsControl() == mModKeyState.IsAlt()) {
    return;
  }

  ModifierKeyState capsLockState(
                     mModKeyState.GetModifiers() & MODIFIER_CAPSLOCK);

  mUnshiftedString =
    keyboardLayout->GetUniCharsAndModifiers(mVirtualKeyCode, capsLockState);
  capsLockState.Set(MODIFIER_SHIFT);
  mShiftedString =
    keyboardLayout->GetUniCharsAndModifiers(mVirtualKeyCode, capsLockState);

  // The current keyboard cannot input alphabets or numerics,
  // we should append them for Shortcut/Access keys.
  // E.g., for Cyrillic keyboard layout.
  capsLockState.Unset(MODIFIER_SHIFT);
  WidgetUtils::GetLatinCharCodeForKeyCode(mDOMKeyCode,
                                          capsLockState.GetModifiers(),
                                          &mUnshiftedLatinChar,
                                          &mShiftedLatinChar);

  // If the mShiftedLatinChar isn't 0, the key code is NS_VK_[A-Z].
  if (mShiftedLatinChar) {
    // If the produced characters of the key on current keyboard layout
    // are same as computed Latin characters, we shouldn't append the
    // Latin characters to alternativeCharCode.
    if (mUnshiftedLatinChar == mUnshiftedString.mChars[0] &&
        mShiftedLatinChar == mShiftedString.mChars[0]) {
      mShiftedLatinChar = mUnshiftedLatinChar = 0;
    }
  } else if (mUnshiftedLatinChar) {
    // If the mShiftedLatinChar is 0, the mKeyCode doesn't produce
    // alphabet character.  At that time, the character may be produced
    // with Shift key.  E.g., on French keyboard layout, NS_VK_PERCENT
    // key produces LATIN SMALL LETTER U WITH GRAVE (U+00F9) without
    // Shift key but with Shift key, it produces '%'.
    // If the mUnshiftedLatinChar is produced by the key on current
    // keyboard layout, we shouldn't append it to alternativeCharCode.
    if (mUnshiftedLatinChar == mUnshiftedString.mChars[0] ||
        mUnshiftedLatinChar == mShiftedString.mChars[0]) {
      mUnshiftedLatinChar = 0;
    }
  }

  if (!mModKeyState.IsControl()) {
    return;
  }

  // If the mCharCode is not ASCII character, we should replace the
  // mCharCode with ASCII character only when Ctrl is pressed.
  // But don't replace the mCharCode when the mCharCode is not same as
  // unmodified characters. In such case, Ctrl is sometimes used for a
  // part of character inputting key combination like Shift.
  uint32_t ch =
    mModKeyState.IsShift() ? mShiftedLatinChar : mUnshiftedLatinChar;
  if (!ch) {
    return;
  }
  if (mInputtingStringAndModifiers.IsEmpty() ||
      mInputtingStringAndModifiers.UniCharsCaseInsensitiveEqual(
        mModKeyState.IsShift() ? mShiftedString : mUnshiftedString)) {
    mInputtingStringAndModifiers.Clear();
    mInputtingStringAndModifiers.Append(ch, mModKeyState.GetModifiers());
  }
}

bool
NativeKey::DispatchKeyPressEventsWithoutCharMessage() const
{
  MOZ_ASSERT(IsKeyDownMessage());
  MOZ_ASSERT(!mIsDeadKey || !mCommittedCharsAndModifiers.IsEmpty());

  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Error,
      ("%p   NativeKey::DispatchKeyPressEventsWithoutCharMessage(), FAILED due "
       "to BeginNativeInputTransaction() failure", this));
    return true;
  }

  WidgetKeyboardEvent keypressEvent(true, eKeyPress, mWidget);
  if (mInputtingStringAndModifiers.IsEmpty() &&
      mShiftedString.IsEmpty() && mUnshiftedString.IsEmpty()) {
    keypressEvent.mKeyCode = mDOMKeyCode;
  }
  MOZ_LOG(sNativeKeyLogger, LogLevel::Debug,
    ("%p   NativeKey::DispatchKeyPressEventsWithoutCharMessage(), initializing "
     "keypress event...", this));
  nsEventStatus status = InitKeyEvent(keypressEvent, mModKeyState);
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::DispatchKeyPressEventsWithoutCharMessage(), dispatching "
     "keypress event(s)...", this));
  bool dispatched =
    mDispatcher->MaybeDispatchKeypressEvents(keypressEvent, status,
                                             const_cast<NativeKey*>(this));
  if (mWidget->Destroyed()) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::DispatchKeyPressEventsWithoutCharMessage(), "
       "keypress event(s) caused destroying the widget", this));
    return true;
  }
  bool consumed = status == nsEventStatus_eConsumeNoDefault;
  MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
    ("%p   NativeKey::DispatchKeyPressEventsWithoutCharMessage(), dispatched "
     "keypress event(s), dispatched=%s, consumed=%s",
     this, GetBoolName(dispatched), GetBoolName(consumed)));
  return consumed;
}

void
NativeKey::WillDispatchKeyboardEvent(WidgetKeyboardEvent& aKeyboardEvent,
                                     uint32_t aIndex)
{
  uint32_t longestLength =
    std::max(mInputtingStringAndModifiers.mLength,
             std::max(mShiftedString.mLength, mUnshiftedString.mLength));
  uint32_t skipUniChars = longestLength - mInputtingStringAndModifiers.mLength;
  uint32_t skipShiftedChars = longestLength - mShiftedString.mLength;
  uint32_t skipUnshiftedChars = longestLength - mUnshiftedString.mLength;
  if (aIndex >= longestLength) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::WillDispatchKeyboardEvent(), does nothing for %uth "
       "%s event",
       this, aIndex + 1, ToChar(aKeyboardEvent.mMessage)));
    return;
  }

  // Check if aKeyboardEvent is the last event for a key press.
  // So, if it's not an eKeyPress event, it's always the last event.
  // Otherwise, check if the index is the last character of
  // mCommittedCharsAndModifiers.
  bool isLastIndex =
    aKeyboardEvent.mMessage != eKeyPress ||
    mCommittedCharsAndModifiers.IsEmpty() ||
    mCommittedCharsAndModifiers.mLength - 1 == aIndex;

  nsTArray<AlternativeCharCode>& altArray =
    aKeyboardEvent.mAlternativeCharCodes;

  // Set charCode and adjust modifier state for every eKeyPress event.
  // This is not necessary for the other keyboard events because the other
  // keyboard events shouldn't have non-zero charCode value and should have
  // current modifier state.
  if (aKeyboardEvent.mMessage == eKeyPress && skipUniChars <= aIndex) {
    // XXX Modifying modifier state of aKeyboardEvent is illegal, but no way
    //     to set different modifier state per keypress event except this
    //     hack.  Note that ideally, dead key should cause composition events
    //     instead of keypress events, though.
    if (aIndex - skipUniChars  < mInputtingStringAndModifiers.mLength) {
      ModifierKeyState modKeyState(mModKeyState);
      // If key in combination with Alt and/or Ctrl produces a different
      // character than without them then do not report these flags
      // because it is separate keyboard layout shift state. If dead-key
      // and base character does not produce a valid composite character
      // then both produced dead-key character and following base
      // character may have different modifier flags, too.
      modKeyState.Unset(MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT |
                        MODIFIER_ALTGRAPH | MODIFIER_CAPSLOCK);
      modKeyState.Set(
        mInputtingStringAndModifiers.mModifiers[aIndex - skipUniChars]);
      modKeyState.InitInputEvent(aKeyboardEvent);
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::WillDispatchKeyboardEvent(), "
         "setting %uth modifier state to %s",
         this, aIndex + 1, ToString(modKeyState).get()));
    }
    uint16_t uniChar =
      mInputtingStringAndModifiers.mChars[aIndex - skipUniChars];

    // The mCharCode was set from mKeyValue. However, for example, when Ctrl key
    // is pressed, its value should indicate an ASCII character for backward
    // compatibility rather than inputting character without the modifiers.
    // Therefore, we need to modify mCharCode value here.
    aKeyboardEvent.SetCharCode(uniChar);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::WillDispatchKeyboardEvent(), "
       "setting %uth charCode to %s",
       this, aIndex + 1, GetCharacterCodeName(uniChar).get()));
  }

  // We need to append alterntaive charCode values:
  //   - if the event is eKeyPress, we need to append for the index because
  //     eKeyPress event is dispatched for every character inputted by a
  //     key press.
  //   - if the event is not eKeyPress, we need to append for all characters
  //     inputted by the key press because the other keyboard events (e.g.,
  //     eKeyDown are eKeyUp) are fired only once for a key press.
  size_t count;
  if (aKeyboardEvent.mMessage == eKeyPress) {
    // Basically, append alternative charCode values only for the index.
    count = 1;
    // However, if it's the last eKeyPress event but different shift state
    // can input longer string, the last eKeyPress event should have all
    // remaining alternative charCode values.
    if (isLastIndex) {
      count = longestLength - aIndex;
    }
  } else {
    count = longestLength;
  }
  for (size_t i = 0; i < count; ++i) {
    uint16_t shiftedChar = 0, unshiftedChar = 0;
    if (skipShiftedChars <= aIndex + i) {
      shiftedChar = mShiftedString.mChars[aIndex + i - skipShiftedChars];
    }
    if (skipUnshiftedChars <= aIndex + i) {
      unshiftedChar = mUnshiftedString.mChars[aIndex + i - skipUnshiftedChars];
    }

    if (shiftedChar || unshiftedChar) {
      AlternativeCharCode chars(unshiftedChar, shiftedChar);
      altArray.AppendElement(chars);
    }

    if (!isLastIndex) {
      continue;
    }

    if (mUnshiftedLatinChar || mShiftedLatinChar) {
      AlternativeCharCode chars(mUnshiftedLatinChar, mShiftedLatinChar);
      altArray.AppendElement(chars);
    }

    // Typically, following virtual keycodes are used for a key which can
    // input the character.  However, these keycodes are also used for
    // other keys on some keyboard layout.  E.g., in spite of Shift+'1'
    // inputs '+' on Thai keyboard layout, a key which is at '=/+'
    // key on ANSI keyboard layout is VK_OEM_PLUS.  Native applications
    // handle it as '+' key if Ctrl key is pressed.
    char16_t charForOEMKeyCode = 0;
    switch (mVirtualKeyCode) {
      case VK_OEM_PLUS:   charForOEMKeyCode = '+'; break;
      case VK_OEM_COMMA:  charForOEMKeyCode = ','; break;
      case VK_OEM_MINUS:  charForOEMKeyCode = '-'; break;
      case VK_OEM_PERIOD: charForOEMKeyCode = '.'; break;
    }
    if (charForOEMKeyCode &&
        charForOEMKeyCode != mUnshiftedString.mChars[0] &&
        charForOEMKeyCode != mShiftedString.mChars[0] &&
        charForOEMKeyCode != mUnshiftedLatinChar &&
        charForOEMKeyCode != mShiftedLatinChar) {
      AlternativeCharCode OEMChars(charForOEMKeyCode, charForOEMKeyCode);
      altArray.AppendElement(OEMChars);
    }
  }
}

bool
NativeKey::DispatchKeyPressEventForFollowingCharMessage(
             const MSG& aCharMsg) const
{
  MOZ_ASSERT(IsKeyDownMessage());

  if (mFakeCharMsgs) {
    if (IsDeadCharMessage(aCharMsg)) {
      return false;
    }
#ifdef DEBUG
    if (mIsPrintableKey) {
      nsPrintfCString log(
        "mOriginalVirtualKeyCode=0x%02X, mCommittedCharsAndModifiers={ "
        "mChars=[ 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X ], mLength=%d }, "
        "wParam=0x%04X",
        mOriginalVirtualKeyCode, mCommittedCharsAndModifiers.mChars[0],
        mCommittedCharsAndModifiers.mChars[1],
        mCommittedCharsAndModifiers.mChars[2],
        mCommittedCharsAndModifiers.mChars[3],
        mCommittedCharsAndModifiers.mChars[4],
        mCommittedCharsAndModifiers.mLength, aCharMsg.wParam);
      if (mCommittedCharsAndModifiers.IsEmpty()) {
        log.Insert("length is zero: ", 0);
        NS_ERROR(log.get());
        NS_ABORT();
      } else if (mCommittedCharsAndModifiers.mChars[0] != aCharMsg.wParam) {
        log.Insert("character mismatch: ", 0);
        NS_ERROR(log.get());
        NS_ABORT();
      }
    }
#endif // #ifdef DEBUG
    return HandleCharMessage(aCharMsg);
  }

  if (IsDeadCharMessage(aCharMsg)) {
    if (!mWidget->PluginHasFocus()) {
      MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
        ("%p   NativeKey::DispatchKeyPressEventForFollowingCharMessage(), "
         "plugin doesn't have focus", this));
      return false;
    }
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::DispatchKeyPressEventForFollowingCharMessage(), "
       "dispatching plugin event...", this));
    bool ok = mWidget->DispatchPluginEvent(aCharMsg) || mWidget->Destroyed();
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::DispatchKeyPressEventForFollowingCharMessage(), "
       "dispatched plugin event, result=%s, mWidget->Destroyed()=%s",
       this, GetBoolName(ok), GetBoolName(mWidget->Destroyed())));
  }

  bool defaultPrevented = HandleCharMessage(aCharMsg);
  // If a syschar keypress wasn't processed, Windows may want to
  // handle it to activate a native menu.
  if (!defaultPrevented && IsSysCharMessage(aCharMsg)) {
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::DispatchKeyPressEventForFollowingCharMessage(), "
       "calling DefWindowProcW(aCharMsg=%s)...",
       this, ToString(aCharMsg).get()));
    ::DefWindowProcW(aCharMsg.hwnd, aCharMsg.message,
                     aCharMsg.wParam, aCharMsg.lParam);
    MOZ_LOG(sNativeKeyLogger, LogLevel::Info,
      ("%p   NativeKey::DispatchKeyPressEventForFollowingCharMessage(), "
       "called DefWindowProcW(aCharMsg=%s)",
       this, ToString(aCharMsg).get()));
  }
  return defaultPrevented;
}

/*****************************************************************************
 * mozilla::widget::KeyboardLayout
 *****************************************************************************/

KeyboardLayout* KeyboardLayout::sInstance = nullptr;
nsIIdleServiceInternal* KeyboardLayout::sIdleService = nullptr;

// This log is very noisy if you don't want to retrieve the mapping table
// of specific keyboard layout.  LogLevel::Debug and LogLevel::Verbose are
// used to log the layout mapping.  If you need to log some behavior of
// KeyboardLayout class, you should use LogLevel::Info or lower level.
LazyLogModule sKeyboardLayoutLogger("KeyboardLayoutWidgets");

// static
KeyboardLayout*
KeyboardLayout::GetInstance()
{
  if (!sInstance) {
    sInstance = new KeyboardLayout();
    nsCOMPtr<nsIIdleServiceInternal> idleService =
      do_GetService("@mozilla.org/widget/idleservice;1");
    // The refcount will be decreased at shut down.
    sIdleService = idleService.forget().take();
  }
  return sInstance;
}

// static
void
KeyboardLayout::Shutdown()
{
  delete sInstance;
  sInstance = nullptr;
  NS_IF_RELEASE(sIdleService);
}

// static
void
KeyboardLayout::NotifyIdleServiceOfUserActivity()
{
  sIdleService->ResetIdleTimeOut(0);
}

KeyboardLayout::KeyboardLayout() :
  mKeyboardLayout(0), mIsOverridden(false),
  mIsPendingToRestoreKeyboardLayout(false)
{
  mDeadKeyTableListHead = nullptr;

  // NOTE: LoadLayout() should be called via OnLayoutChange().
}

KeyboardLayout::~KeyboardLayout()
{
  ReleaseDeadKeyTables();
}

bool
KeyboardLayout::IsPrintableCharKey(uint8_t aVirtualKey)
{
  return GetKeyIndex(aVirtualKey) >= 0;
}

WORD
KeyboardLayout::ComputeScanCodeForVirtualKeyCode(uint8_t aVirtualKeyCode) const
{
  return static_cast<WORD>(
           ::MapVirtualKeyEx(aVirtualKeyCode, MAPVK_VK_TO_VSC, GetLayout()));
}

bool
KeyboardLayout::IsDeadKey(uint8_t aVirtualKey,
                          const ModifierKeyState& aModKeyState) const
{
  int32_t virtualKeyIndex = GetKeyIndex(aVirtualKey);
  if (virtualKeyIndex < 0) {
    return false;
  }

  return mVirtualKeys[virtualKeyIndex].IsDeadKey(
           VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers()));
}

bool
KeyboardLayout::IsSysKey(uint8_t aVirtualKey,
                         const ModifierKeyState& aModKeyState) const
{
  // If Alt key is not pressed, it's never a system key combination.
  // Additionally, if Ctrl key is pressed, it's never a system key combination
  // too.
  if (!aModKeyState.IsAlt() || aModKeyState.IsControl()) {
    return false;
  }

  int32_t virtualKeyIndex = GetKeyIndex(aVirtualKey);
  if (virtualKeyIndex < 0) {
    return true;
  }

  UniCharsAndModifiers inputCharsAndModifiers =
    GetUniCharsAndModifiers(aVirtualKey, aModKeyState);
  if (inputCharsAndModifiers.IsEmpty()) {
    return true;
  }

  // If the Alt key state isn't consumed, that means that the key with Alt
  // doesn't cause text input.  So, the combination is a system key.
  return inputCharsAndModifiers.mModifiers[0] != MODIFIER_ALT;
}

void
KeyboardLayout::InitNativeKey(NativeKey& aNativeKey,
                              const ModifierKeyState& aModKeyState)
{
  if (mIsPendingToRestoreKeyboardLayout) {
    LoadLayout(::GetKeyboardLayout(0));
  }

  // If the aNativeKey is initialized with WM_CHAR, the key information
  // should be discarded because mKeyValue should have the string to be
  // inputted.
  if (aNativeKey.mMsg.message == WM_CHAR) {
    char16_t ch = static_cast<char16_t>(aNativeKey.mMsg.wParam);
    // But don't set key value as printable key if the character is a control
    // character such as 0x0D at pressing Enter key.
    if (!NativeKey::IsControlChar(ch)) {
      aNativeKey.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
      aNativeKey.mCommittedCharsAndModifiers.
        Append(ch, aModKeyState.GetModifiers());
      return;
    }
  }

  uint8_t virtualKey = aNativeKey.mOriginalVirtualKeyCode;
  int32_t virtualKeyIndex = GetKeyIndex(virtualKey);

  if (virtualKeyIndex < 0) {
    // Does not produce any printable characters, but still preserves the
    // dead-key state.
    return;
  }

  MOZ_ASSERT(virtualKey != VK_PACKET,
    "At handling VK_PACKET, we shouldn't refer keyboard layout");
  MOZ_ASSERT(aNativeKey.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING,
    "Printable key's key name index must be KEY_NAME_INDEX_USE_STRING");

  bool isKeyDown = aNativeKey.IsKeyDownMessage();
  uint8_t shiftState =
    VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers());

  if (mVirtualKeys[virtualKeyIndex].IsDeadKey(shiftState)) {
    if ((isKeyDown && mActiveDeadKey < 0) ||
        (!isKeyDown && mActiveDeadKey == virtualKey)) {
      //  First dead key event doesn't generate characters.
      if (isKeyDown) {
        // Dead-key state activated at keydown.
        mActiveDeadKey = virtualKey;
        mDeadKeyShiftState = shiftState;
      }
      UniCharsAndModifiers deadChars =
        mVirtualKeys[virtualKeyIndex].GetNativeUniChars(shiftState);
      NS_ASSERTION(deadChars.mLength == 1,
                   "dead key must generate only one character");
      aNativeKey.mKeyNameIndex = KEY_NAME_INDEX_Dead;
      return;
    }

    // At keydown message handling, we need to forget the first dead key
    // because there is no guarantee coming WM_KEYUP for the second dead
    // key before next WM_KEYDOWN.  E.g., due to auto key repeat or pressing
    // another dead key before releasing current key.  Therefore, we can
    // set only a character for current key for keyup event.
    if (mActiveDeadKey < 0) {
      aNativeKey.mCommittedCharsAndModifiers =
        mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
      return;
    }

    int32_t activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
    if (activeDeadKeyIndex < 0 || activeDeadKeyIndex >= NS_NUM_OF_KEYS) {
#if defined(DEBUG) || defined(MOZ_CRASHREPORTER)
      nsPrintfCString warning("The virtual key index (%d) of mActiveDeadKey "
                              "(0x%02X) is not a printable key (virtualKey="
                              "0x%02X)",
                              activeDeadKeyIndex, mActiveDeadKey, virtualKey);
      NS_WARNING(warning.get());
#ifdef MOZ_CRASHREPORTER
      CrashReporter::AppendAppNotesToCrashReport(
                       NS_LITERAL_CSTRING("\n") + warning);
#endif // #ifdef MOZ_CRASHREPORTER
#endif // #if defined(DEBUG) || defined(MOZ_CRASHREPORTER)
      MOZ_CRASH("Trying to reference out of range of mVirtualKeys");
    }

    // Dead key followed by another dead key may cause a composed character
    // (e.g., "Russian - Mnemonic" keyboard layout's 's' -> 'c').
    if (MaybeInitNativeKeyWithCompositeChar(aNativeKey, aModKeyState)) {
      return;
    }

    // Otherwise, dead key followed by another dead key causes inputting both
    // character.
    UniCharsAndModifiers prevDeadChars =
      mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState);
    UniCharsAndModifiers newChars =
      mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
    // But keypress events should be fired for each committed character.
    aNativeKey.mCommittedCharsAndModifiers = prevDeadChars + newChars;
    if (isKeyDown) {
      DeactivateDeadKeyState();
    }
    return;
  }

  if (MaybeInitNativeKeyWithCompositeChar(aNativeKey, aModKeyState)) {
    return;
  }

  UniCharsAndModifiers baseChars =
    mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
  if (mActiveDeadKey < 0) {
    // No dead-keys are active. Just return the produced characters.
    aNativeKey.mCommittedCharsAndModifiers = baseChars;
    return;
  }

  int32_t activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
  if (NS_WARN_IF(activeDeadKeyIndex < 0)) {
    return;
  }

  // There is no valid dead-key and base character combination.
  // Return dead-key character followed by base character.
  UniCharsAndModifiers deadChars =
    mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState);
  // But keypress events should be fired for each committed character.
  aNativeKey.mCommittedCharsAndModifiers = deadChars + baseChars;
  if (isKeyDown) {
    DeactivateDeadKeyState();
  }

  return;
}

bool
KeyboardLayout::MaybeInitNativeKeyWithCompositeChar(
                  NativeKey& aNativeKey,
                  const ModifierKeyState& aModKeyState)
{
  if (mActiveDeadKey < 0) {
    return false;
  }

  int32_t activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
  if (NS_WARN_IF(activeDeadKeyIndex < 0)) {
    return false;
  }

  int32_t virtualKeyIndex = GetKeyIndex(aNativeKey.mOriginalVirtualKeyCode);
  if (NS_WARN_IF(virtualKeyIndex < 0)) {
    return false;
  }

  uint8_t shiftState =
    VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers());

  UniCharsAndModifiers baseChars =
    mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
  if (baseChars.IsEmpty() || !baseChars.mChars[0]) {
    return false;
  }

  char16_t compositeChar =
    mVirtualKeys[activeDeadKeyIndex].GetCompositeChar(mDeadKeyShiftState,
                                                      baseChars.mChars[0]);
  if (!compositeChar) {
    return false;
  }

  // Active dead-key and base character does produce exactly one composite
  // character.
  aNativeKey.mCommittedCharsAndModifiers.Append(compositeChar,
                                                baseChars.mModifiers[0]);
  if (aNativeKey.IsKeyDownMessage()) {
    DeactivateDeadKeyState();
  }
  return true;
}

UniCharsAndModifiers
KeyboardLayout::GetUniCharsAndModifiers(
                  uint8_t aVirtualKey,
                  const ModifierKeyState& aModKeyState) const
{
  UniCharsAndModifiers result;
  int32_t key = GetKeyIndex(aVirtualKey);
  if (key < 0) {
    return result;
  }
  return mVirtualKeys[key].
    GetUniChars(VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers()));
}

void
KeyboardLayout::LoadLayout(HKL aLayout)
{
  mIsPendingToRestoreKeyboardLayout = false;

  if (mKeyboardLayout == aLayout) {
    return;
  }

  mKeyboardLayout = aLayout;

  MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Debug,
    ("KeyboardLayout::LoadLayout(aLayout=0x%08X)", aLayout));

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  BYTE originalKbdState[256];
  // Bitfield with all shift states that have at least one dead-key.
  uint16_t shiftStatesWithDeadKeys = 0;
  // Bitfield with all shift states that produce any possible dead-key base
  // characters.
  uint16_t shiftStatesWithBaseChars = 0;

  mActiveDeadKey = -1;

  ReleaseDeadKeyTables();

  ::GetKeyboardState(originalKbdState);

  // For each shift state gather all printable characters that are produced
  // for normal case when no any dead-key is active.

  for (VirtualKey::ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    VirtualKey::FillKbdState(kbdState, shiftState);
    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      if (vki < 0) {
        continue;
      }
      NS_ASSERTION(uint32_t(vki) < ArrayLength(mVirtualKeys), "invalid index");
      char16_t uniChars[5];
      int32_t ret =
        ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)uniChars,
                      ArrayLength(uniChars), 0, mKeyboardLayout);
      // dead-key
      if (ret < 0) {
        shiftStatesWithDeadKeys |= (1 << shiftState);
        // Repeat dead-key to deactivate it and get its character
        // representation.
        char16_t deadChar[2];
        ret = ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)deadChar,
                            ArrayLength(deadChar), 0, mKeyboardLayout);
        NS_ASSERTION(ret == 2, "Expecting twice repeated dead-key character");
        mVirtualKeys[vki].SetDeadChar(shiftState, deadChar[0]);

        MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Debug,
          ("  %s (%d): DeadChar(%s, %s) (ret=%d)",
           kVirtualKeyName[virtualKey], vki,
           GetShiftStateName(shiftState).get(),
           GetCharacterCodeName(deadChar, 1).get(), ret));
      } else {
        if (ret == 1) {
          // dead-key can pair only with exactly one base character.
          shiftStatesWithBaseChars |= (1 << shiftState);
        }
        mVirtualKeys[vki].SetNormalChars(shiftState, uniChars, ret);
        MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Verbose,
          ("  %s (%d): NormalChar(%s, %s) (ret=%d)",
           kVirtualKeyName[virtualKey], vki,
           GetShiftStateName(shiftState).get(),
           GetCharacterCodeName(uniChars, ret).get(), ret));
      }
    }
  }

  // Now process each dead-key to find all its base characters and resulting
  // composite characters.
  for (VirtualKey::ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    if (!(shiftStatesWithDeadKeys & (1 << shiftState))) {
      continue;
    }

    VirtualKey::FillKbdState(kbdState, shiftState);

    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      if (vki >= 0 && mVirtualKeys[vki].IsDeadKey(shiftState)) {
        DeadKeyEntry deadKeyArray[256];
        int32_t n = GetDeadKeyCombinations(virtualKey, kbdState,
                                           shiftStatesWithBaseChars,
                                           deadKeyArray,
                                           ArrayLength(deadKeyArray));
        const DeadKeyTable* dkt =
          mVirtualKeys[vki].MatchingDeadKeyTable(deadKeyArray, n);
        if (!dkt) {
          dkt = AddDeadKeyTable(deadKeyArray, n);
        }
        mVirtualKeys[vki].AttachDeadKeyTable(shiftState, dkt);
      }
    }
  }

  ::SetKeyboardState(originalKbdState);

  if (MOZ_LOG_TEST(sKeyboardLayoutLogger, LogLevel::Verbose)) {
    static const UINT kExtendedScanCode[] = { 0x0000, 0xE000 };
    static const UINT kMapType =
      IsVistaOrLater() ? MAPVK_VSC_TO_VK_EX : MAPVK_VSC_TO_VK;
    MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Verbose,
      ("Logging virtual keycode values for scancode (0x%p)...",
       mKeyboardLayout));
    for (uint32_t i = 0; i < ArrayLength(kExtendedScanCode); i++) {
      for (uint32_t j = 1; j <= 0xFF; j++) {
        UINT scanCode = kExtendedScanCode[i] + j;
        UINT virtualKeyCode =
          ::MapVirtualKeyEx(scanCode, kMapType, mKeyboardLayout);
        MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Verbose,
          ("0x%04X, %s", scanCode, kVirtualKeyName[virtualKeyCode]));
      }
      // XP and Server 2003 don't support 0xE0 prefix of the scancode.
      // Therefore, we don't need to continue on them.
      if (!IsVistaOrLater()) {
        break;
      }
    }
  }
}

inline int32_t
KeyboardLayout::GetKeyIndex(uint8_t aVirtualKey)
{
// Currently these 68 (NS_NUM_OF_KEYS) virtual keys are assumed
// to produce visible representation:
// 0x20 - VK_SPACE          ' '
// 0x30..0x39               '0'..'9'
// 0x41..0x5A               'A'..'Z'
// 0x60..0x69               '0'..'9' on numpad
// 0x6A - VK_MULTIPLY       '*' on numpad
// 0x6B - VK_ADD            '+' on numpad
// 0x6D - VK_SUBTRACT       '-' on numpad
// 0x6E - VK_DECIMAL        '.' on numpad
// 0x6F - VK_DIVIDE         '/' on numpad
// 0x6E - VK_DECIMAL        '.'
// 0xBA - VK_OEM_1          ';:' for US
// 0xBB - VK_OEM_PLUS       '+' any country
// 0xBC - VK_OEM_COMMA      ',' any country
// 0xBD - VK_OEM_MINUS      '-' any country
// 0xBE - VK_OEM_PERIOD     '.' any country
// 0xBF - VK_OEM_2          '/?' for US
// 0xC0 - VK_OEM_3          '`~' for US
// 0xC1 - VK_ABNT_C1        '/?' for Brazilian
// 0xC2 - VK_ABNT_C2        separator key on numpad (Brazilian or JIS for Mac)
// 0xDB - VK_OEM_4          '[{' for US
// 0xDC - VK_OEM_5          '\|' for US
// 0xDD - VK_OEM_6          ']}' for US
// 0xDE - VK_OEM_7          ''"' for US
// 0xDF - VK_OEM_8
// 0xE1 - no name
// 0xE2 - VK_OEM_102        '\_' for JIS
// 0xE3 - no name
// 0xE4 - no name

  static const int8_t xlat[256] =
  {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
  //-----------------------------------------------------------------------
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 00
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 10
     0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 20
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, -1, -1, -1, -1, -1, -1,   // 30
    -1, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,   // 40
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, -1, -1, -1, -1, -1,   // 50
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, -1, 49, 50, 51,   // 60
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 70
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 80
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 90
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // A0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 52, 53, 54, 55, 56, 57,   // B0
    58, 59, 60, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // C0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 61, 62, 63, 64, 65,   // D0
    -1, 66, 67, 68, 69, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // E0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1    // F0
  };

  return xlat[aVirtualKey];
}

int
KeyboardLayout::CompareDeadKeyEntries(const void* aArg1,
                                      const void* aArg2,
                                      void*)
{
  const DeadKeyEntry* arg1 = static_cast<const DeadKeyEntry*>(aArg1);
  const DeadKeyEntry* arg2 = static_cast<const DeadKeyEntry*>(aArg2);

  return arg1->BaseChar - arg2->BaseChar;
}

const DeadKeyTable*
KeyboardLayout::AddDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                uint32_t aEntries)
{
  DeadKeyTableListEntry* next = mDeadKeyTableListHead;

  const size_t bytes = offsetof(DeadKeyTableListEntry, data) +
    DeadKeyTable::SizeInBytes(aEntries);
  uint8_t* p = new uint8_t[bytes];

  mDeadKeyTableListHead = reinterpret_cast<DeadKeyTableListEntry*>(p);
  mDeadKeyTableListHead->next = next;

  DeadKeyTable* dkt =
    reinterpret_cast<DeadKeyTable*>(mDeadKeyTableListHead->data);

  dkt->Init(aDeadKeyArray, aEntries);

  return dkt;
}

void
KeyboardLayout::ReleaseDeadKeyTables()
{
  while (mDeadKeyTableListHead) {
    uint8_t* p = reinterpret_cast<uint8_t*>(mDeadKeyTableListHead);
    mDeadKeyTableListHead = mDeadKeyTableListHead->next;

    delete [] p;
  }
}

bool
KeyboardLayout::EnsureDeadKeyActive(bool aIsActive,
                                    uint8_t aDeadKey,
                                    const PBYTE aDeadKeyKbdState)
{
  int32_t ret;
  do {
    char16_t dummyChars[5];
    ret = ::ToUnicodeEx(aDeadKey, 0, (PBYTE)aDeadKeyKbdState,
                        (LPWSTR)dummyChars, ArrayLength(dummyChars), 0,
                        mKeyboardLayout);
    // returned values:
    // <0 - Dead key state is active. The keyboard driver will wait for next
    //      character.
    //  1 - Previous pressed key was a valid base character that produced
    //      exactly one composite character.
    // >1 - Previous pressed key does not produce any composite characters.
    //      Return dead-key character followed by base character(s).
  } while ((ret < 0) != aIsActive);

  return (ret < 0);
}

void
KeyboardLayout::DeactivateDeadKeyState()
{
  if (mActiveDeadKey < 0) {
    return;
  }

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  VirtualKey::FillKbdState(kbdState, mDeadKeyShiftState);

  EnsureDeadKeyActive(false, mActiveDeadKey, kbdState);
  mActiveDeadKey = -1;
}

bool
KeyboardLayout::AddDeadKeyEntry(char16_t aBaseChar,
                                char16_t aCompositeChar,
                                DeadKeyEntry* aDeadKeyArray,
                                uint32_t aEntries)
{
  for (uint32_t index = 0; index < aEntries; index++) {
    if (aDeadKeyArray[index].BaseChar == aBaseChar) {
      return false;
    }
  }

  aDeadKeyArray[aEntries].BaseChar = aBaseChar;
  aDeadKeyArray[aEntries].CompositeChar = aCompositeChar;

  return true;
}

uint32_t
KeyboardLayout::GetDeadKeyCombinations(uint8_t aDeadKey,
                                       const PBYTE aDeadKeyKbdState,
                                       uint16_t aShiftStatesWithBaseChars,
                                       DeadKeyEntry* aDeadKeyArray,
                                       uint32_t aMaxEntries)
{
  bool deadKeyActive = false;
  uint32_t entries = 0;
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  for (uint32_t shiftState = 0; shiftState < 16; shiftState++) {
    if (!(aShiftStatesWithBaseChars & (1 << shiftState))) {
      continue;
    }

    VirtualKey::FillKbdState(kbdState, shiftState);

    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      // Dead-key can pair only with such key that produces exactly one base
      // character.
      if (vki >= 0 &&
          mVirtualKeys[vki].GetNativeUniChars(shiftState).mLength == 1) {
        // Ensure dead-key is in active state, when it swallows entered
        // character and waits for the next pressed key.
        if (!deadKeyActive) {
          deadKeyActive = EnsureDeadKeyActive(true, aDeadKey,
                                              aDeadKeyKbdState);
        }

        // Depending on the character the followed the dead-key, the keyboard
        // driver can produce one composite character, or a dead-key character
        // followed by a second character.
        char16_t compositeChars[5];
        int32_t ret =
          ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)compositeChars,
                        ArrayLength(compositeChars), 0, mKeyboardLayout);
        switch (ret) {
          case 0:
            // This key combination does not produce any characters. The
            // dead-key is still in active state.
            break;
          case 1: {
            char16_t baseChars[5];
            ret = ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)baseChars,
                                ArrayLength(baseChars), 0, mKeyboardLayout);
            if (entries < aMaxEntries) {
              switch (ret) {
                case 1:
                  // Exactly one composite character produced. Now, when
                  // dead-key is not active, repeat the last character one more
                  // time to determine the base character.
                  if (AddDeadKeyEntry(baseChars[0], compositeChars[0],
                                      aDeadKeyArray, entries)) {
                    entries++;
                  }
                  deadKeyActive = false;
                  break;
                case -1: {
                  // If pressing another dead-key produces different character,
                  // we should register the dead-key entry with first character
                  // produced by current key.

                  // First inactivate the dead-key state completely.
                  deadKeyActive =
                    EnsureDeadKeyActive(false, aDeadKey, aDeadKeyKbdState);
                  if (NS_WARN_IF(deadKeyActive)) {
                    MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Error,
                      ("  failed to deactivating the dead-key state..."));
                    break;
                  }
                  for (int32_t i = 0; i < 5; ++i) {
                    ret = ::ToUnicodeEx(virtualKey, 0, kbdState,
                                        (LPWSTR)baseChars,
                                        ArrayLength(baseChars),
                                        0, mKeyboardLayout);
                    if (ret >= 0) {
                      break;
                    }
                  }
                  if (ret > 0 &&
                      AddDeadKeyEntry(baseChars[0], compositeChars[0],
                                      aDeadKeyArray, entries)) {
                    entries++;
                  }
                  // Inactivate dead-key state for current virtual keycode.
                  EnsureDeadKeyActive(false, virtualKey, kbdState);
                  break;
                }
                default:
                  NS_WARN_IF("File a bug for this dead-key handling!");
                  deadKeyActive = false;
                  break;
              }
            }
            MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Debug,
              ("  %s -> %s (%d): DeadKeyEntry(%s, %s) (ret=%d)",
               kVirtualKeyName[aDeadKey], kVirtualKeyName[virtualKey], vki,
               GetCharacterCodeName(compositeChars, 1).get(),
               ret <= 0 ? "''" :
                 GetCharacterCodeName(baseChars, std::min(ret, 5)).get(), ret));
            break;
          }
          default:
            // 1. Unexpected dead-key. Dead-key chaining is not supported.
            // 2. More than one character generated. This is not a valid
            //    dead-key and base character combination.
            deadKeyActive = false;
            MOZ_LOG(sKeyboardLayoutLogger, LogLevel::Verbose,
              ("  %s -> %s (%d): Unsupport dead key type(%s) (ret=%d)",
               kVirtualKeyName[aDeadKey], kVirtualKeyName[virtualKey], vki,
               ret <= 0 ? "''" :
                 GetCharacterCodeName(compositeChars,
                                      std::min(ret, 5)).get(), ret));
            break;
        }
      }
    }
  }

  if (deadKeyActive) {
    deadKeyActive = EnsureDeadKeyActive(false, aDeadKey, aDeadKeyKbdState);
  }

  NS_QuickSort(aDeadKeyArray, entries, sizeof(DeadKeyEntry),
               CompareDeadKeyEntries, nullptr);
  return entries;
}

uint32_t
KeyboardLayout::ConvertNativeKeyCodeToDOMKeyCode(UINT aNativeKeyCode) const
{
  // Alphabet or Numeric or Numpad or Function keys
  if ((aNativeKeyCode >= 0x30 && aNativeKeyCode <= 0x39) ||
      (aNativeKeyCode >= 0x41 && aNativeKeyCode <= 0x5A) ||
      (aNativeKeyCode >= 0x60 && aNativeKeyCode <= 0x87)) {
    return static_cast<uint32_t>(aNativeKeyCode);
  }
  switch (aNativeKeyCode) {
    // Following keycodes are same as our DOM keycodes
    case VK_CANCEL:
    case VK_BACK:
    case VK_TAB:
    case VK_CLEAR:
    case VK_RETURN:
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU: // Alt
    case VK_PAUSE:
    case VK_CAPITAL: // CAPS LOCK
    case VK_KANA: // same as VK_HANGUL
    case VK_JUNJA:
    case VK_FINAL:
    case VK_HANJA: // same as VK_KANJI
    case VK_ESCAPE:
    case VK_CONVERT:
    case VK_NONCONVERT:
    case VK_ACCEPT:
    case VK_MODECHANGE:
    case VK_SPACE:
    case VK_PRIOR: // PAGE UP
    case VK_NEXT: // PAGE DOWN
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_SELECT:
    case VK_PRINT:
    case VK_EXECUTE:
    case VK_SNAPSHOT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_APPS: // Context Menu
    case VK_SLEEP:
    case VK_NUMLOCK:
    case VK_SCROLL: // SCROLL LOCK
    case VK_ATTN: // Attension key of IBM midrange computers, e.g., AS/400
    case VK_CRSEL: // Cursor Selection
    case VK_EXSEL: // Extend Selection
    case VK_EREOF: // Erase EOF key of IBM 3270 keyboard layout
    case VK_PLAY:
    case VK_ZOOM:
    case VK_PA1: // PA1 key of IBM 3270 keyboard layout
      return uint32_t(aNativeKeyCode);

    case VK_HELP:
      return NS_VK_HELP;

    // Windows key should be mapped to a Win keycode
    // They should be able to be distinguished by DOM3 KeyboardEvent.location
    case VK_LWIN:
    case VK_RWIN:
      return NS_VK_WIN;

    case VK_VOLUME_MUTE:
      return NS_VK_VOLUME_MUTE;
    case VK_VOLUME_DOWN:
      return NS_VK_VOLUME_DOWN;
    case VK_VOLUME_UP:
      return NS_VK_VOLUME_UP;

    // Following keycodes are not defined in our DOM keycodes.
    case VK_BROWSER_BACK:
    case VK_BROWSER_FORWARD:
    case VK_BROWSER_REFRESH:
    case VK_BROWSER_STOP:
    case VK_BROWSER_SEARCH:
    case VK_BROWSER_FAVORITES:
    case VK_BROWSER_HOME:
    case VK_MEDIA_NEXT_TRACK:
    case VK_MEDIA_PREV_TRACK:
    case VK_MEDIA_STOP:
    case VK_MEDIA_PLAY_PAUSE:
    case VK_LAUNCH_MAIL:
    case VK_LAUNCH_MEDIA_SELECT:
    case VK_LAUNCH_APP1:
    case VK_LAUNCH_APP2:
      return 0;

    // Following OEM specific virtual keycodes should pass through DOM keyCode
    // for compatibility with the other browsers on Windows.

    // Following OEM specific virtual keycodes are defined for Fujitsu/OASYS.
    case VK_OEM_FJ_JISHO:
    case VK_OEM_FJ_MASSHOU:
    case VK_OEM_FJ_TOUROKU:
    case VK_OEM_FJ_LOYA:
    case VK_OEM_FJ_ROYA:
    // Not sure what means "ICO".
    case VK_ICO_HELP:
    case VK_ICO_00:
    case VK_ICO_CLEAR:
    // Following OEM specific virtual keycodes are defined for Nokia/Ericsson.
    case VK_OEM_RESET:
    case VK_OEM_JUMP:
    case VK_OEM_PA1:
    case VK_OEM_PA2:
    case VK_OEM_PA3:
    case VK_OEM_WSCTRL:
    case VK_OEM_CUSEL:
    case VK_OEM_ATTN:
    case VK_OEM_FINISH:
    case VK_OEM_COPY:
    case VK_OEM_AUTO:
    case VK_OEM_ENLW:
    case VK_OEM_BACKTAB:
    // VK_OEM_CLEAR is defined as not OEM specific, but let's pass though
    // DOM keyCode like other OEM specific virtual keycodes.
    case VK_OEM_CLEAR:
      return uint32_t(aNativeKeyCode);

    // 0xE1 is an OEM specific virtual keycode. However, the value is already
    // used in our DOM keyCode for AltGr on Linux. So, this virtual keycode
    // cannot pass through DOM keyCode.
    case 0xE1:
      return 0;

    // Following keycodes are OEM keys which are keycodes for non-alphabet and
    // non-numeric keys, we should compute each keycode of them from unshifted
    // character which is inputted by each key.  But if the unshifted character
    // is not an ASCII character but shifted character is an ASCII character,
    // we should refer it.
    case VK_OEM_1:
    case VK_OEM_PLUS:
    case VK_OEM_COMMA:
    case VK_OEM_MINUS:
    case VK_OEM_PERIOD:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_8:
    case VK_OEM_102:
    case VK_ABNT_C1:
    {
      NS_ASSERTION(IsPrintableCharKey(aNativeKeyCode),
                   "The key must be printable");
      ModifierKeyState modKeyState(0);
      UniCharsAndModifiers uniChars =
        GetUniCharsAndModifiers(aNativeKeyCode, modKeyState);
      if (uniChars.mLength != 1 ||
          uniChars.mChars[0] < ' ' || uniChars.mChars[0] > 0x7F) {
        modKeyState.Set(MODIFIER_SHIFT);
        uniChars = GetUniCharsAndModifiers(aNativeKeyCode, modKeyState);
        if (uniChars.mLength != 1 ||
            uniChars.mChars[0] < ' ' || uniChars.mChars[0] > 0x7F) {
          return 0;
        }
      }
      return WidgetUtils::ComputeKeyCodeFromChar(uniChars.mChars[0]);
    }

    // IE sets 0xC2 to the DOM keyCode for VK_ABNT_C2.  However, we're already
    // using NS_VK_SEPARATOR for the separator key on Mac and Linux.  Therefore,
    // We should keep consistency between Gecko on all platforms rather than
    // with other browsers since a lot of keyCode values are already different
    // between browsers.
    case VK_ABNT_C2:
      return NS_VK_SEPARATOR;

    // VK_PROCESSKEY means IME already consumed the key event.
    case VK_PROCESSKEY:
      return 0;
    // VK_PACKET is generated by SendInput() API, we don't need to
    // care this message as key event.
    case VK_PACKET:
      return 0;
    // If a key is not mapped to a virtual keycode, 0xFF is used.
    case 0xFF:
      NS_WARNING("The key is failed to be converted to a virtual keycode");
      return 0;
  }
#ifdef DEBUG
  nsPrintfCString warning("Unknown virtual keycode (0x%08X), please check the "
                          "latest MSDN document, there may be some new "
                          "keycodes we've never known.",
                          aNativeKeyCode);
  NS_WARNING(warning.get());
#endif
  return 0;
}

KeyNameIndex
KeyboardLayout::ConvertNativeKeyCodeToKeyNameIndex(uint8_t aVirtualKey) const
{
  if (IsPrintableCharKey(aVirtualKey) || aVirtualKey == VK_PACKET) {
    return KEY_NAME_INDEX_USE_STRING;
  }

  switch (aVirtualKey) {

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX
#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
    case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    default:
      break;
  }

  HKL layout = GetLayout();
  WORD langID = LOWORD(static_cast<HKL>(layout));
  WORD primaryLangID = PRIMARYLANGID(langID);

  if (primaryLangID == LANG_JAPANESE) {
    switch (aVirtualKey) {

#undef NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX
#define NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex)\
      case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_JAPANESE_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

      default:
        break;
    }
  } else if (primaryLangID == LANG_KOREAN) {
    switch (aVirtualKey) {

#undef NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX
#define NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex)\
      case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_KOREAN_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

      default:
        return KEY_NAME_INDEX_Unidentified;
    }
  }

  switch (aVirtualKey) {

#undef NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX
#define NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex)\
    case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_OTHER_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    default:
      return KEY_NAME_INDEX_Unidentified;
  }
}

// static
CodeNameIndex
KeyboardLayout::ConvertScanCodeToCodeNameIndex(UINT aScanCode)
{
  switch (aScanCode) {

#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
    case aNativeKey: return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

    default:
      return CODE_NAME_INDEX_UNKNOWN;
  }
}

nsresult
KeyboardLayout::SynthesizeNativeKeyEvent(nsWindowBase* aWidget,
                                         int32_t aNativeKeyboardLayout,
                                         int32_t aNativeKeyCode,
                                         uint32_t aModifierFlags,
                                         const nsAString& aCharacters,
                                         const nsAString& aUnmodifiedCharacters)
{
  UINT keyboardLayoutListCount = ::GetKeyboardLayoutList(0, nullptr);
  NS_ASSERTION(keyboardLayoutListCount > 0,
               "One keyboard layout must be installed at least");
  HKL keyboardLayoutListBuff[50];
  HKL* keyboardLayoutList =
    keyboardLayoutListCount < 50 ? keyboardLayoutListBuff :
                                   new HKL[keyboardLayoutListCount];
  keyboardLayoutListCount =
    ::GetKeyboardLayoutList(keyboardLayoutListCount, keyboardLayoutList);
  NS_ASSERTION(keyboardLayoutListCount > 0,
               "Failed to get all keyboard layouts installed on the system");

  nsPrintfCString layoutName("%08x", aNativeKeyboardLayout);
  HKL loadedLayout = LoadKeyboardLayoutA(layoutName.get(), KLF_NOTELLSHELL);
  if (loadedLayout == nullptr) {
    if (keyboardLayoutListBuff != keyboardLayoutList) {
      delete [] keyboardLayoutList;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Setup clean key state and load desired layout
  BYTE originalKbdState[256];
  ::GetKeyboardState(originalKbdState);
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));
  // This changes the state of the keyboard for the current thread only,
  // and we'll restore it soon, so this should be OK.
  ::SetKeyboardState(kbdState);

  OverrideLayout(loadedLayout);

  uint8_t argumentKeySpecific = 0;
  switch (aNativeKeyCode & 0xFF) {
    case VK_SHIFT:
      aModifierFlags &= ~(nsIWidget::SHIFT_L | nsIWidget::SHIFT_R);
      argumentKeySpecific = VK_LSHIFT;
      break;
    case VK_LSHIFT:
      aModifierFlags &= ~nsIWidget::SHIFT_L;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_SHIFT;
      break;
    case VK_RSHIFT:
      aModifierFlags &= ~nsIWidget::SHIFT_R;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_SHIFT;
      break;
    case VK_CONTROL:
      aModifierFlags &= ~(nsIWidget::CTRL_L | nsIWidget::CTRL_R);
      argumentKeySpecific = VK_LCONTROL;
      break;
    case VK_LCONTROL:
      aModifierFlags &= ~nsIWidget::CTRL_L;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_CONTROL;
      break;
    case VK_RCONTROL:
      aModifierFlags &= ~nsIWidget::CTRL_R;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_CONTROL;
      break;
    case VK_MENU:
      aModifierFlags &= ~(nsIWidget::ALT_L | nsIWidget::ALT_R);
      argumentKeySpecific = VK_LMENU;
      break;
    case VK_LMENU:
      aModifierFlags &= ~nsIWidget::ALT_L;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_MENU;
      break;
    case VK_RMENU:
      aModifierFlags &= ~nsIWidget::ALT_R;
      argumentKeySpecific = aNativeKeyCode & 0xFF;
      aNativeKeyCode = (aNativeKeyCode & 0xFFFF0000) | VK_MENU;
      break;
    case VK_CAPITAL:
      aModifierFlags &= ~nsIWidget::CAPS_LOCK;
      argumentKeySpecific = VK_CAPITAL;
      break;
    case VK_NUMLOCK:
      aModifierFlags &= ~nsIWidget::NUM_LOCK;
      argumentKeySpecific = VK_NUMLOCK;
      break;
  }

  AutoTArray<KeyPair,10> keySequence;
  WinUtils::SetupKeyModifiersSequence(&keySequence, aModifierFlags);
  keySequence.AppendElement(KeyPair(aNativeKeyCode, argumentKeySpecific));

  // Simulate the pressing of each modifier key and then the real key
  // FYI: Each NativeKey instance here doesn't need to override keyboard layout
  //      since this method overrides and restores the keyboard layout.
  for (uint32_t i = 0; i < keySequence.Length(); ++i) {
    uint8_t key = keySequence[i].mGeneral;
    uint8_t keySpecific = keySequence[i].mSpecific;
    uint16_t scanCode = keySequence[i].mScanCode;
    kbdState[key] = 0x81; // key is down and toggled on if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0x81;
    }
    ::SetKeyboardState(kbdState);
    ModifierKeyState modKeyState;
    // If scan code isn't specified explicitly, let's compute it with current
    // keyboard layout.
    if (!scanCode) {
      scanCode =
        ComputeScanCodeForVirtualKeyCode(keySpecific ? keySpecific : key);
    }
    LPARAM lParam = static_cast<LPARAM>(scanCode << 16);
    // If the scan code is for an extended key, set extended key flag.
    if ((scanCode & 0xFF00) == 0xE000) {
      lParam |= 0x1000000;
    }
    bool makeSysKeyMsg = IsSysKey(key, modKeyState);
    MSG keyDownMsg =
      WinUtils::InitMSG(makeSysKeyMsg ? WM_SYSKEYDOWN : WM_KEYDOWN,
                        key, lParam, aWidget->GetWindowHandle());
    if (i == keySequence.Length() - 1) {
      bool makeDeadCharMsg =
        (IsDeadKey(key, modKeyState) && aCharacters.IsEmpty());
      nsAutoString chars(aCharacters);
      if (makeDeadCharMsg) {
        UniCharsAndModifiers deadChars =
          GetUniCharsAndModifiers(key, modKeyState);
        chars = deadChars.ToString();
        NS_ASSERTION(chars.Length() == 1,
                     "Dead char must be only one character");
      }
      if (chars.IsEmpty()) {
        NativeKey nativeKey(aWidget, keyDownMsg, modKeyState);
        nativeKey.HandleKeyDownMessage();
      } else {
        AutoTArray<NativeKey::FakeCharMsg, 10> fakeCharMsgs;
        for (uint32_t j = 0; j < chars.Length(); j++) {
          NativeKey::FakeCharMsg* fakeCharMsg = fakeCharMsgs.AppendElement();
          fakeCharMsg->mCharCode = chars.CharAt(j);
          fakeCharMsg->mScanCode = scanCode;
          fakeCharMsg->mIsSysKey = makeSysKeyMsg;
          fakeCharMsg->mIsDeadKey = makeDeadCharMsg;
        }
        NativeKey nativeKey(aWidget, keyDownMsg, modKeyState, 0, &fakeCharMsgs);
        bool dispatched;
        nativeKey.HandleKeyDownMessage(&dispatched);
        // If some char messages are not consumed, let's emulate the widget
        // receiving the message directly.
        for (uint32_t j = 1; j < fakeCharMsgs.Length(); j++) {
          if (fakeCharMsgs[j].mConsumed) {
            continue;
          }
          MSG charMsg = fakeCharMsgs[j].GetCharMsg(aWidget->GetWindowHandle());
          NativeKey nativeKey(aWidget, charMsg, modKeyState);
          nativeKey.HandleCharMessage(charMsg);
        }
      }
    } else {
      NativeKey nativeKey(aWidget, keyDownMsg, modKeyState);
      nativeKey.HandleKeyDownMessage();
    }
  }
  for (uint32_t i = keySequence.Length(); i > 0; --i) {
    uint8_t key = keySequence[i - 1].mGeneral;
    uint8_t keySpecific = keySequence[i - 1].mSpecific;
    uint16_t scanCode = keySequence[i - 1].mScanCode;
    kbdState[key] = 0; // key is up and toggled off if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0;
    }
    ::SetKeyboardState(kbdState);
    ModifierKeyState modKeyState;
    // If scan code isn't specified explicitly, let's compute it with current
    // keyboard layout.
    if (!scanCode) {
      scanCode =
        ComputeScanCodeForVirtualKeyCode(keySpecific ? keySpecific : key);
    }
    LPARAM lParam = static_cast<LPARAM>(scanCode << 16);
    // If the scan code is for an extended key, set extended key flag.
    if ((scanCode & 0xFF00) == 0xE000) {
      lParam |= 0x1000000;
    }
    // Don't use WM_SYSKEYUP for Alt keyup.
    bool makeSysKeyMsg = IsSysKey(key, modKeyState) && key != VK_MENU;
    MSG keyUpMsg = WinUtils::InitMSG(makeSysKeyMsg ? WM_SYSKEYUP : WM_KEYUP,
                                     key, lParam,
                                     aWidget->GetWindowHandle());
    NativeKey nativeKey(aWidget, keyUpMsg, modKeyState);
    nativeKey.HandleKeyUpMessage();
  }

  // Restore old key state and layout
  ::SetKeyboardState(originalKbdState);
  RestoreLayout();

  // Don't unload the layout if it's installed actually.
  for (uint32_t i = 0; i < keyboardLayoutListCount; i++) {
    if (keyboardLayoutList[i] == loadedLayout) {
      loadedLayout = 0;
      break;
    }
  }
  if (keyboardLayoutListBuff != keyboardLayoutList) {
    delete [] keyboardLayoutList;
  }
  if (loadedLayout) {
    ::UnloadKeyboardLayout(loadedLayout);
  }
  return NS_OK;
}

/*****************************************************************************
 * mozilla::widget::DeadKeyTable
 *****************************************************************************/

char16_t
DeadKeyTable::GetCompositeChar(char16_t aBaseChar) const
{
  // Dead-key table is sorted by BaseChar in ascending order.
  // Usually they are too small to use binary search.

  for (uint32_t index = 0; index < mEntries; index++) {
    if (mTable[index].BaseChar == aBaseChar) {
      return mTable[index].CompositeChar;
    }
    if (mTable[index].BaseChar > aBaseChar) {
      break;
    }
  }

  return 0;
}

/*****************************************************************************
 * mozilla::widget::RedirectedKeyDownMessage
 *****************************************************************************/

MSG RedirectedKeyDownMessageManager::sRedirectedKeyDownMsg;
bool RedirectedKeyDownMessageManager::sDefaultPreventedOfRedirectedMsg = false;

// static
bool
RedirectedKeyDownMessageManager::IsRedirectedMessage(const MSG& aMsg)
{
  return (aMsg.message == WM_KEYDOWN || aMsg.message == WM_SYSKEYDOWN) &&
         (sRedirectedKeyDownMsg.message == aMsg.message &&
          WinUtils::GetScanCode(sRedirectedKeyDownMsg.lParam) ==
            WinUtils::GetScanCode(aMsg.lParam));
}

// static
void
RedirectedKeyDownMessageManager::RemoveNextCharMessage(HWND aWnd)
{
  MSG msg;
  if (WinUtils::PeekMessage(&msg, aWnd, WM_KEYFIRST, WM_KEYLAST,
                            PM_NOREMOVE | PM_NOYIELD) &&
      (msg.message == WM_CHAR || msg.message == WM_SYSCHAR)) {
    WinUtils::PeekMessage(&msg, aWnd, msg.message, msg.message,
                          PM_REMOVE | PM_NOYIELD);
  }
}

} // namespace widget
} // namespace mozilla

