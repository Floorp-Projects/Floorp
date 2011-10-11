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

#include "mozilla/Util.h"

using namespace mozilla;

#define MAX_UNICODE 0x10FFFF

struct nsKeyConverter {
    int vkCode; // Platform independent key code
    int keysym; // GDK keysym key code
};

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// GTK keycodes are defined in <gdk/gdkkeysyms.h>
//
struct nsKeyConverter nsKeycodes[] = {
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
struct nsKeyConverter nsSunKeycodes[] = {
    {NS_VK_F11, 0x1005ff10 }, //Sun F11 key generates SunF36(0x1005ff10) keysym
    {NS_VK_F12, 0x1005ff11 }  //Sun F12 key generates SunF37(0x1005ff11) keysym
};

int
GdkKeyCodeToDOMKeyCode(int aKeysym)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    // since X has different key symbols for upper and lowercase letters and
    // mozilla does not, convert gdk's to mozilla's
    if (aKeysym >= GDK_a && aKeysym <= GDK_z)
        return aKeysym - GDK_a + NS_VK_A;
    if (aKeysym >= GDK_A && aKeysym <= GDK_Z)
        return aKeysym - GDK_A + NS_VK_A;

    // numbers
    if (aKeysym >= GDK_0 && aKeysym <= GDK_9)
        return aKeysym - GDK_0 + NS_VK_0;

    // keypad numbers
    if (aKeysym >= GDK_KP_0 && aKeysym <= GDK_KP_9)
        return aKeysym - GDK_KP_0 + NS_VK_NUMPAD0;

    // map Sun Keyboard special keysyms
    for (i = 0; i < ArrayLength(nsSunKeycodes); i++) {
        if (nsSunKeycodes[i].keysym == aKeysym)
            return(nsSunKeycodes[i].vkCode);
    }

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); i++) {
        if (nsKeycodes[i].keysym == aKeysym)
            return(nsKeycodes[i].vkCode);
    }

    // function keys
    if (aKeysym >= GDK_F1 && aKeysym <= GDK_F24)
        return aKeysym - GDK_F1 + NS_VK_F1;

    return((int)0);
}

int
DOMKeyCodeToGdkKeyCode(int aKeysym)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    if (aKeysym >= NS_VK_A && aKeysym <= NS_VK_Z)
      // gdk and DOM both use the ASCII codes for these keys.
      return aKeysym;

    // numbers
    if (aKeysym >= NS_VK_0 && aKeysym <= NS_VK_9)
      // gdk and DOM both use the ASCII codes for these keys.
      return aKeysym - GDK_0 + NS_VK_0;

    // keypad numbers
    if (aKeysym >= NS_VK_NUMPAD0 && aKeysym <= NS_VK_NUMPAD9)
      return aKeysym - NS_VK_NUMPAD0 + GDK_KP_0;

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); ++i) {
      if (nsKeycodes[i].vkCode == aKeysym) {
        return nsKeycodes[i].keysym;
      }
    }

    // function keys
    if (aKeysym >= NS_VK_F1 && aKeysym <= NS_VK_F9)
      return aKeysym - NS_VK_F1 + GDK_F1;

    return 0;
}

// Convert gdk key event keyvals to char codes if printable, 0 otherwise
PRUint32 nsConvertCharCodeToUnicode(GdkEventKey* aEvent)
{
    // Anything above 0xf000 is considered a non-printable
    // Exception: directly encoded UCS characters
    if (aEvent->keyval > 0xf000 && (aEvent->keyval & 0xff000000) != 0x01000000) {

        // Keypad keys are an exception: they return a value different
        // from their non-keypad equivalents, but mozilla doesn't distinguish.
        switch (aEvent->keyval)
            {
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
            }

        // non-printables
        return 0;
    }

    // we're supposedly printable, let's try to convert
    long ucs = keysym2ucs(aEvent->keyval);
    if ((ucs != -1) && (ucs < MAX_UNICODE))
        return ucs;

    // I guess we couldn't convert
    return 0;
}
