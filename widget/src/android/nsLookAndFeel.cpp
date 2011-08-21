/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Alex Pakhotin <alexp@mozilla.com>
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

#include "mozilla/dom/ContentChild.h"
#include "nsStyleConsts.h"
#include "nsXULAppAPI.h"
#include "nsLookAndFeel.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

PRBool nsLookAndFeel::mInitializedSystemColors = PR_FALSE;
AndroidSystemColors nsLookAndFeel::mSystemColors;

PRBool nsLookAndFeel::mInitializedShowPassword = PR_FALSE;
PRBool nsLookAndFeel::mShowPassword = PR_TRUE;

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

    mInitializedSystemColors = PR_TRUE;

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

    mInitializedSystemColors = PR_TRUE;

    return NS_OK;
}

nsresult
nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor &aColor)
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
    case eColor_WindowBackground:
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_WindowForeground:
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColor_WidgetBackground:
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_WidgetForeground:
        aColor = mSystemColors.colorForeground;
        break;
    case eColor_WidgetSelectBackground:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColor_WidgetSelectForeground:
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColor_Widget3DHighlight:
        aColor = LIGHT_GRAY_COLOR;
        break;
    case eColor_Widget3DShadow:
        aColor = DARK_GRAY_COLOR;
        break;
    case eColor_TextBackground:
        // not used?
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_TextForeground:
        // not used?
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColor_TextSelectBackground:
    case eColor_IMESelectedRawTextBackground:
    case eColor_IMESelectedConvertedTextBackground:
        // still used
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColor_TextSelectForeground:
    case eColor_IMESelectedRawTextForeground:
    case eColor_IMESelectedConvertedTextForeground:
        // still used
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColor_IMERawInputBackground:
    case eColor_IMEConvertedTextBackground:
        aColor = NS_TRANSPARENT;
        break;
    case eColor_IMERawInputForeground:
    case eColor_IMEConvertedTextForeground:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColor_IMERawInputUnderline:
    case eColor_IMEConvertedTextUnderline:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColor_IMESelectedRawTextUnderline:
    case eColor_IMESelectedConvertedTextUnderline:
        aColor = NS_TRANSPARENT;
        break;
    case eColor_SpellCheckerUnderline:
      aColor = RED_COLOR;
      break;

        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColor_activeborder:
        // active window border
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_activecaption:
        // active window caption background
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_appworkspace:
        // MDI background color
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_background:
        // desktop background
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = mSystemColors.colorForeground;
        break;
    case eColor_graytext:
        // disabled text in windows, menus, etc.
        aColor = mSystemColors.textColorTertiary;
        break;
    case eColor_highlight:
        // background of selected item
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColor_highlighttext:
        // text of selected item
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColor_inactiveborder:
        // inactive window border
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_inactivecaption:
        // inactive window caption
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_inactivecaptiontext:
        // text in inactive window caption
        aColor = mSystemColors.textColorTertiary;
        break;
    case eColor_infobackground:
        // tooltip background color
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_infotext:
        // tooltip text color
        aColor = mSystemColors.colorForeground;
        break;
    case eColor_menu:
        // menu background
        aColor = mSystemColors.colorBackground;
        break;
    case eColor_menutext:
        // menu text
        aColor = mSystemColors.colorForeground;
        break;
    case eColor_scrollbar:
        // scrollbar gray area
        aColor = mSystemColors.colorBackground;
        break;

    case eColor_threedface:
    case eColor_buttonface:
        // 3-D face color
        aColor = mSystemColors.colorBackground;
        break;

    case eColor_buttontext:
        // text on push buttons
        aColor = mSystemColors.colorForeground;
        break;

    case eColor_buttonhighlight:
        // 3-D highlighted edge color
    case eColor_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = LIGHT_GRAY_COLOR;
        break;

    case eColor_threedlightshadow:
        // 3-D highlighted inner edge color
        aColor = mSystemColors.colorBackground;
        break;

    case eColor_buttonshadow:
        // 3-D shadow edge color
    case eColor_threedshadow:
        // 3-D shadow inner edge color
        aColor = GRAY_COLOR;
        break;

    case eColor_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = BLACK_COLOR;
        break;

    case eColor_window:
    case eColor_windowframe:
        aColor = mSystemColors.colorBackground;
        break;

    case eColor_windowtext:
        aColor = mSystemColors.textColorPrimary;
        break;

    case eColor__moz_eventreerow:
    case eColor__moz_field:
        aColor = mSystemColors.colorBackground;
        break;
    case eColor__moz_fieldtext:
        aColor = mSystemColors.textColorPrimary;
        break;
    case eColor__moz_dialog:
        aColor = mSystemColors.colorBackground;
        break;
    case eColor__moz_dialogtext:
        aColor = mSystemColors.colorForeground;
        break;
    case eColor__moz_dragtargetzone:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColor__moz_buttondefault:
        // default button border color
        aColor = BLACK_COLOR;
        break;
    case eColor__moz_buttonhoverface:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColor__moz_buttonhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColor__moz_cellhighlight:
    case eColor__moz_html_cellhighlight:
        aColor = mSystemColors.textColorHighlight;
        break;
    case eColor__moz_cellhighlighttext:
    case eColor__moz_html_cellhighlighttext:
        aColor = mSystemColors.textColorPrimaryInverse;
        break;
    case eColor__moz_menuhover:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColor__moz_menuhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColor__moz_oddtreerow:
        aColor = NS_TRANSPARENT;
        break;
    case eColor__moz_nativehyperlinktext:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColor__moz_comboboxtext:
        aColor = mSystemColors.colorForeground;
        break;
    case eColor__moz_combobox:
        aColor = mSystemColors.colorBackground;
        break;
    case eColor__moz_menubartext:
        aColor = mSystemColors.colorForeground;
        break;
    case eColor__moz_menubarhovertext:
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


NS_IMETHODIMP
nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 &aMetric)
{
    nsresult rv = nsXPLookAndFeel::GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(rv))
        return rv;

    rv = NS_OK;

    switch (aID) {
        case eMetric_CaretBlinkTime:
            aMetric = 500;
            break;

        case eMetric_CaretWidth:
            aMetric = 1;
            break;

        case eMetric_ShowCaretDuringSelection:
            aMetric = 0;
            break;

        case eMetric_SelectTextfieldsOnKeyFocus:
            // Select textfield content when focused by kbd
            // used by nsEventStateManager::sTextfieldSelectModel
            aMetric = 1;
            break;

        case eMetric_SubmenuDelay:
            aMetric = 200;
            break;

        case eMetric_MenusCanOverlapOSBar:
            // we want XUL popups to be able to overlap the task bar.
            aMetric = 1;
            break;

        case eMetric_ScrollArrowStyle:
            aMetric = eMetric_ScrollArrowStyleSingle;
            break;

        case eMetric_ScrollSliderStyle:
            aMetric = eMetric_ScrollThumbStyleProportional;
            break;

        case eMetric_WindowsDefaultTheme:
        case eMetric_TouchEnabled:
        case eMetric_MaemoClassic:
        case eMetric_WindowsThemeIdentifier:
            aMetric = 0;
            rv = NS_ERROR_NOT_IMPLEMENTED;
            break;

        case eMetric_SpellCheckerUnderlineStyle:
            aMetric = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
            break;

        default:
            aMetric = 0;
            rv = NS_ERROR_FAILURE;
    }

    return rv;
}

NS_IMETHODIMP
nsLookAndFeel::GetMetric(const nsMetricFloatID aID,
                         float &aMetric)
{
    nsresult rv = nsXPLookAndFeel::GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(rv))
        return rv;
    rv = NS_OK;

    switch (aID) {
        case eMetricFloat_IMEUnderlineRelativeSize:
            aMetric = 1.0f;
            break;

        case eMetricFloat_SpellCheckerUnderlineRelativeSize:
            aMetric = 1.0f;
            break;

        default:
            aMetric = -1.0;
            rv = NS_ERROR_FAILURE;
            break;
    }
    return rv;
}

/*virtual*/
PRBool nsLookAndFeel::GetEchoPassword()
{
    if (!mInitializedShowPassword) {
        if (XRE_GetProcessType() == GeckoProcessType_Default) {
            if (AndroidBridge::Bridge())
                mShowPassword = AndroidBridge::Bridge()->GetShowPasswordSetting();
            else
                NS_ASSERTION(AndroidBridge::Bridge() != nsnull, "AndroidBridge is not available!");
        } else {
            ContentChild::GetSingleton()->SendGetShowPasswordSetting(&mShowPassword);
        }
        mInitializedShowPassword = PR_TRUE;
    }
    return mShowPassword;
}
