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

#ifndef __ATK_OBJECT_FACTORY_H__
#define __ATK_OBJECT_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ATK_TYPE_OBJECT_FACTORY                     (atk_object_factory_get_type ())
#define ATK_OBJECT_FACTORY(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_OBJECT_FACTORY, AtkObjectFactory))
#define ATK_OBJECT_FACTORY_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), ATK_TYPE_OBJECT_FACTORY, AtkObjectFactoryClass))
#define ATK_IS_OBJECT_FACTORY(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_OBJECT_FACTORY))
#define ATK_IS_OBJECT_FACTORY_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), ATK_TYPE_OBJECT_FACTORY))
#define ATK_OBJECT_FACTORY_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), ATK_TYPE_OBJECT_FACTORY, AtkObjectFactoryClass))

typedef struct _AtkObjectFactory                AtkObjectFactory;
typedef struct _AtkObjectFactoryClass           AtkObjectFactoryClass;

struct _AtkObjectFactory
{
  GObject parent;
};

struct _AtkObjectFactoryClass
{
  GObjectClass parent_class;

  AtkObject* (* create_accessible) (GObject          *obj);
  void       (* invalidate)        (AtkObjectFactory *factory);
  GType      (* get_accessible_type)    (void);

  AtkFunction pad1;
  AtkFunction pad2;
};

GType atk_object_factory_get_type(void);

AtkObject* atk_object_factory_create_accessible (AtkObjectFactory *factory, GObject *obj);
void       atk_object_factory_invalidate (AtkObjectFactory *factory);
GType      atk_object_factory_get_accessible_type (AtkObjectFactory *factory);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_OBJECT_FACTORY_H__ */

