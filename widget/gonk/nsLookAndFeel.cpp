/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsLookAndFeel.h"
#include "nsStyleConsts.h"
#include "gfxFont.h"
#include "cutils/properties.h"

static const PRUnichar UNICODE_BULLET = 0x2022;

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor &aColor)
{
    nsresult rv = NS_OK;

#define BASE_ACTIVE_COLOR     NS_RGB(0xaa,0xaa,0xaa)
#define BASE_NORMAL_COLOR     NS_RGB(0xff,0xff,0xff)
#define BASE_SELECTED_COLOR   NS_RGB(0xaa,0xaa,0xaa)
#define BG_ACTIVE_COLOR       NS_RGB(0xff,0xff,0xff)
#define BG_INSENSITIVE_COLOR  NS_RGB(0xaa,0xaa,0xaa)
#define BG_NORMAL_COLOR       NS_RGB(0xff,0xff,0xff)
#define BG_PRELIGHT_COLOR     NS_RGB(0xee,0xee,0xee)
#define BG_SELECTED_COLOR     NS_RGB(0x99,0x99,0x99)
#define DARK_NORMAL_COLOR     NS_RGB(0x88,0x88,0x88)
#define FG_INSENSITIVE_COLOR  NS_RGB(0x44,0x44,0x44)
#define FG_NORMAL_COLOR       NS_RGB(0x00,0x00,0x00)
#define FG_PRELIGHT_COLOR     NS_RGB(0x77,0x77,0x77)
#define FG_SELECTED_COLOR     NS_RGB(0xaa,0xaa,0xaa)
#define LIGHT_NORMAL_COLOR    NS_RGB(0xaa,0xaa,0xaa)
#define TEXT_ACTIVE_COLOR     NS_RGB(0x99,0x99,0x99)
#define TEXT_NORMAL_COLOR     NS_RGB(0x00,0x00,0x00)
#define TEXT_SELECTED_COLOR   NS_RGB(0x00,0x00,0x00)

    switch (aID) {
        // These colors don't seem to be used for anything anymore in Mozilla
        // (except here at least TextSelectBackground and TextSelectForeground)
        // The CSS2 colors below are used.
    case eColorID_WindowBackground:
        aColor = BASE_NORMAL_COLOR;
        break;
    case eColorID_WindowForeground:
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID_WidgetBackground:
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_WidgetForeground:
        aColor = FG_NORMAL_COLOR;
        break;
    case eColorID_WidgetSelectBackground:
        aColor = BG_SELECTED_COLOR;
        break;
    case eColorID_WidgetSelectForeground:
        aColor = FG_SELECTED_COLOR;
        break;
    case eColorID_Widget3DHighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColorID_Widget3DShadow:
        aColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eColorID_TextBackground:
        // not used?
        aColor = BASE_NORMAL_COLOR;
        break;
    case eColorID_TextForeground: 
        // not used?
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        // still used
        aColor = BASE_SELECTED_COLOR;
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        // still used
        aColor = TEXT_SELECTED_COLOR;
        break;
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
        aColor = NS_TRANSPARENT;
        break;
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
        aColor = NS_TRANSPARENT;
        break;
    case eColorID_SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
      break;

        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColorID_activeborder:
        // active window border
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_activecaption:
        // active window caption background
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_appworkspace:
        // MDI background color
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_background:
        // desktop background
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = FG_NORMAL_COLOR;
        break;
    case eColorID_graytext:
        // disabled text in windows, menus, etc.
        aColor = FG_INSENSITIVE_COLOR;
        break;
    case eColorID_highlight:
        // background of selected item
        aColor = BASE_SELECTED_COLOR;
        break;
    case eColorID_highlighttext:
        // text of selected item
        aColor = TEXT_SELECTED_COLOR;
        break;
    case eColorID_inactiveborder:
        // inactive window border
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_inactivecaption:
        // inactive window caption
        aColor = BG_INSENSITIVE_COLOR;
        break;
    case eColorID_inactivecaptiontext:
        // text in inactive window caption
        aColor = FG_INSENSITIVE_COLOR;
        break;
    case eColorID_infobackground:
        // tooltip background color
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_infotext:
        // tooltip text color
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID_menu:
        // menu background
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID_menutext:
        // menu text
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID_scrollbar:
        // scrollbar gray area
        aColor = BG_ACTIVE_COLOR;
        break;

    case eColorID_threedface:
    case eColorID_buttonface:
        // 3-D face color
        aColor = BG_NORMAL_COLOR;
        break;

    case eColorID_buttontext:
        // text on push buttons
        aColor = TEXT_NORMAL_COLOR;
        break;

    case eColorID_buttonhighlight:
        // 3-D highlighted edge color
    case eColorID_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = LIGHT_NORMAL_COLOR;
        break;

    case eColorID_threedlightshadow:
        // 3-D highlighted inner edge color
        aColor = BG_NORMAL_COLOR;
        break;

    case eColorID_buttonshadow:
        // 3-D shadow edge color
    case eColorID_threedshadow:
        // 3-D shadow inner edge color
        aColor = DARK_NORMAL_COLOR;
        break;

    case eColorID_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = NS_RGB(0,0,0);
        break;

    case eColorID_window:
    case eColorID_windowframe:
        aColor = BG_NORMAL_COLOR;
        break;

    case eColorID_windowtext:
        aColor = FG_NORMAL_COLOR;
        break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
        aColor = BASE_NORMAL_COLOR;
        break;
    case eColorID__moz_fieldtext:
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID__moz_dialog:
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID__moz_dialogtext:
        aColor = FG_NORMAL_COLOR;
        break;
    case eColorID__moz_dragtargetzone:
        aColor = BG_SELECTED_COLOR;
        break; 
    case eColorID__moz_buttondefault:
        // default button border color
        aColor = NS_RGB(0,0,0);
        break;
    case eColorID__moz_buttonhoverface:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_buttonhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
        aColor = BASE_ACTIVE_COLOR;
        break;
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
        aColor = TEXT_ACTIVE_COLOR;
        break;
    case eColorID__moz_menuhover:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_menuhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_oddtreerow:
        aColor = NS_TRANSPARENT;
        break;
    case eColorID__moz_nativehyperlinktext:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColorID__moz_comboboxtext:
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID__moz_combobox:
        aColor = BG_NORMAL_COLOR;
        break;
    case eColorID__moz_menubartext:
        aColor = TEXT_NORMAL_COLOR;
        break;
    case eColorID__moz_menubarhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    default:
        /* default color is BLACK */
        aColor = 0;
        rv = NS_ERROR_FAILURE;
        break;
    }

    return rv;
}

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
    nsresult rv = nsXPLookAndFeel::GetIntImpl(aID, aResult);
    if (NS_SUCCEEDED(rv))
        return rv;

    rv = NS_OK;

    switch (aID) {
        case eIntID_CaretBlinkTime:
            aResult = 500;
            break;

        case eIntID_CaretWidth:
            aResult = 1;
            break;

        case eIntID_ShowCaretDuringSelection:
            aResult = 0;
            break;

        case eIntID_SelectTextfieldsOnKeyFocus:
            // Select textfield content when focused by kbd
            // used by nsEventStateManager::sTextfieldSelectModel
            aResult = 1;
            break;

        case eIntID_SubmenuDelay:
            aResult = 200;
            break;

        case eIntID_TooltipDelay:
            aResult = 500;
            break;

        case eIntID_MenusCanOverlapOSBar:
            // we want XUL popups to be able to overlap the task bar.
            aResult = 1;
            break;

        case eIntID_ScrollArrowStyle:
            aResult = eScrollArrowStyle_Single;
            break;

        case eIntID_ScrollSliderStyle:
            aResult = eScrollThumbStyle_Proportional;
            break;

        case eIntID_TouchEnabled:
            aResult = 1;
            break;

        case eIntID_WindowsDefaultTheme:
        case eIntID_MaemoClassic:
        case eIntID_WindowsThemeIdentifier:
            aResult = 0;
            rv = NS_ERROR_NOT_IMPLEMENTED;
            break;

        case eIntID_SpellCheckerUnderlineStyle:
            aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
            break;

        case eIntID_ScrollbarButtonAutoRepeatBehavior:
            aResult = 0;
            break;

        case eIntID_PhysicalHomeButton: {
            char propValue[PROPERTY_VALUE_MAX];
            property_get("ro.moz.has_home_button", propValue, "1");
            aResult = atoi(propValue);
            break;
        }

        default:
            aResult = 0;
            rv = NS_ERROR_FAILURE;
    }

    return rv;
}

/*virtual*/
bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel)
{
    aFontName.AssignLiteral("\"Droid Sans\"");
    aFontStyle.style = NS_FONT_STYLE_NORMAL;
    aFontStyle.weight = NS_FONT_WEIGHT_NORMAL;
    aFontStyle.stretch = NS_FONT_STRETCH_NORMAL;
    aFontStyle.size = 9.0 * 96.0f / 72.0f;
    aFontStyle.systemFont = true;
    return true;
}

/*virtual*/
bool
nsLookAndFeel::GetEchoPasswordImpl() {
    return true;
}

/*virtual*/
uint32_t
nsLookAndFeel::GetPasswordMaskDelayImpl()
{
    // Same value on Android framework
    return 1500;
}

/* virtual */
PRUnichar
nsLookAndFeel::GetPasswordCharacterImpl()
{
    return UNICODE_BULLET;
}
