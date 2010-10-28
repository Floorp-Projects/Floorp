/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
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

#include "nsLookAndFeel.h"
#include <gtk/gtk.h>

#include "gtkdrawing.h"

#ifdef MOZ_PLATFORM_MAEMO
#include "nsIServiceManager.h"
#include "nsIPropertyBag2.h"
#include "nsLiteralString.h"
#endif

#define GDK_COLOR_TO_NS_RGB(c) \
    ((nscolor) NS_RGB(c.red>>8, c.green>>8, c.blue>>8))

nscolor   nsLookAndFeel::sInfoText = 0;
nscolor   nsLookAndFeel::sInfoBackground = 0;
nscolor   nsLookAndFeel::sMenuBarText = 0;
nscolor   nsLookAndFeel::sMenuBarHoverText = 0;
nscolor   nsLookAndFeel::sMenuText = 0;
nscolor   nsLookAndFeel::sMenuHover = 0;
nscolor   nsLookAndFeel::sMenuHoverText = 0;
nscolor   nsLookAndFeel::sMenuBackground = 0;
nscolor   nsLookAndFeel::sButtonBackground = 0;
nscolor   nsLookAndFeel::sButtonText = 0;
nscolor   nsLookAndFeel::sButtonOuterLightBorder = 0;
nscolor   nsLookAndFeel::sButtonInnerDarkBorder = 0;
nscolor   nsLookAndFeel::sOddCellBackground = 0;
nscolor   nsLookAndFeel::sNativeHyperLinkText = 0;
nscolor   nsLookAndFeel::sComboBoxText = 0;
nscolor   nsLookAndFeel::sComboBoxBackground = 0;
PRUnichar nsLookAndFeel::sInvisibleCharacter = PRUnichar('*');
float     nsLookAndFeel::sCaretRatio = 0;
PRBool    nsLookAndFeel::sMenuSupportsDrag = PR_FALSE;

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
    mStyle = nsnull;
    InitWidget();

    static PRBool sInitialized = PR_FALSE;

    if (!sInitialized) {
        sInitialized = PR_TRUE;
        InitLookAndFeel();
    }
}

nsLookAndFeel::~nsLookAndFeel()
{
    g_object_unref(mStyle);
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor& aColor)
{
    nsresult res = NS_OK;

    switch (aID) {
        // These colors don't seem to be used for anything anymore in Mozilla
        // (except here at least TextSelectBackground and TextSelectForeground)
        // The CSS2 colors below are used.
    case eColor_WindowBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColor_WindowForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColor_WidgetBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_WidgetForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColor_WidgetSelectBackground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
        break;
    case eColor_WidgetSelectForeground:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
        break;
    case eColor_Widget3DHighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColor_Widget3DShadow:
        aColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eColor_TextBackground:
        // not used?
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColor_TextForeground: 
        // not used?
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColor_TextSelectBackground:
    case eColor_IMESelectedRawTextBackground:
    case eColor_IMESelectedConvertedTextBackground:
        // still used
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_SELECTED]);
        break;
    case eColor_TextSelectForeground:
    case eColor_IMESelectedRawTextForeground:
    case eColor_IMESelectedConvertedTextForeground:
        // still used
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_SELECTED]);
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
      aColor = NS_RGB(0xff, 0, 0);
      break;

        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case eColor_activeborder:
        // active window border
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_activecaption:
        // active window caption background
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_appworkspace:
        // MDI background color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_background:
        // desktop background
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColor_graytext:
        // disabled text in windows, menus, etc.
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
        break;
    case eColor_highlight:
        // background of selected item
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_SELECTED]);
        break;
    case eColor_highlighttext:
        // text of selected item
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_SELECTED]);
        break;
    case eColor_inactiveborder:
        // inactive window border
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor_inactivecaption:
        // inactive window caption
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_INSENSITIVE]);
        break;
    case eColor_inactivecaptiontext:
        // text in inactive window caption
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
        break;
    case eColor_infobackground:
        // tooltip background color
        aColor = sInfoBackground;
        break;
    case eColor_infotext:
        // tooltip text color
        aColor = sInfoText;
        break;
    case eColor_menu:
        // menu background
        aColor = sMenuBackground;
        break;
    case eColor_menutext:
        // menu text
        aColor = sMenuText;
        break;
    case eColor_scrollbar:
        // scrollbar gray area
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_ACTIVE]);
        break;

    case eColor_threedface:
    case eColor_buttonface:
        // 3-D face color
        aColor = sButtonBackground;
        break;

    case eColor_buttontext:
        // text on push buttons
        aColor = sButtonText;
        break;

    case eColor_buttonhighlight:
        // 3-D highlighted edge color
    case eColor_threedhighlight:
        // 3-D highlighted outer edge color
        aColor = sButtonOuterLightBorder;
        break;

    case eColor_threedlightshadow:
        // 3-D highlighted inner edge color
        aColor = sButtonBackground; // always same as background in GTK code
        break;

    case eColor_buttonshadow:
        // 3-D shadow edge color
    case eColor_threedshadow:
        // 3-D shadow inner edge color
        aColor = sButtonInnerDarkBorder;
        break;

    case eColor_threeddarkshadow:
        // 3-D shadow outer edge color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
        break;

    case eColor_window:
    case eColor_windowframe:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;

    case eColor_windowtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;

    case eColor__moz_eventreerow:
    case eColor__moz_field:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
        break;
    case eColor__moz_fieldtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_NORMAL]);
        break;
    case eColor__moz_dialog:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
        break;
    case eColor__moz_dialogtext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
        break;
    case eColor__moz_dragtargetzone:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
        break; 
    case eColor__moz_buttondefault:
        // default button border color
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
        break;
    case eColor__moz_buttonhoverface:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_PRELIGHT]);
        break;
    case eColor__moz_buttonhovertext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_PRELIGHT]);
        break;
    case eColor__moz_cellhighlight:
    case eColor__moz_html_cellhighlight:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_ACTIVE]);
        break;
    case eColor__moz_cellhighlighttext:
    case eColor__moz_html_cellhighlighttext:
        aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_ACTIVE]);
        break;
    case eColor__moz_menuhover:
        aColor = sMenuHover;
        break;
    case eColor__moz_menuhovertext:
        aColor = sMenuHoverText;
        break;
    case eColor__moz_oddtreerow:
        aColor = sOddCellBackground;
        break;
    case eColor__moz_nativehyperlinktext:
        aColor = sNativeHyperLinkText;
        break;
    case eColor__moz_comboboxtext:
        aColor = sComboBoxText;
        break;
    case eColor__moz_combobox:
        aColor = sComboBoxBackground;
        break;
    case eColor__moz_menubartext:
        aColor = sMenuBarText;
        break;
    case eColor__moz_menubarhovertext:
        aColor = sMenuBarHoverText;
        break;
    default:
        /* default color is BLACK */
        aColor = 0;
        res    = NS_ERROR_FAILURE;
        break;
    }

    return res;
}

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

static PRInt32 CheckWidgetStyle(GtkWidget* aWidget, const char* aStyle, PRInt32 aMetric) {
    gboolean value = PR_FALSE;
    gtk_widget_style_get(aWidget, aStyle, &value, NULL);
    return value ? aMetric : 0;
}

static PRInt32 ConvertGTKStepperStyleToMozillaScrollArrowStyle(GtkWidget* aWidget)
{
    if (!aWidget)
        return nsILookAndFeel::eMetric_ScrollArrowStyleSingle;
  
    return
        CheckWidgetStyle(aWidget, "has-backward-stepper",
                         nsILookAndFeel::eMetric_ScrollArrowStartBackward) |
        CheckWidgetStyle(aWidget, "has-forward-stepper",
                         nsILookAndFeel::eMetric_ScrollArrowEndForward) |
        CheckWidgetStyle(aWidget, "has-secondary-backward-stepper",
                         nsILookAndFeel::eMetric_ScrollArrowEndBackward) |
        CheckWidgetStyle(aWidget, "has-secondary-forward-stepper",
                         nsILookAndFeel::eMetric_ScrollArrowStartForward);
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    nsresult res = NS_OK;

    // Set these before they can get overrided in the nsXPLookAndFeel. 
    switch (aID) {
    case eMetric_ScrollButtonLeftMouseButtonAction:
        aMetric = 0;
        return NS_OK;
    case eMetric_ScrollButtonMiddleMouseButtonAction:
        aMetric = 1;
        return NS_OK;
    case eMetric_ScrollButtonRightMouseButtonAction:
        aMetric = 2;
        return NS_OK;
    default:
        break;
    }

    res = nsXPLookAndFeel::GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
        return res;
    res = NS_OK;

    switch (aID) {
    case eMetric_CaretBlinkTime:
        {
            GtkSettings *settings;
            gint blink_time;
            gboolean blink;

            settings = gtk_settings_get_default ();
            g_object_get (settings,
                          "gtk-cursor-blink-time", &blink_time,
                          "gtk-cursor-blink", &blink,
                          NULL);
 
            if (blink)
                aMetric = (PRInt32) blink_time;
            else
                aMetric = 0;
            break;
        }
    case eMetric_CaretWidth:
        aMetric = 1;
        break;
    case eMetric_ShowCaretDuringSelection:
        aMetric = 0;
        break;
    case eMetric_SelectTextfieldsOnKeyFocus:
        {
            GtkWidget *entry;
            GtkSettings *settings;
            gboolean select_on_focus;

            entry = gtk_entry_new();
            g_object_ref_sink(GTK_OBJECT(entry));
            settings = gtk_widget_get_settings(entry);
            g_object_get(settings, 
                         "gtk-entry-select-on-focus",
                         &select_on_focus,
                         NULL);
            
            if(select_on_focus)
                aMetric = 1;
            else
                aMetric = 0;

            gtk_widget_destroy(entry);
            g_object_unref(entry);
        }
        break;
    case eMetric_SubmenuDelay:
        {
            GtkSettings *settings;
            gint delay;

            settings = gtk_settings_get_default ();
            g_object_get (settings, "gtk-menu-popup-delay", &delay, NULL);
            aMetric = (PRInt32) delay;
            break;
        }
    case eMetric_MenusCanOverlapOSBar:
        // we want XUL popups to be able to overlap the task bar.
        aMetric = 1;
        break;
    case eMetric_SkipNavigatingDisabledMenuItem:
        aMetric = 1;
        break;
    case eMetric_DragThresholdX:
    case eMetric_DragThresholdY:
        {
            GtkWidget* box = gtk_hbox_new(FALSE, 5);
            gint threshold = 0;
            g_object_get(gtk_widget_get_settings(box),
                         "gtk-dnd-drag-threshold", &threshold,
                         NULL);
            g_object_ref_sink(GTK_OBJECT(box));
            
            aMetric = threshold;
        }
        break;
    case eMetric_ScrollArrowStyle:
        aMetric =
            ConvertGTKStepperStyleToMozillaScrollArrowStyle(moz_gtk_get_scrollbar_widget());
        break;
    case eMetric_ScrollSliderStyle:
        aMetric = eMetric_ScrollThumbStyleProportional;
        break;
    case eMetric_TreeOpenDelay:
        aMetric = 1000;
        break;
    case eMetric_TreeCloseDelay:
        aMetric = 1000;
        break;
    case eMetric_TreeLazyScrollDelay:
        aMetric = 150;
        break;
    case eMetric_TreeScrollDelay:
        aMetric = 100;
        break;
    case eMetric_TreeScrollLinesMax:
        aMetric = 3;
        break;
    case eMetric_DWMCompositor:
    case eMetric_WindowsClassic:
    case eMetric_WindowsDefaultTheme:
    case eMetric_WindowsThemeIdentifier:
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eMetric_TouchEnabled:
#ifdef MOZ_PLATFORM_MAEMO
        // All Hildon devices are touch-enabled
        aMetric = 1;
#else
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
#endif
        break;
    case eMetric_MaemoClassic:
#ifdef MOZ_PLATFORM_MAEMO
        {
            aMetric = 0;
            nsCOMPtr<nsIPropertyBag2> infoService(do_GetService("@mozilla.org/system-info;1"));
            if (infoService) {
                nsCString deviceType;
                nsresult rv = infoService->GetPropertyAsACString(NS_LITERAL_STRING("device"),
                                                                 deviceType);
                if (NS_SUCCEEDED(rv)) {
                    if (deviceType.EqualsLiteral("Nokia N8xx"))
                        aMetric = 1;
                }
            }
        }
#else
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
#endif
        break;
    case eMetric_MacGraphiteTheme:
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eMetric_IMERawInputUnderlineStyle:
    case eMetric_IMEConvertedTextUnderlineStyle:
        aMetric = NS_UNDERLINE_STYLE_SOLID;
        break;
    case eMetric_IMESelectedRawTextUnderlineStyle:
    case eMetric_IMESelectedConvertedTextUnderline:
        aMetric = NS_UNDERLINE_STYLE_NONE;
        break;
    case eMetric_SpellCheckerUnderlineStyle:
        aMetric = NS_UNDERLINE_STYLE_WAVY;
        break;
    case eMetric_ImagesInMenus:
        aMetric = moz_gtk_images_in_menus();
        break;
    case eMetric_ImagesInButtons:
        aMetric = moz_gtk_images_in_buttons();
        break;
    case eMetric_MenuBarDrag:
        aMetric = sMenuSupportsDrag;
        break;
    default:
        aMetric = 0;
        res     = NS_ERROR_FAILURE;
    }

    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID,
                                       float & aMetric)
{
    nsresult res = NS_OK;
    res = nsXPLookAndFeel::GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
        return res;
    res = NS_OK;

    switch (aID) {
    case eMetricFloat_IMEUnderlineRelativeSize:
        aMetric = 1.0f;
        break;
    case eMetricFloat_SpellCheckerUnderlineRelativeSize:
        aMetric = 1.0f;
        break;
    case eMetricFloat_CaretAspectRatio:
        aMetric = sCaretRatio;
        break;
    default:
        aMetric = -1.0;
        res = NS_ERROR_FAILURE;
    }
    return res;
}

void
nsLookAndFeel::InitLookAndFeel()
{
    GtkStyle *style;

    // tooltip foreground and background
    style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
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

    g_object_ref_sink(GTK_OBJECT(menu));

    gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_widget_set_style(accel_label, NULL);
    gtk_widget_set_style(menu, NULL);
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


    // button styles
    GtkWidget *parent = gtk_fixed_new();
    GtkWidget *button = gtk_button_new();
    GtkWidget *label = gtk_label_new("M");
    GtkWidget *combobox = gtk_combo_box_new();
    GtkWidget *comboboxLabel = gtk_label_new("M");
    GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget *treeView = gtk_tree_view_new();
    GtkWidget *linkButton = gtk_link_button_new("http://example.com/");
    GtkWidget *menuBar = gtk_menu_bar_new();
    GtkWidget *entry = gtk_entry_new();

    gtk_container_add(GTK_CONTAINER(button), label);
    gtk_container_add(GTK_CONTAINER(combobox), comboboxLabel);
    gtk_container_add(GTK_CONTAINER(parent), button);
    gtk_container_add(GTK_CONTAINER(parent), treeView);
    gtk_container_add(GTK_CONTAINER(parent), linkButton);
    gtk_container_add(GTK_CONTAINER(parent), combobox);
    gtk_container_add(GTK_CONTAINER(parent), menuBar);
    gtk_container_add(GTK_CONTAINER(window), parent);
    gtk_container_add(GTK_CONTAINER(parent), entry);

    gtk_widget_set_style(button, NULL);
    gtk_widget_set_style(label, NULL);
    gtk_widget_set_style(treeView, NULL);
    gtk_widget_set_style(linkButton, NULL);
    gtk_widget_set_style(combobox, NULL);
    gtk_widget_set_style(comboboxLabel, NULL);
    gtk_widget_set_style(menuBar, NULL);
    gtk_widget_set_style(entry, NULL);

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

    // Some themes have a unified menu bar, and support window dragging on it
    gboolean supports_menubar_drag = FALSE;
    GParamSpec *param_spec =
        gtk_widget_class_find_style_property(GTK_WIDGET_GET_CLASS(menuBar),
                                             "window-dragging");
    if (param_spec) {
        if (g_type_is_a(G_PARAM_SPEC_VALUE_TYPE(param_spec), G_TYPE_BOOLEAN)) {
            gtk_widget_style_get(menuBar,
                                 "window-dragging", &supports_menubar_drag,
                                 NULL);
        }
    }
    sMenuSupportsDrag = supports_menubar_drag;

    // GTK's guide to fancy odd row background colors:
    // 1) Check if a theme explicitly defines an odd row color
    // 2) If not, check if it defines an even row color, and darken it
    //    slightly by a hardcoded value (gtkstyle.c)
    // 3) If neither are defined, take the base background color and
    //    darken that by a hardcoded value
    GdkColor colorValue;
    GdkColor *colorValuePtr = NULL;
    gtk_widget_style_get(treeView,
                         "odd-row-color", &colorValuePtr,
                         NULL);

    if (colorValuePtr) {
        colorValue = *colorValuePtr;
    } else {
        gtk_widget_style_get(treeView,
                             "even-row-color", &colorValuePtr,
                             NULL);
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
        sButtonOuterLightBorder =
            GDK_COLOR_TO_NS_RGB(style->light[GTK_STATE_NORMAL]);
        sButtonInnerDarkBorder =
            GDK_COLOR_TO_NS_RGB(style->dark[GTK_STATE_NORMAL]);
    }

    colorValuePtr = NULL;
    gtk_widget_style_get(linkButton, "link-color", &colorValuePtr, NULL);
    if (colorValuePtr) {
        colorValue = *colorValuePtr; // we can't pass deref pointers to GDK_COLOR_TO_NS_RGB
        sNativeHyperLinkText = GDK_COLOR_TO_NS_RGB(colorValue);
        gdk_color_free(colorValuePtr);
    } else {
        sNativeHyperLinkText = NS_RGB(0x00,0x00,0xEE);
    }

    // invisible character styles
    guint value;
    g_object_get (entry, "invisible-char", &value, NULL);
    sInvisibleCharacter = PRUnichar(value);

    // caret styles
    gtk_widget_style_get(entry,
                         "cursor-aspect-ratio", &sCaretRatio,
                         NULL);

    gtk_widget_destroy(window);
}

// virtual
PRUnichar
nsLookAndFeel::GetPasswordCharacter()
{
    return sInvisibleCharacter;
}

NS_IMETHODIMP
nsLookAndFeel::LookAndFeelChanged()
{
    nsXPLookAndFeel::LookAndFeelChanged();

    g_object_unref(mStyle);
    mStyle = nsnull;
 
    InitWidget();
    InitLookAndFeel();

    return NS_OK;
}
