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
GetCssNodeStyleInternal(WidgetNodeType aNodeType);

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
CreateCheckboxWidget()
{
  GtkWidget* widget = gtk_check_button_new_with_label("M");
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateRadiobuttonWidget()
{
  GtkWidget* widget = gtk_radio_button_new_with_label(nullptr, "M");
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateMenuBarWidget()
{
  GtkWidget* widget = gtk_menu_bar_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateMenuPopupWidget()
{
  GtkWidget* widget = gtk_menu_new();
  gtk_menu_attach_to_widget(GTK_MENU(widget), GetWidget(MOZ_GTK_WINDOW),
                            nullptr);
  return widget;
}

static GtkWidget*
CreateMenuItemWidget(WidgetNodeType aShellType)
{
  GtkWidget* widget = gtk_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(GetWidget(aShellType)), widget);
  return widget;
}

static GtkWidget*
CreateProgressWidget()
{
  GtkWidget* widget = gtk_progress_bar_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateTooltipWidget()
{
  MOZ_ASSERT(gtk_check_version(3, 20, 0) != nullptr,
             "CreateTooltipWidget should be used for Gtk < 3.20 only.");
  GtkWidget* widget = CreateWindowWidget();
  GtkStyleContext* style = gtk_widget_get_style_context(widget);
  gtk_style_context_add_class(style, GTK_STYLE_CLASS_TOOLTIP);
  return widget;
}

static GtkWidget*
CreateExpanderWidget()
{
  GtkWidget* widget = gtk_expander_new("M");
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateFrameWidget()
{
  GtkWidget* widget = gtk_frame_new(nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateGripperWidget()
{
  GtkWidget* widget = gtk_handle_box_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateToolbarWidget()
{
  GtkWidget* widget = gtk_toolbar_new();
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_GRIPPER)), widget);
  gtk_widget_realize(widget);
  return widget;
}

static GtkWidget*
CreateToolbarSeparatorWidget()
{
  GtkWidget* widget = GTK_WIDGET(gtk_separator_tool_item_new());
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateInfoBarWidget()
{
  GtkWidget* widget = gtk_info_bar_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateButtonWidget()
{
  GtkWidget* widget = gtk_button_new_with_label("M");
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateToggleButtonWidget()
{
  GtkWidget* widget = gtk_toggle_button_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateButtonArrowWidget()
{
  GtkWidget* widget = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_TOGGLE_BUTTON)), widget);
  gtk_widget_realize(widget);
  gtk_widget_show(widget);
  return widget;
}

static GtkWidget*
CreateSpinWidget()
{
  GtkWidget* widget = gtk_spin_button_new(nullptr, 1, 0);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateEntryWidget()
{
  GtkWidget* widget = gtk_entry_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateScrolledWindowWidget()
{
  GtkWidget* widget = gtk_scrolled_window_new(nullptr, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateTextViewWidget()
{
  GtkWidget* widget = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_SCROLLED_WINDOW)),
                    widget);
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
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
      return CreateCheckboxWidget();
    case MOZ_GTK_PROGRESSBAR:
      return CreateProgressWidget();
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
      return CreateRadiobuttonWidget();
    case MOZ_GTK_SCROLLBAR_HORIZONTAL:
      return CreateScrollbarWidget(aWidgetType,
                                   GTK_ORIENTATION_HORIZONTAL);
    case MOZ_GTK_SCROLLBAR_VERTICAL:
      return CreateScrollbarWidget(aWidgetType,
                                   GTK_ORIENTATION_VERTICAL);
    case MOZ_GTK_MENUBAR:
      return CreateMenuBarWidget();
    case MOZ_GTK_MENUPOPUP:
      return CreateMenuPopupWidget();
    case MOZ_GTK_MENUBARITEM:
      return CreateMenuItemWidget(MOZ_GTK_MENUBAR);
    case MOZ_GTK_MENUITEM:
      return CreateMenuItemWidget(MOZ_GTK_MENUPOPUP);
    case MOZ_GTK_EXPANDER:
      return CreateExpanderWidget();
    case MOZ_GTK_FRAME:
      return CreateFrameWidget();
    case MOZ_GTK_GRIPPER:
      return CreateGripperWidget();
    case MOZ_GTK_TOOLBAR:
      return CreateToolbarWidget();
    case MOZ_GTK_TOOLBAR_SEPARATOR:
      return CreateToolbarSeparatorWidget();
    case MOZ_GTK_INFO_BAR:
      return CreateInfoBarWidget();
    case MOZ_GTK_SPINBUTTON:
      return CreateSpinWidget();
    case MOZ_GTK_BUTTON:
      return CreateButtonWidget();
    case MOZ_GTK_TOGGLE_BUTTON:
      return CreateToggleButtonWidget();
    case MOZ_GTK_BUTTON_ARROW:
      return CreateButtonArrowWidget();
    case MOZ_GTK_ENTRY:
      return CreateEntryWidget();
    case MOZ_GTK_SCROLLED_WINDOW: 
      return CreateScrolledWindowWidget();
    case MOZ_GTK_TEXT_VIEW:
      return CreateTextViewWidget();
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

GtkStyleContext*
CreateStyleForWidget(GtkWidget* aWidget, GtkStyleContext* aParentStyle)
{
  GtkWidgetPath* path = aParentStyle ?
    gtk_widget_path_copy(gtk_style_context_get_path(aParentStyle)) :
    gtk_widget_path_new();

  // Work around https://bugzilla.gnome.org/show_bug.cgi?id=767312
  // which exists in GTK+ 3.20.
  gtk_widget_get_style_context(aWidget);

  gtk_widget_path_append_for_widget(path, aWidget);
  // Release any floating reference on aWidget.
  g_object_ref_sink(aWidget);
  g_object_unref(aWidget);

  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

GtkStyleContext*
CreateCSSNode(const char* aName, GtkStyleContext* aParentStyle, GType aType)
{
  static auto sGtkWidgetPathIterSetObjectName =
    reinterpret_cast<void (*)(GtkWidgetPath *, gint, const char *)>
    (dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_set_object_name"));

  GtkWidgetPath* path = aParentStyle ?
    gtk_widget_path_copy(gtk_style_context_get_path(aParentStyle)) :
    gtk_widget_path_new();

  gtk_widget_path_append_type(path, aType);

  (*sGtkWidgetPathIterSetObjectName)(path, -1, aName);

  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

static GtkStyleContext*
CreateChildCSSNode(const char* aName, WidgetNodeType aParentNodeType)
{
  return CreateCSSNode(aName, GetCssNodeStyleInternal(aParentNodeType));
}

static GtkStyleContext*
GetWidgetStyleWithClass(WidgetNodeType aWidgetType, const gchar* aStyleClass)
{
  GtkStyleContext* style = gtk_widget_get_style_context(GetWidget(aWidgetType));
  gtk_style_context_save(style);
  MOZ_ASSERT(!sStyleContextNeedsRestore);
  sStyleContextNeedsRestore = true;
  gtk_style_context_add_class(style, aStyleClass);
  return style;
}

/* GetCssNodeStyleInternal is used by Gtk >= 3.20 */
static GtkStyleContext*
GetCssNodeStyleInternal(WidgetNodeType aNodeType)
{
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style)
    return style;

  switch (aNodeType) {
    case MOZ_GTK_SCROLLBAR_CONTENTS_HORIZONTAL:
      style = CreateChildCSSNode("contents",
                                 MOZ_GTK_SCROLLBAR_HORIZONTAL);
      break;
    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_SCROLLBAR_CONTENTS_HORIZONTAL);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER,
                                 MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL);
      break;
    case MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL:
      style = CreateChildCSSNode("contents",
                                 MOZ_GTK_SCROLLBAR_VERTICAL);
      break;
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER,
                                 MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
      break;
    case MOZ_GTK_RADIOBUTTON:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_RADIO,
                                 MOZ_GTK_RADIOBUTTON_CONTAINER);
      break;
    case MOZ_GTK_CHECKBUTTON:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_CHECK,
                                 MOZ_GTK_CHECKBUTTON_CONTAINER);
      break;
    case MOZ_GTK_PROGRESS_TROUGH:
      /* Progress bar background (trough) */
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_PROGRESSBAR);
      break;
    case MOZ_GTK_PROGRESS_CHUNK:
      style = CreateChildCSSNode("progress",
                                 MOZ_GTK_PROGRESS_TROUGH);
      break;
    case MOZ_GTK_TOOLTIP:
      // We create this from the path because GtkTooltipWindow is not public.
      style = CreateCSSNode("tooltip", nullptr, GTK_TYPE_TOOLTIP);
      gtk_style_context_add_class(style, GTK_STYLE_CLASS_BACKGROUND);
      break; 
    case MOZ_GTK_GRIPPER:
      // TODO - create from CSS node
      return GetWidgetStyleWithClass(MOZ_GTK_GRIPPER,
                                     GTK_STYLE_CLASS_GRIP);
    case MOZ_GTK_INFO_BAR:
      // TODO - create from CSS node
      return GetWidgetStyleWithClass(MOZ_GTK_INFO_BAR,
                                     GTK_STYLE_CLASS_INFO);
    case MOZ_GTK_SPINBUTTON_ENTRY:
      // TODO - create from CSS node
      return GetWidgetStyleWithClass(MOZ_GTK_SPINBUTTON,
                                     GTK_STYLE_CLASS_ENTRY);
    case MOZ_GTK_SCROLLED_WINDOW:
      // TODO - create from CSS node
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                     GTK_STYLE_CLASS_FRAME);
    case MOZ_GTK_TEXT_VIEW:
      // TODO - create from CSS node
      return GetWidgetStyleWithClass(MOZ_GTK_TEXT_VIEW,
                                     GTK_STYLE_CLASS_VIEW);
    default:
      // TODO - create style from style path
      GtkWidget* widget = GetWidget(aNodeType);
      return gtk_widget_get_style_context(widget);
  }

  MOZ_ASSERT(style, "missing style context for node type");
  sStyleStorage[aNodeType] = style;
  return style;
}

/* GetWidgetStyleInternal is used by Gtk < 3.20 */
static GtkStyleContext*
GetWidgetStyleInternal(WidgetNodeType aNodeType)
{
  switch (aNodeType) {
    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLBAR_HORIZONTAL,
                                     GTK_STYLE_CLASS_TROUGH);
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLBAR_HORIZONTAL,
                                     GTK_STYLE_CLASS_SLIDER);
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                     GTK_STYLE_CLASS_TROUGH);
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                     GTK_STYLE_CLASS_SLIDER);
    case MOZ_GTK_RADIOBUTTON:
      return GetWidgetStyleWithClass(MOZ_GTK_RADIOBUTTON_CONTAINER,
                                     GTK_STYLE_CLASS_RADIO);
    case MOZ_GTK_CHECKBUTTON:
      return GetWidgetStyleWithClass(MOZ_GTK_CHECKBUTTON_CONTAINER,
                                     GTK_STYLE_CLASS_CHECK);
    case MOZ_GTK_PROGRESS_TROUGH:
      return GetWidgetStyleWithClass(MOZ_GTK_PROGRESSBAR,
                                     GTK_STYLE_CLASS_TROUGH);
    case MOZ_GTK_TOOLTIP: {
      GtkStyleContext* style = sStyleStorage[aNodeType];
      if (style)
        return style;

      // The tooltip style class is added first in CreateTooltipWidget() so
      // that gtk_widget_path_append_for_widget() in CreateStyleForWidget()
      // will find it.
      GtkWidget* tooltipWindow = CreateTooltipWidget();
      style = CreateStyleForWidget(tooltipWindow, nullptr);
      gtk_widget_destroy(tooltipWindow); // Release GtkWindow self-reference.
      sStyleStorage[aNodeType] = style;
      return style;
    }
    case MOZ_GTK_GRIPPER:
      return GetWidgetStyleWithClass(MOZ_GTK_GRIPPER,
                                     GTK_STYLE_CLASS_GRIP);
    case MOZ_GTK_INFO_BAR:
      return GetWidgetStyleWithClass(MOZ_GTK_INFO_BAR,
                                     GTK_STYLE_CLASS_INFO);
    case MOZ_GTK_SPINBUTTON_ENTRY:
      return GetWidgetStyleWithClass(MOZ_GTK_SPINBUTTON,
                                     GTK_STYLE_CLASS_ENTRY);
    case MOZ_GTK_SCROLLED_WINDOW:
      return GetWidgetStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                     GTK_STYLE_CLASS_FRAME);
    case MOZ_GTK_TEXT_VIEW:
      return GetWidgetStyleWithClass(MOZ_GTK_TEXT_VIEW,
                                     GTK_STYLE_CLASS_VIEW);
    default:
      GtkWidget* widget = GetWidget(aNodeType);
      MOZ_ASSERT(widget);
      return gtk_widget_get_style_context(widget);
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
  mozilla::PodArrayZero(sStyleStorage);

  /* This will destroy all of our widgets */
  if (sWidgetStorage[MOZ_GTK_WINDOW])
    gtk_widget_destroy(sWidgetStorage[MOZ_GTK_WINDOW]);

  /* Clear already freed arrays */
  mozilla::PodArrayZero(sWidgetStorage);
}

GtkStyleContext*
ClaimStyleContext(WidgetNodeType aNodeType, GtkTextDirection aDirection,
                  GtkStateFlags aStateFlags, StyleFlags aFlags)
{
  MOZ_ASSERT(!sStyleContextNeedsRestore);
  GtkStyleContext* style;
  if (gtk_check_version(3, 20, 0) != nullptr) {
    style = GetWidgetStyleInternal(aNodeType);
  } else {
    style = GetCssNodeStyleInternal(aNodeType);
  }
#ifdef DEBUG
  MOZ_ASSERT(!sCurrentStyleContext);
  sCurrentStyleContext = style;
#endif
  GtkStateFlags oldState = gtk_style_context_get_state(style);
  GtkTextDirection oldDirection = gtk_style_context_get_direction(style);
  if (oldState != aStateFlags || oldDirection != aDirection) {
    // From GTK 3.8, set_state() will overwrite the direction, so set
    // direction after state.
    gtk_style_context_set_state(style, aStateFlags);
    gtk_style_context_set_direction(style, aDirection);

    // This invalidate is necessary for unsaved style contexts from GtkWidgets
    // in pre-3.18 GTK, because automatic invalidation of such contexts
    // was delayed until a resize event runs.
    //
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1272194#c7
    //
    // Avoid calling invalidate on saved contexts to avoid performing
    // build_properties() (in 3.16 stylecontext.c) unnecessarily early.
    if (!sStyleContextNeedsRestore) {
      gtk_style_context_invalidate(style);
    }
  }
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
