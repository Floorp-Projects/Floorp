/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * ***** END LICENSE BLOCK ***** */

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
