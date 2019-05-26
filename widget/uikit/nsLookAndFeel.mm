/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel(), mInitialized(false) {}

nsLookAndFeel::~nsLookAndFeel() {}

static nscolor GetColorFromUIColor(UIColor* aColor) {
  CGColorRef cgColor = [aColor CGColor];
  CGColorSpaceModel model = CGColorSpaceGetModel(CGColorGetColorSpace(cgColor));
  const CGFloat* components = CGColorGetComponents(cgColor);
  if (model == kCGColorSpaceModelRGB) {
    return NS_RGB((unsigned int)(components[0] * 255.0), (unsigned int)(components[1] * 255.0),
                  (unsigned int)(components[2] * 255.0));
  } else if (model == kCGColorSpaceModelMonochrome) {
    unsigned int val = (unsigned int)(components[0] * 255.0);
    return NS_RGBA(val, val, val, (unsigned int)(components[1] * 255.0));
  }
  MOZ_ASSERT_UNREACHABLE("Unhandled color space!");
  return 0;
}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

void nsLookAndFeel::RefreshImpl() {
  nsXPLookAndFeel::RefreshImpl();

  mInitialized = false;
}

nsresult nsLookAndFeel::NativeGetColor(const ColorID aID, nscolor& aResult) {
  EnsureInit();

  nsresult res = NS_OK;

  switch (aID) {
    case ColorID::WindowBackground:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::WindowForeground:
      aResult = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::WidgetBackground:
      aResult = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case ColorID::WidgetForeground:
      aResult = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::WidgetSelectBackground:
      aResult = NS_RGB(0x80, 0x80, 0x80);
      break;
    case ColorID::WidgetSelectForeground:
      aResult = NS_RGB(0x00, 0x00, 0x80);
      break;
    case ColorID::Widget3DHighlight:
      aResult = NS_RGB(0xa0, 0xa0, 0xa0);
      break;
    case ColorID::Widget3DShadow:
      aResult = NS_RGB(0x40, 0x40, 0x40);
      break;
    case ColorID::TextBackground:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::TextForeground:
      aResult = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::TextSelectBackground:
    case ColorID::Highlight:  // CSS2 color
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozMenuhover:
      aResult = NS_RGB(0xee, 0xee, 0xee);
      break;
    case ColorID::TextSelectForeground:
    case ColorID::Highlighttext:  // CSS2 color
    case ColorID::MozMenuhovertext:
      aResult = mColorTextSelectForeground;
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aResult = NS_TRANSPARENT;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aResult = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aResult = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aResult = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::SpellCheckerUnderline:
      aResult = NS_RGB(0xff, 0, 0);
      break;

    //
    // css2 system colors http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    //
    case ColorID::Buttontext:
    case ColorID::MozButtonhovertext:
    case ColorID::Captiontext:
    case ColorID::Menutext:
    case ColorID::Infotext:
    case ColorID::MozMenubartext:
    case ColorID::Windowtext:
      aResult = mColorDarkText;
      break;
    case ColorID::Activecaption:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::Activeborder:
      aResult = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Appworkspace:
      aResult = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Background:
      aResult = NS_RGB(0x63, 0x63, 0xCE);
      break;
    case ColorID::Buttonface:
    case ColorID::MozButtonhoverface:
      aResult = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Buttonhighlight:
      aResult = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Buttonshadow:
      aResult = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::Graytext:
      aResult = NS_RGB(0x44, 0x44, 0x44);
      break;
    case ColorID::Inactiveborder:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::Inactivecaption:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::Inactivecaptiontext:
      aResult = NS_RGB(0x45, 0x45, 0x45);
      break;
    case ColorID::Scrollbar:
      aResult = NS_RGB(0, 0, 0);  // XXX
      break;
    case ColorID::Threeddarkshadow:
      aResult = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::Threedshadow:
      aResult = NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threedface:
      aResult = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Threedhighlight:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::Threedlightshadow:
      aResult = NS_RGB(0xDA, 0xDA, 0xDA);
      break;
    case ColorID::Menu:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::Infobackground:
      aResult = NS_RGB(0xFF, 0xFF, 0xC7);
      break;
    case ColorID::Windowframe:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::Window:
    case ColorID::MozField:
    case ColorID::MozCombobox:
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::MozFieldtext:
    case ColorID::MozComboboxtext:
      aResult = mColorDarkText;
      break;
    case ColorID::MozDialog:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozDialogtext:
    case ColorID::MozCellhighlighttext:
    case ColorID::MozHtmlCellhighlighttext:
      aResult = mColorDarkText;
      break;
    case ColorID::MozDragtargetzone:
    case ColorID::MozMacChromeActive:
    case ColorID::MozMacChromeInactive:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozMacFocusring:
      aResult = NS_RGB(0x3F, 0x98, 0xDD);
      break;
    case ColorID::MozMacMenushadow:
      aResult = NS_RGB(0xA3, 0xA3, 0xA3);
      break;
    case ColorID::MozMacMenutextdisable:
      aResult = NS_RGB(0x88, 0x88, 0x88);
      break;
    case ColorID::MozMacMenutextselect:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozMacDisabledtoolbartext:
      aResult = NS_RGB(0x3F, 0x3F, 0x3F);
      break;
    case ColorID::MozMacMenuselect:
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozButtondefault:
      aResult = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::MozCellhighlight:
    case ColorID::MozHtmlCellhighlight:
    case ColorID::MozMacSecondaryhighlight:
      // For inactive list selection
      aResult = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::MozEventreerow:
      // Background color of even list rows.
      aResult = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::MozOddtreerow:
      // Background color of odd list rows.
      aResult = NS_TRANSPARENT;
      break;
    case ColorID::MozNativehyperlinktext:
      // There appears to be no available system defined color. HARDCODING to the appropriate color.
      aResult = NS_RGB(0x14, 0x4F, 0xAE);
      break;
    default:
      NS_WARNING("Someone asked nsILookAndFeel for a color I don't know about");
      aResult = NS_RGB(0xff, 0xff, 0xff);
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

NS_IMETHODIMP
nsLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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
      aResult = 1;  // default to just textboxes
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
nsLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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

bool nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName, gfxFontStyle& aFontStyle) {
  // hack for now
  if (aID == eFont_Window || aID == eFont_Document) {
    aFontStyle.style = FontSlantStyle::Normal();
    aFontStyle.weight = FontWeight::Normal();
    aFontStyle.stretch = FontStretch::Normal();
    aFontStyle.size = 14;
    aFontStyle.systemFont = true;

    aFontName.AssignLiteral("sans-serif");
    return true;
  }

  // TODO: implement more here?
  return false;
}

void nsLookAndFeel::EnsureInit() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  nscolor color;
  GetColor(ColorID::TextSelectBackground, color);
  if (color == 0x000000) {
    mColorTextSelectForeground = NS_RGB(0xff, 0xff, 0xff);
  } else {
    mColorTextSelectForeground = NS_DONT_CHANGE_COLOR;
  }

  mColorDarkText = GetColorFromUIColor([UIColor darkTextColor]);
}
