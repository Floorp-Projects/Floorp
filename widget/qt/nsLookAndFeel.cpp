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

#include <QGuiApplication>
#include <QFont>
#include <QScreen>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

#include "nsLookAndFeel.h"
#include "nsStyleConsts.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "mozilla/gfx/2D.h"

static const char16_t UNICODE_BULLET = 0x2022;

#define QCOLOR_TO_NS_RGB(c)                     \
  ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

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

#define BG_PRELIGHT_COLOR     NS_RGB(0xee,0xee,0xee)
#define FG_PRELIGHT_COLOR     NS_RGB(0x77,0x77,0x77)
#define RED_COLOR             NS_RGB(0xff,0x00,0x00)

    QPalette palette = QGuiApplication::palette();

    switch (aID) {
        // These colors don't seem to be used for anything anymore in Mozilla
        // (except here at least TextSelectBackground and TextSelectForeground)
        // The CSS2 colors below are used.
    case eColorID_WindowBackground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_WindowForeground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case eColorID_WidgetBackground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_WidgetForeground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case eColorID_WidgetSelectBackground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_WidgetSelectForeground:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case eColorID_Widget3DHighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColorID_Widget3DShadow:
        aColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eColorID_TextBackground:
        // not used?
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_TextForeground:
        // not used?
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        // still used
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        // still used
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
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
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_activecaption:
        // active window caption background
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_appworkspace:
        // MDI background color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_background:
        // desktop background
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID_graytext:
        // disabled text in windows, menus, etc.
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
        break;
    case eColorID_highlight:
        // background of selected item
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case eColorID_highlighttext:
        // text of selected item
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
        break;
    case eColorID_inactiveborder:
        // inactive window border
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
        break;
    case eColorID_inactivecaption:
        // inactive window caption
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
        break;
    case eColorID_inactivecaptiontext:
        // text in inactive window caption
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
        break;
    case eColorID_infobackground:
        // tooltip background color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipBase));
        break;
    case eColorID_infotext:
        // tooltip text color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipText));
        break;
    case eColorID_menu:
        // menu background
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID_menutext:
        // menu text
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID_scrollbar:
        // scrollbar gray area
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Mid));
        break;

    case eColorID_threedface:
    case eColorID_buttonface:
        // 3-D face color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
        break;

    case eColorID_buttontext:
        // text on push buttons
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ButtonText));
        break;

    case eColorID_buttonhighlight:
        // 3-D highlighted edge color
    case eColorID_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
        break;

    case eColorID_threedlightshadow:
        // 3-D highlighted inner edge color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Light));
        break;

    case eColorID_buttonshadow:
        // 3-D shadow edge color
    case eColorID_threedshadow:
        // 3-D shadow inner edge color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
        break;

    case eColorID_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Shadow));
        break;

    case eColorID_window:
    case eColorID_windowframe:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;

    case eColorID_windowtext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
        break;
    case eColorID__moz_fieldtext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID__moz_dialog:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID__moz_dialogtext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case eColorID__moz_dragtargetzone:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case eColorID__moz_buttondefault:
        // default button border color
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
        break;
    case eColorID__moz_buttonhoverface:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_buttonhovertext:
        aColor = FG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
        break;
    case eColorID__moz_menuhover:
        aColor = BG_PRELIGHT_COLOR;
        break;
    case eColorID__moz_menuhovertext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID__moz_oddtreerow:
        aColor = NS_TRANSPARENT;
        break;
    case eColorID__moz_nativehyperlinktext:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case eColorID__moz_comboboxtext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID__moz_combobox:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
        break;
    case eColorID__moz_menubartext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case eColorID__moz_menubarhovertext:
        aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
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
            // used by EventStateManager::sTextfieldSelectModel
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
        case eIntID_WindowsThemeIdentifier:
        case eIntID_OperatingSystemVersionIdentifier:
            aResult = 0;
            rv = NS_ERROR_NOT_IMPLEMENTED;
            break;

        case eIntID_IMERawInputUnderlineStyle:
        case eIntID_IMEConvertedTextUnderlineStyle:
            aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
            break;

        case eIntID_IMESelectedRawTextUnderlineStyle:
        case eIntID_IMESelectedConvertedTextUnderline:
            aResult = NS_STYLE_TEXT_DECORATION_STYLE_NONE;
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
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
        aResult = 1.0f;
        break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
        aResult = 1.0f;
        break;
    default:
        aResult = -1.0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

/*virtual*/
bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel)
{
  QFont qFont = QGuiApplication::font();

  NS_NAMED_LITERAL_STRING(quote, "\"");
  nsString family((char16_t*)qFont.family().data());
  aFontName = quote + family + quote;

  aFontStyle.systemFont = true;
  aFontStyle.style = qFont.style();
  aFontStyle.weight = qFont.weight();
  aFontStyle.stretch = qFont.stretch();
  // use pixel size directly if it is set, otherwise compute from point size
  if (qFont.pixelSize() != -1) {
    aFontStyle.size = qFont.pixelSize();
  } else {
    aFontStyle.size = qFont.pointSizeF() * qApp->primaryScreen()->logicalDotsPerInch() / 72.0f;
  }

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
char16_t
nsLookAndFeel::GetPasswordCharacterImpl()
{
    return UNICODE_BULLET;
}
