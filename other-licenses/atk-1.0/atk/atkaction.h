/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ATK_ACTION_H__
#define __ATK_ACTION_H__

#include <atk/atkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * The interface AtkAction should be supported by any object that can 
 * perform one or more actions. The interface provides the standard 
 * mechanism for an assistive technology to determine what those actions 
 * are as well as tell the object to perform them. Any object that can 
 * be manipulated should support this interface.
 */


#define ATK_TYPE_ACTION                    (atk_action_get_type ())
#define ATK_IS_ACTION(obj)                 G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_ACTION)
#define ATK_ACTION(obj)                    G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_ACTION, AtkAction)
#define ATK_ACTION_GET_IFACE(obj)          (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_ACTION, AtkActionIface))

#ifndef _TYPEDEF_ATK_ACTION_
#define _TYPEDEF_ATK_ACTION_
typedef struct _AtkAction AtkAction;
#endif
typedef struct _AtkActionIface AtkActionIface;

struct _AtkActionIface
{
  GTypeInterface parent;

  gboolean                (*do_action)         (AtkAction         *action,
                                                gint              i);
  gint                    (*get_n_actions)     (AtkAction         *action);
  G_CONST_RETURN gchar*   (*get_description)   (AtkAction         *action,
                                                gint              i);
  G_CONST_RETURN gchar*   (*get_name)          (AtkAction         *action,
                                                gint              i);
  G_CONST_RETURN gchar*   (*get_keybinding)    (AtkAction         *action,
                                                gint              i);
  gboolean                (*set_description)   (AtkAction         *action,
                                                gint              i,
                                                const gchar       *desc);
  G_CONST_RETURN gchar*   (*get_localized_name)(AtkAction         *action,
						gint              i);
  AtkFunction             pad2;
};

GType atk_action_get_type (void);

/*
 * These are the function which would be called by an application with
 * the argument being a AtkObject object cast to (AtkAction).
 *
 * The function will just check that * the corresponding
 * function pointer is not NULL and will call it.
 *
 * The "real" implementation of the function for accessible will be
 * provided in a support library
 */

gboolean   atk_action_do_action                (AtkAction         *action,
                                            gint              i);
gint   atk_action_get_n_actions            (AtkAction *action);
G_CONST_RETURN gchar* atk_action_get_description  (AtkAction         *action,
                                                   gint              i);
G_CONST_RETURN gchar* atk_action_get_name         (AtkAction         *action,
                                                   gint              i);
G_CONST_RETURN gchar* atk_action_get_keybinding   (AtkAction         *action,
                                                   gint              i);
gboolean              atk_action_set_description  (AtkAction         *action,
                                                   gint              i,
                                                   const gchar       *desc);

/* NEW in ATK 1.1: */

G_CONST_RETURN gchar* atk_action_get_localized_name (AtkAction       *action,
						     gint            i);

/*
 * Additional GObject properties exported by AtkAction:
 *    "accessible_action"
 *       (an accessible action, or the list of actions, has changed)
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ATK_ACTION_H__ */
