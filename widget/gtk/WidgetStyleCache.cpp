/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TODO:
    - implement GtkTextDirection
    - implement StyleFlags
*/

#include <dlfcn.h>
#include <gtk/gtk.h>
#include "WidgetStyleCache.h"
#include "gtkdrawing.h"

static GtkWidget* sWidgetStorage[MOZ_GTK_WIDGET_NODE_COUNT];
static GtkStyleContext* sStyleStorage[MOZ_GTK_WIDGET_NODE_COUNT];

static bool sStyleContextNeedsRestore;
#ifdef DEBUG
static GtkStyleContext* sCurrentStyleContext;
#endif
static GtkStyleContext*
GetStyleInternal(WidgetNodeType aNodeType);

static GtkWidget*
CreateWindowWidget()
{
  GtkWidget *widget = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_realize(widget);
  gtk_widget_set_name(widget, "MozillaGtkWidget");
  return widget;
}

static GtkWidget*
CreateWindowContainerWidget()
{
  GtkWidget *widget = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_WINDOW)), widget);
  return widget;
}

static void
AddToWindowContainer(GtkWidget* widget)
{
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_WINDOW_CONTAINER)), widget);
  gtk_widget_realize(widget);
}

static GtkWidget*
CreateScrollbarWidget(WidgetNodeType aWidgetType, GtkOrientation aOrientation)
{
  GtkWidget* widget = gtk_scrollbar_new(aOrientation, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateWidget(WidgetNodeType aWidgetType)
{
  switch (aWidgetType) {
    case MOZ_GTK_WINDOW:
      return CreateWindowWidget();
    case MOZ_GTK_WINDOW_CONTAINER:
      return CreateWindowContainerWidget();
    case MOZ_GTK_SCROLLBAR_HORIZONTAL:
      return CreateScrollbarWidget(aWidgetType,
                                   GTK_ORIENTATION_HORIZONTAL);
    case MOZ_GTK_SCROLLBAR_VERTICAL:
      return CreateScrollbarWidget(aWidgetType,
                                   GTK_ORIENTATION_VERTICAL);
    default:
      /* Not implemented */
      return nullptr;
  }
}

GtkWidget*
GetWidget(WidgetNodeType aWidgetType)
{
  GtkWidget* widget = sWidgetStorage[aWidgetType];
  if (!widget) {
    widget = CreateWidget(aWidgetType);
    sWidgetStorage[aWidgetType] = widget;
  }
  return widget;
}

static GtkStyleContext*
CreateCSSNode(const char* aName, GtkStyleContext *aParentStyle)
{
  static auto sGtkWidgetPathIterSetObjectName =
    reinterpret_cast<void (*)(GtkWidgetPath *, gint, const char *)>
    (dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_set_object_name"));

  GtkWidgetPath* path =
    gtk_widget_path_copy(gtk_style_context_get_path(aParentStyle));

  gtk_widget_path_append_type(path, G_TYPE_NONE);

  (*sGtkWidgetPathIterSetObjectName)(path, -1, aName);

  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

static GtkStyleContext*
GetChildNodeStyle(WidgetNodeType aStyleType,
                  WidgetNodeType aWidgetType,
                  const gchar*   aStyleClass,
                  WidgetNodeType aParentNodeType)
{
  GtkStyleContext* style;

  if (gtk_check_version(3, 20, 0) != nullptr) {
    style = gtk_widget_get_style_context(sWidgetStorage[aWidgetType]);

    gtk_style_context_save(style);
    MOZ_ASSERT(!sStyleContextNeedsRestore);
    sStyleContextNeedsRestore = true;

    gtk_style_context_add_class(style, aStyleClass);
  }
  else {
    style = sStyleStorage[aStyleType];
    if (!style) {
      style = CreateCSSNode(aStyleClass, GetStyleInternal(aParentNodeType));
      MOZ_ASSERT(!sStyleContextNeedsRestore);
      sStyleStorage[aStyleType] = style;
    }
  }

  return style;
}

static GtkStyleContext*
GetStyleInternal(WidgetNodeType aNodeType)
{
  GtkWidget* widget = GetWidget(aNodeType);
  if (widget) {
    return gtk_widget_get_style_context(widget);
  }

  switch (aNodeType) {
    default:
      MOZ_FALLTHROUGH_ASSERT("missing style context for node type");

    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
      return GetChildNodeStyle(aNodeType,
                               MOZ_GTK_SCROLLBAR_HORIZONTAL,
                               GTK_STYLE_CLASS_TROUGH,
                               MOZ_GTK_SCROLLBAR_HORIZONTAL);

    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
      return GetChildNodeStyle(aNodeType,
                               MOZ_GTK_SCROLLBAR_HORIZONTAL,
                               GTK_STYLE_CLASS_SLIDER,
                               MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL);

    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      return GetChildNodeStyle(aNodeType,
                               MOZ_GTK_SCROLLBAR_VERTICAL,
                               GTK_STYLE_CLASS_TROUGH,
                               MOZ_GTK_SCROLLBAR_VERTICAL);

    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      return GetChildNodeStyle(aNodeType,
                               MOZ_GTK_SCROLLBAR_VERTICAL,
                               GTK_STYLE_CLASS_SLIDER,
                               MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
  }
}

void
ResetWidgetCache(void)
{
  MOZ_ASSERT(!sStyleContextNeedsRestore);
#ifdef DEBUG
  MOZ_ASSERT(!sCurrentStyleContext);
#endif

  for (int i = 0; i < MOZ_GTK_WIDGET_NODE_COUNT; i++) {
    if (sStyleStorage[i])
      g_object_unref(sStyleStorage[i]);
  }
  PodArrayZero(sStyleStorage);

  /* This will destroy all of our widgets */
  if (sWidgetStorage[MOZ_GTK_WINDOW])
    gtk_widget_destroy(sWidgetStorage[MOZ_GTK_WINDOW]);

  /* Clear already freed arrays */
  PodArrayZero(sWidgetStorage);
}

GtkStyleContext*
ClaimStyleContext(WidgetNodeType aNodeType, GtkTextDirection aDirection,
                  StyleFlags aFlags)
{
  GtkStyleContext* style = GetStyleInternal(aNodeType);
#ifdef DEBUG
  MOZ_ASSERT(!sCurrentStyleContext);
  sCurrentStyleContext = style;
#endif
  return style;
}

void
ReleaseStyleContext(GtkStyleContext* aStyleContext)
{
  if (sStyleContextNeedsRestore) {
    gtk_style_context_restore(aStyleContext);
  }
  sStyleContextNeedsRestore = false;
#ifdef DEBUG
  MOZ_ASSERT(sCurrentStyleContext == aStyleContext);
  sCurrentStyleContext = nullptr;
#endif
}
