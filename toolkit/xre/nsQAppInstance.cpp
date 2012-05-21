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
#include <stdlib.h>

QApplication *nsQAppInstance::sQAppInstance = NULL;
#ifdef MOZ_ENABLE_MEEGOTOUCH
MComponentData* nsQAppInstance::sMComponentData = NULL;
#endif
int nsQAppInstance::sQAppRefCount = 0;

void nsQAppInstance::AddRef(int& aArgc, char** aArgv, bool aDefaultProcess) {
  if (qApp)
    return;
  if (!sQAppInstance) {
    const char *graphicsSystem = getenv("MOZ_QT_GRAPHICSSYSTEM");
    if (graphicsSystem) {
      QApplication::setGraphicsSystem(QString(graphicsSystem));
    }
#if (MOZ_PLATFORM_MAEMO == 6)
    // Should create simple windows style for non chrome process
    // otherwise meegotouch style initialize and crash happen
    // because we don't initialize MComponent for child process
    if (!aDefaultProcess) {
      QApplication::setStyle(QLatin1String("windows"));
    }
    if (!aArgc) {
      aArgv[aArgc] = strdup("nsQAppInstance");
      aArgc++;
    }
#endif
    sQAppInstance = new QApplication(aArgc, aArgv);
#ifdef MOZ_ENABLE_MEEGOTOUCH
    if (aDefaultProcess) {
      // GLContext created by meegotouch will be under meego graphics
      // system control, and will drop GL context automatically in background mode
      // In order to use that GLContext we need to implement
      // LayerManager switch in runtime from SW<->HW
      // That not yet implemented so we need to control GL context,
      // force software mode for, and create our own QGLWidget
      gArgv[gArgc] = strdup("-software");
      gArgc++;
      sMComponentData = new MComponentData(aArgc, aArgv, "", new MApplicationService(""));
    }
#endif
  }
  sQAppRefCount++;
}

void nsQAppInstance::Release(void) {
  if (sQAppInstance && !--sQAppRefCount) {
#ifdef MOZ_ENABLE_MEEGOTOUCH
    delete sMComponentData;
    sMComponentData = NULL;
#endif
    delete sQAppInstance;
    sQAppInstance = NULL;
  }
}
