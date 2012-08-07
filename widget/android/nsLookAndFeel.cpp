/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsStyleConsts.h"
#include "nsXULAppAPI.h"
#include "nsLookAndFeel.h"
#include "gfxFont.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

bool nsLookAndFeel::mInitializedSystemColors = false;
AndroidSystemColors nsLookAndFeel::mSystemColors;

bool nsLookAndFeel::mInitializedShowPassword = false;
bool nsLookAndFeel::mShowPassword = true;

static const PRUnichar UNICODE_BULLET = 0x2022;

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

#define BG_PRELIGHT_COLOR      NS_RGB(0xee,0xee,0xee)
#define FG_PRELIGHT_COLOR      NS_RGB(0x77,0x77,0x77)
#define BLACK_COLOR            NS_RGB(0x00,0x00,0x00)
#define DARK_GRAY_COLOR        NS_RGB(0x40,0x40,0x40)
#define GRAY_COLOR             NS_RGB(0x80,0x80,0x80)
#define LIGHT_GRAY_COLOR       NS_RGB(0xa0,0xa0,0xa0)
#define RED_COLOR              NS_RGB(0xff,0x00,0x00)

nsresult
nsLookAndFeel::GetSystemColors()
{
    if (mInitializedSystemColors)
        return NS_OK;

    if (!AndroidBridge::Bridge())
        return NS_ERROR_FAILURE;

    AndroidBridge::Bridge()->GetSystemColors(&mSystemColors);

    mInitializedSystemColors = true;

    return NS_OK;
}

nsresult
nsLookAndFeel::CallRemoteGetSystemColors()
{
    // An array has to be used to get data from remote process
    InfallibleTArray<PRUint32> colors;
    PRUint32 colorsCount = sizeof(AndroidSystemColors) / sizeof(nscolor);

    if (!ContentChild::GetSingleton()->SendGetSystemColors(colorsCount, &colors))
        return NS_ERROR_FAILURE;

    NS_ASSERTION(colors.Length() == colorsCount, "System colors array is incomplete");
    if (colors.Length() == 0)
        return NS_ERROR_FAILURE;

    if (colors.Length() < colorsCount)
        colorsCount = colors.Length();

    // Array elements correspond to the members of mSystemColors structure,
    // so just copy the memory block
    memcpy(&mSystemColors, colors.Elements(), sizeof(nscolor) * colorsCount);

    mInitializedSystemColors = true;

    return NS_OK;
}

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor &aColor)
{
    nsresult rv = NS_OK;

    if (!mInitializedSystemColors) {
        if (XRE_GetProcessType() == GeckoProcessType_Default)
            rv = GetSystemColors();
        else
            rv = CallRemoteGetSystemColors();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX we'll want to use context.obtainStyledAttributes on the java side to
    // get all of these; see TextView.java for a good exmaple.

    switch (aID) {
        // These colors don't seem to be used for anything anymore in Mozilla
        // (except here at least TextSelectBackground and TextSelectForeground)
        // The CSS2 colors below are used.
    case eColorID_WindowBackground:
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_WindowForeground:
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColorID_WidgetBackground:
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_WidgetForeground:
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID_WidgetSelectBackground:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColorID_WidgetSelectForeground:
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColorID_Widget3DHighlight:
        aColor = LIGHT_GRAY_COLOR;
        break;
    case eColorID_Widget3DShadow:
        aColor = DARK_GRAY_COLOR;
        break;
    case eColorID_TextBackground:
        // not used?
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_TextForeground:
        // not used?
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        // still used
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        // still used
        aColor = mSystemColors.textColorPrimaryInverse;
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
      aColor = RED_COLOR;
      break;

        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColorID_activeborder:
        // active window border
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_activecaption:
        // active window caption background
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_appworkspace:
        // MDI background color
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_background:
        // desktop background
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID_graytext:
        // disabled text in windows, menus, etc.
        aColor = mSystemColors.textColorTertiary;
        break;
    case eColorID_highlight:
        // background of selected item
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColorID_highlighttext:
        // text of selected item
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColorID_inactiveborder:
        // inactive window border
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_inactivecaption:
        // inactive window caption
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_inactivecaptiontext:
        // text in inactive window caption
        aColor = mSystemColors.textColorTertiary;
        break;
    case eColorID_infobackground:
        // tooltip background color
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_infotext:
        // tooltip text color
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID_menu:
        // menu background
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID_menutext:
        // menu text
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID_scrollbar:
        // scrollbar gray area
        aColor = mSystemColors.colorBackground;
        break;

    case eColorID_threedface:
    case eColorID_buttonface:
        // 3-D face color
        aColor = mSystemColors.colorBackground;
        break;

    case eColorID_buttontext:
        // text on push buttons
        aColor = mSystemColors.colorForeground;
        break;

    case eColorID_buttonhighlight:
        // 3-D highlighted edge color
    case eColorID_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = LIGHT_GRAY_COLOR;
        break;

    case eColorID_threedlightshadow:
        // 3-D highlighted inner edge color
        aColor = mSystemColors.colorBackground;
        break;

    case eColorID_buttonshadow:
        // 3-D shadow edge color
    case eColorID_threedshadow:
        // 3-D shadow inner edge color
        aColor = GRAY_COLOR;
        break;

    case eColorID_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = BLACK_COLOR;
        break;

    case eColorID_window:
    case eColorID_windowframe:
        aColor = mSystemColors.colorBackground;
        break;

    case eColorID_windowtext:
        aColor = mSystemColors.textColorPrimary;
        break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID__moz_fieldtext:
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColorID__moz_dialog:
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID__moz_dialogtext:
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID__moz_dragtargetzone:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColorID__moz_buttondefault:
        // default button border color
        aColor = BLACK_COLOR;
        break;
    case eColorID__moz_buttonhoverface:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_buttonhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
        aColor = mSystemColors.textColorPrimaryInverse;
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
        aColor = mSystemColors.colorForeground;
        break;
    case eColorID__moz_combobox:
        aColor = mSystemColors.colorBackground;
        break;
    case eColorID__moz_menubartext:
        aColor = mSystemColors.colorForeground;
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
nsLookAndFeel::GetIntImpl(IntID aID, PRInt32 &aResult)
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

        default:
            aResult = 0;
            rv = NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
    nsresult rv = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
    if (NS_SUCCEEDED(rv))
        return rv;
    rv = NS_OK;

    switch (aID) {
        case eFloatID_IMEUnderlineRelativeSize:
            aResult = 1.0f;
            break;

        case eFloatID_SpellCheckerUnderlineRelativeSize:
            aResult = 1.0f;
            break;

        default:
            aResult = -1.0;
            rv = NS_ERROR_FAILURE;
            break;
    }
    return rv;
}

/*virtual*/
bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle)
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
nsLookAndFeel::GetEchoPasswordImpl()
{
    if (!mInitializedShowPassword) {
        if (XRE_GetProcessType() == GeckoProcessType_Default) {
            if (AndroidBridge::Bridge())
                mShowPassword = AndroidBridge::Bridge()->GetShowPasswordSetting();
            else
                NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available!");
        } else {
            ContentChild::GetSingleton()->SendGetShowPasswordSetting(&mShowPassword);
        }
        mInitializedShowPassword = true;
    }
    return mShowPassword;
}

PRUint32
nsLookAndFeel::GetPasswordMaskDelayImpl()
{
  // This value is hard-coded in Android OS's PasswordTransformationMethod.java
  return 1500;
}

/* virtual */
PRUnichar
nsLookAndFeel::GetPasswordCharacterImpl()
{
  // This value is hard-coded in Android OS's PasswordTransformationMethod.java
  return UNICODE_BULLET;
}
