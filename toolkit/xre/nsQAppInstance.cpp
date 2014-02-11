/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQAppInstance.h"
#include <QApplication>
#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <MComponentData>
#include <MApplicationService>
#endif
#include "prenv.h"
#include "nsXPCOMPrivate.h"
#include <stdlib.h>

QApplication *nsQAppInstance::sQAppInstance = nullptr;
#ifdef MOZ_ENABLE_MEEGOTOUCH
MComponentData* nsQAppInstance::sMComponentData = nullptr;
#endif
int nsQAppInstance::sQAppRefCount = 0;

void nsQAppInstance::AddRef(int& aArgc, char** aArgv, bool aDefaultProcess) {
  if (qApp)
    return;
  if (!sQAppInstance) {
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
    const char *graphicsSystem = getenv("MOZ_QT_GRAPHICSSYSTEM");
    if (graphicsSystem) {
      QApplication::setGraphicsSystem(QString(graphicsSystem));
    }
#endif
    mozilla::SetICUMemoryFunctions();
    sQAppInstance = new QApplication(aArgc, aArgv);
  }
  sQAppRefCount++;
}

void nsQAppInstance::Release(void) {
  if (sQAppInstance && !--sQAppRefCount) {
#ifdef MOZ_ENABLE_MEEGOTOUCH
    delete sMComponentData;
    sMComponentData = nullptr;
#endif
    delete sQAppInstance;
    sQAppInstance = nullptr;
  }
}
