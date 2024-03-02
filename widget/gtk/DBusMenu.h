/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_DBusMenu_h
#define mozilla_widget_DBusMenu_h

#include <glib.h>
#include <gdk/gdk.h>

namespace mozilla {

namespace dom {
class Element;
}

namespace widget {

#define DBUSMENU_GLIB_FUNCTIONS                                              \
  FUNC(dbusmenu_menuitem_child_add_position, gboolean,                       \
       (DbusmenuMenuitem * mi, DbusmenuMenuitem * child, guint position))    \
  FUNC(dbusmenu_menuitem_set_root, void,                                     \
       (DbusmenuMenuitem * mi, gboolean root))                               \
  FUNC(dbusmenu_menuitem_child_append, gboolean,                             \
       (DbusmenuMenuitem * mi, DbusmenuMenuitem * child))                    \
  FUNC(dbusmenu_menuitem_child_delete, gboolean,                             \
       (DbusmenuMenuitem * mi, DbusmenuMenuitem * child))                    \
  FUNC(dbusmenu_menuitem_get_children, GList*, (DbusmenuMenuitem * mi))      \
  FUNC(dbusmenu_menuitem_new, DbusmenuMenuitem*, (void))                     \
  FUNC(dbusmenu_menuitem_property_get, const gchar*,                         \
       (DbusmenuMenuitem * mi, const gchar* property))                       \
  FUNC(dbusmenu_menuitem_property_get_bool, gboolean,                        \
       (DbusmenuMenuitem * mi, const gchar* property))                       \
  FUNC(dbusmenu_menuitem_property_remove, void,                              \
       (DbusmenuMenuitem * mi, const gchar* property))                       \
  FUNC(dbusmenu_menuitem_property_set, gboolean,                             \
       (DbusmenuMenuitem * mi, const gchar* property, const gchar* value))   \
  FUNC(dbusmenu_menuitem_property_set_bool, gboolean,                        \
       (DbusmenuMenuitem * mi, const gchar* property, const gboolean value)) \
  FUNC(dbusmenu_menuitem_property_set_int, gboolean,                         \
       (DbusmenuMenuitem * mi, const gchar* property, const gint value))     \
  FUNC(dbusmenu_menuitem_show_to_user, void,                                 \
       (DbusmenuMenuitem * mi, guint timestamp))                             \
  FUNC(dbusmenu_menuitem_take_children, GList*, (DbusmenuMenuitem * mi))     \
  FUNC(dbusmenu_server_new, DbusmenuServer*, (const gchar* object))          \
  FUNC(dbusmenu_server_set_root, void,                                       \
       (DbusmenuServer * server, DbusmenuMenuitem * root))                   \
  FUNC(dbusmenu_server_set_status, void,                                     \
       (DbusmenuServer * server, DbusmenuStatus status))

#define DBUSMENU_GTK_FUNCTIONS                              \
  FUNC(dbusmenu_menuitem_property_set_image, gboolean,      \
       (DbusmenuMenuitem * menuitem, const gchar* property, \
        const GdkPixbuf* data))                             \
  FUNC(dbusmenu_menuitem_property_set_shortcut, gboolean,   \
       (DbusmenuMenuitem * menuitem, guint key, GdkModifierType modifier))

typedef struct _DbusmenuMenuitem DbusmenuMenuitem;
typedef struct _DbusmenuServer DbusmenuServer;

enum DbusmenuStatus { DBUSMENU_STATUS_NORMAL, DBUSMENU_STATUS_NOTICE };

#define DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU "submenu"
#define DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY "children-display"
#define DBUSMENU_MENUITEM_PROP_ENABLED "enabled"
#define DBUSMENU_MENUITEM_PROP_ICON_DATA "icon-data"
#define DBUSMENU_MENUITEM_PROP_LABEL "label"
#define DBUSMENU_MENUITEM_PROP_SHORTCUT "shortcut"
#define DBUSMENU_MENUITEM_PROP_TYPE "type"
#define DBUSMENU_MENUITEM_PROP_TOGGLE_STATE "toggle-state"
#define DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE "toggle-type"
#define DBUSMENU_MENUITEM_PROP_VISIBLE "visible"
#define DBUSMENU_MENUITEM_SIGNAL_ABOUT_TO_SHOW "about-to-show"
#define DBUSMENU_MENUITEM_SIGNAL_EVENT "event"
#define DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED "item-activated"
#define DBUSMENU_MENUITEM_TOGGLE_CHECK "checkmark"
#define DBUSMENU_MENUITEM_TOGGLE_RADIO "radio"
#define DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED 1
#define DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED 0
#define DBUSMENU_SERVER_PROP_DBUS_OBJECT "dbus-object"

class DBusMenuFunctions {
 public:
  DBusMenuFunctions() = delete;

  static bool Init();

#define FUNC(name, type, params)      \
  typedef type(*_##name##_fn) params; \
  static _##name##_fn s_##name;
  DBUSMENU_GLIB_FUNCTIONS
  DBUSMENU_GTK_FUNCTIONS
#undef FUNC
};

#define dbusmenu_menuitem_set_root \
  DBusMenuFunctions::s_dbusmenu_menuitem_set_root
#define dbusmenu_menuitem_child_add_position \
  DBusMenuFunctions::s_dbusmenu_menuitem_child_add_position
#define dbusmenu_menuitem_child_append \
  DBusMenuFunctions::s_dbusmenu_menuitem_child_append
#define dbusmenu_menuitem_child_delete \
  DBusMenuFunctions::s_dbusmenu_menuitem_child_delete
#define dbusmenu_menuitem_get_children \
  DBusMenuFunctions::s_dbusmenu_menuitem_get_children
#define dbusmenu_menuitem_new DBusMenuFunctions::s_dbusmenu_menuitem_new
#define dbusmenu_menuitem_property_get \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_get
#define dbusmenu_menuitem_property_get_bool \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_get_bool
#define dbusmenu_menuitem_property_remove \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_remove
#define dbusmenu_menuitem_property_set \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_set
#define dbusmenu_menuitem_property_set_bool \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_set_bool
#define dbusmenu_menuitem_property_set_int \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_set_int
#define dbusmenu_menuitem_show_to_user \
  DBusMenuFunctions::s_dbusmenu_menuitem_show_to_user
#define dbusmenu_menuitem_take_children \
  DBusMenuFunctions::s_dbusmenu_menuitem_take_children
#define dbusmenu_server_new DBusMenuFunctions::s_dbusmenu_server_new
#define dbusmenu_server_set_root DBusMenuFunctions::s_dbusmenu_server_set_root
#define dbusmenu_server_set_status \
  DBusMenuFunctions::s_dbusmenu_server_set_status

#define dbusmenu_menuitem_property_set_image \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_set_image
#define dbusmenu_menuitem_property_set_shortcut \
  DBusMenuFunctions::s_dbusmenu_menuitem_property_set_shortcut

}  // namespace widget
}  // namespace mozilla

#endif
