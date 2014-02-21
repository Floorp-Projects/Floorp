/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQAppInstance.h"
#include <QGuiApplication>
#include "prenv.h"
#include "nsXPCOMPrivate.h"
#include <stdlib.h>

QGuiApplication *nsQAppInstance::sQAppInstance = nullptr;
int nsQAppInstance::sQAppRefCount = 0;

void nsQAppInstance::AddRef(int& aArgc, char** aArgv, bool aDefaultProcess) {
  if (qApp)
    return;
  if (!sQAppInstance) {
    mozilla::SetICUMemoryFunctions();
    sQAppInstance = new QGuiApplication(aArgc, aArgv);
  }
  sQAppRefCount++;
}

void nsQAppInstance::Release(void) {
  if (sQAppInstance && !--sQAppRefCount) {
    delete sQAppInstance;
    sQAppInstance = nullptr;
  }
}
