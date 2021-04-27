/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// for strtod()
#include <stdlib.h>

#include "nsLookAndFeel.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <pango/pango.h>
#include <pango/pango-fontmap.h>

#include <fontconfig/fontconfig.h>
#include "gfxPlatformGtk.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/Telemetry.h"
#include "ScreenHelperGTK.h"
#include "nsNativeBasicThemeGTK.h"

#include "gtkdrawing.h"
#include "nsStyleConsts.h"
#include "gfxFontConstants.h"
#include "WidgetUtils.h"
#include "nsWindow.h"

#include "mozilla/gfx/2D.h"

#include <cairo-gobject.h>
#include "WidgetStyleCache.h"
#include "prenv.h"
#include "nsCSSColorUtils.h"

using namespace mozilla;
using mozilla::LookAndFeel;

#undef LOG
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetLog;
#  define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#else
#  define LOG(args)
#endif /* MOZ_LOGGING */

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor)NS_RGB(c.red >> 8, c.green >> 8, c.blue >> 8))
#define GDK_RGBA_TO_NS_RGBA(c)                                    \
  ((nscolor)NS_RGBA((int)((c).red * 255), (int)((c).green * 255), \
                    (int)((c).blue * 255), (int)((c).alpha * 255)))

nsLookAndFeel::nsLookAndFeel() = default;

nsLookAndFeel::~nsLookAndFeel() = default;

// Modifies color |*aDest| as if a pattern of color |aSource| was painted with
// CAIRO_OPERATOR_OVER to a surface with color |*aDest|.
static void ApplyColorOver(const GdkRGBA& aSource, GdkRGBA* aDest) {
  gdouble sourceCoef = aSource.alpha;
  gdouble destCoef = aDest->alpha * (1.0 - sourceCoef);
  gdouble resultAlpha = sourceCoef + destCoef;
  if (resultAlpha != 0.0) {  // don't divide by zero
    destCoef /= resultAlpha;
    sourceCoef /= resultAlpha;
    aDest->red = sourceCoef * aSource.red + destCoef * aDest->red;
    aDest->green = sourceCoef * aSource.green + destCoef * aDest->green;
    aDest->blue = sourceCoef * aSource.blue + destCoef * aDest->blue;
    aDest->alpha = resultAlpha;
  }
}

static void GetLightAndDarkness(const GdkRGBA& aColor, double* aLightness,
                                double* aDarkness) {
  double sum = aColor.red + aColor.green + aColor.blue;
  *aLightness = sum * aColor.alpha;
  *aDarkness = (3.0 - sum) * aColor.alpha;
}

static bool GetGradientColors(const GValue* aValue, GdkRGBA* aLightColor,
                              GdkRGBA* aDarkColor) {
  if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN))
    return false;

  auto pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
  if (!pattern) return false;

  // Just picking the lightest and darkest colors as simple samples rather
  // than trying to blend, which could get messy if there are many stops.
  if (CAIRO_STATUS_SUCCESS !=
      cairo_pattern_get_color_stop_rgba(pattern, 0, nullptr, &aDarkColor->red,
                                        &aDarkColor->green, &aDarkColor->blue,
                                        &aDarkColor->alpha))
    return false;

  double maxLightness, maxDarkness;
  GetLightAndDarkness(*aDarkColor, &maxLightness, &maxDarkness);
  *aLightColor = *aDarkColor;

  GdkRGBA stop;
  for (int index = 1;
       CAIRO_STATUS_SUCCESS ==
       cairo_pattern_get_color_stop_rgba(pattern, index, nullptr, &stop.red,
                                         &stop.green, &stop.blue, &stop.alpha);
       ++index) {
    double lightness, darkness;
    GetLightAndDarkness(stop, &lightness, &darkness);
    if (lightness > maxLightness) {
      maxLightness = lightness;
      *aLightColor = stop;
    }
    if (darkness > maxDarkness) {
      maxDarkness = darkness;
      *aDarkColor = stop;
    }
  }

  return true;
}

static bool GetUnicoBorderGradientColors(GtkStyleContext* aContext,
                                         GdkRGBA* aLightColor,
                                         GdkRGBA* aDarkColor) {
  // Ubuntu 12.04 has GTK engine Unico-1.0.2, which overrides render_frame,
  // providing its own border code.  Ubuntu 14.04 has
  // Unico-1.0.3+14.04.20140109, which does not override render_frame, and
  // so does not need special attention.  The earlier Unico can be detected
  // by the -unico-border-gradient style property it registers.
  // gtk_style_properties_lookup_property() is checked first to avoid the
  // warning from gtk_style_context_get_property() when the property does
  // not exist.  (gtk_render_frame() of GTK+ 3.16 no longer uses the
  // engine.)
  const char* propertyName = "-unico-border-gradient";
  if (!gtk_style_properties_lookup_property(propertyName, nullptr, nullptr))
    return false;

  // -unico-border-gradient is used only when the CSS node's engine is Unico.
  GtkThemingEngine* engine;
  GtkStateFlags state = gtk_style_context_get_state(aContext);
  gtk_style_context_get(aContext, state, "engine", &engine, nullptr);
  if (strcmp(g_type_name(G_TYPE_FROM_INSTANCE(engine)), "UnicoEngine") != 0)
    return false;

  // draw_border() of Unico engine uses -unico-border-gradient
  // in preference to border-color.
  GValue value = G_VALUE_INIT;
  gtk_style_context_get_property(aContext, propertyName, state, &value);

  bool result = GetGradientColors(&value, aLightColor, aDarkColor);

  g_value_unset(&value);
  return result;
}

// Sets |aLightColor| and |aDarkColor| to colors from |aContext|.  Returns
// true if |aContext| uses these colors to render a visible border.
// If returning false, then the colors returned are a fallback from the
// border-color value even though |aContext| does not use these colors to
// render a border.
static bool GetBorderColors(GtkStyleContext* aContext, GdkRGBA* aLightColor,
                            GdkRGBA* aDarkColor) {
  // Determine whether the border on this style context is visible.
  GtkStateFlags state = gtk_style_context_get_state(aContext);
  GtkBorderStyle borderStyle;
  gtk_style_context_get(aContext, state, GTK_STYLE_PROPERTY_BORDER_STYLE,
                        &borderStyle, nullptr);
  bool visible = borderStyle != GTK_BORDER_STYLE_NONE &&
                 borderStyle != GTK_BORDER_STYLE_HIDDEN;
  if (visible) {
    // GTK has an initial value of zero for border-widths, and so themes
    // need to explicitly set border-widths to make borders visible.
    GtkBorder border;
    gtk_style_context_get_border(aContext, state, &border);
    visible = border.top != 0 || border.right != 0 || border.bottom != 0 ||
              border.left != 0;
  }

  if (visible &&
      GetUnicoBorderGradientColors(aContext, aLightColor, aDarkColor))
    return true;

  // The initial value for the border-color is the foreground color, and so
  // this will usually return a color distinct from the background even if
  // there is no visible border detected.
  gtk_style_context_get_border_color(aContext, state, aDarkColor);
  // TODO GTK3 - update aLightColor
  // for GTK_BORDER_STYLE_INSET/OUTSET/GROVE/RIDGE border styles.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=978172#c25
  *aLightColor = *aDarkColor;
  return visible;
}

static bool GetBorderColors(GtkStyleContext* aContext, nscolor* aLightColor,
                            nscolor* aDarkColor) {
  GdkRGBA lightColor, darkColor;
  bool ret = GetBorderColors(aContext, &lightColor, &darkColor);
  *aLightColor = GDK_RGBA_TO_NS_RGBA(lightColor);
  *aDarkColor = GDK_RGBA_TO_NS_RGBA(darkColor);
  return ret;
}

// Finds ideal cell highlight colors used for unfocused+selected cells distinct
// from both Highlight, used as focused+selected background, and the listbox
// background which is assumed to be similar to -moz-field
void nsLookAndFeel::PerThemeData::InitCellHighlightColors() {
  int32_t minLuminosityDifference = NS_SUFFICIENT_LUMINOSITY_DIFFERENCE_BG;
  int32_t backLuminosityDifference =
      NS_LUMINOSITY_DIFFERENCE(mMozWindowBackground, mFieldBackground);
  if (backLuminosityDifference >= minLuminosityDifference) {
    mMozCellHighlightBackground = mMozWindowBackground;
    mMozCellHighlightText = mMozWindowText;
    return;
  }

  uint16_t hue, sat, luminance;
  uint8_t alpha;
  mMozCellHighlightBackground = mFieldBackground;
  mMozCellHighlightText = mFieldText;

  NS_RGB2HSV(mMozCellHighlightBackground, hue, sat, luminance, alpha);

  uint16_t step = 30;
  // Lighten the color if the color is very dark
  if (luminance <= step) {
    luminance += step;
  }
  // Darken it if it is very light
  else if (luminance >= 255 - step) {
    luminance -= step;
  }
  // Otherwise, compute what works best depending on the text luminance.
  else {
    uint16_t textHue, textSat, textLuminance;
    uint8_t textAlpha;
    NS_RGB2HSV(mMozCellHighlightText, textHue, textSat, textLuminance,
               textAlpha);
    // Text is darker than background, use a lighter shade
    if (textLuminance < luminance) {
      luminance += step;
    }
    // Otherwise, use a darker shade
    else {
      luminance -= step;
    }
  }
  NS_HSV2RGB(mMozCellHighlightBackground, hue, sat, luminance, alpha);
}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

void nsLookAndFeel::RefreshImpl() {
  nsXPLookAndFeel::RefreshImpl();
  moz_gtk_refresh();

  mInitialized = false;
}

static bool IsSelectionColorForeground(LookAndFeel::ColorID aID) {
  using ColorID = LookAndFeel::ColorID;
  switch (aID) {
    case ColorID::WidgetSelectForeground:
    case ColorID::TextSelectForeground:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::Highlighttext:
    case ColorID::MozHtmlCellhighlighttext:
      return true;
    default:
      return false;
  }
}

static bool IsSelectionColorBackground(LookAndFeel::ColorID aID) {
  using ColorID = LookAndFeel::ColorID;
  switch (aID) {
    case ColorID::WidgetSelectBackground:
    case ColorID::TextSelectBackground:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::MozDragtargetzone:
    case ColorID::MozHtmlCellhighlight:
    case ColorID::Highlight:
      return true;
    default:
      return false;
  }
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme,
                                       nscolor& aColor) {
  EnsureInit();

  return mSystemTheme.GetColor(aID, aColor);
}

nsresult nsLookAndFeel::PerThemeData::GetColor(ColorID aID,
                                               nscolor& aColor) const {
  nsresult res = NS_OK;

  if (IsSelectionColorBackground(aID)) {
    aColor = mTextSelectedBackground;
    return NS_OK;
  }

  if (IsSelectionColorForeground(aID)) {
    aColor = mTextSelectedText;
    return NS_OK;
  }

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // (except here at least TextSelectBackground and TextSelectForeground)
      // The CSS2 colors below are used.
    case ColorID::WindowBackground:
    case ColorID::WidgetBackground:
    case ColorID::TextBackground:
    case ColorID::Activecaption:  // active window caption background
    case ColorID::Appworkspace:   // MDI background color
    case ColorID::Background:     // desktop background
    case ColorID::Window:
    case ColorID::Windowframe:
    case ColorID::MozDialog:
    case ColorID::MozCombobox:
      aColor = mMozWindowBackground;
      break;
    case ColorID::WindowForeground:
    case ColorID::WidgetForeground:
    case ColorID::TextForeground:
    case ColorID::Captiontext:  // text in active window caption, size box, and
                                // scrollbar arrow box (!)
    case ColorID::Windowtext:
    case ColorID::MozDialogtext:
      aColor = mMozWindowText;
      break;
    case ColorID::WidgetSelectBackground:
    case ColorID::TextSelectBackground:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::MozDragtargetzone:
    case ColorID::MozHtmlCellhighlight:
    case ColorID::Highlight:  // preference selected item,
      aColor = mTextSelectedBackground;
      break;
    case ColorID::MozAccentColor:
      aColor = mAccentColor;
      break;
    case ColorID::MozAccentColorForeground:
      aColor = mAccentColorForeground;
      break;
    case ColorID::MozCellhighlight:
      aColor = mMozCellHighlightBackground;
      break;
    case ColorID::MozCellhighlighttext:
      aColor = mMozCellHighlightText;
      break;
    case ColorID::Widget3DHighlight:
      aColor = NS_RGB(0xa0, 0xa0, 0xa0);
      break;
    case ColorID::Widget3DShadow:
      aColor = NS_RGB(0x40, 0x40, 0x40);
      break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
      break;
    case ColorID::ThemedScrollbar:
      aColor = mThemedScrollbar;
      break;
    case ColorID::ThemedScrollbarInactive:
      aColor = mThemedScrollbarInactive;
      break;
    case ColorID::ThemedScrollbarThumb:
      aColor = mThemedScrollbarThumb;
      break;
    case ColorID::ThemedScrollbarThumbHover:
      aColor = mThemedScrollbarThumbHover;
      break;
    case ColorID::ThemedScrollbarThumbActive:
      aColor = mThemedScrollbarThumbActive;
      break;
    case ColorID::ThemedScrollbarThumbInactive:
      aColor = mThemedScrollbarThumbInactive;
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:
      // active window border
      aColor = mMozWindowActiveBorder;
      break;
    case ColorID::Inactiveborder:
      // inactive window border
      aColor = mMozWindowInactiveBorder;
      break;
    case ColorID::Graytext:             // disabled text in windows, menus, etc.
    case ColorID::Inactivecaptiontext:  // text in inactive window caption
      aColor = mMenuTextInactive;
      break;
    case ColorID::Inactivecaption:
      // inactive window caption
      aColor = mMozWindowInactiveCaption;
      break;
    case ColorID::Infobackground:
      // tooltip background color
      aColor = mInfoBackground;
      break;
    case ColorID::Infotext:
      // tooltip text color
      aColor = mInfoText;
      break;
    case ColorID::Menu:
      // menu background
      aColor = mMenuBackground;
      break;
    case ColorID::Menutext:
      // menu text
      aColor = mMenuText;
      break;
    case ColorID::Scrollbar:
      // scrollbar gray area
      aColor = mMozScrollbar;
      break;

    case ColorID::Threedface:
    case ColorID::Buttonface:
      // 3-D face color
      aColor = mMozWindowBackground;
      break;

    case ColorID::Buttontext:
      // text on push buttons
      aColor = mButtonText;
      break;

    case ColorID::Buttonhighlight:
      // 3-D highlighted edge color
    case ColorID::Threedhighlight:
      // 3-D highlighted outer edge color
      aColor = mFrameOuterLightBorder;
      break;

    case ColorID::Buttonshadow:
      // 3-D shadow edge color
    case ColorID::Threedshadow:
      // 3-D shadow inner edge color
      aColor = mFrameInnerDarkBorder;
      break;

    case ColorID::Threedlightshadow:
      aColor = NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threeddarkshadow:
      aColor = NS_RGB(0xDC, 0xDC, 0xDC);
      break;

    case ColorID::MozEventreerow:
    case ColorID::Field:
      aColor = mFieldBackground;
      break;
    case ColorID::Fieldtext:
      aColor = mFieldText;
      break;
    case ColorID::MozButtondefault:
      // default button border color
      aColor = mButtonDefault;
      break;
    case ColorID::MozButtonhoverface:
      aColor = mButtonHoverFace;
      break;
    case ColorID::MozButtonhovertext:
      aColor = mButtonHoverText;
      break;
    case ColorID::MozGtkButtonactivetext:
      aColor = mButtonActiveText;
      break;
    case ColorID::MozMenuhover:
      aColor = mMenuHover;
      break;
    case ColorID::MozMenuhovertext:
      aColor = mMenuHoverText;
      break;
    case ColorID::MozOddtreerow:
      aColor = mOddCellBackground;
      break;
    case ColorID::MozNativehyperlinktext:
      aColor = mNativeHyperLinkText;
      break;
    case ColorID::MozComboboxtext:
      aColor = mComboBoxText;
      break;
    case ColorID::MozMenubartext:
      aColor = mMenuBarText;
      break;
    case ColorID::MozMenubarhovertext:
      aColor = mMenuBarHoverText;
      break;
    case ColorID::MozGtkInfoBarText:
      aColor = mInfoBarText;
      break;
    case ColorID::MozColheadertext:
      aColor = mMozColHeaderText;
      break;
    case ColorID::MozColheaderhovertext:
      aColor = mMozColHeaderHoverText;
      break;
    default:
      /* default color is BLACK */
      aColor = 0;
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

static int32_t CheckWidgetStyle(GtkWidget* aWidget, const char* aStyle,
                                int32_t aResult) {
  gboolean value = FALSE;
  gtk_widget_style_get(aWidget, aStyle, &value, nullptr);
  return value ? aResult : 0;
}

static int32_t ConvertGTKStepperStyleToMozillaScrollArrowStyle(
    GtkWidget* aWidget) {
  if (!aWidget) return mozilla::LookAndFeel::eScrollArrowStyle_Single;

  return CheckWidgetStyle(aWidget, "has-backward-stepper",
                          mozilla::LookAndFeel::eScrollArrow_StartBackward) |
         CheckWidgetStyle(aWidget, "has-forward-stepper",
                          mozilla::LookAndFeel::eScrollArrow_EndForward) |
         CheckWidgetStyle(aWidget, "has-secondary-backward-stepper",
                          mozilla::LookAndFeel::eScrollArrow_EndBackward) |
         CheckWidgetStyle(aWidget, "has-secondary-forward-stepper",
                          mozilla::LookAndFeel::eScrollArrow_StartForward);
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult res = NS_OK;

  // We use delayed initialization by EnsureInit() here
  // to make sure mozilla::Preferences is available (Bug 115807).
  // IntID::UseAccessibilityTheme is requested before user preferences
  // are read, and so EnsureInit(), which depends on preference values,
  // is deliberately delayed until required.
  switch (aID) {
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      break;
    case IntID::ScrollButtonMiddleMouseButtonAction:
      aResult = 1;
      break;
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 2;
      break;
    case IntID::CaretBlinkTime:
      EnsureInit();
      aResult = mCaretBlinkTime;
      break;
    case IntID::CaretWidth:
      aResult = 1;
      break;
    case IntID::ShowCaretDuringSelection:
      aResult = 0;
      break;
    case IntID::SelectTextfieldsOnKeyFocus: {
      GtkSettings* settings;
      gboolean select_on_focus;

      settings = gtk_settings_get_default();
      g_object_get(settings, "gtk-entry-select-on-focus", &select_on_focus,
                   nullptr);

      if (select_on_focus)
        aResult = 1;
      else
        aResult = 0;

    } break;
    case IntID::ScrollToClick: {
      GtkSettings* settings;
      gboolean warps_slider = FALSE;

      settings = gtk_settings_get_default();
      if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                       "gtk-primary-button-warps-slider")) {
        g_object_get(settings, "gtk-primary-button-warps-slider", &warps_slider,
                     nullptr);
      }

      if (warps_slider)
        aResult = 1;
      else
        aResult = 0;
    } break;
    case IntID::SubmenuDelay: {
      GtkSettings* settings;
      gint delay;

      settings = gtk_settings_get_default();
      g_object_get(settings, "gtk-menu-popup-delay", &delay, nullptr);
      aResult = (int32_t)delay;
      break;
    }
    case IntID::TooltipDelay: {
      aResult = 500;
      break;
    }
    case IntID::MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;
    case IntID::SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case IntID::DragThresholdX:
    case IntID::DragThresholdY: {
      gint threshold = 0;
      g_object_get(gtk_settings_get_default(), "gtk-dnd-drag-threshold",
                   &threshold, nullptr);

      aResult = threshold;
    } break;
    case IntID::ScrollArrowStyle: {
      GtkWidget* scrollbar = GetWidget(MOZ_GTK_SCROLLBAR_HORIZONTAL);
      aResult = ConvertGTKStepperStyleToMozillaScrollArrowStyle(scrollbar);
      break;
    }
    case IntID::ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case IntID::TreeOpenDelay:
      aResult = 1000;
      break;
    case IntID::TreeCloseDelay:
      aResult = 1000;
      break;
    case IntID::TreeLazyScrollDelay:
      aResult = 150;
      break;
    case IntID::TreeScrollDelay:
      aResult = 100;
      break;
    case IntID::TreeScrollLinesMax:
      aResult = 3;
      break;
    case IntID::DWMCompositor:
    case IntID::WindowsClassic:
    case IntID::WindowsDefaultTheme:
    case IntID::WindowsThemeIdentifier:
    case IntID::OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::MacGraphiteTheme:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_NONE;
      break;
    case IntID::SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;
    case IntID::MenuBarDrag:
      EnsureInit();
      aResult = mSystemTheme.mMenuSupportsDrag;
      break;
    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 1;
      break;
    case IntID::SwipeAnimationEnabled:
      aResult = 0;
      break;
    case IntID::ContextMenuOffsetVertical:
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 2;
      break;
    case IntID::GTKCSDAvailable:
      EnsureInit();
      aResult = mCSDAvailable;
      break;
    case IntID::GTKCSDHideTitlebarByDefault:
      EnsureInit();
      aResult = mCSDHideTitlebarByDefault;
      break;
    case IntID::GTKCSDMaximizeButton:
      EnsureInit();
      aResult = mCSDMaximizeButton;
      break;
    case IntID::GTKCSDMinimizeButton:
      EnsureInit();
      aResult = mCSDMinimizeButton;
      break;
    case IntID::GTKCSDCloseButton:
      EnsureInit();
      aResult = mCSDCloseButton;
      break;
    case IntID::GTKCSDTransparentBackground: {
      // Enable transparent titlebar corners for titlebar mode.
      GdkScreen* screen = gdk_screen_get_default();
      aResult = gdk_screen_is_composited(screen)
                    ? (nsWindow::GtkWindowDecoration() !=
                       nsWindow::GTK_DECORATION_NONE)
                    : false;
      break;
    }
    case IntID::GTKCSDReversedPlacement:
      EnsureInit();
      aResult = mCSDReversedPlacement;
      break;
    case IntID::PrefersReducedMotion: {
      aResult = mPrefersReducedMotion;
      break;
    }
    case IntID::SystemUsesDarkTheme: {
      EnsureInit();
      aResult = mSystemTheme.mIsDark;
      break;
    }
    case IntID::GTKCSDMaximizeButtonPosition:
      aResult = mCSDMaximizeButtonPosition;
      break;
    case IntID::GTKCSDMinimizeButtonPosition:
      aResult = mCSDMinimizeButtonPosition;
      break;
    case IntID::GTKCSDCloseButtonPosition:
      aResult = mCSDCloseButtonPosition;
      break;
    case IntID::UseAccessibilityTheme: {
      EnsureInit();
      aResult = mSystemTheme.mHighContrast;
      break;
    }
    case IntID::AllowOverlayScrollbarsOverlap: {
      aResult = 1;
      break;
    }
    case IntID::ScrollbarFadeBeginDelay: {
      aResult = 1000;
      break;
    }
    case IntID::ScrollbarFadeDuration: {
      aResult = 400;
      break;
    }
    case IntID::ScrollbarDisplayOnMouseMove: {
      aResult = 1;
      break;
    }
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult rv = NS_OK;
  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::CaretAspectRatio:
      EnsureInit();
      aResult = mSystemTheme.mCaretRatio;
      break;
    case FloatID::TextScaleFactor:
      aResult = gfxPlatformGtk::GetFontScaleFactor();
      break;
    default:
      aResult = -1.0;
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

static void GetSystemFontInfo(GtkStyleContext* aStyle, nsString* aFontName,
                              gfxFontStyle* aFontStyle) {
  aFontStyle->style = FontSlantStyle::Normal();

  // As in
  // https://git.gnome.org/browse/gtk+/tree/gtk/gtkwidget.c?h=3.22.19#n10333
  PangoFontDescription* desc;
  gtk_style_context_get(aStyle, gtk_style_context_get_state(aStyle), "font",
                        &desc, nullptr);

  aFontStyle->systemFont = true;

  constexpr auto quote = u"\""_ns;
  NS_ConvertUTF8toUTF16 family(pango_font_description_get_family(desc));
  *aFontName = quote + family + quote;

  aFontStyle->weight = FontWeight(pango_font_description_get_weight(desc));

  // FIXME: Set aFontStyle->stretch correctly!
  aFontStyle->stretch = FontStretch::Normal();

  float size = float(pango_font_description_get_size(desc)) / PANGO_SCALE;

  // |size| is now either pixels or pango-points (not Mozilla-points!)

  if (!pango_font_description_get_size_is_absolute(desc)) {
    // |size| is in pango-points, so convert to pixels.
    size *= float(gfxPlatformGtk::GetFontScaleDPI()) / POINTS_PER_INCH_FLOAT;
  }

  // |size| is now pixels but not scaled for the hidpi displays,
  aFontStyle->size = size;

  pango_font_description_free(desc);
}

bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                                  gfxFontStyle& aFontStyle) {
  return mSystemTheme.GetFont(aID, aFontName, aFontStyle);
}

bool nsLookAndFeel::PerThemeData::GetFont(FontID aID, nsString& aFontName,
                                          gfxFontStyle& aFontStyle) const {
  switch (aID) {
    case FontID::Menu:             // css2
    case FontID::MozPullDownMenu:  // css3
      aFontName = mMenuFontName;
      aFontStyle = mMenuFontStyle;
      break;

    case FontID::MozField:  // css3
    case FontID::MozList:   // css3
      aFontName = mFieldFontName;
      aFontStyle = mFieldFontStyle;
      break;

    case FontID::MozButton:  // css3
      aFontName = mButtonFontName;
      aFontStyle = mButtonFontStyle;
      break;

    case FontID::Caption:       // css2
    case FontID::Icon:          // css2
    case FontID::MessageBox:    // css2
    case FontID::SmallCaption:  // css2
    case FontID::StatusBar:     // css2
    case FontID::MozWindow:     // css3
    case FontID::MozDocument:   // css3
    case FontID::MozWorkspace:  // css3
    case FontID::MozDesktop:    // css3
    case FontID::MozInfo:       // css3
    case FontID::MozDialog:     // css3
    default:
      aFontName = mDefaultFontName;
      aFontStyle = mDefaultFontStyle;
      break;
  }

  // Scale the font for the current monitor
  double scaleFactor = StaticPrefs::layout_css_devPixelsPerPx();
  if (scaleFactor > 0) {
    aFontStyle.size *=
        widget::ScreenHelperGTK::GetGTKMonitorScaleFactor() / scaleFactor;
  } else {
    // Convert gdk pixels to CSS pixels.
    aFontStyle.size /= gfxPlatformGtk::GetFontScaleFactor();
  }

  return true;
}

// Check color contrast according to
// https://www.w3.org/TR/AERT/#color-contrast
static bool HasGoodContrastVisibility(GdkRGBA& aColor1, GdkRGBA& aColor2) {
  int32_t luminosityDifference = NS_LUMINOSITY_DIFFERENCE(
      GDK_RGBA_TO_NS_RGBA(aColor1), GDK_RGBA_TO_NS_RGBA(aColor2));
  if (luminosityDifference < NS_SUFFICIENT_LUMINOSITY_DIFFERENCE) {
    return false;
  }

  double colorDifference = std::abs(aColor1.red - aColor2.red) +
                           std::abs(aColor1.green - aColor2.green) +
                           std::abs(aColor1.blue - aColor2.blue);
  return (colorDifference * 255.0 > 500.0);
}

// Check if the foreground/background colors match with default white/black
// html page colors.
static bool IsGtkThemeCompatibleWithHTMLColors() {
  GdkRGBA white = {1.0, 1.0, 1.0};
  GdkRGBA black = {0.0, 0.0, 0.0};

  GtkStyleContext* style = GetStyleContext(MOZ_GTK_WINDOW);

  GdkRGBA textColor;
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &textColor);

  // Theme text color and default white html page background
  if (!HasGoodContrastVisibility(textColor, white)) {
    return false;
  }

  GdkRGBA backgroundColor;
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL,
                                         &backgroundColor);

  // Theme background color and default white html page background
  if (HasGoodContrastVisibility(backgroundColor, white)) {
    return false;
  }

  // Theme background color and default black text color
  return HasGoodContrastVisibility(backgroundColor, black);
}

static nsCString GetGtkTheme() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  nsCString ret;
  GtkSettings* settings = gtk_settings_get_default();
  char* themeName = nullptr;
  g_object_get(settings, "gtk-theme-name", &themeName, nullptr);
  if (themeName) {
    ret.Assign(themeName);
    g_free(themeName);
  }
  return ret;
}

static bool GetPreferDarkTheme() {
  GtkSettings* settings = gtk_settings_get_default();
  gboolean preferDarkTheme = FALSE;
  g_object_get(settings, "gtk-application-prefer-dark-theme", &preferDarkTheme,
               nullptr);
  return preferDarkTheme == TRUE;
}

void nsLookAndFeel::ConfigureTheme(const LookAndFeelTheme& aTheme) {
  MOZ_ASSERT(XRE_IsContentProcess());
  GtkSettings* settings = gtk_settings_get_default();
  g_object_set(settings, "gtk-theme-name", aTheme.themeName().get(),
               "gtk-application-prefer-dark-theme",
               aTheme.preferDarkTheme() ? TRUE : FALSE, nullptr);
}

void nsLookAndFeel::WithThemeConfiguredForContent(
    const std::function<void(const LookAndFeelTheme&, bool)>& aFn) {
  nsWindow::WithSettingsChangesIgnored([&]() {
    // Available on Gtk 3.20+.
    static auto sGtkSettingsResetProperty =
        (void (*)(GtkSettings*, const gchar*))dlsym(
            RTLD_DEFAULT, "gtk_settings_reset_property");

    nsCString themeName;
    bool preferDarkTheme = false;

    if (!sGtkSettingsResetProperty) {
      // When gtk_settings_reset_property is not available, we instead
      // record the current theme name and variant and explicitly restore
      // them afterwards.  This means we won't respond to any subsequent
      // theme settings changes, which is unfortunate.  (It's possible we
      // could listen to xsettings changes and update the GtkSettings object
      // ourselves in response, if we wanted to fix this.)
      themeName = GetGtkTheme();
      preferDarkTheme = GetPreferDarkTheme();
    }

    bool changed = ConfigureContentGtkTheme();
    if (changed) {
      RefreshImpl();
    }

    LookAndFeelTheme theme;
    theme.themeName() = GetGtkTheme();
    theme.preferDarkTheme() = GetPreferDarkTheme();

    aFn(theme, changed);

    if (changed) {
      GtkSettings* settings = gtk_settings_get_default();
      if (sGtkSettingsResetProperty) {
        sGtkSettingsResetProperty(settings, "gtk-theme-name");
        sGtkSettingsResetProperty(settings,
                                  "gtk-application-prefer-dark-theme");
      } else {
        g_object_set(settings, "gtk-theme-name", themeName.get(),
                     "gtk-application-prefer-dark-theme",
                     preferDarkTheme ? TRUE : FALSE, nullptr);
      }
      RefreshImpl();
    }
  });
}

bool nsLookAndFeel::FromParentTheme(IntID aID) {
  switch (aID) {
    case IntID::SystemUsesDarkTheme:
    case IntID::UseAccessibilityTheme:
      // We want to take SystemUsesDarkTheme and UseAccessibilityTheme from the
      // parent process theme rather than the content configured theme.
      //
      // This ensures that media queries like (prefers-color-scheme: dark) will
      // match correctly in content processes.
      return true;
    default:
      return false;
  }
}

bool nsLookAndFeel::FromParentTheme(ColorID aID) {
  if (IsSelectionColorBackground(aID) || IsSelectionColorForeground(aID)) {
    return StaticPrefs::widget_content_allow_gtk_dark_theme_selection();
  }
  switch (aID) {
    case ColorID::ThemedScrollbar:
    case ColorID::ThemedScrollbarInactive:
    case ColorID::ThemedScrollbarThumb:
    case ColorID::ThemedScrollbarThumbHover:
    case ColorID::ThemedScrollbarThumbInactive:
      return StaticPrefs::widget_content_allow_gtk_dark_theme_scrollbar();
    case ColorID::ThemedScrollbarThumbActive:
      return StaticPrefs::
          widget_content_allow_gtk_dark_theme_scrollbar_active();
    case ColorID::MozAccentColor:
    case ColorID::MozAccentColorForeground:
      return StaticPrefs::widget_content_allow_gtk_dark_theme_accent();
    default:
      return false;
  }
}

bool nsLookAndFeel::ConfigureContentGtkTheme() {
  bool changed = false;

  GtkSettings* settings = gtk_settings_get_default();

  nsAutoCString themeOverride;
  mozilla::Preferences::GetCString("widget.content.gtk-theme-override",
                                   themeOverride);
  if (!themeOverride.IsEmpty()) {
    g_object_set(settings, "gtk-theme-name", themeOverride.get(), nullptr);
    changed = true;
    LOG(("ConfigureContentGtkTheme(%s)\n", themeOverride.get()));
  } else {
    LOG(("ConfigureContentGtkTheme(%s)\n", GetGtkTheme().get()));
  }

  // Dark theme is active but user explicitly enables it, or we're on
  // high-contrast (in which case we prevent content to mess up with the colors
  // of the page), so we're done now.
  if (!themeOverride.IsEmpty() || mSystemTheme.mHighContrast ||
      StaticPrefs::widget_content_allow_gtk_dark_theme()) {
    return changed;
  }

  // Try to select the light variant of the current theme first...
  if (GetPreferDarkTheme()) {
    LOG(("    disabling gtk-application-prefer-dark-theme\n"));
    g_object_set(settings, "gtk-application-prefer-dark-theme", FALSE, nullptr);
    changed = true;
  }

  // ...and use a default Gtk theme as a fallback.
  if (!IsGtkThemeCompatibleWithHTMLColors()) {
    LOG(("    Non-compatible dark theme, default to Adwaita\n"));
    g_object_set(settings, "gtk-theme-name", "Adwaita", nullptr);
    changed = true;
  }

  return changed;
}

static bool AnyColorChannelIsDifferent(nscolor aColor) {
  return NS_GET_R(aColor) != NS_GET_G(aColor) ||
         NS_GET_R(aColor) != NS_GET_B(aColor);
}

void nsLookAndFeel::EnsureInit() {
  if (mInitialized) {
    return;
  }

  // Gtk manages a screen's CSS in the settings object so we
  // ask Gtk to create it explicitly. Otherwise we may end up
  // with wrong color theme, see Bug 972382
  GtkSettings* settings = gtk_settings_get_default();
  if (MOZ_UNLIKELY(!settings)) {
    NS_WARNING("EnsureInit: No settings");
    return;
  }

  mInitialized = true;

  // gtk does non threadsafe refcounting
  MOZ_ASSERT(NS_IsMainThread());

  gboolean enableAnimations = false;
  g_object_get(settings, "gtk-enable-animations", &enableAnimations, nullptr);
  mPrefersReducedMotion = !enableAnimations;

  gint blink_time;
  gboolean blink;
  g_object_get(settings, "gtk-cursor-blink-time", &blink_time,
               "gtk-cursor-blink", &blink, nullptr);
  mCaretBlinkTime = blink ? (int32_t)blink_time : 0;

  mCSDAvailable =
      nsWindow::GtkWindowDecoration() != nsWindow::GTK_DECORATION_NONE;
  mCSDHideTitlebarByDefault = nsWindow::HideTitlebarByDefault();

  mSystemTheme.Init();

  mCSDCloseButton = false;
  mCSDMinimizeButton = false;
  mCSDMaximizeButton = false;
  mCSDCloseButtonPosition = 0;
  mCSDMinimizeButtonPosition = 0;
  mCSDMaximizeButtonPosition = 0;

  // We need to initialize whole CSD config explicitly because it's queried
  // as -moz-gtk* media features.
  ButtonLayout buttonLayout[TOOLBAR_BUTTONS];

  size_t activeButtons =
      GetGtkHeaderBarButtonLayout(Span(buttonLayout), &mCSDReversedPlacement);
  for (size_t i = 0; i < activeButtons; i++) {
    // We check if a button is represented on the right side of the tabbar.
    // Then we assign it a value from 3 to 5, instead of 0 to 2 when it is on
    // the left side.
    const ButtonLayout& layout = buttonLayout[i];
    int32_t* pos = nullptr;
    switch (layout.mType) {
      case MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE:
        mCSDMinimizeButton = true;
        pos = &mCSDMinimizeButtonPosition;
        break;
      case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE:
        mCSDMaximizeButton = true;
        pos = &mCSDMaximizeButtonPosition;
        break;
      case MOZ_GTK_HEADER_BAR_BUTTON_CLOSE:
        mCSDCloseButton = true;
        pos = &mCSDCloseButtonPosition;
        break;
      default:
        break;
    }

    if (pos) {
      *pos = i;
      if (layout.mAtRight) {
        *pos += TOOLBAR_BUTTONS;
      }
    }
  }

  RecordTelemetry();
}

void nsLookAndFeel::PerThemeData::Init() {
  mName = GetGtkTheme();

  GtkStyleContext* style;

  mHighContrast = StaticPrefs::widget_content_gtk_high_contrast_enabled() &&
                  GetGtkTheme().Find("HighContrast"_ns) >= 0;

  mPreferDarkTheme = GetPreferDarkTheme();

  // It seems GTK doesn't have an API to query if the current theme is
  // "light" or "dark", so we synthesize it from the CSS2 Window/WindowText
  // colors instead, by comparing their luminosity.
  {
    GdkRGBA bg, fg;
    style = GetStyleContext(MOZ_GTK_WINDOW);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &fg);
    mIsDark = (RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(bg)) <
               RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(fg)));
  }

  GdkRGBA color;
  // Colors that we pass to content processes through RemoteLookAndFeel.
  if (ShouldHonorThemeScrollbarColors()) {
    // Some themes style the <trough>, while others style the <scrollbar>
    // itself, so we look at both and compose the colors.
    style = GetStyleContext(MOZ_GTK_SCROLLBAR_VERTICAL);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL,
                                           &color);
    mThemedScrollbar = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_BACKDROP,
                                           &color);
    mThemedScrollbarInactive = GDK_RGBA_TO_NS_RGBA(color);

    style = GetStyleContext(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL,
                                           &color);
    mThemedScrollbar =
        NS_ComposeColors(mThemedScrollbar, GDK_RGBA_TO_NS_RGBA(color));
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_BACKDROP,
                                           &color);
    mThemedScrollbarInactive =
        NS_ComposeColors(mThemedScrollbarInactive, GDK_RGBA_TO_NS_RGBA(color));

    mMozScrollbar = mThemedScrollbar;

    style = GetStyleContext(MOZ_GTK_SCROLLBAR_THUMB_VERTICAL);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL,
                                           &color);
    mThemedScrollbarThumb = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_PRELIGHT,
                                           &color);
    mThemedScrollbarThumbHover = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_background_color(
        style, GtkStateFlags(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE),
        &color);
    mThemedScrollbarThumbActive = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_BACKDROP,
                                           &color);
    mThemedScrollbarThumbInactive = GDK_RGBA_TO_NS_RGBA(color);
  } else {
    mMozScrollbar = mThemedScrollbar = widget::sScrollbarColor.ToABGR();
    mThemedScrollbarInactive = widget::sScrollbarColor.ToABGR();
    mThemedScrollbarThumb = widget::sScrollbarThumbColor.ToABGR();
    mThemedScrollbarThumbHover =
        nsNativeBasicTheme::AdjustUnthemedScrollbarThumbColor(
            mThemedScrollbarThumb, NS_EVENT_STATE_HOVER);
    mThemedScrollbarThumbActive =
        nsNativeBasicTheme::AdjustUnthemedScrollbarThumbColor(
            mThemedScrollbarThumb, NS_EVENT_STATE_ACTIVE);
    mThemedScrollbarThumbInactive = mThemedScrollbarThumb;
  }

  // The label is not added to a parent widget, but shared for constructing
  // different style contexts.  The node hierarchy is constructed only on
  // the label style context.
  GtkWidget* labelWidget = gtk_label_new("M");
  g_object_ref_sink(labelWidget);

  // Window colors
  style = GetStyleContext(MOZ_GTK_WINDOW);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozWindowBackground = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozWindowText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_border_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozWindowActiveBorder = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_border_color(style, GTK_STATE_FLAG_INSENSITIVE, &color);
  mMozWindowInactiveBorder = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_INSENSITIVE,
                                         &color);
  mMozWindowInactiveCaption = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_WINDOW_CONTAINER);
  {
    GtkStyleContext* labelStyle = CreateStyleForWidget(labelWidget, style);
    GetSystemFontInfo(labelStyle, &mDefaultFontName, &mDefaultFontStyle);
    g_object_unref(labelStyle);
  }

  // tooltip foreground and background
  style = GetStyleContext(MOZ_GTK_TOOLTIP);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mInfoBackground = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mInfoText = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_MENUITEM);
  {
    GtkStyleContext* accelStyle =
        CreateStyleForWidget(gtk_accel_label_new("M"), style);

    GetSystemFontInfo(accelStyle, &mMenuFontName, &mMenuFontStyle);

    gtk_style_context_get_color(accelStyle, GTK_STATE_FLAG_NORMAL, &color);
    mMenuText = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(accelStyle, GTK_STATE_FLAG_INSENSITIVE, &color);
    mMenuTextInactive = GDK_RGBA_TO_NS_RGBA(color);
    g_object_unref(accelStyle);
  }

  style = GetStyleContext(MOZ_GTK_MENUPOPUP);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMenuBackground = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_MENUITEM);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_PRELIGHT,
                                         &color);
  mMenuHover = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
  mMenuHoverText = GDK_RGBA_TO_NS_RGBA(color);

  GtkWidget* parent = gtk_fixed_new();
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* treeView = gtk_tree_view_new();
  GtkWidget* linkButton = gtk_link_button_new("http://example.com/");
  GtkWidget* menuBar = gtk_menu_bar_new();
  GtkWidget* menuBarItem = gtk_menu_item_new();
  GtkWidget* entry = gtk_entry_new();
  GtkWidget* textView = gtk_text_view_new();

  gtk_container_add(GTK_CONTAINER(parent), treeView);
  gtk_container_add(GTK_CONTAINER(parent), linkButton);
  gtk_container_add(GTK_CONTAINER(parent), menuBar);
  gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), menuBarItem);
  gtk_container_add(GTK_CONTAINER(window), parent);
  gtk_container_add(GTK_CONTAINER(parent), entry);
  gtk_container_add(GTK_CONTAINER(parent), textView);

  // Text colors
  GdkRGBA bgColor;
  // If the text window background is translucent, then the background of
  // the textview root node is visible.
  style = GetStyleContext(MOZ_GTK_TEXT_VIEW);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL,
                                         &bgColor);

  style = GetStyleContext(MOZ_GTK_TEXT_VIEW_TEXT);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  ApplyColorOver(color, &bgColor);
  mFieldBackground = GDK_RGBA_TO_NS_RGBA(bgColor);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mFieldText = GDK_RGBA_TO_NS_RGBA(color);

  // Selected text and background
  {
    GtkStyleContext* selectionStyle =
        GetStyleContext(MOZ_GTK_TEXT_VIEW_TEXT_SELECTION);
    auto GrabSelectionColors = [&](GtkStyleContext* style) {
      gtk_style_context_get_background_color(
          style,
          static_cast<GtkStateFlags>(GTK_STATE_FLAG_FOCUSED |
                                     GTK_STATE_FLAG_SELECTED),
          &color);
      mTextSelectedBackground = GDK_RGBA_TO_NS_RGBA(color);
      gtk_style_context_get_color(
          style,
          static_cast<GtkStateFlags>(GTK_STATE_FLAG_FOCUSED |
                                     GTK_STATE_FLAG_SELECTED),
          &color);
      mTextSelectedText = GDK_RGBA_TO_NS_RGBA(color);
    };
    GrabSelectionColors(selectionStyle);
    if (mTextSelectedBackground == mTextSelectedText) {
      // Some old distros/themes don't properly use the .selection style, so
      // fall back to the regular text view style.
      GrabSelectionColors(style);
    }

    mAccentColor = mTextSelectedBackground;
    mAccentColorForeground = mTextSelectedText;

    // Accent is the darker of the selection background / foreground, unless the
    // foreground isn't really a color (is all white / black / gray) and the
    // background is, in which case we stick to what we have.
    if (RelativeLuminanceUtils::Compute(mAccentColor) >
            RelativeLuminanceUtils::Compute(mAccentColorForeground) &&
        (AnyColorChannelIsDifferent(mAccentColorForeground) ||
         !AnyColorChannelIsDifferent(mAccentColor))) {
      std::swap(mAccentColor, mAccentColorForeground);
    }

    // Blend with white, ensuring the color is opaque, so that the UI doesn't
    // have to care about alpha.
    mAccentColorForeground =
        NS_ComposeColors(NS_RGB(0xff, 0xff, 0xff), mAccentColorForeground);
    mAccentColor = NS_ComposeColors(NS_RGB(0xff, 0xff, 0xff), mAccentColor);
  }

  // Button text color
  style = GetStyleContext(MOZ_GTK_BUTTON);
  {
    GtkStyleContext* labelStyle = CreateStyleForWidget(labelWidget, style);
    GetSystemFontInfo(labelStyle, &mButtonFontName, &mButtonFontStyle);
    g_object_unref(labelStyle);
  }

  gtk_style_context_get_border_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mButtonDefault = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mButtonText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
  mButtonHoverText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_ACTIVE, &color);
  mButtonActiveText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_PRELIGHT,
                                         &color);
  mButtonHoverFace = GDK_RGBA_TO_NS_RGBA(color);
  if (!NS_GET_A(mButtonHoverFace)) {
    mButtonHoverFace = mMozWindowBackground;
  }

  // Combobox text color
  style = GetStyleContext(MOZ_GTK_COMBOBOX_ENTRY_TEXTAREA);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mComboBoxText = GDK_RGBA_TO_NS_RGBA(color);

  // Menubar text and hover text colors
  style = GetStyleContext(MOZ_GTK_MENUBARITEM);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMenuBarText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
  mMenuBarHoverText = GDK_RGBA_TO_NS_RGBA(color);

  // GTK's guide to fancy odd row background colors:
  // 1) Check if a theme explicitly defines an odd row color
  // 2) If not, check if it defines an even row color, and darken it
  //    slightly by a hardcoded value (gtkstyle.c)
  // 3) If neither are defined, take the base background color and
  //    darken that by a hardcoded value
  style = GetStyleContext(MOZ_GTK_TREEVIEW);

  // Get odd row background color
  gtk_style_context_save(style);
  gtk_style_context_add_region(style, GTK_STYLE_REGION_ROW, GTK_REGION_ODD);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mOddCellBackground = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_restore(style);

  // Column header colors
  style = GetStyleContext(MOZ_GTK_TREE_HEADER_CELL);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozColHeaderText = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
  mMozColHeaderHoverText = GDK_RGBA_TO_NS_RGBA(color);

  // Compute cell highlight colors
  InitCellHighlightColors();

  // GtkFrame has a "border" subnode on which Adwaita draws the border.
  // Some themes do not draw on this node but draw a border on the widget
  // root node, so check the root node if no border is found on the border
  // node.
  style = GetStyleContext(MOZ_GTK_FRAME_BORDER);
  bool themeUsesColors =
      GetBorderColors(style, &mFrameOuterLightBorder, &mFrameInnerDarkBorder);
  if (!themeUsesColors) {
    style = GetStyleContext(MOZ_GTK_FRAME);
    GetBorderColors(style, &mFrameOuterLightBorder, &mFrameInnerDarkBorder);
  }

  // GtkInfoBar
  // TODO - Use WidgetCache for it?
  GtkWidget* infoBar = gtk_info_bar_new();
  GtkWidget* infoBarContent =
      gtk_info_bar_get_content_area(GTK_INFO_BAR(infoBar));
  GtkWidget* infoBarLabel = gtk_label_new(nullptr);
  gtk_container_add(GTK_CONTAINER(parent), infoBar);
  gtk_container_add(GTK_CONTAINER(infoBarContent), infoBarLabel);
  style = gtk_widget_get_style_context(infoBarLabel);
  gtk_style_context_add_class(style, GTK_STYLE_CLASS_INFO);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mInfoBarText = GDK_RGBA_TO_NS_RGBA(color);
  // Some themes have a unified menu bar, and support window dragging on it
  gboolean supports_menubar_drag = FALSE;
  GParamSpec* param_spec = gtk_widget_class_find_style_property(
      GTK_WIDGET_GET_CLASS(menuBar), "window-dragging");
  if (param_spec) {
    if (g_type_is_a(G_PARAM_SPEC_VALUE_TYPE(param_spec), G_TYPE_BOOLEAN)) {
      gtk_widget_style_get(menuBar, "window-dragging", &supports_menubar_drag,
                           nullptr);
    }
  }
  mMenuSupportsDrag = supports_menubar_drag;

  // TODO: It returns wrong color for themes which
  // sets link color for GtkLabel only as we query
  // GtkLinkButton style here.
  style = gtk_widget_get_style_context(linkButton);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_LINK, &color);
  mNativeHyperLinkText = GDK_RGBA_TO_NS_RGBA(color);

  // invisible character styles
  guint value;
  g_object_get(entry, "invisible-char", &value, nullptr);
  mInvisibleCharacter = char16_t(value);

  // caret styles
  gtk_widget_style_get(entry, "cursor-aspect-ratio", &mCaretRatio, nullptr);

  GetSystemFontInfo(gtk_widget_get_style_context(entry), &mFieldFontName,
                    &mFieldFontStyle);

  gtk_widget_destroy(window);
  g_object_unref(labelWidget);
}

// virtual
char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
  EnsureInit();
  return mSystemTheme.mInvisibleCharacter;
}

bool nsLookAndFeel::GetEchoPasswordImpl() { return false; }

bool nsLookAndFeel::WidgetUsesImage(WidgetNodeType aNodeType) {
  static constexpr GtkStateFlags sFlagsToCheck[]{
      GTK_STATE_FLAG_NORMAL, GTK_STATE_FLAG_PRELIGHT,
      GtkStateFlags(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE),
      GTK_STATE_FLAG_BACKDROP, GTK_STATE_FLAG_INSENSITIVE};

  GtkStyleContext* style = GetStyleContext(aNodeType);

  GValue value = G_VALUE_INIT;
  for (GtkStateFlags state : sFlagsToCheck) {
    gtk_style_context_get_property(style, "background-image", state, &value);
    bool hasPattern = G_VALUE_TYPE(&value) == CAIRO_GOBJECT_TYPE_PATTERN &&
                      g_value_get_boxed(&value);
    g_value_unset(&value);
    if (hasPattern) {
      return true;
    }
  }
  return false;
}

void nsLookAndFeel::RecordLookAndFeelSpecificTelemetry() {
  // Gtk version we're on.
  nsString version;
  version.AppendPrintf("%d.%d", gtk_major_version, gtk_minor_version);
  Telemetry::ScalarSet(Telemetry::ScalarID::WIDGET_GTK_VERSION, version);

  // Whether the current Gtk theme has scrollbar buttons.
  bool hasScrollbarButtons =
      GetInt(LookAndFeel::IntID::ScrollArrowStyle) != eScrollArrow_None;
  mozilla::Telemetry::ScalarSet(
      mozilla::Telemetry::ScalarID::WIDGET_GTK_THEME_HAS_SCROLLBAR_BUTTONS,
      hasScrollbarButtons);

  // Whether the current Gtk theme uses something other than a solid color
  // background for scrollbar parts.
  bool scrollbarUsesImage =
      WidgetUsesImage(MOZ_GTK_SCROLLBAR_VERTICAL) ||
      WidgetUsesImage(MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL) ||
      WidgetUsesImage(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL) ||
      WidgetUsesImage(MOZ_GTK_SCROLLBAR_THUMB_VERTICAL);
  mozilla::Telemetry::ScalarSet(
      mozilla::Telemetry::ScalarID::WIDGET_GTK_THEME_SCROLLBAR_USES_IMAGES,
      scrollbarUsesImage);
}

bool nsLookAndFeel::ShouldHonorThemeScrollbarColors() {
  // If the Gtk theme uses anything other than solid color backgrounds for Gtk
  // scrollbar parts, this is a good indication that painting XUL scrollbar part
  // elements using colors extracted from the theme won't provide good results.
  return !WidgetUsesImage(MOZ_GTK_SCROLLBAR_VERTICAL) &&
         !WidgetUsesImage(MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL) &&
         !WidgetUsesImage(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL) &&
         !WidgetUsesImage(MOZ_GTK_SCROLLBAR_THUMB_VERTICAL);
}
