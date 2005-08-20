/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
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

/*
 * This file contains painting functions for each of the gtk widgets.
 * Adapted from the gtk+ 1.2.10 source.
 */

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include "gtkdrawing.h"

extern GtkWidget* gButtonWidget;
extern GtkWidget* gCheckboxWidget;
extern GtkWidget* gScrollbarWidget;

GtkStateType
ConvertGtkState(GtkWidgetState* aState)
{
  if (aState->active)
    return GTK_STATE_ACTIVE;
  else if (aState->disabled)
    return GTK_STATE_INSENSITIVE;
  else if (aState->inHover)
    return GTK_STATE_PRELIGHT;
  else
    return GTK_STATE_NORMAL;
}

void
moz_gtk_button_paint(GdkWindow* window, GtkStyle* style,
                     GdkRectangle* buttonRect, GdkRectangle* clipRect,
                     GtkWidgetState* buttonState)
{
  GtkShadowType shadow_type;
  gint default_spacing = 7; /* xxx fix me */
  GtkStateType button_state = ConvertGtkState(buttonState);
  gint x = buttonRect->x, y=buttonRect->y, width=buttonRect->width, height=buttonRect->height;

  if (((GdkWindowPrivate*)window)->mapped) {
    gdk_window_set_back_pixmap(window, NULL, TRUE);
    gdk_window_clear_area(window, clipRect->x, clipRect->y, clipRect->width, clipRect->height);
  }

  gtk_widget_set_state(gButtonWidget, button_state);
  if (buttonState->isDefault)
    gtk_paint_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                  clipRect, gButtonWidget, "buttondefault", x, y, width, height);

  if (buttonState->canDefault) {
    x += style->klass->xthickness;
    y += style->klass->ythickness;
    width -= 2 * x + default_spacing;
    height -= 2 * y + default_spacing;
    x += (1 + default_spacing) / 2;
    y += (1 + default_spacing) / 2;
  }
       
  if (buttonState->focused) {
    x += 1;
    y += 1;
    width -= 2;
    height -= 2;
  }
	
  shadow_type = (buttonState->active) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
  
  gtk_paint_box(style, window, button_state, shadow_type, clipRect,
                gButtonWidget, "button", x, y, width, height);
  
  if (buttonState->focused) {
    x -= 1;
    y -= 1;
    width += 2;
    height += 2;
    
    gtk_paint_focus(style, window, clipRect, gButtonWidget, "button",
                    x, y, width - 1, height - 1);
  }
}

void
moz_gtk_check_button_draw_indicator(GdkWindow* window, GtkStyle* style,
                                    GdkRectangle* boxRect, GdkRectangle* clipRect,
                                    GtkToggleButtonState* aState)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint indicator_size = 10;
  gint indicator_spacing = 2;
  GtkWidgetState* wState = (GtkWidgetState*) aState;
  gint x, y, width, height;

  /* XXX get indicator size/spacing properties
      _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);
  */

  state_type = ConvertGtkState(wState);

  if (state_type != GTK_STATE_NORMAL &&
      state_type != GTK_STATE_PRELIGHT)
    state_type = GTK_STATE_NORMAL;
  
  if (state_type != GTK_STATE_NORMAL) /* this is for drawing e.g. a prelight box */
    gtk_paint_flat_box (style, window, state_type, 
                        GTK_SHADOW_ETCHED_OUT,
                        clipRect, gCheckboxWidget, "checkbutton",
                        boxRect->x, boxRect->y,
                        boxRect->width, boxRect->height);
  
  x = boxRect->x + indicator_spacing;
  y = boxRect->y + (boxRect->height - indicator_size) / 2;
  width = indicator_size;
  height = indicator_size;
  
  if (aState->selected) {
    state_type = GTK_STATE_ACTIVE;
    shadow_type = GTK_SHADOW_IN;
  }
  else {
    shadow_type = GTK_SHADOW_OUT;
    state_type = ConvertGtkState(wState);
  }
  
  gtk_paint_check (style, window,
                   state_type, shadow_type,
                   clipRect, gCheckboxWidget, "checkbutton",
                   x + 1, y + 1, width, height);
}

void
moz_gtk_checkbox_paint(GdkWindow* window, GtkStyle* style,
                       GdkRectangle* boxRect, GdkRectangle* clipRect,
                       GtkToggleButtonState* aState)
{
  moz_gtk_check_button_draw_indicator(window, style, boxRect, clipRect, aState);
  
  if (((GtkWidgetState*)aState)->focused)
    gtk_paint_focus (style, window,
                     clipRect, gCheckboxWidget, "checkbutton",
                     boxRect->x, boxRect->y, boxRect->width - 1, boxRect->height - 1);

}

void
moz_gtk_scrollbar_button_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* arrowRect, GdkRectangle* clipRect,
                               GtkWidgetState* state, GtkArrowType arrowType)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkShadowType shadow_type = (state->active) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

  moz_gtk_button_paint(window, style, arrowRect, clipRect, state);
  gtk_paint_arrow(style, window, state_type, shadow_type, clipRect,
                  gScrollbarWidget, (arrowType < 2) ? "vscrollbar" : "hscrollbar", 
                  arrowType, TRUE,
                  arrowRect->x, arrowRect->y, arrowRect->width, arrowRect->height);
}

void
moz_gtk_scrollbar_trough_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* troughRect, GdkRectangle* clipRect,
                               GtkWidgetState* state)
{
  gtk_paint_box(style, window, GTK_STATE_ACTIVE, GTK_SHADOW_IN,
                clipRect, gScrollbarWidget, "trough", troughRect->x,
                troughRect->y, troughRect->width, troughRect->height);

  if (state->focused)
    gtk_paint_focus(style, window, clipRect, gScrollbarWidget, "trough",
                    troughRect->x, troughRect->y, troughRect->width, 
                    troughRect->height);
}

void
moz_gtk_scrollbar_thumb_paint(GdkWindow* window, GtkStyle* style,
                              GdkRectangle* thumbRect, GdkRectangle* clipRect,
                              GtkWidgetState* state)
{
  GtkStateType state_type = state->inHover ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
  gtk_paint_box(style, window, state_type, GTK_SHADOW_OUT, clipRect,
                gScrollbarWidget, "slider", thumbRect->x, thumbRect->y,
                thumbRect->width, thumbRect->height);
}

