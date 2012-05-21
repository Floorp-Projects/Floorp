/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
