/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains painting functions for each of the gtk2 widgets.
 * Adapted from the gtkdrawing.c, and gtk+2.0 source.
 */

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <string.h>
#include "gtkdrawing.h"
#include "mozilla/Assertions.h"
#include "prinrval.h"
#include "WidgetStyleCache.h"

#include <math.h>

static style_prop_t style_prop_func;
static gboolean have_arrow_scaling;
static gboolean checkbox_check_state;
static gboolean notebook_has_tab_gap;
static gboolean is_initialized;

#define ARROW_UP      0
#define ARROW_DOWN    G_PI
#define ARROW_RIGHT   G_PI_2
#define ARROW_LEFT    (G_PI+G_PI_2)

#if !GTK_CHECK_VERSION(3,14,0)
#define GTK_STATE_FLAG_CHECKED (1 << 11)
#endif

static gint
moz_gtk_get_tab_thickness(GtkStyleContext *style);

static gint
moz_gtk_menu_item_paint(WidgetNodeType widget, cairo_t *cr, GdkRectangle* rect,
                        GtkWidgetState* state, GtkTextDirection direction);

static GtkStateFlags
GetStateFlagsFromGtkWidgetState(GtkWidgetState* state)
{
    GtkStateFlags stateFlags = GTK_STATE_FLAG_NORMAL;

    if (state->disabled)
        stateFlags = GTK_STATE_FLAG_INSENSITIVE;
    else {    
        if (state->depressed || state->active)
            stateFlags = static_cast<GtkStateFlags>(stateFlags|GTK_STATE_FLAG_ACTIVE);
        if (state->inHover)
            stateFlags = static_cast<GtkStateFlags>(stateFlags|GTK_STATE_FLAG_PRELIGHT);
        if (state->focused)
            stateFlags = static_cast<GtkStateFlags>(stateFlags|GTK_STATE_FLAG_FOCUSED);
    }
  
    return stateFlags;
}

static GtkStateFlags
GetStateFlagsFromGtkTabFlags(GtkTabFlags flags)
{
    return ((flags & MOZ_GTK_TAB_SELECTED) == 0) ?
            GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_ACTIVE;
}

gint
moz_gtk_enable_style_props(style_prop_t styleGetProp)
{
    style_prop_func = styleGetProp;
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_init()
{
    if (is_initialized)
        return MOZ_GTK_SUCCESS;

    is_initialized = TRUE;
    have_arrow_scaling = (gtk_major_version > 2 ||
                          (gtk_major_version == 2 && gtk_minor_version >= 12));
    if (gtk_major_version > 3 ||
       (gtk_major_version == 3 && gtk_minor_version >= 14))
        checkbox_check_state = GTK_STATE_FLAG_CHECKED;
    else
        checkbox_check_state = GTK_STATE_FLAG_ACTIVE;

    if (gtk_check_version(3, 12, 0) == nullptr &&
        gtk_check_version(3, 20, 0) != nullptr)
    {
        // Deprecated for Gtk >= 3.20+
        GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_TAB_TOP);
        gtk_style_context_get_style(style,
                                    "has-tab-gap", &notebook_has_tab_gap, NULL);
        ReleaseStyleContext(style);
    }
    else {
        notebook_has_tab_gap = true;
    }

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkbox_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    gtk_widget_style_get(GetWidget(MOZ_GTK_CHECKBUTTON_CONTAINER),
                         "indicator_size", indicator_size,
                         "indicator_spacing", indicator_spacing,
                         NULL);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_radio_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    gtk_widget_style_get(GetWidget(MOZ_GTK_RADIOBUTTON_CONTAINER),
                         "indicator_size", indicator_size,
                         "indicator_spacing", indicator_spacing,
                          NULL);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_get_focus_outline_size(GtkStyleContext* style,
                               gint* focus_h_width, gint* focus_v_width)
{
    GtkBorder border;
    GtkBorder padding;
    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);
    *focus_h_width = border.left + padding.left;
    *focus_v_width = border.top + padding.top;
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_focus_outline_size(gint* focus_h_width, gint* focus_v_width)
{
    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_ENTRY);
    moz_gtk_get_focus_outline_size(style, focus_h_width, focus_v_width);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_menuitem_get_horizontal_padding(gint* horizontal_padding)
{
    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_MENUITEM);
    gtk_style_context_get_style(style,
                                "horizontal-padding", horizontal_padding,
                                nullptr);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkmenuitem_get_horizontal_padding(gint* horizontal_padding)
{
    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_CHECKMENUITEM);
    gtk_style_context_get_style(style,
                                "horizontal-padding", horizontal_padding,
                                nullptr);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_button_get_default_overflow(gint* border_top, gint* border_left,
                                    gint* border_bottom, gint* border_right)
{
    GtkBorder* default_outside_border;

    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_BUTTON);
    gtk_style_context_get_style(style,
                                "default-outside-border", &default_outside_border,
                                NULL);
    ReleaseStyleContext(style);

    if (default_outside_border) {
        *border_top = default_outside_border->top;
        *border_left = default_outside_border->left;
        *border_bottom = default_outside_border->bottom;
        *border_right = default_outside_border->right;
        gtk_border_free(default_outside_border);
    } else {
        *border_top = *border_left = *border_bottom = *border_right = 0;
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_button_get_default_border(gint* border_top, gint* border_left,
                                  gint* border_bottom, gint* border_right)
{
    GtkBorder* default_border;

    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_BUTTON);
    gtk_style_context_get_style(style,
                                "default-border", &default_border,
                                NULL);
    ReleaseStyleContext(style);

    if (default_border) {
        *border_top = default_border->top;
        *border_left = default_border->left;
        *border_bottom = default_border->bottom;
        *border_right = default_border->right;
        gtk_border_free(default_border);
    } else {
        /* see gtkbutton.c */
        *border_top = *border_left = *border_bottom = *border_right = 1;
    }
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_splitter_get_metrics(gint orientation, gint* size)
{
    GtkStyleContext *style;
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        style = ClaimStyleContext(MOZ_GTK_SPLITTER_HORIZONTAL);
    } else {
        style = ClaimStyleContext(MOZ_GTK_SPLITTER_VERTICAL);
    }
    gtk_style_context_get_style(style, "handle_size", size, NULL);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_window_paint(cairo_t *cr, GdkRectangle* rect,
                     GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_WINDOW, direction);

    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_BACKGROUND);
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_style_context_restore(style);

    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_button_paint(cairo_t *cr, GdkRectangle* rect,
                     GtkWidgetState* state,
                     GtkReliefStyle relief, GtkWidget* widget,
                     GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style = gtk_widget_get_style_context(widget);    
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

    gtk_widget_set_direction(widget, direction);
 
    gtk_style_context_save(style);
    gtk_style_context_set_state(style, state_flags);

    if (state->isDefault && relief == GTK_RELIEF_NORMAL) {
        /* handle default borders both outside and inside the button */
        gint default_top, default_left, default_bottom, default_right;
        moz_gtk_button_get_default_overflow(&default_top, &default_left,
                                            &default_bottom, &default_right);
        x -= default_left;
        y -= default_top;
        width += default_left + default_right;
        height += default_top + default_bottom;
        gtk_render_background(style, cr, x, y, width, height);
        gtk_render_frame(style, cr, x, y, width, height);
        moz_gtk_button_get_default_border(&default_top, &default_left,
                                          &default_bottom, &default_right);
        x += default_left;
        y += default_top;
        width -= (default_left + default_right);
        height -= (default_top + default_bottom);
    } else if (relief != GTK_RELIEF_NONE || state->depressed ||
        (state_flags & GTK_STATE_FLAG_PRELIGHT)) {
        /* the following line can trigger an assertion (Crux theme)
           file ../../gdk/gdkwindow.c: line 1846 (gdk_window_clear_area):
           assertion `GDK_IS_WINDOW (window)' failed */
        gtk_render_background(style, cr, x, y, width, height);
        gtk_render_frame(style, cr, x, y, width, height);
    }

    if (state->focused) {
        GtkBorder border;
        gtk_style_context_get_border(style, state_flags, &border);
        x += border.left;
        y += border.top;
        width -= (border.left + border.right);
        height -= (border.top + border.bottom);
        gtk_render_focus(style, cr, x, y, width, height);
    }
    gtk_style_context_restore(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_paint(cairo_t *cr, GdkRectangle* rect,
                     GtkWidgetState* state,
                     gboolean selected, gboolean inconsistent,
                     gboolean isradio, GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    gint indicator_size, indicator_spacing;
    gint x, y, width, height;
    gint focus_x, focus_y, focus_width, focus_height;
    GtkStyleContext *style;

    GtkWidget *widget = GetWidget(isradio ? MOZ_GTK_RADIOBUTTON_CONTAINER :
                                            MOZ_GTK_CHECKBUTTON_CONTAINER);
    gtk_widget_style_get(widget,
                         "indicator_size", &indicator_size,
                         "indicator_spacing", &indicator_spacing,
                         nullptr);

    // XXX we should assert rect->height >= indicator_size too
    // after bug 369581 is fixed.
    MOZ_ASSERT(rect->width >= indicator_size,
               "GetMinimumWidgetSize was ignored");

    // Paint it center aligned in the rect.
    x = rect->x + (rect->width - indicator_size) / 2;
    y = rect->y + (rect->height - indicator_size) / 2;
    width = indicator_size;
    height = indicator_size;

    focus_x = x - indicator_spacing;
    focus_y = y - indicator_spacing;
    focus_width = width + 2 * indicator_spacing;
    focus_height = height + 2 * indicator_spacing;

    if (selected)
        state_flags = static_cast<GtkStateFlags>(state_flags|checkbox_check_state);

    if (inconsistent)
        state_flags = static_cast<GtkStateFlags>(state_flags|GTK_STATE_FLAG_INCONSISTENT);

    style = ClaimStyleContext(isradio ? MOZ_GTK_RADIOBUTTON :
                                        MOZ_GTK_CHECKBUTTON,
                              direction, state_flags);

    if (gtk_check_version(3, 20, 0) == nullptr) {
        gtk_render_background(style, cr, x, y, width, height);
        gtk_render_frame(style, cr, x, y, width, height);
    }

    if (isradio) {
        gtk_render_option(style, cr, x, y, width, height);
        if (state->focused) {
            gtk_render_focus(style, cr, focus_x, focus_y,
                            focus_width, focus_height);
        }
    }
    else {
        gtk_render_check(style, cr, x, y, width, height);
        if (state->focused) {
            gtk_render_focus(style, cr,
                             focus_x, focus_y, focus_width, focus_height);
        }
    }

    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
calculate_button_inner_rect(GtkWidget* button, GdkRectangle* rect,
                            GdkRectangle* inner_rect,
                            GtkTextDirection direction)
{
    GtkStyleContext* style;
    GtkBorder border;
    GtkBorder padding = {0, 0, 0, 0};

    style = gtk_widget_get_style_context(button);

    /* This mirrors gtkbutton's child positioning */
    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);

    inner_rect->x = rect->x + border.left + padding.left;
    inner_rect->y = rect->y + padding.top + border.top;
    inner_rect->width = MAX(1, rect->width - padding.left -
       padding.right - border.left * 2);
    inner_rect->height = MAX(1, rect->height - padding.top -
       padding.bottom - border.top * 2);

    return MOZ_GTK_SUCCESS;
}


static gint
calculate_arrow_rect(GtkWidget* arrow, GdkRectangle* rect,
                     GdkRectangle* arrow_rect, GtkTextDirection direction)
{
    /* defined in gtkarrow.c */
    gfloat arrow_scaling = 0.7;
    gfloat xalign, xpad;
    gint extent;
    gint mxpad, mypad;
    gfloat mxalign, myalign;
    GtkMisc* misc = GTK_MISC(arrow);

    if (have_arrow_scaling)
        gtk_style_context_get_style(gtk_widget_get_style_context(arrow),
                                    "arrow_scaling", &arrow_scaling, NULL);

    gtk_misc_get_padding(misc, &mxpad, &mypad); 
    extent = MIN((rect->width - mxpad * 2),
                 (rect->height - mypad * 2)) * arrow_scaling;

    gtk_misc_get_alignment(misc, &mxalign, &myalign);
    
    xalign = direction == GTK_TEXT_DIR_LTR ? mxalign : 1.0 - mxalign;
    xpad = mxpad + (rect->width - extent) * xalign;

    arrow_rect->x = direction == GTK_TEXT_DIR_LTR ?
                        floor(rect->x + xpad) : ceil(rect->x + xpad);
    arrow_rect->y = floor(rect->y + mypad +
                          ((rect->height - extent) * myalign));

    arrow_rect->width = arrow_rect->height = extent;

    return MOZ_GTK_SUCCESS;
}

void
moz_gtk_get_widget_min_size(WidgetNodeType aGtkWidgetType, int* width,
                            int* height)
{
  GtkStyleContext* style = ClaimStyleContext(aGtkWidgetType);
  GtkStateFlags state_flags = gtk_style_context_get_state(style);
  gtk_style_context_get(style, state_flags,
                        "min-height", height,
                        "min-width", width,
                        nullptr);

  GtkBorder border, padding, margin;
  gtk_style_context_get_border(style, state_flags, &border);
  gtk_style_context_get_padding(style, state_flags, &padding);
  gtk_style_context_get_margin(style, state_flags,  &margin);
  ReleaseStyleContext(style);

  *width += border.left + border.right + margin.left + margin.right +
            padding.left + padding.right;
  *height += border.top + border.bottom + margin.top + margin.bottom +
             padding.top + padding.bottom;
}

static void
Inset(GdkRectangle* rect, GtkBorder& aBorder)
{
    MOZ_ASSERT(rect);
    rect->x += aBorder.left;
    rect->y += aBorder.top;
    rect->width -= aBorder.left + aBorder.right;
    rect->height -= aBorder.top + aBorder.bottom;
}

// Inset a rectangle by the margins specified in a style context.
static void
InsetByMargin(GdkRectangle* rect, GtkStyleContext* style)
{
    MOZ_ASSERT(rect);
    GtkBorder margin;

    gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                                 &margin);
    Inset(rect, margin);
}

// Inset a rectangle by the border and padding specified in a style context.
static void
InsetByBorderPadding(GdkRectangle* rect, GtkStyleContext* style)
{
    GtkStateFlags state = gtk_style_context_get_state(style);
    GtkBorder padding, border;

    gtk_style_context_get_padding(style, state, &padding);
    Inset(rect, padding);
    gtk_style_context_get_border(style, state, &border);
    Inset(rect, border);
}

static gint
moz_gtk_scrollbar_button_paint(cairo_t *cr, const GdkRectangle* aRect,
                               GtkWidgetState* state,
                               GtkScrollbarButtonFlags flags,
                               GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GdkRectangle arrow_rect;
    gdouble arrow_angle;
    GtkStyleContext* style;
    gint arrow_displacement_x, arrow_displacement_y;

    GtkWidget *scrollbar =
        GetWidget(flags & MOZ_GTK_STEPPER_VERTICAL ?
                  MOZ_GTK_SCROLLBAR_VERTICAL : MOZ_GTK_SCROLLBAR_HORIZONTAL);

    gtk_widget_set_direction(scrollbar, direction);

    if (flags & MOZ_GTK_STEPPER_VERTICAL) {
        arrow_angle = (flags & MOZ_GTK_STEPPER_DOWN) ? ARROW_DOWN : ARROW_UP;        
    } else {
        arrow_angle = (flags & MOZ_GTK_STEPPER_DOWN) ? ARROW_RIGHT : ARROW_LEFT;        
    }

    style = gtk_widget_get_style_context(scrollbar);
  
    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_BUTTON);
    gtk_style_context_set_state(style, state_flags);
    if (arrow_angle == ARROW_RIGHT) {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_RIGHT);
    } else if (arrow_angle == ARROW_DOWN) {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_BOTTOM);
    } else if (arrow_angle == ARROW_LEFT) {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_LEFT);
    } else {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_TOP);
    }

    GdkRectangle rect = *aRect;
    if (gtk_check_version(3,20,0) == nullptr) {
      // The "trough-border" is not used since GTK 3.20.  The stepper margin
      // box occupies the full width of the "contents" gadget content box.
      InsetByMargin(&rect, style);
    } else {
      // Scrollbar button has to be inset by trough_border because its DOM
      // element is filling width of vertical scrollbar's track (or height
      // in case of horizontal scrollbars).
      MozGtkScrollbarMetrics metrics;
      moz_gtk_get_scrollbar_metrics(&metrics);
      if (flags & MOZ_GTK_STEPPER_VERTICAL) {
        rect.x += metrics.trough_border;
        rect.width = metrics.slider_width;
      } else {
        rect.y += metrics.trough_border;
        rect.height = metrics.slider_width;
      }
    }

    gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);

    arrow_rect.width = rect.width / 2;
    arrow_rect.height = rect.height / 2;
    
    gfloat arrow_scaling;
    gtk_style_context_get_style(style, "arrow-scaling", &arrow_scaling, NULL);

    gdouble arrow_size = MIN(rect.width, rect.height) * arrow_scaling;
    arrow_rect.x = rect.x + (rect.width - arrow_size) / 2;
    arrow_rect.y = rect.y + (rect.height - arrow_size) / 2;

    if (state_flags & GTK_STATE_FLAG_ACTIVE) {
        gtk_style_context_get_style(style,
                                    "arrow-displacement-x", &arrow_displacement_x,
                                    "arrow-displacement-y", &arrow_displacement_y,
                                    NULL);

        arrow_rect.x += arrow_displacement_x;
        arrow_rect.y += arrow_displacement_y;
    }
  
    gtk_render_arrow(style, cr, arrow_angle,
                     arrow_rect.x,
                     arrow_rect.y, 
                     arrow_size);
  
    gtk_style_context_restore(style);

    return MOZ_GTK_SUCCESS;
}

static void
moz_gtk_update_scrollbar_style(GtkStyleContext* style,
                               WidgetNodeType widget,
                               GtkTextDirection direction)
{
    if (widget == MOZ_GTK_SCROLLBAR_HORIZONTAL) {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_BOTTOM);
    } else {
        if (direction == GTK_TEXT_DIR_LTR) {
            gtk_style_context_add_class(style, GTK_STYLE_CLASS_RIGHT);
            gtk_style_context_remove_class(style, GTK_STYLE_CLASS_LEFT);
        } else {
            gtk_style_context_add_class(style, GTK_STYLE_CLASS_LEFT);
            gtk_style_context_remove_class(style, GTK_STYLE_CLASS_RIGHT);
        }
    }
}

static void
moz_gtk_draw_styled_frame(GtkStyleContext* style, cairo_t *cr,
                          const GdkRectangle* aRect, bool drawFocus)
{
    GdkRectangle rect = *aRect;
    if (gtk_check_version(3, 6, 0) == nullptr) {
        InsetByMargin(&rect, style);
    }
    gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);
    if (drawFocus) {
        gtk_render_focus(style, cr,
                         rect.x, rect.y, rect.width, rect.height);
    }
}

static gint
moz_gtk_scrollbar_trough_paint(WidgetNodeType widget,
                               cairo_t *cr, const GdkRectangle* rect,
                               GtkWidgetState* state,
                               GtkScrollbarTrackFlags flags,
                               GtkTextDirection direction)
{
    if (flags & MOZ_GTK_TRACK_OPAQUE) {
        GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_WINDOW, direction);
        gtk_render_background(style, cr,
                              rect->x, rect->y, rect->width, rect->height);
        ReleaseStyleContext(style);
    }

    GtkStyleContext* style = ClaimStyleContext(widget, direction);
    moz_gtk_draw_styled_frame(style, cr, rect, state->focused);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_paint(WidgetNodeType widget,
                        cairo_t *cr, const GdkRectangle* rect,
                        GtkWidgetState* state,
                        GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(widget, direction);
    moz_gtk_update_scrollbar_style(style, widget, direction);

    moz_gtk_draw_styled_frame(style, cr, rect, state->focused);

    ReleaseStyleContext(style);
    style = ClaimStyleContext((widget == MOZ_GTK_SCROLLBAR_HORIZONTAL) ?
                              MOZ_GTK_SCROLLBAR_CONTENTS_HORIZONTAL :
                              MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL,
                              direction);
    moz_gtk_draw_styled_frame(style, cr, rect, state->focused);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_thumb_paint(WidgetNodeType widget,
                              cairo_t *cr, const GdkRectangle* aRect,
                              GtkWidgetState* state,
                              GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);

    GdkRectangle rect = *aRect;
    GtkStyleContext* style = ClaimStyleContext(widget, direction, state_flags);
    InsetByMargin(&rect, style);

    gtk_render_slider(style, cr,
                      rect.x,
                      rect.y,
                      rect.width,
                      rect.height,
                     (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
                     GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_paint(cairo_t *cr, GdkRectangle* rect,
                   GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_SPINBUTTON, direction);
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_updown_paint(cairo_t *cr, GdkRectangle* rect,
                          gboolean isDown, GtkWidgetState* state,
                          GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_SPINBUTTON, direction,
                                 GetStateFlagsFromGtkWidgetState(state));

    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);

    /* hard code these values */
    GdkRectangle arrow_rect;
    arrow_rect.width = 6;
    arrow_rect.height = 6;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;
    arrow_rect.y += isDown ? -1 : 1;

    gtk_render_arrow(style, cr, 
                    isDown ? ARROW_DOWN : ARROW_UP,
                    arrow_rect.x, arrow_rect.y,
                    arrow_rect.width);

    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

/* See gtk_range_draw() for reference.
*/
static gint
moz_gtk_scale_paint(cairo_t *cr, GdkRectangle* rect,
                    GtkWidgetState* state,
                    GtkOrientation flags, GtkTextDirection direction)
{
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  gint x, y, width, height, min_width, min_height;
  GtkStyleContext* style;
  GtkBorder margin;

  moz_gtk_get_scale_metrics(flags, &min_width, &min_height);

  WidgetNodeType widget = (flags == GTK_ORIENTATION_HORIZONTAL) ?
                          MOZ_GTK_SCALE_TROUGH_HORIZONTAL :
                          MOZ_GTK_SCALE_TROUGH_VERTICAL;
  style = ClaimStyleContext(widget, direction, state_flags);
  gtk_style_context_get_margin(style, state_flags, &margin);

  // Clamp the dimension perpendicular to the direction that the slider crosses
  // to the minimum size.
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    width = rect->width - (margin.left + margin.right);
    height = min_height - (margin.top + margin.bottom);
    x = rect->x + margin.left;
    y = rect->y + (rect->height - height)/2;
  } else {
    width = min_width - (margin.left + margin.right);
    height = rect->height - (margin.top + margin.bottom);
    x = rect->x + (rect->width - width)/2;
    y = rect->y + margin.top;
  }

  gtk_render_background(style, cr, x, y, width, height);
  gtk_render_frame(style, cr, x, y, width, height);

  if (state->focused)
    gtk_render_focus(style, cr, 
                    rect->x, rect->y, rect->width, rect->height);

  ReleaseStyleContext(style);
  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_thumb_paint(cairo_t *cr, GdkRectangle* rect,
                          GtkWidgetState* state,
                          GtkOrientation flags, GtkTextDirection direction)
{
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* style;
  gint thumb_width, thumb_height, x, y;

  /* determine the thumb size, and position the thumb in the center in the opposite axis 
  */
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_width, &thumb_height);
    x = rect->x;
    y = rect->y + (rect->height - thumb_height) / 2;
  }
  else {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height, &thumb_width);
    x = rect->x + (rect->width - thumb_width) / 2;
    y = rect->y;
  }

  WidgetNodeType widget = (flags == GTK_ORIENTATION_HORIZONTAL) ?
                          MOZ_GTK_SCALE_THUMB_HORIZONTAL :
                          MOZ_GTK_SCALE_THUMB_VERTICAL;
  style = ClaimStyleContext(widget, direction, state_flags);
  gtk_render_slider(style, cr, x, y, thumb_width, thumb_height, flags);
  ReleaseStyleContext(style);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_gripper_paint(cairo_t *cr, GdkRectangle* rect,
                      GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStyleContext* style =
            ClaimStyleContext(MOZ_GTK_GRIPPER, direction,
                              GetStateFlagsFromGtkWidgetState(state));
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_hpaned_paint(cairo_t *cr, GdkRectangle* rect,
                     GtkWidgetState* state)
{
    GtkStyleContext* style =
        ClaimStyleContext(MOZ_GTK_SPLITTER_SEPARATOR_HORIZONTAL,
                          GTK_TEXT_DIR_LTR,
                          GetStateFlagsFromGtkWidgetState(state));
    gtk_render_handle(style, cr,
                      rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_vpaned_paint(cairo_t *cr, GdkRectangle* rect,
                     GtkWidgetState* state)
{
    GtkStyleContext* style =
        ClaimStyleContext(MOZ_GTK_SPLITTER_SEPARATOR_VERTICAL,
                          GTK_TEXT_DIR_LTR,
                          GetStateFlagsFromGtkWidgetState(state));
    gtk_render_handle(style, cr,
                      rect->x, rect->y, rect->width, rect->height);                     
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

// See gtk_entry_draw() for reference.
static gint
moz_gtk_entry_paint(cairo_t *cr, GdkRectangle* rect,
                    GtkWidgetState* state,
                    GtkStyleContext* style)
{
    gint x = rect->x, y = rect->y, width = rect->width, height = rect->height;
    int draw_focus_outline_only = state->depressed; // NS_THEME_FOCUS_OUTLINE

    if (draw_focus_outline_only) {
        // Inflate the given 'rect' with the focus outline size.
        gint h, v;
        moz_gtk_get_focus_outline_size(style, &h, &v);
        rect->x -= h;
        rect->width += 2 * h;
        rect->y -= v;
        rect->height += 2 * v;
        width = rect->width;
        height = rect->height;
    } else {
        gtk_render_background(style, cr, x, y, width, height);
    }
    gtk_render_frame(style, cr, x, y, width, height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_text_view_paint(cairo_t *cr, GdkRectangle* rect,
                        GtkWidgetState* state,
                        GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);

    GtkStyleContext* style_frame =
        ClaimStyleContext(MOZ_GTK_SCROLLED_WINDOW, direction, state_flags);
    gtk_render_frame(style_frame, cr, rect->x, rect->y, rect->width, rect->height);

    GtkBorder border, padding;
    gtk_style_context_get_border(style_frame, state_flags, &border);
    gtk_style_context_get_padding(style_frame, state_flags, &padding);
    ReleaseStyleContext(style_frame);

    // There is a separate "text" window, which provides the background behind
    // the text.
    GtkStyleContext* style =
        ClaimStyleContext(MOZ_GTK_TEXT_VIEW_TEXT, direction, state_flags);

    gint xthickness = border.left + padding.left;
    gint ythickness = border.top + padding.top;

    gtk_render_background(style, cr,
                          rect->x + xthickness, rect->y + ythickness,
                          rect->width - 2 * xthickness,
                          rect->height - 2 * ythickness);

    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint 
moz_gtk_treeview_paint(cairo_t *cr, GdkRectangle* rect,
                       GtkWidgetState* state,
                       GtkTextDirection direction)
{
    gint xthickness, ythickness;
    GtkStyleContext *style;
    GtkStyleContext *style_tree;
    GtkStateFlags state_flags;
    GtkBorder border;

    /* only handle disabled and normal states, otherwise the whole background
     * area will be painted differently with other states */
    state_flags = state->disabled ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL;

    style = ClaimStyleContext(MOZ_GTK_SCROLLED_WINDOW, direction);
    gtk_style_context_get_border(style, state_flags, &border);
    xthickness = border.left;
    ythickness = border.top;    
    ReleaseStyleContext(style);

    style_tree = ClaimStyleContext(MOZ_GTK_TREEVIEW_VIEW, direction);
    gtk_render_background(style_tree, cr,
                          rect->x + xthickness, rect->y + ythickness,
                          rect->width - 2 * xthickness,
                          rect->height - 2 * ythickness);
    ReleaseStyleContext(style_tree);

    style = ClaimStyleContext(MOZ_GTK_SCROLLED_WINDOW, direction);
    gtk_render_frame(style, cr, 
                     rect->x, rect->y, rect->width, rect->height); 
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_cell_paint(cairo_t *cr, GdkRectangle* rect,
                               GtkWidgetState* state,
                               gboolean isSorted, GtkTextDirection direction)
{
    moz_gtk_button_paint(cr, rect, state, GTK_RELIEF_NORMAL,
                         GetWidget(MOZ_GTK_TREE_HEADER_CELL), direction);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_sort_arrow_paint(cairo_t *cr, GdkRectangle* rect,
                                     GtkWidgetState* state, GtkArrowType arrow_type,
                                     GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    gdouble arrow_angle;
    GtkStyleContext* style;

    /* hard code these values */
    arrow_rect.width = 11;
    arrow_rect.height = 11;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;
    style = ClaimStyleContext(MOZ_GTK_TREE_HEADER_SORTARROW, direction,
                              GetStateFlagsFromGtkWidgetState(state));
    switch (arrow_type) {
    case GTK_ARROW_LEFT:
        arrow_angle = ARROW_LEFT;
        break;
    case GTK_ARROW_RIGHT:
        arrow_angle = ARROW_RIGHT;
        break;
    case GTK_ARROW_DOWN:
        arrow_angle = ARROW_DOWN;
        break;
    default:
        arrow_angle = ARROW_UP;
        break;
    }
    if (arrow_type != GTK_ARROW_NONE)
        gtk_render_arrow(style, cr, arrow_angle,
                         arrow_rect.x, arrow_rect.y,
                         arrow_rect.width);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

/* See gtk_expander_paint() for reference. 
 */
static gint
moz_gtk_treeview_expander_paint(cairo_t *cr, GdkRectangle* rect,
                                GtkWidgetState* state,
                                GtkExpanderStyle expander_state,
                                GtkTextDirection direction)
{
    /* Because the frame we get is of the entire treeview, we can't get the precise
     * event state of one expander, thus rendering hover and active feedback useless. */
    GtkStateFlags state_flags = state->disabled ? GTK_STATE_FLAG_INSENSITIVE :
                                                  GTK_STATE_FLAG_NORMAL;

    /* GTK_STATE_FLAG_ACTIVE controls expanded/colapsed state rendering
     * in gtk_render_expander()
     */
    if (expander_state == GTK_EXPANDER_EXPANDED)
        state_flags = static_cast<GtkStateFlags>(state_flags|checkbox_check_state);
    else
        state_flags = static_cast<GtkStateFlags>(state_flags&~(checkbox_check_state));

    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_TREEVIEW_EXPANDER,
                                               direction, state_flags);
    gtk_render_expander(style, cr,
                        rect->x,
                        rect->y,
                        rect->width,
                        rect->height);

    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

/* See gtk_separator_draw() for reference.
*/
static gint
moz_gtk_combo_box_paint(cairo_t *cr, GdkRectangle* rect,
                        GtkWidgetState* state,
                        GtkTextDirection direction)
{
    GdkRectangle arrow_rect, real_arrow_rect;
    gint separator_width;
    gboolean wide_separators;
    GtkStyleContext* style;
    GtkRequisition arrow_req;

    GtkWidget* comboBoxButton = GetWidget(MOZ_GTK_COMBOBOX_BUTTON);
    GtkWidget* comboBoxArrow = GetWidget(MOZ_GTK_COMBOBOX_ARROW);

    /* Also sets the direction on gComboBoxButtonWidget, which is then
     * inherited by the separator and arrow */
    moz_gtk_button_paint(cr, rect, state, GTK_RELIEF_NORMAL,
                         comboBoxButton, direction);

    calculate_button_inner_rect(comboBoxButton, rect, &arrow_rect, direction);
    /* Now arrow_rect contains the inner rect ; we want to correct the width
     * to what the arrow needs (see gtk_combo_box_size_allocate) */
    gtk_widget_get_preferred_size(comboBoxArrow, NULL, &arrow_req);

    if (direction == GTK_TEXT_DIR_LTR)
        arrow_rect.x += arrow_rect.width - arrow_req.width;
    arrow_rect.width = arrow_req.width;

    calculate_arrow_rect(comboBoxArrow,
                         &arrow_rect, &real_arrow_rect, direction);

    style = ClaimStyleContext(MOZ_GTK_COMBOBOX_ARROW);
    gtk_render_arrow(style, cr, ARROW_DOWN,
                     real_arrow_rect.x, real_arrow_rect.y,
                     real_arrow_rect.width);
    ReleaseStyleContext(style);

    /* If there is no separator in the theme, there's nothing left to do. */
    GtkWidget* widget = GetWidget(MOZ_GTK_COMBOBOX_SEPARATOR);
    if (!widget)
        return MOZ_GTK_SUCCESS;
    style = gtk_widget_get_style_context(widget);
    gtk_style_context_get_style(style,
                                "wide-separators", &wide_separators,
                                "separator-width", &separator_width,
                                NULL);

    if (wide_separators) {
        if (direction == GTK_TEXT_DIR_LTR)
            arrow_rect.x -= separator_width;
        else
            arrow_rect.x += arrow_rect.width;
        
        gtk_render_frame(style, cr, arrow_rect.x, arrow_rect.y, separator_width, arrow_rect.height);
    } else {
        if (direction == GTK_TEXT_DIR_LTR) {
            GtkBorder padding;
            GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
            gtk_style_context_get_padding(style, state_flags, &padding);
            arrow_rect.x -= padding.left;
        }
        else
            arrow_rect.x += arrow_rect.width;
        
        gtk_render_line(style, cr, 
                        arrow_rect.x, arrow_rect.y, 
                        arrow_rect.x, arrow_rect.y + arrow_rect.height);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_arrow_paint(cairo_t *cr, GdkRectangle* rect,
                    GtkWidgetState* state,
                    GtkArrowType arrow_type, GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    gdouble arrow_angle;

    if (direction == GTK_TEXT_DIR_RTL) {
        if (arrow_type == GTK_ARROW_LEFT) {
            arrow_type = GTK_ARROW_RIGHT;
        } else if (arrow_type == GTK_ARROW_RIGHT) {
            arrow_type = GTK_ARROW_LEFT;
        }
    }
    switch (arrow_type) {
    case GTK_ARROW_LEFT:
        arrow_angle = ARROW_LEFT;
        break;
    case GTK_ARROW_RIGHT:
        arrow_angle = ARROW_RIGHT;
        break;
    case GTK_ARROW_DOWN:
        arrow_angle = ARROW_DOWN;
        break;
    default:
        arrow_angle = ARROW_UP;
        break;
    }
    if (arrow_type == GTK_ARROW_NONE)
        return MOZ_GTK_SUCCESS;

    calculate_arrow_rect(GetWidget(MOZ_GTK_BUTTON_ARROW), rect, &arrow_rect,
                         direction);
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_BUTTON_ARROW,
                                               direction, state_flags);
    gtk_render_arrow(style, cr, arrow_angle,
                     arrow_rect.x, arrow_rect.y, arrow_rect.width);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_combo_box_entry_button_paint(cairo_t *cr, GdkRectangle* rect,
                                     GtkWidgetState* state,
                                     gboolean input_focus,
                                     GtkTextDirection direction)
{
    gint x_displacement, y_displacement;
    GdkRectangle arrow_rect, real_arrow_rect;
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style;

    GtkWidget* comboBoxEntry = GetWidget(MOZ_GTK_COMBOBOX_ENTRY_BUTTON);
    moz_gtk_button_paint(cr, rect, state, GTK_RELIEF_NORMAL,
                         comboBoxEntry, direction);
    calculate_button_inner_rect(comboBoxEntry, rect, &arrow_rect, direction);

    if (state_flags & GTK_STATE_FLAG_ACTIVE) {
        style = gtk_widget_get_style_context(comboBoxEntry);
        gtk_style_context_get_style(style,
                                    "child-displacement-x", &x_displacement,
                                    "child-displacement-y", &y_displacement,
                                    NULL);
        arrow_rect.x += x_displacement;
        arrow_rect.y += y_displacement;
    }

    calculate_arrow_rect(GetWidget(MOZ_GTK_COMBOBOX_ENTRY_ARROW),
                         &arrow_rect, &real_arrow_rect, direction);

    style = ClaimStyleContext(MOZ_GTK_COMBOBOX_ENTRY_ARROW);
    gtk_render_arrow(style, cr, ARROW_DOWN,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_container_paint(cairo_t *cr, GdkRectangle* rect,
                        GtkWidgetState* state, 
                        WidgetNodeType  widget_type,
                        GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style = ClaimStyleContext(widget_type, direction,
                                               state_flags);
    /* this is for drawing a prelight box */
    if (state_flags & GTK_STATE_FLAG_PRELIGHT) {
        gtk_render_background(style, cr,
                              rect->x, rect->y, rect->width, rect->height);
    }

    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_label_paint(cairo_t *cr, GdkRectangle* rect,
                           GtkWidgetState* state, 
                           gboolean isradio, GtkTextDirection direction)
{
    if (!state->focused)
        return MOZ_GTK_SUCCESS;

    GtkStyleContext *style =
        ClaimStyleContext(isradio ? MOZ_GTK_RADIOBUTTON_CONTAINER :
                                    MOZ_GTK_CHECKBUTTON_CONTAINER,
                          direction,
                          GetStateFlagsFromGtkWidgetState(state));
    gtk_render_focus(style, cr,
                    rect->x, rect->y, rect->width, rect->height);

    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toolbar_paint(cairo_t *cr, GdkRectangle* rect,
                      GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TOOLBAR, direction);
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

/* See _gtk_toolbar_paint_space_line() for reference.
*/
static gint
moz_gtk_toolbar_separator_paint(cairo_t *cr, GdkRectangle* rect,
                                GtkTextDirection direction)
{
    gint     separator_width;
    gint     paint_width;
    gboolean wide_separators;
    
    /* Defined as constants in GTK+ 2.10.14 */
    const double start_fraction = 0.2;
    const double end_fraction = 0.8;

    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TOOLBAR);
    gtk_style_context_get_style(style,
                                "wide-separators", &wide_separators,
                                "separator-width", &separator_width,
                                NULL);
    ReleaseStyleContext(style);

    style = ClaimStyleContext(MOZ_GTK_TOOLBAR_SEPARATOR, direction);
    if (wide_separators) {
        if (separator_width > rect->width)
            separator_width = rect->width;
        
        gtk_render_frame(style, cr,
                          rect->x + (rect->width - separator_width) / 2,
                          rect->y + rect->height * start_fraction,
                          separator_width,
                          rect->height * (end_fraction - start_fraction));
    } else {
        GtkBorder padding;
        gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);
    
        paint_width = padding.left;
        if (paint_width > rect->width)
            paint_width = rect->width;
        
        gtk_render_line(style, cr, 
                        rect->x + (rect->width - paint_width) / 2,
                        rect->y + rect->height * start_fraction,
                        rect->x + (rect->width - paint_width) / 2,
                        rect->y + rect->height * end_fraction);
    }
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tooltip_paint(cairo_t *cr, const GdkRectangle* aRect,
                      GtkTextDirection direction)
{
    // Tooltip widget is made in GTK3 as following tree:
    // Tooltip window
    //   Horizontal Box
    //     Icon (not supported by Firefox)
    //     Label
    // Each element can be fully styled by CSS of GTK theme.
    // We have to draw all elements with appropriate offset and right dimensions.

    // Tooltip drawing
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TOOLTIP, direction);
    GdkRectangle rect = *aRect;
    gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);
    ReleaseStyleContext(style);

    // Horizontal Box drawing
    //
    // The box element has hard-coded 6px margin-* GtkWidget properties, which
    // are added between the window dimensions and the CSS margin box of the
    // horizontal box.  The frame of the tooltip window is drawn in the
    // 6px margin.
    // For drawing Horizontal Box we have to inset drawing area by that 6px
    // plus its CSS margin.
    GtkStyleContext* boxStyle = ClaimStyleContext(MOZ_GTK_TOOLTIP_BOX, direction);

    rect.x += 6;
    rect.y += 6;
    rect.width -= 12;
    rect.height -= 12;

    InsetByMargin(&rect, boxStyle);
    gtk_render_background(boxStyle, cr, rect.x, rect.y, rect.width, rect.height);
    gtk_render_frame(boxStyle, cr, rect.x, rect.y, rect.width, rect.height);

    // Label drawing
    InsetByBorderPadding(&rect, boxStyle);
    ReleaseStyleContext(boxStyle);

    GtkStyleContext* labelStyle =
        ClaimStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL, direction);
    moz_gtk_draw_styled_frame(labelStyle, cr, &rect, false);
    ReleaseStyleContext(labelStyle);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_resizer_paint(cairo_t *cr, GdkRectangle* rect,
                      GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStyleContext* style =
        ClaimStyleContext(MOZ_GTK_RESIZER, GTK_TEXT_DIR_LTR,
                          GetStateFlagsFromGtkWidgetState(state));

    // Workaround unico not respecting the text direction for resizers.
    // See bug 1174248.
    cairo_save(cr);
    if (direction == GTK_TEXT_DIR_RTL) {
      cairo_matrix_t mat;
      cairo_matrix_init_translate(&mat, 2 * rect->x + rect->width, 0);
      cairo_matrix_scale(&mat, -1, 1);
      cairo_transform(cr, &mat);
    }

    gtk_render_handle(style, cr, rect->x, rect->y, rect->width, rect->height);
    cairo_restore(cr);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_frame_paint(cairo_t *cr, GdkRectangle* rect,
                    GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_FRAME, direction);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progressbar_paint(cairo_t *cr, GdkRectangle* rect,
                          GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_PROGRESS_TROUGH,
                                               direction);
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progress_chunk_paint(cairo_t *cr, GdkRectangle* rect,
                             GtkTextDirection direction,
                             WidgetNodeType widget)
{
    GtkStyleContext* style =
        ClaimStyleContext(MOZ_GTK_PROGRESS_CHUNK, direction);

    if (widget == MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE ||
        widget == MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE) {
      /**
       * The bar's size and the bar speed are set depending of the progress'
       * size. These could also be constant for all progress bars easily.
       */
      gboolean vertical = (widget == MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE);

      /* The size of the dimension we are going to use for the animation. */
      const gint progressSize = vertical ? rect->height : rect->width;

      /* The bar is using a fifth of the element size, based on GtkProgressBar
       * activity-blocks property. */
      const gint barSize = MAX(1, progressSize / 5);

      /* Represents the travel that has to be done for a complete cycle. */
      const gint travel = 2 * (progressSize - barSize);

      /* period equals to travel / pixelsPerMillisecond
       * where pixelsPerMillisecond equals progressSize / 1000.0.
       * This is equivalent to 1600. */
      static const guint period = 1600;
      const gint t = PR_IntervalToMilliseconds(PR_IntervalNow()) % period;
      const gint dx = travel * t / period;

      if (vertical) {
        rect->y += (dx < travel / 2) ? dx : travel - dx;
        rect->height = barSize;
      } else {
        rect->x += (dx < travel / 2) ? dx : travel - dx;
        rect->width = barSize;
      }
    }

    // gtk_render_activity was used to render progress chunks on GTK versions
    // before 3.13.7, see bug 1173907.
    if (!gtk_check_version(3, 13, 7)) {
      gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
      gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    } else {
      gtk_render_activity(style, cr, rect->x, rect->y, rect->width, rect->height);
    }
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_get_tab_thickness(GtkStyleContext *style)
{
    if (!notebook_has_tab_gap)
      return 0; /* tabs do not overdraw the tabpanel border with "no gap" style */

    GtkBorder border;
    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
    if (border.top < 2)
        return 2; /* some themes don't set ythickness correctly */

    return border.top;
}

gint
moz_gtk_get_tab_thickness(WidgetNodeType aNodeType)
{
    GtkStyleContext *style = ClaimStyleContext(aNodeType);
    int thickness = moz_gtk_get_tab_thickness(style);
    ReleaseStyleContext(style);
    return thickness;
}

/* actual small tabs */
static gint
moz_gtk_tab_paint(cairo_t *cr, GdkRectangle* rect,
                  GtkWidgetState* state,
                  GtkTabFlags flags, GtkTextDirection direction,
                  WidgetNodeType widget)
{
    /* When the tab isn't selected, we just draw a notebook extension.
     * When it is selected, we overwrite the adjacent border of the tabpanel
     * touching the tab with a pierced border (called "the gap") to make the
     * tab appear physically attached to the tabpanel; see details below. */

    GtkStyleContext* style;
    GdkRectangle tabRect;
    GdkRectangle focusRect;
    GdkRectangle backRect;
    int initial_gap = 0;
    bool isBottomTab = (widget == MOZ_GTK_TAB_BOTTOM);

    style = ClaimStyleContext(widget, direction,
                              GetStateFlagsFromGtkTabFlags(flags));
    tabRect = *rect;

    if (flags & MOZ_GTK_TAB_FIRST) {
        gtk_style_context_get_style(style, "initial-gap", &initial_gap, NULL);
        tabRect.width -= initial_gap;

        if (direction != GTK_TEXT_DIR_RTL) {
            tabRect.x += initial_gap;
        }
    }

    focusRect = backRect = tabRect;

    if (notebook_has_tab_gap) {
        if ((flags & MOZ_GTK_TAB_SELECTED) == 0) {
            /* Only draw the tab */
            gtk_render_extension(style, cr,
                                 tabRect.x, tabRect.y, tabRect.width, tabRect.height,
                                 isBottomTab ? GTK_POS_TOP : GTK_POS_BOTTOM );
        } else {
            /* Draw the tab and the gap
             * We want the gap to be positioned exactly on the tabpanel top
             * border; since tabbox.css may set a negative margin so that the tab
             * frame rect already overlaps the tabpanel frame rect, we need to take
             * that into account when drawing. To that effect, nsNativeThemeGTK
             * passes us this negative margin (bmargin in the graphic below) in the
             * lowest bits of |flags|.  We use it to set gap_voffset, the distance
             * between the top of the gap and the bottom of the tab (resp. the
             * bottom of the gap and the top of the tab when we draw a bottom tab),
             * while ensuring that the gap always touches the border of the tab,
             * i.e. 0 <= gap_voffset <= gap_height, to avoid surprinsing results
             * with big negative or positive margins.
             * Here is a graphical explanation in the case of top tabs:
             *             ___________________________
             *            /                           \
             *           |            T A B            |
             * ----------|. . . . . . . . . . . . . . .|----- top of tabpanel
             *           :    ^       bmargin          :  ^
             *           :    | (-negative margin,     :  |
             *  bottom   :    v  passed in flags)      :  |       gap_height
             *    of  -> :.............................:  |    (the size of the
             * the tab   .       part of the gap       .  |  tabpanel top border)
             *           .      outside of the tab     .  v
             * ----------------------------------------------
             *
             * To draw the gap, we use gtk_render_frame_gap(), see comment in
             * moz_gtk_tabpanels_paint(). This gap is made 3 * gap_height tall,
             * which should suffice to ensure that the only visible border is the
             * pierced one.  If the tab is in the middle, we make the box_gap begin
             * a bit to the left of the tab and end a bit to the right, adjusting
             * the gap position so it still is under the tab, because we want the
             * rendering of a gap in the middle of a tabpanel.  This is the role of
             * the gints gap_{l,r}_offset. On the contrary, if the tab is the
             * first, we align the start border of the box_gap with the start
             * border of the tab (left if LTR, right if RTL), by setting the
             * appropriate offset to 0.*/
            gint gap_loffset, gap_roffset, gap_voffset, gap_height;

            /* Get height needed by the gap */
            gap_height = moz_gtk_get_tab_thickness(style);

            /* Extract gap_voffset from the first bits of flags */
            gap_voffset = flags & MOZ_GTK_TAB_MARGIN_MASK;
            if (gap_voffset > gap_height)
                gap_voffset = gap_height;

            /* Set gap_{l,r}_offset to appropriate values */
            gap_loffset = gap_roffset = 20; /* should be enough */
            if (flags & MOZ_GTK_TAB_FIRST) {
                if (direction == GTK_TEXT_DIR_RTL)
                    gap_roffset = initial_gap;
                else
                    gap_loffset = initial_gap;
            }

            GtkStyleContext* panelStyle =
                ClaimStyleContext(MOZ_GTK_TABPANELS, direction);

            if (isBottomTab) {
                /* Draw the tab on bottom */
                focusRect.y += gap_voffset;
                focusRect.height -= gap_voffset;

                gtk_render_extension(style, cr,
                                     tabRect.x, tabRect.y + gap_voffset, tabRect.width,
                                     tabRect.height - gap_voffset, GTK_POS_TOP);

                backRect.y += (gap_voffset - gap_height);
                backRect.height = gap_height;

                /* Draw the gap; erase with background color before painting in
                 * case theme does not */
                gtk_render_background(panelStyle, cr, backRect.x, backRect.y,
                                     backRect.width, backRect.height);
                cairo_save(cr);
                cairo_rectangle(cr, backRect.x, backRect.y, backRect.width, backRect.height);
                cairo_clip(cr);

                gtk_render_frame_gap(panelStyle, cr,
                                     tabRect.x - gap_loffset,
                                     tabRect.y + gap_voffset - 3 * gap_height,
                                     tabRect.width + gap_loffset + gap_roffset,
                                     3 * gap_height, GTK_POS_BOTTOM,
                                     gap_loffset, gap_loffset + tabRect.width);
                cairo_restore(cr);
            } else {
                /* Draw the tab on top */
                focusRect.height -= gap_voffset;
                gtk_render_extension(style, cr,
                                     tabRect.x, tabRect.y, tabRect.width,
                                     tabRect.height - gap_voffset, GTK_POS_BOTTOM);

                backRect.y += (tabRect.height - gap_voffset);
                backRect.height = gap_height;

                /* Draw the gap; erase with background color before painting in
                 * case theme does not */
                gtk_render_background(panelStyle, cr, backRect.x, backRect.y,
                                      backRect.width, backRect.height);

                cairo_save(cr);
                cairo_rectangle(cr, backRect.x, backRect.y, backRect.width, backRect.height);
                cairo_clip(cr);

                gtk_render_frame_gap(panelStyle, cr,
                                     tabRect.x - gap_loffset,
                                     tabRect.y + tabRect.height - gap_voffset,
                                     tabRect.width + gap_loffset + gap_roffset,
                                     3 * gap_height, GTK_POS_TOP,
                                     gap_loffset, gap_loffset + tabRect.width);
                cairo_restore(cr);
            }
        }
    } else {
        gtk_render_background(style, cr, tabRect.x, tabRect.y, tabRect.width, tabRect.height);
        gtk_render_frame(style, cr, tabRect.x, tabRect.y, tabRect.width, tabRect.height);
    }

    if (state->focused) {
      /* Paint the focus ring */
      GtkBorder padding;
      gtk_style_context_get_padding(style, GetStateFlagsFromGtkWidgetState(state), &padding);

      focusRect.x += padding.left;
      focusRect.width -= (padding.left + padding.right);
      focusRect.y += padding.top;
      focusRect.height -= (padding.top + padding.bottom);

      gtk_render_focus(style, cr,
                      focusRect.x, focusRect.y, focusRect.width, focusRect.height);
    }
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

/* tab area*/
static gint
moz_gtk_tabpanels_paint(cairo_t *cr, GdkRectangle* rect,
                        GtkTextDirection direction)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TABPANELS, direction);
    gtk_render_background(style, cr, rect->x, rect->y, 
                          rect->width, rect->height);
    /*
     * The gap size is not needed in moz_gtk_tabpanels_paint because 
     * the gap will be painted with the foreground tab in moz_gtk_tab_paint.
     *
     * However, if moz_gtk_tabpanels_paint just uses gtk_render_frame(), 
     * the theme will think that there are no tabs and may draw something 
     * different.Hence the trick of using two clip regions, and drawing the 
     * gap outside each clip region, to get the correct frame for 
     * a tabpanel with tabs.
     */
    /* left side */
    cairo_save(cr);
    cairo_rectangle(cr, rect->x, rect->y,
                    rect->x + rect->width / 2,
                    rect->y + rect->height);
    cairo_clip(cr);
    gtk_render_frame_gap(style, cr,
                         rect->x, rect->y,
                         rect->width, rect->height,
                         GTK_POS_TOP, rect->width - 1, rect->width);
    cairo_restore(cr);

    /* right side */
    cairo_save(cr);
    cairo_rectangle(cr, rect->x + rect->width / 2, rect->y,
                    rect->x + rect->width,
                    rect->y + rect->height);
    cairo_clip(cr);
    gtk_render_frame_gap(style, cr,
                         rect->x, rect->y,
                         rect->width, rect->height,
                         GTK_POS_TOP, 0, 1);
    cairo_restore(cr);

    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tab_scroll_arrow_paint(cairo_t *cr, GdkRectangle* rect,
                               GtkWidgetState* state,
                               GtkArrowType arrow_type,
                               GtkTextDirection direction)
{
    GtkStyleContext* style;
    gdouble arrow_angle;
    gint arrow_size = MIN(rect->width, rect->height);
    gint x = rect->x + (rect->width - arrow_size) / 2;
    gint y = rect->y + (rect->height - arrow_size) / 2;

    if (direction == GTK_TEXT_DIR_RTL) {
        arrow_type = (arrow_type == GTK_ARROW_LEFT) ?
                         GTK_ARROW_RIGHT : GTK_ARROW_LEFT;
    }    
    switch (arrow_type) {
    case GTK_ARROW_LEFT:
        arrow_angle = ARROW_LEFT;
        break;
    case GTK_ARROW_RIGHT:
        arrow_angle = ARROW_RIGHT;
        break;
    case GTK_ARROW_DOWN:
        arrow_angle = ARROW_DOWN;
        break;
    default:
        arrow_angle = ARROW_UP;
        break;      
    }
    if (arrow_type != GTK_ARROW_NONE)  {        
        style = ClaimStyleContext(MOZ_GTK_TAB_SCROLLARROW, direction,
                                  GetStateFlagsFromGtkWidgetState(state));
        gtk_render_arrow(style, cr, arrow_angle,
                         x, y, arrow_size);
        ReleaseStyleContext(style);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_bar_paint(cairo_t *cr, GdkRectangle* rect,
                       GtkTextDirection direction)
{
    GtkStyleContext* style;

    GtkWidget* widget = GetWidget(MOZ_GTK_MENUBAR);
    gtk_widget_set_direction(widget, direction);

    style = gtk_widget_get_style_context(widget);
    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_MENUBAR);
    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_style_context_restore(style);
    
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_popup_paint(cairo_t *cr, GdkRectangle* rect,
                         GtkTextDirection direction)
{
    GtkStyleContext* style;

    GtkWidget* widget = GetWidget(MOZ_GTK_MENUPOPUP);
    gtk_widget_set_direction(widget, direction);

    // Draw a backing toplevel. This fixes themes that don't provide a menu
    // background, and depend on the GtkMenu's implementation window to provide it.
    moz_gtk_window_paint(cr, rect, direction);

    style = gtk_widget_get_style_context(widget);
    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_MENU);

    gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    gtk_style_context_restore(style);

    return MOZ_GTK_SUCCESS;
}

// See gtk_menu_item_draw() for reference.
static gint
moz_gtk_menu_separator_paint(cairo_t *cr, GdkRectangle* rect,
                             GtkTextDirection direction)
{
    GtkWidgetState defaultState = { 0 };
    moz_gtk_menu_item_paint(MOZ_GTK_MENUSEPARATOR, cr, rect,
                            &defaultState, direction);

    if (gtk_get_minor_version() >= 20)
        return MOZ_GTK_SUCCESS;

    GtkStyleContext* style;
    gboolean wide_separators;
    gint separator_height;
    gint x, y, w;
    GtkBorder padding;

    style = ClaimStyleContext(MOZ_GTK_MENUSEPARATOR, direction);
    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);

    x = rect->x;
    y = rect->y;
    w = rect->width;

    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_SEPARATOR);

    gtk_style_context_get_style(style,
                                "wide-separators",    &wide_separators,
                                "separator-height",   &separator_height,
                                NULL);

    if (wide_separators) {
      gtk_render_frame(style, cr,
                       x + padding.left,
                       y + padding.top,
                       w - padding.left - padding.right,
                       separator_height);
    } else {
      gtk_render_line(style, cr,
                      x + padding.left,
                      y + padding.top,
                      x + w - padding.right - 1,
                      y + padding.top);
    }

    gtk_style_context_restore(style);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

// See gtk_menu_item_draw() for reference.
static gint
moz_gtk_menu_item_paint(WidgetNodeType widget, cairo_t *cr, GdkRectangle* rect,
                        GtkWidgetState* state, GtkTextDirection direction)
{
    gint x, y, w, h;

    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style = ClaimStyleContext(widget, direction, state_flags);

    bool pre_3_6 = gtk_check_version(3, 6, 0) != nullptr;
    if (pre_3_6) {
        // GTK+ 3.4 saves the style context and adds the menubar class to
        // menubar children, but does each of these only when drawing, not
        // during layout.
        gtk_style_context_save(style);
        if (widget == MOZ_GTK_MENUBARITEM) {
            gtk_style_context_add_class(style, GTK_STYLE_CLASS_MENUBAR);
        }
    }

    x = rect->x;
    y = rect->y;
    w = rect->width;
    h = rect->height;

    gtk_render_background(style, cr, x, y, w, h);
    gtk_render_frame(style, cr, x, y, w, h);

    if (pre_3_6) {
        gtk_style_context_restore(style);
    }
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_arrow_paint(cairo_t *cr, GdkRectangle* rect,
                         GtkWidgetState* state,
                         GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_MENUITEM,
                                               direction, state_flags);
    gtk_render_arrow(style, cr,
                    (direction == GTK_TEXT_DIR_LTR) ? ARROW_RIGHT : ARROW_LEFT,
                    rect->x, rect->y, rect->width);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

// For reference, see gtk_check_menu_item_size_allocate() in GTK versions after
// 3.20 and gtk_real_check_menu_item_draw_indicator() in earlier versions.
static gint
moz_gtk_check_menu_item_paint(WidgetNodeType widgetType,
                              cairo_t *cr, GdkRectangle* rect,
                              GtkWidgetState* state,
                              gboolean checked, GtkTextDirection direction)
{
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
    GtkStyleContext* style;
    gint indicator_size, horizontal_padding;
    gint x, y;

    moz_gtk_menu_item_paint(MOZ_GTK_MENUITEM, cr, rect, state, direction);

    if (checked) {
      state_flags = static_cast<GtkStateFlags>(state_flags|checkbox_check_state);
    }

    bool pre_3_20 = gtk_get_minor_version() < 20;
    gint offset;
    style = ClaimStyleContext(widgetType, direction);
    gtk_style_context_get_style(style,
                                "indicator-size", &indicator_size,
                                "horizontal-padding", &horizontal_padding,
                                NULL);
    if (pre_3_20) {
        GtkBorder padding;
        gtk_style_context_get_padding(style, state_flags, &padding);
        offset = horizontal_padding + padding.left + 2;
    } else {
        GdkRectangle r = { 0 };
        InsetByMargin(&r, style);
        InsetByBorderPadding(&r, style);
        offset = r.x;
    }
    ReleaseStyleContext(style);

    bool isRadio = (widgetType == MOZ_GTK_RADIOMENUITEM);
    WidgetNodeType indicatorType = isRadio ? MOZ_GTK_RADIOMENUITEM_INDICATOR
                                           : MOZ_GTK_CHECKMENUITEM_INDICATOR;
    style = ClaimStyleContext(indicatorType, direction, state_flags);

    if (direction == GTK_TEXT_DIR_RTL) {
        x = rect->width - indicator_size - offset;
    }
    else {
        x = rect->x + offset;
    }
    y = rect->y + (rect->height - indicator_size) / 2;

    if (!pre_3_20) {
        gtk_render_background(style, cr, x, y, indicator_size, indicator_size);
        gtk_render_frame(style, cr, x, y, indicator_size, indicator_size);
    }

    if (isRadio) {
      gtk_render_option(style, cr, x, y, indicator_size, indicator_size);
    } else {
      gtk_render_check(style, cr, x, y, indicator_size, indicator_size);
    }
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_info_bar_paint(cairo_t *cr, GdkRectangle* rect,
                       GtkWidgetState* state)
{
    GtkStyleContext *style =
        ClaimStyleContext(MOZ_GTK_INFO_BAR, GTK_TEXT_DIR_LTR,
                          GetStateFlagsFromGtkWidgetState(state));
    gtk_render_background(style, cr, rect->x, rect->y, rect->width,
                          rect->height);
    gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

static void
moz_gtk_add_style_margin(GtkStyleContext* style,
                         gint* left, gint* top, gint* right, gint* bottom)
{
    GtkBorder margin;

    gtk_style_context_get_margin(style, GTK_STATE_FLAG_NORMAL, &margin);

    *left += margin.left;
    *right += margin.right;
    *top += margin.top;
    *bottom += margin.bottom;
}

static void
moz_gtk_add_style_border(GtkStyleContext* style,
                         gint* left, gint* top, gint* right, gint* bottom)
{
    GtkBorder border;

    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);

    *left += border.left;
    *right += border.right;
    *top += border.top;
    *bottom += border.bottom;
}

static void
moz_gtk_add_style_padding(GtkStyleContext* style,
                          gint* left, gint* top, gint* right, gint* bottom)
{
    GtkBorder padding;

    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);

    *left += padding.left;
    *right += padding.right;
    *top += padding.top;
    *bottom += padding.bottom;
}

static void moz_gtk_add_margin_border_padding(GtkStyleContext *style,
                                              gint* left, gint* top,
                                              gint* right, gint* bottom)
{
    moz_gtk_add_style_margin(style, left, top, right, bottom);
    moz_gtk_add_style_border(style, left, top, right, bottom);
    moz_gtk_add_style_padding(style, left, top, right, bottom);
}

gint
moz_gtk_get_widget_border(WidgetNodeType widget, gint* left, gint* top,
                          gint* right, gint* bottom, GtkTextDirection direction,
                          gboolean inhtml)
{
    GtkWidget* w;
    GtkStyleContext* style;
    *left = *top = *right = *bottom = 0;

    switch (widget) {
    case MOZ_GTK_BUTTON:
    case MOZ_GTK_TOOLBAR_BUTTON:
        {
            style = ClaimStyleContext(MOZ_GTK_BUTTON);

            *left = *top = *right = *bottom =
                gtk_container_get_border_width(GTK_CONTAINER(GetWidget(MOZ_GTK_BUTTON)));

            if (widget == MOZ_GTK_TOOLBAR_BUTTON) {
                gtk_style_context_save(style);
                gtk_style_context_add_class(style, "image-button");
            }

            moz_gtk_add_style_padding(style, left, top, right, bottom);

            if (widget == MOZ_GTK_TOOLBAR_BUTTON)
                gtk_style_context_restore(style);

            // XXX: Subtract 1 pixel from the border to account for the added
            // -moz-focus-inner border (Bug 1228281).
            *left -= 1; *top -= 1; *right -= 1; *bottom -= 1;
            moz_gtk_add_style_border(style, left, top, right, bottom);

            ReleaseStyleContext(style);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_ENTRY:
        {
            style = ClaimStyleContext(MOZ_GTK_ENTRY);

            // XXX: Subtract 1 pixel from the padding to account for the default
            // padding in forms.css. See bug 1187385.
            *left = *top = *right = *bottom = -1;
            moz_gtk_add_style_padding(style, left, top, right, bottom);
            moz_gtk_add_style_border(style, left, top, right, bottom);

            ReleaseStyleContext(style);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TEXT_VIEW:
    case MOZ_GTK_TREEVIEW:
        {
            style = ClaimStyleContext(MOZ_GTK_SCROLLED_WINDOW);
            moz_gtk_add_style_border(style, left, top, right, bottom);
            ReleaseStyleContext(style);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TREE_HEADER_CELL:
        {
            /* A Tree Header in GTK is just a different styled button
             * It must be placed in a TreeView for getting the correct style
             * assigned.
             * That is why the following code is the same as for MOZ_GTK_BUTTON.
             * */
            *left = *top = *right = *bottom =
                gtk_container_get_border_width(GTK_CONTAINER(
                                               GetWidget(MOZ_GTK_TREE_HEADER_CELL)));

            style = ClaimStyleContext(MOZ_GTK_TREE_HEADER_CELL);
            moz_gtk_add_style_border(style, left, top, right, bottom);
            moz_gtk_add_style_padding(style, left, top, right, bottom);
            ReleaseStyleContext(style);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        w = GetWidget(MOZ_GTK_TREE_HEADER_SORTARROW);
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        w = GetWidget(MOZ_GTK_COMBOBOX_ENTRY_TEXTAREA);
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        w = GetWidget(MOZ_GTK_COMBOBOX_ENTRY_BUTTON);
        break;
    case MOZ_GTK_DROPDOWN:
        {
            /* We need to account for the arrow on the dropdown, so text
             * doesn't come too close to the arrow, or in some cases spill
             * into the arrow. */
            gboolean wide_separators;
            gint separator_width;
            GtkRequisition arrow_req;
            GtkBorder border;

            *left = *top = *right = *bottom =
                gtk_container_get_border_width(GTK_CONTAINER(
                                               GetWidget(MOZ_GTK_COMBOBOX_BUTTON)));
            style = ClaimStyleContext(MOZ_GTK_COMBOBOX_BUTTON);
            moz_gtk_add_style_padding(style, left, top, right, bottom);
            moz_gtk_add_style_border(style, left, top, right, bottom);
            ReleaseStyleContext(style);

            /* If there is no separator, don't try to count its width. */
            separator_width = 0;
            GtkWidget* comboBoxSeparator = GetWidget(MOZ_GTK_COMBOBOX_SEPARATOR);
            if (comboBoxSeparator) {
                style = gtk_widget_get_style_context(comboBoxSeparator);
                gtk_style_context_get_style(style,
                                            "wide-separators", &wide_separators,
                                            "separator-width", &separator_width,
                                            NULL);

                if (!wide_separators) {
                    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL,
                                                 &border);
                    separator_width = border.left;
                }
            }

            gtk_widget_get_preferred_size(GetWidget(MOZ_GTK_COMBOBOX_ARROW),
                                          NULL, &arrow_req);

            if (direction == GTK_TEXT_DIR_RTL)
                *left += separator_width + arrow_req.width;
            else
                *right += separator_width + arrow_req.width;

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TABPANELS:
        w = GetWidget(MOZ_GTK_TABPANELS);
        break;
    case MOZ_GTK_PROGRESSBAR:
        w = GetWidget(MOZ_GTK_PROGRESSBAR);
        break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        w = GetWidget(MOZ_GTK_SPINBUTTON);
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        w = GetWidget(widget);
        break;
    case MOZ_GTK_FRAME:
        w = GetWidget(MOZ_GTK_FRAME);
        break;
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        {
            w = GetWidget(widget);
            style = gtk_widget_get_style_context(w);

            *left = *top = *right = *bottom = gtk_container_get_border_width(GTK_CONTAINER(w));
            moz_gtk_add_style_border(style,
                                     left, top, right, bottom);
            moz_gtk_add_style_padding(style,
                                      left, top, right, bottom);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_MENUPOPUP:
        w = GetWidget(MOZ_GTK_MENUPOPUP);
        break;
    case MOZ_GTK_MENUBARITEM:
    case MOZ_GTK_MENUITEM:
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        {
            // Bug 1274143 for MOZ_GTK_MENUBARITEM
            WidgetNodeType type =
                widget == MOZ_GTK_MENUBARITEM ? MOZ_GTK_MENUITEM : widget;
            style = ClaimStyleContext(type);

            if (gtk_get_minor_version() < 20) {
                moz_gtk_add_style_padding(style, left, top, right, bottom);
            } else {
                moz_gtk_add_margin_border_padding(style,
                                                  left, top, right, bottom);
            }
            ReleaseStyleContext(style);
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_INFO_BAR:
        w = GetWidget(MOZ_GTK_INFO_BAR);
        break;
    case MOZ_GTK_TOOLTIP:
        {
            // In GTK 3 there are 6 pixels of additional margin around the box.
            // See details there:
            // https://github.com/GNOME/gtk/blob/5ea69a136bd7e4970b3a800390e20314665aaed2/gtk/ui/gtktooltipwindow.ui#L11
            *left = *right = *top = *bottom = 6;

            // We also need to add margin/padding/borders from Tooltip content.
            // Tooltip contains horizontal box, where icon and label is put.
            // We ignore icon as long as we don't have support for it.
            GtkStyleContext* boxStyle = ClaimStyleContext(MOZ_GTK_TOOLTIP_BOX);
            moz_gtk_add_margin_border_padding(boxStyle,
                                              left, top, right, bottom);
            ReleaseStyleContext(boxStyle);

            GtkStyleContext* labelStyle = ClaimStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL);
            moz_gtk_add_margin_border_padding(labelStyle,
                                              left, top, right, bottom);
            ReleaseStyleContext(labelStyle);

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_SCROLLBAR_VERTICAL:
    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
        {
          if (gtk_check_version(3,20,0) == nullptr) {
            style = ClaimStyleContext(widget);
            moz_gtk_add_margin_border_padding(style, left, top, right, bottom);
            ReleaseStyleContext(style);
            if (widget == MOZ_GTK_SCROLLBAR_VERTICAL) {
              style = ClaimStyleContext(MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL);
              moz_gtk_add_margin_border_padding(style, left, top, right, bottom);
              ReleaseStyleContext(style);
            }
          } else {
            MozGtkScrollbarMetrics metrics;
            moz_gtk_get_scrollbar_metrics(&metrics);
            /* Top and bottom border for whole vertical scrollbar, top and bottom
             * border for horizontal track - to correctly position thumb element */
            *top = *bottom = metrics.trough_border;
          }
          return MOZ_GTK_SUCCESS;
        }
        break;

    case MOZ_GTK_SCROLLBAR_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
        {
          if (gtk_check_version(3,20,0) == nullptr) {
            style = ClaimStyleContext(widget);
            moz_gtk_add_margin_border_padding(style, left, top, right, bottom);
            ReleaseStyleContext(style);
            if (widget == MOZ_GTK_SCROLLBAR_HORIZONTAL) {
              style = ClaimStyleContext(MOZ_GTK_SCROLLBAR_CONTENTS_HORIZONTAL);
              moz_gtk_add_margin_border_padding(style, left, top, right, bottom);
              ReleaseStyleContext(style);
            }
          } else {
            MozGtkScrollbarMetrics metrics;
            moz_gtk_get_scrollbar_metrics(&metrics);
            *left = *right = metrics.trough_border;
          }
          return MOZ_GTK_SUCCESS;
        }
        break;

    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        {
          if (gtk_check_version(3,20,0) == nullptr) {
            style = ClaimStyleContext(widget);
            moz_gtk_add_margin_border_padding(style, left, top, right, bottom);
            ReleaseStyleContext(style);
          }
          return MOZ_GTK_SUCCESS;
        }
        break;
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
    case MOZ_GTK_SPLITTER_HORIZONTAL:
    case MOZ_GTK_SPLITTER_VERTICAL:
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
    case MOZ_GTK_SCROLLBAR_BUTTON:
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
    case MOZ_GTK_GRIPPER:
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE:
    case MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE:
    case MOZ_GTK_TREEVIEW_EXPANDER:
    case MOZ_GTK_TOOLBAR_SEPARATOR:
    case MOZ_GTK_MENUSEPARATOR:
    /* These widgets have no borders.*/
    case MOZ_GTK_SPINBUTTON:
    case MOZ_GTK_WINDOW:
    case MOZ_GTK_RESIZER:
    case MOZ_GTK_MENUARROW:
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
    case MOZ_GTK_TOOLBAR:
    case MOZ_GTK_MENUBAR:
    case MOZ_GTK_TAB_SCROLLARROW:
        return MOZ_GTK_SUCCESS;
    default:
        g_warning("Unsupported widget type: %d", widget);
        return MOZ_GTK_UNKNOWN_WIDGET;
    }
    /* TODO - we're still missing some widget implementations */
    if (w) {
      moz_gtk_add_style_border(gtk_widget_get_style_context(w), 
                               left, top, right, bottom);
    }
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_tab_border(gint* left, gint* top, gint* right, gint* bottom, 
                       GtkTextDirection direction, GtkTabFlags flags, 
                       WidgetNodeType widget)
{
    GtkStyleContext* style = ClaimStyleContext(widget, direction,
                              GetStateFlagsFromGtkTabFlags(flags));

    *left = *top = *right = *bottom = 0;
    moz_gtk_add_style_padding(style, left, top, right, bottom);

    // Gtk >= 3.20 does not use those styles
    if (gtk_check_version(3, 20, 0) != nullptr) {
        int tab_curvature;

        gtk_style_context_get_style(style, "tab-curvature", &tab_curvature, NULL);
        *left += tab_curvature;
        *right += tab_curvature;

        if (flags & MOZ_GTK_TAB_FIRST) {
            int initial_gap = 0;
            gtk_style_context_get_style(style, "initial-gap", &initial_gap, NULL);
            if (direction == GTK_TEXT_DIR_RTL)
                *right += initial_gap;
            else
                *left += initial_gap;
        }
    } else {
        GtkBorder margin;

        gtk_style_context_get_margin(style, GTK_STATE_FLAG_NORMAL, &margin);
        *left += margin.left;
        *right += margin.right;

        if (flags & MOZ_GTK_TAB_FIRST) {
            ReleaseStyleContext(style);
            style = ClaimStyleContext(MOZ_GTK_NOTEBOOK_HEADER, direction);
            gtk_style_context_get_margin(style, GTK_STATE_FLAG_NORMAL, &margin);
            *left += margin.left;
            *right += margin.right;
        }
    }

    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_combo_box_entry_button_size(gint* width, gint* height)
{
    /*
     * We get the requisition of the drop down button, which includes
     * all padding, border and focus line widths the button uses,
     * as well as the minimum arrow size and its padding
     * */
    GtkRequisition requisition;

    gtk_widget_get_preferred_size(GetWidget(MOZ_GTK_COMBOBOX_ENTRY_BUTTON),
                                  NULL, &requisition);
    *width = requisition.width;
    *height = requisition.height;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_tab_scroll_arrow_size(gint* width, gint* height)
{
    gint arrow_size;

    GtkStyleContext *style = ClaimStyleContext(MOZ_GTK_TABPANELS);
    gtk_style_context_get_style(style,
                                "scroll-arrow-hlength", &arrow_size,
                                NULL);
    ReleaseStyleContext(style);

    *height = *width = arrow_size;

    return MOZ_GTK_SUCCESS;
}

void
moz_gtk_get_arrow_size(WidgetNodeType widgetType, gint* width, gint* height)
{
    GtkWidget* widget;
    switch (widgetType) {
        case MOZ_GTK_DROPDOWN:
            widget = GetWidget(MOZ_GTK_COMBOBOX_ARROW);
            break;
        default:
            widget = GetWidget(MOZ_GTK_BUTTON_ARROW);
            break;
    }

    GtkRequisition requisition;
    gtk_widget_get_preferred_size(widget, NULL, &requisition);
    *width = requisition.width;
    *height = requisition.height;
}

gint
moz_gtk_get_toolbar_separator_width(gint* size)
{
    gboolean wide_separators;
    gint separator_width;
    GtkBorder border;

    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TOOLBAR);
    gtk_style_context_get_style(style,
                                "space-size", size,
                                "wide-separators",  &wide_separators,
                                "separator-width", &separator_width,
                                NULL);
    /* Just in case... */
    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
    *size = MAX(*size, (wide_separators ? separator_width : border.left));
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_expander_size(gint* size)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_EXPANDER);
    gtk_style_context_get_style(style,
                                "expander-size", size,
                                NULL);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_treeview_expander_size(gint* size)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_TREEVIEW);
    gtk_style_context_get_style(style, "expander-size", size, NULL);
    ReleaseStyleContext(style);
    return MOZ_GTK_SUCCESS;
}

// See gtk_menu_item_draw() for reference.
gint
moz_gtk_get_menu_separator_height(gint *size)
{
    gboolean  wide_separators;
    gint      separator_height;
    GtkBorder padding;
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_MENUSEPARATOR);
    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);

    gtk_style_context_save(style);
    gtk_style_context_add_class(style, GTK_STYLE_CLASS_SEPARATOR);

    gtk_style_context_get_style(style,
                                "wide-separators",  &wide_separators,
                                "separator-height", &separator_height,
                                NULL);

    gtk_style_context_restore(style);
    ReleaseStyleContext(style);

    *size = padding.top + padding.bottom;
    *size += (wide_separators) ? separator_height : 1;

    return MOZ_GTK_SUCCESS;
}

void
moz_gtk_get_entry_min_height(gint* height)
{
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_ENTRY);
    if (!gtk_check_version(3, 20, 0)) {
        gtk_style_context_get(style, gtk_style_context_get_state(style),
                              "min-height", height,
                              nullptr);
    } else {
        *height = 0;
    }

    GtkBorder border;
    GtkBorder padding;
    gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
    gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);

    *height += (border.top + border.bottom + padding.top + padding.bottom);
    ReleaseStyleContext(style);
}

void
moz_gtk_get_scale_metrics(GtkOrientation orient, gint* scale_width,
                          gint* scale_height)
{
  WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL) ?
                           MOZ_GTK_SCALE_HORIZONTAL :
                           MOZ_GTK_SCALE_VERTICAL;

  if (gtk_check_version(3, 20, 0) != nullptr) {
      gint thumb_length, thumb_height, trough_border;
      moz_gtk_get_scalethumb_metrics(orient, &thumb_length, &thumb_height);

      GtkStyleContext* style = ClaimStyleContext(widget);
      gtk_style_context_get_style(style, "trough-border", &trough_border, NULL);

      if (orient == GTK_ORIENTATION_HORIZONTAL) {
          *scale_width = thumb_length + trough_border * 2;
          *scale_height = thumb_height + trough_border * 2;
      } else {
          *scale_width = thumb_height + trough_border * 2;
          *scale_height = thumb_length + trough_border * 2;
      }
      ReleaseStyleContext(style);
  } else {
      GtkStyleContext* style = ClaimStyleContext(widget);
      gtk_style_context_get(style, gtk_style_context_get_state(style),
                            "min-width", scale_width,
                            "min-height", scale_height,
                            nullptr);
      ReleaseStyleContext(style);
  }
}

gint
moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length, gint* thumb_height)
{

  if (gtk_check_version(3, 20, 0) != nullptr) {
      WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL) ?
                               MOZ_GTK_SCALE_HORIZONTAL:
                               MOZ_GTK_SCALE_VERTICAL;
      GtkStyleContext* style = ClaimStyleContext(widget);
      gtk_style_context_get_style(style,
                                  "slider_length", thumb_length,
                                  "slider_width", thumb_height,
                                  NULL);
      ReleaseStyleContext(style);
  } else {
      WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL) ?
                               MOZ_GTK_SCALE_THUMB_HORIZONTAL:
                               MOZ_GTK_SCALE_THUMB_VERTICAL;
      GtkStyleContext* style = ClaimStyleContext(widget);
      gtk_style_context_get(style, gtk_style_context_get_state(style),
                            "min-width", thumb_length,
                            "min-height", thumb_height,
                             nullptr);
      ReleaseStyleContext(style);
  }

  return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_scrollbar_metrics(MozGtkScrollbarMetrics *metrics)
{
    // For Gtk >= 3.20 scrollbar metrics are ignored
    MOZ_ASSERT(gtk_check_version(3, 20, 0) != nullptr);

    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_SCROLLBAR_VERTICAL);
    gtk_style_context_get_style(style,
                                "slider_width", &metrics->slider_width,
                                "trough_border", &metrics->trough_border,
                                "stepper_size", &metrics->stepper_size,
                                "stepper_spacing", &metrics->stepper_spacing,
                                "min-slider-length", &metrics->min_slider_size,
                                nullptr);
    ReleaseStyleContext(style);

    return MOZ_GTK_SUCCESS;
}

/* cairo_t *cr argument has to be a system-cairo. */
gint
moz_gtk_widget_paint(WidgetNodeType widget, cairo_t *cr,
                     GdkRectangle* rect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction)
{
    /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=694086
     */
    cairo_new_path(cr);

    switch (widget) {
    case MOZ_GTK_BUTTON:
    case MOZ_GTK_TOOLBAR_BUTTON:
        if (state->depressed) {
            return moz_gtk_button_paint(cr, rect, state,
                                        (GtkReliefStyle) flags,
                                        GetWidget(MOZ_GTK_TOGGLE_BUTTON),
                                        direction);
        }
        return moz_gtk_button_paint(cr, rect, state,
                                    (GtkReliefStyle) flags,
                                    GetWidget(MOZ_GTK_BUTTON),
                                    direction);
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        return moz_gtk_toggle_paint(cr, rect, state,
                                    !!(flags & MOZ_GTK_WIDGET_CHECKED),
                                    !!(flags & MOZ_GTK_WIDGET_INCONSISTENT),
                                    (widget == MOZ_GTK_RADIOBUTTON),
                                    direction);
        break;
    case MOZ_GTK_SCROLLBAR_BUTTON:
        return moz_gtk_scrollbar_button_paint(cr, rect, state,
                                              (GtkScrollbarButtonFlags) flags,
                                              direction);
        break;
    case MOZ_GTK_SCROLLBAR_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_VERTICAL:
        if (gtk_check_version(3,20,0) == nullptr) {
          return moz_gtk_scrollbar_paint(widget, cr, rect, state, direction);
        } else {
          WidgetNodeType trough_widget = (widget == MOZ_GTK_SCROLLBAR_HORIZONTAL) ?
              MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL : MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL;
          return moz_gtk_scrollbar_trough_paint(trough_widget, cr, rect,
                                                state,
                                                (GtkScrollbarTrackFlags) flags,
                                                direction);
        }
        break;
    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
        if (gtk_check_version(3,20,0) == nullptr) {
          return moz_gtk_scrollbar_trough_paint(widget, cr, rect,
                                                state,
                                                (GtkScrollbarTrackFlags) flags,
                                                direction);
        }
        break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        return moz_gtk_scrollbar_thumb_paint(widget, cr, rect,
                                             state, direction);
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        return moz_gtk_scale_paint(cr, rect, state,
                                   (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
        return moz_gtk_scale_thumb_paint(cr, rect, state,
                                         (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SPINBUTTON:
        return moz_gtk_spin_paint(cr, rect, direction);
        break;
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        return moz_gtk_spin_updown_paint(cr, rect,
                                         (widget == MOZ_GTK_SPINBUTTON_DOWN),
                                         state, direction);
        break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
        {
            GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_SPINBUTTON_ENTRY,
                                     direction, GetStateFlagsFromGtkWidgetState(state));
            gint ret = moz_gtk_entry_paint(cr, rect, state, style);
            ReleaseStyleContext(style);
            return ret;
        }
        break;
    case MOZ_GTK_GRIPPER:
        return moz_gtk_gripper_paint(cr, rect, state,
                                     direction);
        break;
    case MOZ_GTK_TREEVIEW:
        return moz_gtk_treeview_paint(cr, rect, state,
                                      direction);
        break;
    case MOZ_GTK_TREE_HEADER_CELL:
        return moz_gtk_tree_header_cell_paint(cr, rect, state,
                                              flags, direction);
        break;
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        return moz_gtk_tree_header_sort_arrow_paint(cr, rect, 
                                                    state,
                                                    (GtkArrowType) flags,
                                                    direction);
        break;
    case MOZ_GTK_TREEVIEW_EXPANDER:
        return moz_gtk_treeview_expander_paint(cr, rect, state,
                                               (GtkExpanderStyle) flags, direction);
        break;
    case MOZ_GTK_ENTRY:
        {
            GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_ENTRY,
                                     direction, GetStateFlagsFromGtkWidgetState(state));
            gint ret = moz_gtk_entry_paint(cr, rect, state, style);
            ReleaseStyleContext(style);
            return ret;
        }
    case MOZ_GTK_TEXT_VIEW:
        return moz_gtk_text_view_paint(cr, rect, state, direction);
        break;
    case MOZ_GTK_DROPDOWN:
        return moz_gtk_combo_box_paint(cr, rect, state, direction);
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        return moz_gtk_combo_box_entry_button_paint(cr, rect,
                                                    state, flags, direction);
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        {
            GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_COMBOBOX_ENTRY_TEXTAREA,
                                     direction, GetStateFlagsFromGtkWidgetState(state));
            gint ret = moz_gtk_entry_paint(cr, rect, state, style);
            ReleaseStyleContext(style);
            return ret;
        }
        break;
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        return moz_gtk_container_paint(cr, rect, state, widget, direction);
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        return moz_gtk_toggle_label_paint(cr, rect, state,
                                          (widget == MOZ_GTK_RADIOBUTTON_LABEL),
                                          direction);
        break;
    case MOZ_GTK_TOOLBAR:
        return moz_gtk_toolbar_paint(cr, rect, direction);
        break;
    case MOZ_GTK_TOOLBAR_SEPARATOR:
        return moz_gtk_toolbar_separator_paint(cr, rect,
                                               direction);
        break;
    case MOZ_GTK_TOOLTIP:
        return moz_gtk_tooltip_paint(cr, rect, direction);
        break;
    case MOZ_GTK_FRAME:
        return moz_gtk_frame_paint(cr, rect, direction);
        break;
    case MOZ_GTK_RESIZER:
        return moz_gtk_resizer_paint(cr, rect, state,
                                     direction);
        break;
    case MOZ_GTK_PROGRESSBAR:
        return moz_gtk_progressbar_paint(cr, rect, direction);
        break;
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE:
    case MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE:
        return moz_gtk_progress_chunk_paint(cr, rect,
                                            direction, widget);
        break;
    case MOZ_GTK_TAB_TOP:
    case MOZ_GTK_TAB_BOTTOM:
        return moz_gtk_tab_paint(cr, rect, state,
                                 (GtkTabFlags) flags, direction, widget);
        break;
    case MOZ_GTK_TABPANELS:
        return moz_gtk_tabpanels_paint(cr, rect, direction);
        break;
    case MOZ_GTK_TAB_SCROLLARROW:
        return moz_gtk_tab_scroll_arrow_paint(cr, rect, state,
                                              (GtkArrowType) flags, direction);
        break;
    case MOZ_GTK_MENUBAR:
        return moz_gtk_menu_bar_paint(cr, rect, direction);
        break;
    case MOZ_GTK_MENUPOPUP:
        return moz_gtk_menu_popup_paint(cr, rect, direction);
        break;
    case MOZ_GTK_MENUSEPARATOR:
        return moz_gtk_menu_separator_paint(cr, rect,
                                            direction);
        break;
    case MOZ_GTK_MENUBARITEM:
    case MOZ_GTK_MENUITEM:
        return moz_gtk_menu_item_paint(widget, cr, rect, state, direction);
        break;
    case MOZ_GTK_MENUARROW:
        return moz_gtk_menu_arrow_paint(cr, rect, state,
                                        direction);
        break;
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
        return moz_gtk_arrow_paint(cr, rect, state,
                                   (GtkArrowType) flags, direction);
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        return moz_gtk_check_menu_item_paint(widget, cr, rect, state,
                                             (gboolean) flags, direction);
        break;
    case MOZ_GTK_SPLITTER_HORIZONTAL:
        return moz_gtk_vpaned_paint(cr, rect, state);
        break;
    case MOZ_GTK_SPLITTER_VERTICAL:
        return moz_gtk_hpaned_paint(cr, rect, state);
        break;
    case MOZ_GTK_WINDOW:
        return moz_gtk_window_paint(cr, rect, direction);
        break;
    case MOZ_GTK_INFO_BAR:
        return moz_gtk_info_bar_paint(cr, rect, state);
        break;
    default:
        g_warning("Unknown widget type: %d", widget);
    }

    return MOZ_GTK_UNKNOWN_WIDGET;
}

GtkWidget* moz_gtk_get_scrollbar_widget(void)
{
    return GetWidget(MOZ_GTK_SCROLLBAR_HORIZONTAL);
}

gboolean moz_gtk_has_scrollbar_buttons(void)
{
    gboolean backward, forward, secondary_backward, secondary_forward;
    MOZ_ASSERT(is_initialized, "Forgot to call moz_gtk_init()");
    GtkStyleContext* style = ClaimStyleContext(MOZ_GTK_SCROLLBAR_VERTICAL);
    gtk_style_context_get_style(style,
                                "has-backward-stepper", &backward,
                                "has-forward-stepper", &forward,
                                "has-secondary-backward-stepper", &secondary_backward,
                                "has-secondary-forward-stepper", &secondary_forward,
                                NULL);
    ReleaseStyleContext(style);

    return backward | forward | secondary_forward | secondary_forward;
}

gint
moz_gtk_shutdown()
{
    /* This will destroy all of our widgets */
    ResetWidgetCache();

    is_initialized = FALSE;

    return MOZ_GTK_SUCCESS;
}
