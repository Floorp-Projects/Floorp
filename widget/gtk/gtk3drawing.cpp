/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "mozilla/ScopeExit.h"
#include "prinrval.h"
#include "WidgetStyleCache.h"
#include "nsString.h"
#include "nsDebug.h"
#include "WidgetUtilsGtk.h"

#include <math.h>
#include <dlfcn.h>

static gboolean checkbox_check_state;
static gboolean notebook_has_tab_gap;

static ToggleGTKMetrics sCheckboxMetrics;
static ToggleGTKMetrics sRadioMetrics;
static ToolbarGTKMetrics sToolbarMetrics;
static CSDWindowDecorationSize sToplevelWindowDecorationSize;
static CSDWindowDecorationSize sPopupWindowDecorationSize;

using mozilla::Span;

#define ARROW_UP 0
#define ARROW_DOWN G_PI
#define ARROW_RIGHT G_PI_2
#define ARROW_LEFT (G_PI + G_PI_2)

#if 0
// It's used for debugging only to compare Gecko widget style with
// the ones used by Gtk+ applications.
static void
style_path_print(GtkStyleContext *context)
{
    const GtkWidgetPath* path = gtk_style_context_get_path(context);

    static auto sGtkWidgetPathToStringPtr =
        (char * (*)(const GtkWidgetPath *))
        dlsym(RTLD_DEFAULT, "gtk_widget_path_to_string");

    fprintf(stderr, "Style path:\n%s\n\n", sGtkWidgetPathToStringPtr(path));
}
#endif

static GtkBorder operator+=(GtkBorder& first, const GtkBorder& second) {
  first.left += second.left;
  first.right += second.right;
  first.top += second.top;
  first.bottom += second.bottom;
  return first;
}

static gint moz_gtk_get_tab_thickness(GtkStyleContext* style);

static void Inset(GdkRectangle*, const GtkBorder&);

static void InsetByMargin(GdkRectangle*, GtkStyleContext* style);

static void moz_gtk_add_style_margin(GtkStyleContext* style, gint* left,
                                     gint* top, gint* right, gint* bottom) {
  GtkBorder margin;

  gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                               &margin);
  *left += margin.left;
  *right += margin.right;
  *top += margin.top;
  *bottom += margin.bottom;
}

static void moz_gtk_add_style_border(GtkStyleContext* style, gint* left,
                                     gint* top, gint* right, gint* bottom) {
  GtkBorder border;

  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
                               &border);

  *left += border.left;
  *right += border.right;
  *top += border.top;
  *bottom += border.bottom;
}

static void moz_gtk_add_style_padding(GtkStyleContext* style, gint* left,
                                      gint* top, gint* right, gint* bottom) {
  GtkBorder padding;

  gtk_style_context_get_padding(style, gtk_style_context_get_state(style),
                                &padding);

  *left += padding.left;
  *right += padding.right;
  *top += padding.top;
  *bottom += padding.bottom;
}

static void moz_gtk_add_margin_border_padding(GtkStyleContext* style,
                                              gint* left, gint* top,
                                              gint* right, gint* bottom) {
  moz_gtk_add_style_margin(style, left, top, right, bottom);
  moz_gtk_add_style_border(style, left, top, right, bottom);
  moz_gtk_add_style_padding(style, left, top, right, bottom);
}

static void moz_gtk_add_border_padding(GtkStyleContext* style, gint* left,
                                       gint* top, gint* right, gint* bottom) {
  moz_gtk_add_style_border(style, left, top, right, bottom);
  moz_gtk_add_style_padding(style, left, top, right, bottom);
}

// In case there's an error in Gtk theme and preferred size is zero,
// return some sane values to pass mozilla automation tests.
// It should not happen in real-life.
#define MIN_WIDGET_SIZE 10
static void moz_gtk_sanity_preferred_size(GtkRequisition* requisition) {
  if (requisition->width <= 0) {
    requisition->width = MIN_WIDGET_SIZE;
  }
  if (requisition->height <= 0) {
    requisition->height = MIN_WIDGET_SIZE;
  }
}

// GetStateFlagsFromGtkWidgetState() can be safely used for the specific
// GtkWidgets that set both prelight and active flags.  For other widgets,
// either the GtkStateFlags or Gecko's GtkWidgetState need to be carefully
// adjusted to match GTK behavior.  Although GTK sets insensitive and focus
// flags in the generic GtkWidget base class, GTK adds prelight and active
// flags only to widgets that are expected to demonstrate prelight or active
// states.  This contrasts with HTML where any element may have :active and
// :hover states, and so Gecko's GtkStateFlags do not necessarily map to GTK
// flags.  Failure to restrict the flags in the same way as GTK can cause
// generic CSS selectors from some themes to unintentionally match elements
// that are not expected to change appearance on hover or mouse-down.
static GtkStateFlags GetStateFlagsFromGtkWidgetState(GtkWidgetState* state) {
  GtkStateFlags stateFlags = GTK_STATE_FLAG_NORMAL;

  if (state->disabled)
    stateFlags = GTK_STATE_FLAG_INSENSITIVE;
  else {
    if (state->depressed || state->active)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_ACTIVE);
    if (state->inHover)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_PRELIGHT);
    if (state->focused)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_FOCUSED);
    if (state->backdrop)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_BACKDROP);
  }

  return stateFlags;
}

static GtkStateFlags GetStateFlagsFromGtkTabFlags(GtkTabFlags flags) {
  return ((flags & MOZ_GTK_TAB_SELECTED) == 0) ? GTK_STATE_FLAG_NORMAL
                                               : GTK_STATE_FLAG_ACTIVE;
}

gint moz_gtk_init() {
  if (gtk_major_version > 3 ||
      (gtk_major_version == 3 && gtk_minor_version >= 14))
    checkbox_check_state = GTK_STATE_FLAG_CHECKED;
  else
    checkbox_check_state = GTK_STATE_FLAG_ACTIVE;

  moz_gtk_refresh();

  return MOZ_GTK_SUCCESS;
}

void moz_gtk_refresh() {
  if (gtk_check_version(3, 20, 0) != nullptr) {
    // Deprecated for Gtk >= 3.20+
    GtkStyleContext* style = GetStyleContext(MOZ_GTK_TAB_TOP);
    gtk_style_context_get_style(style, "has-tab-gap", &notebook_has_tab_gap,
                                NULL);
  } else {
    notebook_has_tab_gap = true;
  }

  sCheckboxMetrics.initialized = false;
  sRadioMetrics.initialized = false;
  sToolbarMetrics.initialized = false;
  sToplevelWindowDecorationSize.initialized = false;
  sPopupWindowDecorationSize.initialized = false;

  /* This will destroy all of our widgets */
  ResetWidgetCache();
}

gint moz_gtk_button_get_default_overflow(gint* border_top, gint* border_left,
                                         gint* border_bottom,
                                         gint* border_right) {
  GtkBorder* default_outside_border;

  GtkStyleContext* style = GetStyleContext(MOZ_GTK_BUTTON);
  gtk_style_context_get_style(style, "default-outside-border",
                              &default_outside_border, NULL);

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

static gint moz_gtk_button_get_default_border(gint* border_top,
                                              gint* border_left,
                                              gint* border_bottom,
                                              gint* border_right) {
  GtkBorder* default_border;

  GtkStyleContext* style = GetStyleContext(MOZ_GTK_BUTTON);
  gtk_style_context_get_style(style, "default-border", &default_border, NULL);

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

gint moz_gtk_splitter_get_metrics(gint orientation, gint* size) {
  GtkStyleContext* style;
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    style = GetStyleContext(MOZ_GTK_SPLITTER_HORIZONTAL);
  } else {
    style = GetStyleContext(MOZ_GTK_SPLITTER_VERTICAL);
  }
  gtk_style_context_get_style(style, "handle_size", size, NULL);
  return MOZ_GTK_SUCCESS;
}

static void CalculateToolbarButtonMetrics(WidgetNodeType aAppearance,
                                          ToolbarButtonGTKMetrics* aMetrics) {
  gint iconWidth, iconHeight;
  if (!gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &iconWidth, &iconHeight)) {
    NS_WARNING("Failed to get Gtk+ icon size for titlebar button!");

    // Use some reasonable fallback size
    iconWidth = 16;
    iconHeight = 16;
  }

  GtkStyleContext* style = GetStyleContext(aAppearance);
  gint width = 0, height = 0;
  if (!gtk_check_version(3, 20, 0)) {
    gtk_style_context_get(style, gtk_style_context_get_state(style),
                          "min-width", &width, "min-height", &height, NULL);
  }

  // Cover cases when min-width/min-height is not set, it's invalid
  // or we're running on Gtk+ < 3.20.
  if (width < iconWidth) width = iconWidth;
  if (height < iconHeight) height = iconHeight;

  gint left = 0, top = 0, right = 0, bottom = 0;
  moz_gtk_add_border_padding(style, &left, &top, &right, &bottom);

  // Button size is calculated as min-width/height + border/padding.
  width += left + right;
  height += top + bottom;

  // Place icon at button center.
  aMetrics->iconXPosition = (width - iconWidth) / 2;
  aMetrics->iconYPosition = (height - iconHeight) / 2;

  aMetrics->minSizeWithBorderMargin.width = width;
  aMetrics->minSizeWithBorderMargin.height = height;
}

// We support LTR layout only here for now.
static void CalculateToolbarButtonSpacing(WidgetNodeType aAppearance,
                                          ToolbarButtonGTKMetrics* aMetrics) {
  GtkStyleContext* style = GetStyleContext(aAppearance);
  gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                               &aMetrics->buttonMargin);

  // Get titlebar spacing, a default one is 6 pixels (gtk/gtkheaderbar.c)
  gint buttonSpacing = 6;
  g_object_get(GetWidget(MOZ_GTK_HEADER_BAR), "spacing", &buttonSpacing,
               nullptr);

  // We apply spacing as a margin equally to both adjacent buttons.
  buttonSpacing /= 2;

  if (!aMetrics->firstButton) {
    aMetrics->buttonMargin.left += buttonSpacing;
  }
  if (!aMetrics->lastButton) {
    aMetrics->buttonMargin.right += buttonSpacing;
  }

  aMetrics->minSizeWithBorderMargin.width +=
      aMetrics->buttonMargin.right + aMetrics->buttonMargin.left;
  aMetrics->minSizeWithBorderMargin.height +=
      aMetrics->buttonMargin.top + aMetrics->buttonMargin.bottom;
}

size_t GetGtkHeaderBarButtonLayout(Span<ButtonLayout> aButtonLayout,
                                   bool* aReversedButtonsPlacement) {
  gchar* decorationLayoutSetting = nullptr;
  GtkSettings* settings = gtk_settings_get_default();
  g_object_get(settings, "gtk-decoration-layout", &decorationLayoutSetting,
               nullptr);
  auto free = mozilla::MakeScopeExit([&] { g_free(decorationLayoutSetting); });

  // Use a default layout
  const gchar* decorationLayout = "menu:minimize,maximize,close";
  if (decorationLayoutSetting) {
    decorationLayout = decorationLayoutSetting;
  }

  // "minimize,maximize,close:" layout means buttons are on the opposite
  // titlebar side. close button is always there.
  if (aReversedButtonsPlacement) {
    const char* closeButton = strstr(decorationLayout, "close");
    const char* separator = strchr(decorationLayout, ':');
    *aReversedButtonsPlacement =
        closeButton && separator && closeButton < separator;
  }

  // We check what position a button string is stored in decorationLayout.
  //
  // decorationLayout gets its value from the GNOME preference:
  // org.gnome.desktop.vm.preferences.button-layout via the
  // gtk-decoration-layout property.
  //
  // Documentation of the gtk-decoration-layout property can be found here:
  // https://developer.gnome.org/gtk3/stable/GtkSettings.html#GtkSettings--gtk-decoration-layout
  if (aButtonLayout.IsEmpty()) {
    return 0;
  }

  nsDependentCSubstring layout(decorationLayout, strlen(decorationLayout));

  size_t activeButtons = 0;
  for (const auto& part : layout.Split(':')) {
    for (const auto& button : part.Split(',')) {
      if (button.EqualsLiteral("close")) {
        aButtonLayout[activeButtons++] = {MOZ_GTK_HEADER_BAR_BUTTON_CLOSE};
      } else if (button.EqualsLiteral("minimize")) {
        aButtonLayout[activeButtons++] = {MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE};
      } else if (button.EqualsLiteral("maximize")) {
        aButtonLayout[activeButtons++] = {MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE};
      }
      if (activeButtons == aButtonLayout.Length()) {
        return activeButtons;
      }
    }
  }
  return activeButtons;
}

static void EnsureToolbarMetrics() {
  if (sToolbarMetrics.initialized) {
    return;
  }
  // Make sure we have clean cache after theme reset, etc.
  memset(&sToolbarMetrics, 0, sizeof(sToolbarMetrics));

  // Calculate titlebar button visibility and positions.
  ButtonLayout aButtonLayout[TOOLBAR_BUTTONS];
  size_t activeButtonNums =
      GetGtkHeaderBarButtonLayout(Span(aButtonLayout), nullptr);

  for (size_t i = 0; i < activeButtonNums; i++) {
    int buttonIndex =
        (aButtonLayout[i].mType - MOZ_GTK_HEADER_BAR_BUTTON_CLOSE);
    ToolbarButtonGTKMetrics* metrics = sToolbarMetrics.button + buttonIndex;
    metrics->visible = true;
    // Mark first button
    if (!i) {
      metrics->firstButton = true;
    }
    // Mark last button.
    if (i == (activeButtonNums - 1)) {
      metrics->lastButton = true;
    }

    CalculateToolbarButtonMetrics(aButtonLayout[i].mType, metrics);
    CalculateToolbarButtonSpacing(aButtonLayout[i].mType, metrics);
  }

  sToolbarMetrics.initialized = true;
}

const ToolbarButtonGTKMetrics* GetToolbarButtonMetrics(
    WidgetNodeType aAppearance) {
  EnsureToolbarMetrics();

  int buttonIndex = (aAppearance - MOZ_GTK_HEADER_BAR_BUTTON_CLOSE);
  NS_ASSERTION(buttonIndex >= 0 && buttonIndex <= TOOLBAR_BUTTONS,
               "GetToolbarButtonMetrics(): Wrong titlebar button!");
  return sToolbarMetrics.button + buttonIndex;
}

static gint moz_gtk_window_decoration_paint(cairo_t* cr,
                                            const GdkRectangle* rect,
                                            GtkWidgetState* state,
                                            GtkTextDirection direction) {
  if (mozilla::widget::GdkIsWaylandDisplay()) {
    // Doesn't seem to be needed.
    return MOZ_GTK_SUCCESS;
  }
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* windowStyle =
      GetStyleContext(MOZ_GTK_HEADERBAR_WINDOW, state->image_scale);
  const bool solidDecorations =
      gtk_style_context_has_class(windowStyle, "solid-csd");
  GtkStyleContext* decorationStyle =
      GetStyleContext(solidDecorations ? MOZ_GTK_WINDOW_DECORATION_SOLID
                                       : MOZ_GTK_WINDOW_DECORATION,
                      state->image_scale, GTK_TEXT_DIR_LTR, state_flags);

  gtk_render_background(decorationStyle, cr, rect->x, rect->y, rect->width,
                        rect->height);
  gtk_render_frame(decorationStyle, cr, rect->x, rect->y, rect->width,
                   rect->height);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_button_paint(cairo_t* cr, const GdkRectangle* rect,
                                 GtkWidgetState* state, GtkReliefStyle relief,
                                 GtkWidget* widget,
                                 GtkTextDirection direction) {
  if (!widget) {
    return MOZ_GTK_UNKNOWN_WIDGET;
  }

  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* style = gtk_widget_get_style_context(widget);
  gint x = rect->x, y = rect->y, width = rect->width, height = rect->height;

  gtk_widget_set_direction(widget, direction);

  gtk_style_context_save(style);
  StyleContextSetScale(style, state->image_scale);
  gtk_style_context_set_state(style, state_flags);

  if (state->isDefault && relief == GTK_RELIEF_NORMAL && !state->focused &&
      !(state_flags & GTK_STATE_FLAG_PRELIGHT)) {
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

static gint moz_gtk_header_bar_button_paint(cairo_t* cr,
                                            const GdkRectangle* aRect,
                                            GtkWidgetState* state,
                                            GtkReliefStyle relief,
                                            WidgetNodeType aIconWidgetType,
                                            GtkTextDirection direction) {
  GdkRectangle rect = *aRect;
  // We need to inset our calculated margin because it also
  // contains titlebar button spacing.
  const ToolbarButtonGTKMetrics* metrics = GetToolbarButtonMetrics(
      aIconWidgetType == MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE_RESTORE
          ? MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE
          : aIconWidgetType);
  Inset(&rect, metrics->buttonMargin);

  GtkWidget* buttonWidget = GetWidget(aIconWidgetType);
  if (!buttonWidget) {
    return MOZ_GTK_UNKNOWN_WIDGET;
  }
  moz_gtk_button_paint(cr, &rect, state, relief, buttonWidget, direction);

  GtkWidget* iconWidget =
      gtk_bin_get_child(GTK_BIN(GetWidget(aIconWidgetType)));
  if (!iconWidget) {
    return MOZ_GTK_UNKNOWN_WIDGET;
  }
  cairo_surface_t* surface =
      GetWidgetIconSurface(iconWidget, state->image_scale);

  if (surface) {
    GtkStyleContext* style = gtk_widget_get_style_context(buttonWidget);
    GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);

    gtk_style_context_save(style);
    StyleContextSetScale(style, state->image_scale);
    gtk_style_context_set_state(style, state_flags);

    /* This is available since Gtk+ 3.10 as well as GtkHeaderBar */
    gtk_render_icon_surface(style, cr, surface, rect.x + metrics->iconXPosition,
                            rect.y + metrics->iconYPosition);
    gtk_style_context_restore(style);
  }

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_toggle_paint(cairo_t* cr, GdkRectangle* rect,
                                 GtkWidgetState* state, gboolean selected,
                                 gboolean inconsistent, gboolean isradio,
                                 GtkTextDirection direction) {
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  gint x, y, width, height;
  GtkStyleContext* style;

  // We need to call this before GetStyleContext, because otherwise we would
  // reset state flags
  const ToggleGTKMetrics* metrics =
      GetToggleMetrics(isradio ? MOZ_GTK_RADIOBUTTON : MOZ_GTK_CHECKBUTTON);
  // Clamp the rect and paint it center aligned in the rect.
  x = rect->x;
  y = rect->y;
  width = rect->width;
  height = rect->height;

  if (rect->width < rect->height) {
    y = rect->y + (rect->height - rect->width) / 2;
    height = rect->width;
  }

  if (rect->height < rect->width) {
    x = rect->x + (rect->width - rect->height) / 2;
    width = rect->height;
  }

  if (selected)
    state_flags =
        static_cast<GtkStateFlags>(state_flags | checkbox_check_state);

  if (inconsistent)
    state_flags =
        static_cast<GtkStateFlags>(state_flags | GTK_STATE_FLAG_INCONSISTENT);

  style = GetStyleContext(isradio ? MOZ_GTK_RADIOBUTTON : MOZ_GTK_CHECKBUTTON,
                          state->image_scale, direction, state_flags);

  if (gtk_check_version(3, 20, 0) == nullptr) {
    gtk_render_background(style, cr, x, y, width, height);
    gtk_render_frame(style, cr, x, y, width, height);
    // Indicator is inset by the toggle's padding and border.
    gint indicator_x = x + metrics->borderAndPadding.left;
    gint indicator_y = y + metrics->borderAndPadding.top;
    gint indicator_width = metrics->minSizeWithBorder.width -
                           metrics->borderAndPadding.left -
                           metrics->borderAndPadding.right;
    gint indicator_height = metrics->minSizeWithBorder.height -
                            metrics->borderAndPadding.top -
                            metrics->borderAndPadding.bottom;
    if (isradio) {
      gtk_render_option(style, cr, indicator_x, indicator_y, indicator_width,
                        indicator_height);
    } else {
      gtk_render_check(style, cr, indicator_x, indicator_y, indicator_width,
                       indicator_height);
    }
  } else {
    if (isradio) {
      gtk_render_option(style, cr, x, y, width, height);
    } else {
      gtk_render_check(style, cr, x, y, width, height);
    }
  }

  return MOZ_GTK_SUCCESS;
}

static gint calculate_button_inner_rect(GtkWidget* button,
                                        const GdkRectangle* rect,
                                        GdkRectangle* inner_rect,
                                        GtkTextDirection direction) {
  GtkStyleContext* style;
  GtkBorder border;
  GtkBorder padding = {0, 0, 0, 0};

  style = gtk_widget_get_style_context(button);

  /* This mirrors gtkbutton's child positioning */
  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
                               &border);
  gtk_style_context_get_padding(style, gtk_style_context_get_state(style),
                                &padding);

  inner_rect->x = rect->x + border.left + padding.left;
  inner_rect->y = rect->y + padding.top + border.top;
  inner_rect->width =
      MAX(1, rect->width - padding.left - padding.right - border.left * 2);
  inner_rect->height =
      MAX(1, rect->height - padding.top - padding.bottom - border.top * 2);

  return MOZ_GTK_SUCCESS;
}

static gint calculate_arrow_rect(GtkWidget* arrow, GdkRectangle* rect,
                                 GdkRectangle* arrow_rect,
                                 GtkTextDirection direction) {
  /* defined in gtkarrow.c */
  gfloat arrow_scaling = 0.7;
  gfloat xalign, xpad;
  gint extent;
  gint mxpad, mypad;
  gfloat mxalign, myalign;
  GtkMisc* misc = GTK_MISC(arrow);

  gtk_style_context_get_style(gtk_widget_get_style_context(arrow),
                              "arrow_scaling", &arrow_scaling, NULL);

  gtk_misc_get_padding(misc, &mxpad, &mypad);
  extent = MIN((rect->width - mxpad * 2), (rect->height - mypad * 2)) *
           arrow_scaling;

  gtk_misc_get_alignment(misc, &mxalign, &myalign);

  xalign = direction == GTK_TEXT_DIR_LTR ? mxalign : 1.0 - mxalign;
  xpad = mxpad + (rect->width - extent) * xalign;

  arrow_rect->x = direction == GTK_TEXT_DIR_LTR ? floor(rect->x + xpad)
                                                : ceil(rect->x + xpad);
  arrow_rect->y = floor(rect->y + mypad + ((rect->height - extent) * myalign));

  arrow_rect->width = arrow_rect->height = extent;

  return MOZ_GTK_SUCCESS;
}

/**
 * Get minimum widget size as sum of margin, padding, border and
 * min-width/min-height.
 */
static void moz_gtk_get_widget_min_size(GtkStyleContext* style, int* width,
                                        int* height) {
  GtkStateFlags state_flags = gtk_style_context_get_state(style);
  gtk_style_context_get(style, state_flags, "min-height", height, "min-width",
                        width, nullptr);

  GtkBorder border, padding, margin;
  gtk_style_context_get_border(style, state_flags, &border);
  gtk_style_context_get_padding(style, state_flags, &padding);
  gtk_style_context_get_margin(style, state_flags, &margin);

  *width += border.left + border.right + margin.left + margin.right +
            padding.left + padding.right;
  *height += border.top + border.bottom + margin.top + margin.bottom +
             padding.top + padding.bottom;
}

static void Inset(GdkRectangle* rect, const GtkBorder& aBorder) {
  rect->x += aBorder.left;
  rect->y += aBorder.top;
  rect->width -= aBorder.left + aBorder.right;
  rect->height -= aBorder.top + aBorder.bottom;
}

// Inset a rectangle by the margins specified in a style context.
static void InsetByMargin(GdkRectangle* rect, GtkStyleContext* style) {
  GtkBorder margin;
  gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                               &margin);
  Inset(rect, margin);
}

// Inset a rectangle by the border and padding specified in a style context.
static void InsetByBorderPadding(GdkRectangle* rect, GtkStyleContext* style) {
  GtkStateFlags state = gtk_style_context_get_state(style);
  GtkBorder padding, border;

  gtk_style_context_get_padding(style, state, &padding);
  Inset(rect, padding);
  gtk_style_context_get_border(style, state, &border);
  Inset(rect, border);
}

static void moz_gtk_draw_styled_frame(GtkStyleContext* style, cairo_t* cr,
                                      const GdkRectangle* aRect,
                                      bool drawFocus) {
  GdkRectangle rect = *aRect;

  InsetByMargin(&rect, style);

  gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
  gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);
  if (drawFocus) {
    gtk_render_focus(style, cr, rect.x, rect.y, rect.width, rect.height);
  }
}

static gint moz_gtk_inner_spin_paint(cairo_t* cr, GdkRectangle* rect,
                                     GtkWidgetState* state,
                                     GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_SPINBUTTON, state->image_scale, direction,
                      GetStateFlagsFromGtkWidgetState(state));

  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);

  /* hard code these values */
  GdkRectangle arrow_rect;
  arrow_rect.width = 6;
  arrow_rect.height = 6;

  // align spin to the left
  arrow_rect.x = rect->x;

  // up button
  arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2 - 3;
  gtk_render_arrow(style, cr, ARROW_UP, arrow_rect.x, arrow_rect.y,
                   arrow_rect.width);

  // down button
  arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2 + 3;
  gtk_render_arrow(style, cr, ARROW_DOWN, arrow_rect.x, arrow_rect.y,
                   arrow_rect.width);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_spin_paint(cairo_t* cr, GdkRectangle* rect,
                               GtkWidgetState* state,
                               GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_SPINBUTTON, state->image_scale, direction);
  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_spin_updown_paint(cairo_t* cr, GdkRectangle* rect,
                                      gboolean isDown, GtkWidgetState* state,
                                      GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_SPINBUTTON, state->image_scale, direction,
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

  gtk_render_arrow(style, cr, isDown ? ARROW_DOWN : ARROW_UP, arrow_rect.x,
                   arrow_rect.y, arrow_rect.width);

  return MOZ_GTK_SUCCESS;
}

/* See gtk_range_draw() for reference.
 */
static gint moz_gtk_scale_paint(cairo_t* cr, GdkRectangle* rect,
                                GtkWidgetState* state, GtkOrientation flags,
                                GtkTextDirection direction) {
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  gint x, y, width, height, min_width, min_height;
  GtkStyleContext* style;
  GtkBorder margin;

  moz_gtk_get_scale_metrics(flags, &min_width, &min_height);

  WidgetNodeType widget = (flags == GTK_ORIENTATION_HORIZONTAL)
                              ? MOZ_GTK_SCALE_TROUGH_HORIZONTAL
                              : MOZ_GTK_SCALE_TROUGH_VERTICAL;
  style = GetStyleContext(widget, state->image_scale, direction, state_flags);
  gtk_style_context_get_margin(style, state_flags, &margin);

  // Clamp the dimension perpendicular to the direction that the slider crosses
  // to the minimum size.
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    width = rect->width - (margin.left + margin.right);
    height = min_height - (margin.top + margin.bottom);
    x = rect->x + margin.left;
    y = rect->y + (rect->height - height) / 2;
  } else {
    width = min_width - (margin.left + margin.right);
    height = rect->height - (margin.top + margin.bottom);
    x = rect->x + (rect->width - width) / 2;
    y = rect->y + margin.top;
  }

  gtk_render_background(style, cr, x, y, width, height);
  gtk_render_frame(style, cr, x, y, width, height);

  if (state->focused)
    gtk_render_focus(style, cr, rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_scale_thumb_paint(cairo_t* cr, GdkRectangle* rect,
                                      GtkWidgetState* state,
                                      GtkOrientation flags,
                                      GtkTextDirection direction) {
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* style;
  gint thumb_width, thumb_height, x, y;

  /* determine the thumb size, and position the thumb in the center in the
   * opposite axis
   */
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_width,
                                   &thumb_height);
    x = rect->x;
    y = rect->y + (rect->height - thumb_height) / 2;
  } else {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height,
                                   &thumb_width);
    x = rect->x + (rect->width - thumb_width) / 2;
    y = rect->y;
  }

  WidgetNodeType widget = (flags == GTK_ORIENTATION_HORIZONTAL)
                              ? MOZ_GTK_SCALE_THUMB_HORIZONTAL
                              : MOZ_GTK_SCALE_THUMB_VERTICAL;
  style = GetStyleContext(widget, state->image_scale, direction, state_flags);
  gtk_render_slider(style, cr, x, y, thumb_width, thumb_height, flags);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_hpaned_paint(cairo_t* cr, GdkRectangle* rect,
                                 GtkWidgetState* state) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_SPLITTER_SEPARATOR_HORIZONTAL, state->image_scale,
                      GTK_TEXT_DIR_LTR, GetStateFlagsFromGtkWidgetState(state));
  gtk_render_handle(style, cr, rect->x, rect->y, rect->width, rect->height);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_vpaned_paint(cairo_t* cr, GdkRectangle* rect,
                                 GtkWidgetState* state) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_SPLITTER_SEPARATOR_VERTICAL, state->image_scale,
                      GTK_TEXT_DIR_LTR, GetStateFlagsFromGtkWidgetState(state));
  gtk_render_handle(style, cr, rect->x, rect->y, rect->width, rect->height);
  return MOZ_GTK_SUCCESS;
}

// See gtk_entry_draw() for reference.
static gint moz_gtk_entry_paint(cairo_t* cr, const GdkRectangle* aRect,
                                GtkWidgetState* state, GtkStyleContext* style,
                                WidgetNodeType widget) {
  GdkRectangle rect = *aRect;
  gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);

  // Paint the border, except for 'menulist-textfield' that isn't focused:
  if (widget != MOZ_GTK_DROPDOWN_ENTRY || state->focused) {
    gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);
  }

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_text_view_paint(cairo_t* cr, GdkRectangle* aRect,
                                    GtkWidgetState* state,
                                    GtkTextDirection direction) {
  // GtkTextView and GtkScrolledWindow do not set active and prelight flags.
  // The use of focus with MOZ_GTK_SCROLLED_WINDOW here is questionable
  // because a parent widget will not have focus when its child GtkTextView
  // has focus, but perhaps this may help identify a focused textarea with
  // some themes as GtkTextView backgrounds do not typically render
  // differently with focus.
  GtkStateFlags state_flags = state->disabled  ? GTK_STATE_FLAG_INSENSITIVE
                              : state->focused ? GTK_STATE_FLAG_FOCUSED
                                               : GTK_STATE_FLAG_NORMAL;

  GtkStyleContext* style_frame = GetStyleContext(
      MOZ_GTK_SCROLLED_WINDOW, state->image_scale, direction, state_flags);
  gtk_render_frame(style_frame, cr, aRect->x, aRect->y, aRect->width,
                   aRect->height);

  GdkRectangle rect = *aRect;
  InsetByBorderPadding(&rect, style_frame);

  GtkStyleContext* style = GetStyleContext(
      MOZ_GTK_TEXT_VIEW, state->image_scale, direction, state_flags);
  gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
  // There is a separate "text" window, which usually provides the
  // background behind the text.  However, this is transparent in Ambiance
  // for GTK 3.20, in which case the MOZ_GTK_TEXT_VIEW background is
  // visible.
  style = GetStyleContext(MOZ_GTK_TEXT_VIEW_TEXT, state->image_scale, direction,
                          state_flags);
  gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_treeview_paint(cairo_t* cr, GdkRectangle* rect,
                                   GtkWidgetState* state,
                                   GtkTextDirection direction) {
  gint xthickness, ythickness;
  GtkStyleContext* style;
  GtkStyleContext* style_tree;
  GtkStateFlags state_flags;
  GtkBorder border;

  /* only handle disabled and normal states, otherwise the whole background
   * area will be painted differently with other states */
  state_flags =
      state->disabled ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL;

  style =
      GetStyleContext(MOZ_GTK_SCROLLED_WINDOW, state->image_scale, direction);
  gtk_style_context_get_border(style, state_flags, &border);
  xthickness = border.left;
  ythickness = border.top;

  style_tree =
      GetStyleContext(MOZ_GTK_TREEVIEW_VIEW, state->image_scale, direction);
  gtk_render_background(style_tree, cr, rect->x + xthickness,
                        rect->y + ythickness, rect->width - 2 * xthickness,
                        rect->height - 2 * ythickness);

  style =
      GetStyleContext(MOZ_GTK_SCROLLED_WINDOW, state->image_scale, direction);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_tree_header_cell_paint(cairo_t* cr,
                                           const GdkRectangle* aRect,
                                           GtkWidgetState* state,
                                           gboolean isSorted,
                                           GtkTextDirection direction) {
  moz_gtk_button_paint(cr, aRect, state, GTK_RELIEF_NORMAL,
                       GetWidget(MOZ_GTK_TREE_HEADER_CELL), direction);
  return MOZ_GTK_SUCCESS;
}

/* See gtk_expander_paint() for reference.
 */
static gint moz_gtk_treeview_expander_paint(cairo_t* cr, GdkRectangle* rect,
                                            GtkWidgetState* state,
                                            GtkExpanderStyle expander_state,
                                            GtkTextDirection direction) {
  /* Because the frame we get is of the entire treeview, we can't get the
   * precise event state of one expander, thus rendering hover and active
   * feedback useless. */
  GtkStateFlags state_flags =
      state->disabled ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL;

  if (state->inHover)
    state_flags =
        static_cast<GtkStateFlags>(state_flags | GTK_STATE_FLAG_PRELIGHT);
  if (state->selected)
    state_flags =
        static_cast<GtkStateFlags>(state_flags | GTK_STATE_FLAG_SELECTED);

  /* GTK_STATE_FLAG_ACTIVE controls expanded/colapsed state rendering
   * in gtk_render_expander()
   */
  if (expander_state == GTK_EXPANDER_EXPANDED)
    state_flags =
        static_cast<GtkStateFlags>(state_flags | checkbox_check_state);
  else
    state_flags =
        static_cast<GtkStateFlags>(state_flags & ~(checkbox_check_state));

  GtkStyleContext* style = GetStyleContext(
      MOZ_GTK_TREEVIEW_EXPANDER, state->image_scale, direction, state_flags);
  gtk_render_expander(style, cr, rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

/* See gtk_separator_draw() for reference.
 */
static gint moz_gtk_combo_box_paint(cairo_t* cr, const GdkRectangle* aRect,
                                    GtkWidgetState* state,
                                    GtkTextDirection direction) {
  GdkRectangle arrow_rect, real_arrow_rect;
  gint separator_width;
  gboolean wide_separators;
  GtkStyleContext* style;
  GtkRequisition arrow_req;

  GtkWidget* comboBoxButton = GetWidget(MOZ_GTK_COMBOBOX_BUTTON);
  GtkWidget* comboBoxArrow = GetWidget(MOZ_GTK_COMBOBOX_ARROW);
  if (!comboBoxButton || !comboBoxArrow) {
    return MOZ_GTK_UNKNOWN_WIDGET;
  }

  /* Also sets the direction on gComboBoxButtonWidget, which is then
   * inherited by the separator and arrow */
  moz_gtk_button_paint(cr, aRect, state, GTK_RELIEF_NORMAL, comboBoxButton,
                       direction);

  calculate_button_inner_rect(comboBoxButton, aRect, &arrow_rect, direction);
  /* Now arrow_rect contains the inner rect ; we want to correct the width
   * to what the arrow needs (see gtk_combo_box_size_allocate) */
  gtk_widget_get_preferred_size(comboBoxArrow, NULL, &arrow_req);
  moz_gtk_sanity_preferred_size(&arrow_req);

  if (direction == GTK_TEXT_DIR_LTR)
    arrow_rect.x += arrow_rect.width - arrow_req.width;
  arrow_rect.width = arrow_req.width;

  calculate_arrow_rect(comboBoxArrow, &arrow_rect, &real_arrow_rect, direction);

  style = GetStyleContext(MOZ_GTK_COMBOBOX_ARROW, state->image_scale);
  gtk_render_arrow(style, cr, ARROW_DOWN, real_arrow_rect.x, real_arrow_rect.y,
                   real_arrow_rect.width);

  /* If there is no separator in the theme, there's nothing left to do. */
  GtkWidget* widget = GetWidget(MOZ_GTK_COMBOBOX_SEPARATOR);
  if (!widget) {
    return MOZ_GTK_SUCCESS;
  }
  style = gtk_widget_get_style_context(widget);
  StyleContextSetScale(style, state->image_scale);
  gtk_style_context_get_style(style, "wide-separators", &wide_separators,
                              "separator-width", &separator_width, NULL);

  if (wide_separators) {
    if (direction == GTK_TEXT_DIR_LTR)
      arrow_rect.x -= separator_width;
    else
      arrow_rect.x += arrow_rect.width;

    gtk_render_frame(style, cr, arrow_rect.x, arrow_rect.y, separator_width,
                     arrow_rect.height);
  } else {
    if (direction == GTK_TEXT_DIR_LTR) {
      GtkBorder padding;
      GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
      gtk_style_context_get_padding(style, state_flags, &padding);
      arrow_rect.x -= padding.left;
    } else
      arrow_rect.x += arrow_rect.width;

    gtk_render_line(style, cr, arrow_rect.x, arrow_rect.y, arrow_rect.x,
                    arrow_rect.y + arrow_rect.height);
  }
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_arrow_paint(cairo_t* cr, GdkRectangle* rect,
                                GtkWidgetState* state, GtkArrowType arrow_type,
                                GtkTextDirection direction) {
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
  if (arrow_type == GTK_ARROW_NONE) return MOZ_GTK_SUCCESS;

  GtkWidget* widget = GetWidget(MOZ_GTK_BUTTON_ARROW);
  if (!widget) {
    return MOZ_GTK_UNKNOWN_WIDGET;
  }
  calculate_arrow_rect(widget, rect, &arrow_rect, direction);
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* style = GetStyleContext(
      MOZ_GTK_BUTTON_ARROW, state->image_scale, direction, state_flags);
  gtk_render_arrow(style, cr, arrow_angle, arrow_rect.x, arrow_rect.y,
                   arrow_rect.width);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_tooltip_paint(cairo_t* cr, const GdkRectangle* aRect,
                                  GtkWidgetState* state,
                                  GtkTextDirection direction) {
  // Tooltip widget is made in GTK3 as following tree:
  // Tooltip window
  //   Horizontal Box
  //     Icon (not supported by Firefox)
  //     Label
  // Each element can be fully styled by CSS of GTK theme.
  // We have to draw all elements with appropriate offset and right dimensions.

  // Tooltip drawing
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_TOOLTIP, state->image_scale, direction);
  GdkRectangle rect = *aRect;
  gtk_render_background(style, cr, rect.x, rect.y, rect.width, rect.height);
  gtk_render_frame(style, cr, rect.x, rect.y, rect.width, rect.height);

  // Horizontal Box drawing
  //
  // The box element has hard-coded 6px margin-* GtkWidget properties, which
  // are added between the window dimensions and the CSS margin box of the
  // horizontal box.  The frame of the tooltip window is drawn in the
  // 6px margin.
  // For drawing Horizontal Box we have to inset drawing area by that 6px
  // plus its CSS margin.
  GtkStyleContext* boxStyle =
      GetStyleContext(MOZ_GTK_TOOLTIP_BOX, state->image_scale, direction);

  rect.x += 6;
  rect.y += 6;
  rect.width -= 12;
  rect.height -= 12;

  InsetByMargin(&rect, boxStyle);
  gtk_render_background(boxStyle, cr, rect.x, rect.y, rect.width, rect.height);
  gtk_render_frame(boxStyle, cr, rect.x, rect.y, rect.width, rect.height);

  // Label drawing
  InsetByBorderPadding(&rect, boxStyle);

  GtkStyleContext* labelStyle =
      GetStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL, state->image_scale, direction);
  moz_gtk_draw_styled_frame(labelStyle, cr, &rect, false);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_resizer_paint(cairo_t* cr, GdkRectangle* rect,
                                  GtkWidgetState* state,
                                  GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_RESIZER, state->image_scale, GTK_TEXT_DIR_LTR,
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

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_frame_paint(cairo_t* cr, GdkRectangle* rect,
                                GtkWidgetState* state,
                                GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_FRAME, state->image_scale, direction);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_progressbar_paint(cairo_t* cr, GdkRectangle* rect,
                                      GtkWidgetState* state,
                                      GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_PROGRESS_TROUGH, state->image_scale, direction);
  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_progress_chunk_paint(cairo_t* cr, GdkRectangle* rect,
                                         GtkWidgetState* state,
                                         GtkTextDirection direction,
                                         WidgetNodeType widget) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_PROGRESS_CHUNK, state->image_scale, direction);

  if (widget == MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE ||
      widget == MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE) {
    /**
     * The bar's size and the bar speed are set depending of the progress'
     * size. These could also be constant for all progress bars easily.
     */
    gboolean vertical =
        (widget == MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE);

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

  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_get_tab_thickness(GtkStyleContext* style) {
  if (!notebook_has_tab_gap)
    return 0; /* tabs do not overdraw the tabpanel border with "no gap" style */

  GtkBorder border;
  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
                               &border);
  if (border.top < 2) return 2; /* some themes don't set ythickness correctly */

  return border.top;
}

gint moz_gtk_get_tab_thickness(WidgetNodeType aNodeType) {
  GtkStyleContext* style = GetStyleContext(aNodeType);
  int thickness = moz_gtk_get_tab_thickness(style);
  return thickness;
}

/* actual small tabs */
static gint moz_gtk_tab_paint(cairo_t* cr, GdkRectangle* rect,
                              GtkWidgetState* state, GtkTabFlags flags,
                              GtkTextDirection direction,
                              WidgetNodeType widget) {
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

  style = GetStyleContext(widget, state->image_scale, direction,
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
      gtk_render_extension(style, cr, tabRect.x, tabRect.y, tabRect.width,
                           tabRect.height,
                           isBottomTab ? GTK_POS_TOP : GTK_POS_BOTTOM);
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
      if (gap_voffset > gap_height) gap_voffset = gap_height;

      /* Set gap_{l,r}_offset to appropriate values */
      gap_loffset = gap_roffset = 20; /* should be enough */
      if (flags & MOZ_GTK_TAB_FIRST) {
        if (direction == GTK_TEXT_DIR_RTL)
          gap_roffset = initial_gap;
        else
          gap_loffset = initial_gap;
      }

      GtkStyleContext* panelStyle =
          GetStyleContext(MOZ_GTK_TABPANELS, state->image_scale, direction);

      if (isBottomTab) {
        /* Draw the tab on bottom */
        focusRect.y += gap_voffset;
        focusRect.height -= gap_voffset;

        gtk_render_extension(style, cr, tabRect.x, tabRect.y + gap_voffset,
                             tabRect.width, tabRect.height - gap_voffset,
                             GTK_POS_TOP);

        backRect.y += (gap_voffset - gap_height);
        backRect.height = gap_height;

        /* Draw the gap; erase with background color before painting in
         * case theme does not */
        gtk_render_background(panelStyle, cr, backRect.x, backRect.y,
                              backRect.width, backRect.height);
        cairo_save(cr);
        cairo_rectangle(cr, backRect.x, backRect.y, backRect.width,
                        backRect.height);
        cairo_clip(cr);

        gtk_render_frame_gap(panelStyle, cr, tabRect.x - gap_loffset,
                             tabRect.y + gap_voffset - 3 * gap_height,
                             tabRect.width + gap_loffset + gap_roffset,
                             3 * gap_height, GTK_POS_BOTTOM, gap_loffset,
                             gap_loffset + tabRect.width);
        cairo_restore(cr);
      } else {
        /* Draw the tab on top */
        focusRect.height -= gap_voffset;
        gtk_render_extension(style, cr, tabRect.x, tabRect.y, tabRect.width,
                             tabRect.height - gap_voffset, GTK_POS_BOTTOM);

        backRect.y += (tabRect.height - gap_voffset);
        backRect.height = gap_height;

        /* Draw the gap; erase with background color before painting in
         * case theme does not */
        gtk_render_background(panelStyle, cr, backRect.x, backRect.y,
                              backRect.width, backRect.height);

        cairo_save(cr);
        cairo_rectangle(cr, backRect.x, backRect.y, backRect.width,
                        backRect.height);
        cairo_clip(cr);

        gtk_render_frame_gap(panelStyle, cr, tabRect.x - gap_loffset,
                             tabRect.y + tabRect.height - gap_voffset,
                             tabRect.width + gap_loffset + gap_roffset,
                             3 * gap_height, GTK_POS_TOP, gap_loffset,
                             gap_loffset + tabRect.width);
        cairo_restore(cr);
      }
    }
  } else {
    gtk_render_background(style, cr, tabRect.x, tabRect.y, tabRect.width,
                          tabRect.height);
    gtk_render_frame(style, cr, tabRect.x, tabRect.y, tabRect.width,
                     tabRect.height);
  }

  if (state->focused) {
    /* Paint the focus ring */
    GtkBorder padding;
    gtk_style_context_get_padding(style, GetStateFlagsFromGtkWidgetState(state),
                                  &padding);

    focusRect.x += padding.left;
    focusRect.width -= (padding.left + padding.right);
    focusRect.y += padding.top;
    focusRect.height -= (padding.top + padding.bottom);

    gtk_render_focus(style, cr, focusRect.x, focusRect.y, focusRect.width,
                     focusRect.height);
  }

  return MOZ_GTK_SUCCESS;
}

/* tab area*/
static gint moz_gtk_tabpanels_paint(cairo_t* cr, GdkRectangle* rect,
                                    GtkWidgetState* state,
                                    GtkTextDirection direction) {
  GtkStyleContext* style =
      GetStyleContext(MOZ_GTK_TABPANELS, state->image_scale, direction);
  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
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
  cairo_rectangle(cr, rect->x, rect->y, rect->x + rect->width / 2,
                  rect->y + rect->height);
  cairo_clip(cr);
  gtk_render_frame_gap(style, cr, rect->x, rect->y, rect->width, rect->height,
                       GTK_POS_TOP, rect->width - 1, rect->width);
  cairo_restore(cr);

  /* right side */
  cairo_save(cr);
  cairo_rectangle(cr, rect->x + rect->width / 2, rect->y, rect->x + rect->width,
                  rect->y + rect->height);
  cairo_clip(cr);
  gtk_render_frame_gap(style, cr, rect->x, rect->y, rect->width, rect->height,
                       GTK_POS_TOP, 0, 1);
  cairo_restore(cr);

  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_tab_scroll_arrow_paint(cairo_t* cr, GdkRectangle* rect,
                                           GtkWidgetState* state,
                                           GtkArrowType arrow_type,
                                           GtkTextDirection direction) {
  GtkStyleContext* style;
  gdouble arrow_angle;
  gint arrow_size = MIN(rect->width, rect->height);
  gint x = rect->x + (rect->width - arrow_size) / 2;
  gint y = rect->y + (rect->height - arrow_size) / 2;

  if (direction == GTK_TEXT_DIR_RTL) {
    arrow_type =
        (arrow_type == GTK_ARROW_LEFT) ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT;
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
  if (arrow_type != GTK_ARROW_NONE) {
    style = GetStyleContext(MOZ_GTK_TAB_SCROLLARROW, state->image_scale,
                            direction, GetStateFlagsFromGtkWidgetState(state));
    gtk_render_arrow(style, cr, arrow_angle, x, y, arrow_size);
  }
  return MOZ_GTK_SUCCESS;
}

static gint moz_gtk_header_bar_paint(WidgetNodeType widgetType, cairo_t* cr,
                                     GdkRectangle* rect,
                                     GtkWidgetState* state) {
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* style = GetStyleContext(widgetType, state->image_scale,
                                           GTK_TEXT_DIR_NONE, state_flags);

  // Some themes like Elementary's style the container of the headerbar rather
  // than the header bar itself.
  if (HeaderBarShouldDrawContainer(widgetType)) {
    auto containerType = widgetType == MOZ_GTK_HEADER_BAR
                             ? MOZ_GTK_HEADERBAR_FIXED
                             : MOZ_GTK_HEADERBAR_FIXED_MAXIMIZED;
    style = GetStyleContext(containerType, state->image_scale,
                            GTK_TEXT_DIR_NONE, state_flags);
  }

  gtk_render_background(style, cr, rect->x, rect->y, rect->width, rect->height);
  gtk_render_frame(style, cr, rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_widget_border(WidgetNodeType widget, gint* left, gint* top,
                               gint* right, gint* bottom,
                               // NOTE: callers depend on direction being used
                               // only for MOZ_GTK_DROPDOWN widgets.
                               GtkTextDirection direction) {
  GtkWidget* w;
  GtkStyleContext* style;
  *left = *top = *right = *bottom = 0;

  switch (widget) {
    case MOZ_GTK_BUTTON:
    case MOZ_GTK_TOOLBAR_BUTTON: {
      style = GetStyleContext(MOZ_GTK_BUTTON);

      *left = *top = *right = *bottom = gtk_container_get_border_width(
          GTK_CONTAINER(GetWidget(MOZ_GTK_BUTTON)));

      if (widget == MOZ_GTK_TOOLBAR_BUTTON) {
        gtk_style_context_save(style);
        gtk_style_context_add_class(style, "image-button");
      }

      moz_gtk_add_style_padding(style, left, top, right, bottom);

      if (widget == MOZ_GTK_TOOLBAR_BUTTON) gtk_style_context_restore(style);

      moz_gtk_add_style_border(style, left, top, right, bottom);

      return MOZ_GTK_SUCCESS;
    }
    case MOZ_GTK_ENTRY:
    case MOZ_GTK_DROPDOWN_ENTRY: {
      style = GetStyleContext(widget);

      // XXX: Subtract 1 pixel from the padding to account for the default
      // padding in forms.css. See bug 1187385.
      *left = *top = *right = *bottom = -1;
      moz_gtk_add_border_padding(style, left, top, right, bottom);

      return MOZ_GTK_SUCCESS;
    }
    case MOZ_GTK_TEXT_VIEW:
    case MOZ_GTK_TREEVIEW: {
      style = GetStyleContext(MOZ_GTK_SCROLLED_WINDOW);
      moz_gtk_add_style_border(style, left, top, right, bottom);
      return MOZ_GTK_SUCCESS;
    }
    case MOZ_GTK_TREE_HEADER_CELL: {
      /* A Tree Header in GTK is just a different styled button
       * It must be placed in a TreeView for getting the correct style
       * assigned.
       * That is why the following code is the same as for MOZ_GTK_BUTTON.
       * */
      *left = *top = *right = *bottom = gtk_container_get_border_width(
          GTK_CONTAINER(GetWidget(MOZ_GTK_TREE_HEADER_CELL)));
      style = GetStyleContext(MOZ_GTK_TREE_HEADER_CELL);
      moz_gtk_add_border_padding(style, left, top, right, bottom);
      return MOZ_GTK_SUCCESS;
    }
    case MOZ_GTK_DROPDOWN: {
      /* We need to account for the arrow on the dropdown, so text
       * doesn't come too close to the arrow, or in some cases spill
       * into the arrow. */
      gboolean wide_separators;
      gint separator_width;
      GtkRequisition arrow_req;
      GtkBorder border;

      *left = *top = *right = *bottom = gtk_container_get_border_width(
          GTK_CONTAINER(GetWidget(MOZ_GTK_COMBOBOX_BUTTON)));
      style = GetStyleContext(MOZ_GTK_COMBOBOX_BUTTON);
      moz_gtk_add_border_padding(style, left, top, right, bottom);

      /* If there is no separator, don't try to count its width. */
      separator_width = 0;
      GtkWidget* comboBoxSeparator = GetWidget(MOZ_GTK_COMBOBOX_SEPARATOR);
      if (comboBoxSeparator) {
        style = gtk_widget_get_style_context(comboBoxSeparator);
        gtk_style_context_get_style(style, "wide-separators", &wide_separators,
                                    "separator-width", &separator_width, NULL);

        if (!wide_separators) {
          gtk_style_context_get_border(
              style, gtk_style_context_get_state(style), &border);
          separator_width = border.left;
        }
      }

      gtk_widget_get_preferred_size(GetWidget(MOZ_GTK_COMBOBOX_ARROW), NULL,
                                    &arrow_req);
      moz_gtk_sanity_preferred_size(&arrow_req);

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
    case MOZ_GTK_TOOLTIP: {
      // In GTK 3 there are 6 pixels of additional margin around the box.
      // See details there:
      // https://github.com/GNOME/gtk/blob/5ea69a136bd7e4970b3a800390e20314665aaed2/gtk/ui/gtktooltipwindow.ui#L11
      *left = *right = *top = *bottom = 6;

      // We also need to add margin/padding/borders from Tooltip content.
      // Tooltip contains horizontal box, where icon and label is put.
      // We ignore icon as long as we don't have support for it.
      GtkStyleContext* boxStyle = GetStyleContext(MOZ_GTK_TOOLTIP_BOX);
      moz_gtk_add_margin_border_padding(boxStyle, left, top, right, bottom);

      GtkStyleContext* labelStyle = GetStyleContext(MOZ_GTK_TOOLTIP_BOX_LABEL);
      moz_gtk_add_margin_border_padding(labelStyle, left, top, right, bottom);

      return MOZ_GTK_SUCCESS;
    }
    case MOZ_GTK_HEADER_BAR_BUTTON_BOX: {
      style = GetStyleContext(MOZ_GTK_HEADER_BAR);
      moz_gtk_add_border_padding(style, left, top, right, bottom);
      *top = *bottom = 0;
      bool leftButtonsPlacement = false;
      GetGtkHeaderBarButtonLayout({}, &leftButtonsPlacement);
      if (direction == GTK_TEXT_DIR_RTL) {
        leftButtonsPlacement = !leftButtonsPlacement;
      }
      if (leftButtonsPlacement) {
        *right = 0;
      } else {
        *left = 0;
      }
      return MOZ_GTK_SUCCESS;
    }
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_SPLITTER_HORIZONTAL:
    case MOZ_GTK_SPLITTER_VERTICAL:
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE:
    case MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE:
    case MOZ_GTK_TREEVIEW_EXPANDER:
    case MOZ_GTK_HEADER_BAR:
    case MOZ_GTK_HEADER_BAR_MAXIMIZED:
    case MOZ_GTK_HEADER_BAR_BUTTON_CLOSE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE_RESTORE:
    /* These widgets have no borders.*/
    case MOZ_GTK_INNER_SPIN_BUTTON:
    case MOZ_GTK_SPINBUTTON:
    case MOZ_GTK_WINDOW_DECORATION:
    case MOZ_GTK_WINDOW_DECORATION_SOLID:
    case MOZ_GTK_RESIZER:
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
    case MOZ_GTK_TAB_SCROLLARROW:
      return MOZ_GTK_SUCCESS;
    default:
      g_warning("Unsupported widget type: %d", widget);
      return MOZ_GTK_UNKNOWN_WIDGET;
  }
  /* TODO - we're still missing some widget implementations */
  if (w) {
    moz_gtk_add_style_border(gtk_widget_get_style_context(w), left, top, right,
                             bottom);
  }
  return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_tab_border(gint* left, gint* top, gint* right, gint* bottom,
                            GtkTextDirection direction, GtkTabFlags flags,
                            WidgetNodeType widget) {
  GtkStyleContext* style = GetStyleContext(widget, 1, direction,
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

    gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                                 &margin);
    *left += margin.left;
    *right += margin.right;

    if (flags & MOZ_GTK_TAB_FIRST) {
      style = GetStyleContext(MOZ_GTK_NOTEBOOK_HEADER, direction);
      gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
                                   &margin);
      *left += margin.left;
      *right += margin.right;
    }
  }

  return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_tab_scroll_arrow_size(gint* width, gint* height) {
  gint arrow_size;

  GtkStyleContext* style = GetStyleContext(MOZ_GTK_TABPANELS);
  gtk_style_context_get_style(style, "scroll-arrow-hlength", &arrow_size, NULL);

  *height = *width = arrow_size;

  return MOZ_GTK_SUCCESS;
}

void moz_gtk_get_arrow_size(WidgetNodeType widgetType, gint* width,
                            gint* height) {
  GtkWidget* widget;
  switch (widgetType) {
    case MOZ_GTK_DROPDOWN:
      widget = GetWidget(MOZ_GTK_COMBOBOX_ARROW);
      break;
    default:
      widget = GetWidget(MOZ_GTK_BUTTON_ARROW);
      break;
  }
  if (widget) {
    GtkRequisition requisition;
    gtk_widget_get_preferred_size(widget, NULL, &requisition);
    moz_gtk_sanity_preferred_size(&requisition);

    *width = requisition.width;
    *height = requisition.height;
  } else {
    *width = 0;
    *height = 0;
  }
}

gint moz_gtk_get_expander_size(gint* size) {
  GtkStyleContext* style = GetStyleContext(MOZ_GTK_EXPANDER);
  gtk_style_context_get_style(style, "expander-size", size, NULL);
  return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_treeview_expander_size(gint* size) {
  GtkStyleContext* style = GetStyleContext(MOZ_GTK_TREEVIEW);
  gtk_style_context_get_style(style, "expander-size", size, NULL);
  return MOZ_GTK_SUCCESS;
}

void moz_gtk_get_entry_min_height(gint* min_content_height,
                                  gint* border_padding_height) {
  GtkStyleContext* style = GetStyleContext(MOZ_GTK_ENTRY);
  if (!gtk_check_version(3, 20, 0)) {
    gtk_style_context_get(style, gtk_style_context_get_state(style),
                          "min-height", min_content_height, nullptr);
  } else {
    *min_content_height = 0;
  }

  GtkBorder border;
  GtkBorder padding;
  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
                               &border);
  gtk_style_context_get_padding(style, gtk_style_context_get_state(style),
                                &padding);

  *border_padding_height =
      (border.top + border.bottom + padding.top + padding.bottom);
}

void moz_gtk_get_scale_metrics(GtkOrientation orient, gint* scale_width,
                               gint* scale_height) {
  if (gtk_check_version(3, 20, 0) != nullptr) {
    WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL)
                                ? MOZ_GTK_SCALE_HORIZONTAL
                                : MOZ_GTK_SCALE_VERTICAL;

    gint thumb_length, thumb_height, trough_border;
    moz_gtk_get_scalethumb_metrics(orient, &thumb_length, &thumb_height);

    GtkStyleContext* style = GetStyleContext(widget);
    gtk_style_context_get_style(style, "trough-border", &trough_border, NULL);

    if (orient == GTK_ORIENTATION_HORIZONTAL) {
      *scale_width = thumb_length + trough_border * 2;
      *scale_height = thumb_height + trough_border * 2;
    } else {
      *scale_width = thumb_height + trough_border * 2;
      *scale_height = thumb_length + trough_border * 2;
    }
  } else {
    WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL)
                                ? MOZ_GTK_SCALE_TROUGH_HORIZONTAL
                                : MOZ_GTK_SCALE_TROUGH_VERTICAL;
    moz_gtk_get_widget_min_size(GetStyleContext(widget), scale_width,
                                scale_height);
  }
}

gint moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length,
                                    gint* thumb_height) {
  if (gtk_check_version(3, 20, 0) != nullptr) {
    WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL)
                                ? MOZ_GTK_SCALE_HORIZONTAL
                                : MOZ_GTK_SCALE_VERTICAL;
    GtkStyleContext* style = GetStyleContext(widget);
    gtk_style_context_get_style(style, "slider_length", thumb_length,
                                "slider_width", thumb_height, NULL);
  } else {
    WidgetNodeType widget = (orient == GTK_ORIENTATION_HORIZONTAL)
                                ? MOZ_GTK_SCALE_THUMB_HORIZONTAL
                                : MOZ_GTK_SCALE_THUMB_VERTICAL;
    GtkStyleContext* style = GetStyleContext(widget);

    gint min_width, min_height;
    GtkStateFlags state = gtk_style_context_get_state(style);
    gtk_style_context_get(style, state, "min-width", &min_width, "min-height",
                          &min_height, nullptr);
    GtkBorder margin;
    gtk_style_context_get_margin(style, state, &margin);
    gint margin_width = margin.left + margin.right;
    gint margin_height = margin.top + margin.bottom;

    // Negative margin of slider element also determines its minimal size
    // so use bigger of those two values.
    if (min_width < -margin_width) min_width = -margin_width;
    if (min_height < -margin_height) min_height = -margin_height;

    *thumb_length = min_width;
    *thumb_height = min_height;
  }

  return MOZ_GTK_SUCCESS;
}

const ToggleGTKMetrics* GetToggleMetrics(WidgetNodeType aWidgetType) {
  ToggleGTKMetrics* metrics;

  switch (aWidgetType) {
    case MOZ_GTK_RADIOBUTTON:
      metrics = &sRadioMetrics;
      break;
    case MOZ_GTK_CHECKBUTTON:
      metrics = &sCheckboxMetrics;
      break;
    default:
      MOZ_CRASH("Unsupported widget type for getting metrics");
      return nullptr;
  }

  metrics->initialized = true;
  if (gtk_check_version(3, 20, 0) == nullptr) {
    GtkStyleContext* style = GetStyleContext(aWidgetType);
    GtkStateFlags state_flags = gtk_style_context_get_state(style);
    gtk_style_context_get(style, state_flags, "min-height",
                          &(metrics->minSizeWithBorder.height), "min-width",
                          &(metrics->minSizeWithBorder.width), nullptr);
    // Fallback to indicator size if min dimensions are zero
    if (metrics->minSizeWithBorder.height == 0 ||
        metrics->minSizeWithBorder.width == 0) {
      gint indicator_size = 0;
      gtk_widget_style_get(GetWidget(MOZ_GTK_CHECKBUTTON_CONTAINER),
                           "indicator_size", &indicator_size, nullptr);
      if (metrics->minSizeWithBorder.height == 0) {
        metrics->minSizeWithBorder.height = indicator_size;
      }
      if (metrics->minSizeWithBorder.width == 0) {
        metrics->minSizeWithBorder.width = indicator_size;
      }
    }

    GtkBorder border, padding;
    gtk_style_context_get_border(style, state_flags, &border);
    gtk_style_context_get_padding(style, state_flags, &padding);
    metrics->borderAndPadding.left = border.left + padding.left;
    metrics->borderAndPadding.right = border.right + padding.right;
    metrics->borderAndPadding.top = border.top + padding.top;
    metrics->borderAndPadding.bottom = border.bottom + padding.bottom;
    metrics->minSizeWithBorder.width +=
        metrics->borderAndPadding.left + metrics->borderAndPadding.right;
    metrics->minSizeWithBorder.height +=
        metrics->borderAndPadding.top + metrics->borderAndPadding.bottom;
  } else {
    gint indicator_size = 0, indicator_spacing = 0;
    gtk_widget_style_get(GetWidget(MOZ_GTK_CHECKBUTTON_CONTAINER),
                         "indicator_size", &indicator_size, "indicator_spacing",
                         &indicator_spacing, nullptr);
    metrics->minSizeWithBorder.width = metrics->minSizeWithBorder.height =
        indicator_size;
  }
  return metrics;
}

/*
 * get_shadow_width() from gtkwindow.c is not public so we need
 * to implement it.
 */
void InitWindowDecorationSize(CSDWindowDecorationSize* sWindowDecorationSize,
                              bool aPopupWindow) {
  bool solidDecorations = gtk_style_context_has_class(
      GetStyleContext(MOZ_GTK_HEADERBAR_WINDOW, 1), "solid-csd");
  // solid-csd does not use frame extents, quit now.
  if (solidDecorations) {
    sWindowDecorationSize->decorationSize = {0, 0, 0, 0};
    return;
  }

  // Scale factor is applied later when decoration size is used for actual
  // gtk windows.
  GtkStyleContext* context = GetStyleContext(MOZ_GTK_WINDOW_DECORATION);

  /* Always sum border + padding */
  GtkBorder padding;
  GtkStateFlags state = gtk_style_context_get_state(context);
  gtk_style_context_get_border(context, state,
                               &sWindowDecorationSize->decorationSize);
  gtk_style_context_get_padding(context, state, &padding);
  sWindowDecorationSize->decorationSize += padding;

  // Available on GTK 3.20+.
  static auto sGtkRenderBackgroundGetClip = (void (*)(
      GtkStyleContext*, gdouble, gdouble, gdouble, gdouble,
      GdkRectangle*))dlsym(RTLD_DEFAULT, "gtk_render_background_get_clip");

  if (!sGtkRenderBackgroundGetClip) {
    return;
  }

  GdkRectangle clip;
  sGtkRenderBackgroundGetClip(context, 0, 0, 0, 0, &clip);

  GtkBorder extents;
  extents.top = -clip.y;
  extents.right = clip.width + clip.x;
  extents.bottom = clip.height + clip.y;
  extents.left = -clip.x;

  // Get shadow extents but combine with style margin; use the bigger value.
  // Margin is used for resize grip size - it's not present on
  // popup windows.
  if (!aPopupWindow) {
    GtkBorder margin;
    gtk_style_context_get_margin(context, state, &margin);

    extents.top = MAX(extents.top, margin.top);
    extents.right = MAX(extents.right, margin.right);
    extents.bottom = MAX(extents.bottom, margin.bottom);
    extents.left = MAX(extents.left, margin.left);
  }

  sWindowDecorationSize->decorationSize += extents;
}

GtkBorder GetCSDDecorationSize(bool aIsPopup) {
  auto metrics =
      aIsPopup ? &sPopupWindowDecorationSize : &sToplevelWindowDecorationSize;
  if (!metrics->initialized) {
    InitWindowDecorationSize(metrics, aIsPopup);
    metrics->initialized = true;
  }
  return metrics->decorationSize;
}

/* cairo_t *cr argument has to be a system-cairo. */
gint moz_gtk_widget_paint(WidgetNodeType widget, cairo_t* cr,
                          GdkRectangle* rect, GtkWidgetState* state, gint flags,
                          GtkTextDirection direction) {
  /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=694086
   */
  cairo_new_path(cr);

  switch (widget) {
    case MOZ_GTK_BUTTON:
    case MOZ_GTK_TOOLBAR_BUTTON:
      if (state->depressed) {
        return moz_gtk_button_paint(cr, rect, state, (GtkReliefStyle)flags,
                                    GetWidget(MOZ_GTK_TOGGLE_BUTTON),
                                    direction);
      }
      return moz_gtk_button_paint(cr, rect, state, (GtkReliefStyle)flags,
                                  GetWidget(MOZ_GTK_BUTTON), direction);
    case MOZ_GTK_HEADER_BAR_BUTTON_CLOSE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE:
    case MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE_RESTORE:
      return moz_gtk_header_bar_button_paint(
          cr, rect, state, (GtkReliefStyle)flags, widget, direction);
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
      return moz_gtk_toggle_paint(cr, rect, state,
                                  !!(flags & MOZ_GTK_WIDGET_CHECKED),
                                  !!(flags & MOZ_GTK_WIDGET_INCONSISTENT),
                                  (widget == MOZ_GTK_RADIOBUTTON), direction);
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
      return moz_gtk_scale_paint(cr, rect, state, (GtkOrientation)flags,
                                 direction);
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
      return moz_gtk_scale_thumb_paint(cr, rect, state, (GtkOrientation)flags,
                                       direction);
    case MOZ_GTK_INNER_SPIN_BUTTON:
      return moz_gtk_inner_spin_paint(cr, rect, state, direction);
    case MOZ_GTK_SPINBUTTON:
      return moz_gtk_spin_paint(cr, rect, state, direction);
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
      return moz_gtk_spin_updown_paint(
          cr, rect, (widget == MOZ_GTK_SPINBUTTON_DOWN), state, direction);
    case MOZ_GTK_SPINBUTTON_ENTRY: {
      GtkStyleContext* style =
          GetStyleContext(MOZ_GTK_SPINBUTTON_ENTRY, state->image_scale,
                          direction, GetStateFlagsFromGtkWidgetState(state));
      return moz_gtk_entry_paint(cr, rect, state, style, widget);
    }
    case MOZ_GTK_TREEVIEW:
      return moz_gtk_treeview_paint(cr, rect, state, direction);
    case MOZ_GTK_TREE_HEADER_CELL:
      return moz_gtk_tree_header_cell_paint(cr, rect, state, flags, direction);
    case MOZ_GTK_TREEVIEW_EXPANDER:
      return moz_gtk_treeview_expander_paint(
          cr, rect, state, (GtkExpanderStyle)flags, direction);
    case MOZ_GTK_ENTRY:
    case MOZ_GTK_DROPDOWN_ENTRY: {
      GtkStyleContext* style =
          GetStyleContext(widget, state->image_scale, direction,
                          GetStateFlagsFromGtkWidgetState(state));
      gint ret = moz_gtk_entry_paint(cr, rect, state, style, widget);
      return ret;
    }
    case MOZ_GTK_TEXT_VIEW:
      return moz_gtk_text_view_paint(cr, rect, state, direction);
    case MOZ_GTK_DROPDOWN:
      return moz_gtk_combo_box_paint(cr, rect, state, direction);
    case MOZ_GTK_TOOLTIP:
      return moz_gtk_tooltip_paint(cr, rect, state, direction);
    case MOZ_GTK_FRAME:
      return moz_gtk_frame_paint(cr, rect, state, direction);
    case MOZ_GTK_RESIZER:
      return moz_gtk_resizer_paint(cr, rect, state, direction);
    case MOZ_GTK_PROGRESSBAR:
      return moz_gtk_progressbar_paint(cr, rect, state, direction);
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE:
    case MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE:
      return moz_gtk_progress_chunk_paint(cr, rect, state, direction, widget);
    case MOZ_GTK_TAB_TOP:
    case MOZ_GTK_TAB_BOTTOM:
      return moz_gtk_tab_paint(cr, rect, state, (GtkTabFlags)flags, direction,
                               widget);
    case MOZ_GTK_TABPANELS:
      return moz_gtk_tabpanels_paint(cr, rect, state, direction);
    case MOZ_GTK_TAB_SCROLLARROW:
      return moz_gtk_tab_scroll_arrow_paint(cr, rect, state,
                                            (GtkArrowType)flags, direction);
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
      return moz_gtk_arrow_paint(cr, rect, state, (GtkArrowType)flags,
                                 direction);
    case MOZ_GTK_SPLITTER_HORIZONTAL:
      return moz_gtk_vpaned_paint(cr, rect, state);
    case MOZ_GTK_SPLITTER_VERTICAL:
      return moz_gtk_hpaned_paint(cr, rect, state);
    case MOZ_GTK_WINDOW_DECORATION:
      return moz_gtk_window_decoration_paint(cr, rect, state, direction);
    case MOZ_GTK_HEADER_BAR:
    case MOZ_GTK_HEADER_BAR_MAXIMIZED:
      return moz_gtk_header_bar_paint(widget, cr, rect, state);
    default:
      g_warning("Unknown widget type: %d", widget);
  }

  return MOZ_GTK_UNKNOWN_WIDGET;
}

gint moz_gtk_shutdown() {
  /* This will destroy all of our widgets */
  ResetWidgetCache();

  return MOZ_GTK_SUCCESS;
}
