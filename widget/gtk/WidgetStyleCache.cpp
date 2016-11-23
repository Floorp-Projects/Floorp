/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <gtk/gtk.h>
#include "WidgetStyleCache.h"
#include "gtkdrawing.h"

#define STATE_FLAG_DIR_LTR (1U << 7)
#define STATE_FLAG_DIR_RTL (1U << 8)
#if GTK_CHECK_VERSION(3,8,0)
static_assert(GTK_STATE_FLAG_DIR_LTR == STATE_FLAG_DIR_LTR &&
              GTK_STATE_FLAG_DIR_RTL == STATE_FLAG_DIR_RTL,
              "incorrect direction state flags");
#endif

static GtkWidget* sWidgetStorage[MOZ_GTK_WIDGET_NODE_COUNT];
static GtkStyleContext* sStyleStorage[MOZ_GTK_WIDGET_NODE_COUNT];

static GtkStyleContext*
GetWidgetRootStyle(WidgetNodeType aNodeType);
static GtkStyleContext*
GetCssNodeStyleInternal(WidgetNodeType aNodeType);

static GtkWidget*
CreateWindowWidget()
{
  GtkWidget *widget = gtk_window_new(GTK_WINDOW_POPUP);
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
CreateComboBoxWidget()
{
  GtkWidget* widget = gtk_combo_box_new();
  AddToWindowContainer(widget);
  return widget;
}

typedef struct
{
  GType       type;
  GtkWidget** widget;
} GtkInnerWidgetInfo;

static void
GetInnerWidget(GtkWidget* widget, gpointer client_data)
{
  auto info = static_cast<GtkInnerWidgetInfo*>(client_data);

  if (G_TYPE_CHECK_INSTANCE_TYPE(widget, info->type)) {
    *info->widget = widget;
  }
}

static GtkWidget*
CreateComboBoxButtonWidget()
{
  GtkWidget* comboBox = GetWidget(MOZ_GTK_COMBOBOX);
  GtkWidget* comboBoxButton = nullptr;

  /* Get its inner Button */
  GtkInnerWidgetInfo info = { GTK_TYPE_TOGGLE_BUTTON,
                              &comboBoxButton };
  gtk_container_forall(GTK_CONTAINER(comboBox),
                       GetInnerWidget, &info);

  if (!comboBoxButton) {
    /* Shouldn't be reached with current internal gtk implementation; we
     * use a generic toggle button as last resort fallback to avoid
     * crashing. */
    comboBoxButton = GetWidget(MOZ_GTK_TOGGLE_BUTTON);
  } else {
    /* We need to have pointers to the inner widgets (button, separator, arrow)
     * of the ComboBox to get the correct rendering from theme engines which
     * special cases their look. Since the inner layout can change, we ask GTK
     * to NULL our pointers when they are about to become invalid because the
     * corresponding widgets don't exist anymore. It's the role of
     * g_object_add_weak_pointer().
     * Note that if we don't find the inner widgets (which shouldn't happen), we
     * fallback to use generic "non-inner" widgets, and they don't need that kind
     * of weak pointer since they are explicit children of gProtoLayout and as
     * such GTK holds a strong reference to them. */
    g_object_add_weak_pointer(G_OBJECT(comboBoxButton),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_BUTTON);
  }

  return comboBoxButton;
}

static GtkWidget*
CreateComboBoxArrowWidget()
{
  GtkWidget* comboBoxButton = GetWidget(MOZ_GTK_COMBOBOX_BUTTON);
  GtkWidget* comboBoxArrow = nullptr;

  /* Get the widgets inside the Button */
  GtkWidget* buttonChild = gtk_bin_get_child(GTK_BIN(comboBoxButton));
  if (GTK_IS_BOX(buttonChild)) {
    /* appears-as-list = FALSE, cell-view = TRUE; the button
     * contains an hbox. This hbox is there because the ComboBox
     * needs to place a cell renderer, a separator, and an arrow in
     * the button when appears-as-list is FALSE. */
    GtkInnerWidgetInfo info = { GTK_TYPE_ARROW,
                                &comboBoxArrow };
    gtk_container_forall(GTK_CONTAINER(buttonChild),
                         GetInnerWidget, &info);
  } else if (GTK_IS_ARROW(buttonChild)) {
    /* appears-as-list = TRUE, or cell-view = FALSE;
     * the button only contains an arrow */
    comboBoxArrow = buttonChild;
  }

  if (!comboBoxArrow) {
    /* Shouldn't be reached with current internal gtk implementation;
     * we gButtonArrowWidget as last resort fallback to avoid
     * crashing. */
    comboBoxArrow = GetWidget(MOZ_GTK_BUTTON_ARROW);
  } else {
    g_object_add_weak_pointer(G_OBJECT(comboBoxArrow),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_ARROW);
  }

  return comboBoxArrow;
}

static GtkWidget*
CreateComboBoxSeparatorWidget()
{
  // Ensure to search for separator only once as it can fail
  // TODO - it won't initialize after ResetWidgetCache() call
  static bool isMissingSeparator = false;
  if (isMissingSeparator)
    return nullptr;

  /* Get the widgets inside the Button */
  GtkWidget* comboBoxSeparator = nullptr;
  GtkWidget* buttonChild =
    gtk_bin_get_child(GTK_BIN(GetWidget(MOZ_GTK_COMBOBOX_BUTTON)));
  if (GTK_IS_BOX(buttonChild)) {
    /* appears-as-list = FALSE, cell-view = TRUE; the button
     * contains an hbox. This hbox is there because the ComboBox
     * needs to place a cell renderer, a separator, and an arrow in
     * the button when appears-as-list is FALSE. */
    GtkInnerWidgetInfo info = { GTK_TYPE_SEPARATOR,
                                &comboBoxSeparator };
    gtk_container_forall(GTK_CONTAINER(buttonChild),
                         GetInnerWidget, &info);
  }

  if (comboBoxSeparator) {
    g_object_add_weak_pointer(G_OBJECT(comboBoxSeparator),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_SEPARATOR);
  } else {
    /* comboBoxSeparator may be NULL
     * when "appears-as-list" = TRUE or "cell-view" = FALSE;
     * if there is no separator, then we just won't paint it. */
    isMissingSeparator = true;
  }

  return comboBoxSeparator;
}

static GtkWidget*
CreateComboBoxEntryWidget()
{
  GtkWidget* widget = gtk_combo_box_new_with_entry();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateComboBoxEntryTextareaWidget()
{
  GtkWidget* comboBoxTextarea = nullptr;

  /* Get its inner Entry and Button */
  GtkInnerWidgetInfo info = { GTK_TYPE_ENTRY,
                              &comboBoxTextarea };
  gtk_container_forall(GTK_CONTAINER(GetWidget(MOZ_GTK_COMBOBOX_ENTRY)),
                       GetInnerWidget, &info);

  if (!comboBoxTextarea) {
    comboBoxTextarea = GetWidget(MOZ_GTK_ENTRY);
  } else {
    g_object_add_weak_pointer(G_OBJECT(comboBoxTextarea),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_ENTRY);
  }

  return comboBoxTextarea;
}

static GtkWidget*
CreateComboBoxEntryButtonWidget()
{
  GtkWidget* comboBoxButton = nullptr;

  /* Get its inner Entry and Button */
  GtkInnerWidgetInfo info = { GTK_TYPE_TOGGLE_BUTTON,
                              &comboBoxButton };
  gtk_container_forall(GTK_CONTAINER(GetWidget(MOZ_GTK_COMBOBOX_ENTRY)),
                       GetInnerWidget, &info);

  if (!comboBoxButton) {
    comboBoxButton = GetWidget(MOZ_GTK_TOGGLE_BUTTON);
  } else {
    g_object_add_weak_pointer(G_OBJECT(comboBoxButton),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_ENTRY_BUTTON);
  }

  return comboBoxButton;
}

static GtkWidget*
CreateComboBoxEntryArrowWidget()
{
  GtkWidget* comboBoxArrow = nullptr;

  /* Get the Arrow inside the Button */
  GtkWidget* buttonChild =
    gtk_bin_get_child(GTK_BIN(GetWidget(MOZ_GTK_COMBOBOX_ENTRY_BUTTON)));

  if (GTK_IS_BOX(buttonChild)) {
   /* appears-as-list = FALSE, cell-view = TRUE; the button
     * contains an hbox. This hbox is there because the ComboBox
     * needs to place a cell renderer, a separator, and an arrow in
     * the button when appears-as-list is FALSE. */
    GtkInnerWidgetInfo info = { GTK_TYPE_ARROW,
                                &comboBoxArrow };
    gtk_container_forall(GTK_CONTAINER(buttonChild),
                         GetInnerWidget, &info);
  } else if (GTK_IS_ARROW(buttonChild)) {
    /* appears-as-list = TRUE, or cell-view = FALSE;
     * the button only contains an arrow */
    comboBoxArrow = buttonChild;
  }

  if (!comboBoxArrow) {
    /* Shouldn't be reached with current internal gtk implementation;
     * we gButtonArrowWidget as last resort fallback to avoid
     * crashing. */
    comboBoxArrow = GetWidget(MOZ_GTK_BUTTON_ARROW);
  } else {
    g_object_add_weak_pointer(G_OBJECT(comboBoxArrow),
                              reinterpret_cast<gpointer *>(sWidgetStorage) +
                              MOZ_GTK_COMBOBOX_ENTRY_ARROW);
  }

  return comboBoxArrow;
}

static GtkWidget*
CreateScrolledWindowWidget()
{
  GtkWidget* widget = gtk_scrolled_window_new(nullptr, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateMenuSeparatorWidget()
{
  GtkWidget* widget = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(GetWidget(MOZ_GTK_MENUPOPUP)),
                        widget);
  return widget;
}

static GtkWidget*
CreateTreeViewWidget()
{
  GtkWidget* widget = gtk_tree_view_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateTreeHeaderCellWidget()
{
  /*
   * Some GTK engines paint the first and last cell
   * of a TreeView header with a highlight.
   * Since we do not know where our widget will be relative
   * to the other buttons in the TreeView header, we must
   * paint it as a button that is between two others,
   * thus ensuring it is neither the first or last button
   * in the header.
   * GTK doesn't give us a way to do this explicitly,
   * so we must paint with a button that is between two
   * others.
   */
  GtkTreeViewColumn* firstTreeViewColumn;
  GtkTreeViewColumn* middleTreeViewColumn;
  GtkTreeViewColumn* lastTreeViewColumn;

  GtkWidget *treeView = GetWidget(MOZ_GTK_TREEVIEW);

  /* Create and append our three columns */
  firstTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(firstTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView),
                              firstTreeViewColumn);

  middleTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(middleTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView),
                              middleTreeViewColumn);

  lastTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(lastTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView),
                              lastTreeViewColumn);

  /* Use the middle column's header for our button */
  return gtk_tree_view_column_get_button(middleTreeViewColumn);
}

static GtkWidget*
CreateTreeHeaderSortArrowWidget()
{
  /* TODO, but it can't be NULL */
  GtkWidget* widget = gtk_button_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateHPanedWidget()
{
  GtkWidget* widget = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateVPanedWidget()
{
  GtkWidget* widget = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateScaleWidget(GtkOrientation aOrientation)
{
  GtkWidget* widget = gtk_scale_new(aOrientation, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget*
CreateNotebookWidget()
{
  GtkWidget* widget = gtk_notebook_new();
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
    case MOZ_GTK_MENUSEPARATOR:
      return CreateMenuSeparatorWidget();
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
    case MOZ_GTK_TREEVIEW:
      return CreateTreeViewWidget();
    case MOZ_GTK_TREE_HEADER_CELL:
      return CreateTreeHeaderCellWidget();
    case MOZ_GTK_TREE_HEADER_SORTARROW:
      return CreateTreeHeaderSortArrowWidget();
    case MOZ_GTK_SPLITTER_HORIZONTAL:
      return CreateHPanedWidget();
    case MOZ_GTK_SPLITTER_VERTICAL:
      return CreateVPanedWidget();
    case MOZ_GTK_SCALE_HORIZONTAL:
      return CreateScaleWidget(GTK_ORIENTATION_HORIZONTAL);
    case MOZ_GTK_SCALE_VERTICAL:
      return CreateScaleWidget(GTK_ORIENTATION_VERTICAL);
    case MOZ_GTK_NOTEBOOK:
      return CreateNotebookWidget();
    case MOZ_GTK_COMBOBOX:
      return CreateComboBoxWidget();
    case MOZ_GTK_COMBOBOX_BUTTON:
      return CreateComboBoxButtonWidget();
    case MOZ_GTK_COMBOBOX_ARROW:
      return CreateComboBoxArrowWidget();
    case MOZ_GTK_COMBOBOX_SEPARATOR:
      return CreateComboBoxSeparatorWidget();
    case MOZ_GTK_COMBOBOX_ENTRY:
      return CreateComboBoxEntryWidget();
    case MOZ_GTK_COMBOBOX_ENTRY_TEXTAREA:
      return CreateComboBoxEntryTextareaWidget();
    case MOZ_GTK_COMBOBOX_ENTRY_BUTTON:
      return CreateComboBoxEntryButtonWidget();
    case MOZ_GTK_COMBOBOX_ENTRY_ARROW:
      return CreateComboBoxEntryArrowWidget();
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

static void
AddStyleClassesFromStyle(GtkStyleContext* aDest, GtkStyleContext* aSrc)
{
  GList* classes = gtk_style_context_list_classes(aSrc);
  for (GList* link = classes; link; link = link->next) {
    gtk_style_context_add_class(aDest, static_cast<gchar*>(link->data));
  }
  g_list_free(classes);
}

GtkStyleContext*
CreateStyleForWidget(GtkWidget* aWidget, GtkStyleContext* aParentStyle)
{
  static auto sGtkWidgetClassGetCSSName =
    reinterpret_cast<const char* (*)(GtkWidgetClass*)>
    (dlsym(RTLD_DEFAULT, "gtk_widget_class_get_css_name"));

  GtkWidgetClass *widgetClass = GTK_WIDGET_GET_CLASS(aWidget);
  const gchar* name = sGtkWidgetClassGetCSSName ?
    sGtkWidgetClassGetCSSName(widgetClass) : nullptr;

  GtkStyleContext *context =
    CreateCSSNode(name, aParentStyle, G_TYPE_FROM_CLASS(widgetClass));

  // Classes are stored on the style context instead of the path so that any
  // future gtk_style_context_save() will inherit classes on the head CSS
  // node, in the same way as happens when called on a style context owned by
  // a widget.
  //
  // Classes can be stored on a GtkCssNodeDeclaration and/or the path.
  // gtk_style_context_save() reuses the GtkCssNodeDeclaration, and appends a
  // new object to the path, without copying the classes from the old path
  // head.  The new head picks up classes from the GtkCssNodeDeclaration, but
  // not the path.  GtkWidgets store their classes on the
  // GtkCssNodeDeclaration, so make sure to add classes there.
  //
  // Picking up classes from the style context also means that
  // https://bugzilla.gnome.org/show_bug.cgi?id=767312, which can stop
  // gtk_widget_path_append_for_widget() from finding classes in GTK 3.20,
  // is not a problem.
  GtkStyleContext* widgetStyle = gtk_widget_get_style_context(aWidget);
  AddStyleClassesFromStyle(context, widgetStyle);

  // Release any floating reference on aWidget.
  g_object_ref_sink(aWidget);
  g_object_unref(aWidget);

  return context;
}

static GtkStyleContext*
CreateStyleForWidget(GtkWidget* aWidget, WidgetNodeType aParentType)
{
  return CreateStyleForWidget(aWidget, GetWidgetRootStyle(aParentType));
}

GtkStyleContext*
CreateCSSNode(const char* aName, GtkStyleContext* aParentStyle, GType aType)
{
  static auto sGtkWidgetPathIterSetObjectName =
    reinterpret_cast<void (*)(GtkWidgetPath *, gint, const char *)>
    (dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_set_object_name"));

  GtkWidgetPath* path;
  if (aParentStyle) {
    path = gtk_widget_path_copy(gtk_style_context_get_path(aParentStyle));
    // Copy classes from the parent style context to its corresponding node in
    // the path, because GTK will only match against ancestor classes if they
    // are on the path.
    GList* classes = gtk_style_context_list_classes(aParentStyle);
    for (GList* link = classes; link; link = link->next) {
      gtk_widget_path_iter_add_class(path, -1, static_cast<gchar*>(link->data));
    }
    g_list_free(classes);
  } else {
    path = gtk_widget_path_new();
  }

  gtk_widget_path_append_type(path, aType);

  if (sGtkWidgetPathIterSetObjectName) {
    (*sGtkWidgetPathIterSetObjectName)(path, -1, aName);
  }

  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

// Return a style context matching that of the root CSS node of a widget.
// This is used by all GTK versions.
static GtkStyleContext*
GetWidgetRootStyle(WidgetNodeType aNodeType)
{
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style)
    return style;

  switch (aNodeType) {
    case MOZ_GTK_MENUBARITEM:
      style = CreateStyleForWidget(gtk_menu_item_new(), MOZ_GTK_MENUBAR);
      break;
    case MOZ_GTK_MENUITEM:
      style = CreateStyleForWidget(gtk_menu_item_new(), MOZ_GTK_MENUPOPUP);
      break;
    case MOZ_GTK_CHECKMENUITEM:
      style = CreateStyleForWidget(gtk_check_menu_item_new(), MOZ_GTK_MENUPOPUP);
      break;
    case MOZ_GTK_RADIOMENUITEM:
      style = CreateStyleForWidget(gtk_radio_menu_item_new(nullptr),
                                   MOZ_GTK_MENUPOPUP);
      break;
    case MOZ_GTK_TEXT_VIEW:
      style = CreateStyleForWidget(gtk_text_view_new(),
                                   MOZ_GTK_SCROLLED_WINDOW);
      break;
    case MOZ_GTK_TOOLTIP:
      if (gtk_check_version(3, 20, 0) != nullptr) {
          // The tooltip style class is added first in CreateTooltipWidget()
          // and transfered to style in CreateStyleForWidget().
          GtkWidget* tooltipWindow = CreateTooltipWidget();
          style = CreateStyleForWidget(tooltipWindow, nullptr);
          gtk_widget_destroy(tooltipWindow); // Release GtkWindow self-reference.
      } else {
          // We create this from the path because GtkTooltipWindow is not public.
          style = CreateCSSNode("tooltip", nullptr, GTK_TYPE_TOOLTIP);
          gtk_style_context_add_class(style, GTK_STYLE_CLASS_BACKGROUND);
      }
      break;
    case MOZ_GTK_TOOLTIP_BOX:
      style = CreateStyleForWidget(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0),
                                   MOZ_GTK_TOOLTIP);
      break;
    case MOZ_GTK_TOOLTIP_BOX_LABEL:
      style = CreateStyleForWidget(gtk_label_new(nullptr),
                                   MOZ_GTK_TOOLTIP_BOX);
      break;
    default:
      GtkWidget* widget = GetWidget(aNodeType);
      MOZ_ASSERT(widget);
      return gtk_widget_get_style_context(widget);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aNodeType] = style;
  return style;
}

static GtkStyleContext*
CreateChildCSSNode(const char* aName, WidgetNodeType aParentNodeType)
{
  return CreateCSSNode(aName, GetCssNodeStyleInternal(aParentNodeType));
}

// Create a style context equivalent to a saved root style context of
// |aWidgetType| with |aStyleClass| as an additional class.  This is used to
// produce a context equivalent to what GTK versions < 3.20 use for many
// internal parts of widgets.
static GtkStyleContext*
CreateSubStyleWithClass(WidgetNodeType aWidgetType, const gchar* aStyleClass)
{
  static auto sGtkWidgetPathIterGetObjectName =
    reinterpret_cast<const char* (*)(const GtkWidgetPath*, gint)>
    (dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_get_object_name"));

  GtkStyleContext* parentStyle = GetWidgetRootStyle(aWidgetType);

  // Create a new context that behaves like |parentStyle| would after
  // gtk_style_context_save(parentStyle).
  //
  // Avoiding gtk_style_context_save() avoids the need to manage the
  // restore, and a new context permits caching style resolution.
  //
  // gtk_style_context_save(context) changes the node hierarchy of |context|
  // to add a new GtkCssNodeDeclaration that is a copy of its original node.
  // The new node is a child of the original node, and so the new heirarchy is
  // one level deeper.  The new node receives the same classes as the
  // original, but any changes to the classes on |context| will change only
  // the new node.  The new node inherits properties from the original node
  // (which retains the original heirarchy and classes) and matches CSS rules
  // with the new heirarchy and any changes to the classes.
  //
  // The change in hierarchy can produce some surprises in matching theme CSS
  // rules (e.g. https://bugzilla.gnome.org/show_bug.cgi?id=761870#c2), but it
  // is important here to produce the same behavior so that rules match the
  // same widget parts in Gecko as they do in GTK.
  //
  // When using public GTK API to construct style contexts, a widget path is
  // required.  CSS rules are not matched against the style context heirarchy
  // but according to the heirarchy in the widget path.  The path that matches
  // the same CSS rules as a saved context is like the path of |parentStyle|
  // but with an extra copy of the head (last) object appended.  Setting
  // |parentStyle| as the parent context provides the same inheritance of
  // properties from the widget root node.
  const GtkWidgetPath* parentPath = gtk_style_context_get_path(parentStyle);
  const gchar* name = sGtkWidgetPathIterGetObjectName ?
    sGtkWidgetPathIterGetObjectName(parentPath, -1) : nullptr;
  GType objectType = gtk_widget_path_get_object_type(parentPath);

  GtkStyleContext* style = CreateCSSNode(name, parentStyle, objectType);

  // Start with the same classes on the new node as were on |parentStyle|.
  // GTK puts no regions or junction_sides on widget root nodes, and so there
  // is no need to copy these.
  AddStyleClassesFromStyle(style, parentStyle);

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
    case MOZ_GTK_SCROLLBAR_BUTTON:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_BUTTON,
                                 MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL);
      break;
    case MOZ_GTK_RADIOBUTTON:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_RADIO,
                                 MOZ_GTK_RADIOBUTTON_CONTAINER);
      break;
    case MOZ_GTK_CHECKBUTTON:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_CHECK,
                                 MOZ_GTK_CHECKBUTTON_CONTAINER);
      break;
    case MOZ_GTK_RADIOMENUITEM_INDICATOR:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_RADIO,
                                 MOZ_GTK_RADIOMENUITEM);
      break;
    case MOZ_GTK_CHECKMENUITEM_INDICATOR:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_CHECK,
                                 MOZ_GTK_CHECKMENUITEM);
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
    case MOZ_GTK_GRIPPER:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_GRIPPER,
                                      GTK_STYLE_CLASS_GRIP);
      break;
    case MOZ_GTK_INFO_BAR:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_INFO_BAR,
                                      GTK_STYLE_CLASS_INFO);
      break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_SPINBUTTON,
                                      GTK_STYLE_CLASS_ENTRY);
      break;
    case MOZ_GTK_SCROLLED_WINDOW:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                      GTK_STYLE_CLASS_FRAME);
      break;
    case MOZ_GTK_TEXT_VIEW_TEXT:
    case MOZ_GTK_RESIZER:
      style = CreateChildCSSNode("text", MOZ_GTK_TEXT_VIEW);
      if (aNodeType == MOZ_GTK_RESIZER) {
        // The "grip" class provides the correct builtin icon from
        // gtk_render_handle().  The icon is drawn with shaded variants of
        // the background color, and so a transparent background would lead to
        // a transparent resizer.  gtk_render_handle() also uses the
        // background color to draw a background, and so this style otherwise
        // matches MOZ_GTK_TEXT_VIEW_TEXT to match the background with
        // textarea elements.  GtkTextView creates a separate text window and
        // so the background should not be transparent.
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_GRIP);
      }
      break;
    case MOZ_GTK_FRAME_BORDER:
      style = CreateChildCSSNode("border", MOZ_GTK_FRAME);
      break;
    case MOZ_GTK_TREEVIEW_VIEW:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_TREEVIEW,
                                      GTK_STYLE_CLASS_VIEW);
      break;
    case MOZ_GTK_TREEVIEW_EXPANDER:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_TREEVIEW,
                                      GTK_STYLE_CLASS_EXPANDER);
      break;
    case MOZ_GTK_SPLITTER_SEPARATOR_HORIZONTAL:
      style = CreateChildCSSNode("separator",
                                 MOZ_GTK_SPLITTER_HORIZONTAL);
      break;
    case MOZ_GTK_SPLITTER_SEPARATOR_VERTICAL:
      style = CreateChildCSSNode("separator",
                                 MOZ_GTK_SPLITTER_VERTICAL);
      break;
    case MOZ_GTK_SCALE_CONTENTS_HORIZONTAL:
      style = CreateChildCSSNode("contents",
                                 MOZ_GTK_SCALE_HORIZONTAL);
      break;
    case MOZ_GTK_SCALE_CONTENTS_VERTICAL:
      style = CreateChildCSSNode("contents",
                                 MOZ_GTK_SCALE_VERTICAL);
      break; 
    case MOZ_GTK_SCALE_TROUGH_HORIZONTAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_SCALE_CONTENTS_HORIZONTAL);
      break; 
    case MOZ_GTK_SCALE_TROUGH_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_SCALE_CONTENTS_VERTICAL);
      break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER,
                                 MOZ_GTK_SCALE_TROUGH_HORIZONTAL);
      break;
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER,
                                 MOZ_GTK_SCALE_TROUGH_VERTICAL);
      break;
    case MOZ_GTK_TAB_TOP:
    {
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_NOTEBOOK,
                                      GTK_STYLE_CLASS_TOP);
      gtk_style_context_add_region(style, GTK_STYLE_REGION_TAB,
                                   static_cast<GtkRegionFlags>(0));
      return style;
    }
    case MOZ_GTK_TAB_BOTTOM:
    {
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_NOTEBOOK,
                                      GTK_STYLE_CLASS_BOTTOM);
      gtk_style_context_add_region(style, GTK_STYLE_REGION_TAB,
                                   static_cast<GtkRegionFlags>(0));
      return style;
    }
    case MOZ_GTK_NOTEBOOK:
    case MOZ_GTK_NOTEBOOK_HEADER:
    case MOZ_GTK_TABPANELS:
    case MOZ_GTK_TAB_SCROLLARROW:
    { 
      // TODO - create from CSS node
      GtkWidget* widget = GetWidget(MOZ_GTK_NOTEBOOK);
      return gtk_widget_get_style_context(widget);
    }
    default:
      return GetWidgetRootStyle(aNodeType);
  }

  MOZ_ASSERT(style, "missing style context for node type");
  sStyleStorage[aNodeType] = style;
  return style;
}

/* GetWidgetStyleInternal is used by Gtk < 3.20 */
static GtkStyleContext*
GetWidgetStyleInternal(WidgetNodeType aNodeType)
{
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style)
    return style;

  switch (aNodeType) {
    case MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_HORIZONTAL,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_HORIZONTAL,
                                      GTK_STYLE_CLASS_SLIDER);
      break;
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                      GTK_STYLE_CLASS_SLIDER);
      break;
    case MOZ_GTK_RADIOBUTTON:
      style = CreateSubStyleWithClass(MOZ_GTK_RADIOBUTTON_CONTAINER,
                                      GTK_STYLE_CLASS_RADIO);
      break;
    case MOZ_GTK_CHECKBUTTON:
      style = CreateSubStyleWithClass(MOZ_GTK_CHECKBUTTON_CONTAINER,
                                      GTK_STYLE_CLASS_CHECK);
      break;
    case MOZ_GTK_RADIOMENUITEM_INDICATOR:
      style = CreateSubStyleWithClass(MOZ_GTK_RADIOMENUITEM,
                                      GTK_STYLE_CLASS_RADIO);
      break;
    case MOZ_GTK_CHECKMENUITEM_INDICATOR:
      style = CreateSubStyleWithClass(MOZ_GTK_CHECKMENUITEM,
                                      GTK_STYLE_CLASS_CHECK);
      break;
    case MOZ_GTK_PROGRESS_TROUGH:
      style = CreateSubStyleWithClass(MOZ_GTK_PROGRESSBAR,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_PROGRESS_CHUNK:
      style = CreateSubStyleWithClass(MOZ_GTK_PROGRESSBAR,
                                      GTK_STYLE_CLASS_PROGRESSBAR);
      gtk_style_context_remove_class(style, GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_GRIPPER:
      style = CreateSubStyleWithClass(MOZ_GTK_GRIPPER,
                                      GTK_STYLE_CLASS_GRIP);
      break;
    case MOZ_GTK_INFO_BAR:
      style = CreateSubStyleWithClass(MOZ_GTK_INFO_BAR,
                                      GTK_STYLE_CLASS_INFO);
      break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
      style = CreateSubStyleWithClass(MOZ_GTK_SPINBUTTON,
                                      GTK_STYLE_CLASS_ENTRY);
      break;
    case MOZ_GTK_SCROLLED_WINDOW:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                      GTK_STYLE_CLASS_FRAME);
      break;
    case MOZ_GTK_TEXT_VIEW_TEXT:
    case MOZ_GTK_RESIZER:
      // GTK versions prior to 3.20 do not have the view class on the root
      // node, but add this to determine the background for the text window.
      style = CreateSubStyleWithClass(MOZ_GTK_TEXT_VIEW, GTK_STYLE_CLASS_VIEW);
      if (aNodeType == MOZ_GTK_RESIZER) {
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_GRIP);
      }
      break;
    case MOZ_GTK_FRAME_BORDER:
      return GetWidgetRootStyle(MOZ_GTK_FRAME);
    case MOZ_GTK_TREEVIEW_VIEW:
      style = CreateSubStyleWithClass(MOZ_GTK_TREEVIEW,
                                      GTK_STYLE_CLASS_VIEW);
      break;
    case MOZ_GTK_TREEVIEW_EXPANDER:
      style = CreateSubStyleWithClass(MOZ_GTK_TREEVIEW,
                                      GTK_STYLE_CLASS_EXPANDER);
      break;
    case MOZ_GTK_SPLITTER_SEPARATOR_HORIZONTAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SPLITTER_HORIZONTAL,
                                      GTK_STYLE_CLASS_PANE_SEPARATOR);
      break;
    case MOZ_GTK_SPLITTER_SEPARATOR_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SPLITTER_VERTICAL,
                                      GTK_STYLE_CLASS_PANE_SEPARATOR);
      break;
    case MOZ_GTK_SCALE_TROUGH_HORIZONTAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCALE_HORIZONTAL,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_SCALE_TROUGH_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCALE_VERTICAL,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCALE_HORIZONTAL,
                                      GTK_STYLE_CLASS_SLIDER);
      break;
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCALE_VERTICAL,
                                      GTK_STYLE_CLASS_SLIDER);
      break;
    case MOZ_GTK_TAB_TOP:
      style = CreateSubStyleWithClass(MOZ_GTK_NOTEBOOK, GTK_STYLE_CLASS_TOP);
      gtk_style_context_add_region(style, GTK_STYLE_REGION_TAB,
                                   static_cast<GtkRegionFlags>(0));
      break;
    case MOZ_GTK_TAB_BOTTOM:
      style = CreateSubStyleWithClass(MOZ_GTK_NOTEBOOK, GTK_STYLE_CLASS_BOTTOM);
      gtk_style_context_add_region(style, GTK_STYLE_REGION_TAB,
                                   static_cast<GtkRegionFlags>(0));
      break;
    case MOZ_GTK_NOTEBOOK:
    case MOZ_GTK_NOTEBOOK_HEADER:
    case MOZ_GTK_TABPANELS:
    case MOZ_GTK_TAB_SCROLLARROW:
    { 
      GtkWidget* widget = GetWidget(MOZ_GTK_NOTEBOOK);
      return gtk_widget_get_style_context(widget);
    }
    default:
      return GetWidgetRootStyle(aNodeType);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aNodeType] = style;
  return style;
}

void
ResetWidgetCache(void)
{
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
  GtkStyleContext* style;
  if (gtk_check_version(3, 20, 0) != nullptr) {
    style = GetWidgetStyleInternal(aNodeType);
  } else {
    style = GetCssNodeStyleInternal(aNodeType);
  }
  bool stateChanged = false;
  bool stateHasDirection = gtk_get_minor_version() >= 8;
  GtkStateFlags oldState = gtk_style_context_get_state(style);
  MOZ_ASSERT(!(aStateFlags & (STATE_FLAG_DIR_LTR|STATE_FLAG_DIR_RTL)));
  unsigned newState = aStateFlags;
  if (stateHasDirection) {
    switch (aDirection) {
      case GTK_TEXT_DIR_LTR:
        newState |= STATE_FLAG_DIR_LTR;
        break;
      case GTK_TEXT_DIR_RTL:
        newState |= STATE_FLAG_DIR_RTL;
        break;
      default:
        MOZ_FALLTHROUGH_ASSERT("Bad GtkTextDirection");
      case GTK_TEXT_DIR_NONE:
        // GtkWidget uses a default direction if neither is explicitly
        // specified, but here DIR_NONE is interpreted as meaning the
        // direction is not important, so don't change the direction
        // unnecessarily.
        newState |= oldState & (STATE_FLAG_DIR_LTR|STATE_FLAG_DIR_RTL);
    }
  } else if (aDirection != GTK_TEXT_DIR_NONE) {
    GtkTextDirection oldDirection = gtk_style_context_get_direction(style);
    if (aDirection != oldDirection) {
      gtk_style_context_set_direction(style, aDirection);
      stateChanged = true;
    }
  }
  if (oldState != newState) {
    gtk_style_context_set_state(style, static_cast<GtkStateFlags>(newState));
    stateChanged = true;
  }
  // This invalidate is necessary for unsaved style contexts from GtkWidgets
  // in pre-3.18 GTK, because automatic invalidation of such contexts
  // was delayed until a resize event runs.
  //
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1272194#c7
  //
  // Avoid calling invalidate on contexts that are not owned and constructed
  // by widgets to avoid performing build_properties() (in 3.16 stylecontext.c)
  // unnecessarily early.
  if (stateChanged && sWidgetStorage[aNodeType]) {
    gtk_style_context_invalidate(style);
  }
  return style;
}

void
ReleaseStyleContext(GtkStyleContext* aStyleContext)
{
}
