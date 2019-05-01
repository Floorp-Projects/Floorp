/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MAI_REDUNDANT_OBJECT_FACTORY_H__
#define __MAI_REDUNDANT_OBJECT_FACTORY_H__

G_BEGIN_DECLS

typedef struct _maiRedundantObjectFactory maiRedundantObjectFactory;
typedef struct _maiRedundantObjectFactoryClass maiRedundantObjectFactoryClass;

struct _maiRedundantObjectFactory {
  AtkObjectFactory parent;
};

struct _maiRedundantObjectFactoryClass {
  AtkObjectFactoryClass parent_class;
};

GType mai_redundant_object_factory_get_type();

AtkObjectFactory* mai_redundant_object_factory_new();

G_END_DECLS

#endif /* __NS_MAI_REDUNDANT_OBJECT_FACTORY_H__ */
