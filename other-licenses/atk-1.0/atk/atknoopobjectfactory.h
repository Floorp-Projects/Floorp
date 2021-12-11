/* ATK - Accessibility Toolkit
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

#ifndef __ATK_NO_OP_OBJECT_FACTORY_H__
#define __ATK_NO_OP_OBJECT_FACTORY_H__

#include <atk/atkobjectfactory.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ATK_TYPE_NO_OP_OBJECT_FACTORY                (atk_no_op_object_factory_get_type ())
#define ATK_NO_OP_OBJECT_FACTORY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_NO_OP_OBJECT_FACTORY, AtkNoOpObjectFactory))
#define ATK_NO_OP_OBJECT_FACTORY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), ATK_TYPE_NO_OP_OBJECT_FACTORY, AtkNoOpObjectFactoryClass))
#define ATK_IS_NO_OP_OBJECT_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_NO_OP_OBJECT_FACTORY))
#define ATK_IS_NO_OP_OBJECT_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), ATK_TYPE_NO_OP_OBJECT_FACTORY))
#define ATK_NO_OP_OBJECT_FACTORY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ( (obj), ATK_TYPE_NO_OP_OBJECT_FACTORY, AtkNoOpObjectFactoryClass))

typedef struct _AtkNoOpObjectFactory                 AtkNoOpObjectFactory;
typedef struct _AtkNoOpObjectFactoryClass            AtkNoOpObjectFactoryClass;

struct _AtkNoOpObjectFactory
{
  AtkObjectFactory parent;
};

struct _AtkNoOpObjectFactoryClass
{
  AtkObjectFactoryClass parent_class;
};

GType atk_no_op_object_factory_get_type(void);

AtkObjectFactory *atk_no_op_object_factory_new(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ATK_NO_OP_OBJECT_FACTORY_H__ */
