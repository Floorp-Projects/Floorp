/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZMEEGOAPPSERVICE_H
#define MOZMEEGOAPPSERVICE_H

#include <MApplicationService>

/**
 * App service class, which prevents registration to d-bus
 * and allows multiple instances of application. This is
 * required for Mozillas remote service to work, because
 * it is initialized after MApplication.
 */
class MozMeegoAppService: public MApplicationService
{
  Q_OBJECT
public:
  MozMeegoAppService(): MApplicationService(QString()) {}
public Q_SLOTS:
  virtual QString registeredName() { return QString(); }
  virtual bool isRegistered() { return false; }
  virtual bool registerService() { return true; }
};
#endif // MOZMEEGOAPPSERVICE_H
