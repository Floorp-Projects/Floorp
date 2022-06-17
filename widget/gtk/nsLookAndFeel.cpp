/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// for strtod()
#include <stdlib.h>
#include <dlfcn.h>

#include "nsLookAndFeel.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <pango/pango.h>
#include <pango/pango-fontmap.h>
#include <fontconfig/fontconfig.h>

#include "GRefPtr.h"
#include "GUniquePtr.h"
#include "nsGtkUtils.h"
#include "gfxPlatformGtk.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "ScreenHelperGTK.h"
#include "ScrollbarDrawing.h"

#include "gtkdrawing.h"
#include "nsStyleConsts.h"
#include "gfxFontConstants.h"
#include "WidgetUtils.h"
#include "nsWindow.h"

#include "mozilla/gfx/2D.h"

#include <cairo-gobject.h>
#include <dlfcn.h>
#include "WidgetStyleCache.h"
#include "prenv.h"
#include "nsCSSColorUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
static LazyLogModule gLnfLog("LookAndFeel");
#  define LOGLNF(...) MOZ_LOG(gLnfLog, LogLevel::Debug, (__VA_ARGS__))
#  define LOGLNF_ENABLED() MOZ_LOG_TEST(gLnfLog, LogLevel::Debug)
#else
#  define LOGLNF(args)
#  define LOGLNF_ENABLED() false
#endif /* MOZ_LOGGING */

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor)NS_RGB(c.red >> 8, c.green >> 8, c.blue >> 8))
#define GDK_RGBA_TO_NS_RGBA(c)                                    \
  ((nscolor)NS_RGBA((int)((c).red * 255), (int)((c).green * 255), \
                    (int)((c).blue * 255), (int)((c).alpha * 255)))

static bool sIgnoreChangedSettings = false;

static void OnSettingsChange() {
  if (sIgnoreChangedSettings) {
    return;
  }
  // TODO: We could be more granular here, but for now assume everything
  // changed.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::StyleAndLayout);
  widget::IMContextWrapper::OnThemeChanged();
}

static void settings_changed_cb(GtkSettings*, GParamSpec*, void*) {
  OnSettingsChange();
}

static bool sCSDAvailable;

static nsCString GVariantToString(GVariant* aVariant) {
  nsCString ret;
  gchar* s = g_variant_print(aVariant, TRUE);
  if (s) {
    ret.Assign(s);
    g_free(s);
  }
  return ret;
}

static nsDependentCString GVariantGetString(GVariant* aVariant) {
  gsize len = 0;
  const gchar* v = g_variant_get_string(aVariant, &len);
  return nsDependentCString(v, len);
}

// Observed settings for portal.
static constexpr struct {
  nsLiteralCString mNamespace;
  nsLiteralCString mKey;
} kObservedSettings[] = {
    {"org.freedesktop.appearance"_ns, "color-scheme"_ns},
};

static void settings_changed_signal_cb(GDBusProxy* proxy, gchar* sender_name,
                                       gchar* signal_name, GVariant* parameters,
                                       gpointer user_data) {
  LOGLNF("Settings Change sender=%s signal=%s params=%s\n", sender_name,
         signal_name, GVariantToString(parameters).get());
  if (strcmp(signal_name, "SettingChanged")) {
    NS_WARNING("Unknown change signal for settings");
    return;
  }
  RefPtr<GVariant> ns = dont_AddRef(g_variant_get_child_value(parameters, 0));
  RefPtr<GVariant> key = dont_AddRef(g_variant_get_child_value(parameters, 1));
  // Third parameter is the value, but we don't care about it.
  if (!ns || !key || !g_variant_is_of_type(ns, G_VARIANT_TYPE_STRING) ||
      !g_variant_is_of_type(key, G_VARIANT_TYPE_STRING)) {
    MOZ_ASSERT(false, "Unexpected setting change signal parameters");
    return;
  }

  auto nsStr = GVariantGetString(ns);
  auto keyStr = GVariantGetString(key);
  for (const auto& setting : kObservedSettings) {
    if (setting.mNamespace.Equals(nsStr) && setting.mKey.Equals(keyStr)) {
      OnSettingsChange();
      return;
    }
  }
}

nsLookAndFeel::nsLookAndFeel() {
  static constexpr nsLiteralCString kObservedSettings[] = {
      // Affects system font sizes.
      "notify::gtk-xft-dpi"_ns,
      // Affects mSystemTheme and mAltTheme as expected.
      "notify::gtk-theme-name"_ns,
      // System fonts?
      "notify::gtk-font-name"_ns,
      // prefers-reduced-motion
      "notify::gtk-enable-animations"_ns,
      // CSD media queries, etc.
      "notify::gtk-decoration-layout"_ns,
      // Text resolution affects system font and widget sizes.
      "notify::resolution"_ns,
      // These three Affect mCaretBlinkTime
      "notify::gtk-cursor-blink"_ns,
      "notify::gtk-cursor-blink-time"_ns,
      "notify::gtk-cursor-blink-timeout"_ns,
      // Affects SelectTextfieldsOnKeyFocus
      "notify::gtk-entry-select-on-focus"_ns,
      // Affects ScrollToClick
      "notify::gtk-primary-button-warps-slider"_ns,
      // Affects SubmenuDelay
      "notify::gtk-menu-popup-delay"_ns,
      // Affects DragThresholdX/Y
      "notify::gtk-dnd-drag-threshold"_ns,
  };

  GtkSettings* settings = gtk_settings_get_default();
  for (const auto& setting : kObservedSettings) {
    g_signal_connect_after(settings, setting.get(),
                           G_CALLBACK(settings_changed_cb), nullptr);
  }

  sCSDAvailable =
      nsWindow::GetSystemGtkWindowDecoration() != nsWindow::GTK_DECORATION_NONE;

  if (ShouldUsePortal(PortalKind::Settings)) {
    GUniquePtr<GError> error;
    mDBusSettingsProxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings", nullptr, getter_Transfers(error)));
    if (mDBusSettingsProxy) {
      g_signal_connect(mDBusSettingsProxy, "g-signal",
                       G_CALLBACK(settings_changed_signal_cb), nullptr);
    } else {
      LOGLNF("Can't create DBus proxy for settings: %s\n", error->message);
    }
  }
}

nsLookAndFeel::~nsLookAndFeel() {
  if (mDBusSettingsProxy) {
    g_signal_handlers_disconnect_by_func(
        mDBusSettingsProxy, FuncToGpointer(settings_changed_signal_cb),
        nullptr);
    mDBusSettingsProxy = nullptr;
  }
  g_signal_handlers_disconnect_by_func(
      gtk_settings_get_default(), FuncToGpointer(settings_changed_cb), nullptr);
}

#if 0
static void DumpStyleContext(GtkStyleContext* aStyle) {
  static auto sGtkStyleContextToString =
      reinterpret_cast<char* (*)(GtkStyleContext*, gint)>(
          dlsym(RTLD_DEFAULT, "gtk_style_context_to_string"));
  char* str = sGtkStyleContextToString(aStyle, ~0);
  printf("%s\n", str);
  g_free(str);
  str = gtk_widget_path_to_string(gtk_style_context_get_path(aStyle));
  printf("%s\n", str);
  g_free(str);
}
#endif

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
  if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN)) {
    return false;
  }

  auto pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
  if (!pattern) {
    return false;
  }

  // Just picking the lightest and darkest colors as simple samples rather
  // than trying to blend, which could get messy if there are many stops.
  if (CAIRO_STATUS_SUCCESS !=
      cairo_pattern_get_color_stop_rgba(pattern, 0, nullptr, &aDarkColor->red,
                                        &aDarkColor->green, &aDarkColor->blue,
                                        &aDarkColor->alpha)) {
    return false;
  }

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

static bool GetColorFromImagePattern(const GValue* aValue, nscolor* aColor) {
  if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN)) {
    return false;
  }

  auto pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
  if (!pattern) {
    return false;
  }

  cairo_surface_t* surface;
  if (cairo_pattern_get_surface(pattern, &surface) != CAIRO_STATUS_SUCCESS) {
    return false;
  }

  cairo_format_t format = cairo_image_surface_get_format(surface);
  if (format == CAIRO_FORMAT_INVALID) {
    return false;
  }
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);
  int stride = cairo_image_surface_get_stride(surface);
  if (!width || !height) {
    return false;
  }

  // Guesstimate the central pixel would have a sensible color.
  int x = width / 2;
  int y = height / 2;

  unsigned char* data = cairo_image_surface_get_data(surface);
  switch (format) {
    // Most (all?) GTK images / patterns / etc use ARGB32.
    case CAIRO_FORMAT_ARGB32: {
      size_t offset = x * 4 + y * stride;
      uint32_t* pixel = reinterpret_cast<uint32_t*>(data + offset);
      *aColor = gfx::sRGBColor::UnusualFromARGB(*pixel).ToABGR();
      return true;
    }
    default:
      break;
  }

  return false;
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
  mInitialized = false;
  moz_gtk_refresh();

  nsXPLookAndFeel::RefreshImpl();
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme,
                                       nscolor& aColor) {
  EnsureInit();

  auto& theme = aScheme == ColorScheme::Light ? LightTheme() : DarkTheme();
  return theme.GetColor(aID, aColor);
}

static bool ShouldUseColorForActiveDarkScrollbarThumb(nscolor aColor) {
  auto IsDifferentEnough = [](int32_t aChannel, int32_t aOtherChannel) {
    return std::abs(aChannel - aOtherChannel) > 10;
  };
  return IsDifferentEnough(NS_GET_R(aColor), NS_GET_G(aColor)) ||
         IsDifferentEnough(NS_GET_R(aColor), NS_GET_B(aColor));
}

static bool ShouldUseThemedScrollbarColor(StyleSystemColor aID, nscolor aColor,
                                          bool aIsDark) {
  if (!aIsDark) {
    return true;
  }
  if (StaticPrefs::widget_non_native_theme_scrollbar_dark_themed()) {
    return true;
  }
  return aID == StyleSystemColor::ThemedScrollbarThumbActive &&
         StaticPrefs::widget_non_native_theme_scrollbar_active_always_themed();
}

nsresult nsLookAndFeel::PerThemeData::GetColor(ColorID aID,
                                               nscolor& aColor) const {
  nsresult res = NS_OK;

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // The CSS2 colors below are used.
    case ColorID::Appworkspace:  // MDI background color
    case ColorID::Background:    // desktop background
    case ColorID::Window:
    case ColorID::Windowframe:
    case ColorID::MozDialog:
    case ColorID::MozCombobox:
      aColor = mMozWindowBackground;
      break;
    case ColorID::Windowtext:
    case ColorID::MozDialogtext:
      aColor = mMozWindowText;
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::MozDragtargetzone:
    case ColorID::Highlight:  // preference selected item,
      aColor = mTextSelectedBackground;
      break;
    case ColorID::Highlighttext:
      if (NS_GET_A(mTextSelectedBackground) < 155) {
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
      }
      [[fallthrough]];
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      aColor = mTextSelectedText;
      break;
    case ColorID::Selecteditem:
      aColor = mSelectedItem;
      break;
    case ColorID::Selecteditemtext:
      aColor = mSelectedItemText;
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
    case ColorID::Scrollbar:
      aColor = mThemedScrollbar;
      break;
    case ColorID::ThemedScrollbar:
      aColor = mThemedScrollbar;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarInactive:
      aColor = mThemedScrollbarInactive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumb:
      aColor = mThemedScrollbarThumb;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbHover:
      aColor = mThemedScrollbarThumbHover;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbActive:
      aColor = mThemedScrollbarThumbActive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbInactive:
      aColor = mThemedScrollbarThumbInactive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
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
    case ColorID::Graytext:  // disabled text in windows, menus, etc.
      aColor = mMenuTextInactive;
      break;
    case ColorID::Activecaption:
      aColor = mTitlebarBackground;
      break;
    case ColorID::Captiontext:  // text in active window caption (titlebar)
      aColor = mTitlebarText;
      break;
    case ColorID::Inactivecaption:
      // inactive window caption
      aColor = mTitlebarInactiveBackground;
      break;
    case ColorID::Inactivecaptiontext:  // text in active window caption
                                        // (titlebar)
      aColor = mTitlebarInactiveText;
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
    case ColorID::Threedface:
    case ColorID::Buttonface:
    case ColorID::MozButtondisabledface:
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
    case ColorID::MozDisabledfield:
      aColor = mIsDark ? *GenericDarkColor(aID) : NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threeddarkshadow:
      aColor = mIsDark ? *GenericDarkColor(aID) : NS_RGB(0xDC, 0xDC, 0xDC);
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
    case ColorID::MozButtonactiveface:
      aColor = mButtonHoverFace;
      break;
    case ColorID::MozButtonhovertext:
      aColor = mButtonHoverText;
      break;
    case ColorID::MozButtonactivetext:
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
    case ColorID::MozNativevisitedhyperlinktext:
      aColor = mNativeVisitedHyperLinkText;
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
    case IntID::CaretBlinkCount:
      EnsureInit();
      aResult = mCaretBlinkCount;
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
      GtkWidget* scrollbar = GetWidget(MOZ_GTK_SCROLLBAR_VERTICAL);
      aResult = ConvertGTKStepperStyleToMozillaScrollArrowStyle(scrollbar);
      break;
    }
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
      aResult = sCSDAvailable;
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
      if (mColorSchemePreference) {
        aResult = *mColorSchemePreference == ColorScheme::Dark;
      } else {
        aResult = mSystemTheme.mIsDark;
      }
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
    case IntID::TitlebarRadius: {
      EnsureInit();
      aResult = EffectiveTheme().mTitlebarRadius;
      break;
    }
    case IntID::GtkMenuRadius: {
      EnsureInit();
      aResult = EffectiveTheme().mMenuRadius;
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
    case IntID::UseOverlayScrollbars: {
      aResult = StaticPrefs::widget_gtk_overlay_scrollbars_enabled();
      break;
    }
    case IntID::TouchDeviceSupportPresent:
      aResult = widget::WidgetUtilsGTK::IsTouchDeviceSupportPresent() ? 1 : 0;
      break;
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
  aFontStyle->style = FontSlantStyle::NORMAL;

  // As in
  // https://git.gnome.org/browse/gtk+/tree/gtk/gtkwidget.c?h=3.22.19#n10333
  PangoFontDescription* desc;
  gtk_style_context_get(aStyle, gtk_style_context_get_state(aStyle), "font",
                        &desc, nullptr);

  aFontStyle->systemFont = true;

  constexpr auto quote = u"\""_ns;
  NS_ConvertUTF8toUTF16 family(pango_font_description_get_family(desc));
  *aFontName = quote + family + quote;

  aFontStyle->weight =
      FontWeight::FromInt(pango_font_description_get_weight(desc));

  // FIXME: Set aFontStyle->stretch correctly!
  aFontStyle->stretch = FontStretch::NORMAL;

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

  // Convert gdk pixels to CSS pixels.
  aFontStyle.size /= gfxPlatformGtk::GetFontScaleFactor();
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

static nsCString GetGtkSettingsStringKey(const char* aKey) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  nsCString ret;
  GtkSettings* settings = gtk_settings_get_default();
  char* value = nullptr;
  g_object_get(settings, aKey, &value, nullptr);
  if (value) {
    ret.Assign(value);
    g_free(value);
  }
  return ret;
}

static nsCString GetGtkTheme() {
  return GetGtkSettingsStringKey("gtk-theme-name");
}

static bool GetPreferDarkTheme() {
  GtkSettings* settings = gtk_settings_get_default();
  gboolean preferDarkTheme = FALSE;
  g_object_get(settings, "gtk-application-prefer-dark-theme", &preferDarkTheme,
               nullptr);
  return preferDarkTheme == TRUE;
}

// It seems GTK doesn't have an API to query if the current theme is "light" or
// "dark", so we synthesize it from the CSS2 Window/WindowText colors instead,
// by comparing their luminosity.
static bool GetThemeIsDark() {
  GdkRGBA bg, fg;
  GtkStyleContext* style = GetStyleContext(MOZ_GTK_WINDOW);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &bg);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &fg);
  return RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(bg)) <
         RelativeLuminanceUtils::Compute(GDK_RGBA_TO_NS_RGBA(fg));
}

void nsLookAndFeel::ConfigureTheme(const LookAndFeelTheme& aTheme) {
  MOZ_ASSERT(XRE_IsContentProcess());
  GtkSettings* settings = gtk_settings_get_default();
  g_object_set(settings, "gtk-theme-name", aTheme.themeName().get(),
               "gtk-application-prefer-dark-theme",
               aTheme.preferDarkTheme() ? TRUE : FALSE, nullptr);
}

void nsLookAndFeel::RestoreSystemTheme() {
  LOGLNF("RestoreSystemTheme(%s, %d, %d)\n", mSystemTheme.mName.get(),
         mSystemTheme.mPreferDarkTheme, mSystemThemeOverridden);

  if (!mSystemThemeOverridden) {
    return;
  }

  // Available on Gtk 3.20+.
  static auto sGtkSettingsResetProperty =
      (void (*)(GtkSettings*, const gchar*))dlsym(
          RTLD_DEFAULT, "gtk_settings_reset_property");

  GtkSettings* settings = gtk_settings_get_default();
  if (sGtkSettingsResetProperty) {
    sGtkSettingsResetProperty(settings, "gtk-theme-name");
    sGtkSettingsResetProperty(settings, "gtk-application-prefer-dark-theme");
  } else {
    g_object_set(settings, "gtk-theme-name", mSystemTheme.mName.get(),
                 "gtk-application-prefer-dark-theme",
                 mSystemTheme.mPreferDarkTheme, nullptr);
  }
  moz_gtk_refresh();
  mSystemThemeOverridden = false;
}

static bool AnyColorChannelIsDifferent(nscolor aColor) {
  return NS_GET_R(aColor) != NS_GET_G(aColor) ||
         NS_GET_R(aColor) != NS_GET_B(aColor);
}

void nsLookAndFeel::ConfigureAndInitializeAltTheme() {
  GtkSettings* settings = gtk_settings_get_default();

  bool fellBackToDefaultTheme = false;

  // Try to select the opposite variant of the current theme first...
  LOGLNF("    toggling gtk-application-prefer-dark-theme\n");
  g_object_set(settings, "gtk-application-prefer-dark-theme",
               !mSystemTheme.mIsDark, nullptr);
  moz_gtk_refresh();

  // Toggling gtk-application-prefer-dark-theme is not enough generally to
  // switch from dark to light theme.  If the theme didn't change, and we have
  // a dark theme, try to first remove -Dark{,er,est} from the theme name to
  // find the light variant.
  if (mSystemTheme.mIsDark && mSystemTheme.mIsDark == GetThemeIsDark()) {
    nsCString potentialLightThemeName = mSystemTheme.mName;
    // clang-format off
    constexpr nsLiteralCString kSubstringsToRemove[] = {
        "-darkest"_ns, "-darker"_ns, "-dark"_ns,
        "-Darkest"_ns, "-Darker"_ns, "-Dark"_ns,
        "_darkest"_ns, "_darker"_ns, "_dark"_ns,
        "_Darkest"_ns, "_Darker"_ns, "_Dark"_ns,
    };
    // clang-format on
    bool found = false;
    for (auto& s : kSubstringsToRemove) {
      potentialLightThemeName = mSystemTheme.mName;
      potentialLightThemeName.ReplaceSubstring(s, ""_ns);
      if (potentialLightThemeName.Length() != mSystemTheme.mName.Length()) {
        found = true;
        break;
      }
    }
    if (found) {
      g_object_set(settings, "gtk-theme-name", potentialLightThemeName.get(),
                   nullptr);
      moz_gtk_refresh();
    }
  }

  if (mSystemTheme.mIsDark == GetThemeIsDark()) {
    // If the theme still didn't change enough, fall back to Adwaita with the
    // appropriate color preference.
    g_object_set(settings, "gtk-theme-name", "Adwaita",
                 "gtk-application-prefer-dark-theme", !mSystemTheme.mIsDark,
                 nullptr);
    moz_gtk_refresh();

    // If it _still_ didn't change enough, and we're dark, try to set
    // Adwaita-dark as a theme name. This might be needed in older GTK versions.
    if (!mSystemTheme.mIsDark && !GetThemeIsDark()) {
      g_object_set(settings, "gtk-theme-name", "Adwaita-dark", nullptr);
      moz_gtk_refresh();
    }

    fellBackToDefaultTheme = true;
  }

  mAltTheme.Init();

  // Some of the alt theme colors we can grab from the system theme, if we fell
  // back to the default light / dark themes.
  if (fellBackToDefaultTheme) {
    if (StaticPrefs::widget_gtk_alt_theme_selection()) {
      mAltTheme.mTextSelectedText = mSystemTheme.mTextSelectedText;
      mAltTheme.mTextSelectedBackground = mSystemTheme.mTextSelectedBackground;
    }

    if (StaticPrefs::widget_gtk_alt_theme_scrollbar_active() &&
        (!mAltTheme.mIsDark || ShouldUseColorForActiveDarkScrollbarThumb(
                                   mSystemTheme.mThemedScrollbarThumbActive))) {
      mAltTheme.mThemedScrollbarThumbActive =
          mSystemTheme.mThemedScrollbarThumbActive;
    }

    if (StaticPrefs::widget_gtk_alt_theme_accent()) {
      mAltTheme.mAccentColor = mSystemTheme.mAccentColor;
      mAltTheme.mAccentColorForeground = mSystemTheme.mAccentColorForeground;
    }
  }

  // Special case for Adwaita: In GTK3 we don't have more proper accent colors,
  // so we use the selected background colors. Those colors, however, don't have
  // much contrast in dark mode (see bug 1741293). We know, however, that GTK4
  // uses the light accent color for the dark theme, see:
  //
  //   https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/named-colors.html#accent-colors
  //
  // So we manually do that.
  if (mSystemTheme.mName.EqualsLiteral("Adwaita") ||
      mSystemTheme.mName.EqualsLiteral("Adwaita-dark")) {
    auto& dark = mSystemTheme.mIsDark ? mSystemTheme : mAltTheme;
    auto& light = mSystemTheme.mIsDark ? mAltTheme : mSystemTheme;

    dark.mAccentColor = light.mAccentColor;
    dark.mAccentColorForeground = light.mAccentColorForeground;
  }

  // Right now we're using the opposite color-scheme theme, make sure to record
  // it.
  mSystemThemeOverridden = true;
}

Maybe<ColorScheme> nsLookAndFeel::ComputeColorSchemeSetting() {
  {
    // Check the pref explicitly here. Usually this shouldn't be needed, but
    // since we can only load one GTK theme at a time, and the pref will
    // override the effective value that the rest of gecko assumes for the
    // "system" color scheme, we need to factor it in our GTK theme decisions.
    int32_t pref = 0;
    if (NS_SUCCEEDED(Preferences::GetInt("ui.systemUsesDarkTheme", &pref))) {
      return Some(pref ? ColorScheme::Dark : ColorScheme::Light);
    }
  }

  if (!mDBusSettingsProxy) {
    return Nothing();
  }
  GUniquePtr<GError> error;
  RefPtr<GVariant> variant = dont_AddRef(g_dbus_proxy_call_sync(
      mDBusSettingsProxy, "Read",
      g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"),
      G_DBUS_CALL_FLAGS_NONE,
      StaticPrefs::widget_gtk_settings_portal_timeout_ms(), nullptr,
      getter_Transfers(error)));
  if (!variant) {
    LOGLNF("color-scheme query error: %s\n", error->message);
    return Nothing();
  }
  LOGLNF("color-scheme query result: %s\n", GVariantToString(variant).get());
  variant = dont_AddRef(g_variant_get_child_value(variant, 0));
  while (variant && g_variant_is_of_type(variant, G_VARIANT_TYPE_VARIANT)) {
    // Unbox the return value.
    variant = dont_AddRef(g_variant_get_variant(variant));
  }
  if (!variant || !g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32)) {
    MOZ_ASSERT(false, "Unexpected color-scheme query return value");
    return Nothing();
  }
  switch (g_variant_get_uint32(variant)) {
    default:
      MOZ_FALLTHROUGH_ASSERT("Unexpected color-scheme query return value");
    case 0:
      break;
    case 1:
      return Some(ColorScheme::Dark);
    case 2:
      return Some(ColorScheme::Light);
  }
  return Nothing();
}

void nsLookAndFeel::Initialize() {
  LOGLNF("nsLookAndFeel::Initialize");
  MOZ_DIAGNOSTIC_ASSERT(!mInitialized);
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread(),
                        "LookAndFeel init should be done on the main thread");

  mInitialized = true;

  GtkSettings* settings = gtk_settings_get_default();
  if (MOZ_UNLIKELY(!settings)) {
    NS_WARNING("EnsureInit: No settings");
    return;
  }

  AutoRestore<bool> restoreIgnoreSettings(sIgnoreChangedSettings);
  sIgnoreChangedSettings = true;

  // Our current theme may be different from the system theme if we're matching
  // the Firefox theme or using the alt theme intentionally due to the
  // color-scheme preference. Make sure to restore the original system theme.
  RestoreSystemTheme();

  // First initialize global settings.
  InitializeGlobalSettings();

  // Record our system theme settings now.
  mSystemTheme.Init();

  // Find the alternative-scheme theme (light if the system theme is dark, or
  // vice versa), configure it and initialize it.
  ConfigureAndInitializeAltTheme();

  LOGLNF("System Theme: %s. Alt Theme: %s\n", mSystemTheme.mName.get(),
         mAltTheme.mName.get());

  // Go back to the system theme or keep the alt theme configured, depending on
  // Firefox theme or user color-scheme preference.
  ConfigureFinalEffectiveTheme();

  RecordTelemetry();
}

void nsLookAndFeel::InitializeGlobalSettings() {
  GtkSettings* settings = gtk_settings_get_default();

  mColorSchemePreference = ComputeColorSchemeSetting();

  gboolean enableAnimations = false;
  g_object_get(settings, "gtk-enable-animations", &enableAnimations, nullptr);
  mPrefersReducedMotion = !enableAnimations;

  gint blink_time = 0;     // In milliseconds
  gint blink_timeout = 0;  // in seconds
  gboolean blink;
  g_object_get(settings, "gtk-cursor-blink-time", &blink_time,
               "gtk-cursor-blink-timeout", &blink_timeout, "gtk-cursor-blink",
               &blink, nullptr);
  // From
  // https://docs.gtk.org/gtk3/property.Settings.gtk-cursor-blink-timeout.html:
  //
  //     Setting this to zero has the same effect as setting
  //     GtkSettings:gtk-cursor-blink to FALSE.
  //
  mCaretBlinkTime = blink && blink_timeout ? (int32_t)blink_time : 0;

  if (mCaretBlinkTime) {
    // blink_time * 2 because blink count is a full blink cycle.
    mCaretBlinkCount =
        std::max(1, int32_t(std::ceil(float(blink_timeout * 1000) /
                                      (float(blink_time) * 2.0f))));
  } else {
    mCaretBlinkCount = -1;
  }

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
    }
  }
}

void nsLookAndFeel::ConfigureFinalEffectiveTheme() {
  MOZ_ASSERT(mSystemThemeOverridden,
             "By this point, the alt theme should be configured");

  const bool shouldUseSystemTheme = [&] {
    // NOTE: We can't call ColorSchemeForChrome directly because this might run
    // while we're computing it.
    switch (ColorSchemeSettingForChrome()) {
      case ChromeColorSchemeSetting::Light:
        return !mSystemTheme.mIsDark;
      case ChromeColorSchemeSetting::Dark:
        return mSystemTheme.mIsDark;
      case ChromeColorSchemeSetting::System:
        break;
    };
    if (!mColorSchemePreference) {
      return true;
    }
    bool preferenceIsDark = *mColorSchemePreference == ColorScheme::Dark;
    return preferenceIsDark == mSystemTheme.mIsDark;
  }();

  const bool usingSystem = !mSystemThemeOverridden;
  LOGLNF("OverrideSystemThemeIfNeeded(matchesSystem=%d, usingSystem=%d)\n",
         shouldUseSystemTheme, usingSystem);

  if (shouldUseSystemTheme) {
    RestoreSystemTheme();
  } else if (usingSystem) {
    LOGLNF("Setting theme %s, %d\n", mAltTheme.mName.get(),
           mAltTheme.mPreferDarkTheme);

    GtkSettings* settings = gtk_settings_get_default();
    if (mSystemTheme.mName == mAltTheme.mName) {
      // Prefer setting only gtk-application-prefer-dark-theme, so we can still
      // get notified from notify::gtk-theme-name if the user changes the theme.
      g_object_set(settings, "gtk-application-prefer-dark-theme",
                   mAltTheme.mPreferDarkTheme, nullptr);
    } else {
      g_object_set(settings, "gtk-theme-name", mAltTheme.mName.get(),
                   "gtk-application-prefer-dark-theme",
                   mAltTheme.mPreferDarkTheme, nullptr);
    }
    moz_gtk_refresh();
    mSystemThemeOverridden = true;
  }
}

void nsLookAndFeel::GetGtkContentTheme(LookAndFeelTheme& aTheme) {
  if (NS_SUCCEEDED(Preferences::GetCString("widget.content.gtk-theme-override",
                                           aTheme.themeName()))) {
    return;
  }

  auto& theme = StaticPrefs::widget_content_allow_gtk_dark_theme()
                    ? mSystemTheme
                    : LightTheme();
  aTheme.preferDarkTheme() = theme.mPreferDarkTheme;
  aTheme.themeName() = theme.mName;
}

static nscolor GetBackgroundColor(
    GtkStyleContext* aStyle, nscolor aForForegroundColor,
    GtkStateFlags aState = GTK_STATE_FLAG_NORMAL) {
  GdkRGBA gdkColor;
  gtk_style_context_get_background_color(aStyle, aState, &gdkColor);
  nscolor color = GDK_RGBA_TO_NS_RGBA(gdkColor);
  if (NS_GET_A(color)) {
    return color;
  }

  // Try to synthesize a color from a background-image.
  GValue value = G_VALUE_INIT;
  gtk_style_context_get_property(aStyle, "background-image", aState, &value);
  auto cleanup = MakeScopeExit([&] { g_value_unset(&value); });

  if (GetColorFromImagePattern(&value, &color)) {
    return color;
  }

  {
    GdkRGBA light, dark;
    if (GetGradientColors(&value, &light, &dark)) {
      nscolor l = GDK_RGBA_TO_NS_RGBA(light);
      nscolor d = GDK_RGBA_TO_NS_RGBA(dark);
      // Return the one with more contrast.
      // TODO(emilio): This could do interpolation or what not but seems
      // overkill.
      return NS_LUMINOSITY_DIFFERENCE(l, aForForegroundColor) >
                     NS_LUMINOSITY_DIFFERENCE(d, aForForegroundColor)
                 ? l
                 : d;
    }
  }

  return NS_TRANSPARENT;
}

static bool GetNamedColorPair(GtkStyleContext* aStyle, const char* aBgName,
                              const char* aFgName, nscolor* aBg, nscolor* aFg) {
  GdkRGBA bg, fg;
  if (!gtk_style_context_lookup_color(aStyle, aBgName, &bg) ||
      !gtk_style_context_lookup_color(aStyle, aFgName, &fg)) {
    return false;
  }

  *aBg = GDK_RGBA_TO_NS_RGBA(bg);
  *aFg = GDK_RGBA_TO_NS_RGBA(fg);

  // If the colors are semi-transparent and the theme provides a
  // background color, blend with them to get the "final" color, see
  // bug 1717077.
  if (NS_GET_A(*aBg) != 255 &&
      (gtk_style_context_lookup_color(aStyle, "bg_color", &bg) ||
       gtk_style_context_lookup_color(aStyle, "theme_bg_color", &bg))) {
    *aBg = NS_ComposeColors(GDK_RGBA_TO_NS_RGBA(bg), *aBg);
  }

  // A semi-transparent foreground color would be kinda silly, but is done
  // for symmetry.
  if (NS_GET_A(*aFg) != 255 &&
      (gtk_style_context_lookup_color(aStyle, "fg_color", &fg) ||
       gtk_style_context_lookup_color(aStyle, "theme_fg_color", &fg))) {
    *aFg = NS_ComposeColors(GDK_RGBA_TO_NS_RGBA(fg), *aFg);
  }

  return true;
}

static void EnsureColorPairIsOpaque(nscolor& aBg, nscolor& aFg) {
  // Blend with white, ensuring the color is opaque, so that the UI doesn't have
  // to care about alpha.
  aBg = NS_ComposeColors(NS_RGB(0xff, 0xff, 0xff), aBg);
  aFg = NS_ComposeColors(aBg, aFg);
}

static void PreferDarkerBackground(nscolor& aBg, nscolor& aFg) {
  // We use the darker one unless the foreground isn't really a color (is all
  // white / black / gray) and the background is, in which case we stick to what
  // we have.
  if (RelativeLuminanceUtils::Compute(aBg) >
          RelativeLuminanceUtils::Compute(aFg) &&
      (AnyColorChannelIsDifferent(aFg) || !AnyColorChannelIsDifferent(aBg))) {
    std::swap(aBg, aFg);
  }
}

void nsLookAndFeel::PerThemeData::Init() {
  mName = GetGtkTheme();

  GtkStyleContext* style;

  mHighContrast = StaticPrefs::widget_content_gtk_high_contrast_enabled() &&
                  mName.Find("HighContrast"_ns) >= 0;

  mPreferDarkTheme = GetPreferDarkTheme();

  mIsDark = GetThemeIsDark();

  mCompatibleWithHTMLLightColors =
      !mIsDark && IsGtkThemeCompatibleWithHTMLColors();

  GdkRGBA color;
  // Some themes style the <trough>, while others style the <scrollbar>
  // itself, so we look at both and compose the colors.
  style = GetStyleContext(MOZ_GTK_SCROLLBAR_VERTICAL);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mThemedScrollbar = GDK_RGBA_TO_NS_RGBA(color);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_BACKDROP,
                                         &color);
  mThemedScrollbarInactive = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mThemedScrollbar =
      NS_ComposeColors(mThemedScrollbar, GDK_RGBA_TO_NS_RGBA(color));
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_BACKDROP,
                                         &color);
  mThemedScrollbarInactive =
      NS_ComposeColors(mThemedScrollbarInactive, GDK_RGBA_TO_NS_RGBA(color));

  style = GetStyleContext(MOZ_GTK_SCROLLBAR_THUMB_VERTICAL);
  gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
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

  // Make sure that the thumb is visible, at least.
  const bool fallbackToUnthemedColors = [&] {
    if (!StaticPrefs::widget_gtk_theme_scrollbar_colors_enabled()) {
      return true;
    }

    if (!ShouldHonorThemeScrollbarColors()) {
      return true;
    }
    // If any of the scrollbar thumb colors are fully transparent, fall back to
    // non-native ones.
    if (!NS_GET_A(mThemedScrollbarThumb) ||
        !NS_GET_A(mThemedScrollbarThumbHover) ||
        !NS_GET_A(mThemedScrollbarThumbActive)) {
      return true;
    }
    // If the thumb and track are the same color and opaque, fall back to
    // non-native colors as well.
    if (mThemedScrollbar == mThemedScrollbarThumb &&
        NS_GET_A(mThemedScrollbar) == 0xff) {
      return true;
    }
    return false;
  }();

  if (fallbackToUnthemedColors) {
    if (mIsDark) {
      // Taken from Adwaita-dark.
      mThemedScrollbar = NS_RGB(0x31, 0x31, 0x31);
      mThemedScrollbarInactive = NS_RGB(0x2d, 0x2d, 0x2d);
      mThemedScrollbarThumb = NS_RGB(0xa3, 0xa4, 0xa4);
      mThemedScrollbarThumbInactive = NS_RGB(0x59, 0x5a, 0x5a);
    } else {
      // Taken from Adwaita.
      mThemedScrollbar = NS_RGB(0xce, 0xce, 0xce);
      mThemedScrollbarInactive = NS_RGB(0xec, 0xed, 0xef);
      mThemedScrollbarThumb = NS_RGB(0x82, 0x81, 0x7e);
      mThemedScrollbarThumbInactive = NS_RGB(0xce, 0xcf, 0xce);
    }

    mThemedScrollbarThumbHover = ThemeColors::AdjustUnthemedScrollbarThumbColor(
        mThemedScrollbarThumb, dom::ElementState::HOVER);
    mThemedScrollbarThumbActive =
        ThemeColors::AdjustUnthemedScrollbarThumbColor(
            mThemedScrollbarThumb, dom::ElementState::ACTIVE);
  }

  // The label is not added to a parent widget, but shared for constructing
  // different style contexts.  The node hierarchy is constructed only on
  // the label style context.
  GtkWidget* labelWidget = gtk_label_new("M");
  g_object_ref_sink(labelWidget);

  // Window colors
  style = GetStyleContext(MOZ_GTK_WINDOW);

  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozWindowText = GDK_RGBA_TO_NS_RGBA(color);

  mMozWindowBackground = GetBackgroundColor(style, mMozWindowText);

  gtk_style_context_get_border_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mMozWindowActiveBorder = GDK_RGBA_TO_NS_RGBA(color);

  gtk_style_context_get_border_color(style, GTK_STATE_FLAG_INSENSITIVE, &color);
  mMozWindowInactiveBorder = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_WINDOW_CONTAINER);
  {
    GtkStyleContext* labelStyle = CreateStyleForWidget(labelWidget, style);
    GetSystemFontInfo(labelStyle, &mDefaultFontName, &mDefaultFontStyle);
    g_object_unref(labelStyle);
  }

  // tooltip foreground and background
  style = GetStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
  mInfoText = GDK_RGBA_TO_NS_RGBA(color);

  style = GetStyleContext(MOZ_GTK_TOOLTIP);
  mInfoBackground = GetBackgroundColor(style, mInfoText);

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

  const auto effectiveHeaderBarStyle =
      HeaderBarShouldDrawContainer(MOZ_GTK_HEADER_BAR) ? MOZ_GTK_HEADERBAR_FIXED
                                                       : MOZ_GTK_HEADER_BAR;
  style = GetStyleContext(effectiveHeaderBarStyle);
  {
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    mTitlebarText = GDK_RGBA_TO_NS_RGBA(color);
    mTitlebarBackground = GetBackgroundColor(style, mTitlebarText);

    gtk_style_context_get_color(style, GTK_STATE_FLAG_BACKDROP, &color);
    mTitlebarInactiveText = GDK_RGBA_TO_NS_RGBA(color);
    mTitlebarInactiveBackground =
        GetBackgroundColor(style, mTitlebarText, GTK_STATE_FLAG_BACKDROP);
    mTitlebarRadius = IsSolidCSDStyleUsed() ? 0 : GetBorderRadius(style);
  }

  style = GetStyleContext(MOZ_GTK_MENUPOPUP);
  mMenuBackground = [&] {
    nscolor color = GetBackgroundColor(style, mMenuText);
    if (NS_GET_A(color)) {
      return color;
    }
    // Some themes only style menupopups with the backdrop pseudo-class. Since a
    // context / popup menu always seems to match that, try that before giving
    // up.
    color = GetBackgroundColor(style, mMenuText, GTK_STATE_FLAG_BACKDROP);
    if (NS_GET_A(color)) {
      return color;
    }
    // If we get here we couldn't figure out the right color to use. Rather than
    // falling back to transparent, fall back to the window background.
    NS_WARNING(
        "Couldn't find menu background color, falling back to window "
        "background");
    return mMozWindowBackground;
  }();
  mMenuRadius = 0;
  if (!IsSolidCSDStyleUsed()) {
    mMenuRadius = GetBorderRadius(style);
    if (!mMenuRadius) {
      mMenuRadius =
          GetBorderRadius(GetStyleContext(MOZ_GTK_MENUPOPUP_DECORATION));
    }
  }

  style = GetStyleContext(MOZ_GTK_MENUITEM);
  gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
  mMenuHoverText = GDK_RGBA_TO_NS_RGBA(color);
  mMenuHover = NS_ComposeColors(
      mMenuBackground,
      GetBackgroundColor(style, mMenuHoverText, GTK_STATE_FLAG_PRELIGHT));

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

    // Default selected item color is the selection background / foreground
    // colors, but we prefer named colors, as those are more general purpose
    // than the actual selection style, which might e.g. be too-transparent.
    //
    // NOTE(emilio): It's unclear which one of the theme_selected_* or the
    // selected_* pairs should we prefer, in all themes that define both that
    // I've found, they're always the same.
    if (!GetNamedColorPair(style, "selected_bg_color", "selected_fg_color",
                           &mSelectedItem, &mSelectedItemText) &&
        !GetNamedColorPair(style, "theme_selected_bg_color",
                           "theme_selected_fg_color", &mSelectedItem,
                           &mSelectedItemText)) {
      mSelectedItem = mTextSelectedBackground;
      mSelectedItemText = mTextSelectedText;
    }

    EnsureColorPairIsOpaque(mSelectedItem, mSelectedItemText);

    // In a similar fashion, default accent color is the selected item/text
    // pair, but we also prefer named colors, if available.
    //
    // accent_{bg,fg}_color is not _really_ a gtk3 thing (it's a gtk4 thing),
    // but if gtk 3 themes want to specify these we let them, see:
    //
    //   https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/named-colors.html#accent-colors
    if (!GetNamedColorPair(style, "accent_bg_color", "accent_fg_color",
                           &mAccentColor, &mAccentColorForeground)) {
      mAccentColor = mSelectedItem;
      mAccentColorForeground = mSelectedItemText;
    }

    EnsureColorPairIsOpaque(mAccentColor, mAccentColorForeground);
    PreferDarkerBackground(mAccentColor, mAccentColorForeground);
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

  gtk_style_context_get_color(style, GTK_STATE_FLAG_VISITED, &color);
  mNativeVisitedHyperLinkText = GDK_RGBA_TO_NS_RGBA(color);

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

  if (LOGLNF_ENABLED()) {
    LOGLNF("Initialized theme %s (%d)\n", mName.get(), mPreferDarkTheme);
    for (auto id : MakeEnumeratedRange(ColorID::End)) {
      nscolor color;
      nsresult rv = GetColor(id, color);
      LOGLNF(" * color %d: pref=%s success=%d value=%x\n", int(id),
             GetColorPrefName(id), NS_SUCCEEDED(rv),
             NS_SUCCEEDED(rv) ? color : 0);
    }
    LOGLNF(" * titlebar-radius: %d\n", mTitlebarRadius);
    LOGLNF(" * menu-radius: %d\n", mMenuRadius);
  }
}

// virtual
char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
  EnsureInit();
  return mSystemTheme.mInvisibleCharacter;
}

bool nsLookAndFeel::GetEchoPasswordImpl() { return false; }

bool nsLookAndFeel::GetDefaultDrawInTitlebar() {
  static bool drawInTitlebar = []() {
    // When user defined widget.default-hidden-titlebar don't do any
    // heuristics and just follow it.
    if (Preferences::HasUserValue("widget.default-hidden-titlebar")) {
      return Preferences::GetBool("widget.default-hidden-titlebar", false);
    }

    // Don't hide titlebar when it's disabled on current desktop.
    const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (!currentDesktop || !sCSDAvailable) {
      return false;
    }

    // We hide system titlebar on Gnome/ElementaryOS without any restriction.
    return strstr(currentDesktop, "GNOME-Flashback:GNOME") ||
           strstr(currentDesktop, "GNOME") ||
           strstr(currentDesktop, "Pantheon");
  }();
  return drawInTitlebar;
}

void nsLookAndFeel::GetThemeInfo(nsACString& aInfo) {
  aInfo.Append(mSystemTheme.mName);
  aInfo.Append(" / ");
  aInfo.Append(mAltTheme.mName);
}

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
  bool scrollbarUsesImage = !ShouldHonorThemeScrollbarColors();
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

#undef LOGLNF
#undef LOGLNF_ENABLED
