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

#ifndef __ATK_RELATION_H__
#define __ATK_RELATION_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib-object.h>
#include <atk/atkrelationtype.h>

/*
 * An AtkRelation describes a relation between the object and one or more 
 * other objects. The actual relations that an object has with other objects
 * are defined as an AtkRelationSet, which is a set of AtkRelations. 
 */

#define ATK_TYPE_RELATION                         (atk_relation_get_type ())
#define ATK_RELATION(obj)                         (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_RELATION, AtkRelation))
#define ATK_RELATION_CLASS(klass)                 (G_TYPE_CHECK_CLASS_CAST ((klass), ATK_TYPE_RELATION, AtkRelationClass))
#define ATK_IS_RELATION(obj)                      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_RELATION))
#define ATK_IS_RELATION_CLASS(klass)              (G_TYPE_CHECK_CLASS_TYPE ((klass), ATK_TYPE_RELATION))
#define ATK_RELATION_GET_CLASS(obj)               (G_TYPE_INSTANCE_GET_CLASS ((obj), ATK_TYPE_RELATION, AtkRelationClass))

typedef struct _AtkRelation            AtkRelation;
typedef struct _AtkRelationClass       AtkRelationClass;

struct _AtkRelation
{
  GObject parent;

  GPtrArray       *target;
  AtkRelationType relationship;
};

struct _AtkRelationClass
{
  GObjectClass parent;
};

GType atk_relation_get_type (void);

AtkRelationType       atk_relation_type_register      (const gchar     *name);
G_CONST_RETURN gchar* atk_relation_type_get_name      (AtkRelationType type);
AtkRelationType       atk_relation_type_for_name      (const gchar     *name);

/*
 * Create a new relation for the specified key and the specified list
 * of targets.
 */
AtkRelation*          atk_relation_new                (AtkObject       **targets,
                                                       gint            n_targets,
                                                       AtkRelationType relationship);
/*
 * Returns the type of a relation.
 */
AtkRelationType       atk_relation_get_relation_type  (AtkRelation     *relation);
/*
 * Returns the target list of a relation.
 */
GPtrArray*            atk_relation_get_target         (AtkRelation     *relation);
void                  atk_relation_add_target         (AtkRelation     *relation,
                                                       AtkObject       *target);
                

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ATK_RELATION_H__ */
