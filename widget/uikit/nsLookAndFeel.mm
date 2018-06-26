/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIColor.h>
#import <UIKit/UIInterface.h>

#include "nsLookAndFeel.h"

#include "mozilla/FontPropertyTypes.h"
#include "nsStyleConsts.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel()
    , mInitialized(false)
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

static nscolor GetColorFromUIColor(UIColor* aColor)
{
    CGColorRef cgColor = [aColor CGColor];
    CGColorSpaceModel model = CGColorSpaceGetModel(CGColorGetColorSpace(cgColor));
    const CGFloat* components = CGColorGetComponents(cgColor);
    if (model == kCGColorSpaceModelRGB) {
        return NS_RGB((unsigned int)(components[0] * 255.0),
                      (unsigned int)(components[1] * 255.0),
                      (unsigned int)(components[2] * 255.0));
    }
    else if (model == kCGColorSpaceModelMonochrome) {
        unsigned int val = (unsigned int)(components[0] * 255.0);
        return NS_RGBA(val, val, val,
                       (unsigned int)(components[1] * 255.0));
    }
    MOZ_ASSERT_UNREACHABLE("Unhandled color space!");
    return 0;
}

void
nsLookAndFeel::NativeInit()
{
  EnsureInit();
}

void
nsLookAndFeel::RefreshImpl()
{
  nsXPLookAndFeel::RefreshImpl();

  mInitialized = false;
}

nsresult
nsLookAndFeel::NativeGetColor(const ColorID aID, nscolor &aResult)
{
  EnsureInit();

  nsresult res = NS_OK;

  switch (aID) {
    case eColorID_WindowBackground:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_WindowForeground:
      aResult = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_WidgetBackground:
      aResult = NS_RGB(0xdd,0xdd,0xdd);
      break;
    case eColorID_WidgetForeground:
      aResult = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_WidgetSelectBackground:
      aResult = NS_RGB(0x80,0x80,0x80);
      break;
    case eColorID_WidgetSelectForeground:
      aResult = NS_RGB(0x00,0x00,0x80);
      break;
    case eColorID_Widget3DHighlight:
      aResult = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColorID_Widget3DShadow:
      aResult = NS_RGB(0x40,0x40,0x40);
      break;
    case eColorID_TextBackground:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_TextForeground:
      aResult = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_TextSelectBackground:
    case eColorID_highlight: // CSS2 color
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_menuhover:
      aResult = NS_RGB(0xee,0xee,0xee);
      break;
    case eColorID_TextSelectForeground:
    case eColorID_highlighttext:  // CSS2 color
    case eColorID__moz_menuhovertext:
      aResult = mColorTextSelectForeground;
      break;
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
      aResult = NS_TRANSPARENT;
      break;
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
      aResult = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
      aResult = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
      aResult = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_SpellCheckerUnderline:
      aResult = NS_RGB(0xff, 0, 0);
      break;

    //
    // css2 system colors http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    //
    case eColorID_buttontext:
    case eColorID__moz_buttonhovertext:
    case eColorID_captiontext:
    case eColorID_menutext:
    case eColorID_infotext:
    case eColorID__moz_menubartext:
    case eColorID_windowtext:
      aResult = mColorDarkText;
      break;
    case eColorID_activecaption:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_activeborder:
      aResult = NS_RGB(0x00,0x00,0x00);
      break;
     case eColorID_appworkspace:
      aResult = NS_RGB(0xFF,0xFF,0xFF);
      break;
    case eColorID_background:
      aResult = NS_RGB(0x63,0x63,0xCE);
      break;
    case eColorID_buttonface:
    case eColorID__moz_buttonhoverface:
      aResult = NS_RGB(0xF0,0xF0,0xF0);
      break;
    case eColorID_buttonhighlight:
      aResult = NS_RGB(0xFF,0xFF,0xFF);
      break;
    case eColorID_buttonshadow:
      aResult = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID_graytext:
      aResult = NS_RGB(0x44,0x44,0x44);
      break;
    case eColorID_inactiveborder:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_inactivecaption:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID_inactivecaptiontext:
      aResult = NS_RGB(0x45,0x45,0x45);
      break;
    case eColorID_scrollbar:
      aResult = NS_RGB(0,0,0); //XXX
      break;
    case eColorID_threeddarkshadow:
      aResult = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID_threedshadow:
      aResult = NS_RGB(0xE0,0xE0,0xE0);
      break;
    case eColorID_threedface:
      aResult = NS_RGB(0xF0,0xF0,0xF0);
      break;
    case eColorID_threedhighlight:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_threedlightshadow:
      aResult = NS_RGB(0xDA,0xDA,0xDA);
      break;
    case eColorID_menu:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_infobackground:
      aResult = NS_RGB(0xFF,0xFF,0xC7);
      break;
    case eColorID_windowframe:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID_window:
    case eColorID__moz_field:
    case eColorID__moz_combobox:
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID__moz_fieldtext:
    case eColorID__moz_comboboxtext:
      aResult = mColorDarkText;
      break;
    case eColorID__moz_dialog:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_dialogtext:
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
      aResult = mColorDarkText;
      break;
    case eColorID__moz_dragtargetzone:
    case eColorID__moz_mac_chrome_active:
    case eColorID__moz_mac_chrome_inactive:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_mac_focusring:
      aResult = NS_RGB(0x3F,0x98,0xDD);
      break;
    case eColorID__moz_mac_menushadow:
      aResult = NS_RGB(0xA3,0xA3,0xA3);
      break;
    case eColorID__moz_mac_menutextdisable:
      aResult = NS_RGB(0x88,0x88,0x88);
      break;
    case eColorID__moz_mac_menutextselect:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_mac_disabledtoolbartext:
      aResult = NS_RGB(0x3F,0x3F,0x3F);
      break;
    case eColorID__moz_mac_menuselect:
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_buttondefault:
      aResult = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
    case eColorID__moz_mac_secondaryhighlight:
      // For inactive list selection
      aResult = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID__moz_eventreerow:
      // Background color of even list rows.
      aResult = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID__moz_oddtreerow:
      // Background color of odd list rows.
      aResult = NS_TRANSPARENT;
      break;
    case eColorID__moz_nativehyperlinktext:
      // There appears to be no available system defined color. HARDCODING to the appropriate color.
      aResult = NS_RGB(0x14,0x4F,0xAE);
      break;
    default:
      NS_WARNING("Someone asked nsILookAndFeel for a color I don't know about");
      aResult = NS_RGB(0xff,0xff,0xff);
      res = NS_ERROR_FAILURE;
      break;
    }

  return res;
}

NS_IMETHODIMP
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 567;
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
    case eIntID_MenusCanOverlapOSBar:
      // xul popups are not allowed to overlap the menubar.
      aResult = 0;
      break;
    case eIntID_SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case eIntID_DragThresholdX:
    case eIntID_DragThresholdY:
      aResult = 4;
      break;
    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrow_None;
      break;
    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case eIntID_TreeOpenDelay:
      aResult = 1000;
      break;
    case eIntID_TreeCloseDelay:
      aResult = 1000;
      break;
    case eIntID_TreeLazyScrollDelay:
      aResult = 150;
      break;
    case eIntID_TreeScrollDelay:
      aResult = 100;
      break;
    case eIntID_TreeScrollLinesMax:
      aResult = 3;
      break;
    case eIntID_DWMCompositor:
    case eIntID_WindowsClassic:
    case eIntID_WindowsDefaultTheme:
    case eIntID_TouchEnabled:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_MacGraphiteTheme:
      aResult = 0;
      break;
    case eIntID_TabFocusModel:
      aResult = 1;    // default to just textboxes
      break;
    case eIntID_ScrollToClick:
      aResult = 0;
      break;
    case eIntID_ChosenMenuItemsShouldBlink:
      aResult = 1;
      break;
    case eIntID_IMERawInputUnderlineStyle:
    case eIntID_IMEConvertedTextUnderlineStyle:
    case eIntID_IMESelectedRawTextUnderlineStyle:
    case eIntID_IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED;
      break;
    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 2;
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

NS_IMETHODIMP
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString &aFontName,
                           gfxFontStyle &aFontStyle,
                           float aDevPixPerCSSPixel)
{
    // hack for now
    if (aID == eFont_Window || aID == eFont_Document) {
        aFontStyle.style      = FontSlantStyle::Normal();
        aFontStyle.weight     = FontWeight::Normal();
        aFontStyle.stretch    = FontStretch::Normal();
        aFontStyle.size       = 14 * aDevPixPerCSSPixel;
        aFontStyle.systemFont = true;

        aFontName.AssignLiteral("sans-serif");
        return true;
    }

    //TODO: implement more here?
    return false;
}

void
nsLookAndFeel::EnsureInit()
{
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  nscolor color;
  GetColor(eColorID_TextSelectBackground, color);
  if (color == 0x000000) {
    mColorTextSelectForeground = NS_RGB(0xff,0xff,0xff);
  } else {
    mColorTextSelectForeground = NS_DONT_CHANGE_COLOR;
  }

  mColorDarkText = GetColorFromUIColor([UIColor darkTextColor]);
}
