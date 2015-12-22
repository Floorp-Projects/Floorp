/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * gtkdrawing.h: GTK widget rendering utilities
 *
 * gtkdrawing provides an API for rendering GTK widgets in the
 * current theme to a pixmap or window, without requiring an actual
 * widget instantiation, similar to the Macintosh Appearance Manager
 * or Windows XP's DrawThemeBackground() API.
 */

#ifndef _GTK_DRAWING_H_
#define _GTK_DRAWING_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*** type definitions ***/
typedef struct {
  guint8 active;
  guint8 focused;
  guint8 inHover;
  guint8 disabled;
  guint8 isDefault;
  guint8 canDefault;
  /* The depressed state is for buttons which remain active for a longer period:
   * activated toggle buttons or buttons showing a popup menu. */
  guint8 depressed;
  gint32 curpos; /* curpos and maxpos are used for scrollbars */
  gint32 maxpos;
} GtkWidgetState;

typedef struct {
  gint slider_width;
  gint trough_border;
  gint stepper_size;
  gint stepper_spacing;
  gint min_slider_size;
} MozGtkScrollbarMetrics;

typedef enum {
  MOZ_GTK_STEPPER_DOWN        = 1 << 0,
  MOZ_GTK_STEPPER_BOTTOM      = 1 << 1,
  MOZ_GTK_STEPPER_VERTICAL    = 1 << 2
} GtkScrollbarButtonFlags;

typedef enum {
  MOZ_GTK_TRACK_OPAQUE        = 1 << 0
} GtkScrollbarTrackFlags;

/** flags for tab state **/
typedef enum {
  /* first eight bits are used to pass a margin */
  MOZ_GTK_TAB_MARGIN_MASK     = 0xFF,
  /* bottom tabs */
  MOZ_GTK_TAB_BOTTOM          = 1 << 8,
  /* the first tab in the group */
  MOZ_GTK_TAB_FIRST           = 1 << 9,
  /* the selected tab */
  MOZ_GTK_TAB_SELECTED        = 1 << 10
} GtkTabFlags;

/** flags for menuitems **/
typedef enum {
  /* menuitem is part of the menubar */
  MOZ_TOPLEVEL_MENU_ITEM      = 1 << 0
} GtkMenuItemFlags;

/* function type for moz_gtk_enable_style_props */
typedef gint (*style_prop_t)(GtkStyle*, const gchar*, gint);

/*** result/error codes ***/
#define MOZ_GTK_SUCCESS 0
#define MOZ_GTK_UNKNOWN_WIDGET -1
#define MOZ_GTK_UNSAFE_THEME -2

/*** checkbox/radio flags ***/
#define MOZ_GTK_WIDGET_CHECKED 1
#define MOZ_GTK_WIDGET_INCONSISTENT (1 << 1)

/*** widget type constants ***/
typedef enum {
  /* Paints a GtkButton. flags is a GtkReliefStyle. */
  MOZ_GTK_BUTTON,
  /* Paints a button with image and no text */
  MOZ_GTK_TOOLBAR_BUTTON,
  /* Paints a GtkCheckButton. flags is a boolean, 1=checked, 0=not checked. */
  MOZ_GTK_CHECKBUTTON,
  /* Paints a GtkRadioButton. flags is a boolean, 1=checked, 0=not checked. */
  MOZ_GTK_RADIOBUTTON,
  /**
   * Paints the button of a GtkScrollbar. flags is a GtkArrowType giving
   * the arrow direction.
   */
  MOZ_GTK_SCROLLBAR_BUTTON,
  /* Paints the trough (track) of a GtkScrollbar. */
  MOZ_GTK_SCROLLBAR_HORIZONTAL,
  MOZ_GTK_SCROLLBAR_VERTICAL,
  /* Paints the slider (thumb) of a GtkScrollbar. */
  MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL,
  MOZ_GTK_SCROLLBAR_THUMB_VERTICAL,
  /* Paints a GtkScale. */
  MOZ_GTK_SCALE_HORIZONTAL,
  MOZ_GTK_SCALE_VERTICAL,
  /* Paints a GtkScale thumb. */
  MOZ_GTK_SCALE_THUMB_HORIZONTAL,
  MOZ_GTK_SCALE_THUMB_VERTICAL,
  /* Paints a GtkSpinButton */
  MOZ_GTK_SPINBUTTON,
  MOZ_GTK_SPINBUTTON_UP,
  MOZ_GTK_SPINBUTTON_DOWN,
  MOZ_GTK_SPINBUTTON_ENTRY,
  /* Paints the gripper of a GtkHandleBox. */
  MOZ_GTK_GRIPPER,
  /* Paints a GtkEntry. */
  MOZ_GTK_ENTRY,
  /* Paints a GtkOptionMenu. */
  MOZ_GTK_DROPDOWN,
  /* Paints a dropdown arrow (a GtkButton containing a down GtkArrow). */
  MOZ_GTK_DROPDOWN_ARROW,
  /* Paints an entry in an editable option menu */
  MOZ_GTK_DROPDOWN_ENTRY,
  /* Paints the container part of a GtkCheckButton. */
  MOZ_GTK_CHECKBUTTON_CONTAINER,
  /* Paints the container part of a GtkRadioButton. */
  MOZ_GTK_RADIOBUTTON_CONTAINER,
  /* Paints the label of a GtkCheckButton (focus outline) */
  MOZ_GTK_CHECKBUTTON_LABEL,
  /* Paints the label of a GtkRadioButton (focus outline) */
  MOZ_GTK_RADIOBUTTON_LABEL,
  /* Paints the background of a GtkHandleBox. */
  MOZ_GTK_TOOLBAR,
  /* Paints a toolbar separator */
  MOZ_GTK_TOOLBAR_SEPARATOR,
  /* Paints a GtkToolTip */
  MOZ_GTK_TOOLTIP,
  /* Paints a GtkFrame (e.g. a status bar panel). */
  MOZ_GTK_FRAME,
  /* Paints a resize grip for a GtkWindow */
  MOZ_GTK_RESIZER,
  /* Paints a GtkProgressBar. */
  MOZ_GTK_PROGRESSBAR,
  /* Paints a progress chunk of a GtkProgressBar. */
  MOZ_GTK_PROGRESS_CHUNK,
  /* Paints a progress chunk of an indeterminated GtkProgressBar. */
  MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE,
  /* Paints a progress chunk of a vertical indeterminated GtkProgressBar. */
  MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE,
  /* Paints a tab of a GtkNotebook. flags is a GtkTabFlags, defined above. */
  MOZ_GTK_TAB,
  /* Paints the background and border of a GtkNotebook. */
  MOZ_GTK_TABPANELS,
  /* Paints a GtkArrow for a GtkNotebook. flags is a GtkArrowType. */
  MOZ_GTK_TAB_SCROLLARROW,
  /* Paints the background and border of a GtkTreeView */
  MOZ_GTK_TREEVIEW,
  /* Paints treeheader cells */
  MOZ_GTK_TREE_HEADER_CELL,
  /* Paints sort arrows in treeheader cells */
  MOZ_GTK_TREE_HEADER_SORTARROW,
  /* Paints an expander for a GtkTreeView */
  MOZ_GTK_TREEVIEW_EXPANDER,
  /* Paints the background of the menu bar. */
  MOZ_GTK_MENUBAR,
  /* Paints the background of menus, context menus. */
  MOZ_GTK_MENUPOPUP,
  /* Paints the arrow of menuitems that contain submenus */
  MOZ_GTK_MENUARROW,
  /* Paints an arrow in a toolbar button. flags is a GtkArrowType. */
  MOZ_GTK_TOOLBARBUTTON_ARROW,
  /* Paints items of menubar and popups. */
  MOZ_GTK_MENUITEM,
  MOZ_GTK_CHECKMENUITEM,
  MOZ_GTK_RADIOMENUITEM,
  MOZ_GTK_MENUSEPARATOR,
  /* Paints a GtkVPaned separator */
  MOZ_GTK_SPLITTER_HORIZONTAL,
  /* Paints a GtkHPaned separator */
  MOZ_GTK_SPLITTER_VERTICAL,
  /* Paints the background of a window, dialog or page. */
  MOZ_GTK_WINDOW,
  /* Paints a GtkInfoBar, for notifications. */
  MOZ_GTK_INFO_BAR
} GtkThemeWidgetType;

/*** General library functions ***/
/**
 * Initializes the drawing library.  You must call this function
 * prior to using any other functionality.
 * returns: MOZ_GTK_SUCCESS if there were no errors
 *          MOZ_GTK_UNSAFE_THEME if the current theme engine is known
 *                               to crash with gtkdrawing.
 */
gint moz_gtk_init();

/**
 * Enable GTK+ 1.2.9+ theme enhancements. You must provide a pointer
 * to the GTK+ 1.2.9+ function "gtk_style_get_prop_experimental".
 * styleGetProp:  pointer to gtk_style_get_prop_experimental
 * 
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_enable_style_props(style_prop_t styleGetProp);

/**
 * Perform cleanup of the drawing library. You should call this function
 * when your program exits, or you no longer need the library.
 *
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_shutdown();

#if (MOZ_WIDGET_GTK == 2)
/**
 * Retrieves the colormap to use for drawables passed to moz_gtk_widget_paint.
 */
GdkColormap* moz_gtk_widget_get_colormap();
#endif

/*** Widget drawing ***/
#if (MOZ_WIDGET_GTK == 2)
/**
 * Paint a widget in the current theme.
 * widget:    a constant giving the widget to paint
 * drawable:  the drawable to paint to;
 *            it's colormap must be moz_gtk_widget_get_colormap().
 * rect:      the bounding rectangle for the widget
 * cliprect:  a clipprect rectangle for this painting operation
 * state:     the state of the widget.  ignored for some widgets.
 * flags:     widget-dependant flags; see the GtkThemeWidgetType definition.
 * direction: the text direction, to draw the widget correctly LTR and RTL.
 */
gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkDrawable* drawable,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction);
#else
gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, cairo_t *cr,
                     GdkRectangle* rect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction);
#endif


/*** Widget metrics ***/
/**
 * Get the border size of a widget
 * left/right:  [OUT] the widget's left/right border
 * top/bottom:  [OUT] the widget's top/bottom border
 * direction:   the text direction for the widget
 * inhtml:      boolean indicating whether this widget will be drawn as a HTML form control,
 *              in order to workaround a size issue (MOZ_GTK_BUTTON only, ignored otherwise)
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint* left, gint* top, 
                               gint* right, gint* bottom, GtkTextDirection direction,
                               gboolean inhtml);

/**
 * Get the border size of a notebook tab
 * left/right:  [OUT] the tab's left/right border
 * top/bottom:  [OUT] the tab's top/bottom border
 * direction:   the text direction for the widget
 * flags:       tab-dependant flags; see the GtkTabFlags definition.
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_get_tab_border(gint* left, gint* top, gint* right, gint* bottom, 
                       GtkTextDirection direction, GtkTabFlags flags);

/**
 * Get the desired size of a GtkCheckButton
 * indicator_size:     [OUT] the indicator size
 * indicator_spacing:  [OUT] the spacing between the indicator and its
 *                     container
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_checkbox_get_metrics(gint* indicator_size, gint* indicator_spacing);

/**
 * Get the desired size of a GtkRadioButton
 * indicator_size:     [OUT] the indicator size
 * indicator_spacing:  [OUT] the spacing between the indicator and its
 *                     container
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_radio_get_metrics(gint* indicator_size, gint* indicator_spacing);

/** Get the extra size for the focus ring for outline:auto.
 * widget:             [IN]  the widget to get the focus metrics for    
 * focus_h_width:      [OUT] the horizontal width
 * focus_v_width:      [OUT] the vertical width
 *
 * returns:    MOZ_GTK_SUCCESS
 */
gint
moz_gtk_get_focus_outline_size(gint* focus_h_width, gint* focus_v_width);

/** Get the horizontal padding for the menuitem widget or checkmenuitem widget.
 * horizontal_padding: [OUT] The left and right padding of the menuitem or checkmenuitem
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_menuitem_get_horizontal_padding(gint* horizontal_padding);

gint
moz_gtk_checkmenuitem_get_horizontal_padding(gint* horizontal_padding);

/**
 * Some GTK themes draw their indication for the default button outside
 * the button (e.g. the glow in New Wave). This gets the extra space necessary.
 *
 * border_top:  [OUT] extra space to add above
 * border_left:  [OUT] extra space to add to the left
 * border_bottom:  [OUT] extra space to add underneath
 * border_right:  [OUT] extra space to add to the right
 *
 * returns:   MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_button_get_default_overflow(gint* border_top, gint* border_left,
                                    gint* border_bottom, gint* border_right);

/**
 * Gets the minimum size of a GtkScale.
 * orient:           [IN] the scale orientation
 * scale_width:      [OUT] the width of the scale
 * scale_height:     [OUT] the height of the scale
 */
void
moz_gtk_get_scale_metrics(GtkOrientation orient, gint* scale_width,
                          gint* scale_height);

/**
 * Get the desired size of a GtkScale thumb
 * orient:           [IN] the scale orientation
 * thumb_length:     [OUT] the length of the thumb
 * thumb_height:     [OUT] the height of the thumb
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length, gint* thumb_height);

/**
 * Get the desired metrics for a GtkScrollbar
 * metrics:          [IN]  struct which will contain the metrics
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_get_scrollbar_metrics(MozGtkScrollbarMetrics* metrics);

/**
 * Get the desired size of a dropdown arrow button
 * width:   [OUT] the desired width
 * height:  [OUT] the desired height
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_combo_box_entry_button_size(gint* width, gint* height);

/**
 * Get the desired size of a scroll arrow widget
 * width:   [OUT] the desired width
 * height:  [OUT] the desired height
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_tab_scroll_arrow_size(gint* width, gint* height);

/**
 * Get the desired size of an arrow in a button
 * width:   [OUT] the desired width
 * height:  [OUT] the desired height
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_arrow_size(gint* width, gint* height);

/**
 * Get the desired size of a toolbar separator
 * size:    [OUT] the desired width
 *
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_toolbar_separator_width(gint* size);

/**
 * Get the size of a regular GTK expander that shows/hides content
 * size:    [OUT] the size of the GTK expander, size = width = height.
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_expander_size(gint* size);

/**
 * Get the size of a treeview's expander (we call them twisties)
 * size:    [OUT] the size of the GTK expander, size = width = height.
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_treeview_expander_size(gint* size);

/**
 * Get the desired height of a menu separator
 * size:    [OUT] the desired height
 *
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_menu_separator_height(gint* size);

/**
 * Get the desired size of a splitter
 * orientation:   [IN]  GTK_ORIENTATION_HORIZONTAL or GTK_ORIENTATION_VERTICAL
 * size:          [OUT] width or height of the splitter handle
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_splitter_get_metrics(gint orientation, gint* size);

/**
 * Retrieve an actual GTK scrollbar widget for style analysis. It will not
 * be modified.
 */
GtkWidget* moz_gtk_get_scrollbar_widget(void);

/**
 * Get the YTHICKNESS of a tab (notebook extension).
 */
gint moz_gtk_get_tab_thickness(void);

/**
 * Get a boolean which indicates whether or not to use images in menus.
 * If TRUE, use images in menus.
 */
gboolean moz_gtk_images_in_menus(void);

/**
 * Get a boolean which indicates whether or not to use images in buttons.
 * If TRUE, use images in buttons.
 */
gboolean moz_gtk_images_in_buttons(void);

/**
 * Get a boolean which indicates whether the theme draws scrollbar buttons.
 * If TRUE, draw scrollbar buttons.
 */
gboolean moz_gtk_has_scrollbar_buttons(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
