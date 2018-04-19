/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef WidgetStyleCache_h
#define WidgetStyleCache_h

#include <gtk/gtk.h>
#include "gtkdrawing.h"


typedef unsigned StyleFlags;
enum : StyleFlags {
  NO_STYLE_FLAGS,
  WHATEVER_MIGHT_BE_NEEDED = 1U << 0,
};

GtkWidget*
GetWidget(WidgetNodeType aNodeType);

cairo_surface_t*
GetWidgetIconSurface(GtkWidget* aWidgetIcon, int aScale);

/*
 * Return a new style context based on aWidget, as a child of aParentStyle.
 * If aWidget still has a floating reference, then it is sunk and released.
 */
GtkStyleContext*
CreateStyleForWidget(GtkWidget* aWidget, GtkStyleContext* aParentStyle);

GtkStyleContext*
CreateCSSNode(const char*      aName,
              GtkStyleContext* aParentStyle,
              GType            aType = G_TYPE_NONE);

/*
 * Returns a pointer to a style context for the specified node and state.
 * aStateFlags is applied only to last widget in css style path,
 * for instance GetStyleContext(MOZ_GTK_BUTTON, .., GTK_STATE_FLAG_HOVER)
 * you get "window button:hover" css selector.
 * If you want aStateFlags applied to all path elements use
 * CreateStyleContextWithStates().
 *
 * The context is owned by WidgetStyleCache. Do not unref.
 */
GtkStyleContext*
GetStyleContext(WidgetNodeType aNodeType,
                GtkTextDirection aDirection = GTK_TEXT_DIR_NONE,
                GtkStateFlags aStateFlags = GTK_STATE_FLAG_NORMAL,
                StyleFlags aFlags = NO_STYLE_FLAGS);

/*
 * Returns a pointer to a style context for the specified node
 * and state applied to all elements at widget style path.
 *
 * The context is owned by caller and must be released by g_object_unref().
 */
GtkStyleContext*
CreateStyleContextWithStates(WidgetNodeType aNodeType,
                             GtkTextDirection aDirection = GTK_TEXT_DIR_NONE,
                             GtkStateFlags aStateFlags = GTK_STATE_FLAG_NORMAL);

void
ResetWidgetCache(void);

#endif // WidgetStyleCache_h
