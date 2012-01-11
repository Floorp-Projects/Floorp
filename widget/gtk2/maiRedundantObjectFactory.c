/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ginn Chen (ginn.chen@sun.com)
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

#include <atk/atk.h>
#include "maiRedundantObjectFactory.h"

static void mai_redundant_object_factory_class_init (
                              maiRedundantObjectFactoryClass *klass);

static AtkObject* mai_redundant_object_factory_create_accessible (
                              GObject *obj);
static GType mai_redundant_object_factory_get_accessible_type (void);

static gpointer parent_class = NULL;

GType
mai_redundant_object_factory_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (maiRedundantObjectFactoryClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) mai_redundant_object_factory_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (maiRedundantObjectFactory), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };
    type = g_type_register_static (
                           ATK_TYPE_OBJECT_FACTORY,
                           "MaiRedundantObjectFactory" , &tinfo, 0);
  }

  return type;
}

static void
mai_redundant_object_factory_class_init (maiRedundantObjectFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->create_accessible = mai_redundant_object_factory_create_accessible;
  class->get_accessible_type = mai_redundant_object_factory_get_accessible_type;
}

/**
 * mai_redundant_object_factory_new:
 *
 * Creates an instance of an #AtkObjectFactory which generates primitive
 * (non-functioning) #AtkObjects.
 *
 * Returns: an instance of an #AtkObjectFactory
 **/
AtkObjectFactory*
mai_redundant_object_factory_new ()
{
  GObject *factory;

  factory = g_object_new (mai_redundant_object_factory_get_type(), NULL);

  g_return_val_if_fail (factory != NULL, NULL);
  return ATK_OBJECT_FACTORY (factory);
}

static AtkObject*
mai_redundant_object_factory_create_accessible (GObject   *obj)
{
  AtkObject *accessible;

  g_return_val_if_fail (obj != NULL, NULL);

  accessible = g_object_new (ATK_TYPE_OBJECT, NULL);
  g_return_val_if_fail (accessible != NULL, NULL);

  accessible->role = ATK_ROLE_REDUNDANT_OBJECT;

  return accessible;
}

static GType
mai_redundant_object_factory_get_accessible_type ()
{
  return mai_redundant_object_factory_get_type();
}
