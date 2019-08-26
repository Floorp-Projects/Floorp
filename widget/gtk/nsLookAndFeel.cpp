/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "ScreenHelperGTK.h"

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

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor)NS_RGB(c.red >> 8, c.green >> 8, c.blue >> 8))
#define GDK_RGBA_TO_NS_RGBA(c)                                    \
  ((nscolor)NS_RGBA((int)((c).red * 255), (int)((c).green * 255), \
                    (int)((c).blue * 255), (int)((c).alpha * 255)))

#if !GTK_CHECK_VERSION(3, 12, 0)
#  define GTK_STATE_FLAG_LINK (static_cast<GtkStateFlags>(1 << 9))
#endif

nsLookAndFeel::nsLookAndFeel() = default;

nsLookAndFeel::~nsLookAndFeel() {}

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
nsresult nsLookAndFeel::InitCellHighlightColors() {
  // NS_SUFFICIENT_LUMINOSITY_DIFFERENCE is the a11y standard for text
  // on a background. Use 20% of that standard since we have a background
  // on top of another background
  int32_t minLuminosityDifference = NS_SUFFICIENT_LUMINOSITY_DIFFERENCE / 5;
  int32_t backLuminosityDifference =
      NS_LUMINOSITY_DIFFERENCE(mMozWindowBackground, mMozFieldBackground);
  if (backLuminosityDifference >= minLuminosityDifference) {
    mMozCellHighlightBackground = mMozWindowBackground;
    mMozCellHighlightText = mMozWindowText;
    return NS_OK;
  }

  uint16_t hue, sat, luminance;
  uint8_t alpha;
  mMozCellHighlightBackground = mMozFieldBackground;
  mMozCellHighlightText = mMozFieldText;

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
  return NS_OK;
}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

void nsLookAndFeel::RefreshImpl() {
  nsXPLookAndFeel::RefreshImpl();
  moz_gtk_refresh();

  mDefaultFontCached = false;
  mButtonFontCached = false;
  mFieldFontCached = false;
  mMenuFontCached = false;

  mInitialized = false;
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor) {
  EnsureInit();

  nsresult res = NS_OK;

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
    case ColorID::WidgetSelectForeground:
    case ColorID::TextSelectForeground:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::Highlighttext:
    case ColorID::MozHtmlCellhighlighttext:
      aColor = mTextSelectedText;
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

    case ColorID::Threedlightshadow:
      // 3-D highlighted inner edge color
      // always same as background in GTK code
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

    case ColorID::Threeddarkshadow:
      // Hardcode to black
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;

    case ColorID::MozEventreerow:
    case ColorID::MozField:
      aColor = mMozFieldBackground;
      break;
    case ColorID::MozFieldtext:
      aColor = mMozFieldText;
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

nsresult nsLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  nsresult res = NS_OK;

  // Set these before they can get overrided in the nsXPLookAndFeel.
  switch (aID) {
    case eIntID_ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      return NS_OK;
    case eIntID_ScrollButtonMiddleMouseButtonAction:
      aResult = 1;
      return NS_OK;
    case eIntID_ScrollButtonRightMouseButtonAction:
      aResult = 2;
      return NS_OK;
    default:
      break;
  }

  res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
  res = NS_OK;

  // We use delayed initialization by EnsureInit() here
  // to make sure mozilla::Preferences is available (Bug 115807).
  // eIntID_UseAccessibilityTheme is requested before user preferences
  // are read, and so EnsureInit(), which depends on preference values,
  // is deliberately delayed until required.
  switch (aID) {
    case eIntID_CaretBlinkTime:
      EnsureInit();
      aResult = mCaretBlinkTime;
      break;
    case eIntID_CaretWidth:
      aResult = 1;
      break;
    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;
    case eIntID_SelectTextfieldsOnKeyFocus: {
      GtkWidget* entry;
      GtkSettings* settings;
      gboolean select_on_focus;

      entry = gtk_entry_new();
      g_object_ref_sink(entry);
      settings = gtk_widget_get_settings(entry);
      g_object_get(settings, "gtk-entry-select-on-focus", &select_on_focus,
                   nullptr);

      if (select_on_focus)
        aResult = 1;
      else
        aResult = 0;

      gtk_widget_destroy(entry);
      g_object_unref(entry);
    } break;
    case eIntID_ScrollToClick: {
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
    case eIntID_SubmenuDelay: {
      GtkSettings* settings;
      gint delay;

      settings = gtk_settings_get_default();
      g_object_get(settings, "gtk-menu-popup-delay", &delay, nullptr);
      aResult = (int32_t)delay;
      break;
    }
    case eIntID_TooltipDelay: {
      aResult = 500;
      break;
    }
    case eIntID_MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;
    case eIntID_SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case eIntID_DragThresholdX:
    case eIntID_DragThresholdY: {
      GtkWidget* box = gtk_hbox_new(FALSE, 5);
      gint threshold = 0;
      g_object_get(gtk_widget_get_settings(box), "gtk-dnd-drag-threshold",
                   &threshold, nullptr);
      g_object_ref_sink(box);

      aResult = threshold;
    } break;
    case eIntID_ScrollArrowStyle: {
      GtkWidget* scrollbar = GetWidget(MOZ_GTK_SCROLLBAR_HORIZONTAL);
      aResult = ConvertGTKStepperStyleToMozillaScrollArrowStyle(scrollbar);
      break;
    }
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
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_TouchEnabled:
      aResult = mozilla::widget::WidgetUtils::IsTouchDeviceSupportPresent();
      break;
    case eIntID_MacGraphiteTheme:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
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
    case eIntID_MenuBarDrag:
      EnsureInit();
      aResult = mMenuSupportsDrag;
      break;
    case eIntID_ScrollbarButtonAutoRepeatBehavior:
      aResult = 1;
      break;
    case eIntID_SwipeAnimationEnabled:
      aResult = 0;
      break;
    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 2;
      break;
    case eIntID_GTKCSDAvailable:
      EnsureInit();
      aResult = mCSDAvailable;
      break;
    case eIntID_GTKCSDHideTitlebarByDefault:
      EnsureInit();
      aResult = mCSDHideTitlebarByDefault;
      break;
    case eIntID_GTKCSDMaximizeButton:
      EnsureInit();
      aResult = mCSDMaximizeButton;
      break;
    case eIntID_GTKCSDMinimizeButton:
      EnsureInit();
      aResult = mCSDMinimizeButton;
      break;
    case eIntID_GTKCSDCloseButton:
      EnsureInit();
      aResult = mCSDCloseButton;
      break;
    case eIntID_GTKCSDTransparentBackground: {
      // Enable transparent titlebar corners for titlebar mode.
      GdkScreen* screen = gdk_screen_get_default();
      aResult = gdk_screen_is_composited(screen)
                    ? (nsWindow::GetSystemCSDSupportLevel() !=
                       nsWindow::CSD_SUPPORT_NONE)
                    : false;
      break;
    }
    case eIntID_GTKCSDReversedPlacement:
      EnsureInit();
      aResult = mCSDReversedPlacement;
      break;
    case eIntID_PrefersReducedMotion: {
      GtkSettings* settings;
      gboolean enableAnimations;

      settings = gtk_settings_get_default();
      g_object_get(settings, "gtk-enable-animations", &enableAnimations,
                   nullptr);
      aResult = enableAnimations ? 0 : 1;
      break;
    }
    case eIntID_SystemUsesDarkTheme: {
      EnsureInit();
      aResult = mSystemUsesDarkTheme;
      break;
    }
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

nsresult nsLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  nsresult res = NS_OK;
  res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case eFloatID_CaretAspectRatio:
      EnsureInit();
      aResult = mCaretRatio;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }
  return res;
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

  NS_NAMED_LITERAL_STRING(quote, "\"");
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

bool nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                                gfxFontStyle& aFontStyle) {
  switch (aID) {
    case eFont_Menu:          // css2
    case eFont_PullDownMenu:  // css3
      aFontName = mMenuFontName;
      aFontStyle = mMenuFontStyle;
      break;

    case eFont_Field:  // css3
    case eFont_List:   // css3
      aFontName = mFieldFontName;
      aFontStyle = mFieldFontStyle;
      break;

    case eFont_Button:  // css3
      aFontName = mButtonFontName;
      aFontStyle = mButtonFontStyle;
      break;

    case eFont_Caption:       // css2
    case eFont_Icon:          // css2
    case eFont_MessageBox:    // css2
    case eFont_SmallCaption:  // css2
    case eFont_StatusBar:     // css2
    case eFont_Window:        // css3
    case eFont_Document:      // css3
    case eFont_Workspace:     // css3
    case eFont_Desktop:       // css3
    case eFont_Info:          // css3
    case eFont_Dialog:        // css3
    case eFont_Tooltips:      // moz
    case eFont_Widget:        // moz
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

static void ConfigureContentGtkTheme() {
  GtkSettings* settings = gtk_settings_get_for_screen(gdk_screen_get_default());
  nsAutoCString contentThemeName;
  mozilla::Preferences::GetCString("widget.content.gtk-theme-override",
                                   contentThemeName);
  if (!contentThemeName.IsEmpty()) {
    g_object_set(settings, "gtk-theme-name", contentThemeName.get(), nullptr);
  }

  // Dark theme is active but user explicitly enables it so we're done now.
  if (mozilla::Preferences::GetBool("widget.content.allow-gtk-dark-theme",
                                    false)) {
    return;
  }

  // Try to disable 'gtk-application-prefer-dark-theme' first...
  const gchar* dark_theme_setting = "gtk-application-prefer-dark-theme";
  gboolean darkThemeDefault;
  g_object_get(settings, dark_theme_setting, &darkThemeDefault, nullptr);
  if (darkThemeDefault) {
    g_object_set(settings, dark_theme_setting, FALSE, nullptr);
  }

  // ...and use a default Gtk theme as a fallback.
  if (contentThemeName.IsEmpty() && !IsGtkThemeCompatibleWithHTMLColors()) {
    g_object_set(settings, "gtk-theme-name", "Adwaita", nullptr);
  }
}

void nsLookAndFeel::EnsureInit() {
  GdkColor colorValue;
  GdkColor* colorValuePtr;

  if (mInitialized) {
    return;
  }

  // Gtk manages a screen's CSS in the settings object so we
  // ask Gtk to create it explicitly. Otherwise we may end up
  // with wrong color theme, see Bug 972382
  GdkScreen* screen = gdk_screen_get_default();
  if (MOZ_UNLIKELY(!screen)) {
    NS_WARNING("EnsureInit: No screen");
    return;
  }
  GtkSettings* settings = gtk_settings_get_for_screen(screen);
  if (MOZ_UNLIKELY(!settings)) {
    NS_WARNING("EnsureInit: No settings");
    return;
  }

  mInitialized = true;

  // gtk does non threadsafe refcounting
  MOZ_ASSERT(NS_IsMainThread());

  GdkRGBA color;
  GtkStyleContext* style;

  // It seems GTK doesn't have an API to query if the current theme is
  // "light" or "dark", so we synthesize it from the CSS2 Window/WindowText
  // colors instead, by comparing their luminosity.
  GdkRGBA bg, fg;
  style = GetStyleContext(MOZ_GTK_WINDOW);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &bg);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &fg);
  mSystemUsesDarkTheme =
      (RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(bg)) <
       RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(fg)));

  if (XRE_IsContentProcess()) {
    // Dark themes interacts poorly with widget styling (see bug 1216658).
    // We disable dark themes by default for web content
    // but allow user to overide it by prefs.
    ConfigureContentGtkTheme();
  }

  // The label is not added to a parent widget, but shared for constructing
  // different style contexts.  The node hierarchy is constructed only on
  // the label style context.
  GtkWidget* labelWidget = gtk_label_new("M");
  g_object_ref_sink(labelWidget);

  // Scrollbar colors
  style = GetStyleContext(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozScrollbar = GDK_RGBA_TO_NS_RGBA(color);

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
  mMozFieldBackground = GDK_RGBA_TO_NS_RGBA(bgColor);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozFieldText = GDK_RGBA_TO_NS_RGBA(color);

  // Selected text and background
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

  if (gtk_check_version(3, 12, 0) == nullptr) {
    // TODO: It returns wrong color for themes which
    // sets link color for GtkLabel only as we query
    // GtkLinkButton style here.
    style = gtk_widget_get_style_context(linkButton);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_LINK, &color);
    mNativeHyperLinkText = GDK_RGBA_TO_NS_RGBA(color);
  } else {
    colorValuePtr = nullptr;
    gtk_widget_style_get(linkButton, "link-color", &colorValuePtr, nullptr);
    if (colorValuePtr) {
      colorValue = *colorValuePtr;  // we can't pass deref pointers to
                                    // GDK_COLOR_TO_NS_RGB
      mNativeHyperLinkText = GDK_COLOR_TO_NS_RGB(colorValue);
      gdk_color_free(colorValuePtr);
    } else {
      mNativeHyperLinkText = NS_RGB(0x00, 0x00, 0xEE);
    }
  }

  // invisible character styles
  guint value;
  g_object_get(entry, "invisible-char", &value, nullptr);
  mInvisibleCharacter = char16_t(value);

  // caret styles
  gtk_widget_style_get(entry, "cursor-aspect-ratio", &mCaretRatio, nullptr);

  gint blink_time;
  gboolean blink;
  g_object_get(settings, "gtk-cursor-blink-time", &blink_time,
               "gtk-cursor-blink", &blink, nullptr);
  mCaretBlinkTime = blink ? (int32_t)blink_time : 0;

  GetSystemFontInfo(gtk_widget_get_style_context(entry), &mFieldFontName,
                    &mFieldFontStyle);

  gtk_widget_destroy(window);
  g_object_unref(labelWidget);

  mCSDAvailable =
      nsWindow::GetSystemCSDSupportLevel() != nsWindow::CSD_SUPPORT_NONE;
  mCSDHideTitlebarByDefault = nsWindow::HideTitlebarByDefault();

  mCSDCloseButton = false;
  mCSDMinimizeButton = false;
  mCSDMaximizeButton = false;

  // We need to initialize whole CSD config explicitly because it's queried
  // as -moz-gtk* media features.
  WidgetNodeType buttonLayout[TOOLBAR_BUTTONS];

  int activeButtons = GetGtkHeaderBarButtonLayout(buttonLayout, TOOLBAR_BUTTONS,
                                                  &mCSDReversedPlacement);
  for (int i = 0; i < activeButtons; i++) {
    switch (buttonLayout[i]) {
      case MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE:
        mCSDMinimizeButton = true;
        break;
      case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE:
        mCSDMaximizeButton = true;
        break;
      case MOZ_GTK_HEADER_BAR_BUTTON_CLOSE:
        mCSDCloseButton = true;
        break;
      default:
        break;
    }
  }
}

// virtual
char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
  EnsureInit();
  return mInvisibleCharacter;
}

bool nsLookAndFeel::GetEchoPasswordImpl() { return false; }
