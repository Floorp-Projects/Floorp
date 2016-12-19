/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsScreenGtk.h"

#include "gtkdrawing.h"
#include "nsStyleConsts.h"
#include "gfxFontConstants.h"
#include "WidgetUtils.h"

#include <dlfcn.h>

#include "mozilla/gfx/2D.h"

#if MOZ_WIDGET_GTK != 2
#include <cairo-gobject.h>
#include "WidgetStyleCache.h"
#include "prenv.h"
#endif

using mozilla::LookAndFeel;

#define GDK_COLOR_TO_NS_RGB(c) \
    ((nscolor) NS_RGB(c.red>>8, c.green>>8, c.blue>>8))
#define GDK_RGBA_TO_NS_RGBA(c) \
    ((nscolor) NS_RGBA((int)((c).red*255), (int)((c).green*255), \
                       (int)((c).blue*255), (int)((c).alpha*255)))

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel(),
#if (MOZ_WIDGET_GTK == 2)
      mStyle(nullptr),
#endif
      mDefaultFontCached(false), mButtonFontCached(false),
      mFieldFontCached(false), mMenuFontCached(false)
{
    Init();    
}

nsLookAndFeel::~nsLookAndFeel()
{
#if (MOZ_WIDGET_GTK == 2)
    g_object_unref(mStyle);
#endif
}

#if MOZ_WIDGET_GTK != 2
static void
GetLightAndDarkness(const GdkRGBA& aColor,
                    double* aLightness, double* aDarkness)
{
    double sum = aColor.red + aColor.green + aColor.blue;
    *aLightness = sum * aColor.alpha;
    *aDarkness = (3.0 - sum) * aColor.alpha;
}

static bool
GetGradientColors(const GValue* aValue,
                  GdkRGBA* aLightColor, GdkRGBA* aDarkColor)
{
    if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN))
        return false;

    auto pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
    if (!pattern)
        return false;

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
             cairo_pattern_get_color_stop_rgba(pattern, index, nullptr,
                                               &stop.red, &stop.green,
                                               &stop.blue, &stop.alpha);
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

static bool
GetUnicoBorderGradientColors(GtkStyleContext* aContext,
                             GdkRGBA* aLightColor, GdkRGBA* aDarkColor)
{
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
static bool
GetBorderColors(GtkStyleContext* aContext,
                GdkRGBA* aLightColor, GdkRGBA* aDarkColor)
{
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
        gtk_style_context_get_border(aContext, GTK_STATE_FLAG_NORMAL, &border);
        visible = border.top != 0 || border.right != 0 ||
            border.bottom != 0 || border.left != 0;
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

static bool
GetBorderColors(GtkStyleContext* aContext,
                nscolor* aLightColor, nscolor* aDarkColor)
{
    GdkRGBA lightColor, darkColor;
    bool ret = GetBorderColors(aContext, &lightColor, &darkColor);
    *aLightColor = GDK_RGBA_TO_NS_RGBA(lightColor);
    *aDarkColor = GDK_RGBA_TO_NS_RGBA(darkColor);
    return ret;
}
#endif

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor)
{
#if (MOZ_WIDGET_GTK == 3)
    GdkRGBA gdk_color;
#endif
    nsresult res = NS_OK;

    switch (aID) {
        // These colors don't seem to be used for anything anymore in Mozilla
        // (except here at least TextSelectBackground and TextSelectForeground)
        // The CSS2 colors below are used.
#if (MOZ_WIDGET_GTK == 2)
    case eColorID_WindowBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColorID_WindowForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColorID_WidgetBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_WidgetForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColorID_WidgetSelectBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
        break;
    case eColorID_WidgetSelectForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
        break;
#else
    case eColorID_WindowBackground:
    case eColorID_WidgetBackground:
    case eColorID_TextBackground:
    case eColorID_activecaption: // active window caption background
    case eColorID_appworkspace: // MDI background color
    case eColorID_background: // desktop background
    case eColorID_window:
    case eColorID_windowframe:
    case eColorID__moz_dialog:
    case eColorID__moz_combobox:
        aColor = sMozWindowBackground;
        break;
    case eColorID_WindowForeground:
    case eColorID_WidgetForeground:
    case eColorID_TextForeground: 
    case eColorID_captiontext: // text in active window caption, size box, and scrollbar arrow box (!)
    case eColorID_windowtext:
    case eColorID__moz_dialogtext:
        aColor = sMozWindowText;
        break;
    case eColorID_WidgetSelectBackground:
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID__moz_dragtargetzone:
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
    case eColorID_highlight: // preference selected item,
        aColor = sTextSelectedBackground;
        break;
    case eColorID_WidgetSelectForeground:
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_highlighttext:
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
        aColor = sTextSelectedText;
        break;
#endif
    case eColorID_Widget3DHighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColorID_Widget3DShadow:
        aColor = NS_RGB(0x40,0x40,0x40);
        break;
#if (MOZ_WIDGET_GTK == 2)
    case eColorID_TextBackground:
        // not used?
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColorID_TextForeground: 
        // not used?
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        // still used
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_SELECTED]);
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        // still used
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_SELECTED]);
        break;
#endif
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

#if (MOZ_WIDGET_GTK == 2)
        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColorID_activeborder:
        // active window border
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_activecaption:
        // active window caption background
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_appworkspace:
        // MDI background color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_background:
        // desktop background
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColorID_graytext:
        // disabled text in windows, menus, etc.
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
        break;
    case eColorID_highlight:
        // background of selected item
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_SELECTED]);
        break;
    case eColorID_highlighttext:
        // text of selected item
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_SELECTED]);
        break;
    case eColorID_inactiveborder:
        // inactive window border
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID_inactivecaption:
        // inactive window caption
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_INSENSITIVE]);
        break;
    case eColorID_inactivecaptiontext:
        // text in inactive window caption
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
        break;
#else
        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColorID_activeborder: {
        // active window border
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_WINDOW);
        gtk_style_context_get_border_color(style,
                                           GTK_STATE_FLAG_NORMAL, &gdk_color);
        aColor = GDK_RGBA_TO_NS_RGBA(gdk_color);
        ReleaseStyleContext(style);
        break;
    }
    case eColorID_inactiveborder: {
        // inactive window border
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_WINDOW);
        gtk_style_context_get_border_color(style,
                                           GTK_STATE_FLAG_INSENSITIVE,
                                           &gdk_color);
        aColor = GDK_RGBA_TO_NS_RGBA(gdk_color);
        ReleaseStyleContext(style);
        break;
    }
    case eColorID_graytext: // disabled text in windows, menus, etc.
    case eColorID_inactivecaptiontext: // text in inactive window caption
        aColor = sMenuTextInactive;
        break;
    case eColorID_inactivecaption: {
        // inactive window caption
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_WINDOW);
        gtk_style_context_get_background_color(style,
                                               GTK_STATE_FLAG_INSENSITIVE, 
                                               &gdk_color);
        aColor = GDK_RGBA_TO_NS_RGBA(gdk_color);
        ReleaseStyleContext(style);
        break;
    }
#endif
    case eColorID_infobackground:
        // tooltip background color
        aColor = sInfoBackground;
        break;
    case eColorID_infotext:
        // tooltip text color
        aColor = sInfoText;
        break;
    case eColorID_menu:
        // menu background
        aColor = sMenuBackground;
        break;
    case eColorID_menutext:
        // menu text
        aColor = sMenuText;
        break;
    case eColorID_scrollbar:
        // scrollbar gray area
#if (MOZ_WIDGET_GTK == 2)
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_ACTIVE]);
#else
        aColor = sMozScrollbar;
#endif
        break;

    case eColorID_threedlightshadow:
        // 3-D highlighted inner edge color
        // always same as background in GTK code
    case eColorID_threedface:
    case eColorID_buttonface:
        // 3-D face color
#if (MOZ_WIDGET_GTK == 3)
        aColor = sMozWindowBackground;
#else
        aColor = sButtonBackground;
#endif
        break;

    case eColorID_buttontext:
        // text on push buttons
        aColor = sButtonText;
        break;

    case eColorID_buttonhighlight:
        // 3-D highlighted edge color
    case eColorID_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = sFrameOuterLightBorder;
        break;

    case eColorID_buttonshadow:
        // 3-D shadow edge color
    case eColorID_threedshadow:
        // 3-D shadow inner edge color
        aColor = sFrameInnerDarkBorder;
        break;

#if (MOZ_WIDGET_GTK == 2)
    case eColorID_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
        break;

    case eColorID_window:
    case eColorID_windowframe:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;

    case eColorID_windowtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColorID__moz_fieldtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColorID__moz_dialog:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColorID__moz_dialogtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColorID__moz_dragtargetzone:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
        break; 
    case eColorID__moz_buttondefault:
        // default button border color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
        break;
    case eColorID__moz_buttonhoverface:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_PRELIGHT]);
        break;
    case eColorID__moz_buttonhovertext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_PRELIGHT]);
        break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_ACTIVE]);
        break;
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_ACTIVE]);
        break;
#else
    case eColorID_threeddarkshadow:
        // Hardcode to black
        aColor = NS_RGB(0x00,0x00,0x00);
        break;

    case eColorID__moz_eventreerow:
    case eColorID__moz_field:
        aColor = sMozFieldBackground;
        break;
    case eColorID__moz_fieldtext:
        aColor = sMozFieldText;
        break;
    case eColorID__moz_buttondefault: {
        // default button border color
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_BUTTON);
        gtk_style_context_get_border_color(style,
                                           GTK_STATE_FLAG_NORMAL, &gdk_color);
        aColor = GDK_RGBA_TO_NS_RGBA(gdk_color);
        ReleaseStyleContext(style);
        break;
    }
    case eColorID__moz_buttonhoverface: {
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_BUTTON);
        gtk_style_context_get_background_color(style,
                                               GTK_STATE_FLAG_PRELIGHT, 
                                               &gdk_color);
        aColor = GDK_RGBA_TO_NS_RGBA(gdk_color);
        ReleaseStyleContext(style);
        break;
    }
    case eColorID__moz_buttonhovertext:
        aColor = sButtonHoverText;
        break;
#endif
    case eColorID__moz_menuhover:
        aColor = sMenuHover;
        break;
    case eColorID__moz_menuhovertext:
        aColor = sMenuHoverText;
        break;
    case eColorID__moz_oddtreerow:
        aColor = sOddCellBackground;
        break;
    case eColorID__moz_nativehyperlinktext:
        aColor = sNativeHyperLinkText;
        break;
    case eColorID__moz_comboboxtext:
        aColor = sComboBoxText;
        break;
#if (MOZ_WIDGET_GTK == 2)
    case eColorID__moz_combobox:
        aColor = sComboBoxBackground;
        break;
#endif
    case eColorID__moz_menubartext:
        aColor = sMenuBarText;
        break;
    case eColorID__moz_menubarhovertext:
        aColor = sMenuBarHoverText;
        break;
    case eColorID__moz_gtk_info_bar_text:
#if (MOZ_WIDGET_GTK == 3)
        aColor = sInfoBarText;
#else
        aColor = sInfoText;
#endif
        break;
    default:
        /* default color is BLACK */
        aColor = 0;
        res    = NS_ERROR_FAILURE;
        break;
    }

    return res;
}

#if (MOZ_WIDGET_GTK == 2)
static void darken_gdk_color(GdkColor *src, GdkColor *dest)
{
    gdouble red;
    gdouble green;
    gdouble blue;

    red = (gdouble) src->red / 65535.0;
    green = (gdouble) src->green / 65535.0;
    blue = (gdouble) src->blue / 65535.0;

    red *= 0.93;
    green *= 0.93;
    blue *= 0.93;

    dest->red = red * 65535.0;
    dest->green = green * 65535.0;
    dest->blue = blue * 65535.0;
}
#endif

static int32_t CheckWidgetStyle(GtkWidget* aWidget, const char* aStyle, int32_t aResult) {
    gboolean value = FALSE;
    gtk_widget_style_get(aWidget, aStyle, &value, nullptr);
    return value ? aResult : 0;
}

static int32_t ConvertGTKStepperStyleToMozillaScrollArrowStyle(GtkWidget* aWidget)
{
    if (!aWidget)
        return mozilla::LookAndFeel::eScrollArrowStyle_Single;
  
    return
        CheckWidgetStyle(aWidget, "has-backward-stepper",
                         mozilla::LookAndFeel::eScrollArrow_StartBackward) |
        CheckWidgetStyle(aWidget, "has-forward-stepper",
                         mozilla::LookAndFeel::eScrollArrow_EndForward) |
        CheckWidgetStyle(aWidget, "has-secondary-backward-stepper",
                         mozilla::LookAndFeel::eScrollArrow_EndBackward) |
        CheckWidgetStyle(aWidget, "has-secondary-forward-stepper",
                         mozilla::LookAndFeel::eScrollArrow_StartForward);
}

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
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
    if (NS_SUCCEEDED(res))
        return res;
    res = NS_OK;

    switch (aID) {
    case eIntID_CaretBlinkTime:
        {
            GtkSettings *settings;
            gint blink_time;
            gboolean blink;

            settings = gtk_settings_get_default ();
            g_object_get (settings,
                          "gtk-cursor-blink-time", &blink_time,
                          "gtk-cursor-blink", &blink,
                          nullptr);
 
            if (blink)
                aResult = (int32_t) blink_time;
            else
                aResult = 0;
            break;
        }
    case eIntID_CaretWidth:
        aResult = 1;
        break;
    case eIntID_ShowCaretDuringSelection:
        aResult = 0;
        break;
    case eIntID_SelectTextfieldsOnKeyFocus:
        {
            GtkWidget *entry;
            GtkSettings *settings;
            gboolean select_on_focus;

            entry = gtk_entry_new();
            g_object_ref_sink(entry);
            settings = gtk_widget_get_settings(entry);
            g_object_get(settings, 
                         "gtk-entry-select-on-focus",
                         &select_on_focus,
                         nullptr);
            
            if(select_on_focus)
                aResult = 1;
            else
                aResult = 0;

            gtk_widget_destroy(entry);
            g_object_unref(entry);
        }
        break;
    case eIntID_ScrollToClick:
        {
            GtkSettings *settings;
            gboolean warps_slider = FALSE;

            settings = gtk_settings_get_default ();
            if (g_object_class_find_property (G_OBJECT_GET_CLASS(settings),
                                              "gtk-primary-button-warps-slider")) {
                g_object_get (settings,
                              "gtk-primary-button-warps-slider",
                              &warps_slider,
                              nullptr);
            }

            if (warps_slider)
                aResult = 1;
            else
                aResult = 0;
        }
        break;
    case eIntID_SubmenuDelay:
        {
            GtkSettings *settings;
            gint delay;

            settings = gtk_settings_get_default ();
            g_object_get (settings, "gtk-menu-popup-delay", &delay, nullptr);
            aResult = (int32_t) delay;
            break;
        }
    case eIntID_TooltipDelay:
        {
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
    case eIntID_DragThresholdY:
        {
            GtkWidget* box = gtk_hbox_new(FALSE, 5);
            gint threshold = 0;
            g_object_get(gtk_widget_get_settings(box),
                         "gtk-dnd-drag-threshold", &threshold,
                         nullptr);
            g_object_ref_sink(box);
            
            aResult = threshold;
        }
        break;
    case eIntID_ScrollArrowStyle:
        moz_gtk_init();
        aResult =
            ConvertGTKStepperStyleToMozillaScrollArrowStyle(moz_gtk_get_scrollbar_widget());
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
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eIntID_TouchEnabled:
#if MOZ_WIDGET_GTK == 3
        aResult = mozilla::widget::WidgetUtils::IsTouchDeviceSupportPresent();
        break;
#else
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
#endif
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
        aResult = sMenuSupportsDrag;
        break;
    case eIntID_ScrollbarButtonAutoRepeatBehavior:
        aResult = 1;
        break;
    case eIntID_SwipeAnimationEnabled:
        aResult = 0;
        break;
    case eIntID_ColorPickerAvailable:
        aResult = 1;
        break;
    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
        aResult = 2;
        break;
    default:
        aResult = 0;
        res     = NS_ERROR_FAILURE;
    }

    return res;
}

nsresult
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
    nsresult res = NS_OK;
    res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
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
    case eFloatID_CaretAspectRatio:
        aResult = sCaretRatio;
        break;
    default:
        aResult = -1.0;
        res = NS_ERROR_FAILURE;
    }
    return res;
}

static void
GetSystemFontInfo(GtkWidget *aWidget,
                  nsString *aFontName,
                  gfxFontStyle *aFontStyle)
{
    GtkSettings *settings = gtk_widget_get_settings(aWidget);

    aFontStyle->style       = NS_FONT_STYLE_NORMAL;

    gchar *fontname;
    g_object_get(settings, "gtk-font-name", &fontname, nullptr);

    PangoFontDescription *desc;
    desc = pango_font_description_from_string(fontname);

    aFontStyle->systemFont = true;

    g_free(fontname);

    NS_NAMED_LITERAL_STRING(quote, "\"");
    NS_ConvertUTF8toUTF16 family(pango_font_description_get_family(desc));
    *aFontName = quote + family + quote;

    aFontStyle->weight = pango_font_description_get_weight(desc);

    // FIXME: Set aFontStyle->stretch correctly!
    aFontStyle->stretch = NS_FONT_STRETCH_NORMAL;

    float size = float(pango_font_description_get_size(desc)) / PANGO_SCALE;

    // |size| is now either pixels or pango-points (not Mozilla-points!)

    if (!pango_font_description_get_size_is_absolute(desc)) {
        // |size| is in pango-points, so convert to pixels.
        size *= float(gfxPlatformGtk::GetDPI()) / POINTS_PER_INCH_FLOAT;
    }

    // Scale fonts up on HiDPI displays.
    // This would be done automatically with cairo, but we manually manage
    // the display scale for platform consistency.
    size *= nsScreenGtk::GetGtkMonitorScaleFactor();

    // |size| is now pixels

    aFontStyle->size = size;

    pango_font_description_free(desc);
}

static void
GetSystemFontInfo(LookAndFeel::FontID aID,
                  nsString *aFontName,
                  gfxFontStyle *aFontStyle)
{
    if (aID == LookAndFeel::eFont_Widget) {
        GtkWidget *label = gtk_label_new("M");
        GtkWidget *parent = gtk_fixed_new();
        GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);

        gtk_container_add(GTK_CONTAINER(parent), label);
        gtk_container_add(GTK_CONTAINER(window), parent);

        gtk_widget_ensure_style(label);
        GetSystemFontInfo(label, aFontName, aFontStyle);
        gtk_widget_destroy(window);  // no unref, windows are different

    } else if (aID == LookAndFeel::eFont_Button) {
        GtkWidget *label = gtk_label_new("M");
        GtkWidget *parent = gtk_fixed_new();
        GtkWidget *button = gtk_button_new();
        GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);

        gtk_container_add(GTK_CONTAINER(button), label);
        gtk_container_add(GTK_CONTAINER(parent), button);
        gtk_container_add(GTK_CONTAINER(window), parent);

        gtk_widget_ensure_style(label);
        GetSystemFontInfo(label, aFontName, aFontStyle);
        gtk_widget_destroy(window);  // no unref, windows are different

    } else if (aID == LookAndFeel::eFont_Field) {
        GtkWidget *entry = gtk_entry_new();
        GtkWidget *parent = gtk_fixed_new();
        GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);

        gtk_container_add(GTK_CONTAINER(parent), entry);
        gtk_container_add(GTK_CONTAINER(window), parent);

        gtk_widget_ensure_style(entry);
        GetSystemFontInfo(entry, aFontName, aFontStyle);
        gtk_widget_destroy(window);  // no unref, windows are different

    } else {
        MOZ_ASSERT(aID == LookAndFeel::eFont_Menu, "unexpected font ID");
        GtkWidget *accel_label = gtk_accel_label_new("M");
        GtkWidget *menuitem = gtk_menu_item_new();
        GtkWidget *menu = gtk_menu_new();
        g_object_ref_sink(menu);

        gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
        gtk_menu_shell_append((GtkMenuShell *)GTK_MENU(menu), menuitem);

        gtk_widget_ensure_style(accel_label);
        GetSystemFontInfo(accel_label, aFontName, aFontStyle);
        g_object_unref(menu);
    }
}

bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel)
{
  nsString *cachedFontName = nullptr;
  gfxFontStyle *cachedFontStyle = nullptr;
  bool *isCached = nullptr;

  switch (aID) {
    case eFont_Menu:         // css2
    case eFont_PullDownMenu: // css3
      cachedFontName = &mMenuFontName;
      cachedFontStyle = &mMenuFontStyle;
      isCached = &mMenuFontCached;
      aID = eFont_Menu;
      break;

    case eFont_Field:        // css3
    case eFont_List:         // css3
      cachedFontName = &mFieldFontName;
      cachedFontStyle = &mFieldFontStyle;
      isCached = &mFieldFontCached;
      aID = eFont_Field;
      break;

    case eFont_Button:       // css3
      cachedFontName = &mButtonFontName;
      cachedFontStyle = &mButtonFontStyle;
      isCached = &mButtonFontCached;
      break;

    case eFont_Caption:      // css2
    case eFont_Icon:         // css2
    case eFont_MessageBox:   // css2
    case eFont_SmallCaption: // css2
    case eFont_StatusBar:    // css2
    case eFont_Window:       // css3
    case eFont_Document:     // css3
    case eFont_Workspace:    // css3
    case eFont_Desktop:      // css3
    case eFont_Info:         // css3
    case eFont_Dialog:       // css3
    case eFont_Tooltips:     // moz
    case eFont_Widget:       // moz
      cachedFontName = &mDefaultFontName;
      cachedFontStyle = &mDefaultFontStyle;
      isCached = &mDefaultFontCached;
      aID = eFont_Widget;
      break;
  }

  if (!*isCached) {
    GetSystemFontInfo(aID, cachedFontName, cachedFontStyle);
    *isCached = true;
  }

  aFontName = *cachedFontName;
  aFontStyle = *cachedFontStyle;
  return true;
}

void
nsLookAndFeel::Init()
{
    GdkColor colorValue;
    GdkColor *colorValuePtr;

#if (MOZ_WIDGET_GTK == 2)
    NS_ASSERTION(!mStyle, "already initialized");
    // GtkInvisibles come with a refcount that is not floating
    // (since their initialization code calls g_object_ref_sink) and
    // their destroy code releases that reference (which means they
    // have to be explicitly destroyed, since calling unref enough
    // to cause destruction would lead to *another* unref).
    // However, this combination means that it's actually still ok
    // to use the normal pattern, which is to g_object_ref_sink
    // after construction, and then destroy *and* unref when we're
    // done.  (Though we could skip the g_object_ref_sink and the
    // corresponding g_object_unref, but that's particular to
    // GtkInvisibles and GtkWindows.)
    GtkWidget *widget = gtk_invisible_new();
    g_object_ref_sink(widget); // effectively g_object_ref (see above)

    gtk_widget_ensure_style(widget);
    mStyle = gtk_style_copy(gtk_widget_get_style(widget));

    gtk_widget_destroy(widget);
    g_object_unref(widget);
        
    // tooltip foreground and background
    GtkStyle *style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                                "gtk-tooltips", "GtkWindow",
                                                GTK_TYPE_WINDOW);
    if (style) {
        sInfoBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        sInfoText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
    }

    // menu foreground & menu background
    GtkWidget *accel_label = gtk_accel_label_new("M");
    GtkWidget *menuitem = gtk_menu_item_new();
    GtkWidget *menu = gtk_menu_new();

    g_object_ref_sink(menu);

    gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_widget_set_style(accel_label, nullptr);
    gtk_widget_set_style(menu, nullptr);
    gtk_widget_realize(menu);
    gtk_widget_realize(accel_label);

    style = gtk_widget_get_style(accel_label);
    if (style) {
        sMenuText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
    }

    style = gtk_widget_get_style(menu);
    if (style) {
        sMenuBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
    }
    
    style = gtk_widget_get_style(menuitem);
    if (style) {
        sMenuHover = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_PRELIGHT]);
        sMenuHoverText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_PRELIGHT]);
    }

    g_object_unref(menu);
#else
    GdkRGBA color;
    GtkStyleContext *style;

    // Gtk manages a screen's CSS in the settings object so we
    // ask Gtk to create it explicitly. Otherwise we may end up
    // with wrong color theme, see Bug 972382
    GtkSettings *settings = gtk_settings_get_for_screen(gdk_screen_get_default());

    // Disable dark theme because it interacts poorly with widget styling in
    // web content (see bug 1216658).
    // To avoid triggering reload of theme settings unnecessarily, only set the
    // setting when necessary.
    const gchar* dark_setting = "gtk-application-prefer-dark-theme";
    gboolean dark;
    g_object_get(settings, dark_setting, &dark, nullptr);

    if (dark && !PR_GetEnv("MOZ_ALLOW_GTK_DARK_THEME")) {
        g_object_set(settings, dark_setting, FALSE, nullptr);
    }

    // Scrollbar colors
    style = ClaimStyleContext(MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMozScrollbar = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    // Window colors
    style = ClaimStyleContext(MOZ_GTK_WINDOW);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMozWindowBackground = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMozWindowText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    // tooltip foreground and background
    style = ClaimStyleContext(MOZ_GTK_TOOLTIP);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sInfoBackground = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    style = ClaimStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sInfoText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    style = ClaimStyleContext(MOZ_GTK_MENUITEM);
    {
        GtkStyleContext* accelStyle =
            CreateStyleForWidget(gtk_accel_label_new("M"), style);
        gtk_style_context_get_color(accelStyle, GTK_STATE_FLAG_NORMAL, &color);
        sMenuText = GDK_RGBA_TO_NS_RGBA(color);
        gtk_style_context_get_color(accelStyle, GTK_STATE_FLAG_INSENSITIVE, &color);
        sMenuTextInactive = GDK_RGBA_TO_NS_RGBA(color);
        g_object_unref(accelStyle);
    }
    ReleaseStyleContext(style);

    style = ClaimStyleContext(MOZ_GTK_MENUPOPUP);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMenuBackground = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    style = ClaimStyleContext(MOZ_GTK_MENUITEM);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
    sMenuHover = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
    sMenuHoverText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);
#endif

    // button styles
    GtkWidget *parent = gtk_fixed_new();
    GtkWidget *button = gtk_button_new();
    GtkWidget *label = gtk_label_new("M");
#if (MOZ_WIDGET_GTK == 2)
    GtkWidget *combobox = gtk_combo_box_new();
    GtkWidget *comboboxLabel = gtk_label_new("M");
    gtk_container_add(GTK_CONTAINER(combobox), comboboxLabel);
#endif
    GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget *treeView = gtk_tree_view_new();
    GtkWidget *linkButton = gtk_link_button_new("http://example.com/");
    GtkWidget *menuBar = gtk_menu_bar_new();
    GtkWidget *menuBarItem = gtk_menu_item_new();
    GtkWidget *entry = gtk_entry_new();
    GtkWidget *textView = gtk_text_view_new();

    gtk_container_add(GTK_CONTAINER(button), label);
    gtk_container_add(GTK_CONTAINER(parent), button);
    gtk_container_add(GTK_CONTAINER(parent), treeView);
    gtk_container_add(GTK_CONTAINER(parent), linkButton);
#if (MOZ_WIDGET_GTK == 2)
    gtk_container_add(GTK_CONTAINER(parent), combobox);
#endif
    gtk_container_add(GTK_CONTAINER(parent), menuBar);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), menuBarItem);
    gtk_container_add(GTK_CONTAINER(window), parent);
    gtk_container_add(GTK_CONTAINER(parent), entry);
    gtk_container_add(GTK_CONTAINER(parent), textView);
    
#if (MOZ_WIDGET_GTK == 2)
    gtk_widget_set_style(button, nullptr);
    gtk_widget_set_style(label, nullptr);
    gtk_widget_set_style(treeView, nullptr);
    gtk_widget_set_style(linkButton, nullptr);
    gtk_widget_set_style(combobox, nullptr);
    gtk_widget_set_style(comboboxLabel, nullptr);
    gtk_widget_set_style(menuBar, nullptr);
    gtk_widget_set_style(entry, nullptr);

    gtk_widget_realize(button);
    gtk_widget_realize(label);
    gtk_widget_realize(treeView);
    gtk_widget_realize(linkButton);
    gtk_widget_realize(combobox);
    gtk_widget_realize(comboboxLabel);
    gtk_widget_realize(menuBar);
    gtk_widget_realize(entry);

    style = gtk_widget_get_style(label);
    if (style) {
        sButtonText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
    }

    style = gtk_widget_get_style(comboboxLabel);
    if (style) {
        sComboBoxText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
    }
    style = gtk_widget_get_style(combobox);
    if (style) {
        sComboBoxBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
    }

    style = gtk_widget_get_style(menuBar);
    if (style) {
        sMenuBarText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        sMenuBarHoverText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_SELECTED]);
    }

    // GTK's guide to fancy odd row background colors:
    // 1) Check if a theme explicitly defines an odd row color
    // 2) If not, check if it defines an even row color, and darken it
    //    slightly by a hardcoded value (gtkstyle.c)
    // 3) If neither are defined, take the base background color and
    //    darken that by a hardcoded value
    colorValuePtr = nullptr;
    gtk_widget_style_get(treeView,
                         "odd-row-color", &colorValuePtr,
                         nullptr);

    if (colorValuePtr) {
        colorValue = *colorValuePtr;
    } else {
        gtk_widget_style_get(treeView,
                             "even-row-color", &colorValuePtr,
                             nullptr);
        if (colorValuePtr)
            darken_gdk_color(colorValuePtr, &colorValue);
        else
            darken_gdk_color(&treeView->style->base[GTK_STATE_NORMAL], &colorValue);
    }

    sOddCellBackground = GDK_COLOR_TO_NS_RGB(colorValue);
    if (colorValuePtr)
        gdk_color_free(colorValuePtr);

    style = gtk_widget_get_style(button);
    if (style) {
        sButtonBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        sFrameOuterLightBorder =
            GDK_COLOR_TO_NS_RGB(style->light[GTK_STATE_NORMAL]);
        sFrameInnerDarkBorder =
            GDK_COLOR_TO_NS_RGB(style->dark[GTK_STATE_NORMAL]);
    }
#else
    // Text colors
    style = ClaimStyleContext(MOZ_GTK_TEXT_VIEW);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMozFieldBackground = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMozFieldText = GDK_RGBA_TO_NS_RGBA(color);

    // Selected text and background
    gtk_style_context_get_background_color(style,
        static_cast<GtkStateFlags>(GTK_STATE_FLAG_FOCUSED|GTK_STATE_FLAG_SELECTED),
        &color);
    sTextSelectedBackground = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(style,
        static_cast<GtkStateFlags>(GTK_STATE_FLAG_FOCUSED|GTK_STATE_FLAG_SELECTED),
        &color);
    sTextSelectedText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    // Button text color
    style = ClaimStyleContext(MOZ_GTK_BUTTON);
    {
        GtkStyleContext* labelStyle =
            CreateStyleForWidget(gtk_label_new("M"), style);
        gtk_style_context_get_color(labelStyle, GTK_STATE_FLAG_NORMAL, &color);
        sButtonText = GDK_RGBA_TO_NS_RGBA(color);
        gtk_style_context_get_color(labelStyle, GTK_STATE_FLAG_PRELIGHT, &color);
        sButtonHoverText = GDK_RGBA_TO_NS_RGBA(color);
        g_object_unref(labelStyle);
    }
    ReleaseStyleContext(style);

    // Combobox text color
    style = ClaimStyleContext(MOZ_GTK_COMBOBOX_ENTRY_TEXTAREA);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sComboBoxText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    // Menubar text and hover text colors    
    style = ClaimStyleContext(MOZ_GTK_MENUBARITEM);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sMenuBarText = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_PRELIGHT, &color);
    sMenuBarHoverText = GDK_RGBA_TO_NS_RGBA(color);
    ReleaseStyleContext(style);

    // GTK's guide to fancy odd row background colors:
    // 1) Check if a theme explicitly defines an odd row color
    // 2) If not, check if it defines an even row color, and darken it
    //    slightly by a hardcoded value (gtkstyle.c)
    // 3) If neither are defined, take the base background color and
    //    darken that by a hardcoded value
    style = ClaimStyleContext(MOZ_GTK_TREEVIEW);

    // Get odd row background color
    gtk_style_context_save(style);
    gtk_style_context_add_region(style, GTK_STYLE_REGION_ROW, GTK_REGION_ODD);
    gtk_style_context_get_background_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sOddCellBackground = GDK_RGBA_TO_NS_RGBA(color);
    gtk_style_context_restore(style);
    ReleaseStyleContext(style);

    // GtkFrame has a "border" subnode on which Adwaita draws the border.
    // Some themes do not draw on this node but draw a border on the widget
    // root node, so check the root node if no border is found on the border
    // node.
    style = ClaimStyleContext(MOZ_GTK_FRAME_BORDER);
    bool themeUsesColors =
        GetBorderColors(style, &sFrameOuterLightBorder, &sFrameInnerDarkBorder);
    ReleaseStyleContext(style);
    if (!themeUsesColors) {
        style = ClaimStyleContext(MOZ_GTK_FRAME);
        GetBorderColors(style, &sFrameOuterLightBorder, &sFrameInnerDarkBorder);
        ReleaseStyleContext(style);
    }

    // GtkInfoBar
    // TODO - Use WidgetCache for it?
    GtkWidget* infoBar = gtk_info_bar_new();
    GtkWidget* infoBarContent = gtk_info_bar_get_content_area(GTK_INFO_BAR(infoBar));
    GtkWidget* infoBarLabel = gtk_label_new(nullptr);
    gtk_container_add(GTK_CONTAINER(parent), infoBar);
    gtk_container_add(GTK_CONTAINER(infoBarContent), infoBarLabel);
    style = gtk_widget_get_style_context(infoBarLabel);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_INFO);
    gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &color);
    sInfoBarText = GDK_RGBA_TO_NS_RGBA(color);
#endif
    // Some themes have a unified menu bar, and support window dragging on it
    gboolean supports_menubar_drag = FALSE;
    GParamSpec *param_spec =
        gtk_widget_class_find_style_property(GTK_WIDGET_GET_CLASS(menuBar),
                                             "window-dragging");
    if (param_spec) {
        if (g_type_is_a(G_PARAM_SPEC_VALUE_TYPE(param_spec), G_TYPE_BOOLEAN)) {
            gtk_widget_style_get(menuBar,
                                 "window-dragging", &supports_menubar_drag,
                                 nullptr);
        }
    }
    sMenuSupportsDrag = supports_menubar_drag;

    colorValuePtr = nullptr;
    gtk_widget_style_get(linkButton, "link-color", &colorValuePtr, nullptr);
    if (colorValuePtr) {
        colorValue = *colorValuePtr; // we can't pass deref pointers to GDK_COLOR_TO_NS_RGB
        sNativeHyperLinkText = GDK_COLOR_TO_NS_RGB(colorValue);
        gdk_color_free(colorValuePtr);
    } else {
        sNativeHyperLinkText = NS_RGB(0x00,0x00,0xEE);
    }

    // invisible character styles
    guint value;
    g_object_get (entry, "invisible-char", &value, nullptr);
    sInvisibleCharacter = char16_t(value);

    // caret styles
    gtk_widget_style_get(entry,
                         "cursor-aspect-ratio", &sCaretRatio,
                         nullptr);

    gtk_widget_destroy(window);
}

// virtual
char16_t
nsLookAndFeel::GetPasswordCharacterImpl()
{
    return sInvisibleCharacter;
}

void
nsLookAndFeel::RefreshImpl()
{
    nsXPLookAndFeel::RefreshImpl();

    mDefaultFontCached = false;
    mButtonFontCached = false;
    mFieldFontCached = false;
    mMenuFontCached = false;

#if (MOZ_WIDGET_GTK == 2)
    g_object_unref(mStyle);
    mStyle = nullptr;
#endif

    Init();
}

bool
nsLookAndFeel::GetEchoPasswordImpl() {
    return false;
}
